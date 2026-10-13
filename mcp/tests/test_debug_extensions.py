import sys
import pytest
from machines_mcp.codecs.debug_memory import DebugSnapshot, DebugSnapshots
from machines_mcp.codecs.vic_ring import decode_vic_ring, HEX, LISTS, DECIMAL
from machines_mcp.errors import AdapterError
from machines_mcp.schemas.tools import validate_tool
from machines_mcp.backends.apple2 import dispatch

C={'connection':'c'}
def test_bulk_snapshot_layout_packed_pc_not_age():
    history=bytearray(65536*8)
    history[8:16]=(0x1000200030004000).to_bytes(8,sys.byteorder)
    payload=bytes([1])*65536+bytes([2])*65536+bytes([3])*65536+history
    snap=DebugSnapshot('generation=9007199254740993 map=65536 ram=65536 rom=65536 write_history=1',payload)
    window=snap.window(1,1)
    assert window['generation']=='9007199254740993'
    assert window['views']=={'map':'01','ram':'02','rom':'03'}
    assert window['writer_pcs_latest_first']==[[0x4000,0x3000,0x2000,0x1000]]
    assert window['write_history_packed']==[str(0x1000200030004000)]
    for text,body in [('generation=1 map=65535 ram=65536 rom=65536 write_history=1',payload),('generation=1 map=65536 ram=65536 rom=65536 write_history=0',payload),('generation=1 map=65536 ram=65536 rom=65536 write_history=1',payload[:-1])]:
        with pytest.raises(ValueError): DebugSnapshot(text,body)

def test_bulk_no_history_and_connection_cache():
    snap=DebugSnapshot('generation=2 map=65536 ram=65536 rom=65536 write_history=0',bytes(65536*3))
    assert 'writer_pcs_latest_first' not in snap.window(0,1)
    store=DebugSnapshots(); token=store.add('c',snap)
    assert store.get('c',token) is snap
    with pytest.raises(AdapterError): store.get('new',token)
    for _ in range(4): store.add('c',snap)
    with pytest.raises(AdapterError): store.get('c',token)
    with pytest.raises(ValueError): snap.window(65535,2)

def vic_line():
    fields={'frame':'9007199254740993','cycle':'9007199254740994',**{k:'1' for k in DECIMAL},**{k:'F'*width for k,width in HEX.items()},**{k:','.join(['A'*width]*n) for k,(n,width) in LISTS.items()},'irq':'81/01'}
    return ' '.join(f'{k}={v}' for k,v in fields.items())+'\n'

def test_vic_all_native_fields_and_strict_boundaries():
    line=vic_line()
    data=decode_vic_ring('count=1',line.encode())
    assert data['records'][0]['frame']=='9007199254740993'
    assert data['records'][0]['cycle']=='9007199254740994'
    assert data['records'][0]['spr_x']==[0xAAAA]*8
    assert data['records'][0]['irq']=={'status':129,'enable':1}
    assert decode_vic_ring('count=0',b'')['records']==[]
    for text,body in [('count=2',line.encode()),('count=1',line[:-1].encode()),('count=1',line.replace('spr_x=AAAA,','spr_x=').encode()),('count=2049',b''),('count=1',line.replace('d011=FF','d011=zz').encode())]:
        with pytest.raises(ValueError): decode_vic_ring(text,body)

def test_extension_serialization_and_admission():
    cmd=dispatch('c64','c64_vic_ring_find',{**C,'frame':'12','raster':{'first':0,'last':311},'limit':2048})
    assert cmd.args=='frame=12 raster=0-311 limit=2048'
    assert dispatch('c64','c64_debug_memory',{**C,'address':0}).args=='write-history=1'
    assert dispatch('c64','c64_vic_ring',{**C,'action':'record','enabled':True}).outcome=='accepted'
    for name,args in [('c64_debug_memory',{'address':65535,'length':2}),('disassemble',{'address':65535,'length':2}),('disassemble',{'address':0,'cpu_class':'65c02'})]:
        with pytest.raises(ValueError): validate_tool(name,{**C,**args},'c64')

@pytest.mark.asyncio
async def test_adapter_snapshot_pages_are_one_sample(tmp_path):
    from machines_mcp.server import Adapter
    from machines_mcp.config import Config
    from machines_mcp.wire import Reply
    adapter=Adapter(Config(write_roots=[tmp_path],artifact_dir=tmp_path))
    life=adapter.life
    life.connection='c'; life.product='c64'; life.protocol='C64M/10'
    class FakeWire:
        closed=False
        calls=[]
        async def request(self,verb,args='',payload=None,deadline=3):
            self.calls.append((verb,args))
            return Reply('data','generation=2 map=65536 ram=65536 rom=65536 write_history=0',bytes([7])*65536*3,'debug-memory')
    life.wire=FakeWire(); life.status='ATTACHED'
    result,_=await adapter.call('c64_debug_memory',{**C,'address':0,'length':2,'write_history':False,'path':str(tmp_path/'bulk.bin')})
    token=result['data']['snapshot_token']
    page,_=await adapter.call('c64_debug_memory_page',{**C,'token':token,'address':65535,'length':1})
    assert page['data']['generation']=='2'
    assert page['data']['views']=={'map':'07','ram':'07','rom':'07'}
    assert life.wire.calls==[('get-debug-memory','write-history=0')]
    assert (tmp_path/'bulk.bin').read_bytes()==bytes([7])*65536*3

@pytest.mark.asyncio
async def test_adapter_disassembly_auto_model_and_single_memory(tmp_path):
    from machines_mcp.server import Adapter
    from machines_mcp.config import Config
    from machines_mcp.wire import Reply
    adapter=Adapter(Config(write_roots=[tmp_path],artifact_dir=tmp_path))
    life=adapter.life; life.connection='c'; life.product='apple2'; life.protocol='A2M/16'
    class FakeWire:
        closed=False
        calls=[]
        async def request(self,verb,args='',payload=None,deadline=3):
            self.calls.append((verb,args))
            return Reply('ok','model=//e-enh frame=1') if verb=='get-softswitches' else Reply('data','cycle=9007199254740993',b'\x80\xFE','memory')
    life.wire=FakeWire(); life.status='ATTACHED'
    result,_=await adapter.call('disassemble',{**C,'address':4096,'length':2})
    assert result['data']['cpu_class']=='65c02'
    assert 'BRA' in result['data']['records'][0]['text']
    assert result['data']['native_memory_metadata']=='cycle=9007199254740993'
    assert life.wire.calls==[('get-softswitches',''),('get-memory','$1000 2 map')]
