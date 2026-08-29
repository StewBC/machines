# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. If a
note and the code disagree, fix the note (or the code) in the same change.
Design drafts live under [`design/`](../design/); they are not product-as-is.

An agent given **only this file** can find the shared shell versus Apple
versus C64. Do not open `src/c64` to decide Apple silicon. Do not open
`src/apple2` to decide C64 silicon.

## Read order

1. This file (layout, freeze, what is shared).
2. The product handoff for the tree you are changing:
   - a2m: [`apple2/README.md`](apple2/README.md)
   - c64m: [`c64/README.md`](c64/README.md)
3. Shared shell shape, when the work is chrome / control framing / HST1 /
   Inspector *tab*: [`shell/frontend.md`](shell/frontend.md) ·
   [`shell/control.md`](shell/control.md) · [`shell/history.md`](shell/history.md) ·
   [`shell/inspector-shape.md`](shell/inspector-shape.md)
4. [`design/README.md`](../design/README.md) only for in-flight designs.
   Stages 0–11 are done. There is no Stage 12. Historical stage docs may
   still say `src/machine/...`; current trees are `src/{apple2,c64,shell}/`.

## Where things live

**`src/` is emulator / linked C only.** Link-into-both and no machine ifdef →
`src/shell/`. Link-into-one → `src/apple2/` or `src/c64/`. Repo-root
`external/` is the one vendor copy (including C64 TrueType fonts).

Root `CMakeLists.txt` is `project(machines)` (CMake **3.24**). It
`add_executable(a2m)` and `add_executable(c64m)`. Nested leftover
`project(a2m)` / `project(c64m)` is **not** the CI entry — product CMake is
library + tests included from the root (prefixed `apple2_*` / `c64_*`
targets so names do not collide).

| Path | What it is |
|------|------------|
| `src/shell/` | Shared util / platform / nuklear vendor / `control/` framing + verb runner + memory-source type / `runtime/` history+BP conditions + `runtime_client_subset.h` / `frontend/` forensics+disk LEDs+help source + debugger chrome (layout, 6502 CPU pane, disasm pane, memview pane, BP list, window title, Inspector tab) / `tools/{am65,disasm_6502,symbols,gen_help.py}` (linked C + help generator). Static `shell` plus named tool targets. `help_view.c` is compiled by product frontend (per-binary `help_content.inc`). Product picture/key/media/Inspector-film client APIs stay in the product tree. Exclusive Misc tabs (Machine/Debugger/Hardware/Assembler/Config) stay product-local. Inspector clocks stay product-local. |
| `src/apple2/` | a2m silicon, `runtime_thread`, HostFS util, `platform_audio`, product frontend chrome. Flattened (no nested `src/`). Domain package `machine/` holds CPU/video/disk. |
| `src/c64/` | c64m silicon, `runtime_thread`, BASIC/paste util, `platform_audio`, product frontend chrome, format parsers under `tools/{d64,g64,t64,crt}/`. Flattened (no nested `src/`). |
| `external/` | argparse, inih, logc, stb, tiny-regex-c, whereami, C64 TrueType font pack. |
| `roms/` | Flat C64 ROM set (`system.rom`, `character.rom`, `1541.rom`). |
| `samples/apple2/` | Clone-shipped Apple demos (core tracked; local HostFS scratch ignored). |
| `samples/c64/` | Placeholder for curated C64 demos. |
| `symbols/{apple2,c64}/` | Debugger symbol files. |
| `tools/{apple2,c64}/` | Scripts, corpora, profile/helper side binaries (not linked into the emulators as libraries). |
| `assets/` | Personal playground; **not** git-tracked. |
| `manual/a2m/` | Apple user book (`manual.md` + `HELP_MARKDOWN.md`). |
| `manual/c64m/` | C64 user book (`manual.md` + `HELP_MARKDOWN.md`). |
| `tests/shell/` | Tests for shared shell TUs. Registered under both `a2m.*` and `c64m.*` ctest names where they already were. |
| `tests/apple2/` | a2m tests + fixtures. |
| `tests/c64/` | c64m tests + fixtures. Asset SKIP 77 still uses gitignored `assets/`. |
| `agents/apple2/` | a2m product-as-is notes. Paths are repo-root literal (`src/apple2/...`, `src/shell/...`). |
| `agents/c64/` | c64m product-as-is notes. Same literal path convention. |
| `agents/shell/` | Shared debugger *shape* only. Not a silicon story. |

`runtime_thread.c` lives at:

- `src/apple2/runtime/runtime_thread.c`
- `src/c64/runtime/runtime_thread.c`

## Do not mix product trees

- Do not open `src/c64` to decide Apple silicon. Do not open `src/apple2`
  to decide C64 silicon. Shared *shape* is `src/shell/` plus `agents/shell/`.
- Do not "fix" a remaining twin in only one product tree. Remaining twins
  (product `runtime_client` extras, CRT, exclusive tabs) still exist in
  **both** until a later EXTRACT deletes a copy. The shared client
  *subset* is `src/shell/runtime/runtime_client_subset.h`; do not re-fork it.
  Debugger chrome lives in `src/shell/frontend/`; do not re-copy layout/disasm/memview.
- Leftover C64 memory-mode aliases in a2m are gone (`DRIVE8_MAP` et al.).
  The `vic_cycle` BP alias is already gone.
- Do not smash Inspector clocks (Apple F/S vs C64 `film_cycle`) or product
  `runtime_thread` command handling. Shape: [`shell/inspector-shape.md`](shell/inspector-shape.md).
  Clocks: [`apple2/timemachine.md`](apple2/timemachine.md) and
  [`c64/runtime-control.md`](c64/runtime-control.md).
- Do not unify `cpu65` with `c6510` or turn on `CPU_65c02` in C64 execution.
- Do not `add_subdirectory` two leftover `project()` files into one CMake
  invocation (those `project()` calls are gone; product CMake is included
  from the root).
- Do not configure `-S src/apple2` or `-S src/c64` as CI. Nested trees at
  `build/a2m/` / `build/c64m/` collide with the product binaries
  (`./build/a2m`, `./build/c64m`).
- Do not leave a second `thread.c`, `nuklear.h`, or `am65/` in a product tree.
- Do not merge the two user manuals. Do not put `agents/` links in manuals.

## am65 is one copy

`src/shell/tools/am65/` is the only assembler source tree.
`src/c64/tools/` keeps d64/g64/t64/crt parsers only.

Nuklear is **one** copy under `src/shell/frontend/`. Disasm CPU class is
NMOS vs 65C02 on `disasm_6502_decode_line`; C64 call sites always pass NMOS.

Help overlay: one `src/shell/tools/gen_help.py`, two books
(`manual/a2m/manual.md`, `manual/c64m/manual.md`). ASCII-only
(`HELP_MARKDOWN.md` beside each book).

## Freeze

Feature work on `a2m.git`, `c64m.git`, and `am65.git` has stopped. Those
GitHub repos are archived. Hotfixes land in `machines` first. Tag names:
`design/import-revisions.md` and `design/retire-remotes.md`. Archive
record: `design/retire-remotes.md`.

Control **framing + verb runner + memory-source type** is `src/shell/control/`.
Product binaries supply verb tables and memory-source tables. `capabilities`
is generated from the product table (static advertisement). Deferred
capacity (a2m 1, c64m 16) and product `control_server.c` loops stay product-local.
`hello` is `A2M/14` / `C64M/8`.

HST1 store / find grammar / wire, breakpoint-condition parse (published
LHS table: Apple `cycle_in_line`, C64 `vic_cycle` / `raster`), Forensics,
and disk LED bitmaps are `src/shell`. `runtime_breakpoint_ini.c` stays
product-local (mapping / swap / save-ini policy). FIND is not Inspector.

Shared `runtime_client` subset (run/pause/step, get-cpu, get-memory
`source_id`, breakpoint id-ops, history FIND, inspector enter/leave/land/step
*names*) is `src/shell/runtime/runtime_client_subset.h`. Implementations
stay product-local. Do not include `apple2.h` / `c64.h` from that header.
A2M wire has `enter-inspector` / `leave-inspector`. Record does not arm HST1.

## Verification

From the **machines repo root**:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/a2m --help
./build/c64m --help
```

Or `make test`. Product groups: `ctest --test-dir build -L a2m` and
`ctest --test-dir build -L c64m`.

ctest: a2m **82/82**. c64m **78 pass + 10 SKIP** (CTest 77 without
gitignored `assets/`) **+ `c64m.history_control_integration` fails**.
Do not "fix" that fail. Hello shows A2M/14. Help overlay still builds from
each manual.

## Design docs

[`design/README.md`](../design/README.md) indexes monorepo writeups. Promote
lasting invariants into `agents/` when the work lands. Do not put design
drafts here. Landed product design trees under the old leftover prefixes
were removed; use `agents/` and `manual/`.
