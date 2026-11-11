// The panel's Goals tab in HD (port/panelmenu.h's frame): the goals the game lists (mission goals,
// then the hotel's requests; GameState::town.goals, RAM) wrapped in HD text in a list well, each
// with its mark (a mission's green dot, the game's own bullet; a request's lavender one), the
// game's own selection on a raised bar, the giver's portrait (a live crop of the game's well: the
// pictures are texture bundles) and the hotel's footer ("Your hotel is amazing!" / "Improve your
// hotel!" and "Score: N%") when the game shows it. A goal done leaves the game's list (it lists
// active goals that are not completed), so no goal is ever drawn done.
//
// The selection and the scroll are the game's (MenuView::selected): Up / Down tap the game's own
// arrows (62,59 / 62,145: it selects the previous / next goal and scrolls so it shows), and the
// HD list starts at the goal on the game's first visible line, moved on until the selected goal
// fits the well (HD lines are not the DS's). A does nothing (the game's goals have no action).
// The mouse: a goal's bar maps onto its lines on the touch screen while the game shows them.
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

// The game's touch points and pictures (DS bottom pixels).
constexpr int kGoalsUpX = 62, kGoalsUpY = 59, kGoalsDownX = 62, kGoalsDownY = 145;
constexpr int kGoalsLineX = 86, kGoalsLineY0 = 52, kGoalsLineW = 160, kGoalsLineH = 16, kGoalsLinesShown = 7;
constexpr Rect kGoalsPortraitFrom {47, 88, 30, 30}; // inside the giver's well (outline x 45 / 78, rows 86 / 119, less its bevel)

// The game's strings.
constexpr int kStrGoalFirst = 1627;              // a mission goal's text: 1627 + id
constexpr int kStrFooterImprove = 1841, kStrFooterScore = 1842, kStrFooterAmazing = 1843;

// HD geometry (canvas), inside the frame's content area (200, 206, 880 x 372).
constexpr Rect kGoalsPortrait {200, 206, 128, 128};
constexpr Rect kGoalsList {344, 206, 736, 372};              // without the footer
constexpr Rect kGoalsListFooter {344, 206, 736, 312};        // with it
constexpr Rect kGoalsFooter {344, 530, 736, 48};
constexpr int kGoalsPad = 14, kGoalsGap = 8, kGoalsMark = 30;   // inside the well; between goals; the mark's column
constexpr float kGoalsSize = 26, kGoalsMin = 20;
constexpr u32 kGoalsId = kModGoals + 0x400;                               // + 0x100 for a request, + the goal's id

// A goal's text as the game shows it: a mission's 1627 + id; a request's own string with its
// Sim's name (@1) and place (@2), its other placeholders dropped.
std::string GoalText(const RomData& rom, const GoalEntry& e);

// Goals' items appended to v: one per goal the HD list shows (from the game's scroll), the game's
// selection held (selected), Up / Down the game's arrows. rom (null: no words) gives the title and
// the texts the list's heights come from; without it each goal is as tall as its DS lines.
void AddGoalsView(MenuView& v, const GoalsState& g, const RomData* rom);
// Its drawing (UI items, the mouse's pieces) in the frame's content area.
void BuildGoals(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, const u32* bottom);

// A thin scroll bar in a list's right margin (UI items, layer 1): `first` and `shown` of `count`.
void AddScrollBar(Composition& c, Rect track, int first, int shown, int count);
// Every scrolling list's column at its right (one look: Goals, Relationships, the phone, Build
// Rooms, music): the kit's up arrow at the top of `col`, its down arrow at the foot (both col.w
// square; sunk when the list cannot go that way: upOn, downOn) and the scroll bar between them,
// on layer 1 (`layer`). The arrows' rectangles in up / down (the callers' mouse pieces).
void AddScrollColumn(Composition& c, Rect col, int first, int shown, int count, bool upOn, bool downOn, Rect* up = nullptr, Rect* down = nullptr, int layer = 1);

// (A check, --measure-menu) every goal's text (1627 + 0..33 and the requests' ids the code knows)
// alone in the list well at its smallest size, and the footer's words.
struct BlockMeasure;
void MeasureGoals(const RomData& rom, BlockMeasure& m);
}
