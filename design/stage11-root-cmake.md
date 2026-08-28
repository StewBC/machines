# Stage 11 — Root two-target CMake (landed)

| Field | Value |
|-------|-------|
| **Author** | Grok |
| **Date** | 2026-08-27 |
| **Status** | Landed |
| **Canonical path** | [`design/stage11-root-cmake.md`](stage11-root-cmake.md) |
| **Stage map** | [`merge-stage-map.md`](merge-stage-map.md) Stage 11 + Key Decision 14 |

No follow-on design. This is the landed note: CMake layout, binary paths,
test counts, remotes.

## Layout

Root `CMakeLists.txt` is `project(machines)` with
`cmake_minimum_required(VERSION 3.24)` and
`CMAKE_LINK_LIBRARIES_STRATEGY REORDER_FREELY`. Stage 0 already showed
c64m configures on 3.24; the leftover 3.28 line was a version *gate* only.
Do not raise the root to 3.28.

```text
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/a2m --help
./build/c64m --help
```

Binaries: `./build/a2m`, `./build/c64m`, `./build/am65`.
`enable_testing()` is at the root. One CMake generation.

Leftover silicon CMake has **no** `project()` / `cmake_minimum_required()`.
Root `add_subdirectory`s `src/machine/apple2` and `src/machine/c64` after
`external/` and `src/shell/`. Leftover library targets are prefixed
(`apple2_machine` vs `c64_machine`, …) because CMake target names are
global. Product executables keep names `a2m` / `c64m`. Nested
`-S src/machine/{apple2,c64}` is not the CI entry.

Nested leftover `src/` is **not** flattened:

- `src/machine/apple2/src/runtime/runtime_thread.c`
- `src/machine/c64/src/runtime/runtime_thread.c`
- `src/machine/apple2/src/machine/cpu65.c`
- `src/machine/c64/src/machine/c6510.c`

`import/` is gone (Stage 1). One `src/shell/tools/am65/`, one
`external/`, one `src/shell/frontend/nuklear.h`.

## Tests

CTest names are prefixed so both products share one test dir. Labels
preserve Stage 10 groups:

| Label | Count | Result |
|-------|-------|--------|
| `a2m` | 82 | **82/82 passed** |
| `c64m` | 89 | **78 pass + 10 SKIP** (CTest 77, no leftover `assets/`) **+ `c64m.history_control_integration` fails** |

Do not fix `history_control_integration`. `ctest -L a2m` / `ctest -L c64m`
or unfiltered `ctest --test-dir build` (171 tests: 82 + 89).

## Remotes

a2m.git / c64m.git freeze tags: [`import-revisions.md`](import-revisions.md).
GitHub repos archived + freeze README: [`retire-remotes.md`](retire-remotes.md).
