"""Independent boundary tests for transport, admission, and retained ownership."""
import asyncio
from pathlib import Path
import pytest
from machines_mcp.config import Config
from machines_mcp.errors import AdapterError
from machines_mcp.wire import Wire
from machines_mcp.artifacts import Artifacts
from machines_mcp.lifecycle import Lifecycle


def test_path_roots_symlinks_and_overwrites(tmp_path):
    root = tmp_path / 'allowed'; root.mkdir()
    outside = tmp_path / 'outside'; outside.mkdir()
    source = root / 'source.bin'; source.write_bytes(b'abc')
    cfg = Config(read_roots=[root.resolve()], write_roots=[root.resolve()])
    assert cfg.path(str(source)) == source
    with pytest.raises(AdapterError, match='outside configured roots'):
        cfg.path(str(outside / 'out.bin'), write=True)
    with pytest.raises(AdapterError, match='overwrite'):
        cfg.path(str(source), write=True)
    assert cfg.path(str(source), write=True, overwrite=True) == source
    try:
        (root / 'escape').symlink_to(outside, target_is_directory=True)
    except OSError:
        return  # Windows unprivileged hosts may prohibit symlinks.
    with pytest.raises(AdapterError, match='outside configured roots'):
        cfg.path(str(root / 'escape' / 'out.bin'), write=True)


def test_artifact_exclusive_publish_and_quota(tmp_path):
    cfg = Config(write_roots=[tmp_path], artifact_dir=tmp_path, artifact_quota=20)
    store = Artifacts(cfg)
    path = str(tmp_path / 'memory.bin')
    result = store.write(b'abc', 'bin', path=path)
    assert result['size'] == 3 and len(result['sha256']) == 64
    with pytest.raises(AdapterError, match='overwrite'):
        store.write(b'replace', 'bin', path=path)
    assert Path(path).read_bytes() == b'abc'
    with pytest.raises(AdapterError, match='quota'):
        store.write(b'x' * 20, 'bin')
    assert not list(tmp_path.glob('.machines-mcp-*'))


@pytest.mark.asyncio
async def test_fragmented_binary_and_interleaved_events():
    events = []; failures = []
    async def peer(reader, writer):
        await reader.readline()
        for piece in (b'0 event state-', b'changed reason=step\n1 data memory 4 source=ram\n', b'\0\n', b'\xffA\n'):
            writer.write(piece); await writer.drain(); await asyncio.sleep(.005)
        await reader.read()
        writer.close(); await writer.wait_closed()
    server = await asyncio.start_server(peer, '127.0.0.1', 0)
    async with server:
        wire = await Wire.connect(server.sockets[0].getsockname()[1], events.append, failures.append)
        try:
            reply = await wire.request('get-memory', '$0000 4 ram')
            assert reply.payload == b'\0\n\xffA'
            assert reply.data_type == 'memory' and reply.text == 'source=ram'
            assert events == ['state-changed reason=step'] and not failures
        finally:
            await wire.close()


@pytest.mark.asyncio
@pytest.mark.parametrize('response', [b'2 ok wrong-id\n', b'1 data memory 99999999\n', b'1 data memory 1\nAx', b'1 mystery\n'])
async def test_bad_wire_response_fails_connection(response):
    failures = []
    async def peer(reader, writer):
        await reader.readline(); writer.write(response); await writer.drain()
        await reader.read(); writer.close(); await writer.wait_closed()
    server = await asyncio.start_server(peer, '127.0.0.1', 0)
    async with server:
        wire = await Wire.connect(server.sockets[0].getsockname()[1], lambda _: None, failures.append)
        try:
            with pytest.raises(AdapterError):
                await wire.request('ping', deadline=.5)
            assert wire.closed and failures
        finally:
            await wire.close()


@pytest.mark.asyncio
async def test_wire_deadline_does_not_retry():
    requests = []
    async def peer(reader, writer):
        requests.append(await reader.readline())
        await reader.read(); writer.close(); await writer.wait_closed()
    server = await asyncio.start_server(peer, '127.0.0.1', 0)
    async with server:
        failures = []
        wire = await Wire.connect(server.sockets[0].getsockname()[1], lambda _: None, failures.append)
        with pytest.raises(AdapterError) as caught:
            await wire.request('reset', deadline=.02)
        assert caught.value.data['operation_effect'] == 'possible'
        assert wire.closed and len(requests) == 1 and failures


@pytest.mark.asyncio
async def test_failed_owned_connection_retains_stop_access(tmp_path):
    class Child:
        pid = 1234
        returncode = None
        def terminate(self): self.returncode = -15
        def kill(self): self.returncode = -9
        async def wait(self): return self.returncode
    life = Lifecycle(Config(), Artifacts(Config()))
    child = Child()
    life.owned = dict(ownership='owned', process=child, pid=child.pid, log_tasks=[], logs={}, keep_on_exit=False)
    life.connection = 'connection'
    life.failure(AdapterError('unavailable', 'peer disappeared'))
    assert life.info()['owned']['ownership'] == 'owned'
    with pytest.raises(AdapterError, match='stop_owned'):
        life._available()
    result = await life.stop_owned('owned')
    assert result['pid'] == child.pid and child.returncode == -15
    assert life.owned is None and life.status == 'DISCONNECTED'
    with pytest.raises(AdapterError):
        await life.stop_owned('owned')

@pytest.mark.asyncio
async def test_cancelled_dispatched_call_retains_operation_lock():
    from machines_mcp.server import Adapter
    adapter = Adapter(Config())
    started = asyncio.Event(); finish = asyncio.Event()
    async def operation(name, args):
        started.set(); await finish.wait()
        return {'finished': True}, None
    adapter._operation = operation
    task = asyncio.create_task(adapter.call('attach', {'port': 1234}))
    await started.wait()
    task.cancel(); await asyncio.sleep(0)
    assert adapter.lock.locked()
    with pytest.raises(AdapterError, match='conflicts'):
        await adapter.call('launch', {'profile': 'test'})
    finish.set()
    with pytest.raises(asyncio.CancelledError):
        await task
    assert not adapter.lock.locked() and adapter.queued == 0


@pytest.mark.asyncio
async def test_cancelled_queued_call_never_dispatches():
    from machines_mcp.server import Adapter
    adapter = Adapter(Config())
    calls = []
    async def operation(name, args):
        calls.append(name)
        return {}, None
    adapter._operation = operation
    await adapter.lock.acquire()
    task = asyncio.create_task(adapter.call('get_cpu', {'connection': 'test'}))
    await asyncio.sleep(0)
    assert adapter.queued == 1
    task.cancel()
    with pytest.raises(asyncio.CancelledError): await task
    adapter.lock.release()
    assert calls == [] and adapter.queued == 0

@pytest.mark.asyncio
async def test_write_failure_reports_possible_effect_and_unavailable():
    class Writer:
        def write(self, data): raise BrokenPipeError('peer closed during write')
        def close(self): pass
        async def wait_closed(self): pass
    reader = asyncio.StreamReader()
    failures = []
    wire = Wire(reader, Writer(), lambda _: None, failures.append)
    with pytest.raises(AdapterError) as caught:
        await wire.request('reset')
    assert caught.value.data['code'] == 'unavailable'
    assert caught.value.data['operation_effect'] == 'possible'
    assert wire.closed and wire.pending is None and failures
