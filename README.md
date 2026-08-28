# machines

Home of two C99 emulators that share a debugger *shell* over time, not silicon:

- **a2m** — Apple ][+ / //e Enhanced (protocol `A2M/N`)
- **c64m** — Commodore 64 (protocol `C64M/N`)

They remain **two binaries**. There is no dual-machine executable, no plugin
loader, and no shared `runtime_thread`.

Leftover silicon lives under `src/machine/{apple2,c64}` (Stage 1 relocate).
Each tree is still its own CMake root. Internal `src/` / `main.c` layout is
not flattened. `src/shell/` does not exist yet.

| Path | Product | Build as |
|------|---------|----------|
| `src/machine/apple2/` | a2m | its own `project(a2m)` |
| `src/machine/c64/` | c64m | its own `project(c64m)` |

Nothing is shared yet except this README, `LICENSE`, `agents/`, and `design/`.
Twins still exist in **both** leftover trees until EXTRACT deletes a copy.
Do not `add_subdirectory` both nested `project()` files into one CMake
invocation.

The program of work is [`design/merge-stage-map.md`](design/merge-stage-map.md).
Imported SHAs and Stage 0 commands: [`design/import-revisions.md`](design/import-revisions.md).

## Build (Stage 1)

Requires CMake, SDL2, and Python 3 (help generation). On macOS: `brew install cmake sdl2`.

```bash
make test
```

That configures **two separate** source trees, builds both, and runs both ctest
gates:

```bash
cmake -B build/a2m  -S src/machine/apple2  -DCMAKE_BUILD_TYPE=Debug
cmake -B build/c64m -S src/machine/c64    -DCMAKE_BUILD_TYPE=Debug
cmake --build build/a2m -j && cmake --build build/c64m -j
ctest --test-dir build/a2m  --output-on-failure
ctest --test-dir build/c64m --output-on-failure
```

Then `./build/a2m/a2m --help` and `./build/c64m/c64m --help`.

c64m may SKIP about ten tests (CTest code 77) when gitignored `assets/` is
missing. That is the baseline, not a failure. On the imported SHA, c64m
`history_control_integration` also fails in the original source tree; that is
pre-existing, not an import regression.

Root CMake minimum for the later two-target build is **3.24**. Each imported
tree still uses its own `cmake_minimum_required` until Stage 11. Stage 0
confirmed c64m's 3.28 gate is a version line only (`REORDER_FREELY` is 3.24);
do not raise the root to 3.28.

## Freeze policy

`machines` is the home. Feature work on `a2m.git` (`https://github.com/StewBC/a2m.git`)
and `c64m.git` (`https://github.com/StewBC/c64m.git`) **stops** at the freeze
tags recorded in [`design/import-revisions.md`](design/import-revisions.md).

- No feature PRs on those remotes after the freeze tags.
- Hotfixes during migration land **here first**. Cherry-pick back to a frozen
  branch only if a shipped binary still needs it.
- Do not resume Inspector work (or any other feature) in the old trees.

## License

Public domain (Unlicense), except vendored files under `external/` — see
[`LICENSE`](LICENSE).

## Agents

Read [`agents/README.md`](agents/README.md) before editing either leftover tree.
