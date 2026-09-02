# Machine

`src/apple2/machine/` — Apple ][+ / //e Enhanced. No SDL. The runtime worker owns the
live `apple2_t`.

## Models

| Model | Notes |
|-------|--------|
| //e Enhanced (default) | `APPLE2_MODEL_IIE_ENHANCED`, 65C02, aux, 80-col hardware bits |
| ][+ | `APPLE2_MODEL_II_PLUS`; CLI `--model plus`; 6502 class |

Φ0 rate: `APPLE2_CPU_FREQUENCY_HZ` = **1020484.4**. CPU is `cpu65_*`
(microcycle beam path; atomic `cpu65_step` on the max path).

Always allocated: `ram_main` 128K (][+ uses the first 64K), `ram_lc` 32K
(16K LC × main/aux). Snapshots for ][+ omit the unused aux halves; //e always
stores the full banks.

## Public API

`apple2.h`: init / shutdown / warm `apple2_reset` / `apple2_cold_reset`;
`apple2_set_model`; attach Disk II / SmartPort / Mockingboard / SSC in slots
**1–7** (one Mockingboard and one SSC total); slot+device mount/eject/swap/writable;
step cycle / instruction / max-instruction; `apple2_read_in_view` /
`write_in_view`; gameport; paste; observers (`apple2_set_memory_access_callback`,
`apple2_set_cpu_observer`); `apple2_set_replay_sealed`;
`apple2_imagewriter_force_flush`.

Snapshots: `apple2_snapshot_*` — [`snapshots.md`](snapshots.md).

## Key files

| File | Role |
|------|------|
| `apple2.c` / `.h` | Core machine |
| `softswitch.c` / `.h` | `$C0xx`, banking, gameport softswitches |
| `cpu65*` | Microcycle 6502 / 65C02 |
| `video.c` / `.h` | Beam + paint — [`video.md`](video.md) |
| `diskii.c` / `image.c` | Disk II + images — [`disk.md`](disk.md) |
| `smrtprt.c` / `hostfs.c` | SmartPort + HostFS |
| `mboard.c` / `ay38910.c` / `via6522.c` | Mockingboard |
| `ssc.c` / `ssc_rom.c` / `acia6551*` | Super Serial Card + 6551 + firmware |
| `imagewriter.c` / `.h` | ImageWriter II mono raster + host pages |
| `keyboard.c` | Host key → strobe / Open-Apple |
| `memview.h` | VIEW_FLAGS debug areas |
| `rom_data.c` | Embedded ROMs |

## Slots

`EMPTY | DISKII | SMARTPORT | MOCKINGBOARD | SSC`. Defaults: Disk II in
**slot 6**, Mockingboard in **slot 4**, no SSC. Configure / `[Slots]` can put
cards in 1–7. Selecting a second Mockingboard or second SSC clears the previous
one.

**SSC / ImageWriter (v1):** presence is the SSC slot only (`slotN = ssc` /
Configure **Super Serial**). Installing an SSC always sinks ACIA TX into the
ImageWriter II mono rasterizer; there is no `[printer] enabled=` and no Misc
soft-power toggle. Host pages go to `[printer] output_dir` (default `prints`,
`bmp` only). `apple2_attach_ssc` **fails** if the target slot already holds a
different card type — Configure Apply and snapshot `SLOT` restore must detach
first, then attach. Force flush: Misc → Machine `[n]` or control
`printer-flush` (A2M/16).

## Banking / CXXX

Soft-switch flags in `softswitch.h` (`A2S_*`). Language card, 80STORE, aux,
CXXX slot map — `cxxx_map` tests are the contract.

- Empty-slot `$Cn` shadows are captured from RAM underlay **before** bank apply.
- `SETCXROM` (`$C007`): internal `$C100–$CFFF` ROM **hides** slot-card I/O
  (Mockingboard `$Cn` must not intercept while INTCXROM is on).
- `$C800` card latch: first I/O SELECT (`$Cnxx` read or write) until `$CFFF`
  (first claimant only). SmartPort claims like an expansion-ROM card; there is
  no ROM image, so the host trap *is* that mapping. SSC claims the same way and
  maps the embedded 2K firmware under `$C800` when latched. Internal `$C3xx`
  sets a motherboard 80-col overlay (does not drop the card latch). INTCXROM
  overlays `$C100–$CFFF` and restores the card latch when cleared. `$CFFF`
  drops both latches. `SETC3ROM` (`$C00B`) is not I/O SELECT; the overlay stays
  until `$CFFF` (a2audit E000B).

`apple2_debug_read` / `write` skip softswitch side effects. Live bus is
`apple2_bus_*`. Host traps (SmartPort `$C800`) are **not** 6502 opcode fetches
and will not fire as execute breakpoints.

## Gameport

Axes 0..255, clamped max **254** so PTRIG bit7 can still clear. Buttons OR with
Open/Closed Apple on `$C061` / `$C062`.

## Memory views

Three independent VIEW_FLAGS fields (`memview.h`):

| Field | Choices |
|-------|---------|
| 48K | Map / Main / Aux |
| `$C100` | Map / ROM |
| `$D000–$FFFF` | Map / LC1 / LC2 / ROM |

Product UI cycles named presets. Control/BP INI tokens: `map`, `main`, `aux`,
`c100map`, `c100rom`, `d000map`, `lc1`, `lc2`, `rom`.

## Tests

`apple2_stub`, `softswitch`, `rom_boot`, `video_beam`, `diskii`, `peripherals`,
`cxxx_map`, `memview`, `cpu65_basic`, `apple2_snapshot`, `imagewriter`,
`ssc_printshop_smoke`.
