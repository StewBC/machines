"""Strict asynchronous A2M/C64M framing. One continuously draining reader."""
import asyncio
from dataclasses import dataclass
from .errors import AdapterError

@dataclass
class Reply:
    kind: str
    text: str
    payload: bytes = b""
    data_type: str = ""

class Wire:
    def __init__(self, reader, writer, event, failure):
        self.reader, self.writer = reader, writer
        self.event, self.failure = event, failure
        self.pending = None
        self.request_id = 0
        self.closed = False
        self.task = asyncio.create_task(self._reader())

    @classmethod
    async def connect(cls, port, event, failure):
        r, w = await asyncio.wait_for(asyncio.open_connection('127.0.0.1', port, limit=513), 3)
        return cls(r, w, event, failure)

    async def _reader(self):
        try:
            while True:
                raw = await self.reader.readline()
                if not raw:
                    raise EOFError('emulator closed connection')
                if len(raw) > 512 or not raw.endswith(b'\n') or b'\x00' in raw:
                    raise ValueError('invalid or oversized response header')
                parts = raw[:-1].decode('utf-8', errors='strict').split(' ', 2)
                if len(parts) < 2:
                    raise ValueError('incomplete response header')
                rid, kind = int(parts[0]), parts[1]
                text = parts[2] if len(parts) == 3 else ''
                if rid == 0 and kind == 'event':
                    self.event(text)
                    continue
                if self.pending is None or rid != self.pending[0]:
                    raise ValueError('unexpected response id')
                payload, dtype = b'', ''
                if kind == 'data':
                    fields = text.split(' ', 2)
                    dtype, size = fields[0], int(fields[1])
                    kind_limit={'memory':65536,'frame':2080*312,'history':24+129*(48+67*8),'breakpoints':64*384,'vic-ring':2048*512,'debug-memory':65536*11}.get(dtype,8*1024*1024)
                    if size < 0 or size > kind_limit:
                        raise ValueError('invalid payload size')
                    payload = await self.reader.readexactly(size)
                    if await self.reader.readexactly(1) != b'\n':
                        raise ValueError('missing binary delimiter')
                    text = fields[2] if len(fields) > 2 else ''
                elif kind not in ('ok', 'error'):
                    raise ValueError('unknown response kind')
                future = self.pending[1]
                self.pending = None
                if not future.done():
                    future.set_result(Reply(kind, text, payload, dtype))
        except asyncio.CancelledError:
            return
        except Exception as exc:
            self.closed = True
            self.writer.close()
            error = AdapterError('unavailable', str(exc), effect='possible')
            if self.pending and not self.pending[1].done():
                self.pending[1].set_exception(error)
            self.pending = None
            self.failure(error)

    async def request(self, verb, args='', payload=None, deadline=3):
        if self.closed:
            raise AdapterError('unavailable', 'Control connection is unavailable')
        if self.pending:
            raise AdapterError('busy', 'A wire request is already in flight')
        self.request_id += 1
        text = f'{self.request_id} {verb}' + (f' {args}' if args else '')
        if any(c in text for c in '\r\n\x00'):
            raise AdapterError('invalid_argument', 'Wire text cannot contain CR, LF, or NUL')
        raw = (text + '\n').encode('utf-8')
        if len(raw) >= 512:
            raise AdapterError('invalid_argument', 'Serialized request exceeds native header bound')
        future = asyncio.get_running_loop().create_future()
        self.pending = (self.request_id, future)
        try:
            self.writer.write(raw)
            if payload is not None:
                self.writer.write(payload + b'\n')
            await self.writer.drain()
            reply = await asyncio.wait_for(asyncio.shield(future), deadline)
            if reply.kind == 'error':
                code, _, message = reply.text.partition(' ')
                raise AdapterError('emulator_rejection', message or code, wire_code=code)
            return reply
        except asyncio.TimeoutError:
            await self.close()
            error = AdapterError('deadline', 'Emulator reply deadline exceeded; no automatic retry', effect='possible')
            self.failure(error)
            raise error
        except (OSError,ConnectionError) as exc:
            await self.close()
            error=AdapterError('unavailable',str(exc),effect='possible')
            self.failure(error)
            raise error from exc

    async def close(self):
        self.closed = True
        if self.task is not asyncio.current_task():
            self.task.cancel()
            await asyncio.gather(self.task, return_exceptions=True)
        if self.pending and not self.pending[1].done():
            self.pending[1].set_exception(AdapterError('unavailable','Connection closed',effect='possible'))
            self.pending[1].exception() # mark consumed when deadline already abandoned the future
        self.pending = None
        self.writer.close()
        try: await self.writer.wait_closed()
        except OSError: pass
