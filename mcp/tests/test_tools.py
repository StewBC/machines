import importlib.util
from pathlib import Path
import pytest
from jsonschema import ValidationError, Draft202012Validator
from machines_mcp.schemas.tools import TOOLS, validate_tool
from machines_mcp.backends.apple2 import dispatch, parse_response
from machines_mcp.schemas.protocol_manifest import MANIFEST, delivery

C={'connection':'test'}
def call(product,name,**args): return dispatch(product,name,{**C,**args})

def test_source_manifest():
    path=Path(__file__).resolve().parents[1]/'tools/check_protocol_manifest.py'
    spec=importlib.util.spec_from_file_location('manifest_check',path)
    module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    assert module.check()=={'apple2':59,'c64':80}


def test_all_schemas_valid_and_closed():
    for name,tool in TOOLS.items():
        Draft202012Validator.check_schema(tool['inputSchema'])
        branches=tool['inputSchema'].get('oneOf',[tool['inputSchema']])
        for branch in branches: assert branch['additionalProperties'] is False

@pytest.mark.parametrize('name,args',[
    ('execute',{'action':'run','count':'2'}),
    ('wait',{'condition':'paused','event':'frame'}),
    ('c64_key',{'action':'restore','key':'a'}),
    ('snapshot',{'action':'load','path':'/tmp/a','overwrite':False}),
    ('breakpoints',{'action':'list','id':1}),
    ('recorded_frame',{'target':{'frame':'1','cycle':'1'}}),
])
def test_tagged_fields_rejected(name,args):
    with pytest.raises(ValidationError): validate_tool(name,{**C,**args})


def test_memory_limits_and_product_sources():
    with pytest.raises(ValueError): call('apple2','read_memory',address=65535,length=2)
    with pytest.raises(ValueError): call('apple2','read_memory',address=0,source='ram')
    with pytest.raises(ValueError): call('c64','write_memory',address=0,bytes_hex='FF',source='drive8')
    cmd=call('apple2','write_memory',address=4096,bytes_hex='000AFF',source='aux')
    assert (cmd.verb,cmd.args,cmd.payload,cmd.outcome)==('set-memory','$1000 3 aux',b'\x00\x0a\xff','accepted')
    assert call('c64','write_memory',address=4096,bytes_hex='FF').outcome=='completed'


def test_breakpoint_complete_definition():
    d={'access':'read_write','address':4096,'end':4100,'actions':['break','swap'],'counter':0,'reset':2,'swap_slot':7,'mapping':{'ram':'aux','d000':'lc1'},'conditions':[{'lhs':'mem','address':8192,'op':'mask_clear','value':1},{'lhs':'cycle_in_line','op':'ge','value':4}]}
    cmd=call('apple2','breakpoints',action='update',id=3,definition=d)
    assert cmd.args=='3 read-write $1000 enabled=1 end=$1004 actions=break,swap ram=aux d000=lc1 counter=0 reset=2 swap-slot=7 when=mem($2000)!&$0001,cycle_in_line>=$0004'
    with pytest.raises(ValueError): call('c64','breakpoints',action='create',definition=d)
    with pytest.raises(ValueError): call('apple2','breakpoints',action='create',definition={'access':'exec','address':0,'conditions':[{'lhs':'value','op':'eq','value':2}]})


def test_history_query_and_counters():
    cmd=call('c64','history_find',pc={'first':32768,'last':32769},cycle={'first':'9007199254740993','last':'9007199254740994'},value='?F',opcodes=['A9','??'],**{'from':{'kind':'id','id':'17'}})
    assert cmd.args=='pc=$8000-$8001 from=17 cycle=9007199254740993-9007199254740994 value=$?F opcodes=A9,?? limit=32'
    assert cmd.deadline==11
    with pytest.raises(ValueError): call('c64','history_read',id=str(2**64))
    with pytest.raises(ValueError): call('c64','history_find',pc={'first':2,'last':1})


def test_paths_unicode_spaces_and_framing():
    cmd=call('c64','c64_binary_file',action='load',path='/tmp/a spaced ü.bin',address=4096,use_file_address=False,reset_first=True,is_basic=False)
    assert cmd.args=='/tmp/a spaced ü.bin $1000 0 1 0'
    with pytest.raises(ValidationError): call('apple2','snapshot',action='load',path='/tmp/a\nrun')


def test_media_slot_zero_and_paste_binary():
    assert call('apple2','apple2_media',action='mount',kind='smartport',drive=1,path='/tmp/disk.po').args=='kind=smartport 1 /tmp/disk.po'
    assert call('apple2','apple2_media',action='mount',kind='diskii',slot=6,drive=0,path='/tmp/disk.nib').args=='kind=diskii 6 0 /tmp/disk.nib'
    cmd=call('c64','c64_paste',kind='text',text='ü\nRUN\r')
    assert cmd.payload=='ü\nRUN\r'.encode() and cmd.args=='7'


def test_deadline_and_honest_delivery():
    cmd=call('apple2','wait',condition='paused',timeout_ms=30000)
    assert cmd.deadline==31 and cmd.outcome=='observed'
    assert call('apple2','snapshot',action='save',path='/tmp/s').deadline==6
    assert call('c64','snapshot',action='save',path='/tmp/s').outcome=='accepted'
    assert call('apple2','execute',action='pause').outcome=='accepted'
    for p,m in MANIFEST.items():
        for v in m['verbs']: assert delivery(p,v) in {'immediate_observation','runtime_completed_reply','queued_acceptance','wait'}


def test_response_bases_no_precision_loss():
    value=parse_response('apple2','get-cpu','pc=1000 a=10 x=FF y=00 sp=FE p=20 cycles=9007199254740993')
    assert value['pc']==4096 and value['a']==16 and value['cycles']=='9007199254740993'


def example(schema):
    if 'oneOf' in schema: return example(schema['oneOf'][0])
    if 'const' in schema: return schema['const']
    if 'default' in schema: return schema['default']
    if 'enum' in schema: return schema['enum'][0]
    if schema.get('type')=='object': return {k:example(schema['properties'][k]) for k in schema.get('required',[])}
    if schema.get('type')=='integer': return schema.get('minimum',0)
    if schema.get('type')=='number': return schema.get('minimum',1)
    if schema.get('type')=='boolean': return False
    if schema.get('type')=='array': return [example(schema['items']) for _ in range(schema.get('minItems',0))]
    if schema.get('description','').startswith('Unsigned'): return '1'
    if schema.get('description','').startswith('Absolute'): return '/tmp/test-file'
    if '{2}' in schema.get('pattern',''): return 'FF'
    if '1024' in schema.get('pattern',''): return '00'
    return 'test'


def test_every_backend_tool_branch_dispatches():
    local={'attach','launch','connection_info','stop_owned','abandon_owned','detach','events','result_page','disassemble','history_export','c64_debug_memory_page'}
    for name,spec in TOOLS.items():
        if name in local: continue
        branches=spec['inputSchema'].get('oneOf',[spec['inputSchema']])
        for branch in branches:
            args=example(branch)
            for product in [spec['product']] if spec['product'] else ['apple2','c64']:
                # Generic frame wait/event uses the shared first vocabulary item.
                if name in {'history_next','history_close'}: args['cursor']='1'
                command=dispatch(product,name,args)
                assert command.verb in MANIFEST[product]['verbs'],(name,product,command)
                assert isinstance(command.args,str)
                assert command.deadline>=3


def test_counter_range_overflow_and_mli_reset_guard():
    with pytest.raises(ValueError): call('apple2','history_find',cycle={'first':'0','last':str(2**64)})
    with pytest.raises(ValueError): call('apple2','apple2_assemble',path='/tmp/source.s',mli_launch=True)
    assert 'reset=0' in call('apple2','apple2_assemble',path='/tmp/source.s',mli_launch=True,reset=False).args


def test_hardware_native_bases_and_compound_fields():
    vic=parse_response('c64','get-vic','standard=PAL raster=123 cycle=14 d011=1B d016=08 mem=14 irq_st=81 irq_en=01 vc=0100 vcbase=1000 frame=9007199254740993')
    assert vic['cycle']==14 and vic['mem']==20 and vic['d011']==27 and vic['vcbase']==4096
    assert vic['frame']=='9007199254740993'
    cia=parse_response('c64','get-cia','cia=1 pra=10 prb=FF ddra=08 ta=FFFF/1000 cra=11 tb=0ABC/0001 crb=08 icr_flags=01 icr_mask=10 irq=0 tod=00:00:00.00 alarm=00:00:00.00')
    assert cia['pra']==16 and cia['icr_mask']==16 and cia['ta']=={'counter':65535,'latch':4096}
    stack=parse_response('c64','get-call-stack','depth=1 frame0=1000:2000')
    assert stack['entries']==[{'index':0,'jsr_address':4096,'destination_address':8192}]
    disk=parse_response('c64','get-disk-status','device=8 name=a disk with spaces title=MY DISK')
    assert disk['name']=='a disk with spaces' and disk['title']=='MY DISK'
    history=parse_response('apple2','history-info','records=9007199254740993 requested_bytes=9999 oldest=123 newest=124 partial=1')
    assert history['records']=='9007199254740993' and history['oldest']=='123'
