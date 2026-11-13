// HD conversations: the game's dialogue box redrawn with the UI kit over the whole world. The
// page's text and the speaker's name are the game's own strings (RomData, in the game's
// language) picked by what RAM says is shown (GameState::dialogue: the page's string id, the
// speaker); the portrait is a live crop of the DS's touch screen (the game draws it there); an
// "A" chip says how to go on (the game has no word for "next" in its table: the chip alone).
// The DS's A (the navigator passes it through) and a click on the panel (an invisible piece
// mapped onto the game's arrow) both turn the page, as the game's arrow does.
#pragma once
#include <string>
#include <vector>

#include "compose.h"
#include "gamestate.h"

namespace sims2hd
{
class RomData;

// The panel along the canvas's bottom (the mockup's dialogue frame: 1060 wide, centred) and its
// insides, canvas pixels.
constexpr int kDlgPanelX = 110, kDlgPanelW = 1060, kDlgBottomGap = 14;
constexpr int kDlgPad = 22;              // the panel's inner margin (left, right, bottom)
constexpr int kDlgTopPad = 40;           // above the wells: room for the name plate's lower half
constexpr int kDlgTextPad = 20;          // the text inside its well
constexpr int kDlgRows = 4;              // the text's rows before the panel grows upwards
constexpr int kDlgMaxRows = 9;           // at most (a longer page is clipped: never seen in the game's five languages)
constexpr float kDlgTextSize = 30, kDlgTextMin = 28; // the text, pixels per em (TV-readable: 28 at least at 720p)
constexpr float kDlgNameSize = 28, kDlgChipSize = 28;
// The game's am / pm strings (for @am in a page).
constexpr int kDlgAmId = 178, kDlgPmId = 179;
// The portrait on the DS's touch screen: inside its frame's outline (x 170-243, rows 34-123 on
// s5, sit-dialogue and sit-concierge) less the frame's bevel and its rounded corners' pixels
// (x 174-240, rows 38-121); shown 2x in a well at the right.
constexpr Rect kDlgPortraitFrom {174, 38, 67, 84};
constexpr int kDlgPortraitScale = 2, kDlgPortraitPad = 8;

// A page's text as shown: @1 the Sim's name, @hh / @mm / @am the HUD's clock (when it reads),
// any other @ placeholder dropped (a lone '@' too), RomData's {xx} / {F0xx} escapes dropped,
// the spaces a drop leaves folded (and none left before . , ! ? ; :), the game's own line
// breaks kept, the ends trimmed. Never shows a placeholder or an escape.
std::string DialogueText(const std::string& raw, const HudState& hud, const std::string& am = "am", const std::string& pm = "pm");

// The page laid out in the panel: the text's size and lines, the rows the panel holds (kDlgRows,
// or more for a long page), the text box's width, the panel's rectangle. fits: every line within
// the text box's width and the rows within kDlgMaxRows.
struct DialogueLayout
{
    float size = kDlgTextSize;
    std::vector<std::string> lines;
    int rows = kDlgRows;
    int textW = 0, lineH = 0;
    Rect panel {};
    bool portrait = true;
    bool fits = true;
};
// rom: the language of the A chip's word ("Continue", the port's own; null: English).
DialogueLayout LayoutDialogue(const std::string& text, bool portrait, const RomData* rom = nullptr);
// The A chip's word in the text well's corner (the port's own "Continue").
std::string DialogueChipWord(const RomData* rom);

// Whether the touch screen shows a portrait in its frame (the game draws none for some pages:
// the rectangle is then the frame's flat colours, or black while the box fades in).
bool PortraitShown(const u32* bottom);

// The HD conversation (pieces, UI) appended to c: the world whole (the top picture without its
// status bar rows, never cut at its sides), the panel, the name plate (the speaker's name; the
// player: the Sim's name; no speaker: no plate), the text, the portrait framed, the A chip, and
// an invisible touchable piece over the panel mapped onto the game's arrow (a click turns the
// page). game.dialogue not valid (no page drawn yet: a speech starting, the frames between two):
// the panel alone. CONSOLE (console.cpp) calls it only with RAM read and rom's strings, and keeps
// milestone 1's composition otherwise. topW x topH: the top picture's size; bottom: the DS's touch screen.
void BuildDialogue(Composition& c, const GameState& game, const RomData& rom, const u32* bottom, int topW, int topH);
}
