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

The agents index points here once it exists: `agents/README.md` → Design docs.

Until Stage 0 import completes, the product-as-is notes still live in the
sibling trees:

- a2m: `/Users/swessels/Develop/github/personal/a2m/agents/`
- c64m: `/Users/swessels/Develop/github/personal/c64m/agents/`

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

Follow-on designs named by the stage map (not written yet):
`import-revisions.md`, `shell-extract-platform.md`, `assembler-disasm.md`,
`control-framing.md`, `control-command-tables.md`,
`runtime-shell-extract.md`, `runtime-client-seam.md`, `debugger-chrome.md`,
`inspector-unification.md`, `monorepo-agents.md`.

Landed Inspector / Forensics designs remain in the product trees until those
trees are imported; do not copy them here until Stage 0.

## Conventions

- One topic per file; kebab-case names.
- Start with Title & Metadata (author, date, Draft/Accepted).
- Cite real source paths and APIs; prefer Mermaid for flows.
- When implementation merges, update this index (active → landed) and fold
  durable rules into the relevant `agents/` note in the same change set when
  practical.
