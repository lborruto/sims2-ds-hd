#!/usr/bin/env bash
# Checks 60 fps's in-between frames (src/melonds_ext/GPU3D_Wide.cpp, WideAccess::Pair) on the
# walking Sim, from s7 with --wide --res3d 2 --interp and the pairing's log (SSR_PAIR_DEBUG).
# A model (a run of polygons with one non-scenery polygon ID; the Sim's is 7) is paired as a
# unit: "whole", or "left whole" (drawn at its new pose: one 30 fps step), never partly.
# The guards that hold whatever the build (steps()):
#   - a straight walk (scripts/interp/walk-up.txt) keeps every in-between frame of the Sim;
#   - the zigzag and the turn mid-walk (scripts/interp/zigzag.txt, walk-turn.txt) keep all but
#     5 at most;
#   - no other model is left whole more than 5 times in a run (other Sims: nothing judders
#     beside the Sim; the scenery is never paired as a unit, so it never steps), the shadow
#     volumes (id 256 in the log, translucent) not more than 8 (their order changes when two
#     shadows cross).
# Pinned pictures (same()): at the 4 frames where the Sim used to tear (zigzag 104, 134, 193;
# walk-turn 154) the Sim's box must be the same as with SSR_PAIR_LOOSE (which pairs every
# polygon it can; whole there, checked by eye). A change to the pairing may move these: look at
# the frames before changing the check.
# Needs the ROM, shots/s7.st and Pillow (not in CI):
#   bash tests/interp_check.sh
# Override with SIMS2WIDE (the program), ROM, SHOTS (the save states' folder), PYTHON and
# MAX_SIMS (the emulators run at once, 4).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms/" -name "*.nds" 2>/dev/null | head -n 1)}
shots=${SHOTS:-$repo/shots}
python=${PYTHON:-python3}
[ -x "$bin" ] || { echo "no program at $bin (ninja -C build sims2wide)"; exit 2; }
[ -f "$rom" ] || { echo "no ROM (set ROM)"; exit 2; }
[ -f "$shots/s7.st" ] || { echo "no save state s7.st in $shots (set SHOTS)"; exit 2; }
"$python" -c "import PIL" 2>/dev/null || { echo "no Pillow for $python (set PYTHON)"; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
max_sims=${MAX_SIMS:-4}
[ "$max_sims" -ge 1 ] 2>/dev/null || max_sims=1

# run NAME SCRIPT FRAMES SHOTS [env...]: the log in NAME.log, the top screens in NAME/, NAME.ok
# only if the run went to its end.
run() {
    local name=$1 script=$2 frames=$3 at=$4; shift 4
    env "$@" "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/s7.st" \
        --clock "2026-01-01 12:00" --save "$tmp/$name.sav" --input "$repo/scripts/interp/$script.txt" \
        --wide --res3d 2 --interp --shot-at "$at" --shot-dir "$tmp/$name" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
jobs_wait() { while [ "$(jobs -rp | wc -l)" -ge "$max_sims" ]; do sleep 0.2; done; }

run zigzag zigzag 341 104,134,193 SSR_PAIR_DEBUG=1 & jobs_wait
run walk-turn walk-turn 241 154 SSR_PAIR_DEBUG=1 & jobs_wait
run walk-up walk-up 341 340 SSR_PAIR_DEBUG=1 & jobs_wait
run zigzag-loose zigzag 341 104,134,193 SSR_PAIR_LOOSE=1 & jobs_wait
run walk-turn-loose walk-turn 241 154 SSR_PAIR_LOOSE=1 & jobs_wait
wait

fail=0
for name in zigzag walk-turn walk-up zigzag-loose walk-turn-loose; do
    [ -f "$tmp/$name.ok" ] || { echo "FAIL $name: the run did not end well: $(tail -n 1 "$tmp/$name.log")"; fail=1; }
done

# steps NAME MAX: the Sim's model in every in-between frame: whole, or left whole at most MAX times.
steps() {
    "$python" -I - "$tmp/$1.log" "$1" "$2" <<'EOF'
import re, sys
log, name, most = sys.argv[1], sys.argv[2], int(sys.argv[3])
frames = whole = left = 0
others = {}
for line in open(log, errors="replace"):
    if line.startswith("[pair] ---"):
        frames += 1
    m = re.match(r"\[pair\] model \d+-\d+ \(id (\d+)\): (.*)", line)
    if not m:
        continue
    if m.group(1) == "7":
        whole += m.group(2).startswith("whole")
        left += m.group(2) == "left whole"
    elif m.group(2) == "left whole":
        others[m.group(1)] = others.get(m.group(1), 0) + 1
print(f"{name}: {frames} in-between frames, the Sim whole in {whole}, left whole (30 fps) in {left}; others left whole: {others or 'none'}")
if frames < 100 or whole + left < frames - 2 or left > most:
    print(f"FAIL {name}: the Sim kept too few in-between frames"); sys.exit(1)
if any(n > (8 if i == "256" else 5) for i, n in others.items()):
    print(f"FAIL {name}: another model was left whole more than 5 times"); sys.exit(1)
EOF
}
[ -f "$tmp/zigzag.ok" ] && { steps zigzag 5 || fail=1; }
[ -f "$tmp/walk-turn.ok" ] && { steps walk-turn 5 || fail=1; }
[ -f "$tmp/walk-up.ok" ] && { steps walk-up 0 || fail=1; }

# same NAME FRAME: the Sim's box at FRAME as with SSR_PAIR_LOOSE.
same() {
    "$python" -I - "$tmp/$1/f$(printf %06d "$2")-top.png" "$tmp/$1-loose/f$(printf %06d "$2")-top.png" "$1 $2" <<'EOF'
import sys
from PIL import Image, ImageChops
a, b = (Image.open(p).convert("RGB") for p in sys.argv[1:3])
box = (280, 90, 430, 350)  # the Sim, on the 683 x 384 top screen
if a.size != (683, 384) or ImageChops.difference(a.crop(box), b.crop(box)).getbbox():
    print(f"FAIL {sys.argv[3]}: the Sim is not drawn whole"); sys.exit(1)
EOF
}
if [ -f "$tmp/zigzag.ok" ] && [ -f "$tmp/zigzag-loose.ok" ]; then
    for f in 104 134 193; do same zigzag $f || fail=1; done
fi
if [ -f "$tmp/walk-turn.ok" ] && [ -f "$tmp/walk-turn-loose.ok" ]; then same walk-turn 154 || fail=1; fi

[ $fail = 0 ] && echo "interp_check: OK" || echo "interp_check: FAILED"
exit $fail
