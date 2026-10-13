import json
import struct
import zlib
import pytest
from machines_mcp.codecs.frame import decode_frame
from machines_mcp.codecs.hst1 import decode_hst1
from machines_mcp.codecs.breakpoints import decode_breakpoints


def png_rows(png):
    offset, compressed = 8, b''
    while offset < len(png):
        length = struct.unpack_from('>I', png, offset)[0]
        kind, data = png[offset + 4:offset + 8], png[offset + 8:offset + 8 + length]
        assert struct.unpack_from('>I', png, offset + 8 + length)[0] == zlib.crc32(kind + data)
        if kind == b'IDAT':
            compressed += data
        offset += 12 + length
    return zlib.decompress(compressed)


@pytest.mark.parametrize('product,width,height,stride', [('apple2',560,192,2240),('c64',504,312,2080),('c64',520,263,2080)])
def test_frame_colors_and_stride(product,width,height,stride):
    payload = bytearray(stride * height)
    payload[:12] = bytes((0,0,255,255,0,255,0,255,255,0,0,255))
    payload[stride:stride+4] = bytes((9,8,7,255))
    text = f'width={width} height={height} stride={stride} format=argb8888 frame=3'
    meta, png = decode_frame(product,text,payload)
    rows = png_rows(png)
    assert rows[:10] == bytes((0,255,0,0,0,255,0,0,0,255))
    assert rows[width*3+1:width*3+5] == bytes((0,7,8,9))
    assert meta['completeness'] == 'unknown_or_partial'
    with pytest.raises(ValueError):
        decode_frame(product,text,payload[:-1])


def history_fixture():
    header = b'HST1' + struct.pack('<HHQII',1,0,123,1,0)
    record = bytearray(56)
    struct.pack_into('<HBBIQQH',record,0,56,0,15,4,456,789,0x1234)
    record[26:36] = bytes((1,2,3,4,5,0xA9,0x11,0,2,1))
    struct.pack_into('<HHBBH',record,48,0x4321,7,0xFF,1,0)
    return header + record


def test_hst1_decode_and_every_truncation():
    payload = history_fixture()
    text = 'epoch=123 count=1 cursor=2 more=0 oldest=1 newest=456'
    decoded = decode_hst1(text,payload)
    rec = decoded['records'][0]
    assert rec['pc'] == 0x1234 and rec['operands'] == [0x11,0]
    assert rec['accesses'] == [dict(address=0x4321,cycle_offset=7,value=255,kind=1,kind_name="data_write")]
    json.dumps(decoded)
    for n in range(len(payload)):
        with pytest.raises(ValueError):
            decode_hst1(text,payload[:n])
    for position, value in ((27,16),(59,68),(77,9),(62,1)):
        corrupt = bytearray(payload)
        corrupt[position] = value
        with pytest.raises(ValueError):
            decode_hst1(text,corrupt)
    with pytest.raises(ValueError):
        decode_hst1(text.replace('epoch=123','epoch=124'),payload)


@pytest.mark.parametrize('product,extra',[('apple2','ram=0 c100=0 d000=0 swap_slot=2 swap_param=-1 swap_relative=1'),('c64','mapping=0')])
def test_breakpoints_condition_and_hex(product,extra):
    line = f'id=1 enabled=1 start=ABCD end=FFFF has_end=1 access=1 {extra} actions=1 use_counter=0 hits=0 initial=0 reset=0 counter=0 cond=1 when=a == $12\n'
    result = decode_breakpoints(product,'count=1',line.encode())
    assert result['breakpoints'][0]['start'] == 0xABCD
    assert result['breakpoints'][0]['when'] == 'a == $12'
    with pytest.raises(ValueError):
        decode_breakpoints(product,'count=2',line.encode())
    with pytest.raises(ValueError):
        decode_breakpoints(product,'count=1',line[:-1].encode())


def test_hst1_matches_existing_client_decoder():
    import importlib.util
    from pathlib import Path
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location('audited_apple_client', root / 'tools/apple2/a2m_control_client.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    expected = module.Ctl.decode_hst1(history_fixture())
    for record in expected['records']:
        record['operands'] = list(record['operands'])
        record['id'] = str(record['id'])
        record['machine_cycle'] = str(record['machine_cycle'])
        record['kind_name'] = 'instruction'
        record['marker_kind_name'] = 'none'
        for access in record['accesses']:
            access['kind_name'] = 'data_write'
    actual = decode_hst1('epoch=123 count=1 cursor=2 more=0 oldest=1 newest=456', history_fixture())
    assert actual['epoch'] == str(expected['epoch'])
    assert actual['records'] == expected['records']


def test_hst1_marker_and_empty_page():
    payload = bytearray(history_fixture())
    payload[26] = 3
    struct.pack_into('<H',payload,60,13)
    record = decode_hst1('epoch=123 count=1 cursor=2 more=0 oldest=1 newest=456',payload)['records'][0]
    assert record['kind'] == 3 and record['marker_kind'] == 13
    empty = b'HST1' + struct.pack('<HHQII',1,0,1,0,0)
    assert decode_hst1('epoch=1 count=0 cursor=0 more=0 oldest=0 newest=0',empty)['records'] == []
