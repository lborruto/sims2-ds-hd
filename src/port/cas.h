// Create-a-Sim in HD (milestone 5), driven by the controller (GameState::front.cas, research
// m5-front-end §4). The game's own screen (CCreateASim, overlay 3) does all the work: every item
// here is one of its touch targets, and what the HD shows is read back from RAM every frame, so
// the HD page is the game's page (S+0x74) and follows it the frame its page object (S+0x58)
// swaps; while a page change is in flight (`CasState::switching`, 2-4 frames) the panel shows the
// new page's tab with nothing in it and nothing to press.
//
// The model: the game's top screen (the 3D Sim in its room) at its shape, large, at the left,
// framed. The right stick turns it (MenuView::stickTurns: the DS's L / R held, as the game turns
// it); the mouse's right drag too (the piece is the camera). An RS chip under it.
//
// The panel at the right: a tab bar with the game's five page tabs (live crops of its own tab
// column, 2x: their sprites are not known) between LB and RB, which tap the game's tabs (18,
// 23 + 37 i) in a ring; Start taps the next page's tab (pages 0-3: on the Name page that is what
// commits the name, as the game commits it when the page closes); the title well; the page:
// - Name (0): the title well is the name field, the game's live buffer (N+0x70) in seven slots
//   (the game's limit: never more); the gender buttons ♀ / ♂ and the four skin figures as live
//   crops 2x (A taps them: 66,25 / 102,25; 144 + 29 i, 23); the keyboard's three pages as a row
//   (ABC abc 123!, the game's own labels; A or X taps the game's page buttons, 58, 115 / 145 /
//   174; X the next one in a ring) over the HD keyboard: the page's characters from the game's
//   strings (241 + k A-Z, 267 + k a-z, 293 + k the 20 of 123!), nine to a row as the game has
//   them; A types (the game's cell tap, 88 + 19 c, 85 + 30 r), nothing once the name holds seven
//   (the game refuses it); the backspace key (the game's ← as a live crop; 165,168) and Y erase
//   the last character (nothing at empty, as the game).
// - Hair (1): the eight colour swatches (live crops 2x; 59 + 25 i, 42) and the 2 x 2 style grid
//   (live crops 3x of the game's cells, 117 + 60 c, 90 + 63 r) over its column scroll; Right from
//   the right column / Left from the left one tap the game's arrows (228,120 / 65,120) while the
//   game shows them (CasState::canLeft / canRight, its sprites); the arrows drawn as live crops.
// - Face (2): the 3 x 3 grid (live crops 3x, 90 + 57 c, 58 + 51 r).
// - Clothes (3): the six categories (live crops 2x, 50, y_i) and the 3 x 2 cells of the game's
//   page (live crops 3x, 111 + 48 c, 72 + 64 r), its arrows as buttons beside them (77,105 /
//   240,105: A pages; focusable only while the game shows them, the categories left of them);
//   the cell holding the game's choice rimmed (its ring sprite, CasState::clothesCell). Medium:
//   which item a cell holds is the game's table (not decoded), the HD shows its picture.
// - Keep this Sim? (4): the four aspirations (the game's pictures, sprite 362, in its order on
//   screen: Fortune, Popularity, Romance, Party, the others dimmed; A taps them, 82 + 45 j, 44), the chosen one's
//   name (1994 + a) and text (1998 + a), the game's tick (974, "Yes" 48: A taps 116,170, the
//   game then starts: never replayed) and cross (7683, "No" 49: A or B tap 176,170, the title at
//   phase 0, as the game). Once one is pressed (C+0x11) nothing more is (the game counts down).
// Every item maps the mouse onto its game target.
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

constexpr u32 kModCas = 18u << 24;
constexpr u32 kCasGenderId = kModCas + 0x100, kCasSkinId = kModCas + 0x110, kCasKeyId = kModCas + 0x200, kCasBackId = kModCas + 0x220,
              kCasKbPageId = kModCas + 0x230, kCasSwatchId = kModCas + 0x300, kCasHairId = kModCas + 0x310, kCasFaceId = kModCas + 0x400,
              kCasCatId = kModCas + 0x500, kCasClothId = kModCas + 0x510, kCasAspId = kModCas + 0x600, kCasTickId = kModCas + 0x610,
              kCasCrossId = kModCas + 0x611;

constexpr int kCasPages = 5;            // the tabs: Name, Hair, Face, Clothes, Keep this Sim?
constexpr int kCasKeys = 26, kCasKeysPerRow = 9;
constexpr int kCasKeyCount[3] = {26, 26, 20};
// The game's touch points and pictures (DS bottom pixels).
constexpr int kCasTabX = 18, kCasTabY0 = 23, kCasTabDy = 37;
constexpr Rect kCasTabFrom {1, 8, 34, 30};                     // + dy i
constexpr int kCasGenderX[2] = {66, 102}, kCasGenderY = 25;
constexpr Rect kCasGenderFrom[2] = {{47, 7, 37, 37}, {83, 7, 37, 37}};
constexpr int kCasSkinX0 = 144, kCasSkinDx = 29, kCasSkinY = 23;
constexpr Rect kCasSkinFrom {134, 4, 21, 42};                  // + dx i
constexpr int kCasKeyX0 = 88, kCasKeyDx = 19, kCasKeyY0 = 85, kCasKeyDy = 30;
constexpr Rect kCasKeyBoxFrom {79, 70, 19, 30};                // key (c, r): + (19 c, 30 r)
constexpr int kCasBackX = 165, kCasBackY = 168;
constexpr Rect kCasBackFrom {153, 158, 26, 21};
constexpr int kCasKbX = 58, kCasKbY[3] = {115, 145, 174};
constexpr int kCasSwatchX0 = 59, kCasSwatchDx = 25, kCasSwatchY = 42;
constexpr Rect kCasSwatchFrom {49, 33, 20, 19};                // + dx i
constexpr int kCasHairX0 = 117, kCasHairDx = 60, kCasHairY0 = 90, kCasHairDy = 63;
constexpr Rect kCasHairFrom {94, 63, 46, 53};                  // + (dx c, dy r)
constexpr int kCasHairLeftX = 65, kCasHairRightX = 228, kCasHairArrowY = 120;
constexpr Rect kCasHairLeftFrom {59, 88, 13, 68}, kCasHairRightFrom {221, 88, 15, 68};
constexpr int kCasFaceX0 = 90, kCasFaceDx = 57, kCasFaceY0 = 58, kCasFaceDy = 51;
constexpr Rect kCasFaceFrom {68, 39, 43, 39};                  // + (dx c, dy r)
constexpr int kCasCatX = 50, kCasCatY[6] = {20, 52, 84, 112, 142, 172}, kCasCatFromY[6] = {5, 34, 66, 98, 130, 159};
constexpr Rect kCasCatFrom {37, 0, 26, 28};                    // y from kCasCatFromY
constexpr int kCasClothX0 = 111, kCasClothDx = 48, kCasClothY0 = 72, kCasClothDy = 64;
constexpr Rect kCasClothFrom {92, 45, 38, 54};                 // + (dx c, dy r)
constexpr int kCasClothLeftX = 77, kCasClothRightX = 240, kCasClothArrowY = 105;
constexpr Rect kCasClothLeftFrom {70, 75, 14, 66}, kCasClothRightFrom {233, 74, 14, 67};
constexpr int kCasAspX0 = 82, kCasAspDx = 45, kCasAspY = 44;
constexpr Rect kCasAspFrom {60, 24, 44, 48};                   // + dx j
constexpr int kCasAspOfSlot[4] = {0, 2, 1, 3};                 // the game's icons, left to right: Fortune, Popularity, Romance, Party
constexpr int kCasTickX = 116, kCasCrossX = 176, kCasAnswerY = 170;
// The game's strings and sprites.
constexpr int kStrCasKeys[3] = {241, 267, 293};
constexpr int kStrCasTitle0 = 2016;                           // + page 1..4: Hair Style, Face Style, Clothes, Confirm
constexpr int kStrCasKeep = 2021, kStrAsp0 = 1994, kStrAspText0 = 1998, kStrCasYes = 48, kStrCasNo = 49;
constexpr int kSprCasTick = 974, kSprCasCross = 7683;
// The aspirations' picture (the stat panel's too): frames Fortune, Popularity, Romance, Party, the
// game's order on this page (kCasAspOfSlot), 64 x 64, drawn kCasAspIcon wide.
constexpr int kSprCasAsp = 362, kCasAspIcon = 96;
// The keyboard pages' labels, as the game's own buttons show them (not words: the same in every language).
constexpr const char* kCasKbLabel[3] = {"ABC", "abc", "123!"};

// HD geometry (canvas).
constexpr Rect kCasTopBox {16, 16, 664, 640};                  // the model, fitted at its shape
constexpr Rect kCasPanel {696, 16, 568, 688};
constexpr int kCasX = 716, kCasW = 528;                        // the panel's inside
constexpr Rect kCasTabBar {kCasX, 30, kCasW, 76};
constexpr int kCasTabCell = 80, kCasTabsX = kCasX + (kCasW - 5 * kCasTabCell) / 2;
constexpr Rect kCasTitle {kCasX, 118, kCasW, 52};
constexpr Rect kCasHints {kCasX, 652, kCasW, 40};
// Name
constexpr int kCasSlotW = 44, kCasSlotStep = 52, kCasSlotsX = kCasX + (kCasW - 7 * kCasSlotStep + 8) / 2;
constexpr Rect CasGenderRect(int g) { return {750 + 94 * g, 188, 84, 84}; }
constexpr Rect CasSkinRect(int i) { return {956 + 66 * i, 184, 56, 92}; }
constexpr Rect CasKbPageRect(int p) { return {744 + 130 * p, 290, 120, 48}; }
constexpr int kCasKeyW = 50, kCasKeyStep = 56, kCasKeysX = kCasX + 15, kCasKeysY = 352;
constexpr Rect CasKeyRect(int i) { return {kCasKeysX + kCasKeyStep * (i % 9), kCasKeysY + kCasKeyStep * (i / 9), kCasKeyW, kCasKeyW}; }
constexpr Rect kCasBackRect {kCasKeysX + 3 * kCasKeyStep, kCasKeysY + 3 * kCasKeyStep, 3 * kCasKeyStep - 6, kCasKeyW};
// Hair
constexpr Rect CasSwatchRect(int i) { return {728 + 64 * i, 186, 56, 56}; }
constexpr Rect CasHairRect(int cell) { return {822 + 166 * (cell & 1), 258 + 183 * (cell >> 1), 150, 171}; }
constexpr Rect kCasHairLeft {778, 258, 30, 354}, kCasHairRight {1152, 258, 30, 354};
// Face
constexpr Rect CasFaceRect(int i) { return {754 + 155 * (i % 3), 190 + 141 * (i / 3), 141, 129}; }
// Clothes
constexpr Rect CasCatRect(int i) { return {kCasX, 190 + 70 * i, 64, 64}; }
constexpr Rect CasClothRect(int cell) { return {824 + 130 * (cell % 3), 200 + 184 * (cell / 3), 120, 174}; }
constexpr Rect kCasClothLeft {788, 200, 32, 358}, kCasClothRight {1208, 200, 32, 358};
// Keep this Sim?
constexpr Rect CasAspRect(int slot) { return {746 + 120 * slot, 186, 108, 112}; }
constexpr Rect kCasAspName {kCasX, 312, kCasW, 48};
constexpr Rect kCasAspText {kCasX, 370, kCasW, 150};
constexpr Rect kCasTick {756, 544, 210, 88}, kCasCross {994, 544, 210, 88};

// The character key i of keyboard page p shows (the game's string), empty past the page's keys.
std::string CasKeyText(const RomData& rom, int page, int i);
// A name as the HD shows it: the game's '$' its simoleon, drawn §.
std::string CasNameText(const std::string& name);

// The view (valid when the RAM's part reads, the screen is Create-a-Sim's and the game's strings
// are there: CONSOLE's HD Create-a-Sim; else milestone 1's picture); the Of form without the
// strings' check (tests; rom null leaves the words and the keys' characters empty).
MenuView CasView(ScreenState s, const GameState* game, const RomData* rom);
MenuView CasViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The screen drawn into c (empty: the whole canvas is this screen's), the focus on item `focus`.
void BuildCas(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom,
              int topW, int topH);
// (A check, --measure-menu) the pages' titles, the aspirations' names and texts, Yes / No, the
// hints' words and every key's character, in their boxes.
void MeasureCas(const RomData& rom, BlockMeasure& m);
}
