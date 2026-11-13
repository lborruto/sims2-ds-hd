// CONSOLE: the game on one 16:9 screen. Per recognised bottom screen (screens.h), a composition
// of pieces of the two DS screens on a 1280 x 720 canvas, as in the approved mockup: in town (the
// panel) the world fills the canvas (a small hint for the HD menu; milestone 1: a rail of the touch
// screen's tab icons and phone at the right); the menu, the phone, popups and Give Item are the whole touch screen 3x over the dimmed
// world (framed for SaveSlots and Unknown); a black load is the bare backdrop; without a
// HUD (the front end) the top screen at the left and the touch screen 2.5x at the right, each in its frame; a conversation is the world with the
// game's dialogue redrawn in HD at the bottom (port/dialogue.h; without the game's text, its own box rebuilt wide); an activity is the touch screen 3x at the left
// with the world small at the top right. DS pieces are drawn as the game draws them, 2x or 3x;
// frames and the focus ring are overlays in the game's colours.
#pragma once
#include "activities.h"
#include "compose.h"
#include "gamestate.h"
#include "port/prompt.h"
#include "port/statusbar.h"
#include "screens.h"

namespace sims2hd
{
class RomData;
class IconCache;
constexpr int kConsoleW = 1280, kConsoleH = 720;

// The game's colours (frame, panel, outline, highlight).
constexpr u32 kFrameArgb = 0xFF828ACB, kPanelArgb = 0xFF5961A2, kOutlineArgb = 0xFF383869, kRingArgb = 0xFF309A41;
// The world behind the menu.
constexpr u32 kDimArgb = 0x8C000000;

// Town with the game's words (RomData and RAM reading): no rail, the HD menu (Back / E) has the
// tabs; a hint chip (the button, the word) sits at the canvas's right edge level with the status bar.
// Without them (milestone 1) the rail stays, the only way to see the tabs: at the canvas's right
// edge, vertically centred, the phone icon above the tab column, both 2x, in a frame.
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
// the right, both framed on the dark canvas, and the buttons' hint (the kit's chips: the right
// stick, A) in a panel centred below (these screens are touch-only: the navigator's A presses the
// stick cursor there).
constexpr Rect kFrontBottomTo {616, 72, SW * 5 / 2, SH * 5 / 2};
constexpr Rect kFrontTopBox {24, 72, 568, SH * 5 / 2};
constexpr int kFrontHintY = 600;

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
// Screen::None, FrontEnd, CreateASim: the front end's view (kFront*); SaveSlots, Unknown:
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
// Which of BuildConsole's compositions a frame got (--print-console; ConsoleHold, consolehold.h).
// The *M1 ones are milestone 1's (the game's own screen framed): FrontM1 the front end's two
// screens, CursorM1 the touch screen 3x over the dimmed world, DialogueM1 the game's box rebuilt,
// ActivityM1 an activity's two screens; the rest are the HD ones (Backdrop the bare canvas of a
// black load, WorldOnly / Veiled the world while the panel comes).
enum class ConsoleBranch { Front, Cas, CasFade, Backdrop, FrontM1, Screen, Veiled, WorldOnly, CursorM1, Town, Dialogue, DialogueM1, Minigame, ActivityM1 };
const char* ConsoleBranchName(ConsoleBranch b);
inline bool IsMilestone1(ConsoleBranch b) { return b == ConsoleBranch::FrontM1 || b == ConsoleBranch::CursorM1 || b == ConsoleBranch::DialogueM1 || b == ConsoleBranch::ActivityM1; }
Composition BuildConsole(ScreenState s, bool menuOpen, int focus, int topW, int topH, const HudState* hud = nullptr,
                         const HudStrings* strings = nullptr, const GameState* game = nullptr, const RomData* rom = nullptr,
                         const u32* bottom = nullptr, IconCache* icons = nullptr, u32 armed = 0, const PointerView* pointer = nullptr,
                         int payout = 0, bool pictureClosed = false, bool veiled = false, int worldOnlyFrames = 0,
                         ConsoleBranch* branch = nullptr, const PromptSteady* prompt = nullptr);
// pictureClosed: the view's big picture (MenuView::picture: Settings' concept art) closed by B.
// veiled: the panel's menu is coming back (Navigator::Returning: B from the Save slots or the phone
// opened from it, the game passing through Interact first): the dimmed world and the status bar
// alone while the panel shows with its menu closed, never the world's flash. worldOnlyFrames: the
// frames in a row the panel was coming without its tab read, or no bottom object at all
// (App::consoleWorldOnly): the town's world for kWorldOnlyMax of them, then milestone 1's framed
// touch screen and the cursor's hints (an unforeseen screen is never a dead end).
constexpr int kWorldOnlyMax = 10;
// prompt: the Interact target's steadiness (App::consolePrompt, port/prompt.h; null: none): the
// town's world shows what A does there ("A Buy", the target's name) once the target drawn has been
// steady kPromptSteadyFrames, on its own layer above the status bar.
// payout (above): the money's last change (App::consolePayout: the slot machine's coin and its
// payout, a ticket's prize, ...), shown on the panel's world beside the HD status bar while not 0.

// The town's hints at the strip's right (unzoomed; console.cpp zooms them about kTownHintsRight):
// the camera's (the right stick; the keyboard's own L / R keys when it was used last: its two
// keycaps) and the TOUCH MENU's, "Menu". w: their width together.
constexpr float kTownHintSize = 24;
constexpr int kTownHintsRight = kConsoleW - 30, kTownHintsBetween = 22, kTownKeysGap = 6;
struct TownHints
{
    const char* menu = "";
    const char* camera = "";
    bool keys = false;
    int w = 0, menuW = 0;
};
TownHints TownHintsFor(const RomData* rom, const ui::ChipLook& look);
void AddTownHints(Composition& c, const TownHints& h);

// The world (the top picture's first `rows` rows, topW wide) as CONSOLE draws it: 720 high,
// centred when narrower than the canvas; keepSides: never cut at its sides (fitted to the
// canvas's width, top-aligned, when wider), else cropped to 16:9.
Piece ConsoleWorld(int topW, int rows, bool keepSides);
// The kit's focus ring (ui::FocusRing's look: the game's green kFocusOuterW outside `r`, a white
// line kFocusInnerW wide inside it) as two outline overlays, so the DS picture under `r` shows
// through: the focus over DS pieces (the cards, the social screen, MixerSave's keyboard, MooGoo).
void AddKitRingOverlay(Composition& c, Rect r);
}
