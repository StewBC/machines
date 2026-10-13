# Shared emulator MCP adapter

Author: Designer subagent · Date: 2026-10-12 · Status: FINAL — independently reviewed design; implementation authorized and completed 2026-10-12; independently reviewed

## Version scope

This document retains the **original v0.1 release plan** for implementation history.
Its initial inventory and deferred-feature list describe that release, not current
adapter coverage. **v0.2 implements VIC rings, bulk debug-memory/writer-PC snapshots,
full retained-history export and disassembly**; see the [landed addendum](debugging-extensions.md)
and [current usage guide](../../mcp/README.md). Native history cursor behavior below
has been corrected to the source-verified single-cursor contract.

## Decision and goals

Build one separately installable Python package in top-level `mcp/`, exposing a local stdio MCP server to Codex and attaching to either A2M/16 or C64M/10 over existing localhost TCP. No emulator implementation changes are required for the first release. Preserve direct-script access when the adapter is disconnected. The user authorized this scoped implementation on 2026-10-12 after discussion of the omissions and agent coordination.

The user has accepted Codex first, both attach and launch, exclusive control connections, honest state/completion contracts, bounded large results, schema maintenance, local file/process policy, and comparing native MCP. The release must support investigation of an independently running game without altering it on attachment. MCP discovers tools, not emulator processes or hardware meaning.

The first release includes execution, CPU/memory, complete wire-expressible breakpoint definitions, assembly and supported loading, screenshots, Apple softswitches, C64 VIC/CIA/drive inspection, input, supported media, snapshots, symbols, HST1 queries, frame rings, and wire-exposed Inspector navigation. The extra evidence surfaces avoid a nominal debugging release unable to inspect recorded pre-failure evidence. Their inclusion increases implementation scope deliberately. C64 debug-memory bulk/writer-PC-history arrays and VIC rings were deferred in v0.1, not silently omitted; see the exhaustive inventory.

## Evidence and repository fit

Source inspected: `src/{apple2,c64}/control/control_verbs.c`, their `control_protocol.{c,h}`, Apple `control_dispatch.c`, shared framing/table/memory-source files, `src/shell/runtime/runtime_history_query_parse.{c,h}`, `tools/apple2/a2m_control_client.py`, `tools/c64/c64_control_client.py`, product READMEs, `agents/shell/control.md`, product control notes, Apple time-machine and C64 runtime-control contracts. Source paths in some older notes omit product directories; use the actual paths above. No applicable AGENTS.md was found in repository/ancestors.

The emulators are portable C99; `src/` is reserved for emulator/linked C. Keep Python and packaging outside CMake's mandatory dependency path. Existing scripts remain independent; do not move or wholesale refactor their clients. Reuse audited decoding algorithms by extraction/copy with attribution and fixture equivalence tests, rather than importing scripts via mutable `sys.path`.

A2M has one deferred slot. C64 has sixteen but only CPU/memory RPCs can genuinely overlap; waits and other deferred commands remain exclusive. Both bind localhost, admit one client, bind a runtime session, and emit id-zero state-changed events. Capabilities are static tokens from verb tables, not an exact per-verb schema. `set-reg` is real on Apple despite lacking a capability token. Never deduce its absence from capabilities.

## External versus native MCP

| Issue | External stdio adapter | Shared native MCP endpoint |
|---|---|---|
| Attach to already running GUI | Launch adapter, then connect to TCP | Native HTTP endpoint is natural; native stdio alone cannot be attached by Codex after independent GUI launch |
| Launch | Adapter manages a child and its port | A stdio emulator could be launched directly; HTTP variant still needs a launcher/connector |
| Dependencies | Python/SDK outside emulators | HTTP/JSON/schema/MCP lifecycle dependencies linked into portable C99 |
| Images and trace decoding | Host presentation module, independently testable | More host presentation and protocol code inside process |
| Failure isolation | Adapter crash loses connection, leaves attached emulator | Server defect shares emulator address space |
| Shipping | Extra package/configuration and Python | Potentially one executable, but platform packaging gains dependencies |
| Contract accuracy | Requires tested translation/version manifest | Direct runtime APIs reduce wire translation, but tools still need their own semantics/versioning |

Choose external for stdio attach/launch symmetry, maintaining C99 portability, isolation, and reuse across products. Native MCP is feasible and could win if a single executable becomes a priority; it would preserve runtime ownership and deferred semantics rather than bypass them. This choice is not based solely on backward compatibility. No native HTTP, proxy, emulator discovery service, or dual MCP architecture is included in v1.

## Runtime, packaging, portability, and topology

Python >=3.11, `asyncio`, official `mcp` Python SDK, and a project-local locked dependency set. Use SDK protocol/lifecycle/cancellation implementations, not handwritten JSON-RPC. Prefer the SDK low-level server where needed to control schemas and result blocks. Pin a tested stable SDK release during the first implementation compatibility spike and commit its exact resolved versions; do not specify a guessed future version or float `latest`. Core PNG encoding uses `struct`/`zlib`; no Pillow or native imaging dependency is necessary. Tests use pytest plus async support in the dev dependency group.

Package `machines-emulator-mcp`, executable `machines-mcp`; install from `./mcp` into a dedicated venv (or pipx). Use the venv executable's absolute path in Codex configuration, avoiding startup downloads or dependence on a project working directory. `python -m machines_mcp` is an equivalent diagnostic entry point. A maintained sample configuration and installation guide live in `mcp/README.md`. Python packaging is optional to building emulators; CI adds a separate job.

Target macOS, Linux, Windows. Pure adapter transport/codecs tests must run on all three. Real emulator integration runs wherever builds/ROMs are available; macOS Codex desktop end-to-end is the initial product acceptance target. Windows/Linux packaging support is required, but successful Codex desktop behavior on those hosts is not implied by macOS testing. Record tested OS, Python, SDK, Codex version, and negotiated MCP protocol in release notes.

One MCP client connection and at most one emulator connection per adapter process. Static tools for both products are listed even before attach; descriptions state availability and calls reject the wrong product. `connection_info` reports actual supported surfaces. This avoids reliance on dynamic tools/list changes being refreshed by every client. To operate two emulators concurrently, configure two named MCP server entries; each starts an independent adapter. No multiplexing, shared ownership, or multi-agent arbitration is promised. Parallel analysis agents are supported, but one designated agent owns emulator interaction during an investigation. Other agents analyze source and captured artifacts or request measurements from that owner. Serialization prevents wire collisions, not conflicting intentions; tool descriptions and setup guidance state this coordination rule and connection conflicts return clear diagnostics.

Use a single async reader for framing, an async request lock, process-watcher task and bounded event queue. Serialize all wire requests in v1, including C64 reads: correctness before pipelining. MCP can receive concurrent calls; a per-process execution lock allows one emulator operation plus bounded queued calls (max 8); lifecycle conflict returns `busy`. Connection_info/events reads do not wait for the operation lock. Encode at most one bounded frame synchronously initially; add `asyncio.to_thread` only if measured event-loop delay exceeds 50ms under maximum frame conversion. No permanent worker pool is needed by design.

## Configuration and local boundaries

Adapter CLI accepts `--config <absolute TOML path>` only plus logging/version options. Configuration contains named launch profiles: product, executable absolute path, cwd absolute path, headless default, INI path or noini, supported startup settings (Inspector/history enablement and budgets), and fixed additional argv supplied by the human configuration. Tool calls select profiles, never inject an executable or arbitrary shell command. Spawn with an argv array, no shell. CLI flags and history defaults are product-specific and must be validated against app_options and --help before profile examples are shipped.

Configuration declares absolute `read_roots`, `write_roots`, and artifact directory; paths outside them are rejected with the remedy of updating local configuration. Resolve symlinks and Windows case/junction behavior before containment tests; nonexistent output paths resolve through existing parent directories. Reject CR/LF/NUL in every wire text argument, and reject serialized headers >= the source framing limit (512 bytes including framing overhead). Do not truncate. A path containing spaces is allowed according to each actual parser; no invented quoting convention. Assembly includes and assembler output directives can access/write further host paths inside emulator execution, so adapter roots are admission rules for explicit tool paths, not a sandbox. Document this limitation; OS sandboxing is outside v1. Never silently create unrestricted roots.

Attached instances must be configured with a cwd explicitly if relative guest/host paths are involved; tools themselves accept only absolute host paths. Save/export requires `overwrite=false` default and rejects existing files; explicit true is allowed. Mounted writable images and directory HostFS can be changed by guest execution; returned media metadata and tool descriptions say so. Config profile controls startup options; there is no arbitrary live configuration editor. Printer output remains governed by emulator settings.

## Lifecycle and preservation

State machine: DISCONNECTED → CONNECTING → ATTACHED or OWNED → DISCONNECTING → DISCONNECTED; failures carry last known identity/evidence and mark connection state UNAVAILABLE. Wire teardown does not clear child ownership. Track a separate owned-process record (opaque ownership token, process handle/PID, profile, logs, keep_on_exit) until confirmed exit, successful stop, or explicit abandonment. If an owned child remains alive after startup/handshake/wire failure, retain that record in UNAVAILABLE, include its ownership token in the error and connection_info, and block new attach/launch until stop_owned or abandon_owned (or observed child exit). A failed attempt to terminate retains ownership and reports cleanup failure; never silently transition to reusable DISCONNECTED while a live owned child is hidden. Connection identifiers are opaque per-generation strings, invalid after reconnect. No automatic reconnect or retry of mutating requests.

`attach(port, expected_product?)` connects to 127.0.0.1, sends hello and capabilities, validates exact identity/version, then reads get-state. It must not pause, reset, run, leave Inspector, clear history, enable recording, load files, inject keys, or adjust turbo. Return mode, running state, clock metadata, supported features, and existing recording availability where read-only status verbs allow it. Subsequent observations can differ if the emulator is running. Attach is observational but consumes the exclusive control slot and creates its runtime session; it is not literally invisible to control infrastructure.

Handshake deadline 3s. A busy emulator may accept/queue a TCP connection without speaking; classify handshake timeout as unavailable-or-control-occupied, not proof of protocol mismatch. Always close failed connections. Exact supported versions initially A2M/16 and C64M/10; future/older versions rejected before mutations, with detected/expected versions and adapter upgrade guidance. Capability mismatch disables affected features and returns diagnostics, never best-effort guessing. Product identity cannot be switched while connected: detach first.

`launch(profile, port?, headless?)` starts a configured executable in its configured cwd, ensures a control port, captures stdout/stderr into bounded rotating artifacts, and attempts handshake until startup deadline 15s. Default port comes from profile; an explicit port may override. Check loopback availability before spawning but acknowledge bind race; fail cleanly if child exits/bind fails. No automatic attachment to an unrelated process occupying that port: establish that the listening socket belongs to the recorded child PID before connect and recheck after hello, while also checking child liveness/identity. Platform ownership providers: Linux resolves the LISTEN entry in `/proc/net/tcp` to a socket inode and finds that inode in `/proc/<child>/fd`; macOS invokes system `/usr/sbin/lsof -nP -a -p <pid> -iTCP:<port> -sTCP:LISTEN -Fpn` without a shell; Windows calls `GetExtendedTcpTable` via ctypes with TCP_TABLE_OWNER_PID_LISTENER and verifies loopback local port/PID. Require unambiguous exact child ownership. Missing permission/provider or inconclusive observation fails closed as launch-ownership-unverified, terminates only the recorded child, and suggests independent launch+attach instead. Ownership probes have 2s bounded deadlines within overall startup deadline. A successful child-alive check or matching product identity alone is insufficient. A reserved socket cannot be passed to current emulators, so profile ports remain the predictable v1 solution rather than claiming race-free auto allocation. Startup does not load a program automatically; use explicit subsequent tools, keeping lifecycle and mutation distinct.

`detach` closes cursors/socket without altering execution; never kills an attached process. For an owned process, detach is allowed only with explicit `keep_running=true`, converting ownership to abandoned; logs/port/pid returned for later attach. `stop_owned(ownership)` works in OWNED or UNAVAILABLE independently of an active connection id; it terminates only the recorded child PID/process handle, first ordinary termination then 3s wait then kill. `abandon_owned(ownership)` explicitly relinquishes a live child without touching its execution, closes any socket, returns pid/port/logs, and transitions to DISCONNECTED. Tokens are never accepted for an unowned attached process. No tool can terminate an attached emulator. Owned-child output uses durable file descriptors, not adapter-dependent pipes. While owned, logs are periodically bounded; after explicit abandonment or keep_on_exit, durable logs may grow and become the caller’s responsibility. On MCP EOF/adapter normal shutdown, leave attached instances running; terminate all retained owned children, including those in UNAVAILABLE, unless launch specifies `keep_on_exit=true` (default false). On adapter crash, owned children may survive: report this operational limitation and provide pid/log records, no promise of crash-proof cleanup. Disconnect does not necessarily undo queued runtime mutations.

If game execution hangs but emulator responds, inspect normally. If process died/unresponsive, return unavailable and retain logs; no automatic restart, pause, or reconstruction. Pre-failure history/Inspector data exists only if already recorded and retained. No promise to recover vanished state or enable a control port retroactively.

## Tool schema conventions and result contract

All schemas are JSON objects with `additionalProperties:false`, named required fields, descriptions, and explicit defaults. The tables below specify complete public fields; optional fields are marked `?`. Names are stable snake_case. All connected tools except ownership-token lifecycle operations require `connection` (opaque id) to prevent accidental calls against a replacement session. Unsupported product/source/action combinations fail before wire I/O.

Types: `Addr` integer 0..65535; `Byte` integer 0..255; `U32` integer 0..4294967295; `Counter` decimal string representing unsigned 64-bit (avoid JSON precision loss); `Timeout` integer 1..600000, default 2000; `Path` absolute host path; `Range<T>` object `{first:T,last:T}` inclusive ordered endpoints; ids/cursors string tokens. CPU addresses/registers are numeric with optional formatted hex companions; clocks/epoch/frame/history ids are Counter. Do not parse all wire numbers with one base: register fields often bare hex, counters decimal; product parsers define the mapping.

Every result carries `{connection, product, protocol, outcome, state_context, data}`. `outcome` is `completed`, `accepted`, `observed`, or `unknown`; `state_context` contains available mode, focus_cycle, frame/cycles/epoch and `sample_consistency` (single_response or multiple_nonatomic). Missing values remain absent, never fabricated. Native sample metadata and cached last-observed metadata are separate fields; cached context carries adapter observation sequence/time and is never represented as the current reply’s measurement. MCP structuredContent mirrors a concise text JSON result for clients that only consume text. Frames add MCP image content with mimeType image/png. Domain/connection errors use `isError:true`, with `{code,message,wire_code?,operation_effect:"none"|"possible"|"known",retry_safe}`. Malformed tool parameters are SDK/schema errors. Never expose an emulator rejection as successful work.

Read-only annotations are hints, not enforcement. Mutating tools describe auto-pause/reset/file/Inspector effects explicitly. No raw send_command escape hatch; it would bypass typed contracts and file policy.

### Lifecycle and common tools

| Tool | Fields after common connection (if applicable) | Meaning/result |
|---|---|---|
| `attach` | no connection; port integer 1..65535, expected_product? enum apple2/c64 | Observational handshake; connection metadata |
| `launch` | no connection; profile string, port? 1..65535, headless? bool, keep_on_exit? bool=false | Own child; metadata/log paths |
| `connection_info` | connection? | Local cached status, actual features/sources, owned pid/profile, unavailable state; no wire call |
| `health_check` | none | Wire ping through operation lock; observation only |
| `detach` | keep_running? bool=false | Close; attached untouched; owned requires keep_running=true |
| `stop_owned` | no connection; ownership opaque string | Terminate recorded owned child in OWNED/UNAVAILABLE only |
| `abandon_owned` | no connection; ownership opaque string | Explicitly relinquish live child; close socket, return pid/port/logs |
| `get_state`, `get_cpu` | none | Native state / typed 6502 CPU snapshot |
| `execute` | action enum run/pause/reset/step_cycle/step_instruction/step_over/step_out | Dispatch once; explicit accepted/completed class |
| `wait` | condition tagged union paused/running/frame/event; delta? U32>=1 only frame; event? source-enumerated string only event; timeout_ms? Timeout | Explicit wait; result includes sticky semantics and actual stop reason |
| `read_memory` | address Addr, length integer 1..4096 default 256, source enum map/main/aux/lc1/lc2/rom/ram/drive8/drive9 | Product-specific source validation; hex string bytes, length, source; address+length<=65536 |
| `write_memory` | address Addr, bytes_hex even hex 1..1024 bytes, source enum map/main/aux/lc1/lc2/ram | No wrap: address+length<=65536; Apple excludes rom; C64 map/ram only; completion caveat below |
| `export_memory` | address Addr, length 1..65536, source as read_memory, path Path, overwrite? bool=false | Fetch once, bounded full space export; byte count/hash/path; no huge inline bytes |
| `get_frame` | path? Path, overwrite? bool=false (valid only with path) | Current full raster PNG and original dimensions/stride/clock; may be partial |
| `find_symbol` | name string nonempty within wire bound | Exact symbol lookup, typed address or not-ready/not-found |
| `snapshot` | action enum save/load, path Path, overwrite? bool=false for save only | Native machine snapshot; product completion class |
| `events` | after? Counter, limit? 1..128 default 32 | Adapter event log, next sequence and dropped_before; never a lock |

Wait event enum for Apple: running, paused, step-complete, run-complete, reset-complete, breakpoints, frame, assemble-complete, assemble-error (the event-name mapping in control_dispatch.c). C64 additionally exposes pong, started, stopped, error, cpu-state, machine-state, memory, memory-view, disk-status, call-stack, disk-swap, debug-memory, save-state-complete, load-state-complete, history-status, history-result, session, state-changed as mapped by control_runtime_event_name; only documented sticky events are persistent. Source drift tests check this vocabulary. Wait observes these names; it does not promise useful events will be produced for every idle system state.

Tool forms `wait` and `execute` keep execution and observation separate. Every per-verb deadline starts when dispatched on the wire, not when queued for the operation lock. Immediate/ordinary RPC budget is 3s (native deferred usually 2s plus transport margin), assembly and history-find 11s (native 10s), Apple save/load-state 6s (native 5s), and explicit wait timeout_ms plus 1s. MCP tool duration for a wire request defaults to that dispatch budget, with explicit wait timeout honored up to 600s. Codex configuration's tool timeout must cover intended long waits; initial setup recommends bounded 30s waits, not a universal 600s default.

### Product execution, hardware, assembly, media, and input

| Tool | Fields | Contract |
|---|---|---|
| `apple2_set_register` | name enum pc/a/x/y/sp/p, value Addr (Byte except pc) | Apple only; queued acceptance, no C64 substitute |
| `apple2_set_turbo` | oneOf `{mhz:number>=0.001 and <=10000}` / `{max:true}` | Serialize MHz using product accepted syntax; do not expose ladder indices |
| `c64_set_turbo` | mode enum normal/max | Wire 1/2; normal/max are product modes, not MHz |
| `c64_execute` | action enum run_cycles/run_instructions/run_to/step_frame/run_to_raster; count? Counter>0 for counted actions; address? Addr for run_to; raster? Addr, cycle_in_line? Addr for raster | Strict tagged schema; report native advancement/breakpoint stop; no invented Apple equivalents |
| `apple2_softswitches` | none | Latched state/beam; C0xx memory is not softswitch state |
| `c64_vic`, `c64_call_stack` | none | Internal VIC state / native stack heuristic, not reconstructed from registers |
| `c64_cia` | index enum 1/2 | Hardware snapshot incl. unreadable mask |
| `c64_drive_cpu`, `c64_disk_status` | device enum 8/9 | Native cached snapshot/status; label cache freshness as reported/unknown |
| `apple2_assemble` | path Path, address? Addr=32768, run_address? Addr defaults address, auto_run? bool=false, mli_launch? bool=false, reset? bool=true, auto_adjust_segments? bool=false | Product-specific assembly contract; path dependencies/output sandbox caveat applies |
| `c64_assemble` | path Path, address? Addr=32768, run_address? Addr defaults address, auto_run? bool=false, basic_run? bool=false, reset? bool=true | Reject both run flags true; result actual emitted origin, not default origin |
| `c64_load_program` | path Path | load-prg; accepted only, do not claim BASIC editor execution complete |
| `c64_binary_file` | action load/save, path Path; load: address Addr,use_file_address bool,reset_first bool,is_basic bool; save: start Addr,end Addr,write_file_address bool,is_basic bool,overwrite? bool=false | Non-BASIC save end is inclusive, requires start<=end; BASIC save ignores supplied start/end/header fields and derives TXTTAB..VARTAB exclusive end, writes PRG header; no Basic Text frontend flag; accepted only |
| `apple2_media` | action mount/unmount/select/set_writable; kind enum diskii/smartport required mount/unmount; slot? integer 0..7=0; drive? enum 0/1=0; path? Path mount; index? integer>=1 select; writable? bool set_writable | Tagged fields; slot 0 resolves live card map; select/writable Disk II only; never guess .po kind |
| `c64_media` | action mount/unmount/power; device enum 8/9; path? Path mount; powered? bool power | Explicit D64 mounting wire surface; power-off ejects; no generic Apple mapping |
| `apple2_key` | value Byte | Single Apple character; 0x8D/CR Return semantics native |
| `c64_key` | action enum down/up/restore; key? enum generated from parse_key_token vocabulary | No key for restore; exact source key vocabulary, not invented host key names |
| `c64_joystick` | port enum 1/2, up/down/left/right/fire bool each default false | Typed direction fields → native mask 0..31 |
| `c64_paste` | kind enum text/events, text string UTF-8 1..4096 bytes | Binary data wire forms; events uses native documented grammar (bounded string, not free-form wire command); validate framing/limit against source |

Apple drive numbers are zero-based 0/1 for both exposed media kinds; slot 0 is represented by omitted native slot tokens, because explicit native slots must be 1..7. Always serialize explicit kind and absolute path. C64 key enum is a..z, 0..9, space, return, delete, left-shift, right-shift, plus, minus, asterisk, equals, colon, semicolon, comma, period, slash, at, cursor-right, cursor-down, home, run-stop, control, commodore, left-arrow, up-arrow, pound, f1, f3, f5, f7 (case-sensitive lowercase).

### Breakpoints

`breakpoints` has action list/create/update/enable/delete/clear_all/rearm. list/clear_all/rearm take no other fields; enable takes id U32>=1 and enabled bool; delete takes id; create/update take definition, update also id. No shorthand raw definition string.

Definition: `{access:exec|read|write|read_write,address:Addr,end?:Addr,enabled?:bool=true,actions?:array(unique enum break/fast/slow/type/swap),counter?:U32,reset?:U32=1,swap_slot?:integer 0..7=6 (Apple only),conditions?:array(maxItems=4),mapping?:...}`. Empty actions maps to `none`; default `[break]`. A condition is `{lhs: a|x|y|sp|p|n|v|b|d|i|z|c|value|mem|raster|vic_cycle|cycle_in_line, address?:Addr only mem, op: eq|ne|lt|gt|le|ge|mask_set|mask_clear, value:Addr}`. Reject value with exec. C64 allows raster/vic_cycle and mapping enum map/rom/ram; Apple allows its published clock terms and mapping object `{ram:map|main|aux,c100:map|rom,d000:map|lc1|lc2|rom}`. Reject other product terms. Apple swap_slot serializes as `swap-slot=N`; reject that field on C64. The default 6 matches Apple native default, and output preserves swap_slot plus existing swap_param/swap_relative fields even though the latter cannot be set through wire. Map operators deterministically to bounded native AND grammar, no OR. Output preserves id, mapping, access bits, actions, guards and counters/hits. C64 access bits are exec=1/read=2/write=4; do not use VICE numbering. Rich frontend type text/swap parameters lack a complete wire representation and remain unsupported; listing must preserve any unknown raw fields rather than erasing them.

### Recorded evidence (included)

`history_info` no fields. `history_record(enabled:bool)` and `history_clear` are explicit mutations. `history_find` fields: pc? Range<Addr>, address? Range<Addr>, access? enum from shared find-access table, direction? forward/backward=backward, limit? integer 1..64=32, from? tagged `{kind:oldest|newest|id,id?:Counter}`, epoch? Counter, timeline? U32, cycle? Range<Counter>, value? string matching `[0-9A-Fa-f?]{2}` (one byte with nibble wildcards), opcodes? array 1..32 of that same pattern. `history_next(cursor:string,limit?1..64=32)`, `history_read(id:Counter,epoch?:Counter,before?0..64=32,after?0..64=8)`, `history_close(cursor:string)` expose bounded HST1 results. Omitted filter fields use native defaults. `from.id` is required only for kind=id and must be nonzero. Serialize value with `$` prefix; join opcode patterns by commas without spaces. Range fields support native singleton queries by first=last. Access enum is execute/fetch/opcode/operand/data-read/data-write/read/write/data/dummy-read/rmw-dummy-write/stack-read/stack-write/vector-read. Source meanings matter: data-read aliases read (including opcode/dummy/stack/vector reads), data-write aliases write (including RMW dummy/stack writes); data is ordinary data read/write only. Query descriptions preserve these meanings.

Decode HST1 v1 little-endian header/records/accesses; validate magic/version/reserved fields, record sizes/count, payload boundary and response epoch/count. Return all decoded fields including partial/access_truncated/timing_truncated/anchor_match flags, marker kind+args and bus access kind+cycle offset. Media marker 13 is not an ordinary instruction. Preserve raw enum ids plus known names. Opaque cursors bind connection generation, runtime session, epoch and query. CURSOR_STALE is returned distinctly: never silently re-find and misrepresent continuation. Cursor closes on detach. Native source supports one cursor per control session: a successful new find replaces the previous query, exhausted continuation retires its token, and full export rejects an outstanding query. The originally proposed multi-cursor capacity is superseded by this source-verified contract.

`frame_ring(action:info|record|clear,enabled?:bool only record)` and `recorded_frame(target:oneOf {frame:Counter}/{cycle:Counter},path?:Path,overwrite?:bool=false only with path)` expose status and PNG evidence. No automatic recording enablement. `inspector(action:enter|leave|land|land_exact,cycle?:Counter only land variants)` exposes existing navigation. Mode is global; land can imply enter, leave restores live and does not resume. Run/step in Inspector are sealed replay clamped to live; mutating calls must pass through native read-only rejection. Recorder startup availability comes from launch profile/INI, not an invented record control verb. Apple F/S clocks and C64 film_cycle are separate; never equate HST1 ids to Inspector cycles. Native Inspector UI catalog and ± controls without wire verbs are unsupported.

## Completion, consistency, events, cancellation

Implement a source-derived delivery manifest keyed by product/verb: `immediate_observation`, `runtime_completed_reply`, `queued_acceptance`, and `wait`. Audit main dispatch and runtime rejection paths, not only text `ok`. Examples: both execute controls largely return accepted; both assemble reply deferred with diagnostics; Apple save/load-state reply deferred (5s), C64 snapshot commands return accepted and have completion waits; C64 set-memory waits for write completion, Apple set-memory immediately replies after posting writes. Apple set-reg also acceptance. Apple immediate write ok does not prove every queued byte was applied; mark accepted even if text lacks accepted=1. An adapter cannot upgrade those guarantees without an emulator change.

V1 returns these honestly and provides explicit wait tools; it does not automatically consume stale sticky events and label asynchronous file operations completed. Wait reports observed native event/sequence, not a request-correlated completion unless wire source provides that correlation. Assembly error/success events and prior snapshot latches can persist; sequential requests do not make old latches new. UI mutations remain possible at all times. Future stronger composite operations need an emulator sequence/token contract or carefully verified operation-specific freshness baseline, not guesswork.

Reads while running are sampled, not atomic cross-tool snapshots. get-state cache, CPU response, frame and hardware state may have different timestamps. Do not auto-pause for inspection. An explicit execute pause plus wait paused may establish a useful barrier, but UI mutation can invalidate it and every result keeps its native timestamps. Frame metadata may not prove completeness; current frame returns `completeness:unknown_or_partial` unless native evidence proves complete. Recorded frame is a completed-ring sample. PNG honors row stride: Apple 560×192 width*4, C64 PAL 504×312 / NTSC 520×263 with ARGB stride 2080. Decode byte order according to current clients/native source; test color swatches and padding. Full raster, no frontend crop or rescale in v1.

Wire reader continuously demultiplexes id-zero events and exactly framed data, one pending wire request in v1. Bound header 512, payload 8MiB global and tighter per-kind expected sizes; validate exactly byte_count bytes and delimiter. Never read binary through readline. Events stored as sequence-tagged bounded deque (256), dropped count exposed. Do not convert every event into MCP notifications; events tool is portable and quiet. Emitted metadata is awareness, not a lock or state restoration instruction.

Cancellation while queued removes call. Cancellation after dispatch never rolls back a mutation or implicitly pauses execution. Continue draining that outstanding reply under its deadline before releasing the lock; any effect is possible and visible on subsequent state. Cancellation of a long wait may require waiting its requested deadline; document this v1 limitation. If framing/deadline fails, close connection and mark unavailable, invalidate cursors, fail queued calls; attached process remains running. Do not reconnect/retry automatically. Disconnect cancels wire deferred session work but already queued runtime effects may occur. Client cancellation may preclude returning an outcome; retain operation-effect status in local events/logs.

## Large results and artifacts

Inline memory max 4096 bytes, write max 1024 bytes, history max 64 results and window 129 records. Enforce returned text budget 64KiB: if a decoded page exceeds it, retain full page as an artifact and return bounded preview plus an opaque continuation over that immutable decoded page; never discard records behind a live native cursor. Local immutable page tokens expire on detach or after 15 minutes, bounded 16MiB aggregate; evict oldest with explicit expired error. Query cursor and local presentation continuation are distinct named fields/tools (`result_page(token,offset>=0,limit1..64=32)`). Full history export is a future feature; do not imply limitless export or snapshot-consistent scan.

Artifacts use configured directory, generated names, atomic adapter writes, metadata size+SHA256+format and explicit paths. Keep bounded files (default quota 256MiB) with clear quota error; do not silently delete caller-owned exports. Adapter-owned logs/temporary pages may rotate. PNG sent inline plus optional saved copy when caller requests `path` and overwrite fields (get_frame/recorded_frame schemas include these optional fields). Images max native dimensions, no arbitrary unbounded transform. Memory export is raw binary. Large C64 debug-memory arrays are deferred rather than chunked into fabricated snapshot consistency.

## Exhaustive source-derived wire inventory

Status I = included in first release (including internal use or typed alias folding); D = deferred with explicit reason; U = unavailable as standalone tool because it is an alias folded into a named typed operation. An alias U is still functionally included through the canonical tool. No free-form wire tool is provided. Inventory is generated from actual verb table string entries; advertise-only NULL entries are listed separately. The following rows cover every source verb, not merely capability groups.

### apple2: 59 named verbs

| Wire verb | Status | Tool / reason |
|---|---|---|
| `hello` | I | attach/launch handshake |
| `version` | I | Internal optional handshake identity cross-check; connection_info uses cached identity |
| `capabilities` | I | connection_info (feature manifest) |
| `ping` | I | Explicit health_check; requires request lock |
| `quit-client` | I | detach |
| `reset` | I | execute(reset) |
| `run` | I | execute(run) |
| `pause` | I | execute(pause) |
| `get-state` | I | get_state |
| `get-cpu` | I | get_cpu |
| `get-softswitches` | I | apple2_softswitches |
| `step-cycle` | I | execute(step_cycle) |
| `step-instruction` | I | execute(step_instruction) |
| `step-over` | I | execute(step_over) |
| `step-out` | I | execute(step_out) |
| `set-turbo` | I | product set_turbo |
| `get-frame` | I | get_frame |
| `frame-ring-info` | I | frame_ring(info) |
| `frame-ring-record` | I | frame_ring(record) |
| `frame-ring-clear` | I | frame_ring(clear) |
| `get-frame-at` | I | recorded_frame |
| `get-memory` | I | read_memory/export_memory |
| `set-memory` | I | write_memory |
| `set-reg` | I | apple2_set_register |
| `break-exec` | I | breakpoints(create,exec) |
| `break-clear` | I | breakpoints(delete) |
| `break-clear-all` | I | breakpoints(clear_all) |
| `break-enable` | I | breakpoints(enable) |
| `break-list` | I | breakpoints(list) |
| `get-breakpoints` | I | breakpoints(list), alias |
| `break-create` | I | breakpoints(create) |
| `break-update` | I | breakpoints(update) |
| `rearm-oneshots` | I | breakpoints(rearm) |
| `wait-paused` | I | wait(paused) |
| `wait-running` | I | wait(running) |
| `wait-frame` | I | wait(frame) |
| `wait-event` | I | wait(event) |
| `key` | I | apple2_key |
| `mount` | I | apple2_media(mount) |
| `mount-disk` | I | apple2_media(mount,diskii), alias |
| `unmount` | I | apple2_media(unmount) |
| `select-disk` | I | apple2_media(select) |
| `set-disk-writable` | I | apple2_media(set_writable) |
| `save-state` | I | snapshot(save) |
| `load-state` | I | snapshot(load) |
| `history-info` | I | history_info |
| `history-record` | I | history_record |
| `history-clear` | I | history_clear |
| `history-find` | I | history_find |
| `history-next` | I | history_next |
| `history-read` | I | history_read |
| `history-close` | I | history_close |
| `assemble` | I | product assemble |
| `find-symbol` | I | find_symbol |
| `leave-inspector` | I | inspector(leave) |
| `enter-inspector` | I | inspector(enter) |
| `land-inspector` | I | inspector(land) |
| `land-inspector-exact` | I | inspector(land_exact) |
| `printer-flush` | D | Host printer side effects/configuration and no completion/readback; separate follow-on. |

Advertise-only entries: `sessions`, `state-changed` (NULL verb names). Included as bound connection/session lifecycle and bounded event observation, not callable verbs.

### c64: 80 named verbs

| Wire verb | Status | Tool / reason |
|---|---|---|
| `hello` | I | attach/launch handshake |
| `version` | I | Internal optional handshake identity cross-check; connection_info uses cached identity |
| `capabilities` | I | connection_info (feature manifest) |
| `ping` | I | Explicit health_check; requires request lock |
| `quit-client` | I | detach |
| `reset` | I | execute(reset) |
| `run` | I | execute(run) |
| `pause` | I | execute(pause) |
| `run-cycles` | I | c64_execute(run_cycles) |
| `run-instructions` | I | c64_execute(run_instructions) |
| `run-to` | I | c64_execute(run_to) |
| `step-frame` | I | c64_execute(step_frame) |
| `get-state` | I | get_state |
| `get-cpu` | I | get_cpu |
| `step-cycle` | I | execute(step_cycle) |
| `step-instruction` | I | execute(step_instruction) |
| `step-over` | I | execute(step_over) |
| `step-out` | I | execute(step_out) |
| `set-turbo` | I | product set_turbo |
| `get-frame` | I | get_frame |
| `get-frame-at` | I | recorded_frame |
| `get-memory` | I | read_memory/export_memory |
| `set-memory` | I | write_memory |
| `get-debug-memory` | D | Bulk three-space/writer-PC-history snapshot needs a dedicated paging/export contract; ordinary memory included. |
| `get-call-stack` | I | c64_call_stack |
| `key-down` | I | c64_key(down) |
| `key-up` | I | c64_key(up) |
| `restore` | I | c64_key(restore) |
| `joystick` | I | c64_joystick |
| `paste-text` | I | c64_paste(text), folded to data framing |
| `paste-events` | I | c64_paste(events), folded to data framing |
| `paste-text-data` | I | c64_paste(text) |
| `paste-events-data` | I | c64_paste(events) |
| `mount-d64` | I | c64_media(mount) |
| `unmount-disk` | I | c64_media(unmount) |
| `get-disk-status` | I | c64_disk_status |
| `load-prg` | I | c64_load_program |
| `load-bin` | I | c64_binary_file(load) |
| `save-bin` | I | c64_binary_file(save) |
| `save-state` | I | snapshot(save) |
| `load-state` | I | snapshot(load) |
| `break-exec` | I | breakpoints(create,exec) |
| `break-clear` | I | breakpoints(delete) |
| `break-enable` | I | breakpoints(enable) |
| `break-list` | I | breakpoints(list) |
| `get-breakpoints` | I | breakpoints(list), alias |
| `break-clear-all` | I | breakpoints(clear_all) |
| `break-create` | I | breakpoints(create) |
| `break-update` | I | breakpoints(update) |
| `rearm-oneshots` | I | breakpoints(rearm) |
| `wait-paused` | I | wait(paused) |
| `wait-running` | I | wait(running) |
| `wait-frame` | I | wait(frame) |
| `wait-event` | I | wait(event) |
| `assemble` | I | product assemble |
| `find-symbol` | I | find_symbol |
| `get-drive-cpu` | I | c64_drive_cpu |
| `get-vic` | I | c64_vic |
| `get-cia` | I | c64_cia |
| `run-to-raster` | I | c64_execute(run_to_raster) |
| `history-info` | I | history_info |
| `history-record` | I | history_record |
| `history-clear` | I | history_clear |
| `history-find` | I | history_find |
| `history-next` | I | history_next |
| `history-read` | I | history_read |
| `history-close` | I | history_close |
| `power-drive` | I | c64_media(power) |
| `frame-ring-info` | I | frame_ring(info) |
| `frame-ring-record` | I | frame_ring(record) |
| `frame-ring-clear` | I | frame_ring(clear) |
| `vic-ring-info` | D | C64 VIC line-ring schema/large-result query deferred; current VIC snapshot and frame/history evidence included. |
| `vic-ring-record` | D | C64 VIC line-ring schema/large-result query deferred; current VIC snapshot and frame/history evidence included. |
| `vic-ring-clear` | D | C64 VIC line-ring schema/large-result query deferred; current VIC snapshot and frame/history evidence included. |
| `vic-ring-find` | D | C64 VIC line-ring schema/large-result query deferred; current VIC snapshot and frame/history evidence included. |
| `leave-inspector` | I | inspector(leave) |
| `enter-inspector` | I | inspector(enter) |
| `land-inspector` | I | inspector(land) |
| `land-inspector-exact` | I | inspector(land_exact) |
| `printer-flush` | D | Host printer side effects/configuration and no completion/readback; separate follow-on. |

Advertise-only entries: `sessions`, `state-changed` (NULL verb names). Included as bound connection/session lifecycle and bounded event observation, not callable verbs.

## Explicit omissions and limitations

Deferred: C64 VIC-ring controls/query; C64 bulk debug-memory/write-history arrays; printer-flush; full history export; automatic instance/port discovery; concurrent emulator instances in one process; multiple MCP clients/shared daemon; wire pipelining; remote/non-loopback access and HTTP; image crop/rescale/comparison tools; protocol versions other than exact initial pair; standalone bundled executables without Python; automatic crash report ingestion/reproduction/recovery; live startup configuration editing; stronger request-correlated async completion.

Unsupported because absent from current wire: C64 set-register, general cartridge/G64 mounting through a nonexistent generic mount verb, Apple PRG loading semantics, symbol-file import (find-symbol only), Basic Text load flag, rich UI breakpoint type/swap payloads, Inspector catalog browsing/record-toggle/± UI-only operations, changing printer config or confirming output completion, emulator graceful process shutdown verb. Native guest execution may access HostFS/mounted media; adapter root rules are not an emulator sandbox. Existing GUI recording/startup configuration remains usable independently.

Aliases are folded, not functionality removed: get-breakpoints, break-exec, mount-disk, text paste forms. No additional standalone tool is needed. No generic disassembler is promised; read bytes/CPU/symbols plus agent reasoning are initial access, and a future tool may wrap the shared assembler disassembler separately.

## Components and schema maintenance

Proposed paths: `mcp/pyproject.toml`, lock file, `README.md`, `config.example.toml`; `mcp/src/machines_mcp/{__main__,server,config,lifecycle,wire,results,artifacts}.py`; `backends/{apple2,c64}.py`; `schemas/{tools,protocol_manifest}.py`; `codecs/{frame,hst1,breakpoints}.py`; `mcp/tests/{unit,integration,fixtures}`; `mcp/tools/check_protocol_manifest.py`. Avoid a speculative plugin framework; two backends implement small explicit dispatch maps.

Schema models are the single adapter authority for generated tools/list schemas, validation, serialization and output definitions. Protocol manifest contains exact identity, named verb/alias sets, capability sets, source memory names/writability, key vocabulary, history options/access names, breakpoint terms/actions, delivery class and limits. Static source extraction can verify table/vocabulary drift; behavioral tests verify actual serialization/semantics. A source command addition must update coverage status even if deferred, and schema changes must update snapshots/docs together. Feature tokens alone do not prove a verb exists or its argument contract. No need to add C schema generators in v1.

Maintain adapter package semver independently of emulator identities. Add new wire version support explicitly through tested backend manifests; do not compare protocol numbers as permissive >= thresholds. MCP's negotiated version and adapter tool schema version are separate from A2M/C64M versions. Stable tools use additive optional changes; incompatible meaning/output changes require adapter major version or a new named tool.

## Implementation sequence and acceptance

1. Compatibility spike: lock a stable official SDK, validate Codex initialization, stdio launch, tools/list, typed calls, JSON results and PNG display with a tiny fixture server. Generate complete schema and delivery manifests from source using the exact contracts above, including source vocabulary equality tests. Record negotiated MCP version and tested host versions. This is required before committing to full handlers; it may adjust SDK pin, not product scope.
2. Package/configuration, wire reader, strict framing/error bounds, lifecycle attach/launch and ownership/log handling. Unit/fake-server tests plus live identity/attach preservation on both emulators.
3. State/execution/waits, memory, breakpoints, hardware inspection, PNG. Verify honest acceptance and partial-frame behavior; never fill gaps with undocumented native mutations.
4. Assembly/files/input/media/snapshots/symbols with explicit product contracts and local path rules.
5. HST1/cursors, frame rings and Inspector, bounded immutable result pages and exports. Add source-manifest drift gate, docs and Codex setup.
6. Independent implementation review and first-release validation against every included inventory row; any scope change requires explicit design update instead of quietly dropping it.

Meaningful checks: fragmented TCP headers/binary/delimiters, events interleaved with replies, oversized/malformed payloads, wrong identity/version, busy connection, deadline/cancellation after accepted mutation, reconnect generation/cursor stale, real paused/running/Inspector reads, Apple immediate write acknowledgement vs C64 deferred completion, snapshot sticky-latch honesty, host paths with spaces/Unicode/CRLF, symlink containment and overwrite behavior, launch bind race/exit/cleanup/keep_on_exit, live owned child retained across failed handshakes/disconnect and stop/abandon via ownership token, full frame colors and C64 padded stride, HST1 byte-order/truncation/markers/access flags, source table/alias coverage equality. Property-like fuzz fixtures for framing and HST1 bounded decoding are useful; no emulation core refactor/tests are required by adapter work.

Live integration uses tiny repository-owned assembly fixtures, known ROM configuration, and temporary directories; no private assets dependency. Test each included verb/backend mapping through real wire behavior where feasible, including media fixtures. Existing documented emulator test failures remain separate baseline failures, not excuses to weaken adapter assertions or unrelated fixes.

Codex end-to-end acceptance: configure one installed stdio server; tools visible before connection; launch each product in turn, assemble a small fixture, pause/step, read CPU/memory, set/hit guarded BP, view recognizable PNG, query bounded history; detach an independently launched emulator and prove pid/state persist; attach in Inspector and confirm attachment leaves mode/cursors/evidence recording unchanged; inspect logs after simulated child failure. Show mismatch and occupied-port diagnostics. Human verifies actual image presentation and agent can choose correct machine-specific tools from descriptions without a copied protocol file. Record evidence; do not claim merely passing SDK client tests proves Codex compatibility.

## External references and compatibility strategy

Official Codex MCP configuration: https://learn.chatgpt.com/docs/extend/mcp (checked 2026-10-12); supports local command/stdio and HTTP configuration, so stdio fits this local server. MCP transports: https://modelcontextprotocol.io/specification/2025-11-25/basic/transports; official Python SDK: https://github.com/modelcontextprotocol/python-sdk. SDK supports standard initialization negotiation; choose common standard tools/image/text functionality, avoid experimental tasks/UI extensions or reliance on every client supporting the latest revision. The implementation spike verifies actual supported negotiation instead of assuming newest published spec equals installed Codex capabilities. MCP configuration/settings documentation can move; README setup examples are verified against the installed Codex version during acceptance.

## Review checklist

Reviewer must approve scope coverage, typed schema implementability, source-derived delivery honesty, evidence preservation, ownership/file boundaries, cancellation/deadline behavior, SDK/Codex portability strategy and explicitly deferred work. Approval applies to the design only. No adapter code or emulator changes are part of this deliverable.

## Independent review record

2026-10-12: Reviewer approved the revised design with zero unresolved blockers. The review required and verified three corrections: Apple breakpoint swap-slot coverage; retained owned-child ownership and stop/abandon access after connection failures; explicit screenshot output-path/overwrite schema fields. Dispatch-started per-verb deadlines were also clarified. Approval covers this design only; no implementation was performed or authorized by the review.

2026-10-12: User approved implementation, retaining the documented debugging follow-ons and requiring one designated agent to own emulator interaction while other agents analyze in parallel. This authorization applies only to the scoped first release.

2026-10-12: Implementation reviewer approved with zero unresolved blockers after correcting hardware numeric decoding, discovery contracts, startup deadlines/diagnostics, semantic drift coverage and workflow triggers. Final broken-pipe classification was separately approved with its regression test. Local Codex CLI workflows and emulator integrations are recorded in `mcp/VALIDATION.md`; desktop activation and hosted cross-platform CI are distinguished from local checks.

The four debugging follow-ons are implemented in the subsequent [debugging extensions](debugging-extensions.md) addendum. The inventory above records the original v1 release; current coverage is maintained in the adapter manifest. Native write history contains packed writer PCs, not write ages.
