// The panel's Interact tab in HD (port/panelmenu.h's frame): the target's picture (a live crop of
// the game's own portrait well: Sims' and objects' pictures are not sprites), the actions as
// rows (the game's verbs, 1845 + the action's code; greyed rows dim, skipped by the focus, never
// performed), and the strip of nearby targets (the previous / current / next names; Left / Right
// switch). Everything from GameState::town.interact (RAM) and the game's strings.
//
// What performs: a row's A taps the row's own arrow on the touch screen (the game's touch target
// is the arrow only: x 219-251, rows 46 + 34i to 78 + 34i; the tap at 236, 59 + 34i), exactly as
// the stylus does; Left / Right tap the strip's previous / next slot (55, 167 / 122, 167), which
// selects that target. The game's own pad mode (A held, Up / Down, release A) does the same, but
// it holds the hero still and turns him, and switching targets in it performs an action on the
// release: the taps are used (the same gate, no key held across frames).
#pragma once
#include <string>
#include <vector>

#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"

namespace sims2wide
{
class RomData;
class IconCache;

constexpr int kInteractRows = 4;                     // the game draws four rows at most
// The game's touch points.
constexpr int kInteractArrowX = 236, kInteractArrowY0 = 59, kInteractArrowStep = 34;
constexpr Rect kInteractArrowFrom {219, 46, 33, 33}; // row 0's arrow (row i: y + 34i)
constexpr int kInteractPrevX = 55, kInteractNextX = 122, kInteractStripY = 167;
constexpr Rect kInteractPrevFrom {40, 152, 32, 32}, kInteractCurFrom {72, 152, 34, 32}, kInteractNextFrom {106, 152, 33, 32};
// The game's portrait well's inside (its outline x 58 / 131, rows 42 / 131), less its bevels:
// the target's picture, or the green plumbob without a target.
constexpr Rect kInteractPortraitFrom {63, 47, 65, 81};

// HD geometry (canvas): the portrait well at the left, the action rows at the right, the strip.
constexpr Rect kInteractPortrait {200, 206, 236, 292};
constexpr int kInteractRowX = 460, kInteractRowW = 620, kInteractRowY = 210, kInteractRowH = 62, kInteractRowGap = 14;
constexpr Rect kInteractStrip {200, 514, 880, 64};
// Item ids (stable across rebuilds): a row's from its code and place.
constexpr u32 kInteractRowId = 0x100;

// Interact's items appended to v (the rows: one per action the game lists, greyed ones disabled)
// and its Left / Right (the strip's previous / next target, when there is one).
// rom (null: no words): the rows' words (the verbs, with the game's amount) and the title.
void AddInteractView(MenuView& v, const InteractState& in, const RomData* rom);
// Its drawing (UI items, the mouse's pieces) in the frame's content area.
void BuildInteract(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons,
                   const u32* bottom);
// The row label as shown (amount: the game's @1 for it, -1 none).
std::string InteractLabel(const RomData& rom, int labelId, int amount = -1);

// (A check, --measure-menu) every word the tab can show, in rom's language, fitted to its HD
// box: the verbs 1845-1951 in a row and in the A hint, the titles "Interact: <name>" and the
// strip's names for every Sim (321 "???", 323-368) and object (463-793) name; the game's unused
// "XXX…" and "Dummy" strings left out. shrunk: drawn smaller than their size; cut: an ellipsis
// was needed (their ids in cutIds).
struct MenuMeasure
{
    int checked = 0, shrunk = 0, cut = 0;
    int cutIn[5] = {};      // cut per box: 0 the row, 1 the A hint, 2 the title, 3 the strip's current, 4 its sides
    std::vector<int> cutIds;
    int widestId = -1;      // the widest verb
    float widestW = 0;
};
MenuMeasure MeasureInteract(const RomData& rom);
}
