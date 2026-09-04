# Status

**Version:** 3.1.0

What the product is **now**. Source of truth is `src/`. User-facing catalog:
[`manual/a2m/manual.md`](../../manual/a2m/manual.md). Rules: [`rules.md`](rules.md).

## Product line

| Layer | Reality |
|-------|---------|
| Shell | Debugger UI: Apple display, CPU, disasm, memory, Misc, Configure, CRT |
| Machine | Apple II `src/apple2/machine` (//e Enhanced default, or ][+). Disk II, SmartPort/HostFS, Mockingboard, SSC + ImageWriter II mono host pages |
| Runtime | Two-thread; worker owns `apple2_t`; UI/control use `runtime_client` only |
| Display | ARGB **560×192** throughout (`display_frame` / runtime slot / frontend) |
| Video | Beam-stepped a2m-class: LORES, DLORES, 40/80 text, HGR colour, DHGR; host Colour vs discrete-bit Mono (White/Green/Amber). Max uses full-frame block paint |
| Memory areas | Map · Main · Aux · LC1 · LC2 · ROM |
| Control port | **A2M/16** on `--control-port` (windowed or headless). Ops: [`control-tools.md`](control-tools.md) |
| Snapshots | **`.a2state`** — drop, `--sna`, Alt+Shift+`.`/`,`, control, Machine tab. [`snapshots.md`](snapshots.md) |
| Time travel | Misc → Inspector: Record / Inspect / Leave. Land on checkpoints; HST1 is FIND only. [`timemachine.md`](timemachine.md) |
| Assembler | Misc → Assembler + standalone `am65`. Optional MLI launch. `src/shell/tools/am65/` |

## Host keys (agent-relevant)

Full user list is in the manual. These are the chords the host loop actually
binds in `src/main.c`:

| Key | Action |
|-----|--------|
| **F9** | Toggle debugger (starts display-only) |
| **F8** / **Alt+F8** | Warm / cold reset |
| **F10** / Shift+F10 | Pause/step · step out (time travel: sealed step / step-out; no-op at live) |
| **F11** | Step over (time travel: sealed step-over; no-op at live) |
| **F12** / Shift+F12 | Run · run to cursor (time travel: re-execute to a breakpoint or **live**; stay in Inspect) |
| **Alt+B** | Toggle execute BP at disasm cursor (one list in live and time travel) |
| **Alt+T** | Cycle turbo ladder (MHz / max) |
| **Shift+Alt+C** | Toggle colour / configured mono phosphor (White, Green, Amber) |
| **Alt+Shift+.** / **,** | Quicksave / quickload `.a2state` |
| **Alt+Shift+A** | Assemble configured source (reset / auto-run / MLI / one-shot) |
| **Alt+H** | Help (modal; pauses while open). **Esc** closes it; returns to CRT / debugger / Forensics. |
| **Alt+R** | Toggle Forensics; leave returns to entry surface (CRT may resume; debugger stays paused). **F9** from Forensics → debugger paused. |
| **Alt+M** | Memory area cycle (disasm is Map/ROM/Main; memory pane is six areas) |
| **Alt+F / Alt+G / Alt+Shift+G** | Active Memory view: Find / next / previous |
| **Alt+Shift+M** | Kbd stick layout numpad ↔ WASD |
| **Alt+Shift+0/1/2** | Kbd stick off / stick 1 / stick 2 |
| **Alt+1 / Alt+2** | Gamepad → stick map / swap |
| **Alt+Insert** | Paste clipboard → Apple keyboard (does not change turbo) |
| **Alt+Tab** | Cycle debugger focus (when F9 is up) |
| Letters/digits/… | Apple keyboard when machine input focus |

F8 stands in for CTRL+RESET (macOS often eats Control+F-keys). Alt+F8 adds
Open-Apple for cold start.

Keyboard stick (when on): KP0 (numpad) or Cmd/Win (WASD) and Space are fire
keys (optional swap in Configure). Alt/Option is host-only. Solid-apple is
released on stick-assign chords so Alt+Shift+1 does not latch BUTN0.

## What works (evidence)

| Area | Evidence |
|------|----------|
| Build / ctest | **82 green** - [`testing.md`](testing.md) |
| Assembler | Misc → Assembler; `file=` HostFS; optional Auto-run; **MLI launch** gates auto-run on CPU-visible `$BF00 == $4C` (mutually exclusive with Reset). Sample: `samples/apple2/asm_mli_launch/` |
| CLI / INI | model, mounts `sNdN` (multi-image queue), turbo MHz/`max`, `--video-display`, `--ssc N`, lifecycle, headless, `[DEBUG] break.*`, `--inspector` |
| Turbo / step / reset | Alt+T (default ladder `1,max`); F10–F12 family; F8 / Alt+F8 |
| Display | Full frame in display-only and F9 debugger |
| Breakpoints | Exec + R/W, mapping, FAST/SLOW, TYPE, SWAP, INI, control RPC. [`breakpoints.md`](breakpoints.md) |
| Disk II / SmartPort | Cards in slots 1–7; Configure live-applies model/cards via power-cycle reset; live insert/eject + slot boot; `[SmartPort] boot_slot=N`; Disk II queue/swap with dirty flush; ProDOS `$C0s4/5`; `$C800` host trap; **HostFS** folder volumes |
| SSC / ImageWriter | One soft-present SSC (`--ssc N` / `[Slots] slotN=ssc` / Configure Super Serial); TX → ImageWriter II mono BMP under `prints/` (or `[printer] output_dir`); Misc Force flush + `printer-flush`; no Misc soft-power |
| Gameport | Paddles, buttons, kbd stick, SDL pads, motor LEDs |
| Control | `--control-port` A2M/16; `tools/a2m_control_client.py` (`Ctl`); `tools/a2m_coop_watch.py` |

## Tree

```text
src/apple2/main.c     host loop
src/apple2/frontend/  leftover UI (exclusive tabs, input, CRT)
src/shell/frontend/               shared chrome
src/apple2/runtime/   Apple-backed runtime
src/apple2/machine/   Apple II silicon
src/apple2/control/   A2M/16 leftover verbs
src/shell/control/                framing + verb runner
src/apple2/platform/  leftover audio
src/shell/tools/am65/             assembler library + CLI
src/apple2/util/      leftover files / HostFS helpers
manual/a2m/manual.md              user manual (compiled into help)
tests/apple2/                     leftover tests
tests/shell/                      shared tests
```
