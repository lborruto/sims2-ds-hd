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
exit $fail
