# c64m turbo/max vs VICE: further exploration (perf gap)

| Field | Value |
|-------|--------|
| Status | **landed** (mode-3 / warp turbo removed; perf exploration still open) |
| Author | bake-off session (2026-08-29) |
| Audience | future agent closing free-run throughput toward VICE |
| Source of truth | `src/c64/` (product); VICE tree optional oracle only |

## Purpose

Handoff for **deeper dives** on making c64m **max** (free-run, fully correct)
faster — ideally nearer VICE `x64sc` warp throughput — without turning max into
a wrong-picture or collision-broken mode.

Product vocabulary below is **as shipped**. Remaining sections are perf
exploration; do not promote unmeasured claims into `agents/`.

## Product vocabulary

| Mode | Meaning |
|------|---------|
| **`1` / normal** | Real-time paced; live paint; host audio as today |
| **`2` / `max`** | Free-run as fast as the host allows; **100% correct** (live paint + sprite collisions + timing-visible side effects); UI may present at PAL/NTSC field rate |

Notes:

- Numeric **`2` is `RUNTIME_TURBO_MODE_MAX`**; CSV / CLI / `set-turbo` also
  accept the token **`max`** as an alias for 2.
- Default Alt+T ladder is **`1,max`**.
- Value **`3` is hard-rejected** on CLI `--turbo`, INI `turbo_speeds`, and
  control `set-turbo` (old warp / paint-off turbo path is gone).
- Paint-off remains a useful **lab** probe (`profile_c64_hotloop … no-video`)
  and via breakpoint action `fast`; it is not a product turbo mode.
- Prefer **`max`**, not VICE's word **warp**, in UI/manual/control copy.
- A future ladder might look a2m-like (`1,2,4,max` style) for paced multipliers
  plus max; that is optional. The invariant is: **whatever is called max stays
  fully correct**.

**Anti-goal for max:** missing sprites, incomplete paint, geometric debug
frames, or collision latches that diverge from a full-paint run of the same
workload.

Optional host optimization that **is** in-scope for max: only **present** a
correct completed live frame to the UI at ~PAL 50 Hz / ~NTSC 60 Hz wall cadence
(field rate). That must not skip collision-bearing paint on emulated fields.

## Bake-off baseline (2026-08-29, Apple M2)

Workload: idle BASIC ready loop (~`$E5Cx`). Metric: emulated cycles / wall
second (MHz). PAL realtime ≈ 0.985 MHz.

| Config | ~MHz | ≈ PAL % realtime |
|--------|------|------------------|
| c64m **Debug** turbo=2 runtime (`profile_runtime_hotloop`, history off) | ~3.4–3.6 | ~350% |
| c64m **Release** turbo=2 runtime | ~14.2 | ~1440% |
| c64m **Release** pure core, paint on (`profile_c64_hotloop`) | ~16.0 | ~1620% |
| c64m **Release** pure core, paint off (`no-video`) | ~21.1 | ~2140% |
| VICE **x64sc** 3.10 warp, no true drive (remote `stopwatch`) | ~25.3 | ~2570% |

Implications already known:

1. **Always bench Release (`-O3`)** for speed claims. Debug inflated the gap
   (~4×) and made 350% look like the product story.
2. Release max is already past a casual "800–1000%" bar on this host; remaining
   work is closing toward VICE (~1.6–1.8× on this idle recipe), not escaping
   "barely faster than realtime."
3. Paint-on → paint-off in Release is only ~25% on idle BASIC. Most of the
   remaining VICE gap is **engine structure**, not "forget to throttle the UI."

Reproduce:

```bash
# c64m Release profiles (target names are prefixed in CMake)
cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j --target c64m_profile_c64_hotloop c64m_profile_runtime_hotloop
./build-release/profile_c64_hotloop 20000000
./build-release/profile_c64_hotloop 20000000 no-video
./build-release/profile_runtime_hotloop 3 config-off

# VICE (example paths from the bake-off machine; adjust locally)
# Binary: x64sc (cycle-exact C64), NOT xscpu64 (SuperCPU)
# Data:   …/VICE.app/Contents/Resources/share/vice
# Measure: -warp -remotemonitor, monitor `stopwatch reset` / `x` / reconnect / `stopwatch`
```

VICE tree used for reading mechanisms (may lag the installed 3.10 binary):
`/Users/swessels/Develop/svm/vice-emu-code` (when present).

## What a future agent should investigate

Prioritized for **gain / risk** under the **max must stay correct** rule.

### 1. Measurement hygiene (do before claiming wins)

- Keep a tiny bake script or checklist: Release c64m vs VICE `x64sc` warp,
  same host, idle BASIC **plus** one paint-heavy / collision-heavy title.
- Compare apples: history off, Inspector off, 1541 off unless the recipe is
  "with drive." Document true-drive / SID-on-warp knobs on the VICE side.
- `get-state` during free-run is cached; prefer `profile_*` tools or a known-good
  cycle sampler for throughput. Do not trust a single control `get-state` delta.
- Re-sample with macOS `sample` (or Instruments) on **Release**
  `profile_c64_hotloop` after each serious change.

### 2. Live paint hot path (largest exclusive cost with paint on)

Release `sample` top-of-stack (idle BASIC, paint on) pointed at:

- `vicii_render_live_cycle`
- `vicii_begin_cycle` / `vicii_finish_cycle`
- `vicii_hborder_flush_slot`, `vicii_border_gfx_pixel`, coord helpers
  (`vicii_vic_x_to_frame_x`, etc.)

Try / investigate:

- Hoist invariants; flatten per-cycle helper call tax.
- Specialize spans when sprites/gfx/mode bits make the generic path wasteful
  (idle border vs active graphics), **without** dropping collision sampling.
- Keep collisions accurate: today they update on the live render path
  (`pixel_output_enabled`); any "cheaper paint" must still produce the same
  `$D01E`/`$D01F` / IRQ behavior as full paint for the same inputs.
- Reject geometric snapshot / paint-off as a max strategy (old mode 3; removed).

Expected band if this lands well: roughly **tens of percent** on paint-on
free-run — not another 4×. Demo risk is high; use EoD / lft-nine / Deus Ex
Machina style oracles from `agents/c64/`.

### 3. CIA timer stepping

`cia_step_timer` / `cia_step_cycle` showed up hot in Release samples.

Investigate VICE-style **advance-by-Δ until next underflow** (or equivalent)
when the timer state allows safe skipping of idle cycles. Preserve edge-visible
IRQ and read-back behavior.

Expected band: **small–moderate**, title-dependent. Correctness risk medium.

### 4. Runtime cost in max (frame ring / film / publish flood)

Release runtime turbo=2 (~14 MHz) sat a bit under pure core (~16 MHz).

Investigate:

- In max, avoid pushing **every** emulated completed frame into frame ring /
  Inspector film / UI slot flood; present/publish on **wall field cadence**
  while still painting every emulated field for correctness.
- Confirm headless vs windowed present tax separately from core MHz.

Low correctness risk if the machine paint path stays on; policy must be
explicit in agents notes when landed.

### 5. VICE architectural steals (longer horizon)

Read (installed source tree when available):

- `vice/src/vsync.c` — warp host redraw cap (~10 fps); present thrift only.
- `vice/src/vicii/` — `raster_draw_alarm`, draw batching vs per-cycle cost shape.
- `vice/src/c64/c64cpusc.c` — cycle-exact CPU loop structure for `x64sc`.
- Alarm / event scheduling around CIA and VIC (advance to next interesting clk).

Goal: identify **transferable** batching that does **not** imply paint-off or
collision-off. This is the plausible path for much of the remaining ~1.6× idle
gap; effort and risk are high.

### 6. Optional later: paced multipliers

a2m-style finite MHz entries (`2`, `4`, …) plus `max` may return for UX parity.
Out of scope for "close the VICE gap"; do not conflate paced multipliers with
max correctness rules.

## Explicit non-goals for this exploration

- Making **max** = old turbo=3 (paint off / debug frames) — that path is gone.
- Calling max "warp" in user-facing copy.
- Claiming VICE is faster without a Release-on-Release bake on the same host.
- Treating UI frame drops as a substitute for a deliberate field-rate present
  policy (drops still pay paint + often ring/film today).

## Suggested first tickets for a future agent

1. Document and script **Release** bake-off (c64m max vs VICE `x64sc` warp) under
   `tools/c64/`; record idle + one heavy title.
2. Profile Release paint-on; land one **safe** hot-path win in
   `vicii_render_live_cycle` / hborder flush (with VIC oracle tests).
3. Spike CIA Δ-step behind tests; keep or discard based on IRQ/read-back.
4. Only then attempt larger VICE alarm/draw-structure experiments.

## Key source pointers (c64m)

| Area | Path |
|------|------|
| Turbo mode IDs | `src/c64/runtime/runtime.h` (`RUNTIME_TURBO_MODE_*`) |
| Free-run / paint policy | `src/c64/runtime/runtime_thread.c` |
| Frame publish / ring | `runtime_publish_completed_frame` in same file |
| VIC live paint / collisions | `src/c64/machine/vicii.c` (`pixel_output_enabled`, `vicii_render_live_cycle`) |
| Core step | `src/c64/machine/c64.c` (`c64_step_cycles_ex`, micro strip) |
| Benches | `tools/c64/profile_c64_hotloop.c`, `profile_runtime_hotloop.c`, `bench_core_mhz.sh` |
| Testing / perf notes | `agents/c64/testing.md` |

## Open questions

- After Release + paint hot-path + CIA Δ-step, how much of the idle gap to VICE
  remains on this host? Re-bake before larger surgery.
- Does field-rate **UI present** (with full per-field paint) move the needle on
  windowed max more than headless core MHz suggests?
- Which collision-heavy titles are the kill criteria for paint specializations?
