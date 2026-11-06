// CONSOLE's activities on buttons: the navigator (navigator.h) hands the social reaction, the
// card game and the vacuum to ActivityInput, which turns the player's buttons into the touches
// the game wants (research: docs/research/sims2ds-mechanics.md, 4.1, 4.5, 4.13):
//   Social: LEFT / UP / RIGHT tap the left / middle / right answer (the focus follows); A taps
//     the focused one.
//   Cards: the D-pad moves the focus over the table (the navigator's, starting on the first
//     card); A on a card lifts it (a tap), A again on a card it lifted drags that card onto the
//     play arrow; A on the deck, the discard pile, ? or X taps them; X taps the deck (draw); Y
//     drags the focused card onto the discard pile; B taps X (quit: the game asks first). While
//     the question is up (it still reads Cards; or RAM says so: the mouse asked) A taps its tick,
//     B its cross. While RAM shows the end-of-turn message, A taps it (the turn goes on).
//   Vacuum: the left stick (and the D-pad) are the DS's D-pad (the game's own steering); the
//     hand is the stick cursor (input.h): the right stick moves it, and so does the D-pad while
//     X or A is held (the keyboard's way, no stick needed); A held keeps the stylus down at the
//     hand (grab a find, drag it into the box); Y blows into the microphone (7 frames or more
//     clear a clog).
// The other DS buttons stay the DS's where the game may use them (Start, Select; the vacuum's
// B, L, R). Presses are edges against ActivityState::prev (the navigator's previous PadState);
// a tap or drag is queued only when the touch script does not own the stylus.
#pragma once
#include <vector>

#include "common.h"
#include "padstate.h"
#include "screens.h"
#include "touchscript.h"

namespace sims2wide
{
// One line of an activity's hints: the button (as printed on a controller) and what it does,
// in capitals (the 5 x 7 font), its lines split by hand with '\n' (14 characters fit beside a
// button, 20 in a note: console.h).
struct Hint
{
    const char* button;
    const char* text;
};

// What the game's RAM says about the activity this frame (GameState::minigame), when it reads
// (cards: false, nothing known: milestone 1's way).
struct ActivityRam
{
    bool cards = false;               // the cards' part read
    bool quitAsk = false;             // their quit question is up (opened by B or by the mouse)
    bool message = false;             // their end-of-turn message is up (any touch goes on)
};

struct ActivityState
{
    // In: the RAM's word on the activity (Navigator::SetActivityRam, every frame).
    ActivityRam ram;
    // In: the previous frame's buttons (the navigator's, so its resync holds here too).
    PadState prev;
    // In and out: the focused target, an index in HotspotsFor (Social, Cards), or -1.
    int focus = -1;
    // Kept between frames (Cards): the hand's places lifted by a tap, and the quit question up.
    bool lifted[10] = {};
    bool confirm = false;
    int blowFrames = 0; // Vacuum: frames of blow still to give after a Y press (the game needs 7)
    // Out, this frame (Vacuum): blow into the microphone; the stylus held at the hand; how far
    // the hand moves (DS pixels) besides the right stick.
    bool blow = false, grab = false;
    float handDx = 0, handDy = 0;
};

// The screens ActivityInput plays.
bool IsActivity(Screen s);
// Once a frame on an activity screen: DS keys OR-ed into dsKeys, touches queued on touch.
void ActivityInput(Screen s, const PadState& pad, TouchScript& touch, u32& dsKeys, ActivityState& st);
// What the hints' box shows for screen s (empty: no box).
std::vector<Hint> ActivityHints(Screen s);

// Where the cards' quit question has its answers (it still reads Cards; checked headless from
// act-cards: the tick quits through a black load, the cross closes it).
constexpr Point kCardsQuitYes {93, 92}, kCardsQuitNo {163, 92};
// The cards' end-of-turn message ("Touch for @1's turn."): any touch goes on; its box's middle.
constexpr Point kCardsMessage {128, 120};
// The vacuum's hand speed on the D-pad, DS pixels a frame.
constexpr float kHandStep = 2.0f;
}
