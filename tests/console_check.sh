#!/usr/bin/env bash
# Checks the CONSOLE layout's pictures (src/console.*): from save states, frame 119 drawn with
# --layout console --compose must hold the DS screens' own pixels where the composition puts
# them (the same run without --compose gives the two screens), and the frames, the dimmed
# backdrop and the focus ring in the game's colours:
#   sit-cityhall (the panel, in town): the world (no rail: the HD menu replaced it), the menu hint chip;
#   the same with --console-menu: the HD menu (src/port/panelmenu.*: no target, no
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
#   act-social, act-vacuum, act-cards: the HD minigame frame (src/port/minigame.*): the touch
#   screen 3x at the left, the world small at the top right, the title plate (none for the social
#   game), the side panel, the prompt bar's words (cards-ask: B's quit question, A and B alone;
#   vacuum-clog: the blow's hint lit red; cards-wide, social-wide, vacuum-2x); cards-m1
#   (--no-romdata): milestone 1's hints' box; s2 (the main menu), title, boot: the HD front end (src/port/front.*);
#   s3-m1 (Create-a-Sim with --no-romdata: no HUD): milestone 1's front end view, the top screen framed at the left at its shape and the touch screen
#   2.5x framed at the right, the buttons' hint below, never SIDE; s3, s4, cas-hair, cas-face, cas-clothes (and s3 at 2x and
#   --wide, s4 in Italian): the HD Create-a-Sim (src/port/cas.*: the model framed at the left, the game's tabs and the
#   page's cells the touch screen's pixels, the focus's halo); slots (sit-store, Options >
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
# from sit-store, A on Buy opens the catalogue; the HD catalogue (src/port/catalogue.*, on copies of
# the complete save): a couch bought twice, a bed sold, the list scrolled by the game's arrows and
# closed, a purchase the game refuses not made; HD Build Rooms (src/port/build.*, the research's
# states with game flag 19 patched in, on copies of a save): the list scrolled by the game's arrows,
# a room chosen and the question cancelled both ways, a room bought (the money less its permit, its
# building timer set, the list rebuilt without it, the game's popup 2628 in HD once the menu closes),
# a room the money does not cover refused. In none of them do the pixels disagree with the
# game for long ("pixels disagree").
# Then the activities on buttons (src/activities.*, scripts/console/): from act-social, RIGHT
# answers with the right icon (it lights green; the game itself plays on for about 1600
# frames whatever the answers); from act-cards, X draws (Jun's count on the score bar changes),
# X, Y then A goes on past the end-of-turn message (RAM's message=1, then 0, the opponent drawing),
# X then Y draws and discards (the count back to its start), B then A quits (Cards, None,
# Panel/Interact; the game: Cards, Unknown in the black load, which CONSOLE shows as Loading, then
# Panel/Interact), B then B answers no and X still draws; from act-vacuum-clog, Y held 10
# frames clears the clog (the bowl's row 1 back to #30A2D3) within 30 frames (RAM: clogged=1, then 0).
# The pointing minigames (src/activities.*, src/port/minigame.*; the research's G-* states on copies
# of the complete save): pictures autopsy (and -2x), detector (and -wide), art, art-ask (the frame,
# the pointer in the playfield's middle, the hints lit as the game asks); and the mg-* runs: an organ
# carried to the tray and banked, the payout, the autopsy quit (B twice) and not quit (B once), a dig
# with the depth slider moved, the detector off, a stroke, undo, the bucket's fill and a colour, the
# painting's question answered both ways. The lottery, the mixer, its save page and the wireless lobby
# (pictures scratch, mixer, mixer-wide, mixer-save, trading; the mg-scratch-*, mg-mixer-*,
# mg-mixersave-back and mg-trading-exit runs): a ticket won and paid, a quit asked then done, a
# recording saved into a slot, the microphone recorded, the save page left, the lobby left.
# Needs the ROM, the save states and Pillow (not in git, so not in CI):
#   bash tests/console_check.sh
# Override with SIMS2WIDE, ROM, SHOTS, MORE_SHOTS, COMPLETE_SAVE (as tests/screens_check.sh),
# PYTHON (a Python with Pillow) and MAX_SIMS (the emulators run at once, 4).
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
# At most MAX_SIMS (default 4) emulators at once: "$bin" is this function, which takes one of
# the slots (a directory each) for the run.
max_sims=${MAX_SIMS:-4}
[ "$max_sims" -ge 1 ] 2>/dev/null || max_sims=1
real_bin=$bin
sims2wide_slot() {
    local i slot=
    while [ -z "$slot" ]; do
        for i in $(seq 1 "$max_sims"); do mkdir "$tmp/slot$i" 2>/dev/null && { slot=$tmp/slot$i; break; }; done
        [ -n "$slot" ] || sleep 0.2
    done
    "$real_bin" "$@"
    local rc=$?
    rmdir "$slot"
    return $rc
}
bin=sims2wide_slot

# run NAME STATE [options...]: frame 119 as the console's picture (NAME/c/f000119.png) and as
# the two screens (NAME/r/f000119.png); NAME.ok only if both runs went to their end.
run() {
    local name=$1 state=$2 file; shift 2
    file=$shots/$state.st
    [ -f "$file" ] || file=$more_shots/$state.st
    if [ ! -f "$file" ]; then echo "no save state $state.st in $shots or $more_shots" > "$tmp/$name.log"; return; fi
    local save=$tmp/$name.sav
    case $state in act-cards|sit-casino|A-pk-cnt3-slot1|A-pk-slot1-key2|B-cas-*|C-cas-*|M-music|M-mu-*|SP-*|G-*|F-cload-*|F-copt-*) cp "$complete_save" "$save" || return ;; esac
    "$bin" "$rom" --headless --frames 120 --state-load "$file" --save "$save" --layout console \
        --compose --shot-at 119 --shot-dir "$tmp/$name/c" --console-focus -1 "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" || return
    case $state in act-cards|sit-casino|A-pk-cnt3-slot1|A-pk-slot1-key2|B-cas-*|C-cas-*|M-music|M-mu-*|SP-*|G-*|F-cload-*|F-copt-*) cp "$complete_save" "$save" ;; esac
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
run s3-m1 s3 "${town[@]}" --no-romdata &
wait
run cards-m1 act-cards "${late[@]}" --no-romdata &
run cards-ask act-cards "${late[@]}" --input "$repo/scripts/console/cards-ask.txt" &
run cards-wide act-cards "${late[@]}" --wide &
run vacuum-clog act-vacuum-clog "${hotel[@]}" &
run vacuum-2x act-vacuum "${hotel[@]}" --compose-scale 2 &
run social-wide act-social "${town[@]}" --wide &
wait
# The pointing minigames (the research's held-out states, on copies of the complete save): the
# pointer in the playfield's middle (a new screen's), the game's values and prompts.
run autopsy G-A-para "${late[@]}" &
run autopsy-2x G-A-para "${late[@]}" --compose-scale 2 &
run detector G-A-md-walk "${late[@]}" &
run detector-wide G-A-md-walk "${late[@]}" --wide &
wait
run art G-D-art-stroke "${late[@]}" &
run art-ask G-D-art-x "${late[@]}" &
wait
# The timing minigames: MooGoo's player's turn (the focus on the first monkey that takes a bet), the
# Drencher (its gauges), the crime game.
run moogoo G-B-mg-r1-full "${late[@]}" &
run moogoo-2x G-B-mg-r1-full "${late[@]}" --compose-scale 2 &
run drencher G-C-wg-pump "${late[@]}" &
run drencher-wide G-C-wg-pump "${late[@]}" --wide &
wait
run crime G-X-crime "${late[@]}" &
# The Scratch lottery (two boxes revealed, the prize 10), the music mixer (and --wide), its save page
# (the focus on the first key), the wireless lobby (out of scope: its note).
run scratch G-B-sc-two-p10 "${late[@]}" &
run mixer G-X-music "${late[@]}" &
wait
run mixer-wide G-X-music "${late[@]}" --wide &
run mixer-save G-D-mus-save "${late[@]}" --console-focus 0 &
run trading G-T-trade "${late[@]}" &
wait
run s2 s2 "${town[@]}" --console-focus 0 &
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
run shop sit-storebuy "${town[@]}" --console-focus 0 &
run shop-2x sit-storebuy "${town[@]}" --console-focus 0 --compose-scale 2 &
run shop-wide sit-storebuy "${town[@]}" --console-focus 0 --wide &
run shop-sell BY-own4 "${town[@]}" --console-focus 10 &
run shop-poor BY-dn2 "${town[@]}" --console-focus 6 &
wait
run build-list BD-list "${town[@]}" --console-focus 1 &
run build-list-2x BD-list "${town[@]}" --console-focus 1 --compose-scale 2 &
run build-list-wide BD-list "${town[@]}" --console-focus 1 --wide &
run build-ask BD-row2 "${town[@]}" --console-focus 1 &
run build-poor BD-down3 "${town[@]}" --console-focus 0 &
run build-empty sit-build "${town[@]}" &
wait
run registry M-ci-occ-reg2 "${hotel[@]}" --console-focus 0 &
run registry-2x M-ci-occ-reg2 "${hotel[@]}" --console-focus 0 --compose-scale 2 &
run registry-wide M-ci-occ-reg2 "${hotel[@]}" --console-focus 0 --wide &
run registry-guest M-ci-g-base "${hotel[@]}" --console-focus 0 &
run roominfo M-ci-occ-info "${hotel[@]}" --console-focus 1 &
run roominfo-vacant M-ci-e-btn1 "${hotel[@]}" --console-focus 1 &
run roominfo-hint M-ci-occ-info "${hotel[@]}" --input "$repo/scripts/console/roominfo-hint.txt" &
wait
run music M-music "${late[@]}" --console-focus 2 &
run music-2x M-music "${late[@]}" --console-focus 2 --compose-scale 2 &
run music-wide M-music "${late[@]}" --console-focus 2 --wide &
run music-scrolled M-mu-down3 "${late[@]}" --console-focus 0 &
run stat-main SP-stat-base "${late[@]}" --console-focus 0 &
run stat-main-2x SP-stat-base "${late[@]}" --console-focus 0 --compose-scale 2 &
run stat-main-wide SP-stat-base "${late[@]}" --console-focus 0 --wide &
wait
run stat-asp SP-asp-150 "${late[@]}" --console-focus 0 &
run stat-hotel SP-hotel-150 "${late[@]}" --console-focus 0 &
run stat-skills SP-skills "${late[@]}" --console-focus 0 &
run stat-stats SP-stats "${late[@]}" --console-focus 0 &
wait
run title F-title0 "${town[@]}" --console-focus 0 &
run title-wide F-title0 "${town[@]}" --wide &
run boot F-boot-900 "${town[@]}" --console-focus 0 &
run boot-wide F-boot-900 "${town[@]}" --wide &
run menu-2x s2 "${town[@]}" --console-focus 1 --compose-scale 2 &
run menu-wide s2 "${town[@]}" --console-focus 0 --wide &
run menu-de s2 "${town[@]}" --console-focus 3 --language 3 &
wait
run settings F-mm-opt-150 "${town[@]}" --console-focus 0 &
run settings-2x F-mm-opt-150 "${town[@]}" --console-focus 0 --compose-scale 2 &
run settings-wide F-mm-opt-150 "${town[@]}" --console-focus 0 --wide &
run settings-concept F-copt-concept-80 "${town[@]}" --console-focus 3 &
run settings-credits F-opt-credits-400 "${town[@]}" &
run settings-fr F-copt-concept-80 "${town[@]}" --console-focus 3 --language 2 &
run loadgame F-cload-150 "${town[@]}" --console-focus 0 &
run loadgame-2x F-cload-150 "${town[@]}" --console-focus 1 --compose-scale 2 &
run loadgame-wide F-cload-150 "${town[@]}" --console-focus 0 --wide &
run loadgame-empty F-mm-load-150 "${town[@]}" &
run loadgame-loading F-cload-t1-60 "${town[@]}" &
wait
run s3 s3 "${town[@]}" --console-focus 9 &
run s3-2x s3 "${town[@]}" --console-focus 9 --compose-scale 2 &
run s3-wide s3 "${town[@]}" --console-focus 9 --wide &
run s3-res2 s3 "${town[@]}" --console-focus 9 --res3d 2 &
run s4 s4 "${town[@]}" --console-focus 0 &
wait
run s4-it s4 "${town[@]}" --console-focus 4 --language 4 &
run cas-hair F-hair-s3-40 "${town[@]}" --console-focus 10 &
run cas-face F-face-5-40 "${town[@]}" --console-focus 4 &
run cas-clothes F-thr-6-40 "${town[@]}" --console-focus 11 &
wait
# The UI's size (task 10, --ui-scale) and the game's pictures (--game-pictures smooth): the
# conversation and the panel's menu at 75 % and 150 %, the Pockets' and the catalogue's icons Smooth.
run s5-hd-75 s5 "${town[@]}" --ui-scale 75 &
run s5-hd-150 s5 "${town[@]}" --ui-scale 150 &
run casino-menu-75 sit-casino "${late[@]}" --console-menu --console-focus 1 --print-hud --ui-scale 75 &
run casino-menu-150 sit-casino "${late[@]}" --console-menu --console-focus 1 --print-hud --ui-scale 150 &
wait
run pockets-smooth A-pk-cnt3-slot1 "${late[@]}" --console-menu --console-focus 0 --game-pictures smooth &
run shop-smooth sit-storebuy "${town[@]}" --console-focus 0 --game-pictures smooth &
run shop-smooth-2x sit-storebuy "${town[@]}" --console-focus 0 --game-pictures smooth --compose-scale 2 &
wait
# The world's prompt (src/port/prompt.*): the store's clerk (A Buy), the hotel's concierge (three
# actions: the hold hint), no target (City Hall), another tab up (Pockets: no Interact object).
run prompt-store L-sit-store-none-250 "${late[@]}" &
run prompt-concierge D-hotel-down "${hotel[@]}" &
run prompt-none L-sit-cityhall-none-250 "${late[@]}" &
run prompt-pockets A-pk-town-cnt1 "${late[@]}" &
wait
# The prompt at the UI's sizes (its own group: bottom centre, above the strip).
run prompt-store-150 L-sit-store-none-250 "${late[@]}" --ui-scale 150 &
run prompt-store-75 L-sit-store-none-250 "${late[@]}" --ui-scale 75 &
wait

fails=0
for name in s7 cityhall cityhall-wide cityhall-menu s7-menu gift s5 s5-hd s5-hd-wide social vacuum cards s2 s3-m1 slots load casino casino-2x \
    cards-m1 cards-ask cards-wide vacuum-clog vacuum-2x social-wide autopsy autopsy-2x detector detector-wide art art-ask \
    moogoo moogoo-2x drencher drencher-wide crime scratch mixer mixer-wide mixer-save trading \
    s3 s3-2x s3-wide s3-res2 s4 s4-it cas-hair cas-face cas-clothes \
    title title-wide boot boot-wide menu-2x menu-wide menu-de \
    settings settings-2x settings-wide settings-concept settings-credits settings-fr loadgame loadgame-2x loadgame-wide loadgame-empty loadgame-loading \
    menu-m1 casino-menu casino-menu-2x casino-menu-wide pockets-menu pockets-armed gift-items gift-items-2x gift-items-wide \
    goals-menu rel-menu rel-menu-wide opt-menu slots-hd popup-goal popup-pause popup-quit popup-menu phone-msg phone-contacts phone-wide \
    shop shop-2x shop-wide shop-sell shop-poor build-list build-list-2x build-list-wide build-ask build-poor build-empty \
    registry registry-2x registry-wide registry-guest roominfo roominfo-vacant roominfo-hint \
    music music-2x music-wide music-scrolled stat-main stat-main-2x stat-main-wide stat-asp stat-hotel stat-skills stat-stats \
    s5-hd-75 s5-hd-150 casino-menu-75 casino-menu-150 pockets-smooth shop-smooth shop-smooth-2x \
    prompt-store prompt-concierge prompt-none prompt-pockets prompt-store-150 prompt-store-75; do
    if [ ! -f "$tmp/$name.ok" ]; then
        echo "FAIL $name: the run did not end well: $(tail -n 1 "$tmp/$name.log")"
        fails=$((fails + 1))
        continue
    fi
    if "$python" -I - "$tmp/$name/c/f000119.png" "$tmp/$name/r/f000119.png" "$name" "$tmp/$name.log" <<'EOF'
import re
import sys
from PIL import Image, ImageChops

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

# Canvas points the pointing minigames' pointer covers (its ring, 15 around its point): not compared.
SKIP = [(421, 356)] if case.split("-")[0] in ("autopsy", "art", "scratch") or case in ("mixer", "mixer-wide") else []
# DS rectangles a ring is drawn around (MooGoo's focus: the first monkey, 56,32 24 x 32): not compared.
SKIP_DS = [(50, 26, 86, 70)] if case.split("-")[0] == "moogoo" else [(0, 68, 31, 110)] if case == "mixer-save" else []

def piece(what, fx, fy, fw, fh, tx, ty, scale, step=5):
    """A piece of the bottom screen drawn whole-number times bigger: each DS pixel's block."""
    for y in range(0, fh, step):
        for x in range(0, fw, step):
            if any(abs(tx + x * scale - sx) < 20 and abs(ty + y * scale - sy) < 20 for sx, sy in SKIP):
                continue
            if any(x0 <= fx + x <= x1 and y0 <= fy + y <= y1 for x0, y0, x1, y1 in SKIP_DS):
                continue
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
    """No rail (the owner: the HD menu, Back / E, replaced it): where it was is the backdrop at 4:3
    (the world ends at 1163); the menu's hint is in the strip (strip(hints=True))."""
    BACK = (16, 18, 24)
    expect("no rail: backdrop where its frame was", C(1170, 360), BACK)
    expect("no rail: backdrop where its tabs were", C(1200, 300), BACK)
    expect("no rail: backdrop where its phone was", C(1210, 174), BACK)
    # The world without the game's status bar (its last 16 rows): 176 rows 720 high, its last
    # rows under the strip (sampled clear of the world's prompt, src/port/prompt.*, centred above
    # row 640, and its shadow).
    w = world_full(176)
    for cx, cy in ((400, 100), (1050, 650), (1000, 300), (900, 658)):
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

def meter(values, x0, x1, y0, y1):
    """The Sanity meter in x0..x1, rows y0..y1: its body in the tier's colour, filled cur / max
    of the track (the well's inside less 2 pixels a side) within 2 pixels."""
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

STRIP_Y = 661

def strip(values=None, hints=False):
    """The strip under the world (port/statusbar.h's AddStatusStrip): one kit panel the canvas's
    whole width from row 661 (the widescreen world's foot): its dark outline, then the frame band
    across, its fill below to the canvas's foot, never the backdrop; white text in the status at
    the left (from x 16), no panel of its own there; with values (from --print-hud), the meter;
    with hints (the town), at the right the BACK pill (headless: the kit's Xbox look)."""
    BAND = hexrgb("6171B2")
    for x in (0, 16, 640, 1263, 1279):
        expect(f"strip's outline at {x}", C(x, STRIP_Y), OUTLINE)
        expect(f"strip's band at {x}", C(x, STRIP_Y + 2), BAND)
        expect(f"strip's fill at {x}", C(x, 717), FRAME)
    back = sum(1 for y in range(STRIP_Y, 720, 2) for x in range(0, 1280, 4) if C(x, y) == (16, 18, 24))
    if back:
        errors.append(f"the backdrop in the strip ({back} samples)")
    # (no panel of the status's own: the strip's fill left of its first value and between them)
    expect("strip's fill left of the status", C(20, 691), FRAME)
    inside = [C(x, y) for y in range(STRIP_Y + 6, 716) for x in range(16, 760)]
    if inside.count((255, 255, 255)) < 40:
        errors.append("no status text in the strip")
    if values is not None:
        meter(values, 16, 760, STRIP_Y + 4, 718)
    if hints:
        expect("the strip's BACK pill", C(1140, 698), (46, 48, 99))
        right = [C(x, y) for y in range(STRIP_Y + 6, 716) for x in range(900, 1260)]
        if right.count((255, 255, 255)) < 40:
            errors.append("no hint words at the strip's right")

def front():
    """No HUD: the top screen fitted in 24,72 568 x 480 (4:3: 568 x 426 at 24,99), the touch
    screen 2.5x at 616,72, both framed; the hints' panel below; the canvas dark elsewhere."""
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
    # the hints: a kit panel of chips below (the right stick, A, the pause menu's), its fill and white words
    if FRAME not in [C(x, y) for y in range(600, 660) for x in range(240, 1040)]:
        errors.append("no hint panel")
    row = [C(x, y) for y in range(614, 646) for x in range(240, 1040)]
    if (255, 255, 255) not in row or OUTLINE not in row:
        errors.append("no hint text")

BACKDROP = (16, 18, 24)

def front_cas(page, focus, raw_top=True):
    """HD Create-a-Sim (src/port/cas.*): the top screen (the model) at its shape in 16,16 664 x 640
    (4:3: 664 x 498 at 16,87), framed; the panel at the right; the game's five tabs = the touch
    screen's pixels 2x in the tab bar; one live crop of the page = the touch screen's pixels (the
    gender button 2x, a hair cell, a face, a clothes cell 3x, an aspiration 2x); the focus's green
    halo just outside `focus` (x, y, w, h); white words in the title's well (the name's letters on
    page 0, the page's title on 1-4); the hints."""
    expect("canvas", comp.size, (1280, 720))
    if raw_top:
        k = 664 / 256
        for x in range(0, 256, 11):
            for y in range(0, 192, 11):
                # (the game's "L" / "R" pills on its turn arrows: covered by the pad's turn chips)
                if (26 <= x <= 59 or 197 <= x <= 230) and 141 <= y <= 159:
                    continue
                expect(f"top DS {x},{y}", C(16 + int((x + 0.5) * k), 87 + int((y + 0.5) * k)), top(x, y))
                if len(errors) > 4:
                    return
        # The game's "L" pill (DS 29-56, 144-156) under the pad's chip's dark plate (#5961A2).
        expect("turn pill covered", C(16 + int(31 * k), 87 + int(150 * k)), hexrgb("5961A2"))
    expect("top frame", C(12, 300), OUTLINE)
    expect("backdrop", C(5, 5), BACKDROP)
    expect("panel", C(704, 640), FRAME)
    for i in range(5):
        x0, y0 = 780 + 80 * i + 6, 34 + 4
        for dy in range(0, 30, 5):
            for dx in range(0, 34, 5):
                expect(f"tab {i} DS {1 + dx},{8 + 37 * i + dy}", C(x0 + 2 * dx + 1, y0 + 2 * dy + 1), bottom(1 + dx, 8 + 37 * i + dy))
        if len(errors) > 4:
            return
    # One live crop of the page: DS x, y, w, h, its scale, its HD box (Keep this Sim?: the game's
    # aspiration pictures, sprite 362, their blue splash in each cell).
    if page == 4:
        for j in range(4):
            blue = sum(1 for yy in range(190, 294, 2) for xx in range(750 + 120 * j, 850 + 120 * j, 2) if C(xx, yy)[2] > 200 and C(xx, yy)[0] < 140)
            if blue < 100: errors.append(f"aspiration {j}: no picture ({blue})")
        fw = fh = 0
    else:
        fx, fy, fw, fh, s, (bx, by, bw, bh) = {
            0: (47, 7, 37, 37, 2, (750, 188, 84, 84)), 1: (94, 126, 46, 53, 3, (822, 441, 150, 171)), 2: (125, 90, 43, 39, 3, (909, 331, 141, 129)),
            3: (188, 109, 38, 54, 3, (1084, 384, 120, 174))}[page]
        ox, oy = bx + (bw - fw * s) // 2, by + (bh - fh * s) // 2
    for dy in range(0, fh, 4):
        for dx in range(0, fw, 4):
            expect(f"crop DS {fx + dx},{fy + dy}", C(ox + s * dx + s // 2, oy + s * dy + s // 2), bottom(fx + dx, fy + dy))
            if len(errors) > 4:
                return
    x, y, w, h = focus
    r, g, b = C(x + w // 2, y - 3)
    if not (g > r + 80 and g > b + 60): errors.append(f"no focus halo above {focus}: {(r, g, b)}")
    words = sum(1 for yy in range(122, 166) for xx in range(730, 1230) if C(xx, yy) == (255, 255, 255))
    if words < 60: errors.append(f"no title or name ({words})")
    hints = [C(xx, yy) for yy in range(654, 690) for xx in range(1000, 1240)]
    if sum(1 for p in hints if p != FRAME) < 40: errors.append("no hints")

def front_menu(focus, raw_top=True):
    """The HD main menu (src/port/front.*): the top screen 3x at 24,72 framed, the panel at the
    right with four buttons, each a white label, the focused one ringed in green with the game's
    green arrow, the others' arrows blue; DS Wireless (button 3) greyed: its label dim, no arrow;
    the hints under them."""
    expect("canvas", comp.size, (1280, 720))
    if raw_top:
        for x in range(0, 256, 9):
            for y in range(0, 192, 9):
                expect(f"top 3x DS {x},{y}", C(24 + 3 * x + 1, 72 + 3 * y + 1), top(x, y))
                if len(errors) > 4:
                    return
    expect("top frame", C(20, 300), OUTLINE)
    expect("panel", C(1240, 600), FRAME)
    expect("backdrop", C(5, 5), BACKDROP)
    for i in range(4):
        y0 = 84 + 128 * i
        white = sum(1 for y in range(y0 + 25, y0 + 75) for x in range(868, 1150) if C(x, y) == (255, 255, 255))
        arrow = [C(x, y) for y in range(y0 + 18, y0 + 82) for x in range(1164, 1228)]
        if i == 3:
            # Greyed: a dim label (no white), no arrow.
            dim = sum(1 for y in range(y0 + 25, y0 + 75) for x in range(868, 1150) if C(x, y) == (0xB8, 0xBC, 0xE0))
            if white or dim < 30: errors.append(f"button 3: not greyed ({white} white, {dim} dim pixels)")
            if sum(1 for r, g, b in arrow if b > r + 80 and b > g + 40) > 300: errors.append("button 3: an arrow")  # (an arrow: about 1100; the rim alone: under 100)
            r, g, b = C(835, y0 + 50)
            if (i == focus) != (g > r + 80 and g > b + 60): errors.append(f"button 3: the focus ring {'missing' if i == focus else 'drawn'}")
            continue
        if white < 30: errors.append(f"button {i}: no label ({white} white pixels)")
        green = sum(1 for r, g, b in arrow if g > r + 60 and g > b + 40)
        blue = sum(1 for r, g, b in arrow if b > r + 80 and b > g + 40)
        if i == focus:
            r, g, b = C(835, y0 + 50)
            if not (g > r + 80 and g > b + 60): errors.append(f"button {i}: no focus ring ({r}, {g}, {b})")
            if green < 50: errors.append(f"button {i}: the arrow is not green ({green})")
        else:
            r, g, b = C(835, y0 + 50)
            if g > r + 80 and g > b + 60: errors.append(f"button {i}: ringed")
            if blue < 50: errors.append(f"button {i}: the arrow is not blue ({blue})")
    hints = [C(x, y) for y in range(616, 656) for x in range(1100, 1232)]
    if sum(1 for p in hints if p != FRAME) < 40: errors.append("no hints")

def front_title(raw_top=True):
    """The HD title: the top screen 3.75x (960 x 720 at x 160), the backdrop at its sides, the A
    chip on its dark plate at the foot."""
    expect("canvas", comp.size, (1280, 720))
    if raw_top:
        for x in range(0, 256, 7):
            for y in range(0, 160, 7):
                expect(f"top DS {x},{y}", C(160 + int(x * 3.75 + 1.8), int(y * 3.75 + 1.8)), top(x, y))
                if len(errors) > 4:
                    return
    expect("backdrop left", C(5, 5), BACKDROP)
    expect("backdrop right", C(1200, 300), BACKDROP)
    plate = [C(x, y) for y in range(640, 712) for x in range(540, 740)]
    if sum(1 for r, g, b in plate if g > 180 and r < 160 and b < 140) < 10: errors.append("no A chip")
    if sum(1 for r, g, b in plate if max(r, g, b) < 70) < 200: errors.append("no dark plate")

def front_boot(raw=True):
    """The boot: the video (top) 3x at 24,72 and the splash (the touch screen) 1.5x at 838,216,
    both framed; the A and START chips under the splash."""
    expect("canvas", comp.size, (1280, 720))
    if raw:
        for x in range(0, 256, 9):
            for y in range(0, 192, 9):
                expect(f"video 3x DS {x},{y}", C(24 + 3 * x + 1, 72 + 3 * y + 1), top(x, y))
        for x in range(0, 256, 6):
            for y in range(0, 192, 6):
                expect(f"splash 1.5x DS {x},{y}", C(838 + x * 3 // 2, 216 + y * 3 // 2), bottom(x, y))
                if len(errors) > 4:
                    return
    expect("top frame", C(20, 300), OUTLINE)
    expect("splash frame", C(834, 300), OUTLINE)
    expect("backdrop", C(5, 5), BACKDROP)
    chips = [C(x, y) for y in range(534, 590) for x in range(838, 1222)]
    if sum(1 for p in chips if p != BACKDROP) < 300: errors.append("no A / START chips")

def front_settings(page, focus):
    """HD Settings (src/port/frontmenus.*): the top screen 3x framed at the left, the panel at the
    right, the game's four tabs = the touch screen's pixels 2x in the tab bar, the page: Music's
    slider (its percent's white text, the knob at the right end: 100 %), Concept Art's twelve
    cells = the touch screen's pixels 2x with the focus ring on `focus`, Credits' lines (white and
    the game's green) in their well."""
    expect("canvas", comp.size, (1280, 720))
    for x in range(0, 256, 11):
        for y in range(0, 192, 11):
            expect(f"top 3x DS {x},{y}", C(24 + 3 * x + 1, 72 + 3 * y + 1), top(x, y))
            if len(errors) > 4:
                return
    expect("panel", C(1240, 600), FRAME)
    expect("backdrop", C(5, 5), BACKDROP)
    for i in range(4):
        cx = 908 + 64 * i
        for dy in range(0, 26, 5):
            for dx in range(0, 28, 5):
                expect(f"tab {i} DS {7 + dx},{41 + 40 * i + dy}", C(cx + 4 + 2 * dx, 138 + 6 + 2 * dy), bottom(7 + dx, 41 + 40 * i + dy))
        if len(errors) > 4:
            return
    if page == 0:
        well = [C(x, y) for y in range(338, 402) for x in range(950, 1130)]
        if sum(1 for p in well if p == (255, 255, 255)) < 100: errors.append("no percent text")
        r, g, b = C(1192, 444)
        if not (r > 120 and b > 180): errors.append(f"knob not at 100 %: {(r, g, b)}")
        r, g, b = C(1036, 318)
        if not (g > r + 80 and g > b + 60): errors.append(f"slider not focused: {(r, g, b)}")
    elif page == 2:
        for i in range(12):
            gx, gy = 842 + (i % 4) * 100 + 4, 292 + (i // 4) * 88 + 4
            fx, fy = 56 + (i % 4) * 48, 72 + (i // 4) * 40
            for dy in range(0, 32, 7):
                for dx in range(0, 40, 7):
                    expect(f"cell {i} DS {fx + dx},{fy + dy}", C(gx + 2 * dx, gy + 2 * dy), bottom(fx + dx, fy + dy))
            r, g, b = C(842 + (i % 4) * 100 + 44, 292 + (i // 4) * 88 - 3)
            ringed = g > r + 80 and g > b + 60
            if ringed != (i == focus): errors.append(f"cell {i}: ring {ringed}")
            if len(errors) > 4:
                return
    elif page == 3:
        well = [C(x, y) for y in range(286, 582) for x in range(850, 1222)]
        if sum(1 for p in well if p == (255, 255, 255)) < 200: errors.append("no white credits lines")
        if sum(1 for p in well if p == (0x79, 0xFB, 0x00)) < 100: errors.append("no green credits headings")
    hints = [C(x, y) for y in range(612, 644) for x in range(1100, 1232)]
    if sum(1 for p in hints if p != FRAME) < 40: errors.append("no hints")

def front_load(focus, used, loading=False):
    """HD Load-a-Sim: the dimmed top screen behind the in-game Save slots' panel; each used row
    two lines of white text (name, money) and the place and time, the current slot's number green,
    the focus ring on `focus`; "No Data" rows a lavender line; the "Loading" popup over it all."""
    expect("canvas", comp.size, (1280, 720))
    for i in range(3):
        y0 = 124 + 156 * i
        if loading:
            continue
        name = sum(1 for y in range(y0 + 26, y0 + 66) for x in range(348, 700) if C(x, y) == (255, 255, 255))
        line2 = sum(1 for y in range(y0 + 78, y0 + 114) for x in range(348, 1050) if C(x, y)[2] > 200 and C(x, y)[0] > 180)
        if used[i]:
            if name < 60: errors.append(f"row {i}: no name ({name})")
            if line2 < 60: errors.append(f"row {i}: no place / time ({line2})")
        r, g, b = C(640, y0 - 3)
        ringed = g > r + 80 and g > b + 60
        if ringed != (i == focus): errors.append(f"row {i}: ring {ringed}")
    if not loading:
        num = [C(x, y) for y in range(124 + 30, 124 + 110) for x in range(240, 300)]
        if used[0] and sum(1 for r, g, b in num if g > r + 60 and g > b + 60) < 50: errors.append("slot 1's number not green")
        expect("panel", C(190, 360), FRAME)
    else:
        box = [C(x, y) for y in range(230, 480) for x in range(340, 940)]
        if sum(1 for p in box if p == (255, 255, 255)) < 300: errors.append("no Loading popup text")

def big_over_dim(rows=176):
    piece("touch screen 3x", 0, 0, 256, 192, 256, 72, 3, step=9)
    expect("touch screen 3x top-left", C(256, 72), bottom(0, 0))
    # The world without the game's status bar (176 rows): row 174 at the canvas's foot.
    w = world_full(rows)
    for cx, cy in ((200, 100), (1100, 650), (900, 715), (1050, 700)):
        r, g, b = w(cx, cy)
        expect(f"dimmed world at {cx},{cy}", C(cx, cy), tuple(v * (255 - 0x8C) // 255 for v in (r, g, b)))

def activity(rows=192, hd=True, skip=0):
    if case in ("cards-ask", "art-ask"):
        # A question of the game's: the HD box over the playfield, everything else under its veil;
        # the box's fill, its words and the focus's halo (No first).
        fill = sum(1 for y in range(70, 640, 3) for x in range(40, 800, 3) if C(x, y) == FRAME)
        if fill < 3000:
            errors.append(f"no HD box over the playfield ({fill} fill pixels)")
        white_text("question box", 80, 150, 760, 600)
        if not any(C(x, y) == RING for y in range(150, 640, 2) for x in range(60, 800, 2)):
            errors.append("no focus halo on the question's No")
        return
    expect("background", C(10, 10), PANEL)
    expect("touch screen frame", C(32, 300), OUTLINE)
    expect("touch screen 3x top-left", C(36, 68), bottom(0, 0))
    if case in ("moogoo", "moogoo-2x"):
        # (MooGoo's line, DS rows 2-21, is the HD plate's: the table below it.)
        piece("touch screen 3x", 0, 27, 256, 165, 36, 68 + 81, 3, step=9)
    else:
        piece("touch screen 3x", 0, 0, 256, 192, 36, 68, 3, step=9)
    expect("world frame", C(855, 200), OUTLINE)
    # The small world, 384 wide; without the game's status bar (176 rows: 264 high) where the
    # HD bar sits under it.
    # (skip: the social game's own bar, its top rows, left out.)
    h = round(384 * (rows - skip) / 256)
    # (At --compose-scale 2 the small world is sampled from twice as many pixels: not compared.)
    for x, y in ((0, 0), (100, 50), (383, h - 1), (200, h - 8)) if not case.endswith("-2x") else ():
        expect(f"world small {x},{y}", C(858 + x, 68 + y), top(x * 256 // 384, skip + y * (rows - skip) // h))
    if rows < 192:
        region = [C(x, y) for y in range(68 + h + 14, 68 + h + 58) for x in range(852, 1248)]
        if (255, 255, 255) not in region or OUTLINE not in region:
            errors.append("no status bar text under the small world")
    if hd:
        minigame_frame(h + (72 if rows < 192 else 18))
    else:
        # Milestone 1's hints' box (kHintBox, 850,400; --no-romdata): its outline, its fill, the
        # first button's chip and, in the first row's text (x 972 on), white letters with the
        # outline's shadow.
        expect("hints outline", C(851, 500), OUTLINE)
        expect("hints fill", C(1000, 406), FRAME)
        expect("hint chip", C(870, 430), PANEL)
        row = [C(x, y) for y in range(420, 444) for x in range(972, 1100)]
        if (255, 255, 255) not in row or OUTLINE not in row:
            errors.append("no hint text in the first row")

def white_text(name, x0, y0, x1, y1):
    region = [C(x, y) for y in range(y0, y1) for x in range(x0, x1)]
    if (255, 255, 255) not in region:
        errors.append(f"no white text in the {name}")
    return region

def minigame_frame(side_top):
    # side_top: the side panel's top less 68 (the secondary panel's top).
    # The HD minigame frame (src/port/minigame.*): the prompt bar under the playfield (the kit's
    # panel fill at its left end, white words), the side panel under the secondary one (its fill
    # just inside its top left), the title plate over the playfield (the cards' and the vacuum's).
    expect("prompt bar fill", C(40, 686), FRAME)
    bar = white_text("prompt bar", 40, 664, 1240, 708)
    if case in ("cards", "cards-ask", "cards-wide", "vacuum", "vacuum-2x", "vacuum-clog", "autopsy", "autopsy-2x", "detector", "detector-wide",
                "art", "art-ask", "moogoo", "moogoo-2x", "drencher", "drencher-wide", "crime", "scratch", "mixer", "mixer-wide", "mixer-save",
                "trading"):
        white_text("title plate", 40, 12, 400, 52)
    else:
        for y in range(10, 54, 4):
            for x in range(40, 400, 8):
                if C(x, y) != PANEL:
                    errors.append(f"a title plate where the social game has none, at {x},{y}")
                    return
    expect("side panel fill", C(1238, 68 + side_top + 30), FRAME)
    # The cards' question: A and B only (the face buttons' green and red, no X blue); the clog: the
    # blow's hint lit red; none of it otherwise.
    near = lambda p, q: all(abs(a - b) <= 24 for a, b in zip(p, q))
    has = lambda colour: any(near(p, colour) for p in bar)
    green, red, blue, lit = (0x6E, 0xDB, 0x5E), (0xFF, 0x6A, 0x5E), (0x5C, 0xB4, 0xFF), (0xB8, 0x32, 0x3A)
    if case == "cards-ask":
        if not has(green) or not has(red) or has(blue):
            errors.append("the question's prompts are not A and B alone")
    elif case.startswith("cards") and not (has(red) and has(blue)):
        # (The turn's first step: draw, X, and quit, B.)
        errors.append("the cards' prompts lack X or B")
    # (Lit too: the autopsy's tweezers while a parasite eats, G-A-para; the detector's dig with a find in range.)
    if (case in ("vacuum-clog", "autopsy", "autopsy-2x", "detector", "detector-wide")) != has(lit):
        errors.append("a hint lit when the game asks for nothing, or not lit when it does")
    # The painting's question: A and B alone.
    if case == "art-ask" and (not has(green) or not has(red) or has(blue)):
        errors.append("the painting's question's prompts are not A and B alone")
    if case.split("-")[0] == "moogoo":
        # The focus: the game's selection green around the first monkey (DS 56,32: canvas 204,164).
        ring = [C(x, y) for y in range(150, 270) for x in range(190, 290)]
        if RING not in ring:
            errors.append("no focus ring around the first monkey")
    if case == "trading" and (not has(red) or has(green) or has(blue)):
        errors.append("the wireless lobby's prompts are not B alone")
    if case == "mixer-save":
        # The focus: the ring around the keyboard's first key (DS 6,74 19 x 30: canvas 54,290).
        ring = [C(x, y) for y in range(276, 394) for x in range(40, 126)]
        if RING not in ring:
            errors.append("no focus ring around the first key")
    if case.split("-")[0] in ("autopsy", "art", "scratch") or case in ("mixer", "mixer-wide"):
        # The pointer: its ring's white (or gold) around the playfield's middle (DS 128,96).
        ring = [C(x, y) for y in range(338, 376) for x in range(402, 442)]
        if (255, 255, 255) not in ring:
            errors.append("no pointer in the playfield's middle")

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
    the buttons (kind 0: Close focused; kind 2: No focused, the safe answer, Yes plain)."""
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
        expect("No", C(400, 448), hexrgb("9AA2E3"))
        r, g, b = C(345, 448)
        if not (g > r + 60 and g > b + 40): errors.append("no focus ring around No")
        r, g, b = C(670, 448)
        if g > r + 60 and g > b + 40: errors.append("a focus ring around Yes")
        expect("Yes", C(880, 448), WELL)
    if menu:
        r, g, b = C(190, 300)
        if not b > r + 30: errors.append("no HD menu beneath")
    not_big()

if case == "cityhall":
    expect("canvas", comp.size, (1280, 720))
    rail()
    strip(hints=True)
elif case == "cityhall-wide":
    # Widescreen: the world without the game's bar keeps its sides (never cut to make room for
    # the crop): 341 x 176 fitted to the canvas's width, 1280 x 661, top-aligned; the strip right
    # under it, the canvas's whole width: no black between (the owner's playtest).
    big = Image.open(raw_path.replace(".png", "-top.png")).convert("RGB")
    expect("wide picture", big.size, (341, 192))
    for cx, cy in ((0, 300), (1279, 100), (640, 400), (300, 655), (780, 660), (1000, 660)):
        expect(f"wide world at {cx},{cy}", C(cx, cy), big.getpixel((cx * 341 // 1280, cy * 176 // 661)))
    strip(hints=True)
elif case == "s7":
    hd_phone(None)
    strip()
elif case == "cityhall-menu":
    # City Hall: no target (the plumbob; the title "Interact" alone), no row, the focus on the phone.
    hd_menu(0, -1)
    strip()
elif case == "menu-m1":
    # Without the game's text (--no-romdata): milestone 1's menu, the touch screen 3x (and the
    # game's own status bar in the world: no HD bar without its words).
    big_over_dim(192)
elif case in ("casino-menu", "casino-menu-2x"):
    # Keelhaulin' Cards: three rows, the focus on the second (Hotseat).
    hd_menu(3, 1)
    strip(hud_values())
elif case == "casino-menu-wide":
    hd_menu(3, 0, wide=True)
elif case in ("casino", "casino-2x"):
    # The complete save (yellow tier): the casino's panel, the meter filled cur / max in yellow.
    values = hud_values()
    if values is None:
        errors.append("no hud line from --print-hud")
    else:
        expect("tier", values[5], 2)
        strip(values, hints=True)
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
elif case in ("shop", "shop-2x", "shop-wide", "shop-sell", "shop-poor"):
    # The catalogue in HD (src/port/catalogue.*): the panel, the title's well, the money in its
    # warm white, the game's mode lit (Buy: shop, shop-poor; Sell: shop-sell), the focus ring
    # (cell 1; the fourth owned place; Pay), the first cell's icon the game's own, Pay's word
    # white when the game would pay and dim when not (shop-poor: the XAM stereo, §3,000 > §2,000);
    # never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("panel band", C(180, 300), hexrgb("6171B2"))
    expect("title well", C(640, 50), hexrgb("6A72B8"))
    warm = sum(1 for y in range(54, 100, 2) for x in range(840, 1010, 2) if (lambda p: p[0] > 230 and p[1] > 200 and p[2] < 190)(C(x, y)))
    if warm < 20: errors.append(f"no money in the title ({warm} warm pixels)")
    white = lambda x0, y0, x1, y1: (255, 255, 255) in [C(x, y) for y in range(y0, y1) for x in range(x0, x1)]
    sell = case == "shop-sell"
    if white(210, 128, 526, 160) == sell: errors.append(f"the Buy header {'lit' if sell else 'dim'}")
    if white(888, 128, 1070, 160) != sell: errors.append(f"the Sell header {'dim' if sell else 'lit'}")
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    at = {"shop-sell": (981, 330), "shop-poor": (537, 514)}.get(case, (197, 224)) # (cell 1; owned place 4; Pay)
    if not ring(*at): errors.append(f"no focus ring at {at}")
    if case != "shop-sell" and case != "shop-poor" and ring(313, 224): errors.append("a focus ring around cell 2")
    cell = {C(x, y) for y in range(186, 266, 4) for x in range(210, 290, 4)}
    if len(cell) < 8: errors.append(f"cell 1 has no icon ({len(cell)} colours)")
    if white(630, 490, 840, 540) == (case == "shop-poor"): errors.append(f"Pay's word {'white' if case == 'shop-poor' else 'dim'}")
    not_big()
elif case in ("build-list", "build-list-2x", "build-list-wide", "build-ask", "build-poor", "build-empty"):
    # Build Rooms in HD (src/port/build.*): the panel, the title's well, the money in its warm
    # white; the list: the focus ring on the row given (row 2; build-poor row 1), a row's knob the
    # game's own sprite, the Spa & Gym's permit red (build-poor: §2,500 against §2,000) and no A
    # hint for it; the question: the tick focused, its sum (§2,000 - §1,000 = §1,000) in warm white;
    # the empty list (sit-build: the town's save lists nothing): no row; never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("panel band", C(180, 300), hexrgb("6171B2"))
    expect("title well", C(640, 50), hexrgb("6A72B8"))
    warm = lambda x0, y0, x1, y1: sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if (lambda p: p[0] > 230 and p[1] > 200 and p[2] < 190)(C(x, y)))
    if warm(840, 54, 1010, 100) < 20: errors.append("no money in the title")
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    red = lambda x0, y0, x1, y1: sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if (lambda p: p[0] > 200 and p[1] < 140 and p[2] < 140)(C(x, y)))
    if case == "build-ask":
        if not ring(678, 496): errors.append("no focus ring around the tick")
        if ring(378, 496): errors.append("a focus ring around the cross")
        if warm(440, 380, 840, 420) < 40: errors.append("no sum under the room")
        icon = {C(x, y) for y in range(476, 520, 3) for x in range(700, 745, 3)}
        if len(icon) < 6: errors.append(f"the tick has no sprite ({len(icon)} colours)")
    elif case == "build-empty":
        if ring(214, 206) or ring(214, 350): errors.append("a focus ring in the empty list")
        if warm(600, 140, 720, 560) > 0: errors.append("a price in the empty list")
    else:
        at = (214, 206) if case == "build-poor" else (214, 350)
        if not ring(*at): errors.append(f"no focus ring at {at}")
        knob = {C(x, y) for y in range(185, 225, 3) for x in range(232, 276, 3)}
        if len(knob) < 6: errors.append(f"row 1 has no knob ({len(knob)} colours)")
        if case == "build-poor":
            if red(600, 180, 720, 230) < 10: errors.append("the Spa & Gym's permit is not red")
        elif red(200, 124, 1080, 578) > 0: errors.append("a red permit in a list the money covers")
    not_big()
elif case in ("registry", "registry-2x", "registry-wide", "registry-guest"):
    # The Registry / a guest's check-in in HD (src/port/checkin.*): the panel, the title's well;
    # the focus ring around the first room; the Registry's score on its warm plate (none in the
    # check-in); M-ci-occ-reg2: the Small Guest Room's occupant (the concierge) the game's own row
    # button 2x in its row and 3x beside the list, the four other rooms' names pink (not built);
    # M-ci-g-base: the Small Guest Room's key (the game's sprite) and its name white (vacant);
    # never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("panel band", C(180, 300), hexrgb("6171B2"))
    expect("title well", C(640, 50), hexrgb("6A72B8"))
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    if not ring(212, 175): errors.append("no focus ring around the first room")
    if ring(212, 261): errors.append("a focus ring around the second room")
    warm = sum(1 for y in range(56, 98, 2) for x in range(700, 1000, 2) if (lambda p: p[0] > 230 and p[1] > 200 and p[2] < 190)(C(x, y)))
    if (warm >= 20) != (case != "registry-guest"): errors.append(f"the score plate ({warm} warm pixels)")
    pink = lambda y0: sum(1 for y in range(y0, y0 + 60, 2) for x in range(300, 640, 2) if C(x, y) == hexrgb("F35961"))
    if min(pink(230), pink(316), pink(402), pink(488)) < 10: errors.append("an unbuilt room's name not pink")
    white = sum(1 for y in range(140, 180, 2) for x in range(300, 640, 2) if C(x, y) == (255, 255, 255))
    if case == "registry-guest":
        if pink(144) > 0 or white < 20: errors.append(f"the vacant room's name not white ({white})")
        key = {C(x, y) for y in range(150, 205, 3) for x in range(228, 285, 3)}
        if len(key) < 6: errors.append(f"no key in the first row ({len(key)} colours)")
    else:
        if pink(144) < 10: errors.append("the occupied room's name not pink")
        piece("row portrait 2x", 8, 24, 28, 30, 228, 145, 2, step=3)
        piece("big portrait 3x", 8, 24, 28, 30, 906, 151, 3, step=3)
    not_big()
elif case in ("roominfo", "roominfo-vacant", "roominfo-hint"):
    # Room info in HD (src/port/roominfo.*): the panel; the portrait the game's own (the concierge:
    # occupying, or vacant) 3x; the focus ring around the guest's line; the values in warm white;
    # the hint (M-ci-occ-info, Down, Down, A: the check-out's 2603): the popups' veil and box over
    # it, its Close button (the picture's focus is pinned to none), the portrait under the box
    # gone; never the touch screen 3x.
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    if case == "roominfo-hint":
        expect("box band", C(304, 400), hexrgb("6171B2"))
        expect("the Close button", C(500, 470), hexrgb("6A72B8"))
        if C(330, 300) == bottom(172 + (330 - 223) // 3, 36 + (300 - 145) // 3): errors.append("the portrait drawn under the box")
        text = sum(1 for y in range(232, 380, 2) for x in range(340, 940, 2) if C(x, y) == (255, 255, 255))
        if text < 100: errors.append(f"no hint text ({text} white pixels)")
    else:
        expect("panel outline", C(176, 300), OUTLINE)
        expect("title well", C(640, 50), hexrgb("6A72B8"))
        piece("portrait 3x", 172, 36, 70, 86, 223, 145, 3, step=6)
        if not ring(484, 163): errors.append("no focus ring around the guest's line")
        if ring(484, 225): errors.append("a focus ring around the second line")
        warm = sum(1 for y in range(140, 570, 2) for x in range(880, 1060, 2) if (lambda p: p[0] > 230 and p[1] > 200 and p[2] < 190)(C(x, y)))
        if warm < (200 if case == "roominfo" else 100): errors.append(f"too few values ({warm} warm pixels)")
    not_big()
elif case in ("music", "music-2x", "music-wide", "music-scrolled"):
    # The music panel in HD (src/port/music.*): the panel, the title's well; M-music: five rows,
    # the third (Strange Day, playing) with the game's button green (frame 4) and the focus ring,
    # the others blue, the song playing's button 3x beside the list; M-mu-down3 (scrolled by
    # three): no green button in the rows (the song playing is off the list's view) and the ring
    # around the first; never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("title well", C(640, 50), hexrgb("6A72B8"))
    green = lambda x0, y0, w, h: sum(1 for y in range(y0, y0 + h, 2) for x in range(x0, x0 + w, 2) if (lambda p: p[1] > p[0] + 60 and p[1] > p[2] + 100)(C(x, y)))
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    rows = [green(312, 143 + 86 * i, 64, 64) for i in range(5)]
    playing = 2 if case != "music-scrolled" else None
    for i, n in enumerate(rows):
        if (n >= 20) != (i == playing): errors.append(f"row {i}'s button green pixels {n}")
    focus = 2 if case != "music-scrolled" else 0
    if not ring(301, 136 + 86 * focus + 39): errors.append(f"no focus ring around row {focus}")
    if ring(301, 136 + 86 * ((focus + 1) % 5) + 39): errors.append("a focus ring around another row")
    if green(892, 148, 112, 112) < (100 if playing is not None else 0): errors.append("no green button beside the list")
    blue = {C(x, y) for y in range(150, 200, 3) for x in range(315, 375, 3)}
    if len(blue) < 4: errors.append(f"no button in the first row ({len(blue)} colours)")
    not_big()
elif case.startswith("stat-"):
    # The stat panel in HD (src/port/statpanel.*): the panel, the title's well, the tab bar's five
    # pictures; Main: four buttons, the ring around Aspirations; a page: the ring around its tab,
    # its content: Aspirations' gauge green (level 2's palette), Hotel Info's and Game Stats'
    # values in warm white, Skills' star bars (2, 2, 1, 1, 6 filled); never the touch screen 3x.
    expect("panel outline", C(176, 300), OUTLINE)
    expect("title well", C(640, 146), hexrgb("6A72B8"))
    ring = lambda x, y: (lambda p: p[1] > p[0] + 80 and p[1] > p[2] + 60)(C(x, y))
    colours = lambda x0, y0, w, h: len({C(x, y) for y in range(y0, y0 + h, 3) for x in range(x0, x0 + w, 3)})
    for i in range(5):
        if colours(420 + 88 * i + 12, 52, 64, 64) < 4: errors.append(f"no picture in tab {i}")
    page = {"stat-asp": 1, "stat-hotel": 2, "stat-skills": 3, "stat-stats": 4}.get(case, 0)
    warm = lambda x0, y0, w, h: sum(1 for y in range(y0, y0 + h, 2) for x in range(x0, x0 + w, 2) if (lambda p: p[0] > 230 and p[1] > 200 and p[2] < 190)(C(x, y)))
    if page == 0:
        if not ring(209, 298): errors.append("no focus ring around Aspirations")
        if ring(641, 298) or ring(209, 486): errors.append("a focus ring around another button")
        for x, y in ((212, 210), (644, 210), (212, 398), (644, 398)):
            if colours(x + 140, y + 10, 144, 108) < 6: errors.append(f"no picture on the button at {x},{y}")
    else:
        cell = 420 + 88 * page
        if not ring(cell, 86): errors.append(f"no focus ring around tab {page}")
        if ring(209, 298): errors.append("Main's ring on a page")
        if page == 1:
            g = sum(1 for y in range(296, 334, 2) for x in range(380, 800, 2) if (lambda p: p[1] > p[0] + 60 and p[1] > p[2] + 60)(C(x, y)))
            if g < 1000: errors.append(f"the gauge not green ({g})")
        elif page in (2, 4):
            if warm(880, 214, 180, 360) < 150: errors.append("too few values")
        else:
            lit = [colours(728, 214 + 72 * i + 16, 320, 32) for i in range(5)]
            if min(lit) < 4: errors.append(f"a star bar missing {lit}")
    not_big()
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
elif case in ("s5-hd-75", "s5-hd-150", "casino-menu-75", "casino-menu-150"):
    # The UI's size: the same picture as at 100 % (the case without the suffix, run beside it), its
    # HD group zoomed about its anchor (the conversation about the canvas's bottom centre, 75 % or as
    # wide as the canvas holds, 120 %; the menu about 640,338, 75 % or never larger), the world
    # unchanged where the group no longer lies; the strip's status at its own size (75 %, or the
    # strip's 108 %) at the strip's left.
    from PIL import ImageStat
    base_name = case.rsplit("-", 1)[0]
    base = Image.open(comp_path.replace(f"/{case}/", f"/{base_name}/")).convert("RGB")
    z = 0.75 if case.endswith("-75") else 1.0
    dialogue = case.startswith("s5")
    if dialogue and case.endswith("-150"): z = 636 / 530
    ax, ay = (640, 720) if dialogue else (640, 338)
    box = (110, 426, 1170, 706) if dialogue else (176, 30, 1104, 646)
    T = lambda x, y: (ax + z * (x - ax), ay + z * (y - ay))
    if z == 1:
        # (the menu at 150 %: as at 100 %, the strip apart)
        diff = sum(1 for y in range(0, 655, 3) for x in range(0, 1280, 3) if C(x, y) != base.getpixel((x, y)))
        if diff: errors.append(f"the menu at 150 % differs from 100 % at {diff} samples")
    else:
        # The group: the 100 % picture's box made z times smaller / larger, nearly the same pixels.
        x0, y0 = T(box[0], box[1])
        x1, y1 = T(box[2], box[3])
        want = base.crop(box).resize((round(x1 - x0), round(y1 - y0)), Image.BILINEAR)
        got = comp.crop((round(x0), round(y0), round(x0) + want.size[0], round(y0) + want.size[1]))
        mean = sum(ImageStat.Stat(ImageChops.difference(want, got)).mean) / 3
        if mean > 14: errors.append(f"the zoomed group differs from 100 % (mean {mean:.1f})")
        if z < 1:
            # Where the group was and no longer is: the world (the conversation's) or the dimmed world (the menu's).
            for px, py in ((box[0] + 70, (box[1] + box[3]) // 2), (box[2] - 70, box[1] + 12)):
                if dialogue:
                    expect(f"world at {px},{py}", C(px, py), world_full(176)(px, py))
                elif not all(v < 80 for v in C(px, py)):
                    errors.append(f"no dimmed world at {px},{py}: {C(px, py)}")
    if not dialogue:
        # The strip: the status's white words end further left at 75 %, further right at 150 %.
        def right_end(im):
            xs = [x for y in range(668, 716, 2) for x in range(0, 900) if min(im.getpixel((x, y))) > 235]
            return max(xs) if xs else 0
        a, b = right_end(base), right_end(comp)
        if not (b < a - 20 if case.endswith("-75") else b > a + 10):
            errors.append(f"the status's words end at {b} (at 100 %: {a})")
elif case in ("pockets-smooth", "shop-smooth", "shop-smooth-2x"):
    # Smooth game pictures: the still items as with Pixel (the case without "-smooth"), the icons
    # enlarged by the pixel-art scaler: blended edges (more colours than Pixel's), close to Pixel's.
    from PIL import ImageStat
    base_name = case.replace("-smooth", "-menu" if case.startswith("pockets") else "")
    base = Image.open(comp_path.replace(f"/{case}/", f"/{base_name}/")).convert("RGB")
    if case.endswith("-2x"): base = base.resize((1280, 720), Image.NEAREST)
    icon = (215, 215, 290, 290) if case.startswith("pockets") else (318, 190, 395, 262)
    still = (692, 222, 900, 258) if case.startswith("pockets") else (220, 60, 440, 95)
    a, b = base.crop(icon), comp.crop(icon)
    ca, cb = len(a.getcolors(1 << 20)), len(b.getcolors(1 << 20))
    if cb <= ca: errors.append(f"the icon is not smoothed ({cb} colours, Pixel {ca})")
    mean = sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3
    if not 0 < mean < 30: errors.append(f"the smooth icon is not close to Pixel's (mean {mean:.1f})")
    if ImageChops.difference(base.crop(still), comp.crop(still)).getbbox():
        errors.append("the still items differ from Pixel's")
elif case == "social":
    activity(176, skip=16)
elif case in ("vacuum", "vacuum-2x", "vacuum-clog", "autopsy", "autopsy-2x", "detector", "art", "art-ask",
              "moogoo", "moogoo-2x", "drencher", "crime", "scratch", "mixer", "mixer-save", "trading"):
    activity(176)
elif case in ("cards", "cards-ask", "cards-m1"):
    # The card game has no HUD: its world whole, no status bar.
    expect("canvas", comp.size, (1280, 720))
    activity(hd=case != "cards-m1")
elif case in ("cards-wide", "social-wide", "detector-wide", "drencher-wide", "mixer-wide"):
    # The widescreen top screen: the secondary panel shorter, the side panel under it.
    expect("background", C(10, 10), PANEL)
    piece("touch screen 3x", 0, 0, 256, 192, 36, 68, 3, step=9)
    expect("world frame", C(855, 100), OUTLINE)
    minigame_frame(round(384 * (192 if case == "cards-wide" else 160 if case == "social-wide" else 176) / 341) + (18 if case == "cards-wide" else 72))
elif case == "s2":
    front_menu(0)
elif case in ("menu-2x", "menu-wide", "menu-de"):
    front_menu({"menu-2x": 1, "menu-wide": 0, "menu-de": 3}[case], raw_top=case == "menu-2x")
elif case in ("settings", "settings-2x", "settings-wide"):
    front_settings(0, 0)
elif case in ("settings-concept", "settings-fr"):
    front_settings(2, 3)
elif case == "settings-credits":
    front_settings(3, -1)
elif case in ("loadgame", "loadgame-wide"):
    front_load(0, (1, 1, 1))
elif case == "loadgame-2x":
    front_load(1, (1, 1, 1))
elif case == "loadgame-empty":
    front_load(-1, (0, 0, 0))
elif case == "loadgame-loading":
    front_load(-1, (1, 1, 1), loading=True)
elif case in ("title", "title-wide"):
    front_title(raw_top=case == "title")
elif case in ("boot", "boot-wide"):
    front_boot(raw=case == "boot")
elif case == "s3-m1":
    front()
elif case in ("s3", "s3-2x", "s3-wide"):
    front_cas(0, (731, 352, 50, 50), raw_top=case != "s3-wide")
elif case == "s3-res2":
    # --res3d 2: the top picture is 512 x 384; the game's "L" / "R" pills (DS 29-56 / 200-227,
    # 144-156) must still be under the pad's chips' dark plates (#5961A2), at the DS's places.
    front_cas(0, (731, 352, 50, 50), raw_top=False)
    k = 664 / 256
    for name, dx in (("L", 31), ("R", 202)):
        expect(f"turn pill {name} covered", C(16 + int(dx * k), 87 + int(150 * k)), hexrgb("5961A2"))
        expect(f"turn pill {name} covered (its end)", C(16 + int((dx + 23) * k), 87 + int(146 * k)), hexrgb("5961A2"))
elif case == "s4":
    front_cas(4, (746, 186, 108, 112))
elif case == "s4-it":
    front_cas(4, (756, 544, 210, 88))
elif case == "cas-hair":
    front_cas(1, (822, 441, 150, 171))
elif case == "cas-face":
    front_cas(2, (909, 331, 141, 129))
elif case == "cas-clothes":
    front_cas(3, (1084, 384, 120, 174))
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
elif case.startswith("prompt-"):
    # The world's prompt: a kit plate (the panel's fill) centred at the world's foot, it and its
    # shadow above the strip (row 661), white words in it; none without a target or on another tab.
    # -75 / -150: the UI's size (its own group, zoomed about its bottom centre, x 640 row 640).
    z = 1.5 if case.endswith("-150") else 0.75 if case.endswith("-75") else 1.0
    cols = [x for x in range(0, 1280) if any(C(x, y) == FRAME for y in range(644 - int(88 * z), 640, 6))]
    if case.startswith(("prompt-store", "prompt-concierge")):
        if not cols:
            errors.append("no prompt plate")
        else:
            lo, hi = min(cols), max(cols)
            if abs((lo + hi) / 2 - 640) > 12: errors.append(f"the plate not centred: {lo}..{hi}")
            if lo < 4 or hi > 1275: errors.append(f"the plate outside the canvas: {lo}..{hi}")
            if case.startswith("prompt-store") and not 120 * z < hi - lo < 320 * z:
                errors.append(f"the store's plate (A Buy, the clerk) at {z}: {lo}..{hi}")
            if case == "prompt-concierge" and hi - lo < 340: errors.append(f"no hint beside the concierge's verb: {lo}..{hi}")
            if (255, 255, 255) not in [C(x, y) for y in range(640 - int(88 * z), 640) for x in range(lo, hi)]: errors.append("no verb")
            # Nothing of it from row 648 to the strip, its drop shadow (7 rows, zoomed) neither:
            # the world's own pixels there.
            w = world_full(176)
            for y in range(648 + (4 if z > 1 else 0), STRIP_Y):
                bad = [x for x in range(lo - 8, hi + 9, 4) if C(x, y) != w(x, y)]
                if bad:
                    errors.append(f"the plate or its shadow at {bad[0]},{y}: {C(bad[0], y)} (the world {w(bad[0], y)})")
                    break
    elif len(cols) > 20:
        errors.append(f"a prompt plate where none belongs ({len(cols)} columns)")
    # (below 100 % the strip's hints zoom about its right end: the BACK pill moves)
    strip(hints=z >= 1)
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
# (the frame of a log line, the first one matching)
first_frame() { grep -m 1 -E "$2" "$tmp/$1.log" | sed -n 's/^frame \([0-9]*\):.*/\1/p'; }
last_before() { awk -v F="$2" -v P="$3" '$1=="frame" && ($2+0)<=F && $0 ~ P {l=$0} END{print l}' "$tmp/$1.log"; }
# pad_late NAME STATE SCRIPT FRAMES: the same on the complete save (its clock).
pad_late() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game --print-town "${late[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
# pad_shop NAME STATE SCRIPT FRAMES: the catalogue's runs, on a copy of the complete save (the
# research's states are the town's: its clock), with the shops' records (--print-hotel).
pad_shop() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 file=$shots/$2.st
    [ -f "$file" ] || file=$more_shots/$2.st
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game --print-hotel "${town[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_shop pad-shop-buy sit-storebuy catalogue-buy 220 &
pad_shop pad-shop-sell BY-own4 catalogue-sell 120 &
pad_shop pad-shop-scroll sit-storebuy catalogue-scroll 240 &
pad_shop pad-shop-poor BY-dn2 catalogue-poor 140 &
wait
# pad_build NAME STATE SCRIPT FRAMES: Build Rooms' runs (the research's BD-* states: the town's
# save with game flag 19 patched in), on a copy of the complete save, with the hotel's records
# (--print-hotel) and the popups (--print-town).
pad_build() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 file=$shots/$2.st
    [ -f "$file" ] || file=$more_shots/$2.st
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game --print-hotel --print-town "${town[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
# pad_hotel NAME STATE SCRIPT FRAMES: the early hotel's states (its clock, a scratch save), the hotel logged.
pad_hotel() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 file=$shots/$2.st
    [ -f "$file" ] || file=$more_shots/$2.st
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game --print-hotel "${hotel[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_hotel pad-registry D-hotel-down registry-concierge 380 &
pad_hotel pad-roominfo M-ci-early registry-info 330 &
pad_hotel pad-checkin M-ci-g-base checkin-guest 260 &
# pad_late_hotel NAME STATE SCRIPT FRAMES: the complete save's hotel screens (its clock, a copy of
# the save), the hotel logged.
pad_late_hotel() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 file=$shots/$2.st
    [ -f "$file" ] || file=$more_shots/$2.st
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-game --print-hotel "${late[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_late_hotel pad-music M-music music-play 400 &
pad_late_hotel pad-stat SP-stat-base stat-pages 1450 &
pad_late_hotel pad-stat-quick SP-stat-base stat-quick 400 &
pad_build pad-build-scroll BD-list build-scroll 260 &
pad_build pad-build-cancel BD-list build-cancel 260 &
pad_build pad-build-buy BD-list build-buy 330 &
pad_build pad-build-double BD-list build-double 200 &
pad_build pad-build-poor BD-down3 build-poor 120 &
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
pad pad-phone s7 phone-pages 540 &
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
# The catalogue: a couch bought twice (§2,000 -> §1,850 -> §1,700, two couches owned, no third),
# a bed sold (§1,040 -> §1,120, five left), the list scrolled by the game's arrows and closed, a
# purchase the game refuses (nothing bought).
shop_buy() {
    in_order pad-shop-buy 'catalogue .*sel=0 .*money=2000 ' 'catalogue .*sel=1 .*money=2000 ' 'catalogue .*money=1850 .*owned=132:[^,]*$' \
        'catalogue .*money=1700 .*owned=132:[^,]*,132:[^,]*$' || return 1
    ! grep -q 'money=1550' "$tmp/pad-shop-buy.log" || { echo "  pad-shop-buy: a third couch"; return 1; }
}
shop_sell() { in_order pad-shop-sell 'catalogue .*sell=1 .*money=1040 ' 'catalogue .*money=1120 .*owned=(106:[^,]*,){4}106:[^,]*$'; }
shop_scroll() { in_order pad-shop-scroll 'catalogue .*scroll=0 ' 'catalogue .*scroll=3 ' 'catalogue .*scroll=6 ' 'catalogue .*scroll=3 ' 'game Panel/Interact'; }
shop_poor() {
    in_order pad-shop-poor 'catalogue .*sel=6 .*money=2000 ' 'console menu closed, focus 6' || return 1
    ! grep -E 'catalogue .*money=' "$tmp/pad-shop-poor.log" | grep -qv 'money=2000 ' || { echo "  pad-shop-poor: money changed"; return 1; }
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
# selected), MENU, A → the Super Drencher's game (overlay 18, Drencher: CONSOLE's Drencher); MENU, Left (slot 1), X →
# the game selects the slot (nothing thrown), X again → the bin empties the pockets; from
# A-gift2-base, Right, A → the bed given (the pockets keep the arcade), back to the panel; from
# sit-gift, B → the game's X, the panel.
drencher() { in_order pad-drencher 'console menu open, focus 7' 'game Drencher'; }
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
check_pad pad-shop-buy shop_buy
check_pad pad-shop-sell shop_sell
check_pad pad-shop-scroll shop_scroll
check_pad pad-shop-poor shop_poor
# Build Rooms: the list scrolled by the game's arrows past the rows (0, 1, 2, back to 1) and closed;
# the question opened for the Art Gallery and cancelled by B and by its cross (nothing bought);
# the Art Gallery bought (§2,000 -> §1,000, spent §1,000, its building timer set: bit 1 of
# `building`; the list rebuilt without it), then B: the panel and the game's popup 2628 in HD,
# closed by A; the Spa & Gym (§2,500 > §2,000) refused, the Jungle Guest Room's question opened.
build_scroll() { in_order pad-build-scroll 'build scroll=0 page=0 ' 'build scroll=1 page=0 ' 'build scroll=2 page=0 ' 'build scroll=1 page=0 ' 'game Panel/Interact'; }
build_cancel() {
    in_order pad-build-cancel 'build scroll=0 page=1 chosen=1 ' 'build scroll=0 page=0 ' 'build scroll=0 page=1 chosen=1 ' 'build scroll=0 page=0 ' 'game Panel/Interact' || return 1
    ! grep -q 'hotel .*money=1000 ' "$tmp/pad-build-cancel.log" || { echo "  pad-build-cancel: bought"; return 1; }
}
build_buy() {
    in_order pad-build-buy 'build .*page=1 chosen=1 money=2000 ' 'hotel .*building=0x2 .*money=1000 earned=0 spent=1000 ' \
        'build scroll=0 page=0 chosen=-1 money=1000 places=0:2000:70:poor,8:' 'game Panel/Interact' 'popup state=2 kind=0 .*text="Your build permit has been approved' \
        'popup none' || return 1
    ! grep -q 'money=0 ' "$tmp/pad-build-buy.log" || { echo "  pad-build-buy: bought twice"; return 1; }
}
build_poor() {
    in_order pad-build-poor 'build scroll=3 page=0 ' 'build scroll=3 page=1 chosen=4 ' || return 1
    ! grep -q 'chosen=3 ' "$tmp/pad-build-poor.log" || { echo "  pad-build-poor: the Spa & Gym's question"; return 1; }
}
check_pad pad-build-scroll build_scroll
check_pad pad-build-cancel build_cancel
check_pad pad-build-buy build_buy
# A, A five frames apart on the tick buys once (the question closes at once; a second press is no second spend).
build_double() {
    in_order pad-build-double 'build .*page=1 chosen=1 money=2000 ' 'hotel .*building=0x2 .*money=1000 earned=0 spent=1000 ' || return 1
    ! grep -Eq 'money=0 |spent=[2-9][0-9]{3} ' "$tmp/pad-build-double.log" || { echo "  pad-build-double: bought twice"; return 1; }
}
check_pad pad-build-double build_double
check_pad pad-build-poor build_poor
# The Registry and room info (src/port/checkin.*, roominfo.*), the early hotel: from the lobby,
# MENU, Down, A on the concierge's "Registry" → the game's Registry; A → the Small Guest Room's
# room info; B → the Registry, the focus back on the room; B → the panel. From M-ci-early: A, Down,
# A → the base value's hint (2597, from the stylus's point); A → closed, the focus back on the
# line; B, B → the panel. From M-ci-g-base (the research's patch: the concierge waiting as Sim 45),
# Down, A on the Deluxe Guest Room (not built) → nothing; Up, A → checked into the Small Guest
# Room (slot 0, the guests stayed 1), then the guest's speech; once.
registry() {
    in_order pad-registry 'game Panel/Interact' 'console menu open, focus 1' 'checkin mode=1 guest=-1 score=0 rows=29:ba:-1,8:--' \
        'console menu closed, focus 0' 'roominfo place=29 occupant=45 occupied=0 .* total=422 hint=0' 'console menu closed, focus 1' \
        'checkin mode=1 ' 'console menu closed, focus 0' 'game Panel/Interact'
}
roominfo_hints() {
    in_order pad-roominfo 'checkin mode=1 ' 'roominfo place=29 .*hint=0' 'console menu closed, focus 2' 'roominfo .*hint=1:2597' \
        'roominfo .*hint=0' 'console menu closed, focus 2' 'checkin mode=1 ' 'game Panel/Interact'
}
checkin_guest() {
    in_order pad-checkin 'checkin mode=0 guest=45 score=0 rows=29:ba:-1' 'console menu closed, focus 1' 'console menu closed, focus 0' \
        'hotel .*stayed=1 .*slots=29:45:[1-3],' 'checkin mode=0 guest=45 score=0 rows=29:b-:45' 'game Dialogue' || return 1
    [ "$(grep -c 'stayed=1 ' "$tmp/pad-checkin.log")" -ge 1 ] && ! grep -q 'stayed=2 ' "$tmp/pad-checkin.log" || { echo "  pad-checkin: checked in twice"; return 1; }
    ! grep -E 'slots=(8|15|18|20):45' "$tmp/pad-checkin.log" -q || { echo "  pad-checkin: an unbuilt room taken"; return 1; }
}
check_pad pad-registry registry
check_pad pad-roominfo roominfo_hints
check_pad pad-checkin checkin_guest
# The music panel and the stat panel (src/port/music.*, statpanel.*; the complete save, the
# research's states): from M-music, Up, A → Create-a-Sim plays and the casino keeps it (G+0x449C
# + 4 = 3), Down x4 → the game's scroll 1, A → Edge of Town (code 7), A on it again nothing, B →
# the panel; from SP-stat-base (the research's patched Music Panel: medium), A, RB, Right, LB,
# B, LB, B, B → the pages 1, 2 (by Main), 3, 2, Main (the focus back on Hotel Info), 4, Main (on
# Game Stats), the panel; A then RB during the fade (waited out) → 1, 2, RB → 3.
music_play() {
    in_order pad-music 'music scroll=0 playing=2 place=4 roomsong=42' 'songs=(42,){4}3,' 'music scroll=0 playing=1 place=4 roomsong=3' \
        'music scroll=1 playing=1 ' 'music scroll=1 playing=5 place=4 roomsong=7' 'game Panel/Interact' || return 1
    [ "$(grep -c 'music .*playing=' "$tmp/pad-music.log")" -eq 4 ] || { echo "  pad-music: a song changed more than asked"; return 1; }
}
stat_pages() {
    in_order pad-stat 'stat page=0 transition=0' 'stat page=1 transition=0' 'stat page=0 ' 'stat page=2 transition=0' 'stat page=0 ' \
        'stat page=3 transition=0' 'stat page=0 ' 'stat page=2 transition=0' 'stat page=0 transition=1' 'focus 1' 'stat page=4 transition=0' \
        'stat page=0 transition=1' 'focus 3' 'game Panel/Interact'
}
stat_quick() { in_order pad-stat-quick 'stat page=1 transition=1' 'stat page=0 ' 'stat page=2 transition=0' 'stat page=0 ' 'stat page=3 transition=0'; }
check_pad pad-music music_play
check_pad pad-stat stat_pages
check_pad pad-stat-quick stat_quick
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
    in_order pad-phone 'phone page=0 row=1' 'phone page=5' 'phone page=7 .*list=22' 'phone page=9 .*caller=22' 'phone page=9 row=5' 'phone page=7' 'phone page=5' 'phone page=0' 'game Panel/Interact'
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
        --print-screen --print-game --print-minigame --clock "$clock" --input "$input" --shot-at "$at" --shot-dir "$tmp/$name" \
        > "$tmp/$name.log" 2>&1 && grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
act act-social act-social social 50 34,36,38,40,42,44 "2026-01-01 12:00" &
act act-draw act-cards cards-draw 200 10,199 "2026-11-05 12:05" &
act act-discard act-cards cards-discard 300 10,90,299 "2026-11-05 12:05" &
act act-quit act-cards cards-quit 300 10 "2026-11-05 12:05" &
wait
act act-cancel act-cards cards-cancel 300 10,299 "2026-11-05 12:05" &
act act-blow act-vacuum-clog vacuum-blow 60 9,40 "2026-01-01 14:00" &
act act-continue act-cards cards-continue 300 10 "2026-11-05 12:05" &
wait
# pixels NAME CASE: the Python checks below on NAME's shots.
pixels() {
    "$python" -I - "$tmp/$1" "$2" <<'EOF'
import sys
from PIL import Image, ImageChops

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
# The cards' end of turn (RAM: GameState::minigame.cards): the game's message up, then A goes on,
# and the opponent plays (their hand changes); the vacuum's clog read from RAM and gone after the blow.
continued() {
    in_order act-continue 'minigame cards .*message=0' 'minigame cards .*message=1' 'minigame cards .*message=0' 'minigame cards .*opponent=9 '
}
check_pad act-continue continued
unclogged() { in_order act-blow 'minigame vacuum clogged=1' 'minigame vacuum clogged=0'; }
check_pad act-blow unclogged
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
from PIL import Image, ImageChops

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
from PIL import Image, ImageChops

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
# The front end (src/port/front.*, --print-front): from F-title0, A → the main menu, A on
# Create-a-Sim once it is live → Create-a-Sim; Start → the menu, A twice while it fades in →
# nothing chosen (no press lands mid-fade); from s2, A on Load-a-Sim and Settings → each screen,
# and B (the game's own) → the title at phase 0, never the menu; A on DS Wireless (greyed: a
# second DS) → nothing, still the menu; from F-boot-900, A
# or Start (the splash's touch) → the title.
# (Settings, Load-a-Sim and Wireless have no GScreen: Unknown in RAM, None in the pixels, which
# PixelsAgree takes as one: --print-game logs no "pixels disagree" there.)
pad_front() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4; shift 4
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" \
        --layout console --print-screen --print-front "$@" "${town[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_front pad-front-title F-title0 front-title 300 --print-game &
pad_front pad-front-fade F-title0 front-fade 200 --print-game &
pad_front pad-front-load s2 front-load 330 --print-game &
pad_front pad-front-settings s2 front-settings 330 --print-game &
pad_front pad-front-wireless s2 front-wireless 330 --print-game &
pad_front pad-front-boot-a F-boot-900 front-boot-a 150 --print-game &
pad_front pad-front-boot-start F-boot-900 front-boot-start 150 --print-game &
wait
front_title() { in_order pad-front-title 'title phase=0' 'title phase=3' 'game CreateASim' 'front CreateASim'; }
front_fade() {
    in_order pad-front-fade 'title phase=0' 'title phase=2' 'title phase=3' || return 1
    ! grep -q -E 'game CreateASim|front (Load|Settings|Wireless)' "$tmp/pad-front-fade.log" || { echo "  pad-front-fade: a press landed mid-fade"; return 1; }
}
front_back() { in_order "$1" 'title phase=3' "front $2" 'front Title' 'title phase=0'; }
front_boot() { in_order "$1" 'front Video' 'boot mode=0' 'front Title' 'title phase=0'; }
check_pad pad-front-title front_title
check_pad pad-front-fade front_fade
check_pad pad-front-load front_back pad-front-load Load
check_pad pad-front-settings front_back pad-front-settings Settings
front_wireless() {
    in_order pad-front-wireless 'title phase=3' || return 1
    ! grep -q -E 'front Wireless' "$tmp/pad-front-wireless.log" || { echo "  pad-front-wireless: DS Wireless opened"; return 1; }
}
check_pad pad-front-wireless front_wireless
check_pad pad-front-boot-a front_boot pad-front-boot-a
check_pad pad-front-boot-start front_boot pad-front-boot-start
# Settings and Load-a-Sim (src/port/frontmenus.*, --print-front, copies of the complete save):
# from F-mm-opt-150, Left, Left, Right → the game's music 90, 80, 90 % read back from RAM, RB →
# the SFX page, Left → SFX 90 %, Right → SFX 100 % (the top end), B → the title at phase 0; from F-copt-concept-80, A → the unlocked
# picture on the top screen (art=1), Left, A on a locked one → nothing, RB → Credits, RB → Music
# (the ring), LB, LB → Concept Art, B → the title; from F-cload-150, Down, Up, A → slot 1 loaded:
# the game's "Loading" popup (HD), then the town (Panel/Interact, the arrival's Dialogue); a fresh
# boot (no state): Start and A during the splashes that take no touch (modes 3, 2, 1) are the
# DS's own and change nothing, A on "Touch the Touch Screen to continue." → the title.
pad_fm() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4; shift 4
    cp "$complete_save" "$tmp/$name.sav" || return
    local load=(--state-load "$shots/$state.st")
    [ "$state" = - ] && load=()
    "$bin" "$rom" --headless --frames "$frames" ${load[@]+"${load[@]}"} --save "$tmp/$name.sav" \
        --layout console --print-game --print-front --print-town "${late[@]}" --input "$input" "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_fm pad-settings-volume F-mm-opt-150 settings-volume 380 &
pad_fm pad-settings-pages F-copt-concept-80 settings-pages 400 &
pad_fm pad-load-slot F-cload-150 load-slot 360 &
pad_fm pad-boot-keys - boot-keys 1000 &
wait
settings_volume() {
    # (the settings line last logged by each frame: 90 % = 3686 ± 32, 80 % = 3277 ± 32)
    awk -v F=55 '$1=="frame" && ($2+0)<=F && /settings page/{l=$0} END{print l}' "$tmp/pad-settings-volume.log" | grep -q 'page=0 music=3[67][0-9][0-9] sfx=4096' || { echo "  music not 90 % after Left"; return 1; }
    awk -v F=105 '$1=="frame" && ($2+0)<=F && /settings page/{l=$0} END{print l}' "$tmp/pad-settings-volume.log" | grep -q 'page=0 music=3[23][0-9][0-9] sfx=4096' || { echo "  music not 80 % after Left"; return 1; }
    awk -v F=155 '$1=="frame" && ($2+0)<=F && /settings page/{l=$0} END{print l}' "$tmp/pad-settings-volume.log" | grep -q 'page=0 music=3[67][0-9][0-9] sfx=4096' || { echo "  music not 90 % after Right"; return 1; }
    awk -v F=255 '$1=="frame" && ($2+0)<=F && /settings page/{l=$0} END{print l}' "$tmp/pad-settings-volume.log" | grep -q 'page=1 music=3[67][0-9][0-9] sfx=3[67][0-9][0-9]' || { echo "  SFX not 90 % on its page"; return 1; }
    awk -v F=305 '$1=="frame" && ($2+0)<=F && /settings page/{l=$0} END{print l}' "$tmp/pad-settings-volume.log" | grep -q 'page=1 music=3[67][0-9][0-9] sfx=4096' || { echo "  SFX not 100 % after Right"; return 1; }
    in_order pad-settings-volume 'settings page=1' 'front Title' 'title phase=0'
}
settings_pages() {
    in_order pad-settings-pages 'settings page=2 .*art=0 concept=8' 'settings page=2 .*art=1 concept=8' 'settings page=3' 'settings page=0' 'settings page=3' 'settings page=2' 'front Title' 'title phase=0'
}
load_slot() {
    in_order pad-load-slot 'front Load' 'popup state=[1-3] kind=3 .*title="Loading"' 'front None' 'game Panel/Interact' 'game Dialogue'
}
boot_keys() {
    in_order pad-boot-keys 'boot mode=3' 'boot mode=2' 'boot mode=1' 'boot mode=0' 'front Title' 'title phase=0' || return 1
    ! grep -q -E 'front (Settings|Load|Wireless|CreateASim)|game CreateASim' "$tmp/pad-boot-keys.log" || { echo "  pad-boot-keys: a key went past the title"; return 1; }
}
check_pad pad-settings-volume settings_volume
check_pad pad-settings-pages settings_pages
check_pad pad-load-slot load_slot
check_pad pad-boot-keys boot_keys
# Create-a-Sim (src/port/cas.*, --print-front): from s3, Y, A, X, Right, A, X, A x5, Start → the
# name typed through the three keyboards, the backspace, never past seven, then committed to G+0x53
# when the page closes; from F-cas-kbs4-140, Y at empty → nothing, A → "A"; from s3, the gender,
# the skin, the hair colour and style and the game's scroll both ways, a face, a clothes cell and
# the next page by its arrow; from s4 on a copy of the complete save, Popularity, the tick → the
# game starts (the town); B → the cross: the title at phase 0; the right stick turns the model
# (the model's picture differs from the same run without it).
pad_cas() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4; shift 4
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" \
        --layout console --print-game --print-front "${town[@]}" --input "$input" "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_cas pad-cas-name s3 cas-name 260 &
pad_cas pad-cas-empty F-cas-kbs4-140 cas-empty 90 &
pad_cas pad-cas-looks s3 cas-looks 640 &
pad_cas pad-cas-keep s4 cas-keep 400 &
wait
pad_cas pad-cas-cross s4 cas-cross 200 &
pad_cas pad-cas-type s3 cas-type 260 &
pad_cas pad-cas-turn s3 cas-turn 50 --compose --shot-at 49 --shot-dir "$tmp/pad-cas-turn" &
pad_cas pad-cas-still s3 cas-cross 50 --compose --shot-at 49 --shot-dir "$tmp/pad-cas-still" &
wait
cas_name() {
    in_order pad-cas-name 'name=Si len=2 keyboard=0' 'name=SiA len=3 keyboard=0' 'name=SiA len=3 keyboard=1' 'name=SiAb len=4' 'name=SiAb len=4 keyboard=2' \
        'name=SiAb111 len=7' 'cas page=1 .* switching' 'cas page=1 gender=0 simname=SiAb111 ' || return 1
    ! grep -q -E 'len=[89]' "$tmp/pad-cas-name.log" || { echo "  pad-cas-name: past seven"; return 1; }
}
cas_empty() { in_order pad-cas-empty 'name= len=0' 'name=A len=1'; }
# The name typed on the keyboard (Input::TypeName, scripts/console/cas-type.txt): the game's own keys
# and page labels tapped, never more than seven, committed by Start.
cas_type() {
    in_order pad-cas-type 'name=Si len=2 keyboard=0' 'name=Si len=2 keyboard=1' 'name=Sia len=3' 'name=Sia len=3 keyboard=0' 'name=SiaB len=4' \
        'keyboard=2' 'name=SiaB1 len=5' 'name=SiaB1x len=6' 'name=SiaB1xy len=7' 'cas page=1 .* switching' 'simname=SiaB1xy ' || return 1
    ! grep -q -E 'len=[89]' "$tmp/pad-cas-type.log" || { echo "  pad-cas-type: past seven"; return 1; }
}
cas_looks() {
    in_order pad-cas-looks 'cas page=0 gender=1 ' 'gender=1 .*skin=1 ' 'cas page=1 ' 'haircolour=4 hair=0 ' 'haircolour=4 hair=1 scroll=0' 'scroll=1' 'scroll=2' 'scroll=3' \
        'scroll=2' 'scroll=1' 'scroll=0' 'cas page=2 ' 'face=4' 'cas page=3 ' 'item=15' 'clothespage=1 item=15'
}
cas_keep() { in_order pad-cas-keep 'aspiration=0 answer=0' 'aspiration=2 answer=0' 'aspiration=2 answer=1' 'game Panel/Interact'; }
# (one B only puts the focus on the cross: the answer comes from the second, at frame 60)
cas_cross() {
    in_order pad-cas-cross 'aspiration=0 answer=2' 'front Title' 'title phase=0' || return 1
    local f; f=$(first_frame pad-cas-cross 'answer=2')
    [ -n "$f" ] && [ "$f" -ge 60 ] || { echo "  pad-cas-cross: one B discarded the Sim (frame $f)"; return 1; }
}
cas_turn() {
    "$python" -I -c '
import sys
from PIL import Image, ImageChops
a, b = (Image.open(p).convert("RGB").crop((16, 87, 680, 585)) for p in sys.argv[1:3])
sys.exit(0 if ImageChops.difference(a, b).getbbox() else 1)' "$tmp/pad-cas-turn/f000049.png" "$tmp/pad-cas-still/f000049.png"
}
check_pad pad-cas-name cas_name
check_pad pad-cas-empty cas_empty
check_pad pad-cas-type cas_type
check_pad pad-cas-looks cas_looks
check_pad pad-cas-keep cas_keep
check_pad pad-cas-cross cas_cross
check_pad pad-cas-turn cas_turn
# The HD screens hold through the game's swaps (src/consolehold.*, --print-console: each frame's
# composition, milestone 1's marked -m1, and the counts at the end): Create-a-Sim's pages from s3 by
# RB, LB and Start (every page change the game makes), the front end (title → menu → Create-a-Sim,
# Load-a-Sim and Settings and back), the panel's tabs, the phone, Give Gift closing after a gift, the
# Registry ↔ room info, a save loaded into the town: no frame shows milestone 1's picture (a frame of
# the game's own between two objects shows the last HD picture, 12 frames at most, taking no input);
# the tabs, the phone and the Registry's swaps are held (the hold is exercised).
hold_walk() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 save=$5; shift 5
    rm -f "$tmp/$name.sav"
    [ "$save" = complete ] && { cp "$complete_save" "$tmp/$name.sav" || return; }
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" \
        --layout console --print-console --print-front --print-game "$@" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
hold_walk hold-cas s3 cas-walk 640 complete "${town[@]}" &
hold_walk hold-title F-title0 front-title 300 fresh "${town[@]}" &
hold_walk hold-load s2 front-load 330 fresh "${town[@]}" &
hold_walk hold-settings s2 front-settings 330 fresh "${town[@]}" &
wait
hold_walk hold-tabs s7 tabs 320 fresh "${town[@]}" &
hold_walk hold-phone s7 phone-pages 540 fresh "${town[@]}" &
hold_walk hold-gift A-gift2-base gift-give 200 complete "${late[@]}" &
hold_walk hold-registry D-hotel-down registry-concierge 380 fresh "${hotel[@]}" &
hold_walk hold-load-slot F-cload-150 load-slot 360 complete "${late[@]}" &
wait
# (no frame shows milestone 1's picture; held: at least that many frames held)
hold_none() {
    local name=$1 held=${2:-0} line
    line=$(grep '^console: ' "$tmp/$name.log") || { echo "  $name: no console summary"; return 1; }
    echo "$line" | grep -q ', 0 milestone-1 shown$' || { echo "  $name: $line"; grep -E 'console [a-z-]+-m1( |$)' "$tmp/$name.log" | grep -v ' held ' | head -n 3; return 1; }
    [ "$(echo "$line" | sed -n 's/.* \([0-9]*\) held,.*/\1/p')" -ge "$held" ] || { echo "  $name: fewer than $held held ($line)"; return 1; }
}
hold_cas() {
    in_order hold-cas 'cas page=1 ' 'cas page=2 ' 'cas page=3 ' 'cas page=4 ' 'cas page=3 ' 'cas page=2 ' 'cas page=1 ' 'cas page=0 ' \
        'cas page=1 ' 'cas page=2 ' 'cas page=3 ' 'cas page=4 ' || return 1
    hold_none hold-cas
}
hold_title() { in_order hold-title 'game CreateASim' 'console cas$' && hold_none hold-title; }
hold_load() { in_order hold-load 'front Load' 'front Title' && hold_none hold-load; }
hold_settings() { in_order hold-settings 'front Settings' 'front Title' && hold_none hold-settings; }
hold_tabs() { in_order hold-tabs 'game Panel/Pockets' 'game Panel/Goals' && hold_none hold-tabs 1; }
hold_phone() { hold_none hold-phone 1; }
hold_gift() { in_order hold-gift 'game Gift' 'game Panel/Interact' && hold_none hold-gift; }
hold_registry() { hold_none hold-registry 1; }
hold_load_slot() { in_order hold-load-slot 'front Load' 'game Panel/Interact' 'game Dialogue' && hold_none hold-load-slot; }
check_pad hold-cas hold_cas
check_pad hold-title hold_title
check_pad hold-load hold_load
check_pad hold-settings hold_settings
check_pad hold-tabs hold_tabs
check_pad hold-phone hold_phone
check_pad hold-gift hold_gift
check_pad hold-registry hold_registry
check_pad hold-load-slot hold_load_slot
# The world's camera on the right stick (navigator.h, WorldKeys; from s7, the phone closed): held
# left it turns (the DS's L), held up it zooms in (X): the world's picture differs from the same
# run without it; LB held (the shoulders' default L) changes nothing there any more.
pad_world() {
    local name=$1 input=$repo/scripts/console/$2.txt
    "$bin" "$rom" --headless --frames 260 --state-load "$shots/s7.st" --save "$tmp/$name.sav" \
        --layout console --print-game "${town[@]}" --input "$input" --compose --shot-at 259 --shot-dir "$tmp/$name" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_world pad-world-turn world-turn &
pad_world pad-world-zoom world-zoom &
pad_world pad-world-lb world-lb &
pad_world pad-world-still world-still &
wait
# world_differs NAME 0|1: the world (above the strip) differs from world-still's (1) or not (0).
world_differs() {
    "$python" -I -c '
import sys
from PIL import Image, ImageChops
a, b = (Image.open(p).convert("RGB").crop((0, 0, 1280, 640)) for p in sys.argv[1:3])
sys.exit(0 if bool(ImageChops.difference(a, b).getbbox()) == (sys.argv[3] == "1") else 1)' "$tmp/$1/f000259.png" "$tmp/pad-world-still/f000259.png" "$2"
}
check_pad pad-world-turn world_differs pad-world-turn 1
check_pad pad-world-zoom world_differs pad-world-zoom 1
check_pad pad-world-lb world_differs pad-world-lb 0
# The HD Interact tab's words in each language (src/port/interact.*: --measure-menu): every verb
# in its row and in the A hint, every Sim's and object's name in the title and the strip, held in
# their boxes (shrunk at most; a neighbour's name beside the plate may end in an ellipsis).
for lang in 1 2 3 4 5; do
    if out=$("$bin" "$rom" --language $lang --measure-menu 2> "$tmp/menu-$lang.log"); then echo "ok   menu-words-$lang: $out"
    else echo "FAIL menu-words-$lang: $out $(tail -n 1 "$tmp/menu-$lang.log")"; fails=$((fails + 1)); fi
done
# The pointing minigames on the controller (src/activities.*, the research's held-out states on
# copies of the complete save, --print-minigame: the game's own fields, and the pointer's presses):
# pad_mg NAME STATE SCRIPT FRAMES.
pad_mg() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" --layout console \
        --print-screen --print-game --print-town --print-minigame --print-hud "${late[@]}" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_mg mg-autopsy-bank G-A-autopsy-nopara autopsy-bank 1000 &
pad_mg mg-autopsy-quit G-X-autopsy autopsy-quit 150 &
pad_mg mg-autopsy-stay G-X-autopsy autopsy-quit-once 200 &
pad_mg mg-autopsy-payout G-A-payout-base autopsy-wait 120 &
wait
pad_mg mg-detector-dig G-A-md-walk detector-dig 220 &
pad_mg mg-detector-quit G-X-metal detector-quit 120 &
pad_mg mg-art-play G-X-artgal art-play 200 &
pad_mg mg-art-done G-X-artgal art-done 200 &
wait
# The autopsy: the research's laser cut frees organ 1; then Y (the hand), the D-pad's pointer onto it,
# A held and LB carry it to the tray: the hand drags it (sub 3), it goes in (4), §84 banked, the slot
# removed. B asks, B quits (the panel); B then A goes on (still the autopsy 140 frames later). The
# second specimen done (G-A-payout-base, §123 banked): the money paid, +123, and the panel.
autopsy_bank() {
    in_order mg-autopsy-bank 'minigame autopsy .*organs=0:[0-9]+:freed' 'minigame autopsy phase=1 .*tool=1 ' 'pointer down at 77,93' \
        'minigame autopsy .*sub=3 .*dragged=0' 'minigame autopsy .*sub=4 ' 'minigame autopsy .*earnings=84 .*organs=5:'
}
check_pad mg-autopsy-bank autopsy_bank
autopsy_quit() { in_order mg-autopsy-quit 'game Autopsy' 'game Panel/Interact'; }
check_pad mg-autopsy-quit autopsy_quit
autopsy_stay() {
    if grep -q 'game Panel' "$tmp/mg-autopsy-stay.log"; then echo "  mg-autopsy-stay: quit on one B"; return 1; fi
    in_order mg-autopsy-stay 'game Autopsy' 'frame 19[0-9]: minigame autopsy'
}
check_pad mg-autopsy-stay autopsy_stay
autopsy_payout() {
    local m0 m1
    m0=$(grep -m 1 -o 'hud money=[0-9]*' "$tmp/mg-autopsy-payout.log" | cut -d= -f2)
    m1=$(grep -o 'hud money=[0-9]*' "$tmp/mg-autopsy-payout.log" | tail -n 1 | cut -d= -f2)
    [ -n "$m0" ] && [ -n "$m1" ] && [ $((m1 - m0)) = 123 ] || { echo "  mg-autopsy-payout: money $m0 -> $m1 (want +123)"; return 1; }
    in_order mg-autopsy-payout 'minigame autopsy .*earnings=123' 'game Panel'
}
check_pad mg-autopsy-payout autopsy_payout
# The detector: RB holds the depth slider's knob from where RAM has it and moves it deeper (the
# game's value follows), A digs the find in range: the card shows it (find=0, a lead rock), the
# earnings 2020 -> 2045, a pocket filled (item 300), and the card goes by itself (state 5 lasts about
# 125 frames, then state 3, no card: no button is needed); B powers it off: the panel.
detector_dig() {
    in_order mg-detector-dig 'pointer down at 195,77' 'minigame detector .*slider=(3[5-9]|40)[0-9][0-9] ' 'pointer up' \
        'minigame detector state=5 .*card=1 find=0 .*earnings=2045' 'minigame detector state=3 .*card=0 find=-1 ' &&
        in_order mg-detector-dig 'inventory slots=171:[0-9/]+,300:'
}
check_pad mg-detector-dig detector_dig
detector_quit() { in_order mg-detector-quit 'game Detector' 'minigame detector state=4' 'game Panel/Interact'; }
check_pad mg-detector-quit detector_quit
# The painting: A held with Right drags a stroke (cells painted), Y undoes it (none), LB four times
# goes from the small pencil back round to the bucket (tool buttons 9, 8, 7, 6), A on the canvas fills
# all 1536 cells, X the next swatch; B asks "Are you done painting?" (the game's popup, kind 2), B its
# cross (closed, still painting), B and A its tick (closed: the research's patched easel has no
# controller to finish the painting, so the screen stays).
art_play() {
    in_order mg-art-play 'pointer down at 128,96' 'minigame art stroke=1 .*painted=[1-9]' 'pointer up' 'minigame art .*painted=0 ' \
        'minigame art .*button=9 ' 'minigame art .*button=8 ' 'minigame art .*button=7 ' 'minigame art .*button=6 tool=3' \
        'minigame art .*painted=1536 ' 'minigame art .*swatch=4 '
}
check_pad mg-art-play art_play
art_done() {
    in_order mg-art-done 'popup state=2 kind=2 .*done painting' 'popup state=2 kind=2 pressed=0,1' 'popup none' \
        'popup state=2 kind=2 .*done painting' 'popup state=2 kind=2 pressed=1,1' 'popup none'
}
check_pad mg-art-done art_done
# The timing minigames on the controller (the research's held-out states on copies of the complete
# save). MooGoo: from its intro, A every 30 frames (scripts/console/moogoo-round.txt) plays a whole
# round at the game's pace: the arrow, the players' prompt (one player), the hint and the banners,
# the player's opening bet (sub 2 on the player's turn: the focus's first monkey that takes one),
# the shuffle and deal, the player's cards picked (sub 4) and played, the AI's turns, a monkey out
# (the lowest card) and round 2 begun. A King Gorilla played on the pile the focus starts on (a
# monkey with cards: G-B-mg-r1-king2): its card destroyed, the player plays again (sub 3). B on the
# player's bet asks the game's question, A its Yes: the panel; B then B: the question closed, still
# MooGoo; B on the intro goes on to the game and asks there, A: the panel.
pad_mg mg-moogoo-round G-X-moogoo-intro moogoo-round 5500 &
pad_mg mg-moogoo-target G-B-mg-r1-king2 moogoo-target 300 &
pad_mg mg-moogoo-quit G-B-mg-r1-full moogoo-quit 120 &
pad_mg mg-moogoo-stay G-B-mg-r1-full moogoo-stay 160 &
wait
pad_mg mg-moogoo-intro-quit G-X-moogoo-intro moogoo-intro-quit 220 &
pad_mg mg-drencher G-C-wg-pump drencher-play 880 &
pad_mg mg-crime G-X-crime crime-throw 260 &
wait
moogoo_round() {
    in_order mg-moogoo-round 'minigame moogoo intro' 'minigame moogoo phase=0 ' 'minigame moogoo phase=6 sub=2 timer=-?[0-9]+ current=0 ' \
        'minigame moogoo phase=7 ' 'minigame moogoo phase=13 sub=4 timer=-?[0-9]+ current=0 ' 'minigame moogoo phase=13 sub=9 ' \
        'minigame moogoo phase=14 .*monkeys=[^ ]*out:' 'minigame moogoo phase=9 .*round=2 '
}
check_pad mg-moogoo-round moogoo_round
moogoo_target() {
    in_order mg-moogoo-target 'minigame moogoo phase=13 sub=5 .*monkeys=[^ ]*,33:' 'minigame moogoo phase=13 sub=6 ' \
        'minigame moogoo phase=13 sub=3 .*monkeys=-:001:1,-:010:1,-:200:2,-:000:0,-:000:0,'
}
check_pad mg-moogoo-target moogoo_target
moogoo_quit() { in_order mg-moogoo-quit 'game MooGoo' 'minigame moogoo .*quit=01 ' 'minigame moogoo .*quit=10 ' 'game Panel/Interact'; }
check_pad mg-moogoo-quit moogoo_quit
moogoo_stay() {
    if grep -q 'game Panel' "$tmp/mg-moogoo-stay.log"; then echo "  mg-moogoo-stay: quit on B, B"; return 1; fi
    in_order mg-moogoo-stay 'minigame moogoo .*quit=01 ' 'minigame moogoo phase=13 sub=1 .*quit=00 '
}
check_pad mg-moogoo-stay moogoo_stay
moogoo_intro_quit() { in_order mg-moogoo-intro-quit 'minigame moogoo intro' 'minigame moogoo phase=0 ' 'minigame moogoo .*quit=01 ' 'game Panel/Interact'; }
check_pad mg-moogoo-intro-quit moogoo_intro_quit
# The Drencher: A held fires (the pressure and the water go down: under 30 after 400 frames), X held
# pumps (the stylus swept along the band: back to 125 in under 150 frames), Y locks the nearest
# target (the casino's cards table, object 29), B puts it away: the panel.
drencher_play() {
    in_order mg-drencher 'minigame drencher .*firing=1 ' 'minigame drencher .*firing=0 .*pressure=2[0-9]$' 'pointer down at' \
        'minigame drencher .*pumping=1 .*pressure=125$' 'pointer up' 'minigame drencher .*target=29/11 ' 'game Panel/Interact' &&
        "$python" -I - "$tmp/mg-drencher.log" <<'EOF'
import re, sys
lines = [l for l in open(sys.argv[1]) if "minigame drencher" in l]
up = [int(re.match(r"frame (\d+)", l).group(1)) for l in lines if "pumping=1" in l]
full = [int(re.match(r"frame (\d+)", l).group(1)) for l in lines if "pumping=1" in l and l.rstrip().endswith("pressure=125")]
if not up or not full or full[0] - up[0] > 150:
    print(f"  mg-drencher: the pump took {full[0] - up[0] if up and full else '?'} frames to fill")
    sys.exit(1)
EOF
}
check_pad mg-drencher drencher_play
# The crime game: A grabs the centre rat (the stylus at 128,96: grabbed, then aiming), Right held
# turns the aim to +90 (1.125 degrees a pixel), letting A go throws (phase 3); A again, Left: the
# aim below 0; B puts it away: the panel. (No goon in any state: a hit is the game's own.)
crime_throw() {
    # (A frame's minigame line comes before its pointer line.)
    in_order mg-crime 'minigame crime .*phase=1 ' 'pointer down at 128,96' 'minigame crime .*phase=2 angle=90 ' 'minigame crime .*phase=3 ' \
        'pointer up at 207,96' 'minigame crime .*phase=1 ' 'pointer down at 128,96' 'minigame crime .*phase=2 angle=-[1-9]' 'pointer up' \
        'game Panel/Interact'
}
check_pad mg-crime crime_throw
# The Scratch lottery, the music mixer, its save page and the wireless lobby on the controller (the
# research's held-out states on copies of the complete save; G-T-trade the lobby, reached as the
# research reached the others: its request written into the casino state).
pad_mg mg-scratch-win G-B-sc-two-p10 scratch-win 560 &
pad_mg mg-scratch-quit G-X-scratch scratch-quit 150 &
pad_mg mg-scratch-stay G-X-scratch scratch-quit-once 300 &
pad_mg mg-mixer-record G-X-music mixer-record 700 &
wait
pad_mg mg-mixer-vox G-X-music mixer-vox 200 &
pad_mg mg-mixersave-back G-D-mus-save mixersave-back 160 &
pad_mg mg-trading-exit G-T-trade trading-exit 120 &
wait
# The lottery: the pointer taken to box 7 (the stick's D-pad), X rubs it from its top left corner
# (the stylus pressed at 200,96) until the game reveals it, the third 4: won; A closes the game's
# "Congratulations": the prize paid (§14,843 -> §14,853), the panel.
scratch_win() {
    in_order mg-scratch-win 'pointer down at 200,96' 'minigame scratch .*active=7 ' 'pointer up' 'minigame scratch .*state=2 revealed=3 ' \
        'hud money=14853 ' 'game Panel/Interact' && grep -Eq 'minigame scratch .*state=1 .*,4:6[89][0-9]:open$' "$tmp/mg-scratch-win.log"
}
check_pad mg-scratch-win scratch_win
# B on the ticket asks (the hint closed by A first), a second B taps its X: the panel; one B alone
# leaves the ticket up.
scratch_quit() { in_order mg-scratch-quit 'game Scratch' 'popup none' 'game Panel/Interact'; }
check_pad mg-scratch-quit scratch_quit
scratch_stay() {
    if grep -q 'game Panel' "$tmp/mg-scratch-stay.log"; then echo "  mg-scratch-stay: quit on one B"; return 1; fi
    in_order mg-scratch-stay 'minigame scratch .*state=1 ' 'popup none'
}
check_pad mg-scratch-stay scratch_stay
# The mixer: RB records (phase 2), the D key played with the pointer and A, RB stops: the save
# question (phase 10), A Yes: the save page; "AB" typed, saved into the second slot, back to the mixer.
mixer_record() {
    in_order mg-mixer-record 'minigame mixer .*phase=2 ' 'minigame mixer .*phase=2 .*keys=2,' 'pointer down at 73,176' 'minigame mixer .*phase=10 ' \
        'game MixerSave' 'minigame mixersave .*name="AB" slots=empty,empty,empty' 'minigame mixersave .*slots=empty,"AB",empty' 'game Mixer'
}
check_pad mg-mixer-record mixer_record
# Y held records the microphone (phase 5: the game reads it), its release stops it (the voice is
# instrument 5), B quits: the panel.
mixer_vox() {
    in_order mg-mixer-vox 'minigame mixer .*phase=5 ' 'minigame mixer .*phase=0 .*instrument=5 ' 'game Panel/Interact' &&
        grep -Eq 'mic: [1-9][0-9]* samples' "$tmp/mg-mixer-vox.log"
}
check_pad mg-mixer-vox mixer_vox
mixersave_back() { in_order mg-mixersave-back 'name="A"' 'name=""' 'game Mixer'; }
check_pad mg-mixersave-back mixersave_back
trading_exit() { in_order mg-trading-exit 'game Trading' 'game FrontEnd'; }
check_pad mg-trading-exit trading_exit
# Its scare and subdue buttons (X, Y): the game's own fail animation where the stylus touched (no
# goon near), against the same run without them.
for v in crime-buttons crime-none; do
    cp "$complete_save" "$tmp/$v.sav"
    "$bin" "$rom" --headless --frames 50 --state-load "$shots/G-X-crime.st" --save "$tmp/$v.sav" --layout console "${late[@]}" \
        --input "$repo/scripts/console/$v.txt" --shot-at 20 --shot-at 40 --shot-dir "$tmp/$v" > "$tmp/$v.log" 2>&1 &
done
wait
if "$python" -I - "$tmp/crime-buttons" "$tmp/crime-none" <<'EOF'
import sys
from PIL import Image, ImageChops
a, b = sys.argv[1:3]
for f, (y0, y1) in (("f000020.png", (13, 56)), ("f000040.png", (135, 178))):
    box = ImageChops.difference(Image.open(f"{a}/{f}").convert("RGB"), Image.open(f"{b}/{f}").convert("RGB")).getbbox()
    if not box or box[1] > 192 + y1 or box[3] < 192 + y0 or box[0] > 161 or box[2] < 94:
        print(f"  {f}: no change on the button ({box})")
        sys.exit(1)
EOF
then echo "ok   mg-crime-buttons"; else echo "FAIL mg-crime-buttons"; fails=$((fails + 1)); fi
# No dead ends (the controller alone leaves every screen; copies of the complete save):
# pad_t1 NAME STATE SCRIPT FRAMES CLOCK. The vacuum's B taps the bowl's X (the panel); the
# painting's MENU its palette's next page (1, 2: all 240 colours), Start its X (the question); the
# Drencher's, the crime game's and the mixer's Start their "?" (help mode), a button its control's
# text, any press closing it, B leaving help; the furniture editor's B its X (the panel); the boot's
# last splash takes Start and A at frames where a 3-frame touch fell between its slow polls.
pad_t1() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 clock=$5
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" --layout console \
        --print-game --print-town --print-minigame --print-front --print-hotel --clock "$clock" --input "$input" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_t1 mg-vacuum-leave act-vacuum vacuum-leave 120 "2026-01-01 14:00" &
pad_t1 mg-art-palette G-X-artgal art-palette 200 "2026-11-05 12:05" &
pad_t1 mg-drencher-help G-C-wg-pump drencher-help 240 "2026-11-05 12:05" &
pad_t1 mg-crime-help G-X-crime crime-help 240 "2026-11-05 12:05" &
wait
pad_t1 mg-mixer-help G-X-music mixer-help 280 "2026-11-05 12:05" &
pad_t1 pad-placement-leave BD-place-lobby placement-leave 120 "2026-11-05 12:05" &
pad_t1 pad-boot-late-start F-boot-900 boot-late-start 220 "2026-11-05 12:05" &
pad_t1 pad-boot-late-a F-boot-900 boot-late-a 220 "2026-11-05 12:05" &
wait
vacuum_leave() { in_order mg-vacuum-leave 'game Vacuum' 'minigame none' 'game Panel/Interact'; }
check_pad mg-vacuum-leave vacuum_leave
art_palette() {
    in_order mg-art-palette 'minigame art .*swatch=3 page=0 ' 'minigame art .*page=1 ' 'minigame art .*page=2 ' 'minigame art .*swatch=4 page=2 ' \
        'popup state=2 kind=2 .*done painting'
}
check_pad mg-art-palette art_palette
drencher_help() {
    in_order mg-drencher-help 'minigame drencher .*help=00' 'minigame drencher .*help=10' 'minigame drencher .*help=11' 'minigame drencher .*help=00' \
        'minigame drencher .*help=10' 'minigame drencher .*help=00'
}
check_pad mg-drencher-help drencher_help
crime_help() {
    in_order mg-crime-help 'crime help=0 text=0 ' 'crime help=1 text=0 ' 'crime help=1 text=1 ' 'crime help=0 text=0 ' 'crime help=1 text=0 ' \
        'crime help=0 text=0 ' && ! grep -q 'game Panel' "$tmp/mg-crime-help.log"
}
check_pad mg-crime-help crime_help
mixer_help() {
    in_order mg-mixer-help 'minigame mixer .* phase=0 ' 'minigame mixer .* phase=8 ' 'minigame mixer .* phase=9 ' 'minigame mixer .* phase=0 ' \
        'minigame mixer .* phase=8 ' 'minigame mixer .* phase=0 ' && ! grep -q 'game Panel' "$tmp/mg-mixer-help.log"
}
check_pad mg-mixer-help mixer_help
placement_leave() { in_order pad-placement-leave 'game Unknown\(ov=-1,vt=0x021351A8\)' 'placement none' 'game Panel/Interact'; }
check_pad pad-placement-leave placement_leave
boot_late() { in_order "$1" 'boot mode=0' 'front Title' 'title phase=0'; }
check_pad pad-boot-late-start boot_late pad-boot-late-start
check_pad pad-boot-late-a boot_late pad-boot-late-a
# Console manners (the remake's polish, task 2; copies of the complete save, pad only): a question
# opens on No (two quick A on "Overwrite this save?" and on Quit answer No), one B on Keep this Sim?
# lands on its cross (the second discards), LB on the Name page goes nowhere, 123!'s Down never lands
# on the backspace, fast slider presses all count (Settings: twelve Lefts at five a second → 0 %,
# three Rights → 30 %; Options: five Lefts six frames apart → 50 %), B on a concept art shown big
# closes it (still Settings), B from the Save slots or the phone opened from the menu comes back to
# the menu on Options and on the item, Right on Interact's single target or Pockets' key items stays,
# the Give Gift bow takes the focus and gives, the stat panel keeps the page it leaves (held) while
# it goes by Main, Load-a-Sim's list is done after "Loading", Create-a-Sim's fade in from black shows
# no panel of black crops.
pad_t2() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 clock=$5; shift 5
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" --layout console \
        --print-game --print-screen --print-town --print-front --print-hotel --clock "$clock" --input "$input" "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
pad_t2 t2-ask-aa B-cas-opt-save saveslots-ask-aa 200 "2026-11-05 12:05" &
pad_t2 t2-quit-aa B-cas-opt popup-quit-aa 280 "2026-11-05 12:05" &
pad_t2 t2-cas-lb s3 cas-lb 120 "2026-01-01 12:00" &
pad_t2 t2-cas-kb s3 cas-keyboard-down 170 "2026-01-01 12:00" &
wait
pad_t2 t2-settings-fast F-mm-opt-150 settings-fast 460 "2026-11-05 12:05" &
pad_t2 t2-options-fast B-cas-opt options-fast 300 "2026-11-05 12:05" &
pad_t2 t2-concept-b F-copt-concept-80 concept-b 240 "2026-11-05 12:05" &
pad_t2 t2-return-slots B-cas-opt return-slots 340 "2026-11-05 12:05" &
wait
pad_t2 t2-return-phone B-cas-opt return-phone 340 "2026-11-05 12:05" &
pad_t2 t2-interact-one sit-casino interact-one 100 "2026-11-05 12:05" &
pad_t2 t2-pockets-right A-pk-cnt3-slot1 pockets-right 120 "2026-11-05 12:05" &
pad_t2 t2-gift-bow A-gift2-base gift-bow 200 "2026-01-01 12:00" &
wait
pad_t2 t2-cas-arrive s2 cas-arrive 140 "2026-01-01 12:00" --compose --shot-every 1 --shot-dir "$tmp/t2-cas-arrive" &
wait
ask_aa() {
    in_order t2-ask-aa 'popup state=1 kind=2 .*Overwrite' 'focus 0' 'popup none' || return 1
    if grep -q -E 'kind=3|2=Jun/14843' "$tmp/t2-ask-aa.log"; then echo "  t2-ask-aa: saved by the second A"; return 1; fi
}
quit_aa() {
    in_order t2-quit-aa 'popup state=1 kind=2 .*Quit' 'console menu open, focus 0' 'popup none' 'game Panel/Options' 'console menu open, focus 1' || return 1
    if grep -q 'game FrontEnd' "$tmp/t2-quit-aa.log"; then echo "  t2-quit-aa: quit by the second A"; return 1; fi
}
cas_lb() {
    in_order t2-cas-lb 'cas page=0 ' 'cas page=1 ' || return 1
    if grep -q 'cas page=4' "$tmp/t2-cas-lb.log"; then echo "  t2-cas-lb: LB went round to Keep this Sim?"; return 1; fi
}
cas_kb() { in_order t2-cas-kb 'keyboard=2' 'name=[^ ]*\) len='; }
settings_fast() {
    last_before t2-settings-fast 320 'settings page' | grep -q 'page=0 music=0 sfx=4096' || { echo "  t2-settings-fast: music not 0 % after twelve Lefts"; return 1; }
    last_before t2-settings-fast 459 'settings page' | grep -q -E 'page=0 music=1(1[5-9]|2[0-9]|30)[0-9] sfx=4096' || { echo "  t2-settings-fast: music not 30 % after three Rights"; return 1; }
}
options_fast() {
    in_order t2-options-fast 'options music=3686 ' 'options music=3277 ' 'options music=2867 ' 'options music=2458 ' 'options music=2048 sfx=4096' || return 1
    if grep -q -E 'music=1638|sfx=3686' "$tmp/t2-options-fast.log"; then echo "  t2-options-fast: a press counted twice or on SFX"; return 1; fi
}
concept_b() {
    in_order t2-concept-b 'settings page=2 .*art=1' 'front Title' || return 1
    local f; f=$(first_frame t2-concept-b 'front Title')
    [ -n "$f" ] && [ "$f" -ge 160 ] || { echo "  t2-concept-b: Settings left by the B that closed the picture (frame $f)"; return 1; }
}
return_slots() { in_order t2-return-slots 'game SaveSlots' 'game Panel/Interact' 'game Panel/Options' 'console menu open, focus 0'; }
return_phone() { in_order t2-return-phone 'console menu open, focus 4' 'game Phone' 'game Panel/Interact' 'game Panel/Options' 'console menu open, focus 4'; }
interact_one() {
    in_order t2-interact-one 'console menu open, focus 0' || return 1
    if grep -q -E 'focus [1-9]' "$tmp/t2-interact-one.log"; then echo "  t2-interact-one: Right moved the focus"; return 1; fi
}
pockets_right() {
    in_order t2-pockets-right 'console menu open, focus 0' 'focus 6' || return 1
    if grep -q 'focus 13' "$tmp/t2-pockets-right.log"; then echo "  t2-pockets-right: Right went to the phone"; return 1; fi
}
gift_bow() { in_order t2-gift-bow 'game Gift' 'console menu closed, focus 6' 'inventory slots=106:[^,]* counted' 'game Panel/Interact'; }
cas_arrive() {
    in_order t2-cas-arrive 'game CreateASim' || return 1
    # Every frame: an HD panel drawn (its fill at the right) never over black tab crops.
    "$python" -I - "$tmp/t2-cas-arrive" <<'PYEOF'
import glob, sys
from PIL import Image, ImageChops
bad = []
for f in sorted(glob.glob(sys.argv[1] + "/f*.png")):
    im = Image.open(f).convert("RGB")
    panel = im.getpixel((980, 640)) == (0x82, 0x8A, 0xCB)
    tabs = [im.getpixel((x, 67)) for x in (820, 900, 980, 1060, 1140)]  # (the five tabs' crops' centres)
    if panel and all(max(p) < 16 for p in tabs):
        bad.append(f.rsplit("/", 1)[1])
if bad:
    print(f"  t2-cas-arrive: a panel over black crops: {bad[:3]}")
sys.exit(1 if bad else 0)
PYEOF
}
check_pad t2-ask-aa ask_aa
check_pad t2-quit-aa quit_aa
check_pad t2-cas-lb cas_lb
check_pad t2-cas-kb cas_kb
check_pad t2-settings-fast settings_fast
check_pad t2-options-fast options_fast
check_pad t2-concept-b concept_b
check_pad t2-return-slots return_slots
check_pad t2-return-phone return_phone
check_pad t2-interact-one interact_one
check_pad t2-pockets-right pockets_right
check_pad t2-gift-bow gift_bow
check_pad t2-cas-arrive cas_arrive
stat_held() { grep -q -E 'stat page=0 .* held=[1-4]' "$tmp/pad-stat.log" || { echo "  pad-stat: the page left not held while the game showed Main"; return 1; }; }
check_pad pad-stat stat_held
load_done() { in_order pad-load-slot 'popup state=[1-3] kind=3 .*title="Loading"' 'load .* loading' 'front None'; }
check_pad pad-load-slot load_done
# The minigames' messages in HD (the remake's polish, task 3; copies of the complete save, pad only): a
# question of a minigame (the cards' and MooGoo's quit, the painting's "done?", the mixer's save
# question) opens on No, so A A never answers Yes (the question closed, the game goes on); the HD box
# hides the game's (its words, its buttons, the focus's halo on No); the cards' end of a turn in an HD
# box, then the opponent's turn with no focus ring on its cards; MooGoo's line in HD over the game's
# ("Touch a Monkey!" in the port's words); the help texts read back from the stylus's point (the
# Drencher's fire 1666, the crime game's scare 1667, the mixer's variations 1798); the phone's B goes
# back one level (the ringtones → the menu), then hangs up; the detector powered off never shows
# milestone 1's rail.
pad_t3() {
    local name=$1 state=$2 input=$repo/scripts/console/$3.txt frames=$4 clock=$5; shift 5
    cp "$complete_save" "$tmp/$name.sav" || return
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$tmp/$name.sav" --layout console \
        --print-game --print-town --print-minigame --clock "$clock" --input "$input" "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
t3shots() { echo --compose --shot-dir "$tmp/$1" --shot-at "$2"; }
pad_t3 t3-cards-aa act-cards cards-quit-aa 220 "2026-01-01 12:00" $(t3shots t3-cards-aa 70) &
pad_t3 t3-cards-turn act-cards cards-turn 700 "2026-01-01 12:00" $(t3shots t3-cards-turn 10,230,640) &
pad_t3 t3-art-aa G-X-artgal art-done-aa 160 "2026-11-05 12:05" $(t3shots t3-art-aa 40) &
pad_t3 t3-moogoo-aa G-B-mg-r1-full moogoo-quit-aa 200 "2026-11-05 12:05" $(t3shots t3-moogoo-aa 5,50) &
wait
pad_t3 t3-mixer-aa G-X-music mixer-save-aa 300 "2026-11-05 12:05" $(t3shots t3-mixer-aa 170) &
pad_t3 t3-mixer-help G-X-music mixer-help-text 200 "2026-11-05 12:05" $(t3shots t3-mixer-help 100) &
pad_t3 t3-phone-back s7 phone-back 300 "2026-01-01 12:00" &
pad_t3 t3-detector-off G-A-md-walk detector-off 120 "2026-11-05 12:05" --compose --shot-dir "$tmp/t3-detector-off" --shot-every 1 &
wait
pad_t3 t3-scratch-win G-B-sc-two-p10 scratch-win 480 "2026-11-05 12:05" $(t3shots t3-scratch-win 330) &
wait
# A picture's HD box over the playfield: the kit's fill and white words inside it, the focus's halo
# (#309A41) on its No when `halo`, none when not; `ring_free`: no green ring over the playfield.
t3_pic() {
    "$python" -I - "$@" <<'PYEOF'
import sys
from PIL import Image, ImageChops
path, what = sys.argv[1], sys.argv[2]
im = Image.open(path).convert("RGB")
px = lambda x, y: im.getpixel((x, y))
area = [(x, y) for y in range(70, 640, 3) for x in range(40, 800, 3)]
green = sum(1 for x, y in area if px(x, y) == (0x30, 0x9A, 0x41))
fill = sum(1 for x, y in area if px(x, y) == (0x82, 0x8A, 0xCB))
white = sum(1 for x, y in area if px(x, y) == (255, 255, 255))
err = []
if what in ("box", "halo"):
    if fill < 3000: err.append(f"no HD box ({fill} fill pixels)")
    if white < 20: err.append("no white words in the box")
    if what == "halo" and green < 50: err.append("no focus halo on No")
if what == "ring_free" and green > 0: err.append(f"a green ring over the playfield ({green} pixels)")
if what == "moo_line":
    plate = sum(1 for y in range(74, 134, 2) for x in range(100, 740, 4) if px(x, y) == (0x5A, 0x20, 0x38))
    if plate < 200: err.append(f"no HD line over MooGoo's ({plate} plate pixels)")
if err: print(f"  {path.rsplit('/', 2)[-2]}: {'; '.join(err)}")
sys.exit(1 if err else 0)
PYEOF
}
t3_cards_aa() {
    in_order t3-cards-aa 'cards .*quit=1 ' 'cards .*quit=0 ' || return 1
    if grep -q -E 'game (Panel|Loading)' "$tmp/t3-cards-aa.log"; then echo "  t3-cards-aa: quit by A A"; return 1; fi
    t3_pic "$tmp/t3-cards-aa/f000070.png" halo
}
t3_cards_turn() {
    in_order t3-cards-turn 'cards .*message=1 .*phase=6 turn=0 ' 'cards .*message=0 .*turn=1 ' || return 1
    t3_pic "$tmp/t3-cards-turn/f000230.png" box && t3_pic "$tmp/t3-cards-turn/f000640.png" ring_free
}
t3_art_aa() {
    in_order t3-art-aa 'popup state=2 kind=2 .*done painting' 'popup state=2 kind=2 pressed=0,1' 'popup none' 'minigame art .*painted=[1-9]' || return 1
    if grep -q 'pressed=1,1' "$tmp/t3-art-aa.log"; then echo "  t3-art-aa: Yes by A A"; return 1; fi
    t3_pic "$tmp/t3-art-aa/f000040.png" halo
}
t3_moogoo_aa() {
    in_order t3-moogoo-aa 'moogoo .*quit=01 ' 'moogoo .*quit=00 ' 'moogoo phase=13 sub=[2-4] ' || return 1
    if grep -q 'game Panel' "$tmp/t3-moogoo-aa.log"; then echo "  t3-moogoo-aa: quit by A A"; return 1; fi
    t3_pic "$tmp/t3-moogoo-aa/f000005.png" moo_line && t3_pic "$tmp/t3-moogoo-aa/f000050.png" halo
}
t3_mixer_aa() {
    in_order t3-mixer-aa 'mixer .*phase=10 ' 'mixer .*phase=0 ' || return 1
    if grep -q -E 'game MixerSave|mixer .*phase=1 ' "$tmp/t3-mixer-aa.log"; then echo "  t3-mixer-aa: saved or played by A A"; return 1; fi
    t3_pic "$tmp/t3-mixer-aa/f000170.png" halo
}
t3_mixer_help() { in_order t3-mixer-help 'mixer .*phase=8 ' 'mixer .*phase=9 .*helpid=1798' 'mixer .*phase=0 ' && t3_pic "$tmp/t3-mixer-help/f000100.png" box; }
t3_help() {
    grep -q 'drencher .*help=11 .*helpid=1666' "$tmp/mg-drencher-help.log" || { echo "  mg-drencher-help: the fire's text not read back (1666)"; return 1; }
    grep -q 'crime help=1 text=1 .*helpid=1667' "$tmp/mg-crime-help.log" || { echo "  mg-crime-help: the scare's text not read back (1667)"; return 1; }
}
t3_phone_back() {
    in_order t3-phone-back 'phone page=4 ' 'phone page=0 ' 'phone page=10 ' 'game Panel/Interact' || return 1
    [ "$(first_frame t3-phone-back 'phone page=10 ')" -ge 250 ] || { echo "  t3-phone-back: the first B hung up"; return 1; }
}
t3_detector_off() {
    in_order t3-detector-off 'game Detector' 'game Panel/Interact' || return 1
    # (From the first frame on the panel, its tab read or not: the town's world, never a black rail.)
    "$python" -I - "$tmp/t3-detector-off" "$(first_frame t3-detector-off 'game Panel')" <<'PYEOF'
import glob, sys
from PIL import Image, ImageChops
bad = []
for f in sorted(glob.glob(sys.argv[1] + "/f*.png")):
    if int(f.rsplit("/f", 1)[1][:-4]) < int(sys.argv[2]):
        continue
    im = Image.open(f).convert("RGB")
    # milestone 1's rail: a tall black box at the right where the town's world shows
    if sum(1 for y in range(100, 600, 10) for x in range(1190, 1270, 10) if sum(im.getpixel((x, y))) < 30) > 5:
        bad.append(f.rsplit("/", 1)[1])
if bad: print(f"  t3-detector-off: milestone 1's rail in {bad[:3]}")
sys.exit(1 if bad else 0)
PYEOF
}
t3_scratch_win() { in_order t3-scratch-win 'popup state=2 kind=0 .*Congratulations' 'game Panel' && t3_pic "$tmp/t3-scratch-win/f000330.png" box; }
check_pad t3-cards-aa t3_cards_aa
check_pad t3-cards-turn t3_cards_turn
check_pad t3-art-aa t3_art_aa
check_pad t3-moogoo-aa t3_moogoo_aa
check_pad t3-mixer-aa t3_mixer_aa
check_pad t3-mixer-help t3_mixer_help
check_pad mg-drencher-help t3_help
check_pad t3-phone-back t3_phone_back
check_pad t3-detector-off t3_detector_off
check_pad t3-scratch-win t3_scratch_win
# The review fixes of the polish's tasks 1-3: the phone's MENU hangs up from a sub-page (never its
# menu first); B from the phone opened from the menu: the dimmed world while the menu comes back,
# never the town's flash (the frames between the phone and the menu open on its tab darker than
# the town).
pad_t3 fx-phone-menu s7 phone-menu 260 "2026-01-01 12:00" &
pad_t3 fx-return-veil B-cas-opt return-phone 240 "2026-11-05 12:05" --print-screen --compose --shot-dir "$tmp/fx-return-veil" --shot-every 1 &
wait
fx_phone_menu() {
    in_order fx-phone-menu 'phone page=4 ' 'phone page=10 ' 'game Panel/Interact' || return 1
    ! awk '/phone page=4 /{s=1} s && /phone page=0 /{f=1} END{exit !f}' "$tmp/fx-phone-menu.log" || { echo "  fx-phone-menu: the phone's menu before hanging up"; return 1; }
}
fx_return_veil() {
    in_order fx-return-veil 'game Phone' 'game Panel' 'console menu open, focus 4' || return 1
    local from to phone
    from=$(awk '/game Phone/{p=1} p && /game Panel/{sub(":","",$2); print $2; exit}' "$tmp/fx-return-veil.log")
    to=$(awk -v F="$from" '$1=="frame" && ($2+0)>=F && /console menu open/{sub(":","",$2); print $2; exit}' "$tmp/fx-return-veil.log")
    # (the phone's last frames and the Unknown ones before the panel: from the last "game Phone" line)
    phone=$(awk -v F="$from" '$1=="frame" && ($2+0)<F && /game Phone/{sub(":","",$2); p=$2} END{print p}' "$tmp/fx-return-veil.log")
    "$python" -I - "$tmp/fx-return-veil" "$from" "$to" "$phone" <<'PYEOF'
import sys
from PIL import Image, ImageChops, ImageStat
d, a, b, p = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
def frame(n):
    return Image.open(f"{d}/f{n:06d}.png").convert("L").crop((0, 0, 1280, 600))
def mean(n):
    return ImageStat.Stat(frame(n)).mean[0]
town = mean(10)   # (the town, the menu not yet opened)
bad = [n for n in range(a, b) if mean(n) > 0.6 * town]
if bad: print(f"  fx-return-veil: the town shown undimmed at frames {bad[:5]} (town {town:.1f})")
# From the phone's last frames to the panel (Phone, Unknown): never the town itself (the HD phone
# is brighter than the town, so the picture is compared with the town's, not its brightness).
t = frame(10)
flash = [n for n in range(p, a) if ImageStat.Stat(ImageChops.difference(frame(n), t)).mean[0] < 5]
if flash: print(f"  fx-return-veil: the town flashes between the phone and the panel at frames {flash[:5]}")
sys.exit(1 if bad or flash else 0)
PYEOF
}
check_pad fx-phone-menu fx_phone_menu
check_pad fx-return-veil fx_return_veil
# The HD shell (src/shell.*, src/shellhost.*, scripts/shell/), the pad alone, its files in a scratch
# --data-dir: from s7, Start + Back opens the pause menu, slot 1 saved (its state and picture),
# the phone closed, slot 1 loaded after its question (No first: one A closes it, then A, Right, A),
# the phone back; Undo: the panel back; Quit asked (No first), then Yes: the suspend state written
# and the run ends. Then --launch from the same folder: Continue (the first item, A) starts the
# game where it was left (the panel, from frame 5: the launch's turns count) and the suspend
# state is gone. Pictures: each page of the pause menu over s7 (--shell) and the launch panel:
# the panel's fill and the world dimmed under the veil.
mkdir -p "$tmp/shell-data"
"$bin" "$rom" --headless --frames 700 --state-load "$shots/s7.st" --save "$tmp/shell-states.sav" --data-dir "$tmp/shell-data" \
    "${town[@]}" --print-shell --print-game --input "$repo/scripts/shell/states.txt" > "$tmp/shell-states.log" 2>&1 &&
    grep -q '^clock: ' "$tmp/shell-states.log" && touch "$tmp/shell-states.ok"
"$bin" "$rom" --headless --launch --frames 60 --save "$tmp/shell-states.sav" --data-dir "$tmp/shell-data" \
    "${town[@]}" --print-shell --print-game --input "$repo/scripts/shell/continue.txt" > "$tmp/shell-continue.log" 2>&1 &&
    grep -q '^clock: ' "$tmp/shell-continue.log" && touch "$tmp/shell-continue.ok"
shell_states() {
    in_order shell-states 'shell open page=main' 'page=states' 'shell save 1 ok' 'shell closed' 'game Panel/Interact' \
        'ask=load \(no\)' 'ask=none' 'ask=load \(no\)' 'ask=load \(yes\)' 'shell load 1 ok' 'game Phone' \
        'shell undo 0 ok' 'game Panel/Interact' 'ask=quit \(no\)' 'ask=none' 'ask=quit \(yes\)' 'shell suspend written' || return 1
    [ -f "$tmp/shell-data/states/ASJP/slot1.st" ] && [ -f "$tmp/shell-data/states/ASJP/slot1.thumb" ] || { echo "  shell-states: no slot 1 files"; return 1; }
}
shell_continue() {
    in_order shell-continue 'shell launch .*\(continue\)' 'suspend state: loaded' '^frame 5: game Panel/Interact' || return 1
    [ ! -f "$tmp/shell-data/states/ASJP/suspend.st" ] || { echo "  shell-continue: the suspend state kept"; return 1; }
}
check_pad shell-states shell_states
check_pad shell-continue shell_continue
# The review's round (scripts/shell/): Change game keeps the place and comes back to the launch
# panel, whose Continue puts it back (no press left over from the menu's A); a broken slot's load
# fails, the state before it is put back (the phone still there) and the menu says so; a slot that
# cannot be written says so; a place the game saved after is refused by Continue (the .sav
# changed) and dropped; a new start of the same ROM drops its place.
mkdir -p "$tmp/shell-change-data" "$tmp/shell-bad-data/states/ASJP/slot1.st" "$tmp/shell-stale-data" "$tmp/shell-undo-data/states/ASJP" \
    "$tmp/shell-cont-data"
head -c 5000 /dev/urandom > "$tmp/shell-bad-data/states/ASJP/slot2.st"
# (an older before-load.st, another place (s6): the failed load must never put it back, nor write
# over it; slot 1's maker, from before, must outlive the failed save)
cp "$shots/s6.st" "$tmp/shell-bad-data/states/ASJP/before-load.st"
printf 'rom=older.nds\nheader=1\nsave=0\n' > "$tmp/shell-bad-data/states/ASJP/slot1.meta"
cp "$tmp/shell-bad-data/states/ASJP/slot1.meta" "$tmp/shell-bad-slot1.meta"
head -c 5000 /dev/urandom > "$tmp/shell-undo-data/states/ASJP/before-load.st"
shell_run() {
    local name=$1 frames=$2 data=$3 script=$4; shift 4
    "$bin" "$rom" --headless --frames "$frames" --save "$tmp/$name.sav" --data-dir "$tmp/$data" "${town[@]}" --print-shell --print-game \
        --input "$repo/scripts/shell/$script.txt" "$@" > "$tmp/$name.log" 2>&1 && grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
}
shell_run shell-change 300 shell-change-data change --state-load "$shots/s7.st" &
shell_run shell-badload 200 shell-bad-data badload --state-load "$shots/s7.st" &
shell_run shell-badundo 120 shell-undo-data badundo --state-load "$shots/s7.st" &
wait
# A Continue whose suspend state is broken: the place written by a quit, then its bytes spoilt.
( shell_run shell-cont-quit 100 shell-cont-data quit --state-load "$shots/s7.st" &&
  head -c 5000 /dev/urandom > "$tmp/shell-cont-data/states/ASJP/suspend.st" && cp "$tmp/shell-cont-quit.sav" "$tmp/shell-badcont.sav" &&
  shell_run shell-badcont 40 shell-cont-data continue --launch ) &
( shell_run shell-stale-quit 100 shell-stale-data quit --state-load "$shots/s7.st" && printf 'x' >> "$tmp/shell-stale-quit.sav" &&
  cp "$tmp/shell-stale-quit.sav" "$tmp/shell-stale.sav" && shell_run shell-stale 60 shell-stale-data stale --launch &&
  cp "$tmp/shell-stale.sav" "$tmp/shell-newstart-quit.sav" && shell_run shell-newstart-quit 100 shell-stale-data quit --state-load "$shots/s7.st" &&
  cp "$tmp/shell-newstart-quit.sav" "$tmp/shell-newstart.sav" && shell_run shell-newstart 5 shell-stale-data quit ) &
wait
shell_change() {
    in_order shell-change 'shell suspend written' 'shell change 0 ok' '^frame 80: shell launch .*\(continue\)' 'suspend state: loaded' 'game Phone' || return 1
    ! grep -q -E '^frame (5[1-9]|[67][0-9]): shell launch' "$tmp/shell-change.log" || { echo "  shell-change: the menu's A chose on the panel"; return 1; }
}
shell_badload() {
    in_order shell-badload 'ask=load \(yes\)' 'shell restore ok' 'shell load 2 failed' 'shell says That did not work' 'shell save 1 failed' 'shell says That did not work' || return 1
    # (never the older before-load.st, s6's place: the game stays on the phone)
    [ "$(grep -c -E 'game ' "$tmp/shell-badload.log")" = 1 ] || { echo "  shell-badload: the game moved: $(grep -E 'game ' "$tmp/shell-badload.log" | tail -n 1)"; return 1; }
    cmp -s "$shots/s6.st" "$tmp/shell-bad-data/states/ASJP/before-load.st" || { echo "  shell-badload: before-load.st written by a failed load"; return 1; }
    cmp -s "$tmp/shell-bad-slot1.meta" "$tmp/shell-bad-data/states/ASJP/slot1.meta" || { echo "  shell-badload: slot 1's maker lost by a failed save"; return 1; }
}
shell_badundo() {
    in_order shell-badundo 'page=states focus=4' 'shell restore ok' 'shell undo 0 failed' 'shell says That did not work' || return 1
    [ "$(grep -c -E 'game ' "$tmp/shell-badundo.log")" = 1 ] || { echo "  shell-badundo: the game moved"; return 1; }
}
shell_badcont() {
    in_order shell-cont-quit 'shell suspend written' || return 1
    in_order shell-badcont 'shell launch .*\(continue\)' 'shell restore ok' 'suspend state: cannot load' 'shell says Your place could not be loaded' || return 1
    [ ! -f "$tmp/shell-cont-data/states/ASJP/suspend.st" ] || { echo "  shell-badcont: the broken place kept"; return 1; }
}
shell_stale() {
    in_order shell-stale-quit 'shell suspend written' || return 1
    in_order shell-stale 'shell launch refused continue: saved since' '^frame 25: shell launch sims2' || return 1
    ! grep -q 'suspend state: loaded' "$tmp/shell-stale.log" || { echo "  shell-stale: the old place loaded"; return 1; }
}
shell_newstart() {
    in_order shell-newstart-quit 'shell suspend written' || return 1
    in_order shell-newstart 'suspend state: dropped' || return 1
    [ ! -f "$tmp/shell-stale-data/states/ASJP/suspend.st" ] || { echo "  shell-newstart: the place kept"; return 1; }
}
check_pad shell-change shell_change
check_pad shell-badload shell_badload
check_pad shell-badundo shell_badundo
check_pad shell-badcont shell_badcont
check_pad shell-stale shell_stale
check_pad shell-newstart shell_newstart
for page in main settings controls keys states quit; do
    "$bin" "$rom" --headless --frames 64 --state-load "$shots/s7.st" --save "$tmp/shell-$page.sav" --compose \
        --shell "$page" --shell-at 60 --shot-at 59,62 --shot-dir "$tmp/shell-$page" "${town[@]}" > "$tmp/shell-$page.log" 2>&1 &
done
"$bin" "$rom" --headless --launch --frames 4 --save "$tmp/shell-launch.sav" --data-dir "$tmp/shell-data" --compose \
    --shot-at 2 --shot-dir "$tmp/shell-launch" "${town[@]}" > "$tmp/shell-launch.log" 2>&1 &
wait
if "$python" -I - "$tmp" <<'PYEOF'
import sys
from PIL import Image, ImageStat
t = sys.argv[1]
bad = []
def near(p, q, d=8): return all(abs(a - b) <= d for a, b in zip(p, q))
for page in ["main", "settings", "controls", "keys", "states", "quit"]:
    try:
        before = Image.open(f"{t}/shell-{page}/f000059.png").convert("RGB")
        im = Image.open(f"{t}/shell-{page}/f000062.png").convert("RGB")
    except OSError:
        bad.append(f"{page}: no picture"); continue
    fill = (0x82, 0x8A, 0xCB) if page != "quit" else None
    # The panel's fill under the hints (veiled once more by the quit question's).
    if fill and not near(im.getpixel((220, 650)), fill): bad.append(f"{page}: no panel fill at 220,650: {im.getpixel((220, 650))}")
    # The world beside the panel: darker than without the menu.
    a = ImageStat.Stat(before.crop((120, 0, 190, 600))).mean
    b = ImageStat.Stat(im.crop((120, 0, 190, 600))).mean
    if not sum(b) < 0.6 * sum(a): bad.append(f"{page}: the world not dimmed ({sum(a):.0f} -> {sum(b):.0f})")
    # White words in the title band (the quit question: in its box, over the veiled title).
    box = (500, 780, 50, 100) if page != "quit" else (420, 860, 250, 310)
    if not any(min(im.getpixel((x, y))) > 240 for x in range(box[0], box[1]) for y in range(box[2], box[3], 2)): bad.append(f"{page}: no title")
try:
    im = Image.open(f"{t}/shell-launch/f000002.png").convert("RGB")
    if not near(im.getpixel((10, 10)), (0x10, 0x12, 0x18), 2): bad.append(f"launch: no backdrop {im.getpixel((10, 10))}")
    if not near(im.getpixel((60, 690)), (0x82, 0x8A, 0xCB)): bad.append(f"launch: no list panel {im.getpixel((60, 690))}")
    if not near(im.getpixel((1220, 690)), (0x82, 0x8A, 0xCB)): bad.append(f"launch: no controls card {im.getpixel((1220, 690))}")
except OSError:
    bad.append("launch: no picture")
for b in bad: print("  " + b)
sys.exit(1 if bad else 0)
PYEOF
then echo "ok   shell pictures: the pause menu's pages and the launch panel"
else echo "FAIL shell pictures"; fails=$((fails + 1)); fi
# The settings' five tabs (scripts/shell/settings-tabs.txt, --shell settings): RB through every tab
# and round (wrapping), LB wraps back, A on Battery saver (Graphics) dims 3D resolution and 60 fps,
# Y asks to reset the tab (No first), B keeps it. Pictures of each tab: all different, the panel's
# fill at its foot; Graphics with the saver on differs from Graphics without it.
"$bin" "$rom" --headless --frames 122 --state-load "$shots/s7.st" --save "$tmp/shell-tabs.sav" --compose --print-shell \
    --shell settings --shell-at 60 --input "$repo/scripts/shell/settings-tabs.txt" --shot-at 63,67,73,79,85,91,115 \
    --shot-dir "$tmp/shell-tabs" "${town[@]}" > "$tmp/shell-tabs.log" 2>&1 && grep -q '^clock: ' "$tmp/shell-tabs.log" && touch "$tmp/shell-tabs.ok"
shell_tabs() {
    in_order shell-tabs 'shell open page=settings' 'page=settings tab=1 focus=0' 'shell setting 8 ok' 'tab=2' 'tab=3' 'tab=4' \
        'page=settings tab=0' 'page=settings tab=4' 'page=settings tab=0' 'ask=reset \(no\)' 'page=settings tab=0 focus=0 ask=none' || return 1
    ! grep -q 'reset tab' "$tmp/shell-tabs.log" || { echo "  shell-tabs: reset without a Yes"; return 1; }
    "$python" -I - "$tmp/shell-tabs" <<'PYEOF'
import sys
from PIL import Image
d = sys.argv[1]
f = {n: Image.open(f"{d}/f{n:06d}.png").convert("RGB") for n in (63, 67, 73, 79, 85, 91, 115)}
tabs = [f[n].tobytes() for n in (63, 67, 79, 85, 91)]
bad = []
if len(set(tabs)) != 5: bad.append("two tabs look the same")
if f[67].tobytes() == f[73].tobytes(): bad.append("Battery saver changed nothing on Graphics")
for n in (63, 67, 79, 85, 91):
    p = f[n].getpixel((220, 650))
    if max(abs(a - b) for a, b in zip(p, (0x82, 0x8A, 0xCB))) > 8: bad.append(f"frame {n}: no panel fill {p}")
for b in bad: print("  shell-tabs: " + b)
sys.exit(1 if bad else 0)
PYEOF
}
check_pad shell-tabs shell_tabs
# Platform (task 5). The game's language: --game-language 2 sets the DS firmware's, so the boot's
# last splash (frame 900, a fresh boot) says the game's French words: its touch screen differs from
# English's and the strings load in French; fast boot (--fast-boot, the window's rule, logged) ends
# at that splash and never before it, pressed nothing (the title still waits: an A then opens it);
# a press during the splashes ends it at once. A save that cannot be written beside the ROM (a
# read-only file in a read-only folder) goes to the data folder's saves/, a copy first, and the
# game's own save (Options > Save > slot 2) lands there, the original untouched.
for l in 1 2; do
    cp "$complete_save" "$tmp/lang$l.sav"
    "$bin" "$rom" --headless --frames 902 --save "$tmp/lang$l.sav" --game-language $l --shot-at 900 --shot-dir "$tmp/lang$l" \
        > "$tmp/lang$l.log" 2>&1 && grep -q '^clock: ' "$tmp/lang$l.log" && touch "$tmp/lang$l.ok" &
done
wait
if [ -f "$tmp/lang1.ok" ] && [ -f "$tmp/lang2.ok" ] && grep -q 'firmware language: 2' "$tmp/lang2.log" &&
   "$python" -I -c '
import sys
from PIL import Image, ImageChops
a, b = (Image.open(p).convert("RGB").crop((0, 192, 256, 384)) for p in sys.argv[1:3])
sys.exit(0 if ImageChops.difference(a, b).getbbox() else 1)' "$tmp/lang1/f000900.png" "$tmp/lang2/f000900.png"
then echo "ok   game-language: the firmware's French reaches the game (the last splash differs from English's)"
else echo "FAIL game-language"; fails=$((fails + 1)); fi
printf '900 2 pad a\n' > "$tmp/fastboot-a.txt"
printf '100 2 pad a\n' > "$tmp/fastboot-early.txt"
cp "$complete_save" "$tmp/fastboot.sav"
"$bin" "$rom" --headless --frames 1000 --save "$tmp/fastboot.sav" --fast-boot --layout console --print-front --input "$tmp/fastboot-a.txt" \
    > "$tmp/fastboot.log" 2>&1 && grep -q '^clock: ' "$tmp/fastboot.log" && touch "$tmp/fastboot.ok" &
cp "$complete_save" "$tmp/fastboot-early.sav"
"$bin" "$rom" --headless --frames 200 --save "$tmp/fastboot-early.sav" --fast-boot --layout console --input "$tmp/fastboot-early.txt" \
    > "$tmp/fastboot-early.log" 2>&1 && grep -q '^clock: ' "$tmp/fastboot-early.log" && touch "$tmp/fastboot-early.ok" &
wait
fastboot() {
    in_order fastboot 'fast boot: on' 'fast boot: done \(the boot waits for a touch\)' 'front Title' || return 1
    [ "$(first_frame fastboot 'fast boot: done')" = "$(first_frame fastboot 'boot mode=0 ')" ] || { echo "  fastboot: not done on the last splash's first frame"; return 1; }
    local f; f=$(first_frame fastboot 'fast boot: done')
    [ -n "$f" ] && [ "$f" -ge 800 ] && [ "$f" -le 900 ] || { echo "  fastboot: done at frame $f, not at the last splash"; return 1; }
    [ "$(first_frame fastboot 'front Title')" -ge 900 ] || { echo "  fastboot: the title before the A"; return 1; }
    grep -q '^frame 100: fast boot: done (a press)' "$tmp/fastboot-early.log" || { echo "  fastboot: a press did not end it"; return 1; }
    [ -f "$tmp/fastboot-early.ok" ]
}
check_pad fastboot fastboot
mkdir -p "$tmp/ro" "$tmp/ro-data"
"$bin" "$rom" --headless --frames 1 --state-load "$shots/B-st-opt-save.st" --save "$tmp/ro/game.sav" > /dev/null 2>&1
chmod 444 "$tmp/ro/game.sav"; chmod 555 "$tmp/ro"
ro_before=$(cksum < "$tmp/ro/game.sav")
"$bin" "$rom" --headless --frames 300 --state-load "$shots/B-st-opt-save.st" --save "$tmp/ro/game.sav" --data-dir "$tmp/ro-data" \
    --layout console --print-town "${town[@]}" --input "$repo/scripts/console/saveslots-save.txt" > "$tmp/save-elsewhere.log" 2>&1 &&
    grep -q '^clock: ' "$tmp/save-elsewhere.log" && touch "$tmp/save-elsewhere.ok"
chmod 755 "$tmp/ro"; chmod 644 "$tmp/ro/game.sav"
save_elsewhere() {
    in_order save-elsewhere 'cannot be written: the game saves to .*/ro-data/saves/game.sav' 'saves current=1 0=empty 1=Sim' || return 1
    [ "$(cksum < "$tmp/ro/game.sav")" = "$ro_before" ] || { echo "  save-elsewhere: the read-only save changed"; return 1; }
    [ -f "$tmp/ro-data/saves/game.sav" ] && [ "$(cksum < "$tmp/ro-data/saves/game.sav")" != "$ro_before" ] || { echo "  save-elsewhere: the game's save not in saves/"; return 1; }
}
check_pad save-elsewhere save_elsewhere
# Round 2: the folder writable again (above) and the copy in saves/ newer (the game saved there):
# at the next start the newer save goes back beside the ROM (verified), loads from there, and the
# copy is set aside as .moved.
copy_sum=$(cksum < "$tmp/ro-data/saves/game.sav" 2>/dev/null)
"$bin" "$rom" --headless --frames 5 --save "$tmp/ro/game.sav" --data-dir "$tmp/ro-data" --layout console > "$tmp/save-back.log" 2>&1 &&
    grep -q '^clock: ' "$tmp/save-back.log" && touch "$tmp/save-back.ok"
save_back() {
    [ -n "$copy_sum" ] || { echo "  save-back: no copy in saves/ from round 1"; return 1; }
    in_order save-back '/ro/game.sav can be written again: the save is back there' 'save: .*/ro/game.sav \([0-9]+ bytes\)' || return 1
    ! grep -q 'cannot be written' "$tmp/save-back.log" || { echo "  save-back: still saving elsewhere"; return 1; }
    [ "$(cksum < "$tmp/ro/game.sav")" = "$copy_sum" ] || { echo "  save-back: the save beside the ROM is not the newer copy"; return 1; }
    [ ! -f "$tmp/ro-data/saves/game.sav" ] && [ "$(cksum < "$tmp/ro-data/saves/game.sav.moved")" = "$copy_sum" ] ||
        { echo "  save-back: the copy not set aside as .moved"; return 1; }
}
check_pad save-back save_back
if [ "$fails" != 0 ]; then echo "console_check: $fails failure(s)"; exit 1; fi
echo "console_check: ok"
