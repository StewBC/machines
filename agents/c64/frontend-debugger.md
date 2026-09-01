# Frontend, debugger, input, and help

Shared chrome (layout, CPU pane, disasm, memview, BP list, help, Forensics,
Inspector *tab*): [`../shell/frontend.md`](../shell/frontend.md). This note is
leftover C64 Misc tabs, input, CRT, and memory-source cycles.

## Source of truth

Leftover `src/frontend/`, integration in leftover `src/main.c`, leftover
`src/runtime/runtime_client.h` extras, leftover `src/platform/platform_audio.*`.
Shared panes live in `src/shell/frontend/`. Automated
coverage: `frontend_input`, `frontend_joystick`, `frontend_mouse`, `help_view`, `forensics_view`,
`window_title`, `crt_renderer`, `disasm_pc_lock`. Most Nuklear UI is manual smoke.

SDL events become frontend intents, intents become `runtime_client` commands,
and `poll_runtime_events()` updates copied debugger state. A new UI action
needs an intent, a runtime-client call, main-loop dispatch, and an event
update if the UI must show completion. The frontend never reads live `c64_t`.

## Layout

Display | registers/disasm | memory | Misc. Memory sources: CPU map, raw RAM,
ROM, drive 8, drive 9. Debugger edits go through runtime commands.

Misc tabs (two rows of three): Machine, Debugger, Breakpoints / Hardware,
Assembler, Inspector. Inspector is the only Inspect entry.

Window title: `c64m - VIDEO - TURBO - STATE`. Modes render as `Normal`,
`Max`. Inspecting replaces the state with `Inspect`.

Host file selection uses `platform_fs`. Remembered browse directories live
in `[browse]`. State hotkeys: Opt+Shift+`>` / `<`.

## Inspector tab

Record off: Record checkbox only. Record on: Inspect plus window summary
(oldest / live / duration). In Inspect: Leave Inspector, `[-]` slider `[+]`,
cycle lines. Window headers use dark cobalt while Inspecting; do not tint
the window background.

Scrub / `[-]` / `[+]` UI:

- Thumb-down preview uses `runtime_client_copy_inspector_cell_film` (quantize
  to nearest CP `<=`, exact `film_cycle` blit). Miss → full pink. No
  reconstruct and no nearest-`<=` neighbour film while dragging.
- Release lands (nearest CP `<=`, or LIVE at the right end). After land /
  `[-]` / `[+]`, the slider **snaps** to the committed focus cycle. Worker
  publishes film-first else reconstruct; UI must not pink-overlay when the
  thumb is up (`thumb_down == false`).
- `[-]` / `[+]` walk Record checkpoints (`INSPECTOR_CHECKPOINT_STEP`).

Product rules and CRT table: `runtime-control.md`. Do not walk HST1 to place
the slider. Inspector Record does **not** arm or stop HST1.

## Forensics

Full-window HST1 FIND surface (`forensics_view.*`), not a Misc tab and not a
Help-style CRT overlay. **Forensics...** on the Inspector tab and **Opt+R**
open it (pauses on enter). **Opt+R** / **Close** return to the entry surface
(CRT restores prior run state if it was running; debugger stays paused).
**F9** from Forensics always opens the debugger paused. **Esc** does not
leave. **Opt+H** stacks modal Help over Forensics (CRT underlay; Forensics
stays open as return surface).

Query line → structured `HISTORY_*` intents → `main.c` claim/decode →
transcript (`session_id = 0`; `history_close` on exit). Find options use
shared `runtime_history_parse_find_options` / public key tables. The query
line is **verb-first** (`find` / `next` / `read` / `info`); bare `key=value`
is not FIND (status = verb help, same string as Tab). Control-port
`history-find` still accepts bare keys. Tab is a grammar walker:
unique-complete or ASCII slot help; caret at end unique-expands every token
(explicit `edit.cursor` after rewrite). Transcript scroll is kept in
`transcript_scroll_y` and restored on re-open (Help `section_scroll_y`
pattern); Clear view resets it and does not call `history-clear`.

**Land before** / **Land exact** on a selected hit: quantized checkpoint
`<=` N vs `runtime_inspector_land_to_cycle`. Live + can enter → **Inspect &
Land** confirm then ENTER+land; soft-fail without checkpoints. Successful
land (any Inspect focus update used for the land status strip) leaves
Forensics like F9 (debugger paused; abandon CRT resume latch) and selects
Misc → Inspector. Cancel / soft-fail / incomplete land stay in Forensics.
Click selects a logical entry/block; **Copy** uses the full unwrapped text.
Double-click `id=` / `cyc=` / `pc=$...` copies that token. UI strings are
ASCII-only. User docs: `manual/c64m/manual.md` (**Forensics**).

## Input

`SDL event -> frontend_input` / `frontend_joystick_input` /
`frontend_mouse_input ->` `runtime_client_keyboard_key`,
`runtime_client_set_joystick`, or `runtime_client_set_mouse` /
`runtime_client_clear_mouse`. Do not write CIA, SID pots, or keyboard state
from frontend code. Dialogs are modal: outside clicks must not focus or
activate base views.

Keyboard joystick layouts: `numpad` and `wasd`. WASD is consumed only while
assigned and C64 keyboard focus is active. Assignment: Alt+Shift+1/2, Alt+Shift+0
disables; real controllers remain Alt+1/2. SDL text input is enabled only
while an edit field has focus.

CBM 1351 (proportional only): default off (`[input] mouse_enabled` /
`--mouse`). Opt+Click CRT captures (relative mode + warp to window center);
Opt+Click releases. Autorelease on focus loss, Help, Forensics, any dialog,
or Inspector. While captured, re-assert relative mode if SDL/OS dropped it
(macOS after Alt-Tab can leave the host cursor free while xrel still moves
the guest).
Host motion: per-event clamp `±CBM1351_MAX_DELTA` (8) into a pending
bucket (capped at `±CBM1351_PENDING_MAX` 48), then a pot-window budget
commits at most `±CBM1351_BUDGET_MAX` (8) into the 6-bit counters every
`CBM1351_BUDGET_MS` (16) and **carries** the unused pending — so fast
moves keep draining across windows instead of feeling laggy from drops. While captured, that port's digital lines come only from the mouse
at the SDL/kbdjoy merge (control-port `joystick` can still overwrite
until the next `set_mouse` — accepted v1 gap). No control `mouse` verb;
no Inspector mouse log event. SID pots use a **512 Ø2 latch** (sample on
`mouse_port` select; keep on other edges; prime on `set_mouse`). Guest
**reads** return the latch when mux selects `mouse_port` or is deselected,
and `$FF` when the other port is exclusive (avoids dual-port). Inactive ⇒
`$FF`.

## Loading and configuration

CLI / INI: `src/c64/app_options.*`. Commented template: repo-root
`c64m.ini.example` (keep in sync when keys change; see
[`../README.md`](../README.md)).

Machine dialogs: D64 queues, writable toggle, PRG/BASIC/BASIC Text, T64
extract, CRT attach, state save/load, ROM endpoints, video standard, audio,
1541 emulation, media mode. Emulator tab also owns CRT presentation (4:3
pixel-aspect option, scanlines, curvature) on a second processed texture.
Controls preview live; Cancel restores. ROM changes apply by reboot/reload;
1541 emulation applies live.

Basic Text is stock BASIC V2 only (`util/basic_v2`).

## Help

`manual/c64m/manual.md` is compiled by `src/shell/tools/gen_help.py`. Before
editing it, read `manual/c64m/HELP_MARKDOWN.md` (ASCII, no links, help-renderer
subset).

Opt+H opens modal Help (Quit still works). Remembers return surface: CRT,
debugger, or Forensics. CRT underlay while open.

Help search highlights hits: inverse band on the jumped-to span, underline
on other visible occurrences. Matching is per drawn line inside
`help_draw_inline_at`. The band must be a real `nk_fill_rect` (`nk_convert`
drops `nk_draw_text` background colour). `help_estimate_span_y` only
estimates scroll; the render pass measures the row that holds the hit and
corrects. `help_view_search()` is what `test_help_view` drives.

## Assembler surface

In-emulator assembly exposes `dest="map"`, ignores `file=`, predefines
`AM65=0` and `C64=1`. `[assembler] auto_adjust_segments=yes` enables bounded
overlap-layout retries. Library details: `tools.md`.

## Limits

File browser has no Windows drive-letter enumeration UI. Undocumented
opcodes display as `.BYTE`. Breakpoint Type during cycle stepping and
per-device Swap remain limited.
