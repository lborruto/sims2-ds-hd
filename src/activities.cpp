#include "activities.h"

#include <algorithm>
#include <cmath>
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

bool Pressed(const PadState& pad, const ActivityState& st, bool PadState::*b) { return pad.*b && !(st.prev.*b); }

// The stylus at the pointer while A is held (not when a tap runs, nor after a glide until A is let go).
void PressPointer(const PadState& pad, ActivityState& st, bool canTap)
{
    if (!pad.a) st.latch = false;
    st.down = pad.a && !st.latch && canTap;
}

void Autopsy(const PadState& pad, TouchScript& touch, u32& dsKeys, ActivityState& st, bool canTap)
{
    st.pointer = true;
    // Quitting: the game's B held a few frames (it quits at once and forfeits the earnings).
    if (st.quitKeys > 0)
    {
        dsKeys |= 1u << kB;
        st.quitKeys--;
        return;
    }
    if (st.quitArmed > 0) st.quitArmed--;
    if (Pressed(pad, st, &PadState::b) || Pressed(pad, st, &PadState::start))
    {
        if (st.quitArmed > 0) { st.quitKeys = 4; st.quitArmed = 0; return; }
        st.quitArmed = kQuitWindow;
    }
    else if (Pressed(pad, st, &PadState::a) || Pressed(pad, st, &PadState::x) || Pressed(pad, st, &PadState::y) ||
             Pressed(pad, st, &PadState::lb) || Pressed(pad, st, &PadState::rb))
        st.quitArmed = 0; // (any other press: not quitting)
    // LB: the organ or the parasite held carried to the tray / the flask, then let go.
    if (st.gliding)
    {
        const float dx = st.glideTo.x - st.px, dy = st.glideTo.y - st.py, d = std::sqrt(dx * dx + dy * dy);
        if (d > kPtrDrag) { st.px += dx * kPtrDrag / d; st.py += dy * kPtrDrag / d; }
        else { st.px = (float)st.glideTo.x; st.py = (float)st.glideTo.y; st.glideRest++; }
        st.down = st.glideRest < 4;
        if (!st.down) { st.gliding = false; st.latch = true; }
        return;
    }
    const bool laser = st.ram.autopsy && st.ram.autopsyTool == 0;
    MovePointer(pad, st, pad.a ? (laser ? kPtrLaser : kPtrDrag) : kPtrMax, true);
    PressPointer(pad, st, canTap);
    if (pad.a && Pressed(pad, st, &PadState::lb) && st.ram.autopsy && (st.ram.autopsySub == 3 || st.ram.autopsySub == 6))
    {
        st.gliding = true;
        st.glideRest = 0;
        st.glideTo = st.ram.autopsySub == 3 ? kAutopsyTray : kAutopsyFlask;
        st.down = true;
        return;
    }
    if (pad.a || !canTap) return; // (the tools while the stylus is free)
    int tool = -1;
    if (Pressed(pad, st, &PadState::x)) tool = 0;
    else if (Pressed(pad, st, &PadState::y)) tool = 1;
    else if (Pressed(pad, st, &PadState::rb)) tool = 2;
    if (tool >= 0) touch.Tap(kAutopsyTool[tool].x, kAutopsyTool[tool].y);
}

void Detector(const PadState& pad, TouchScript& touch, u32& dsKeys, ActivityState& st, bool canTap)
{
    // The hero walks on the game's own D-pad (its controller reads it): the left stick too.
    auto set = [&](bool on, int key) { if (on) dsKeys |= 1u << key; };
    set(pad.up || pad.ly < -kStick, kUp);
    set(pad.down || pad.ly > kStick, kDown);
    set(pad.left || pad.lx < -kStick, kLeft);
    set(pad.right || pad.lx > kStick, kRight);
    // The depth slider: its knob held and moved while LB / RB or the right stick say so.
    float v = kKnobStep * ((pad.rb ? 1 : 0) - (pad.lb ? 1 : 0));
    if (std::fabs(pad.ry) > kPtrDead) v += kKnobStick * pad.ry;
    if (v != 0 && (canTap || st.sliderFrames > 0))
    {
        if (st.sliderFrames == 0)
        {
            st.px = (float)kDetectorKnobX;
            st.py = kDetectorKnobTop + (float)kDetectorKnobSpan * std::clamp(st.ram.detectorSlider, 0, 4096) / 4096.0f;
        }
        else if (st.sliderFrames >= 2) // (the press lands on the knob first)
            st.py = std::clamp(st.py + v, (float)kDetectorKnobTop, (float)(kDetectorKnobTop + kDetectorKnobSpan));
        st.sliderFrames++;
        st.down = true;
        return;
    }
    st.sliderFrames = 0;
    if (!canTap) return;
    if (Pressed(pad, st, &PadState::a)) touch.Tap(kDetectorDig.x, kDetectorDig.y);
    else if (Pressed(pad, st, &PadState::b)) touch.Tap(kDetectorPower.x, kDetectorPower.y);
}

void Art(const PadState& pad, TouchScript& touch, ActivityState& st, bool canTap)
{
    st.pointer = true;
    if (st.ram.ask)
    {
        // "Are you done painting?": A its tick, B its cross; the rest waits.
        if (!canTap) return;
        if (Pressed(pad, st, &PadState::a)) { touch.Tap(st.ram.askYes.x, st.ram.askYes.y); st.latch = true; }
        else if (Pressed(pad, st, &PadState::b)) { touch.Tap(st.ram.askNo.x, st.ram.askNo.y); st.latch = true; }
        return;
    }
    MovePointer(pad, st, pad.a ? kPtrDrag : kPtrMax, true);
    PressPointer(pad, st, canTap);
    if (pad.a || !canTap) return;
    const int button = std::clamp(st.ram.artButton, 0, kArtTools - 1);
    if (Pressed(pad, st, &PadState::lb)) { const Point t = ArtTool((button + kArtTools - 1) % kArtTools); touch.Tap(t.x, t.y); }
    else if (Pressed(pad, st, &PadState::rb)) { const Point t = ArtTool((button + 1) % kArtTools); touch.Tap(t.x, t.y); }
    else if (Pressed(pad, st, &PadState::x)) { const Point t = ArtSwatch((std::clamp(st.ram.artSwatch, 0, kArtSwatches - 1) + 1) % kArtSwatches); touch.Tap(t.x, t.y); }
    else if (Pressed(pad, st, &PadState::y)) touch.Tap(kArtUndo.x, kArtUndo.y);
    else if (Pressed(pad, st, &PadState::b)) touch.Tap(kArtDone.x, kArtDone.y);
}
}

Point ArtTool(int b)
{
    b = std::clamp(b, 0, kArtTools - 1);
    return b < 6 ? Point {14, 14 + 26 * b} : Point {241, 66 + 26 * (b - 6)};
}

Point ArtSwatch(int i)
{
    i = std::clamp(i, 0, kArtSwatches - 1);
    return i < 7 ? Point {55 + 25 * i, 165} : Point {43 + 25 * (i - 7), 187};
}

bool IsPointing(Screen s) { return s == Screen::Autopsy || s == Screen::Detector || s == Screen::Art; }

void MovePointer(const PadState& pad, ActivityState& st, float cap, bool dpad)
{
    // The stick over further: either one, the one further over.
    float sx = pad.lx, sy = pad.ly;
    if (pad.rx * pad.rx + pad.ry * pad.ry > sx * sx + sy * sy) { sx = pad.rx; sy = pad.ry; }
    const float mag = std::sqrt(sx * sx + sy * sy);
    if (mag > kPtrDead)
    {
        const float m = std::min(1.0f, (mag - kPtrDead) / (1.0f - kPtrDead));
        float speed = kPtrMax * m * m;
        if (pad.lt > 0.5f) speed *= kPtrSlow;
        speed = std::min(speed, cap);
        st.px += sx / mag * speed;
        st.py += sy / mag * speed;
    }
    const int dx = (pad.right ? 1 : 0) - (pad.left ? 1 : 0), dy = (pad.down ? 1 : 0) - (pad.up ? 1 : 0);
    if (dpad && (dx || dy))
    {
        st.dpadHeld++;
        float step = st.dpadHeld == 1 ? 1.0f : st.dpadHeld <= kNudgeWait ? 0.0f : st.dpadHeld <= kNudgeFast ? 1.0f : kNudgeSpeed;
        step = std::min(step, std::max(cap, 1.0f));
        st.px += dx * step;
        st.py += dy * step;
    }
    else st.dpadHeld = 0;
    st.px = std::clamp(st.px, 0.0f, (float)(SW - 1));
    st.py = std::clamp(st.py, 0.0f, (float)(SH - 1));
}

bool IsActivity(Screen s) { return s == Screen::Social || s == Screen::Cards || s == Screen::Vacuum || IsPointing(s); }

void ActivityInput(Screen s, const PadState& pad, TouchScript& touch, u32& dsKeys, ActivityState& st)
{
    st.blow = st.grab = false;
    st.handDx = st.handDy = 0;
    st.pointer = st.down = false;
    // (The autopsy quits on Start at once: its Start asks first, ActivityInput's Autopsy.)
    if (pad.start && s != Screen::Autopsy) dsKeys |= 1u << kStart;
    if (pad.select) dsKeys |= 1u << kSelect;
    const bool canTap = !touch.Owns();
    switch (s)
    {
    case Screen::Social: Social(pad, touch, st, canTap); break;
    case Screen::Cards: Cards(pad, touch, st, canTap); break;
    case Screen::Vacuum: Vacuum(pad, dsKeys, st); st.focus = -1; break;
    case Screen::Autopsy: Autopsy(pad, touch, dsKeys, st, canTap); st.focus = -1; break;
    case Screen::Detector: Detector(pad, touch, dsKeys, st, canTap); st.focus = -1; break;
    case Screen::Art: Art(pad, touch, st, canTap); st.focus = -1; break;
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
    case Screen::Autopsy:
        return {{"STICK", "POINT (LT SLOW)"}, {"A", "HOLD: USE TOOL"}, {"X Y RB", "LASER, HAND,\nTWEEZERS"},
                {"LB", "TO TRAY/FLASK"}, {"B B", "QUIT (NO PAY)"}};
    case Screen::Detector:
        return {{"L-STICK", "WALK"}, {"A", "DIG"}, {"LB RB", "DEPTH"}, {"B", "POWER OFF"}};
    case Screen::Art:
        return {{"STICK", "POINT (LT SLOW)"}, {"A", "HOLD: PAINT"}, {"LB RB", "TOOL"}, {"X", "COLOR"},
                {"Y", "UNDO"}, {"B", "DONE"}};
    default: return {};
    }
}
}
