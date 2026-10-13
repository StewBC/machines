"""Typed public contracts; no free-form emulator command escape hatch."""
from __future__ import annotations
import re
import math
from jsonschema import Draft202012Validator


def integer(lo=0, hi=65535, default=None):
    s = {"type":"integer", "minimum":lo, "maximum":hi}
    if default is not None: s["default"] = default
    return s

def enum(*values): return {"type":"string", "enum":list(values)}
def obj(properties=None, required=()):
    return {"type":"object", "properties":properties or {}, "required":list(required), "additionalProperties":False}
def tagged(key, variants):
    return {"oneOf":[obj({key:{"const":tag}, **props}, [key,*required]) for tag,props,required in variants]}
ADDR=integer(); BYTE=integer(0,255); U32=integer(0,4294967295)
COUNTER={"type":"string","pattern":"^(0|[1-9][0-9]{0,19})$", "description":"Unsigned 64-bit decimal string."}
TOKEN={"type":"string","minLength":1,"maxLength":256}
PATH={"type":"string","minLength":1,"maxLength":400,"pattern":"^[^\\r\\n\\u0000]+$", "description":"Absolute host path inside configured roots."}
BOOL={"type":"boolean"}; OVERWRITE={"type":"boolean","default":False}
PATTERN={"type":"string","pattern":"^[0-9A-Fa-f?]{2}$"}
SOURCES=enum("map","main","aux","lc1","lc2","rom","ram","drive8","drive9")
ACCESS=enum("execute","fetch","opcode","operand","data-read","data-write","read","write","data","dummy-read","rmw-dummy-write","stack-read","stack-write","vector-read")
APPLE_EVENTS="running paused step-complete run-complete reset-complete breakpoints frame assemble-complete assemble-error".split()
C64_EVENTS=APPLE_EVENTS+"pong started stopped error cpu-state machine-state memory memory-view disk-status call-stack disk-swap debug-memory save-state-complete load-state-complete history-status history-result session state-changed".split()
KEYS=list("abcdefghijklmnopqrstuvwxyz0123456789")+"space return delete left-shift right-shift plus minus asterisk equals colon semicolon comma period slash at cursor-right cursor-down home run-stop control commodore left-arrow up-arrow pound f1 f3 f5 f7".split()
TOOLS={}
def add(name, schema=None, *, description=None, readonly=False, product=None, connected=True):
    schema=schema or obj()
    # Each union branch is a closed object, including its connection field.
    branches=schema.get("oneOf", [schema])
    if connected:
        for branch in branches:
            branch["properties"]={"connection":TOKEN,**branch["properties"]}
            branch["required"]=["connection",*branch["required"]]
    TOOLS[name]={"description": description or name.replace("_"," ")+". Results are sampled; designate one agent as emulator owner during parallel investigations.","inputSchema":schema,"read_only":readonly,"product":product}

add("attach",obj({"port":integer(1,65535),"expected_product":enum("apple2","c64")},["port"]),connected=False)
add("launch",obj({"profile":TOKEN,"port":integer(1,65535),"headless":BOOL,"keep_on_exit":OVERWRITE},["profile"]),connected=False)
add("connection_info",obj({"connection":TOKEN}),connected=False,readonly=True)
for name in ("stop_owned","abandon_owned"): add(name,obj({"ownership":TOKEN},["ownership"]),connected=False)
add("detach",obj({"keep_running":OVERWRITE}))
for name in ("health_check","get_state","get_cpu","history_info","apple2_softswitches","c64_vic","c64_call_stack"):
    add(name,readonly=True,product="apple2" if name.startswith("apple2_") else "c64" if name.startswith("c64_") else None)
add("execute",obj({"action":enum("run","pause","reset","step_cycle","step_instruction","step_over","step_out")},["action"]),description="Change execution explicitly. Reset destroys current state. Inspector run/step use sealed replay. Completion is reported honestly; no implicit pause.")
add("wait",tagged("condition",[(x,{"timeout_ms":integer(1,600000,2000),**({"delta":integer(1,4294967295,1)} if x=="frame" else {"event":enum(*C64_EVENTS)} if x=="event" else {})},["event"] if x=="event" else []) for x in ["paused","running","frame","event"]]),readonly=True,description="Observe a native wait event. Sticky events may precede this request; this is not request-correlated completion. Cancellation may drain until timeout.")
for name,maximum in [("read_memory",4096),("export_memory",65536)]:
    add(name,obj({"address":ADDR,"length":integer(1,maximum,256),"source":SOURCES,**({"path":PATH,"overwrite":OVERWRITE} if name=="export_memory" else {})},["address",*(["path"] if name=="export_memory" else [])]),readonly=name=="read_memory")
add("write_memory",obj({"address":ADDR,"bytes_hex":{"type":"string","pattern":"^(?:[0-9A-Fa-f]{2}){1,1024}$"},"source":SOURCES},["address","bytes_hex"]))
add("get_frame",obj({"path":PATH,"overwrite":OVERWRITE}),readonly=True,description="Read full raster PNG without pausing. Current frame may be partial. Optional path saves a copy.")
add("find_symbol",obj({"name":{"type":"string","minLength":1,"maxLength":400,"pattern":"^[^\\r\\n\\u0000]+$"}},["name"]),readonly=True)
add("snapshot",tagged("action",[("save",{"path":PATH,"overwrite":OVERWRITE},["path"]),("load",{"path":PATH},["path"])]))
add("events",obj({"after":COUNTER,"limit":integer(1,128,32)}),readonly=True)
add("result_page",obj({"token":TOKEN,"offset":integer(0,2147483647,0),"limit":integer(1,64,32)},["token"]),readonly=True)
add("apple2_set_register",obj({"name":enum("pc","a","x","y","sp","p"),"value":ADDR},["name","value"]),product="apple2")
add("apple2_set_turbo",{"oneOf":[obj({"mhz":{"type":"number","minimum":.001,"maximum":10000}},["mhz"]),obj({"max":{"const":True}},["max"])]},product="apple2")
add("c64_set_turbo",obj({"mode":enum("normal","max")},["mode"]),product="c64")
add("c64_execute",tagged("action",[("run_cycles",{"count":COUNTER},["count"]),("run_instructions",{"count":COUNTER},["count"]),("run_to",{"address":ADDR},["address"]),("step_frame",{},[]),("run_to_raster",{"raster":ADDR,"cycle_in_line":ADDR},["raster","cycle_in_line"])]),product="c64")
add("c64_cia",obj({"index":{"enum":[1,2]}},["index"]),product="c64",readonly=True)
for name in ["c64_drive_cpu","c64_disk_status"]: add(name,obj({"device":{"enum":[8,9]}},["device"]),product="c64",readonly=True)
for product in ["apple2","c64"]:
    props={"path":PATH,"address":integer(0,65535,32768),"run_address":ADDR,"auto_run":{"type":"boolean","default":False},"reset":{"type":"boolean","default":True}}
    props.update({"mli_launch":OVERWRITE,"auto_adjust_segments":OVERWRITE} if product=="apple2" else {"basic_run":OVERWRITE})
    add(product+"_assemble",obj(props,["path"]),product=product,description="Assemble host source; reset defaults true. Includes and output directives are native filesystem access, not sandboxed by adapter roots.")
add("c64_load_program",obj({"path":PATH},["path"]),product="c64")
add("c64_binary_file",tagged("action",[("load",{"path":PATH,"address":ADDR,"use_file_address":BOOL,"reset_first":BOOL,"is_basic":BOOL},["path","address","use_file_address","reset_first","is_basic"]),("save",{"path":PATH,"start":ADDR,"end":ADDR,"write_file_address":BOOL,"is_basic":BOOL,"overwrite":OVERWRITE},["path","start","end","write_file_address","is_basic"])]),product="c64")
media={"slot":integer(0,7,0),"drive":{"enum":[0,1],"default":0}}
add("apple2_media",tagged("action",[("mount",{**media,"kind":enum("diskii","smartport"),"path":PATH},["kind","path"]),("unmount",{**media,"kind":enum("diskii","smartport")},["kind"]),("select",{**media,"index":integer(1,4294967295)},["index"]),("set_writable",{**media,"writable":BOOL},["writable"])]),product="apple2")
add("c64_media",tagged("action",[("mount",{"device":{"enum":[8,9]},"path":PATH},["device","path"]),("unmount",{"device":{"enum":[8,9]}},["device"]),("power",{"device":{"enum":[8,9]},"powered":BOOL},["device","powered"])]),product="c64")
add("apple2_key",obj({"value":BYTE},["value"]),product="apple2")
add("c64_key",tagged("action",[("down",{"key":enum(*KEYS)},["key"]),("up",{"key":enum(*KEYS)},["key"]),("restore",{},[])]),product="c64")
add("c64_joystick",obj({"port":{"enum":[1,2]},**{k:{"type":"boolean","default":False} for k in ["up","down","left","right","fire"]}},["port"]),product="c64")
add("c64_paste",obj({"kind":enum("text","events"),"text":{"type":"string","minLength":1,"maxLength":4096}},["kind","text"]),product="c64")
CONDITION=obj({"lhs":enum("a","x","y","sp","p","n","v","b","d","i","z","c","value","mem","raster","vic_cycle","cycle_in_line"),"address":ADDR,"op":enum("eq","ne","lt","gt","le","ge","mask_set","mask_clear"),"value":ADDR},["lhs","op","value"])
DEFINITION=obj({"access":enum("exec","read","write","read_write"),"address":ADDR,"end":ADDR,"enabled":{"type":"boolean","default":True},"actions":{"type":"array","items":enum("break","fast","slow","type","swap"),"uniqueItems":True,"default":["break"]},"counter":U32,"reset":integer(0,4294967295,1),"swap_slot":integer(0,7,6),"conditions":{"type":"array","maxItems":4,"items":CONDITION},"mapping":{"oneOf":[enum("map","rom","ram"),obj({"ram":enum("map","main","aux"),"c100":enum("map","rom"),"d000":enum("map","lc1","lc2","rom")})]}},["access","address"])
add("breakpoints",tagged("action",[("list",{},[]),("clear_all",{},[]),("rearm",{},[]),("enable",{"id":integer(1,4294967295),"enabled":BOOL},["id","enabled"]),("delete",{"id":integer(1,4294967295)},["id"]),("create",{"definition":DEFINITION},["definition"]),("update",{"id":integer(1,4294967295),"definition":DEFINITION},["id","definition"])]))
def range_of(s): return obj({"first":s,"last":s},["first","last"])
add("history_record",obj({"enabled":BOOL},["enabled"]))
add("history_clear")
add("history_find",obj({"pc":range_of(ADDR),"address":range_of(ADDR),"access":ACCESS,"direction":{**enum("forward","backward"),"default":"backward"},"limit":integer(1,64,32),"from":tagged("kind",[("oldest",{},[]),("newest",{},[]),("id",{"id":COUNTER},["id"])]),"epoch":COUNTER,"timeline":U32,"cycle":range_of(COUNTER),"value":PATTERN,"opcodes":{"type":"array","minItems":1,"maxItems":32,"items":PATTERN}}),readonly=True,description="Bounded recorded history query. read/data-read includes opcode,dummy,stack,vector reads; write/data-write includes dummy and stack writes; data selects ordinary data accesses only. Does not arm recording.")
add("history_next",obj({"cursor":TOKEN,"limit":integer(1,64,32)},["cursor"]),readonly=True)
add("history_read",obj({"id":COUNTER,"epoch":COUNTER,"before":integer(0,64,32),"after":integer(0,64,8)},["id"]),readonly=True)
add("history_close",obj({"cursor":TOKEN},["cursor"]))
add("frame_ring",tagged("action",[("info",{},[]),("record",{"enabled":BOOL},["enabled"]),("clear",{},[])]))
add("recorded_frame",obj({"target":{"oneOf":[obj({"frame":COUNTER},["frame"]),obj({"cycle":COUNTER},["cycle"])]},"path":PATH,"overwrite":OVERWRITE},["target"]),readonly=True)
add("inspector",tagged("action",[("enter",{},[]),("leave",{},[]),("land",{"cycle":COUNTER},["cycle"]),("land_exact",{"cycle":COUNTER},["cycle"])]),description="Change global Inspector mode/cursor explicitly. Land implies enter; leave restores live without resuming. Never equate history ids with Inspector clocks.")


def validate_tool(name, arguments, product=None):
    if name not in TOOLS: raise ValueError("unknown tool")
    spec=TOOLS[name]
    Draft202012Validator(spec["inputSchema"]).validate(arguments)
    if product and spec["product"] and product!=spec["product"]: raise ValueError("wrong product for tool")
    def walk(value):
        if isinstance(value,dict):
            for key,item in value.items():
                if isinstance(item,float) and (key!="mhz" or not math.isfinite(item)): raise ValueError("integer fields require JSON integers; MHz must be finite")
                if key in {"epoch","cycle","frame","after","count","id","first","last"} and isinstance(item,str) and item.isdecimal() and int(item)>2**64-1: raise ValueError("counter exceeds unsigned 64-bit")
                walk(item)
            if "first" in value and int(value["first"])>int(value["last"]): raise ValueError("range endpoints reversed")
        elif isinstance(value,list):
            for item in value: walk(item)
    walk(arguments)
    if "overwrite" in arguments and name in {"get_frame","recorded_frame"} and "path" not in arguments: raise ValueError("overwrite requires path")
    if name in {"read_memory","write_memory","export_memory"}:
        source=arguments.get("source","map")
        valid={"apple2":{"map","main","aux","lc1","lc2","rom"},"c64":{"map","ram","rom","drive8","drive9"}}
        if product and source not in valid[product]: raise ValueError("unsupported memory source")
        if name=="write_memory" and (source=="rom" or (product=="c64" and source not in {"map","ram"})): raise ValueError("read-only memory source")
        length=len(arguments["bytes_hex"] )//2 if name=="write_memory" else arguments.get("length",256)
        if arguments["address"]+length>65536: raise ValueError("memory operation would wrap")
    if name=="apple2_set_register" and arguments["name"]!="pc" and arguments["value"]>255: raise ValueError("register requires a byte")
    if name=="c64_execute" and "count" in arguments and int(arguments["count"])==0: raise ValueError("count must be positive")
    if name=="wait" and arguments["condition"]=="event" and product=="apple2" and arguments["event"] not in APPLE_EVENTS: raise ValueError("unsupported Apple event")
    if name=="apple2_assemble" and arguments.get("mli_launch") and arguments.get("reset",True): raise ValueError("mli_launch requires reset=false")
    if name=="c64_assemble" and arguments.get("auto_run") and arguments.get("basic_run"): raise ValueError("run flags are mutually exclusive")
    if name=="c64_binary_file" and arguments["action"]=="save" and not arguments["is_basic"] and arguments["start"]>arguments["end"]: raise ValueError("save range reversed")
    if name=="c64_paste" and len(arguments["text"].encode())>4096: raise ValueError("paste exceeds UTF-8 byte bound")
    if name=="history_find" and arguments.get("from",{}).get("kind")=="id" and int(arguments["from"]["id"])==0: raise ValueError("history id must be positive")
    if name=="breakpoints" and "definition" in arguments:
        d=arguments["definition"]
        if d.get("end",d["address"])<d["address"]: raise ValueError("breakpoint range reversed")
        if product=="c64" and ("swap_slot" in d or isinstance(d.get("mapping"),dict)): raise ValueError("Apple breakpoint fields on C64")
        if product=="apple2" and isinstance(d.get("mapping"),str): raise ValueError("Apple mapping requires independent axes")
        for c in d.get("conditions",[]):
            if (c["lhs"]=="mem") != ("address" in c): raise ValueError("condition address only required for mem")
            if c["lhs"]=="value" and d["access"]=="exec": raise ValueError("value unavailable for execution breakpoint")
            if product=="apple2" and c["lhs"] in {"raster","vic_cycle"}: raise ValueError("C64 clock term on Apple")
            if product=="c64" and c["lhs"]=="cycle_in_line": raise ValueError("Apple clock term on C64")
    return arguments

# Tool discovery must carry operational guidance; agent projects need no wire manual.
_DESCRIPTIONS={
'attach':'Attach observationally to an already-running localhost emulator control port. Consumes its exclusive client slot; never pauses, resets, resumes or enables recording. Fails for unsupported identity or occupied port.',
'launch':'Launch configured Apple or C64 profile and attach. Owns the child, which is stopped on clean adapter exit unless keep_on_exit=true. Profile controls executable and fixed argv; tools cannot inject shell commands.',
'connection_info':'Read cached connection identity, supported capabilities/memory sources, process ownership and unavailable state without emulator I/O. Cached observations are not fresh measurements.',
'health_check':'Ping the connected emulator through the serialized operation queue. Reports responsiveness, without changing execution.',
'detach':'Release the exclusive control socket without changing guest execution. Attached processes survive; owned processes require keep_running=true, explicitly relinquishing ownership.',
'stop_owned':'Terminate only the child identified by an ownership token, including after failed connection. Cannot terminate an independently attached emulator. Guest state can be lost.',
'abandon_owned':'Relinquish ownership of the live child and release its socket, returning PID/port/log paths. Leaves it running; adapter exit will no longer terminate it.',
'get_state':'Read native execution state, stop reason and live/Inspector mode with available clocks. This may use a native cache and is not atomic with subsequent CPU or memory tools.',
'get_cpu':'Read numeric 6502/6510 registers and decimal-string cycle clock from one native CPU response. Inspector returns historical focused state. Does not pause.',
'execute':'Explicit run/pause/reset/step operation. Reset destroys current state; replies usually mean queued acceptance, not completion. In Inspector, run/steps use sealed replay clamped to live. Use wait separately and heed sticky events.',
'wait':'Observe paused/running/frame/event using the native wait contract. Sticky event latches may precede this request, so success does not prove a specific earlier mutation finished. Cancellation can drain until the requested timeout; maximum 600 seconds.',
'read_memory':'Read up to 4096 bytes without address wrap; returns hex bytes. Sources are product-specific. Apple $C0xx reads peek memory and do not report softswitch state: use apple2_softswitches. Running samples are not cross-tool atomic.',
'write_memory':'Write at most 1024 bytes from even hex, without address wrap. Apple acknowledges queued writes; C64 waits for runtime write completion and auto-pauses. Inspector rejects mutations. C64 writable sources are map/ram; Apple excludes ROM.',
'export_memory':'Read one bounded memory range up to 65536 bytes and save raw binary under configured write roots, with size/hash metadata. Does not promise consistency across separate exports; overwrite must be explicit.',
'get_frame':'Return current full native raster as PNG, with optional saved copy under write roots. No pause/crop/rescale. Current raster may be partial; recorded_frame selects completed ring evidence.',
'find_symbol':'Look up an exact existing symbol name. Returns typed address or native not-ready/not-found. Does not import symbol files.',
'snapshot':'Save or load a native machine snapshot by absolute path. Load replaces state. Apple replies after runtime completion; C64 replies accepted, and completion-event waits are sticky rather than request-correlated. Save overwrite requires explicit permission.',
'events':'Read bounded sequence-tagged adapter/native event observations without waiting for the operation lock. Reports dropped events. This is awareness, not a synchronization lock or guaranteed mutation completion.',
'result_page':'Continue an immutable decoded large-result page by opaque token/offset. This token differs from a native history cursor; pages expire after detach, eviction or 15 minutes.',
'apple2_set_register':'Apple only: queue a register change; PC permits 16 bits and other registers permit one byte. Reply is acceptance. C64 has no wire set-register equivalent; Inspector rejects mutation.',
'apple2_set_turbo':'Apple only: set speed in MHz (0.001..10000) or max=true. Native queued acceptance; these are not C64 turbo ladder indices.',
'c64_set_turbo':'C64 only: choose normal or maximum speed. Queued acceptance; not an Apple MHz setting.',
'c64_execute':'C64 only: run bounded cycles/instructions, run to address or raster/cycle, or step a frame. Native breakpoints may stop advancement early. Reply is queued acceptance; inspect/wait separately.',
'apple2_softswitches':'Apple only: inspect latched softswitch flags, model and beam state without triggering I/O. Memory peeks at $C0xx cannot substitute for this tool.',
'c64_vic':'C64 only: inspect native VIC-II internal state and beam snapshot, including decimal raster/cycle and hexadecimal registers decoded numerically. Native cache freshness may be unknown; does not access deferred VIC rings.',
'c64_cia':'C64 only: inspect CIA1 or CIA2 snapshot, ports, timer counters/latches, interrupt fields and TOD/alarm. Reports only fields present in current wire response; no fabricated unreadable state.',
'c64_drive_cpu':'C64 only: inspect drive 8 or 9 native cached CPU/mechanical/media snapshot. Cache freshness is unknown unless native evidence says otherwise.',
'c64_disk_status':'C64 only: inspect drive 8 or 9 mounted/powered/writable/dirty status and image names from native state. Cache freshness is unknown unless supplied.',
'c64_call_stack':'C64 only: native heuristic call-stack evidence, with JSR and destination addresses. Not a reconstructed perfect language-level stack.',
'apple2_assemble':'Apple only: assemble an absolute host source. Address defaults $8000 and run address defaults to address. reset defaults true; mli_launch requires reset=false and native implies auto_run. Segment adjustment is optional. Includes/output directives use native filesystem access outside adapter sandbox.',
'c64_assemble':'C64 only: assemble an absolute host source; default address $8000 and reset=true. auto_run and basic_run are mutually exclusive. With reset=false, native auto_run sets PC without resetting CPU microcycle phase; explicitly finish a paused instruction before choosing a new PC. Returns native diagnostics and actual emitted origin. Native includes/output directives are not sandboxed by adapter roots.',
'c64_load_program':'C64 only: queue PRG loading from an absolute allowed path. Resets the machine, detaches cartridges, and temporarily runs the boot process before loading. Acceptance does not mean subsequent BASIC editor execution has completed; preserve evidence before calling.',
'c64_binary_file':'C64 only: queue binary load/save. Load pauses guest execution, may reset, and can use file header address. Non-BASIC save end is inclusive; BASIC save derives TXTTAB..VARTAB and writes PRG header, ignoring supplied bounds/header option. No frontend Basic Text flag. Save requires explicit overwrite.',
'apple2_media':'Apple only: mount/unmount explicit Disk II or SmartPort media; slot 0 resolves live card map and drives are 0/1. select/set_writable affect Disk II only. Mount path is explicit, never inferred from .po. Writable images and SmartPort directory HostFS may be changed by guest execution; path roots do not sandbox the guest. Reply is queued acceptance; Inspector rejects mutations.',
'c64_media':'C64 only: mount D64, eject, or set power for drive 8/9. Power-off ejects. Mounted writable images may be changed by guest execution. Reply is queued acceptance; no generic cartridge or G64 mount exists here.',
'apple2_key':'Apple only: inject one native byte keystroke. $8D or CR maps to Return. Queued acceptance; not a host keyboard event API.',
'c64_key':'C64 only: press/release a source-enumerated key or pulse Restore. Lowercase names describe C64 keys, not arbitrary host keys. Reply is queued acceptance.',
'c64_joystick':'C64 only: set port 1/2 direction/fire state; omitted buttons default released. Queued acceptance. No implicit timed pulse or automatic release.',
'c64_paste':'C64 only: queue up to 4096 UTF-8 bytes of text or native paste-event grammar using binary framing. Events text is a bounded native input script, not a free-form control command. Reply is acceptance.',
'breakpoints':'List/create/update/enable/delete/clear/rearm native breakpoints. Definitions are typed ranges, actions, counters and up to four AND conditions. Apple mapping axes and C64 map/ram/rom differ; value conditions require memory access. Listings preserve native/unknown fields. No rich frontend type-text or swap payload creation.',
'history_info':'Read native HST1 recording status, budgets, epoch/timeline and retained record bounds. Recording history differs from Inspector tape recording.',
'history_record':'Explicitly enable/disable HST1 history recording; does not arm Inspector tape. Changes evidence collection and may be rejected in Inspector.',
'history_clear':'Explicitly clear retained HST1 history, invalidating its native epoch/cursors and discarding debugging evidence.',
'history_find':'Search retained HST1 evidence with bounded filters and at most 64 matches. read/data-read includes opcode, operand, dummy, stack and vector reads; write/data-write includes dummy/stack writes; data means ordinary data accesses only. Decimal-string IDs/cycles differ from Inspector clocks. Does not arm recording.',
'history_next':'Continue a connection/session/epoch-bound opaque history cursor, up to 64 records. Stale cursors fail distinctly; never silently reruns the original query.',
'history_read':'Read a bounded window around native history record ID, up to 64 before/after. Optional epoch constrains the request. Marker records and partial/truncated evidence remain explicit.',
'history_close':'Release an opaque history cursor belonging to the current connection/session. Does not clear recorded evidence.',
'frame_ring':'Inspect, explicitly enable/disable recording, or clear completed-frame ring evidence. No automatic recording on attach. Clear discards retained screenshots.',
'recorded_frame':'Select completed frame-ring evidence by frame number or native cycle; return full raster PNG and optional saved copy. No crop/rescale. Recording must already have retained the target.',
'inspector':'Explicitly enter/leave or land global Inspector state. Land implies enter; leave restores live without resuming. Apple tape clocks and C64 film_cycle differ from HST1 record IDs. Mutations must obey native read-only rejection; no UI-only catalog/record-toggle tools.',
}
for _name,_description in _DESCRIPTIONS.items():
    _product=TOOLS[_name]['product']
    TOOLS[_name]['description']=_description+' Require one designated emulator-owner agent when collaborating; parallel analysis may use captured results.'
