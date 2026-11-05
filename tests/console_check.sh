#!/usr/bin/env bash
# Checks the CONSOLE layout's pictures (src/console.*): from save states, frame 119 drawn with
# --layout console --compose must hold the DS screens' own pixels where the composition puts
# them (the same run without --compose gives the two screens), and the frames, the dimmed
# backdrop and the focus ring in the game's colours:
#   s7 (the phone, in town): the world, the rail of the touch screen's tab column and phone icon;
#   sit-cityhall (the panel): the same rail; s7 with --console-menu --console-focus 0: the touch
#   screen 3x over the dimmed world, the ring on the phone's first button; sit-gift: the same
#   without the flags (Give Item); s5 (a conversation): the dialogue rebuilt wide;
#   act-social, act-vacuum, act-cards: the touch screen 3x at the left, the world small at the
#   top right; s3 (Create-a-Sim, no HUD): SIDE.
# Needs the ROM, the save states and Pillow (not in git, so not in CI):
#   bash tests/console_check.sh
# Override with SIMS2WIDE, ROM, SHOTS, MORE_SHOTS, COMPLETE_SAVE (as tests/screens_check.sh) and
# PYTHON (a Python with Pillow).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms" -name "*.nds" 2>/dev/null | head -n 1)}
shots=${SHOTS:-$repo/shots}
more_shots=${MORE_SHOTS:-$repo/shots}
complete_save=${COMPLETE_SAVE:-$repo/saves/complete-play.sav}
python=${PYTHON:-python3}
[ -x "$bin" ] || { echo "no program at $bin (ninja -C build sims2wide)"; exit 2; }
[ -f "$rom" ] || { echo "no ROM (set ROM)"; exit 2; }
"$python" -c "import PIL" 2>/dev/null || { echo "no Pillow for $python (set PYTHON)"; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# run NAME STATE [options...]: frame 119 as the console's picture (NAME/c/f000119.png) and as
# the two screens (NAME/r/f000119.png); NAME.ok only if both runs went to their end.
run() {
    local name=$1 state=$2 file; shift 2
    file=$shots/$state.st
    [ -f "$file" ] || file=$more_shots/$state.st
    if [ ! -f "$file" ]; then echo "no save state $state.st in $shots or $more_shots" > "$tmp/$name.log"; return; fi
    local save=$tmp/$name.sav
    if [ "$state" = act-cards ]; then cp "$complete_save" "$save" || return; fi
    "$bin" "$rom" --headless --frames 120 --state-load "$file" --save "$save" --layout console \
        --compose --shot-at 119 --shot-dir "$tmp/$name/c" "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" || return
    [ "$state" = act-cards ] && cp "$complete_save" "$save"
    "$bin" "$rom" --headless --frames 120 --state-load "$file" --save "$save" \
        --shot-at 119 --shot-dir "$tmp/$name/r" "$@" > "$tmp/$name-r.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name-r.log" && touch "$tmp/$name.ok"
}

town=(--clock "2026-01-01 12:00")
hotel=(--clock "2026-01-01 14:00")
late=(--clock "2026-11-05 12:05")
run s7 s7 "${town[@]}" &
run cityhall sit-cityhall "${town[@]}" &
run s7-menu s7 "${town[@]}" --console-menu --console-focus 0 &
run gift sit-gift "${town[@]}" &
run s5 s5 "${town[@]}" &
run social act-social "${town[@]}" &
wait
run vacuum act-vacuum "${hotel[@]}" &
run cards act-cards "${late[@]}" &
run s3 s3 "${town[@]}" &
wait

fails=0
for name in s7 cityhall s7-menu gift s5 social vacuum cards s3; do
    if [ ! -f "$tmp/$name.ok" ]; then
        echo "FAIL $name: the run did not end well: $(tail -n 1 "$tmp/$name.log")"
        fails=$((fails + 1))
        continue
    fi
    if "$python" -I - "$tmp/$name/c/f000119.png" "$tmp/$name/r/f000119.png" "$name" <<'EOF'
import sys
from PIL import Image

comp_path, raw_path, case = sys.argv[1:4]
comp = Image.open(comp_path).convert("RGB")
raw = Image.open(raw_path).convert("RGB")
top = lambda x, y: raw.getpixel((x, y))
bottom = lambda x, y: raw.getpixel((x, 192 + y))
C = lambda x, y: comp.getpixel((x, y))
hexrgb = lambda h: (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))
FRAME, OUTLINE, PANEL, RING = hexrgb("828ACB"), hexrgb("383869"), hexrgb("5961A2"), hexrgb("309A41")
errors = []

def expect(what, got, want):
    if got != want:
        errors.append(f"{what}: {got} (want {want})")

def piece(what, fx, fy, fw, fh, tx, ty, scale, step=5):
    """A piece of the bottom screen drawn whole-number times bigger: each DS pixel's block."""
    for y in range(0, fh, step):
        for x in range(0, fw, step):
            expect(f"{what} DS {fx + x},{fy + y}", C(tx + x * scale + scale // 2, ty + y * scale + scale // 2), bottom(fx + x, fy + y))
            if len(errors) > 4:
                return

def world_full(rows=192):
    """The top screen 720 high, centred (cropped to the canvas when wider)."""
    w = round(720 * 256 / rows)
    fw, x0 = (256, (1280 - w) // 2) if w <= 1280 else (round(rows * 16 / 9), 0)
    rw = min(w, 1280)
    fx = (256 - fw) // 2
    return lambda cx, cy: top(fx + (cx - x0) * fw // rw, cy * rows // 720)

def rail():
    expect("rail outline", C(1170, 360), OUTLINE)
    expect("rail frame", C(1175, 360), FRAME)
    piece("rail tabs", 0, 6, 34, 184, 1178, 208, 2)
    piece("rail phone", 222, 4, 30, 30, 1182, 144, 2)
    w = world_full()
    for cx, cy in ((400, 100), (700, 650), (1000, 300)):
        expect(f"world at {cx},{cy}", C(cx, cy), w(cx, cy))

def big_over_dim():
    piece("touch screen 3x", 0, 0, 256, 192, 256, 72, 3, step=9)
    expect("touch screen 3x top-left", C(256, 72), bottom(0, 0))
    w = world_full()
    for cx, cy in ((200, 100), (1100, 650)):
        r, g, b = w(cx, cy)
        expect(f"dimmed world at {cx},{cy}", C(cx, cy), tuple(v * (255 - 0x8C) // 255 for v in (r, g, b)))

def activity():
    expect("background", C(10, 10), PANEL)
    expect("touch screen frame", C(32, 300), OUTLINE)
    expect("touch screen 3x top-left", C(36, 68), bottom(0, 0))
    piece("touch screen 3x", 0, 0, 256, 192, 36, 68, 3, step=9)
    expect("world frame", C(855, 200), OUTLINE)
    for x, y in ((0, 0), (100, 50), (383, 287), (200, 280)):
        expect(f"world small {x},{y}", C(858 + x, 68 + y), top(x * 256 // 384, y * 192 // 288))

if case in ("s7", "cityhall"):
    expect("canvas", comp.size, (1280, 720))
    rail()
elif case == "s7-menu":
    big_over_dim()
    # The ring around the phone's first button (up: x 12-79, y 140-189), 4 pixels outside it.
    expect("focus ring left", C(256 + 36 - 2, 72 + 420 + 75), RING)
    expect("focus ring top", C(256 + 36 + 100, 72 + 420 - 3), RING)
    expect("inside the ring", C(256 + 36 + 1, 72 + 420 + 75), bottom(12, 165))
elif case == "gift":
    big_over_dim()
elif case == "s5":
    expect("dialogue frame left", C(112, 600), OUTLINE)
    expect("dialogue frame top", C(640, 458), OUTLINE)
    expect("dialogue frame fill", C(118, 600), FRAME)
    expect("text box", C(700, 600), PANEL)
    piece("name", 10, 6, 144, 16, 126, 464, 2, step=3)
    piece("text", 8, 26, 152, 98, 122, 500, 2)
    piece("portrait", 164, 30, 76, 90, 1002, 494, 2)
    piece("arrow", 214, 156, 30, 30, 918, 636, 2, step=3)
    w = world_full(176)
    for cx, cy in ((400, 100), (1000, 300)):
        expect(f"world at {cx},{cy}", C(cx, cy), w(cx, cy))
elif case in ("social", "vacuum", "cards"):
    expect("canvas", comp.size, (1280, 720))
    activity()
elif case == "s3":
    expect("canvas (SIDE)", comp.size, (512, 192))
for e in errors[:5]:
    print(f"  {case}: {e}")
sys.exit(1 if errors else 0)
EOF
    then echo "ok   $name"; else echo "FAIL $name"; fails=$((fails + 1)); fi
done
if [ "$fails" != 0 ]; then echo "console_check: $fails failure(s)"; exit 1; fi
echo "console_check: ok"
