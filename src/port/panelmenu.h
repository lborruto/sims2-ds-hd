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
#include <vector>

#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"
#include "ui/kit.h"

namespace sims2hd
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
constexpr int kPmTabCell = 88, kPmTabIcon = 72;             // a tab's cell; its icon at ui::kTierL (24 px, 3x at 100 %)
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
// The phone button: the game's sprite (descriptor 1952, 32 x 32: frame 0 at rest, 1-7 ringing,
// 8-16 lit), whose frame the touch screen shows at (218, 10) (PhoneButtonFrame).
constexpr int kSprPhoneButton = 1952;
constexpr int kPmPhoneSprX = 218, kPmPhoneSprY = 10;

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
// (an ellipsis) when it still does not fit. fits: no ellipsis was needed. Neither size is ever
// below ui::kMinText (18: the console's smallest text); FitBlock the same.
struct FittedText
{
    std::string text;
    float size = 0;
    bool fits = true;
};
FittedText FitLine(const std::string& text, float size, float minSize, int width, ui::Weight w = ui::Weight::Bold);
// One fitted line of text in `r` (no wrap, centred vertically), on `layer` (1 by default); colour 0: the theme's.
void PanelText(Composition& c, Rect r, const std::string& s, float size, float minSize, ui::Align a, ui::Weight w, u32 colour = 0, int layer = 1);
// (A check, --measure-menu) what a screen's measure tallies: a line or block fitted (its size
// shrunk or not, its ellipsis, its lines), each cut id once.
struct BlockMeasure;
struct Measurer
{
    BlockMeasure& m;
    void Note(int id, bool shrunk, bool fits, int lines = 0) const;
    void Line(int id, const std::string& text, float size, float minSize, int width, ui::Weight w = ui::Weight::ExtraBold) const;
    void Block(int id, const std::string& text, float size, float minSize, Rect box) const;
};
// A touchable, invisible piece mapping the HD rectangle `to` onto the touch screen's `from`.
void AddMousePiece(Composition& c, Rect from, Rect to);
// A live crop of the touch screen `bottom` (256 x 192) drawn at `to` with the game's own frame
// around the picture keyed out: from the crop's edges inwards, and only within kKeyBand pixels of
// them (the DS cells' bevels lie in the outer 1-3: measured on every state with a portrait), every
// pixel in the game's panel colours (KeyedColour: its wells #51518A / #5961A2, bevels #414979 /
// #6171B2, outline #383869, fill #828ACB) turns transparent, so a portrait sits on the HD well
// without the DS cell's bevelled corners, and a Sim's hair or clothes in one of those colours
// further in are never eaten (the well behind is the background's colour, so what stays meets
// it). tier > 0: the picture at that tier centred in `to` (ui::TierImage: a small portrait); 0:
// filling `to`. The copy is kept per `from` until the next call for the same rectangle (the
// raster of the frame that built it reads it). A null bottom or a rectangle off the screen: an
// item with no source, which draws nothing (ImageItem's empty item).
constexpr int kKeyBand = 3;
bool KeyedColour(u32 argb);
ui::UiItem KeyedCrop(const u32* bottom, Rect from, Rect to, int tier = 0);
// The last KeyedCrop of `from` as it was (no new copy): a portrait while the game's own box lies
// over it on the touch screen (room info's hint), kept as it was before the box. Nothing (an item
// with no source) when that rectangle was never cropped.
ui::UiItem KeptCrop(Rect from, Rect to, int tier = 0);
// A row's focus ring (UI items, on `layer`): ui::FocusRing around `r` (the game's green, a line inside
// it, white or dark by `behind`, the ringed item's fill), drawn before the row.
void AddFocusRing(Composition& c, Rect r, float radius, int layer = 1, u32 behind = ui::kFillRow);
// The frame of the phone button's sprite the touch screen `bottom` (256 x 192) shows: the frame
// whose opaque pixels match the screen's at the button's place (a ringing frame within 8 pixels
// of it, as the game shakes it), the closest one; 0 (at rest) when none comes near, and without a
// search when frame 0 at its place misses more than half its pixels (no button on the screen).
int PhoneButtonFrame(IconCache& icons, const u32* bottom);
// The panel's D-pad hints for the focus (each a chip and its word, right to left; none, one or
// two): "Choose" (HintChoose) whenever the D-pad moves the focus (the tab has two items or more
// besides the phone, or the game's own selection moves on Up / Down: Goals, Relationships): its
// vertical arms for a list in one column, the whole cross for a grid (Pockets, Options); then a
// slider's Left / Right (DPadH, "Adjust", every slider's: HintAdjust; the cross then says Up /
// Down alone) or the view's Left / Right (DPadH, "Other target": Interact's targets).
struct DPadHint { ui::Button button; std::string word; };
std::vector<DPadHint> PanelDPadHints(const MenuView& v, int focus, const RomData& rom);
// A hint chip right-aligned at `right` in the hints band; returns the next right edge.
int AddHint(Composition& c, int right, ui::Button b, const std::string& label);
}
