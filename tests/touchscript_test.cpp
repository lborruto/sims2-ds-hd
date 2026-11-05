// Checks the synthesised touches: a tap lasts three frames, a drag walks its path, queued scripts
// run one after another with a released frame between, the queue holds at most 8, and Cancel
// clears everything.
//   ninja -C build touchscript_test && ./build/touchscript_test
#include <cstdio>
#include <cstdlib>

#include "touchscript.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool Is(TouchScript& t, int ex, int ey)
{
    int x = -1, y = -1;
    return t.Next(x, y) && x == ex && y == ey;
}
static bool Idle(TouchScript& t)
{
    int x = -1, y = -1;
    return !t.Next(x, y);
}

static void tap_lasts_three_frames()
{
    TouchScript t;
    CHECK(!t.Busy());
    t.Tap(10, 20);
    CHECK(t.Busy());
    CHECK(Is(t, 10, 20)); CHECK(Is(t, 10, 20)); CHECK(Is(t, 10, 20));
    CHECK(Idle(t)); CHECK(Idle(t));
    CHECK(!t.Busy());
}

static void drag_walks_the_path()
{
    TouchScript t;
    t.Drag({{0, 0}, {10, 0}}, 1, 2);
    CHECK(Is(t, 0, 0)); CHECK(Is(t, 10, 0)); CHECK(Is(t, 10, 0));
    CHECK(Idle(t));
    CHECK(!t.Busy());
}

static void hold_and_clamp()
{
    TouchScript t;
    t.Hold(300, -5, 2);
    CHECK(Is(t, 255, 0)); CHECK(Is(t, 255, 0)); CHECK(Idle(t));
    t.Tap(0, 999);
    CHECK(Is(t, 0, 191));
}

static void queued_waits()
{
    TouchScript t;
    t.Tap(1, 2);
    t.Tap(3, 4);
    CHECK(Is(t, 1, 2)); CHECK(Is(t, 1, 2)); CHECK(Is(t, 1, 2));
    CHECK(Idle(t)); // the released frame: two separate taps for the DS
    CHECK(Is(t, 3, 4)); CHECK(Is(t, 3, 4)); CHECK(Is(t, 3, 4));
    CHECK(Idle(t));
    CHECK(!t.Busy());
}

static void queue_capped_at_8()
{
    TouchScript t;
    for (int i = 0; i < 12; i++) t.Tap(i, 0, 1);
    int taps = 0, x, y;
    for (int n = 0; n < 100; n++) if (t.Next(x, y)) taps++;
    CHECK(taps == 8);
    CHECK(!t.Busy());
}

static void tap_queued_on_the_release_frame()
{
    TouchScript t;
    t.Tap(1, 2);
    CHECK(Is(t, 1, 2)); CHECK(Is(t, 1, 2)); CHECK(Is(t, 1, 2));
    t.Tap(3, 4); // queued after the last touch frame, before the release frame
    CHECK(t.Owns());
    CHECK(Idle(t));
    CHECK(Is(t, 3, 4)); CHECK(Is(t, 3, 4)); CHECK(Is(t, 3, 4));
    CHECK(Idle(t));
    CHECK(!t.Owns());
}

static void cancel_then_tap_still_lifts()
{
    TouchScript t;
    t.Tap(1, 1);
    CHECK(Is(t, 1, 1));
    t.Cancel();
    t.Tap(5, 6);
    CHECK(Idle(t)); // the lift
    CHECK(Is(t, 5, 6));
}

static void cancel_clears()
{
    TouchScript t;
    t.Tap(1, 1); t.Tap(2, 2);
    CHECK(Is(t, 1, 1));
    t.Cancel();
    CHECK(!t.Busy());
    CHECK(Idle(t));
    t.Tap(5, 6, 1);
    CHECK(Is(t, 5, 6)); CHECK(Idle(t));
}

int main()
{
    tap_lasts_three_frames();
    drag_walks_the_path();
    hold_and_clamp();
    queued_waits();
    queue_capped_at_8();
    tap_queued_on_the_release_frame();
    cancel_then_tap_still_lifts();
    cancel_clears();
    if (failures) { fprintf(stderr, "touchscript_test: %d failures\n", failures); return 1; }
    puts("touchscript_test: ok");
    return 0;
}
