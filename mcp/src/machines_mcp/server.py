"""Official SDK stdio entry point and one-owner operation coordination."""
import asyncio
import base64
import json
import uuid
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, ToolAnnotations, CallToolResult, TextContent, ImageContent
from . import __version__
from .artifacts import Artifacts
from .errors import AdapterError
from .lifecycle import Lifecycle
from .schemas.tools import TOOLS, validate_tool
from .backends.apple2 import dispatch, parse_response
from .codecs.frame import decode_frame
from .codecs.hst1 import decode_hst1
from .codecs.breakpoints import decode_breakpoints
from .codecs.debug_memory import DebugSnapshot, DebugSnapshots
from .codecs.vic_ring import decode_vic_ring
from .codecs.disassembly import decode as decode_disassembly

class Adapter:
    def __init__(self,config):
        self.artifacts=Artifacts(config)
        self.life=Lifecycle(config,self.artifacts)
        self.lock=asyncio.Lock()
        self.queued=0
        self.debug_snapshots=DebugSnapshots()

    def envelope(self,data,outcome='observed'):
        return dict(connection=self.life.connection,product=self.life.product,protocol=self.life.protocol,outcome=outcome,state_context=dict(cached_last_observed=self.life.context,sample_consistency='single_response'),data=data)

    async def call(self,name,args):
        validate_tool(name,args,self.life.product)
        if name=='connection_info':
            if args.get('connection'): self.life.require(args['connection'])
            return self.envelope(self.life.info()),None
        if name=='events':
            if args['connection'] != self.life.connection:
                raise AdapterError('stale_connection','Event generation does not match')
            after=int(args.get('after','0'))
            rows=[e for e in self.life.event_log if int(e['sequence'])>after][:args.get('limit',32)]
            return self.envelope(dict(events=rows,next_sequence=rows[-1]['sequence'] if rows else str(after),dropped_before=str(self.life.dropped))),None
        if name=='result_page':
            self.life.require(args['connection'])
            return self.envelope(self.artifacts.page(args['token'],args.get('offset',0),args.get('limit',32))),None
        lifecycle=name in ('attach','launch','detach','stop_owned','abandon_owned')
        if lifecycle and self.lock.locked():
            raise AdapterError('busy','Lifecycle operation conflicts with active emulator operation')
        if self.queued>=8:
            raise AdapterError('busy','Emulator operation queue is full')
        self.queued+=1
        try:
            async with self.lock:
                self.queued-=1
                task=asyncio.create_task(self._operation(name,args))
                try:
                    return await asyncio.shield(task)
                except asyncio.CancelledError:
                    # Keep lock until the dispatched reply is drained under its deadline.
                    try: await task
                    except Exception as exc: self.life.event('cancelled-operation-finished',error=str(exc),operation_effect='possible')
                    else: self.life.event('cancelled-operation-finished',operation_effect='possible')
                    raise
        finally:
            # Calls cancelled before acquiring the lock remove themselves.
            if 'task' not in locals(): self.queued-=1

    async def _operation(self,name,args):
        life=self.life
        if name=='attach': return self.envelope(await life.attach(args['port'],args.get('expected_product'))),None
        if name=='launch': return self.envelope(await life.launch(**args),'completed'),None
        if name in ('stop_owned','abandon_owned'):
            data=await getattr(life,name)(args['ownership'])
            return self.envelope(data,'completed'),None
        life.require(args['connection'])
        if name=='detach': return self.envelope(await life.detach(args.get('keep_running',False)),'completed'),None
        if name=='c64_debug_memory_page':
            data=self.debug_snapshots.get(life.connection,args['token']).window(args['address'],args.get('length',256))
            data['snapshot_token']=args['token']
            return self.envelope(data),None
        if name=='history_export':
            from .history_export import export_history
            self._paths(name,args)
            try:
                data=await export_history(life,self.artifacts,args)
            except AdapterError as exc:
                if exc.data['code']=='invalid_response':
                    await life.close_connection(); life.failure(exc)
                raise
            except ValueError as exc:
                await life.close_connection(); life.failure(AdapterError('invalid_response',str(exc)))
                raise AdapterError('invalid_response',str(exc),effect='possible')
            result=self.envelope(data,'completed')
            result['state_context']['sample_consistency']='verified_retained_window_multiple_observations'
            return result,None
        if name=='disassemble':
            cpu=args.get('cpu_class','auto'); model=None
            if cpu=='auto':
                if life.product=='c64' or args.get('source') in ('drive8','drive9'): cpu='nmos'
                else:
                    if 'get-softswitches' in life.disabled_verbs: raise AdapterError('unsupported_feature','Apple model observation unavailable; choose explicit cpu_class')
                    observed=await life.wire.request('get-softswitches')
                    model=parse_response('apple2','get-softswitches',observed.text)
                    if model.get('model') not in ('//e-enh','][+'): raise AdapterError('invalid_response','Unknown Apple CPU model; choose explicit cpu_class')
                    cpu='65c02' if model['model']=='//e-enh' else 'nmos'
            if 'get-memory' in life.disabled_verbs: raise AdapterError('unsupported_feature','Memory inspection unavailable')
            from .backends.apple2 import hx
            length=args.get('length',192)
            reply=await life.wire.request('get-memory',f"{hx(args['address'])} {length} {args.get('source','map')}")
            if reply.kind!='data' or reply.data_type!='memory' or len(reply.payload)!=length:
                await life.close_connection()
                raise AdapterError('invalid_response','Disassembly memory response mismatch')
            records=decode_disassembly(reply.payload,args['address'],cpu,args.get('max_instructions',64))
            consumed=sum(r['length'] for r in records)
            result=self.envelope(dict(address=args['address'],source=args.get('source','map'),cpu_class=cpu,model_observation=model,native_memory_metadata=reply.text,requested_bytes=length,decoded_bytes=consumed,next_address=args['address']+consumed if consumed<length else None,records=records))
            result['state_context']['sample_consistency']='separate_model_and_single_memory_sample' if model else 'single_memory_sample'
            return result,None
        if name=='history_next' or name=='history_close':
            token=args['cursor']
            if token not in life.cursors:
                raise AdapterError('cursor_stale','Cursor belongs to a prior connection or was closed')
            args=dict(args,cursor=life.cursors[token]['native'])
        if name=='history_find' and len(life.cursors)>=8:
            raise AdapterError('busy','Close an existing history cursor before opening another')
        self._paths(name,args)
        cmd=dispatch(life.product,name,args)
        if cmd.verb in life.disabled_verbs:
            raise AdapterError('unsupported_feature','Connected emulator lacks advertised capability for this operation')
        try:
            reply=await life.wire.request(cmd.verb,cmd.args,cmd.payload,cmd.deadline)
        except AdapterError as exc:
            if exc.data.get('wire_code','').lower().replace('-','_') == 'cursor_stale' or 'history-cursor-stale' in exc.data['message']:
                raise AdapterError('cursor_stale',exc.data['message'],wire_code=exc.data['wire_code'])
            raise
        image=None
        try:
            if name in ('get_frame','recorded_frame'):
                data,image=decode_frame(life.product,reply.text,reply.payload)
                if args.get('path'):
                    data['artifact']=self.artifacts.write(image,'png',path=args['path'],overwrite=args.get('overwrite',False))
            elif name=='c64_debug_memory':
                if reply.kind!='data' or reply.data_type!='debug-memory': raise ValueError('Unexpected debug-memory response')
                snapshot=DebugSnapshot(reply.text,reply.payload)
                data=snapshot.window(args['address'],args.get('length',256))
                if args.get('path'): data['artifact']=self.artifacts.write(reply.payload,'bin',path=args['path'],overwrite=args.get('overwrite',False))
                data['snapshot_token']=self.debug_snapshots.add(life.connection,snapshot)
            elif name=='c64_vic_ring_find':
                if reply.kind!='data' or reply.data_type!='vic-ring': raise ValueError('Unexpected VIC-ring response')
                data=decode_vic_ring(reply.text,reply.payload)
                if data['count']>args.get('limit',312): raise ValueError('VIC-ring exceeds requested limit')
                data['selection_limit']=args.get('limit',312)
                data['possibly_limited']=data['count']==args.get('limit',312)
                data=self.artifacts.bounded(data)
            elif name.startswith('history_') and reply.payload:
                data=decode_hst1(reply.text,reply.payload)
                native=data.get('cursor')
                if name=='history_find' or name=='history_next' and str(native)=='0':
                    life.cursors.clear()
                if str(native)=='0': data['cursor']=None
                if native is not None and str(native) not in ('0',''):
                    token=next((k for k,v in life.cursors.items() if v['native']==str(native)),uuid.uuid4().hex)
                    life.cursors[token]=dict(native=str(native),epoch=data.get('epoch'),generation=life.connection)
                    data['cursor']=token
                data=self.artifacts.bounded(data)
            elif name=='breakpoints' and reply.kind=='data':
                data=decode_breakpoints(life.product,reply.text,reply.payload)
            elif name in ('read_memory','export_memory'):
                if len(reply.payload)!=args.get('length',256):
                    raise ValueError('Memory response length mismatch')
                data=dict(address=args['address'],source=args.get('source','map'),length=len(reply.payload))
                if name=='export_memory':
                    data['artifact']=self.artifacts.write(reply.payload,'bin',path=args['path'],overwrite=args.get('overwrite',False))
                else: data['bytes_hex']=reply.payload.hex()
            else:
                data=parse_response(life.product,cmd.verb,reply.text,reply.payload)
        except ValueError as exc:
            await life.close_connection();life.failure(AdapterError('invalid_response',str(exc)))
            raise AdapterError('invalid_response',str(exc),effect='possible')
        if name=='history_close':
            for token,record in list(life.cursors.items()):
                if record['native']==args['cursor']: del life.cursors[token]
        if name in ('history_clear','snapshot','inspector'):
            life.cursors.clear()
        if name=='get_state':
            life.context=dict(data,observation_sequence=str(life.seq))
        return self.envelope(data,cmd.outcome),image

    def _paths(self,name,args):
        if 'path' not in args: return
        write=name in ('export_memory','get_frame','recorded_frame','c64_debug_memory','history_export') or name=='snapshot' and args['action']=='save' or name=='c64_binary_file' and args['action']=='save'
        self.life.config.path(args['path'],write=write,overwrite=args.get('overwrite',False))

async def run(config):
    adapter=Adapter(config)
    server=Server('machines-emulator-mcp',version=__version__)
    @server.list_tools()
    async def list_tools():
        return [Tool(name=name,description=spec['description']+' Parallel analysis is supported; designate one agent to own emulator interaction. Serialized calls do not make a multi-call investigation atomic.',inputSchema=spec['inputSchema'],annotations=ToolAnnotations(readOnlyHint=spec.get('read_only',False),openWorldHint=False)) for name,spec in TOOLS.items()]
    @server.call_tool()
    async def call_tool(name,arguments):
        try:
            result,image=await adapter.call(name,arguments)
            content=[TextContent(type='text',text=json.dumps(result,ensure_ascii=False))]
            if image: content.append(ImageContent(type='image',data=base64.b64encode(image).decode(),mimeType='image/png'))
            return CallToolResult(content=content,structuredContent=result)
        except AdapterError as exc:
            return CallToolResult(content=[TextContent(type='text',text=json.dumps(exc.data))],structuredContent=exc.data,isError=True)
        except (OSError,ValueError) as exc:
            result=dict(code='invalid_argument',message=str(exc),operation_effect='none',retry_safe=False)
            return CallToolResult(content=[TextContent(type='text',text=json.dumps(result))],structuredContent=result,isError=True)
    try:
        async with stdio_server() as (reader,writer):
            await server.run(reader,writer,server.create_initialization_options())
    finally:
        await adapter.life.shutdown()
