// Checks the HD shell (src/shell.*, src/slots.*) on made-up models: every page of the pause menu
// and the launch panel is controller-complete (every item reached by the D-pad, A does what it
// says, B goes back and never quits), the questions open on No (B is No, two quick A never say
// Yes), a message stays until A, the settings' rows change and wrap within their range, the
// bindings' capture in turn stops on a cancel, the save states' grid and Undo, the mouse lands on
// the items it shows; every item inside the canvas, the panel's words in five languages (no
// letter the font lacks), layers ascending; the thumbnails' files and the ROM header's kinds.
//   ninja -C build shell_test && ./build/shell_test
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

#include "port/ownwords.h"
#include "shell.h"
#include "slots.h"
#include "ui/font.h"

using namespace sims2wide;
using namespace sims2wide::shell;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Input Press(const char* what)
{
    Input in;
    const std::string w = what;
    in.up = w == "up";
    in.down = w == "down";
    in.left = w == "left";
    in.right = w == "right";
    in.a = w == "a";
    in.b = w == "b";
    in.x = w == "x";
    in.y = w == "y";
    return in;
}

struct Fixture
{
    Settings s;
    Model m;
    Fixture()
    {
        m.settings = &s;
        for (int a = 0; a < Controls::COUNT; a++) { m.keyGlyph[a] = "Key" + std::to_string(a); m.padGlyph[a] = a % 2 ? "lb" : "a"; }
        m.statesOk = true;
    }
};

// Every item of the page inside the canvas, the layers ascending, the font has every letter.
static void CheckBuild(const Shell& sh, const Model& m, const char* what)
{
    ui::ResetMissingGlyphs();
    const std::vector<ui::UiItem> items = sh.Build(m);
    int layer = 0;
    bool images = false;
    for (const ui::UiItem& it : items)
    {
        if (it.kind == ui::UiItem::Image) { images = true; continue; }
        if (images) { fprintf(stderr, "%s: a still item after an image\n", what); failures++; }
        if (it.layer < layer) { fprintf(stderr, "%s: layers not ascending\n", what); failures++; }
        layer = it.layer;
        if (it.to.x < 0 || it.to.y < 0 || it.to.x + it.to.w > kW || it.to.y + it.to.h > kH)
        {
            fprintf(stderr, "%s: item outside the canvas (%d,%d %dx%d) '%s'\n", what, it.to.x, it.to.y, it.to.w, it.to.h, it.text.c_str());
            failures++;
        }
    }
    for (u32 cp : ui::MissingGlyphs()) { fprintf(stderr, "%s: the font lacks U+%04X\n", what, cp); failures++; }
    // Every focusable item has a place for the mouse, inside the canvas, none overlapping.
    const std::vector<Rect>& r = sh.Rects();
    for (int i = 0; i < (int)r.size(); i++)
    {
        if (!sh.Focusable(m, i)) continue;
        if (r[i].w <= 0 || r[i].x < 0 || r[i].y < 0 || r[i].x + r[i].w > kW || r[i].y + r[i].h > kH)
        {
            if (sh.page == Page::Settings || sh.page == Page::Keys) continue; // (scrolled out)
            fprintf(stderr, "%s: item %d has no place\n", what, i);
            failures++;
        }
    }
}

// Every focusable item reached from the first by the D-pad (any path).
static void CheckReach(Shell sh, Model m, const char* what)
{
    Settings copy = *m.settings; // (the rows a Left / Right lands on change their copy)
    m.settings = &copy;
    std::set<int> seen {sh.focus};
    std::vector<int> todo {sh.focus};
    while (!todo.empty())
    {
        const int f = todo.back();
        todo.pop_back();
        for (const char* d : {"up", "down", "left", "right"})
        {
            Shell t = sh;
            t.focus = f;
            t.Update(Press(d), m);
            if (seen.insert(t.focus).second) todo.push_back(t.focus);
        }
    }
    for (int i = 0; i < sh.Count(m); i++)
        if (sh.Focusable(m, i) && !seen.count(i)) { fprintf(stderr, "%s: item %d never reached\n", what, i); failures++; }
}

static void main_page()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::Main);
    CheckReach(sh, f.m, "main");
    CHECK(sh.Update(Press("a"), f.m).act == Act::Resume);
    sh.Update(Press("down"), f.m);
    sh.Update(Press("a"), f.m);
    CHECK(sh.page == Page::Settings);
    sh.Update(Press("b"), f.m);
    CHECK(sh.page == Page::Main && sh.focus == 1);
    CHECK(sh.Update(Press("b"), f.m).act == Act::Resume);
    // Change game: at once (the host keeps the place first); Open the saves folder only with one
    // (the window); Quit last.
    std::vector<MainItem> e = MainEntries(f.m);
    CHECK(e.size() == 6 && e[4] == MainItem::ChangeGame && e.back() == MainItem::Quit);
    sh.focus = 4;
    CHECK(sh.Update(Press("a"), f.m).act == Act::ChangeGame);
    f.m.canOpenFolder = true;
    e = MainEntries(f.m);
    CHECK(e.size() == 7 && e[5] == MainItem::OpenFolder);
    sh.focus = 5;
    CHECK(sh.Update(Press("a"), f.m).act == Act::OpenFolder);
    CheckReach(sh, f.m, "main with the folder");
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        CheckBuild(sh, f.m, "main with the folder");
    }
    f.m.language = 1;
    // Quit: asked, No first; B is No; A on No closes; Right then A quits.
    sh.focus = (int)e.size() - 1;
    CHECK(sh.Update(Press("a"), f.m).act == Act::None && sh.ask == Ask::Quit && sh.askFocus == 0);
    CHECK(sh.Update(Press("a"), f.m).act == Act::None && sh.ask == Ask::None);
    sh.Update(Press("a"), f.m);
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && sh.ask == Ask::None);
    sh.Update(Press("a"), f.m);
    sh.Update(Press("right"), f.m);
    CHECK(sh.Update(Press("a"), f.m).act == Act::Quit);
    // The window closed: the same question.
    sh.Open(Page::Main);
    sh.AskQuit();
    CHECK(sh.ask == Ask::Quit && sh.askFocus == 0);
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        CheckBuild(sh, f.m, "quit question");
    }
}

static void settings_page()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::Settings);
    CheckReach(sh, f.m, "settings");
    // Every row changes and comes back; the values stay in range.
    for (int r = 0; r < kRows; r++)
    {
        const Settings before = f.s;
        sh.focus = r;
        Result res = sh.Update(Press("right"), f.m);
        CHECK(res.act == Act::Setting && res.arg == r);
        for (int k = 0; k < 20; k++) sh.Update(Press("right"), f.m);
        CHECK(f.s.volume >= 0 && f.s.volume <= 100 && f.s.resolution3D >= 1 && f.s.resolution3D <= 4 && f.s.stickDeadZone >= 5 &&
              f.s.stickDeadZone <= 60 && f.s.triggerThreshold >= 10 && f.s.triggerThreshold <= 90 && f.s.language >= 0 && f.s.language <= 5);
        CHECK(f.s.cursorSpeed >= 50 && f.s.cursorSpeed <= 200 && (f.s.fastSpeed == 0 || (f.s.fastSpeed >= 2 && f.s.fastSpeed <= 4)) &&
              (int)f.s.scaling >= 0 && (int)f.s.scaling <= 2);
        // Every row's value changed by Right, and back by Left (cycles and toggles; a range from its middle).
        {
            Settings a;
            a.volume = 50; a.cursorSpeed = 100; a.resolution3D = 2; a.stickDeadZone = 25; a.triggerThreshold = 50;
            Settings b = a;
            ChangeRow((Row)r, 1, b);
            CHECK(RowValue((Row)r, a, 1) != RowValue((Row)r, b, 1));
            ChangeRow((Row)r, -1, b);
            CHECK(RowValue((Row)r, a, 1) == RowValue((Row)r, b, 1));
            CHECK(!RowLabel((Row)r, 1).empty());
        }
        for (int k = 0; k < 40; k++) sh.Update(Press("left"), f.m);
        CHECK(f.s.volume >= 0 && f.s.resolution3D >= 1);
        (void)before;
    }
    // The language cycles through automatic and the five.
    f.s.language = 0;
    std::set<std::string> names;
    for (int k = 0; k < 6; k++) { names.insert(RowValue(Row::Language, f.s, 1)); ChangeRow(Row::Language, 1, f.s); }
    CHECK(names.size() == 6 && f.s.language == 0);
    // The list scrolls with the focus: the last row is shown.
    sh.focus = 0;
    sh.scroll = 0;
    for (int k = 0; k < kRows - 1; k++) sh.Update(Press("down"), f.m);
    CHECK(sh.focus == kRows - 1 && sh.scroll > 0);
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        for (int r = 0; r < kRows; r += 7)
        {
            sh.focus = r;
            sh.scroll = std::max(0, r - 7);
            CheckBuild(sh, f.m, "settings");
        }
        // Each row with a note: the note shown under the list, inside the canvas.
        for (int r = 0; r < kRows; r++)
            if (RowNote((Row)r) != (int)OwnWordId::Count)
            {
                sh.focus = r;
                sh.scroll = std::max(0, r - 7);
                CheckBuild(sh, f.m, "settings note");
            }
    }
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && sh.page == Page::Main);
}

static void keys_page()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::Controls);
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        f.m.keyboard = lang % 2;
        CheckBuild(sh, f.m, "controls");
    }
    sh.Update(Press("a"), f.m);
    CHECK(sh.page == Page::Keys);
    CheckReach(sh, f.m, "keys");
    sh.focus = 3;
    CHECK(sh.Update(Press("a"), f.m).act == Act::Capture);
    // Capturing: the page waits.
    f.m.capturing = 3;
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && sh.page == Page::Keys);
    CheckBuild(sh, f.m, "capture");
    f.m.capturing = -1;
    CHECK(sh.CaptureDone(false).act == Act::None); // not in turn: one only
    // X clears; Y captures every action in turn, a cancel stops it.
    Result x = sh.Update(Press("x"), f.m);
    CHECK(x.act == Act::ClearBinding && x.arg == 3);
    Result y = sh.Update(Press("y"), f.m);
    CHECK(y.act == Act::Capture && y.arg == 3 && sh.sequence);
    Result n = sh.CaptureDone(false);
    CHECK(n.act == Act::Capture && n.arg == 4);
    CHECK(sh.CaptureDone(true).act == Act::None && !sh.sequence && sh.focus == 4);
    // The last row resets.
    sh.focus = Controls::COUNT;
    CHECK(sh.Update(Press("a"), f.m).act == Act::ResetBindings);
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        for (int fo : {0, 10, (int)Controls::COUNT})
        {
            sh.focus = fo;
            sh.scroll = std::max(0, fo - 7);
            CheckBuild(sh, f.m, "keys");
        }
    }
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && sh.page == Page::Controls);
}

static void states_page()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::States);
    // Undo greyed: not reached; the slots are.
    CheckReach(sh, f.m, "states");
    for (int k = 0; k < 4; k++) sh.Update(Press("down"), f.m);
    CHECK(sh.focus != kStateSlots);
    f.m.canUndo = true;
    sh.focus = 2;
    sh.Update(Press("down"), f.m);
    CHECK(sh.focus == kStateSlots);
    CHECK(sh.Update(Press("a"), f.m).act == Act::Undo);
    sh.Update(Press("up"), f.m);
    CHECK(sh.focus == 2);
    // An empty slot: A saves. A used one: A asks to load (No first), X asks to save over it.
    Result r = sh.Update(Press("a"), f.m);
    CHECK(r.act == Act::SaveSlot && r.arg == 2);
    f.m.used[2] = true;
    f.m.slots[2].w = 4;
    f.m.slots[2].h = 2;
    f.m.slots[2].px.assign(8, 0xFF336699);
    f.m.slots[2].time = 1762430000;
    CHECK(sh.Update(Press("a"), f.m).act == Act::None && sh.ask == Ask::Load && sh.askFocus == 0);
    CheckBuild(sh, f.m, "load question");
    // Two quick A never say Yes: the first is on No.
    CHECK(sh.Update(Press("a"), f.m).act == Act::None && sh.ask == Ask::None);
    sh.Update(Press("a"), f.m);
    sh.Update(Press("right"), f.m);
    r = sh.Update(Press("a"), f.m);
    CHECK(r.act == Act::LoadSlot && r.arg == 2);
    sh.Update(Press("x"), f.m);
    CHECK(sh.ask == Ask::Overwrite && sh.askFocus == 0);
    sh.Update(Press("right"), f.m);
    r = sh.Update(Press("a"), f.m);
    CHECK(r.act == Act::SaveSlot && r.arg == 2);
    // The grid: Right from 0 is 1, Down from 1 is 3.
    sh.focus = 0;
    sh.Update(Press("right"), f.m);
    CHECK(sh.focus == 1);
    sh.Update(Press("down"), f.m);
    CHECK(sh.focus == 3);
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        CheckBuild(sh, f.m, "states");
    }
    // Without a folder: nothing to do but B.
    f.m.statesOk = false;
    CHECK(sh.Update(Press("a"), f.m).act == Act::None);
    CheckBuild(sh, f.m, "states without a folder");
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && sh.page == Page::Main && sh.focus == 3);
}

static void launch_panel()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::Launch);
    // No recent ROM: Choose first, no Continue.
    std::vector<LaunchEntry> e = LaunchEntries(f.m);
    CHECK(e.front().kind == LaunchItem::Choose && e.back().kind == LaunchItem::Quit);
    CHECK(sh.Update(Press("a"), f.m).act == Act::ChooseFile);
    // B never quits: it moves onto Quit.
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && sh.focus == (int)e.size() - 1);
    CHECK(sh.Update(Press("a"), f.m).act == Act::Exit);
    // Recents: Continue the first, the others listed.
    f.m.recents = {"sims2.nds", "other.nds", "third.nds"};
    f.m.hasSuspend = true;
    f.m.suspend.w = 2;
    f.m.suspend.h = 1;
    f.m.suspend.px.assign(2, 0xFF000000);
    f.m.suspend.time = 1762430000;
    sh.Open(Page::Launch);
    CheckReach(sh, f.m, "launch");
    e = LaunchEntries(f.m);
    CHECK(e[0].kind == LaunchItem::Continue && e[2].kind == LaunchItem::Recent && e[2].arg == 1);
    CHECK(sh.Update(Press("a"), f.m).act == Act::Continue);
    sh.focus = 2;
    Result r = sh.Update(Press("a"), f.m);
    CHECK(r.act == Act::OpenRecent && r.arg == 1);
    // The language row: Left / Right / A.
    for (int i = 0; i < (int)e.size(); i++)
        if (e[i].kind == LaunchItem::Language) sh.focus = i;
    r = sh.Update(Press("right"), f.m);
    CHECK(r.act == Act::Setting && f.s.language == 1);
    // A message stays until A (B, the D-pad and a click elsewhere do nothing).
    f.m.message = "x.nds is not The Sims 2 for the DS.";
    CHECK(sh.Update(Press("b"), f.m).act == Act::None && !f.m.message.empty());
    CHECK(sh.Update(Press("down"), f.m).act == Act::None && !f.m.message.empty());
    CheckBuild(sh, f.m, "launch message");
    CHECK(sh.Update(Press("a"), f.m).act == Act::None && f.m.message.empty());
    for (int lang = 1; lang <= 5; lang++)
    {
        f.m.language = lang;
        f.s.language = lang;
        for (int fo = 0; fo < (int)e.size(); fo++)
        {
            sh.focus = fo;
            CheckBuild(sh, f.m, "launch");
        }
    }
    f.m.hasSuspend = false;
    CheckBuild(sh, f.m, "launch without a suspend state");
}

// A cleared or moved A / B never locks the menu: the pad's own south / east then, and one press
// is one shell action (the other action on that button does nothing in the shell).
static void pad_fallback()
{
    using namespace Controls;
    auto press = [](int in) {
        static bool n[IN_COUNT];
        for (bool& x : n) x = false;
        if (in >= 0) n[in] = true;
        return (const bool*)n;
    };
    const bool none[IN_COUNT] = {};
    auto count = [](const PadNav& n) { return (int)n.a + n.b + n.x + n.y; };
    Bindings b;
    {
        const PadNav n = ShellPadNav(b, press(IN_A), none);
        CHECK(n.a && count(n) == 1);                    // the default: A on south
        CHECK(!ShellPadNav(b, press(IN_A), press(IN_A)).a); // held: no press
        CHECK(ShellPadNav(b, press(IN_B), none).b);
    }
    b.pad[A] = kNone;                                   // cleared
    CHECK(ShellPadNav(b, press(IN_A), none).a);         // south still accepts
    b.pad[B] = kNone;
    CHECK(ShellPadNav(b, press(IN_B), none).b);         // east still goes back
    b.SetPad(A, IN_X);                                  // moved to X: X accepts (as its chip says)
    {
        const PadNav n = ShellPadNav(b, press(IN_X), none);
        CHECK(n.a && count(n) == 1);
        CHECK(!ShellPadNav(b, press(IN_A), none).a);    // south is no longer accept
    }
    // X bound to south, A cleared: south is accept, X does nothing in the shell.
    {
        Bindings c;
        c.pad[A] = kNone;
        c.SetPad(X, IN_A);
        CHECK(ShellPadIns(c).accept == IN_A);
        const PadNav n = ShellPadNav(c, press(IN_A), none);
        CHECK(n.a && !n.x && count(n) == 1);
    }
    // Y bound to east, B cleared: east is back, Y does nothing in the shell.
    {
        Bindings c;
        c.pad[B] = kNone;
        c.SetPad(Y, IN_B);
        CHECK(ShellPadIns(c).back == IN_B);
        const PadNav n = ShellPadNav(c, press(IN_B), none);
        CHECK(n.b && !n.y && count(n) == 1);
    }
    // B moved to south, A cleared: south stays back, accept is east.
    {
        Bindings c;
        c.SetPad(B, IN_A);                              // (A gets east)
        c.pad[A] = kNone;
        const PadIns ins = ShellPadIns(c);
        CHECK(ins.accept == IN_B && ins.back == IN_A);
        CHECK(count(ShellPadNav(c, press(IN_A), none)) == 1 && ShellPadNav(c, press(IN_A), none).b);
        CHECK(count(ShellPadNav(c, press(IN_B), none)) == 1 && ShellPadNav(c, press(IN_B), none).a);
    }
    // A on east, B cleared: east is accept (A's) and the main page's entries activate; back is
    // south while nobody owns it, else none from the pad (Esc).
    {
        Bindings c;
        c.SetPad(A, IN_B);                              // (B gets south)
        c.pad[B] = kNone;                               // south now nobody's
        PadIns ins = ShellPadIns(c);
        CHECK(ins.accept == IN_B && ins.back == IN_A);
        const PadNav n = ShellPadNav(c, press(IN_B), none);
        CHECK(n.a && count(n) == 1);
        CHECK(ShellPadNav(c, press(IN_A), none).b && count(ShellPadNav(c, press(IN_A), none)) == 1);
        Fixture f;
        Shell sh;
        sh.Open(Page::Main);
        Input in;
        in.a = n.a;
        in.b = n.b;
        sh.focus = 1;
        sh.Update(in, f.m);
        CHECK(sh.page == Page::Settings);
        c.SetPad(X, IN_A);                              // X on south: back none from the pad
        ins = ShellPadIns(c);
        CHECK(ins.accept == IN_B && ins.back < 0);
        const PadNav s = ShellPadNav(c, press(IN_A), none);
        CHECK(s.x && count(s) == 1);
        CHECK(count(ShellPadNav(c, press(IN_B), none)) == 1);
    }
    // Nothing bound anywhere: the pad's south and east still work.
    {
        Bindings c;
        for (int a = 0; a < COUNT; a++) c.pad[a] = kNone;
        const PadIns ins = ShellPadIns(c);
        CHECK(ins.accept == IN_A && ins.back == IN_B);
    }
}

// A state made with another ROM file: the load question says so.
static void other_rom()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::States);
    f.m.used[1] = true;
    f.m.otherRom[1] = "other.nds";
    sh.focus = 1;
    sh.Update(Press("a"), f.m);
    CHECK(sh.ask == Ask::Load && sh.askFocus == 0);
    bool said = false;
    for (const ui::UiItem& it : sh.Build(f.m))
        if (it.text.find("other.nds") != std::string::npos) said = true;
    CHECK(said);
    // (No folder: the quit question promises nothing.)
    f.m.statesOk = false;
    sh.Open(Page::Main);
    sh.AskQuit();
    for (const ui::UiItem& it : sh.Build(f.m)) CHECK(it.text != OwnWordIn(1, OwnWordId::QuitNote));
}

static void mouse()
{
    Fixture f;
    Shell sh;
    sh.Open(Page::Main);
    sh.Build(f.m);
    // A click on Settings opens it; a click outside the panel goes back.
    const Rect r = sh.Rects()[1];
    Input in;
    in.click = true;
    in.mx = r.x + r.w / 2.0f;
    in.my = r.y + r.h / 2.0f;
    CHECK(sh.Update(in, f.m).act == Act::None && sh.page == Page::Settings);
    sh.Build(f.m);
    // A click on a value row's left third: Left; elsewhere Right.
    const Rect v = sh.Rects()[(int)Row::Volume];
    f.s.volume = 50;
    in.mx = v.x + 10.0f;
    in.my = v.y + v.h / 2.0f;
    CHECK(sh.Update(in, f.m).act == Act::Setting && f.s.volume == 45);
    in.mx = v.x + v.w - 10.0f;
    sh.Update(in, f.m);
    CHECK(f.s.volume == 50);
    in.mx = 5;
    in.my = 5;
    sh.Update(in, f.m);
    CHECK(sh.page == Page::Main);
    // Hover focuses the item moved onto, a pointer resting does not fight the D-pad.
    sh.Build(f.m);
    Input h;
    h.hover = true;
    h.mx = sh.Rects()[3].x + 5.0f;
    h.my = sh.Rects()[3].y + 5.0f;
    sh.Update(h, f.m);
    CHECK(sh.focus == 3);
    sh.Update(Press("up"), f.m);
    sh.Update(h, f.m);
    CHECK(sh.focus == 2);
}

static void files()
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "sims2wide-shell-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    DataDir d;
    d.root = dir.string() + "/";
    // The thumbnail: the picture made small, its time, back from its file.
    std::vector<u8> rgb(1280 * 720 * 3, 0);
    for (size_t i = 0; i < rgb.size(); i += 3) rgb[i] = 200;
    Thumb t = MakeThumb(rgb, 1280, 720, 1762430000);
    CHECK(t.w == kThumbW && t.h == 180 && t.px[0] == 0xFFC80000u);
    const std::string p = d.SlotThumb("ASJP", 0);
    CHECK(WriteThumb(p, t));
    Thumb back;
    CHECK(ReadThumb(p, back) && back.w == t.w && back.h == t.h && back.px == t.px && back.time == t.time);
    CHECK(!FormatTime(back.time).empty() && FormatTime(0).empty());
    CHECK(!ReadThumb(d.SlotThumb("ASJP", 1), back) && back.w == 0);
    { FILE* g = fopen(d.SlotThumb("ASJP", 2).c_str(), "wb"); fputs("S2WT garbage", g); fclose(g); }
    CHECK(!ReadThumb(d.SlotThumb("ASJP", 2), back));
    // The paths keep to the game's folder (letters and digits of the code only).
    CHECK(d.Slot("../x", 0).find("..") == std::string::npos);
    // The recent ROMs: newest first, no twice, only files that exist.
    const std::string a = (dir / "a.nds").string(), b = (dir / "b.nds").string();
    { FILE* g = fopen(a.c_str(), "wb"); fclose(g); g = fopen(b.c_str(), "wb"); fclose(g); }
    d.PushRecent(a);
    d.PushRecent(b);
    d.PushRecent(a);
    d.PushRecent((dir / "gone.nds").string());
    std::vector<std::string> r = d.Recents();
    CHECK(r.size() == 2 && r[0] == a && r[1] == b);
    // The ROM's header.
    u8 h[16] = {'T', 'H', 'E', ' ', 'S', 'I', 'M', 'S', ' ', '2', 0, 0, 'A', 'S', 'J', 'P'};
    std::string code;
    CHECK(ClassifyHeader(h, 16, &code) == RomKind::Sims2EU && code == "ASJP");
    h[15] = 'E';
    CHECK(ClassifyHeader(h, 16) == RomKind::Sims2Other);
    h[4] = 'X';
    CHECK(ClassifyHeader(h, 16) == RomKind::NotSims2);
    CHECK(ClassifyHeader(h, 8) == RomKind::Unreadable);
    CHECK(ClassifyRom(a) == RomKind::Unreadable);
    CHECK(!DataDir().Ok() && DataDir().Slot("ASJP", 0).empty() && DataDir().Recents().empty());
    // No code (an unreadable file): no path, no folder made.
    CHECK(d.Slot("", 0).empty() && d.SuspendMeta("").empty() && !fs::exists(dir / "states" / "unknown"));
    // A state's maker, back from its file; the save's hash.
    StateMeta m {"/x/sims2.nds", 0x1234567890ABCDEFull, 42}, back2;
    CHECK(WriteMeta(d.SlotMeta("ASJP", 0), m) && ReadMeta(d.SlotMeta("ASJP", 0), back2) && back2.rom == m.rom && back2.header == m.header && back2.save == 42);
    CHECK(!ReadMeta(d.SlotMeta("ASJP", 3), back2) && back2.rom.empty());
    CHECK(HashFile(a) == HashBytes(nullptr, 0) && HashFile((dir / "none").string()) == 0);
    fs::remove_all(dir);
}

// Task 5's pure parts: the window's first size, a screenshot's name, the save's writability and
// its fallback, the settings file's new keys, the main page's banner.
static void SaveRules(const std::filesystem::path& dir);
static void platform()
{
    // The window: 1280 x 720 on a large screen, 16:9 within 85 % on a small one, a remembered
    // size kept when it fits, shrunk (its shape kept) when it does not.
    int w = 0, h = 0;
    FitWindow(2560, 1400, false, w, h);
    CHECK(w == 1280 && h == 720);
    FitWindow(1366, 728, false, w, h);
    CHECK(w <= 1366 * 85 / 100 && h <= 728 * 85 / 100 && std::abs(w * 9 - h * 16) <= 32);
    w = 1000, h = 900;
    FitWindow(2560, 1400, true, w, h);
    CHECK(w == 1000 && h == 900);
    w = 3000, h = 1500;
    FitWindow(1920, 1040, true, w, h);
    CHECK(w <= 1632 && h <= 884 && std::abs(w - 2 * h) <= 4);
    w = 640, h = 480;
    FitWindow(0, 0, false, w, h);
    CHECK(w == 640 && h == 480);
    // A remembered place: kept when the window fits there, moved onto the screen when it runs off.
    {
        int x = 100, y = 50;
        FitPlace(0, 25, 1920, 1015, 1280, 720, x, y);
        CHECK(x == 100 && y == 50);
        x = 1500, y = 900;
        FitPlace(0, 25, 1920, 1015, 1280, 720, x, y);
        CHECK(x == 640 && y == 320);
        x = -300, y = 0;
        FitPlace(0, 25, 1920, 1015, 1280, 720, x, y);
        CHECK(x == 0 && y == 25);
        x = 2000, y = 100;   // a second screen at the right
        FitPlace(1920, 0, 1440, 900, 1280, 720, x, y);
        CHECK(x == 2000 && y == 100);
        x = 3000;
        FitPlace(1920, 0, 1440, 900, 1280, 720, x, y);
        CHECK(x == 2080);
    }
    // A screenshot's name: the date and time, a number for a second one in the same second.
    const std::string n1 = ShotName(1762430000), n2 = ShotName(1762430000, 2);
    CHECK(n1.rfind("sims2-", 0) == 0 && n1.size() == std::string("sims2-20251106-120000.png").size() && n1.substr(n1.size() - 4) == ".png");
    CHECK(n2 == n1.substr(0, n1.size() - 4) + "-2.png");
    // The locale's language.
    CHECK(LanguageOfLocale("fr_FR") == 2 && LanguageOfLocale("de") == 3 && LanguageOfLocale("it_CH") == 4 && LanguageOfLocale("es") == 5 &&
          LanguageOfLocale("en_GB") == 1 && LanguageOfLocale("ja") == 1 && LanguageOfLocale("") == 1);
    // Writability: a file made in a folder and removed again; a read-only file refused; the fallback.
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / ("sims2wide-shell-test-" + std::to_string((long long)SDL_GetTicks64()));
    fs::create_directories(dir);
    const std::string fresh = (dir / "new.sav").string();
    CHECK(CanWrite(fresh) && !FileExists(fresh));
    { std::ofstream(dir / "ro.sav") << "x"; }
    fs::permissions(dir / "ro.sav", fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read, fs::perm_options::replace);
    {
        // (Refused, unless the system lets this user write a read-only file anyway: root.)
        FILE* u = fopen((dir / "ro.sav").string().c_str(), "r+b");
        CHECK(CanWrite((dir / "ro.sav").string()) == (u != nullptr));
        if (u) fclose(u);
        else CHECK(!CanWrite((dir / "ro.sav").string()));
    }
    fs::permissions(dir / "ro.sav", fs::perms::owner_write, fs::perm_options::add);
    CHECK(CanWrite((dir / "ro.sav").string()));
    CHECK(!CanWrite((dir / "no-such-folder" / "x.sav").string()));
    DataDir d;
    CHECK(SaveFallback(d, "/roms/game.sav").empty());
    d.root = (dir / "data").string() + "/";
    CHECK(SaveFallback(d, "/roms/game.sav") == d.root + "saves/game.sav" && fs::is_directory(d.root + "saves"));
    SaveRules(dir);
    CHECK(fs::is_directory(d.Screenshots()));
    // The settings file: every new key round-trips; the old smooth_scaling on reads as Smooth, off as the default.
    {
        Settings a;
        a.scaling = Scaling::Nearest; a.batterySaver = true; a.mute = true; a.runInBackground = true; a.fastSpeed = 3; a.fastToggle = true;
        a.fastBoot = false; a.hidePointer = false; a.cursorSpeed = 175; a.leftHanded = true; a.volume = 35; a.windowX = 40; a.windowY = 60;
        const std::string path = (dir / "settings.ini").string();
        CHECK(a.Save(path));
        Settings b;
        CHECK(b.Load(path));
        CHECK(b.scaling == Scaling::Nearest && b.batterySaver && b.mute && b.runInBackground && b.fastSpeed == 3 && b.fastToggle && !b.fastBoot &&
              !b.hidePointer && b.cursorSpeed == 175 && b.leftHanded && b.volume == 35 && b.windowX == 40 && b.windowY == 60 && b.windowKnown);
        CHECK(b.Res3D() == 1 && !b.Interpolate60());
        { std::ofstream(path) << "smooth_scaling=1\n"; }
        Settings c;
        c.Load(path);
        CHECK(c.scaling == Scaling::Smooth && !c.windowKnown);
        { std::ofstream(path) << "smooth_scaling=0\nfast_forward_speed=9\ncursor_speed=5\n"; }
        Settings e;
        e.Load(path);
        CHECK(e.scaling == Scaling::Sharp && e.fastSpeed == 4 && e.cursorSpeed == 50);
    }
    fs::remove_all(dir);
    // The main page's banner: shown, the buttons still inside the panel and reachable.
    Fixture f;
    f.m.banner = "The ROM's folder is read-only: the game saves to /a/very/long/path/to/the/settings/folder/saves/sims2-eu.sav";
    Shell sh;
    sh.Open(Page::Main);
    CheckBuild(sh, f.m, "main banner");
    CheckReach(sh, f.m, "main banner");
}

// The save's fallback copy at a start (ResolveSave, NewerSave, CopyVerified): which one leads, the
// copy back beside the ROM once it can be written, nothing made by a check.
static std::string Slurp(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
static void SaveRules(const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    auto put = [](const fs::path& p, const std::string& text, int age) {
        { std::ofstream(p, std::ios::binary | std::ios::trunc) << text; }
        // age: seconds before now (the dates decide when the contents differ)
        fs::last_write_time(p, fs::file_time_type::clock::now() - std::chrono::seconds(age));
    };
    const fs::path rom = dir / "rom";
    fs::create_directories(rom);
    DataDir d;
    d.root = (dir / "rules-data").string() + "/";
    const std::string beside = (rom / "game.sav").string(), fb = d.root + "saves/game.sav";
    // A check names the copy and makes nothing.
    CHECK(SaveFallbackAt(d, beside) == fb && !fs::exists(d.root + "saves"));
    CHECK(SaveFallbackAt(DataDir {}, beside).empty());
    // Nothing there yet, the save beside writable: the save beside, no folder made.
    put(beside, "beside", 100);
    SaveStart st = ResolveSave(d, beside);
    CHECK(st.path == beside && st.elsewhere.empty() && !st.movedBack && !fs::exists(d.root + "saves"));
    CHECK(NewerSave(beside, fb) == beside);
    fs::create_directories(d.root + "saves");
    // NewerSave: the copy newer, the save beside newer, the same content, the save beside gone.
    put(fb, "copy", 10);
    CHECK(NewerSave(beside, fb) == fb);
    put(fb, "copy", 1000);
    CHECK(NewerSave(beside, fb) == beside);
    put(fb, "beside", 10);
    CHECK(NewerSave(beside, fb) == beside);
    CHECK(NewerSave((rom / "none.sav").string(), fb) == fb);
    // CopyVerified: the bytes, a read-only target made writable, no .tmp left.
    {
        const fs::path a = rom / "a.bin", b = rom / "b.bin";
        put(a, std::string("\0\1\2 bytes", 9), 0);
        put(b, "old", 0);
        fs::permissions(b, fs::perms::owner_read, fs::perm_options::replace);
        CHECK(CopyVerified(a.string(), b.string()) && Slurp(b) == Slurp(a) && !fs::exists(b.string() + ".tmp"));
        CHECK(!CopyVerified((rom / "none").string(), b.string()) && Slurp(b) == Slurp(a));
    }
    // The copy newer and the folder writable again: it goes back beside, set aside as .moved.
    put(beside, "old save", 100);
    put(fb, "newer save", 10);
    st = ResolveSave(d, beside);
    CHECK(st.path == beside && st.movedBack && st.elsewhere.empty() && !st.copyFailed);
    CHECK(Slurp(beside) == "newer save" && !fs::exists(fb) && Slurp(fb + ".moved") == "newer save");
    // The save beside newer than a copy left there: the save beside kept, the copy set aside.
    put(beside, "played elsewhere", 10);
    put(fb, "stale copy", 100);
    st = ResolveSave(d, beside);
    CHECK(st.path == beside && st.movedBack && Slurp(beside) == "played elsewhere" && !fs::exists(fb) && Slurp(fb + ".moved") == "stale copy");
    // The save beside read-only (unless this user writes it anyway: root).
    put(beside, "read-only save", 100);
    fs::permissions(beside, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read, fs::perm_options::replace);
    if (!CanWrite(beside))
    {
        fs::remove(fb + ".moved");
        // No copy yet: the save copied to saves/, which leads.
        st = ResolveSave(d, beside);
        CHECK(st.path == fb && st.elsewhere == fb && !st.movedBack && Slurp(fb) == "read-only save");
        // The copy newer (the game saved there): it stays and leads.
        put(fb, "played in saves", 10);
        st = ResolveSave(d, beside);
        CHECK(st.path == fb && Slurp(fb) == "played in saves" && Slurp(beside) == "read-only save");
        // The save beside newer (put there by hand): copied over the copy, the old copy kept as .old.
        fs::permissions(beside, fs::perms::owner_write, fs::perm_options::add);
        put(beside, "newer by hand", 5);
        fs::permissions(beside, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read, fs::perm_options::replace);
        put(fb, "played in saves", 100);
        st = ResolveSave(d, beside);
        CHECK(st.path == fb && Slurp(fb) == "newer by hand" && Slurp(fb + ".old") == "played in saves");
    }
    else fprintf(stderr, "shell_test: a read-only file is writable here (root): the read-only cases skipped\n");
    fs::permissions(beside, fs::perms::owner_write, fs::perm_options::add);
}

int main(int, char**)
{
    main_page();
    settings_page();
    keys_page();
    states_page();
    launch_panel();
    mouse();
    pad_fallback();
    other_rom();
    files();
    platform();
    if (failures) { fprintf(stderr, "shell_test: %d failure(s)\n", failures); return 1; }
    printf("shell_test: ok\n");
    return 0;
}
