#!/bin/sh
# Makes the macOS icon (AppIcon.icns) from packaging/icon.png, with the tools macOS ships.
#   packaging/make_icns.sh <icon.png> <out.icns>
set -eu
src="$1"
out="$2"
set="${out%.icns}.iconset"
rm -rf "$set"
mkdir -p "$set"
for s in 16 32 128 256 512; do
    sips -z "$s" "$s" "$src" --out "$set/icon_${s}x${s}.png" >/dev/null
    sips -z "$((s * 2))" "$((s * 2))" "$src" --out "$set/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$set" -o "$out"
rm -rf "$set"
