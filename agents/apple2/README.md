# a2m agent handoff

Implementation brief for leftover Apple silicon and leftover a2m chrome.
**Source is authoritative** — if a doc and the code disagree, fix the doc
(or the code) in the same change. These files describe the product as it is.

Monorepo index: [`../README.md`](../README.md). Shared debugger shape:
[`../shell/`](../shell/). Do not open `src/machine/c64` to decide Apple silicon.

**Paths in this folder:** bare `src/...` means leftover
`src/machine/apple2/src/...` unless the path already starts with `src/shell/`,
`manual/`, or `tests/`.

## Product

C99 **Apple ][+ / //e Enhanced** emulator (version **3.0.0**, protocol
**A2M/14**).

This tree is a descendant of V1–V2 notes in
[`src/machine/apple2/doc/a2m-v1-2/`](../../src/machine/apple2/doc/a2m-v1-2/README-v1-2.md).
It shares debugger muscle memory with leftover c64m. Treat that as ancestry,
not a second source of truth.

**am65** (`src/shell/tools/am65/`) is the shared 6502 / 65C02 assembler.

## Read order

1. [`status.md`](status.md) — what the product is now
2. [`rules.md`](rules.md) — constraints that must not be broken
3. [`testing.md`](testing.md) — build + ctest gate

Then open the component note for the area you are changing:

[`machine.md`](machine.md) · [`video.md`](video.md) · [`disk.md`](disk.md) ·
[`runtime.md`](runtime.md) · [`frontend.md`](frontend.md) ·
[`breakpoints.md`](breakpoints.md) · [`snapshots.md`](snapshots.md) ·
[`timemachine.md`](timemachine.md) · [`control-tools.md`](control-tools.md)

Parked / not-now work lives in [`known-gaps.md`](known-gaps.md). Read it only
if asked.

Shared chrome / control framing / HST1 / Inspector *tab*:
[`../shell/frontend.md`](../shell/frontend.md) ·
[`../shell/control.md`](../shell/control.md) ·
[`../shell/history.md`](../shell/history.md) ·
[`../shell/inspector-shape.md`](../shell/inspector-shape.md).
Apple F/S clocks stay in [`timemachine.md`](timemachine.md).

## Design docs (proposals, not product-as-is)

Leftover a2m designs: [`src/machine/apple2/design/`](../../src/machine/apple2/design/).
Monorepo designs: [`design/`](../../design/). Do not treat design drafts as
handoff truth — source and `agents/*.md` win when they disagree.

## Manual (users, not agents)

[`manual/a2m/manual.md`](../../manual/a2m/manual.md) is the user-facing
manual. It is also compiled into the in-emulator help overlay
(`src/shell/tools/gen_help.py` → leftover `help_content.inc` via
`src/shell/frontend/help_view.c`).

Before editing that book, read
[`manual/a2m/HELP_MARKDOWN.md`](../../manual/a2m/HELP_MARKDOWN.md)
(ASCII-only subset). **On-screen UI copy is ASCII-only too**; see
[`frontend.md`](frontend.md) and [`../shell/frontend.md`](../shell/frontend.md).
If you add or change a user-facing feature, update `manual/a2m/manual.md` in
the same change. Do not put agent notes or `agents/` links in the manual.
