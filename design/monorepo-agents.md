# Stage 10 — Agent handoff, manuals, tests layout

| Field | Value |
|-------|-------|
| **Author** | Grok (Designer) |
| **Date** | 2026-08-27 |
| **Status** | Accepted |
| **Canonical path** | [`design/monorepo-agents.md`](monorepo-agents.md) |
| **Stage map** | [`design/merge-stage-map.md`](merge-stage-map.md) Stage 10 (EXTRACT docs/tests layout + PRESERVE two manuals, two product stories) |
| **Depends on** | Stage 9 ([`design/inspector-unification.md`](inspector-unification.md)). |

This is the detailed design for Stage 10. It does not reopen the stage map. Key Decision 3 (target layout `src/shell` vs `src/machine/{apple2,c64}`), the Stage 10 in-scope/out-of-scope lists, and the standing invariants (two binaries, no ifdef in shell, two manuals, no Inspector clock merge) are folded in as constraints.

**Choice: repo-root `agents/`, `manual/`, `tests/` with leftover stubs.** Nested leftover `src/` is **not** flattened. Nested `project(a2m)` / `project(c64m)` stay the ctest gate. No root `add_executable(a2m)`.

---

## Overview

Stages 1–9 moved code. Leftover product notes, two manuals, and tests still sit inside the leftover CMake trees. Stage 10 EXTRACT makes the monorepo navigable: an agent given only `agents/README.md` can find shell vs apple2 vs c64 without the stage map.

This is layout, not product behavior. Apple `timemachine.md` and C64 Inspector clock notes stay two silicon stories. Help still compiles from two books through one `src/shell/tools/gen_help.py`.

---

## Directory plan (locked)

```text
machines/
  agents/
    README.md                 # monorepo handoff; source wins; find shell vs leftover
    shell/
      frontend.md             # shared chrome shape
      control.md              # framing + verb-table runner
      history.md              # HST1 FIND; not Inspector
      inspector-shape.md      # Record/Inspect/Land/Leave/NOW/film-vs-reconstruct
    apple2/                   # a2m leftover agents/*.md, product-as-is
    c64/                      # c64m leftover agents/*.md, product-as-is
  manual/
    a2m/manual.md             # + HELP_MARKDOWN.md (Apple book)
    c64m/manual.md            # + HELP_MARKDOWN.md (C64 book)
  tests/
    shell/                    # from src/shell/tests/
    apple2/                   # from src/machine/apple2/tests/
    c64/                      # from src/machine/c64/tests/
  src/
    shell/                    # shared C (unchanged this stage)
    machine/apple2/           # leftover silicon; nested project(a2m)
    machine/c64/              # leftover silicon; nested project(c64m)
```

### agents/

- **Move, do not copy.** `git mv` leftover `src/machine/apple2/agents/*.md` → `agents/apple2/` and leftover `src/machine/c64/agents/*.md` → `agents/c64/`.
- Leftover trees keep a **stub** `agents/README.md` that points at the repo-root notes. Dual copies of product notes are forbidden (am65-again for docs).
- `agents/shell/*.md` are **new** shape notes. They are not a merge of leftover files. Do not fold Apple F/S pairing and C64 `film_cycle` / pink / vic-ring into one silicon story.
- Leftover notes keep product verbs, exclusive Misc tabs, clocks, and silicon. Add a pointer at the top of leftover frontend / control / Inspector notes to the matching `agents/shell/` file.
- Stale two-remote wording dies: "do not open c64m.git" → "do not open `src/machine/c64` to decide Apple silicon" (and the reverse).
- Bare `src/...` inside `agents/apple2/` means leftover `src/machine/apple2/src/...` unless the path already starts with `src/shell/`, `manual/`, or `tests/`. Same for `agents/c64/`. The leftover README states that convention.

### manuals/

Two books. `HELP_MARKDOWN.md` stays **with each book** (they already differ: Apple documents table `\|` escapes). Do not unify into one `HELP_MARKDOWN.md`. ASCII-only UI/manual rule remains. No `agents/` links in manuals (already true; keep it).

CMake help generation (leftover frontend):

```text
${MACHINES_ROOT}/src/shell/tools/gen_help.py
${MACHINES_ROOT}/manual/{a2m,c64m}/manual.md
```

One `gen_help.py`. Per-binary `help_content.inc`.

Leftover product `README.md` links retarget to `../../../manual/{a2m,c64m}/manual.md`.

### tests/

`git mv` as far as nested `-S` builds allow. Nested leftover CMake keeps registering tests; it just points at repo-root paths:

```text
${MACHINES_ROOT}/tests/shell/...
${MACHINES_ROOT}/tests/apple2/...
${MACHINES_ROOT}/tests/c64/...
```

Compile definitions:

| Macro | Stays / becomes | Why |
|-------|-----------------|-----|
| `A2M_SOURCE_DIR` | leftover `src/machine/apple2` | `samples/` |
| `A2M_FIXTURE_DIR` | `${MACHINES_ROOT}/tests/apple2/fixtures` | moved fixtures |
| `A2M_SAMPLE_DIR` | `${MACHINES_ROOT}/tests/apple2/fixtures/asm` | moved |
| `C64M_SOURCE_DIR` | leftover `src/machine/c64` | `roms/`, gitignored `assets/` |
| `C64M_TEST_DIR` | `${MACHINES_ROOT}/tests/c64` | snapshot fixture `tests/c64/fixtures/` |

C64 Python control tests keep leftover as `REPO_ROOT` (`tools/c64_control_client.py` still lives there). Only the script path moves to `tests/c64/control/`.

Quoted includes such as `#include "../test_file.h"` stay valid because the relative tree under `tests/{apple2,c64}/` is unchanged.

c64m asset SKIP 77 is preserved. Do not "fix" `history_control_integration`.

Root Makefile (or the documented two-dir cmake) remains the gate until Stage 11. Do not invent root `project(machines)`.

### Flatten leftover nested `src/`

**Skip.** `src/machine/apple2/src/machine/cpu65.c` stays nested. Flatten is rename-only in theory and include-path chaos in practice. Nested leftover `src/` is still OK.

---

## agents/shell contents (shape only)

| File | Owns | Does not own |
|------|------|--------------|
| `frontend.md` | Layout slot, 6502 CPU pane, disasm pane, memview pane, BP list chrome, window-title parameter, help overlay, Forensics, Inspector *tab*, ASCII-only UI | Exclusive Misc tabs, leftover input, leftover CRT paint, Apple vs C64 memory-source *tables* |
| `control.md` | Framing (`id` / `verb` / rest), verb-table runner, capabilities as static advertisement, `hello` parameterized `A2M/N` vs `C64M/N`, localhost bind | Product verb tables, deferred capacity 1 vs 16, leftover `control_server.c` loops |
| `history.md` | HST1 store / find grammar / wire, FIND ≠ Inspector, Record does not arm HST1 | Leftover `runtime_history` observer install, leftover session policy |
| `inspector-shape.md` | Record / Inspect / Land / Leave / NOW / sealed / film-vs-reconstruct / pink-on-miss where the machine says so | Apple F/S + sample IDs; C64 `film_cycle` + vic-ring + `--inspector-off-on-max` |

Clock paragraphs stay in `agents/apple2/timemachine.md` and `agents/c64/runtime-control.md`.

---

## Nested leftover CMake (still the gate)

```bash
cmake -B build/a2m  -S src/machine/apple2  -DCMAKE_BUILD_TYPE=Debug
cmake -B build/c64m -S src/machine/c64    -DCMAKE_BUILD_TYPE=Debug
cmake --build build/a2m -j && cmake --build build/c64m -j
ctest --test-dir build/a2m  --output-on-failure
ctest --test-dir build/c64m --output-on-failure
./build/a2m/a2m --help
./build/c64m/c64m --help
```

`MACHINES_ROOT` remains `src/machine/{apple2,c64}/../../..`. Do not `add_subdirectory` both nested `project()` files into one CMake invocation.

---

## Out of scope

- One user manual.
- Product behavior changes (Inspector clocks, leftover `runtime_thread`, CPU cores).
- Root two-target CMake (Stage 11).
- Flatten leftover `src/machine/apple2/src/…`.
- Merging leftover `timemachine.md` with C64 Inspector notes.
- Putting design drafts in `agents/`.

---

## Key Decisions

1. **Repo-root `agents/{README,shell,apple2,c64}`.** Leftover `agents/*.md` move, not blend. Leftover trees keep a stub pointer only.
2. **`agents/shell/` is four shape notes**, named by the stage map. They do not absorb leftover silicon stories.
3. **Two manuals at `manual/{a2m,c64m}/`.** Two `HELP_MARKDOWN.md` files stay with the books. One `gen_help.py`.
4. **Tests at `tests/{shell,apple2,c64}`.** Nested CMake retargets. `C64M_SOURCE_DIR` stays leftover (ROMs/assets). New `C64M_TEST_DIR` for moved fixtures.
5. **Do not flatten leftover nested `src/`.**
6. **Nested `project()` remains the gate.** Root Makefile documents the two `-S` trees. No `project(machines)`.

---

## Exit

- An agent given only `agents/README.md` can find `src/shell` vs `src/machine/apple2` vs `src/machine/c64`.
- Both manuals still generate help. No `agents/` links in manuals.
- a2m ctest 82/82. c64m: same known `history_control_integration` fail + asset SKIP 77.
- Nested leftover `project()` still configures via `-S src/machine/{apple2,c64}`.
- Stage 11 not started.
