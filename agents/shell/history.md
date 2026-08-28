# Shared HST1 / FIND

HST1 is the CPU flight recorder. It answers "who wrote `$22` to this
address". It does **not** restore the machine. Inspector is a different
product. Do not walk HST1 to place the Inspector slider.

Apple leftover recorder policy: [`../apple2/runtime.md`](../apple2/runtime.md),
[`../apple2/timemachine.md`](../apple2/timemachine.md).
C64 leftover: [`../c64/runtime-control.md`](../c64/runtime-control.md).

## Source

| Path | Role |
|------|------|
| `src/shell/runtime/runtime_history.*` | Arena, retain_oldest_id, O(blocks) `partial_count` |
| `src/shell/runtime/runtime_history_query_parse.*` | FIND option grammar + public key tables |
| `src/shell/runtime/runtime_history_wire.*` | HST1 24-byte header / 48-byte record / 8-byte accesses |
| `src/shell/runtime/runtime_breakpoint_condition.*` | Guarded BP LHS table (Apple `cycle_in_line`; C64 `vic_cycle` / raster) |
| `src/shell/frontend/forensics_view.*` | In-emulator FIND UI |

Tests: `tests/shell/runtime/`. Leftover `runtime_breakpoint_ini.c` stays
leftover (mapping / swap / save-ini).

## Product shape

- FIND / NEXT / READ require an explicitly paused runtime.
- Forensics query line is **verb-first** (`find` / `next` / `read` / `info`).
  Control-port `history-find` still accepts bare keys.
- One cursor per leftover session. Mutation stamps cursors stale.
- Record (Inspector) does **not** arm or stop HST1. Independent toggles.
- a2m `history_off_on_max` pauses dense HST1 in max; TimeMachine continues.
  That is leftover Apple policy, not a shell ifdef.
- Leftover a2m `vic_cycle` alias for `cycle_in_line` is gone; do not restore it.
