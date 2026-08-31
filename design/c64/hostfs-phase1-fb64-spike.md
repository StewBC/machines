# HostFS Phase 1 spike — CBM FileBrowser / fb64 trap surface

| Field | Value |
|-------|--------|
| Status | Complete (spike gate for PR5) |
| Date | 2026-08-30 |
| Parent design | [`hostfs-sd2iec-folder-volume.md`](hostfs-sd2iec-folder-volume.md) — Phase 1 precondition / PR5 |
| Oracle | **CBM FileBrowser 1.6** / `fb64` |

---

## Oracle binary

- GitHub: `0cjs/cbm-filebrowser` → `programs/fb64` (binary identical)
- Also at `/Volumes/EXTERNAL/Temp/fb64`
- Local copy: `assets/c64/prg/fb64.prg` (**gitignored**)
- Analysis source: `CBM-FileBrowser.asm` (C64 target)

This spike satisfies the design gate: named FB oracle, KERNAL vectors for `OPEN …15,"CD…"` + directory reload, and `$` `DIR` / free-blocks confirmation.

---

## Vectors for CD + directory reload

### `OPEN 1,device,15,"CD…":CLOSE 1` (`openclose`)

| Vector | Addr | Role for FB CD |
|--------|------|----------------|
| SETNAM | `$FFBD` | Setup only — **not trapped** |
| SETLFS | `$FFBA` | Setup only — **not trapped** |
| **OPEN** | `$FFC0` | **Must trap** (HostFS SA=15 command) |
| **CLOSE** | `$FFC3` | **Must trap** (HostFS SA=15) |

FB does **not** call CHKIN / CHRIN / CHKOUT / CLALL for CD (source comment: *"error detection would be nice :)"*).

### Directory list after CD (`loadlist`)

| Vector | Addr | Role |
|--------|------|------|
| SETLFS / SETNAM / **LOAD** `"$"` | `$FFD5` | Existing HostFS LOAD trap |
| CHROUT | `$FFD2` | UI/screen only — **not disk** |

### Other vectors (not on C64 CD / `$` path)

| Vector | Addr | Notes |
|--------|------|-------|
| CHKIN | `$FFC6` | VIC autodetection (OPEN SA=0 peek PRG load address) — not C64 CD |
| CHRIN | `$FFCF` | Same VIC path — not C64 CD |
| CHKOUT | `$FFC9` | **Unused** for FB CD / `$` navigation |
| CLALL | `$FFE7` | **Unused** for FB CD / `$` navigation |

---

## CD command strings FB sends

| Action | Bytes (PETSCII) | Notes |
|--------|-----------------|-------|
| Root | `CD//` | `diskcmdroot`; openclose A=1 → namelen 4 |
| Parent | `CD:` + `$5F` (← / `_`) | `diskcmdexit`; namelen 4 |
| Enter dir | `CD:` + 16-char name | `diskcmdcd` + name; namelen = 3 + name_len |
| Quirk | `CD:` alone (len 3) | when `disknamepetlen=0` on first prev-dir path |

---

## `$` / DIR column confirmation

- FB `filetypes` in binary is uppercase: `DIR DEL SEQ PRG USR REL CBM` (bytes at offset ~2059 in `fb64`).
- `parseext` masks bit7 (`AND #$7F`) so shifted PETSCII still matches.
- Directory synthesizer must keep type column **`DIR`** / **`PRG`** (already Phase 0).
- Listing ends when parser hits a 0 before `"` → free-blocks line; Phase 0 `65535 BLOCKS FREE.` is compatible.

---

## PR5 trap subset (from this spike)

| Must implement | Addr | Purpose |
|----------------|------|---------|
| OPEN | `$FFC0` | HostFS SA=15 CD + command channel |
| CLOSE | `$FFC3` | HostFS SA=15 |

| Recommended cheap add | Addr | Purpose |
|----------------------|------|---------|
| CHKIN / CHRIN | `$FFC6` / `$FFCF` | Status string `00, OK,00,00` reads (design exit) — even though FB CD does not use them |

| Defer → PR6 | Addr / feature |
|-------------|----------------|
| CHKOUT | `$FFC9` |
| CLALL | `$FFE7` |
| SEQ file I/O | channel data path beyond CD/`$` |

Existing Phase 0 traps remain: LOAD `$FFD5`, SAVE `$FFD8`.

---

## Acceptance

- **Product exit (Phase 1):** full interactive FB enter-subdir + load PRG from nested folder (oracle = `fb64`).
- **Automated unit tests:** OPEN/CLOSE CD forms (`CD//`, `CD:←`, `CD:name`, empty `CD:` quirk) + LOAD `$` after CD; missing-dir → DOS error.
- **Asset-gated FB smoke:** optional/manual with `assets/c64/prg/fb64.prg` (gitignored).

---

## Open follow-ups for PR6

1. **SEQ** as needed by the `fb64` oracle (design Open Question / Phase 1 polish).
2. **CHKOUT** / **CLALL** if any non-CD FB path or status-write path requires them.
3. Full **DOS error subset** + status channel polish beyond the cheap `00, OK,00,00` read path.
4. Open Question **C**: `@:` overwrite and Scratch `S:` timing.
5. Open Question **A**: identity / `$` header string freeze (else keep Phase 0 provisional).
6. Manual subset freeze + `agents/c64/disk-iec1541.md` / `design/README.md` status updates.
7. Confirm whether interactive FB needs SEQ for any navigation path beyond CD + `$` + PRG LOAD.
`)