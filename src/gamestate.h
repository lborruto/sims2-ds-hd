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

// A row of the game's shared record table (names, icons, prices of the Sims and the items).
// Resources are rom.bin ids; -1 where the row does not read.
struct RecordRow
{
    bool valid = false;
    int nameId = -1;                  // string id of the name
    int iconRes = -1;                 // +0xA: the portrait / second picture resource
    int tilesRes = -1, descRes = -1, palRes = -1;   // +0 / +2 / +4: the sprite triple (descRes names the sprite, spr_NNNNN)
    int price = 0;                    // items
    bool operator==(const RecordRow&) const = default;
};

// A Sim record of the game block G (relationships): who is met, mood and relationship level.
struct SimRecord
{
    bool valid = false;
    int simId = -1;                   // name = string 323 + simId
    int mood = 0, level = 0;          // strings 184 + mood, 192 + 7 * mood + level
    bool met = false, secret = false; // flags bit 0 / bit 1
    bool operator==(const SimRecord&) const = default;
};

// One inventory entry: a pocket slot (id = item id, count 1), a counted item (id = 0..3) or a key
// item (id = 0..2, count 1 when owned). Strings are ids (strings_EN numbering).
struct ItemRec
{
    int id = -1;
    int nameId = -1, descId = -1;
    int iconRes = -1;                 // slot items: the sprite descriptor resource (RecordRow::descRes); -1 for the others
    int count = 0;
    bool operator==(const ItemRec&) const = default;
};

struct InventoryState
{
    bool valid = false;
    std::vector<ItemRec> slots;       // the pockets, in slot order (at most 6)
    ItemRec counted[4];               // Nuclear Fuel Rods, License Plates, Strange Gourds, Alien Ship Parts
    ItemRec keys[3];                  // Vacuum, Super Drencher, Metal Detector (count 1 = owned)
    bool operator==(const InventoryState&) const = default;
};

// The popup the manager runs (P+0x8 in 1..3). text and title are the game's raw text bytes
// (GameTextToUtf8, romdata.h, decodes them); title is empty when the popup has none.
struct PopupState
{
    bool valid = false;
    int state = 0;                    // 1 opening, 2 shown, 3 closing
    int kind = -1;                    // 0 one X, 2 cross + tick, 3 no button
    std::string text, title;
    int pressed = -1;                 // button under the stylus (0 X / cross, 1 tick)
    bool pressing = false;
    bool operator==(const PopupState&) const = default;
};

struct OptionsState
{
    bool valid = false;
    int music = 0, sfx = 0;           // 0..4096 (0x1000 = 100 %)
    bool operator==(const OptionsState&) const = default;
};

constexpr int kSaveSlotCount = 3;
struct SaveSlot
{
    bool used = false;
    int money = 0, place = -1;        // place: string 0x43 + place
    u32 time = 0;                     // the header's raw time word
    std::string simName;              // ASCII (blank when it does not read)
    bool operator==(const SaveSlot&) const = default;
};
struct SaveSlots
{
    bool valid = false;
    bool used[kSaveSlotCount] = {};
    int current = -1;                 // the game's current slot, -1 none
    SaveSlot slot[kSaveSlotCount];
    bool operator==(const SaveSlots&) const = default;
};

// The town's menus' content, each part read apart (garbage in one leaves only that one invalid).
struct TownState
{
    InventoryState inventory;
    PopupState popup;
    OptionsState options;
    SaveSlots saves;
    bool operator==(const TownState&) const = default;
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
    TownState town;                // inventory, popup, options, save slots (any screen); logged apart (--print-town)
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
// The shared record table's rows: Sim k (0..45) and item id (0..0x1FF); valid false out of range.
RecordRow ReadSimRow(RamView ram, int k);
RecordRow ReadItemRow(RamView ram, int id);
SimRecord ReadSimRecord(RamView ram, int k);    // k 0..45, from G
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
