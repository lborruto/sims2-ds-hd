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
//   The pointing minigames (milestone 6) have a pointer of their own, a stick cursor that works
//   headless too (ActivityState::px / py; MovePointer): either stick moves it, faster with a
//   fuller deflection (a square curve: a light touch is slow), LT held makes it slow (a quarter);
//   the D-pad nudges it one pixel a press, then after kNudgeWait frames held one pixel a frame,
//   after kNudgeFast kNudgeSpeed a frame: every pixel is reachable. A held is the stylus down at
//   the pointer; while it is down the pointer moves at most kPtrDrag a frame (the overlays tick at
//   30 Hz) and the autopsy's laser at kPtrLaser (its cutting pace).
//   Alien Autopsy: X / Y / RB take the laser / hand / tweezers (their buttons tapped); LB, while
//     the hand holds an organ or the tweezers a parasite, carries it to the tray / the flask
//     (the pointer glides there at kPtrDrag, the stylus held, and lets go); B or Start quits
//     (the game's own keys: at once, the earnings forfeited), so the first press asks
//     (ActivityState::quitArmed, the prompt lit) and a second within kQuitWindow frames quits.
//   Metal Detector: the left stick and the D-pad walk (the game's own D-pad); A digs, B powers
//     off (the game's buttons tapped: the way out); LB / RB (or the right stick, up and down)
//     hold the depth slider's knob and move it shallower / deeper (a drag along x 195).
//   Art Gallery: LB / RB the previous / next tool (the game's ten buttons in their order), X the
//     next colour of the page, Y undo, B the game's X ("Are you done painting?"); while that
//     question is up A taps its tick, B its cross (the other buttons wait).
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
    // The pointing minigames (GameState::minigame's parts; each false when its part does not read).
    bool autopsy = false;
    int autopsyTool = 0, autopsySub = 0;   // the tool (0 laser, 1 hand, 2 tweezers); 3 an organ dragged, 6 a parasite held
    bool detector = false;
    int detectorSlider = 0;                // the depth slider, 0..4096
    bool art = false;
    int artButton = 0, artSwatch = 0;      // the tool button 0..9, the swatch 0..14
    // A question of the game's popup is up over the minigame (kind 2 shown): its tick and cross.
    bool ask = false;
    Point askYes {0, 0}, askNo {0, 0};
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
    bool askSeen = false; // Cards: RAM has shown the question (its going then disarms confirm)
    int blowFrames = 0; // Vacuum: frames of blow still to give after a Y press (the game needs 7)
    // Out, this frame (Vacuum): blow into the microphone; the stylus held at the hand; how far
    // the hand moves (DS pixels) besides the right stick.
    bool blow = false, grab = false;
    float handDx = 0, handDy = 0;
    // The pointing minigames' pointer (DS touch pixels; it starts in the middle of a new screen).
    // Out: pointer (it is this screen's, drawn), down (the stylus down at px, py this frame).
    float px = SW / 2, py = SH / 2;
    bool pointer = false, down = false;
    int dpadHeld = 0;                 // frames the D-pad has been held (its nudge, then its speed)
    bool latch = false;               // A must be let go before it presses again (after a glide)
    bool gliding = false;             // the autopsy's LB: carrying to glideTo, the stylus held
    Point glideTo {0, 0};
    int glideRest = 0;                // frames resting on glideTo before letting go
    int quitArmed = 0;                // the autopsy: frames left for the second B / Start
    int quitKeys = 0;                 // the autopsy: frames B is still held for the game
    int sliderFrames = 0;             // the detector: frames the knob has been held
};

// What CONSOLE draws of a pointing minigame's pointer (App::consolePointer, from the navigator):
// shown, where (DS touch pixels), the stylus down; the autopsy's quit asked once.
struct PointerView
{
    bool shown = false, down = false, quitArmed = false;
    float x = 0, y = 0;
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

// The pointer (the pointing minigames): DS pixels a frame.
constexpr float kPtrDead = 0.2f;      // a stick's dead zone
constexpr float kPtrMax = 5.0f;       // a stick fully over (the screen's width in about 50 frames)
constexpr float kPtrSlow = 0.25f;     // LT held: the speed's share
constexpr float kPtrDrag = 3.0f;      // the stylus down: at most (drags of a few pixels a frame)
constexpr float kPtrLaser = 1.0f;     // the autopsy's laser down: at most (it cuts through in one pass)
constexpr int kNudgeWait = 10, kNudgeFast = 30;   // the D-pad: one pixel, then a pixel a frame, then faster
constexpr float kNudgeSpeed = 3.0f;
// Moves st's pointer by the sticks and (dpad) the D-pad, at most cap a frame, held on the screen.
void MovePointer(const PadState& pad, ActivityState& st, float cap, bool dpad);
// The pointing minigames.
bool IsPointing(Screen s);

// Alien Autopsy's buttons (the game's own rectangles: the tools x 0-31, the tray and flask at the
// right): laser, hand, tweezers; where an organ is dropped (the tray) and a parasite (the flask).
constexpr Point kAutopsyTool[3] = {{16, 96}, {16, 32}, {16, 160}};
constexpr Point kAutopsyTray {236, 36}, kAutopsyFlask {236, 156};
constexpr int kQuitWindow = 180;      // frames the second B / Start has to quit
// The Metal Detector's buttons (power 54-101 and dig 156-203, rows 136-167) and its depth slider
// (the knob along x 195, rows 33-121 for 0..4096).
constexpr Point kDetectorPower {78, 152}, kDetectorDig {180, 152};
constexpr int kDetectorKnobX = 195, kDetectorKnobTop = 33, kDetectorKnobSpan = 88;
constexpr float kKnobStep = 1.5f, kKnobStick = 3.0f;
// The Art Gallery's buttons (24 x 24, tapped in their middle): the ten tools (pencil sizes 0-2,
// eraser sizes 0-2 at the left; bucket, line, rectangle, filled rectangle at the right), the
// game's X and undo, the page arrows, the fifteen swatches (7 above, 8 below).
Point ArtTool(int button);
Point ArtSwatch(int swatch);
constexpr Point kArtDone {241, 14}, kArtUndo {241, 40}, kArtPagePrev {17, 182}, kArtPageNext {241, 182};
constexpr int kArtTools = 10, kArtSwatches = 15;
}
