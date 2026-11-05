// Keyboard, mouse and controller: the DS buttons, the stylus, and the frontend's own actions
// (menu, fast forward, swapping the screens, save states, blowing into the microphone), through
// the remappable bindings (controls.h). The mouse also works the game's camera over the world
// screen: the wheel zooms (X / Y), a right-button drag turns it (L / R). A controller's right
// stick moves a cursor on the touch screen, which its CURSOR PRESS button touches.
#pragma once
#include <SDL.h>

#include <vector>

#include "app.h"
#include "script.h"
#include "ui.h"
#include "video.h"

namespace sims2wide
{
struct PadState
{
    bool btn[Controls::IN_COUNT] = {};
    float lx = 0, ly = 0, rx = 0, ry = 0;
};

struct Input
{
    // The keyboard: which actions' keys are down.
    bool held[Controls::COUNT] = {};
    bool fastPrev = false; // (App::fast follows the key only when it changes: --fast holds until then)
    // Controllers (any number: each button pressed on any of them counts).
    std::vector<SDL_GameController*> pads;
    PadState pad, prevPad;
    bool prevCombo = false;       // START and BACK together (the menu, like GUIDE)
    int padStyle = Controls::XBOX; // the last controller's labels, for the menu

    bool mouseDown = false, rightDown = false;
    // The left button went down on the touch screen: until it is released the stylus stays on
    // it, held to its edges (a drag that overshoots does not lift the stylus).
    bool stylusDrag = false;
    int dragPiece = 0;             // the composition piece the stylus drag began on
    // A press on the touch screen keeps the stylus down for at least this many game frames,
    // where it was pressed: a quick click or a trackpad tap sends the button down and up within
    // milliseconds, before the next frame, and the DS would never see the touch.
    int tapFrames = 0, tapX = 0, tapY = 0;
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
    // While the menu is open: what it understands, from the keyboard, the mouse and the controller.
    MenuInput nav;
    // While the menu captures a binding: the first key or controller input pressed.
    bool capturing = false, captureCancel = false;
    int newKey = Controls::kDefault, newPad = -1; // kDefault: nothing yet; kNone: cleared

    // An input script's window events for this turn of the loop (script.h), pushed as SDL events.
    void PushScript(const std::vector<InputEvent>& script, long iter, const Video& video) const;
    // After SDL_Init(SDL_INIT_GAMECONTROLLER): opens the controllers already connected.
    void OpenPads();
    void ClosePads();
    void HandleEvent(const SDL_Event& ev, App& app, const Video& video, bool menuOpen);
    // Once a frame, before Frame or the menu: reads the controllers, turns presses into
    // requests (or the menu's input while it is open).
    void Poll(const App& app, bool menuOpen);
    // Adds this frame's DS keys and touch from the keyboard, the mouse, the controller and
    // the stick cursor; sets App::fast and App::micBlow.
    void Frame(App& app, const Video& video, long frame, u32& keys, bool& touching, int& tx, int& ty);
    // Clears the one-frame requests and the menu's input.
    void EndFrame();

private:
    bool PadOn(const Settings& s, int action) const;
    bool PadPressed(const Settings& s, int action) const;
    void OpenPad(int index);
};
}
