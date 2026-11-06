// The panel's Pockets tab in HD (port/panelmenu.h's frame): the six pocket slots as a grid of the
// game's own item icons (the record table's sprite, upscaled), the three key items (the
// Vacuum, the Super Drencher, the Metal Detector: the game's tool buttons, coloured when owned,
// a silhouette when not), the four counted items with their counts (their icons a live crop of
// the game's buttons: their sprite is not one the port decodes), and the focused item's name and
// description beside, with the hand ("Use") and the bin. Empty slots are drawn but never focused.
// Everything from GameState::town.inventory and .pockets (RAM) and the game's strings.
//
// What performs, always through the game's own taps (src/hotspots.cpp's kPockets points):
//   A on a slot or an owned key item: tap it (the game selects it), then tap the hand (191,107):
//     the game uses it (the Super Drencher starts its game, overlay 18).
//   X on a slot: the bin. The game's bin throws the selected slot's item away at once (no
//     question), so the HD menu asks (ViewItem::xAsks): one X never throws. The first X arms the
//     slot (the navigator's Armed(); it taps the slot, so the game's ring is there too) and the
//     HD bin turns green; a second X on it taps the slot and the bin (67,167). Moving the focus,
//     changing tab or closing the menu disarms it.
//   Counted items and key items not owned: the focus shows their name and description; A does
//   nothing (the game's hand does nothing for them either).
#pragma once
#include <string>
#include <vector>

#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"

namespace sims2wide
{
class RomData;
class IconCache;

// The game's touch points (DS bottom pixels).
constexpr int kPkSlotX0 = 67, kPkSlotY0 = 89, kPkSlotDx = 39, kPkSlotDy = 37;  // slot i: (x0 + dx (i % 3), y0 + dy (i / 3))
constexpr int kPkKeyX = 234, kPkKeyY0 = 89, kPkKeyDy = 38;                       // key item k
constexpr int kPkCountX0 = 110, kPkCountDx = 29, kPkCountY = 170;               // counted item i
constexpr int kPkHandX = 191, kPkHandY = 107, kPkBinX = 67, kPkBinY = 167;
// The counted items' icons on the touch screen (inside their buttons' dark well #51518A).
constexpr Rect kPkCountIconFrom {99, 155, 23, 23}; // item i: x + 29i
constexpr u32 kPkCountWell = 0xFF51518A;

// The game's sprites (descriptor ids; tiles - 1, palette + 1): the hand and the bin, blue and
// green (the game's pressed / armed look), and the tool buttons (frame 0 a silhouette, 1 owned).
constexpr int kSprHand = 6667, kSprHandGreen = 6664, kSprBin = 6661, kSprBinGreen = 6658;
constexpr int kSprTool[3] = {6670, 6655, 6652};

// The game's strings: "Use" (the hand).
// The bin's word: the game has none for its bin (an icon), so its trash cans' name, "Trash" (469:
// Ordures, Müll, Rifiuti, Basura).
constexpr int kStrUse = 1845, kStrTrash = 469;

// HD geometry (canvas), inside the frame's content area (200, 206, 880 x 372).
constexpr int kPkCell = 104, kPkGap = 12;
constexpr int kPkGridX = 200, kPkGridY = 206;                         // 3 x 2 slots: 336 x 220
constexpr Rect kPkCounted {200, 446, 336, 132};                       // the counted items' row
constexpr Rect kPkKeys {552, 206, 104, 372};                          // the key items' column
constexpr Rect kPkInfo {672, 206, 408, 372};                          // name, description, the hand and the bin
constexpr Rect kPkHandBtn {kPkInfo.x + 16, kPkInfo.y + kPkInfo.h - 96, 180, 80};
constexpr Rect kPkBinBtn {kPkInfo.x + kPkInfo.w - 16 - 180, kPkInfo.y + kPkInfo.h - 96, 180, 80};
constexpr Rect kPkDesc {kPkInfo.x + 20, kPkInfo.y + 60, kPkInfo.w - 40, kPkHandBtn.y - 12 - (kPkInfo.y + 60)};
constexpr float kPkNameSize = 30, kPkDescSize = 24, kPkDescMin = 18;

// Item ids (stable across rebuilds).
constexpr u32 kPkSlotId = 0x200, kPkKeyId = 0x210, kPkCountId = 0x220;

// Pockets' items appended to v: the six slots (empty ones disabled), the three key items, the four
// counted items, in that order. rom (null: no words): the A words and the title.
void AddPocketsView(MenuView& v, const InventoryState& inv, const PocketsSel& sel, const RomData* rom);
// Its drawing (UI items, the mouse's pieces) in the frame's content area.
void BuildPockets(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons,
                  const u32* bottom);

// The name and description shown for view item `item` (a slot, key or counted item), as string
// ids; false for another item. Shared with Give Gift (port/gift.h) for its slots.
bool PocketsItemText(const InventoryState& inv, int item, int& nameId, int& descId);

// A cell of the grids (UI items, layer 1): a well in the game's dark button colour, the focus's
// green halo around it, rimmed in lavender when the game holds it selected (red: a key item not
// owned, as the game's ring); empty: flat and pale.
void AddItemCell(Composition& c, Rect r, bool focused, bool selected, bool empty, bool red = false);
// A game sprite (descriptor desc, its frame) scale times, centred in box (an image item);
// opacity: ImageItem's fill (0 opaque).
void AddSpriteIcon(Composition& c, IconCache* icons, int desc, int frame, int scale, Rect box, u32 opacity = 0);

// An item's description fitted to a box: the size shrunk from kPkDescSize to kPkDescMin until
// its lines fit the height. fits: they did at kPkDescMin.
struct FittedBlock { std::string text; float size = 0; int lines = 0; bool fits = true; };
FittedBlock FitBlock(const std::string& text, float size, float minSize, Rect box);

// (A check, --measure-menu) every name and description Pockets and Give Gift can show, in rom's
// language, in their boxes: the items' (463 + id, 822 + id), the key items', the counted items'.
struct BlockMeasure
{
    int checked = 0, shrunk = 0, cut = 0; // cut: an ellipsis needed outside the items' names
    int namesCut = 0;                    // an item's name ending in an ellipsis (allowed)
    std::vector<int> cutIds;
    int tallestId = -1, tallestLines = 0;
};
BlockMeasure MeasurePockets(const RomData& rom);
}
