// Milestone 1's 5 x 7 bitmap font (capitals, digits and a few signs), kept only for the overlays
// of milestone 1's compositions (compose.cpp's text overlays: CONSOLE without the game's strings).
// Nothing new uses it: the HD screens and the shell are on the UI kit (ui/kit.h).
//
// Adapted from Pokémon FullDex's frontend/ui.h (GPLv3, https://github.com/mamesj/pokemon-fulldex).
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace sims2wide
{
// The font's 5 x 7 glyph for c (lower case drawn as capitals), one row per byte, bit 4 the
// leftmost pixel; nullptr for a sign it does not have. (Also CONSOLE's hints, compose.cpp.)
const u8* Glyph(char c);
}
