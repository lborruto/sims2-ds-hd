// Give Gift (overlay 6) in HD: a game-style panel over the dimmed world like the panel's menu
// (port/panelmenu.h), with the game's title "Give Item to @1" (2422, the recipient's name), the
// pockets' six slots as a grid of the game's item icons (empty ones drawn, never focused; one the
// game will not let this Sim have greyed and skipped), the focused item's name and description,
// the bow ("Give Gift", 1850; a live crop of the game's button: its sprite is not one the port
// decodes) and the recipient's portrait (a live crop of the game's well: Sims' pictures are
// texture bundles). Everything from GameState::town.gift and .inventory (RAM).
//
// What performs: A on a slot taps it (the game selects it) and then the bow (127,85): the game
// gives that item (the screen goes back to the panel). B taps the game's X (235,19). The game
// gives at once, as it does for a tap on its bow: no question of its own, and none here (giving
// is what the screen is for; the slot and its name are on screen when A is pressed).
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;

// The game's touch points (DS bottom pixels) and pictures.
constexpr int kGiftSlotX0 = 37, kGiftSlotY0 = 50, kGiftSlotDx = 39, kGiftSlotDy = 37; // slot i: (x0 + dx (i % 2), y0 + dy (i / 2))
constexpr int kGiftBowX = 127, kGiftBowY = 85, kGiftCloseX = 235, kGiftCloseY = 19;
constexpr Rect kGiftBowFrom {108, 67, 38, 38};      // the bow's round button
constexpr Rect kGiftPortraitFrom {167, 47, 65, 81}; // inside the portrait's well (outline x 162 / 235, rows 42 / 131)

// The game's strings.
constexpr int kStrGiftTitle = 2422, kStrGive = 1850;

// HD geometry (canvas): the frame as the panel's, without the tab bar.
constexpr Rect kGiftTitle {200, 44, 880, 64};
constexpr int kGiftGridX = 200, kGiftGridY = 183;   // 2 x 3 slots of kPkCell (336 high), centred in the content's 454
constexpr Rect kGiftInfo {436, 124, 388, 454};
constexpr Rect kGiftPortrait {840, 179, 240, 276};
constexpr Rect kGiftName {840, 467, 240, 56};
constexpr Rect kGiftBow {kGiftInfo.x + 16, kGiftInfo.y + kGiftInfo.h - 112, kGiftInfo.w - 32, 96};
constexpr Rect kGiftDesc {kGiftInfo.x + 20, kGiftInfo.y + 60, kGiftInfo.w - 40, kGiftBow.y - 12 - (kGiftInfo.y + 60)};

constexpr u32 kGiftSlotId = 0x300;

// The view (valid when RAM's gift and inventory read and the game's strings are there).
MenuView GiftMenuView(ScreenState s, const GameState* game, const RomData* rom);
// The same without the strings' check (tests; rom null leaves the words empty).
MenuView GiftMenuViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c (after the dimmed world): the panel, title, slots, info, bow,
// portrait, hints, the focus on view item `focus`, and the mouse's pieces.
void BuildGiftMenu(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons,
                   const u32* bottom);
}
