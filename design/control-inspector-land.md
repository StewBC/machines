# Control-port Inspector land + a2m sealed step

| Field | Value |
|-------|-------|
| Status | **landed** |
| Date | 2026-08-30 |
| Products | a2m (A2M/15), c64m (C64M/9) |

## Goal

Close the agent investigation loop on the control port:

`history-find` → **land** → inspect THEN (`get-cpu` / `get-memory`) → sealed `step-*` / `run` → `leave-inspector`

Humans already have Forensics **Land before** / **Land exact** / **Inspect & Land**. Agents had enter/leave and (on c64m) sealed step, but not land. a2m additionally over-blocked sealed step/run on the socket.

## Decisions (locked)

1. **Land implies enter.** If `mode=live`, the land verb queues enter then land (same idea as UI Inspect & Land). Runtime land APIs still require `inspecting`; composition is in control dispatch.
2. **Both quantized and exact** on the wire.
3. **a2m allows sealed `step-*` and `run`** while Inspecting (align with runtime/UI and c64m). True mutators stay `read-only-inspector`.
4. **Protocol bumps:** `A2M/15`, `C64M/9`.
5. **Out of scope:** catalog `[-]`/`[+]` sample-step on the wire; Record arming verbs; HST1-driven slider.

## Wire surface

Capability group: `inspector` (unchanged advertisement token).

| Verb | Args | Runtime |
|------|------|---------|
| `land-inspector` | `cycle=<n>` | `runtime_client_inspector_land` (nearest checkpoint ≤ cycle; ≥ live → NOW) |
| `land-inspector-exact` | `cycle=<n>` | `runtime_client_inspector_land_to_cycle` (checkpoint ≤ N then sealed fill to N) |

Shape matches `enter-inspector` / `leave-inspector`: fire-and-forget `ok accepted=1`. Peers observe completion via `get-state` (`mode=`, `focus_cycle=`) and existing inspector mode / state-changed events. Failed land (empty tape, cannot enter) leaves mode/focus unchanged; client rediscovers with `get-state`.

Empty / missing `cycle=` → `error bad-args`.

## a2m sealed execute fix

Today `control_dispatch` rejects anything in `control_command_mutates_machine` while Inspecting, including `run` and `step-*`. Runtime already sealed-steps those commands when `rt->inspecting`.

Change: introduce a narrower inspector-forbidden set (c64m pattern) — pokes, media, reset, history-record/clear, save/load-state, keys, assemble, etc. **Exclude** `run` and `step-*`.

## Implementation map

### a2m

- `src/apple2/control/control_protocol.h` — version `A2M/15`; enums; `control_args` for land cycle
- `src/apple2/control/control_verbs.c` — parse `cycle=`; register verbs
- `src/apple2/control/control_dispatch.c` — forbidden-list split; land cases (enter-if-live then land)
- Tests: `tests/apple2/runtime/test_runtime_inspector_mode.c` (protocol string + land + sealed step); protocol parse test
- Docs: `agents/apple2/control-tools.md`, `timemachine.md`, `known-gaps.md`, `status.md`, `README.md`; `manual/a2m/manual.md`

### c64m

- `src/c64/control/control_protocol.h` / `.c` — version `C64M/9`; enums; parse `cycle=`
- `src/c64/control/control_verbs.c` — register verbs
- `src/c64/main.c` — land dispatch (enter-if-live then land); keep sealed step/run as today
- Tests: `tests/c64/control/test_inspector_control.py`; protocol parse test; other hello asserts `C64M/8` → `C64M/9`
- Docs: `agents/c64/control-port.md` (+ known-gaps/status as needed); `manual/c64m/manual.md`

### Shared notes

- `agents/shell/inspector-shape.md`, `agents/README.md`
- `design/README.md` index → landed when done

## Agent recipe (after)

```text
N history-find ...
N land-inspector cycle=<hit_or_earlier>    # or land-inspector-exact
N get-state                                 # expect mode=inspector focus_cycle=...
N get-cpu / get-memory ...
N step-instruction                          # sealed on both products
N leave-inspector
```

Requires Inspector Record already armed (`--inspector` / Record on) with a non-empty catalog — same prerequisite as UI land.
