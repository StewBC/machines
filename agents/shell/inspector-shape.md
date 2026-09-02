# Shared Inspector shape

One Inspector **product shape** in UI and `runtime_client` verb *names*.
Each machine keeps its Record clock and picture type. A shared slider
widget is not a shared birth function.

Apple clocks: [`../apple2/timemachine.md`](../apple2/timemachine.md).
C64 clocks: [`../c64/runtime-control.md`](../c64/runtime-control.md).
Do not merge those notes.

## Shared

- Record on/off, Inspect (enter), Land (quantized / exact), Leave, `[-]` /
  `[+]`, NOW, sealed re-execute, film-vs-reconstruct.
- Misc → Inspector tab chrome: Record checkbox, Inspect/Leave, slider,
  cobalt headers while Inspecting. Source: `src/shell/frontend/inspector_tab.*`.
- Subset names: `runtime_client_inspector_{set_enabled,enter,leave,land,land_to_cycle}`
  in `src/shell/runtime/runtime_client_subset.h`. Picture blit and catalog
  stay leftover.
- Wire: `get-state` reports `mode=live|inspector`. A2M/15 and C64M/10 both
  have `enter-inspector` / `leave-inspector` / `land-inspector` /
  `land-inspector-exact`. Land from live implies enter. Socket `run` /
  `step-*` while Inspecting are sealed execute on both products.
- Record does **not** arm or stop HST1. FIND is Forensics, not the slider.
- Inspect is read-only. Forward motion is sealed re-execute, clamped to
  live. No reverse CPU. No Promote/Branch.

Tests for the tab: `tests/shell/frontend/test_inspector_tab.c`. Leftover
clock tests stay under `tests/apple2/` and `tests/c64/`.

## Max-turbo knobs (shared names, product defaults)

Both products expose the same CLI/INI/Configure pair under `[debug]`:

| Knob | Meaning | a2m default | c64m default |
|------|---------|-------------|--------------|
| `history_off_on_max` | Pause HST1 in max; resume on leave | true | true |
| `inspector_off_on_max` | Wipe Inspector Record (+ film) in max; restore empty Record on leave if it was on | false | true |

Record does not arm or stop HST1. Budgets: `history_memory_mb` / `inspector_memory_mb` / `frame_ring_memory_mb` (`0` disables that stream).

## Leftover (do not smash)

| Axis | Apple | C64 |
|------|-------|-----|
| Record clock | Pair completed beam frame `F` with first instruction-boundary snapshot `S >= F` | Birth CP on frame-publish; `film_cycle` |
| Picture | ARGB 560×192; join by sample/picture ID | indexed8 + VIC ring; exact `film_cycle`; miss = full pink |
| Max turbo defaults | TimeMachine continues (`inspector_off_on_max` false); HST1 pauses | Record wiped (`inspector_off_on_max` true); HST1 pauses |
| Recorder files | `runtime_inspector.c` + `runtime_inspector_recorder.c` | recorder inlined; plus `runtime_vic_ring` |

Thumb-down preview must not reconstruct on either product if that product's
honesty rule says pink.
