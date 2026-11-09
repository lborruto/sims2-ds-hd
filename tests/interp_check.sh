#!/usr/bin/env bash
# Checks 60 fps's in-between frames (src/melonds_ext/GPU3D_Wide.cpp, WideAccess::Pair) on the
# walking Sim, from s7 with --wide --res3d 2 --interp and the pairing's log (SSR_PAIR_DEBUG):
#   - a model (a run of polygons with one polygon ID; the Sim's is 7) moves in one piece or not
#     at all: no in-between frame draws the Sim partly paired. Its log line says "N of N paired"
#     or the model is "left whole" (one 30 fps step);
#   - a straight walk (scripts/interp/walk-up.txt) keeps every in-between frame of the Sim;
#   - the zigzag and the turn mid-walk (scripts/interp/zigzag.txt, walk-turn.txt) keep all but a
#     few (at most 5 of their frames are 30 fps steps);
#   - at the frames where the Sim used to tear (zigzag 104, 134, 193; walk-turn 154) the Sim's
#     box is the same as with SSR_PAIR_LOOSE (which pairs every polygon it can: whole there,
#     checked by eye).
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
frames = whole = left = partly = 0
for line in open(log, errors="replace"):
    if line.startswith("[pair] ---"):
        frames += 1
    m = re.match(r"\[pair\] model \d+-\d+ \(id 7\): (\d+) of (\d+) paired", line)
    if m and m.group(1) == m.group(2):
        whole += 1
    if re.match(r"\[pair\] model \d+-\d+ \(id 7\) left whole", line):
        left += 1
print(f"{name}: {frames} in-between frames, the Sim whole in {whole}, left whole (30 fps) in {left}")
if frames < 100 or whole + left < frames - 2 or left > most:
    print(f"FAIL {name}: the Sim kept too few in-between frames"); sys.exit(1)
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
