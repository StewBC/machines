# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. Design
drafts live under [`design/`](../design/); they are not product-as-is.

## Read order

1. This file (layout, freeze, what is shared).
2. [`design/merge-stage-map.md`](../design/merge-stage-map.md) — stage map.
   Stage 5 is done (command tables + memory sources). Stage 6 is done
   (history / BP conditions / forensics / help EXTRACT).
   Do not start Stage 7 (`runtime_client`), Stage 8, or Stage 9 from this note.
3. [`design/shell-extract-platform.md`](../design/shell-extract-platform.md) —
   Stage 2 host layer. [`design/assembler-disasm.md`](../design/assembler-disasm.md) —
   Stage 3 tools. [`design/control-framing.md`](../design/control-framing.md) —
   Stage 4 framing. [`design/control-command-tables.md`](../design/control-command-tables.md) —
   Stage 5 command tables. [`design/runtime-shell-extract.md`](../design/runtime-shell-extract.md) —
   Stage 6 runtime shell twins.
4. [`design/import-revisions.md`](../design/import-revisions.md) — imported
   SHAs, freeze tags, ctest baseline.
5. The product handoff for the leftover tree you are changing:
   - a2m: [`src/machine/apple2/agents/README.md`](../src/machine/apple2/agents/README.md)
   - c64m: [`src/machine/c64/agents/README.md`](../src/machine/c64/agents/README.md)

Do not invent a blended `agents/apple2` / `agents/c64` layout yet (Stage 10).

## Canonical sources (Stage 5+6)

**Shared host layer is `src/shell/`** (plus repo-root `external/`). Link-into-both
→ shell. Link-into-one → that leftover machine tree.

| Path | What it is |
|------|------------|
| `src/shell/` | Shared util / platform / nuklear vendor / `control/` framing + verb runner + memory-source type / `runtime/` history+BP conditions / `frontend/` forensics+disk LEDs+help source / `tools/{am65,disasm_6502,symbols,gen_help.py}`. Static `shell` plus named tool targets. `help_view.c` is compiled by leftover frontend (per-binary `help_content.inc`). |
| `external/` | argparse, inih, logc, stb, tiny-regex-c, whereami (unprefixed targets). |
| `src/machine/apple2/` | Leftover a2m silicon, `runtime_thread`, leftover util (HostFS), leftover `platform_audio`, leftover frontend chrome. Still `project(a2m)`. |
| `src/machine/c64/` | Leftover c64m silicon, `runtime_thread`, leftover util (BASIC/paste), leftover `platform_audio`, leftover frontend chrome, TrueType, format parsers. Still `project(c64m)`. |

Internal leftover `src/` is **not** flattened. `runtime_thread.c` still lives at:

- `src/machine/apple2/src/runtime/runtime_thread.c`
- `src/machine/c64/src/runtime/runtime_thread.c`

There is still no root `project(machines)` with two `add_executable`s.

## Do not mix leftover trees

- Do not "fix" a remaining twin in only one machine tree. Remaining twins
  (`runtime_client`, layout/disasm chrome, …) still exist in **both**
  until later EXTRACT deletes a copy.
- Do not flatten `src/machine/apple2/src/machine/cpu65.c`.
- Leftover C64 memory-mode aliases in a2m are gone (`DRIVE8_MAP` et al.).
  The `vic_cycle` BP alias is already gone.
- Do not touch Inspector clocks, leftover `runtime_thread` command handling,
  or `frontend.c` chrome beyond Stage 5 memory-source ids.
- Do not unify `cpu65` with `c6510` or turn on `CPU_65c02` in C64 execution.
- Do not invent a root `project(machines)` (Stage 11).
- Do not leave a second `thread.c`, `nuklear.h`, or `am65/` in a machine tree.

## am65 is one copy

`src/shell/tools/am65/` is the only assembler source tree. Leftover
`src/machine/c64/src/tools/` keeps d64/g64/t64/crt parsers only.

Nuklear is **one** copy under `src/shell/frontend/`. Disasm CPU class is
NMOS vs 65C02 on `disasm_6502_decode_line`; C64 call sites always pass NMOS.

## Freeze

Feature work on `a2m.git` and `c64m.git` has stopped. Hotfixes land in
`machines` first. Tag names are in `design/import-revisions.md`.

Control **framing + verb runner + memory-source type** is `src/shell/control/`.
Leftover binaries supply verb tables and memory-source tables. `capabilities`
is generated from the leftover table (static advertisement). Deferred
capacity (a2m 1, c64m 16) and leftover `control_server.c` loops stay leftover.
`hello` is still `A2M/13` / `C64M/8`.

HST1 store / find grammar / wire, breakpoint-condition parse (published
LHS table: Apple `cycle_in_line`, C64 `vic_cycle` / `raster`), Forensics,
and disk LED bitmaps are `src/shell`. `runtime_breakpoint_ini.c` stays
leftover (mapping / swap / save-ini policy). FIND is not Inspector.

## Verification (Stage 5+6)

```bash
make test
./build/a2m/a2m --help
./build/c64m/c64m --help
```

Two `-S` trees:

```bash
cmake -B build/a2m  -S src/machine/apple2  -DCMAKE_BUILD_TYPE=Debug
cmake -B build/c64m -S src/machine/c64    -DCMAKE_BUILD_TYPE=Debug
```

ctest: a2m 75/75 plus new `control_command_table` / `memory_source` tests;
c64m 69 pass + 10 SKIP + the same `history_control_integration` fail, plus
the same new shell tests. Do not "fix" that fail.

## Design docs

[`design/README.md`](../design/README.md) indexes writeups. Promote lasting
invariants into `agents/` when the work lands. Do not put design drafts here.
