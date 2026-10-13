# Machines emulator MCP adapter

`machines-mcp` is a local stdio MCP server for Codex. It translates typed tools
into the existing **A2M/16** and **C64M/10** localhost control protocols. It is
separate from the C99 emulators and is not required to build or use them.

## Install

Python 3.11 or newer is required. From the machines repository root:

```sh
python3 -m venv mcp/.venv
mcp/.venv/bin/python -m pip install -r mcp/requirements.lock
mcp/.venv/bin/python -m pip install --no-deps --no-build-isolation -e ./mcp
```

On Windows use `mcp\.venv\Scripts\python.exe` and
`mcp\.venv\Scripts\machines-mcp.exe`. The lock includes the development test
packages so the same environment can run checks. Installation needs network
access; starting the adapter does not download anything. Build the emulators
separately using the repository's normal CMake instructions.

Copy `config.example.toml` to a local configuration file and replace the
placeholder absolute paths. Configure input roots, output roots, an artifact
directory, and named emulator launch profiles. Make these directories exist.
Profiles name an executable and working directory; tool calls cannot substitute
arbitrary shell commands. The example deliberately enables Inspector with small
recording budgets. Choose startup options or an INI for your actual workload.
An existing emulator's recording configuration is never changed on attach.

## Configure Codex once

Use absolute executable and configuration paths. For example:

```sh
codex mcp add machines -- /absolute/path/to/machines/mcp/.venv/bin/machines-mcp --config /absolute/path/to/local.config.toml
```

Equivalent user-level `~/.codex/config.toml` configuration:

```toml
[mcp_servers.machines]
command = "/absolute/path/to/machines/mcp/.venv/bin/machines-mcp"
args = ["--config", "/absolute/path/to/local.config.toml"]
startup_timeout_sec = 10
tool_timeout_sec = 60
default_tools_approval_mode = "writes"
```

This example permits read-only observations under Codex's supported tool policy
and leaves mutating calls subject to approval. Attach and launch have lifecycle
effects even though attaching preserves machine state. If an unattended Codex
run reports that a tool needs approval but its approval policy is `never`, the
call was not executed: configure an appropriate supported tool policy or use an
interactive session. Do not mark mutating tools as read-only to avoid approval.
Long waits require a compatible Codex tool timeout; initially use waits of 30s
or less. Refresh/restart the client's MCP connection after configuration changes.
See the [official Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp)
for current configuration surfaces and tool policy options.

A user-wide configuration makes the adapter available across projects. You do
not need to copy a wire-protocol document into each game project. Project-local
MCP configuration is another option. The server lists tools before connection;
product-specific descriptions say which emulator supports them. Unsupported
product calls are rejected before sending commands.

## Attach to a game already running

Start the emulator with a control port enabled:

```sh
./build/c64m --control-port 6511
```

Then ask Codex, for example:

> The game has hung in c64m on port 6511. Attach and inspect its current state.
> Preserve the evidence; do not reset or resume it.

The adapter connects to `127.0.0.1` and checks identity, exact protocol version,
capabilities, and state. It does not pause, reset, run, inject keys, load files,
leave Inspector, clear history, or enable recording. Reads of a running machine
may observe different moments; attachment is not an atomic snapshot. Detaching
from an independently started emulator leaves the process and execution alone.

A port is required; automatic instance discovery is deferred. An emulator
started without a control port cannot be attached retroactively. The port admits
one control client: disconnect an old direct script before attaching. An occupied
or unresponsive port produces a diagnostic, not a guessed connection.

## Launch a new emulator

Ask Codex to launch the configured `apple2` or `c64` profile. A profile determines
executable, working directory, INI/startup arguments, default port and GUI versus
headless behavior. The adapter verifies that the listener belongs to the child
it started; a matching emulator name alone is insufficient. Linux uses `/proc`,
macOS uses `/usr/sbin/lsof`, and Windows uses the native TCP ownership API. If
ownership cannot be verified, launch fails safely; independent launch plus
explicit attach remains available.

Launch returns connection and ownership identifiers. Connected tools require
the current connection identifier to prevent accidentally acting on a replacement
session. `stop_owned` uses the ownership identifier and can stop a surviving
child even after the control connection fails. It cannot stop an attached process.
`abandon_owned` explicitly releases ownership and leaves the child running.
Detaching an owned emulator requires `keep_running=true`.

Owned logs are periodically bounded while managed; after abandonment or
`keep_on_exit`, durable logs may grow and become your responsibility.

Normal adapter shutdown terminates owned children unless `keep_on_exit=true` was
chosen, and leaves independently attached emulators running. An adapter crash
may leave a child alive; PID and log metadata help locate it. Control failure
retains ownership until stop, abandonment, or confirmed child exit. No automatic
reconnect, restart, or replay of a mutating command occurs.

## Multiple agents

Assign **one agent to own emulator interaction** during an investigation. Other
agents can analyze source, screenshots, exported memory, and traces in parallel,
and request measurements from the owner. Serializing calls prevents wire
collisions but does not prevent one agent's reset or step from invalidating
another agent's investigation.

One adapter process serves one MCP client and one emulator. It serializes wire
commands and bounds queued calls. Conflicts return `busy` rather than promising
concurrent ownership. To use two emulators concurrently, configure two named MCP
server entries with independent adapter processes and different emulator ports.
There is no shared daemon or multi-agent arbitration service.

## Semantics and results

Successful results explicitly distinguish `observed`, `accepted`, `completed`,
and `unknown`. Some native commands acknowledge queued work; an `ok` response
cannot be upgraded into proof that all work finished. Apple memory writes and
register changes are examples. Native completion events can be sticky and are
not always correlated to the request that initiated them. Explicit waits report
what was observed without claiming stronger guarantees.

Do not assume headless launch starts paused: the tested C64 binary starts running.
When a paused session is required, explicitly request pause and wait for paused.
In current Apple headless mode, symbol lookup may return native `not-ready` even
after successful assembly because the symbol cache is published by the frontend
loop. The adapter preserves that failure rather than inventing a symbol result.

CPU, memory, hardware and frame observations can differ in time. Results preserve
native timestamps where available and distinguish cached last-observed context.
Current frames may be partial; recorded frames are ring samples. Screenshots are
PNG image results with original raster dimensions. Memory reads, history queries,
event logs and decoded page continuations are bounded. `export_memory` writes a
bounded binary file; full-history export is not provided.

History cursors are session/generation bound and become stale after native
mutations. The adapter does not silently reissue a query to make a stale cursor
appear valid. Inspector is a global emulator mode: reads can describe the past,
mutations can be rejected, and leaving Inspector does not resume execution.
Recording must already have retained evidence for retrospective investigation.

Cancellation does not undo dispatched work or implicitly pause the emulator.
An outstanding response is drained before later requests; cancelling a long wait
can therefore delay further operations. Framing errors or deadlines make the
connection unavailable. Follow diagnostic effect/ retry-safety metadata rather
than blindly retrying mutations.

## Files and artifacts

Explicit paths must be absolute and lie within configured read/write roots after
symlink resolution. Output overwrites require an explicit opt-in. CR, LF and NUL
are rejected in wire text arguments, and oversized headers are rejected rather
than truncated. Spaces in native final-path fields remain supported.

These rules are **path admission checks, not an emulator sandbox**. Guest HostFS,
writable media, assembly includes and output directives may access other host
paths according to emulator configuration. Native asynchronous saves cannot
provide stronger durability or overwrite guarantees than their existing wire
contracts. Configure guest mounts and assembly sources accordingly.

Generated artifacts include size, format and SHA-256 metadata. The quota
counts files in the configured managed artifact directory; explicit exports to
other admitted directories are individually bounded but do not contribute to
that directory quota. Caller-owned exports are not silently removed to make space.
Logs and temporary data are bounded. MCP stdout is reserved for protocol traffic;
diagnostics belong on stderr and owned-process log artifacts.

## First-release scope and follow-ons

Included: lifecycle; execution/waits; CPU/memory; wire-expressible breakpoints;
assembly and supported loading; PNG frames; softswitch/VIC/CIA/drive inspection;
input/media; snapshots; symbol lookup; bounded HST1 queries; frame rings; and
wire-exposed Inspector navigation.

Deferred: C64 VIC rings and bulk debug-memory/write-age arrays; printer flushing;
disassembly; full-history export; automatic instance discovery; multi-client or
multi-emulator hosting; pipelining; HTTP/remote access; image crop/rescale/comparison;
other emulator protocol versions; self-contained packaging without Python;
automatic crash report ingestion/reproduction/recovery; general live configuration
editing; and stronger request-correlated asynchronous completion.

Some GUI capabilities lack current wire commands, including C64 register writes,
generic cartridge/G64 mounting, symbol-file import, richer breakpoint type/swap
payloads, and Inspector catalog/record UI controls. Existing startup/GUI features
remain available independently. A dead emulator cannot return vanished state.
The [approved design](../design/mcp/emulator-mcp-adapter.md) inventories every
wire verb and explains each inclusion, alias, and omission.

## Verification

```sh
mcp/.venv/bin/python mcp/tools/check_protocol_manifest.py
mcp/.venv/bin/python -m pytest mcp/tests
# After building both emulators, also run live integration:
MACHINES_MCP_LIVE=1 mcp/.venv/bin/python -m pytest mcp/tests/integration
```

The independent adapter CI matrix exercises macOS, Linux and Windows. Live
integration requires built emulators and ROMs; test reports distinguish skipped
integration prerequisites from completed verification. See `VALIDATION.md` for
actual host, SDK, client and test results, including outstanding desktop/manual
acceptance. A passing SDK client test alone is not proof of Codex compatibility.

## Maintaining protocol contracts

A protocol change must update the backend/schema, delivery outcome, coverage
inventory and tests together. Run `tools/check_protocol_manifest.py` before
claiming compatibility. Besides exact verbs/capabilities/source names and query
vocabularies, the gate verifies reviewed semantic source-region signatures in
`schemas/protocol_manifest.py`. A same-name parser or acknowledgment change must
therefore trigger review. Line endings are normalized for cross-platform checks.

When a signature changes, inspect the source diff and audit argument ranges,
serialization, reply types, numeric bases, accepted/completed classification and
side effects. Update behavioral tests and descriptions before consciously
updating the corresponding SHA-256 entry. A checksum refresh alone is not a
compatibility fix. Add support for a new wire identity explicitly; do not change
the exact version check into a permissive greater-than comparison.
