import asyncio
import pytest
from machines_mcp.wire import Wire
from machines_mcp.errors import AdapterError


async def exchange(response, *, deadline=1):
    connections = []
    async def handler(reader, writer):
        connections.append(writer)
        await reader.readline()
        for fragment in response:
            writer.write(fragment)
            await writer.drain()
            await asyncio.sleep(0)
    server = await asyncio.start_server(handler,'127.0.0.1',0)
    events, failures = [], []
    wire = await Wire.connect(server.sockets[0].getsockname()[1],events.append,failures.append)
    try:
        reply = await wire.request('get-memory',deadline=deadline)
        return reply, events, failures
    finally:
        await wire.close()
        for writer in connections:
            writer.close()
            await writer.wait_closed()
        server.close()
        await server.wait_closed()


@pytest.mark.asyncio
async def test_fragmented_header_binary_and_interleaved_event():
    payload = b'\n\x00\xff123\n'
    response = b'0 event state-changed paused=1\n1 data memory 7 source=ram\n' + payload + b'\n'
    reply,events,failures = await exchange([bytes([x]) for x in response])
    assert reply.payload == payload and reply.text == 'source=ram' and reply.data_type == 'memory'
    assert events == ['state-changed paused=1'] and failures == []


@pytest.mark.asyncio
@pytest.mark.parametrize('response',[b'2 ok yes\n',b'1 data memory 8388609\n',b'1 data memory 2\nab!',b'1 unknown foo\n',b'1 ok '+b'x'*512+b'\n'])
async def test_malformed_reply_closes_connection(response):
    with pytest.raises(AdapterError) as caught:
        await exchange([response])
    assert caught.value.data['code'] == 'unavailable'
    assert caught.value.data['operation_effect'] == 'possible'


@pytest.mark.asyncio
async def test_missing_reply_deadline():
    with pytest.raises(AdapterError) as caught:
        await exchange([],deadline=.02)
    assert caught.value.data['code'] == 'deadline'


@pytest.mark.asyncio
async def test_native_error_retains_wire_code():
    with pytest.raises(AdapterError) as caught:
        await exchange([b'1 error busy paused runtime required\n'])
    assert caught.value.data['code'] == 'emulator_rejection'
    assert caught.value.data['wire_code'] == 'busy'
