# Validation record

This record separates executed local checks from CI targets and manual acceptance.
Dates use America/Vancouver. The scoped implementation is independently reviewed
and approved. Local checks below are complete; hosted CI and current desktop
chat activation remain separate acceptance items.

## Compatibility spike — 2026-10-12

Host: macOS arm64. Python: 3.12.14 (bundled local runtime). Official MCP SDK:
1.30.0, exact dependencies in `requirements.lock`. Installed Codex client:
`codex-cli 0.162.0-alpha.17.2`.

A temporary official-SDK stdio fixture was tested with both the SDK client and
an actual ephemeral Codex CLI invocation. SDK initialization negotiated
`2025-11-25`. Codex discovered and called a read-only tool; the result contained
structured JSON and a 16x16 PNG. Codex consumed the structured values and reported
the image color. This validates the basic client path, not emulator handlers.

The first CLI call was rejected because a new tool required approval while the
unattended client's approval policy was `never`. Repeating with an accurately
read-only tool annotation and `default_tools_approval_mode="writes"` succeeded.
Tool rejection did not execute the fixture. Mutating adapter tools need suitable
interactive or configured approval handling; annotations must remain accurate.

## Adapter checks — final 2026-10-12

- Independent implementation review: APPROVED, zero unresolved blockers.
- Final consolidated local command: `MACHINES_MCP_LIVE=1 mcp/.venv/bin/python -m pytest mcp/tests -q` — **54 passed in 5.12s**.
- Protocol gate: exact **59 Apple and 80 C64 named verbs**, capabilities, memory sources, query/action vocabularies and reviewed semantic source-region signatures verified.
- Package installed at `mcp/.venv`; `pip check` reports no broken requirements.
- Transport/cancellation tests include fragmented binary, interleaved events, malformed reply bounds, lost connection, deadlines, retained ownership and dispatched/queued cancellation. Broken-pipe errors report unavailable with possible effects.
- Live workflows exercise both built emulator executables with deterministic ROM fixtures, including guarded breakpoint hits, explicit paused barriers, assembly/memory/export, hardware/input, snapshots, media/binary/program loading, bounded history, recorded frames, Inspector preservation and surviving detached processes.
- Real-ROM Codex CLI run discovered **50 tools**, launched both profiles, obtained typed CPU/memory, PNG images, history and frame-ring status, and stopped owned children. Codex described Apple’s black 560x192 frame and C64’s blue 520x263 BASIC screen.
- A fuller actual Codex CLI run verified explicit pause → wait → fresh state for both, assembly bytes `A9 09 8D 00 C0 4C 00 80`, guarded breakpoint creation, instruction stepping, screenshots, decoded HST1 queries, and child cleanup. The step used the existing PC, so guard-hit evidence comes from deterministic live integration rather than that CLI run.
- One generic `resources/list` discovery attempt returned method-not-found; this tools-only server does not advertise MCP resources. No requested adapter workflow operation failed.
- Maximum tested native-frame conversion with random C64 PAL pixels: 28.42ms on this host, below the design's 50ms worker-offload threshold.

Review corrected native hardware numeric bases, tool discovery descriptions, the
overall startup deadline and its diagnostics, semantic drift checks, CI source
triggers, and final transport write-error classification. Normal/full tests were
rerun after corrections; earlier failures are not counted as passes.

## Native boundaries discovered

Apple headless symbol lookup may return native `not-ready` after successful
assembly because symbol cache publication occurs in the frontend loop. The
adapter preserves the rejection. C64 assembly `auto_run` with `reset=false`
assigns PC while retaining CPU microcycle phase; an arbitrary mid-instruction
pause needs an explicit instruction-boundary workflow before that native mode.
The integration fixture takes that explicit step; the adapter does not secretly
step or pause on behalf of a caller. Default reset semantics and their effects
remain visible in tool descriptions.

## Installed local setup

The stable executable is `mcp/.venv/bin/machines-mcp`. An ignored
`mcp/local.config.toml` contains this checkout's launch profiles (ports 6510 and
6511), a repository read root, and `mcp/artifacts` as the admitted output root.
User-wide Codex MCP configuration registers the server as `machines`, with a
60s tool timeout and the documented `writes` policy. Configuration did not launch
or attach an emulator. Refresh the desktop MCP connection to expose it in a new
or refreshed chat; add other project directories deliberately to the file roots.

## Platform and desktop acceptance

CI targets Python 3.11 on macOS, Linux, and Windows. Defining that matrix is not
proof that hosted CI has passed; no remote workflow run is claimed here.

An SDK subprocess test and a Codex CLI run do not establish that the current
Codex desktop chat has loaded this server or that its inline screenshot display
has been manually accepted. Desktop connection refresh and visible tool/image
acceptance must be recorded separately. No automatic restart of the user's
application is part of implementation.
