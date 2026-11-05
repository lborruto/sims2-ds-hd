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
exit $fail
