"""C64 native fresh bulk sample; history stores writer PCs, not ages."""
import sys
from ..errors import AdapterError
from ._common import metadata, integer
import time
import uuid
from collections import OrderedDict

class DebugSnapshot:
    def __init__(self,text,payload):
        m=metadata(text)
        if set(m)!={'generation','map','ram','rom','write_history'}: raise ValueError('Unexpected debug-memory metadata')
        self.generation=str(integer(m,'generation'))
        if any(integer(m,k)!=65536 for k in ('map','ram','rom')): raise ValueError('Invalid debug-memory dimensions')
        self.has_history=integer(m,'write_history',1)==1
        if len(payload)!=65536*(3+(8 if self.has_history else 0)): raise ValueError('Invalid debug-memory payload length')
        self.payload=payload
        self.metadata=dict(generation=self.generation,write_history=self.has_history,write_history_semantics='four packed 16-bit opcode PCs, latest first; zero is ambiguous; no timestamps or write ages',byte_order=sys.byteorder,layout='map[65536],ram[65536],rom[65536],optional native uint64 write_history[65536]',sample_consistency='single_fresh_native_snapshot',unavailable_native_fields=['drive8_map','drive9_map','drive8_valid','drive9_valid'])

    def window(self,address,length):
        if not 0<=address<65536 or not 1<=length<=512 or address+length>65536: raise ValueError('Invalid debug-memory window')
        result=dict(self.metadata,address=address,length=length)
        result['views']={k:self.payload[i*65536+address:i*65536+address+length].hex() for i,k in enumerate(('map','ram','rom'))}
        if self.has_history:
            packed=[int.from_bytes(self.payload[3*65536+8*a:3*65536+8*(a+1)],sys.byteorder) for a in range(address,address+length)]
            result['write_history_packed']=[str(v) for v in packed]
            result['writer_pcs_latest_first']=[[(v>>(16*i))&65535 for i in range(4)] for v in packed]
        return result

class DebugSnapshots:
    """Four immutable raw samples, connection-bound and fifteen-minute TTL."""
    def __init__(self): self.samples=OrderedDict()
    def add(self,connection,snapshot):
        self.expire()
        while len(self.samples)>=4: self.samples.popitem(last=False)
        token=uuid.uuid4().hex
        self.samples[token]=(time.monotonic(),connection,snapshot)
        return token
    def expire(self):
        for token,(at,_,_) in list(self.samples.items()):
            if time.monotonic()-at>900: del self.samples[token]
    def get(self,connection,token):
        self.expire()
        if token not in self.samples or self.samples[token][1]!=connection: raise AdapterError('expired','Debug sample expired, evicted, or belongs to another connection')
        return self.samples[token][2]
