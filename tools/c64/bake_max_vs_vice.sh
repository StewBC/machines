#!/usr/bin/env bash
# Release bake-off: c64m max vs VICE x64sc warp.
#
# Idle BASIC plus one paint/collision-heavy PRG (default: lft-nine). History
# off, Inspector off, 1541 off. Always measure Release; Debug is not the story.
#
# Usage (from repo root):
#   ./tools/c64/bake_max_vs_vice.sh
#
# Env:
#   BUILD_DIR   Release build dir (default: build-release)
#   BUILD=0     skip cmake --build even if binaries are missing
#   CYCLES      c64m core / VICE delta cycles (default: 20000000)
#   RUNTIME_SECS  runtime free-run window in seconds (default: 3)
#                 (do not use SECONDS — that is bash's special timer)
#   PRG         heavy title (default: assets/c64/prg/lft-nine.prg)
#   X64SC       x64sc binary (auto-detected)
#   SKIP_VICE=1 skip the VICE side
#   SAMPLES     repeats per recipe before averaging (default: 2)
#
# VICE is measured the same way as the 2026-08 bake-off: x64sc -warp with
# remote-monitor stopwatch after boot (not -limitcycles; that path under-counts
# warp on this GTK3 binary). A VICE window opens for the VICE recipes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build-release}"
CYCLES="${CYCLES:-20000000}"
RUNTIME_SECS="${RUNTIME_SECS:-3}"
SAMPLES="${SAMPLES:-2}"
PRG="${PRG:-$ROOT/assets/c64/prg/lft-nine.prg}"
SKIP_VICE="${SKIP_VICE:-0}"
BUILD="${BUILD:-1}"
PAL_MHZ="0.985248"

CORE_BIN="$BUILD_DIR/profile_c64_hotloop"
RT_BIN="$BUILD_DIR/profile_runtime_hotloop"

find_x64sc() {
  if [[ -n "${X64SC:-}" && -x "$X64SC" ]]; then
    echo "$X64SC"
    return
  fi
  local candidates=(
    /Applications/vice-arm64-gtk3-3.10/bin/x64sc
    /Applications/VICE.app/Contents/MacOS/x64sc
  )
  local c
  for c in "${candidates[@]}"; do
    if [[ -x "$c" ]]; then
      echo "$c"
      return
    fi
  done
  if command -v x64sc >/dev/null 2>&1; then
    command -v x64sc
    return
  fi
  return 1
}

avg2() {
  python3 -c "print('%.3f' % ((float('$1')+float('$2'))/2.0))"
}

pct_pal() {
  python3 -c "print('%.0f' % (100.0 * float('$1') / float('$PAL_MHZ')))"
}

mhz_from_line() {
  echo "$1" | sed -n 's/.*mhz=\([0-9.]*\).*/\1/p'
}

run_core() {
  local label="$1"; shift
  local i line mhz a="" b=""
  for i in $(seq 1 "$SAMPLES"); do
    line="$("$CORE_BIN" "$CYCLES" "$@" | tail -1)"
    mhz="$(mhz_from_line "$line")"
    if [[ -z "$a" ]]; then a="$mhz"; else b="$mhz"; fi
    printf "  sample %s  %s\n" "$i" "$line"
  done
  if [[ "$SAMPLES" -lt 2 ]]; then b="$a"; fi
  local avg pct
  avg="$(avg2 "$a" "$b")"
  pct="$(pct_pal "$avg")"
  printf "RESULT  %-28s avg_mhz=%s  (~%s%% PAL)  (%s / %s)\n" \
    "$label" "$avg" "$pct" "$a" "$b"
}

run_runtime() {
  local label="$1"; shift
  local i line mhz a="" b=""
  for i in $(seq 1 "$SAMPLES"); do
    line="$("$RT_BIN" "$RUNTIME_SECS" config-off "$@" | tail -1)"
    mhz="$(mhz_from_line "$line")"
    if [[ -z "$a" ]]; then a="$mhz"; else b="$mhz"; fi
    printf "  sample %s  %s\n" "$i" "$line"
  done
  if [[ "$SAMPLES" -lt 2 ]]; then b="$a"; fi
  local avg pct
  avg="$(avg2 "$a" "$b")"
  pct="$(pct_pal "$avg")"
  printf "RESULT  %-28s avg_mhz=%s  (~%s%% PAL)  (%s / %s)\n" \
    "$label" "$avg" "$pct" "$a" "$b"
}

# One VICE warp sample via remotemonitor stopwatch. Extra args are x64sc flags
# after the common set (e.g. -autoload). Prints: mhz=... cycles=... seconds=...
vice_stopwatch_sample() {
  local warmup="$1"; shift
  python3 - "$X64SC" "$RUNTIME_SECS" "$warmup" "$@" <<'PY'
import os, socket, subprocess, sys, time

x64sc = sys.argv[1]
measure = float(sys.argv[2])
warmup = float(sys.argv[3])
extra = sys.argv[4:]
port = 17660 + (os.getpid() % 900)

cmd = [
    x64sc, "-default", "-pal", "-VICIImodel", "6569", "-VICIIfilter", "0",
    "-warp", "+sound", "-soundwarpmode", "0", "-sounddev", "dummy",
    "+drive8truedrive", "-drive8type", "0",
    "-remotemonitor", "-remotemonitoraddress", "ip4://127.0.0.1:%d" % port,
] + extra
proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def wait_port(timeout):
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout:
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=0.3)
            s.close()
            return
        except OSError as e:
            last = e
            time.sleep(0.1)
    raise SystemExit("error: x64sc remotemonitor not listening on %d (%s)" % (port, last))

def session(cmds, read_timeout=2.0):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    s.settimeout(read_timeout)
    buf = b""
    t0 = time.time()
    while time.time() - t0 < read_timeout:
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf += chunk
            if b"(C:$" in buf:
                break
        except socket.timeout:
            break
    for c in cmds:
        s.sendall((c + "\n").encode("ascii"))
        t1 = time.time()
        while time.time() - t1 < read_timeout:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break
                buf += chunk
                if b"(C:$" in buf.split(c.encode("ascii"))[-1] or b"Stopwatch" in buf:
                    # keep reading a bit for the rest of the line
                    if b"\n" in buf[buf.rfind(b"Stopwatch"):] or c == "x":
                        break
            except socket.timeout:
                break
    try:
        s.close()
    except OSError:
        pass
    return buf.decode("latin1", "replace")

def parse_cycles(text):
    for line in text.splitlines():
        if "Stopwatch:" in line and "reset" not in line.lower():
            num = "".join(ch for ch in line.split("Stopwatch:", 1)[1] if ch.isdigit())
            if num:
                return int(num)
    raise SystemExit("error: no Stopwatch cycles in monitor output: %r" % text[-300:])

try:
    wait_port(12.0)
    time.sleep(warmup)
    session(["stopwatch reset", "x"], read_timeout=2.0)
    # Wall time is the free-run window only. Reconnecting pauses VICE, so do
    # not fold the second monitor session into dt (that under-counts MHz).
    t0 = time.time()
    time.sleep(measure)
    t1 = time.time()
    text = session(["stopwatch", "x"], read_timeout=2.0)
    dt = t1 - t0
    cycles = parse_cycles(text)
    mhz = cycles / dt / 1e6 if dt > 0 else 0.0
    print("mhz=%.3f cycles=%d seconds=%.3f" % (mhz, cycles, dt))
finally:
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
PY
}

run_vice() {
  local label="$1"
  local warmup="$2"
  shift 2
  local i line mhz a="" b=""
  for i in $(seq 1 "$SAMPLES"); do
    line="$(vice_stopwatch_sample "$warmup" "$@")"
    mhz="$(mhz_from_line "$line")"
    if [[ -z "$a" ]]; then a="$mhz"; else b="$mhz"; fi
    printf "  sample %s  %s\n" "$i" "$line"
  done
  if [[ "$SAMPLES" -lt 2 ]]; then b="$a"; fi
  local avg pct
  avg="$(avg2 "$a" "$b")"
  pct="$(pct_pal "$avg")"
  printf "RESULT  %-28s avg_mhz=%s  (~%s%% PAL)  (%s / %s)\n" \
    "$label" "$avg" "$pct" "$a" "$b"
}

if [[ "$BUILD" != "0" ]]; then
  if [[ ! -x "$CORE_BIN" || ! -x "$RT_BIN" ]]; then
    echo "configuring $BUILD_DIR (Release)"
    cmake -B "$BUILD_DIR" -S "$ROOT" -DCMAKE_BUILD_TYPE=Release
  fi
  cmake --build "$BUILD_DIR" -j --target c64m_profile_c64_hotloop c64m_profile_runtime_hotloop
fi

if [[ ! -x "$CORE_BIN" || ! -x "$RT_BIN" ]]; then
  echo "missing $CORE_BIN or $RT_BIN — build Release first" >&2
  exit 1
fi

X64SC=""
if [[ "$SKIP_VICE" != "1" ]]; then
  if X64SC="$(find_x64sc)"; then
    :
  else
    echo "warning: x64sc not found (set X64SC=... or SKIP_VICE=1); skipping VICE" >&2
    SKIP_VICE=1
  fi
fi

echo "host=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -m)"
echo "build_dir=$BUILD_DIR  cycles=$CYCLES  runtime_seconds=$RUNTIME_SECS  samples=$SAMPLES"
echo "prg=$PRG"
if [[ "$SKIP_VICE" != "1" ]]; then
  echo "x64sc=$X64SC"
fi
echo "note=history off, inspector off, 1541 off; turbo=max / VICE warp"
echo

echo "== c64m Release core (profile_c64_hotloop) =="
run_core "c64m core idle paint-on"
echo

echo "== c64m Release runtime max (profile_runtime_hotloop, history off) =="
run_runtime "c64m runtime idle max"
if [[ -f "$PRG" ]]; then
  run_runtime "c64m runtime lft-nine max" "$PRG"
else
  echo "warning: missing $PRG — skip c64m heavy title" >&2
fi
echo

if [[ "$SKIP_VICE" != "1" ]]; then
  echo "== VICE x64sc warp (remotemonitor stopwatch; windowed warp loop) =="
  echo "  knobs: -warp +sound -soundwarpmode 0 +drive8truedrive -drive8type 0 -VICIImodel 6569"
  run_vice "x64sc warp idle" 2.0
  if [[ -f "$PRG" ]]; then
    run_vice "x64sc warp lft-nine" 3.0 \
      -autostartprgmode 1 -autoload "$PRG"
  else
    echo "warning: missing $PRG — skip VICE heavy title" >&2
  fi
fi
