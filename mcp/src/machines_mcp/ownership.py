"""Fail-closed platform listener ownership probes; never identify by hello alone."""
import asyncio
import ctypes
import os
import socket
import sys
from pathlib import Path

async def listener_owned(pid,port):
    if sys.platform=='darwin':
        p=await asyncio.create_subprocess_exec('/usr/sbin/lsof','-nP','-a','-p',str(pid),f'-iTCP:{port}','-sTCP:LISTEN','-Fpn',stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
        try:
            out,_=await asyncio.wait_for(p.communicate(),2)
        except BaseException:
            p.kill(); await p.wait(); raise
        names=[line[1:] for line in out.decode().splitlines() if line.startswith('n')]
        return p.returncode==0 and f'p{pid}' in out.decode().splitlines() and bool(names) and all(n in (f'127.0.0.1:{port}',f'[::1]:{port}') for n in names)
    if sys.platform.startswith('linux'):
        def probe():
            inodes=set()
            for line in Path('/proc/net/tcp').read_text().splitlines()[1:]:
                fields=line.split()
                address,hexport=fields[1].split(':')
                if fields[3]=='0A' and int(hexport,16)==port and address=='0100007F':
                    inodes.add(fields[9])
            child={os.readlink(p)[8:-1] for p in Path(f'/proc/{pid}/fd').iterdir() if os.readlink(p).startswith('socket:[')}
            return len(inodes)==1 and bool(inodes & child)
        return await asyncio.wait_for(asyncio.to_thread(probe),2)
    if sys.platform=='win32':
        def probe():
            class Row(ctypes.Structure):
                _fields_=[(n,ctypes.c_ulong) for n in ('state','addr','port','remote_addr','remote_port','pid')]
            size=ctypes.c_ulong(0)
            api=ctypes.windll.iphlpapi.GetExtendedTcpTable
            api(None,ctypes.byref(size),False,socket.AF_INET,3,0)
            buf=ctypes.create_string_buffer(size.value)
            if api(buf,ctypes.byref(size),False,socket.AF_INET,3,0)!=0:
                return False
            count=ctypes.c_ulong.from_buffer(buf).value
            rows=[Row.from_buffer(buf,4+i*ctypes.sizeof(Row)) for i in range(count)]
            matched=[r for r in rows if socket.ntohs(r.port & 0xffff)==port]
            return bool(matched) and all(r.pid==pid and r.addr==0x0100007f for r in matched)
        return await asyncio.wait_for(asyncio.to_thread(probe),2)
    return False
