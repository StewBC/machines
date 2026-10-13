"""Strict typed decoding of all fields actually published by C64M/10."""
import re
from ._common import metadata,integer
COUNTERS={'frame','cycle'}
DECIMAL={'raster','badline','allow_bl','display','vborder','mborder','rc','vmli'}
HEX={'d011':2,'d016':2,'d018':2,'vc':4,'vcbase':4,'border':1,**{k:2 for k in ('spr_en','spr_vis','spr_act','spr_pri','spr_mc','spr_xe','spr_yeff')}}
LISTS={'bg':(4,1),'spr_x':(8,4),**{k:(8,2) for k in ('spr_y','spr_ptr','spr_col','spr_mcnt','spr_mcbase')}}
FIELDS=COUNTERS|DECIMAL|set(HEX)|set(LISTS)|{'irq'}
def decode_vic_ring(text,payload):
    meta=metadata(text)
    if set(meta)!={'count'}: raise ValueError('Invalid VIC-ring metadata')
    count=integer(meta,'count',2048)
    if len(payload)>2048*512: raise ValueError('Oversized VIC-ring payload')
    lines=payload.decode('ascii').splitlines()
    if len(lines)!=count or (payload and not payload.endswith(b'\n')): raise ValueError('VIC-ring count/terminator mismatch')
    records=[]
    for line in lines:
        if len(line)>=512: raise ValueError('Oversized VIC record')
        fields=metadata(line)
        if set(fields)!=FIELDS: raise ValueError('Missing or unknown VIC-ring field')
        record={k:str(integer(fields,k)) for k in COUNTERS}
        record.update({k:integer(fields,k,65535) for k in DECIMAL})
        for key,width in HEX.items():
            if not re.fullmatch('[0-9A-Fa-f]{'+str(width)+'}',fields[key]): raise ValueError('Invalid VIC hex field')
            record[key]=int(fields[key],16)
        for key,(n,width) in LISTS.items():
            values=fields[key].split(',')
            if len(values)!=n or any(not re.fullmatch('[0-9A-Fa-f]{'+str(width)+'}',v) for v in values): raise ValueError('Invalid VIC array')
            record[key]=[int(v,16) for v in values]
        if not re.fullmatch('[0-9A-Fa-f]{2}/[0-9A-Fa-f]{2}',fields['irq']): raise ValueError('Invalid VIC IRQ pair')
        record['irq']={k:int(v,16) for k,v in zip(('status','enable'),fields['irq'].split('/'))}
        records.append(record)
    return dict(count=count,records=records,raw_metadata=text,ordering='native retained order',limit_semantics='native bounded selection; no continuation cursor; count equal to requested limit may be incomplete',sample_consistency='single_native_ring_copy')
