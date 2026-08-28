# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. If a
note and the code disagree, fix the note (or the code) in the same change.
Design drafts live under [`design/`](../design/); they are not product-as-is.

An agent given **only this file** can find the shared shell versus leftover
Apple versus leftover C64. Do not open `src/machine/c64` to decide Apple
silicon. Do not open `src/machine/apple2` to decide C64 silicon.

## Read order

1. This file (layout, freeze, what is shared).
2. The leftover product handoff for the tree you are changing:
   - a2m: [`apple2/README.md`](apple2/README.md)
   - c64m: [`c64/README.md`](c64/README.md)
3. Shared shell shape, when the work is chrome / control framing / HST1 /
   Inspector *tab*: [`shell/frontend.md`](shell/frontend.md) ·
   [`shell/control.md`](shell/control.md) · [`shell/history.md`](shell/history.md) ·
   [`shell/inspector-shape.md`](shell/inspector-shape.md)
4. [`design/README.md`](../design/README.md) only for in-flight designs.
   Stage 10 is done. Do not start Stage 11 from this note.

## Where things live

**Link-into-both and no machine ifdef → `src/shell/`. Link-into-one → that
leftover machine tree.** Repo-root `external/` is the one vendor copy.

| Path | What it is |
|------|------------|
| `src/shell/` | Shared util / platform / nuklear vendor / `control/` framing + verb runner + memory-source type / `runtime/` history+BP conditions + `runtime_client_subset.h` / `frontend/` forensics+disk LEDs+help source + debugger chrome (layout, 6502 CPU pane, disasm pane, memview pane, BP list, window title, Inspector tab) / `tools/{am65,disasm_6502,symbols,gen_help.py}`. Static `shell` plus named tool targets. `help_view.c` is compiled by leftover frontend (per-binary `help_content.inc`). Leftover picture/key/media/Inspector-film client APIs stay leftover. Exclusive Misc tabs (Machine/Debugger/Hardware/Assembler/Config) stay leftover. Inspector clocks stay leftover. |
| `external/` | argparse, inih, logc, stb, tiny-regex-c, whereami (unprefixed targets). |
| `src/machine/apple2/` | Leftover a2m silicon, `runtime_thread`, leftover util (HostFS), leftover `platform_audio`, leftover frontend chrome. Still `project(a2m)`. Nested leftover `src/` is **not** flattened. |
| `src/machine/c64/` | Leftover c64m silicon, `runtime_thread`, leftover util (BASIC/paste), leftover `platform_audio`, leftover frontend chrome, TrueType, format parsers. Still `project(c64m)`. Nested leftover `src/` is **not** flattened. |
| `manual/a2m/` | Apple user book (`manual.md` + `HELP_MARKDOWN.md`). |
| `manual/c64m/` | C64 user book (`manual.md` + `HELP_MARKDOWN.md`). |
| `tests/shell/` | Tests for shared shell TUs. Registered by **both** nested CMake trees where they already were. |
| `tests/apple2/` | a2m leftover tests + fixtures. |
| `tests/c64/` | c64m leftover tests + fixtures. Asset SKIP 77 still uses leftover `assets/`. |
| `agents/apple2/` | a2m product-as-is notes. Bare `src/...` there means leftover `src/machine/apple2/src/...` unless the path already starts with `src/shell/`, `manual/`, or `tests/`. |
| `agents/c64/` | c64m product-as-is notes. Same leftover-relative `src/...` convention under `src/machine/c64/`. |
| `agents/shell/` | Shared debugger *shape* only. Not a silicon story. |

`runtime_thread.c` still lives at:

- `src/machine/apple2/src/runtime/runtime_thread.c`
- `src/machine/c64/src/runtime/runtime_thread.c`

There is still no root `project(machines)` with two `add_executable`s.

## Do not mix leftover trees

- Do not open `src/machine/c64` to decide Apple silicon. Do not open
  `src/machine/apple2` to decide C64 silicon. Shared *shape* is
  `src/shell/` plus `agents/shell/`.
- Do not "fix" a remaining twin in only one machine tree. Remaining twins
  (leftover `runtime_client` extras, CRT, exclusive tabs) still exist
  in **both** until a later EXTRACT deletes a copy. The shared client
  *subset* is `src/shell/runtime/runtime_client_subset.h`; do not re-fork it.
  Debugger chrome lives in `src/shell/frontend/`; do not re-copy layout/disasm/memview.
- Do not flatten `src/machine/apple2/src/machine/cpu65.c`.
- Leftover C64 memory-mode aliases in a2m are gone (`DRIVE8_MAP` et al.).
  The `vic_cycle` BP alias is already gone.
- Do not smash Inspector clocks (Apple F/S vs C64 `film_cycle`) or leftover `runtime_thread` command handling. Shape: [`shell/inspector-shape.md`](shell/inspector-shape.md). Clocks: [`apple2/timemachine.md`](apple2/timemachine.md) and [`c64/runtime-control.md`](c64/runtime-control.md).
- Do not unify `cpu65` with `c6510` or turn on `CPU_65c02` in C64 execution.
- Do not invent a root `project(machines)` (Stage 11).
- Do not leave a second `thread.c`, `nuklear.h`, or `am65/` in a machine tree.
- Do not merge the two user manuals. Do not put `agents/` links in manuals.

## am65 is one copy

`src/shell/tools/am65/` is the only assembler source tree. Leftover
`src/machine/c64/src/tools/` keeps d64/g64/t64/crt parsers only.

Nuklear is **one** copy under `src/shell/frontend/`. Disasm CPU class is
NMOS vs 65C02 on `disasm_6502_decode_line`; C64 call sites always pass NMOS.

Help overlay: one `src/shell/tools/gen_help.py`, two books
(`manual/a2m/manual.md`, `manual/c64m/manual.md`). ASCII-only
(`HELP_MARKDOWN.md` beside each book).

## Freeze

Feature work on `a2m.git` and `c64m.git` has stopped. Hotfixes land in
`machines` first. Tag names are in `design/import-revisions.md`.

Control **framing + verb runner + memory-source type** is `src/shell/control/`.
Leftover binaries supply verb tables and memory-source tables. `capabilities`
is generated from the leftover table (static advertisement). Deferred
capacity (a2m 1, c64m 16) and leftover `control_server.c` loops stay leftover.
`hello` is `A2M/14` / `C64M/8`.

HST1 store / find grammar / wire, breakpoint-condition parse (published
LHS table: Apple `cycle_in_line`, C64 `vic_cycle` / `raster`), Forensics,
and disk LED bitmaps are `src/shell`. `runtime_breakpoint_ini.c` stays
leftover (mapping / swap / save-ini policy). FIND is not Inspector.

Shared `runtime_client` subset (run/pause/step, get-cpu, get-memory
`source_id`, breakpoint id-ops, history FIND, inspector enter/leave/land/step
*names*) is `src/shell/runtime/runtime_client_subset.h`. Implementations
stay leftover. Do not include `apple2.h` / `c64.h` from that header.
A2M wire has `enter-inspector` / `leave-inspector`. Record does not arm HST1.

## Verification

From the **machines repo root**:

```bash
make test
./build/a2m/a2m --help
./build/c64m/c64m --help
```

Two `-S` trees (the gate until Stage 11):

```bash
cmake -B build/a2m  -S src/machine/apple2  -DCMAKE_BUILD_TYPE=Debug
cmake -B build/c64m -S src/machine/c64    -DCMAKE_BUILD_TYPE=Debug
cmake --build build/a2m -j && cmake --build build/c64m -j
ctest --test-dir build/a2m  --output-on-failure
ctest --test-dir build/c64m --output-on-failure
```

ctest: a2m **82/82**; c64m **78 pass + 10 SKIP** (CTest 77 without
gitignored leftover `assets/`) **+ `history_control_integration` fails**.
Do not "fix" that fail. Hello shows A2M/14. Help overlay still builds from
each manual.

## Design docs

[`design/README.md`](../design/README.md) indexes writeups. Promote lasting
invariants into `agents/` when the work lands. Do not put design drafts here.
