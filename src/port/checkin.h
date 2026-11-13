// The hotel's Registry and a guest's check-in (overlay 4, CCheckInMenu: the concierge's
// "Registry", a waiting guest's "Check In") in HD: a hotel ledger. A game-style panel over the
// dimmed world (port/panelmenu.h's frame, no tab bar), from GameState::checkIn (RAM, every frame):
//   the title band: the Registry's title (2577) and the hotel score ("Hotel Score %: @1" 2006)
//     on a plate; a guest's check-in: "Check In" (1890) and the guest's name (323 + k) in the
//     game's "@1: @2" (1840);
//   the ledger: the game's five guest rooms (its table: Small, Deluxe, Jungle, Modern, Penthouse),
//     each with the game's row picture (its key, 8451 frame 0, when the room is vacant; frame 1,
//     its silhouette, when it is not built; the occupant's portrait, a live crop of the game's
//     own row button, when someone stays there), the room's name (0x43 + place) white when the
//     room is built and vacant, the game's pink otherwise, and under it the occupant's name or
//     "Vacant" (2462); a room not built is flat;
//   beside it, the focused room: its picture 3x, its name, who stays there (2458) or "Vacant".
//
// What performs, always the game's own taps (the touch points measured in the game):
//   A on a room: its row button (20, 39 + 33 i). The Registry opens the room's info (port/roominfo.h)
//     for a built room; a guest's check-in checks the guest into a built, vacant room (the game
//     then talks: the guest's speech). A room the game refuses stays focusable (its line is read)
//     but A does nothing. The check-in is never replayed (MenuView::replay false).
//   B: the game's X (229,170), back to the panel.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2hd
{
class RomData;
class IconCache;
struct BlockMeasure;

// The game's touch points (DS bottom pixels): row i's button, the X.
constexpr int kCiRowX = 20, kCiRowY0 = 39, kCiRowDy = 33, kCiRows = 5;
constexpr int kCiCloseX = 229, kCiCloseY = 170;
// The game's row picture on the touch screen (row i: y + dy i): the live crop of a portrait.
constexpr Rect kCiPortraitFrom {8, 24, 28, 30};

// The game's sprites (descriptor ids): the key (frame 0; frame 1 its silhouette), the X.
constexpr int kSprCiKey = 8451, kSprCiX = 7683;
// The game's strings.
constexpr int kStrCiTitle = 2577, kStrCiScore = 2006, kStrCiCheckIn = 1890, kStrCiExamine = 1873;
constexpr int kStrCiGuest = 2458, kStrCiVacant = 2462, kStrCiPlaceBase = 0x43;
// The game's colours for a room's name: white (built and vacant), pink (occupied or not built).
constexpr u32 kCiNameOk = 0xFFFFFFFF, kCiNamePink = 0xFFF35961;

// HD geometry (canvas), the frame as the panel's (kPmPanel), no tab bar.
constexpr Rect kCiTitle {200, 44, 880, 64};
constexpr Rect kCiClose {1016, 48, 56, 56};
constexpr Rect kCiList {200, 124, 600, 454};
constexpr Rect kCiInfo {816, 124, 264, 454};
Rect CheckInRowRect(int i);
constexpr float kCiNameSize = 30, kCiNameMin = 20, kCiSubSize = 22, kCiSubMin = 18;

// Item ids: the kind in bits 20-23 (1 a room's row), the room's place below.
constexpr u32 kCiRowId = kModCheckIn | (1u << 20);

// The view (valid when RAM's check-in screen reads and the game's strings are there): one item per
// room, A its row button when the game takes it (the Registry: built; a check-in: built and
// vacant); B the X. A check-in is never replayed.
MenuView CheckInView(ScreenState s, const GameState* game, const RomData* rom);
MenuView CheckInViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c: the panel, title and score, the ledger, the focused room, the
// hints, the mouse's pieces. `bottom`: the touch screen (the portraits' live crops; null none).
void BuildCheckIn(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom);

// (A check, --measure-menu) every word the Registry can show in rom's language: the titles beside
// the widest score, every guest's name in the check-in's title, the rooms' names, the lines.
void MeasureCheckIn(const RomData& rom, BlockMeasure& m);
}
