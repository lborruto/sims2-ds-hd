// The shop's catalogue (overlay 2, CBuyMenu) in HD: the General Store and the food vendors.
// A game-style panel over the dimmed world (port/panelmenu.h's frame, no tab bar): the shop's
// name and the player's money in the title band, the shop list (the store: a 3 x 2 grid over its
// twelve items, scrolled by the game's own arrows; a vendor: its six fixed items, 2 x 3), the
// owned items beside it (the pockets' six places: the store only), the focused item's name,
// price and description, and the pay button (the game's simoleon button). Everything from
// GameState::catalogue (RAM, every frame: the store's list is re-rolled each time it opens) and
// the game's strings and item icons (the record table's sprites, upscaled).
//
// The game's catalogue is the stylus only (no DS button is read). What performs, always the
// game's own taps (the hit rectangles measured in the game):
//   A on a shop item or an owned one: tap it. The game selects it, and that sets its mode: a shop
//     item is "Buy" (2391), an owned one "Sell" (2392); the HD headers light the game's mode.
//   A on Pay: tap the simoleon button (146,129; a vendor's 114,129): the game buys the selected
//     shop item (money − 2 × its record price, the item into the pockets) or sells the selected
//     owned one (money + 1 × the price). Pay stays focusable, but is greyed and A does nothing
//     when the game would not pay (CataloguePays: not enough money, the pockets full, a vendor's
//     sale, a quest item the game will not buy back, nothing selected just after a sale).
//     The view never replays a press the gate held (MenuView::replay false): a press is a spend.
//   Up / Down past the shop grid's rows: the game's arrows (45,168 / 77,168; the store only, when
//     the game has them enabled): its list moves by a row of three and the focus follows.
//   B (and MENU): the game's X (235,19), back to the panel.
// The focused item's name, description and price (a shop item's buy price 2 × the record's, an
// owned one's sell price 1 ×) are shown before A; the pay button shows the game's mode and the
// price of the game's selection, which is what the game charges or pays.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "port/pockets.h"
#include "screens.h"

namespace sims2hd
{
class RomData;
class IconCache;

// The game's touch points (DS bottom pixels; its hit boxes are ±16 around the cells' and the
// places' centres, 30 x 30 for the buttons).
constexpr int kCaStoreCellX0 = 22, kCaStoreCellDx = 39, kCaCellY0 = 88, kCaCellDy = 40; // store cell c: (x0 + dx (c % 3), y0 + dy (c / 3))
constexpr int kCaVendorCellX0 = 25, kCaVendorCellDx = 39;                                   // vendor cell c: (x0 + dx (c % 2), y0 + dy (c / 2))
constexpr int kCaOwnedX0 = 193, kCaOwnedDx = 39;                                            // owned place i: (x0 + dx (i % 2), y0 + dy (i / 2))
constexpr int kCaUpX = 45, kCaDownX = 77, kCaArrowY = 168;
constexpr int kCaPayX = 146, kCaVendorPayX = 114, kCaPayY = 129;
constexpr int kCaCloseX = 235, kCaCloseY = 19;
constexpr int kCaStore = 0x33;    // Johnny Smith's General Store (the only shop that buys back)

// The game's sprites (descriptor ids; tiles − 1, palette + 1), as the ROM draws them: the
// simoleon (pay) button 7671 (32 x 32; pressed 7668), the arrows up 7698 / down 7677 (24 x 24),
// the X 7683. (An earlier reading named 7671 the down arrow and 7677 the pay button: the decoded
// pictures say the reverse.)
constexpr int kSprCaPay = 7671, kSprCaUp = 7698, kSprCaDown = 7677, kSprCaX = 7683;

// The game's strings: the store's title, the modes, the money's / price's "$@1".
constexpr int kStrStore = 2518, kStrBuy = 2391, kStrSell = 2392, kStrCaMoney = 28;

// HD geometry (canvas), the frame as the panel's (kPmPanel), no tab bar.
constexpr int kCaCell = 104, kCaGap = 8;   // (a cell holds an item of 28 px at tier L: 84; a larger one at S)
constexpr Rect kCaTitle {200, 44, 880, 64};
constexpr Rect kCaClose {1016, 48, 56, 56};      // the game's X, in the title band
constexpr int kCaTop = 124, kCaBottom = 578;     // the content's band
constexpr int kCaHeadH = 40, kCaGridY = 176;     // the Buy / Sell headers, the grids under them
constexpr float kCaNameSize = 30, kCaNameMin = 18, kCaPriceSize = 28, kCaDescSize = 24, kCaDescMin = 18, kCaPaySize = 28, kCaPayMin = 20,
                kCaPayNameSize = 22, kCaPayNameMin = 16;
// The store: the shop grid 3 x 2 at the left with the arrows under it, the info in the middle,
// the owned places 2 x 3 at the right. A vendor: its grid 2 x 3, the info beside it.
struct CatalogueGeometry
{
    Rect shopHead, ownedHead, info, name, price, desc, pay, payIcon, payText, up, down;
    int shopX, shopCols, ownedX;
};
CatalogueGeometry CatalogueLayout(bool vendor);

// Item ids (stable across rebuilds; a cell's id carries its record row and list index, an owned
// place's its item id and place: a list re-rolled or an item sold is another item). The kinds
// sit in bits 20-23 and the row / item id (< 0x1000) above the index (4 bits), so no two meet.
constexpr u32 kCaCellId = kModCatalogue + (1u << 20), kCaOwnedId = kModCatalogue + (2u << 20), kCaPayId = kModCatalogue + (3u << 20),
              kCaEmptyId = kModCatalogue + (4u << 20); // + 8 for an empty owned place
inline u32 CatalogueCellId(int row, int index) { return kCaCellId + ((u32)row << 4) + (u32)index; }
inline u32 CatalogueOwnedId(int itemId, int place) { return kCaOwnedId + ((u32)itemId << 4) + (u32)place; }
// The view's order: the six shop cells, Pay, then the store's six owned places.
constexpr int kCaPayIndex = 6, kCaOwned0 = 7;

// The item ids the game will not buy back (its quest items: a tap on its button only plays a sound).
bool CatalogueRefusesSale(int itemId);

// Whether the game's pay button would buy or sell the game's selection now: the rules its text
// draw recomputes S+0x25D from (Buy: the money covers 2 × the price and the pockets have a free
// place; Sell: the General Store, a price above 0, not a quest item) and a selection in its list
// (none just after a sale). Not the flag itself: it reads 0 for the frame the game re-typesets
// after every tap, and the HD Pay would flicker.
bool CataloguePays(const CatalogueState& ca);

// The view (valid when RAM's catalogue reads and the game's strings are there): the six shop
// cells (empty ones past the list disabled), Pay, then the store's six owned places (empty ones
// disabled).
MenuView CatalogueView(ScreenState s, const GameState* game, const RomData* rom);
// The same without the strings' check (tests; rom null leaves the words empty).
MenuView CatalogueViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c (after the dimmed world): the panel, title and money, the grids,
// the info, Pay, the arrows, the hints, the focus on view item `focus`, and the mouse's pieces.
void BuildCatalogue(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons);

// (A check, --measure-menu) every word the catalogue can show in rom's language, in its box:
// every item's name and description (store and vendor boxes), the shops' titles beside the
// widest money, the headers, the pay button at the widest price, the hints.
void MeasureCatalogue(const RomData& rom, BlockMeasure& m);
}
