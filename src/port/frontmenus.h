// The front end's Settings and Load-a-Sim in HD (milestone 5), driven by the controller
// (GameState::front, research m5-front-end §2, §3).
//
// Settings (COptionsMenu in the bottom slot, the title's backdrop on the top): the top screen at
// the left as the main menu shows it (a concept art the game puts there shows large), a panel at
// the right: the title "Settings" (39), a tab bar with the game's four page tabs (live crops of
// its own tab column, 2x: their sprites are not known) between LB and RB, which tap the game's
// tabs (21, 54 + 40 i) in a ring; the page's name (40 + page) and the page:
// - Music / SFX: a slider of the game's value (G+0x2F70 / +0x2F74, 0..4096, read back from RAM,
//   shown in percent); Left / Right drag the game's knob (track x 84..212 on row 96 / 114, the knob
//   at 84 + 128 v / 4096, moved only by a drag that starts on it) to the next tenth
//   (SettingsNotchX); none past an end.
// - Concept Art: the game's 4 x 3 buttons as live crops 2x (the game draws the unlocked picture's
//   thumbnail and a padlock for a locked one: what the HD shows); the unlocked ones (bit i of the
//   current save slot's header word, `SettingsState::conceptBits`) take A: the game's tap (the
//   cell's centre) puts that art on the top screen; a locked one is focusable and does nothing (the
//   game only plays a sound).
// - Credits: the game's own lines (strings 2665 + ((line - 1) mod 436); '^1' a heading in the
//   game's pink, '^2' in its green) scrolling up as the game scrolls them (its position read
//   every frame: the game's speed, nothing to press; the game takes no drag there either).
// B taps the game's X (19,17): the title at phase 0, as the game does.
//
// Load-a-Sim / DS Wireless (CLoadMenu / CWirelessLoadMenu, overlay 8): the top screen dimmed
// over the canvas and a panel like the in-game Save slots' (port/options.h): the title (10), the
// three slots (the number in a round well, green for the game's current slot; the Sim's name, the
// money 28, the place 67 + place, the time the slot was saved as the game prints it: its format
// string 2432 with the decoded header time; "No Data" 11 for an empty slot). A on a used slot
// taps its tick (231, 66 / 112 / 158): the game loads it (its "Loading" popup, port/popup.h, then
// the town); on DS Wireless the tick opens the game's wireless lobby (out of scope: milestone 1's
// picture, its X back). Empty slots are drawn, never focused (the game shows no tick there). B
// taps the game's X (231,22): the title. Never replayed (MenuView::replay false): a late A loads.
//
// Mouse: the tabs, the concept cells, the slider's track and the slots' rows map onto the game's
// own targets.
#pragma once
#include <string>

#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

constexpr u32 kModFrontMenus = 17u << 24;
constexpr u32 kSeSliderId = kModFrontMenus + 0x100, kSeConceptId = kModFrontMenus + 0x200, kLdRowId = kModFrontMenus + 0x300; // + page / cell / row

// The game's touch points and pictures (DS bottom pixels).
constexpr int kSeCloseX = 19, kSeCloseY = 17;
constexpr int kSeTabX = 21, kSeTabY0 = 54, kSeTabDy = 40, kSeTabs = 4;     // tab i at (x, y0 + dy i), box ± 12
constexpr Rect kSeTabFrom {6, 40, 30, 28};                                  // tab i's picture: + dy i (its plate when selected)
constexpr int kSeTrackX0 = 84, kSeTrackW = 128, kSeMusicY = 96, kSeSfxY = 114;
constexpr int kSeCells = 12, kSeCellX0 = 56, kSeCellY0 = 72, kSeCellDx = 48, kSeCellDy = 40, kSeCellW = 40, kSeCellH = 32;
constexpr int kLdTickX = 231, kLdTickY[3] = {66, 112, 158}, kLdCloseX = 231, kLdCloseY = 22;
// The game's strings.
constexpr int kStrSeTitle = 39, kStrSePage0 = 40;                           // + page: Music Volume, SFX Volume, Concept Art, Credits
constexpr int kStrCredits0 = 2665, kCreditsLines = 436;
constexpr int kStrLdTitle = 10, kStrLdNoData = 11, kStrLdMoney = 28, kStrLdPlace0 = 67, kStrLdTime = 2432, kStrLdAm = 178, kStrLdPm = 179;
constexpr int kStrWireless = 9;
// The game's colours for the credits' headings (sampled on F-opt-credits-400).
constexpr u32 kCreditsPink = 0xFFF35961, kCreditsGreen = 0xFF79FB00;

// HD geometry (canvas). Settings: the panel at the right of the top screen (port/front.h's main
// menu: kFrTopBox, kFrPanel), its title, tab bar, the page's name, its content and hints.
constexpr Rect kSeTitle {840, 64, 392, 56};
constexpr Rect kSeTabBar {840, 132, 392, 76};
constexpr int kSeTabCell = 64, kSeTabsX = 1036 - 2 * kSeTabCell;
constexpr Rect kSeLabel {840, 222, 392, 44};
constexpr Rect kSeSlider {840, 320, 392, 180};
constexpr int kSeHdTrackX = 880, kSeHdTrackW = 312;                         // the slider's HD track (0 % .. 100 %)
constexpr int kSeHdCellW = 88, kSeHdCellH = 72, kSeCellGapX = 12, kSeCellGapY = 16, kSeGridX = 842, kSeGridY = 292;
constexpr Rect kSeCredits {840, 276, 392, 316};
constexpr int kSeCreditsRow = 34;
constexpr Rect kSeHints {840, 608, 392, 40};
constexpr Rect SettingsTabRect(int i) { return {kSeTabsX + i * kSeTabCell, kSeTabBar.y + 6, kSeTabCell, kSeTabBar.h - 12}; }
constexpr Rect SettingsCellRect(int i) { return {kSeGridX + (i & 3) * (kSeHdCellW + kSeCellGapX), kSeGridY + (i >> 2) * (kSeHdCellH + kSeCellGapY), kSeHdCellW, kSeHdCellH}; }
// Load: the in-game Save slots' panel (port/panelmenu.h's kPmPanel), its rows.
constexpr Rect kLdTitle {200, 44, 880, 64};
constexpr int kLdRowY0 = 124, kLdRowH = 142, kLdRowGap = 14;
constexpr Rect LoadRowRect(int i) { return {200, kLdRowY0 + i * (kLdRowH + kLdRowGap), 880, kLdRowH}; }

// The DS x the game's knob is at for volume v (0..4096), and where a drag from it must end for
// the volume one tenth down (dir -1) or up (+1) from v: -1 when nothing lies that way. The game
// moves the knob by the stylus's move from where it took it (32 a pixel), so the drag is that
// distance. A volume within 1.5 % of a tenth counts as on it.
int SettingsKnobX(int volume);
int SettingsNotchX(int volume, int dir);

// The time a slot was saved as the game prints it: its format (string 2432: %n month, %j day,
// %g / %h hour 1..12 (%h two digits), %G / %H hour 0..23 (%H two digits), %i minutes, %a am / pm,
// %A AM / PM, %d / %m day / month in two digits) with the row's decoded time; am / pm the game's
// words (178 / 179). Empty when the time was not decoded.
std::string SlotTimeText(const std::string& format, const LoadRow& r, const std::string& am, const std::string& pm);
// A credits line as the game shows it: its string, the '^1' / '^2' heading mark taken off into
// `colour` (0: plain white).
std::string CreditsLine(const RomData& rom, int line, u32& colour);

// The views (valid when the RAM's part reads, the screen is the front end's and the game's strings
// are there); the Of forms without the strings' check (tests; rom null leaves the words empty).
MenuView SettingsView(ScreenState s, const GameState* game, const RomData* rom);
MenuView SettingsViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
MenuView LoadView(ScreenState s, const GameState* game, const RomData* rom);
MenuView LoadViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The screen drawn into c (empty: the whole canvas is this screen's), the focus on item `focus`.
void BuildSettings(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, const u32* bottom, int topW, int topH);
void BuildLoad(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, int topW, int topH);
// (A check, --measure-menu) Settings' and Load's words in their boxes: the titles, the pages'
// names, every credits line, "No Data", the places, the widest name, money and time.
void MeasureFrontMenus(const RomData& rom, BlockMeasure& m);
}
