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
    case Screen::Social: case Screen::Cards: case Screen::Vacuum: return Mode::Activity;
    case Screen::Popup: return Mode::Popup;
    case Screen::Unknown: return Mode::Cursor;
    case Screen::ItemHand: case Screen::SaveSlots: return Mode::Cursor; // no table yet: as Unknown
    case Screen::None: case Screen::Dialogue: case Screen::FrontEnd: case Screen::CreateASim: case Screen::Loading: return Mode::Pass;
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

// Mirrors the taps of Update's Mode::Focus and Mode::Popup cases: change one, change the other.
bool Navigator::WouldTap(ScreenState s, bool PadState::*b) const
{
    const Mode mode = ModeOf(s.screen, menuOpen_);
    if (mode == Mode::Popup) return b == &PadState::a || b == &PadState::b;
    if (mode != Mode::Focus) return false;
    const std::vector<Hotspot>& h = HotspotsFor(s);
    if (b == &PadState::a) return First(h) >= 0;
    if (s.screen == Screen::Panel) return b == &PadState::lb || b == &PadState::rb; // B / MENU close the menu, no tap
    if (s.screen == Screen::Phone) return b == &PadState::b || b == &PadState::menu;
    return b == &PadState::b && CloseOf(h) >= 0;
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

void Navigator::Update(ScreenState s, const PadState& pad, TouchScript& touch, u32& dsKeys, bool settled)
{
    if (fresh_)
    {
        prev_ = pad;
        repeatDir_ = Direction(pad);
        repeatFrames_ = 0;
        fresh_ = false;
    }
    auto edge = [&](bool PadState::*b) { return pad.*b && !(prev_.*b); };

    if (!(s == screen_))
    {
        // The menu stays up only while the panel changes tab; the focus starts again.
        if (s.screen != Screen::Panel || screen_.screen != Screen::Panel) menuOpen_ = false;
        focus_ = -1;
        closing_ = 0;
        screen_ = s;
        activity_ = ActivityState();
        held_ = nullptr; // a held press was meant for the screen that went
    }
    else if (closing_ > 0) closing_--;

    // The gate: on a focus screen or a popup, a press that would tap waits while the game's
    // bottom slot is switching (the RAM leads the pixels) and is replayed on the first settled
    // frame; the latest one wins, and after kHoldFrames it is dropped. Focus moves and the DS's
    // own buttons never wait.
    const Mode mode = ModeOf(s.screen, menuOpen_);
    bool PadState::*replay = nullptr;
    if (held_ && ++heldFrames_ > kHoldFrames) held_ = nullptr;
    if (mode == Mode::Focus || mode == Mode::Popup)
    {
        static bool PadState::*const kTapButtons[] = {&PadState::a, &PadState::b, &PadState::menu, &PadState::lb, &PadState::rb};
        if (!settled)
        {
            for (bool PadState::*b : kTapButtons)
                if (edge(b) && WouldTap(s, b)) { held_ = b; heldFrames_ = 0; }
        }
        else if (held_) { replay = held_; held_ = nullptr; }
    }
    else held_ = nullptr; // a screen that does not tap (or no longer): nothing to replay
    auto pressed = [&](bool PadState::*b)
    {
        if (b == replay) return true;
        if (!settled && (mode == Mode::Focus || mode == Mode::Popup) && WouldTap(s, b)) return false;
        return edge(b);
    };

    const std::vector<Hotspot>& h = HotspotsFor(s);
    cursorScreen_ = h.empty();
    cursorPress_ = blow_ = false;
    handDx_ = handDy_ = 0;
    // A tap only when nothing else touches: a press while a tap or a drag runs is dropped.
    const bool canTap = !touch.Owns() && closing_ == 0;
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
        break;
    case Mode::Activity:
        if (s.screen == Screen::Cards && focus_ >= 0 && focus_ < (int)h.size() && move)
        {
            static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
            Move(s, dx[dir], dy[dir]);
        }
        activity_.prev = prev_;
        activity_.focus = focus_;
        ActivityInput(s.screen, pad, touch, dsKeys, activity_);
        focus_ = activity_.focus;
        // The vacuum's hand is the stick cursor: A held presses it.
        cursorPress_ = activity_.grab;
        blow_ = activity_.blow;
        handDx_ = activity_.handDx;
        handDy_ = activity_.handDy;
        break;
    }

    // Where the focus shows: on the targets of a focus screen and on a popup's X.
    switch (ModeOf(s.screen, menuOpen_))
    {
    case Mode::Focus: case Mode::Popup:
        if (focus_ < 0 || focus_ >= (int)h.size()) focus_ = First(h);
        break;
    case Mode::Activity: break; // (ActivityInput sets it: -1 in the vacuum)
    default: focus_ = -1; break;
    }
    prev_ = pad;
}

void Navigator::Leave(TouchScript& touch)
{
    touch.Cancel();
    menuOpen_ = cursorPress_ = cursorScreen_ = blow_ = false;
    handDx_ = handDy_ = 0;
    activity_ = ActivityState();
    focus_ = -1;
    closing_ = 0;
    held_ = nullptr;
    screen_ = ScreenState();
    fresh_ = true;
}
}
