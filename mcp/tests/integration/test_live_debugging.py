"""Live new-tool acceptance on deterministic emulator fixtures."""
import asyncio
import json
import os
from pathlib import Path
import socket
import pytest
from machines_mcp.config import Config
from machines_mcp.server import Adapter

ROOT = Path(__file__).resolve().parents[3]
pytestmark = pytest.mark.skipif(os.environ.get('MACHINES_MCP_LIVE') != '1', reason='Set MACHINES_MCP_LIVE=1 after building emulators')


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


@pytest.mark.parametrize('product,exe', [('apple2','a2m'), ('c64','c64m')])
async def test_debug_extensions_live(product, exe, tmp_path):
    art = tmp_path / 'artifacts'; art.mkdir()
    adapter = Adapter(Config(read_roots=[ROOT, tmp_path], write_roots=[tmp_path], artifact_dir=art,
                             profiles={product:dict(product=product,executable=str(ROOT/'build'/exe),cwd=str(ROOT),port=free_port(),headless=True,history_memory=16)}))
    async def call(name, **args):
        if name not in ('launch','stop_owned'): args['connection'] = adapter.life.connection
        result, _ = await adapter.call(name, args)
        return result['data']
    try:
        await call('launch', profile=product)
        await call('execute', action='pause')
        await call('wait', condition='paused', timeout_ms=5000)
        state = await call('get_state'); assert state['state'] == 'paused'
        # Known bytes do not depend on a ROM's boot progress or instruction phase.
        await call('write_memory', address=0x8000, bytes_hex='a9098d00c04c0080', source='ram' if product=='c64' else 'main')
        dis = await call('disassemble', address=0x8000, length=8, max_instructions=3, source='ram' if product=='c64' else 'main')
        assert [r['text'] for r in dis['records']] == ['LDA #$09','STA $C000','JMP $8000']
        assert dis['cpu_class'] == ('nmos' if product=='c64' else '65c02')
        await call('history_clear')
        await call('execute', action='step_instruction')
        await call('wait', condition='event', event='step-complete', timeout_ms=5000)
        await call('execute', action='pause'); await call('wait', condition='paused', timeout_ms=5000)
        for _ in range(70):
            await call('execute', action='step_instruction')
            await call('get_cpu')  # native response barrier; do not reuse sticky step events
        info = await call('history_info')
        assert int(info['records']) > 64
        export_path = tmp_path / 'retained history ü.jsonl'
        exported = await call('history_export', path=str(export_path), timeout_ms=5000)
        rows = [json.loads(x) for x in export_path.read_text().splitlines()]
        assert exported['complete'] and rows[-1]['complete']
        assert exported['records'] == info['records'] and exported['pages'] >= 2
        assert len(rows) == int(exported['records']) + 2
        if product == 'c64':
            debug_path = tmp_path / 'full debug memory.bin'
            snap = await call('c64_debug_memory', address=0x8000, length=8, write_history=True, path=str(debug_path))
            assert snap['views']['ram'] == 'a9098d00c04c0080'
            assert debug_path.stat().st_size == 65536 * 11
            token = snap['snapshot_token']
            await call('write_memory', address=0x8000, bytes_hex='ea', source='ram')
            same = await call('c64_debug_memory_page', token=token, address=0x8000, length=8)
            assert same['views']['ram'] == snap['views']['ram']
            assert same['generation'] == snap['generation']
            await call('c64_vic_ring', action='clear')
            await call('c64_vic_ring', action='record', enabled=True)
            await call('execute', action='run')
            await call('wait', condition='frame', delta=2, timeout_ms=5000)
            await call('execute', action='pause'); await call('wait', condition='paused', timeout_ms=5000)
            ring = await call('c64_vic_ring', action='info')
            assert int(ring['count']) > 0
            lines = await call('c64_vic_ring_find', limit=8)
            assert lines['count'] == 8 and lines['possibly_limited']
            assert all(len(r['spr_x']) == 8 for r in lines['records'])
            await call('c64_vic_ring', action='record', enabled=False)
    finally:
        if adapter.life.owned:
            await call('stop_owned', ownership=adapter.life.owned['ownership'])
        await adapter.life.shutdown()
