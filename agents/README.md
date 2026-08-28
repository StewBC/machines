# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. Design
drafts live under [`design/`](../design/); they are not product-as-is.

## Read order

1. This file (layout, freeze, what is shared).
2. [`design/merge-stage-map.md`](../design/merge-stage-map.md) — stage map.
   Stage 1 is done (leftover trees relocated). Do not start Stage 2 EXTRACT
   (`src/shell`, platform/util/nuklear) from this note.
3. [`design/import-revisions.md`](../design/import-revisions.md) — imported
   SHAs, freeze tags, baseline ctest, Stage 1 rename.
4. The product handoff for the tree you are changing:
   - a2m: [`src/machine/apple2/agents/README.md`](../src/machine/apple2/agents/README.md)
   - c64m: [`src/machine/c64/agents/README.md`](../src/machine/c64/agents/README.md)

Do not invent a blended `agents/apple2` / `agents/c64` layout yet (Stage 10).

## Canonical sources (Stage 1)

**Leftover silicon is `src/machine/apple2` and `src/machine/c64`.** `import/`
is gone (history stays in git). Each tree is still its own CMake root
(`project(a2m)` / `project(c64m)`). Internal layout is **not** flattened:
`main.c`, `src/runtime/`, `src/frontend/`, `src/machine/` still live inside
each product tree.

| Path | What it is |
|------|------------|
| `src/machine/apple2/` | Full a2m tree. Still `project(a2m)`. |
| `src/machine/c64/` | Full c64m tree. Still `project(c64m)`. |
| `src/shell/` | Does **not** exist yet. |

`runtime_thread.c` lives at:

- `src/machine/apple2/src/runtime/runtime_thread.c`
- `src/machine/c64/src/runtime/runtime_thread.c`

**Twins still exist in both trees** until EXTRACT deletes a copy. Identical
files (am65, nuklear, history parse, forensics, …) are two copies. That is
tolerated until later EXTRACT stages. **Nothing is shared yet** except this
file, root `README.md` / `LICENSE`, and `design/`.

## Do not mix the leftover trees

- Do not "fix" a twin in only one tree. If a bug is in a file that exists in
  both, either fix both with the same change or leave both alone until EXTRACT
  lifts one copy.
- Do not merge onto the same inner `src/` paths. Both trees have
  `src/frontend/frontend.c`.
- Do not `add_subdirectory` both nested `project()` files into one CMake
  invocation. Build with two `-S` trees (see root `Makefile`).
- Do not flatten `src/machine/apple2/src/machine/cpu65.c` →
  `src/machine/apple2/cpu65.c` (optional Stage 10).
- Do not start cleaning leftover C64 aliases in a2m.
- Do not touch Inspector clocks, control protocol, or `frontend.c` as part of
  "the merge." Those are later stages.
- Do not invent a root `project(machines)` with two `add_executable`s (Stage 11).

## am65 is radioactive

Each leftover tree has `src/tools/am65/` (37 files, intended to be
byte-identical). **No assembler edits in either copy.** Two copies will fork
if you touch one. One in-tree copy is Stage 3. There is no third am65 remote
as a merge requirement. Same for nuklear until Stage 2.

## Freeze

Feature work on `a2m.git` and `c64m.git` has stopped. Hotfixes land in
`machines` first. Tag names are in `design/import-revisions.md`.

## Verification (Stage 1)

```bash
make test
./build/a2m/a2m --help
./build/c64m/c64m --help
```

That is two `-S` trees:

```bash
cmake -B build/a2m  -S src/machine/apple2  -DCMAKE_BUILD_TYPE=Debug
cmake -B build/c64m -S src/machine/c64    -DCMAKE_BUILD_TYPE=Debug
```

Both ctest gates must match the Stage 0 baseline. c64m SKIP (CTest 77) without
`assets/` is not a fail. Do not "fix" `history_control_integration`.

## Design docs

[`design/README.md`](../design/README.md) indexes writeups. Promote lasting
invariants into `agents/` when the work lands. Do not put design drafts here.
