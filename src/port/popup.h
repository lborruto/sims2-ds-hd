// The game's popups in HD (CONSOLE): a goal unlocked, a tip, Game Paused, Options > Quit's
// question, the Save slots' "Overwrite this save?" and "Saving", and any other popup the game's
// manager runs (P+0x8 1..3) over the panel or over an HD screen (Give Gift, the Save slots, the
// phone, a note). A game-style box over whatever is under (the dimmed world, or the HD screen
// beneath dimmed with no focus of its own): the game's title and text from RAM
// (GameState::town.popup, decoded by GameTextToUtf8) and its buttons per kind: one X (kind 0)
// → a "Close" button (1866) that A and B both press; a cross and a tick (kind 2) → "No" (49, B:
// the cross) and "Yes" (48, A: the tick); none (kind 3, "Saving") → no button, the box only.
// The buttons are where the game places them (PopupButton, from the box's place and size in
// RAM); a press goes there through the touch script, as the stylus would. Game Paused also
// resumes on Start (passed on). A popup over the panel's open menu wins the focus; when it
// goes, the menu is back on the same tab and row (the navigator's resume).
//
// A popup over a screen the HD port does not redraw (the catalogue, the activities) keeps
// milestone 1's composition: the game's own box in the touch screen.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

// The game's strings.
constexpr int kStrPopClose = 1866, kStrPopYes = 48, kStrPopNo = 49;

// HD geometry (canvas): the box, its title plate, its text well, its buttons' row.
constexpr Rect kPopBox {300, 182, 680, 330};
constexpr Rect kPopTitle {kPopBox.x + 120, kPopBox.y - 26, kPopBox.w - 240, 56};
constexpr Rect kPopWell {kPopBox.x + 28, kPopBox.y + 42, kPopBox.w - 56, 168};
constexpr Rect kPopWellTall {kPopBox.x + 28, kPopBox.y + 42, kPopBox.w - 56, kPopBox.h - 70}; // kind 3: no buttons
constexpr int kPopBtnY = kPopBox.y + kPopBox.h - 104, kPopBtnH = 80;
constexpr Rect kPopClose {kPopBox.x + (kPopBox.w - 320) / 2, kPopBtnY, 320, kPopBtnH};
constexpr Rect kPopNo {kPopBox.x + 48, kPopBtnY, 260, kPopBtnH};
constexpr Rect kPopYes {kPopBox.x + kPopBox.w - 308, kPopBtnY, 260, kPopBtnH};
constexpr int kPopLayer = 4;   // above the screen beneath (0, 1), the status bar (2) and the phone's clock (3); the bar is lifted above it (kPopLayer + 1: never dimmed)
constexpr u32 kPopCloseId = kModPopup + 0x700, kPopYesId = kModPopup + 0x701, kPopNoId = kModPopup + 0x702;

// The game's button `index` of popup p (0: the X or the cross, 1: the tick), its centre on the
// touch screen (x, y): the manager's own rectangles (0x02057314, 24 x 24 from the box's place
// and size). False for none (kind 3, an index the kind has not, the box not read).
bool PopupButton(const PopupState& p, int index, int& x, int& y);

// Whether a popup is up for CONSOLE on screen s: the game's popup over the panel (GScreen::Popup,
// whatever the tab: Options > Quit's too) or over an HD screen.
bool PopupOver(ScreenState s, const GameState& game);
// The popup's view (valid when it is up, its kind and box read, and the game's strings are
// there): own and popup set; kind 0 one item (A, the X; B the same), kind 2 Yes (A, the tick;
// B the cross, no replay of a held press), kind 3 none.
MenuView PopupView(ScreenState s, const GameState* game, const RomData* rom);
MenuView PopupViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);

// The HD view CONSOLE draws and the navigator drives on screen s: the popup's when one is up,
// else the screen's own (the panel's tab, Give Gift, the Save slots, the phone, a note); valid
// false: milestone 1's composition.
MenuView ConsoleView(ScreenState s, const GameState* game, const RomData* rom);
// The screen's own view, a popup or not (what a popup is drawn over).
MenuView ScreenView(ScreenState s, const GameState* game, const RomData* rom);
// What lies under a popup: the screen (the panel and its tab for a popup over it) and its view.
ScreenState UnderScreen(ScreenState s, const GameState& game);

// The popup over what c already holds: a veil over everything, the box with the title, the
// text, the buttons (the focus on item `focus`; a button the stylus holds green, as the game
// draws it), the hints, the mouse's pieces onto the game's buttons. Everything on kPopLayer;
// the pieces already in c stop taking the mouse, and the pictures beneath that the box would
// cover go (images are drawn above every still item: they would show through it).
void BuildPopup(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom);
// A screen's own hint box in the popups' look (room info's line hints, port/roominfo.h): over what
// c holds, the veil, the box with `title` on its plate (none when empty) and `text`, one Close
// button (A and B chips, the focus on it when `focus` is 0) whose mouse piece maps onto the DS
// rectangle `from` (none when empty). Everything on kPopLayer, as BuildPopup.
void BuildHintBox(Composition& c, int focus, const std::string& title, const std::string& text, const RomData& rom, Rect from);

// (A check, --measure-menu) every popup text the game opens (the ids at its call sites) in the
// box, in rom's language.
void MeasurePopups(const RomData& rom, BlockMeasure& m);
}
