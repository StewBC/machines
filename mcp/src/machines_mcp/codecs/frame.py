"""PNG conversion audited against tools/apple2/a2m_control_client.py's
write_png algorithm and both native control frame formatters. Wire ARGB8888
is little-endian BGRA; C64 retains a 520-pixel row pitch for PAL frames.
"""
import struct
import zlib
from ._common import metadata, integer, product_name


def _chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def decode_frame(product: str, text: str, payload: bytes) -> tuple[dict, bytes]:
    product = product_name(product)
    meta = metadata(text)
    width, height, stride = (integer(meta, k, 4096) for k in ('width', 'height', 'stride'))
    expected = {(560, 192, 2240)} if product == 'apple2' else {(504, 312, 2080), (520, 263, 2080)}
    if (width, height, stride) not in expected or meta.get('format') != 'argb8888':
        raise ValueError('unsupported frame dimensions, stride or format')
    if len(payload) != stride * height:
        raise ValueError('frame payload size mismatch')
    for key in ('width', 'height', 'stride', 'frame', 'cycle', 'target'):
        if key in meta:
            meta[key] = integer(meta, key)
            if key in ('frame', 'cycle', 'target'):
                meta[key] = str(meta[key])
    if 'target_kind' in meta and meta['target_kind'] not in ('frame', 'cycle'):
        raise ValueError('invalid frame target kind')
    meta['completeness'] = 'completed_ring_sample' if 'target_kind' in meta else 'unknown_or_partial'
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        row = payload[y * stride:y * stride + width * 4]
        for x in range(0, len(row), 4):
            rows.extend((row[x + 2], row[x + 1], row[x]))
    png = b'\x89PNG\r\n\x1a\n' + _chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += _chunk(b'IDAT', zlib.compress(rows)) + _chunk(b'IEND', b'')
    return meta, png
