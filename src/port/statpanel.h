// The Stat Panel (overlay 14, CStatPanelMenu: the hotel lobby's Stat Panel, "Examine") in HD. A
// game-style panel over the dimmed world, the panel menu's frame with a tab bar for the game's
// five pages (LB, Main and the four pages' own pictures, RB; the game's X at the right), the
// page's title in a well, the page, and the hints; from GameState::stat (RAM, every frame; the
// game typesets its pages from G when it opens and G does not change while it is up):
//   Main (page 0): the game's four buttons (Aspirations, Hotel Info, Skills, Game Stats: 1990..1993
//     under their pictures);
//   Aspirations (1): the aspiration's picture (362: its frames in the game's on-screen order,
//   Fortune, Popularity, Romance, Party, so frame = kStAspFrame[G+0x68]) and name (1994 + it), the
//     gauge (383, frame = the game's round(20 · points / threshold), full at the top level) and the
//     level's stars (401, frame = the level), both in the level's palette as the game loads it,
//     what the aspiration gives (1998 + it) and how to level it (2002 + it);
//   Hotel Info (2): the score, guests stayed, money earned and spent (999999: "LOTS!" 2011) and
//     the net profit, each with the game's icon (2006..2010);
//   Skills (3): the five skills (2399 + i) with their icons and the game's star bars (frame = the
//     level, u8 G+0x5B + i);
//   Game Stats (4): aliens, goons, vacuum and metal detector earnings (2012..2015; a value below 0
//     or above 2,000,000,000: "LOTS!").
//
// What performs, always the game's own taps (the research's touch points, m4 §7; the game reads no
// DS key here): on Main, A on a button taps it (the game fades to its page, S+0x38); LB / RB (and
// Left / Right on a page) go to the previous / next page in the ring Main, Aspirations, Hotel
// Info, Skills, Game Stats through the game's own way: from Main the page's button, from a page
// its X (back to Main) then, once Main is back, the button (a wait the game's fade needs). B: the
// game's X (228,25): back to Main from a page, closed (the panel) from Main. While the game still
// fades the old page out its taps would land on it: every action then waits for the fade first.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "port/panelmenu.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

// The game's touch points (DS bottom pixels): Main's buttons for pages 1..4 (their centres), the X.
constexpr int kStPages = 5;
constexpr int kStButtonX[kStPages] = {0, 72, 176, 72, 176}, kStButtonY[kStPages] = {0, 50, 50, 130, 130};
constexpr Rect kStButtonFrom[kStPages] = {{}, {32, 22, 80, 56}, {136, 22, 80, 56}, {32, 102, 80, 56}, {136, 102, 80, 56}};
constexpr int kStCloseX = 228, kStCloseY = 25;
constexpr Rect kStCloseFrom {212, 9, 32, 32};
// Frames: a page's fade-out before the new page's buttons take a tap (measured: the old page fades
// out for about 33 frames after the tap, S+0x3C 1, then the new one fades in with its buttons
// up, 2), and an X's wait before Main's buttons do (the tap's own frames and that fade).
constexpr int kStFadeWait = 40, kStSwitchWait = 44, kStShowWait = 16;

// The game's sprites (descriptor ids).
constexpr int kSprStMain = 7630;                                             // the Stat Panel object's own picture (the Main tab)
constexpr int kSprStNav[kStPages] = {kSprStMain, 398, 4100, 7488, 7636};     // Main's buttons (frame 0)
constexpr int kSprStAspiration = 362;                                        // frame = kStAspFrame[the aspiration]
// Sprite 362's frame for aspiration a (0 Fortune, 1 Romance, 2 Popularity, 3 Party): its frames are
// Fortune, Popularity (the group), Romance (the couple), Party (the balloons), as the ROM draws them.
constexpr int kStAspFrame[4] = {0, 2, 1, 3};
constexpr int kSprStGauge = 383, kSprStStars = 401;                          // frame = the gauge's fill / the level
// The level's palettes (resource ids), j = {0, 1, 1, 2}[level] (the game's 0x0214B2AC).
constexpr int kStGaugePal[4] = {0x18C, 0x186, 0x183, 0x189}, kStStarsPal[4] = {0x19E, 0x198, 0x195, 0x19B};
constexpr int kStLevelPal[4] = {0, 1, 1, 2};
constexpr int kSprStHotel[5] = {4123, 4103, 4114, 4117, 4120};
constexpr int kSprStSkill[5] = {7509, 7497, 7491, 7503, 7515};
constexpr int kSprStSkillBar[5] = {7512, 7500, 7494, 7506, 7518};            // frame = the level (ten stars)
constexpr int kSprStStats[4] = {7639, 7642, 7648, 7645};
constexpr int kSprStX = 7683;
// The game's strings.
constexpr int kStrStTitle = 477;                 // "Stat Panel" (the object's name: Main has no title of its own)
constexpr int kStrStPage = 1989;                 // + page 1..4
constexpr int kStrStAspName = 1994, kStrStAspBonus = 1998, kStrStAspHow = 2002;   // + aspiration
constexpr int kStrStHotel = 2006;                // 2006..2010
constexpr int kStrStLots = 2011;
constexpr int kStrStStats = 2012;                // 2012..2015
constexpr int kStrStSkill = 2399;                // + skill

// HD geometry (canvas): the panel menu's (kPmPanel, kPmTabBar, kPmTitle, kPmContent, kPmHints).
// The tab bar's cell of a page: the pages' names in a row between LB and RB, each as wide as its
// widest word needs (rom's; null: all alike), the rest shared.
Rect StatTabRect(int page, const RomData* rom = nullptr);
Rect StatButtonRect(int page);                   // Main's button for page 1..4
constexpr Rect kStClose {kPmTitle.x + kPmTitle.w - 52, kPmTitle.y + 2, 48, 48};   // the X, at the title well's right end

// Item ids: the kind in bits 20-23 (1 a page: Main's button for it, or its tab while it is shown),
// the page below.
constexpr u32 kStPageId = kModStat | (1u << 20);

// The game's own way from the page shown to page `to` (empty: the same page). Every step taps:
// Main's button, the X, or the X, a wait and the button.
MenuAction StatPageAction(const StatState& st, int to);
// A page's line as the game prints it, split as a receipt: the words before its first value, the
// value (the game's placeholders filled, its '$' drawn §). Hotel Info i 0..4, Game Stats i 0..3.
void StatHotelLine(const StatState& st, const RomData& rom, int i, std::string& label, std::string& value);
void StatStatsLine(const StatState& st, const RomData& rom, int i, std::string& label, std::string& value);

// The view (valid when RAM's stat panel reads and the game's strings are there): on Main, the
// four buttons (A the page's tap), B the X (closes); on a page, one item (its tab: where the focus
// rests, its id the page's so Main's button for it gets the focus back), Left / Right and B the
// game's way to the previous / next page and to Main; LB / RB everywhere.
MenuView StatPanelView(ScreenState s, const GameState* game, const RomData* rom);
MenuView StatPanelViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c.
void BuildStatPanel(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons);

// (A check, --measure-menu) every word the stat panel can show in rom's language: the titles, the
// buttons' names, every aspiration's name and texts, the lines beside their widest values (LOTS!,
// the largest numbers), the skills' names, the hints.
void MeasureStatPanel(const RomData& rom, BlockMeasure& m);
}
