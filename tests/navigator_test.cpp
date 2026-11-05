// Checks CONSOLE's navigator on synthetic screen states: the menu opens and closes, the D-pad
// moves the focus, A taps the focused target, L / R tap the tab icons, a popup closes with A, the
// town keeps the DS's buttons, the phone closes with B, Unknown presses the stick cursor (A stays
// the DS's without one), the pad's touch menu acts on release unless Start joins it, and leaving
// CONSOLE releases everything; the activities (activities.h): the social answers on LEFT / UP /
// RIGHT, the cards' draw, lift, play, discard and quit question, the vacuum's blow, hand and
// steering; presses that would tap wait for a settled screen (90 frames at most), DS buttons do
// not; the front end's screens keep the DS's buttons, save slots and the quit question the cursor.
//   ninja -C build navigator_test && ./build/navigator_test
#include <cstdio>
#include <cstdlib>
#include <string>

#include "hotspots.h"
#include "navigator.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const ScreenState kInteract {Screen::Panel, Tab::Interact};
static const ScreenState kGoals {Screen::Panel, Tab::Goals};
static const ScreenState kPhone {Screen::Phone, Tab::None};
static const ScreenState kPopup {Screen::Popup, Tab::None};
static const ScreenState kDialogue {Screen::Dialogue, Tab::None};
static const ScreenState kUnknown {Screen::Unknown, Tab::None};
static const ScreenState kSocial {Screen::Social, Tab::None};
static const ScreenState kCards {Screen::Cards, Tab::None};
static const ScreenState kVacuum {Screen::Vacuum, Tab::None};
static const ScreenState kFrontEnd {Screen::FrontEnd, Tab::None};
static const ScreenState kCreateASim {Screen::CreateASim, Tab::None};
static const ScreenState kSaveSlots {Screen::SaveSlots, Tab::None};

// The index of the target named `name` on screen s.
static int Index(ScreenState s, const char* name)
{
    const auto& h = HotspotsFor(s);
    for (int i = 0; i < (int)h.size(); i++)
        if (std::string(h[i].name) == name) return i;
    return -1;
}

struct Rig
{
    Navigator nav;
    TouchScript touch;
    u32 keys = 0;
    // One frame with these buttons held (the touch script is not advanced: see Touched).
    u32 Frame(ScreenState s, PadState p = {}, bool settled = true)
    {
        keys = 0;
        nav.Update(s, p, touch, keys, settled);
        return keys;
    }
    // A press: a frame held, then a frame released.
    void Press(ScreenState s, bool PadState::*button)
    {
        PadState p;
        p.*button = true;
        Frame(s, p);
        Frame(s);
    }
    // The first frame of the touch script's current tap, if any.
    bool Touched(int& x, int& y) { return touch.Owns() && touch.Next(x, y); }
    // Where the touch script's current touch ends (its first and last points).
    bool Path(int& x0, int& y0, int& x1, int& y1)
    {
        if (!touch.Owns() || !touch.Next(x0, y0)) return false;
        x1 = x0, y1 = y0;
        int x, y;
        while (touch.Next(x, y)) { x1 = x; y1 = y; }
        return true;
    }
    void Drain()
    {
        int x, y;
        while (touch.Owns()) touch.Next(x, y);
    }
};

static void menu_opens_and_closes()
{
    Rig r;
    r.Frame(kInteract);
    CHECK(!r.nav.MenuOpen());
    CHECK(r.nav.Focus() == -1);
    r.Press(kInteract, &PadState::menu);
    CHECK(r.nav.MenuOpen());
    CHECK(r.nav.Focus() == 0); // the tab's first target
    r.Press(kInteract, &PadState::b);
    CHECK(!r.nav.MenuOpen());
    CHECK(r.nav.Focus() == -1);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::menu);
    CHECK(!r.nav.MenuOpen());
    CHECK(!r.touch.Owns()); // closing the panel's menu taps nothing
}

static void dpad_moves_focus()
{
    Rig r;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    const auto& h = HotspotsFor(kInteract);
    CHECK(r.nav.Focus() == 0);
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.Focus() == Neighbour(h, 0, 0, 1));
    CHECK(r.nav.Focus() != 0);
    const int f = r.nav.Focus();
    r.Press(kInteract, &PadState::up);
    CHECK(r.nav.Focus() == Neighbour(h, f, 0, -1));
    // The left stick too.
    PadState p;
    p.ly = 0.9f;
    const int g = r.nav.Focus();
    r.Frame(kInteract, p);
    CHECK(r.nav.Focus() == Neighbour(h, g, 0, 1));
    // Held: one move, then a repeat only after a while.
    const int k = r.nav.Focus();
    for (int i = 0; i < 5; i++) r.Frame(kInteract, p);
    CHECK(r.nav.Focus() == k);
    // In town (menu closed) the D-pad is the DS's.
    r.Press(kInteract, &PadState::b);
    PadState d;
    d.down = true;
    CHECK(r.Frame(kInteract, d) & (1u << kDown));
    CHECK(r.nav.Focus() == -1);
}

static void a_taps_focused()
{
    Rig r;
    r.Frame(kGoals);
    r.Press(kGoals, &PadState::menu);
    r.Press(kGoals, &PadState::down);
    const Hotspot& h = HotspotsFor(kGoals)[r.nav.Focus()];
    PadState a;
    a.a = true;
    CHECK(!(r.Frame(kGoals, a) & (1u << kA))); // not the DS's A while the menu is up
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == h.x && y == h.y);
    // A held does not tap again; a press while the tap runs is dropped.
    r.Frame(kGoals, a);
    r.Frame(kGoals);
    r.Frame(kGoals, a);
    r.Drain();
    CHECK(!r.touch.Busy());
}

static void lb_rb_tap_tabs()
{
    Rig r;
    r.Frame(kInteract);
    // In town L and R are the DS's (the camera).
    PadState l;
    l.lb = true;
    CHECK(r.Frame(kInteract, l) & (1u << kL));
    CHECK(!r.touch.Owns());
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::rb);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == kTabIcon[1].x && y == kTabIcon[1].y); // Pockets
    r.Drain();
    r.Press(kInteract, &PadState::lb);
    CHECK(r.Touched(x, y));
    CHECK(x == kTabIcon[4].x && y == kTabIcon[4].y); // round to Options
    r.Drain();
    // The tab changed: the menu stays up, the focus on the new tab's first target.
    r.Frame(kGoals);
    CHECK(r.nav.MenuOpen());
    CHECK(r.nav.Focus() == 0);
    r.Press(kGoals, &PadState::rb);
    CHECK(r.Touched(x, y));
    CHECK(x == kTabIcon[3].x && y == kTabIcon[3].y); // Relationships
}

static void popup_closed_by_a()
{
    Rig r;
    r.Frame(kPopup);
    CHECK(r.nav.Focus() == 0);
    PadState a;
    a.a = true;
    CHECK(!(r.Frame(kPopup, a) & (1u << kA)));
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == kPopupClose.x && y == kPopupClose.y);
    r.Drain();
    // Still reading Popup (the box slides away): a second press does not tap what lies behind.
    r.Frame(kPopup);
    r.Press(kPopup, &PadState::a);
    CHECK(!r.touch.Owns());
    // B too, once the screen changed and came back; Start stays the DS's.
    r.Frame(kInteract);
    r.Frame(kPopup);
    r.Press(kPopup, &PadState::b);
    CHECK(r.Touched(x, y));
    CHECK(x == kPopupClose.x && y == kPopupClose.y);
    PadState s;
    s.start = true;
    CHECK(r.Frame(kPopup, s) & (1u << kStart));
}

static void phone_closed_by_b()
{
    Rig r;
    r.Frame(kPhone);
    CHECK(r.nav.Focus() == 0);
    r.Press(kPhone, &PadState::b);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == kPhoneClose.x && y == kPhoneClose.y);
    r.Drain();
    r.Frame(kInteract);
    CHECK(!r.nav.MenuOpen());
    // Opened from the menu's phone icon: closing it closes the menu as well.
    r.Press(kInteract, &PadState::menu);
    r.Frame(kPhone);
    r.Press(kPhone, &PadState::menu);
    CHECK(r.Touched(x, y));
    CHECK(x == kPhoneClose.x && y == kPhoneClose.y);
    r.Drain();
    r.Frame(kInteract);
    CHECK(!r.nav.MenuOpen());
}

static void town_and_dialogue_keep_the_buttons()
{
    Rig r;
    r.Frame(kDialogue);
    PadState p;
    p.a = p.b = p.x = p.y = p.start = p.select = p.right = true;
    const u32 k = r.Frame(kDialogue, p);
    for (int key : {kA, kB, kX, kY, kStart, kSelect, kRight}) CHECK(k & (1u << key));
    CHECK(!r.touch.Owns());
    CHECK(r.nav.Focus() == -1);
    PadState m;
    m.menu = true;
    r.Frame(kDialogue, m);
    CHECK(!r.nav.MenuOpen()); // no menu over a conversation
    CHECK(r.Frame(kInteract, p) & (1u << kA));
}

static void unknown_presses_the_cursor()
{
    Rig r;
    r.Frame(kUnknown);
    CHECK(r.nav.CursorScreen());
    PadState a;
    a.a = a.cursor = true;
    const u32 k = r.Frame(kUnknown, a);
    CHECK(!(k & (1u << kA)));
    CHECK(r.nav.CursorPress());
    CHECK(r.nav.Focus() == -1);
    r.Frame(kUnknown);
    CHECK(!r.nav.CursorPress());
    r.Frame(kInteract);
    CHECK(!r.nav.CursorScreen());
}

static void unknown_without_a_cursor_keeps_a()
{
    // The keyboard alone (no controller, or the cursor off): A stays the DS's on Unknown screens.
    Rig r;
    r.Frame(kUnknown);
    PadState a;
    a.a = true;
    CHECK(r.Frame(kUnknown, a) & (1u << kA));
    CHECK(!r.nav.CursorPress());
}

static void touch_menu_on_release_unless_start()
{
    // Back alone: the touch menu, once, when it goes up.
    ReleasePulse back;
    CHECK(!back.Update(true, false));
    CHECK(!back.Update(true, false));
    CHECK(back.Update(false, false));
    CHECK(!back.Update(false, false));
    // Back, then Start with it (the pause menu): nothing, whichever is released first.
    CHECK(!back.Update(true, false));
    CHECK(!back.Update(true, true));
    CHECK(!back.Update(true, false));
    CHECK(!back.Update(false, false));
    // Start first, then Back.
    CHECK(!back.Update(true, true));
    CHECK(!back.Update(false, false));
    // A fresh press is not spoiled by the last one.
    CHECK(!back.Update(true, false));
    CHECK(back.Update(false, false));
    // The pulse opens the menu through the navigator's edge.
    Rig r;
    r.Frame(kInteract);
    ReleasePulse b;
    PadState p;
    p.menu = b.Update(true, false);
    r.Frame(kInteract, p);
    CHECK(!r.nav.MenuOpen());
    p.menu = b.Update(false, false);
    r.Frame(kInteract, p);
    CHECK(r.nav.MenuOpen());
    p.menu = b.Update(false, false);
    r.Frame(kInteract, p);
    CHECK(r.nav.MenuOpen());
}

static void leaving_console_releases_all()
{
    Rig r;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    PadState a;
    a.a = true;
    r.Frame(kInteract, a);
    CHECK(r.touch.Owns());
    r.nav.Leave(r.touch);
    CHECK(!r.touch.Owns());
    CHECK(!r.nav.MenuOpen());
    CHECK(r.nav.Focus() == -1);
    CHECK(!r.nav.CursorPress());
    // Back in CONSOLE with A and MENU still held: not presses; nothing taps, the menu stays shut.
    a.menu = true;
    r.Frame(kInteract, a);
    CHECK(!r.nav.MenuOpen());
    CHECK(!r.touch.Owns());
    // A held in town is the DS's A, as held (not a stuck key: it goes when released).
    CHECK(r.Frame(kInteract, a) & (1u << kA));
    CHECK(r.Frame(kInteract) == 0);
}

static void social_answers_on_the_d_pad()
{
    Rig r;
    r.Frame(kSocial);
    const auto& h = HotspotsFor(kSocial);
    CHECK(r.nav.Focus() == 0);
    bool PadState::*dirs[3] = {&PadState::left, &PadState::up, &PadState::right};
    for (int i = 0; i < 3; i++)
    {
        PadState p;
        p.*dirs[i] = true;
        CHECK(r.Frame(kSocial, p) == 0); // not the DS's D-pad
        int x = -1, y = -1;
        CHECK(r.Touched(x, y));
        CHECK(x == h[i].x && y == h[i].y);
        CHECK(r.nav.Focus() == i);
        r.Drain();
        r.Frame(kSocial);
    }
    // A taps the focused answer (the last one chosen); the stick's left too.
    r.Press(kSocial, &PadState::a);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == h[2].x && y == h[2].y);
    r.Drain();
    PadState stick;
    stick.lx = -1;
    r.Frame(kSocial, stick);
    CHECK(r.Touched(x, y));
    CHECK(x == h[0].x && y == h[0].y);
}

static void cards_draw_lift_play_discard()
{
    Rig r;
    r.Frame(kCards);
    const auto& h = HotspotsFor(kCards);
    const int card1 = Index(kCards, "card 1");
    CHECK(r.nav.Focus() == card1);
    int x = -1, y = -1, x1 = -1, y1 = -1;
    // X: the deck.
    r.Press(kCards, &PadState::x);
    CHECK(r.Touched(x, y));
    CHECK(x == h[Index(kCards, "deck")].x && y == h[Index(kCards, "deck")].y);
    r.Drain();
    // RIGHT: the next card; A lifts it (a tap), A again drags it onto the play arrow.
    r.Press(kCards, &PadState::right);
    const int card2 = Index(kCards, "card 2");
    CHECK(r.nav.Focus() == card2);
    CHECK(!r.touch.Owns());
    r.Press(kCards, &PadState::a);
    CHECK(r.Path(x, y, x1, y1));
    CHECK(x == h[card2].x && y == h[card2].y && x1 == x && y1 == y);
    r.Press(kCards, &PadState::a);
    CHECK(r.Path(x, y, x1, y1));
    CHECK(x == h[card2].x && y == h[card2].y);
    CHECK(x1 == h[Index(kCards, "play")].x && y1 == h[Index(kCards, "play")].y);
    // Played: A lifts again. Y drags the focused card onto the discard pile.
    r.Press(kCards, &PadState::a);
    CHECK(r.Path(x, y, x1, y1));
    CHECK(x1 == x && y1 == y);
    r.Press(kCards, &PadState::y);
    CHECK(r.Path(x, y, x1, y1));
    CHECK(x == h[card2].x && y == h[card2].y);
    CHECK(x1 == h[Index(kCards, "discard")].x && y1 == h[Index(kCards, "discard")].y);
    // A press while a drag runs is dropped.
    r.Press(kCards, &PadState::y);
    CHECK(r.touch.Owns());
    r.Press(kCards, &PadState::x);
    r.Drain();
    CHECK(!r.touch.Owns());
    // The focus never rests on the play arrow (a drag's target only).
    for (int i = 0; i < 6; i++) r.Press(kCards, &PadState::up);
    CHECK(r.nav.Focus() != Index(kCards, "play"));
}

static void cards_quit_question()
{
    Rig r;
    r.Frame(kCards);
    int x = -1, y = -1;
    const Hotspot& quit = HotspotsFor(kCards)[Index(kCards, "quit")];
    // B: the quit button; B again answers no (the cross), and the game goes on.
    r.Press(kCards, &PadState::b);
    CHECK(r.Touched(x, y));
    CHECK(x == quit.x && y == quit.y);
    r.Drain();
    r.Press(kCards, &PadState::x); // waits for the answer
    CHECK(!r.touch.Owns());
    r.Press(kCards, &PadState::b);
    CHECK(r.Touched(x, y));
    CHECK(x == kCardsQuitNo.x && y == kCardsQuitNo.y);
    r.Drain();
    r.Press(kCards, &PadState::x);
    CHECK(r.Touched(x, y));
    CHECK(x == HotspotsFor(kCards)[Index(kCards, "deck")].x);
    r.Drain();
    // B, then A: yes (the tick).
    r.Press(kCards, &PadState::b);
    r.Drain();
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y));
    CHECK(x == kCardsQuitYes.x && y == kCardsQuitYes.y);
    r.Drain();
    // A fresh game (the screen changed) asks nothing.
    r.Frame(kInteract);
    r.Frame(kCards);
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y));
    CHECK(x == HotspotsFor(kCards)[Index(kCards, "card 1")].x);
}

static void vacuum_blows_drives_and_grabs()
{
    Rig r;
    r.Frame(kVacuum);
    CHECK(r.nav.CursorScreen() && r.nav.Hand());
    CHECK(r.nav.Focus() == -1);
    // Y: the microphone, not the DS's Y.
    PadState p;
    p.y = true;
    CHECK(!(r.Frame(kVacuum, p) & (1u << kY)));
    CHECK(r.nav.Blow());
    r.Frame(kVacuum);
    // A quick tap (2 frames) still blows 8 frames or more: the game needs 7.
    {
        for (int i = 0; i < 12; i++) r.Frame(kVacuum);
        int blows = 0;
        PadState y;
        y.y = true;
        for (int i = 0; i < 2; i++) { r.Frame(kVacuum, y); blows += r.nav.Blow(); }
        for (int i = 0; i < 20; i++) { r.Frame(kVacuum); blows += r.nav.Blow(); }
        CHECK(blows >= 8);
        CHECK(!r.nav.Blow());
    }
    // The D-pad and the stick drive; B, L and R stay the DS's.
    PadState d;
    d.up = d.b = d.lb = true;
    d.lx = 1;
    u32 k = r.Frame(kVacuum, d);
    for (int key : {kUp, kRight, kB, kL}) CHECK(k & (1u << key));
    CHECK(r.nav.HandDx() == 0 && r.nav.HandDy() == 0);
    // X held: the D-pad moves the hand (no driving); the stick still drives.
    PadState hx;
    hx.x = hx.up = hx.right = true;
    hx.ly = 1;
    k = r.Frame(kVacuum, hx);
    CHECK(!(k & (1u << kUp)) && !(k & (1u << kRight)) && !(k & (1u << kX)));
    CHECK(k & (1u << kDown));
    CHECK(r.nav.HandDx() > 0 && r.nav.HandDy() < 0);
    CHECK(!r.nav.CursorPress());
    // A held: the stylus down at the hand (and the D-pad drags it), not the DS's A.
    PadState a;
    a.a = a.left = true;
    k = r.Frame(kVacuum, a);
    CHECK(!(k & (1u << kA)) && !(k & (1u << kLeft)));
    CHECK(r.nav.CursorPress());
    CHECK(r.nav.HandDx() < 0);
    CHECK(!r.touch.Owns()); // (the hand is the stick cursor, not a touch script)
    r.Frame(kVacuum);
    CHECK(!r.nav.CursorPress());
    // Leaving the vacuum lets go.
    r.Frame(kVacuum, a);
    r.Frame(kInteract);
    CHECK(!r.nav.CursorPress() && !r.nav.Hand());
}

static void unsettled_press_waits()
{
    // The game's bottom slot switching (GameState::settled false): A on the popup is held, then
    // tapped once on the first settled frame.
    Rig r;
    r.Frame(kPopup, {}, false);
    PadState a;
    a.a = true;
    r.Frame(kPopup, a, false);
    r.Frame(kPopup, {}, false);
    CHECK(!r.touch.Owns());
    r.Frame(kPopup, {}, true);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == kPopupClose.x && y == kPopupClose.y);
    r.Drain();
    r.Frame(kPopup, {}, true);
    CHECK(!r.touch.Owns());
    // The latest press wins: R (the next tab's icon) then A on the open menu, while the tab
    // switches, taps A's target.
    Rig m;
    m.Frame(kInteract);
    m.Press(kInteract, &PadState::menu);
    m.Frame(kGoals, {}, false);
    PadState rb;
    rb.rb = true;
    m.Frame(kGoals, rb, false);
    m.Frame(kGoals, {}, false);
    m.Frame(kGoals, a, false);
    m.Frame(kGoals, {}, true);
    CHECK(m.Touched(x, y));
    const Hotspot& f = HotspotsFor(kGoals)[m.nav.Focus()];
    CHECK(x == f.x && y == f.y);
    CHECK(m.nav.MenuOpen());
}

static void unsettled_press_dropped_after_90()
{
    Rig r;
    r.Frame(kPopup, {}, false);
    PadState a;
    a.a = true;
    r.Frame(kPopup, a, false);
    for (int i = 0; i < 90; i++) r.Frame(kPopup, {}, false); // 91 unsettled frames with the press
    r.Frame(kPopup, {}, true);
    CHECK(!r.touch.Owns());
    // At 90 it still comes.
    Rig s;
    s.Frame(kPopup, {}, false);
    s.Frame(kPopup, a, false);
    for (int i = 0; i < 89; i++) s.Frame(kPopup, {}, false);
    s.Frame(kPopup, {}, true);
    CHECK(s.touch.Owns());
}

static void unsettled_ds_buttons_pass()
{
    Rig r;
    r.Frame(kPopup, {}, false);
    PadState st;
    st.start = true;
    CHECK(r.Frame(kPopup, st, false) & (1u << kStart));
    r.Frame(kInteract, {}, false);
    PadState p;
    p.a = p.b = p.right = true;
    const u32 k = r.Frame(kInteract, p, false);
    for (int key : {kA, kB, kRight}) CHECK(k & (1u << key));
}

static void unsettled_press_dropped_on_screen_change()
{
    // Held for the popup, the screen changes before it settles: nothing is tapped.
    Rig r;
    r.Frame(kPopup, {}, false);
    PadState a;
    a.a = true;
    r.Frame(kPopup, a, false);
    r.Frame(kPhone, {}, false);
    r.Frame(kPhone, {}, true);
    CHECK(!r.touch.Owns());
    // Nor when it goes through a screen that does not tap and comes back.
    Rig s;
    s.Frame(kPopup, {}, false);
    s.Frame(kPopup, a, false);
    s.Frame(kDialogue, {}, false);
    s.Frame(kPopup, {}, true);
    CHECK(!s.touch.Owns());
    // Nor when the menu closes (B: no tap) and opens again on the same tab before it settles.
    Rig m;
    m.Frame(kGoals);
    m.Press(kGoals, &PadState::menu);
    CHECK(m.nav.MenuOpen());
    m.Frame(kGoals, a, false);
    PadState b;
    b.b = true;
    m.Frame(kGoals, b, false);
    CHECK(!m.nav.MenuOpen());
    m.Frame(kGoals, {}, false);
    PadState menu;
    menu.menu = true;
    m.Frame(kGoals, menu, false);
    CHECK(m.nav.MenuOpen());
    m.Frame(kGoals, {}, true);
    CHECK(!m.touch.Owns());
}

static void save_slots_and_quit_keep_the_cursor()
{
    // Options > Save slots, and Options > Quit (the RAM's popup over the Options tab, which
    // CONSOLE takes as Unknown): A presses the stick cursor with a controller, stays the DS's
    // without one; the D-pad stays the DS's.
    for (ScreenState s : {kSaveSlots, kUnknown})
    {
        Rig r;
        r.Frame(s);
        CHECK(r.nav.CursorScreen());
        PadState a;
        a.a = a.cursor = a.down = true;
        const u32 k = r.Frame(s, a);
        CHECK(!(k & (1u << kA)) && (k & (1u << kDown)));
        CHECK(r.nav.CursorPress());
        CHECK(!r.touch.Owns());
        Rig n;
        n.Frame(s);
        PadState b;
        b.a = true;
        CHECK(n.Frame(s, b) & (1u << kA));
        CHECK(!n.nav.CursorPress());
    }
}

static void front_end_screens_pass_the_buttons()
{
    // Title / main menu, Create-a-Sim, a black load: no table of targets, the DS's own buttons
    // (A, B, the D-pad), even with a stick cursor.
    for (ScreenState s : {kFrontEnd, kCreateASim, ScreenState {Screen::Loading, Tab::None}})
    {
        Rig r;
        r.Frame(s);
        PadState p;
        p.a = p.b = p.up = p.left = p.cursor = true;
        const u32 k = r.Frame(s, p);
        for (int key : {kA, kB, kUp, kLeft}) CHECK(k & (1u << key));
        CHECK(!r.touch.Owns());
        CHECK(r.nav.Focus() == -1);
        CHECK(!r.nav.CursorPress());
        CHECK(HotspotsFor(s).empty());
    }
    CHECK(std::string(ScreenName(Screen::SaveSlots)) == "SaveSlots");
    CHECK(std::string(ScreenName(Screen::Loading)) == "Loading");
}

int main()
{
    menu_opens_and_closes();
    dpad_moves_focus();
    a_taps_focused();
    lb_rb_tap_tabs();
    popup_closed_by_a();
    phone_closed_by_b();
    town_and_dialogue_keep_the_buttons();
    unknown_presses_the_cursor();
    unknown_without_a_cursor_keeps_a();
    touch_menu_on_release_unless_start();
    leaving_console_releases_all();
    social_answers_on_the_d_pad();
    cards_draw_lift_play_discard();
    cards_quit_question();
    vacuum_blows_drives_and_grabs();
    unsettled_press_waits();
    unsettled_press_dropped_after_90();
    unsettled_ds_buttons_pass();
    unsettled_press_dropped_on_screen_change();
    save_slots_and_quit_keep_the_cursor();
    front_end_screens_pass_the_buttons();
    if (failures) { fprintf(stderr, "navigator_test: %d failure(s)\n", failures); return 1; }
    printf("navigator_test: ok\n");
    return 0;
}
