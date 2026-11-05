// Checks the bindings: each input does one action. A settings file is loaded through the swap, so
// a file written before a default changed (the pad's Back moved from SELECT to TOUCH MENU) still
// gives every input to one action only.
//   ninja -C build controls_test && ./build/controls_test
#define SDL_MAIN_HANDLED // (a plain main, on Windows too)
#include <SDL.h>

#include <cstdio>
#include <string>

#include "controls.h"
#include "settings.h"

using namespace sims2wide;
using namespace Controls;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// No two actions share a pad input (or a key, among those with one).
static bool OneEach(const Bindings& b, bool quiet = false)
{
    for (int a = 0; a < COUNT; a++)
        for (int c = a + 1; c < COUNT; c++)
        {
            if (b.Pad(a) >= 0 && b.Pad(a) == b.Pad(c)) { if (!quiet) fprintf(stderr, "  pad %s: %s and %s\n", InputKey(b.Pad(a)), ActionKey(a), ActionKey(c)); return false; }
            if (b.Key(a) > 0 && b.Key(a) == b.Key(c)) { if (!quiet) fprintf(stderr, "  key %d: %s and %s\n", b.Key(a), ActionKey(a), ActionKey(c)); return false; }
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

static void old_file_with_e_once_keys_known()
{
    // An older file giving A the key E (TOUCH MENU's default since, asked by position). Settings
    // load before SDL's video is up, when the positional defaults are still 0: the window
    // resolves the keys again once the keymap is known (main.cpp, after SDL_Init).
    Bindings b;
    b.Apply("key_a", std::to_string((int)SDLK_e));
    b.Apply("key_b", std::to_string((int)SDLK_ESCAPE));
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); failures++; return; }
    CHECK(DefaultKey(PANEL) == SDLK_e); // the keymap is known now
    CHECK(b.ActionOfKey(SDLK_e) == A);
    CHECK(!OneEach(b, true));                 // before: A and TOUCH MENU on E
    b.ResolveKeys();
    CHECK(OneEach(b));
    CHECK(b.ActionOfKey(SDLK_e) == A);
    CHECK(b.ActionOfKey(SDLK_ESCAPE) == B);
    CHECK(b.Key(PANEL) == DefaultKey(A)); // the swap: TOUCH MENU took A's place (X), MENU B's (Z)
    CHECK(b.Key(MENU) == DefaultKey(B));
    // A consistent file resolves to itself.
    Bindings c;
    c.SetKey(PANEL, SDLK_F2);
    c.SetKey(A, SDLK_e);
    Bindings d = c;
    d.ResolveKeys();
    for (int a = 0; a < COUNT; a++) CHECK(d.Key(a) == c.Key(a));
    CHECK(OneEach(d));
    SDL_Quit();
}

// The layout and the looks round-trip through settings.ini; the defaults are CONSOLE's.
static void layout_round_trip()
{
    Settings d;
    CHECK(d.layout == Layout::Console && d.widescreen && d.resolution3D == 2 && d.interpolate60);
    const std::string path = "controls_test_settings.ini";
    for (Layout l : {Layout::Console, Layout::Auto, Layout::Focus, Layout::Side, Layout::Stacked})
    {
        Settings a;
        a.layout = l;
        a.widescreen = false;
        CHECK(a.Save(path));
        Settings b;
        b.layout = Layout::Stacked;
        CHECK(b.Load(path));
        CHECK(b.layout == l);
        CHECK(!b.widescreen);
    }
    remove(path.c_str());
}

// A settings file from before CONSOLE (no layout= line) with the old default looks gets the new ones.
static void upgrade_looks()
{
    const std::string path = "controls_test_upgrade.ini";
    auto write = [&](const char* text) { FILE* f = fopen(path.c_str(), "w"); fputs(text, f); fclose(f); };
    Settings a;
    write("mic=blow\nresolution_3d=1\nwidescreen=0\ninterpolate_60=0\n");
    CHECK(a.Load(path));
    CHECK(a.resolution3D == 2 && a.widescreen && a.interpolate60);
    Settings b; // a layout= line: the looks are the player's
    write("layout=auto\nresolution_3d=1\nwidescreen=0\ninterpolate_60=0\n");
    CHECK(b.Load(path));
    CHECK(b.resolution3D == 1 && !b.widescreen && !b.interpolate60);
    Settings c; // no layout= line, but not the old defaults
    write("resolution_3d=1\nwidescreen=1\ninterpolate_60=0\n");
    CHECK(c.Load(path));
    CHECK(c.resolution3D == 1 && c.widescreen && !c.interpolate60);
    remove(path.c_str());
}

int main(int, char**)
{
    upgrade_looks();
    layout_round_trip();
    defaults_one_each();
    old_file_with_back();
    saved_file_loads_back();
    old_file_with_e_once_keys_known();
    if (failures) { fprintf(stderr, "controls_test: %d failure(s)\n", failures); return 1; }
    printf("controls_test: ok\n");
    return 0;
}
