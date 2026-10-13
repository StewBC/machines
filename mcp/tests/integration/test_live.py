"""Real emulator acceptance. Set MACHINES_MCP_LIVE=1 after building both products."""
import asyncio
import os
from pathlib import Path
import socket
import pytest
from machines_mcp.config import Config
from machines_mcp.server import Adapter
from machines_mcp.errors import AdapterError

ROOT=Path(__file__).resolve().parents[3]
pytestmark=pytest.mark.skipif(os.environ.get('MACHINES_MCP_LIVE')!='1',reason='Set MACHINES_MCP_LIVE=1 for built emulator integration')

def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]

@pytest.mark.parametrize('product,exe',[('apple2','a2m'),('c64','c64m')])
async def test_live_tools(product,exe,tmp_path):
    artifacts=tmp_path/'artifacts';artifacts.mkdir()
    cwd=ROOT
    if product=='c64':
        # Deterministic repo-owned synthetic ROM keeps IRQs disabled. Real
        # BASIC/KERNAL boot IRQs can divert a freshly auto-run fixture.
        system=bytearray([0xEA]*16384)
        system[8192:8195]=bytes([0x4C,0,0xE0])
        system[8192+0x1FFC:8192+0x1FFE]=bytes([0,0xE0])
        (tmp_path/'system.rom').write_bytes(system)
        (tmp_path/'character.rom').write_bytes(bytes(4096))
        cwd=tmp_path
    ini=None
    if product=='c64':
        ini=tmp_path/'fixture.ini'
        ini.write_text('[roms]\nsingle_system=true\nsystem='+str(tmp_path/'system.rom')+'\ncharacter='+str(tmp_path/'character.rom')+'\n')
    c=Config(read_roots=[ROOT,tmp_path],write_roots=[tmp_path],artifact_dir=artifacts,profiles={product:dict(product=product,executable=str(ROOT/'build'/exe),cwd=str(cwd),port=port(),headless=True,history_memory=16,inspector=True,inspector_memory=16,**({'ini':str(ini)} if ini else {}))})
    a=Adapter(c)
    calls=[]
    async def call(tool,**args):
        if tool not in ('launch','stop_owned','attach','abandon_owned'):args=dict(connection=a.life.connection,**args)
        result,image=await a.call(tool,args);calls.append(tool)
        return result['data'],image,result['outcome']
    try:
        await call('launch',profile=product)
        await call('execute',action='pause');await call('wait',condition='paused')
        await call('health_check')
        cpu,_,_=await call('get_cpu');assert isinstance(cpu['pc'],int)
        source=tmp_path/'tiny program ü.asm'
        source.write_text('start:\n lda #$06\n sta $c000\n jmp start\n')
        data,_,outcome=await call(product+'_assemble',path=str(source),address=32768,auto_run=False,reset=False)
        assert outcome=='completed'
        await asyncio.sleep(.05) # native durable symbol publication is a later main-loop poll
        try: await call('find_symbol',name='start')
        except AdapterError as exc:
            assert exc.data.get('wire_code')=='not-ready' # native headless symbols may not be published
        await call('write_memory',address=49152,bytes_hex='42',source='main' if product=='apple2' else 'ram')
        memory,_,_=await call('read_memory',address=49152,length=1,source='main' if product=='apple2' else 'ram')
        assert memory['bytes_hex']=='42'
        await call('export_memory',address=32768,length=8,source='map',path=str(tmp_path/'memory.bin'))
        bp,_,_=await call('breakpoints',action='create',definition=dict(access='write',address=49152,conditions=[dict(lhs='value',op='eq',value=6)]))
        bid=bp['breakpoints'][0]['id']
        await call('breakpoints',action='enable',id=bid,enabled=False)
        await call('breakpoints',action='update',id=bid,definition=dict(access='write',address=49152,conditions=[dict(lhs='value',op='eq',value=6)]))
        if product=='c64':
            # C64 native auto-run changes PC but retains CPU microcycle phase.
            # Finish the paused instruction before choosing a new start PC.
            await call('execute',action='step_instruction')
            await call('wait',condition='event',event='step-complete',timeout_ms=5000)
        await call(product+'_assemble',path=str(source),address=32768,auto_run=True,reset=False)
        try: await call('wait',condition='paused',timeout_ms=5000)
        except AdapterError:
            print('FAILED GUARD',await call('read_memory',address=57344,length=8,source='rom'),await call('get_state'),await call('get_cpu'),await call('read_memory',address=32768,length=8,source='ram' if product=='c64' else 'main'),await call('breakpoints',action='list'))
            raise
        state,_,_=await call('get_state');assert state['stop']=='breakpoint'
        cpu,_,_=await call('get_cpu');assert cpu['a']==6
        listing,_,_=await call('breakpoints',action='list');assert listing['breakpoints'][0]['hits']>=1
        await call('breakpoints',action='rearm')
        await call('breakpoints',action='delete',id=bid)
        await call('breakpoints',action='clear_all')
        if product=='apple2':
            await call('apple2_set_register',name='pc',value=32768)
            await call('apple2_softswitches');await call('apple2_key',value=13)
            await call('apple2_set_turbo',mhz=1)
        else:
            await call('c64_vic');await call('c64_cia',index=1);await call('c64_cia',index=2)
            await call('c64_drive_cpu',device=8);await call('c64_disk_status',device=8);await call('c64_call_stack')
            await call('c64_key',action='down',key='a');await call('c64_key',action='up',key='a');await call('c64_key',action='restore')
            await call('c64_joystick',port=1,fire=True);await call('c64_paste',kind='text',text='HELLO\n')
            await call('c64_set_turbo',mode='normal')
        statefile=tmp_path/'state snapshot.bin'
        await call('snapshot',action='save',path=str(statefile))
        if product=='c64': await call('wait',condition='event',event='save-state-complete',timeout_ms=5000)
        assert statefile.exists()
        await call('snapshot',action='load',path=str(statefile))
        if product=='c64': await call('wait',condition='event',event='load-state-complete',timeout_ms=5000)
        await call('execute',action='pause');await call('wait',condition='paused')
        if product=='apple2':
            disk=tmp_path/'blank disk.dsk';disk.write_bytes(bytes(143360))
            await call('apple2_media',action='mount',kind='diskii',path=str(disk))
            await call('apple2_media',action='set_writable',writable=False)
            await call('apple2_media',action='select',index=1)
            await call('apple2_media',action='unmount',kind='diskii')
            await call('apple2_media',action='mount',kind='smartport',path=str(tmp_path))
            await call('apple2_media',action='unmount',kind='smartport')
        else:
            disk=tmp_path/'blank disk.d64';disk.write_bytes(bytes(174848))
            await call('c64_media',action='power',device=8,powered=True)
            await call('c64_media',action='mount',device=8,path=str(disk))
            await call('c64_disk_status',device=8)
            await call('c64_media',action='unmount',device=8)
            await call('c64_media',action='power',device=8,powered=False)
            binary=tmp_path/'binary file.bin';binary.write_bytes(bytes([0xEA,0xEA,0x4C,0,0x80]))
            await call('c64_binary_file',action='load',path=str(binary),address=32768,use_file_address=False,reset_first=False,is_basic=False)
            await call('c64_binary_file',action='save',path=str(tmp_path/'saved.bin'),start=32768,end=32772,write_file_address=False,is_basic=False)
            prg=tmp_path/'program.prg';prg.write_bytes(bytes([0,0x80,0xEA,0x4C,0,0x80]))
            await call('c64_load_program',path=str(prg))
            await call('execute',action='pause');await call('wait',condition='paused')
        await call('history_record',enabled=True)
        await call('execute',action='step_instruction');await asyncio.sleep(.1);await call('execute',action='pause');await call('wait',condition='paused')
        page,_,_=await call('history_find',limit=2)
        assert 'records' in page
        if page.get('cursor'):
            token=page['cursor'];await call('history_next',cursor=token,limit=2);await call('history_close',cursor=token)
        if page['records']:await call('history_read',id=page['records'][0]['id'],epoch=page['epoch'],before=0,after=0)
        await call('frame_ring',action='record',enabled=True)
        await call('execute',action='run');await call('wait',condition='frame',delta=2,timeout_ms=5000)
        await call('execute',action='pause');await call('wait',condition='paused')
        ring,_,_=await call('frame_ring',action='info')
        assert int(ring['count'])>0
        _,png,_=await call('get_frame',path=str(tmp_path/'screen.png'))
        assert png.startswith(b'\x89PNG')
        _,png,_=await call('recorded_frame',target=dict(frame=ring['newest_frame']))
        assert png.startswith(b'\x89PNG')
        await call('inspector',action='enter')
        state,_,_=await call('get_state');assert state['mode']=='inspector'
        # Detaching/re-attaching must preserve Inspector mode and child liveness.
        child=a.life.owned['process']
        pid=a.life.owned['pid'];control_port=a.life.owned['port']
        record,_,_=await call('abandon_owned',ownership=a.life.owned['ownership'])
        try:
            await call('attach',port=control_port,expected_product=product)
            state,_,_=await call('get_state');assert state['mode']=='inspector'
            await call('inspector',action='leave')
            state,_,_=await call('get_state');assert state['state']=='paused'
            await call('history_clear');await call('frame_ring',action='clear')
            await call('events');await call('detach')
            os.kill(pid,0)
        finally:
            os.kill(pid,15)
            try: await asyncio.wait_for(child.wait(),3)
            except asyncio.TimeoutError:
                child.kill();await asyncio.wait_for(child.wait(),3)
    finally:
        await a.life.shutdown()
