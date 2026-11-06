#!/usr/bin/env bash
# Checks what the game says is up, read from main RAM (src/gamestate.*, --print-game), from save
# states: each state runs 120 frames headless and must settle on its screen; from s6,
# scripts/tabs.txt walks the panel's tabs and opens the phone.
# Needs the ROM and the save states (not in git, so not in CI):
#   bash tests/gamestate_check.sh
# Override with SIMS2WIDE (the program), ROM, SHOTS (the save states' folder), MORE_SHOTS (a
# second folder, looked in when SHOTS lacks a state) and COMPLETE_SAVE (the downloaded complete
# save the casino, late hotel and cards states were made on); they default to this checkout's
# build/, roms/, shots/, shots/ and saves/complete-play.sav.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms/" -name "*.nds" 2>/dev/null | head -n 1)}
shots=${SHOTS:-$repo/shots}
more_shots=${MORE_SHOTS:-$repo/shots}
complete_save=${COMPLETE_SAVE:-$repo/saves/complete-play.sav}
[ -x "$bin" ] || { echo "no program at $bin (ninja -C build sims2wide)"; exit 2; }
[ -f "$rom" ] || { echo "no ROM (set ROM)"; exit 2; }
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

# run NAME STATE FRAMES [options...]: the whole log in NAME.log, the game states printed (one
# "frame N: game X" per line) in NAME.out, and NAME.ok only if the run went to its end (exit
# status 0 and the headless end line, the DS clock's): a crash or a missing state must not pass.
run() {
    local name=$1 state=$2 frames=$3 file; shift 3
    file=$shots/$state.st
    [ -f "$file" ] || file=$more_shots/$state.st
    if [ ! -f "$file" ]; then echo "no save state $state.st in $shots or $more_shots" > "$tmp/$name.log"; : > "$tmp/$name.out"; return; fi
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" \
        --save "$tmp/$name.sav" --print-game "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
    grep -E '^frame [0-9]+: game ' "$tmp/$name.log" > "$tmp/$name.out"
}

# ended NAME: the run went to its end; otherwise says why and fails.
ended() {
    [ -f "$tmp/$1.ok" ] && return 0
    echo "FAIL $1: the run did not end well: $(tail -n 1 "$tmp/$1.log")"
    return 1
}

# names NAME: the game states printed, one name each, consecutive repeats (a new selection under
# the same name) folded.
names() { sed 's/^frame [0-9]*: game //' "$tmp/$1.out" | uniq | tr '\n' ' ' | sed 's/ $//'; }
# settled NAME: the last one printed.
settled() { sed 's/^frame [0-9]*: game //' "$tmp/$1.out" | tail -n 1; }

town=(--clock "2026-01-01 12:00")
hotel=(--clock "2026-01-01 14:00")
late=(--clock "2026-11-05 12:05")
# state:expected, the state the game is in after 120 frames.
cases=(
    "s2:FrontEnd" "s3:CreateASim" "s4:CreateASim" "s5:Dialogue" "s6:Popup" "s7:Phone"
    "sit-dialogue:Dialogue" "sit-gift:Gift" "sit-cityhall:Panel/Interact" "sit-build:Build"
    "sit-store:Panel/Interact" "sit-storebuy:Catalogue" "sit-pause:Popup" "act-social:Social"
    "sit-hotel:Panel/Interact" "sit-concierge:Dialogue" "sit-basement:Panel/Interact"
    "act-vacuum:Vacuum" "act-vacuum-clog:Vacuum"
    "sit-casino:Panel/Interact" "sit-hotel-late:Panel/Interact" "act-cards:Cards"
)
for c in "${cases[@]}"; do
    st=${c%%:*}
    case $st in
        sit-hotel|sit-concierge|sit-basement|act-vacuum|act-vacuum-clog) run "$st" "$st" 120 "${hotel[@]}" & ;;
        act-cards|sit-casino|sit-hotel-late)
            if [ -f "$complete_save" ]; then cp "$complete_save" "$tmp/$st.sav"
            else echo "warning: no $complete_save (COMPLETE_SAVE): $st runs without its save"; fi
            if [ "$st" = act-cards ]; then run "$st" "$st" 120 &
            else run "$st" "$st" 120 "${late[@]}" &
            fi ;;
        *) run "$st" "$st" 120 "${town[@]}" & ;;
    esac
done
run tabs s6 1650 --input "$repo/scripts/tabs.txt" "${town[@]}" &
wait

fail=0
for c in "${cases[@]}"; do
    st=${c%%:*} want=${c#*:}
    ended "$st" || { fail=1; continue; }
    got=$(settled "$st")
    if [ "$got" = "$want" ]; then echo "ok   $st: $want"
    else echo "FAIL $st: want '$want', got '${got:-nothing}' (all: $(names "$st"))"; fail=1; fi
done

# tabs.txt: the popups closed, then each tab touched and the phone: the game goes through them
# in this order (the RAM leads the pixels, so no timing is asserted).
want_seq="Popup Panel/Interact Popup Panel/Interact Panel/Pockets Panel/Goals Panel/Relationships Panel/Options Phone"
if ! ended tabs; then fail=1
else
    got_seq=$(names tabs)
    if [ "$got_seq" = "$want_seq" ]; then echo "ok   tabs: $got_seq"
    else echo "FAIL tabs: want '$want_seq', got '${got_seq:-nothing}'"; fail=1; fi
fi

# The status bar and the conversation (--print-hud): the clock follows --clock, the complete
# save's money, tier and Sim, and the conversations' page / string id / speaker series. The
# lines are `frame N: hud ...` and `frame N: dialogue ...`; the frame numbers are not asserted.
hud_run() { # NAME STATE FRAMES [options]: the hud and dialogue lines in NAME.hud
    local name=$1 state=$2 frames=$3 file; shift 3
    file=$shots/$state.st; [ -f "$file" ] || file=$more_shots/$state.st
    if [ ! -f "$file" ]; then echo "no save state $state.st" > "$tmp/$name.hud"; return; fi
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" --save "$tmp/$name.sav" --print-hud "$@" > "$tmp/$name.hudlog" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.hudlog" && touch "$tmp/$name.hudok"
    grep -E '^frame [0-9]+: (hud|dialogue) ' "$tmp/$name.hudlog" | sed 's/^frame [0-9]*: //' > "$tmp/$name.hud"
}
# hud_want NAME PATTERN LABEL: the last hud line matches (grep -E).
hud_want() {
    if [ -f "$tmp/$1.hudok" ] && grep '^hud ' "$tmp/$1.hud" | tail -n 1 | grep -q -E "$2"; then echo "ok   $3"
    else echo "FAIL $3: got '$(grep '^hud ' "$tmp/$1.hud" 2>/dev/null | tail -n 1)'"; fail=1; fi
}
hud_run hud-s7 s7 120 "${town[@]/12:00/15:37}"
cp "$complete_save" "$tmp/hud-complete.sav" 2>/dev/null
hud_run hud-complete sit-hotel-late 150 "${late[@]}"
hud_run hud-s5 s5 520 "${town[@]}" --input "$repo/scripts/dlg-taps.txt"
hud_run hud-conc sit-concierge 60 "${hotel[@]}"
hud_run hud-sit sit-dialogue 60 "${town[@]}"
hud_want hud-s7 '^hud money=2000 time=15:37 sanity=[0-9]+/100 tier=0 name=Sim$' "hud: s7 on a 15:37 clock reads 15:37, money 2000, tier 0"
hud_want hud-complete '^hud money=14798 time=12:0[5-9] sanity=[0-9]+/225 tier=2 name=Jun$' "hud: the complete save reads money 14798, max 225, tier 2, Jun"
dlg_want() { # NAME SERIES LABEL: the dialogue lines folded to speech page id speaker name last
    local got; got=$(grep '^dialogue ' "$tmp/$1.hud" | grep -v none | sed -E 's/^dialogue speech=([0-9]+) page=([0-9]+)\/([0-9]+) id=([0-9]+) speaker=([0-9]+) name=(-?[0-9]+) last=([01])/\1:\2\/\3:\4:\5:\6:\7/' | tr '\n' ' ' | sed 's/ $//')
    if [ "$got" = "$2" ]; then echo "ok   $3"; else echo "FAIL $3: want '$2', got '$got'"; fail=1; fi
}
dlg_want hud-s5 '0:1/4:1183:22:345:0 0:2/4:1184:22:345:0 0:3/4:1185:22:345:0 0:4/4:1186:22:345:1 1:1/3:1187:22:345:0 1:2/3:1188:22:345:0 1:3/3:1189:22:345:1' "dialogue: s5 tapped through, pages 1183-1186 then 1187-1189, last flags"
dlg_want hud-conc '4:1/5:1198:45:368:0' "dialogue: the concierge, speaker 45 = name id 368"
dlg_want hud-sit '2:1/1:1190:22:345:1' "dialogue: the sit conversation, one page, last"

# The recorder: --game-unknown-after N fakes an unmapped screen (a new pair every 200 frames);
# --record-dir saves a state and a picture per pair, once, 30 frames in; the state loads.
rec_run() { # DIR FRAMES [options]
    local d=$1 n=$2; shift 2; mkdir -p "$d"
    "$bin" "$rom" --headless --frames "$n" --state-load "$shots/s6.st" --save "$tmp/rec.sav" --record-dir "$d" --game-unknown-after 50 "$@" > "$d.log" 2>&1
}
count() { find "$1" -name "*.$2" | wc -l | tr -d ' '; }
if [ -f "$shots/s6.st" ]; then
    rec_run "$tmp/rec1" 200; rec_run "$tmp/rec2" 350
    r1s=$(count "$tmp/rec1" st) r1p=$(count "$tmp/rec1" png) r2s=$(count "$tmp/rec2" st) r2p=$(count "$tmp/rec2" png)
    if [ "$r1s $r1p $r2s $r2p" = "1 1 2 2" ]; then echo "ok   recorder: 1 + 1 after 200 frames, 2 + 2 after 350"
    else echo "FAIL recorder: want '1 1 2 2', got '$r1s $r1p $r2s $r2p'"; fail=1; fi
    rec_run "$tmp/rec2" 350
    [ "$(count "$tmp/rec2" st)" = 2 ] && echo "ok   recorder: a second session records nothing new" || { echo "FAIL recorder: a second session recorded again"; fail=1; }
    st=$(find "$tmp/rec1" -name "*.st" | head -n 1)
    if "$bin" "$rom" --headless --frames 10 --state-load "$st" --save "$tmp/rec.sav" > "$tmp/rec-load.log" 2>&1 && grep -q '^clock: ' "$tmp/rec-load.log"; then echo "ok   recorder: the recorded state loads"
    else echo "FAIL recorder: the recorded state does not load"; fail=1; fi
    # nothing unknown: nothing recorded
    mkdir -p "$tmp/rec0"
    "$bin" "$rom" --headless --frames 200 --state-load "$shots/s6.st" --save "$tmp/rec.sav" --record-dir "$tmp/rec0" > "$tmp/rec0.log" 2>&1
    [ "$(count "$tmp/rec0" st)" = 0 ] && echo "ok   recorder: nothing recorded on a mapped screen" || { echo "FAIL recorder: recorded on a mapped screen"; fail=1; }
else echo "FAIL recorder: no $shots/s6.st"; fail=1; fi

# The ROM gate: the RAM map is the European ROM's (ASJP). A copy with another game code (bytes
# 0x0C-0x0F) must read no game state, say so once, and still run to its end.
python3 - "$rom" "$tmp/other.nds" <<'PY'
import sys
d = bytearray(open(sys.argv[1], 'rb').read()); d[0x0C:0x10] = b'XXXX'
open(sys.argv[2], 'wb').write(d)
PY
if [ -f "$shots/s6.st" ] && [ -f "$tmp/other.nds" ]; then
    "$bin" "$tmp/other.nds" --headless --frames 120 --state-load "$shots/s6.st" --save "$tmp/other.sav" --print-game > "$tmp/other.log" 2>&1
    st=$?
    games=$(grep -c -E '^frame [0-9]+: game ' "$tmp/other.log")
    said=$(grep -c 'NOT THE EUROPEAN ONE' "$tmp/other.log")
    if [ "$st" = 0 ] && [ "$games" = 0 ] && [ "$said" = 1 ] && grep -q '^clock: ' "$tmp/other.log"; then echo "ok   rom gate: another game code reads no game state, said once"
    else echo "FAIL rom gate: exit $st, game lines $games, messages $said"; fail=1; fi
    # the recorder's invalid-RAM watch (600 + 300 frames) must not fire without the map
    mkdir -p "$tmp/rec-other"
    "$bin" "$tmp/other.nds" --headless --frames 1100 --state-load "$shots/s6.st" --save "$tmp/other.sav" --record-dir "$tmp/rec-other" > "$tmp/rec-other.log" 2>&1
    if [ "$(count "$tmp/rec-other" st)" = 0 ] && grep -q '^clock: ' "$tmp/rec-other.log"; then echo "ok   rom gate: another game code records nothing"
    else echo "FAIL rom gate: recorded on another game code"; fail=1; fi
else echo "FAIL rom gate: no $shots/s6.st or no copy"; fail=1; fi

# The town's records read from RAM (--print-town): inventory, options, popup, save slots, from
# the held-out states A-gift2-base (two items, key items not owned), A-pk-cnt3-slot1 (the complete
# save's pockets), B-st-opt-music, B-ch-opt-quit / B-pause / s6 (popups; their texts must equal
# the ROM's strings 65, 22 / 23 and 2448) and B-cas-opt-save (three save slots). Copies of these
# states from research/ram/gen go in shots/ (git-ignored, listed in CLAUDE.md).
town_run() { # NAME STATE: the last town lines in NAME.town
    local name=$1 file=$shots/$2.st
    if [ ! -f "$file" ]; then echo "no save state $2.st" > "$tmp/$name.town"; return; fi
    "$bin" "$rom" --headless --frames 60 --state-load "$file" --save "$tmp/$name.sav" --print-town "${late[@]}" > "$tmp/$name.townlog" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.townlog" && touch "$tmp/$name.townok"
    grep -E '^frame [0-9]+: (inventory|options|popup|saves|interact|pockets|goals|relations|phone|gift|note) ' "$tmp/$name.townlog" | sed 's/^frame [0-9]*: //' > "$tmp/$name.town"
}
town_want() { # NAME KIND PATTERN LABEL: the last line of that kind matches (grep -E)
    if [ -f "$tmp/$1.townok" ] && grep "^$2 " "$tmp/$1.town" | tail -n 1 | grep -q -E "$3"; then echo "ok   $4"
    else echo "FAIL $4: got '$(grep "^$2 " "$tmp/$1.town" 2>/dev/null | tail -n 1)'"; fail=1; fi
}
town_text() { # NAME TEXT_ID TITLE_ID LABEL: the popup's text (and title, -1 none) is the ROM's string
    local want got
    want="title=\"$([ "$3" = -1 ] || "$bin" "$rom" --dump-string "$3" 2>/dev/null)\" text=\"$("$bin" "$rom" --dump-string "$2" 2>/dev/null)\""
    got=$(grep '^popup ' "$tmp/$1.town" | tail -n 1 | sed -E 's/^popup state=[0-9]+ kind=[0-9]+ pressed=-?[0-9]+,[01] //')
    if [ -n "$got" ] && [ "$got" = "$want" ]; then echo "ok   $4"; else echo "FAIL $4: want '$want', got '$got'"; fail=1; fi
}
town_run t-gift A-gift2-base; town_run t-pk A-pk-cnt3-slot1; town_run t-music B-st-opt-music
town_run t-quit B-ch-opt-quit; town_run t-pause B-pause; town_run t-goal s6; town_run t-save B-cas-opt-save
town_want t-gift inventory '^inventory slots=171:634/993/321,106:569/928/1991 counted=0,0,0,0 keys=000$' "town: two pocket items with names, descriptions, icons"
town_want t-pk inventory '^inventory slots=171:634/993/321 counted=27,0,2,0 keys=111$' "town: counted items 27 0 2 0, the three tools owned"
town_want t-music options '^options music=1229 sfx=4096$' "town: music volume 30 %"
town_want t-gift options '^options music=4096 sfx=4096$' "town: volumes at 100 %"
town_want t-gift popup '^popup none$' "town: no popup"
town_want t-quit popup 'kind=2 ' "town: the quit question is a yes / no popup"
town_text t-quit 65 -1 "town: the quit popup's text is string 65, no title"
town_want t-pause popup 'kind=0 ' "town: Game Paused has one button"
town_text t-pause 23 22 "town: Game Paused's text and title are strings 23 and 22"
town_text t-goal 2448 -1 "town: the goal popup's text is string 2448"
town_want t-save saves '^saves current=2 0=[^ ]+/3226/3 1=[^ ]+/1055/19 2=[^ ]+/9150/20$' "town: three save slots, the third current"
town_want t-gift saves '^saves current=-1 0=empty 1=empty 2=empty$' "town: no save slots used"
# The menus' lists (--print-town: interact, pockets, goals, relations, phone, gift, note), from
# held-out states (copies in shots/): the store's Buy, the casino's three rows, the concierge, the
# strip, the pad's hold, Pockets selections, Goals (8 goals; the complete save's 100 % footer; a
# hotel goal), Relationships (23 Sims; the scroll; one Sim), the phone's pages, Give Gift.
for st in L-sit-store-none-250 L-sit-casino-none-250 L-sit-cityhall-none-250 D-hotel-down D-cas-right2 D-cas-hold D-cas-hold-down2 \
          A-pk-slot1-key2 A-pk-town-cnt1 A-pk-cnt3-slot1 B-pg8-goals-l7 B-cas-goals B-hot-goals B-cas-rel-next5 B-st-rel \
          C-cas-msg1 C-cas-msg1-dn4 C-cas-pb1 C-s7-msgs A-gift2-slot2; do town_run "m-$st" "$st"; done
town_want m-L-sit-store-none-250 interact '^interact target=sim51:374:4359 prev=- next=- big=4359 pad=1 hl=0 actions=43:1888$' "menus: the store's target (Johnny Smith, Buy)"
town_want m-L-sit-casino-none-250 interact 'target=obj29:492:6638 .* actions=98:1943,73:1918,93:1938$' "menus: the casino's cards, three actions"
town_want m-L-sit-cityhall-none-250 interact '^interact target=- prev=- next=- ' "menus: no target"
town_want m-D-hotel-down interact 'target=sim45:368:2168 .* actions=6:1851,71:1916,4:1849$' "menus: the concierge (Sim 45) and his actions"
town_want m-D-cas-right2 interact 'target=obj15:478:5744 prev=obj29:492:6638 next=- .* actions=28:1873$' "menus: the strip's previous target"
town_first() { # NAME KIND PATTERN LABEL: the FIRST line of that kind matches (the state's own frame 0)
    if [ -f "$tmp/$1.townok" ] && grep "^$2 " "$tmp/$1.town" | head -n 1 | grep -q -E "$3"; then echo "ok   $4"
    else echo "FAIL $4: got '$(grep "^$2 " "$tmp/$1.town" 2>/dev/null | head -n 1)'"; fail=1; fi
}
town_first m-D-cas-hold interact '^interact .* pad=0 hl=0 ' "menus: pad mode (A held), first row lit"
town_first m-D-cas-hold-down2 interact 'hl=2 ' "menus: the third row lit"
town_want m-A-pk-slot1-key2 pockets '^pockets slot=-1 key=1 counted=-1$' "menus: Pockets key item 1"
town_want m-A-pk-town-cnt1 pockets '^pockets slot=-1 key=-1 counted=0$' "menus: Pockets counted item 0"
town_want m-A-pk-cnt3-slot1 pockets '^pockets slot=0 key=-1 counted=-1$' "menus: Pockets slot 0"
town_want m-B-pg8-goals-l7 goals '^goals sel=3 first=1 lines=14 portrait=4257 footer=-1/-1/-1 entries=m0:1627:0\+1,m1:1628:1\+3,m2:1629:4\+1,m3:1630:5\+3,m4:1631:8\+1,m5:1632:9\+2,m6:1633:11\+1,m7:1634:12\+2$' "menus: 8 goals, the fourth selected, scrolled"
town_want m-B-cas-goals goals 'footer=1843/1842/100 entries=-$' "menus: the hotel footer at 100 %"
town_want m-B-hot-goals goals '^goals sel=0 first=0 lines=3 portrait=2168 .* entries=m1:1628:0\+3$' "menus: the hotel goal and the concierge's portrait"
town_want m-B-cas-rel-next5 relations '^relations first=2 sel=5 sims=1:324/390/184/195/26,2:' "menus: 23 Sims, the sixth selected, first 2"
town_want m-B-st-rel relations '^relations first=0 sel=0 sims=22:345/411/184/195/4257$' "menus: one Sim (id 22, Jebediah)"
town_want m-C-cas-msg1 phone '^phone page=3 row=0 scroll=0 shown=0 .* menu=2066,2068,2069,2070 inbox=0:2311:368:read,1:1182:368:new date=11/5 end=10 maid=-1 reply=-1 caller=-1 list=\?$' "menus: the phone's message page, two messages, one unread, Next Message on row 10"
town_want m-C-cas-msg1-dn4 phone 'page=3 row=4 scroll=3 ' "menus: the phone's scrolled message"
town_want m-C-cas-pb1 phone 'page=7 .* contacts=22/2 .* list=2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23$' "menus: the contact list, 22 rows, Sim 2, the list worked out agreeing"
town_want m-C-s7-msgs phone 'page=2 .* inbox=- date=' "menus: no messages"
town_want m-A-gift2-slot2 gift '^gift recipient=22 name=345 slot=1 refused=000000$' "menus: Give Gift to Jebediah, slot 2"
# The hotel and the shops (--print-hotel): the hotel block on any screen, the registry, room info,
# the music panel, Build Rooms, the placement editor, the catalogue and the stat panel, from held-out
# states (copies of the research's M-*, BD-*, BY-*, SP-* and h-catalogue-* in shots/): the early
# hotel (clock 14:00), the complete save (late clock, a copy of the save), the town (town clock).
hotel_run() { # NAME STATE CLASS(hotel|late|town)
    local name=$1 file=$shots/$2.st opts
    if [ ! -f "$file" ]; then echo "no save state $2.st" > "$tmp/$name.hot"; return; fi
    case $3 in
        late) cp "$complete_save" "$tmp/$name.sav"; opts=("${late[@]}");;
        hotel) opts=("${hotel[@]}");;
        *) opts=("${town[@]}");;
    esac
    "$bin" "$rom" --headless --frames 30 --state-load "$file" --save "$tmp/$name.sav" --print-hotel "${opts[@]}" > "$tmp/$name.hotlog" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.hotlog" && touch "$tmp/$name.hotok"
    grep -E '^frame [0-9]+: (hotel|checkin|roominfo|music|build|placement|catalogue|stat) ' "$tmp/$name.hotlog" | sed 's/^frame [0-9]*: //' > "$tmp/$name.hot"
}
hotel_want() { # NAME KIND PATTERN LABEL: the first line of that kind matches (the state's own frame 0)
    if [ -f "$tmp/$1.hotok" ] && grep "^$2 " "$tmp/$1.hot" | head -n 1 | grep -q -E "$3"; then echo "ok   $4"
    else echo "FAIL $4: got '$(grep "^$2 " "$tmp/$1.hot" 2>/dev/null | head -n 1 | cut -c1-300)'"; fail=1; fi
}
hotel_run h-early M-ci-early hotel; hotel_run h-guest M-ci-g-base hotel; hotel_run h-occ M-ci-occ-reg2 hotel; hotel_run h-info M-ci-e-btn1 hotel
hotel_run h-oinfo M-ci-occ-info hotel; hotel_run h-music M-music late; hotel_run h-mdown M-mu-down3 late; hotel_run h-mrow2 M-mu-row2 late
hotel_run h-bd BD-list town; hotel_run h-bd3 BD-down3 town; hotel_run h-bdrow2 BD-row2 town; hotel_run h-bdcas BD-cas town; hotel_run h-bdbuy BD-row2-tick town
hotel_run h-place BD-place-lobby town; hotel_run h-cat h-catalogue-base town; hotel_run h-cat5 BY-sel5 town; hotel_run h-cat4 BY-dn4 town
hotel_run h-own BY-own4-pay town; hotel_run h-stat SP-stat-base late; hotel_run h-statp SP-skills late; hotel_run h-town s7 town
hotel_want h-early hotel '^hotel built=0x6b086c6c unlocked=0x942d950f building=0x0 score=0 stayed=0 money=2000 earned=0 spent=0 place=13 slots=33:63:0,' "hotel: the early hotel, one room built of the buildable, no guests"
hotel_want h-early checkin '^checkin mode=1 guest=-1 score=0 rows=29:ba:-1,8:--:-1,15:--:-1,18:--:-1,20:--:-1$' "hotel: the registry, only the Small Guest Room usable"
hotel_want h-guest checkin '^checkin mode=0 guest=45 ' "hotel: guest check-in mode, the concierge as the guest"
hotel_want h-occ checkin 'rows=29:b-:45,8:--:-1' "hotel: the Small Guest Room occupied by Sim 45"
hotel_want h-info roominfo '^roominfo place=29 occupant=45 occupied=0 mood=-1 checkout=0/0/0 days=0 base=400 service=2 bonus=0 total=422 hint=0$' "hotel: room info vacant, 400 + 2 + 0 = 422"
hotel_want h-oinfo roominfo '^roominfo place=29 occupant=45 occupied=1 mood=2086 checkout=1/1/3 days=2 base=400 service=2 bonus=0 total=422 ' "hotel: room info occupied, out 1/3 in 2 days"
hotel_want h-music hotel 'built=0x1ff3fffff .* score=100 stayed=12 money=14843 earned=73194 spent=62367 place=4 ' "hotel: the complete save, 100 %, 12 guests, §73,194 earned"
hotel_want h-music music '^music scroll=0 playing=2 place=4 roomsong=42 list=3:1:101,4:3:103,5:4:104,.*,22:36:1775,25:23:102$' "hotel: 21 songs, Strange Day playing"
hotel_want h-mdown music '^music scroll=3 playing=2 ' "hotel: music scrolled by 3"
hotel_want h-mrow2 music 'playing=1 place=4 roomsong=3 ' "hotel: the song chosen is the room's"
hotel_want h-bd build '^build scroll=0 page=0 chosen=-1 money=2000 places=0:2000:70,1:1000:69,8:1500:94,12:2500:69:poor,15:1000:94,16:1500:69,18:1000:94,21:3500:70:poor,26:1000:69,28:2000:70,31:2500:69:poor$' "hotel: Build Rooms lists 11 rooms, three too dear"
hotel_want h-bd3 build '^build scroll=3 ' "hotel: Build Rooms scrolled"
hotel_want h-bdrow2 build '^build scroll=0 page=1 chosen=1 ' "hotel: the buy page, the Art Gallery chosen"
hotel_want h-bdcas build 'places=4:1500:69$' "hotel: the casino alone"
hotel_want h-bdbuy build '^build scroll=0 page=0 chosen=-1 money=1000 places=0:2000:70:poor,8:' "hotel: after a purchase, 10 rooms, money 1000"
hotel_want h-place placement '^placement mode=1 room=5 phase=1 slot=0 object=1 rot=3 objects=86:5:14:0,114:71:58:0,133:49:27:0,185:6:73:1,209:2:77:0$' "hotel: the placement editor, five objects"
hotel_want h-cat catalogue '^catalogue shop=51 vendor=0 sel=0 sell=0 scroll=0 pay=1 up=0 down=1 ring=0 money=2000 items=106:569:1991:160,132:595:2276:150,191:654:7251:130,215:678:7914:24,238:701:7420:100,84:547:3509:350,105:568:8844:3000,155:618:2861:500,227:690:7979:250,103:566:2100:5000,92:555:3606:700,99:562:2483:400 owned=-$' "hotel: the General Store's twelve items and prices"
hotel_want h-cat5 catalogue 'sel=4 sell=0 .* ring=1 ' "hotel: the fifth item selected"
hotel_want h-cat4 catalogue 'sel=9 sell=0 scroll=9 pay=0 up=1 down=0 ' "hotel: scrolled to the end, pay disabled (too dear)"
hotel_want h-own catalogue 'sel=3 sell=1 .* money=1120 .* owned=106:569:1991:80,106:569:1991:80,106:569:1991:80,106:569:1991:80,106:569:1991:80$' "hotel: five owned beds at §80, one selected for sale"
hotel_want h-stat stat '^stat page=0 transition=0 aspiration=0 level=2 points=29 gauge=19 skills=2,2,1,1,6 score=100 stayed=12 earned=73194 spent=62367 aliens=22 goons=12 vacuum=359 metal=2020$' "hotel: the stat panel's values"
hotel_want h-statp stat '^stat page=3 ' "hotel: the Skills page"
hotel_want h-town hotel '^hotel built=' "hotel: the block reads in the town too"
hotel_want h-town checkin '^checkin none$' "hotel: no registry on the town's screen"
# The front end (--print-front; milestone 5): the title and menu, Settings, Load-a-Sim, Create-a-Sim's pages, Keep this Sim?
# and the boot screens, from copies of the research's F-* states in shots/. The first line of each kind (the state's own frame 0).
front_run() { # NAME STATE: the front lines in NAME.front
    local name=$1 file=$shots/$2.st
    [ -f "$file" ] || file=$more_shots/$2.st
    if [ ! -f "$file" ]; then echo "no save state $2.st" > "$tmp/$name.front"; return; fi
    "$bin" "$rom" --headless --frames 30 --state-load "$file" --save "$tmp/$name.sav" --print-front "${town[@]}" > "$tmp/$name.frontlog" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.frontlog" && touch "$tmp/$name.frontok"
    grep -E '^frame [0-9]+: (front|title|settings|load|cas|cas-sprites|boot|video) ' "$tmp/$name.frontlog" | sed 's/^frame [0-9]*: //' > "$tmp/$name.front"
}
front_want() { # NAME KIND PATTERN LABEL: the last line of that kind matches
    if [ -f "$tmp/$1.frontok" ] && grep "^$2 " "$tmp/$1.front" | tail -n 1 | grep -q -E "$3"; then echo "ok   $4"
    else echo "FAIL $4: got '$(grep "^$2 " "$tmp/$1.front" 2>/dev/null | tail -n 1 | cut -c1-300)'"; fail=1; fi
}
for st in F-title0 F-mm-touch-150 F-mm-load-150 F-mm-opt-150 F-mm-wl-150 F-cas-a-30 F-cas-male-40 F-cas-fig3-40 F-cas-kabc-a-80 F-cas-k123-40 F-cas-kabcdef-200 F-cas-kbs4-140 \
          F-cas-tab2-60 F-cas-tab3-60 F-cas-tab4-60 F-hair-c8-40 F-hair-s3-40 F-hair-next3-130 F-face-5-40 F-thr-cat3-40 F-thr-6-40 F-keep-asp2-30 F-keep-tick-30 F-keep-cross-30 \
          F-opt-sfx-40 F-opt-concept-40 F-opt-credits-400 F-opt-music-drag-80 F-copt-concept-80 F-cload-150 F-cload-t1-60 F-boot-900 F-boot-skip-1500; do front_run "$st" "$st"; done
front_want F-title0 front '^front Title$' "front: the touch-to-begin title"
front_want F-title0 title '^title phase=0 menu=0 button=-1$' "front: title phase 0, no button"
front_want F-mm-touch-150 front '^front MainMenu$' "front: the main menu"
front_want F-mm-touch-150 title '^title phase=3 menu=1 ' "front: main menu phase 3"
front_want F-boot-skip-1500 front '^front MainMenu$' "front: the boot, skipped, ends on the main menu"
front_want F-mm-load-150 load '^load wireless=0 green=-1 tapped=-1 rows=-,-,-$' "front: Load-a-Sim, three empty slots"
front_want F-mm-wl-150 front '^front Wireless$' "front: DS Wireless"
front_want F-cload-150 load '^load wireless=0 green=0 tapped=-1 rows=Chris:3226:70:11/21 12:52am,Clare:1055:86:11/16 1:30pm,Jun:9150:87:11/20 8:03pm$' "front: the three saves, the game's own date and time"
front_want F-cload-t1-60 load 'tapped=0 ' "front: the first slot's tick tapped"
front_want F-mm-opt-150 settings '^settings page=0 music=4096 sfx=4096 art=0 concept=\? credits=\?$' "front: Settings opens on Music at 100 %"
front_want F-opt-sfx-40 settings '^settings page=1 ' "front: the SFX page"
front_want F-opt-concept-40 settings '^settings page=2 .* concept=0 ' "front: Concept Art, none unlocked without a save"
front_want F-copt-concept-80 settings '^settings page=2 .* concept=8 ' "front: Concept Art, the third unlocked (the complete save)"
front_want F-opt-credits-400 settings '^settings page=3 .* credits=[0-9]+/[0-9]+$' "front: Credits scrolling"
front_want F-opt-music-drag-80 settings '^settings page=0 music=1296 ' "front: the Music slider dragged to 32 %"
front_want F-cas-a-30 front '^front CreateASim$' "front: Create-a-Sim"
front_want F-cas-a-30 cas '^cas page=0 gender=0 simname=Sim skin=0 haircolour=0 name=Sim len=3 keyboard=0$' "front: the Name page, Sim, ABC"
front_want F-cas-male-40 cas '^cas page=0 gender=1 ' "front: the male Sim"
front_want F-cas-fig3-40 cas ' skin=2 ' "front: the third skin figure"
front_want F-cas-kabc-a-80 cas ' name=Sima len=4 keyboard=1$' "front: abc keyboard, Sima"
front_want F-cas-k123-40 cas ' keyboard=2$' "front: the 123! keyboard"
front_want F-cas-kabcdef-200 cas ' name=SimABCD len=7 ' "front: the name stops at 7 characters"
front_want F-cas-kbs4-140 cas ' name= len=0 ' "front: the name emptied by backspace"
front_want F-cas-tab2-60 cas '^cas page=1 .* hair=0 scroll=0$' "front: the Hair page"
front_want F-hair-c8-40 cas 'haircolour=7 hair=0 ' "front: hair colour 8"
front_want F-hair-s3-40 cas ' hair=2 scroll=0$' "front: the third hair style"
front_want F-hair-next3-130 cas ' hair=0 scroll=3$' "front: the hair scrolled by 3"
front_want F-cas-tab3-60 cas '^cas page=2 .* face=0$' "front: the Face page"
front_want F-face-5-40 cas ' face=4$' "front: face 5"
front_want F-cas-tab4-60 cas '^cas page=3 .* category=0 clothespage=0 item=0$' "front: the Clothes page"
front_want F-thr-cat3-40 cas ' category=2 ' "front: the third clothes category"
front_want F-thr-6-40 cas ' item=16$' "front: the sixth cell of Clothes"
front_want F-cas-tab2-60 cas-sprites '^cas-sprites left=0 right=1 cell=-1 pos=0$' "front: the Hair page, its right arrow alone"
front_want F-hair-s3-40 cas-sprites ' pos=9$' "front: the third hair style at row 2, column 1"
front_want F-hair-next3-130 cas-sprites '^cas-sprites left=1 right=1 ' "front: the hair scrolled, both arrows"
front_want F-thr-6-40 cas-sprites '^cas-sprites left=0 right=1 cell=5 ' "front: Clothes, the game's ring on the sixth cell"
front_want F-thr-cat3-40 cas-sprites '^cas-sprites left=0 right=1 cell=0 ' "front: the third category's first page"
front_want F-keep-asp2-30 front '^front KeepSim$' "front: Keep this Sim?"
front_want F-keep-asp2-30 cas ' aspiration=2 answer=0$' "front: Popularity chosen"
front_want F-keep-tick-30 cas ' aspiration=0 answer=1$' "front: the tick pressed"
front_want F-keep-cross-30 cas ' answer=2$' "front: the cross pressed"
front_want F-boot-900 front '^front Video$' "front: the intro video"
front_want F-boot-900 video '^video movie=3$' "front: the intro is movie 3"
front_want F-boot-900 boot '^boot mode=0 ' "front: touch to continue under the video"
front_want F-title0 settings '^settings none$' "front: no Settings on the title"
front_want F-cas-a-30 title '^title none$' "front: no title in Create-a-Sim"

exit $fail
