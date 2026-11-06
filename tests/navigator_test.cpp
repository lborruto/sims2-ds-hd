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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hotspots.h"
#include "navigator.h"
#include "port/interact.h"

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
    // Opened from the menu's phone icon: closing it opens the menu again on the tab it came from
    // (the game comes back on Interact: Goals' icon tapped, once the panel is there).
    r.Press(kInteract, &PadState::menu);
    r.Frame(kPhone);
    r.Press(kPhone, &PadState::menu);
    CHECK(r.Touched(x, y));
    CHECK(x == kPhoneClose.x && y == kPhoneClose.y);
    r.Drain();
    r.Frame(kInteract);
    CHECK(r.nav.MenuOpen() && !r.touch.Owns());
    r.Press(kInteract, &PadState::b);
    CHECK(!r.nav.MenuOpen());
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::rb);
    r.Drain();
    r.Frame(kGoals);
    r.Frame(kPhone);
    r.Press(kPhone, &PadState::b);
    r.Drain();
    r.Frame(kInteract);
    CHECK(!r.nav.MenuOpen()); // (drawn closed while the tab comes back)
    CHECK(r.Touched(x, y) && x == kTabIcon[2].x && y == kTabIcon[2].y);
    r.Drain();
    r.Frame(kGoals);
    CHECK(r.nav.MenuOpen());
    // From the world (not the menu): the phone's X leaves the menu closed.
    r.Press(kGoals, &PadState::b);
    r.Frame(kPhone);
    r.Press(kPhone, &PadState::b);
    r.Drain();
    r.Frame(kInteract);
    CHECK(!r.nav.MenuOpen() && !r.touch.Owns());
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

// The hotel's furniture editor (an Unknown screen, the stick cursor's): B and MENU tap its X, B
// never the DS's; on another Unknown screen B stays the DS's.
static void placement_editor_leaves()
{
    Rig r;
    ActivityRam ram;
    ram.placement = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kUnknown);
    for (bool PadState::*b : {&PadState::b, &PadState::menu})
    {
        PadState p;
        p.*b = true;
        CHECK(!(r.Frame(kUnknown, p) & (1u << kB)));
        int x, y;
        CHECK(r.Touched(x, y) && x == kPlacementX.x && y == kPlacementX.y);
        r.Drain();
        r.Frame(kUnknown);
    }
    r.nav.SetActivityRam(ActivityRam());
    PadState p;
    p.b = true;
    CHECK(r.Frame(kUnknown, p) & (1u << kB));
    CHECK(!r.touch.Owns());
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
    // B, then A: no (the question opens on its safe answer: A A never quits).
    r.Press(kCards, &PadState::b);
    r.Drain();
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y));
    CHECK(x == kCardsQuitNo.x && y == kCardsQuitNo.y);
    r.Drain();
    // B, Right, A: yes (the tick).
    r.Press(kCards, &PadState::b);
    r.Drain();
    r.Press(kCards, &PadState::right);
    CHECK(!r.touch.Owns());
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

// What RAM says on the cards (ActivityRam): a question the mouse opened is answered by A / B; the
// end-of-turn message goes on with A (a touch in its box), the other buttons waiting.
// The question answered by B while RAM shows it: RAM shows it one more frame, then not; the table
// is the controller's again (X draws, A lifts card 1).
static void cards_ram_answered()
{
    Rig r;
    r.Frame(kCards);
    int x = -1, y = -1;
    ActivityRam ram;
    ram.cards = true;
    ram.quitAsk = true;
    r.nav.SetActivityRam(ram);
    r.Press(kCards, &PadState::b);
    CHECK(r.Touched(x, y) && x == kCardsQuitNo.x && y == kCardsQuitNo.y);
    r.Drain();
    r.Frame(kCards);
    ram.quitAsk = false;
    r.nav.SetActivityRam(ram);
    r.Frame(kCards);
    r.Press(kCards, &PadState::x);
    CHECK(r.Touched(x, y) && x == HotspotsFor(kCards)[Index(kCards, "deck")].x);
    r.Drain();
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y) && x == HotspotsFor(kCards)[Index(kCards, "card 1")].x && y == HotspotsFor(kCards)[Index(kCards, "card 1")].y);
}

// The cards' question while a touch still runs (the stylus owned): Right still moves its focus to
// Yes (an edge never lost), A meanwhile is dropped; once the stylus is free, A answers Yes.
static void cards_question_moves_while_touching()
{
    Rig r;
    r.Frame(kCards);
    ActivityRam ram;
    ram.cards = true;
    ram.quitAsk = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kCards);
    CHECK(r.nav.AskFocus() == 0);
    r.touch.Tap(1, 1); // (a touch running)
    r.Press(kCards, &PadState::right);
    CHECK(r.nav.AskFocus() == 1);
    r.Drain();
    int x = -1, y = -1;
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y) && x == kCardsQuitYes.x && y == kCardsQuitYes.y);
}

static void cards_ram()
{
    Rig r;
    r.Frame(kCards);
    int x = -1, y = -1;
    ActivityRam ram;
    ram.cards = true;
    ram.message = true;
    r.nav.SetActivityRam(ram);
    r.Press(kCards, &PadState::x); // waits
    r.Press(kCards, &PadState::b);
    CHECK(!r.touch.Owns());
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y));
    CHECK(x == kCardsMessage.x && y == kCardsMessage.y);
    r.Drain();
    // The message gone: A lifts a card again.
    ram.message = false;
    r.nav.SetActivityRam(ram);
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y) && x == HotspotsFor(kCards)[Index(kCards, "card 1")].x);
    r.Drain();
    // The question opened by the mouse (B never pressed): B answers no.
    ram.quitAsk = true;
    r.nav.SetActivityRam(ram);
    r.Press(kCards, &PadState::b);
    CHECK(r.Touched(x, y) && x == kCardsQuitNo.x && y == kCardsQuitNo.y);
    r.Drain();
    // RAM still shows it one more frame after the answer, then not: the table again (X draws).
    r.Frame(kCards);
    ram.quitAsk = false;
    r.nav.SetActivityRam(ram);
    r.Frame(kCards);
    r.Press(kCards, &PadState::x);
    CHECK(r.Touched(x, y) && x == HotspotsFor(kCards)[Index(kCards, "deck")].x);
    r.Drain();
    // Nothing known (another ROM): milestone 1's way, A on the focused card.
    r.nav.SetActivityRam({});
    r.Frame(kInteract);
    r.Frame(kCards);
    r.Press(kCards, &PadState::a);
    CHECK(r.Touched(x, y) && x == HotspotsFor(kCards)[Index(kCards, "card 1")].x);
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
    // The D-pad and the stick drive; L and R stay the DS's; B taps the bowl's X (the way out), not the DS's B.
    PadState d;
    d.up = d.b = d.lb = true;
    d.lx = 1;
    u32 k = r.Frame(kVacuum, d);
    for (int key : {kUp, kRight, kL}) CHECK(k & (1u << key));
    CHECK(!(k & (1u << kB)));
    {
        int tx, ty;
        CHECK(r.Touched(tx, ty) && tx == kVacuumX.x && ty == kVacuumX.y);
        r.Drain();
        r.Frame(kVacuum, d);   // (held: once)
        CHECK(!r.touch.Owns());
        r.Frame(kVacuum);
    }
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

static const ScreenState kAutopsy {Screen::Autopsy, Tab::None};
static const ScreenState kDetector {Screen::Detector, Tab::None};
static const ScreenState kArt {Screen::Art, Tab::None};
static const ScreenState kMooGoo {Screen::MooGoo, Tab::None};
static const ScreenState kDrencher {Screen::Drencher, Tab::None};
static const ScreenState kCrime {Screen::Crime, Tab::None};

// The pointer (the pointing minigames, activities.h): the D-pad nudges it a pixel a press and
// reaches every pixel; held, it waits, then a pixel a frame, then faster; a stick moves it faster
// with a fuller deflection, a quarter with LT, never past the cap; held on the screen.
static void pointer_moves()
{
    Rig r;
    r.Frame(kArt);
    CHECK(r.nav.PointerShown() && !r.nav.PointerDown() && !r.nav.CursorScreen());
    CHECK(r.nav.PointerX() == SW / 2 && r.nav.PointerY() == SH / 2);
    // One press, one pixel (a press of 1 frame and of 3: the same).
    r.Press(kArt, &PadState::right);
    CHECK(r.nav.PointerX() == SW / 2 + 1);
    PadState d;
    d.right = true;
    for (int i = 0; i < 3; i++) r.Frame(kArt, d);
    r.Frame(kArt);
    CHECK(r.nav.PointerX() == SW / 2 + 2);
    // Held: 1, then nothing until kNudgeWait, then a pixel a frame until kNudgeFast, then kNudgeSpeed.
    float x0 = r.nav.PointerX();
    for (int i = 0; i < kNudgeFast; i++) r.Frame(kArt, d);
    CHECK(r.nav.PointerX() == x0 + 1 + (kNudgeFast - kNudgeWait));
    x0 = r.nav.PointerX();
    r.Frame(kArt, d);
    CHECK(r.nav.PointerX() == x0 + kNudgeSpeed);
    // Every pixel of the screen, both corners included, one press at a time from the edge.
    r.Frame(kArt);
    PadState ul;
    ul.left = ul.up = true;
    for (int i = 0; i < 200; i++) r.Frame(kArt, ul);
    r.Frame(kArt);
    CHECK(r.nav.PointerX() == 0 && r.nav.PointerY() == 0);
    for (int k = 1; k < SW; k++)
    {
        r.Press(kArt, &PadState::right);
        if (r.nav.PointerX() != k) { CHECK(r.nav.PointerX() == k); break; }
    }
    for (int i = 0; i < 10; i++) r.Press(kArt, &PadState::right);
    CHECK(r.nav.PointerX() == SW - 1); // (held on the screen)
    PadState dn;
    dn.down = true;
    for (int i = 0; i < 200; i++) r.Frame(kArt, dn);
    CHECK(r.nav.PointerY() == SH - 1);
    // The stick: a light touch slow, a full one kPtrMax a frame; LT a quarter; under the dead zone nothing.
    Rig s;
    s.Frame(kArt);
    PadState st;
    st.lx = 0.15f;
    s.Frame(kArt, st);
    CHECK(s.nav.PointerX() == SW / 2);
    st.lx = 0.3f;
    s.Frame(kArt, st);
    const float slow = s.nav.PointerX() - SW / 2;
    CHECK(slow > 0 && slow < 0.2f);
    float x = s.nav.PointerX();
    st.lx = -1;
    s.Frame(kArt, st);
    CHECK(std::fabs(x - s.nav.PointerX() - kPtrMax) < 1e-4f);
    x = s.nav.PointerX();
    st.lt = 1;
    s.Frame(kArt, st);
    CHECK(std::fabs(x - s.nav.PointerX() - kPtrMax * kPtrSlow) < 1e-4f);
    // The right stick too (the one further over wins).
    PadState rs;
    rs.ry = 1;
    const float y = s.nav.PointerY();
    s.Frame(kArt, rs);
    CHECK(std::fabs(s.nav.PointerY() - y - kPtrMax) < 1e-4f);
    // A held: the stylus down at the pointer, the stick capped (kPtrDrag; the autopsy's laser kPtrLaser).
    PadState ad;
    ad.a = true;
    ad.lx = 1;
    x = s.nav.PointerX();
    s.Frame(kArt, ad);
    CHECK(s.nav.PointerDown() && std::fabs(s.nav.PointerX() - x - kPtrDrag) < 1e-4f);
    s.Frame(kArt);
    CHECK(!s.nav.PointerDown());
    Rig a;
    ActivityRam ram;
    ram.autopsy = true; // (the laser, tool 0)
    a.nav.SetActivityRam(ram);
    a.Frame(kAutopsy);
    x = a.nav.PointerX();
    a.Frame(kAutopsy, ad);
    CHECK(a.nav.PointerDown() && std::fabs(a.nav.PointerX() - x - kPtrLaser) < 1e-4f);
    ram.autopsyTool = 1;
    a.nav.SetActivityRam(ram);
    x = a.nav.PointerX();
    a.Frame(kAutopsy, ad);
    CHECK(std::fabs(a.nav.PointerX() - x - kPtrDrag) < 1e-4f);
    // A new screen: the pointer back in the middle.
    a.Frame(kInteract);
    a.Frame(kArt);
    CHECK(a.nav.PointerX() == SW / 2 && a.nav.PointerY() == SH / 2);
}

// Alien Autopsy: X / Y / RB tap the tools (not while A holds the stylus); LB carries the organ
// held to the tray and lets go; B (or Start) asks, a second one quits (the game's B held), any
// other press forgets the question; Start is never the DS's.
static void autopsy_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.autopsy = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kAutopsy);
    int x, y;
    bool PadState::*const keys[3] = {&PadState::x, &PadState::y, &PadState::rb};
    for (int t = 0; t < 3; t++)
    {
        r.Press(kAutopsy, keys[t]);
        CHECK(r.Touched(x, y) && x == kAutopsyTool[t].x && y == kAutopsyTool[t].y);
        r.Drain();
    }
    PadState ax;
    ax.a = ax.x = true;
    r.Frame(kAutopsy, ax);
    CHECK(!r.touch.Owns() && r.nav.PointerDown());
    r.Frame(kAutopsy);
    // LB with an organ dragged: the pointer glides to the tray at kPtrDrag, the stylus down, then up.
    ram.autopsySub = 3;
    r.nav.SetActivityRam(ram);
    PadState a;
    a.a = true;
    r.Frame(kAutopsy, a);
    PadState alb = a;
    alb.lb = true;
    r.Frame(kAutopsy, alb);
    float last = r.nav.PointerX();
    int frames = 0;
    bool smooth = true;
    while (r.nav.PointerDown() && frames < 200)
    {
        r.Frame(kAutopsy, a);
        smooth &= std::fabs(r.nav.PointerX() - last) <= kPtrDrag + 1e-4f;
        last = r.nav.PointerX();
        frames++;
    }
    CHECK(smooth && frames < 200);
    CHECK(r.nav.PointerX() == kAutopsyTray.x && r.nav.PointerY() == kAutopsyTray.y);
    // Let go there; A still held does not press again until it is let go.
    r.Frame(kAutopsy, a);
    CHECK(!r.nav.PointerDown());
    r.Frame(kAutopsy);
    r.Frame(kAutopsy, a);
    CHECK(r.nav.PointerDown());
    r.Frame(kAutopsy);
    // B asks; A forgets it; B, B quits (B held for the game a few frames); Start is never passed on.
    ram.autopsySub = 0;
    r.nav.SetActivityRam(ram);
    PadState b;
    b.b = true;
    u32 k = r.Frame(kAutopsy, b);
    CHECK(!(k & (1u << kB)) && r.nav.QuitArmed());
    r.Frame(kAutopsy);
    r.Press(kAutopsy, &PadState::a);
    CHECK(!r.nav.QuitArmed());
    PadState sp;
    sp.start = true;
    k = r.Frame(kAutopsy, sp);
    CHECK(!(k & (1u << kStart)) && r.nav.QuitArmed());
    r.Frame(kAutopsy);
    k = r.Frame(kAutopsy, b);
    int held = 0;
    for (int i = 0; i < 10; i++) held += (r.Frame(kAutopsy) & (1u << kB)) ? 1 : 0;
    CHECK(held >= 2);
    // The question goes after kQuitWindow frames.
    r.Frame(kAutopsy, b);
    for (int i = 0; i < kQuitWindow + 1; i++) r.Frame(kAutopsy);
    CHECK(!r.nav.QuitArmed());
}

// The Metal Detector: the D-pad and the stick walk (the DS's D-pad); A digs, B powers off (their
// taps); LB / RB hold the slider's knob where RAM has it and move it, the stylus let go after.
static void detector_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.detector = true;
    ram.detectorSlider = 2048;
    r.nav.SetActivityRam(ram);
    PadState w;
    w.up = true;
    w.lx = -1;
    u32 k = r.Frame(kDetector, w);
    CHECK((k & (1u << kUp)) && (k & (1u << kLeft)));
    CHECK(!r.nav.PointerShown());
    r.Frame(kDetector);
    int x, y;
    r.Press(kDetector, &PadState::a);
    CHECK(r.Touched(x, y) && x == kDetectorDig.x && y == kDetectorDig.y);
    r.Drain();
    r.Press(kDetector, &PadState::b);
    CHECK(r.Touched(x, y) && x == kDetectorPower.x && y == kDetectorPower.y);
    r.Drain();
    PadState rb;
    rb.rb = true;
    r.Frame(kDetector, rb);
    CHECK(r.nav.PointerDown() && r.nav.PointerX() == kDetectorKnobX && r.nav.PointerY() == kDetectorKnobTop + kDetectorKnobSpan / 2);
    for (int i = 0; i < 200; i++) r.Frame(kDetector, rb);
    CHECK(r.nav.PointerDown() && r.nav.PointerY() == kDetectorKnobTop + kDetectorKnobSpan);
    r.Frame(kDetector);
    CHECK(!r.nav.PointerDown());
    PadState lb;
    lb.lb = true;
    r.Frame(kDetector, lb);
    r.Frame(kDetector, lb);
    r.Frame(kDetector, lb);
    CHECK(r.nav.PointerY() == kDetectorKnobTop + kDetectorKnobSpan / 2 - kKnobStep);
}

// MooGoo Monkey: the intro's arrow on A, B the arrow then the game's X once the game is up (through
// the black load); the player's bet: the focus on the first monkey that takes one, Right skipping
// the full and the eliminated, A its tap, B the game's X; the card step: the game's selected card,
// A its tap, B the game's own; the target step: a monkey with cards first; the question: A Yes, B No;
// a banner or the AI's turn: A, Left / Right the game's, Start its X; no stick cursor.
static void moogoo_scheme()
{
    Rig r;
    int x, y;
    auto tapped = [&](bool PadState::*b, Point p) {
        r.Press(kMooGoo, b);
        const bool ok = r.Touched(x, y) && x == p.x && y == p.y;
        r.Drain();
        return ok;
    };
    ActivityRam ram;
    ram.mooGoo = ram.mooGooIntro = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kMooGoo);
    CHECK(!r.nav.CursorScreen());
    CHECK(tapped(&PadState::a, kMooGooArrow));
    // B: the arrow, then (after the load) the game's X.
    CHECK(tapped(&PadState::b, kMooGooArrow));
    // The tap not taken: the intro persists past the wait, then A and B work again.
    {
        Rig s;
        ram.mooGooIntro = true;
        s.nav.SetActivityRam(ram);
        s.Frame(kMooGoo);
        s.Press(kMooGoo, &PadState::b);
        CHECK(s.Touched(x, y) && x == kMooGooArrow.x && y == kMooGooArrow.y);
        s.Drain();
        for (int i = 0; i < kMooGooChainWait + 5; i++) { s.Frame(kMooGoo); s.Drain(); }
        s.Press(kMooGoo, &PadState::a);
        CHECK(s.Touched(x, y) && x == kMooGooArrow.x && y == kMooGooArrow.y);
        s.Drain();
        for (int i = 0; i < kMooGooChainWait + 5; i++) { s.Frame(kMooGoo); s.Drain(); }
        s.Press(kMooGoo, &PadState::b);
        CHECK(s.Touched(x, y) && x == kMooGooArrow.x && y == kMooGooArrow.y);
    }
    r.Frame({Screen::Loading, Tab::None});
    ram.mooGooIntro = false;
    r.nav.SetActivityRam(ram);
    bool sawX = false;
    for (int i = 0; i < 40 && !sawX; i++)
    {
        r.Frame(kMooGoo);
        if (r.Touched(x, y)) { sawX = x == kMooGooX.x && y == kMooGooX.y; r.Drain(); }
    }
    CHECK(sawX);
    // Its part unreadable: B and Start tap the X, so the controller can always leave.
    {
        Rig u;
        u.nav.SetActivityRam({});
        u.Frame(kMooGoo);
        u.Press(kMooGoo, &PadState::b);
        CHECK(u.Touched(x, y) && x == kMooGooX.x && y == kMooGooX.y);
        u.Drain();
        u.Frame(kMooGoo);
        u.Press(kMooGoo, &PadState::start);
        CHECK(u.Touched(x, y) && x == kMooGooX.x && y == kMooGooX.y);
    }
    // The question.
    Rig q;
    ram.mooGooQuitAsk = true;
    q.nav.SetActivityRam(ram);
    q.Frame(kMooGoo);
    q.Press(kMooGoo, &PadState::a);   // (No first: the safe answer)
    CHECK(q.Touched(x, y) && x == kMooGooNo.x && y == kMooGooNo.y);
    q.Drain();
    q.Press(kMooGoo, &PadState::right);
    q.Press(kMooGoo, &PadState::a);
    CHECK(q.Touched(x, y) && x == kMooGooYes.x && y == kMooGooYes.y);
    q.Drain();
    q.Press(kMooGoo, &PadState::b);
    CHECK(q.Touched(x, y) && x == kMooGooNo.x && y == kMooGooNo.y);
    // The bet: monkeys 0 out, 1 full; the focus on 2, Right skips 3 (full) to 4.
    Rig b;
    ram = {};
    ram.mooGoo = ram.mooGooHuman = true;
    ram.mooGooPhase = 0xD;
    ram.mooGooSub = 1;
    for (int m = 0; m < 6; m++) ram.mooGooAlive[m] = ram.mooGooBet[m] = m >= 2 && m != 3;
    ram.mooGooAlive[1] = ram.mooGooAlive[3] = true;
    b.nav.SetActivityRam(ram);
    b.Frame(kMooGoo);
    CHECK(b.nav.Mark().x == MooGooMonkeyRect(2).x);
    PadState right;
    right.right = true;
    b.Frame(kMooGoo, right);
    b.Frame(kMooGoo);
    CHECK(b.nav.Mark().x == MooGooMonkeyRect(4).x);
    b.Press(kMooGoo, &PadState::a);
    CHECK(b.Touched(x, y) && x == 68 + 32 * 4 && y == 48);
    b.Drain();
    b.Press(kMooGoo, &PadState::b);
    CHECK(b.Touched(x, y) && x == kMooGooX.x && y == kMooGooX.y);
    b.Drain();
    // The cards: the game's selected one; B is the game's (undo the bet), no tap.
    ram.mooGooSub = 3;
    for (int i = 0; i < 5; i++) ram.mooGooCard[i] = i != 1;
    ram.mooGooSelected = 3;
    b.nav.SetActivityRam(ram);
    b.Frame(kMooGoo);
    CHECK(b.nav.Mark().x == MooGooCardRect(3).x && b.nav.Mark().y == 137);
    PadState bb;
    bb.b = true;
    CHECK(b.Frame(kMooGoo, bb) & (1u << kB));
    b.Frame(kMooGoo);
    CHECK(!b.touch.Owns());
    b.Press(kMooGoo, &PadState::a);
    CHECK(b.Touched(x, y) && x == 84 + 32 * 3 && y == 153);
    b.Drain();
    // The target: a monkey with cards first.
    ram.mooGooSub = 5;
    ram.mooGooPile[4] = true;
    b.nav.SetActivityRam(ram);
    b.Frame(kMooGoo);
    CHECK(b.nav.Mark().x == MooGooPileRect(4).x && b.nav.Mark().y == 88);
    // A banner: A and Left / Right the game's own; Start its X.
    ram.mooGooPhase = 9;
    b.nav.SetActivityRam(ram);
    PadState a;
    a.a = a.left = true;
    const u32 k = b.Frame(kMooGoo, a);
    CHECK((k & (1u << kA)) && (k & (1u << kLeft)) && b.nav.Mark().w == 0);
    b.Frame(kMooGoo);
    b.Press(kMooGoo, &PadState::start);
    CHECK(b.Touched(x, y) && x == kMooGooX.x && y == kMooGooX.y);
    b.Drain();
    // The human's bet placed (sub 2): B is the game's own (no tap); the next step starts its focus afresh.
    ram.mooGooPhase = 0xD;
    ram.mooGooSub = 2;
    b.nav.SetActivityRam(ram);
    bb.b = true;
    CHECK(b.Frame(kMooGoo, bb) & (1u << kB));
    b.Frame(kMooGoo);
    CHECK(!b.touch.Owns());
    ram.mooGooSub = 1;
    b.nav.SetActivityRam(ram);
    b.Frame(kMooGoo);
    CHECK(b.nav.Mark().x == MooGooMonkeyRect(2).x);   // (the first monkey that takes a bet, not the hand's index)
}

// The Super Drencher: walk on the stick / D-pad, A held fires, LB runs (the game's B); X held pumps
// (the stylus on the band, the press landing first, then swept at kPumpSpeed between the ends);
// Y the target, RB refill, B the X.
static void drencher_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.drencher = true;
    ram.drencherPump = 20;
    r.nav.SetActivityRam(ram);
    PadState p;
    p.a = p.lb = p.left = true;
    const u32 k = r.Frame(kDrencher, p);
    CHECK((k & (1u << kA)) && (k & (1u << kB)) && (k & (1u << kLeft)));
    CHECK(!r.nav.PointerShown() && !r.nav.CursorScreen());
    PadState x;
    x.x = true;
    r.Frame(kDrencher, x);
    CHECK(r.nav.PointerDown() && r.nav.PointerX() == 70 && r.nav.PointerY() == kPumpY);
    r.Frame(kDrencher, x);
    CHECK(r.nav.PointerX() == 70);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 100; i++)
    {
        r.Frame(kDrencher, x);
        lo = std::min(lo, r.nav.PointerX());
        hi = std::max(hi, r.nav.PointerX());
        CHECK(r.nav.PointerDown() && r.nav.PointerY() == kPumpY);
    }
    CHECK(lo == kPumpLeft && hi == kPumpRight);
    r.Frame(kDrencher);
    CHECK(!r.nav.PointerDown());
    int tx, ty;
    auto tapped = [&](bool PadState::*b, Point q) {
        r.Press(kDrencher, b);
        const bool ok = r.Touched(tx, ty) && tx == q.x && ty == q.y;
        r.Drain();
        return ok;
    };
    CHECK(tapped(&PadState::y, kDrencherTarget));
    CHECK(tapped(&PadState::rb, kDrencherRefill));
    CHECK(tapped(&PadState::b, kDrencherX));
    // Start taps the "?" (help); in its mode each button taps its own control (the game explains
    // it), A not firing; B or Start the "?" again (not the X); a help text closes on any press.
    CHECK(tapped(&PadState::start, kDrencherHelp));
    ram.drencherHelp = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kDrencher);
    CHECK(tapped(&PadState::b, kDrencherHelp));
    CHECK(tapped(&PadState::start, kDrencherHelp));
    CHECK(tapped(&PadState::y, kDrencherTarget));
    CHECK(tapped(&PadState::x, kDrencherPumpBand));
    CHECK(tapped(&PadState::rb, kDrencherRefill));
    {
        PadState fire;
        fire.a = true;
        CHECK(!(r.Frame(kDrencher, fire) & (1u << kA)));
        CHECK(r.Touched(tx, ty) && tx == kDrencherFire.x && ty == kDrencherFire.y);
        r.Drain();
        r.Frame(kDrencher);
    }
    ram.drencherHelpText = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kDrencher);
    CHECK(tapped(&PadState::a, kDrencherHelp));
    CHECK(tapped(&PadState::y, kDrencherHelp));
    CHECK(tapped(&PadState::start, kDrencherHelp));
}

// The crime game: A held grabs the centre rat (the pointer shown, the stylus down), Right aims along
// its row up to its end, letting A go lets the stylus up; X scares, Y subdues, B the X; LB runs.
static void crime_scheme()
{
    Rig r;
    PadState a;
    a.a = true;
    r.Frame(kCrime, a);
    CHECK(r.nav.PointerDown() && r.nav.PointerShown() && r.nav.PointerX() == kCrimeRat.x && r.nav.PointerY() == kCrimeRat.y);
    a.right = true;
    const u32 k = r.Frame(kCrime, a);
    CHECK(!(k & (1u << kRight)));   // (aiming: the D-pad does not walk)
    for (int i = 0; i < 200; i++) r.Frame(kCrime, a);
    CHECK(r.nav.PointerDown() && r.nav.PointerX() == kCrimeAimMax && r.nav.PointerY() == kCrimeRat.y);
    r.Frame(kCrime);
    CHECK(!r.nav.PointerDown() && !r.nav.PointerShown());
    PadState w;
    w.right = w.lb = true;
    const u32 k2 = r.Frame(kCrime, w);
    CHECK((k2 & (1u << kRight)) && (k2 & (1u << kB)));
    r.Frame(kCrime);
    int x, y;
    auto tapped = [&](bool PadState::*b, Point q) {
        r.Press(kCrime, b);
        const bool ok = r.Touched(x, y) && x == q.x && y == q.y;
        r.Drain();
        return ok;
    };
    CHECK(tapped(&PadState::x, kCrimeScare));
    CHECK(tapped(&PadState::y, kCrimeSubdue));
    CHECK(tapped(&PadState::b, kCrimeX));
    // Start taps the "?" (help); in its mode X / A / Y tap scare, the rat, subdue (no grab), B or
    // Start the "?" again; a help text closes on any press; A still held after it grabs nothing.
    CHECK(tapped(&PadState::start, kCrimeHelp));
    ActivityRam ram;
    ram.crime = ram.crimeHelp = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kCrime);
    CHECK(tapped(&PadState::x, kCrimeScare));
    CHECK(tapped(&PadState::y, kCrimeSubdue));
    CHECK(tapped(&PadState::b, kCrimeHelp));
    CHECK(tapped(&PadState::start, kCrimeHelp));
    {
        PadState held;
        held.a = true;
        r.Frame(kCrime, held);
        CHECK(r.Touched(x, y) && x == kCrimeRat.x && y == kCrimeRat.y && !r.nav.PointerShown());
        r.Drain();
        ram.crimeHelp = false;
        r.nav.SetActivityRam(ram);
        for (int i = 0; i < 5; i++) r.Frame(kCrime, held);
        CHECK(!r.nav.PointerDown() && !r.touch.Owns());   // (the help's A, still held: no grab)
        r.Frame(kCrime);
    }
    ram.crimeHelp = ram.crimeHelpText = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kCrime);
    CHECK(tapped(&PadState::a, kCrimeHelp));
    CHECK(tapped(&PadState::rb, kCrimeHelp));
}

static const ScreenState kScratch {Screen::Scratch, Tab::None};
static const ScreenState kMixer {Screen::Mixer, Tab::None};
static const ScreenState kMixerSave {Screen::MixerSave, Tab::None};
static const ScreenState kTrading {Screen::Trading, Tab::None};

// The Scratch lottery: the pointer and A held scratch; X rubs the box under the pointer (or the
// nearest covered one): the stylus pressed inside it, swept in passes kRubStep apart over its
// whole square, let go when RAM reveals it; a popup's A / B / Start are the game's; B asks, a
// second B taps the ticket's X (tier 0 and 2's).
static void scratch_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.scratch = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kScratch);
    CHECK(r.nav.PointerShown() && !r.nav.CursorScreen());
    PadState a;
    a.a = true;
    a.lx = 1;
    float x = r.nav.PointerX();
    r.Frame(kScratch, a);
    CHECK(r.nav.PointerDown() && std::fabs(r.nav.PointerX() - x - kPtrDrag) < 1e-4f);
    r.Frame(kScratch);
    // The box centres the game puts on each tier; the rub's path over a box stays inside its square.
    CHECK(ScratchCentre(0, 0).x == 95 && ScratchCentre(0, 3).x == 216 && ScratchCentre(0, 7).x == 215 && ScratchCentre(0, 7).y == 111);
    CHECK(ScratchCentre(1, 4).x == 104 && ScratchCentre(1, 4).y == 112 && ScratchCentre(2, 3).x == 223 && ScratchCentre(2, 3).y == 63);
    CHECK(ScratchX(0).x == 232 && ScratchX(2).y == 167);
    for (int t = 0; t < 3; t++)
        for (int i = 0; i < 8; i++)
        {
            const Point c = ScratchCentre(t, i);
            const std::vector<Point> p = ScratchRub(t, i);
            CHECK(p.size() == 2 * (2 * kRubHalf / kRubStep + 1));
            for (Point q : p) CHECK(std::abs(q.x - c.x) <= kRubHalf && std::abs(q.y - c.y) <= kRubHalf);
        }
    // X from the middle: the nearest covered box is 5 (135,111); box 5 revealed: box 1 (135,63).
    Rig b;
    ram.scratchOpen[5] = true;
    b.nav.SetActivityRam(ram);
    b.Frame(kScratch);
    b.Press(kScratch, &PadState::x);
    const Point start = ScratchRub(0, 1)[0];
    int f = 0, downs = 0, minY = 999, maxY = -1;
    for (; f < 400 && !b.nav.PointerDown(); f++) b.Frame(kScratch);
    CHECK(b.nav.PointerX() == start.x && b.nav.PointerY() == start.y);   // (pressed on the path's start, inside the box)
    for (; f < 800 && b.nav.PointerDown(); f++)
    {
        downs++;
        minY = std::min(minY, (int)b.nav.PointerY());
        maxY = std::max(maxY, (int)b.nav.PointerY());
        b.Frame(kScratch);
    }
    CHECK(downs > 100 && minY == 63 - kRubHalf && maxY == 63 + kRubHalf && !b.nav.PointerDown());
    // RAM revealing the box ends it at once; A stops it too.
    Rig c;
    ram.scratchOpen[5] = false;
    c.nav.SetActivityRam(ram);
    c.Frame(kScratch);
    c.Press(kScratch, &PadState::x);
    for (int i = 0; i < 40; i++) c.Frame(kScratch);
    CHECK(c.nav.PointerDown());
    ram.scratchOpen[5] = true;
    c.nav.SetActivityRam(ram);
    c.Frame(kScratch);
    CHECK(!c.nav.PointerDown());
    ram.scratchOpen[5] = false;
    c.nav.SetActivityRam(ram);
    c.Press(kScratch, &PadState::x);
    for (int i = 0; i < 40; i++) c.Frame(kScratch);
    c.Press(kScratch, &PadState::a);
    c.Frame(kScratch);
    CHECK(!c.nav.PointerDown());
    // B asks, a second taps the X; another press forgets the question; Start as B, never the DS's.
    int tx, ty;
    r.Press(kScratch, &PadState::b);
    CHECK(r.nav.QuitArmed() && !r.touch.Owns());
    r.Press(kScratch, &PadState::x);
    r.Drain();
    for (int i = 0; i < 200; i++) r.Frame(kScratch);
    r.Drain();
    CHECK(!r.nav.QuitArmed());
    PadState st;
    st.start = true;
    CHECK(!(r.Frame(kScratch, st) & (1u << kStart)));
    r.Frame(kScratch);
    CHECK(r.nav.QuitArmed());
    r.Press(kScratch, &PadState::b);
    CHECK(r.Touched(tx, ty) && tx == ScratchX(0).x && ty == ScratchX(0).y);
    r.Drain();
    // A popup of the game's: A, B and Start are its own (no question, no tap, no stylus).
    ram.scratchPopup = true;
    r.nav.SetActivityRam(ram);
    PadState all;
    all.a = all.b = all.start = true;
    const u32 k = r.Frame(kScratch, all);
    CHECK((k & (1u << kA)) && (k & (1u << kB)) && (k & (1u << kStart)) && !r.nav.PointerDown() && !r.touch.Owns() && !r.nav.QuitArmed());
    // (A still held when it has gone: no stylus until A is let go.)
    ram.scratchPopup = false;
    r.nav.SetActivityRam(ram);
    PadState held;
    held.a = true;
    r.Frame(kScratch, held);
    CHECK(!r.nav.PointerDown());
}

// The music mixer: the D-pad is the game's own (held: octave, sustain, PREP); X PLAY, RB RECORD, LB
// GENRE, B QUIT (stop while it plays or records); Y taps VOX and holds the microphone, its release
// taps it again while the game records; the song's list (7): Up / Down a mark over its rows, A
// picks, B cancels; the save question (0xA): A Yes, B No, X Preview; help: B the ?.
static void mixer_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.mixer = true;
    ram.mixerSongs = 6;
    r.nav.SetActivityRam(ram);
    r.Frame(kMixer);
    CHECK(r.nav.PointerShown() && !r.nav.CursorScreen());
    PadState d;
    d.up = d.right = true;
    const u32 k = r.Frame(kMixer, d);
    CHECK((k & (1u << kUp)) && (k & (1u << kRight)) && r.nav.PointerX() == SW / 2);   // (the D-pad is the game's, not the pointer's)
    r.Frame(kMixer);
    int x, y;
    auto tapped = [&](bool PadState::*b, Point q) {
        r.Press(kMixer, b);
        const bool ok = r.Touched(x, y) && x == q.x && y == q.y;
        r.Drain();
        return ok;
    };
    CHECK(tapped(&PadState::x, kMixPlay));
    CHECK(tapped(&PadState::rb, kMixRecord));
    CHECK(tapped(&PadState::lb, kMixGenre));
    CHECK(tapped(&PadState::b, kMixQuit));
    ram.mixerPhase = 1;
    r.nav.SetActivityRam(ram);
    CHECK(tapped(&PadState::b, kMixPlay));
    ram.mixerPhase = 2;
    r.nav.SetActivityRam(ram);
    CHECK(tapped(&PadState::b, kMixRecord));
    ram.mixerPhase = 5;
    r.nav.SetActivityRam(ram);
    CHECK(tapped(&PadState::b, kMixVox));   // (the voice recording: B stops it, never quits)
    ram.mixerPhase = 2;
    r.nav.SetActivityRam(ram);
    PadState st;
    st.start = true;
    CHECK(!(r.Frame(kMixer, st) & (1u << kStart)));
    r.Frame(kMixer);
    // Y: VOX tapped from the idle mixer, the microphone held; the release taps it again while it records.
    ram.mixerPhase = 0;
    r.nav.SetActivityRam(ram);
    PadState yy;
    yy.y = true;
    r.Frame(kMixer, yy);
    CHECK(r.Touched(x, y) && x == kMixVox.x && y == kMixVox.y && r.nav.Voice() && !r.nav.Blow());
    r.Drain();
    ram.mixerPhase = 5;
    r.nav.SetActivityRam(ram);
    for (int i = 0; i < 30; i++) r.Frame(kMixer, yy);
    CHECK(r.nav.Voice() && !r.touch.Owns());
    r.Frame(kMixer);
    CHECK(!r.nav.Voice() && r.Touched(x, y) && x == kMixVox.x);
    r.Drain();
    // The song's list.
    ram.mixerPhase = 7;
    r.nav.SetActivityRam(ram);
    r.Frame(kMixer);
    CHECK(r.nav.Mark().y == MixerRowRect(0).y && !r.nav.PointerShown());
    for (int i = 0; i < 9; i++) r.Press(kMixer, &PadState::down);
    CHECK(r.nav.Mark().y == MixerRowRect(5).y);   // (held at the list's last song)
    r.Press(kMixer, &PadState::up);
    CHECK(tapped(&PadState::a, MixerRow(4)));
    CHECK(tapped(&PadState::b, kMixGenre));
    // The save question and help.
    ram.mixerPhase = 0xA;
    r.nav.SetActivityRam(ram);
    CHECK(tapped(&PadState::a, kMixNo));    // (No first: the safe answer)
    r.Press(kMixer, &PadState::right);
    CHECK(tapped(&PadState::a, kMixYes));
    CHECK(tapped(&PadState::b, kMixNo));
    CHECK(tapped(&PadState::x, kMixPreview));
    ram.mixerPhase = 8;
    r.nav.SetActivityRam(ram);
    CHECK(tapped(&PadState::b, kMixHelp));
    CHECK(tapped(&PadState::start, kMixHelp));
    ram.mixerPhase = 9;
    r.nav.SetActivityRam(ram);
    CHECK(tapped(&PadState::b, kMixHelp));
    // Start from the idle mixer: its "?" (help).
    ram.mixerPhase = 0;
    r.nav.SetActivityRam(ram);
    r.Frame(kMixer);
    CHECK(tapped(&PadState::start, kMixHelp));
    ram.mixerPhase = 1;   // (playing: Start nothing)
    r.nav.SetActivityRam(ram);
    r.Press(kMixer, &PadState::start);
    CHECK(!r.touch.Owns());
}

// The mixer's save page: the D-pad moves the focus over its table (from the first key), A taps the
// focused target, a used slot asks first (A again saves, a move forgets it), Y backspace, B the X.
static void mixer_save_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.mixerSave = true;
    ram.mixerSaveUsed[0] = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kMixerSave);
    const std::vector<Hotspot>& h = HotspotsFor(kMixerSave);
    CHECK(r.nav.Focus() == 0 && !r.nav.PointerShown() && !r.nav.CursorScreen());
    int x, y;
    r.Press(kMixerSave, &PadState::a);
    CHECK(r.Touched(x, y) && x == h[0].x && y == h[0].y);
    r.Drain();
    r.Press(kMixerSave, &PadState::right);
    CHECK(r.nav.Focus() == 1);
    r.Press(kMixerSave, &PadState::y);
    CHECK(r.Touched(x, y) && x == 94 && y == 174);
    r.Drain();
    r.Press(kMixerSave, &PadState::b);
    CHECK(r.Touched(x, y) && x == 235 && y == 18);
    r.Drain();
    // To slot 1 (the right column): asked first, then saved.
    for (int i = 0; i < 12 && strncmp(h[r.nav.Focus()].name, "slot ", 5); i++) r.Press(kMixerSave, &PadState::right);
    CHECK(!strcmp(h[r.nav.Focus()].name, "slot 2"));   // (the top row's Right: the nearer slot)
    r.Press(kMixerSave, &PadState::up);
    CHECK(!strcmp(h[r.nav.Focus()].name, "slot 1"));
    r.Press(kMixerSave, &PadState::a);
    CHECK(!r.touch.Owns() && r.nav.QuitArmed());
    r.Press(kMixerSave, &PadState::a);
    CHECK(r.Touched(x, y) && x == 235 && y == 57 && !r.nav.QuitArmed());
    r.Drain();
    r.Press(kMixerSave, &PadState::a);
    CHECK(r.nav.QuitArmed());
    r.Press(kMixerSave, &PadState::down);
    CHECK(!r.nav.QuitArmed() && !strcmp(h[r.nav.Focus()].name, "slot 2"));
    r.Press(kMixerSave, &PadState::a);   // (an empty slot: at once)
    CHECK(r.Touched(x, y) && x == 235 && y == 109);
    r.Drain();
}

// The wireless lobby: B its X, nothing else reaches the game.
static void trading_scheme()
{
    Rig r;
    r.Frame(kTrading);
    PadState all;
    all.a = all.x = all.y = all.start = all.select = all.up = true;
    CHECK(r.Frame(kTrading, all) == 0 && !r.touch.Owns());
    r.Frame(kTrading);
    int x, y;
    r.Press(kTrading, &PadState::b);
    CHECK(r.Touched(x, y) && x == kTradingX.x && y == kTradingX.y);
    CHECK(!r.nav.CursorScreen() && !r.nav.PointerShown());
}

// The Art Gallery: LB / RB the previous / next tool in the ring of ten, X the next swatch, Y undo,
// B the game's X; its question: A the tick, B the cross, the rest waiting.
static void art_scheme()
{
    Rig r;
    ActivityRam ram;
    ram.art = true;
    ram.artButton = 0;
    ram.artSwatch = 14;
    r.nav.SetActivityRam(ram);
    r.Frame(kArt);
    int x, y;
    auto tapped = [&](bool PadState::*b, Point p) {
        r.Press(kArt, b);
        const bool ok = r.Touched(x, y) && x == p.x && y == p.y;
        r.Drain();
        return ok;
    };
    CHECK(tapped(&PadState::lb, ArtTool(9)));
    CHECK(tapped(&PadState::rb, ArtTool(1)));
    CHECK(tapped(&PadState::x, ArtSwatch(0)));
    CHECK(tapped(&PadState::y, kArtUndo));
    CHECK(tapped(&PadState::b, kArtDone));
    CHECK(tapped(&PadState::start, kArtDone));      // (Start too: the game reads no key here)
    CHECK(tapped(&PadState::menu, kArtPageNext));   // MENU: the palette's next page
    // The buttons the game draws: the ten tools in two columns, the fifteen swatches in two rows.
    CHECK(ArtTool(0).x == 14 && ArtTool(5).y == 144 && ArtTool(6).x == 241 && ArtTool(6).y == 66 && ArtTool(9).y == 144);
    CHECK(ArtSwatch(5).x == 180 && ArtSwatch(5).y == 165 && ArtSwatch(7).x == 43 && ArtSwatch(14).y == 187);
    ram.askUp = true;
    ram.askYes = {164, 116};
    ram.askNo = {92, 116};
    r.nav.SetActivityRam(ram);
    r.Press(kArt, &PadState::y);
    CHECK(!r.touch.Owns());
    CHECK(!tapped(&PadState::a, ram.askYes));   // (opening or closing: not yet / no more)
    ram.ask = true;
    r.nav.SetActivityRam(ram);
    r.Frame(kArt);
    CHECK(tapped(&PadState::a, ram.askNo));   // (No first: the safe answer)
    r.Press(kArt, &PadState::right);
    CHECK(tapped(&PadState::a, ram.askYes));
    CHECK(tapped(&PadState::b, ram.askNo));
    PadState a;
    a.a = true;
    r.Frame(kArt, a);
    CHECK(!r.nav.PointerDown());
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

// An item whose A starts an animation on the world (the slot machine's Insert Coin, research/m6 12):
// its tap runs and the panel's menu closes with it, so the world shows big; the others keep it open.
static void view_animation_closes_the_menu()
{
    Rig r;
    MenuView v = ListView();
    v.items[1].closesMenu = true;
    r.view = &v;
    r.Frame(kInteract);
    r.Press(kInteract, &PadState::menu);
    r.Press(kInteract, &PadState::a);   // (row 0: the menu stays)
    CHECK(r.nav.MenuOpen());
    r.Drain();
    for (int i = 0; i < 4; i++) r.Frame(kInteract);
    r.Press(kInteract, &PadState::down);
    r.Press(kInteract, &PadState::a);
    int x, y;
    CHECK(!r.nav.MenuOpen() && r.Touched(x, y) && x == 201 && y == 51);
    r.Drain();
    CHECK(InteractAnimation(0x18) && InteractAnimation(0x32) && InteractAnimation(0x33) && InteractAnimation(0xC) && !InteractAnimation(0x62));
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

// A slider's fast presses (ViewItem::queues): none dropped while a drag runs; each owed step runs
// in the order pressed once the game has read the last one back (its Left / Right rebuilt from the
// new value), past an end a step does nothing, and the queue runs on when the focus moves on.
static void slider_presses_queue()
{
    const ScreenState opt {Screen::Panel, Tab::Options};
    int value = 5;
    MenuView v;
    auto build = [&]() {
        v = MenuView();
        v.valid = v.own = true;
        ViewItem s;
        s.rect = {200, 300, 880, 130};
        s.id = 0x7000010;
        s.queues = true;
        if (value > 0) s.left = {DragStep(72 + 16 * value, 121, 72 + 16 * (value - 1), 121)};
        if (value < 10) s.right = {DragStep(72 + 16 * value, 121, 72 + 16 * (value + 1), 121)};
        for (int& n : s.next) n = kNoMove;
        s.next[1] = 1;
        v.items.push_back(s);
        ViewItem other;
        other.rect = {200, 460, 880, 130};
        other.id = 0x7000011;
        other.next[0] = 0;
        v.items.push_back(other);
    };
    build();
    Rig r;
    r.view = &v;
    r.Frame(opt);
    CHECK(r.nav.Focus() == 0);
    // Left, Left, Right pressed while the first drag runs: one drag now.
    r.Press(opt, &PadState::left);
    r.Press(opt, &PadState::left);
    r.Press(opt, &PadState::right);
    int x0, y0, x1, y1;
    CHECK(r.Path(x0, y0, x1, y1) && x0 == 72 + 16 * 5 && x1 == 72 + 16 * 4);
    // The game has not read it back yet: nothing more.
    r.Frame(opt);
    CHECK(!r.touch.Owns());
    value = 4; build();
    r.Frame(opt);
    CHECK(r.Path(x0, y0, x1, y1) && x0 == 72 + 16 * 4 && x1 == 72 + 16 * 3);
    value = 3; build();
    // The focus moves on: the owed Right still runs on the slider.
    r.Press(opt, &PadState::down);
    CHECK(r.nav.Focus() == 1);
    CHECK(r.Path(x0, y0, x1, y1) && x0 == 72 + 16 * 3 && x1 == 72 + 16 * 4);
    value = 4; build();
    for (int i = 0; i < 40; i++) r.Frame(opt);
    CHECK(!r.touch.Owns());
    // Past an end: the press does nothing, the next one still counts.
    r.Press(opt, &PadState::up);
    value = 10; build();
    r.Frame(opt);
    r.Press(opt, &PadState::right);
    CHECK(!r.touch.Owns());
    r.Press(opt, &PadState::left);
    CHECK(r.Path(x0, y0, x1, y1) && x0 == 72 + 16 * 10 && x1 == 72 + 16 * 9);
}

// A question (a popup's Yes / No) opens on No whatever the focus beneath was; Yes is one Right
// away; when it goes, the screen beneath gets its item back (FocusFor: on the first frame too).
static void question_opens_on_no()
{
    const ScreenState opt {Screen::Panel, Tab::Options};
    MenuView panel;
    panel.valid = true;
    for (int i = 0; i < 2; i++)
    {
        ViewItem it;
        it.rect = {200 + 440 * i, 206, 432, 80};
        it.action = {TapStep(57 + 177 * i, 44)};
        it.id = 0x7000001 + (u32)i;
        panel.items.push_back(it);
    }
    MenuView q;
    q.valid = q.own = q.popup = q.question = true;
    q.replay = false;
    for (int i = 0; i < 2; i++)
    {
        ViewItem it;
        it.rect = {348 + 400 * i, 420, 260, 72};
        it.action = {TapStep(92 + 72 * i, 116)};
        it.id = 0x8000702 - (u32)i; // (No, Yes)
        it.closes = true;
        q.items.push_back(it);
    }
    q.back = {TapStep(92, 116)};
    q.initial = 0;
    Rig r;
    r.view = &panel;
    r.Frame(opt);
    r.Press(opt, &PadState::menu);
    r.Press(opt, &PadState::right);
    CHECK(r.nav.Focus() == 1);
    CHECK(r.nav.FocusFor(q) == 0); // (the first frame the popup is drawn)
    r.view = &q;
    r.Frame(opt);
    CHECK(r.nav.Focus() == 0);
    r.Press(opt, &PadState::a);
    int x, y;
    CHECK(r.Touched(x, y) && x == 92 && y == 116);
    r.Drain();
    CHECK(r.nav.FocusFor(panel) == 1); // (the frame the popup has gone: Quit, not Save)
    r.view = &panel;
    r.Frame(opt);
    CHECK(r.nav.Focus() == 1 && r.nav.MenuOpen());
    // Again, Right then A: Yes.
    r.view = &q;
    r.Frame(opt);
    CHECK(r.nav.Focus() == 0);
    r.Press(opt, &PadState::right);
    for (int i = 0; i < 100; i++) r.Frame(opt); // (No's closing wait over)
    CHECK(r.nav.Focus() == 1);
    r.Press(opt, &PadState::a);
    CHECK(r.Touched(x, y) && x == 164 && y == 116);
}

// A list page left for its question and back without the item it was on (Build Rooms' room
// just bought): the focus on the item now in its place, never the list's top.
static void page_back_without_its_item()
{
    const ScreenState build {Screen::Build, Tab::None};
    auto list = [](std::vector<u32> ids) {
        MenuView v;
        v.valid = v.own = true;
        for (size_t i = 0; i < ids.size(); i++)
        {
            ViewItem it;
            it.rect = {200, 150 + (int)i * 110, 600, 100};
            it.action = {TapStep(48, 68 + 35 * (int)i)};
            it.id = ids[i];
            v.items.push_back(it);
        }
        return v;
    };
    MenuView l = list({0x0B000001, 0x0B000002, 0x0B000003});
    MenuView q = list({0x0B000101, 0x0B000102});
    q.page = 1;
    q.question = true;
    q.initial = 0;
    Rig r;
    r.view = &l;
    r.Frame(build);
    r.Press(build, &PadState::down);
    CHECK(r.nav.Focus() == 1);
    r.view = &q;
    r.Frame(build);
    CHECK(r.nav.Focus() == 0);
    MenuView after = list({0x0B000001, 0x0B000003});
    r.view = &after;
    r.Frame(build);
    CHECK(r.nav.Focus() == 1 && after.items[1].id == 0x0B000003);
}

// B's safe landing (MenuView::backFocus, Keep this Sim?'s cross): one B moves the focus there,
// B again there runs back.
static void back_focus_first()
{
    const ScreenState cas {Screen::CreateASim, Tab::None};
    MenuView v;
    v.valid = v.own = true;
    for (int i = 0; i < 3; i++)
    {
        ViewItem it;
        it.rect = {700 + 120 * i, 400, 100, 80};
        it.action = {TapStep(80 + 40 * i, 170)};
        it.id = 0x12000000 + (u32)i;
        v.items.push_back(it);
    }
    v.back = {TapStep(160, 170)};
    v.backFocus = 2;
    v.initial = 0;
    Rig r;
    r.view = &v;
    r.Frame(cas);
    CHECK(r.nav.Focus() == 0);
    r.Press(cas, &PadState::b);
    CHECK(r.nav.Focus() == 2 && !r.touch.Owns());
    r.Press(cas, &PadState::b);
    int x, y;
    CHECK(r.Touched(x, y) && x == 160 && y == 170);

    // A big picture (MenuView::picture: Settings' concept art): shown big once an A ran; the first
    // B closes it (no tap), the next one is the screen's B; LB / RB close it too.
    const ScreenState fe {Screen::FrontEnd, Tab::None};
    MenuView p;
    p.valid = p.own = p.picture = true;
    ViewItem cell;
    cell.rect = {840, 300, 88, 72};
    cell.action = {TapStep(76, 88)};
    cell.id = 0x11000200;
    p.items.push_back(cell);
    p.back = {TapStep(19, 17)};
    p.buttons[(int)MenuButton::RB] = {TapStep(21, 54)};
    Rig q;
    q.view = &p;
    q.Frame(fe);
    CHECK(q.nav.PictureClosed());
    q.Press(fe, &PadState::a);
    q.Drain();
    CHECK(!q.nav.PictureClosed());
    q.Press(fe, &PadState::b);
    CHECK(q.nav.PictureClosed() && !q.touch.Owns());
    q.Press(fe, &PadState::b);
    CHECK(q.Touched(x, y) && x == 19 && y == 17);
    q.Drain();
    q.Press(fe, &PadState::a);
    q.Drain();
    q.Press(fe, &PadState::rb);
    CHECK(q.nav.PictureClosed());
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
    // A view with MENU's own action (the phone: its X from any page, while B goes back a level):
    // MENU runs it, B its back.
    {
        MenuView o = ListView(3, -1);
        o.own = true;
        o.back = {TapStep(100, 100)};
        o.menu = {TapStep(237, 15)};
        o.menuCloses = true;
        Rig q;
        q.view = &o;
        q.Frame(kPhone);
        q.Press(kPhone, &PadState::menu);
        for (int i = 0; i < 3 && !q.touch.Owns(); i++) q.Frame(kPhone);
        CHECK(q.Touched(x, y) && x == 237 && y == 15);
        Rig b;
        b.view = &o;
        b.Frame(kPhone);
        b.Press(kPhone, &PadState::b);
        for (int i = 0; i < 3 && !b.touch.Owns(); i++) b.Frame(kPhone);
        CHECK(b.Touched(x, y) && x == 100 && y == 100);
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

// Create-a-Sim (port/cas.h) on its own screen: the right stick holds the DS's L / R while
// deflected (stickTurns; none without it, none from LB / RB, which are the view's page taps), Y and
// X the view's own taps (backspace, the keyboard's next page), Start its next page, A a key's tap.
static void view_cas()
{
    const ScreenState cas {Screen::CreateASim, Tab::None};
    MenuView v;
    v.valid = v.own = true;
    v.replay = false;
    v.stickTurns = true;
    ViewItem key;
    key.rect = {731, 352, 50, 50};
    key.id = 0x12000200;
    key.action = {TapStep(88, 85)};
    v.items.push_back(key);
    v.buttons[(int)MenuButton::Y] = {TapStep(165, 168)};
    v.buttons[(int)MenuButton::X] = {TapStep(58, 145)};
    v.buttons[(int)MenuButton::Start] = {TapStep(18, 60)};
    v.buttons[(int)MenuButton::LB] = {TapStep(18, 171)};
    v.buttons[(int)MenuButton::RB] = {TapStep(18, 60)};
    int x, y;
    {
        Rig r;
        r.view = &v;
        r.Frame(cas);
        PadState left;
        left.rx = -0.9f;
        CHECK((r.Frame(cas, left) & ((1u << kL) | (1u << kR))) == (1u << kL));
        PadState right;
        right.rx = 0.9f;
        CHECK((r.Frame(cas, right) & ((1u << kL) | (1u << kR))) == (1u << kR));
        PadState slight;
        slight.rx = 0.3f;
        CHECK((r.Frame(cas, slight) & ((1u << kL) | (1u << kR))) == 0);
        PadState lb;
        lb.lb = true;
        CHECK((r.Frame(cas, lb) & ((1u << kL) | (1u << kR))) == 0 && r.Touched(x, y) && x == 18 && y == 171);
        r.Drain();
        r.Frame(cas);
        CHECK(!r.touch.Owns());
        r.Press(cas, &PadState::y);
        CHECK(r.Touched(x, y) && x == 165 && y == 168);
        r.Drain();
        r.Press(cas, &PadState::x);
        CHECK(r.Touched(x, y) && x == 58 && y == 145);
        r.Drain();
        r.Press(cas, &PadState::a);
        CHECK(r.Touched(x, y) && x == 88 && y == 85);
        r.Drain();
        r.Press(cas, &PadState::start);
        CHECK(r.Touched(x, y) && x == 18 && y == 60);
    }
    {
        // X / Y holding L / R (the keyboard's turn, off the Name page): held, not tapped.
        MenuView w = v;
        w.xyTurns = true;
        w.buttons[(int)MenuButton::X].clear();
        w.buttons[(int)MenuButton::Y].clear();
        Rig r;
        r.view = &w;
        r.Frame(cas);
        PadState x;
        x.x = true;
        CHECK((r.Frame(cas, x) & ((1u << kL) | (1u << kR))) == (1u << kL));
        CHECK((r.Frame(cas, x) & (1u << kL)) != 0 && !r.touch.Owns());
        PadState y;
        y.y = true;
        CHECK((r.Frame(cas, y) & ((1u << kL) | (1u << kR))) == (1u << kR));
        CHECK((r.Frame(cas) & ((1u << kL) | (1u << kR))) == 0);
    }
    {
        MenuView w = v;
        w.stickTurns = false;
        Rig r;
        r.view = &w;
        r.Frame(cas);
        PadState left;
        left.rx = -0.9f;
        CHECK((r.Frame(cas, left) & (1u << kL)) == 0);
    }
}

// The boot's splashes that take no touch (port/front.h): a view with no item passes the DS's own
// Start and A on (passKeys) while they are held; Settings and Load-a-Sim (port/frontmenus.h) on the
// front end's None screen: LB / RB the view's page taps, a slider's Left / Right its drag, B its
// back tap then the closing wait, an unsettled A dropped (never replayed).
static void view_front_menus()
{
    const ScreenState none {Screen::None, Tab::None};
    {
        MenuView v;
        v.valid = v.own = true;
        v.replay = false;
        v.passKeys = (1u << kStart) | (1u << kA);
        Rig r;
        r.view = &v;
        r.Frame(none);
        PadState st;
        st.start = true;
        CHECK((r.Frame(none, st) & (1u << kStart)) != 0);
        PadState a;
        a.a = true;
        CHECK((r.Frame(none, a) & (1u << kA)) != 0 && !r.touch.Owns());
        PadState b;
        b.b = true;
        CHECK((r.Frame(none, b) & (1u << kB)) == 0);
    }
    MenuView v;
    v.valid = v.own = true;
    v.replay = false;
    ViewItem slider;
    slider.rect = {840, 320, 392, 180};
    slider.id = 0x11000100;
    slider.left = {DragStep(212, 96, 199, 96)};
    for (int& n : slider.next) n = kNoMove;
    v.items.push_back(slider);
    v.buttons[(int)MenuButton::LB] = {TapStep(21, 174)};
    v.buttons[(int)MenuButton::RB] = {TapStep(21, 94)};
    v.back = {TapStep(19, 17)};
    v.backCloses = true;
    int x0, y0, x1, y1;
    {
        Rig r;
        r.view = &v;
        r.Frame(none);
        r.Press(none, &PadState::left);
        CHECK(r.Path(x0, y0, x1, y1) && x0 == 212 && x1 == 199 && y0 == 96 && y1 == 96);
        r.Press(none, &PadState::right); // (nothing that way: the focus stays)
        CHECK(!r.touch.Owns() && r.nav.Focus() == 0);
        r.Press(none, &PadState::rb);
        CHECK(r.Touched(x0, y0) && x0 == 21 && y0 == 94);
        r.Drain();
        r.Frame(none);
        r.Press(none, &PadState::lb);
        CHECK(r.Touched(x0, y0) && x0 == 21 && y0 == 174);
        r.Drain();
        r.Frame(none);
        r.Press(none, &PadState::b);
        CHECK(r.Touched(x0, y0) && x0 == 19 && y0 == 17);
    }
    // Load-a-Sim: A taps the slot's tick; held while unsettled, dropped.
    MenuView ld;
    ld.valid = ld.own = true;
    ld.replay = false;
    for (int i = 0; i < 3; i++)
    {
        ViewItem it;
        it.rect = {200, 124 + 156 * i, 880, 142};
        it.id = 0x11000300u + (u32)i;
        it.enabled = i != 1;
        if (it.enabled) it.action = {TapStep(231, 66 + 46 * i)};
        ld.items.push_back(it);
    }
    {
        Rig r;
        r.view = &ld;
        r.Frame(none);
        r.Press(none, &PadState::down); // (past the empty slot)
        CHECK(r.nav.Focus() == 2);
        r.Press(none, &PadState::a);
        CHECK(r.Touched(x0, y0) && x0 == 231 && y0 == 158);
    }
    {
        Rig r;
        r.view = &ld;
        r.Frame(none);
        PadState a;
        a.a = true;
        r.Frame(none, a, false);
        r.Frame(none, {}, false);
        r.Frame(none, {}, true);
        r.Frame(none, {}, true);
        CHECK(!r.touch.Owns());
    }
}

int main()
{
    view_front_menus();
    view_cas();
    view_modules_on_one_screen();
    view_catalogue_spends_once();
    menu_opens_and_closes();
    view_save_slots_and_slider();
    slider_presses_queue();
    question_opens_on_no();
    back_focus_first();
    page_back_without_its_item();
    view_selection_phone_and_no_replay();
    dpad_moves_focus();
    a_taps_focused();
    lb_rb_tap_tabs();
    popup_closed_by_a();
    phone_closed_by_b();
    town_and_dialogue_keep_the_buttons();
    unknown_presses_the_cursor();
    placement_editor_leaves();
    unknown_without_a_cursor_keeps_a();
    touch_menu_on_release_unless_start();
    leaving_console_releases_all();
    social_answers_on_the_d_pad();
    cards_draw_lift_play_discard();
    cards_quit_question();
    cards_ram();
    cards_ram_answered();
    cards_question_moves_while_touching();
    vacuum_blows_drives_and_grabs();
    pointer_moves();
    autopsy_scheme();
    detector_scheme();
    art_scheme();
    moogoo_scheme();
    drencher_scheme();
    crime_scheme();
    scratch_scheme();
    mixer_scheme();
    mixer_save_scheme();
    trading_scheme();
    unsettled_press_waits();
    unsettled_press_dropped_after_90();
    unsettled_ds_buttons_pass();
    unsettled_press_dropped_on_screen_change();
    save_slots_and_quit_keep_the_cursor();
    front_end_screens_touch_with_a();
    popup_opening_holds_a();
    view_focus_skips_greyed_and_a_runs();
    view_animation_closes_the_menu();
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
