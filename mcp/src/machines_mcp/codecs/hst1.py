"""HST1 decoder adapted from tools/apple2/a2m_control_client.py Ctl.decode_hst1,
checked against src/shell/runtime/runtime_history_wire.c. Adds JSON-safe
operands, explicit limits, unknown-flag rejection and response cross-checks.
"""
import struct
from ._common import metadata, integer

RECORD_NAMES = ("instruction", "irq", "nmi", "marker")
ACCESS_NAMES = ("data_read", "data_write", "opcode_fetch", "operand_read", "dummy_read", "rmw_dummy_write", "stack_read", "stack_write", "vector_read")
MARKER_NAMES = ("none", "recorder_start", "recorder_stop", "recorder_resume", "reset_complete", "state_load", "program_inject", "crt_attach", "assemble", "direct_memory_write", "kernal_load_trap", "kernal_save_trap", "clock_discontinuity", "media_changed")


def decode_hst1(text: str, payload: bytes) -> dict:
    meta = metadata(text)
    for key in ('epoch', 'count', 'cursor', 'more', 'oldest', 'newest'):
        meta[key] = integer(meta, key)
    if meta['count'] > 129 or meta['more'] > 1:
        raise ValueError('history page exceeds adapter bounds')
    if len(payload) < 24 or len(payload) > 24 + 129 * (48 + 67 * 8) or payload[:4] != b'HST1':
        raise ValueError('invalid HST1 header or size')
    version, flags, epoch, count, reserved = struct.unpack_from('<HHQII', payload, 4)
    if version != 1 or flags or reserved or epoch != meta['epoch'] or count != meta['count']:
        raise ValueError('invalid HST1 header or metadata mismatch')
    offset = 24
    records = []
    for _ in range(count):
        if offset + 48 > len(payload):
            raise ValueError('truncated HST1 record header')
        size, kind, flags, timeline, record_id, cycle, pc = struct.unpack_from('<HBBIQQH', payload, offset)
        accesses = payload[offset + 35]
        marker, reserved, arg0, arg1 = struct.unpack_from('<HHII', payload, offset + 36)
        length = payload[offset + 34]
        if kind > 3 or flags & ~15 or accesses > 67 or reserved or size != 48 + accesses * 8 or offset + size > len(payload):
            raise ValueError('invalid HST1 record')
        if length > 3 or marker > 13:
            raise ValueError('invalid HST1 instruction length or marker')
        rec = dict(kind=kind, kind_name=RECORD_NAMES[kind], timeline=timeline, id=str(record_id), machine_cycle=str(cycle), pc=pc,
                   partial=bool(flags & 1), access_truncated=bool(flags & 2),
                   anchor_match=bool(flags & 4), timing_truncated=bool(flags & 8),
                   instruction_length=length, marker_kind=marker, marker_kind_name=MARKER_NAMES[marker], marker_arg0=arg0, marker_arg1=arg1,
                   operands=list(payload[offset + 32:offset + 34]), accesses=[])
        for i, key in enumerate(('a', 'x', 'y', 'sp', 'p', 'opcode')):
            rec[key] = payload[offset + 26 + i]
        for position in range(offset + 48, offset + size, 8):
            address, cycle_offset, value, access_kind, reserved = struct.unpack_from('<HHBBH', payload, position)
            if access_kind > 8 or reserved:
                raise ValueError('invalid HST1 access')
            rec['accesses'].append(dict(address=address, cycle_offset=cycle_offset, value=value, kind=access_kind, kind_name=ACCESS_NAMES[access_kind]))
        records.append(rec)
        offset += size
    if offset != len(payload):
        raise ValueError('trailing HST1 bytes')
    for key in ('epoch', 'cursor', 'oldest', 'newest'):
        meta[key] = str(meta[key])
    meta['records'] = records
    return meta
