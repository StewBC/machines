# Disk, IEC, and 1541

## Formats

Parsers in `src/tools/d64`, `t64`, `g64`, `crt`. Machine/runtime integration
in `c64.c`, `c1541.c`, `c1541_media.c`, `c64_hostfs.c`, runtime disk code.

- D64: 35-track, error tails, BAM/directory, PRG extract/write, wildcards,
  `@:` replacement.
- HostFS: host directory as volume (`c64_mount_hostfs` / `--disk N=<dir>`).
  Trap-fast `$` / LOAD PRG / SAVE create-or-file-exists; no IEC ATN.
- Devices 8 and 9 have independent ordered disk queues. Images are
  read-only by default. Writable KERNAL SAVE updates the in-memory image;
  runtime flushes to the host path. Failed flushes leave the image dirty.
- T64 is host convenience: first loadable entry. No Datasette, no T64
  KERNAL trap.
- CRT is not disk I/O. Mapper list and attach rules: `machine.md`.

Runtime owns host flushing. Machine mutates the in-memory image and marks
the slot dirty. Devices other than 8 and 9 must be rejected.

Entry points: `c64_mount_d64_ex()`, `c64_mount_g64()`, `c64_mount_hostfs()`,
`c64_set_drive_writable()`, `c64_unmount_drive()`, `c64_copy_drive_status()`,
matching `runtime_client_*`.

## Three load paths

1. **KERNAL trap** at `$FFD5`/`$FFD8` when `emulate_1541` is off or no 1541
   ROM is loaded (**or** always for `backend==HOSTFS`, before the emulate
   bail). D64 PRG/`$`, or HostFS PRG/`$`. G64 has no trap path.
2. **Real 1541 ROM + IEC** when `[disk] emulate_1541=1` and a 16 KiB DOS 2.6
   ROM is present (`[roms] 1541` or `1541.rom` next to the binary / in
   `rom` / `roms`). Drive 6502, RAM, two VIAs, IEC, fractional 1.000 MHz
   drive clock. D64 sector READ/SEARCH/WRITE jobs are intercepted at the
   DOS job layer unless media mode is on.
3. **Media GCR** when `media_1541=1` (requires `emulate_1541=1`): D64-to-GCR
   synthesis or G64 attach, rotation/SYNC/BYTE READY, motor/stepper/WPS.
   Physical READ/SEARCH/VERIFY run the ROM path. D64 WRITE is hybrid
   (sector + GCR poke). G64 WRITE is Port-A flux only.

DOS command/error channel (scratch, rename, validate, initialize, format,
status) goes through the ROM plus the FORMT intercept.

Validated fast-loader samples: Arkanoid V-MAX PAL and Robocop NTSC
load-to-game. That is not broad commercial coverage.

Mixed 8/9 coexistence (`emulate_1541=1`, 1541 ROM in both units): one slot
may be `IMAGE` (ROM/IEC path) while the other is `HOSTFS` (traps). HostFS
never becomes `iec_active` and must not enter the 1541 job path. Regression:
`tests/c64/machine/test_c64_real_1541_load.c`
(`test_hostfs_sibling_with_real_1541`).

## Soft power

Each unit has a sticky `c64_drive_slot.powered` latch (UI green LED /
`power-drive`). That latch is **not** the same as sitting on the IEC bus.

**IEC / 1541 eligibility** (`c64_drive_iec_active`):

```text
powered && backend == IMAGE && mounted
```

Step (`c64_drive_sync_to`), bus pull / ATN-ack, and reset-for-IEC use this
predicate. HostFS mounts and powered-empty units (`[8]`/`[9]` then cancel,
eject-with-power-held, `--disk N=`) keep the LED on but **do not** ATN-ack.

| Event | Effect |
|-------|--------|
| Cold start | Off: not stepped, does not pull IEC |
| First successful D64/G64 mount | `backend=IMAGE`, powers on, DOS reset if ROM loaded |
| Directory `--disk` / HostFS mount | `backend=HOSTFS`, powers on; **not** iec_active |
| Explicit power-on (UI / `power-drive` / CLI `-d N=`) | On, even without media; empty → not iec_active |
| Eject / unmount | Media cleared, `backend=NONE`; **stays powered** |
| Power-off | Ejects media, then powers off |

Loading a 1541 ROM is not power-on. An idle **iec_active** 1541 still answers
ATN by pulling DATA (`DATA = PB1 | (ATN XOR ATNA)`). Historically, a
powered-empty unit did the same and destroyed Edge of Disgrace's post-swap
streaming depacker — hence soft power and the powered-empty tightening above.
Keep unused units cold when possible.

## G64 write-back

G64 mounts are read-only by default. Writable requires `media_1541`. Live
bit ring is `halves[].data`; host blob is `slot->image_bytes`. Export is
copy-only (no phase-rotate of the live ring). Triggers: leave write gate,
seek-off-dirty, unmount, media disable. Length-preserving in-place patch;
no empty-slot grow / format rebuild.

## Lessons that stay load-bearing

- Job `$E0` (EXECUTE) jumps into the job buffer. With `media_1541=1` it
  must **not** complete as synthetic `format_track()`: that froze
  multi-stage loaders after a disk swap. Sector-intercept mode (media off)
  still maps EXECUTE to hybrid D64 track erase for FORMT tests.
- Disk swap starts a VICE-style attach blanking window
  (`C1541_MEDIA_ATTACH_DELAY`).
- Runtime `-a` autorun injects `LOAD"*",8` / `RUN` only when mounting into
  an **empty** device 8. Replacing an already-mounted image is a swap and
  must not re-arm autorun: injecting into the KERNAL keyboard buffer can
  overwrite live loader code at `$0277-$0280`.
- G64 uses the flux-transition decoder; synthetic D64 uses NRZ GCR +
  immediate BYTE READY. BYTE READY is a sticky SO edge after drive Phi2;
  **CLV discards any pending edge**. Dual-BVC loaders require that.
- While PC is in drive RAM and no job is queued, VIA2 T1 is acked so
  custom code is not stolen by `$F2B0` (Robocop). Intentional, not
  hardware-accurate.
- Full drive-object save-state is snapshot v13 `DR8C`/`DR9C` for **powered**
  units only.

When a real-1541 load fails, inspect `get-drive-cpu`, ROM-loaded and media
state, and whether the KERNAL trap ran. Do not infer success from a host
RUN log.

```sh
./build/c64m --disk 8=path/to/game.d64 --autorun
./build/c64m --disk 9=path/to/host/folder
./build/c64m --crt path/to/cart.crt
```

Machine UI: **[8]/[9]** opens **Mount Disk / HostFS** with **Open** (image) and
**Use This Folder** (HostFS). HostFS has no multi-image queue; Shift+add while
HostFS is mounted replaces it with the chosen image.
