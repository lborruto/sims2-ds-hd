// CONSOLE: the game on one 16:9 screen. Per recognised bottom screen (screens.h), a composition
// of pieces of the two DS screens on a 1280 x 720 canvas, as in the approved mockup: in town (the
// panel) the world fills the canvas with a rail of the touch screen's tab icons and phone at the
// right; the menu, the phone, popups and Give Item are the whole touch screen 3x over the dimmed
// world (framed for SaveSlots, ItemHand and Unknown); a black load is the bare backdrop; without a
// HUD (the front end) the top screen at the left and the touch screen 2.5x at the right, each in its frame; a conversation is the world with the
// game's dialogue redrawn in HD at the bottom (port/dialogue.h; without the game's text, its own box rebuilt wide); an activity is the touch screen 3x at the left
// with the world small at the top right. DS pieces are drawn as the game draws them, 2x or 3x;
// frames and the focus ring are overlays in the game's colours.
#pragma once
#include "activities.h"
#include "compose.h"
#include "gamestate.h"
#include "port/statusbar.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
constexpr int kConsoleW = 1280, kConsoleH = 720;

// The game's colours (frame, panel, outline, highlight).
constexpr u32 kFrameArgb = 0xFF828ACB, kPanelArgb = 0xFF5961A2, kOutlineArgb = 0xFF383869, kRingArgb = 0xFF309A41;
// The world behind the menu.
constexpr u32 kDimArgb = 0x8C000000;

// Town: the rail, at the canvas's right edge, vertically centred: the phone icon above the tab
// column, both 2x, in a frame.
constexpr Rect kRailFrame {1168, 130, 88, 460};
constexpr Rect kRailPhoneFrom {222, 4, 30, 30}, kRailPhoneTo {1182, 144, 60, 60};
// (The tab column is x 0-34: the selected icon's ring takes all 35 columns.)
constexpr Rect kRailTabsFrom {0, 6, 35, 184}, kRailTabsTo {1177, 208, 70, 368};

// The menu (and Phone, Popup, Gift): the whole touch screen 3x, centred.
constexpr Rect kMenuTo {256, 72, SW * 3, SH * 3};

// A conversation: the frame, and the dialogue's parts at 2x (from the DS dialogue screen). The
// name bar whole (x 10-165, rows 10-25: the name's last pixel row is 24) and the text below it
// as on the DS (rows 26 on, 4 columns further left); the arrow whole, x 219-249, rows 155-186
// (its dark outline ends at x 249; column 250 holds only a fringe in the panel's own #6171B2
// on rows 168-175, left out because the screen frame's corner starts at 250,186).
constexpr Rect kDialogueFrame {110, 456, 1060, 256};
constexpr Rect kDialogueNameFrom {10, 6, 156, 20}, kDialogueNameTo {126, 462, 312, 40};
constexpr Rect kDialogueTextFrom {8, 26, 152, 98}, kDialogueTextTo {122, 502, 304, 196};
constexpr Rect kDialoguePortraitFrom {164, 30, 76, 90}, kDialoguePortraitTo {1002, 494, 152, 180};
constexpr Rect kDialogueArrowFrom {219, 155, 31, 32}, kDialogueArrowTo {916, 634, 62, 64};
// The text box carried on to the right of the text piece (the game's own box is narrow): from
// its left border (DS x 9) and top border (row 33).
constexpr Rect kDialogueTextBox {124, 516, 776, 188};

// An activity (with the game's RAM and strings: the HD minigame frame, port/minigame.h, the same two
// pictures at the same places): the touch screen 3x at the left, the world 384 wide at the top right (its height
// from its shape: 288 at the DS's 4:3), both framed; the hints' box (AddHints, below).
constexpr Rect kActivityBottomTo {36, 68, SW * 3, SH * 3};
constexpr int kActivityTopX = 858, kActivityTopY = 68, kActivityTopW = 384;
constexpr Rect kHintBox {850, 400, 400, 252};
// The HD status bar under the small world (its frame's 6 pixels and a gap), as wide as that
// frame: 4:3, rows 344-391, above the hints' box (400) with the bar's shadow.
constexpr int kActivityBarGap = 12;
// Inside it: the padding, the gap between rows, the buttons' chips' height and the column the
// texts start at (after the widest chip, a 7-letter word at 2x); a text line at 3x is 18 pixels
// a character, so 14 fit beside a chip and 20 in a note (activities.cpp wraps them by hand).
constexpr int kHintPad = 14, kHintGap = 8, kHintChipH = 33, kHintTextX = kHintBox.x + kHintPad + 98 + 10;

// No HUD (FrontEnd, CreateASim, None: the title, the main menu, Create-a-Sim, unmapped screens):
// the top screen in kFrontTopBox at its own shape (fitted, centred), the touch screen 2.5x at
// the right, both framed on the dark canvas, and the buttons' hint in a box centred below (these
// screens are touch-only: the navigator's A presses the stick cursor there).
constexpr Rect kFrontBottomTo {616, 72, SW * 5 / 2, SH * 5 / 2};
constexpr Rect kFrontTopBox {24, 72, 568, SH * 5 / 2};
constexpr int kFrontHintY = 600, kFrontHintPad = 14;
constexpr const char* kFrontHint = "STICK: CURSOR - A: TOUCH";

// The top picture's rows without the game's status bar (its last 16 of 192, whatever the
// picture's size): the world CONSOLE shows wherever the HD bar or a conversation's box replaces it.
constexpr int HudCropRows(int topH) { return topH * 176 / 192 > 1 ? topH * 176 / 192 : 1; }

// The hints' box for an activity, at kHintBox's top, as high as its rows (at most kHintBox's):
// a row per hint, its button in a chip of the panel's colour (round for one letter), what it
// does beside it at 3x; a hint without a button is a note across the box.
void AddHints(Composition& c, const std::vector<Hint>& hints);

// The composition for screen s. menuOpen: the menu is up (in town: the touch screen 3x over the
// dimmed world). focus: the index in HotspotsFor(s) the focus ring frames (-1: none; no ring
// when no touchable bottom piece shows it). topW x topH: the top picture's size (App::TopImage).
// Screen::Loading (a black load): the bare backdrop, no piece.
// Screen::None, FrontEnd, CreateASim: the front end's view (kFront*); SaveSlots, ItemHand, Unknown:
// the menu composition with a frame (the stick cursor's screens); never SIDE's bare pair of screens.
// hud, strings (both given and strings->ok: the game's RAM and text read): the HD status bar
// (port/statusbar.h) wherever the world shows the game's HUD, the game's own bar (the top
// picture's last 16 of 192 rows) cropped off that world: town, the menu over the dimmed world,
// and under the small world of the activities that keep the HUD (Social, Vacuum, Catalogue,
// Build; not Cards). Without them: milestone 1's compositions, the game's HUD left in the world.
// game, rom, bottom (the RAM's state, read, the game's strings, the DS's touch screen; all
// given): a conversation is the HD one (port/dialogue.h; its panel alone while the game has no
// page drawn, dialogue not valid); without them, milestone 1's (the game's own box rebuilt wide
// from pieces). The panel's menu (menuOpen on the panel) is the HD one (port/panelmenu.h) when
// its tab's view is valid (PanelMenuView: game and rom given, the tab's data read); focus is then
// an index in that view's items; icons (null: none drawn) gives the game's icons. Else
// milestone 1's: the touch screen 3x over the dimmed world. armed: the view item whose X is
// armed (the navigator's Armed(): Pockets' bin asking), 0 none. pointer: a pointing minigame's
// pointer (port/minigame.h draws it; null: none).
Composition BuildConsole(ScreenState s, bool menuOpen, int focus, int topW, int topH, const HudState* hud = nullptr,
                         const HudStrings* strings = nullptr, const GameState* game = nullptr, const RomData* rom = nullptr,
                         const u32* bottom = nullptr, IconCache* icons = nullptr, u32 armed = 0, const PointerView* pointer = nullptr);

// The world (the top picture's first `rows` rows, topW wide) as CONSOLE draws it: 720 high,
// centred when narrower than the canvas; keepSides: never cut at its sides (fitted to the
// canvas's width, top-aligned, when wider), else cropped to 16:9.
Piece ConsoleWorld(int topW, int rows, bool keepSides);
}
