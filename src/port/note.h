// A note or letter (overlay 11, CNoteMenu: the mission scripts' letters, the hotel's welcome)
// in HD: a parchment panel over the dimmed world with the note's text (its string id S+0x2C,
// GameState::town.note; @1 the player's name for the three letters that carry it, as the game)
// centred as the game centres it, and the game's blue X (sprite 7683, 2x). A and B close it:
// the game closes a note on its own A (research m3 §4.2), so both press the DS's A. The mouse:
// the X maps onto the game's close button (234,171, its CActorStylusPushbutton's place: from
// the code only). The game's parchment backgrounds (4161 / 4162) are BG images the port does
// not decode: the panel is drawn in the parchment's colours instead.
//
// No save state reaches a note (the research found it in the code only): the view is checked on
// made-up states (layout_test, navigator_test) and its words by --measure-menu.
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

constexpr int kNoteCloseX = 234, kNoteCloseY = 171;
constexpr int kSprNoteX = 7683;
constexpr Rect kNotePanel {250, 40, 780, 600};
constexpr Rect kNoteText {kNotePanel.x + 70, kNotePanel.y + 60, kNotePanel.w - 140, kNotePanel.h - 160};
constexpr Rect kNoteX {kNotePanel.x + kNotePanel.w - 88, kNotePanel.y + kNotePanel.h - 88, 64, 64};
constexpr u32 kNoteCloseId = kModNote + 0x900;

// Whether the game shows a note (its object read: GameState::town.note).
bool NoteUp(const GameState& game);
// The note's text as shown (the game's string, @1 the player's name where the game fills it).
std::string NoteText(const GameState& game, const RomData& rom);
MenuView NoteView(ScreenState s, const GameState* game, const RomData* rom);
MenuView NoteViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
void BuildNote(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons);
// (A check, --measure-menu) the notes the code names (0x4A8, 0x509, 0x657) in the panel.
void MeasureNotes(const RomData& rom, BlockMeasure& m);
}
