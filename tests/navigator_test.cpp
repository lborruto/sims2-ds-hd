// Checks CONSOLE's navigator on synthetic screen states: the menu opens and closes, the D-pad
// moves the focus, A taps the focused target, L / R tap the tab icons, a popup closes with A, the
// town keeps the DS's buttons, the phone closes with B, Unknown presses the stick cursor, and
// leaving CONSOLE releases everything.
//   ninja -C build navigator_test && ./build/navigator_test
#include <cstdio>
#include <cstdlib>

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

struct Rig
{
    Navigator nav;
    TouchScript touch;
    u32 keys = 0;
    // One frame with these buttons held (the touch script is not advanced: see Touched).
    u32 Frame(ScreenState s, PadState p = {})
    {
        keys = 0;
        nav.Update(s, p, touch, keys);
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
    a.a = true;
    const u32 k = r.Frame(kUnknown, a);
    CHECK(!(k & (1u << kA)));
    CHECK(r.nav.CursorPress());
    CHECK(r.nav.Focus() == -1);
    r.Frame(kUnknown);
    CHECK(!r.nav.CursorPress());
    r.Frame(kInteract);
    CHECK(!r.nav.CursorScreen());
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
    leaving_console_releases_all();
    if (failures) { fprintf(stderr, "navigator_test: %d failure(s)\n", failures); return 1; }
    printf("navigator_test: ok\n");
    return 0;
}
