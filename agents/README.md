# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. Design
drafts live under [`design/`](../design/); they are not product-as-is.

## Read order

1. This file (layout, freeze, what is shared).
2. [`design/merge-stage-map.md`](../design/merge-stage-map.md) — stage map.
   Stage 3 is done (am65 / disasm_6502 / symbols EXTRACT + disasm CPU class).
   Do not start Stage 4 (control framing), Stage 5 (command tables), or
   Stage 6 (forensics/help) from this note.
3. [`design/shell-extract-platform.md`](../design/shell-extract-platform.md) —
   Stage 2 host layer. [`design/assembler-disasm.md`](../design/assembler-disasm.md) —
   Stage 3 tools.
4. [`design/import-revisions.md`](../design/import-revisions.md) — imported
   SHAs, freeze tags, ctest baseline.
5. The product handoff for the leftover tree you are changing:
   - a2m: [`src/machine/apple2/agents/README.md`](../src/machine/apple2/agents/README.md)
   - c64m: [`src/machine/c64/agents/README.md`](../src/machine/c64/agents/README.md)

Do not invent a blended `agents/apple2` / `agents/c64` layout yet (Stage 10).

## Canonical sources (Stage 3)

**Shared host layer is `src/shell/`** (plus repo-root `external/`). Link-into-both
→ shell. Link-into-one → that leftover machine tree.

| Path | What it is |
|------|------------|
| `src/shell/` | Shared util / platform / nuklear vendor / `tools/{am65,disasm_6502,symbols}`. Static `shell` plus named tool targets. |
| `external/` | argparse, inih, logc, stb, tiny-regex-c, whereami (unprefixed targets). |
| `src/machine/apple2/` | Leftover a2m silicon, `runtime_thread`, leftover util (HostFS), leftover `platform_audio`, leftover frontend chrome. Still `project(a2m)`. |
| `src/machine/c64/` | Leftover c64m silicon, `runtime_thread`, leftover util (BASIC/paste), leftover `platform_audio`, leftover frontend chrome, TrueType, format parsers. Still `project(c64m)`. |

Internal leftover `src/` is **not** flattened. `runtime_thread.c` still lives at:

- `src/machine/apple2/src/runtime/runtime_thread.c`
- `src/machine/c64/src/runtime/runtime_thread.c`

There is still no root `project(machines)` with two `add_executable`s.

## Do not mix leftover trees

- Do not "fix" a remaining twin in only one machine tree. Remaining twins
  (history parse, forensics, help, `disk_led_data`, …) still exist in
  **both** until later EXTRACT deletes a copy.
- Do not flatten `src/machine/apple2/src/machine/cpu65.c`.
- Do not start cleaning leftover C64 aliases in a2m.
- Do not touch Inspector clocks, control protocol, or `frontend.c` chrome
  beyond Stage 3 disasm class call sites already landed.
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

## Verification (Stage 3)

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

ctest: a2m 72/72 (was 71; new `disasm_6502_65c02`); c64m 69 pass + 10 SKIP +
the same `history_control_integration` fail. Do not "fix" that fail.

## Design docs

[`design/README.md`](../design/README.md) indexes writeups. Promote lasting
invariants into `agents/` when the work lands. Do not put design drafts here.
