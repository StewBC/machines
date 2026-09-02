# Print Shop + ImageWriter manual smoke

There is **no Apple Print Shop disk** in `assets/apple2/` (only C64 Print Shop).
Automated coverage is `a2m.ssc_printshop_smoke` (SSC ACIA TX → BIM → BMP).
Use this checklist when you have a local Apple Print Shop disk.

## Setup

1. Install an SSC in a free slot (Configure → Machine, or `[Slots] slotN = ssc`).
2. Confirm printer output dir (`prints/` default, or `--printer-dir` / `[printer] output_dir`).
3. Boot DOS / ProDOS as required by your disk.

## Print Shop steps

1. Launch Print Shop; choose a greeting card (or similar single-page graphic).
2. When asked for printer type, select **Apple ImageWriter** (or ImageWriter II).
3. Set the slot to match the SSC (e.g. slot 1 → `PR#1` path / SSC in slot 1).
4. Print the card.
5. If the page stays dirty (no form-feed from the app), use Misc → Machine **Force flush**, or control `printer-flush`.
6. Open `prints/YYYYMMDD-HHMMSSXX.bmp` — expect non-blank card art (gapless 8-pin BIM bands, not striped/blank).

## Notes

- Presence is the SSC slot only (no Misc soft-power toggle).
- `--noini` helps keep slot/printer paths reproducible.
- Full user docs are deferred to the manual PR.
