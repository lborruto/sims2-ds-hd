// The panel's Options tab and the Save slots (overlay 8) in HD.
//
// Options (port/panelmenu.h's frame): Save and Quit as two buttons (the game's words 64 / 61),
// then the music and SFX volumes (62 / 63) as sliders whose value is read back from RAM
// (GameState::town.options, 0..4096). What performs, by the game's own input: A on Save taps the
// game's Save (57,44: the slot screen opens), A on Quit its Quit (234,69: the game's question,
// a popup); Left / Right on a slider drag the game's knob one notch (the game keeps a volume in
// tenths: its knob at x 72 + 16 k on row 121 / 171 moves only by a drag starting on it, and lands
// on the nearest tenth when let go). The button by the title (193,17) does nothing the game shows:
// not offered.
//
// Save slots (SaveSlots; no panel menu to open: CONSOLE shows it whenever the game does): a panel
// like the menu's without the tab bar, the title "Save Game" (12), the three slots from RAM
// (GameState::town.saves: the Sim's name, the place 0x43 + place, the money; "No Data" 11 when
// empty; the game's current slot's number in green, as the game draws it). A on a slot taps its
// tick (232, 67 / 113 / 159): the game saves into that slot at once, as its tick does. B taps the
// game's X (232,22). The time a slot was saved is not shown (its encoding was not decoded).
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2wide
{
class RomData;

// The game's touch points (DS bottom pixels).
constexpr int kOptSaveX = 57, kOptSaveY = 44, kOptQuitX = 234, kOptQuitY = 69;
constexpr int kOptKnobX0 = 72, kOptKnobStep = 16, kOptMusicY = 121, kOptSfxY = 171; // knob at x0 + step k, k = 0..10
constexpr int kSlotTickX = 232, kSlotTickY[3] = {67, 113, 159}, kSlotCloseX = 232, kSlotCloseY = 22;

// The game's strings.
constexpr int kStrQuit = 61, kStrMusic = 62, kStrSfx = 63, kStrSave = 64;
constexpr int kStrNoData = 11, kStrSaveGame = 12, kStrMoney = 28, kStrPlaceFirst = 0x43;

// HD geometry (canvas): Options inside the frame's content area (200, 206, 880 x 372).
constexpr Rect kOptSaveBtn {200, 206, 432, 80}, kOptQuitBtn {648, 206, 432, 80};
constexpr Rect kOptMusic {200, 302, 880, 130}, kOptSfx {200, 448, 880, 130};
constexpr int kOptTrackX = 268, kOptTrackW = 744;   // a slider's track (0 % .. 100 %) inside its well
// The Save slots' panel.
constexpr Rect kSlotTitle {200, 44, 880, 64};
constexpr int kSlotY0 = 124, kSlotH = 142, kSlotGap = 14;
constexpr u32 kOptSaveId = 0x600, kOptQuitId = 0x601, kOptMusicId = 0x602, kOptSfxId = 0x603, kSlotId = 0x610;

// A volume (0..4096) as the game's tenth (0..10).
int VolumeNotch(int volume);

// Options' items appended to v. rom (null: no words): the title and the A words.
void AddOptionsView(MenuView& v, const OptionsState& o, const RomData* rom);
// Its drawing (UI items, the mouse's pieces) in the frame's content area.
void BuildOptions(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom);

// The Save slots' view (valid when RAM's slots read and the game's strings are there).
MenuView SaveSlotsView(ScreenState s, const GameState* game, const RomData* rom);
// The same without the strings' check (tests; rom null leaves the words empty).
MenuView SaveSlotsViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The HD screen appended to c (after the dimmed world): the panel, title, slots, hints, the focus
// on view item `focus`, and the mouse's pieces.
void BuildSaveSlots(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom);

// (A check, --measure-menu) the words of Options and the Save slots in their boxes.
struct BlockMeasure;
void MeasureOptions(const RomData& rom, BlockMeasure& m);
}
