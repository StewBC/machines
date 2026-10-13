import json
import struct
from pathlib import Path
from types import SimpleNamespace
import pytest
from machines_mcp.artifacts import Artifacts
from machines_mcp.config import Config
from machines_mcp.errors import AdapterError
from machines_mcp.history_export import export_history
from machines_mcp.wire import Reply


def page(ids, *, more=0, epoch=1, oldest=None, newest=None, cursor=None):
    cursor = (7 if more else 0) if cursor is None else cursor
    oldest = ids[0] if oldest is None and ids else (oldest or 0)
    newest = ids[-1] if newest is None and ids else (newest or 0)
    body = b'HST1' + struct.pack('<HHQII', 1, 0, epoch, len(ids), 0)
    for rid in ids:
        record = bytearray(48)
        struct.pack_into('<HBBIQQH', record, 0, 48, 0, 0, 1, rid, rid * 2, 0x8000)
        record[31], record[34] = 0xEA, 1
        body += record
    return Reply('data', f'epoch={epoch} count={len(ids)} cursor={cursor} more={more} oldest={oldest} newest={newest}', body, 'history')


class FakeWire:
    def __init__(self):
        self.closed = False
        self.calls = []
        self.state = 'state=paused mode=live'
        self.info = 'available=1 epoch=1 oldest=1 newest=3 records=3'
        self.end_info = self.info
        self.info_calls = 0
        self.find = page([1, 2], more=1)
        self.next = page([3])

    async def request(self, verb, text='', payload=None, deadline=3, **kwargs):
        self.calls.append((verb, text))
        if verb == 'get-state': return Reply('ok', self.state)
        if verb == 'history-info':
            self.info_calls += 1
            return Reply('ok', self.info if self.info_calls == 1 else self.end_info)
        if verb == 'history-find': return self.find
        if verb == 'history-next': return self.next
        if verb == 'history-close': return Reply('ok', '')
        raise AssertionError(verb)


@pytest.fixture
def setup(tmp_path):
    wire = FakeWire()
    events = []
    life = SimpleNamespace(wire=wire, product='c64', protocol='C64M/10', connection='test', cursors={}, event=lambda *a, **k: events.append((a, k)))
    artifacts = Artifacts(Config(write_roots=[tmp_path], artifact_dir=tmp_path))
    return life, artifacts, str(tmp_path / 'history.jsonl')


@pytest.mark.asyncio
async def test_complete_stream_export_order_hash_and_cursor_cleanup(setup):
    life, artifacts, path = setup
    result = await export_history(life, artifacts, dict(path=path))
    rows = [json.loads(x) for x in Path(path).read_text().splitlines()]
    assert rows[0]['scope'] == 'all_retained_records_at_start'
    assert [r['id'] for r in rows[1:-1]] == ['1', '2', '3']
    assert rows[-1]['complete'] and result['records'] == '3'
    assert result['sample_consistency'] == 'multiple_nonatomic'
    assert len(result['artifact']['sha256']) == 64
    assert not any(v=='history-close' for v,_ in life.wire.calls)  # native exhausted cursor was released
    assert not list(Path(path).parent.glob('.machines-mcp-*'))
    assert life.cursors == {}


@pytest.mark.asyncio
@pytest.mark.parametrize('state', ['state=running mode=live', 'state=paused mode=inspector'])
async def test_export_never_pauses_or_leaves_inspector(setup, state):
    life, artifacts, path = setup
    life.wire.state = state
    with pytest.raises(AdapterError) as caught:
        await export_history(life, artifacts, dict(path=path))
    assert caught.value.data['code'] == 'export_requires_paused_live'
    assert life.wire.calls == [('get-state', '')]
    assert not Path(path).exists()


@pytest.mark.asyncio
@pytest.mark.parametrize('kind', ['bounds', 'duplicate', 'epoch', 'missing'])
async def test_changed_export_is_not_published(setup, kind):
    life, artifacts, path = setup
    if kind == 'bounds': life.wire.end_info = 'available=1 epoch=1 oldest=1 newest=4 records=4'
    elif kind == 'duplicate': life.wire.next = page([2])
    elif kind == 'epoch': life.wire.next = page([3], epoch=2)
    else: life.wire.next = page([3], newest=4)
    Path(path).write_text('original')
    with pytest.raises(AdapterError) as caught:
        await export_history(life, artifacts, dict(path=path, overwrite=True))
    assert caught.value.data['code'] == ('invalid_response' if kind=='missing' else 'export_changed')
    assert Path(path).read_text() == 'original'
    assert life.wire.calls[-1][0] in ('history-close','history-info','history-next')
    assert not list(Path(path).parent.glob('.machines-mcp-*'))


@pytest.mark.asyncio
async def test_export_byte_limit_never_publishes_partial(setup):
    life, artifacts, path = setup
    with pytest.raises(AdapterError) as caught:
        await export_history(life, artifacts, dict(path=path, max_bytes=500))
    assert caught.value.data['code'] == 'export_byte_limit'
    assert not Path(path).exists()
    assert not list(Path(path).parent.glob('.machines-mcp-*'))


@pytest.mark.asyncio
async def test_empty_available_history_is_complete_export(setup):
    life, artifacts, path = setup
    life.wire.info = life.wire.end_info = 'available=1 epoch=1 oldest=0 newest=0 records=0'
    result = await export_history(life, artifacts, dict(path=path))
    assert result['records'] == '0' and result['complete']
    assert [json.loads(x)['type'] for x in Path(path).read_text().splitlines()] == ['header', 'footer']
    assert not any(v.startswith('history-') and v not in ('history-info',) for v, _ in life.wire.calls)


def test_stream_external_race_refuses_existing_and_aborts(tmp_path):
    artifacts = Artifacts(Config(write_roots=[tmp_path], artifact_dir=tmp_path))
    target = tmp_path / 'export.jsonl'
    with pytest.raises(AdapterError):
        with artifacts.stream('jsonl', path=str(target)) as stream:
            stream.write(b'new')
            target.write_text('other writer')
            stream.finish()
    assert target.read_text() == 'other writer'
    assert not list(tmp_path.glob('.machines-mcp-*'))


@pytest.mark.asyncio
async def test_existing_history_query_is_preserved(setup):
    life, artifacts, path = setup
    life.cursors = {'active': {'native': '7'}}
    with pytest.raises(AdapterError) as caught:
        await export_history(life, artifacts, dict(path=path))
    assert caught.value.data['code'] == 'history_cursor_active'
    assert life.wire.calls == [] and life.cursors == {'active': {'native': '7'}}
    assert not Path(path).exists()


@pytest.mark.asyncio
async def test_adapter_retires_exhausted_and_replaced_native_cursors(setup):
    from machines_mcp.server import Adapter
    life, artifacts, path = setup
    adapter = Adapter(artifacts.config)
    adapter.life.connection = 'test'; adapter.life.product = 'c64'; adapter.life.protocol = 'C64M/10'
    adapter.life.status = 'ATTACHED'; adapter.life.wire = life.wire; adapter.life.disabled_verbs = set()
    first,_ = await adapter.call('history_find',dict(connection='test',limit=2))
    old = first['data']['cursor']
    second,_ = await adapter.call('history_find',dict(connection='test',limit=2))
    assert old not in adapter.life.cursors
    token = second['data']['cursor']
    last,_ = await adapter.call('history_next',dict(connection='test',cursor=token,limit=2))
    assert last['data']['cursor'] is None and not adapter.life.cursors


@pytest.mark.asyncio
async def test_export_capability_is_checked_before_wire(setup):
    life, artifacts, path = setup
    life.disabled_verbs = {'get-state'}
    with pytest.raises(AdapterError) as caught:
        await export_history(life, artifacts, dict(path=path))
    assert caught.value.data['code'] == 'unsupported_feature'
    assert life.wire.calls == [] and not Path(path).exists()
