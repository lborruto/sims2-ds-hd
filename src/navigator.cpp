#include "navigator.h"

#include <cstring>
#include <vector>

#include "activities.h"
#include "hotspots.h"

namespace sims2wide
{
namespace
{
// A tap that closes the screen (a popup's X, the phone's X): A and B do nothing more until the
// screen changes, or this many frames if it does not (the tap missed). A popup's box slides away
// for a while and still reads Popup: a second tap would land on the panel behind it.
constexpr int kClosingFrames = 90;
// A press that would tap while the game's bottom slot is switching (GameState::settled false):
// held this many frames at most, then dropped.
constexpr int kHoldFrames = 90;
// A direction held: one focus move, then one every kRepeatEvery frames after kRepeatAfter.
constexpr int kRepeatAfter = 18, kRepeatEvery = 6;
constexpr float kStick = 0.5f;
// A slider's queued step (ViewItem::queues) waits for the game to read the last one back this
// many frames at most (an end: nothing changes).
constexpr int kQueueRead = 30;
// Back from the Save slots or the phone opened from the menu (Navigator::returnFrames_ counts
// down from kReturnFrames): the panel must come back within this many frames (the slots' saving
// and its popup included), else the return is forgotten. Once the panel is back,
// Navigator::returnFocus_ counts down from kReturnFocusFrames: the tab is tapped and the item
// refocused within that time, else the menu stays as the game left it. kLeftFrames: the Save slots
// or the phone must follow the panel's menu within this many frames to count as opened from it.
constexpr int kReturnFrames = 900, kReturnFocusFrames = 900, kLeftFrames = 60;
// The dimmed world instead of the town while the menu comes back (Returning): this many frames
// at most once the panel is back (the tab's tap takes about 5).
constexpr int kReturnVeilFrames = 60;

enum class Mode
{
    Pass,   // the buttons are the DS's
    Focus,  // a focus over the screen's targets, A taps
    Popup,  // A or B close the popup
    Cursor, // the DS's buttons, A presses the stick cursor
    Activity, // activities.h plays it (the cards' focus moves as on a focus screen)
};

Mode ModeOf(Screen s, bool menuOpen)
{
    switch (s)
    {
    case Screen::Panel: return menuOpen ? Mode::Focus : Mode::Pass;
    case Screen::Phone: case Screen::Gift: case Screen::Catalogue: case Screen::Build: return Mode::Focus;
    case Screen::Social: case Screen::Cards: case Screen::Vacuum: case Screen::Autopsy: case Screen::Detector: case Screen::Art:
    case Screen::Drencher: case Screen::MooGoo: case Screen::Crime:
    case Screen::Scratch: case Screen::Mixer: case Screen::MixerSave: case Screen::Trading: return Mode::Activity;
    case Screen::Popup: return Mode::Popup;
    case Screen::Unknown: return Mode::Cursor;
    // No table yet: as Unknown. The front end (title, main menu, Create-a-Sim, None) is
    // touch-only: the DS's buttons there change nothing, so A presses the stick cursor.
    case Screen::SaveSlots: case Screen::FrontEnd: case Screen::CreateASim: case Screen::None: return Mode::Cursor;
    case Screen::Dialogue: case Screen::Loading: return Mode::Pass;
    }
    return Mode::Pass;
}

bool Up(const PadState& p) { return p.up || p.ly < -kStick; }
bool Down(const PadState& p) { return p.down || p.ly > kStick; }
bool Left(const PadState& p) { return p.left || p.lx < -kStick; }
bool Right(const PadState& p) { return p.right || p.lx > kStick; }

// The direction held (0 up, 1 down, 2 left, 3 right; the first of them), or -1.
int Direction(const PadState& p)
{
    return Up(p) ? 0 : Down(p) ? 1 : Left(p) ? 2 : Right(p) ? 3 : -1;
}

// The DS's buttons as held (the left stick as the D-pad).
u32 DsKeys(const PadState& p, bool withA)
{
    u32 k = 0;
    auto set = [&](bool on, int key) { if (on) k |= 1u << key; };
    set(p.a && withA, kA);
    set(p.b, kB);
    set(p.x, kX);
    set(p.y, kY);
    set(p.lb, kL);
    set(p.rb, kR);
    set(p.start, kStart);
    set(p.select, kSelect);
    set(Up(p), kUp);
    set(Down(p), kDown);
    set(Left(p), kLeft);
    set(Right(p), kRight);
    return k;
}

// The first target the focus may rest on (not a drag-only one), or -1.
int First(const std::vector<Hotspot>& h)
{
    for (int i = 0; i < (int)h.size(); i++)
        if (!h[i].dragOnly) return i;
    return -1;
}

// The target named "close" (Give Item's, the catalogue's, Build Rooms' X), or -1.
int CloseOf(const std::vector<Hotspot>& h)
{
    for (int i = 0; i < (int)h.size(); i++)
        if (!strcmp(h[i].name, "close")) return i;
    return -1;
}
}

// The screen's mode: an HD screen of its own (a valid view: Give Gift, the Save slots, the phone,
// a popup, a note) a focus screen whatever the screen state says.
static Mode ModeWith(ScreenState s, bool menuOpen, const MenuView* view)
{
    if (view && view->valid && view->own) return Mode::Focus;
    return ModeOf(s.screen, menuOpen);
}

// Mirrors the taps of Update's Mode::Focus and Mode::Popup cases: change one, change the other.
bool Navigator::WouldTap(ScreenState s, bool PadState::*b) const
{
    const Mode mode = ModeWith(s, menuOpen_, view_);
    if (mode == Mode::Popup) return b == &PadState::a || b == &PadState::b;
    if (mode != Mode::Focus) return false;
    if (UsesView(s))
    {
        const MenuView& v = *view_;
        const int f = MenuFocusable(v, focus_) ? focus_ : MenuFirst(v);
        if (b == &PadState::a) return f >= 0 && !v.items[f].action.empty();
        if (b == &PadState::b) return !v.back.empty() && !(MenuFocusable(v, v.backFocus) && f != v.backFocus) && !(v.picture && pictureOpen_);
        if (b == &PadState::menu) return v.own && !v.popup && !v.back.empty(); // (an own screen's MENU is B)
        if (b == &PadState::x) return !XAction(v, f).empty();
        if (b == &PadState::y) return !v.buttons[(int)MenuButton::Y].empty();
        if (b == &PadState::start) return !v.buttons[(int)MenuButton::Start].empty();
        if (b == &PadState::lb || b == &PadState::rb)
        {
            if (s.screen == Screen::Panel) return true; // (the panel's tabs)
            return !v.buttons[(int)(b == &PadState::lb ? MenuButton::LB : MenuButton::RB)].empty();
        }
        return false;
    }
    const std::vector<Hotspot>& h = HotspotsFor(s);
    if (b == &PadState::a) return First(h) >= 0;
    if (s.screen == Screen::Panel) return b == &PadState::lb || b == &PadState::rb; // B / MENU close the menu, no tap
    if (s.screen == Screen::Phone) return b == &PadState::b || b == &PadState::menu;
    return b == &PadState::b && CloseOf(h) >= 0;
}

const MenuAction& Navigator::XAction(const MenuView& v, int f) const
{
    if (MenuFocusable(v, f) && !v.items[f].buttonX.empty())
    {
        const ViewItem& it = v.items[f];
        return it.xAsks && armed_ != it.id ? it.xArm : it.buttonX;
    }
    return v.buttons[(int)MenuButton::X];
}

bool Navigator::Run(const MenuAction& a, const TouchScript& touch)
{
    if (a.empty() || !running_.empty() || touch.Owns() || closing_ > 0) return false;
    running_ = a;
    return true;
}

void Navigator::Step(TouchScript& touch, u32& dsKeys)
{
    // Steps in order: a tap or a drag waits for the stylus to be free (the one before it done,
    // its released frame too), keys are held their frames, a wait counts frames.
    while (!running_.empty())
    {
        MenuStep& st = running_.front();
        if (touch.Owns()) return;
        switch (st.kind)
        {
        case MenuStep::Tap:
            if (st.frames > 0) touch.Tap(st.x, st.y, st.frames);
            else touch.Tap(st.x, st.y);
            running_.erase(running_.begin());
            continue; // (the stylus is now owned: the next step waits)
        case MenuStep::Drag:
        {
            // As the cards' drag (activities.cpp): 8 steps between the two points.
            std::vector<Point> path;
            for (int i = 0; i <= 8; i++) path.push_back({st.x + (st.x2 - st.x) * i / 8, st.y + (st.y2 - st.y) * i / 8});
            touch.Drag(std::move(path));
            running_.erase(running_.begin());
            continue;
        }
        case MenuStep::Keys:
            if (st.frames-- > 0) { dsKeys |= st.keys; if (st.frames > 0) return; }
            running_.erase(running_.begin());
            return; // (one step of keys a frame: the next starts on the next frame)
        case MenuStep::Wait:
            if (st.frames-- > 1) return;
            running_.erase(running_.begin());
            return;
        }
    }
}

void Navigator::Held(const TouchScript& touch, u32& dsKeys)
{
    // As Step's keys (the stylus first: a keys step after a tap waits for it there too).
    if (running_.empty() || running_.front().kind != MenuStep::Keys || touch.Owns()) return;
    MenuStep& st = running_.front();
    if (st.frames-- > 0) { dsKeys |= st.keys; if (st.frames > 0) return; }
    running_.erase(running_.begin());
}

const MenuAction* Navigator::DirAction(const MenuView& v, int dir, bool& scroll) const
{
    scroll = false;
    const bool vertical = dir <= 1;
    const MenuAction& gameScroll = dir == 0 ? v.scrollUp : v.scrollDown;
    // A selection the game owns, the focus on it: Up / Down are its own buttons (its scroll); none
    // that way (its first entry's Up): a focus move.
    if (MenuFocusable(v, v.selected) && focus_ == v.selected && vertical) return gameScroll.empty() ? nullptr : &gameScroll;
    if (!vertical)
    {
        if (MenuFocusable(v, focus_))
        {
            const MenuAction& own = dir == 2 ? v.items[focus_].left : v.items[focus_].right;
            if (!own.empty()) return &own;
        }
        const MenuAction& side = v.buttons[(int)(dir == 2 ? MenuButton::Left : MenuButton::Right)];
        return side.empty() ? nullptr : &side;
    }
    // Past the list's end: the game's own scroll (the focus follows once the rebuilt view shows more).
    if (MenuFocusable(v, focus_) && MenuMove(v, focus_, dir) == focus_ && v.items[focus_].group == v.scrollGroup && !gameScroll.empty())
    {
        scroll = true;
        return &gameScroll;
    }
    return nullptr;
}

bool Navigator::UsesView(ScreenState s) const
{
    // The panel's menu while open; a screen of its own (Give Gift, the Save slots, the phone, a
    // popup, a note) whenever it is up.
    return view_ && view_->valid && ((s.screen == Screen::Panel && menuOpen_) || view_->own);
}

void Navigator::Move(ScreenState s, int dx, int dy)
{
    // Neighbour over the targets the focus may rest on, mapped back to HotspotsFor's indices.
    const std::vector<Hotspot>& all = HotspotsFor(s);
    std::vector<Hotspot> list;
    std::vector<int> index;
    int from = -1;
    for (int i = 0; i < (int)all.size(); i++)
    {
        if (all[i].dragOnly) continue;
        if (i == focus_) from = (int)list.size();
        list.push_back(all[i]);
        index.push_back(i);
    }
    if (from < 0) return;
    focus_ = index[Neighbour(list, from, dx, dy)];
}

// Whether a view holds an item of the module `tag` (an id's top bits).
static bool HasModule(const MenuView& v, u32 tag)
{
    for (const ViewItem& it : v.items)
        if (it.id >> 24 == tag) return true;
    return false;
}

bool Navigator::Returning() const
{
    // (Still on the Save slots or the phone too: the frame the game leaves them is drawn before the
    // navigator sees it; CONSOLE veils only the panel, Unknown and Loading, never an HD screen.)
    // Both clauses capped at kReturnVeilFrames once the HD screen is left: a veil never hides an
    // Unknown cursor screen or the world-only fallback for long.
    const bool onHd = screen_.screen == Screen::SaveSlots || screen_.screen == Screen::Phone;
    return (returnFrames_ > 0 && (onHd || veilFrames_ > 0)) ||
           (menuOpen_ && returnFocus_ > kReturnFocusFrames - kReturnVeilFrames && screen_.tab != returnTab_);
}

int Navigator::FocusFor(const MenuView& v) const
{
    if (!v.valid) return focus_;
    for (int i = 0; i < (int)v.items.size(); i++)
        if (v.items[i].id == focusId_ && v.items[i].enabled) return i;
    if (popupUp_ && !v.popup && resumeValid_) return MenuKeep(v, resumeId_, resumeIndex_);
    if (v.popup) return MenuFirst(v);
    return Arrive(v);
}

int Navigator::Arrive(const MenuView& v) const
{
    // Another screen module on the same screen (the Registry and its room info, both Unknown):
    // the item it last had the focus on, else its first.
    const u32 tag = v.items.empty() ? 0 : v.items[0].id >> 24;
    if (focusId_ != 0 && !HasModule(v, focusId_ >> 24) && tag < 32 && moduleId_[tag]) return MenuKeep(v, moduleId_[tag], MenuFirst(v));
    return MenuFirst(v);
}

void Navigator::Update(ScreenState s, const PadState& pad, TouchScript& touch, u32& dsKeys, bool settled, const MenuView* view)
{
    view_ = view;
    if (fresh_)
    {
        prev_ = pad;
        repeatDir_ = Direction(pad);
        repeatFrames_ = 0;
        fresh_ = false;
    }
    auto edge = [&](bool PadState::*b) { return pad.*b && !(prev_.*b); };

    // A popup coming (over the panel, its menu open or not; over an HD screen): what was under it
    // is remembered, to come back to when it goes.
    const bool popupNow = view && view->valid && view->popup;
    if (popupNow && !popupUp_)
    {
        resumeScreen_ = screen_;
        resumeMenu_ = menuOpen_;
        resumeValid_ = viewFocus_ && focus_ >= 0;
        resumeId_ = focusId_;
        resumeIndex_ = focus_;
    }
    if (popupNow != popupUp_)
    {
        // A popup coming or going: a multi-step action must not keep feeding keys under it, nor
        // a held press or a pending move land on the other side.
        running_.clear();
        pendingDir_ = -1;
        held_ = nullptr;
    }
    const bool back = !popupNow && popupUp_ && resumeValid_ && s == resumeScreen_;
    if (!(s == screen_))
    {
        const bool wasOpen = menuOpen_;
        // The menu stays up only while the panel changes tab, or under a popup (and again when
        // the screen it was over is back); the focus starts again (or comes back: the resume).
        if (popupNow) {}
        else if (popupUp_ && s == resumeScreen_) menuOpen_ = resumeMenu_;
        else if (s.screen != Screen::Panel || screen_.screen != Screen::Panel) menuOpen_ = false;
        // A screen of its own opened from the panel's menu (Options > Save's slots, the phone
        // button): when the panel is back (its X, a save done), the menu opens again on the tab
        // it came from (the game goes back to Interact, its menu closed), on the item it was on.
        // (The menu left for another screen: kept a moment, the way there may pass through an
        // Unknown frame.)
        if (screen_.screen == Screen::Panel && wasOpen && screen_.tab != Tab::None && s.screen != Screen::Panel)
        {
            leftTab_ = screen_.tab;
            leftId_ = focusId_;
            leftFrames_ = kLeftFrames;
        }
        if ((s.screen == Screen::SaveSlots || s.screen == Screen::Phone) && leftFrames_ > 0)
        {
            returnTab_ = leftTab_;
            returnId_ = leftId_;
            returnFrames_ = kReturnFrames;
            leftFrames_ = 0;
        }
        else if (s.screen == Screen::Panel && s.tab != Tab::None && returnFrames_ > 0)
        {
            menuOpen_ = true;
            returnTap_ = s.tab != returnTab_;
            returnFrames_ = 0;
            returnFocus_ = kReturnFocusFrames;
        }
        else if (s.screen != Screen::Panel && s.screen != Screen::SaveSlots && s.screen != Screen::Phone && s.screen != Screen::Popup &&
                 s.screen != Screen::Unknown && s.screen != Screen::Loading)
            returnFrames_ = returnFocus_ = 0, returnTap_ = false;
        // (Leaving the Save slots or the phone on the way back: the veil's frames start.)
        if ((screen_.screen == Screen::SaveSlots || screen_.screen == Screen::Phone) && s.screen != Screen::SaveSlots && s.screen != Screen::Phone && returnFrames_ > 0)
            veilFrames_ = kReturnVeilFrames;
        focus_ = -1;
        closing_ = 0;
        screen_ = s;
        {
            // (MooGoo's B on its intro goes on through the black load to the game's X.)
            const bool through = s.screen == Screen::MooGoo || s.screen == Screen::Loading || s.screen == Screen::None || s.screen == Screen::Unknown;
            const int chain = through ? activity_.quitChain : 0;
            activity_ = ActivityState();
            activity_.quitChain = chain;
        }
        held_ = nullptr; // a held press was meant for the screen that went
        running_.clear(); // (and an action running)
        pendingDir_ = -1;
        viewHash_ = 0;
        viewPage_ = 0;
        viewQuestion_ = false;
        pageKeptId_ = 0;
        pageKeptIndex_ = -1;
        pictureOpen_ = false;
        ClearQueue();
        armed_ = 0; // (an armed X belongs to the screen or tab it was pressed on)
        for (u32& id : moduleId_) id = 0; // (the modules' last focus belongs to their screen)
    }
    else if (closing_ > 0) closing_--;
    if (!popupNow && popupUp_) closing_ = 0; // (the popup went: its closing wait is over)
    if (back) { restore_ = true; restoreFrames_ = 0; }
    else if (popupNow) restore_ = false;

    // The gate: on a focus screen or a popup, a press that would tap waits while the game's
    // bottom slot is switching (the RAM leads the pixels) and is replayed on the first settled
    // frame; the latest one wins, and after kHoldFrames it is dropped. Focus moves and the DS's
    // own buttons never wait.
    const Mode mode = ModeWith(s, menuOpen_, view_);
    // The HD menu: rebuilt this frame; the focus follows its item's id when the content changed.
    const bool useView = mode == Mode::Focus && UsesView(s);
    const u64 viewHash = useView ? view_->Hash() : 0;
    const bool viewChanged = useView && viewHash != viewHash_;
    if (useView && viewChanged && viewHash_ != 0 && focus_ >= 0)
    {
        // Another page of the screen (Build Rooms' question), or another screen module on the same
        // screen (the Registry's room info), no popup coming or going: the focus starts again there
        // (below: the page's first item; the module's last item, else its first).
        if (!popupNow && !popupUp_ && view_->page != viewPage_)
        {
            if (!viewQuestion_) { pageKeptId_ = focusId_; pageKeptIndex_ = focus_; }
            focus_ = -1;
            // Back on the item it was left on; that item gone (Build Rooms' room just bought), the
            // one in its place, never the list's top.
            if (!view_->question && pageKeptId_) focus_ = MenuKeep(*view_, pageKeptId_, pageKeptIndex_);
        }
        else if (!popupNow && !popupUp_ && !view_->items.empty() && !HasModule(*view_, focusId_ >> 24)) focus_ = -1;
        else focus_ = MenuKeep(*view_, focusId_, focus_);
    }
    if (!useView && viewFocus_) focus_ = -1; // (an index of the view means nothing to HotspotsFor)
    // A popup coming: its own first focus (MenuView::initial: a question's No), never the index
    // the screen beneath had, nor an answer a popup before it left the focus on.
    if (popupNow && !popupUp_ && useView)
    {
        focus_ = -1;
        if (!view_->items.empty() && (view_->items[0].id >> 24) < 32) moduleId_[view_->items[0].id >> 24] = 0;
    }
    // The resume: the item the focus was on before the popup, once its view is back.
    if (restore_ && useView)
    {
        const int f = MenuKeep(*view_, resumeId_, resumeIndex_);
        if (f >= 0) focus_ = f;
        restore_ = false;
    }
    else if (restore_ && ++restoreFrames_ > kHoldFrames) restore_ = false;
    bool PadState::*replay = nullptr;
    if (held_ && ++heldFrames_ > kHoldFrames) held_ = nullptr;
    if (mode == Mode::Focus || mode == Mode::Popup)
    {
        static bool PadState::*const kTapButtons[] = {&PadState::a, &PadState::b, &PadState::menu, &PadState::lb, &PadState::rb, &PadState::x, &PadState::y, &PadState::start};
        if (!settled)
        {
            for (bool PadState::*b : kTapButtons)
                if (edge(b) && WouldTap(s, b)) { held_ = b; heldFrames_ = 0; heldHash_ = viewHash; }
        }
        else if (held_)
        {
            // A press made on a view whose content has changed since is stale: dropped.
            if (heldHash_ == viewHash && (!useView || view_->replay)) replay = held_;
            held_ = nullptr;
        }
    }
    else held_ = nullptr; // a screen that does not tap (or no longer): nothing to replay
    auto pressed = [&](bool PadState::*b)
    {
        if (b == replay) return true;
        if (!settled && (mode == Mode::Focus || mode == Mode::Popup) && WouldTap(s, b)) return false;
        return edge(b);
    };

    const std::vector<Hotspot>& h = HotspotsFor(s);
    cursorScreen_ = h.empty() && !useView && !OwnScheme(s.screen); // (the HD Save slots, the minigames with a scheme of their own: no cursor)
    cursorPress_ = blow_ = voice_ = false;
    handDx_ = handDy_ = 0;
    // A tap only when nothing else touches: a press while a tap or a drag runs is dropped.
    const bool canTap = !touch.Owns() && closing_ == 0;
    // Back from the Save slots or the phone: the tab it came from, tapped once the panel is settled.
    if (returnFrames_ > 0) returnFrames_--;
    if (veilFrames_ > 0) veilFrames_--;
    if (leftFrames_ > 0) leftFrames_--;
    if (returnFocus_ > 0) returnFocus_--;
    if (returnTap_ && s.screen == Screen::Panel && settled && canTap && running_.empty())
    {
        touch.Tap(kTabIcon[(int)returnTab_ - (int)Tab::Interact].x, kTabIcon[(int)returnTab_ - (int)Tab::Interact].y);
        returnTap_ = false;
    }
    auto tap = [&](const Hotspot& t) { if (canTap) touch.Tap(t.x, t.y); return canTap; };

    // The focus moves on a newly held direction, then repeats while it is held.
    bool move = false;
    const int dir = Direction(pad);
    if (dir != repeatDir_) { repeatDir_ = dir; repeatFrames_ = 0; move = dir >= 0; }
    else if (dir >= 0 && ++repeatFrames_ >= kRepeatAfter && (repeatFrames_ - kRepeatAfter) % kRepeatEvery == 0) move = true;

    switch (mode)
    {
    case Mode::Pass:
        dsKeys |= DsKeys(pad, true);
        if (s.screen == Screen::Panel && pressed(&PadState::menu)) menuOpen_ = true;
        break;
    case Mode::Focus: // (the taps here are listed again in WouldTap, for the gate)
    {
        if (useView)
        {
            // The HD menu (port/menuview.h): the focus over the view's enabled items; a selection
            // the game owns holds it.
            const MenuView& v = *view_;
            // (held while the focus is in the list: the phone above it keeps the focus)
            if (MenuFocusable(v, v.selected) && (!MenuFocusable(v, focus_) || v.items[focus_].group == v.scrollGroup)) focus_ = v.selected;
            if (!MenuFocusable(v, focus_)) focus_ = Arrive(v);
            // Back on the tab a screen of its own was opened from: the item it was opened by.
            if (returnFocus_ > 0 && s.screen == Screen::Panel && s.tab == returnTab_ && !returnTap_)
            {
                if (const int f = MenuKeep(v, returnId_, focus_); f >= 0) focus_ = f;
                returnFocus_ = 0;
            }
            // A move that waited for the list's own scroll: once the rebuilt view shows more.
            if (pendingDir_ >= 0 && (viewChanged || ++pendingFrames_ > kHoldFrames))
            {
                if (viewChanged && focus_ >= 0 && !MenuFocusable(v, v.selected)) focus_ = MenuMove(v, focus_, pendingDir_);
                pendingDir_ = -1;
            }
            // A direction: an action (the item's Left / Right, the view's, the game's scroll) is a
            // tap like A's, held by the gate while not settled and replayed (or dropped when the
            // view changed); otherwise a focus move, never held.
            static bool PadState::*const kDirs[4] = {&PadState::up, &PadState::down, &PadState::left, &PadState::right};
            int act = -1;
            for (int d = 0; d < 4; d++)
                if (replay == kDirs[d]) act = d;
            // A slider (ViewItem::queues): its Left / Right owed, not dropped while a step runs.
            if (move && act < 0 && (dir == 2 || dir == 3) && MenuFocusable(v, focus_) && v.items[focus_].queues)
            {
                const ViewItem& it = v.items[focus_];
                const bool busy = queueId_ == it.id && (!queue_.empty() || queueWaiting_);
                if (busy || !(dir == 2 ? it.left : it.right).empty())
                {
                    if (queueId_ != it.id) { ClearQueue(); queueId_ = it.id; }
                    if (queue_.size() < 16) queue_.push_back(dir == 2 ? -1 : 1);
                    queueFrames_ = 0;
                    move = false;
                }
            }
            if (queueId_ != 0)
            {
                // (by id: the focus may have moved on; the owed steps still run)
                int qi = -1;
                for (int i = 0; i < (int)v.items.size(); i++)
                    if (v.items[i].id == queueId_ && v.items[i].enabled) qi = i;
                if (qi < 0) ClearQueue();
                else
                {
                    const ViewItem& it = v.items[qi];
                    ++queueFrames_;
                    // The game took the last step: its Left / Right rebuilt from the new value
                    // (or it never moves: an end, after kQueueRead frames).
                    if (queueWaiting_ && (it.left != queueLeft_ || it.right != queueRight_ || queueFrames_ > kQueueRead))
                    {
                        queueWaiting_ = false;
                        queueFrames_ = 0;
                    }
                    if (queueFrames_ > kHoldFrames || (!queueWaiting_ && queue_.empty())) ClearQueue();
                    else if (!queueWaiting_ && settled)
                    {
                        const MenuAction& a = queue_.front() < 0 ? it.left : it.right;
                        if (a.empty()) queue_.erase(queue_.begin()); // (past an end: that press does nothing)
                        else if (Run(a, touch))
                        {
                            queueLeft_ = it.left;
                            queueRight_ = it.right;
                            queueWaiting_ = true;
                            queue_.erase(queue_.begin());
                            queueFrames_ = 0;
                        }
                    }
                }
            }
            if (move && act < 0)
            {
                bool scroll = false;
                if (DirAction(v, dir, scroll))
                {
                    if (settled) act = dir;
                    else { held_ = kDirs[dir]; heldFrames_ = 0; heldHash_ = viewHash; }
                }
                else if (focus_ >= 0) focus_ = MenuMove(v, focus_, dir);
            }
            if (act >= 0)
            {
                bool scroll = false;
                if (const MenuAction* a = DirAction(v, act, scroll); a && Run(*a, touch) && scroll)
                {
                    pendingDir_ = act;
                    pendingFrames_ = 0;
                }
            }
            if (pressed(&PadState::a) && MenuFocusable(v, focus_) && Run(v.items[focus_].action, touch))
            {
                pictureOpen_ = true; // (the picture it shows, if any: big)
                if (v.items[focus_].closes) closing_ = kClosingFrames;
                if (v.items[focus_].closesMenu && !v.own) menuOpen_ = false;
            }
            if (pressed(&PadState::x))
            {
                // An asking X (Pockets' bin): the first press arms the item (and runs its xArm),
                // the second performs it.
                const bool asks = MenuFocusable(v, focus_) && v.items[focus_].xAsks && !v.items[focus_].buttonX.empty();
                const MenuAction& a = XAction(v, focus_);
                if (asks && armed_ != v.items[focus_].id) { if (a.empty() || Run(a, touch)) armed_ = v.items[focus_].id; }
                else if (Run(a, touch) && asks) armed_ = 0;
            }
            if (pressed(&PadState::y)) Run(v.buttons[(int)MenuButton::Y], touch);
            // Start: the view's own (the front end's touch); else nothing (or passed on, below).
            if (!v.buttons[(int)MenuButton::Start].empty() && pressed(&PadState::start)) Run(v.buttons[(int)MenuButton::Start], touch);
            // B, MENU, LB or RB leave the slider (another page, tab or screen): a step still
            // queued for it must not run after (its drag would land on what comes next).
            if (edge(&PadState::b) || edge(&PadState::menu) || edge(&PadState::lb) || edge(&PadState::rb)) ClearQueue();
            const int t = s.tab == Tab::None ? -1 : (int)s.tab - (int)Tab::Interact;
            if (s.screen == Screen::Panel && pressed(&PadState::rb)) tap(kTabIcon[(t + 1) % 5]);
            else if (s.screen == Screen::Panel && pressed(&PadState::lb)) tap(kTabIcon[t < 0 ? 4 : (t + 4) % 5]);
            else if (s.screen != Screen::Panel && pressed(&PadState::rb)) { Run(v.buttons[(int)MenuButton::RB], touch); pictureOpen_ = false; }
            else if (s.screen != Screen::Panel && pressed(&PadState::lb)) { Run(v.buttons[(int)MenuButton::LB], touch); pictureOpen_ = false; }
            const bool back = pressed(&PadState::b), menu = pressed(&PadState::menu);
            // MENU does what B does on a screen of its own (not a popup: its question is B's)
            // (unless the view gives MENU its own action: the phone's X from any page)
            const bool ownMenu = menu && v.own && !v.popup && !v.menu.empty();
            const bool backKey = back || (menu && v.own && !v.popup && v.menu.empty());
            // A big picture first: B closes it. B's safe landing (Keep this Sim?'s cross): one B
            // never discards.
            if (ownMenu) { if (Run(v.menu, touch) && v.menuCloses) closing_ = kClosingFrames; }
            else if (backKey && v.picture && pictureOpen_) pictureOpen_ = false;
            else if (backKey && MenuFocusable(v, v.backFocus) && focus_ != v.backFocus) focus_ = v.backFocus;
            else if (backKey && !v.back.empty()) { if (Run(v.back, touch) && v.backCloses) closing_ = kClosingFrames; }
            else if ((back || menu) && !v.own) menuOpen_ = false;
            // DS buttons the view passes on (Game Paused's Start).
            if (v.passKeys) dsKeys |= DsKeys(pad, true) & v.passKeys;
            // The right stick turns what the game turns on L / R (Create-a-Sim's model).
            if (v.stickTurns && pad.rx < -kStick) dsKeys |= 1u << kL;
            else if (v.stickTurns && pad.rx > kStick) dsKeys |= 1u << kR;
            // X / Y the same (the keyboard's way: no right stick).
            if (v.xyTurns && pad.x) dsKeys |= 1u << kL;
            if (v.xyTurns && pad.y) dsKeys |= 1u << kR;
            break;
        }
        if (focus_ < 0 || focus_ >= (int)h.size()) focus_ = First(h);
        if (move && focus_ >= 0)
        {
            static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
            Move(s, dx[dir], dy[dir]);
        }
        if (pressed(&PadState::a) && focus_ >= 0) tap(h[focus_]);
        const bool back = pressed(&PadState::b), menu = pressed(&PadState::menu);
        if (s.screen == Screen::Panel)
        {
            // The tabs in a ring: Interact … Options.
            const int t = s.tab == Tab::None ? -1 : (int)s.tab - (int)Tab::Interact;
            if (pressed(&PadState::rb)) tap(kTabIcon[(t + 1) % 5]);
            else if (pressed(&PadState::lb)) tap(kTabIcon[t < 0 ? 4 : (t + 4) % 5]);
            if (back || menu) menuOpen_ = false;
        }
        else if (s.screen == Screen::Phone)
        {
            if ((back || menu) && tap(kPhoneClose)) { menuOpen_ = false; closing_ = kClosingFrames; }
        }
        else if (back)
        {
            const int c = CloseOf(h);
            if (c >= 0 && tap(h[c])) closing_ = kClosingFrames;
        }
        break;
    }
    case Mode::Popup:
        if ((pressed(&PadState::a) || pressed(&PadState::b)) && tap(kPopupClose)) closing_ = kClosingFrames;
        if (pad.start) dsKeys |= 1u << kStart; // Game Paused also resumes on Start
        break;
    case Mode::Cursor:
        dsKeys |= DsKeys(pad, !pad.cursor);
        cursorPress_ = pad.cursor && pad.a;
        // The hotel's furniture editor (the game reads no key there): B and MENU tap its X.
        if (activityRam_.placement)
        {
            dsKeys &= ~(1u << kB);
            if (pressed(&PadState::b) || pressed(&PadState::menu)) tap({"close", kPlacementX.x, kPlacementX.y, {}});
        }
        break;
    case Mode::Activity:
        if ((s.screen == Screen::Cards || s.screen == Screen::MixerSave) && focus_ >= 0 && focus_ < (int)h.size() && move)
        {
            static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
            Move(s, dx[dir], dy[dir]);
        }
        activity_.prev = prev_;
        activity_.focus = focus_;
        activity_.ram = activityRam_;
        ActivityInput(s.screen, pad, touch, dsKeys, activity_);
        focus_ = activity_.focus;
        // The vacuum's hand is the stick cursor: A held presses it.
        cursorPress_ = activity_.grab;
        blow_ = activity_.blow;
        voice_ = activity_.voice;
        handDx_ = activity_.handDx;
        handDy_ = activity_.handDy;
        break;
    }

    // The view's action runs on (taps queued as the stylus frees, keys held).
    Step(touch, dsKeys);

    // Where the focus shows: on the targets of a focus screen and on a popup's X.
    const Mode after = ModeWith(s, menuOpen_, view_); // (B may have closed the menu)
    viewFocus_ = after == Mode::Focus && UsesView(s);
    switch (after)
    {
    case Mode::Focus: case Mode::Popup:
        if (viewFocus_)
        {
            if (!MenuFocusable(*view_, focus_)) focus_ = Arrive(*view_);
            focusId_ = focus_ >= 0 ? view_->items[focus_].id : 0;
            if ((focusId_ >> 24) < 32) moduleId_[focusId_ >> 24] = focusId_;
            viewHash_ = view_->Hash();
            viewPage_ = view_->page;
            viewQuestion_ = view_->question;
        }
        else if (focus_ < 0 || focus_ >= (int)h.size()) focus_ = First(h);
        break;
    case Mode::Activity: break; // (ActivityInput sets it: -1 in the vacuum)
    default: focus_ = -1; break;
    }
    // An armed X lasts while its item keeps the focus on the same HD view (a move, a tab change,
    // the item going, the menu closing: disarmed).
    if (!viewFocus_ || focusId_ != armed_) armed_ = 0;
    popupUp_ = popupNow;
    prev_ = pad;
    view_ = nullptr; // (the caller's view lives for this call only)
}

void Navigator::Leave(TouchScript& touch)
{
    touch.Cancel();
    menuOpen_ = cursorPress_ = cursorScreen_ = blow_ = voice_ = false;
    handDx_ = handDy_ = 0;
    activity_ = ActivityState();
    focus_ = -1;
    closing_ = 0;
    held_ = nullptr;
    running_.clear();
    pendingDir_ = -1;
    view_ = nullptr;
    viewFocus_ = false;
    viewHash_ = 0;
    viewPage_ = 0;
    viewQuestion_ = false;
    pageKeptId_ = 0;
    pageKeptIndex_ = -1;
    pictureOpen_ = false;
    returnFrames_ = returnFocus_ = leftFrames_ = veilFrames_ = 0;
    returnTap_ = false;
    ClearQueue();
    armed_ = 0;
    popupUp_ = resumeMenu_ = resumeValid_ = restore_ = false;
    screen_ = ScreenState();
    fresh_ = true;
}
}
