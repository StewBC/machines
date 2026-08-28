# c64m agent handoff

Current-state briefing for leftover C64 silicon and leftover c64m chrome.
Source and tests are authoritative. If a handoff and the code disagree,
trust the code and fix the handoff in the same change.

Monorepo index: [`../README.md`](../README.md). Shared debugger shape:
[`../shell/`](../shell/). Do not open `src/machine/apple2` to decide C64 silicon.

**Paths in this folder:** bare `src/...` means leftover
`src/machine/c64/src/...` unless the path already starts with `src/shell/`,
`manual/`, or `tests/`.

Do not read files under leftover `md-files/` if they reappear. They are
historical working notes and are not guaranteed to be accurate.

## Read in this order

1. This file (rules, diagnosis, where to look)
2. `architecture.md`
3. The component handoff for the task
4. `testing.md` before claiming a change is done
5. The source and tests named by that handoff

If the task is driving c64m over the control port, start with `using-c64m.md`.
If the task compares c64m to VICE on titles under leftover `assets/prg/`,
read `vice-oracle.md` before launching VICE.

Shared chrome / control framing / HST1 / Inspector *tab*:
[`../shell/frontend.md`](../shell/frontend.md) ·
[`../shell/control.md`](../shell/control.md) ·
[`../shell/history.md`](../shell/history.md) ·
[`../shell/inspector-shape.md`](../shell/inspector-shape.md).
C64 `film_cycle` / pink / vic-ring stay in `runtime-control.md`.

## Product

c64m is a C99 Commodore 64 emulator. It boots real ROM, runs PAL and NTSC, and
covers BASIC, PRG inject, D64/G64 (devices 8 and 9), CRT types 0, 5, 7, 8, 15,
17, and 19, SID audio, a debugger, Inspector time travel, an always-on CPU
flight recorder, and a localhost control port (`C64M/8`).

It is not a promise of cycle-perfect demo-scene behavior, every cartridge
mapper, analog SID, or full 1541 mechanics. See `known-gaps.md`.

No hacks. The goal is to meet hardware so software that works on hardware (or
VICE) also works here.

## Design docs (proposals, not product-as-is)

Leftover c64m designs: [`src/machine/c64/design/`](../../src/machine/c64/design/).
Monorepo designs: [`design/`](../../design/). Do not treat design drafts as
handoff truth — source and `agents/*.md` win when they disagree.

## User manual

[`manual/c64m/manual.md`](../../manual/c64m/manual.md) is the user-facing
manual. `src/shell/tools/gen_help.py` compiles it into the in-emulator help
view. It is not an agent handoff.

If you add or change a user-facing feature, update that book in the same
change. Read [`manual/c64m/HELP_MARKDOWN.md`](../../manual/c64m/HELP_MARKDOWN.md)
first: ASCII only, no Markdown links or autolinks, and only the Markdown
subset the help renderer understands. Do not put `agents/` links in the manual.

## Diagnosis: locate, kill, then model

Written after sessions were lost to plausible mechanisms that were never
measured. The examples are VIC-II, but the method is not.

1. **Locate the defect in observables first.** Before any mechanism talk: where
   is it wrong (x/y/raster/cycle/frame), when (which rows/frames), what differs
   (colour? border vs field? sprite vs graphics?). One histogram that answers
   "where is every wrong pixel?" beats three clever theories. A bug report marks
   the symptom, not its extent.

2. **The handoff is a suspect, not a map.** Prior notes, comments, tests, and
   "we already know it's X" must be falsified or confirmed by measurement. If
   the latches say `$B` and the story says `$8`, the story is dead.

3. **One kill criterion per hypothesis.** Write the measurement that would kill
   the idea before you implement a fix. If you cannot name the kill test, you
   are not diagnosing.

4. **Instrument freely; land nothing until one mechanism is left standing.**
   Throwaway probes are how you get to one hypothesis. What multiplies thrash
   is landing a fix while two hypotheses still compete.

5. **Ground truth: VICE outranks our tests; hardware outranks VICE.** If a unit
   test encodes c64m's old model and VICE disagrees, rewrite the test. VICE is
   still a model: treat it as the default oracle and hardware as the tiebreak.
   Match VIC-II models before comparing (`vice-oracle.md`).

6. **Prove blast radius after the fix.** Same frames, the affected region, plus
   the known demos (lft-nine, Edge of Disgrace, Deus Ex Machina) when the change
   can touch them. No "should be fine".

## Where to look

| Task | Handoff | Source |
|------|---------|--------|
| Layers, threads, ownership | `architecture.md` | leftover `src/main.c`, `src/*/CMakeLists.txt` |
| What is not done | `known-gaps.md` | comments / `TODO.txt` |
| CPU, bus, carts, snapshots | `machine.md` | leftover `src/machine/c64*.c`, `c6510*`, `c64_bus`, `c64_snapshot` |
| VIC-II | `vicii.md` | leftover `src/machine/vicii.*`, `c64_frame.*` |
| CIA, keyboard, IEC pins | `cia.md` | leftover `src/machine/cia.*`, `keyboard.*` |
| SID and host audio | `sid-audio.md` | leftover `src/machine/sid.*`; shell `src/shell/util/audio_buffer.*`; leftover `src/platform/platform_audio.*` |
| Disk, 1541, IEC | `disk-iec1541.md` | leftover `src/machine/c1541*`, `via6522.*` |
| Runtime, Inspector clocks, recorder, rings | `runtime-control.md` | leftover `src/runtime/` |
| Control-port wire | `control-port.md` | leftover `src/control/`, `src/main.c` dispatch; shell framing in `src/shell/control/` |
| Driving c64m from a script | `using-c64m.md` | leftover `tools/c64_control_client.py` |
| Debugger UI, input, leftover tabs | `frontend-debugger.md` | leftover `src/frontend/`, `src/main.c`; shared chrome in `src/shell/frontend/` |
| Assembler, parsers, util | `tools.md` | leftover `src/tools/` parsers; `src/shell/tools/am65/`; leftover `src/util/` |
| Tests and measure recipes | `testing.md` | leftover `CMakeLists.txt`; `tests/c64/`; `tests/shell/` |
| VICE as oracle | `vice-oracle.md` | local `x64sc` |

Keep docs current. Stale handoffs are worse than none.

## Verification

From the **machines repo root**:

```text
cmake -B build/c64m -S src/machine/c64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/c64m -j
ctest --test-dir build/c64m --output-on-failure
```

Expect **78 pass + 10 SKIP** (CTest 77 without leftover gitignored `assets/`)
**+ `history_control_integration` fails**. Do not "fix" that fail. Details in
`testing.md`.
