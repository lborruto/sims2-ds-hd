#!/usr/bin/env bash
# Checks which bottom screen the game is recognised to show (src/screens.*), from save states:
# each state runs 120 frames headless with --print-screen and must settle on its screen, and
# nothing else; from s6, scripts/tabs.txt walks the panel's tabs and opens the phone; from
# sit-storebuy, items whose description takes two lines are selected in the catalogue.
# Needs the ROM and the save states (not in git, so not in CI):
#   bash tests/screens_check.sh
# Override with SIMS2WIDE (the program), ROM, SHOTS (the save states' folder), MORE_SHOTS (a
# second folder, looked in when SHOTS lacks a state: a worktree's own shots/ besides the main
# checkout's) and COMPLETE_SAVE (the downloaded complete save the casino and late hotel states
# were made on; CARDS_SAVE is its old name); they default to this checkout's build/, roms/,
# shots/, shots/ and saves/complete-play.sav.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms" -name "*.nds" 2>/dev/null | head -n 1)}
shots=${SHOTS:-$repo/shots}
more_shots=${MORE_SHOTS:-$repo/shots}
complete_save=${COMPLETE_SAVE:-${CARDS_SAVE:-$repo/saves/complete-play.sav}}
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

# run NAME STATE FRAMES [options...]: the whole log in NAME.log, the screens printed (one
# "frame N: screen X" per line) in NAME.out, and NAME.ok only if the run went to its end (exit
# status 0 and the headless end line, the DS clock's): an empty .out reads as None, so a crash
# or a missing state must not pass for it.
run() {
    local name=$1 state=$2 frames=$3 file; shift 3
    file=$shots/$state.st
    [ -f "$file" ] || file=$more_shots/$state.st
    if [ ! -f "$file" ]; then echo "no save state $state.st in $shots or $more_shots" > "$tmp/$name.log"; : > "$tmp/$name.out"; return; fi
    "$bin" "$rom" --headless --frames "$frames" --state-load "$file" \
        --save "$tmp/$name.sav" --print-screen "$@" > "$tmp/$name.log" 2>&1 &&
        grep -q '^clock: ' "$tmp/$name.log" && touch "$tmp/$name.ok"
    grep -E '^frame [0-9]+: screen ' "$tmp/$name.log" > "$tmp/$name.out"
}

# ended NAME: the run went to its end; otherwise says why and fails.
ended() {
    [ -f "$tmp/$1.ok" ] && return 0
    echo "FAIL $1: the run did not end well: $(tail -n 1 "$tmp/$1.log")"
    return 1
}

town=(--clock "2026-01-01 12:00")
hotel=(--clock "2026-01-01 14:00")
# The complete save's states: the casino (sit-casino: act-cards quit to the casino's panel) and
# the late hotel lobby (sit-hotel-late), its status bar's motive bar yellow, not red.
late=(--clock "2026-11-05 12:05")
# state:expected (None: nothing printed, the tracker starts there)
cases=(
    "s7:Phone" "sit-dialogue:Dialogue" "s5:Dialogue" "s6:Popup" "sit-gift:Gift"
    "act-social:Social" "sit-storebuy:Catalogue" "sit-build:Build" "sit-store:Panel/Interact"
    "sit-cityhall:Panel/Interact" "s2:None" "s3:None" "s4:None"
    "sit-hotel:Panel/Interact" "act-vacuum:Vacuum" "act-vacuum-clog:Vacuum" "act-cards:Cards"
    "sit-casino:Panel/Interact" "sit-hotel-late:Panel/Interact"
    "storebuy-long3:Catalogue" "storebuy-long4:Catalogue"
)
for c in "${cases[@]}"; do
    st=${c%%:*}
    case $st in
        sit-hotel|act-vacuum|act-vacuum-clog) run "$st" "$st" 120 "${hotel[@]}" & ;;
        act-cards|sit-casino|sit-hotel-late)
            if [ -f "$complete_save" ]; then cp "$complete_save" "$tmp/$st.sav"
            else echo "warning: no $complete_save (COMPLETE_SAVE): $st runs without its save"; fi
            if [ "$st" = act-cards ]; then run "$st" "$st" 120 &
            else run "$st" "$st" 120 "${late[@]}" &
            fi ;;
        # The catalogue with Red Shower (item 3) or Crimson Toilet (item 4) selected: two lines
        # of description.
        storebuy-long3|storebuy-long4)
            run "$st" sit-storebuy 160 --input "$repo/scripts/hotspots/catalogue-item-${st#storebuy-long}.txt" "${town[@]}" & ;;
        *) run "$st" "$st" 120 "${town[@]}" & ;;
    esac
done
run tabs s6 1650 --input "$repo/scripts/tabs.txt" "${town[@]}" &
wait

fail=0
for c in "${cases[@]}"; do
    st=${c%%:*} want=${c#*:}
    ended "$st" || { fail=1; continue; }
    got=$(sed 's/^frame [0-9]*: screen //' "$tmp/$st.out" | tr '\n' ' ' | sed 's/ $//')
    [ "$want" = None ] && want=""
    if [ "$got" = "$want" ]; then echo "ok   $st: ${want:-None}"
    else echo "FAIL $st: want '${want:-None}', got '${got:-None}'"; fail=1; fi
done

# tabs.txt: the popups closed (frames 10, 80), then each tab touched at 450, 650, 850, 1050 and
# the phone at 1250; each must be recognised within 60 frames of its touch.
want_seq="Popup Panel/Interact Popup Panel/Interact Panel/Pockets Panel/Goals Panel/Relationships Panel/Options Phone"
got_seq=$(sed 's/^frame [0-9]*: screen //' "$tmp/tabs.out" | tr '\n' ' ' | sed 's/ $//')
if ! ended tabs; then fail=1
elif [ "$got_seq" != "$want_seq" ]; then echo "FAIL tabs: want '$want_seq', got '${got_seq:-None}'"; fail=1
else
    late=0
    for pair in "Panel/Pockets:450" "Panel/Goals:650" "Panel/Relationships:850" "Panel/Options:1050" "Phone:1250"; do
        s=${pair%%:*} t=${pair#*:}
        f=$(grep -E ": screen $s\$" "$tmp/tabs.out" | sed 's/^frame \([0-9]*\):.*/\1/')
        if [ "$f" -lt "$t" ] || [ "$f" -gt $((t + 60)) ]; then echo "FAIL tabs: $s at frame $f, touched at $t"; late=1; fi
    done
    [ $late = 0 ] && echo "ok   tabs: $got_seq" || fail=1
fi
exit $fail
