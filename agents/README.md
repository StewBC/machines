# machines agent handoff

Implementation brief for the **monorepo**. Source is authoritative. Design
drafts live under [`design/`](../design/); they are not product-as-is.

## Read order

1. This file (layout, freeze, what is shared).
2. [`design/merge-stage-map.md`](../design/merge-stage-map.md) — stage map.
   Stage 0 only until that stage's exit criteria are met. Do not start Stage 1
   (`git mv` to `src/machine/`) or any EXTRACT from this note.
3. [`design/import-revisions.md`](../design/import-revisions.md) — imported SHAs,
   freeze tags, baseline ctest.
4. The product handoff for the prefix you are changing:
   - a2m: [`import/a2m/agents/README.md`](../import/a2m/agents/README.md)
   - c64m: [`import/c64m/agents/README.md`](../import/c64m/agents/README.md)

Until Stage 1 relocates the trees, the product-as-is notes still live inside
those prefixes. Do not invent a blended `agents/apple2` / `agents/c64` layout
yet.

## Canonical sources (Stage 0)

**Canonical sources are `import/a2m` and `import/c64m` until Stage 1.**

| Path | What it is |
|------|------------|
| `import/a2m/` | Full a2m tree with history. Still `project(a2m)`. |
| `import/c64m/` | Full c64m tree with history. Still `project(c64m)`. |
| `src/shell/` | Does **not** exist yet. |
| `src/machine/{apple2,c64}` | Does **not** exist yet. |

**Nothing is shared yet** except this file, root `README.md` / `LICENSE`, and
`design/`. Identical files (am65, nuklear, history parse, forensics, …) exist
as **two copies**. That is tolerated only until later EXTRACT stages delete a
copy.

## Do not mix the prefixes

- Do not "fix" a twin in only one prefix. If a bug is in a file that exists in
  both trees, either fix both with the same change or leave both alone until
  EXTRACT lifts one copy.
- Do not merge onto the same `src/` paths. Both trees have
  `src/frontend/frontend.c`.
- Do not `add_subdirectory` both imported `project()` files into one CMake
  invocation. Build with two `-S` trees (see root `Makefile`).
- Do not start cleaning leftover C64 aliases in a2m while it lives under
  `import/`.
- Do not touch Inspector clocks, control protocol, or `frontend.c` as part of
  "the merge." Those are later stages.

## am65 is radioactive

Each prefix has `src/tools/am65/` (37 files, intended to be byte-identical).
**No assembler edits in either copy.** Two copies will fork if you touch one.
One in-tree copy is Stage 3. There is no third am65 remote as a merge
requirement.

## Freeze

Feature work on `a2m.git` and `c64m.git` has stopped. Hotfixes land in
`machines` first. Tag names are in `design/import-revisions.md`.

## Verification (Stage 0)

```bash
make test
./build/a2m/a2m --help
./build/c64m/c64m --help
```

Both ctest gates must match the Stage 0 baseline. c64m SKIP (CTest 77) without
`assets/` is not a fail.

## Design docs

[`design/README.md`](../design/README.md) indexes writeups. Promote lasting
invariants into `agents/` when the work lands. Do not put design drafts here.
