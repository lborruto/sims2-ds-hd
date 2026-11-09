// The world's prompt: what the DS's A does where the hero stands, like a console game ("A Buy",
// the target's name). The game keeps a current target, picked by proximity, in the Interact tab's
// object (GameState::town.interact: E = *(T+0x300)); a tap of A in the world is its pad mode, and
// releasing A runs the highlighted row, the first enabled action outside pad mode (label 1845 +
// code). Held, Up / Down pick another row; Left / Right (held or not) switch to the strip's
// previous / next target. Everything from RAM and the game's strings; the hints in the port's
// own words (port/ownwords.h).
//
// Shown in town only (CONSOLE's town branch: the panel's world, no HD menu, no popup, no
// conversation, minigame or shell), while the panel's tab is Interact (the world's default: on
// another tab the game has no Interact object, so no target to read), with a target holding at
// least one enabled action, steady for kPromptSteadyFrames frames (walking past things does not
// flash it), and not right after an action (pad mode 2, until A is released).
#pragma once
#include <string>
#include <vector>

#include "compose.h"
#include "gamestate.h"

namespace sims2wide
{
class RomData;
struct BlockMeasure;

// Frames the target and its actions must stay the same before the prompt shows.
constexpr int kPromptSteadyFrames = 6;
// Its still layer: alone (above the status bar's 2, the phone's clock 3, a popup's 4-5, the
// minigames' pointer 6; below the window's marks 8 and toasts 9), so a change rasters only it.
constexpr int kPromptLayer = 7;
// The plate: centred at the bottom of the world, its bottom at kPromptBottom (its drop shadow,
// 7 rows under it, ends above the strip, row 661: port/statusbar.h's kStripY), kPromptH high, at
// most kPromptMaxW wide. The UI's size zooms it about its bottom centre (console.cpp).
constexpr int kPromptShadow = 7;
constexpr int kPromptBottom = 640, kPromptH = 88, kPromptMaxW = 1000;
// The texts: the verb beside the A chip, the name above it, the hints at the right.
constexpr float kPromptVerbSize = 30, kPromptVerbMin = 22, kPromptNameSize = 22, kPromptNameMin = 19, kPromptHintSize = 20;
constexpr int kPromptVerbW = 440, kPromptNameW = 520;

// How long the Interact target and its actions have been the same (App::consolePrompt, updated
// by Input::Steer every frame CONSOLE runs; a pad mode change or a new highlight does not reset it;
// no target resets it). Steer runs before the frame's RAM is read: the target counted may be the
// frame before's, so the prompt shows only when it is also the one drawn (PromptUp).
struct PromptSteady
{
    TargetRef target;
    std::vector<InteractAction> actions;
    bool had = false;
    int frames = 0;
    // in: the panel's Interact as read this frame (null or not valid: none). Returns the frames.
    int Update(const InteractState* in);
};

// What the prompt says (the words empty when rom has no strings).
struct WorldPrompt
{
    bool shown = false;
    std::string verb;          // the action A runs (pad mode: the highlighted row's)
    std::string name;          // the target's name
    bool held = false;         // A held (pad mode): Up / Down choose, release runs
    bool more = false;         // other enabled actions (hint: hold A for more / choose)
    bool others = false;       // a previous or next target (hint, while A is held: Left / Right)
};
// The prompt for this Interact state (shown: a target, an enabled action to run, not pad mode 2).
WorldPrompt WorldPromptOf(const InteractState& in, const RomData* rom);
// Whether the world shows it: the game's panel up on Interact, settled, no popup, and steady.
// steady: its target and actions the ones drawn, for kPromptSteadyFrames or more.
bool PromptUp(ScreenState s, const GameState& game, const PromptSteady& steady);
// The plate (UI items on kPromptLayer appended to c); nothing when !p.shown.
void BuildWorldPrompt(Composition& c, const WorldPrompt& p, const RomData* rom);
// CONSOLE's town: PromptUp, then WorldPromptOf and BuildWorldPrompt.
void AddWorldPrompt(Composition& c, ScreenState s, const GameState& game, const RomData& rom, const PromptSteady& steady);
// A composition without the prompt (a held picture, the game under the shell).
void DropWorldPrompt(Composition& c);
// The plate's rectangle for p as BuildWorldPrompt draws it (w 0: none).
Rect WorldPromptRect(const WorldPrompt& p, const RomData* rom);

// (A check, --measure-menu) every verb 1845-1951 (an amount at the widest the money reaches)
// beside the A chip, every Sim's and object's name above it, the hints, in rom's language.
void MeasurePrompt(const RomData& rom, BlockMeasure& m);
}
