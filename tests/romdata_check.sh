#!/usr/bin/env bash
# Checks RomData (src/romdata.*): the game's strings read from the player's ROM, in its five
# languages, then its compressed resources and its sprites. --strings-count must print 3127
# for languages 1 to 5, and without --language for the firmware's (English on melonDS's
# built-in firmware, GameLanguage); --dump-string of three English ids must equal the
# research's decoded lines at those ids, and a French string with an "é" must come out as
# UTF-8 "é". --dump-sprite of the panel's tab icons (2247 green, 2250 blue), the phone button
# (2230), the phone's own buttons and a few item icons must decode every frame, and a bad
# resource, frame or number must be refused. Then, with the research folder: --dump-resource
# of a sample of the compressed resources (every 100th of the game's own kind, which every
# one of them is) must equal the research's outputs (the game's decoder run under an
# emulator, extract/dec/NNNNN.dec), and the sprites' frames must hold the research's rendered
# pixels (its sheets, m3/frames/spr_NNNNN_all.png beside the folder, drew each frame's first
# piece only). The expected data is read at run time from the research folder (RESEARCH=,
# the folder holding text/strings_NNNNN.txt: 00123 English, 03496 French), never copied into
# the repository: without it those parts are skipped with a message; the sprites' pixels also
# need a Python with Pillow (PYTHON=).
# Needs the ROM (not in git, so not in CI):
#   RESEARCH=/path/to/research/rom PYTHON=python3 bash tests/romdata_check.sh
# Override with SIMS2WIDE (the program) and ROM; they default to this checkout's build/ and roms/.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms/" -name "*.nds" 2>/dev/null | head -n 1)}
research=${RESEARCH:-}
python=${PYTHON:-python3}
[ -x "$bin" ] || { echo "no program at $bin (ninja -C build sims2wide)"; exit 2; }
[ -f "$rom" ] || { echo "no ROM (set ROM)"; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0

# game_text OPTIONS...: the program on the ROM with the text's options; what it prints, its
# messages in $tmp/err (one line: the language, the file and the count), its status.
game_text() { "$bin" "$rom" --headless --frames 0 --save "$tmp/scratch.sav" "$@" 2> "$tmp/err"; }
said() { tail -n 1 "$tmp/err"; }

# Each language has the whole table.
for lang in 1 2 3 4 5; do
    got=$(game_text --language "$lang" --strings-count)
    if [ "$got" = 3127 ]; then echo "ok   language $lang: 3127 strings ($(said))"
    else echo "FAIL language $lang: want 3127, got '${got:-nothing}' ($(said))"; fail=1; fi
done
# Language 0 (Japanese) reads the English file, as the game's own table says.
got=$(game_text --language 0 --strings-count)
if [ "$got" = 3127 ] && grep -q '^strings: language 0 from file 123,' "$tmp/err"; then echo "ok   language 0: the English file ($(said))"
else echo "FAIL language 0: want 3127 strings from file 123, got '${got:-nothing}' ($(said))"; fail=1; fi
# Without --language, the firmware's: melonDS's built-in firmware is English.
got=$(game_text --strings-count)
if [ "$got" = 3127 ] && grep -q '^strings: language 1 from file 123,' "$tmp/err"; then echo "ok   firmware language: $(said)"
else echo "FAIL firmware language: want 3127 strings of language 1, got '${got:-nothing}' ($(said))"; fail=1; fi
# No such language, no such string: a failure, not a guess.
if game_text --language 6 --strings-count > /dev/null; then echo "FAIL language 6: accepted"; fail=1
else echo "ok   language 6: refused ($(said))"; fi
if game_text --language 1 --dump-string 3127 > /dev/null; then echo "FAIL string 3127: accepted"; fail=1
else echo "ok   string 3127: refused ($(said))"; fi
# Not a number, or a negative one: the usage, not a 0.
if game_text --language fr --strings-count > /dev/null; then echo "FAIL language fr: accepted"; fail=1
else echo "ok   language fr: refused ($(grep -c . "$tmp/err") lines of usage)"; fi
if game_text --language 1 --dump-string -1 > /dev/null; then echo "FAIL string -1: accepted"; fail=1
else echo "ok   string -1: refused ($(grep -c . "$tmp/err") lines of usage)"; fi

# The sprites: every frame of the icons the HD menus use decodes to a PNG of the frame's size
# (its sizes and pieces as the game's descriptors give them); the ones with 6 or 5 frames are
# the phone button and the tab icons.
# sprite ID FRAME: the frame dumped to $tmp/spr/ID_FRAME.png; what the program said in $tmp/err.
mkdir -p "$tmp/spr"
sprite() { game_text --dump-sprite "$1" "$2" "$tmp/spr/$1_$2.png"; }
icons="2247:5 2250:5 2230:6 2233:6 2419:6 2506:1 2212:1 2215:1 3353:1 4157:1 8448:1 8445:1 5806:1 5803:1 2755:1 2752:1 3068:1 3065:1 7683:1 7680:1 1991:1 8553:1 5033:1 321:1 6667:1 6661:1 7665:1 974:1 7698:1 7677:1 8451:2"
for entry in $icons; do
    id=${entry%%:*}; want=${entry##*:}; got=0; bad=""
    f=0
    while [ "$f" -lt "$want" ]; do
        if sprite "$id" "$f" && grep -q "^sprite $id: $want frames; frame $f: [0-9]* x [0-9]*, [0-9]* pieces" "$tmp/err" && [ -s "$tmp/spr/${id}_$f.png" ]; then got=$((got + 1))
        else bad="$bad $f"; fi
        f=$((f + 1))
    done
    if [ "$got" = "$want" ]; then echo "ok   sprite $id: $want frame(s) ($(said))"
    else echo "FAIL sprite $id: frames$bad did not decode ($(said))"; fail=1; fi
done
# Not a sprite, a frame the sprite lacks, a resource the archive lacks, not a number: refused.
if sprite 123 0 > /dev/null; then echo "FAIL sprite 123: the string table accepted as a sprite"; fail=1
else echo "ok   sprite 123: refused ($(said))"; fi
if sprite 2247 5 > /dev/null; then echo "FAIL sprite 2247 frame 5: accepted"; fail=1
else echo "ok   sprite 2247 frame 5: refused ($(said))"; fi
if sprite 99999 0 > /dev/null; then echo "FAIL sprite 99999: accepted"; fail=1
else echo "ok   sprite 99999: refused ($(said))"; fi
if sprite 2247 -1 > /dev/null || sprite x 0 > /dev/null; then echo "FAIL sprite -1 / x: accepted"; fail=1
else echo "ok   sprite 2247 -1, x 0: refused ($(grep -c . "$tmp/err") lines of usage)"; fi
if game_text --dump-resource 99999 "$tmp/r.bin" > /dev/null || game_text --dump-resource x "$tmp/r.bin" > /dev/null; then echo "FAIL resource 99999 / x: accepted"; fail=1
else echo "ok   resource 99999, x: refused"; fi

# The text itself, against the research's decoded lines (id, a tab, a Python repr of the
# cp1252-decoded bytes).
if [ -z "$research" ] || [ ! -f "$research/text/strings_00123.txt" ]; then
    echo "skip the strings' text, the compression and the sprites' pixels: no research folder (RESEARCH=... holding text/strings_00123.txt)"
    exit $fail
fi
# expected FILE ID: that line's text, unquoted. For the ids chosen here (plain ASCII, no
# escape but \n) the repr is the text itself, with a newline written as the two characters
# "\n", which is how --dump-string prints one.
expected() { grep -m 1 "^$2	" "$1" | cut -f 2- | sed "s/^['\"]//; s/['\"]$//"; }
for id in 0 5 23; do
    want=$(expected "$research/text/strings_00123.txt" "$id")
    got=$(game_text --language 1 --dump-string "$id")
    if [ -n "$want" ] && [ "$got" = "$want" ]; then echo "ok   English $id: $got"
    else echo "FAIL English $id: want '$want', got '${got:-nothing}' ($(said))"; fail=1; fi
done
# French: the first string made of letters, spaces and the game's "é" (the byte 0xA3, "£" in
# cp1252) must come out with a UTF-8 "é" in its place.
line=$(grep -m 1 -E "^[0-9]+	'[A-Za-z ]*£[A-Za-z ]*'$" "$research/text/strings_03496.txt")
id=${line%%	*}
want=$(expected "$research/text/strings_03496.txt" "$id")
want=${want//£/é}
got=$(game_text --language 2 --dump-string "$id")
if [ -n "$line" ] && [ "$got" = "$want" ] && [[ $got == *é* ]]; then echo "ok   French $id: $got"
else echo "FAIL French ${id:-?}: want '$want', got '${got:-nothing}' ($(said))"; fail=1; fi

# The compression, against the research's outputs: extract/decomp_index.json names each
# compressed resource's kind ("c6" the game's own, "c6d" with the delta, "c1" LZ10) and its
# packed and unpacked sizes, and extract/dec/NNNNN.dec holds what the game's own decoder
# made of it under an emulator. Every LZ10 and delta one, and every 100th of the others,
# must come out the same. Resources under 64 bytes are left out: the research's heuristic
# took the four 32-byte palettes that begin 0xE0 0x03 (green) for delta streams of over a
# megabyte, and 32 sprite descriptors 16 pixels wide (0x10 0xNN 0x00 0x00, 16 to 40 bytes)
# for LZ10 streams; the port refuses the former (their stream runs out) and the game has
# neither: its compressed resources are all of its own kind (5464), or stored (16).
index=$research/extract/decomp_index.json
if [ ! -f "$index" ] || [ ! -d "$research/extract/dec" ]; then
    echo "skip the compression: no $index or extract/dec/"
else
    grep -o '"[0-9]*": \["c[16]d\{0,1\}", [0-9]*' "$index" | sed 's/"\([0-9]*\)": \["\(c[16]d\{0,1\}\)", \([0-9]*\)/\1 \2 \3/' | awk '$3 >= 64 {print $1, $2}' | sort -n > "$tmp/compressed.txt"
    { grep -v ' c6$' "$tmp/compressed.txt"; grep ' c6$' "$tmp/compressed.txt" | awk 'NR % 100 == 0'; } | sort -n > "$tmp/sample.txt"
    total=0; same=0; bad=""
    while read -r id kind; do
        total=$((total + 1))
        dec=$(printf '%s/extract/dec/%05d.dec' "$research" "$id")
        if game_text --dump-resource "$id" "$tmp/r.bin" && grep -q "^resource $id: kind [16], " "$tmp/err" && cmp -s "$tmp/r.bin" "$dec"; then same=$((same + 1))
        else bad="$bad $id($kind)"; fi
    done < "$tmp/sample.txt"
    kinds=$(awk '{print $2}' "$tmp/sample.txt" | sort | uniq -c | tr -s ' \n' ' ')
    if [ "$total" -gt 0 ] && [ "$same" = "$total" ]; then echo "ok   compression: $same of $total resources as the research's ($kinds)"
    else echo "FAIL compression: $same of $total as the research's; differ:$bad"; fail=1; fi
fi

# The sprites' pixels, against the research's rendered frames: a sheet per sprite with its
# frames side by side (2 pixels between), m3/frames/spr_NNNNN_all.png beside the research
# folder (RESEARCH_FRAMES= to say where). The research's renderer took the tile unit as 128
# bytes where the game's is 32, so it drew each frame's first piece only: every pixel it drew
# must be in the port's frame, the same alpha and the colour within 4 a channel (the research
# spread the 5-bit colours as c * 255 / 31, the port as melonDS does); a frame of one piece
# must have exactly its pixels, one of several must have more.
frames=${RESEARCH_FRAMES:-$research/../m3/frames}
if [ ! -d "$frames" ]; then
    echo "skip the sprites' pixels: no $frames (RESEARCH_FRAMES=)"
elif ! "$python" -c "import PIL" 2>/dev/null; then
    echo "skip the sprites' pixels: no Pillow for $python (set PYTHON)"
else
    for entry in $icons; do
        id=${entry%%:*}; want=${entry##*:}
        sheet=$(printf '%s/spr_%05d_all.png' "$frames" "$id")
        [ -f "$sheet" ] || continue
        args=""
        f=0
        while [ "$f" -lt "$want" ]; do
            sprite "$id" "$f" > /dev/null
            pieces=$(sed -n "s/^sprite $id: .* \([0-9]*\) pieces$/\1/p" "$tmp/err")
            args="$args $tmp/spr/${id}_$f.png ${pieces:-0}"
            f=$((f + 1))
        done
        # shellcheck disable=SC2086
        if "$python" -I - "$sheet" $args <<'EOF'
import sys
from PIL import Image
sheet = Image.open(sys.argv[1]).convert('RGBA')
args = sys.argv[2:]
x0 = 0
problems = []
for n in range(0, len(args), 2):
    frame = Image.open(args[n]).convert('RGBA')
    pieces = int(args[n + 1])
    if x0 + frame.width > sheet.width or frame.height > sheet.height:
        problems.append('frame %d: %dx%d does not fit the sheet at x %d' % (n // 2, frame.width, frame.height, x0))
        break
    ref = sheet.crop((x0, 0, x0 + frame.width, frame.height))
    x0 += frame.width + 2
    rp, fp = ref.load(), frame.load()
    drawn = extra = 0
    for y in range(frame.height):
        for x in range(frame.width):
            r, f = rp[x, y], fp[x, y]
            if r[3] == 255:
                drawn += 1
                if f[3] != 255 or max(abs(r[i] - f[i]) for i in range(3)) > 4:
                    problems.append('frame %d (%d,%d): research %s, port %s' % (n // 2, x, y, r, f))
            elif f[3]:
                extra += 1
    if drawn == 0:
        problems.append('frame %d: the research drew nothing' % (n // 2))
    if pieces == 1 and extra:
        problems.append('frame %d: one piece, yet %d pixels beyond the research' % (n // 2, extra))
    if pieces > 1 and not extra:
        problems.append('frame %d: %d pieces, yet nothing beyond the research' % (n // 2, pieces))
if x0 != sheet.width:  # every frame and its 2 pixels, the last's too
    problems.append('the frames and their gaps end at %d, the sheet at %d' % (x0, sheet.width))
for p in problems[:5]:
    print('  ' + p)
sys.exit(1 if problems else 0)
EOF
        then echo "ok   sprite $id pixels: the research's in every frame"
        else echo "FAIL sprite $id pixels"; fail=1; fi
    done
fi
exit $fail
