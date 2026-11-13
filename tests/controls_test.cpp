// Checks the bindings: each input does one action. A settings file is loaded through the swap, so
// a file written before a default changed (the pad's Back moved from SELECT to TOUCH MENU) still
// gives every input to one action only.
//   ninja -C build controls_test && ./build/controls_test
#define SDL_MAIN_HANDLED // (a plain main, on Windows too)
#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "controls.h"

#include "prefdir.h"
#include "settings.h"

using namespace sims2hd;
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

// v0.1.1 wrote layout=auto on any change: without settings_version it means CONSOLE, once;
// a version-2 file keeps what it says; a save writes the version.
static void upgrade_layout()
{
    const std::string path = "controls_test_version.ini";
    auto write = [&](const char* text) { FILE* f = fopen(path.c_str(), "w"); fputs(text, f); fclose(f); };
    Settings a;
    write("layout=auto\nresolution_3d=1\nwidescreen=1\ninterpolate_60=0\n");
    CHECK(a.Load(path));
    CHECK(a.layout == Layout::Console && a.resolution3D == 1 && !a.interpolate60);
    Settings b;
    write("settings_version=2\nlayout=auto\n");
    CHECK(b.Load(path));
    CHECK(b.layout == Layout::Auto);
    Settings c; // an old file with another layout keeps it
    write("layout=side\n");
    CHECK(c.Load(path));
    CHECK(c.layout == Layout::Side);
    // the upgraded settings save as the current version (3), and reload as chosen
    CHECK(a.Save(path));
    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)), {});
    CHECK(text.find("settings_version=3\n") != std::string::npos);
    a.layout = Layout::Auto;
    CHECK(a.Save(path));
    Settings d;
    CHECK(d.Load(path));
    CHECK(d.layout == Layout::Auto);
    remove(path.c_str());
}

// The controller's settings round-trip, clamped to their ranges; the Guide button has no action
// by default (it is Steam's and the system's): the pause menu is Start + Back, or Esc.
static void controller_settings()
{
    Settings d;
    CHECK(d.stickDeadZone == 25 && d.triggerThreshold == 50 && d.rumble);
    Bindings b;
    CHECK(b.Pad(MENU) == kNone);
    CHECK(b.ActionOfPad(IN_GUIDE) == -1);
    CHECK(OneEach(b));
    const std::string path = "controls_test_pad.ini";
    Settings a;
    a.stickDeadZone = 40;
    a.triggerThreshold = 30;
    a.rumble = false;
    CHECK(a.Save(path));
    Settings c;
    CHECK(c.Load(path));
    CHECK(c.stickDeadZone == 40 && c.triggerThreshold == 30 && !c.rumble);
    FILE* f = fopen(path.c_str(), "w");
    fputs("stick_dead_zone=99\ntrigger_threshold=0\n", f);
    fclose(f);
    Settings e;
    CHECK(e.Load(path));
    CHECK(e.stickDeadZone == 60 && e.triggerThreshold == 10 && e.rumble);
    // A file that gave the pause menu the Guide button keeps it.
    Bindings g;
    g.Apply("pad_menu", "guide");
    CHECK(g.Pad(MENU) == IN_GUIDE && OneEach(g));
    remove(path.c_str());
}

// The world's camera (navigator.h): the shoulders' default L / R no longer turn there, a pad input
// the player gave L / R himself does (PadOwn). An older file that names the default (`pad_l=lb`,
// written by a capture of the same button) moves with the default; a remap is kept, through a
// save and a load too; LB / RB stay L / R's inputs (the menus' pages and tabs).
static void world_camera_bindings()
{
    Bindings d;
    CHECK(d.Pad(L) == IN_LB && d.Pad(R) == IN_RB && !d.PadOwn(L) && !d.PadOwn(R));
    Bindings old;
    old.Apply("pad_l", "lb");
    old.Apply("pad_r", "rb");
    CHECK(old.Pad(L) == IN_LB && !old.PadOwn(L) && !old.PadOwn(R) && OneEach(old));
    Bindings own;
    own.Apply("pad_l", "lt");          // L on the left trigger: TOUCH (LT's default) gets LB
    CHECK(own.Pad(L) == IN_LT && own.PadOwn(L) && !own.PadOwn(R) && OneEach(own));
    Bindings back;
    for (size_t at = 0, nl; (nl = own.Lines().find('\n', at)) != std::string::npos; at = nl + 1)
    {
        const std::string line = own.Lines().substr(at, nl - at);
        const size_t eq = line.find('=');
        back.Apply(line.substr(0, eq), line.substr(eq + 1));
    }
    CHECK(back.Pad(L) == IN_LT && back.PadOwn(L) && OneEach(back));
    Bindings swapped;
    swapped.Apply("pad_l", "rb");      // the shoulders swapped: both the player's own
    CHECK(swapped.Pad(L) == IN_RB && swapped.Pad(R) == IN_LB && swapped.PadOwn(L) && swapped.PadOwn(R));
    Bindings none;
    none.Apply("pad_l", "none");
    CHECK(none.Pad(L) == kNone && none.PadOwn(L));
}

// Input::Actions' turnL / turnR (Bindings::CameraTurns): the keyboard's L / R keys always turn; a
// pad's L / R only on an input of the player's own (the default LB / RB, or an older file naming
// them, never: they are the menus' pages); held both, both.
// The pause menu's keys: the MENU key (Esc) resumes from every page, before Esc's Back takes it;
// Backspace and the DS's B key go back; a MENU moved to another key leaves Esc as Back.
static void menu_keys()
{
    Bindings b;
    CHECK(MenuKeyOf(SDLK_ESCAPE, b.ActionOfKey(SDLK_ESCAPE)) == MenuKey::Menu);
    CHECK(MenuKeyOf(SDLK_BACKSPACE, b.ActionOfKey(SDLK_BACKSPACE)) == MenuKey::B);
    CHECK(MenuKeyOf(SDLK_RETURN, b.ActionOfKey(SDLK_RETURN)) == MenuKey::A);
    CHECK(MenuKeyOf(SDLK_UP, b.ActionOfKey(SDLK_UP)) == MenuKey::Up);
    CHECK(MenuKeyOf('z', B) == MenuKey::B && MenuKeyOf('x', A) == MenuKey::A);
    CHECK(MenuKeyOf('q', L) == MenuKey::Lb && MenuKeyOf('w', R) == MenuKey::Rb);
    b.SetKey(MENU, 'p');
    CHECK(MenuKeyOf('p', b.ActionOfKey('p')) == MenuKey::Menu);
    CHECK(MenuKeyOf(SDLK_ESCAPE, b.ActionOfKey(SDLK_ESCAPE)) == MenuKey::B);
    // MENU bound to Enter, Space or the DS's A key still resumes (it is tested before A); Enter
    // without MENU on it still accepts.
    for (int key : {(int)SDLK_RETURN, (int)SDLK_SPACE, (int)'x'})
    {
        Bindings m;
        m.SetKey(MENU, key);
        CHECK(MenuKeyOf(key, m.ActionOfKey(key)) == MenuKey::Menu);
    }
    CHECK(MenuKeyOf(SDLK_RETURN, Bindings().ActionOfKey(SDLK_RETURN)) == MenuKey::A);
}

static void camera_turns()
{
    bool l = true, r = true;
    Bindings d;
    d.CameraTurns(false, false, false, false, l, r);
    CHECK(!l && !r);
    d.CameraTurns(true, false, false, false, l, r);
    CHECK(l && !r);
    d.CameraTurns(false, true, false, false, l, r);
    CHECK(!l && r);
    d.CameraTurns(false, false, true, true, l, r); // (LB / RB by default: no turn)
    CHECK(!l && !r);
    Bindings old;
    old.Apply("pad_l", "lb");
    old.Apply("pad_r", "rb");
    old.CameraTurns(false, false, true, true, l, r);
    CHECK(!l && !r);
    Bindings own;
    own.Apply("pad_l", "lt");
    own.CameraTurns(false, false, true, true, l, r); // (L the player's own, R still the default)
    CHECK(l && !r);
    own.CameraTurns(true, true, false, false, l, r);
    CHECK(l && r);
    Bindings swapped;
    swapped.Apply("pad_l", "rb");
    swapped.CameraTurns(false, false, false, true, l, r);
    CHECK(!l && r);
}

// The UI's Auto size (settings.h's AutoUiScale): the canvas pixel's millimetres and the display's
// diagonal for the displays the rule was chosen on (fullscreen unless said), as printed in the
// task's report; nonsense or nothing known: 100 %.
static void auto_ui_scale()
{
    auto at = [](float widthMm, float heightMm, float canvasMm) {
        return std::lround(AutoUiScale(canvasMm / 1280, std::hypot(widthMm, heightMm)) * 100);
    };
    CHECK(at(708.4f, 398.5f, 708.4f) == 65);    // 32" 4K (16:9): 63 % wanted (the capitals 7.6 mm)
    CHECK(at(708.4f, 398.5f, 354.2f) == 125);   // the same, a 1280 x 720 window at "looks like 2560 x 1440"
    CHECK(at(531.4f, 298.9f, 531.4f) == 75);    // 24" 1080p
    CHECK(at(597.7f, 336.2f, 597.7f) == 70);    // 27" 1440p
    CHECK(at(286.5f, 179.1f, 286.5f) == 100);   // 13.3" 2560 x 1600 laptop (16:10: the canvas as wide as the screen)
    CHECK(at(286.5f, 179.1f, 254.7f) == 110);   // the same, a 1280 x 720 window at "looks like 1440 x 900"
    CHECK(at(344.2f, 193.6f, 344.2f) == 90);    // 15.6" 1080p laptop
    CHECK(at(150.8f, 94.2f, 150.8f) == 130);    // 7" 1280 x 800 handheld (the Steam Deck)
    CHECK(at(1209.6f, 680.4f, 1209.6f) == 115); // 55" TV
    CHECK(AutoUiScale(1, 800) == 0.6f);          // (the floor: 60 %)
    CHECK(AutoUiScale(0, 0) == 1 && AutoUiScale(0.5f, 0) == 1 && AutoUiScale(0.5f, 5000) == 1 && AutoUiScale(10, 500) == 1);
    CHECK(UiZoomOf(0, 0.8f) == 0.8f && UiZoomOf(125, 0.8f) == 1.25f);
    // The Steam Deck's own screen by its DMI product name (gamescope's reported size is not
    // trusted there): 7" (LCD) and 7.4" (OLED) at 1280 x 800 either way round, Auto 130 % and 125 %
    // full screen; docked (another display size) or another machine: not known.
    float d = 0;
    CHECK(HandheldPanel("Jupiter\n", 1280, 800, d) && std::abs(d - 177.8f) < 0.1f);
    CHECK(std::lround(AutoUiScale(d / std::hypot(1280.0f, 800.0f), d) * 100) == 130);
    CHECK(HandheldPanel("Galileo", 800, 1280, d) && std::abs(d - 187.96f) < 0.1f);
    CHECK(std::lround(AutoUiScale(d / std::hypot(1280.0f, 800.0f), d) * 100) == 125);
    CHECK(!HandheldPanel("Jupiter", 1920, 1080, d) && !HandheldPanel("MacBookPro18,1", 1280, 800, d) && !HandheldPanel("", 1280, 800, d));
}

// The per-user folder of the project's old name (sims2-ds-wide) moved into the new one: every
// entry, folders too, renamed; nothing when the new folder has settings already or the old one is
// missing; an entry the new folder has stays in the old one, which is then kept.
static void move_old_prefs()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / ("sims2hd-prefdir-test-" + std::to_string((unsigned long long)SDL_GetPerformanceCounter()));
    auto put = [](const fs::path& p, const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << text; };
    auto read = [](const fs::path& p) { std::ifstream in(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
    const fs::path old = root / "sims2-ds-wide", now = root / "sims2-ds-hd";

    // The usual first run: SDL made the new folder, empty; everything moves, the old folder goes.
    put(old / "settings.ini", "volume=40\n");
    put(old / "recent.txt", "a\n");
    put(old / "captures" / "c1.png", "png");
    fs::create_directories(now);
    PrefMove m = MoveOldPrefs(old, now);
    CHECK(m.moved == 3 && m.copied == 0 && m.kept == 0 && m.failed == 0 && m.removedOld);
    CHECK(!fs::exists(old));
    CHECK(read(now / "settings.ini") == "volume=40\n" && read(now / "recent.txt") == "a\n" && read(now / "captures" / "c1.png") == "png");

    // Again: the old folder is gone, nothing happens.
    m = MoveOldPrefs(old, now);
    CHECK(m.moved + m.copied + m.kept + m.failed == 0 && !m.removedOld);

    // The new folder has settings already: the old folder is left alone.
    put(old / "settings.ini", "volume=10\n");
    m = MoveOldPrefs(old, now);
    CHECK(m.moved + m.copied + m.kept + m.failed == 0 && !m.removedOld);
    CHECK(read(old / "settings.ini") == "volume=10\n" && read(now / "settings.ini") == "volume=40\n");

    // No settings yet but a file of the same name (the mappings): that one stays in the old folder,
    // never overwritten, and the old folder is kept; the rest moves. The new folder made if missing.
    fs::remove_all(now);
    put(now / "gamecontrollerdb.txt", "new");
    put(old / "gamecontrollerdb.txt", "old");
    m = MoveOldPrefs(old, now);
    CHECK(m.moved == 1 && m.kept == 1 && m.failed == 0 && !m.removedOld);
    CHECK(read(now / "gamecontrollerdb.txt") == "new" && read(old / "gamecontrollerdb.txt") == "old");
    CHECK(read(now / "settings.ini") == "volume=10\n" && !fs::exists(old / "settings.ini"));
    fs::remove_all(now);
    m = MoveOldPrefs(old, root / "deeper" / "sims2-ds-hd");
    CHECK(m.moved == 1 && m.removedOld && read(root / "deeper" / "sims2-ds-hd" / "gamecontrollerdb.txt") == "old");

    // The old name is a file, not a folder; or the same folder twice: nothing.
    put(old, "not a folder");
    m = MoveOldPrefs(old, now);
    CHECK(m.moved + m.copied + m.kept + m.failed == 0 && fs::is_regular_file(old) && !fs::exists(now));
    fs::remove(old);
    put(old / "recent.txt", "b\n");
    m = MoveOldPrefs(old, old);
    CHECK(m.moved + m.copied + m.kept + m.failed == 0 && read(old / "recent.txt") == "b\n");

    std::error_code ec;
    fs::remove_all(root, ec);
}

int main(int, char**)
{
    move_old_prefs();
    auto_ui_scale();
    camera_turns();
    menu_keys();
    world_camera_bindings();
    controller_settings();
    upgrade_looks();
    upgrade_layout();
    layout_round_trip();
    defaults_one_each();
    old_file_with_back();
    saved_file_loads_back();
    old_file_with_e_once_keys_known();
    if (failures) { fprintf(stderr, "controls_test: %d failure(s)\n", failures); return 1; }
    printf("controls_test: ok\n");
    return 0;
}
