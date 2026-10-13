# Shared control framing

`src/shell/control/` is line/binary framing plus the verb-table runner.
Product verbs, deferred capacity, and leftover `control_server.c` loops stay
leftover.

Apple leftover: [`../apple2/control-tools.md`](../apple2/control-tools.md).
C64 leftover: [`../c64/control-port.md`](../c64/control-port.md).

## Source

| Path | Role |
|------|------|
| `src/shell/control/control_framing.*` | Split `<id> <verb> <rest>\n`; `ok` / `error` / `data` / `event`; binary payload; listen/accept/line helpers |
| `src/shell/control/control_command_table.*` | Verb lookup, capabilities dump, split-and-lookup |
| `src/shell/control/memory_source.*` | Named memory-source table consumed by dasm/memview |

Tests: `tests/shell/control/`. Shared framing does not mention
`GET_SOFTSWITCHES` or `RUN_TO_RASTER`.

## Product shape

- Core parse stops at `id` / `verb` / rest. No mega `control_args`.
- Leftover binaries supply `{name, capability, parse}` tables.
- `capabilities` is a **static advertisement** generated from the table.
  Unknown verbs error. No negotiate/enable.
- `hello` / `version` are parameterized: `name=a2m protocol=A2M/16` or
  `name=c64m protocol=C64M/10`. Do not invent `MACHINES/1`.
- Bind is `127.0.0.1` only, one client. `quit-client` closes the socket,
  not the process. `--headless` requires `--control-port`.
- Deferred / pipeline capacity is a **parameter**: a2m **1**, c64m **16**.
  Do not unify by shipping one `control_server.c`.
- Media is an extension, not one `mount` for Disk II and D64.

Memory is a table of named sources, not a bitmask. C64 drives 8/9 are another
bus. Leftover a2m `DRIVE8_MAP` aliases are gone; do not restore them.

Fetch each product's memory-source table and count before calling the lookup
helper. Passing `product_memory_sources(&n)` alongside `n` in the same argument
list depends on unspecified C argument evaluation order and fails with GCC.

## MCP agent access

`mcp/` exposes these existing product protocols through a separately installed
local stdio MCP adapter; it does not replace the wire or link Python into either
emulator. See [`../../mcp/README.md`](../../mcp/README.md) for setup and bounded
typed tools. One adapter owns one control connection. During parallel agent
work, designate one agent to own emulator interaction; other agents analyze
captured results. Protocol changes must update the adapter manifests, serializers,
delivery classifications and behavioral tests together.
