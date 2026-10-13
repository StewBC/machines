"""Decode product breakpoint listings emitted by control_breakpoint.c (Apple)
and control_format_breakpoints_response in c64/main.c. Conditions are retained
as native expressions, including spaces; no guessed reformatting is performed.
"""
from ._common import metadata, integer, product_name


def decode_breakpoints(product: str, text: str, payload: bytes) -> dict:
    product = product_name(product)
    count = integer(metadata(text), 'count', 64)
    if len(payload) > 64 * 384:
        raise ValueError('breakpoint listing too large')
    try:
        body = payload.decode('ascii')
    except UnicodeDecodeError as exc:
        raise ValueError('non-ASCII breakpoint listing') from exc
    if body and not body.endswith('\n'):
        raise ValueError('truncated breakpoint listing')
    entries = []
    common = {'id', 'enabled', 'start', 'end', 'has_end', 'access', 'actions', 'use_counter', 'hits', 'initial', 'reset', 'counter', 'cond'}
    specific = {'ram', 'c100', 'd000', 'swap_slot', 'swap_param', 'swap_relative'} if product == 'apple2' else {'mapping'}
    for line in body.splitlines():
        fixed, separator, condition = line.partition(' when=')
        data = metadata(fixed)
        if not separator or set(data) != common | specific:
            raise ValueError('invalid breakpoint fields')
        entry = {}
        for key, value in data.items():
            try:
                entry[key] = int(value, 16 if key in ('start', 'end') else 10)
            except ValueError as exc:
                raise ValueError('invalid breakpoint integer') from exc
            if key == 'swap_param':
                if not -(2**31) <= entry[key] < 2**31:
                    raise ValueError('invalid swap parameter')
            elif not 0 <= entry[key] <= (65535 if key in ('start', 'end') else 2**32 - 1):
                raise ValueError('breakpoint field out of range')
        for key in ('enabled', 'has_end', 'use_counter', 'swap_relative'):
            if key in entry and entry[key] not in (0, 1):
                raise ValueError('invalid breakpoint boolean')
        if entry['access'] & ~7 or not entry['access'] or entry['actions'] & ~103:
            raise ValueError('invalid breakpoint flags')
        if entry['cond'] > 4:
            raise ValueError('invalid condition count')
        limits = {'ram': 2, 'c100': 1, 'd000': 3, 'swap_slot': 7} if product == 'apple2' else {'mapping': 2}
        if any(entry[key] > maximum for key, maximum in limits.items()):
            raise ValueError('invalid breakpoint mapping or slot')
        if len(condition) > 127 or '\x00' in condition:
            raise ValueError('invalid breakpoint condition')
        entry['when'] = condition
        entries.append(entry)
    if len(entries) != count or len({e['id'] for e in entries}) != count:
        raise ValueError('breakpoint count or ID mismatch')
    return {'count': count, 'breakpoints': entries}
