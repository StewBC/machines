"""Source-audited serializers shared where contracts coincide, Apple dispatch."""
from __future__ import annotations
from dataclasses import dataclass
import re
from machines_mcp.schemas.tools import validate_tool
from machines_mcp.schemas.protocol_manifest import delivery

@dataclass(frozen=True)
class Command:
    verb: str
    args: str = ""
    payload: bytes | None = None
    deadline: float = 3.0
    outcome: str = "observed"
    @property
    def argument(self): return self.args


def hx(n): return f"${n:04X}"
def flag(value): return "1" if value else "0"

def _definition(d, product):
    parts=[d["access"].replace("_","-"),hx(d["address"]),"enabled="+flag(d.get("enabled",True))]
    if "end" in d: parts.append("end="+hx(d["end"]))
    parts.append("actions="+(",".join(d.get("actions",["break"])) or "none"))
    mapping=d.get("mapping",{})
    if isinstance(mapping,str): parts.append("mapping="+mapping)
    else: parts.extend(f"{k}={v}" for k,v in mapping.items())
    for k in ["counter","reset"]:
        if k in d or k=="reset": parts.append(f"{k}={d.get(k,1)}")
    if product=="apple2": parts.append(f"swap-slot={d.get('swap_slot',6)}")
    ops={"eq":"==","ne":"!=","lt":"<","gt":">","le":"<=","ge":">=","mask_set":"&","mask_clear":"!&"}
    cond=[]
    for c in d.get("conditions",[]):
        lhs=f"mem({hx(c['address'])})" if c["lhs"]=="mem" else c["lhs"]
        cond.append(lhs+ops[c["op"]]+hx(c["value"]))
    if cond: parts.append("when="+",".join(cond))
    return " ".join(parts)


def dispatch(product, name, a):
    validate_tool(name,a,product)
    payload=None; args=""; deadline=3.0
    direct={"health_check":"ping","get_state":"get-state","get_cpu":"get-cpu","get_frame":"get-frame","apple2_softswitches":"get-softswitches","c64_vic":"get-vic","c64_call_stack":"get-call-stack","history_info":"history-info","history_clear":"history-clear"}
    if name in direct: verb=direct[name]
    elif name in {"execute","c64_execute"}:
        verb=a["action"].replace("_","-")
        if "count" in a: args=a["count"]
        elif "address" in a: args=hx(a["address"])
        elif "raster" in a: args=f"{a['raster']} {a['cycle_in_line']}"
    elif name=="wait":
        verb="wait-"+a["condition"]; timeout=a.get("timeout_ms",2000); deadline=timeout/1000+1
        args=(str(a.get("delta",1))+" " if a["condition"]=="frame" else a["event"]+" " if a["condition"]=="event" else "")+str(timeout)
    elif name in {"read_memory","export_memory","write_memory"}:
        verb="set-memory" if name=="write_memory" else "get-memory"
        if name=="write_memory": payload=bytes.fromhex(a["bytes_hex"])
        args=f"{hx(a['address'])} {len(payload) if payload is not None else a.get('length',256)} {a.get('source','map')}"
    elif name=="find_symbol": verb="find-symbol"; args=a["name"]
    elif name=="snapshot": verb=a["action"]+"-state"; args=a["path"]; deadline=6 if product=="apple2" else 3
    elif name=="apple2_set_register": verb="set-reg"; args=a["name"]+" "+hx(a["value"])
    elif name=="apple2_set_turbo": verb="set-turbo"; args="max" if a.get("max") else str(a["mhz"])
    elif name=="c64_set_turbo": verb="set-turbo"; args="1" if a["mode"]=="normal" else "2"
    elif name=="c64_cia": verb="get-cia"; args=str(a["index"])
    elif name in {"c64_drive_cpu","c64_disk_status"}: verb="get-drive-cpu" if name=="c64_drive_cpu" else "get-disk-status"; args=str(a["device"])
    elif name.endswith("_assemble"):
        verb="assemble"; deadline=11; address=a.get("address",32768)
        fields={"address":hx(address),"run-address":hx(a.get("run_address",address)),"auto-run":flag(a.get("auto_run",False)),"reset":flag(a.get("reset",True))}
        for field in (["mli_launch","auto_adjust_segments"] if product=="apple2" else ["basic_run"]): fields[field.replace("_","-")]=flag(a.get(field,False))
        args=" ".join(f"{k}={v}" for k,v in fields.items())+" "+a["path"]
    elif name=="c64_load_program": verb="load-prg"; args=a["path"]
    elif name=="c64_binary_file":
        verb=a["action"]+"-bin"
        tail=[hx(a["address"]),flag(a["use_file_address"]),flag(a["reset_first"]),flag(a["is_basic"])] if a["action"]=="load" else [hx(a["start"]),hx(a["end"]),flag(a["write_file_address"]),flag(a["is_basic"])]
        args=a["path"]+" "+" ".join(tail)
    elif name=="apple2_media":
        action=a["action"]; verb={"mount":"mount","unmount":"unmount","select":"select-disk","set_writable":"set-disk-writable"}[action]
        parts=["kind="+a["kind"]] if "kind" in a else []
        slot=a.get("slot",0); drive=a.get("drive",0)
        if slot: parts.extend([str(slot),str(drive)])
        elif drive: parts.append(str(drive))
        if action=="mount": parts.append(a["path"])
        elif action=="select": parts.append(str(a["index"]))
        elif action=="set_writable": parts.append(flag(a["writable"]))
        args=" ".join(parts)
    elif name=="c64_media":
        action=a["action"]; verb={"mount":"mount-d64","unmount":"unmount-disk","power":"power-drive"}[action]
        args=str(a["device"])
        if action=="mount": args+=" "+a["path"]
        elif action=="power": args+=" "+("on" if a["powered"] else "off")
    elif name=="apple2_key": verb="key"; args=hx(a["value"])
    elif name=="c64_key": verb="restore" if a["action"]=="restore" else "key-"+a["action"]; args=a.get("key","")
    elif name=="c64_joystick":
        verb="joystick"; mask=sum(1<<i for i,k in enumerate(["up","down","left","right","fire"]) if a.get(k,False)); args=f"{a['port']} {mask}"
    elif name=="c64_paste": verb="paste-"+a["kind"]+"-data"; payload=a["text"].encode(); args=str(len(payload))
    elif name=="breakpoints":
        action=a["action"]; verb={"list":"break-list","clear_all":"break-clear-all","rearm":"rearm-oneshots","enable":"break-enable","delete":"break-clear","create":"break-create","update":"break-update"}[action]
        if "id" in a: args=str(a["id"])
        if action=="enable": args+=" "+flag(a["enabled"])
        if "definition" in a: args+=(" " if args else "")+_definition(a["definition"],product)
    elif name=="history_record": verb="history-record"; args="on" if a["enabled"] else "off"
    elif name=="history_find":
        verb="history-find"; deadline=11; parts=[]
        for k in ["pc","address","access","direction","limit","from","epoch","timeline","cycle","value","opcodes"]:
            if k not in a: continue
            value=a[k]
            if k in {"pc","address","cycle"}:
                first,last=value["first"],value["last"]
                value=f"{hx(first)}-{hx(last)}" if k!="cycle" else f"{first}-{last}"
            elif k=="from": value=value["id"] if value["kind"]=="id" else value["kind"]
            elif k=="value": value="$"+value
            elif k=="opcodes": value=",".join(value)
            parts.append(f"{k}={value}")
        if "limit" not in a: parts.append("limit=32")
        args=" ".join(parts)
    elif name in {"history_next","history_close"}:
        verb=name.replace("_","-"); args=a["cursor"]
        if name=="history_next": args+=f" limit={a.get('limit',32)}"
    elif name=="history_read":
        verb="history-read"; args=a["id"]+f" before={a.get('before',32)} after={a.get('after',8)}"
        if "epoch" in a: args+=" epoch="+a["epoch"]
    elif name=="c64_vic_ring": verb="vic-ring-"+a["action"]; args=("on" if a["enabled"] else "off") if a["action"]=="record" else ""
    elif name=="c64_vic_ring_find":
        verb="vic-ring-find"; parts=[]
        if "frame" in a: parts.append("frame="+a['frame'])
        if "raster" in a: parts.append(f"raster={a['raster']['first']}-{a['raster']['last']}")
        parts.append(f"limit={a.get('limit',312)}"); args=" ".join(parts)
    elif name=="c64_debug_memory": verb="get-debug-memory"; args="write-history="+flag(a.get('write_history',True))
    elif name=="frame_ring": verb="frame-ring-"+a["action"]; args=("on" if a["enabled"] else "off") if a["action"]=="record" else ""
    elif name=="recorded_frame": verb="get-frame-at"; args=" ".join(f"{k}={v}" for k,v in a["target"].items())
    elif name=="inspector":
        verb={"enter":"enter-inspector","leave":"leave-inspector","land":"land-inspector","land_exact":"land-inspector-exact"}[a["action"]]
        if "cycle" in a: args="cycle="+a["cycle"]
    else: raise ValueError("tool is handled locally, not by emulator backend")
    if len((verb+" "+args).encode())>490: raise ValueError("serialized command exceeds wire header limit")
    outcome={"immediate_observation":"observed","runtime_completed_reply":"completed","queued_acceptance":"accepted","wait":"observed"}[delivery(product,verb)]
    return Command(verb,args,payload,deadline,outcome)


COUNTER_FIELDS={"cycles","cycle","focus_cycle","epoch","frame","film_cycle","oldest_frame","newest_frame","oldest_cycle","newest_cycle","oldest_id","newest_id","next_id","dropped","bytes","sequence","session","request_token","cursor","seq","delta","requested_bytes","capacity_bytes","used_bytes","records","oldest","newest","wrapped","partial","truncated_accesses","target","generation"}
HEX_FIELDS={"pc","a","x","y","sp","p","flags","motors","address","start","end","stop_address","irq_vector","nmi_vector","d011","d016","mem","irq_st","irq_en","vc","vcbase","pra","prb","ddra","ddrb","cra","crb","icr_flags","icr_mask"}
def parse_response(product,verb,text,payload=None):
    """Decode known scalar bases; preserve all unknown native fields and raw text."""
    data={"raw_text":text}
    for key,value in re.findall(r"([^\s=]+)=([\s\S]*?)(?=\s+[^\s=]+=|$)",text):
        value=value.strip()
        if key=="cycle" and verb in {"get-vic","get-softswitches"} and value.isdecimal(): data[key]=int(value)
        elif key in COUNTER_FIELDS and value.isdecimal(): data[key]=str(int(value))
        elif value.startswith("$"):
            try: data[key]=int(value[1:],16)
            except ValueError: data[key]=value
        elif key.lower() in HEX_FIELDS and re.fullmatch(r"[0-9A-Fa-f]+",value): data[key]=int(value,16)
        elif re.fullmatch(r"-?[0-9]+",value): data[key]=int(value)
        else: data[key]=value
    if verb=="get-cia":
        for key in ["ta","tb"]:
            if key in data and re.fullmatch(r"[0-9A-Fa-f]{4}/[0-9A-Fa-f]{4}",str(data[key])):
                counter,latch=data[key].split("/"); data[key]={"counter":int(counter,16),"latch":int(latch,16)}
    if verb=="get-call-stack":
        data["entries"]=[{"index":int(key[5:]),"jsr_address":int(value.split(":")[0],16),"destination_address":int(value.split(":")[1],16)} for key,value in data.items() if re.fullmatch(r"frame[0-9]+",key) and isinstance(value,str) and re.fullmatch(r"[0-9A-Fa-f]{4}:[0-9A-Fa-f]{4}",value)]
    if payload is not None: data["byte_count"]=len(payload)
    return data
