#!/usr/bin/env bash
# Checks the touch targets of src/hotspots.cpp in the game: each script of scripts/hotspots/
# taps targets from a save state headless, and the run must show the tap's effect: either the
# recognised screens (--print-screen, the whole sequence) or a last picture that differs from
# the same run without that tap (a base script). Targets whose tap shows nothing (an empty slot,
# a list with one entry) are placed from shots only: CLAUDE.md, traps.
# Needs the ROM and the save states (not in git, so not in CI):
#   bash tests/hotspots_check.sh
# Override with SIMS2WIDE, ROM, SHOTS and COMPLETE_SAVE (or CARDS_SAVE) as for
# tests/screens_check.sh. The casino checks start from act-cards on its complete save and quit
# the card game first (the casino's panel reads Panel/Interact).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms" -name "*.nds" 2>/dev/null | head -n 1)}
shots=${SHOTS:-$repo/shots}
complete_save=${COMPLETE_SAVE:-${CARDS_SAVE:-$repo/saves/complete-play.sav}}
dir=$repo/scripts/hotspots
[ -x "$bin" ] || { echo "no program at $bin (ninja -C build sims2wide)"; exit 2; }
[ -f "$rom" ] || { echo "no ROM (set ROM)"; exit 2; }
[ -f "$complete_save" ] || { echo "no $complete_save (COMPLETE_SAVE): the card game and casino checks need it"; exit 2; }
tmp=$(mktemp -d)
# KEEP=1 keeps the runs (logs, pictures) and says where.
[ -n "${KEEP:-}" ] && echo "keeping $tmp" || trap 'rm -rf "$tmp"' EXIT

# script:state:frames:expect, expect = "screens=A B C" (the screens printed, in order) or
# "differs=BASE" (the last picture differs from the run of scripts/hotspots/BASE.txt).
cases=(
    "tab-pockets:sit-store:120:screens=Panel/Interact Panel/Pockets"
    "tab-goals:sit-store:120:screens=Panel/Interact Panel/Goals"
    "tab-relationships:sit-store:120:screens=Panel/Interact Panel/Relationships"
    "tab-options:sit-store:120:screens=Panel/Interact Panel/Options"
    "tab-interact:sit-store:160:screens=Panel/Interact Panel/Pockets Panel/Interact"
    "panel-phone:sit-store:160:screens=Panel/Interact Phone"
    "interact-action-1:sit-store:200:differs=interact-base"
    "interact-action-2:act-cards:1200:screens=Cards None Panel/Interact None Cards"
    "interact-action-3:act-cards:1200:differs=casino-base"
    "pockets-slot-1:act-cards:1200:differs=pockets-base"
    "pockets-bin:act-cards:1200:differs=pockets-slot-1"
    "pockets-key-item-1:act-cards:1200:differs=pockets-base"
    "pockets-key-item-2:act-cards:1200:differs=pockets-base"
    "pockets-key-item-3:act-cards:1200:differs=pockets-base"
    "pockets-hand:act-cards:1200:differs=pockets-key-item-2"
    "pockets-counted-1:act-cards:1200:differs=pockets-base"
    "pockets-counted-2:act-cards:1200:differs=pockets-base"
    "pockets-counted-3:act-cards:1200:differs=pockets-base"
    "pockets-counted-4:act-cards:1200:differs=pockets-base"
    "relationships-sim-1:act-cards:1200:differs=relationships-sim-2-base"
    "relationships-sim-2:act-cards:1200:differs=relationships-base"
    "relationships-sim-3:act-cards:1200:differs=relationships-sim-2-base"
    "relationships-sim-4:act-cards:1200:differs=relationships-sim-2-base"
    "relationships-next:act-cards:1200:differs=relationships-base"
    "relationships-previous:act-cards:1200:differs=relationships-sim-2-base"
    "options-save:sit-store:200:screens=Panel/Interact Panel/Options Unknown"
    "options-quit:sit-store:200:screens=Panel/Interact Panel/Options Unknown"
    "options-music:sit-store:200:differs=options-base"
    "options-sound:sit-store:200:differs=options-base"
    "phone-down:s7:120:differs=phone-base"
    "phone-up:s7:120:differs=phone-down"
    "phone-ok:s7:120:differs=phone-base"
    "phone-close:s7:120:screens=Phone Panel/Interact"
    "dialogue-next:sit-dialogue:120:screens=Dialogue Panel/Interact"
    "popup-goal:s6:120:screens=Popup Panel/Interact Popup"
    "popup-tip:s6:200:screens=Popup Panel/Interact Popup Panel/Interact"
    "popup-paused:sit-pause:120:screens=Popup Panel/Interact"
    "gift-close:sit-gift:120:screens=Gift Panel/Interact"
    "social-left:act-social:200:differs=social-base"
    "social-middle:act-social:200:differs=social-base"
    "social-right:act-social:200:differs=social-base"
    "cards-deck:act-cards:200:differs=cards-base"
    "cards-help:act-cards:200:differs=cards-base"
    "cards-quit:act-cards:200:differs=cards-base"
    "cards-card-1:act-cards:200:differs=cards-draw-base"
    "cards-card-2:act-cards:200:differs=cards-draw-base"
    "cards-card-3:act-cards:200:differs=cards-draw-base"
    "cards-card-4:act-cards:200:differs=cards-draw-base"
    "cards-card-5:act-cards:200:differs=cards-draw-base"
    "cards-card-6:act-cards:200:differs=cards-draw-base"
    "cards-card-7:act-cards:200:differs=cards-draw-base"
    "cards-card-8:act-cards:200:differs=cards-draw-base"
    "cards-card-9:act-cards:200:differs=cards-draw-base"
    "catalogue-item-1:sit-storebuy:160:differs=catalogue-item-4-base"
    "catalogue-item-2:sit-storebuy:160:differs=catalogue-base"
    "catalogue-item-3:sit-storebuy:160:differs=catalogue-base"
    "catalogue-item-4:sit-storebuy:160:differs=catalogue-base"
    "catalogue-item-5:sit-storebuy:160:differs=catalogue-base"
    "catalogue-item-6:sit-storebuy:160:differs=catalogue-base"
    "catalogue-scroll-down:sit-storebuy:160:differs=catalogue-base"
    "catalogue-scroll-up:sit-storebuy:160:differs=catalogue-scroll-down-base"
    "catalogue-pay:sit-storebuy:160:differs=catalogue-base"
    "catalogue-owned-1:sit-storebuy:300:differs=catalogue-owned-2-base"
    "catalogue-owned-2:sit-storebuy:300:differs=catalogue-owned-base"
    "catalogue-owned-3:sit-storebuy:300:differs=catalogue-owned-base"
    "catalogue-owned-4:sit-storebuy:300:differs=catalogue-owned-base"
    "catalogue-owned-5:sit-storebuy:300:differs=catalogue-owned-base"
    "catalogue-owned-6:sit-storebuy:300:differs=catalogue-owned-base"
    "catalogue-close:sit-storebuy:120:screens=Catalogue Panel/Interact"
    "build-close:sit-build:120:screens=Build Panel/Interact"
)

# run SCRIPT STATE FRAMES: the screens printed in SCRIPT@FRAMES.out, the last picture in
# SCRIPT@FRAMES/, SCRIPT@FRAMES.ok only if the run went to its end.
run() {
    local name=$1 state=$2 frames=$3 out=$tmp/$1@$3
    mkdir -p "$out"
    if [ ! -f "$shots/$state.st" ]; then echo "no save state $shots/$state.st" > "$out.log"; : > "$out.out"; return; fi
    # The town's states on the clock their output is compared with; act-cards after the time it
    # was saved at (never earlier: CLAUDE.md, the clock), or the pictures would differ by the
    # second the run started.
    local opts=(--clock "2026-01-01 12:00")
    [ "$state" = act-cards ] && { cp "$complete_save" "$out.sav"; opts=(--clock "2026-11-05 12:00"); }
    "$bin" "$rom" --headless --frames "$frames" --state-load "$shots/$state.st" --save "$out.sav" \
        --input "$dir/$name.txt" --print-screen --shot-dir "$out" --shot-at $((frames - 1)) "${opts[@]}" \
        > "$out.log" 2>&1 && grep -q '^clock: ' "$out.log" && touch "$out.ok"
    grep -E '^frame [0-9]+: screen ' "$out.log" | sed 's/^frame [0-9]*: screen //' | tr '\n' ' ' | sed 's/ $//' > "$out.out"
}

# Every run once (the bases too), a few at a time.
jobs_max=${JOBS:-8}
runs=()
for c in "${cases[@]}"; do
    IFS=: read -r name state frames expect <<< "$c"
    runs+=("$name:$state:$frames")
    case $expect in differs=*) runs+=("${expect#differs=}:$state:$frames") ;; esac
done
for r in $(printf '%s\n' "${runs[@]}" | sort -u); do
    IFS=: read -r name state frames <<< "$r"
    while [ "$(jobs -rp | wc -l)" -ge "$jobs_max" ]; do sleep 0.2; done
    run "$name" "$state" "$frames" &
done
wait

fail=0
ended() { [ -f "$tmp/$1.ok" ] && return 0; echo "FAIL $1: the run did not end well: $(tail -n 1 "$tmp/$1.log")"; return 1; }
for c in "${cases[@]}"; do
    IFS=: read -r name state frames expect <<< "$c"
    ended "$name@$frames" || { fail=1; continue; }
    case $expect in
        screens=*)
            want=${expect#screens=} got=$(cat "$tmp/$name@$frames.out")
            if [ "$got" = "$want" ]; then echo "ok   $name: $got"
            else echo "FAIL $name: want '$want', got '$got'"; fail=1; fi ;;
        differs=*)
            base=${expect#differs=}
            ended "$base@$frames" || { fail=1; continue; }
            a=$(ls "$tmp/$name@$frames"/*.png 2>/dev/null) b=$(ls "$tmp/$base@$frames"/*.png 2>/dev/null)
            if [ -z "$a" ] || [ -z "$b" ]; then echo "FAIL $name: no picture"; fail=1
            elif cmp -s "$a" "$b"; then echo "FAIL $name: the same picture as $base"; fail=1
            else echo "ok   $name: differs from $base"; fi ;;
    esac
done
exit $fail
