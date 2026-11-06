#!/usr/bin/env bash
# Checks the CONSOLE layout's pictures (src/console.*): from save states, frame 119 drawn with
# --layout console --compose must hold the DS screens' own pixels where the composition puts
# them (the same run without --compose gives the two screens), and the frames, the dimmed
# backdrop and the focus ring in the game's colours:
#   sit-cityhall (the panel, in town): the world, the rail of the touch screen's tab column and
#   phone icon; the same with --console-menu: the HD menu (src/port/panelmenu.*: no target, no
#   row) and with --no-romdata (menu-m1) milestone 1's menu, the touch screen 3x over the dimmed
#   world; casino-menu (sit-casino, --console-menu, the focus on row 2), at 2x and --wide: the HD
#   Interact tab (the portrait the touch screen's pixels 3x, three labelled rows, the focus ring on
#   the second only, the world dimmed); s7 (the phone): the touch screen 3x over the dimmed world; the same with
#   --console-focus 0: the ring on the phone's first button; sit-gift (Give Item, empty pockets)
#   and gift-items (A-gift2-base; 1x, 2x, --wide): the HD Give Gift (src/port/gift.*: the portrait
#   3x as the touch screen's pixels, the bow the game's sprite, the focus around the selected slot);
#   pockets-menu (A-pk-cnt3-slot1, the complete save): the HD Pockets tab (src/port/pockets.*); s5 with --no-romdata (no game text: milestone 1's conversation): the dialogue
#   rebuilt wide from the game's pieces; s5-hd (the HD conversation, src/port/dialogue.*): the
#   world whole, the panel, the name plate's and the text's white pixels, the portrait as the
#   touch screen's own pixels 2x; s5-hd-wide (--wide): the world not cut at its sides;
#   act-social, act-vacuum, act-cards: the touch screen 3x at the left, the world small at the
#   top right, the hints' box; s2, s3, s4 (the main menu, Create-a-Sim, Keep this Sim?: no HUD):
#   the front end's view, the top screen framed at the left at its shape and the touch screen
#   2.5x framed at the right, the buttons' hint below, never SIDE; slots (sit-store, Options >
#   Save: the save slots, from RAM): the HD slot list (src/port/options.*, three "No Data" rows),
#   never the touch screen 3x; slots-hd (B-cas-opt-save): the current slot's number green;
#   goals-menu, rel-menu (and --wide), opt-menu: the HD Goals, Relationships and Options tabs; load
#   (act-cards quitting, frame 119: RAM between screens, both screens black): the bare backdrop. (The pictures pin the menu and the focus with
#   --console-focus / --console-menu: the navigator does not move them.)
# Then the navigator, driven by the scripts' "pad" events (scripts/console/), from what
# --print-game (the game's screen, which CONSOLE acts on) and --print-screen (the pixels') print:
# from s7, B closes the phone, MENU opens the menu, R twice goes to Goals, B closes the menu;
# from s6, A closes the popup; from s6, a touch closes the popup and A, pressed while the next
# one opens (the game reads Popup before the box is drawn), waits and closes it; from s7,
# CONSOLE -> FOCUS -> CONSOLE with the menu open and A held: the menu closes on leaving, nothing
# is tapped on coming back, MENU opens it again; the HD Interact tab: from sit-casino, MENU, Down
# and A start Hotseat (Cards); from D-cas-right2, Left and Right switch the target (--print-town);
# from sit-store, A on Buy opens the catalogue. In none of them do the pixels disagree with the
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
    case $state in act-cards|sit-casino|A-pk-cnt3-slot1|A-pk-slot1-key2|B-cas-*|C-cas-*) cp "$complete_save" "$save" || return ;; esac
    "$bin" "$rom" --headless --frames 120 --state-load "$file" --save "$save" --layout console \
        --compose --shot-at 119 --shot-dir "$tmp/$name/c" --console-focus -1 "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" || return
    case $state in act-cards|sit-casino|A-pk-cnt3-slot1|A-pk-slot1-key2|B-cas-*|C-cas-*) cp "$complete_save" "$save" ;; esac
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
run s5 s5 "${town[@]}" --no-romdata &
run s5-hd s5 "${town[@]}" --print-hud &
run s5-hd-wide s5 "${town[@]}" --wide &
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
run menu-m1 sit-cityhall "${town[@]}" --console-menu --no-romdata &
run casino-menu sit-casino "${late[@]}" --console-menu --console-focus 1 --print-hud &
run casino-menu-2x sit-casino "${late[@]}" --console-menu --console-focus 1 --print-hud --compose-scale 2 &
run casino-menu-wide sit-casino "${late[@]}" --console-menu --console-focus 0 --wide &
wait
run pockets-menu A-pk-cnt3-slot1 "${late[@]}" --console-menu --console-focus 0 &
run pockets-armed A-pk-cnt3-slot1 "${late[@]}" --console-menu --console-focus 0 --input "$repo/scripts/console/pockets-bin-once.txt" &
run gift-items A-gift2-base "${town[@]}" --console-focus 0 &
run gift-items-2x A-gift2-base "${town[@]}" --console-focus 0 --compose-scale 2 &
run gift-items-wide A-gift2-base "${town[@]}" --console-focus 0 --wide &
run goals-menu B-pg8-goals "${town[@]}" --console-menu --console-focus 0 &
run rel-menu B-cas-rel "${late[@]}" --console-menu --console-focus 0 &
run rel-menu-wide B-cas-rel "${late[@]}" --console-menu --console-focus 0 --wide &
run opt-menu B-cas-opt "${late[@]}" --console-menu --console-focus 2 &
run slots-hd B-cas-opt-save "${late[@]}" --console-focus 2 &
wait
run popup-goal s6 "${town[@]}" --console-focus 0 &
run popup-pause B-pause "${town[@]}" --console-focus 0 &
run popup-quit B-ch-opt-quit "${town[@]}" --console-focus 0 &
run popup-menu B-cas-opt "${late[@]}" --console-menu --console-focus 0 --input "$repo/scripts/console/popup-quit-no.txt" &
run phone-msg C-cas-msg1 "${late[@]}" --console-focus 0 &
run phone-contacts C-cas-pb1 "${late[@]}" --console-focus 0 &
run phone-wide C-cas-msgs "${late[@]}" --console-focus 0 --wide &
wait

fails=0
for name in s7 cityhall cityhall-wide cityhall-menu s7-menu gift s5 s5-hd s5-hd-wide social vacuum cards s2 s3 s4 slots load casino casino-2x \
    menu-m1 casino-menu casino-menu-2x casino-menu-wide pockets-menu pockets-armed gift-items gift-items-2x gift-items-wide \
    goals-menu rel-menu rel-menu-wide opt-menu slots-hd popup-goal popup-pause popup-quit popup-menu phone-msg phone-contacts phone-wide; do
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

def big_over_dim(rows=176):
    piece("touch screen 3x", 0, 0, 256, 192, 256, 72, 3, step=9)
    expect("touch screen 3x top-left", C(256, 72), bottom(0, 0))
    # The world without the game's status bar (176 rows): row 174 at the canvas's foot.
    w = world_full(rows)
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

def hd_menu(rows, focus, wide=False):
    """The panel's HD menu (src/port/panelmenu.*, interact.*): the panel's outline and frame band,
    the target's portrait as the touch screen's own pixels 3x (DS x 63-127, rows 47-127), the
    title's well, a white label on each action row, the focus ring around row `focus` only, the
    world dimmed outside the panel; never the touch screen 3x (the milestone 1 menu)."""
    expect("panel outline", C(176, 300), OUTLINE)
    expect("panel band", C(180, 300), hexrgb("6171B2"))
    expect("panel fill", C(190, 300), FRAME)
    expect("title well", C(640, 166), hexrgb("6A72B8"))
    # The tab bar: the game's own icons, Interact's (the first of the five cells from x 420, 88
    # wide) the green one, the others blue (no green pixel).
    def greens(cell):
        x0 = 420 + 88 * cell
        return sum(1 for y in range(48, 126, 2) for x in range(x0 + 8, x0 + 80, 2)
                   if (lambda p: p[1] > p[0] + 50 and p[1] > p[2] + 30)(C(x, y)))
    counts = [greens(i) for i in range(5)]
    if counts[0] < 200 or any(counts[1:]):
        errors.append(f"tab icons' green pixels {counts} (want Interact's only)")
    piece("portrait 3x", 63, 47, 65, 81, 220, 230, 3, step=4)
    for i in range(4):
        row = [C(x, y) for y in range(214 + 76 * i, 268 + 76 * i) for x in range(488, 900)]
        white = (255, 255, 255) in row
        if white != (i < rows):
            errors.append(f"row {i}: label {'missing' if i < rows else 'drawn'}")
        r, g, b = C(456, 241 + 76 * i)
        ring = g > r + 80 and g > b + 60  # the ring's green (its edge softened by the row's shadow)
        if ring != (i == focus):
            errors.append(f"row {i}: focus ring {'missing' if i == focus else 'drawn'}")
    if C(256 + 3 * 120 + 1, 72 + 3 * 100 + 1) == bottom(120, 100) and C(256 + 3 * 30 + 1, 72 + 3 * 160 + 1) == bottom(30, 160):
        errors.append("the touch screen 3x is drawn")
    if not wide:
        w = world_full(176)
        for cx, cy in ((150, 100), (1140, 400)):
            r, g, b = w(cx, cy)
            expect(f"dimmed world at {cx},{cy}", C(cx, cy), tuple(v * (255 - 0x8C) // 255 for v in (r, g, b)))

INK, LCD, BAR = hexrgb("203020"), hexrgb("9AB29A"), hexrgb("384838")

def not_big():
    if C(256 + 3 * 120 + 1, 72 + 3 * 100 + 1) == bottom(120, 100) and C(256 + 3 * 30 + 1, 72 + 3 * 160 + 1) == bottom(30, 160):
        errors.append("the touch screen 3x is drawn")

def hd_phone(y, wide=False):
    """The phone in HD (src/port/phone.*): the panel, the LCD's green, the game's highlight (a dark
    bar with the focus's green halo) across canvas row `y` (None: no focus drawn), the rows'
    letters, never the touch screen 3x."""
    expect("phone outline", C(350, 300), OUTLINE)
    expect("phone body", C(360, 300), FRAME)
    expect("LCD", C(420, 500), LCD)
    if y is not None:
        expect("highlight bar", C(410, y), BAR)
        r, g, b = C(393, y)
        if not (g > r + 80 and g > b + 60):
            errors.append(f"no focus halo at {y}")
    if INK not in [C(x, yy) for yy in range(152, 500) for x in range(500, 780)]:
        errors.append("no row letters")
    not_big()

def hd_popup(kind, title, menu):
    """A popup in HD: its outline and fill, the text's well with white letters, the title plate,
    the buttons (kind 0: Close focused; kind 2: No plain, Yes focused)."""
    expect("popup outline", C(300, 350), OUTLINE)
    expect("popup fill", C(310, 350), FRAME)
    expect("popup well", C(340, 300), WELL)
    if (255, 255, 255) not in [C(x, y) for y in range(240, 380) for x in range(360, 920)]:
        errors.append("no popup text")
    if title:
        expect("title plate", C(450, 170), hexrgb("6171B2"))
    if kind == 0:
        r, g, b = C(477, 448)
        if not (g > r + 60 and g > b + 40): errors.append("no focus ring around Close")
    else:
        expect("No", C(400, 448), WELL)
        r, g, b = C(670, 448)
        if not (g > r + 60 and g > b + 40): errors.append("no focus ring around Yes")
    if menu:
        r, g, b = C(190, 300)
        if not b > r + 30: errors.append("no HD menu beneath")
    not_big()

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
    hd_phone(None)
    status_bar()
elif case == "cityhall-menu":
    # City Hall: no target (the plumbob; the title "Interact" alone), no row, the focus on the phone.
    hd_menu(0, -1)
    status_bar()
elif case == "menu-m1":
    # Without the game's text (--no-romdata): milestone 1's menu, the touch screen 3x (and the
    # game's own status bar in the world: no HD bar without its words).
    big_over_dim(192)
elif case in ("casino-menu", "casino-menu-2x"):
    # Keelhaulin' Cards: three rows, the focus on the second (Hotseat).
    hd_menu(3, 1)
    status_bar(hud_values())
elif case == "casino-menu-wide":
    hd_menu(3, 0, wide=True)
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
    # --console-focus 0: the HD phone's first row (the game's own highlight).
    hd_phone(176)
elif case in ("phone-msg", "phone-wide", "phone-contacts"):
    # A message (its sender, its text, then Next / Delete / Exit: the highlight on Next, the
    # game's being on the sender); the messages' list; the contacts (7 rows of 23 shown).
    hd_phone(380 if case == "phone-msg" else 176, wide=case == "phone-wide")
    if case == "phone-msg":
        text = [C(x, y) for y in range(210, 400) for x in range(410, 870)]
        if INK not in text:
            errors.append("no message text")
    if case == "phone-contacts":
        bar = [C(x, y) for y in range(150, 490) for x in range(892, 898)]
        if not any(abs(p[0] - p[2]) > 30 for p in bar):
            errors.append("no scroll bar")
elif case in ("popup-goal", "popup-pause", "popup-quit", "popup-menu"):
    # The game's popup in HD (src/port/popup.*): the box, its text white in the well, its title
    # plate (Game Paused), its button(s): one focused "Close" (A / B) or No / Yes with Yes focused;
    # over the HD Options menu for popup-menu (dimmed under the veil).
    hd_popup(2 if case in ("popup-quit", "popup-menu") else 0, case == "popup-pause", case == "popup-menu")
elif case in ("gift", "gift-items", "gift-items-2x", "gift-items-wide"):
    # Give Item in HD (src/port/gift.*): the panel, the title's well, the recipient's portrait as
    # the touch screen's own pixels 3x (DS 167,47 65 x 81), the bow the game's sprite; never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("panel band", C(180, 300), hexrgb("6171B2"))
    expect("title well", C(640, 50), hexrgb("6A72B8"))
    piece("portrait 3x", 167, 47, 65, 81, 862, 195, 3, step=4)
    # The bow: the game's sprite (3631, blue on its round button) in its bar.
    bow = [C(x, y) for y in range(486, 546, 2) for x in range(470, 530, 2)]
    if sum(1 for p in bow if p[2] > p[0] + 60 and p[2] > p[1] + 40) < 20:
        errors.append("no bow in its bar")
    if C(256 + 3 * 120 + 1, 72 + 3 * 100 + 1) == bottom(120, 100) and C(256 + 3 * 30 + 1, 72 + 3 * 160 + 1) == bottom(30, 160):
        errors.append("the touch screen 3x is drawn")
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    # The focus ring (green) around the game's selected slot when the pockets hold something.
    if case == "gift":
        if ring(197, 235): errors.append("a focus ring with empty pockets")
    else:
        if not ring(197, 235): errors.append("no focus ring around slot 1")
        if ring(313, 235): errors.append("a focus ring around slot 2")
        # Slot 1's item: the game's own icon (not the cell's flat fill).
        cell = {C(x, y) for y in range(200, 290, 4) for x in range(215, 290, 4)}
        if len(cell) < 8: errors.append(f"slot 1 has no icon ({len(cell)} colours)")
elif case in ("pockets-menu", "pockets-armed"):
    # Pockets in HD (src/port/pockets.*): the Pockets tab's icon green, the focus ring around slot
    # 1 (the Arcade-Pie Factory), the counted items' icons the touch screen's pixels 2x (DS 99 +
    # 29i, 155, 23 x 23), their counts in white, the item's name in the info; never the touch
    # screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    def greens(cell):
        x0 = 420 + 88 * cell
        return sum(1 for y in range(48, 126, 2) for x in range(x0 + 8, x0 + 80, 2)
                   if (lambda p: p[1] > p[0] + 50 and p[1] > p[2] + 30)(C(x, y)))
    counts = [greens(i) for i in range(5)]
    if counts[1] < 200 or counts[0] or any(counts[2:]):
        errors.append(f"tab icons' green pixels {counts} (want Pockets' only)")
    for i in range(4):
        piece(f"counted {i} 2x", 99 + 29 * i, 155, 23, 23, 200 + 87 * i + 14, 462, 2, step=3)
    r, g, b = C(197, 258)
    if not (g > r + 80 and g > b + 60): errors.append("no focus ring around slot 1")
    if (255, 255, 255) not in [C(x, y) for y in range(540, 560) for x in range(215, 260)]:
        errors.append("no count under the first counted item")
    if (255, 255, 255) not in [C(x, y) for y in range(222, 258) for x in range(692, 900)]:
        errors.append("no name in the info")
    # The bin's button: green only once X asked (pockets-armed: one X), plain before.
    r, g, b = C(1010, 550)
    green = g > r + 40 and g > b + 20
    if green != (case == "pockets-armed"):
        errors.append(f"the bin's button {'not ' if case == 'pockets-armed' else ''}green: {(r, g, b)}")
    # (The panel's colours are the game's: a few pixels may match the touch screen 3x by chance.)
    same = sum(1 for y in range(8, 184, 16) for x in range(8, 248, 16) if C(256 + 3 * x + 1, 72 + 3 * y + 1) == bottom(x, y))
    if same > 100: errors.append(f"the touch screen 3x is drawn ({same} of 165 samples)")
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
elif case in ("s5-hd", "s5-hd-wide"):
    # The HD conversation (port/dialogue.h): the panel 1060 wide from x 110 (its frame band
    # #6171B2), the text well, white text in it, the name plate's white text above it, the
    # portrait: the touch screen's x 174-240, rows 38-121, 2x from (1006, 504) (a page of
    # four rows: the panel 254 high from y 452), nothing of the game's own box.
    BAND, TWELL = hexrgb("6171B2"), hexrgb("6A72B8")
    expect("panel band", C(112, 600), BAND)
    expect("text well", C(560, 676), TWELL)
    white = lambda x0, y0, x1, y1: sum(1 for y in range(y0, y1) for x in range(x0, x1) if min(C(x, y)) > 235)
    if white(150, 500, 960, 680) < 1000:
        errors.append("no text in the panel")
    if white(140, 430, 400, 476) < 300:
        errors.append("no name on the plate")
    grid = [(x, y) for y in range(0, 84, 3) for x in range(0, 67, 3)]
    bad = sum(1 for x, y in grid if C(1006 + 2 * x, 504 + 2 * y) != bottom(174 + x, 38 + y))
    if bad:
        errors.append(f"portrait: {bad} pixels not the touch screen's")
    if sum(1 for x, y in grid if max(C(1006 + 2 * x, 504 + 2 * y)) > 40) < len(grid) // 2:
        errors.append("portrait black")
    if case == "s5-hd":
        w = world_full(176)
        for cx, cy in ((400, 100), (1000, 300), (130, 420)):
            expect(f"world at {cx},{cy}", C(cx, cy), w(cx, cy))
        if not re.search(r"frame \d+: hd dialogue id=\d+ lines=\d+", open(log_path).read()):
            errors.append("no hd dialogue line in the log")
    else:
        # --wide: the world's whole width on the canvas (1280 wide, top-aligned, its status bar's
        # rows dropped), as the HD status bar's town: never cut at its sides.
        big = Image.open(raw_path.replace(".png", "-top.png")).convert("RGB")
        tw, th = big.size
        rows = th * 176 // 192
        h = round(1280 * rows / tw)
        for cx, cy in ((2, 100), (1277, 100), (640, 300), (40, 440)):
            expect(f"wide world at {cx},{cy}", C(cx, cy), big.getpixel((cx * tw // 1280, cy * rows // h)))
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
elif case in ("slots", "slots-hd"):
    # The Save slots in HD (src/port/options.*): the panel, three rows, never the touch screen 3x;
    # slots (sit-store, three empty): "No Data" in the light lavender in each row; slots-hd (the
    # complete save, Jun's slot the game's current one): its number green, the focus ring on it.
    expect("panel outline", C(176, 300), OUTLINE)
    same = sum(1 for y in range(8, 184, 16) for x in range(8, 248, 16) if C(256 + 3 * x + 1, 72 + 3 * y + 1) == bottom(x, y))
    if same > 100: errors.append(f"the touch screen 3x is drawn ({same} of 165 samples)")
    rows = [124 + 156 * i for i in range(3)]
    if case == "slots":
        for i, y0 in enumerate(rows):
            pale = sum(1 for y in range(y0 + 40, y0 + 100, 2) for x in range(348, 560, 2) if (lambda p: p[0] > 190 and p[2] > 230)(C(x, y)))
            if pale < 40: errors.append(f"row {i + 1}: no \"No Data\" ({pale})")
    else:
        green = lambda y0: sum(1 for y in range(y0 + 30, y0 + 112, 2) for x in range(230, 310, 2) if (lambda p: p[1] > p[0] + 60 and p[1] > p[2] + 60)(C(x, y)))
        if green(rows[2]) < 40 or green(rows[0]) or green(rows[1]): errors.append(f"current slot's green number {[green(r) for r in rows]}")
        r, g, b = C(197, rows[2] + 70)
        if not (g > r + 80 and g > b + 60): errors.append("no focus ring around slot 3")
        if (255, 255, 255) not in [C(x, y) for y in range(rows[2] + 30, rows[2] + 70) for x in range(348, 460)]:
            errors.append("no name in slot 3")
elif case == "goals-menu":
    # Goals in HD (src/port/goals.*): the Goals tab's icon green, the giver's portrait the touch
    # screen's pixels 3x (DS 47,88 30 x 30 at 219,225), the focus ring on the first goal (the
    # game's selection), white text in the list; never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    piece("giver", 47, 88, 30, 30, 219, 225, 3, step=3)
    r, g, b = C(355, 240)
    if not (g > r + 80 and g > b + 60): errors.append("no focus ring around the first goal")
    if sum(1 for y in range(220, 400, 2) for x in range(390, 1000, 2) if min(C(x, y)) > 235) < 300:
        errors.append("no goal text")
    same = sum(1 for y in range(8, 184, 16) for x in range(8, 248, 16) if C(256 + 3 * x + 1, 72 + 3 * y + 1) == bottom(x, y))
    if same > 100: errors.append(f"the touch screen 3x is drawn ({same} of 165 samples)")
elif case in ("rel-menu", "rel-menu-wide"):
    # Relationships in HD (src/port/relations.*): the big portrait the touch screen's pixels 3x
    # (DS 84,37 69 x 84 at 474,222), the first row's portrait 2x (DS 43,28 28 x 28 at 214,220),
    # the focus ring on the first row (the game's selection), the mood in white.
    expect("panel outline", C(176, 300), OUTLINE)
    piece("big portrait", 84, 37, 69, 84, 474, 222, 3, step=4)
    piece("row 1 portrait", 43, 28, 28, 28, 214, 220, 2, step=3)
    r, g, b = C(197, 248)
    if not (g > r + 80 and g > b + 60): errors.append("no focus ring around the first Sim")
    if (255, 255, 255) not in [C(x, y) for y in range(240, 280) for x in range(728, 900)]:
        errors.append("no mood word")
elif case == "opt-menu":
    # Options in HD (src/port/options.*): both knobs at 100 % (the track's right end, x 1012), the
    # music's focused (lighter) with the focus ring around its well.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("music knob (focused)", C(1012, 390), hexrgb("9AA2E3"))
    expect("SFX knob", C(1012, 536), FRAME)
    r, g, b = C(197, 360)
    if not (g > r + 80 and g > b + 60): errors.append("no focus ring around the music volume")
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
        --layout console --print-screen --print-game --print-town "${town[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
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
# pad_late NAME STATE SCRIPT FRAMES: the same on the complete save (its clock).
pad_late() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game --print-town "${late[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_late pad-hotseat sit-casino interact-hotseat 300 &
pad_late pad-target D-cas-right2 interact-target 220 &
pad pad-buy sit-store interact-buy 200 &
pad_late pad-drencher A-pk-slot1-key2 pockets-drencher 200 &
pad_late pad-bin A-pk-slot1-key2 pockets-bin 240 &
wait
pad_late pad-bin-once A-pk-cnt3-slot1 pockets-bin-once 240 &
pad_late pad-bin-twice A-pk-cnt3-slot1 pockets-bin-twice 240 &
pad_late pad-bin-cancel A-pk-cnt3-slot1 pockets-bin-cancel 240 &
pad pad-give A-gift2-base gift-give 200 &
pad pad-gift-close sit-gift gift-close 150 &
pad pad-goals B-pg8-goals goals-scroll 230 &
pad_late pad-rel B-cas-rel relations-select 230 &
pad_late pad-volume B-cas-opt options-volume 330 &
pad_late pad-save B-cas-opt options-save 330 &
pad pad-slot B-st-opt-save saveslots-save 300 &
pad_late pad-ask-no B-cas-opt-save saveslots-ask-no 200 &
pad_late pad-ask-yes B-cas-opt-save saveslots-ask-yes 200 &
pad pad-pause-a B-pause popup-pause-a 120 &
pad pad-pause-start B-pause popup-pause-start 120 &
pad_late pad-quit-no B-cas-opt popup-quit-no 260 &
pad pad-phone s7 phone-pages 380 &
pad_late pad-phone-msg C-cas-msg1 phone-message 160 &
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
# The HD Interact tab (src/port/interact.*): from sit-casino, MENU, Down, A → Hotseat → Cards; from
# D-cas-right2 (Music Panel, the cards before it), Left → the cards (object 29), Right → the
# panel again (object 15); from sit-store, A on Buy → the catalogue.
hotseat() { in_order pad-hotseat 'console menu open, focus 0' 'console menu open, focus 1' 'game Cards'; }
target() { in_order pad-target 'interact target=obj15' 'console menu open' 'interact target=obj29' 'interact target=obj15.*prev=obj29'; }
buy() { in_order pad-buy 'console menu open, focus 0' 'game Catalogue'; }
check_pad pad-hotseat hotseat
check_pad pad-target target
check_pad pad-buy buy
# Pockets and Give Gift (src/port/pockets.*, gift.*): from A-pk-slot1-key2 (the Super Drencher
# selected), MENU, A → the Super Drencher's game (overlay 18, ItemHand); MENU, Left (slot 1), X →
# the game selects the slot (nothing thrown), X again → the bin empties the pockets; from
# A-gift2-base, Right, A → the bed given (the pockets keep the arcade), back to the panel; from
# sit-gift, B → the game's X, the panel.
drencher() { in_order pad-drencher 'console menu open, focus 7' 'game ItemHand'; }
bin_twice() {
    in_order pad-bin 'console menu open, focus 7' 'focus 0' 'pockets slot=0' 'inventory slots=- ' || return 1
    # The first X threw nothing: the pockets emptied only after the second (frame 140 on).
    local f
    f=$(grep -m 1 'inventory slots=- ' "$tmp/pad-bin.log" | sed 's/frame \([0-9]*\):.*/\1/')
    [ -n "$f" ] && [ "$f" -ge 140 ] || { echo "  pad-bin: the pockets emptied at frame $f (before the second X)"; return 1; }
}
# From A-pk-cnt3-slot1 (the slot already the game's selection): one X never throws, two do, and a
# focus move between them drops the question (the X after it asks again).
not_thrown() { ! grep -q 'inventory slots=- ' "$tmp/$1.log" || { echo "  $1: thrown"; return 1; }; }
bin_once() { in_order pad-bin-once 'console menu open, focus 0' && not_thrown pad-bin-once; }
bin_twice_sel() { in_order pad-bin-twice 'console menu open, focus 0' 'inventory slots=- '; }
bin_cancel() { in_order pad-bin-cancel 'console menu open, focus 0' 'focus 6' 'focus 0' && not_thrown pad-bin-cancel; }
give() { in_order pad-give 'console menu closed, focus 1' 'inventory slots=171:[^,]* counted' 'game Panel/Interact'; }
gift_close() { in_order pad-gift-close 'game Gift' 'game Panel/Interact'; }
check_pad pad-drencher drencher
check_pad pad-bin bin_twice
check_pad pad-bin-once bin_once
check_pad pad-bin-twice bin_twice_sel
check_pad pad-bin-cancel bin_cancel
check_pad pad-give give
check_pad pad-gift-close gift_close
# Goals, Relationships, Options and the Save slots (src/port/goals.*, relations.*, options.*):
# from B-pg8-goals, MENU, Down x3, Up → the game's selection 1, 2, 3 (its scroll first=1), 2,
# the HD focus following; from B-cas-rel, Down x5, Up → the game's Sim 1..5 (its portraits
# scrolled), 4; from B-cas-opt, Down, Left, Left, Right, Down, Left → the music 90 %, 80 %, 90 %,
# the SFX 90 % read back from RAM; A on Save → the game's slot screen, B → its X, the panel (the
# game goes back to Interact); from B-st-opt-save (a scratch save), Down, A → the game saves into
# slot 2 ("Saving"), the slot used and current.
goals() { in_order pad-goals 'goals sel=0 first=0' 'console menu open' 'goals sel=1 ' 'goals sel=2 ' 'goals sel=3 first=1' 'goals sel=2 first=1' 'focus 1'; }
rel() { in_order pad-rel 'relations first=0 sel=0' 'console menu open, focus 0' 'sel=1 ' 'focus 1' 'sel=4 ' 'first=2 sel=5' 'first=2 sel=4' 'focus 2'; }
volume() {
    in_order pad-volume 'options music=4096 sfx=4096' 'console menu open, focus 0' 'focus 2' 'options music=3686 sfx=4096' \
        'options music=3277 sfx=4096' 'options music=3686 sfx=4096' 'focus 3' 'options music=3686 sfx=3686'
}
save_open() { in_order pad-save 'console menu open, focus 0' 'game SaveSlots' 'console menu closed, focus 2' 'game Panel/'; }
slot_save() {
    in_order pad-slot 'saves current=-1 0=empty 1=empty 2=empty' 'game SaveSlots' 'focus 1' 'popup state=1 kind=3' 'saves current=1 0=empty 1=Sim'
}
check_pad pad-goals goals
check_pad pad-rel rel
check_pad pad-volume volume
check_pad pad-save save_open
check_pad pad-slot slot_save
# The game's question over a used slot (on a copy of the complete save): from B-cas-opt-save, A on
# Jun's slot → "Overwrite this save?" (kind 2), the HD focus on its Yes; B → its cross, the
# question gone, nothing saved; A, A → its tick, "Saving", slot 3 written (Jun's money now).
ask_no() {
    in_order pad-ask-no 'saves current=2 .*2=Jun/9150/' 'console menu closed, focus 2' 'popup state=1 kind=2 .*Overwrite' 'focus 0' 'popup none' || return 1
    if grep -q -E 'kind=3|2=Jun/14843' "$tmp/pad-ask-no.log"; then echo "  pad-ask-no: saved after B"; return 1; fi
}
ask_yes() { in_order pad-ask-yes 'popup state=1 kind=2 .*Overwrite' 'focus 0' 'popup state=1 kind=3' 'saves current=2 .*2=Jun/14843/'; }
check_pad pad-ask-no ask_no
check_pad pad-ask-yes ask_yes
# Popups, the phone (src/port/popup.*, phone.*): from B-pause, A (the HD popup's button: the
# game's X) or Start (passed on) → the panel; from B-cas-opt, MENU, Right, A → the game's quit
# question over the open HD menu, B → its cross: the menu open again on Quit (focus 1), never the
# title; from s7, Down, A → the phonebook (page 5), A → the contacts (7), A → the call's answer
# (9, the Sim), A → the game's Down five times to Exit, then its A (back to 7), B → the panel; from
# C-cas-msg1, A → ten Downs past the text to "Next Message", then A: the next message (slot 1).
pause_a() { in_order pad-pause-a 'popup state=2 kind=0 .*Game Paused' 'popup state=3' 'game Panel/Interact'; }
pause_start() { in_order pad-pause-start 'popup state=2 kind=0' 'popup state=3' 'game Panel/Interact'; }
quit_no() {
    in_order pad-quit-no 'console menu open, focus 1' 'popup state=1 kind=2 .*Quit' 'popup state=3' 'game Panel/Options' 'console menu open, focus 1' || return 1
    if grep -q -E 'game FrontEnd|console menu closed' "$tmp/pad-quit-no.log"; then echo "  pad-quit-no: the menu closed or the game quit"; return 1; fi
}
phone_pages() {
    in_order pad-phone 'phone page=0 row=1' 'phone page=5' 'phone page=7 .*list=22' 'phone page=9 .*caller=22' 'phone page=9 row=5' 'phone page=7' 'game Panel/Interact'
}
phone_msg() { in_order pad-phone-msg 'phone page=3 row=0 .*shown=0' 'phone page=3 row=10 ' 'phone page=3 row=0 .*shown=1'; }
check_pad pad-pause-a pause_a
check_pad pad-pause-start pause_start
check_pad pad-quit-no quit_no
check_pad pad-phone phone_pages
check_pad pad-phone-msg phone_msg

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
# The HD conversation's pages (src/port/dialogue.*): from s5 tapping the arrow through the first
# speech (scripts/dlg-taps.txt), the HD text follows the game's page counter the same frame:
# --print-hud logs the page and the HD box's string on the same frame, and the box's text
# changes between the frame before and that frame; on every page the text, the name and a
# portrait that is not black. From s5, A pressed twice (scripts/console/dialogue-a.txt) turns
# exactly two pages. Then each language's conversations (every page of the game's speech table,
# --measure-string all; a layout check: the game itself stays in its firmware's language) fit
# the box with no placeholder or escape left in their text, the longest one too (--measure-string ID), and its picture (--dialogue-string) keeps
# every white text pixel of the panel inside the text well.
dlg() {
    local name=$1; shift
    "$bin" "$rom" --headless --state-load "$shots/s5.st" --save "$tmp/$name.sav" --layout console --clock "2026-01-01 12:00" \
        --print-hud "$@" > "$tmp/$name.log" 2>&1 && grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
dlg dlg-pages --frames 160 --input "$repo/scripts/dlg-taps.txt" --compose --shot-at 28,29,88,89,149,150 --shot-dir "$tmp/dlg-pages" &
dlg dlg-a --frames 160 --input "$repo/scripts/console/dialogue-a.txt" &
dlg dlg-norom --frames 240 --input "$repo/scripts/dlg-taps.txt" --no-romdata &
wait
pages() {
    local f
    for f in 29 89 150; do
        grep -q -E "^frame $f: dialogue .*page=" "$tmp/dlg-pages.log" && grep -q -E "^frame $f: hd dialogue " "$tmp/dlg-pages.log" ||
            { echo "  dlg-pages: no page change and HD text both at frame $f"; return 1; }
    done
    "$python" -I - "$tmp/dlg-pages" <<'EOF'
import sys
from PIL import Image

d = sys.argv[1]
shot = lambda f: Image.open(f"{d}/f{f:06d}.png").convert("RGB")
errors = []
white = lambda im, x0, y0, x1, y1: sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if min(im.getpixel((x, y))) > 235)
for f in (28, 29, 88, 89, 149, 150):
    im = shot(f)
    if white(im, 150, 500, 960, 680) < 250:
        errors.append(f"frame {f}: no text")
    if white(im, 140, 430, 400, 476) < 75:
        errors.append(f"frame {f}: no name")
    if sum(1 for y in range(504, 672, 6) for x in range(1006, 1140, 6) if max(im.getpixel((x, y))) > 40) < 300:
        errors.append(f"frame {f}: no portrait")
for a, b in ((28, 29), (88, 89), (149, 150)):
    A, B = shot(a), shot(b)
    if sum(1 for y in range(500, 680, 2) for x in range(150, 960, 2) if A.getpixel((x, y)) != B.getpixel((x, y))) < 200:
        errors.append(f"the text did not change from frame {a} to {b}")
for e in errors:
    print(f"  dlg-pages: {e}")
sys.exit(1 if errors else 0)
EOF
}
check_pad dlg-pages pages
two_pages() {
    in_order dlg-a 'dialogue speech=0 page=1/4' 'dialogue speech=0 page=2/4' 'dialogue speech=0 page=3/4' || return 1
    if grep -q 'page=4/4' "$tmp/dlg-a.log"; then echo "  dlg-a: A turned more than two pages"; return 1; fi
}
check_pad dlg-a two_pages
# Without the game's text, --print-hud still logs the pages, and "dialogue none" only when the
# conversation is not up (never on a frame that logged a page).
norom() {
    in_order dlg-norom 'dialogue speech=0 page=1/4' 'dialogue speech=0 page=4/4' 'dialogue none' 'dialogue speech=1 page=1/3' || return 1
    local f
    for f in $(sed -n 's/^frame \([0-9]*\): dialogue speech=.*/\1/p' "$tmp/dlg-norom.log"); do
        if grep -q "^frame $f: dialogue none" "$tmp/dlg-norom.log"; then echo "  dlg-norom: frame $f logged a page and none"; return 1; fi
    done
    if grep -q 'hd dialogue' "$tmp/dlg-norom.log"; then echo "  dlg-norom: an HD line without the game's text"; return 1; fi
}
check_pad dlg-norom norom
for lang in 1 2 3 4 5; do
    name=measure-$lang
    if ! out=$("$bin" "$rom" --headless --frames 1 --state-load "$shots/s5.st" --save "$tmp/$name.sav" --language $lang --measure-string all 2> "$tmp/$name.log"); then
        echo "FAIL $name: $out $(tail -n 1 "$tmp/$name.log")"; fails=$((fails + 1)); continue
    fi
    id=$(echo "$out" | sed -n 's/.*longest string \([0-9]*\).*/\1/p')
    one=$("$bin" "$rom" --headless --frames 1 --state-load "$shots/s5.st" --save "$tmp/$name.sav" --language $lang --measure-string "$id" 2>/dev/null)
    if [ "$(echo "$one" | grep -c ' fits$')" != 2 ]; then echo "FAIL $name: string $id: $one"; fails=$((fails + 1)); continue; fi
    dlg "$name" --frames 40 --language $lang --dialogue-string "$id" --compose --shot-at 30 --shot-dir "$tmp/$name"
    if [ ! -f "$tmp/$name.ok" ]; then echo "FAIL $name: the picture's run did not end well"; fails=$((fails + 1)); continue; fi
    # Every white text pixel of the panel's rows inside the text well (x 132-974) or on the
    # name plate (above the well's top edge); the portrait's column left out.
    if "$python" -I - "$tmp/$name/f000030.png" <<'EOF'
import sys
from PIL import Image

im = Image.open(sys.argv[1]).convert("RGB")
# The well's top edge: the first row of the well's colour at x 600, from y 380 down.
top = next((y for y in range(380, 700) if im.getpixel((600, y)) == (0x6A, 0x72, 0xB8)), None)
if top is None:
    print("  no text well")
    sys.exit(1)
out = [(x, y) for y in range(top, 706) for x in range(110, 1170)
       if not (132 <= x < 975) and not (996 <= x < 1150) and min(im.getpixel((x, y))) > 245]
if out:
    print(f"  text outside the well at {out[:3]}")
    sys.exit(1)
EOF
    then echo "ok   $name: $(echo "$out" | head -n 1)"
    else echo "FAIL $name"; fails=$((fails + 1)); fi
done
# The HD Interact tab's words in each language (src/port/interact.*: --measure-menu): every verb
# in its row and in the A hint, every Sim's and object's name in the title and the strip, held in
# their boxes (shrunk at most; a neighbour's name beside the plate may end in an ellipsis).
for lang in 1 2 3 4 5; do
    if out=$("$bin" "$rom" --language $lang --measure-menu 2> "$tmp/menu-$lang.log"); then echo "ok   menu-words-$lang: $out"
    else echo "FAIL menu-words-$lang: $out $(tail -n 1 "$tmp/menu-$lang.log")"; fails=$((fails + 1)); fi
done
if [ "$fails" != 0 ]; then echo "console_check: $fails failure(s)"; exit 1; fi
echo "console_check: ok"
