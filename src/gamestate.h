// The game's own screen manager, read from main RAM (EU table): which screen each DS screen
// shows and the selections on it, without looking at pixels. The RAM map is in docs/research/ram-map.md.
#pragma once
#include <string>
#include <vector>

#include "common.h"
#include "screens.h"

namespace sims2wide
{
enum class GScreen { Unknown, FrontEnd /*title or main menu*/, CreateASim, Panel, Popup, Phone, Dialogue, Gift, Catalogue, Build, Vacuum, Social, Cards, SaveSlots, ItemHand };

// The status bar (top screen rows 176-191), from the game-state block G. valid: G reads and every
// field is in range (hour 0..23, minute 0..59, sanity <= max, tier 0..3, a printable name).
struct HudState
{
    bool valid = false;
    int money = 0;
    int hour = -1, minute = -1;     // the DS clock the game shows (24 h)
    int sanity = 0, sanityMax = 0;  // whole units (the game keeps them 20.12 fixed)
    int tier = -1;                  // 0 red, 1 green, 2 yellow, 3 lavender bar
    std::string simName;            // the current Sim, ASCII, at most 8 characters
    bool operator==(const HudState&) const = default;
};

// The conversation on the bottom screen (only read when the screen is Dialogue). stringId is the
// text id of the shown page (strings_EN numbering); the text itself is not here.
struct DialogueState
{
    bool valid = false;
    int speech = -1;                // index into the game's speech table (S+0x34); a bare string id in one-off mode
    int page = -1, pages = 0;       // page shown (1-based) of pages
    int stringId = -1;
    int speaker = -1;               // Sim record index, 0x3F none, 0x40 the player
    int speakerNameId = -1;         // string id of the speaker's name; -1 for none / the player
    bool last = false;              // the shown page is the last one (a tap closes the conversation)
    bool operator==(const DialogueState&) const = default;
};

struct GameState
{
    bool valid = false;            // the manager pointer chain read cleanly
    // The manager reads but its bottom-screen object pointer (P+0xDAC) is null: the game is
    // between screens, in a black load (valid stays false). Measured on every load seen: the
    // title -> Create-a-Sim, Create-a-Sim -> town, quitting the cards.
    bool between = false;
    GScreen screen = GScreen::Unknown;
    u32 vtable = 0; int overlay = -1;   // raw, for Unknown logging
    u8 popup = 0;                  // P+0x8 (0xFF front end, 0 none, 1 opening, 2 shown, 3 closing)
    bool settled = false;          // P+0xE42 == 3 and popup 0, 2 or 0xFF (none sliding)
    int tab = -1;                  // Panel: S+0x64 (0..4)
    int selection = -1;            // per screen: catalogue S+0x254, phone S+0x89, cards S+0x2E0, relationships S+0xD0 (Sim id), pockets *(S+0x38)+0xE0
    int scroll = -1, list = -1;    // catalogue S+0x268, S+0x258
    int page = -1, speech = -1;    // dialogue page counter S+0x36, speech index S+0x34; phone page S+0x88 in `page`
    HudState hud;                  // the status bar's values (any screen with the world's G block valid)
    DialogueState dialogue;        // filled only when screen == Dialogue
    // Screen identity only: hud and dialogue change every minute / page and are logged apart
    // (--print-hud), so --print-game stays one line per screen change.
    bool operator==(const GameState& o) const
    {
        return valid == o.valid && between == o.between && screen == o.screen && vtable == o.vtable && overlay == o.overlay &&
               popup == o.popup && settled == o.settled && tab == o.tab && selection == o.selection && scroll == o.scroll &&
               list == o.list && page == o.page && speech == o.speech;
    }
};

// Main RAM as melonDS holds it: nds.MainRAM and nds.MainRAMMask.
struct RamView { const u8* ram; u32 mask; };

GameState ReadGameState(RamView ram);           // EU table
// Every page's string id in the game's speech table (static data, any state of the European
// ROM), in table order, repeats kept; empty when the table does not read. For checks: the
// conversations' texts in each language measured against the HD box (--measure-string all).
std::vector<int> SpeechStringIds(RamView ram);
const char* GScreenName(GScreen);
std::string GameStateName(const GameState&);   // "Panel/Goals", "Popup", "Unknown(ov=8,vt=0x0214B1C4)"
// CONSOLE's screen from the game's: Panel with its tab, the rest by name; not valid, a Panel
// whose tab is not 0..4, or an unmapped screen: Unknown (the DS's buttons, the stick cursor).
// Options > Quit is a popup over the Options tab (tab 4): Unknown too, since its yes / no
// buttons are not kPopupClose (the stick cursor answers it, or the DS's A).
ScreenState ToScreenState(const GameState&);
// What CONSOLE shows and steers: ToScreenState when RAM reads, else the pixels' answer. Without
// the HUD and with both screens black (`black`: ScreensBlack), the game between screens
// (GameState::between) or an Unknown is Loading (the black loads: the bare backdrop); an Unknown
// without the HUD that is not black is None (an unmapped front-end screen: the front end's view).
// The pixels' None (RAM not read at all) stays None.
ScreenState ConsoleScreenOf(const GameState&, ScreenState pixels, bool hud, bool black);
// Whether the pixel tracker's answer (screens.h) is what the game's screen looks like:
// the same, or None for FrontEnd / CreateASim (no HUD), Unknown for SaveSlots / ItemHand.
bool PixelsAgree(const GameState&, ScreenState pixels);
}
