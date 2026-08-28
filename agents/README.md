# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. Design
drafts live under [`design/`](../design/); they are not product-as-is.

## Read order

1. This file (layout, freeze, what is shared).
2. [`design/merge-stage-map.md`](../design/merge-stage-map.md) — stage map.
   Stage 2 is done (platform / util / external / nuklear EXTRACT). Do not start
   Stage 3 (am65), Stage 4 (control framing), or Stage 6 (forensics/help)
   from this note.
3. [`design/shell-extract-platform.md`](../design/shell-extract-platform.md) —
   what moved vs stayed.
4. [`design/import-revisions.md`](../design/import-revisions.md) — imported
   SHAs, freeze tags, ctest baseline.
5. The product handoff for the leftover tree you are changing:
   - a2m: [`src/machine/apple2/agents/README.md`](../src/machine/apple2/agents/README.md)
   - c64m: [`src/machine/c64/agents/README.md`](../src/machine/c64/agents/README.md)

Do not invent a blended `agents/apple2` / `agents/c64` layout yet (Stage 10).

## Canonical sources (Stage 2)

**Shared host layer is `src/shell/`** (plus repo-root `external/`). Link-into-both
→ shell. Link-into-one → that leftover machine tree.

| Path | What it is |
|------|------------|
| `src/shell/` | Shared util / platform (window, fs, socket) / nuklear vendor. Static `shell`. |
| `external/` | argparse, inih, logc, stb, tiny-regex-c, whereami (unprefixed targets). |
| `src/machine/apple2/` | Leftover a2m silicon, `runtime_thread`, leftover util (HostFS), leftover `platform_audio`, leftover frontend chrome. Still `project(a2m)`. |
| `src/machine/c64/` | Leftover c64m silicon, `runtime_thread`, leftover util (BASIC/paste), leftover `platform_audio`, leftover frontend chrome, TrueType. Still `project(c64m)`. |

Internal leftover `src/` is **not** flattened. `runtime_thread.c` still lives at:

- `src/machine/apple2/src/runtime/runtime_thread.c`
- `src/machine/c64/src/runtime/runtime_thread.c`

There is still no root `project(machines)` with two `add_executable`s.

## Do not mix leftover trees

- Do not "fix" a remaining twin in only one machine tree. Remaining twins
  (history parse, forensics, help, am65, `disk_led_data`, …) still exist in
  **both** until later EXTRACT deletes a copy.
- Do not flatten `src/machine/apple2/src/machine/cpu65.c`.
- Do not start cleaning leftover C64 aliases in a2m.
- Do not touch Inspector clocks, control protocol, or `frontend.c` as part of
  "the merge" beyond the Stage 2 `host_log` include rename already landed.
- Do not edit `src/tools/am65/` in either leftover tree (Stage 3).
- Do not invent a root `project(machines)` (Stage 11).
- Do not leave a second `thread.c` or `nuklear.h` in a machine tree.

## am65 is still radioactive

Each leftover tree still has `src/tools/am65/`. **No assembler edits in either
copy.** One in-tree copy is Stage 3.

Nuklear is **one** copy under `src/shell/frontend/`.

## Freeze

Feature work on `a2m.git` and `c64m.git` has stopped. Hotfixes land in
`machines` first. Tag names are in `design/import-revisions.md`.

## Verification (Stage 2)

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

ctest must match Stage 1: a2m 71/71; c64m 69 pass + 10 SKIP + the same
`history_control_integration` fail. Do not "fix" that fail.

## Design docs

[`design/README.md`](../design/README.md) indexes writeups. Promote lasting
invariants into `agents/` when the work lands. Do not put design drafts here.
