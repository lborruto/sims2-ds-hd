// The game's own screen manager, read from main RAM (EU table): which screen each DS screen
// shows and the selections on it, without looking at pixels. The RAM map is in docs/research/ram-map.md.
#pragma once
#include <string>

#include "common.h"
#include "screens.h"

namespace sims2wide
{
enum class GScreen { Unknown, FrontEnd /*title or main menu*/, CreateASim, Panel, Popup, Phone, Dialogue, Gift, Catalogue, Build, Vacuum, Social, Cards, SaveSlots, ItemHand };

struct GameState
{
    bool valid = false;            // the manager pointer chain read cleanly
    GScreen screen = GScreen::Unknown;
    u32 vtable = 0; int overlay = -1;   // raw, for Unknown logging
    u8 popup = 0;                  // P+0x8 (0xFF front end, 0 none, 1 opening, 2 shown, 3 closing)
    bool settled = false;          // P+0xE42 == 3
    int tab = -1;                  // Panel: S+0x64 (0..4)
    int selection = -1;            // per screen: catalogue S+0x254, phone S+0x89, cards S+0x2E0, relationships S+0xD0 (Sim id), pockets *(S+0x38)+0xE0
    int scroll = -1, list = -1;    // catalogue S+0x268, S+0x258
    int page = -1, speech = -1;    // dialogue S+0x34, S+0x0C; phone page S+0x88 in `page`
    bool operator==(const GameState&) const = default;
};

// Main RAM as melonDS holds it: nds.MainRAM and nds.MainRAMMask.
struct RamView { const u8* ram; u32 mask; };

GameState ReadGameState(RamView ram);           // EU table
const char* GScreenName(GScreen);
std::string GameStateName(const GameState&);   // "Panel/Goals", "Popup", "Unknown(ov=8,vt=0x0214B1C4)"
// CONSOLE's screen from the game's: Panel with its tab, the rest by name; not valid, a Panel
// whose tab is not 0..4, or an unmapped screen: Unknown (the DS's buttons, the stick cursor).
ScreenState ToScreenState(const GameState&);
// Whether the pixel tracker's answer (screens.h) is what the game's screen looks like:
// the same, or None for FrontEnd / CreateASim (no HUD), Unknown for SaveSlots / ItemHand.
bool PixelsAgree(const GameState&, ScreenState pixels);
}
