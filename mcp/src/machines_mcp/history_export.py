"""Stream a verified retained HST1 window without collecting the trace in RAM.

This is a multi-response scan, not a native atomic snapshot. The caller must
already have paused live execution. Native epoch/bounds and record continuity
are checked before publication; UI or other mutations never cause silent retry.
"""
import json
import time
from .backends.apple2 import parse_response
from .codecs.hst1 import decode_hst1
from .errors import AdapterError


def _line(value):
    return (json.dumps(value, ensure_ascii=False, separators=(',', ':')) + '\n').encode('utf-8')


def _bounds(info):
    try:
        if int(info.get('available', 0)) != 1:
            raise AdapterError('history_unavailable', 'Native history recording storage is unavailable')
        return tuple(int(info[k]) for k in ('epoch', 'oldest', 'newest', 'records'))
    except (KeyError, TypeError, ValueError) as exc:
        raise AdapterError('invalid_response', 'Invalid native history status') from exc


async def export_history(life, artifacts, args):
    if life.cursors:
        raise AdapterError('history_cursor_active', 'Close the outstanding history query before exporting; the native session has one cursor')
    deadline = time.monotonic() + args.get('timeout_ms', 30000) / 1000
    cursor = None

    async def request(verb, text='', budget=3):
        if verb in getattr(life, 'disabled_verbs', set()):
            raise AdapterError('unsupported_feature', 'Connected emulator lacks required history-export capability')
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AdapterError('export_deadline', 'Full export exceeded timeout; no partial file published')
        try:
            return await life.wire.request(verb, text, deadline=min(budget, remaining))
        except AdapterError as exc:
            code = exc.data.get('wire_code', '').lower().replace('-', '_')
            if 'stale' in code or 'epoch' in code or 'stale' in exc.data['message']:
                raise AdapterError('export_changed', 'History changed during export; no partial file published', wire_code=code) from exc
            raise

    async def state():
        reply = await request('get-state')
        result = parse_response(life.product, 'get-state', reply.text)
        if result.get('state') != 'paused' or result.get('mode', 'live') != 'live':
            raise AdapterError('export_requires_paused_live', 'Pause live execution explicitly before exporting; adapter will not pause or leave Inspector')
        return result

    await state()
    start_reply = await request('history-info')
    start_info = parse_response(life.product, 'history-info', start_reply.text)
    epoch, oldest, newest, expected = _bounds(start_info)
    if expected < 0 or oldest < 0 or newest < 0 or ((expected == 0) != (oldest == newest == 0)):
        raise AdapterError('invalid_response', 'Inconsistent native retained-history bounds')
    if expected and newest - oldest + 1 != expected:
        raise AdapterError('invalid_response', 'Native retained record IDs are not contiguous')
    count = 0
    last_id = oldest - 1 if expected else 0
    pages = 0
    # Exclusively use the native session cursor for the duration of this scan.
    with artifacts.stream('jsonl', path=args.get('path'), overwrite=args.get('overwrite', False), max_bytes=args.get('max_bytes')) as output:
        output.write(_line(dict(type='header', format='machines-history-jsonl', version=1,
                               product=life.product, protocol=life.protocol, connection=life.connection,
                               scope='all_retained_records_at_start', consistency='verified_retained_window',
                               epoch=str(epoch), oldest=str(oldest), newest=str(newest), records=str(expected))))
        try:
            if expected:
                reply = await request('history-find', f'direction=forward from=oldest epoch={epoch} limit=64', 11)
                while True:
                    if reply.kind != 'data' or reply.data_type != 'history':
                        raise AdapterError('invalid_response', 'Expected HST1 history data')
                    try:
                        page = decode_hst1(reply.text, reply.payload)
                    except ValueError as exc:
                        raise AdapterError('invalid_response', str(exc)) from exc
                    cursor = page['cursor'] if page['cursor'] != '0' else None
                    if int(page['epoch']) != epoch:
                        raise AdapterError('export_changed', 'Native epoch changed during export')
                    if page['count'] > 64 or not page['records']:
                        raise AdapterError('invalid_response', 'Empty or oversized nonempty history export page')
                    if int(page['oldest']) != int(page['records'][0]['id']) or int(page['newest']) != int(page['records'][-1]['id']):
                        raise AdapterError('invalid_response', 'Native history page metadata does not match its record bounds')
                    for record in page['records']:
                        rid = int(record['id'])
                        if rid != last_id + 1 or rid > newest:
                            raise AdapterError('export_changed', 'Retained record continuity changed during export')
                        output.write(_line(dict(type='record', **record)))
                        last_id = rid
                        count += 1
                    pages += 1
                    if not page['more']:
                        break
                    if cursor is None:
                        raise AdapterError('invalid_response', 'History continuation missing native cursor')
                    reply = await request('history-next', f'{cursor} limit=64')
            end_reply = await request('history-info')
            end_info = parse_response(life.product, 'history-info', end_reply.text)
            if _bounds(end_info) != (epoch, oldest, newest, expected) or count != expected or (expected and last_id != newest):
                raise AdapterError('export_changed', 'Retained history changed or was incomplete; no partial file published')
            await state()
            output.write(_line(dict(type='footer', complete=True, records=str(count), pages=pages)))
            result = dict(complete=True, records=str(count), pages=pages, epoch=str(epoch),
                          oldest=str(oldest), newest=str(newest), consistency='verified_retained_window',
                          sample_consistency='multiple_nonatomic', artifact=output.finish())
        finally:
            if cursor is not None and not life.wire.closed:
                # Cleanup has its own bounded budget even after the export limit.
                try:
                    await life.wire.request('history-close', str(cursor), deadline=3)
                except AdapterError as exc:
                    life.event('export-cursor-close-failed', **exc.data)
    return result
