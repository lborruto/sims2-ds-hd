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
//     rows, B back one level (a page's Exit row; the menu's: the X), MENU the X from any page
//     (MenuView::menu); else the focus as in the menu; B or MENU tap the phone's X (back to the
//     panel, menu closed). Opened from the menu, the menu comes back on its tab and item after it
//     (and after the Save slots), the dimmed world drawn meanwhile (Returning).
//   A question (a popup of kind 2, Build Rooms', a minigame's) opens on its No (MenuView::initial);
//     a view's backFocus takes the first B (Create-a-Sim's Keep this Sim?: the cross), the second
//     runs it; a slider's presses queue (ViewItem::queues), dropped on B / MENU / LB / RB.
//   A popup over the panel or an HD screen, a note: in HD (port/popup.h, port/note.h) whenever its
//     view is valid (MenuView::own). A popup over the open menu wins the focus and keeps the menu
//     open beneath; when it goes and the screen is back, the focus returns to the item it was on
//     (the resume).
//   Gift: in HD (port/gift.h) whenever its view is valid, as the HD menu (B or MENU its back: the X;
//     no tabs); else, like Catalogue and Build: the focus as in the menu; B taps the screen's X
//     when it has one.
//   On every HD screen of its own (phone, Give Gift, the Save slots, a note) MENU does what B does
//     unless the view gives it its own action (the phone's X); a popup's MENU does nothing (its
//     question is B's).
//   Social, Cards, Vacuum: the activity's own buttons (activities.h); the cards' focus moves as
//     in the menu, the vacuum's hand is the stick cursor.
//   Popup (a goal, tips, Game Paused) without its HD view: A or B tap its X; Start stays the DS's
//     (it resumes a pause).
//   Dialogue: every button is the DS's (A is the game's own "next").
//   FrontEnd, CreateASim (from RAM), None: touch-only (the DS's buttons change nothing there),
//     no table yet: as Unknown.
//   SaveSlots: in HD (port/options.h) whenever its view is valid, as Give Gift (A the slot's tick,
//     B its back: the X); else, like Drencher (from RAM; no table yet): as Unknown.
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
    // Instead of Update while CONSOLE's picture is held (consolehold.h): no press is taken and
    // nothing taps, but a running action's keys step goes on to its end (a held Up / Down is not
    // released and pressed again mid-step); the step after it waits for Update.
    void Held(const TouchScript& touch, u32& dsKeys);
    // CONSOLE left (another layout): the touch script cancelled, the menu closed, nothing held;
    // buttons still held when CONSOLE comes back are not taken as presses.
    void Leave(TouchScript& touch);
    // Buttons held now are not taken as presses on the next Update (after the pause menu).
    void Resync() { fresh_ = true; }
    // The game's RAM on the activity (activities.h), for the next Update (CONSOLE's frame loop sets
    // it every frame; default: nothing known).
    void SetActivityRam(const ActivityRam& r) { activityRam_ = r; }

    // The panel's menu is up (its HD view, else milestone 1's touch screen 3x, over the dimmed world).
    // (Not drawn open while it is coming back to the tab it was left on: the game shows Interact
    // first for a few frames.)
    bool MenuOpen() const { return menuOpen_ && !(returnFocus_ > 0 && screen_.tab != returnTab_); }
    // The menu is coming back (B from the Save slots or the phone opened from it): from that screen
    // through the frames between (no bottom object, Loading) to the panel on another tab than the
    // one it was left on, before the tab's tap: CONSOLE draws the panel, Unknown and Loading as the
    // dimmed world then, not the town (a flash of a few frames).
    // At most kReturnVeilFrames after the panel is back (a tab tap that missed: the town again).
    bool Returning() const;
    // The focused target, an index in HotspotsFor(screen) or, when ViewFocus, in the MenuView's
    // items; -1: no focus on this screen.
    int Focus() const { return focus_; }
    // The focus is over the last Update's MenuView.
    bool ViewFocus() const { return viewFocus_; }
    // The id of the focused MenuView item (ViewFocus), 0 none.
    u32 FocusId() const { return viewFocus_ && focus_ >= 0 ? focusId_ : 0; }
    // The focus the next Update would put on view v (an index of v): the focused item's id there,
    // else (a popup gone, its screen back) the item it was on before the popup, else the view's
    // first focus (a question's initial No). For drawing the frame after the game changed screen.
    int FocusFor(const MenuView& v) const;
    // A popup's view was up at the last Update.
    bool PopupUp() const { return popupUp_; }
    // The view's big picture (MenuView::picture) not opened by an A on this screen, or closed by
    // B since (the screen drawn without it).
    bool PictureClosed() const { return !pictureOpen_; }
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
    // The mixer's Y (activities.h): its microphone held (VOX): the blow's signal, or the computer's
    // own microphone when the settings choose it (Input::Steer).
    bool Voice() const { return voice_; }
    // The pointing minigames (activities.h): their pointer, drawn (PointerShown), and the stylus
    // down at it this frame (PointerDown: also the detector's slider knob, not drawn), DS pixels.
    bool PointerShown() const { return activity_.pointer; }
    bool PointerDown() const { return activity_.down; }
    float PointerX() const { return activity_.px; }
    float PointerY() const { return activity_.py; }
    // MooGoo's focus on the playfield (DS pixels; w 0 none).
    Rect Mark() const { return activity_.mark; }
    int AskFocus() const { return activity_.askOpen ? activity_.askFocus : -1; }
    // The autopsy's or the lottery's quit asked once (a second B / Start quits); the mixer's save
    // page asking before it saves over a slot.
    bool QuitArmed() const { return activity_.quitArmed > 0 || activity_.armedSlot >= 0; }

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
    bool menuOpen_ = false, cursorPress_ = false, cursorScreen_ = false, blow_ = false, voice_ = false;
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
    u32 moduleId_[32] = {}; // per module tag (the id's top bits): the item it last had the focus on, on this screen
    int Arrive(const MenuView& v) const; // the focus on a view that has none: that module's last item, else its first
    u64 viewHash_ = 0;
    int viewPage_ = 0;   // the view's page last frame (MenuView::page)
    bool viewQuestion_ = false;   // and whether it was a question
    u32 pageKeptId_ = 0; // the focused item's id when the last non-question page was left
    int pageKeptIndex_ = -1; // and its index (the item gone when the page is back: the one in its place)
    bool pictureOpen_ = false; // MenuView::picture shown big: an A ran; B, LB / RB or another screen close it
    // The Save slots or the phone opened from the menu: the tab and item it was opened from, the
    // frames left to come back to the panel (returnFrames_) and to put the focus back
    // (returnFocus_; kReturnFrames / kReturnFocusFrames in navigator.cpp), the tab still to tap
    // (the game comes back on Interact).
    Tab returnTab_ = Tab::None;
    u32 returnId_ = 0;
    int returnFrames_ = 0, returnFocus_ = 0;
    int veilFrames_ = 0; // (the HD screen left on the way back: frames the veil may still last, Returning)
    Tab leftTab_ = Tab::None; // (the menu's tab and item when it was last left for another screen)
    u32 leftId_ = 0;
    int leftFrames_ = 0;
    bool returnTap_ = false; // the panel came back on another tab than returnTab_: tap returnTab_'s icon first
    // A slider's presses (ViewItem::queues): the steps still owed in the order pressed (-1 Left,
    // +1 Right; 16 at most, one entry a press: nothing is merged, so Left then Right is a step
    // there and one back), its item's id, the Left / Right it had when the last step ran, and how
    // long the queue has waited. It drains one step at a time, settled: each waits until the item's
    // Left / Right were rebuilt from the game's new value (the game took the last one) or
    // kQueueRead frames passed (an end); an empty or stale queue (kHoldFrames) is cleared, and so
    // is any queue on a screen change, the item going, or B / MENU / LB / RB.
    std::vector<int> queue_;
    int queueFrames_ = 0;
    u32 queueId_ = 0;
    bool queueWaiting_ = false;
    MenuAction queueLeft_, queueRight_;
    void ClearQueue() { queue_.clear(); queueFrames_ = 0; queueId_ = 0; queueWaiting_ = false; queueLeft_.clear(); queueRight_.clear(); }
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
