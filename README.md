# machines

Home of two C99 emulators that share a debugger *shell*, not silicon:

- **a2m** — Apple ][+ / //e Enhanced (protocol `A2M/N`) —
  [product README](doc/README-A2M.md)
- **c64m** — Commodore 64 (protocol `C64M/N`) —
  [product README](doc/README-C64.md)

They remain **two binaries**. There is no dual-machine executable, no plugin
loader, and no shared `runtime_thread`. Root CMake (`project(machines)`) is
the build entry. Nested leftover `project(a2m)` / `project(c64m)` is retired.

Host primitives that link into both binaries live under `src/shell/` and
repo-root `external/`. Product silicon lives under `src/apple2/` and
`src/c64/`.

| Path | Product | Build as |
|------|---------|----------|
| `src/shell/` | shared debugger shell | `libshell.a` |
| `src/apple2/` | a2m leftover silicon | `apple2_*` libraries + `a2m` |
| `src/c64/` | c64m leftover silicon | `c64_*` libraries + `c64m` |
| `manual/a2m/` | a2m user book | help overlay |
| `manual/c64m/` | c64m user book | help overlay |
| `tests/shell/` | shared tests | both product ctest labels |
| `tests/apple2/` | a2m leftover tests | `ctest -L a2m` |
| `tests/c64/` | c64m leftover tests | `ctest -L c64m` |
| `agents/` | handoff: `shell/` vs `apple2/` vs `c64/` | — |

Agent notes: [`agents/README.md`](agents/README.md).
The program of work is [`design/merge-stage-map.md`](design/merge-stage-map.md)
(Stage 11 landed). Imported SHAs: [`design/import-revisions.md`](design/import-revisions.md).
Root CMake layout: [`design/stage11-root-cmake.md`](design/stage11-root-cmake.md).

## Build

Requires CMake **3.24+**, SDL2, and Python 3 (help generation). On macOS:
`brew install cmake sdl2`. Root stays 3.24 (`REORDER_FREELY`); do not raise
to 3.28 — c64m's old 3.28 line was a version gate only.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/a2m --help
./build/c64m --help
```

Or `make test` from the repo root. One CMake generation builds both
binaries. Binaries land at `./build/a2m`, `./build/c64m`, and `./build/am65`.

CTest names are prefixed (`a2m.audio_buffer`, `c64m.sid`) so both products
share one test dir. Labels match Stage 10 gates:

```bash
ctest --test-dir build -L a2m   # 82 tests
ctest --test-dir build -L c64m  # 89 tests
```

c64m may SKIP about ten tests (CTest code 77) when gitignored leftover
`assets/` is missing. That is the baseline, not a failure.
`c64m.history_control_integration` still fails; that is pre-existing, not
a merge regression. Do not "fix" it here.

## Freeze / retired remotes

`machines` is the only home. Feature work on `a2m.git`
(`https://github.com/StewBC/a2m.git`), `c64m.git`
(`https://github.com/StewBC/c64m.git`), and the assembler hub `am65.git`
(`https://github.com/StewBC/am65.git`) **stopped** at the freeze tags in
[`design/import-revisions.md`](design/import-revisions.md) and
[`design/retire-remotes.md`](design/retire-remotes.md). Those GitHub
repos are **archived** (read-only) with a freeze README. Assembler source
is `src/shell/tools/am65/` (one copy; not a hub).

## License

Public domain (Unlicense), except vendored files under `external/` — see
[`LICENSE`](LICENSE).

## Agents

Read [`agents/README.md`](agents/README.md) before editing. Shared shell vs
leftover Apple vs leftover C64 is named there.
