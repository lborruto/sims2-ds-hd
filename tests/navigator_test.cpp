// Checks CONSOLE's navigator on synthetic screen states: the menu opens and closes, the D-pad
// moves the focus, A taps the focused target, L / R tap the tab icons, a popup closes with A, the
// town keeps the DS's buttons, the phone closes with B, Unknown presses the stick cursor (A stays
// the DS's without one), the pad's touch menu acts on release unless Start joins it, and leaving
// CONSOLE releases everything; the activities (activities.h): the social answers on LEFT / UP /
// RIGHT, the cards' draw, lift, play, discard and quit question, the vacuum's blow, hand and
// steering; presses that would tap wait for a settled screen (90 frames at most), DS buttons do
// not (a popup opening waits); the front end's screens, save slots and the quit question the
// cursor, a black load the DS's buttons; the HD menu (port/menuview.h) on synthetic MenuViews:
// the focus over the view's enabled items (greyed rows skipped, never performed), A runs the
// focused item's steps (taps, DS keys over frames, drags, in order), Left / Right the view's
// screen-wide actions, B its back action or the menu's close, the focus kept on its item's id when
// the content changes (or clamped), a press held by the gate dropped when the view changed
// (stale), the list's own scroll past its end with the focus following the rebuilt view.
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
    const MenuView* view = nullptr; // the screen's HD menu, if any
    // One frame with these buttons held (the touch script is not advanced: see Touched).
    u32 Frame(ScreenState s, PadState p = {}, bool settled = true)
    {
        keys = 0;
        nav.Update(s, p, touch, keys, settled, view);
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
    // No stick cursor on a conversation (it has a table: the arrow), so none is drawn over the HD
    // panel's invisible tap piece, with or without a controller.
    PadState c;
    c.cursor = true;
    c.a = true;
    CHECK(!r.nav.CursorScreen());
    CHECK(r.Frame(kDialogue, c) & (1u << kA));
    CHECK(!r.nav.CursorScreen() && !r.nav.CursorPress());
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

static void front_end_screens_touch_with_a()
{
    // Title / main menu, Create-a-Sim, None (an unmapped front-end screen): touch-only screens
    // with no table of targets, so as Unknown: A presses the stick cursor with a controller and
    // stays the DS's without one; B and the D-pad stay the DS's.
    for (ScreenState s : {kFrontEnd, kCreateASim, ScreenState {Screen::None, Tab::None}})
    {
        Rig r;
        r.Frame(s);
        CHECK(r.nav.CursorScreen());
        PadState p;
        p.a = p.b = p.up = p.left = p.cursor = true;
        const u32 k = r.Frame(s, p);
        CHECK(!(k & (1u << kA)));
        for (int key : {kB, kUp, kLeft}) CHECK(k & (1u << key));
        CHECK(r.nav.CursorPress());
        CHECK(!r.touch.Owns());
        CHECK(r.nav.Focus() == -1);
        CHECK(HotspotsFor(s).empty());
        Rig n;
        n.Frame(s);
        PadState a;
        a.a = true;
        CHECK(n.Frame(s, a) & (1u << kA));
        CHECK(!n.nav.CursorPress());
    }
    // A black load: nothing to touch, every button the DS's.
    {
        const ScreenState s {Screen::Loading, Tab::None};
        Rig r;
        r.Frame(s);
        PadState p;
        p.a = p.b = p.up = p.left = p.cursor = true;
        const u32 k = r.Frame(s, p);
        for (int key : {kA, kB, kUp, kLeft}) CHECK(k & (1u << key));
        CHECK(!r.nav.CursorPress());
        CHECK(!r.touch.Owns());
    }
    CHECK(std::string(ScreenName(Screen::SaveSlots)) == "SaveSlots");
    CHECK(std::string(ScreenName(Screen::Loading)) == "Loading");
}

static void popup_opening_holds_a()
{
    // A popup opening (the game reads Popup about 13 frames before the box is drawn: not settled,
    // GameState::settled): A is held, then taps the popup's X once the box is shown.
    Rig r;
    r.Frame(kInteract);
    r.Frame(kPopup, {}, false);
    PadState a;
    a.a = true;
    r.Frame(kPopup, a, false);
    for (int i = 0; i < 10; i++) r.Frame(kPopup, {}, false);
    CHECK(!r.touch.Owns());
    r.Frame(kPopup, {}, true);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == kPopupClose.x && y == kPopupClose.y);
    r.Drain();
    r.Frame(kPopup, {}, true);
    CHECK(!r.touch.Owns());
}

// A synthetic HD menu: a list of rows (group 1) in a column, each tapping (200 + i, 50 + i), the
// third greyed, and a button at the top right (the phone's place) tapping (232, 22).
static MenuView ListView(int rows = 4, int greyed = 2)
{
    MenuView v;
    v.valid = true;
    for (int i = 0; i < rows; i++)
    {
        ViewItem it;
        it.rect = {460, 210 + 76 * i, 620, 62};
        it.action = {TapStep(200 + i, 50 + i)};
        it.enabled = i != greyed;
        it.group = 1;
        it.id = 0x100 + (u32)i;
        v.items.push_back(it);
    }
    ViewItem phone;
    phone.rect = {1000, 48, 74, 76};
    phone.action = {TapStep(232, 22)};
    phone.id = 1;
    v.items.push_back(phone);
    v.valid = true;
    return v;
}

static void view_focus_skips_greyed_and_a_runs()
{
    Rig r;
    const MenuView v = ListView();
    r.view = &v;
    r.Frame(kInteract);
    CHECK(r.nav.Focus() == -1 && !r.nav.ViewFocus()); // menu closed: the DS's buttons
    r.Press(kInteract, &PadState::menu);
    CHECK(r.nav.ViewFocus());
    CHECK(r.nav.Focus() == 0);
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.Focus() == 1);
    r.Press(kInteract, &PadState::down); // row 2 is greyed: skipped
    CHECK(r.nav.Focus() == 3);
    r.Press(kInteract, &PadState::up);
    CHECK(r.nav.Focus() == 1);
    PadState a;
    a.a = true;
    CHECK(!(r.Frame(kInteract, a) & (1u << kA))); // not the DS's A
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == 201 && y == 51);
    r.Drain();
    r.Frame(kInteract);
    // Up from the first row: the button above (the phone), then A taps it.
    r.Press(kInteract, &PadState::up);
    r.Press(kInteract, &PadState::up);
    CHECK(r.nav.Focus() == 4);
    r.Frame(kInteract, a);
    CHECK(r.Touched(x, y));
    CHECK(x == 232 && y == 22);
    r.Drain();
    // A pinned onto a greyed item (a check) never runs it.
    MenuView g = ListView(3, 0);
    g.items.pop_back();
    g.items[1].enabled = g.items[2].enabled = false;
    Rig q;
    q.view = &g;
    q.Frame(kInteract);
    q.Press(kInteract, &PadState::menu);
    CHECK(q.nav.Focus() == -1); // nothing enabled: no focus
    q.Press(kInteract, &PadState::a);
    CHECK(!q.touch.Owns());
    // B closes the menu (no back action), no tap.
    q.Press(kInteract, &PadState::b);
    CHECK(!q.nav.MenuOpen() && !q.touch.Owns());
}

static void view_buttons_keys_and_sequences()
{
    // Left / Right: the view's screen-wide actions (Interact's targets), not focus moves.
    MenuView v = ListView();
    v.buttons[(int)MenuButton::Left] = {TapStep(55, 167)};
    Rig r;
    r.view = &v;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::left);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == 55 && y == 167);
    r.Drain();
    r.Frame(kInteract);
    // Right has none: a focus move as usual.
    const int f = r.nav.Focus();
    r.Press(kInteract, &PadState::right);
    CHECK(!r.touch.Owns());
    CHECK(r.nav.Focus() == MenuNeighbour(v, f, 1, 0));
    // An action of DS keys: held its frames, one step after the other, a tap after them.
    MenuView k = ListView(1, -1);
    k.items[0].action = {KeysStep(1u << kA, 3), KeysStep(1u << kDown, 1), TapStep(10, 20)};
    Rig s;
    s.view = &k;
    s.Frame(kInteract);
    s.Press(kInteract, &PadState::menu);
    PadState a;
    a.a = true;
    CHECK(s.Frame(kInteract, a) == (1u << kA));
    CHECK(s.nav.Running());
    CHECK(s.Frame(kInteract) == (1u << kA));
    CHECK(s.Frame(kInteract) == (1u << kA));
    CHECK(s.Frame(kInteract) == (1u << kDown));
    CHECK(!s.touch.Owns());
    s.Frame(kInteract);
    CHECK(s.Touched(x, y));
    CHECK(x == 10 && y == 20);
    CHECK(!s.nav.Running());
    // A press while an action runs is dropped (not queued).
    MenuView w = ListView(1, -1);
    w.items[0].action = {WaitStep(5), TapStep(1, 2)};
    Rig t;
    t.view = &w;
    t.Frame(kInteract);
    t.Press(kInteract, &PadState::menu);
    t.Press(kInteract, &PadState::a);
    t.Press(kInteract, &PadState::a);
    for (int i = 0; i < 6; i++) t.Frame(kInteract);
    CHECK(t.Touched(x, y));
    t.Drain();
    for (int i = 0; i < 6; i++) t.Frame(kInteract);
    CHECK(!t.touch.Owns());
    // B runs the view's back action when it has one (and the menu stays).
    MenuView b = ListView();
    b.back = {TapStep(238, 18)};
    Rig u;
    u.view = &b;
    u.Frame(kInteract);
    u.Press(kInteract, &PadState::menu);
    PadState pb;
    pb.b = true;
    u.Frame(kInteract, pb);
    CHECK(u.Touched(x, y));
    CHECK(x == 238 && y == 18);
    CHECK(u.nav.MenuOpen());
    // A drag: from its first point to its last.
    MenuView d = ListView(1, -1);
    d.items[0].action = {DragStep(72, 121, 160, 121)};
    Rig e;
    e.view = &d;
    e.Frame(kInteract);
    e.Press(kInteract, &PadState::menu);
    e.Frame(kInteract, a);
    int x0, y0, x1, y1;
    CHECK(e.Path(x0, y0, x1, y1));
    CHECK(x0 == 72 && y0 == 121 && x1 == 160 && y1 == 121);
}

static void view_content_changes()
{
    // The focused row goes (used up): the focus stays on its id when it moved, else clamps.
    MenuView v = ListView(4, -1);
    Rig r;
    r.view = &v;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::down);
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.Focus() == 2);
    MenuView w = v;
    w.items.erase(w.items.begin()); // row 0 gone: row 2 is now index 1
    for (int i = 0; i < 3; i++) w.items[i].rect.y -= 76;
    r.view = &w;
    r.Frame(kInteract);
    CHECK(r.nav.Focus() == 1);
    CHECK(w.items[r.nav.Focus()].id == 0x102);
    MenuView z = w;
    z.items.erase(z.items.begin() + 1); // the focused row itself gone: the one in its place
    r.view = &z;
    r.Frame(kInteract);
    CHECK(r.nav.Focus() == 1);
    CHECK(z.items[1].id == 0x103);
    // Its row greyed: the focus leaves it.
    MenuView gz = z;
    gz.items[1].enabled = false;
    r.view = &gz;
    r.Frame(kInteract);
    CHECK(r.nav.Focus() != 1 && MenuFocusable(gz, r.nav.Focus()));
    // A press held by the gate is dropped when the content changed before it runs (stale)…
    Rig s;
    MenuView a1 = ListView(4, -1);
    s.view = &a1;
    s.Frame(kInteract);
    s.Press(kInteract, &PadState::menu);
    PadState a;
    a.a = true;
    s.Frame(kInteract, a, false);
    s.Frame(kInteract, {}, false);
    MenuView a2 = a1;
    a2.items[0].action = {TapStep(99, 99)};
    s.view = &a2;
    s.Frame(kInteract, {}, true);
    CHECK(!s.touch.Owns());
    // …and replayed when it did not.
    Rig t;
    t.view = &a1;
    t.Frame(kInteract);
    t.Press(kInteract, &PadState::menu);
    t.Frame(kInteract, a, false);
    t.Frame(kInteract, {}, false);
    CHECK(!t.touch.Owns());
    t.Frame(kInteract, {}, true);
    int x = -1, y = -1;
    CHECK(t.Touched(x, y));
    CHECK(x == 200 && y == 50);
    // The view going invalid (a tab whose data stops reading): back to HotspotsFor.
    MenuView none;
    t.Drain();
    t.view = &none;
    t.Frame(kInteract);
    CHECK(!t.nav.ViewFocus());
    CHECK(t.nav.Focus() == 0); // HotspotsFor's first
}

static void view_scrolls_with_the_game()
{
    // A list of 3 shown rows of a longer one: Down past the last row performs the game's scroll;
    // when the rebuilt view (the game scrolled) shows the next row, the focus moves onto it.
    auto shown = [](int first) {
        MenuView v;
        v.valid = true;
        for (int i = 0; i < 3; i++)
        {
            ViewItem it;
            it.rect = {460, 210 + 76 * i, 620, 62};
            it.action = {TapStep(200, 50 + 20 * i)};
            it.group = 1;
            it.id = 0x100 + (u32)(first + i);
            v.items.push_back(it);
        }
        v.scrollGroup = 1;
        v.scrollDown = {TapStep(240, 170)};
        v.scrollUp = {TapStep(240, 40)};
        return v;
    };
    MenuView v0 = shown(0), v1 = shown(1);
    Rig r;
    r.view = &v0;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::down);
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.Focus() == 2);
    PadState d;
    d.down = true;
    r.Frame(kInteract, d);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == 240 && y == 170);
    CHECK(r.nav.Focus() == 2);
    r.Drain();
    r.view = &v1; // the game scrolled by one: id 0x102 is now row 1
    r.Frame(kInteract);
    CHECK(r.nav.Focus() == 2);
    CHECK(v1.items[2].id == 0x103);
    // Not settled: the scroll is a tap like A's, held, then replayed on the first settled frame.
    r.Frame(kInteract);
    r.Frame(kInteract, d, false);
    r.Frame(kInteract, {}, false);
    CHECK(!r.touch.Owns());
    r.Frame(kInteract, {}, true);
    CHECK(r.Touched(x, y));
    CHECK(x == 240 && y == 170);
}

// A selection the game owns (Relationships' Sim, the phone's row): the focus is held on it, and
// Up / Down are the game's own scroll whatever the geometry; the rebuilt view moves the focus.
static void view_selection_the_game_owns()
{
    auto sel = [](int selected) {
        MenuView v;
        v.valid = true;
        for (int i = 0; i < 4; i++)
        {
            ViewItem it;
            it.rect = {460, 210 + 76 * i, 620, 62};
            it.action = {TapStep(100, 40 + 30 * i)};
            it.id = 0x200 + (u32)i;
            v.items.push_back(it);
        }
        v.selected = selected;
        v.scrollUp = {TapStep(240, 40)};
        v.scrollDown = {TapStep(240, 170)};
        return v;
    };
    MenuView v1 = sel(1), v2 = sel(2);
    Rig r;
    r.view = &v1;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    CHECK(r.nav.Focus() == 1);
    PadState d;
    d.down = true;
    r.Frame(kInteract, d);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == 240 && y == 170);
    CHECK(r.nav.Focus() == 1); // the game has not moved yet
    r.Drain();
    r.Frame(kInteract);
    r.view = &v2;
    r.Frame(kInteract);
    CHECK(r.nav.Focus() == 2);
    // Up from the first row too (no geometry decides), A on the selected item.
    MenuView v0 = sel(0);
    r.view = &v0;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::up);
    CHECK(r.Touched(x, y));
    CHECK(x == 240 && y == 40);
    r.Drain();
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::a);
    CHECK(r.Touched(x, y));
    CHECK(x == 100 && y == 40);
    // Unsettled: held; the view changing meanwhile (the game moved by itself): dropped.
    r.Drain();
    r.Frame(kInteract);
    r.Frame(kInteract, d, false);
    r.Frame(kInteract, {}, false);
    r.view = &v2;
    r.Frame(kInteract, {}, true);
    CHECK(!r.touch.Owns());
}

// An item's own Left / Right (Options' sliders): before the view's Left / Right and a focus move;
// held while unsettled like a tap; the view's Left / Right held too.
static void view_item_left_right()
{
    MenuView v = ListView();
    v.items[1].right = {DragStep(232, 121, 200, 121)};
    v.items[1].left = {DragStep(232, 121, 100, 121)};
    v.buttons[(int)MenuButton::Right] = {TapStep(122, 167)};
    Rig r;
    r.view = &v;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::right); // row 0: the view's Right
    int x = -1, y = -1;
    CHECK(r.Touched(x, y));
    CHECK(x == 122 && y == 167);
    r.Drain();
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.Focus() == 1);
    PadState right;
    right.right = true;
    r.Frame(kInteract, right);
    int x0, y0, x1, y1;
    CHECK(r.Path(x0, y0, x1, y1));
    CHECK(x0 == 232 && x1 == 200);
    r.Frame(kInteract);
    // Unsettled: held, then replayed.
    PadState left;
    left.left = true;
    r.Frame(kInteract, left, false);
    r.Frame(kInteract, {}, false);
    CHECK(!r.touch.Owns());
    r.Frame(kInteract, {}, true);
    CHECK(r.Path(x0, y0, x1, y1));
    CHECK(x0 == 232 && x1 == 100);
    // The view's Left / Right held the same way (Interact's targets).
    MenuView t = ListView();
    t.buttons[(int)MenuButton::Left] = {TapStep(55, 167)};
    Rig q;
    q.view = &t;
    q.Frame(kInteract);
    q.Press(kInteract, &PadState::menu);
    q.Frame(kInteract, left, false);
    q.Frame(kInteract, {}, false);
    CHECK(!q.touch.Owns());
    q.Frame(kInteract, {}, true);
    CHECK(q.Touched(x, y));
    CHECK(x == 55 && y == 167);
    // An explicit move wins over the geometry.
    MenuView e = ListView(4, -1);
    e.items[3].next[1] = 0; // Down from the last row: back to the first
    Rig w;
    w.view = &e;
    w.Frame(kInteract);
    w.Press(kInteract, &PadState::menu);
    for (int i = 0; i < 3; i++) w.Press(kInteract, &PadState::down);
    CHECK(w.nav.Focus() == 3);
    w.Press(kInteract, &PadState::down);
    CHECK(w.nav.Focus() == 0);
}

// An item's own X (Pockets' bin) before the view's, held while unsettled; Give Gift's view
// (no menu to open): A runs the focused slot's taps, B its back (the game's X), LB / RB tap no tab.
static void view_item_x_and_gift()
{
    MenuView v = ListView(4, -1);
    v.items[1].buttonX = {TapStep(67, 167)};
    v.buttons[(int)MenuButton::X] = {TapStep(10, 10)};
    Rig r;
    r.view = &v;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::x); // row 0: the view's X
    int x = -1, y = -1;
    CHECK(r.Touched(x, y) && x == 10 && y == 10);
    r.Drain();
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::down);
    PadState px;
    px.x = true;
    r.Frame(kInteract, px, false); // unsettled: held
    r.Frame(kInteract, {}, false);
    CHECK(!r.touch.Owns());
    r.Frame(kInteract, {}, true);
    CHECK(r.Touched(x, y) && x == 67 && y == 167);

    const ScreenState gift {Screen::Gift, Tab::None};
    MenuView g;
    g.valid = g.own = true; // (a screen of its own, as GiftMenuViewOf makes it)
    for (int i = 0; i < 6; i++)
    {
        ViewItem it;
        it.rect = {200 + (i % 2) * 116, 183 + (i / 2) * 116, 104, 104};
        it.enabled = i < 2;
        if (it.enabled) it.action = {TapStep(37 + 39 * (i % 2), 50), TapStep(127, 85)};
        it.id = 0x300 + (u32)i;
        g.items.push_back(it);
    }
    g.back = {TapStep(235, 19)};
    g.initial = 1;
    Rig q;
    q.view = &g;
    q.Frame(gift);
    CHECK(q.nav.Focus() == 1);
    q.Press(gift, &PadState::rb);
    q.Press(gift, &PadState::lb);
    CHECK(!q.touch.Owns());
    q.Press(gift, &PadState::left);
    CHECK(q.nav.Focus() == 0);
    q.Press(gift, &PadState::down); // the slots below are empty: the focus stays
    CHECK(q.nav.Focus() == 0);
    q.Press(gift, &PadState::a);
    CHECK(q.Touched(x, y) && x == 37 && y == 50);
    q.Drain();
    for (int i = 0; i < 3 && !q.touch.Owns(); i++) q.Frame(gift);
    CHECK(q.Touched(x, y) && x == 127 && y == 85);
    q.Drain();
    q.Frame(gift);
    q.Press(gift, &PadState::b);
    for (int i = 0; i < 3 && !q.touch.Owns(); i++) q.Frame(gift);
    CHECK(q.Touched(x, y) && x == 235 && y == 19);
}

// The HD Save slots (a valid view, no menu to open): a focus screen, not the stick cursor's; A
// taps the focused slot's tick, B the game's X. Without a view: the cursor's, as before. Options'
// slider: Left / Right drag the game's knob (its own actions), not a focus move.
static void view_save_slots_and_slider()
{
    const ScreenState slots {Screen::SaveSlots, Tab::None};
    MenuView v;
    v.valid = v.own = true; // (as SaveSlotsViewOf makes it)
    for (int i = 0; i < 3; i++)
    {
        ViewItem it;
        it.rect = {200, 124 + i * 156, 880, 142};
        it.action = {TapStep(232, 67 + 46 * i)};
        it.id = 0x610 + (u32)i;
        v.items.push_back(it);
    }
    v.back = {TapStep(232, 22)};
    v.initial = 2;
    Rig r;
    r.view = &v;
    r.Frame(slots);
    CHECK(r.nav.Focus() == 2 && !r.nav.CursorScreen());
    r.Press(slots, &PadState::up);
    CHECK(r.nav.Focus() == 1);
    r.Press(slots, &PadState::a);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y) && x == 232 && y == 113);
    r.Drain();
    r.Frame(slots);
    r.Press(slots, &PadState::b);
    CHECK(r.Touched(x, y) && x == 232 && y == 22);
    Rig c; // no view: the stick cursor's screen, A stays the DS's without a cursor
    c.Frame(slots);
    CHECK(c.nav.CursorScreen());

    const ScreenState opt {Screen::Panel, Tab::Options};
    MenuView o;
    o.valid = true;
    ViewItem save;
    save.rect = {200, 206, 432, 80};
    save.action = {TapStep(57, 44)};
    o.items.push_back(save);
    ViewItem music;
    music.rect = {200, 302, 880, 130};
    music.left = {DragStep(232, 121, 216, 121)};
    music.next[0] = 0;
    o.items.push_back(music);
    save.next[1] = 1;
    o.items[0].next[1] = 1;
    Rig q;
    q.view = &o;
    q.Frame(opt);
    q.Press(opt, &PadState::menu);
    q.Press(opt, &PadState::down);
    CHECK(q.nav.Focus() == 1);
    q.Press(opt, &PadState::left);
    int x0, y0, x1, y1;
    CHECK(q.Path(x0, y0, x1, y1) && x0 == 232 && y0 == 121 && x1 == 216 && y1 == 121);
    q.Drain();
    q.Frame(opt);
    q.Press(opt, &PadState::right); // nothing to drag that way: no touch, the focus stays
    CHECK(!q.touch.Owns() && q.nav.Focus() == 1);
}

// An asking X (ViewItem::xAsks, Pockets' bin): the first X arms (its xArm runs, nothing else),
// the second performs; a focus move, the menu closing or the item going disarm it.
static void view_item_x_asks()
{
    MenuView v = ListView(4, -1);
    for (int i : {0, 1})
    {
        v.items[i].xAsks = true;
        v.items[i].xArm = {TapStep(60 + i, 89)};
        v.items[i].buttonX = {TapStep(60 + i, 89), TapStep(67, 167)};
    }
    Rig r;
    r.view = &v;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    int x = -1, y = -1;
    r.Press(kInteract, &PadState::x); // armed: only the slot tapped
    CHECK(r.nav.Armed() == v.items[0].id);
    CHECK(r.Touched(x, y) && x == 60 && y == 89);
    r.Drain();
    for (int i = 0; i < 4; i++) r.Frame(kInteract);
    CHECK(!r.touch.Owns() && r.nav.Armed() == v.items[0].id);
    r.Press(kInteract, &PadState::x); // the second: the slot, then the bin
    CHECK(r.nav.Armed() == 0);
    CHECK(r.Touched(x, y) && x == 60);
    r.Drain();
    for (int i = 0; i < 4 && !r.touch.Owns(); i++) r.Frame(kInteract);
    CHECK(r.Touched(x, y) && x == 67 && y == 167);
    r.Drain();
    r.Frame(kInteract);
    // A move between the two presses disarms: the next X arms again, nothing thrown.
    r.Press(kInteract, &PadState::x);
    CHECK(r.nav.Armed() == v.items[0].id);
    r.Drain();
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.Armed() == 0);
    r.Press(kInteract, &PadState::up);
    r.Press(kInteract, &PadState::x);
    CHECK(r.nav.Armed() == v.items[0].id);
    CHECK(r.Touched(x, y) && x == 60 && y == 89);
    r.Drain();
    for (int i = 0; i < 4 && !r.touch.Owns(); i++) r.Frame(kInteract);
    CHECK(!r.touch.Owns());
    // The menu closing disarms.
    r.Press(kInteract, &PadState::b);
    CHECK(r.nav.Armed() == 0);
    // The item going (another id at its place) disarms.
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::x);
    CHECK(r.nav.Armed() == v.items[0].id);
    r.Drain();
    v.items[0].id = 0x999;
    r.Frame(kInteract);
    CHECK(r.nav.Armed() == 0);
}

// A tab change disarms the asking X: arm in Pockets, R to Goals, L back to Pockets, one X arms
// again and taps no bin. MENU is B on a screen of its own (the phone, Give Gift, the Save slots,
// a note), but not on a popup (its question is B's).
static void view_tab_change_disarms_and_menu_is_b()
{
    const ScreenState pockets {Screen::Panel, Tab::Pockets};
    MenuView v = ListView(4, -1);
    v.items[0].xAsks = true;
    v.items[0].xArm = {TapStep(60, 89)};
    v.items[0].buttonX = {TapStep(60, 89), TapStep(67, 167)};
    Rig r;
    r.view = &v;
    r.Frame(pockets);
    r.Press(pockets, &PadState::menu);
    r.Press(pockets, &PadState::x);
    CHECK(r.nav.Armed() == v.items[0].id);
    r.Drain();
    for (int i = 0; i < 4; i++) r.Frame(pockets);
    r.Press(pockets, &PadState::rb);
    r.Drain();
    r.Frame(kGoals);
    CHECK(r.nav.Armed() == 0);
    r.Frame(pockets);
    CHECK(r.nav.Armed() == 0);
    r.Frame(pockets);
    r.Press(pockets, &PadState::x); // arms: the slot only, no bin
    int x = -1, y = -1;
    CHECK(r.nav.Armed() == v.items[0].id);
    CHECK(r.Touched(x, y) && x == 60 && y == 89);
    r.Drain();
    for (int i = 0; i < 6 && !r.touch.Owns(); i++) r.Frame(pockets);
    CHECK(!r.touch.Owns());

    for (ScreenState s : {kPhone, kSaveSlots, ScreenState {Screen::Gift, Tab::None}, kUnknown})
    {
        MenuView o = ListView(3, -1);
        o.own = true;
        o.back = {TapStep(235, 19)};
        Rig q;
        q.view = &o;
        q.Frame(s);
        q.Press(s, &PadState::menu);
        for (int i = 0; i < 3 && !q.touch.Owns(); i++) q.Frame(s);
        CHECK(q.Touched(x, y) && x == 235 && y == 19);
        // A popup: MENU does nothing.
        MenuView p = o;
        p.popup = true;
        Rig z;
        z.view = &p;
        z.Frame(s);
        z.Press(s, &PadState::menu);
        for (int i = 0; i < 3; i++) z.Frame(s);
        CHECK(!z.touch.Owns());
    }
}

// A selection the game owns with no arrow above it (its first entry): Up goes to the phone and
// the focus stays there; Down from the phone comes back to the selection. A view that does not
// replay (the Save slots): a press held while unsettled is dropped once it settles.
static void view_selection_phone_and_no_replay()
{
    const ScreenState goals {Screen::Panel, Tab::Goals};
    MenuView v = ListView(3, -1);
    for (ViewItem& it : v.items) it.action.clear();
    v.selected = 0;
    v.scrollGroup = 1;
    v.scrollDown = {TapStep(62, 145)};
    v.items[0].next[0] = 3; // (the frame's link: the top item Up to the phone)
    v.items[3].next[1] = 0;
    Rig r;
    r.view = &v;
    r.Frame(goals);
    r.Press(goals, &PadState::menu);
    CHECK(r.nav.Focus() == 0);
    r.Press(goals, &PadState::up);
    CHECK(r.nav.Focus() == 3 && !r.touch.Owns());
    r.Frame(goals);
    CHECK(r.nav.Focus() == 3);
    r.Press(goals, &PadState::down);
    CHECK(r.nav.Focus() == 0);
    r.Press(goals, &PadState::down); // the game's arrow
    int x = -1, y = -1;
    CHECK(r.Touched(x, y) && x == 62 && y == 145);

    const ScreenState slots {Screen::SaveSlots, Tab::None};
    MenuView s = ListView(3, -1);
    s.own = true;
    s.replay = false;
    Rig q;
    q.view = &s;
    q.Frame(slots);
    PadState pa;
    pa.a = true;
    q.Frame(slots, pa, false); // unsettled: held...
    q.Frame(slots, {}, false);
    q.Frame(slots, {}, true);  // ...and dropped
    q.Frame(slots, {}, true);
    CHECK(!q.touch.Owns());
    s.replay = true;
    q.Frame(slots, pa, false);
    q.Frame(slots, {}, true);
    CHECK(q.touch.Owns()); // (replayed where the view allows it)
}

// A popup over the open HD menu (port/popup.h): it wins the focus, the menu stays open beneath, A
// taps its button and then waits (its closing), Start is passed on; when it goes and the panel
// is back, the focus is on the row it was on. A popup over a screen of its own (the Save slots):
// the same without a screen change. The phone's view: Up / Down its keys, B its X.
static void view_popup_resume_and_phone()
{
    MenuView menu = ListView(4, -1);
    MenuView pop;
    pop.valid = pop.own = pop.popup = true;
    ViewItem close;
    close.rect = {490, 420, 300, 80};
    close.action = {TapStep(132, 132)};
    close.closes = true;
    close.id = 0x700;
    pop.items.push_back(close);
    pop.back = close.action;
    pop.backCloses = true;
    pop.passKeys = 1u << kStart;
    Rig r;
    r.view = &menu;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::down);
    r.Press(kInteract, &PadState::down);
    CHECK(r.nav.MenuOpen() && r.nav.Focus() == 2);
    r.view = &pop;
    r.Frame(kPopup);
    CHECK(r.nav.MenuOpen() && r.nav.Focus() == 0 && r.nav.ViewFocus());
    PadState st;
    st.start = true;
    CHECK(r.Frame(kPopup, st) & (1u << kStart));
    r.Frame(kPopup);
    r.Press(kPopup, &PadState::a);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y) && x == 132 && y == 132);
    r.Drain();
    r.Press(kPopup, &PadState::a); // (closing: the box slides away, a second tap would miss)
    CHECK(!r.touch.Owns());
    r.Press(kPopup, &PadState::b);
    CHECK(!r.touch.Owns());
    r.view = &menu;
    r.Frame(kInteract);
    CHECK(r.nav.MenuOpen() && r.nav.Focus() == 2);
    // A popup over the closed panel: the menu stays closed after.
    Rig c;
    c.view = &menu;
    c.Frame(kInteract);
    c.view = &pop;
    c.Frame(kPopup);
    CHECK(!c.nav.MenuOpen() && c.nav.Focus() == 0);
    c.view = &menu;
    c.Frame(kInteract);
    CHECK(!c.nav.MenuOpen());
    // Over the Save slots (no screen change): back on the slot it was on.
    MenuView slots = ListView(3, -1);
    slots.own = true;
    Rig s;
    s.view = &slots;
    s.Frame(kSaveSlots);
    s.Press(kSaveSlots, &PadState::down);
    CHECK(s.nav.Focus() == 1);
    s.view = &pop;
    s.Frame(kSaveSlots);
    CHECK(s.nav.Focus() == 0);
    s.Press(kSaveSlots, &PadState::b);
    CHECK(s.Touched(x, y) && x == 132);
    s.Drain();
    s.view = &slots;
    s.Frame(kSaveSlots);
    CHECK(s.nav.Focus() == 1);
    s.Press(kSaveSlots, &PadState::a); // (the popup went: no closing wait)
    CHECK(s.touch.Owns());
    // The phone: a selection the game owns, Up / Down its keys over frames, A keys then OK, B its X.
    MenuView ph;
    ph.valid = ph.own = true;
    for (int i = 0; i < 3; i++)
    {
        ViewItem it;
        it.rect = {400, 150 + 48 * i, 480, 48};
        it.group = 1;
        it.id = 0x800 + (u32)i;
        it.action = {KeysStep(1u << kA, 2)};
        ph.items.push_back(it);
    }
    ph.scrollGroup = 1;
    ph.selected = 0;
    ph.scrollDown = {KeysStep(1u << kDown, 2), WaitStep(4)};
    ph.back = {TapStep(237, 15)};
    ph.backCloses = true;
    Rig p;
    p.view = &ph;
    p.Frame(kPhone);
    PadState d;
    d.down = true;
    CHECK(p.Frame(kPhone, d) & (1u << kDown));
    CHECK(p.Frame(kPhone) & (1u << kDown));
    CHECK(!(p.Frame(kPhone) & (1u << kDown)));
    CHECK(p.nav.Focus() == 0);
    for (int i = 0; i < 6; i++) p.Frame(kPhone); // (its wait)
    p.Press(kPhone, &PadState::b);
    CHECK(p.Touched(x, y) && x == 237 && y == 15);
    // A popup coming while a multi-step action runs: it stops (no key under the popup).
    MenuView k = ListView(1, -1);
    k.items[0].action = {KeysStep(1u << kA, 4), KeysStep(1u << kDown, 4), TapStep(10, 20)};
    Rig q;
    q.view = &k;
    q.Frame(kInteract);
    q.Press(kInteract, &PadState::menu);
    PadState a;
    a.a = true;
    CHECK(q.Frame(kInteract, a) == (1u << kA));
    CHECK(q.nav.Running());
    q.view = &pop;
    CHECK(!(q.Frame(kPopup) & ((1u << kA) | (1u << kDown))));
    CHECK(!q.nav.Running());
}

// The HD catalogue's shape (port/catalogue.h): a screen of its own on the Catalogue screen (no
// menu to open); A on a cell taps it; Pay with no action (the game would not pay) taps nothing; a
// press held while unsettled is dropped, never replayed (a spend); B taps the X and then waits;
// Down past the grid's last row the game's arrow.
static void view_catalogue_spends_once()
{
    const ScreenState cat {Screen::Catalogue, Tab::None};
    MenuView v;
    v.valid = v.own = true;
    v.replay = false;
    v.backCloses = true;
    for (int c = 0; c < 2; c++)
    {
        ViewItem it;
        it.rect = {200 + 106 * c, 176, 96, 96};
        it.action = {TapStep(22 + 39 * c, 88)};
        it.group = 1;
        it.id = 0x100 + (u32)c;
        it.next[0] = it.next[1] = kNoMove;
        v.items.push_back(it);
    }
    ViewItem pay;
    pay.rect = {540, 466, 306, 96};
    pay.id = 0x300; // (no action: the game would not pay)
    v.items.push_back(pay);
    v.scrollGroup = 1;
    v.scrollDown = {TapStep(77, 168)};
    v.back = {TapStep(235, 19)};
    Rig r;
    r.view = &v;
    r.Frame(cat);
    CHECK(r.nav.Focus() == 0);
    r.Press(cat, &PadState::a);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y) && x == 22 && y == 88);
    r.touch = TouchScript {};
    r.Press(cat, &PadState::right);
    r.Press(cat, &PadState::right);
    CHECK(r.nav.Focus() == 2);
    r.Press(cat, &PadState::a);
    CHECK(!r.touch.Owns()); // (greyed Pay: nothing)
    r.Press(cat, &PadState::left);
    r.Press(cat, &PadState::down); // past the last row: the game's arrow
    CHECK(r.Touched(x, y) && x == 77 && y == 168);
    r.touch = TouchScript {};
    PadState pa;
    pa.a = true;
    r.Frame(cat, pa, false); // unsettled: held...
    r.Frame(cat, {}, false);
    r.Frame(cat, {}, true);  // ...and dropped
    r.Frame(cat, {}, true);
    CHECK(!r.touch.Owns());
    r.Press(cat, &PadState::b);
    CHECK(r.Touched(x, y) && x == 235 && y == 19);
}

// Two screen modules on one screen state (the Registry and its room info, both Unknown): when the
// other module's view comes the focus starts on its first item (not the old index), and when the
// first module's view is back the focus is on the item it was on, by id; a view gap between them
// (the game between its overlays) changes nothing of that. Another view of the same module keeps
// the focus by id as ever.
static void view_modules_on_one_screen()
{
    const ScreenState un {Screen::Unknown, Tab::None};
    auto list = [](u32 tag, int n, int initial) {
        MenuView v;
        v.valid = v.own = true;
        for (int i = 0; i < n; i++)
        {
            ViewItem it;
            it.rect = {200, 140 + 80 * i, 500, 70};
            it.action = {TapStep(20, 39 + 33 * i)};
            it.id = (tag << 24) + 0x100 + (u32)i;
            v.items.push_back(it);
        }
        v.initial = initial;
        return v;
    };
    const MenuView reg = list(12, 5, 0), info = list(13, 6, 1), none;
    Rig r;
    r.view = &reg;
    r.Frame(un);
    r.Press(un, &PadState::down);
    r.Press(un, &PadState::down);
    r.Press(un, &PadState::down);
    CHECK(r.nav.Focus() == 3);
    r.view = &info; // (straight over)
    r.Frame(un);
    CHECK(r.nav.Focus() == 1);
    r.Press(un, &PadState::down);
    CHECK(r.nav.Focus() == 2);
    r.view = &none; // (a gap)
    r.Frame(un);
    r.view = &reg;
    r.Frame(un);
    CHECK(r.nav.Focus() == 3);
    r.view = &info;
    r.Frame(un);
    CHECK(r.nav.Focus() == 2); // (room info's own last item)
    MenuView fewer = list(13, 6, 1);
    fewer.items.erase(fewer.items.begin()); // (the same module, a row gone: the focus by id)
    r.view = &fewer;
    r.Frame(un);
    CHECK(r.nav.Focus() == 1);
}

// A game popup over the Registry (Unknown, an own view of module 12; the popup reads as its own
// screen, or on the same one): it wins the focus, and when it goes the Registry is back on the row
// the focus was on, by id, in both cases; room info's view in between (module 13) starts at its
// own first item, and the Registry's row is restored after it too.
static void view_popup_over_the_registry()
{
    const ScreenState un {Screen::Unknown, Tab::None};
    MenuView reg;
    reg.valid = reg.own = true;
    for (int i = 0; i < 5; i++)
    {
        ViewItem it;
        it.rect = {200, 140 + 80 * i, 500, 70};
        it.action = {TapStep(20, 39 + 33 * i)};
        it.id = (12u << 24) + 0x100 + (u32)i;
        reg.items.push_back(it);
    }
    MenuView pop;
    pop.valid = pop.own = pop.popup = true;
    ViewItem close;
    close.rect = {490, 420, 300, 80};
    close.action = {TapStep(132, 132)};
    close.closes = true;
    close.id = 0x700;
    pop.items.push_back(close);
    pop.back = close.action;
    pop.backCloses = true;
    for (bool own : {false, true})
    {
        const ScreenState during = own ? kPopup : un;   // (the popup as a screen of its own, or over the same one)
        Rig r;
        r.view = &reg;
        r.Frame(un);
        r.Press(un, &PadState::down);
        r.Press(un, &PadState::down);
        CHECK(r.nav.Focus() == 2);
        r.view = &pop;
        r.Frame(during);
        CHECK(r.nav.Focus() == 0 && r.nav.ViewFocus());
        r.Press(during, &PadState::a);
        int x = -1, y = -1;
        CHECK(r.Touched(x, y) && x == 132 && y == 132);
        r.Drain();
        r.view = &reg;
        r.Frame(un);
        CHECK(r.nav.Focus() == 2);
        r.Press(un, &PadState::down);
        CHECK(r.nav.Focus() == 3);
    }
}

// A screen's page changing (Build Rooms' list, then its question): a question's focus starts on its
// initial item, whatever index it had on the old page (it never opens on Yes because the focus was
// on the list's second row), stays where the player moves it, and the list comes back on its room.
static void view_page_change_restarts_the_focus()
{
    MenuView list;
    list.valid = list.own = true;
    for (int i = 0; i < 3; i++)
    {
        ViewItem it;
        it.rect = {200, 140 + 80 * i, 500, 70};
        it.action = {TapStep(48, 68 + 35 * i)};
        it.id = (11u << 24) + 0x100 + (u32)i;
        list.items.push_back(it);
    }
    MenuView ask;
    ask.valid = ask.own = true;
    ask.page = 1;
    ask.question = true;
    for (int i = 0; i < 2; i++)
    {
        ViewItem it;
        it.rect = {380 + 300 * i, 448, 220, 96};
        it.action = {TapStep(96 + 62 * i, 130)};
        it.id = (11u << 24) + 0x200 + (u32)i;
        it.next[0] = it.next[1] = kNoMove;
        ask.items.push_back(it);
    }
    ask.initial = 0;
    Rig r;
    r.view = &list;
    r.Frame(kUnknown);
    r.Press(kUnknown, &PadState::down);
    r.Press(kUnknown, &PadState::down);
    CHECK(r.nav.Focus() == 2);
    r.view = &ask;
    r.Frame(kUnknown);
    CHECK(r.nav.Focus() == 0);
    r.Press(kUnknown, &PadState::right);
    CHECK(r.nav.Focus() == 1);
    r.Frame(kUnknown);
    CHECK(r.nav.Focus() == 1); // (the same page: kept)
    r.view = &list;
    r.Frame(kUnknown);
    CHECK(r.nav.Focus() == 2); // (back on the room the question was about)
    r.view = &ask;
    r.Frame(kUnknown);
    CHECK(r.nav.Focus() == 0); // (a question always opens on its initial item, never on the last answer)
}

// A screen's own pages (the stat panel: Unknown, an own view): LB / RB run the view's LB / RB
// actions (multi-step: a tap, a wait, a tap), held while unsettled and replayed; on the panel
// they stay its tabs.
static void view_lb_rb_pages()
{
    const ScreenState us {Screen::Unknown, Tab::None};
    MenuView v;
    v.valid = v.own = true;
    ViewItem it;
    it.rect = {508, 42, 88, 88};
    it.id = 0x0F100001;
    v.items.push_back(it);
    v.buttons[(int)MenuButton::RB] = {TapStep(228, 25), WaitStep(3), TapStep(176, 50)};
    v.buttons[(int)MenuButton::LB] = {TapStep(228, 25)};
    Rig r;
    r.view = &v;
    r.Frame(us);
    r.Press(us, &PadState::rb);
    int x = -1, y = -1;
    CHECK(r.Touched(x, y) && x == 228 && y == 25);
    r.Drain();
    for (int i = 0; i < 12 && !r.touch.Owns(); i++) r.Frame(us);
    CHECK(r.Touched(x, y) && x == 176 && y == 50);
    r.Drain();
    r.Frame(us);
    PadState lb;
    lb.lb = true;
    r.Frame(us, lb, false); // unsettled: held
    r.Frame(us, {}, false);
    CHECK(!r.touch.Owns());
    r.Frame(us, {}, true);
    CHECK(r.Touched(x, y) && x == 228 && y == 25);
    // Without the view's LB / RB: nothing.
    MenuView w = v;
    w.buttons[(int)MenuButton::RB].clear();
    Rig q;
    q.view = &w;
    q.Frame(us);
    q.Press(us, &PadState::rb);
    CHECK(!q.touch.Owns());
}

// The front end's own views (port/front.h): Start runs the view's Start action (the boot's and the
// title's touch) as A runs the item's; a press while the game is not settled is held and, the
// view never replaying, dropped; disabled items (the menu fading in): A does nothing; without a
// Start action Start does nothing on a view.
static void view_front_start()
{
    MenuView v;
    v.valid = v.own = true;
    v.replay = false;
    ViewItem it;
    it.rect = {0, 0, 1280, 720};
    it.id = 0x10000001;
    it.action = {TapStep(128, 105)};
    v.items.push_back(it);
    v.buttons[(int)MenuButton::Start] = it.action;
    int x = -1, y = -1;
    {
        Rig r;
        r.view = &v;
        r.Frame(kFrontEnd);
        r.Press(kFrontEnd, &PadState::start);
        CHECK(r.Touched(x, y) && x == 128 && y == 105);
    }
    {
        Rig r;
        r.view = &v;
        r.Frame(kFrontEnd);
        r.Press(kFrontEnd, &PadState::a);
        CHECK(r.Touched(x, y) && x == 128 && y == 105);
    }
    {
        Rig r; // unsettled: held, then dropped (never replayed)
        r.view = &v;
        r.Frame(kFrontEnd);
        PadState st;
        st.start = true;
        r.Frame(kFrontEnd, st, false);
        r.Frame(kFrontEnd, {}, false);
        r.Frame(kFrontEnd, {}, true);
        r.Frame(kFrontEnd, {}, true);
        CHECK(!r.touch.Owns());
    }
    {
        MenuView m = v; // the menu fading in: its buttons dead
        m.buttons[(int)MenuButton::Start].clear();
        m.items[0].enabled = false;
        m.items[0].action.clear();
        Rig r;
        r.view = &m;
        r.Frame(kFrontEnd);
        r.Press(kFrontEnd, &PadState::a);
        r.Press(kFrontEnd, &PadState::start);
        CHECK(!r.touch.Owns() && r.nav.Focus() < 0);
    }
}

int main()
{
    view_modules_on_one_screen();
    view_catalogue_spends_once();
    menu_opens_and_closes();
    view_save_slots_and_slider();
    view_selection_phone_and_no_replay();
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
    front_end_screens_touch_with_a();
    popup_opening_holds_a();
    view_focus_skips_greyed_and_a_runs();
    view_buttons_keys_and_sequences();
    view_content_changes();
    view_scrolls_with_the_game();
    view_selection_the_game_owns();
    view_item_left_right();
    view_item_x_and_gift();
    view_item_x_asks();
    view_tab_change_disarms_and_menu_is_b();
    view_popup_resume_and_phone();
    view_popup_over_the_registry();
    view_page_change_restarts_the_focus();
    view_lb_rb_pages();
    view_front_start();
    if (failures) { fprintf(stderr, "navigator_test: %d failure(s)\n", failures); return 1; }
    printf("navigator_test: ok\n");
    return 0;
}
