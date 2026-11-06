#include "activities.h"

#include <cstdlib>
#include <cstring>

#include "hotspots.h"

namespace sims2wide
{
namespace
{
constexpr float kStick = 0.5f;

bool Up(const PadState& p) { return p.up || p.ly < -kStick; }
bool Left(const PadState& p) { return p.left || p.lx < -kStick; }
bool Right(const PadState& p) { return p.right || p.lx > kStick; }

// The target named `name` in h, or -1.
int Named(const std::vector<Hotspot>& h, const char* name)
{
    for (int i = 0; i < (int)h.size(); i++)
        if (!strcmp(h[i].name, name)) return i;
    return -1;
}

// The hand's place (0-9) a target is, or -1 ("card 1" ... "card 10").
int CardOf(const Hotspot& h) { return !strncmp(h.name, "card ", 5) ? atoi(h.name + 5) - 1 : -1; }

// A drag from a to b: the stylus rests on a for 4 frames (the game picks the card up), then
// moves in 8 steps of 2 frames and rests on b for 4 (the game drops it there).
void DragTo(TouchScript& touch, Point a, Point b)
{
    std::vector<Point> path {a, a};
    for (int i = 1; i <= 8; i++) path.push_back({a.x + (b.x - a.x) * i / 8, a.y + (b.y - a.y) * i / 8});
    touch.Drag(path, 2, 4);
}

void Social(const PadState& pad, TouchScript& touch, ActivityState& st, bool canTap)
{
    const std::vector<Hotspot>& h = HotspotsFor({Screen::Social, Tab::None});
    if (st.focus < 0 || st.focus >= (int)h.size()) st.focus = 0;
    if (!canTap) return;
    int pick = -1;
    if (Left(pad) && !Left(st.prev)) pick = 0;
    else if (Up(pad) && !Up(st.prev)) pick = 1;
    else if (Right(pad) && !Right(st.prev)) pick = 2;
    else if (pad.a && !st.prev.a) pick = st.focus;
    if (pick < 0) return;
    st.focus = pick;
    if (canTap) touch.Tap(h[pick].x, h[pick].y);
}

void Cards(const PadState& pad, TouchScript& touch, ActivityState& st, bool canTap)
{
    const std::vector<Hotspot>& h = HotspotsFor({Screen::Cards, Tab::None});
    if (st.focus < 0 || st.focus >= (int)h.size() || h[st.focus].dragOnly) st.focus = Named(h, "card 1");
    if (!canTap) return; // a press while a touch runs is dropped
    auto pressed = [&](bool PadState::*b) { return pad.*b && !(st.prev.*b); };
    const Hotspot& f = h[st.focus];
    const int card = CardOf(f);
    auto clear = [&] { for (bool& l : st.lifted) l = false; };
    auto quit = [&] {
        const Hotspot& q = h[Named(h, "quit")];
        touch.Tap(q.x, q.y);
        st.confirm = true;
    };
    // The question opened by the mouse: RAM knows (it still reads Cards).
    if (st.ram.cards && st.ram.quitAsk) st.confirm = true;
    if (!st.confirm && st.ram.cards && st.ram.message)
    {
        // The end of a turn: a touch anywhere goes on (the other buttons wait).
        if (pressed(&PadState::a)) touch.Tap(kCardsMessage.x, kCardsMessage.y);
        return;
    }
    if (st.confirm)
    {
        // The question: A yes, B no (the other buttons wait for the answer).
        if (pressed(&PadState::a)) { touch.Tap(kCardsQuitYes.x, kCardsQuitYes.y); st.confirm = false; }
        else if (pressed(&PadState::b)) { touch.Tap(kCardsQuitNo.x, kCardsQuitNo.y); st.confirm = false; }
        return;
    }
    if (pressed(&PadState::a))
    {
        if (card < 0)
        {
            if (!strcmp(f.name, "quit")) quit();
            else touch.Tap(f.x, f.y);
        }
        else if (st.lifted[card])
        {
            // Played: the lifted cards go to the stairs (or snap back, one alone); none stays up.
            const Hotspot& play = h[Named(h, "play")];
            DragTo(touch, {f.x, f.y}, {play.x, play.y});
            clear();
        }
        else
        {
            touch.Tap(f.x, f.y);
            st.lifted[card] = true;
        }
    }
    else if (pressed(&PadState::x))
    {
        const Hotspot& deck = h[Named(h, "deck")];
        touch.Tap(deck.x, deck.y);
    }
    else if (pressed(&PadState::y) && card >= 0)
    {
        const Hotspot& discard = h[Named(h, "discard")];
        DragTo(touch, {f.x, f.y}, {discard.x, discard.y});
        clear();
    }
    else if (pressed(&PadState::b)) quit();
}

void Vacuum(const PadState& pad, u32& dsKeys, ActivityState& st)
{
    // A tap on Y is 4-6 frames and the game wants 7: a press holds the blow for 8.
    if (pad.y && !st.prev.y) st.blowFrames = 8;
    st.blow = pad.y || st.blowFrames > 0;
    if (st.blowFrames > 0) st.blowFrames--;
    st.grab = pad.a;
    // The D-pad moves the hand while X or A is held, and drives otherwise; the stick always drives.
    const bool hand = pad.x || pad.a;
    if (hand)
    {
        st.handDx = kHandStep * ((pad.right ? 1 : 0) - (pad.left ? 1 : 0));
        st.handDy = kHandStep * ((pad.down ? 1 : 0) - (pad.up ? 1 : 0));
    }
    auto set = [&](bool on, int key) { if (on) dsKeys |= 1u << key; };
    set((pad.up && !hand) || pad.ly < -kStick, kUp);
    set((pad.down && !hand) || pad.ly > kStick, kDown);
    set((pad.left && !hand) || pad.lx < -kStick, kLeft);
    set((pad.right && !hand) || pad.lx > kStick, kRight);
    set(pad.b, kB);
    set(pad.lb, kL);
    set(pad.rb, kR);
}
}

bool IsActivity(Screen s) { return s == Screen::Social || s == Screen::Cards || s == Screen::Vacuum; }

void ActivityInput(Screen s, const PadState& pad, TouchScript& touch, u32& dsKeys, ActivityState& st)
{
    st.blow = st.grab = false;
    st.handDx = st.handDy = 0;
    if (pad.start) dsKeys |= 1u << kStart;
    if (pad.select) dsKeys |= 1u << kSelect;
    const bool canTap = !touch.Owns();
    switch (s)
    {
    case Screen::Social: Social(pad, touch, st, canTap); break;
    case Screen::Cards: Cards(pad, touch, st, canTap); break;
    case Screen::Vacuum: Vacuum(pad, dsKeys, st); st.focus = -1; break;
    default: break;
    }
}

std::vector<Hint> ActivityHints(Screen s)
{
    switch (s)
    {
    case Screen::Social:
        return {{"LEFT", "LEFT ICON"}, {"UP", "TOP ICON"}, {"RIGHT", "RIGHT ICON"}, {nullptr, "MATCH THE GESTURE"}};
    case Screen::Cards:
        return {{"D-PAD", "CHOOSE A CARD"}, {"A", "PICK UP, AGAIN\nTO PLAY"}, {"X", "DRAW A CARD"}, {"Y", "DISCARD"}, {"B", "QUIT"}};
    case Screen::Vacuum:
        return {{"L-STICK", "DRIVE"}, {"R-STICK", "MOVE THE HAND"}, {"X+D-PAD", "MOVE THE HAND"},
                {"A", "GRAB, DRAG TO\nTHE BOX"}, {"Y", "BLOW: UNCLOG"}};
    default: return {};
    }
}
}
