// The panel's Relationships tab in HD (port/panelmenu.h's frame): the Sims met, as the game lists
// them (GameState::town.relations, RAM): the four the game shows (its own scroll) as rows of a
// live crop of the game's small portrait and the name, the game's selected Sim on a raised row;
// beside them the selected Sim: the big portrait (a live crop of the game's: Sims' pictures are
// texture bundles), its name on a plate, its mood (181 "Mood", 184 + mood) and relationship (182
// "Relationship", 192 + 7 mood + level) and its description (389 + id).
//
// The selection is the game's (MenuView::selected): Up / Down tap the game's own arrows (56,10 /
// 56,180: the previous / next Sim; the game scrolls its four portraits to keep it shown); the HD
// rows follow RAM. A does nothing (the game's Secret Info button, shown for a Sim with secret
// info, was seen in code only: not offered). The mouse: a row maps onto its portrait on the touch
// screen (a tap selects that Sim, as the game's).
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"

namespace sims2wide
{
class RomData;

// The game's touch points and pictures (DS bottom pixels).
constexpr int kRelPrevX = 56, kRelPrevY = 10, kRelNextX = 56, kRelNextY = 180;
constexpr int kRelRowsShown = 4;
constexpr int kRelSimX = 56, kRelSimY[kRelRowsShown] = {41, 78, 114, 150};
constexpr Rect kRelSimFrom {43, 28, 28, 28};            // portrait row 0's inside (row i: y + kRelSimY[i] - 41)
constexpr Rect kRelBigFrom {84, 37, 69, 84};            // the big portrait's inside

// The game's strings.
constexpr int kStrMood = 181, kStrRelationship = 182;

// HD geometry (canvas), inside the frame's content area (200, 206, 880 x 372).
constexpr int kRelRowX = 200, kRelRowW = 236, kRelRowH = 84, kRelRowGap = 12;
constexpr Rect kRelScroll {442, 206, 8, 372};
constexpr Rect kRelPortrait {462, 206, 232, 284};
constexpr Rect kRelName {462, 502, 232, 56};
constexpr Rect kRelMood {710, 206, 370, 76};
constexpr Rect kRelLevel {710, 292, 370, 76};
constexpr Rect kRelDescWell {710, 378, 370, 200};
constexpr Rect kRelDesc {kRelDescWell.x + 18, kRelDescWell.y + 12, kRelDescWell.w - 36, kRelDescWell.h - 24};
constexpr u32 kRelId = 0x500;   // + the Sim's id

// Relationships' items appended to v: the rows the game shows, its selection held, Up / Down its
// arrows. rom (null: no words): the title.
void AddRelationsView(MenuView& v, const RelState& r, const RomData* rom);
// Its drawing (UI items, the mouse's pieces) in the frame's content area.
void BuildRelations(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, const u32* bottom);

// (A check, --measure-menu) every Sim's name in a row and on the plate, description in its box,
// mood and relationship words in their wells.
struct BlockMeasure;
void MeasureRelations(const RomData& rom, BlockMeasure& m);
}
