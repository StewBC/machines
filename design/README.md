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

Canonical product-as-is notes (until Stage 1 relocates the trees):

- a2m: [`import/a2m/agents/`](../import/a2m/agents/)
- c64m: [`import/c64m/agents/`](../import/c64m/agents/)

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
| Import revisions (Stage 0 SHAs, ctest, freeze) | **landed** | 2026-08-27 | [`import-revisions.md`](import-revisions.md) |

Follow-on designs named by the stage map (not written yet):
`shell-extract-platform.md`, `assembler-disasm.md`,
`control-framing.md`, `control-command-tables.md`,
`runtime-shell-extract.md`, `runtime-client-seam.md`, `debugger-chrome.md`,
`inspector-unification.md`, `monorepo-agents.md`.

Landed Inspector / Forensics designs live in the imported product trees
(`import/a2m/design/`, `import/c64m/design/`). Do not copy them here.

## Conventions

- One topic per file; kebab-case names.
- Start with Title & Metadata (author, date, Draft/Accepted).
- Cite real source paths and APIs; prefer Mermaid for flows.
- When implementation merges, update this index (active → landed) and fold
  durable rules into the relevant `agents/` note in the same change set when
  practical.
