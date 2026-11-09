// The hotel's music panel (overlay 5, CHotelMusicMenu: a Music Panel's "Examine") in HD: a
// jukebox. A game-style panel over the dimmed world (port/panelmenu.h's frame, no tab bar), from
// GameState::music (RAM, every frame):
//   the title band: "Music Panel" (2439) and the game's X;
//   the game's arrows (▲ 5735, ▼ 5732, 2x, dim where the game cannot scroll) and a scroll bar;
//   the songs the game lists (its list, gated by the game's flags: S+0x2C[S+0x6C]) from its own
//     scroll (S+0x28), five rows as the game shows: the game's play button (5754: frame 0 blue,
//     frame 4 green for the song playing, S+0x70) and the song's name (its string; a recorded
//     song's name from G);
//   beside it, the song playing: the green button 3x, its name, and the room it plays in
//     (0x43 + place).
//
// What performs, always the game's own taps (the research's touch points, m4 §4; the game reads
// no DS key here):
//   A on a row: its play button (62, 41 + 30 i): the game plays the song and keeps it for the room
//     (u8 G+0x449C + place). The song already playing: no action (the game does nothing there).
//   Up / Down past the rows: the game's arrows (28,73) / (28,126) while it can scroll.
//   B: the game's X (224,26), back to the panel.
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

// The game's touch points (DS bottom pixels): row i's play button, the arrows, the X.
constexpr int kMuRowX = 62, kMuRowY0 = 41, kMuRowDy = 30, kMuRows = 5;
constexpr int kMuUpX = 28, kMuUpY = 73, kMuDownX = 28, kMuDownY = 126;
constexpr int kMuCloseX = 224, kMuCloseY = 26;

// The game's sprites (descriptor ids): the play button (frame 0 blue, 4 green: playing), the
// arrows, the X.
constexpr int kSprMuPlay = 5754, kSprMuPlayingFrame = 4, kSprMuUp = 5735, kSprMuDown = 5732, kSprMuX = 7683;
// The game's strings.
constexpr int kStrMuTitle = 2439, kStrMuPlay = 1879 /* "Turn On": the game has no "Play" */, kStrMuPlaceBase = 0x43;
// Every song name the game's table holds (custom songs aside): what --measure-menu measures.
constexpr int kMuSongNames[] = {101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 127,
                                1770, 1771, 1772, 1773, 1774, 1775, 1776, 1777, 102, 88};

// HD geometry (canvas), the frame as the panel's (kPmPanel), no tab bar.
constexpr Rect kMuTitle {200, 44, 880, 64};
constexpr Rect kMuClose {1016, 48, 56, 56};
constexpr Rect kMuArrows {200, 124, 72, 454};
constexpr Rect kMuList {288, 124, 512, 454};
constexpr Rect kMuInfo {816, 124, 264, 454};
constexpr Rect kMuUp {kMuArrows.x + 8, kMuArrows.y + 8, 56, 56};
constexpr Rect kMuDown {kMuArrows.x + 8, kMuArrows.y + kMuArrows.h - 64, 56, 56};
Rect MusicRowRect(int i);
constexpr float kMuNameSize = 30, kMuNameMin = 20;

// Item ids: the kind in bits 20-23 (1 a song's row), the song table's index below.
constexpr u32 kMuRowId = kModMusic | (1u << 20);

// The song's name as the game shows it (its string, or a recorded song's own name).
std::string SongName(const RomData& rom, const SongRow& s);

// The view (valid when RAM's music panel reads and the game's strings are there): one item per
// row the game shows, A its play button (none on the song playing); Up / Down past them the
// game's arrows; B the X.
MenuView MusicView(ScreenState s, const GameState* game, const RomData* rom);
MenuView MusicViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c: the panel, the title, the arrows, the rows, the song playing, the
// hints, the mouse's pieces.
void BuildMusic(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons);

// (A check, --measure-menu) every word the music panel can show in rom's language: the title,
// every song's name in a row and in the info well, every room's name there, the hints.
void MeasureMusic(const RomData& rom, BlockMeasure& m);
}
