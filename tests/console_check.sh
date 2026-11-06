#!/usr/bin/env bash
# Checks the CONSOLE layout's pictures (src/console.*): from save states, frame 119 drawn with
# --layout console --compose must hold the DS screens' own pixels where the composition puts
# them (the same run without --compose gives the two screens), and the frames, the dimmed
# backdrop and the focus ring in the game's colours:
#   sit-cityhall (the panel, in town): the world, the rail of the touch screen's tab column and
#   phone icon; the same with --console-menu: the menu, 3x over the dimmed world; s7 (the phone): the touch screen 3x over the dimmed world; the same with
#   --console-focus 0: the ring on the phone's first button; sit-gift (Give Item): 3x over the
#   dimmed world; s5 (a conversation): the dialogue rebuilt wide;
#   act-social, act-vacuum, act-cards: the touch screen 3x at the left, the world small at the
#   top right, the hints' box; s2, s3, s4 (the main menu, Create-a-Sim, Keep this Sim?: no HUD):
#   the front end's view, the top screen framed at the left at its shape and the touch screen
#   2.5x framed at the right, the buttons' hint below, never SIDE; slots (sit-store, Options >
#   Save: the save slots, from RAM): the touch screen 3x, framed, over the dimmed world; load
#   (act-cards quitting, frame 119: RAM between screens, both screens black): the bare backdrop. (The pictures pin the menu and the focus with
#   --console-focus / --console-menu: the navigator does not move them.)
# Then the navigator, driven by the scripts' "pad" events (scripts/console/), from what
# --print-game (the game's screen, which CONSOLE acts on) and --print-screen (the pixels') print:
# from s7, B closes the phone, MENU opens the menu, R twice goes to Goals, B closes the menu;
# from s6, A closes the popup; from s6, a touch closes the popup and A, pressed while the next
# one opens (the game reads Popup before the box is drawn), waits and closes it; from s7,
# CONSOLE -> FOCUS -> CONSOLE with the menu open and A held: the menu closes on leaving, nothing
# is tapped on coming back, MENU opens it again. In none of them do the pixels disagree with the
# game for long ("pixels disagree").
# Then the activities on buttons (src/activities.*, scripts/console/): from act-social, RIGHT
# answers with the right icon (it lights green; the game itself plays on for about 1600
# frames whatever the answers); from act-cards, X draws (Jun's count on the score bar changes),
# X then Y draws and discards (the count back to its start), B then A quits (Cards, None,
# Panel/Interact; the game: Cards, Unknown in the black load, which CONSOLE shows as Loading, then
# Panel/Interact), B then B answers no and X still draws; from act-vacuum-clog, Y held 10
# frames clears the clog (the bowl's row 1 back to #30A2D3) within 30 frames.
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
    case $state in act-cards|sit-casino) cp "$complete_save" "$save" || return ;; esac
    "$bin" "$rom" --headless --frames 120 --state-load "$file" --save "$save" --layout console \
        --compose --shot-at 119 --shot-dir "$tmp/$name/c" --console-focus -1 "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" || return
    case $state in act-cards|sit-casino) cp "$complete_save" "$save" ;; esac
    "$bin" "$rom" --headless --frames 120 --state-load "$file" --save "$save" \
        --shot-at 119 --shot-dir "$tmp/$name/r" "$@" > "$tmp/$name-r.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name-r.log" && touch "$tmp/$name.ok"
}

town=(--clock "2026-01-01 12:00")
hotel=(--clock "2026-01-01 14:00")
late=(--clock "2026-11-05 12:05")
run s7 s7 --clock "2026-01-01 15:37" &
run cityhall sit-cityhall "${town[@]}" &
run cityhall-wide sit-cityhall "${town[@]}" --wide &
run s7-menu s7 "${town[@]}" --console-focus 0 &
run cityhall-menu sit-cityhall "${town[@]}" --console-menu &
run gift sit-gift "${town[@]}" &
run s5 s5 "${town[@]}" &
run social act-social "${town[@]}" &
wait
run vacuum act-vacuum "${hotel[@]}" &
run cards act-cards "${late[@]}" &
run s3 s3 "${town[@]}" &
wait
run s2 s2 "${town[@]}" &
run s4 s4 "${town[@]}" &
run slots sit-store "${town[@]}" --input "$repo/scripts/hotspots/options-save.txt" &
run load act-cards "${late[@]}" --input "$repo/scripts/console/cards-quit.txt" &
wait
run casino sit-casino "${late[@]}" --print-hud &
run casino-2x sit-casino "${late[@]}" --print-hud --compose-scale 2 &
wait

fails=0
for name in s7 cityhall cityhall-wide cityhall-menu s7-menu gift s5 social vacuum cards s2 s3 s4 slots load casino casino-2x; do
    if [ ! -f "$tmp/$name.ok" ]; then
        echo "FAIL $name: the run did not end well: $(tail -n 1 "$tmp/$name.log")"
        fails=$((fails + 1))
        continue
    fi
    if "$python" -I - "$tmp/$name/c/f000119.png" "$tmp/$name/r/f000119.png" "$name" "$tmp/$name.log" <<'EOF'
import re
import sys
from PIL import Image

comp_path, raw_path, case, log_path = sys.argv[1:5]
comp = Image.open(comp_path).convert("RGB")
if case.endswith("-2x"):
    # --compose-scale 2: the canvas at twice its size (pieces nearest): read it at canvas points.
    expect_size = (2560, 1440)
    if comp.size != expect_size:
        print(f"  {case}: size {comp.size}")
        sys.exit(1)
    comp = comp.resize((1280, 720), Image.NEAREST)
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
    piece("rail tabs", 0, 6, 35, 184, 1177, 208, 2, step=2)
    piece("rail phone", 222, 4, 30, 30, 1182, 144, 2)
    # The world without the game's status bar (its last 16 rows): 176 rows 720 high.
    w = world_full(176)
    for cx, cy in ((400, 100), (700, 650), (1000, 300), (900, 715)):
        expect(f"world at {cx},{cy}", C(cx, cy), w(cx, cy))

WELL = hexrgb("6A72B8")
BODY = [hexrgb(h) for h in ("EB0000", "38BA38", "DBA230", "B2B2E3")]

def hud_values():
    """The last status bar --print-hud logged: (money, hh, mm, cur, max, tier)."""
    found = None
    for line in open(log_path, encoding="utf-8", errors="replace"):
        m = re.search(r"hud money=(\d+) time=(\d+):(\d+) sanity=(\d+)/(\d+) tier=(\d+)", line)
        if m:
            found = tuple(int(v) for v in m.groups())
    return found

def status_bar(values=None, left=132):
    """The HD status bar: its panel from x `left` (the 4:3 world's left + 16), rows 660-707, found
    by its frame band (#6171B2) at both ends of row 684; inside it, and only there, white text;
    with values (from --print-hud), the meter: its body in the tier's colour, filled cur / max of
    the track (the well's inside less 2 pixels a side) within 2 pixels."""
    BAND = hexrgb("6171B2")
    x0, y0, y1 = left, 660, 708
    expect("status bar band, top", C(x0 + 18, y0 + 2), BAND)
    expect("status bar band, left", C(x0 + 2, 684), BAND)
    band = [x for x in range(x0, 1160) if C(x, 684) == BAND]
    if not band or band[0] > x0 + 3:
        errors.append("no status bar panel")
        return
    x1 = band[-1] + 2
    if x1 - x0 < 200 or C(x1 - 1, 684) != OUTLINE:
        errors.append(f"status bar panel {x0}..{x1}: no right edge")
        return
    inside = [C(x, y) for y in range(y0 + 4, y1 - 4) for x in range(x0 + 4, x1 - 4)]
    if inside.count((255, 255, 255)) < 40:
        errors.append("no status bar text")
    if values is None:
        return
    money, hh, mm, cur, mx, tier = values
    body = BODY[tier]
    rows = {}
    for y in range(y0, y1):
        xs = [x for x in range(x0, x1) if C(x, y) == body]
        if xs:
            rows[y] = xs
    if not rows:
        errors.append(f"no meter body in tier {tier}'s colour")
        return
    y = sorted(rows)[len(rows) // 2 + 1] if len(rows) > 2 else sorted(rows)[0]
    xs = rows[y]
    wells = [x for x in range(x0, x1) if C(x, y) == WELL]
    if not wells:
        errors.append("no meter well")
        return
    track_w = (max(max(wells), max(xs)) - min(wells) + 1) - 4
    filled = (max(xs) - min(xs) + 1) + 2
    want = round(track_w * cur / mx)
    if abs(filled - want) > 2:
        errors.append(f"meter filled {filled} of {track_w} (want {want}: {cur}/{mx})")

def front():
    """No HUD: the top screen fitted in 24,72 568 x 480 (4:3: 568 x 426 at 24,99), the touch
    screen 2.5x at 616,72, both framed; the hint's box below; the canvas dark elsewhere."""
    expect("canvas", comp.size, (1280, 720))
    for x in range(0, 256, 7):
        for y in range(0, 192, 7):
            expect(f"touch screen 2.5x DS {x},{y}", C(616 + x * 5 // 2 + 1, 72 + y * 5 // 2 + 1), bottom(x, y))
            if len(errors) > 4:
                return
    for cx, cy in ((24, 99), (300, 300), (591, 524), (100, 500)):
        expect(f"top at {cx},{cy}", C(cx, cy), top((cx - 24) * 256 // 568, (cy - 99) * 192 // 426))
    expect("touch screen frame", C(612, 300), OUTLINE)
    expect("top frame", C(20, 300), OUTLINE)
    expect("backdrop", C(5, 5), (16, 18, 24))
    expect("backdrop between", C(604, 300), (16, 18, 24))
    expect("hint box", C(640, 604), FRAME)
    row = [C(x, y) for y in range(614, 646) for x in range(240, 1040)]
    if (255, 255, 255) not in row or OUTLINE not in row:
        errors.append("no hint text")

def big_over_dim():
    piece("touch screen 3x", 0, 0, 256, 192, 256, 72, 3, step=9)
    expect("touch screen 3x top-left", C(256, 72), bottom(0, 0))
    # The world without the game's status bar (176 rows): row 174 at the canvas's foot.
    w = world_full(176)
    for cx, cy in ((200, 100), (1100, 650), (900, 715), (1050, 700)):
        r, g, b = w(cx, cy)
        expect(f"dimmed world at {cx},{cy}", C(cx, cy), tuple(v * (255 - 0x8C) // 255 for v in (r, g, b)))

def activity(rows=192):
    expect("background", C(10, 10), PANEL)
    expect("touch screen frame", C(32, 300), OUTLINE)
    expect("touch screen 3x top-left", C(36, 68), bottom(0, 0))
    piece("touch screen 3x", 0, 0, 256, 192, 36, 68, 3, step=9)
    expect("world frame", C(855, 200), OUTLINE)
    # The small world, 384 wide; without the game's status bar (176 rows: 264 high) where the
    # HD bar sits under it.
    h = round(384 * rows / 256)
    for x, y in ((0, 0), (100, 50), (383, h - 1), (200, h - 8)):
        expect(f"world small {x},{y}", C(858 + x, 68 + y), top(x * 256 // 384, y * rows // h))
    if rows < 192:
        region = [C(x, y) for y in range(68 + h + 14, 68 + h + 58) for x in range(852, 1248)]
        if (255, 255, 255) not in region or OUTLINE not in region:
            errors.append("no status bar text under the small world")
    # The hints' box (kHintBox, 850,400): its outline, its fill, the first button's chip and,
    # in the first row's text (x 972 on), white letters with the outline's shadow.
    expect("hints outline", C(851, 500), OUTLINE)
    expect("hints fill", C(1000, 406), FRAME)
    expect("hint chip", C(870, 430), PANEL)
    row = [C(x, y) for y in range(420, 444) for x in range(972, 1100)]
    if (255, 255, 255) not in row or OUTLINE not in row:
        errors.append("no hint text in the first row")

if case == "cityhall":
    expect("canvas", comp.size, (1280, 720))
    rail()
    status_bar()
elif case == "cityhall-wide":
    # Widescreen: the world without the game's bar keeps its sides (never cut to make room for
    # the crop): 341 x 176 fitted to the canvas's width, 1280 x 661, top-aligned; the backdrop
    # in the strip below, where the bar sits.
    big = Image.open(raw_path.replace(".png", "-top.png")).convert("RGB")
    expect("wide picture", big.size, (341, 192))
    for cx, cy in ((0, 300), (1279, 100), (640, 400), (300, 655), (1000, 660)):
        expect(f"wide world at {cx},{cy}", C(cx, cy), big.getpixel((cx * 341 // 1280, cy * 176 // 661)))
    for cx, cy in ((1000, 680), (1270, 715), (700, 700)):
        expect(f"backdrop below the world at {cx},{cy}", C(cx, cy), (16, 18, 24))
    expect("rail outline", C(1170, 360), OUTLINE)
    status_bar(left=16)
elif case == "s7":
    big_over_dim()
    status_bar()
elif case == "cityhall-menu":
    big_over_dim()
    status_bar()
elif case in ("casino", "casino-2x"):
    # The complete save (yellow tier): the casino's panel, the meter filled cur / max in yellow.
    values = hud_values()
    if values is None:
        errors.append("no hud line from --print-hud")
    else:
        expect("tier", values[5], 2)
        status_bar(values)
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
    piece("name", 10, 6, 156, 20, 126, 462, 2, step=1)
    piece("text", 8, 26, 152, 98, 122, 502, 2)
    piece("portrait", 164, 30, 76, 90, 1002, 494, 2)
    piece("arrow", 219, 155, 31, 32, 916, 634, 2, step=1)
    w = world_full(176)
    for cx, cy in ((400, 100), (1000, 300)):
        expect(f"world at {cx},{cy}", C(cx, cy), w(cx, cy))
elif case in ("social", "vacuum"):
    expect("canvas", comp.size, (1280, 720))
    activity(176)
elif case == "cards":
    # The card game has no HUD: its world whole, no status bar.
    expect("canvas", comp.size, (1280, 720))
    activity()
elif case in ("s2", "s3", "s4"):
    front()
elif case == "load":
    # Frame 119 of the cards' quit: the game between Cards and the casino's panel, both screens
    # black: only the backdrop, no frame, no hint.
    for x in range(0, 256, 4):
        for y in range(0, 192, 4):
            if max(top(x, y)) > 8 or max(bottom(x, y)) > 8:
                errors.append(f"the screens are not black at {x},{y}")
                break
        if errors:
            break
    for cy in range(0, 720, 8):
        for cx in range(0, 1280, 8):
            if C(cx, cy) != (16, 18, 24):
                errors.append(f"not the bare backdrop at {cx},{cy}: {C(cx, cy)}")
                break
        if errors:
            break
elif case == "slots":
    big_over_dim()
    expect("touch screen frame", C(252, 300), OUTLINE)
    expect("touch screen frame top", C(640, 68), OUTLINE)
for e in errors[:5]:
    print(f"  {case}: {e}")
sys.exit(1 if errors else 0)
EOF
    then echo "ok   $name"; else echo "FAIL $name"; fails=$((fails + 1)); fi
done
# pad NAME STATE SCRIPT FRAMES: a CONSOLE run with --print-screen (NAME.log; NAME.ok if it ended).
pad() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game "${town[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
# in_order NAME LINE...: each line (a grep -E pattern) is printed, after the one before.
in_order() {
    local name=$1 from=0 n; shift
    for want in "$@"; do
        n=$(tail -n +$((from + 1)) "$tmp/$name.log" | grep -n -m 1 -E "$want" | cut -d: -f1)
        [ -n "$n" ] || { echo "  $name: no \"$want\" after line $from"; return 1; }
        from=$((from + n))
    done
}
pad pad-tabs s7 tabs 320 &
pad pad-popup s6 popup 120 &
pad pad-opening s6 popup-opening 200 &
pad pad-leave s7 leave 300 &
wait
check_pad() {
    local name=$1; shift
    if [ ! -f "$tmp/$name.ok" ]; then
        echo "FAIL $name: the run did not end well: $(tail -n 1 "$tmp/$name.log")"
        fails=$((fails + 1))
    elif grep -q 'pixels disagree' "$tmp/$name.log"; then
        echo "FAIL $name: $(grep -m 1 'pixels disagree' "$tmp/$name.log")"
        fails=$((fails + 1))
    elif "$@"; then echo "ok   $name"
    else echo "FAIL $name"; fails=$((fails + 1)); fi
}
tabs() {
    in_order pad-tabs 'screen Phone' 'screen Panel/Interact' 'console menu open' 'screen Panel/Pockets' \
        'screen Panel/Goals' 'console menu closed' || return 1
    in_order pad-tabs 'game Phone' 'game Panel/Interact' 'console menu open' 'game Panel/Pockets' \
        'game Panel/Goals' 'console menu closed'
}
popup() { in_order pad-popup 'screen Popup' 'screen Panel/' && in_order pad-popup 'game Popup' 'game Panel/'; }
# The second popup closed too: A pressed while it opened waited for its box.
opening() {
    in_order pad-opening 'screen Popup' 'screen Panel/' 'screen Popup' 'screen Panel/' &&
        in_order pad-opening 'game Popup' 'game Panel/' 'game Popup' 'game Panel/'
}
leave() {
    in_order pad-leave 'console menu open' 'layout focus' 'console menu closed, focus -1' 'layout console' \
        'console menu open' || return 1
    # Nothing tapped from leaving on: the screen stays the panel's Interact tab.
    if sed -n '/layout focus/,$p' "$tmp/pad-leave.log" | grep -q -E 'screen |game '; then
        echo "  pad-leave: the screen changed after leaving CONSOLE"; return 1
    fi
}
check_pad pad-tabs tabs
check_pad pad-popup popup
check_pad pad-opening opening
check_pad pad-leave leave

# The activities on buttons (src/activities.*): act NAME STATE SCRIPT FRAMES SHOTS CLOCK, a
# CONSOLE run with --print-screen and shots of the two screens at SHOTS (NAME/f*.png).
act() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 at=$5 clock=$6 file save=$tmp/$1.sav
    file=$shots/$state.st
    [ -f "$file" ] || file=$more_shots/$state.st
    case $state in act-cards|sit-casino) cp "$complete_save" "$save" || return ;; esac
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" --save "$save" --layout console \
        --print-screen --print-game --clock "$clock" --input "$input" --shot-at "$at" --shot-dir "$tmp/$name" \
        > "$tmp/$name.log" 2>&1 && grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
act act-social act-social social 50 34,36,38,40,42,44 "2026-01-01 12:00" &
act act-draw act-cards cards-draw 200 10,199 "2026-11-05 12:05" &
act act-discard act-cards cards-discard 300 10,90,299 "2026-11-05 12:05" &
act act-quit act-cards cards-quit 300 10 "2026-11-05 12:05" &
wait
act act-cancel act-cards cards-cancel 300 10,299 "2026-11-05 12:05" &
act act-blow act-vacuum-clog vacuum-blow 60 9,40 "2026-01-01 14:00" &
wait
# pixels NAME CASE: the Python checks below on NAME's shots.
pixels() {
    "$python" -I - "$tmp/$1" "$2" <<'EOF'
import sys
from PIL import Image

d, case = sys.argv[1:3]
shot = lambda f: Image.open(f"{d}/f{f:06d}.png").convert("RGB")
errors = []

def green(im, x, y, w, h):
    """Pixels of the lit answer's green in a bottom-screen area."""
    return sum(1 for yy in range(y, y + h) for xx in range(x, x + w)
               if (lambda p: p[1] > p[0] + 40 and p[1] > p[2] + 40)(im.getpixel((xx, 192 + yy))))

def count(im):
    """Jun's card count on the score bar (top screen, x 82-87, rows 2-13), as pixels."""
    return [im.getpixel((x, y)) for y in range(2, 14) for x in range(82, 88)]

if case == "social":
    # The tapped icon lights green for a few frames (about 4 to 12 after the touch); the
    # others stay unlit.
    shots = [shot(f) for f in (34, 36, 38, 40, 42, 44)]
    if max(green(im, 186, 101, 45, 45) for im in shots) < 200:
        errors.append("the right icon never lit")
    if any(green(im, 26, 101, 45, 45) or green(im, 106, 55, 45, 45) for im in shots):
        errors.append("another icon lit")
elif case == "draw":
    if count(shot(10)) == count(shot(199)):
        errors.append("the card count did not change (no draw)")
elif case == "discard":
    if count(shot(10)) == count(shot(90)):
        errors.append("the card count did not change (no draw)")
    if count(shot(10)) != count(shot(299)):
        errors.append("the card count is not back to its start (no discard)")
elif case == "cancel":
    if count(shot(10)) == count(shot(299)):
        errors.append("no draw after answering no")
elif case == "blow":
    clean = lambda im: all(im.getpixel((x, 193)) == (0x30, 0xA2, 0xD3) for x in range(4, 45))
    if clean(shot(9)):
        errors.append("not clogged before the blow")
    if not clean(shot(40)):
        errors.append("still clogged 30 frames after the blow began")
for e in errors:
    print(f"  {case}: {e}")
sys.exit(1 if errors else 0)
EOF
}
check_pad act-social pixels act-social social
check_pad act-draw pixels act-draw draw
check_pad act-discard pixels act-discard discard
quit() {
    in_order act-quit 'screen Cards' 'screen None' 'screen Panel/Interact' &&
        in_order act-quit 'game Cards' 'game Unknown' 'game Panel/Interact'
}
check_pad act-quit quit
cancelled() {
    pixels act-cancel cancel || return 1
    if grep -q 'screen None' "$tmp/act-cancel.log"; then echo "  act-cancel: the game quit"; return 1; fi
}
check_pad act-cancel cancelled
check_pad act-blow pixels act-blow blow
if [ "$fails" != 0 ]; then echo "console_check: $fails failure(s)"; exit 1; fi
echo "console_check: ok"
