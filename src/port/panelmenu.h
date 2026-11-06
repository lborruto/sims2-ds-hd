// The panel's menu redrawn in HD (CONSOLE, the menu open on the panel): a game-style panel over
// the dimmed world, the tab bar (the game's own five tab icons from the ROM, upscaled: the
// selected one green and raised, the others blue; LB / RB chips at its ends; the phone button
// at the right), the tab's title, its content (port/interact.h, …) and the button hints. The tab's
// module builds its MenuView (port/menuview.h) from the game's state; the navigator moves the
// focus over it and performs the items' actions; this file draws the same view with that focus.
// A tab whose data does not read (its part of GameState not valid, no ROM strings) gets no view:
// CONSOLE keeps milestone 1's composition (the touch screen 3x) for it.
//
// Mouse: invisible touchable pieces (alpha 0) lie over the HD items and map them onto the game's
// own touch targets (a row onto its arrow, a tab onto its icon), so a click does what the item
// does; the rest of the panel and the dimmed world take no click.
#pragma once
#include <string>

#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"
#include "ui/kit.h"

namespace sims2wide
{
class RomData;
class IconCache;

// The panel and its bands, canvas pixels (1280 x 720; the status bar stays below at 660).
constexpr Rect kPmPanel {176, 30, 928, 616};
constexpr int kPmPad = 24;                                   // the panel's inner margin
constexpr int kPmInnerX = kPmPanel.x + kPmPad, kPmInnerW = kPmPanel.w - 2 * kPmPad; // 200, 880
constexpr Rect kPmTabBar {kPmInnerX, 44, kPmInnerW, 84};    // LB, the five tabs, RB; the phone at the right
constexpr Rect kPmTitle {kPmInnerX, 140, kPmInnerW, 52};    // the tab's title in a well
constexpr Rect kPmContent {kPmInnerX, 206, kPmInnerW, 372}; // the tab's own area (to 578)
constexpr Rect kPmHints {kPmInnerX, 590, kPmInnerW, 40};    // the button hints, right-aligned
constexpr int kPmTabCell = 88, kPmTabIcon = 72;             // a tab's cell; its icon 3x (24 px)
constexpr int kPmTabsX = kPmPanel.x + kPmPanel.w / 2 - 5 * kPmTabCell / 2; // the first tab cell: LB, the tabs, RB centred
constexpr Rect kPmPhone {kPmInnerX + kPmInnerW - 90, 44, 90, 84}; // the phone button's cell
constexpr Rect kPmPhoneWell {kPmPhone.x + 8, kPmPhone.y + 4, kPmPhone.w - 16, kPmPhone.h - 8}; // its well: the item, the mouse's piece
constexpr float kPmTitleSize = 32, kPmTextSize = 30, kPmTextMin = 22, kPmHintSize = 24;

// The game's string ids the frame uses (the same in every language).
constexpr int kStrTabName = 1835;      // 1835..1839: Interact, Pockets, Goals, Relationships, Options
constexpr int kStrTitle = 1840;        // "@1: @2"
constexpr int kStrClose = 1866;        // "Close" (B: the menu closes)
constexpr int kStrPhone = 716;         // "Cellphone" (the phone button's A)
// The game's tab icons: selected (green) and not (blue), frames 0..4 in tab order.
constexpr int kTabIconGreen = 2247, kTabIconBlue = 2250;
// The phone button on the touch screen (a live crop: the ROM's sprite for it is not known), the
// inside of its well (its fill the game's #5961A2; the phone's right edge overhangs the well).
constexpr Rect kPmPhoneFrom {218, 10, 30, 34};

// Whether the open menu on screen s is redrawn in HD, and its view (valid false otherwise):
// the panel's tab whose data reads (RAM valid, its part of GameState valid) with the game's
// strings. Built every frame by both the navigator's caller (input.cpp) and CONSOLE.
MenuView PanelMenuView(ScreenState s, const GameState* game, const RomData* rom);
// The same without the strings' check (tests): rom null leaves the words (title, items' A words)
// empty. The phone comes last, linked to the tab's items: Up from a top item (nothing enabled
// above it) goes to the phone, Down from the phone to the tab's first item.
MenuView PanelMenuViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);

// The HD menu appended to c (after the dimmed world and before the status bar): the frame, the
// tab's content, the focus on view item `focus`, the hints, and the mouse's pieces. icons may be
// null (no icon drawn), bottom too (no live crop).
void BuildPanelMenu(Composition& c, ScreenState s, const MenuView& v, int focus, const GameState& game, const RomData& rom,
                    IconCache* icons, const u32* bottom);

// Helpers the tabs share.
// A game string as shown in a menu: RomData's escapes and any @ placeholder dropped (with a '$'
// or empty brackets left before one: "Pay ($@1)" → "Pay"), spaces folded, one line. amount >= 0
// fills @1 as the game does for the verbs that carry one (grouped by ',', the game's '$' its
// simoleon glyph drawn §: "Buy (§1,200)").
std::string MenuWords(const RomData& rom, int id, int amount = -1);
// The same on a text (UTF-8, RomData's escapes).
std::string MenuText(std::string text, int amount = -1);
// String id with its @1 filled with at1 (a name: "Give Item to @1"), then as MenuWords.
std::string MenuWordsWith(const RomData& rom, int id, const std::string& at1);
// The "@1: @2" title with the tab's name and `name` (empty: the tab's name alone, as the game).
std::string MenuTitle(const RomData& rom, int tab, const std::string& name);
// A single line fitted to `width`: the size shrunk from `size` to `minSize`, then "…"
// (an ellipsis) when it still does not fit. fits: no ellipsis was needed.
struct FittedText
{
    std::string text;
    float size = 0;
    bool fits = true;
};
FittedText FitLine(const std::string& text, float size, float minSize, int width, ui::Weight w = ui::Weight::Bold);
// A touchable, invisible piece mapping the HD rectangle `to` onto the touch screen's `from`.
void AddMousePiece(Composition& c, Rect from, Rect to);
// A row's focus ring (UI items, layer 1): a green halo 4 pixels around `r`, drawn before the row.
void AddFocusRing(Composition& c, Rect r, float radius);
// A hint chip right-aligned at `right` in the hints band; returns the next right edge.
int AddHint(Composition& c, int right, ui::Button b, const std::string& label);
}
