# Design documents

This directory holds **design docs** for the `machines` monorepo: proposed or
in-flight architecture writeups that are not yet (or no longer) part of the
agent handoff surface.

## Why not `agents/`?

[`agents/`](../agents/) is the implementation brief for the products **as they
are**. Design drafts, alternatives, and abandoned plans clutter that handoff
and go stale relative to source. Keep speculative and program-of-work docs
here; promote lasting invariants into `agents/*.md` and the per-binary
`manual/` books when the work lands.

The agents index points here: [`agents/README.md`](../agents/README.md) → Design docs.

Canonical product-as-is notes (leftover trees after Stage 1):

- a2m: [`src/machine/apple2/agents/`](../src/machine/apple2/agents/)
- c64m: [`src/machine/c64/agents/`](../src/machine/c64/agents/)

## Status legend

| Status | Meaning |
|--------|---------|
| **active** | Current design work; last touched for an in-progress feature |
| **landed** | Implemented; doc retained for history / follow-ons |
| **abandoned** | Not pursuing; kept only if useful as negative knowledge |

## Index

| Design | Status | Last worked on | Path |
|--------|--------|----------------|------|
| Merge stage map (a2m + c64m → machines) | **active** (doc Status: **Accepted**; open questions resolved 2026-08-27) | 2026-08-27 | [`merge-stage-map.md`](merge-stage-map.md) |
| Import revisions (Stage 0 SHAs, freeze; Stage 1 rename) | **landed** | 2026-08-27 | [`import-revisions.md`](import-revisions.md) |
| Stage 2: Shared platform / util / external / nuklear | **landed** | 2026-08-27 | [`shell-extract-platform.md`](shell-extract-platform.md) |
| Stage 3: Assembler and disasm CPU class | **landed** | 2026-08-27 | [`assembler-disasm.md`](assembler-disasm.md) |
| Stage 4: Control framing | **landed** | 2026-08-27 | [`control-framing.md`](control-framing.md) |
| Stage 5: Command tables + memory sources | **landed** | 2026-08-27 | [`control-command-tables.md`](control-command-tables.md) |
| Stage 6: Runtime shell twins (history / BP / forensics / help) | **landed** | 2026-08-27 | [`runtime-shell-extract.md`](runtime-shell-extract.md) |
| Stage 7: Runtime client seam (shared subset) | **landed** | 2026-08-27 | [`runtime-client-seam.md`](runtime-client-seam.md) |
| Stage 8: Debugger UI chrome (layout / CPU / disasm / memview / BP) | **landed** | 2026-08-27 | [`debugger-chrome.md`](debugger-chrome.md) |
| Stage 9: Inspector unification (shared tab; leftover clocks) | **active** | 2026-08-27 | [`inspector-unification.md`](inspector-unification.md) |

Follow-on designs named by the stage map (not written yet):
`monorepo-agents.md`.

Landed Inspector / Forensics designs live in the leftover product trees
(`src/machine/apple2/design/`, `src/machine/c64/design/`). Do not copy them here.

## Conventions

- One topic per file; kebab-case names.
- Start with Title & Metadata (author, date, Draft/Accepted).
- Cite real source paths and APIs; prefer Mermaid for flows.
- When implementation merges, update this index (active → landed) and fold
  durable rules into the relevant `agents/` note in the same change set when
  practical.
