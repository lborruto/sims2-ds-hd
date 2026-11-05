#!/usr/bin/env bash
# Checks which bottom screen the game is recognised to show (src/screens.*), from save states:
# each state runs 120 frames headless with --print-screen and must settle on its screen, and
# nothing else; from s6, scripts/tabs.txt walks the panel's tabs and opens the phone.
# Needs the ROM and the save states (not in git, so not in CI):
#   bash tests/screens_check.sh
# Override with SIMS2WIDE (the program), ROM, SHOTS (the save states' folder) and CARDS_SAVE
# (the complete save act-cards was made on); they default to this checkout's build/, roms/,
# shots/ and saves/complete-play.sav.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms" -name "*.nds" 2>/dev/null | head -n 1)}
shots=${SHOTS:-$repo/shots}
cards_save=${CARDS_SAVE:-$repo/saves/complete-play.sav}
[ -x "$bin" ] || { echo "no program at $bin (ninja -C build sims2wide)"; exit 2; }
[ -f "$rom" ] || { echo "no ROM (set ROM)"; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# run NAME STATE FRAMES [options...]: the whole log in NAME.log, the screens printed (one
# "frame N: screen X" per line) in NAME.out, and NAME.ok only if the run went to its end (exit
# status 0 and the headless end line, the DS clock's): an empty .out reads as None, so a crash
# or a missing state must not pass for it.
run() {
    local name=$1 state=$2 frames=$3; shift 3
    if [ ! -f "$shots/$state.st" ]; then echo "no save state $shots/$state.st" > "$tmp/$name.log"; : > "$tmp/$name.out"; return; fi
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" \
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
# state:expected (None: nothing printed, the tracker starts there)
cases=(
    "s7:Phone" "sit-dialogue:Dialogue" "s5:Dialogue" "s6:Popup" "sit-gift:Gift"
    "act-social:Social" "sit-storebuy:Catalogue" "sit-build:Build" "sit-store:Panel/Interact"
    "sit-cityhall:Panel/Interact" "s2:None" "s3:None" "s4:None"
    "sit-hotel:Panel/Interact" "act-vacuum:Vacuum" "act-vacuum-clog:Vacuum" "act-cards:Cards"
)
for c in "${cases[@]}"; do
    st=${c%%:*}
    case $st in
        sit-hotel|act-vacuum|act-vacuum-clog) run "$st" "$st" 120 "${hotel[@]}" & ;;
        act-cards)
            if [ -f "$cards_save" ]; then cp "$cards_save" "$tmp/act-cards.sav"
            else echo "warning: no $cards_save (CARDS_SAVE): act-cards runs without its save"; fi
            run "$st" "$st" 120 & ;;
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
