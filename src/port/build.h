// Build Rooms (overlay 1, CBuildMenu: the City Hall clerk's "Build Rooms") in HD. A game-style panel
// over the dimmed world (port/panelmenu.h's frame, no tab bar): the game's title and the player's
// money in the title band, then one of the game's two pages, from GameState::build (RAM, every
// frame: the game rebuilds its list at open and after every purchase):
//   page 0, the list: the three rooms the game shows from its scroll, each with the game's row
//     knob, the room's name (0x43 + place), where it is built ("(@1)" 2397) and its permit ("$@1"
//     28), red when the money does not cover it (the game's own red); the game's arrows and a
//     scroll bar; beside it the focused room's sum: the money, minus the permit, what is left.
//   page 1, "Are you sure you want to buy this room?" (2398): the chosen room, its permit and the
//     money left after it, the game's cross ("No" 49) and tick ("Yes" 48).
//
// The game's Build Rooms is the stylus only (no DS button is read). What performs, always the
// game's own taps (the research's touch points, m4 §5):
//   A on a row ("Buy", the catalogue's word): its knob (48, 68 / 103 / 138): the game opens page 1 for it. A row the money does
//     not cover stays focusable (its price is read) but A does nothing: the game refuses it.
//   Up / Down past the first / last row: the game's arrows (111,172 / 145,172) while it can scroll.
//   Page 1: A on the tick (158,130) buys (money − the permit, the room's building timer set, the
//     game's popup 2628 once the menu closes); A on the cross or B: the cross (96,130), back to the
//     list. Never replayed (MenuView::replay false): a press is a spend.
//   B on the list (and MENU): the game's X (235,171), back to the panel.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "port/popup.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

// The game's touch points (DS bottom pixels; its buttons' hit areas are circles of radius 16).
constexpr int kBdRowX = 48, kBdRowY0 = 68, kBdRowDy = 35, kBdRows = 3;   // row i's knob: (x, y0 + dy i)
constexpr int kBdUpX = 111, kBdDownX = 145, kBdArrowY = 172;
constexpr int kBdCloseX = 235, kBdCloseY = 171;                           // the X (both pages)
constexpr int kBdCrossX = 96, kBdTickX = 158, kBdConfirmY = 130;          // page 1

// The game's sprites (descriptor ids; tiles − 1, palette + 1): the row knob 7665, the arrows
// 7698 / 7677, the X 7683 (also page 1's cross), the tick 974. All 24 x 24.
constexpr int kSprBdKnob = 7665, kSprBdUp = 7698, kSprBdDown = 7677, kSprBdX = 7683, kSprBdTick = 974;

// The game's strings: the title, a room's place "(@1)", the question, the money's "$@1", Yes / No.
constexpr int kStrBdTitle = 2394, kStrBdWhere = 2397, kStrBdAsk = 2398, kStrBdMoney = 28, kStrBdYes = 48, kStrBdNo = 49;
constexpr int kStrBdBuy = 2391;         // "Buy" (the catalogue's mode): a row's A (the game has no verb of its own there)
constexpr int kStrBdPlaceBase = 0x43;   // a room's name: 0x43 + place

// The rooms with a permit in the game's static room table (0x0211F9EC, read from RAM at run time:
// GameState::hotel.rooms); the measure's list (the table is not in the ROM's files).
constexpr int kBdBuildable[] = {0, 1, 4, 7, 8, 12, 15, 16, 18, 21, 26, 28, 31, 32};

// HD geometry (canvas), the frame as the panel's (kPmPanel), no tab bar.
constexpr Rect kBdTitle {200, 44, 880, 64};
constexpr Rect kBdClose {1016, 48, 56, 56};       // the game's X, in the title band
constexpr Rect kBdList {200, 124, 600, 454};      // page 0: the list's well
constexpr Rect kBdUp {740, 140, 48, 48}, kBdDown {740, 514, 48, 48};
constexpr Rect kBdInfo {816, 124, 264, 454};      // page 0: the focused room's sum
constexpr Rect kBdEmpty {200, 124, 880, 454};     // page 0 with nothing left to build: one well, the port's line
// Page 1: the question in the popups' look (port/popup.h's BuildQuestionBox) over the dimmed list:
// the box, the question's height at the top of its well, its No and Yes.
constexpr Rect kBdAskBox {300, 150, 680, 400};
constexpr int kBdAskTextH = 90;
constexpr Rect kBdCross = QuestionNo(kBdAskBox), kBdTick = QuestionYes(kBdAskBox);
constexpr float kBdNameSize = 30, kBdNameMin = 20, kBdWhereSize = 24, kBdWhereMin = 18, kBdPriceSize = 28;
// Row i of the list.
Rect BuildRowRect(int i);

// Item ids (stable across rebuilds: a row's id carries its room, so the focus stays on a room when
// the game's list is rebuilt or scrolled).
constexpr u32 kBdRowId = kModBuild + 0x100, kBdCrossId = kModBuild + 0x200, kBdTickId = kModBuild + 0x201;

// The view (valid when RAM's Build Rooms reads and the game's strings are there). Page 0: one item
// per row shown (none when the game lists nothing), Up / Down past them the game's scroll; page 1:
// the cross (the first focus: No) and the tick. Never replayed; B the game's X (page 0) or cross.
MenuView BuildRoomsView(ScreenState s, const GameState* game, const RomData* rom);
// The same without the strings' check (tests; rom null leaves the words empty).
MenuView BuildRoomsViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c (after the dimmed world): the panel, title and money, the page, the
// hints, the focus on view item `focus`, and the mouse's pieces.
// Page 1 draws the list beneath without a focus; its question is BuildBuildQuestion's.
void BuildBuildRooms(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons);
// Page 1's question over what c holds (the screen and the status bar: the popups' look, port/popup.h's
// BuildQuestionBox), the focus on its No (0) or Yes (1); nothing on page 0.
void BuildBuildQuestion(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom);

// (A check, --measure-menu) every word Build Rooms can show in rom's language, in its box: every
// room's name and place (rows, the sum, page 1), the title beside the widest money, the question,
// the buttons, the sums at the widest amounts.
void MeasureBuild(const RomData& rom, BlockMeasure& m);
}
