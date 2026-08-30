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
- Active pane outline is a content-region stroke (`debugger_draw_active_view_border`),
  not window bounds. Display and Misc draw it leftover; Disassembly and Memory
  draw it in the shared panes. Hide it while a modal dialog is open.
- Memory/disasm cursor, click-to-place, and right-click open/close live in the
  shared panes (`debugger_context_menu` popup chrome). Menu *contents* stay
  leftover (Apple Source+ASCII, C64 Source including 1541 maps).
- Disassembly rows use Nuklear selectables (default window background, not a
  black fill). Format is `PC BP ADDR LABEL BYTES TEXT [target]`. PC stays
  middle-row locked while following; a user cursor detaches on arrows/click
  and is tracked by address independently of the view. Wheel/scrollbar only
  pan the listing. Execute BPs show as `X`/`x`. Leftover supplies target
  annotation and BP lookup; leftover `frontend_disassembly_handle_key` is
  the live key table. Do not leave unused `*_ops` callbacks or unused shell
  pane key/merge stubs beside a live product handler — delete or wire in
  the same change.
- Opt+M cycles the **machine's published sources**, not a shared enum.
  Apple memview Map→Main→Aux→LC1→LC2→ROM; Apple disasm Map→ROM→Main.
  C64 CPU map / ROM / RAM / drive 8 / drive 9. High-bit ASCII is a source
  view flag (Apple default on, C64 default off).
- Exclusive tabs stay leftover: Machine, Debugger, Hardware, **Assembler**,
  Config. Inspector tab is shared chrome.
- Forensics is HST1 FIND, not Inspector. Opt+R / Inspector **Forensics...**.
- Help: Opt+H. Modal while open (Quit still works). May stack over Forensics
  (CRT underlay; returns to Forensics on close). Generated from that binary's
  `manual/*/manual.md`.

## ASCII-only UI

Labels, status, dialogs, and manuals are ASCII. No `…` / `—` / `–` / `→` /
`≤`. See `manual/a2m/HELP_MARKDOWN.md` and `manual/c64m/HELP_MARKDOWN.md`.
`src/shell/tools/gen_help.py` fails the build on non-ASCII in `manual.md`.
