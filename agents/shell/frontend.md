# Shared debugger chrome

Shape of UI that links into **both** binaries from `src/shell/frontend/`.
Exclusive Misc tabs, leftover input, leftover CRT paint, and per-binary
memory-source *tables* stay leftover.

Apple leftover: [`../apple2/frontend.md`](../apple2/frontend.md).
C64 leftover: [`../c64/frontend-debugger.md`](../c64/frontend-debugger.md).

## Source

| Path | Role |
|------|------|
| `src/shell/frontend/debugger_layout.*` | Splitters |
| `src/shell/frontend/cpu_pane_6502.*` | 6502 registers / flags slot |
| `src/shell/frontend/disasm_pane.*` / `debugger_disasm.*` / `disasm_pc_lock.*` | Disassembly pane |
| `src/shell/frontend/memview_pane.*` / `memory_search.*` | Memory dump / find |
| `src/shell/frontend/breakpoint_chrome.*` | BP list / dialog chrome (mapping axes leftover) |
| `src/shell/frontend/window_title.*` | Title formatter (product label is a parameter) |
| `src/shell/frontend/help_view.*` | Help overlay (compiled per binary) |
| `src/shell/frontend/forensics_view.*` | HST1 FIND surface |
| `src/shell/frontend/inspector_tab.*` | Inspector *tab* chrome |
| `src/shell/frontend/disk_led_data.*` | Disk LED bitmaps |
| `src/shell/frontend/nuklear*` | One Nuklear vendor |

`help_view.c` is compiled by leftover frontend so each binary bakes its own
`help_content.inc`. Tests: `tests/shell/frontend/`.

No `#ifdef APPLE2` / `#ifdef C64` in these files. Panes talk through the
Stage 7 client subset and Stage 5 memory-source table, not `apple2.h` /
`c64.h`.

## Product shape

- Layout slot is Display | CPU | disasm | memory | Misc.
- Opt+M cycles the **machine's published sources**, not a shared enum.
  Apple memview Map→Main→Aux→LC1→LC2→ROM; Apple disasm Map→ROM→Main.
  C64 CPU map / ROM / RAM / drive 8 / drive 9. High-bit ASCII is a source
  view flag (Apple default on, C64 default off).
- Exclusive tabs stay leftover: Machine, Debugger, Hardware, **Assembler**,
  Config. Inspector tab is shared chrome.
- Forensics is HST1 FIND, not Inspector. Opt+R / Inspector **Forensics...**.
- Help: Opt+H. Generated from that binary's `manual/*/manual.md`.

## ASCII-only UI

Labels, status, dialogs, and manuals are ASCII. No `…` / `—` / `–` / `→` /
`≤`. See `manual/a2m/HELP_MARKDOWN.md` and `manual/c64m/HELP_MARKDOWN.md`.
`src/shell/tools/gen_help.py` fails the build on non-ASCII in `manual.md`.
