# TimeMachine / Inspector

Shared Inspector *shape* (Record / Inspect / Land / Leave / NOW / tab chrome):
[`../shell/inspector-shape.md`](../shell/inspector-shape.md). This note is
Apple F/S pairing, sample/picture IDs, and max continuity. Do not copy C64
`film_cycle` / pink / vic-ring into this file.

Runtime-owned time travel: checkpoints + input log + sealed re-execute.
The UI is Misc → **Inspector**. HST1 is a **separate** flight recorder (FIND),
not the Inspector slider.

Files: leftover `src/runtime/runtime_inspector.*`,
`runtime_inspector_recorder.c`, `runtime_frame_ring.*`. HST1 types:
`src/shell/runtime/runtime_history.*`. UI tab: `src/shell/frontend/inspector_tab.*`.

## Two streams (do not conflate)

| Stream | What it is | Product use |
|--------|------------|-------------|
| **TimeMachine** | Checkpoint snapshots + input log + sealed re-execute | Time travel. Slider, land, `±`, F10-family. |
| **HST1** | Instruction log | FIND: “who wrote `$22` to `$2011`”. Not the slider, not F12. |

## How it works

**Record is opt-in.** `[debug] inspector=1` / `--inspector` / the Record
checkbox. Default **off** (play Total Replay with no tape cost). On -> one
navigable snapshot per finite completed beam frame or max block presentation,
an ordered input/mode-barrier log, and exact pictures in the frame ring.
Record does **not** arm or stop HST1. Snapshot budget: `inspector_memory_mb`
(default 128). Ring **slot** capacity is planned from the ][+ minimum
checkpoint (~96KiB), not the full //e ~180KiB case, so ][+ can use the memory
budget (~23s at default). //e checkpoints stay full-RAM (~12s).

Finite samples pair picture cycle `F` with the first instruction-boundary
snapshot `S >= F`. Max samples pair each approximately 60 Hz block picture
with its boundary snapshot. Stable sample/picture IDs join the two. A hidden
predecessor anchor owns the one retained resume framebuffer; ordinary samples
own only their machine blob and metadata. Reconstruct = replay sealed from the
anchor, then load the target blob and repair its framebuffer. Backward is
reconstruction from an earlier state - **not** reverse-CPU.

**Inspect** (enter): requires checkpoints. Film is optional. Starts at **live**
(the NOW blob taken on enter). Machine becomes read-only. `get-memory` /
`get-cpu` see THEN.

**Land** (slider release): select and load that exact snapshot. Far right =
restore the NOW blob when NOW is distinct. Thumb-down is **preview only**:
copy the exact paired picture by ID, or show pink if it was evicted. The Apple
does not move until release. Landing reconstructs an evicted picture and the
target's exact resume framebuffer when possible.

**`[-]` / `[+]`**: immediately adjacent catalog snapshot, including distinct
NOW. Exact Forensics/CPU focuses choose the nearest older/newer sample.

**F10 / F11 / Shift+F10 / F12 / Shift+F12**: re-execute on the landed Apple.
F12 runs until a **breakpoint** or **live**, then **stops**, still in Inspect.
No-op at live. Alt+Left is unbound. Pokes reject (`read-only-inspector`).

**Leave**: restore live NOW including its exact framebuffer and turbo policy,
still **paused**. Does not auto-resume. Unchanged Leave -> Enter reuses the
same provisional NOW endpoint.

**One breakpoint list.** Alt+B and the Breakpoints tab always edit it. Time-travel
F12 hits those breakpoints.

**CRT on stop:** any F10-family / F12 / Pause that leaves the Apple stopped
must publish a CRT frame. Override on (Hardware tab) dumps video RAM; Override
off publishes the beam buffer. Paint-off (max turbo, sealed F12) has no beam
image: dump RAM.

Window headers are dark cobalt while inspecting. Do not tint the panel fill.

Control wire: `get-state` reports `mode=live|inspector`; **`enter-inspector`**,
**`leave-inspector`**, **`land-inspector cycle=N`**, and
**`land-inspector-exact cycle=N`** exist (A2M/15). Land from live implies enter.
While Inspecting, socket `run` / `step-*` are sealed execute. Catalog `[-]`/`[+]`
stay UI / `runtime_client`. FIND stays (`history-find` / `history-next` /
`history-read`).

## Max turbo

TimeMachine remains continuous across finite/max transitions. Enter-max,
block-paint, and leave-max barriers share sequence order with input events so
sealed replay reproduces execution mode and canonical framebuffer changes.
Each max block presentation adds one sample at approximately 60 Hz wall time.

Default `history_off_on_max` (true, `[debug]`) pauses only the dense HST1 CPU
observer. `--no-history-off-on-max` keeps HST1 recording too. Optional
`inspector_off_on_max` (default **false**) wipes Inspector Record (+ film) on
enter max and restores an empty Record on leave if it was on; with the default
off, Record remains available in max and changing turbo alone does not wipe its
catalog. `--inspector` / Record still does not arm HST1.

## Media writes

A **guest** media write that succeeds **cuts the window**: records older than
the write are dropped; recording continues forward. A refused write must not
cut. Housekeeping (eject flush, snapshot flush) must not cut. HostFS directory
refresh is a different marker (`HOST_DIRECTORY`). Disk II mechanical state
rides in the checkpoint; media *bytes* live in the host file, so a replayed
read would otherwise return present-day bytes.

One interval, never islands: a cut moves `oldest` to the marker.

## Sealed replay

CPU observer off, memory-access callback off, no frame push / publish, no
`runtime_produce_audio`. Media write suppression is a safety net (the window
never spans a write). A leaky seal corrupts the tape being stood on. Mockingboard
chip state in the checkpoint is correct; host audio is muted during replay.

Join key for film vs machine is the stable **sample/picture ID**, never nearest
cycle. Frame and snapshot cycles remain separate metadata (`F` and `S`). HST1
is not lock-step with either. Missing picture IDs are honest: pink while
scrubbing, deterministic reconstruction after landing.

## Do not re-open

- A **write-delta** stream instead of snapshots. A blob is still a full
  checkpoint (~180K on //e; ~80K+ on ][+ with no aux in `RAM_`); frame cadence
  plus the hidden anchor bounds deterministic replay and reproduces beam,
  Disk II mechanics, and VIA/AY for free.
- Reverse-execution of the 6502.
- Driving the Inspector slider or time-travel F10 from **HST1**.
- Dual NOW+THEN panels.
- Always-on recording by default.
- Using live `step-instruction` to move a time-travel cursor (that mutates
  the machine).

Promote / Branch (“make this past the new live”) is not implemented — see
[`known-gaps.md`](known-gaps.md) if asked.

## Tests

`runtime_inspector`, `runtime_inspector_replay`, `runtime_inspector_mode`,
`runtime_inspector_bp`, plus history / frame-ring tests in [`testing.md`](testing.md).
