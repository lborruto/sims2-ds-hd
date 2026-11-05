// Checks the bindings: each input does one action. A settings file is loaded through the swap, so
// a file written before a default changed (the pad's Back moved from SELECT to TOUCH MENU) still
// gives every input to one action only.
//   ninja -C build controls_test && ./build/controls_test
#define SDL_MAIN_HANDLED // (a plain main, on Windows too)
#include <SDL.h>

#include <cstdio>

#include "controls.h"

using namespace sims2wide;
using namespace Controls;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// No two actions share a pad input (or a key, among those with one).
static bool OneEach(const Bindings& b)
{
    for (int a = 0; a < COUNT; a++)
        for (int c = a + 1; c < COUNT; c++)
        {
            if (b.Pad(a) >= 0 && b.Pad(a) == b.Pad(c)) { fprintf(stderr, "  pad %s: %s and %s\n", InputKey(b.Pad(a)), ActionKey(a), ActionKey(c)); return false; }
            if (b.Key(a) > 0 && b.Key(a) == b.Key(c)) { fprintf(stderr, "  key %d: %s and %s\n", b.Key(a), ActionKey(a), ActionKey(c)); return false; }
        }
    return true;
}

static void defaults_one_each()
{
    // (Keys asked by position are 0 before SDL's video is up: only named keys are compared.)
    Bindings b;
    CHECK(OneEach(b));
    CHECK(b.Pad(PANEL) == IN_BACK);
    CHECK(b.Pad(SELECT) == IN_LS);
}

static void old_file_with_back()
{
    // An older file: Start and Select swapped on the pad (Select had Back by default then).
    Bindings b;
    b.Apply("pad_start", "back");
    b.Apply("pad_select", "start");
    CHECK(OneEach(b));
    CHECK(b.Pad(START) == IN_BACK);
    CHECK(b.Pad(SELECT) == IN_START);
    CHECK(b.ActionOfPad(IN_BACK) == START);
    // A key another action has by default (Esc, the pause menu's).
    Bindings k;
    k.Apply("key_a", "27");
    CHECK(OneEach(k));
    CHECK(k.ActionOfKey(SDLK_ESCAPE) == A);
}

static void saved_file_loads_back()
{
    // What Lines() writes loads back to the same bindings.
    Bindings b;
    b.SetPad(PANEL, IN_LS);
    b.SetPad(A, IN_B);
    b.SetKey(MENU, SDLK_F1);
    b.pad[BLOW] = kNone;
    Bindings c;
    const std::string lines = b.Lines();
    size_t at = 0;
    while (at < lines.size())
    {
        const size_t nl = lines.find('\n', at), eq = lines.find('=', at);
        c.Apply(lines.substr(at, eq - at), lines.substr(eq + 1, nl - eq - 1));
        at = nl + 1;
    }
    for (int a = 0; a < COUNT; a++)
    {
        CHECK(c.Pad(a) == b.Pad(a));
        CHECK(c.Key(a) == b.Key(a));
    }
    // An action with none stays with none, and does not take another's.
    Bindings n;
    n.Apply("pad_fast", "none");
    CHECK(n.Pad(FAST) == kNone);
    CHECK(OneEach(n));
}

int main(int, char**)
{
    defaults_one_each();
    old_file_with_back();
    saved_file_loads_back();
    if (failures) { fprintf(stderr, "controls_test: %d failure(s)\n", failures); return 1; }
    printf("controls_test: ok\n");
    return 0;
}
