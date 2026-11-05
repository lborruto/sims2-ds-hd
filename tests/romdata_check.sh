#!/usr/bin/env bash
# Checks RomData (src/romdata.*): the game's strings read from the player's ROM, in its five
# languages. --strings-count must print 3127 for languages 1 to 5, and without --language for
# the firmware's (English on melonDS's built-in firmware, GameLanguage); --dump-string of three
# English ids must equal the research's decoded lines at those ids, and a French string with
# an "é" must come out as UTF-8 "é". The expected lines are read at run time from the research
# folder (RESEARCH=, the folder holding text/strings_NNNNN.txt: 00123 English, 03496 French),
# never copied into the repository: without it that part is skipped with a message.
# Needs the ROM (not in git, so not in CI):
#   RESEARCH=/path/to/research/rom bash tests/romdata_check.sh
# Override with SIMS2WIDE (the program) and ROM; they default to this checkout's build/ and roms/.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
bin=${SIMS2WIDE:-$repo/build/sims2wide}
rom=${ROM:-$(find "$repo/roms/" -name "*.nds" 2>/dev/null | head -n 1)}
research=${RESEARCH:-}
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

# The text itself, against the research's decoded lines (id, a tab, a Python repr of the
# cp1252-decoded bytes).
if [ -z "$research" ] || [ ! -f "$research/text/strings_00123.txt" ]; then
    echo "skip the strings' text: no research folder (RESEARCH=... holding text/strings_00123.txt)"
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
exit $fail
