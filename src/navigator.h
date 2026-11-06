// CONSOLE's navigator: a controller (or the keyboard, through the same bindings) drives the
// game's touch screen. Per recognised screen (screens.h) it either passes the buttons on as the
// DS's own (town, a conversation) or moves a focus over the screen's touch targets (hotspots.h)
// and taps them through a TouchScript:
//   Panel, menu closed: every button is the DS's (L and R turn the camera); MENU opens the menu.
//   Panel, menu open: the D-pad (or the left stick) moves the focus, A taps the focused target,
//     L / R tap the previous / next tab's icon, B or MENU close the menu. When the tab is redrawn
//     in HD (a valid MenuView, port/menuview.h), the focus is over the view's items instead of
//     HotspotsFor: A runs the focused item's action (taps, DS buttons, drags, in order), greyed
//     items are skipped and never run, the view's screen-wide buttons (Left / Right: Interact's
//     targets), the focused item's own Left / Right, a selection the game owns (Up / Down its
//     scroll) and the list's scroll apply (those taps held by the gate like A's), X runs the
//     focused item's own X (Pockets' bin) or the view's, B runs the view's back action or closes
//     the menu.
//   Phone: in HD (port/phone.h) whenever its view is valid: the game's own Up / Down / A on its
//     rows, B or MENU its X; else the focus as in the menu; B or MENU tap the phone's X (back to the
//     panel, menu closed).
//   A popup over the panel or an HD screen, a note: in HD (port/popup.h, port/note.h) whenever its
//     view is valid (MenuView::own). A popup over the open menu wins the focus and keeps the menu
//     open beneath; when it goes and the screen is back, the focus returns to the item it was on
//     (the resume).
//   Gift: in HD (port/gift.h) whenever its view is valid, as the HD menu (B or MENU its back: the X;
//     no tabs); else, like Catalogue and Build: the focus as in the menu; B taps the screen's X
//     when it has one.
//   On every HD screen of its own (phone, Give Gift, the Save slots, a note) MENU does what B does
//     (a popup's MENU does nothing: its question is B's).
//   Social, Cards, Vacuum: the activity's own buttons (activities.h); the cards' focus moves as
//     in the menu, the vacuum's hand is the stick cursor.
//   Popup (a goal, tips, Game Paused) without its HD view: A or B tap its X; Start stays the DS's
//     (it resumes a pause).
//   Dialogue: every button is the DS's (A is the game's own "next").
//   FrontEnd, CreateASim (from RAM), None: touch-only (the DS's buttons change nothing there),
//     no table yet: as Unknown.
//   SaveSlots: in HD (port/options.h) whenever its view is valid, as Give Gift (A the slot's tick,
//     B its back: the X); else, like ItemHand (from RAM; no table yet): as Unknown.
//   Loading (a black load): every button is the DS's.
//   Unknown: the DS's buttons, but A presses the stick cursor when there is one (the fallback for
//     unmeasured screens); without a controller A stays the DS's.
// Presses are edges of the PadState (pressed now, not on the previous Update). While the touch
// script owns the stylus, presses that would tap are dropped, never queued behind it. While the
// game's bottom slot switches or a popup slides (not settled), they wait (90 frames at most).
#pragma once
#include "activities.h"
#include "common.h"
#include "padstate.h"
#include "port/menuview.h"
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
    // taps are queued on touch. settled: the game's bottom slot is not switching
    // (GameState::settled; true without a RAM reading); while it is, a press that would tap is
    // held (the latest wins) and replayed on the first settled frame, or dropped after 90.
    // view: the screen's HD menu (port/menuview.h), rebuilt every frame from the game's state;
    // used while the menu is open on the panel when valid (else, or null, HotspotsFor as before).
    // A press held by the gate is dropped when the view's Hash changes before it runs (no stale
    // taps); the focus follows its item's id across rebuilds.
    void Update(ScreenState s, const PadState& pad, TouchScript& touch, u32& dsKeys, bool settled = true, const MenuView* view = nullptr);
    // CONSOLE left (another layout): the touch script cancelled, the menu closed, nothing held;
    // buttons still held when CONSOLE comes back are not taken as presses.
    void Leave(TouchScript& touch);
    // Buttons held now are not taken as presses on the next Update (after the pause menu).
    void Resync() { fresh_ = true; }
    // The game's RAM on the activity (activities.h), for the next Update (CONSOLE's frame loop sets
    // it every frame; default: nothing known).
    void SetActivityRam(const ActivityRam& r) { activityRam_ = r; }

    // The panel's menu is up (the touch screen 3x over the dimmed world).
    bool MenuOpen() const { return menuOpen_; }
    // The focused target, an index in HotspotsFor(screen) or, when ViewFocus, in the MenuView's
    // items; -1: no focus on this screen.
    int Focus() const { return focus_; }
    // The focus is over the last Update's MenuView.
    bool ViewFocus() const { return viewFocus_; }
    // The id of the focused view item whose X is armed (ViewItem::xAsks: one more X performs it),
    // 0 none.
    u32 Armed() const { return armed_; }
    // An action of the view runs (its steps not all done).
    bool Running() const { return !running_.empty(); }
    // Unknown: A asks for the stick cursor's press.
    bool CursorPress() const { return cursorPress_; }
    // The stick cursor is the way to touch here (no table of targets: the front end, None,
    // Vacuum, Unknown).
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
    // Starts a view's action (nothing when one runs or the stylus is busy); false when not started.
    bool Run(const MenuAction& a, const TouchScript& touch);
    // One frame of the running action: taps and drags queued when the stylus is free, keys held.
    void Step(TouchScript& touch, u32& dsKeys);
    // What a direction (0 up, 1 down, 2 left, 3 right) performs on view v with the current focus:
    // the selection's scroll, the focused item's Left / Right, the view's Left / Right, the list's
    // scroll past its end (scroll true); null: a focus move.
    const MenuAction* DirAction(const MenuView& v, int dir, bool& scroll) const;
    // The view given is valid and shown (the panel's menu open, or Give Gift): the focus is over it.
    bool UsesView(ScreenState s) const;
    // What X runs on view item f: its own X (an asking one not armed yet: its xArm), else the
    // view's.
    const MenuAction& XAction(const MenuView& v, int f) const;
    // Whether a press of b would tap on s (the gate holds only those).
    bool WouldTap(ScreenState s, bool PadState::*b) const;

    PadState prev_;
    bool fresh_ = true;  // take the next pad as the previous one (no presses)
    ScreenState screen_;
    bool menuOpen_ = false, cursorPress_ = false, cursorScreen_ = false, blow_ = false;
    float handDx_ = 0, handDy_ = 0;
    ActivityState activity_;
    ActivityRam activityRam_;
    int focus_ = -1;
    int closing_ = 0;    // frames left in which A / B do nothing after a tap that closes the screen
    int repeatDir_ = -1, repeatFrames_ = 0; // the held direction (0-3) and how long, for the repeat
    bool PadState::*held_ = nullptr; // a press that would tap, waiting for a settled screen
    int heldFrames_ = 0;
    u64 heldHash_ = 0;   // the view's Hash when the held press was made (0: no view)
    // The HD menu (MenuView): the view this frame (null: none), the focused item's id, the view's
    // hash last frame, the action running, a focus move waiting for the list's scroll.
    const MenuView* view_ = nullptr;
    bool viewFocus_ = false;
    u32 focusId_ = 0;
    u32 armed_ = 0;      // the item (id) whose asking X was pressed once (ViewItem::xAsks)
    u32 moduleId_[16] = {}; // per module tag (the id's top bits): the item it last had the focus on, on this screen
    int Arrive(const MenuView& v) const; // the focus on a view that has none: that module's last item, else its first
    u64 viewHash_ = 0;
    int viewPage_ = 0;   // the view's page last frame (MenuView::page)
    bool viewQuestion_ = false;   // and whether it was a question
    u32 pageKeptId_ = 0; // the focused item's id when the last non-question page was left
    std::vector<MenuStep> running_;
    int pendingDir_ = -1, pendingFrames_ = 0;
    // A popup's view last frame; what was under it when it came (the screen, the menu open, the
    // focused item's id and index), and a focus to put back once that screen's view is there.
    bool popupUp_ = false, resumeMenu_ = false, resumeValid_ = false, restore_ = false;
    ScreenState resumeScreen_;
    u32 resumeId_ = 0;
    int resumeIndex_ = -1, restoreFrames_ = 0;
};
}
