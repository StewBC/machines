"""Connection generations and separately retained owned child processes."""
import asyncio
from contextlib import ExitStack
from collections import deque
import json
from pathlib import Path
import socket
import time
import uuid
from .errors import AdapterError
from .wire import Wire
from .ownership import listener_owned

IDENTITIES={'A2M/16':'apple2','C64M/10':'c64'}

def fields(text):
    return dict(item.split('=',1) for item in text.split() if '=' in item)

class Lifecycle:
    def __init__(self,config,artifacts):
        self.config,self.artifacts=config,artifacts
        self.status='DISCONNECTED'
        self.wire=None
        self.connection=None
        self.product=None
        self.protocol=None
        self.owned=None
        self.context={}
        self.features=[]
        self.sources={}
        self.disabled_verbs=set()
        self.seq=0
        self.event_log=deque(maxlen=256)
        self.dropped=0
        self.cursors={}
        self.watcher=None

    def event(self,text,**extra):
        self.seq+=1
        if len(self.event_log)==256:
            self.dropped+=1
        self.event_log.append(dict(sequence=str(self.seq),observed_at=time.time(),text=text,**extra))

    def failure(self,error):
        self.status='UNAVAILABLE'
        self.cursors.clear(); self.artifacts.clear()
        self.event('connection-unavailable',**error.data)

    def info(self):
        result=dict(status=self.status,connection=self.connection,product=self.product,protocol=self.protocol,capabilities=self.features,disabled_verbs=sorted(self.disabled_verbs),memory_sources=self.sources,cached_context=self.context)
        if self.owned:
            result['owned']={k:v for k,v in self.owned.items() if k not in ('process','log_tasks')}
        return result

    def require(self,connection):
        if connection!=self.connection or not connection:
            raise AdapterError('stale_connection','Connection token does not match this generation')
        if self.status not in ('ATTACHED','OWNED') or not self.wire or self.wire.closed:
            raise AdapterError('unavailable','Emulator is unavailable',**({'ownership':self.owned['ownership']} if self.owned else {}))

    async def handshake(self,port,expected=None):
        wire=await Wire.connect(port,self.event,self.failure)
        try:
            hello=fields((await wire.request('hello')).text)
            protocol=hello.get('protocol')
            product=IDENTITIES.get(protocol)
            if not product:
                raise AdapterError('protocol_mismatch','Unsupported emulator protocol; update adapter',detected=protocol,expected=list(IDENTITIES))
            if hello.get('name')!=('a2m' if product=='apple2' else 'c64m') or expected and product!=expected:
                raise AdapterError('product_mismatch','Emulator identity does not match expected product')
            caps=await wire.request('capabilities')
            state=await wire.request('get-state')
            self.wire=wire; self.connection=uuid.uuid4().hex
            self.product,self.protocol=product,protocol
            self.features=caps.text.split()
            from .schemas.protocol_manifest import MANIFEST
            manifest=MANIFEST[product]
            expected=set(manifest.get('capabilities',[]))
            missing=expected-set(self.features)
            self.disabled_verbs={verb for verb,cap in manifest.get('verb_capabilities',{}).items() if cap and cap in missing}
            if missing: self.event('capability-mismatch',missing=sorted(missing))
            from .backends.apple2 import parse_response
            self.context=dict(parse_response(product,'get-state',state.text),observation_sequence=str(self.seq),observed_at=time.time())
            self.status='OWNED' if self.owned else 'ATTACHED'
            self.sources=manifest.get('sources',{})
            for verb in ('history-info','frame-ring-info'):
                if verb in self.disabled_verbs: continue
                try:
                    reply=await wire.request(verb)
                    self.context[verb]=fields(reply.text)
                except AdapterError as exc:
                    if exc.data['code']!='emulator_rejection':
                        raise
            return self.info()
        except BaseException:
            await wire.close()
            raise

    def _available(self):
        if self.owned and self.owned['process'].returncode is not None:
            self.owned=None
        if self.owned or self.status not in ('DISCONNECTED','UNAVAILABLE'):
            raise AdapterError('busy','Detach, stop_owned, or abandon_owned before connecting again')

    async def attach(self,port,expected_product=None):
        self._available(); self.status='CONNECTING'
        try:
            return await asyncio.wait_for(self.handshake(port,expected_product),3)
        except asyncio.TimeoutError:
            self.status='UNAVAILABLE'
            raise AdapterError('unavailable_or_control_occupied','Handshake timed out; emulator may be busy or control slot occupied')
        except BaseException:
            self.status='UNAVAILABLE'
            raise

    async def launch(self,profile,port=None,headless=None,keep_on_exit=False):
        self._available()
        if profile not in self.config.profiles:
            raise AdapterError('configuration','Unknown configured launch profile')
        cfg=self.config.profiles[profile]
        port=port if port is not None else cfg.get('port')
        if not isinstance(port,int) or not 1<=port<=65535:
            raise AdapterError('configuration','Profile requires a valid control port')
        with socket.socket() as probe:
            try: probe.bind(('127.0.0.1',port))
            except OSError: raise AdapterError('control_occupied','Control port already occupied')
        args=[cfg['executable'],f'--control-port={port}','--nosaveini']
        args+=['--inifile='+cfg['ini']] if cfg.get('ini') else ['--noini']
        if headless if headless is not None else cfg.get('headless',False): args.append('--headless')
        for key in ('history_memory','inspector_memory'):
            if key in cfg:
                value=cfg[key]
                if not isinstance(value,int) or not(value==0 or 16<=value<=4096):
                    raise AdapterError('configuration',f'{key} must be 0 or 16..4096 MiB')
                args.append('--'+key.replace('_','-')+'='+str(value))
        if 'inspector' in cfg: args.append('--inspector' if cfg['inspector'] else '--no-inspector')
        args+=cfg.get('argv',[])
        if self.config.artifact_dir is None:
            raise AdapterError('configuration','Launch requires artifact_dir for captured logs')
        self.status='CONNECTING'
        token=uuid.uuid4().hex
        logs={n:str(self.config.artifact_dir/f'{token}-{n}.log') for n in ('stdout','stderr')}
        # Durable file descriptors survive abandonment/adapter EOF. No pipe
        # drain dependency may block or SIGPIPE a child explicitly kept alive.
        try:
            with ExitStack() as stack:
                handles=[stack.enter_context(open(logs[n],'ab')) for n in ('stdout','stderr')]
                process=await asyncio.create_subprocess_exec(*args,cwd=cfg['cwd'],stdout=handles[0],stderr=handles[1])
        except BaseException:
            self.status='DISCONNECTED'
            raise
        self.owned=dict(ownership=token,process=process,pid=process.pid,profile=profile,port=port,keep_on_exit=keep_on_exit,logs=logs)
        async def rotate():
            while process.returncode is None:
                for path in logs.values():
                    p=Path(path)
                    if p.stat().st_size>1024*1024:
                        # Emulator --log-level bounds typical output; truncate
                        # the same inode so inherited descriptors stay valid.
                        with p.open('r+b') as log: log.truncate(0)
                await asyncio.sleep(.5)
        self.owned['log_tasks']=[asyncio.create_task(rotate())]
        async def watch():
            code=await process.wait()
            self.event('owned-child-exited',pid=process.pid,returncode=code)
            if self.owned and self.owned['process'] is process:
                self.status='UNAVAILABLE'
                if self.wire: await self.wire.close()
                self.cursors.clear();self.artifacts.clear()
        self.watcher=asyncio.create_task(watch())
        start=time.monotonic()
        try:
            while time.monotonic()-start<15:
                if process.returncode is not None:
                    raise AdapterError('child_exited','Emulator exited during startup',returncode=process.returncode)
                try: verified=await asyncio.wait_for(listener_owned(process.pid,port),max(.001,15-(time.monotonic()-start)))
                except (OSError,PermissionError,asyncio.TimeoutError) as exc:
                    raise AdapterError('launch_ownership_unverified',str(exc))
                if verified:
                    result=await asyncio.wait_for(self.handshake(port,cfg['product']),max(.001,15-(time.monotonic()-start)))
                    if process.returncode is not None or not await asyncio.wait_for(listener_owned(process.pid,port),max(.001,15-(time.monotonic()-start))):
                        raise AdapterError('launch_ownership_unverified','Child listener ownership changed during handshake')
                    return result
                await asyncio.sleep(min(.1,max(.001,15-(time.monotonic()-start))))
            raise AdapterError('launch_ownership_unverified','Cannot verify listener belongs to launched child; launch independently and attach')
        except BaseException as exc:
            self.status='UNAVAILABLE'
            try: await self.stop_owned(token)
            except Exception: pass
            details=dict(ownership=token,ownership_retained=bool(self.owned),pid=process.pid,logs=logs)
            if isinstance(exc,asyncio.TimeoutError):
                raise AdapterError('launch_deadline','Startup exceeded its 15-second deadline',effect='possible',**details) from exc
            if isinstance(exc,AdapterError):
                exc.data.update(details)
            raise

    async def close_connection(self):
        if self.wire:
            await self.wire.close();self.wire=None
        self.cursors.clear();self.artifacts.clear()
        self.connection=None

    async def detach(self,keep_running=False):
        if self.owned:
            if not keep_running: raise AdapterError('ownership','Owned detach requires keep_running=true; otherwise stop_owned')
            return await self.abandon_owned(self.owned['ownership'])
        await self.close_connection();self.status='DISCONNECTED'
        return self.info()

    def owned_record(self,token):
        if not self.owned or token!=self.owned['ownership']:
            raise AdapterError('ownership','Token does not identify a child owned by this adapter')
        return self.owned

    async def stop_owned(self,token):
        record=self.owned_record(token); process=record['process']
        await self.close_connection()
        if process.returncode is None:
            try: process.terminate()
            except OSError:
                self.status='UNAVAILABLE'
                raise
            try: await asyncio.wait_for(process.wait(),3)
            except asyncio.TimeoutError:
                process.kill();await asyncio.wait_for(process.wait(),3)
        await asyncio.gather(*record['log_tasks'],return_exceptions=True)
        result={k:v for k,v in record.items() if k not in ('process','log_tasks')}
        self.owned=None;self.status='DISCONNECTED'
        return result

    async def abandon_owned(self,token):
        record=self.owned_record(token)
        result={k:v for k,v in record.items() if k not in ('process','log_tasks')}
        await self.close_connection()
        for task in record['log_tasks']:task.cancel()
        await asyncio.gather(*record['log_tasks'],return_exceptions=True)
        self.owned=None;self.status='DISCONNECTED'
        if self.watcher:
            self.watcher.cancel()
            await asyncio.gather(self.watcher,return_exceptions=True)
        return result

    async def shutdown(self):
        if self.owned:
            if self.owned['keep_on_exit']: await self.abandon_owned(self.owned['ownership'])
            else: await self.stop_owned(self.owned['ownership'])
        else: await self.close_connection()
