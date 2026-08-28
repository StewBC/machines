# Import revisions (Stage 0)

| Field | Value |
|-------|-------|
| **Author** | Grok |
| **Date** | 2026-08-27 |
| **Status** | Landed (Stage 0) |
| **Canonical path** | [`design/import-revisions.md`](import-revisions.md) |

Execution notes for Stage 0 of [`merge-stage-map.md`](merge-stage-map.md). Not a new architecture.

## Imported SHAs

Design-time HEADs were a2m `d863ad9` and c64m `7f3c1ab`. Current HEADs at import were the same:

| Product | Origin | Branch | Original SHA (freeze) | Prefixed tip in `machines` |
|---------|--------|--------|-----------------------|----------------------------|
| a2m | `https://github.com/StewBC/a2m.git` (local `/Users/swessels/Develop/github/personal/a2m`) | `master` | `d863ad98487639445db16134d260221839cb72e9` | `acd2d5e956f8644bfc2ee09da2e01be721ef46d9` |
| c64m | `https://github.com/StewBC/c64m.git` (local `/Users/swessels/Develop/github/personal/c64m`) | `main` | `7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a` | `53ad899afa6c405da33d864803c64dbd6b49b3ca` |

`git log -- import/a2m` / `git log -- import/c64m` show product commits (553 / 596 path commits). Trees are not mixed: there is no repo-root `src/`.

## Exact git commands

`machines` was not a git repo. `git subtree` exists on this host, but a plain `git subtree add --prefix=…` keeps original paths inside the old commits, so `git log -- import/a2m` only shows the merge. Stage 0 exit wants path-limited log to show product commits. Equivalent: rewrite each history into the prefix, then merge unrelated histories.

```bash
# PR 0.1
cd /Users/swessels/Develop/github/personal/machines
git init
# README.md LICENSE agents/README.md design/ .gitignore
git commit -m "docs: bootstrap machines repo and merge stage map"

FILTER="$HOME/Library/Python/3.9/bin/git-filter-repo"

# PR 0.2 — a2m
git clone --no-local /Users/swessels/Develop/github/personal/a2m /tmp/a2m-prefixed
(cd /tmp/a2m-prefixed && "$FILTER" --to-subdirectory-filter import/a2m --refs refs/heads/master --force)
git remote add a2m-import /tmp/a2m-prefixed
git fetch --no-tags a2m-import
git merge --allow-unrelated-histories --no-ff a2m-import/master
# message: import: subtree a2m into import/a2m

# PR 0.3 — c64m
git clone --no-local --single-branch --branch main \
  /Users/swessels/Develop/github/personal/c64m /tmp/c64m-prefixed
(cd /tmp/c64m-prefixed && "$FILTER" --to-subdirectory-filter import/c64m --refs refs/heads/main --force)
git remote add c64m-import /tmp/c64m-prefixed
git fetch --no-tags c64m-import
git merge --allow-unrelated-histories --no-ff c64m-import/main
# message: import: subtree c64m into import/c64m

git remote add a2m  /Users/swessels/Develop/github/personal/a2m
git remote add c64m /Users/swessels/Develop/github/personal/c64m
git fetch --no-tags a2m
git fetch --no-tags c64m
```

Do not `add_subdirectory` both imported `project()` files into one CMake invocation. Build with two `-S` trees (root `Makefile`).

## Baseline vs post-import ctest

Host CMake 4.4.2, Debug, `ctest --output-on-failure`. Logs: [`design/stage0-logs/`](stage0-logs/).

| Gate | Baseline (source tree, same SHA) | Post-import (`-B build/{a2m,c64m} -S import/…`) |
|------|----------------------------------|--------------------------------------------------|
| a2m | **71/71 passed** (`build-stage0-baseline`) | **71/71 passed** |
| c64m | **79 passed, 1 failed** out of 80; `assets/` present so 0 SKIP | **69 passed, 10 skipped (CTest 77), 1 failed** out of 80 |

c64m SKIPs (no gitignored `assets/` in the import): `c64_snapshot_1541_midload`, `c64_disk_load`, `c64_real_1541_load`, `c64_robocop_g64`, `c64_arkanoid_g64`, `c64_arkanoid_alt_g64`, `d64`, `t64`, `runtime_disk`, `runtime_real_1541_autorun`. Allowed; not a regression.

c64m `history_control_integration` failed on the source-tree SHA as well (Debug and existing Release `build/`; `assert writes["records"]` in `tests/control/test_history_control.py`). Pre-existing; not an import regression. Do not "fix" it as merge fallout.

a2m `app_options_mounts` used `../tests/fixtures/…` (assumes `build/` is a child of the product root). That breaks `-B build/a2m -S import/a2m`. Stage 0 pointed the test at `A2M_FIXTURE_DIR` (`${CMAKE_SOURCE_DIR}/tests/fixtures`), matching other a2m tests. Not done in `a2m.git`.

Both binaries: `./build/a2m/a2m --help` and `./build/c64m/c64m --help` run.

## CMake 3.24

Portable CMake **3.24.4** (`/tmp/cmake-3.24.4-macos-universal/…/cmake`):

| Tree | As imported | Diagnostic |
|------|-------------|------------|
| `import/a2m` (`cmake_minimum_required` 3.16) | **configures** | — |
| `import/c64m` (`cmake_minimum_required` 3.28) | **fails** (version gate only) | Patching the minimum to 3.24 in a **temp copy** (not committed): **configures**. Only 3.24-era feature used is `CMAKE_LINK_LIBRARIES_STRATEGY REORDER_FREELY`. |

**Do not raise the machines root to 3.28.** Stage 11 stays **3.24**. Do not change c64m's own `cmake_minimum_required` in Stage 0.

## Freeze tags

Feature work on `a2m.git` / `c64m.git` stops. Hotfixes land in `machines` first.

| Remote | Tag | Annotated tag object | Points at |
|--------|-----|----------------------|-----------|
| a2m | `frozen-for-machines-d863ad98487639445db16134d260221839cb72e9` | `9a1434c6e1c1fe4903c22924961c810497053c62` | `d863ad98487639445db16134d260221839cb72e9` |
| c64m | `frozen-for-machines-7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a` | `ba3e6bfe3a680a40c8bcad24883b63c7f830f41c` | `7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a` |

Created locally and **pushed** (2026-08-27):

```bash
git -C /Users/swessels/Develop/github/personal/a2m tag -a \
  frozen-for-machines-d863ad98487639445db16134d260221839cb72e9 \
  d863ad98487639445db16134d260221839cb72e9 \
  -m "Frozen for machines monorepo import. Feature work stops here; hotfixes land in machines first."
git -C /Users/swessels/Develop/github/personal/c64m tag -a \
  frozen-for-machines-7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a \
  7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a \
  -m "Frozen for machines monorepo import. Feature work stops here; hotfixes land in machines first."
git -C /Users/swessels/Develop/github/personal/a2m push origin \
  refs/tags/frozen-for-machines-d863ad98487639445db16134d260221839cb72e9
git -C /Users/swessels/Develop/github/personal/c64m push origin \
  refs/tags/frozen-for-machines-7f3c1abeb1abc6a5121020cb0650db10ba8e2a0a
```

Owner: README freeze blurb on the old remotes is still pending (Stage 11 retire). Tags are enough for Stage 0.

## Stage 0 exit

- Prefixes exist with history. Both `--help` run. ctest matches baseline (c64m SKIP 77 + the pre-existing `history_control_integration` fail). `agents/README.md` names `import/` as canonical until Stage 1. Freeze documented and tagged.

**Stop.** Do not `git mv` to `src/machine/{apple2,c64}` (Stage 1). Do not EXTRACT.
