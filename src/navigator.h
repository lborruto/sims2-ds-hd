// CONSOLE's navigator: a controller (or the keyboard, through the same bindings) drives the
// game's touch screen. Per recognised screen (screens.h) it either passes the buttons on as the
// DS's own (town, a conversation) or moves a focus over the screen's touch targets (hotspots.h)
// and taps them through a TouchScript:
//   Panel, menu closed: every button is the DS's (L and R turn the camera); MENU opens the menu.
//   Panel, menu open: the D-pad (or the left stick) moves the focus, A taps the focused target,
//     L / R tap the previous / next tab's icon, B or MENU close the menu.
//   Phone: the focus as in the menu; B or MENU tap the phone's X (back to the panel, menu closed).
//   Gift, Catalogue, Build: the focus as in the menu; B taps the screen's X when it has one.
//   Social, Cards, Vacuum: the activity's own buttons (activities.h); the cards' focus moves as
//     in the menu, the vacuum's hand is the stick cursor.
//   Popup (a goal, tips, Game Paused): A or B tap its X; Start stays the DS's (it resumes a pause).
//   Dialogue: every button is the DS's (A is the game's own "next").
//   None: every button is the DS's.
//   Unknown: the DS's buttons, but A presses the stick cursor when there is one (the fallback for
//     unmeasured screens); without a controller A stays the DS's.
// Presses are edges of the PadState (pressed now, not on the previous Update). While the touch
// script owns the stylus, presses that would tap are dropped, never queued behind it.
#pragma once
#include "activities.h"
#include "common.h"
#include "padstate.h"
#include "screens.h"
#include "touchscript.h"

namespace sims2wide
{
// A button that acts once, on its release, and only if `spoil` was never true while it was held:
// the pad's TOUCH MENU on Back, since Start + Back (in either order) is the pause menu.
class ReleasePulse
{
public:
    // Once a frame; true on the frame the button goes up unspoiled.
    bool Update(bool down, bool spoil)
    {
        bool fire = false;
        if (down)
        {
            if (!down_) spoiled_ = false;
            spoiled_ |= spoil;
        }
        else if (down_) fire = !spoiled_;
        down_ = down;
        return fire;
    }

private:
    bool down_ = false, spoiled_ = false;
};

class Navigator
{
public:
    // Once per game frame while CONSOLE is shown: the DS keys to press are OR-ed into dsKeys,
    // taps are queued on touch.
    void Update(ScreenState s, const PadState& pad, TouchScript& touch, u32& dsKeys);
    // CONSOLE left (another layout): the touch script cancelled, the menu closed, nothing held;
    // buttons still held when CONSOLE comes back are not taken as presses.
    void Leave(TouchScript& touch);
    // Buttons held now are not taken as presses on the next Update (after the pause menu).
    void Resync() { fresh_ = true; }

    // The panel's menu is up (the touch screen 3x over the dimmed world).
    bool MenuOpen() const { return menuOpen_; }
    // The focused target, an index in HotspotsFor(screen), or -1 (no focus on this screen).
    int Focus() const { return focus_; }
    // Unknown: A asks for the stick cursor's press.
    bool CursorPress() const { return cursorPress_; }
    // The stick cursor is the way to touch here (no table of targets: None, Vacuum, Unknown).
    bool CursorScreen() const { return cursorScreen_; }
    // The vacuum: the stick cursor is its hand, shown and moved even without a controller (the
    // D-pad moves it while X or A is held: HandDx / HandDy, DS pixels this frame); CursorPress
    // is A held.
    bool Hand() const { return screen_.screen == Screen::Vacuum; }
    float HandDx() const { return handDx_; }
    float HandDy() const { return handDy_; }
    // The vacuum's Y: blow into the microphone this frame.
    bool Blow() const { return blow_; }

private:
    void Move(ScreenState s, int dx, int dy);

    PadState prev_;
    bool fresh_ = true;  // take the next pad as the previous one (no presses)
    ScreenState screen_;
    bool menuOpen_ = false, cursorPress_ = false, cursorScreen_ = false, blow_ = false;
    float handDx_ = 0, handDy_ = 0;
    ActivityState activity_;
    int focus_ = -1;
    int closing_ = 0;    // frames left in which A / B do nothing after a tap that closes the screen
    int repeatDir_ = -1, repeatFrames_ = 0; // the held direction (0-3) and how long, for the repeat
};
}
