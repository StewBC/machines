# am65

`am65` is the assembler used by a2m, c64m, and the standalone `am65`
command-line program. This tree is the only copy (`src/shell/tools/am65/`).
The old hub remote (`am65.git`) is frozen.

The initial CPU profile is NMOS 6502. It can be selected through the library
API (`assembler_set_cpu_profile`), with `am65 --cpu`, or changed within source:

| Directive | Accepted instructions |
|---|---|
| `.6502` | Portable documented NMOS 6502 set |
| `.65c02` | Core WDC/Rockwell-compatible 65C02 additions |
| `.rockwell` | 65C02 plus RMB/SMB and BBR/BBS bit operations |
| `.wdc` | Rockwell profile plus WAI and STP |

Profiles are cumulative. Selecting a profile establishes the initial state for
each assembly; an in-source directive affects subsequent lines. This lets c64m
and Apple ][+ select 6502, Apple //e Enhanced select 65C02, and standalone users
opt into Rockwell or WDC instructions explicitly.

## Include search paths

`.include` and `.incbin` resolve relative to the directory of the **including**
file first. Fallback directories come from:

1. CLI / host seeds: `am65 -I <dir>` (repeatable; paths are cwd-relative), via
   `assembler_add_search_dir`
2. In-source `.search "dir"` (resolved relative to the file that contains it)

Both feed the same list. `-I` entries are applied first; `.search` appends after
them. Duplicates of the same resolved directory are ignored. A bare include that
misses locally is then tried under each search directory using the include
string as written (no basename strip). `.search` only affects includes that
appear **after** it. Empty or missing search directories produce a warning.

```asm
.search "../shared"
.include "local.s"     ; beside this file if present
.include "shared.s"    ; else ../shared/shared.s
```

```sh
am65 -i alt/root.asm -I shared -o out.bin
```

## Named scopes and output targets

Named scopes provide namespaces, and symbols may be referenced with `::`:

```asm
.scope game
main:
    rts
.endscope

.word game::main
```

A named scope becomes a separate output target when it has `file=`, `prg=`, or
`dest=`:

```asm
.scope game file="game.bin" dest="map"
    .org $6000
    ; ...
.endscope

.scope overlay prg="overlay.prg"
    .org $C000
    ; ...
.endscope
```

`file=` and `prg=` are mutually exclusive host-file paths. `file=` writes a raw
contiguous image; `prg=` writes a Commodore PRG (little-endian load address =
lowest address emitted for that target, then the payload). Standalone `am65`
honours both and accepts but ignores `dest=`. Pass `--prg` to apply the same PRG
header to the default `-o` output.

Emulator hosts advertise and validate their own destination names. In a2m the
attributes are orthogonal: `dest=` writes machine memory, `file=`/`prg=` write a
host file beside the source (raw vs PRG), and both together do both. A
file-only / prg-only scope does not poke memory. c64m ignores host-file
redirects and keeps writing RAM. This keeps machine banking out of the shared
assembler.

## End-anchored segments

An end-anchored segment derives its start from its assembled size so its final byte
lands at the inclusive `end=` address:

```asm
.segdef "BSS", end=$CFFF, noemit
.segment "BSS"
cursor: .res 2
buffer: .res $100
```

Both `emit` and `noemit` are supported. Layout restarts pass 1 until the start and
size stabilize, independently of auto-adjust. End-anchored segments are implicitly
locked, must be non-empty, and must fit below their requested end. `.align` is allowed
but reports non-convergence when no stable placement exists. Absolute `.org` and
`* =` are rejected inside the segment; relative `* +=` remains valid. An inclusive
end of `$FFFF` is represented internally by the exclusive location `$10000`.

## After-linked segment chains

Use `after="host"` when a segment must start at the exclusive end of a previously
defined, non-empty segment:

```asm
.segdef "TITLE", $B800
.segdef "LOADING_ART", after="TITLE", reclaimable
.segdef "TABLES", after="LOADING_ART"
```

The derived placement converges through pass-1 restarts even when auto-adjust is
disabled. A host may have one direct follower, allowing linear chains containing
both emitted and `noemit` members. Auto-adjust treats the chain as one unit: it moves
the root and recomputes all followers. Any `locked` or end-anchored member anchors
the entire chain. The host must precede its follower, may not be empty or a
`reclaim=` overlay, and the chain may not extend beyond `$FFFF`.

## Reclaimable segment overlaps

Named `reclaim="host"` segments are implicitly `noemit`, inherit and follow the
emitted host's start, and may not grow larger than that host. For storage with its
own placement, use a two-sided overlap permission instead:

```asm
.segdef "TITLE", $B800, reclaimable
.segdef "LOADING_ART", $C200, reclaimable
.segdef "BSS", end=$CFFF, noemit, overlap_reclaimable
```

The `BSS` segment may overlap any number of emitted segments marked `reclaimable`
without following their placement. It may not overlap ordinary emitted segments or
other `noemit` segments. A plain `noemit` segment may not overlap even reclaimable
contents. Auto-adjust packs emitted segments without treating an opted-in noemit
overlap as a collision; runtime lifetime ordering remains the source's responsibility.

Standalone `am65` predefines `AM65=1` and no machine symbol. Emulator hosts
predefine `AM65=0` plus their machine symbol, currently `APPLE2=1` in a2m and
`C64=1` in c64m.

Regenerate `gperf.c` after changing `gperf.gperf`:

```sh
gperf --language=ANSI-C -c --output-file=gperf.c gperf.gperf
```
