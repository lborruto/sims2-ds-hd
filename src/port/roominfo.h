// A guest room's info (overlay 12, CRoomInfoMenu: a Registry row's) in HD. A game-style panel over
// the dimmed world (port/panelmenu.h's frame, no tab bar), from GameState::roomInfo (RAM, every
// frame): the room's name (0x43 + place) in the title band; the portrait the game shows (the
// occupant's, the concierge's when the room is vacant: a live crop of the game's own, 3x); and the
// game's lines as a receipt, each label at the left and its value at the right: who stays
// ("Guest : @1" 2458) or "Vacant" (2462), the guest's mood ("Guest Mood : @2" 2459, 2086 + mood),
// the check-out date ("Check Out Date : @1/@2 (@3 days)" 2460, or the Penthouse's "?/? (? days)"
// 2461), then the daily cost: the room's base value (2463), the furniture's service charge (2464),
// the hotel score's bonus (2465) and the total (2467), in simoleons ("$@1" 28).
//
// What performs, always the game's own taps (the research's touch points, m4 §3): A on a line taps
// it (x 40, the line's row): the game opens its hint box (2594..2603), drawn in HD in the popups'
// look (port/popup.h, BuildHintBox) with the line's words as its title; while it is up A and B
// close it (a tap on the game's screen away from the lines, which closes it) and the focus comes
// back to the line (the navigator's resume). B: the game's X (234,170), back to the Registry.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

// The game's touch points (DS bottom pixels): a line's tap (x, its row's middle), the X, and a
// point that is no line (a tap there closes a hint box and does nothing else).
constexpr int kRiLineX = 40;
constexpr int kRiCloseX = 234, kRiCloseY = 170;
constexpr int kRiAwayX = 200, kRiAwayY = 100;
// The game's lines (in its order): the name, the guest (or Vacant), the mood, the check-out date,
// the base value, the service charge, the score's bonus, the total; each line's row (its top: the
// tap is 6 below) and the hint it opens.
constexpr int kRiLines = 8;
constexpr int kRiLineY[kRiLines] = {13, 37, 57, 77, 97, 117, 137, 157};
constexpr int kRiHint[kRiLines] = {2594, 2595, 2601, 2603, 2597, 2598, 2599, 2600};
constexpr int kRiHintVacant = 2596;   // line 1 on a vacant room
// The game's portrait on the touch screen (the live crop).
constexpr Rect kRiPortraitFrom {172, 36, 70, 86};

// The game's strings.
constexpr int kStrRiGuest = 2458, kStrRiMood = 2459, kStrRiOut = 2460, kStrRiOutUnknown = 2461, kStrRiVacant = 2462;
constexpr int kStrRiBase = 2463, kStrRiService = 2464, kStrRiBonus = 2465, kStrRiTotal = 2467;
constexpr int kStrRiHints = 2468;     // "Hotel Hints": A's word on a line
constexpr int kStrRiMoney = 28, kStrRiPlaceBase = 0x43;
constexpr int kSprRiX = 7683;

// HD geometry (canvas), the frame as the panel's (kPmPanel), no tab bar.
constexpr Rect kRiTitle {200, 44, 880, 64};
constexpr Rect kRiClose {1016, 48, 56, 56};
constexpr Rect kRiPortrait {200, 124, 256, 300};
constexpr Rect kRiLinesWell {472, 124, 608, 454};
// Line i's HD rectangle (line 0, the name: the title band's text).
Rect RoomInfoLineRect(int i);
constexpr float kRiLabelSize = 26, kRiValueSize = 28, kRiMin = 18;

// Item ids: the kind in bits 20-23 (1 a line, 2 the hint box's Close), the line below.
constexpr u32 kRiLineId = kModRoomInfo | (1u << 20), kRiHintCloseId = kModRoomInfo | (2u << 20);

// Which hint the game opens for a stylus-down at DS (x, y) on the room info (its own rectangles,
// research m4 §3), -1 none: what the frame loop fills RoomInfoState::hintId with.
int RoomInfoHintAt(int x, int y, bool occupied);

// The view (valid when RAM's room info reads and the game's strings are there). No hint up: one
// item per line the game shows (the vacant room: name, Vacant and the four costs), A its tap; B
// the X. A hint up (its string known): a popup of the screen's own, one Close item, A and B a tap
// away from the lines; its string unknown (the box was up before the port saw a touch): no view
// (milestone 1's picture).
MenuView RoomInfoView(ScreenState s, const GameState* game, const RomData* rom);
MenuView RoomInfoViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// Whether the hint box is up (and its view is the box's).
bool RoomHintUp(const GameState& game);
// The HD screen appended to c: the panel, the name, the portrait, the lines, the hints, the mouse's
// pieces (a hint up: the lines without a focus). `bottom`: the touch screen (the portrait's crop).
void BuildRoomInfo(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom);
// The hint box over it (port/popup.h's look), when RoomHintUp: drawn after the status bar.
void BuildRoomHint(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom);

// (A check, --measure-menu) every word room info can show in rom's language: the five rooms'
// names, the labels beside their widest values, every guest's name, the moods, the hints in the box.
void MeasureRoomInfo(const RomData& rom, BlockMeasure& m);
}
