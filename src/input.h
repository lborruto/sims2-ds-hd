// Keyboard, mouse and controller: the DS buttons, the stylus, and the frontend's own actions
// (menu, fast forward, swapping the screens, save states, blowing into the microphone), through
// the remappable bindings (controls.h). The mouse also works the game's camera over the world
// screen: the wheel zooms (X / Y), a right-button drag turns it (L / R). A controller's right
// stick moves a cursor on the touch screen, which its CURSOR PRESS button touches. In CONSOLE the
// DS's buttons go through the navigator (navigator.h) first: it drives the touch screen's menu.
#pragma once
#include <SDL.h>

#include <vector>

#include "app.h"
#include "navigator.h"
#include "pads.h"
#include "script.h"
#include "shell.h"
#include "touchscript.h"
#include "video.h"

namespace sims2hd
{
struct Input
{
    // The keyboard: which actions' keys are down.
    bool held[Controls::COUNT] = {};
    bool fastPrev = false; // (App::fast follows the key only when it changes: --fast holds until then)
    // Controllers (pads.h: any number, hot-plugged; the last one used drives, Steam Input's
    // virtual pad over its physical twin).
    Pads pads;
    PadRaw pad, prevPad;
    bool prevCombo = false;       // START and BACK together (the pause menu, like Esc)
    int padStyle = Controls::XBOX; // the driving controller's labels, for the menu
    // The keyboard was used last (the HD chips show its keys), else a controller (its buttons).
    bool lastKeyboard = false;

    bool mouseDown = false, rightDown = false;
    // The left button went down on the touch screen: until it is released the stylus stays on
    // it, held to its edges (a drag that overshoots does not lift the stylus).
    bool stylusDrag = false;
    int dragPiece = 0;             // the composition piece the stylus drag began on
    // A press on the touch screen keeps the stylus down for at least this many game frames,
    // where it was pressed: a quick click or a trackpad tap sends the button down and up within
    // milliseconds, before the next frame, and the DS would never see the touch.
    int tapFrames = 0, tapX = 0, tapY = 0;
    // Touches made from code (taps, holds, drags over game frames); they win over the mouse and the stick.
    TouchScript touchScript;
    // CONSOLE: the touch screen's menu, focus and taps from the buttons (Steer).
    Navigator navigator;
    bool steering = false;        // the navigator ran last frame (CONSOLE was shown)
    // The money's last change (App::consolePayout): the money last frame (-1 not known), the frames
    // it is still shown.
    int lastMoney_ = -1, payoutFrames_ = 0;
    static constexpr int kPayoutFrames = 240;
    PadState scriptPad;           // this frame's "pad" events of the input script (script.h)
    // The pad's TOUCH MENU acts on release, unless Start was held meanwhile (the pause menu).
    ReleasePulse panelButton;
    bool panelPulse = false;      // this frame
    int mouseX = 0, mouseY = 0, dragX = 0;
    int zoom = 0;   // frames of X (> 0, zoom in) or Y (< 0, zoom out) still to hold, from the wheel
    float zoomAcc = 0;
    int rotate = 0; // frames of R (> 0) or L (< 0) still to hold, from the right-button drag

    // The stick touch cursor, in DS touch-screen pixels; drawn while recently moved.
    float cursorX = SW / 2, cursorY = SH / 2;
    long cursorShownUntil = 0;
    bool cursorPress = false;

    // What the keys and buttons asked for this frame (one frame each; main.cpp acts on them).
    bool wantMenu = false, wantSave = false, wantLoad = false, wantShot = false, wantFullscreen = false;
    int wantLayout = -1; // a Layout, or -1
    // While the menu is open: what it understands, from the keyboard, the mouse (output pixels: the
    // host takes them to the shell's canvas) and the controller (a held direction repeats).
    shell::Input nav;
    int navHeld[4] = {};
    // While the menu captures a binding: the first key, or the controller button released first
    // (Start and Back pressed together: cancelled, as Esc), Backspace no key.
    bool capturing = false, captureCancel = false;
    int newKey = Controls::kDefault, newPad = -1; // kDefault: nothing yet; kNone: cleared
    int captureFirst = -1;
    unsigned captureSeen = 0;
    void StartCapture() { capturing = true; captureFirst = -1; captureSeen = 0; newKey = Controls::kDefault; newPad = -1; captureCancel = false; }
    // The window was asked to close (the shell asks first); a file was dropped on it.
    bool wantQuit = false;
    std::string droppedFile;
    // The window is in front (keyboard focus, not minimised; Poll reads it from the window's flags):
    // off, the game pauses unless it runs in the background.
    SDL_Window* window = nullptr;
    bool focused = true;
    Uint32 mouseMovedAt = 0;      // SDL ticks of the mouse's last move or click (the pointer's auto-hide)
    // Create-a-Sim's name typed on the keyboard (port/cas.h, CasTypeAction): the characters still to
    // type ("\b" the backspace), one at a time through the game's own keyboard; the frames to wait
    // before the next (the game reads a tap within a few), and the tries at its page.
    std::vector<std::string> typed;
    int typeWait = 0, typeTries = 0;
    // The Name page of Create-a-Sim is up in CONSOLE: Enter is the navigator's A there (enterA),
    // Backspace erases, and while the focus is on the game's keyboard (its keys, its page labels,
    // its backspace: TypingKeys) printable keys type, their bindings resting; arrows and Esc stay
    // what they are.
    static bool TypingName(const App& app);
    bool TypingKeys(const App& app) const;
    bool enterA = false;

    // An input script's window events for this turn of the loop (script.h), pushed as SDL events.
    void PushScript(const std::vector<InputEvent>& script, long iter, const Video& video) const;
    // After SDL_Init(SDL_INIT_GAMECONTROLLER) and Pads::LoadMappings: opens the controllers
    // already connected (the others come as they are plugged in).
    void OpenPads(const Settings& s);
    void ClosePads();
    static PadOptions PadOptionsOf(const Settings& s);
    void HandleEvent(const SDL_Event& ev, App& app, const Video& video, bool menuOpen);
    // Once a frame, before Frame or the menu: reads the controllers, turns presses into
    // requests (or the menu's input while it is open).
    void Poll(const App& app, bool menuOpen);
    // Adds this frame's DS keys and touch from the keyboard, the mouse, the controller and
    // the stick cursor; sets App::fast and App::micBlow.
    void Frame(App& app, const Video& video, long frame, u32& keys, bool& touching, int& tx, int& ty);
    // What the keyboard, the controllers and the script's "pad" events hold, as the navigator's actions.
    PadState Actions(const Settings& s) const;
    // CONSOLE shown: the navigator turns the actions into DS keys (OR-ed into keys) and taps,
    // and App::consoleMenu / consoleFocus follow it (unless pinned by --console-menu or
    // --console-focus). CONSOLE left: the navigator lets go of everything (Navigator::Leave).
    void Steer(App& app, const PadState& p, u32& keys);
    // After the frame ran (the game's new screen known), when the screen changed or a popup came
    // or went: App::consoleFocus as an index of the view CONSOLE now draws, by the focused item's
    // id (the navigator ran on the frame before's view: the old index would show for one frame,
    // on the wrong item). Within one view (a list scrolled) the index stays as the navigator left it.
    void FocusForDrawing(App& app, bool screenChanged);
    // One typed character's taps when the game can take them (Steer, after the navigator).
    void TypeName(App& app);
    // The touch script's touch this frame, over any other while it owns the stylus.
    void ScriptTouch(bool& touching, int& tx, int& ty);
    // A pointing minigame's stylus at its pointer (the navigator's; CONSOLE), before ScriptTouch.
    void PointerTouch(const App& app, bool& touching, int& tx, int& ty);
    // Clears the one-frame requests and the menu's input.
    void EndFrame();

private:
    bool PadOn(const Settings& s, int action) const;
    bool PadPressed(const Settings& s, int action) const;
};
}
