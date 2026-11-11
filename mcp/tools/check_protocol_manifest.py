#!/usr/bin/env python3
"""Fail on source inventory/vocabulary drift; run from any directory."""
from pathlib import Path
import re
import hashlib
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'mcp/src'))
from machines_mcp.schemas.protocol_manifest import MANIFEST, DEFERRED, DELIVERY, AUDITED_SOURCE_REGIONS, HISTORY_OPTIONS, BREAKPOINT_ACTIONS
from machines_mcp.schemas.tools import KEYS, ACCESS, CONDITION, APPLE_EVENTS, C64_EVENTS

def strings(text): return re.findall(r'"([^"\n]+)"',text)
def check(root=ROOT):
    # UTF-8 explicitly: Windows runners otherwise decode with the ANSI code page and reject comment punctuation.
    def load(path): return (root/path).read_text(encoding='utf-8')
    for region in AUDITED_SOURCE_REGIONS:
        text=load(region['path'])
        if region['start'] is not None:
            start=text.index(region['start']); text=text[start:text.index(region['end'],start)]
        actual=hashlib.sha256(text.encode()).hexdigest()
        assert actual==region['sha256'],f"{region['path']}: audited control semantics changed; review serializers, delivery and rejection paths before updating AUDITED_SOURCE_REGIONS"
    for product,manifest in MANIFEST.items():
        identity=load(f'src/{product}/control/control_protocol.h')
        assert re.search(r'#define CONTROL_PROTOCOL_VERSION "([^"]+)"',identity).group(1)==manifest['protocol'],f'{product}: identity version drift'
        text=load(f'src/{product}/control/control_verbs.c')
        rows=re.findall(r'\{ \{ ("[^"]+"|NULL), ("[^"]+"|NULL), ("[^"]+"|NULL),',text)
        verbs=[a.strip('"') for a,b,c in rows if a!='NULL']
        assert verbs==manifest['verbs'],f'{product}: named verb inventory drift'
        caps=sorted({v.strip('"') for a,b,c in rows for v in [b,c] if v!='NULL'})
        assert caps==manifest['capabilities'],f'{product}: capability drift'
        vc={a.strip('"'):b.strip('"') for a,b,c in rows if a!='NULL' and b!='NULL'}
        assert vc==manifest['verb_capabilities'],f'{product}: verb capability drift'
        sources=re.findall(r'\{ \d+u, "[^"]+", "([^"]+)", 0u, 0x10000u, ([^}]+)\}',text)
        assert {s:{'writable':'MEMSRC_WRITABLE' in flags} for s,flags in sources}==manifest['sources'],f'{product}: memory-source drift'
        assert set(DELIVERY[product])==set(verbs)
        assert DEFERRED[product]<=set(verbs)
    text=load('src/c64/control/control_protocol.c')
    key_block=text.split('static const char *const names[] = {',1)[1].split('};',1)[0]
    assert strings(key_block)==KEYS,'C64 key vocabulary drift'
    text=load('src/shell/runtime/runtime_history_query_parse.c')
    options=text.split('k_find_option_keys[] = {',1)[1].split('};',1)[0]
    assert strings(options)==HISTORY_OPTIONS,'history option vocabulary drift'
    block=text.split('"execute"',1)[1].split('};',1)[0]
    assert ['execute',*strings(block)]==ACCESS['enum'],'history access vocabulary drift'
    text=load('src/shell/runtime/runtime_breakpoint_condition.c')
    block=text.split('runtime_bp_lhs_names[] = {',1)[1].split('};',1)[0]
    assert set(strings(block))|{'mem'}==set(CONDITION['properties']['lhs']['enum']),'breakpoint term drift'
    text=load('src/apple2/control/control_dispatch.c')
    block=text.split('static const char *event_name(',1)[-1] if 'static const char *event_name(' in text else text[text.index('return "running";',text.index('post_media_ok')):]
    # Narrow to published event-name map (switch immediately before parse_wait_event).
    start=text.index('return "running";',text.index('post_media_ok'))
    end=text.index('return "assemble-error";',start)+len('return "assemble-error";')
    assert set(re.findall(r'return "([^"]+)";',text[start:end]))==set(APPLE_EVENTS),'Apple wait-event vocabulary drift'
    text=load('src/c64/main.c')
    block=text.split('static const char *control_runtime_event_name(',1)[1].split('static void control_format_cpu_response',1)[0]
    assert set(re.findall(r'return "([^"]+)";',block))-{'none'}==set(C64_EVENTS),'C64 wait-event vocabulary drift'
    for path,function in [('src/apple2/control/control_breakpoint.c','static bool parse_breakpoint_actions('),('src/c64/main.c','static bool control_parse_breakpoint_actions(')]:
        text=load(path); block=text.split(function,1)[1].split('\n}',1)[0]
        assert re.findall(r'strncmp\(start, "([^"\n]+)"',block)==BREAKPOINT_ACTIONS,f'{path}: breakpoint action vocabulary drift'
    return {p:len(m['verbs']) for p,m in MANIFEST.items()}

if __name__=='__main__': print('Protocol manifest verified:',check())
