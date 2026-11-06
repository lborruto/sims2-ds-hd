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
//     clear a clog); B taps the bowl's X (kVacuumX: the game's only way out, the vacuum put away).
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
//     next colour of the page, MENU (the TOUCH MENU's button: Back / View) the next palette page
//     (the game's ▶, kArtPageNext: 16 pages in a ring, so all 240 colours), Y undo, B or Start the
//     game's X ("Are you done painting?": its Yes is the game's way back to the panel, on a gallery
//     entered by an easel); while that question is up A taps its tick, B its cross (the other
//     buttons wait).
//   The timing minigames (milestone 6; tuned in game: console_check.sh's mg-moogoo-*, mg-drencher-*,
//   mg-crime-*):
//   MooGoo Monkey: its intro's play arrow on A. The game reads A (the players prompt, its banners),
//     Left / Right (the players' count) and B (a bet undone, a special card put back) itself: they
//     are passed on there. On the player's turn the focus (a ring on the playfield) picks what the
//     game wants touched: a monkey to bet on (sub-state 1: those alive with a free slot), a card of
//     the hand (3), a monkey's pile for a special card (5); Left / Right move it, A taps it. Start,
//     and B where the game has no use for it, tap the game's X: its "Are you sure you want to quit?"
//     (A its Yes, B its No). From the intro (the game has no way out of it) B taps the arrow, then
//     the game's X once it is up: the question again.
//   The Super Drencher (a tool screen over the world): the left stick and the D-pad walk (the
//     game's D-pad), A held fires (the game's A), X held pumps (the stylus held on the pump's band,
//     row kPumpY, swept from side to side at kPumpSpeed a frame between kPumpLeft and kPumpRight:
//     the game counts a stroke's every tick past 5 pixels), Y the target button, RB refill, LB the
//     game's run (its B), B the game's X (the way out).
//   The Ratticator's crime game: the left stick and the D-pad walk, LB runs (the game's B); X
//     scares, Y subdues (their buttons tapped); A held grabs the Rat-A-Rang (the stylus on the
//     centre rat, kCrimeRat), a stick or the D-pad aims (the rat slides along its row, kCrimeAimMin
//     to kCrimeAimMax: 1.125 degrees a pixel, at most kCrimeAimSpeed a frame), letting A go throws;
//     B the game's X (the way out). Start taps its "?" (help); in help mode X / A / Y tap scare /
//     the rat / subdue (the game explains each), B or Start the "?" again; a help text up: any
//     press taps the "?" (it closes).
//   The Scratch lottery (milestone 6): the pointer (the pointing games'), A held scratches (the
//     stylus down at the pointer, capped at kPtrDrag a frame); X rubs the box under the pointer
//     (or the nearest one still covered) for you: the pointer goes there, presses inside it and
//     sweeps it in passes kRubStep apart (ScratchRub) until the game reveals it, A or B stopping
//     it. While a popup of the game's is up (the hint, the win, the loss) A, B and Start are the
//     game's own (they close it; the win's pays). B or Start taps the ticket's X, which the game
//     takes at once and silently (the ticket lost): the first asks (quitArmed), a second within
//     kQuitWindow frames quits.
//   The music mixer: its own DS keys as the game reads them, held: the D-pad (Up / Down the octave
//     up / down, Right sustain, Left PREP). The pointer and A held touch the keys, buttons and
//     sliders; X taps PLAY (stop while it plays), RB RECORD, LB GENRE, B QUIT (stop while it plays
//     or records); Y held records the microphone (VOX: tapped on the press, again on the release
//     while the game still records; meanwhile `voice`: the blow's signal, or the computer's own
//     microphone when the settings say so). The song's list (phase 7): Up / Down a focus over its
//     rows (`mark`), A picks, B cancels (GENRE again). The save question (0xA): A Yes, B No, X
//     Preview. Start taps ? (help, from the idle mixer); help (8: the pointer and A pick the
//     control the game explains; 9: its text, A closes it): B taps ? again.
//   The mixer's save page: the D-pad moves the focus over its targets (HotspotsFor: the keyboard,
//     backspace, the pages, the three slots, the X), A taps the focused one (a slot already used:
//     the first A asks, armedSlot, a second saves over it), Y backspace, B the X (back to the
//     mixer).
//   The wireless lobby (Trading): out of scope; B taps its X (back to the title), the rest waits.
// The DS's own Start and Select are passed on only where the game may read them and no rule
// above takes them: Select everywhere but the wireless lobby; Start on the social game, the
// cards, the vacuum and the detector (the autopsy, MooGoo, the lottery, the mixer, its save page,
// the painting, the Drencher, the crime game and the lobby give it a rule of their own). The
// vacuum's B is its bowl's X now, never the DS's. Presses are edges against ActivityState::prev
// (the navigator's previous PadState); a tap or drag is queued only when the touch script does
// not own the stylus.
#pragma once
#include <vector>

#include "common.h"
#include "layout.h"
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
    // The timing minigames.
    bool mooGoo = false, mooGooIntro = false;
    int mooGooPhase = 0, mooGooSub = 0;   // GameState's MooGooState::phase / sub
    bool mooGooHuman = false;              // the current player is a human (not the game's AI)
    bool mooGooQuitAsk = false;            // the game's quit question is up
    bool mooGooBet[6] = {};                // a monkey takes a bet (alive, a free slot)
    bool mooGooAlive[6] = {};              // a monkey is not out
    bool mooGooPile[6] = {};               // a card lies under the monkey
    bool mooGooCard[5] = {};               // the current player's hand holds a card there
    int mooGooSelected = -1;               // the hand card the game has selected
    bool drencher = false;
    int drencherPump = 0;                  // the pump handle, 0..180
    bool drencherHelp = false, drencherHelpText = false;   // its help mode; a help text is up (a tap closes it)
    bool crime = false;
    int crimePhase = 0;                    // the throw: 0 idle, 1 grabbed, 2 aiming, 3 released
    bool crimeHelp = false, crimeHelpText = false;   // its help mode; a help text is up (the next press closes it)
    // The Scratch lottery: the ticket's tier (its boxes' places), a popup of the game's up over it
    // (the hint, the result), which boxes are revealed (or wiping).
    bool scratch = false, scratchPopup = false;
    int scratchTier = 0;
    bool scratchOpen[8] = {};
    // The music mixer: its phase (MixerState::phase), the songs its list shows; the save page's
    // slots already used.
    bool mixer = false;
    int mixerPhase = 0, mixerSongs = 0;
    bool mixerSave = false;
    bool mixerSaveUsed[3] = {};
    // A question of the game's popup is up over the minigame (kind 2, opening, shown or closing: the
    // painting waits); it takes taps once shown: its tick and cross.
    bool askUp = false, ask = false;
    Point askYes {0, 0}, askNo {0, 0};
    // The hotel's furniture editor is up (GameState::placement: the stick cursor's Unknown screen,
    // where B and MENU tap its X, kPlacementX).
    bool placement = false;
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
    // The timing minigames.
    Rect mark {0, 0, 0, 0};           // out: what the focus picks on the playfield (DS pixels; w 0 none)
    int quitChain = 0;                // MooGoo: B on the intro (1 the arrow tapped, 2 waiting for the X)
    int chainFrames = 0;
    int pumpFrames = 0, pumpDir = 1;  // the Drencher: frames the pump is held, the stroke's way
    int aimFrames = 0;                // the crime game: frames A has held the rat
    int lastSub = -1;                 // MooGoo: the turn's sub-state last frame (a new one: the focus starts again)
    // The Scratch lottery's rub (X): the box, the path (DS pixels), where along it, frames pressed.
    int rubBox = -1, rubAt = 0, rubFrames = 0;
    std::vector<Point> rubPath;
    // The mixer: out, the microphone held (Y: VOX); Y's tap waiting to start it.
    bool voice = false;
    // The save page: the slot whose overwrite was asked (-1 none).
    int armedSlot = -1;
    // A yes / no question of the game's over the activity (the cards' and MooGoo's quit, the
    // painting's "done?", the mixer's save question): it opens on No (the safe answer: A A never
    // says Yes), Left / Right move between No (0) and Yes (1), A answers the focused one, B is No.
    bool askOpen = false;
    int askFocus = 0;
};

// What CONSOLE draws of a pointing minigame's pointer (App::consolePointer, from the navigator):
// shown, where (DS touch pixels), the stylus down; the autopsy's quit asked once.
struct PointerView
{
    bool shown = false, down = false, quitArmed = false;
    float x = 0, y = 0;
    Rect mark {0, 0, 0, 0};           // MooGoo's focus on the playfield (DS pixels; w 0 none)
    int askFocus = -1;                // a question's focused answer (0 No, 1 Yes; ActivityState::askFocus), -1 none up
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
// The timing minigames (MooGoo, the Drencher, the crime game).
bool IsTiming(Screen s);
// A minigame with a scheme of its own (pointing or timing, the lottery, the mixer and its save
// page, the wireless lobby): no stick cursor there.
bool OwnScheme(Screen s);

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
// game's X and undo, the fifteen swatches (7 above, 8 below).
Point ArtTool(int button);
Point ArtSwatch(int swatch);
constexpr Point kArtDone {241, 14}, kArtUndo {241, 40};
// The palette's ▶ (sprite at 229,170: the next of its 16 pages, a ring).
constexpr Point kArtPageNext {241, 182};
// The vacuum's X at the bowl's top right (measured on act-vacuum's touch screen: its round
// button x 226-250, y 6-30): the game's only way out, back to the panel.
constexpr Point kVacuumX {237, 18};
// The hotel's furniture editor's X (module 0x14): back to the panel (checked: BD-place-lobby).
constexpr Point kPlacementX {235, 18};
constexpr int kArtTools = 10, kArtSwatches = 15;

// MooGoo Monkey (the game's rectangles, research/m6 code-B 1.3): the intro's play arrow; monkey m
// (56 + 32 m, 32, 24 x 32), its pile (y 88), hand card i (72 + 32 i, 137); the X (241,177, radius 10)
// and its question's Yes (80,80, 32 x 32) and No (144,80).
constexpr Point kMooGooArrow {238, 174}, kMooGooX {241, 177}, kMooGooYes {96, 96}, kMooGooNo {160, 96};
constexpr Rect MooGooMonkeyRect(int m) { return {56 + 32 * m, 32, 24, 32}; }
constexpr Rect MooGooPileRect(int m) { return {56 + 32 * m, 88, 24, 32}; }
constexpr Rect MooGooCardRect(int i) { return {72 + 32 * i, 137, 24, 32}; }
constexpr int kMooGooChainWait = 240;  // frames the intro's B waits for the game's X
// The Super Drencher (strict lo < v < hi rectangles): the X (219-253, 11-45), target (137-171,
// 46-80), refill (13-51, 159-197); the pump's band (rows 94-146, x 50-230 the handle's travel).
constexpr Point kDrencherX {236, 28}, kDrencherTarget {154, 63}, kDrencherRefill {32, 178}, kDrencherHelp {88, 176};
// Its fire button (219-253, 159-193) and the pump's band: in help mode, their texts (1666, 1663).
constexpr Point kDrencherFire {236, 176}, kDrencherPumpBand {140, 120};
constexpr int kPumpY = 120, kPumpLeft = 52, kPumpRight = 228;
constexpr float kPumpSpeed = 8.0f;     // a frame (16 a tick of the game's: past its 5)
// The crime game: the X (209-239, 17-47), scare (94-161, 13-56), subdue (94-161, 135-178), the
// centre rat (the aim slider, x 47-207 on row 96, 128 at rest).
constexpr Point kCrimeX {224, 32}, kCrimeScare {128, 35}, kCrimeSubdue {128, 156}, kCrimeRat {128, 96};
constexpr Point kCrimeHelp {32, 32};   // its "?" (17-47, 17-47)
constexpr int kCrimeAimMin = 47, kCrimeAimMax = 207;
constexpr float kCrimeAimSpeed = 3.0f;
// The Scratch lottery (research/m6 code-B 2.2): box i's centre on a ticket of tier t (its cover the
// 32 x 32 square around it, a press within 16 of it), the ticket's X; a rub's passes (the game
// erases 3 x 3 around the stylus: passes 3 apart clear the box; it reveals it at 684 of 1024).
Point ScratchCentre(int tier, int i);
Point ScratchX(int tier);
constexpr int kRubHalf = 15, kRubStep = 3;
// The rub's path over box i of tier t: from its top left corner, passes along x, kRubStep apart.
std::vector<Point> ScratchRub(int tier, int i);
// The music mixer's buttons (research/m6 code-D 1.3, 1.7): PLAY, RECORD, QUIT, GENRE, ?, VOX; the
// song list's row i (its tap, its rectangle); the save question's Yes, No, Preview.
constexpr Point kMixPlay {82, 40}, kMixRecord {82, 68}, kMixQuit {17, 13}, kMixGenre {43, 12}, kMixHelp {82, 13}, kMixVox {20, 147};
constexpr Point MixerRow(int i) { return {62, 44 + 16 * i}; }
constexpr Rect MixerRowRect(int i) { return {18, 36 + 16 * i, 88, 16}; }
constexpr Point kMixYes {86, 102}, kMixNo {128, 102}, kMixPreview {170, 102};
// The wireless lobby's X (both its pages: back to the title from the lobby).
constexpr Point kTradingX {236, 178};
}
