// sims2wide: The Sims 2 (DS, 2005) in one window.
//
// The melonDS core runs the game; this frontend shows the two DS screens in one window with a
// choice of layouts, and has a headless mode (input scripts, shots, save states) to study which
// screen carries what in each situation of the game. main.cpp reads the command line, starts the
// DS and runs the frame loop; the parts live beside it:
//   app.h      shared state          input.*   keyboard, mouse, controller   video.*   the window
//   audio.*    sound                 layout.*  where the screens go          scene.*   AUTO's detection
//   script.*   headless input        shots.*   PNG pictures                  state.*   save states
//   picker.*   ROM file picker       menu.*    pause menu and overlay        ui.*      overlay drawing
//   settings.* the settings file     controls.* key and button bindings      backup.*  save copies
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Args.h"
#include "NDS.h"
#include "NDSCart.h"
#include "app.h"
#include "audio.h"
#include "backup.h"
#include "clock.h"
#include "host.h"
#include "input.h"
#include "looks.h"
#include "menu.h"
#include "mic.h"
#include "picker.h"
#include "romdata.h"
#include "port/dialogue.h"
#include "port/interact.h"
#include "script.h"
#include "shots.h"
#include "recorder.h"
#include "state.h"
#include "video.h"

using namespace sims2wide;
using melonDS::NDS;

static void Usage()
{
    fprintf(stderr,
        "sims2wide " SIMS2WIDE_VERSION "\n"
        "usage: sims2wide [rom.nds] [options]   (no ROM: a file picker asks for one)\n"
        "  --layout console|auto|focus|side|stacked  window layout (default console)\n"
        "  --compose                     shots show the window picture (the layout), not the two screens\n"
        "  --compose-scale N             --compose pictures N times the canvas (1 to 4; the UI drawn at that size)\n"
        "  --ui-demo                     (checks) the HD interface kit's demonstration over the picture\n"
        "  --res3d N                     3D drawn N times sharper (1 = the DS's own; default 2 in the window, 1 headless)\n"
        "  --wide                        widescreen 16:9 world\n"
        "  --interp                      60 fps in-between frames for the 3D\n"
        "  --headless                    no window; with --frames and the shot options\n"
        "  --frames N                    stop after N frames\n"
        "  --input FILE                  input script (frame len key[+key] | frame len touch x y | frame len pad action)\n"
        "  --shot-dir DIR                where shots go (default shots)\n"
        "  --shot-every N | --shot-at a,b,c\n"
        "  --state-load FILE / --state-save FILE (saved at the end)\n"
        "  --save FILE                   the game's save (default beside the ROM, .sav)\n"
        "  --mic off|blow|real           what the DS hears: silence, the blow key, the computer's microphone\n"
        "  --clock \"YYYY-MM-DD HH:MM\"    fix the DS clock's start (tests); default: the computer's time\n"
        "  --print-game                  print what the game says is up, read from RAM, when it changes (frame N: game X)\n"
        "                                and when the pixels' screen disagrees with it for over 90 settled frames\n"
        "  --print-town                  print the town's inventory, options, popup, save slots and the menus' lists (interact, pockets, goals, relations, phone, gift, note), read from RAM, when they change\n"
        "  --print-hud                   print the status bar's values and the conversation, read from RAM, when they change\n"
        "                                (frame N: hud money=.. time=hh:mm sanity=cur/max tier=.. name=..; frame N: dialogue ..)\n"
        "  --record-dir DIR              where unmapped screens are recorded (a state + a picture each; default: the\n"
        "                                settings folder's captures/; on in the window, off headless unless given)\n"
        "  --print-screen                print the recognised bottom screen when it changes (frame N: screen X)\n"
        "                                and CONSOLE's menu and focus (frame N: console menu open|closed, focus F)\n"
        "  --console-menu, --console-focus N  (checks) CONSOLE's menu shown, the focus ring on target N\n"
        "                                (pinned: the navigator no longer moves them)\n"
        "  --fast                        start with fast forward held (headless: the clock's hold is tested)\n"
        "  --strings-count               print how many strings the game's text table has, and stop\n"
        "  --dump-string ID              print string ID of the game's text (UTF-8, a newline as \\n), and stop\n"
        "  --dump-resource ID FILE       write resource ID of the game's archive to FILE, its compression undone, and stop\n"
        "  --dump-sprite ID FRAME FILE   write frame FRAME of the sprite whose descriptor is resource ID as a PNG, and stop\n"
        "  --measure-menu                (checks) every word the HD Interact tab can show fitted to its box, in --language's\n"
        "                                words: how many shrink, how many need an ellipsis (exit 2), and stop\n"
        "  --measure-string ID|all       (checks) string ID laid out in the HD conversation's box (lines, size, fits),\n"
        "                                or all: every page of the game's conversations (needs --state-load), and stop\n"
        "  --language N                  the text's language for those and the HD interface's words: 0 Japanese (the\n"
        "                                English file), 1 English, 2 French, 3 German, 4 Italian, 5 Spanish (default:\n"
        "                                the firmware's; the game itself keeps the firmware's)\n"
        "  --no-romdata                  (checks) do not read the game's text: milestone 1's CONSOLE\n"
        "  --version, --help\n"
        "keys: arrows, X=A Z=B S=X A=Y Q=L W=R Enter=Start RShift=Select, M blow (all remappable)\n"
        "      Esc menu (settings, save states, quit), Space fast forward, Tab swaps the big screen\n"
        "      CONSOLE: E opens the touch screen's menu (arrows move, A taps, L/R tabs, B closes)\n"
        "      0 auto, 1/2/3/4 focus/side/stacked/console, F5/F7 save/load state, F11 fullscreen, F12 screenshot\n"
        "      mouse: the touch screen; over the world, wheel zooms and a right-button drag turns\n");
}

#ifdef _WIN32
// From kernel32, declared here rather than through <windows.h> (its macros clash with the rest).
extern "C" __declspec(dllimport) int __stdcall AttachConsole(unsigned long processId);
extern "C" __declspec(dllimport) void* __stdcall GetStdHandle(unsigned long which);
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    // A Windows GUI program has no console: when started from a terminal, write to that one, so
    // that the messages (and --help) still show there. Output already redirected (to a file, a
    // pipe, CI's log) stays where it is.
    void* out = GetStdHandle((unsigned long)-11); // STD_OUTPUT_HANDLE
    if ((!out || out == (void*)-1) && AttachConsole((unsigned long)-1)) // ATTACH_PARENT_PROCESS
    {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
    }
#endif
    std::string romPath, inputPath, shotDir = "shots", stateLoad, stateSave, savePath, micArg, recordDir;
    long unknownAfter = -1; // (a check) from this frame the game reads as an unmapped screen
    bool headless = false, fast = false;
    long frames = -1;
    int shotEvery = 0;
    std::set<long> shotAt;
    Layout layout = Layout::Console;
    bool layoutGiven = false;
    bool compose = false, printScreen = false, printGame = false, printHud = false, printTown = false, consoleMenu = false, consolePinned = false;
    int consoleFocus = -1;
    int composeScale = 1;
    bool uiDemo = false;
    int res3d = 0;
    bool wide = false, interp = false;
    int language = -1, dumpString = -1; // the game's text (--language, --dump-string ID)
    bool stringsCount = false, noRomData = false;
    int dumpResource = -1, dumpSprite = -1, dumpFrame = -1; // the archive's data (--dump-resource, --dump-sprite)
    std::string dumpFile;
    std::string measure; // --measure-string ID|all
    bool measureMenu = false;
    int dialogueString = -1; // (a check) the HD conversation shows this string instead of the page's
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { Usage(); exit(1); } return argv[++i]; };
        if (a == "--help" || a == "-h") { Usage(); return 0; }
        else if (a == "--version") { printf("sims2wide %s\n", SIMS2WIDE_VERSION); return 0; }
        else if (a == "--headless") headless = true;
        else if (a == "--compose") compose = true;
        else if (a == "--compose-scale") composeScale = std::clamp(atoi(next().c_str()), 1, 4);
        else if (a == "--ui-demo") uiDemo = true;
        else if (a == "--res3d") res3d = std::clamp(atoi(next().c_str()), 1, 8);
        else if (a == "--wide") wide = true;
        else if (a == "--interp") interp = true;
        else if (a == "--frames") frames = atol(next().c_str());
        else if (a == "--input") inputPath = next();
        else if (a == "--shot-dir") shotDir = next();
        else if (a == "--shot-every") shotEvery = atoi(next().c_str());
        else if (a == "--shot-at") { std::stringstream ss(next()); std::string t; while (std::getline(ss, t, ',')) shotAt.insert(atol(t.c_str())); }
        else if (a == "--state-load") stateLoad = next();
        else if (a == "--state-save") stateSave = next();
        else if (a == "--save") savePath = next();
        else if (a == "--mic") micArg = next();
        else if (a == "--fast") fast = true;
        else if (a == "--print-screen") printScreen = true;
        else if (a == "--print-game") printGame = true;
        else if (a == "--print-hud") printHud = true;
        else if (a == "--print-town") printTown = true;
        else if (a == "--record-dir") recordDir = next();
        // For checks only (tests/gamestate_check.sh; not in --help): fakes a new unmapped screen
        // every 200 frames from frame N, to drive the recorder.
        else if (a == "--game-unknown-after") unknownAfter = atol(next().c_str());
        else if (a == "--console-menu") consoleMenu = consolePinned = true;
        else if (a == "--console-focus") { consoleFocus = atoi(next().c_str()); consolePinned = true; }
        else if (a == "--clock") { if (!clock::SetFixed(next())) { fprintf(stderr, "--clock wants \"YYYY-MM-DD HH:MM\"\n"); return 1; } }
        else if (a == "--language" || a == "--dump-string")
        {
            // A whole number, 0 or more; anything else is a usage error, not a 0.
            const std::string v = next();
            char* rest = nullptr;
            const long number = strtol(v.c_str(), &rest, 10);
            if (v.empty() || *rest || number < 0 || number > 1000000) { Usage(); return 1; }
            if (a == "--language" && number > 5) { Usage(); return 1; }
            (a == "--language" ? language : dumpString) = (int)number;
        }
        else if (a == "--strings-count") stringsCount = true;
        else if (a == "--no-romdata") noRomData = true;
        // The archive's data to a file (tests/romdata_check.sh): whole numbers, else the usage.
        else if (a == "--dump-resource" || a == "--dump-sprite")
        {
            auto number = [&](long max) {
                const std::string v = next();
                char* rest = nullptr;
                const long n = strtol(v.c_str(), &rest, 10);
                if (v.empty() || *rest || n < 0 || n > max) { Usage(); exit(1); }
                return (int)n;
            };
            if (a == "--dump-resource") dumpResource = number(1000000);
            else { dumpSprite = number(1000000); dumpFrame = number(255); }
            dumpFile = next();
        }
        // For checks only (not in --help): the HD conversation shows string ID on every page (the
        // longest strings' pictures, tests/console_check.sh).
        else if (a == "--dialogue-string")
        {
            const std::string v = next();
            char* rest = nullptr;
            const long number = strtol(v.c_str(), &rest, 10);
            if (v.empty() || *rest || number < 0 || number > 1000000) { Usage(); return 1; }
            dialogueString = (int)number;
        }
        else if (a == "--measure-menu") measureMenu = true;
        else if (a == "--measure-string")
        {
            measure = next();
            char* rest = nullptr;
            const long number = strtol(measure.c_str(), &rest, 10);
            if (measure != "all" && (measure.empty() || *rest || number < 0 || number > 1000000)) { Usage(); return 1; }
        }
        else if (a == "--layout")
        {
            std::string l = next();
            layoutGiven = true;
            layout = l == "side" ? Layout::Side : l == "stacked" ? Layout::Stacked : l == "focus" ? Layout::Focus : l == "console" ? Layout::Console : Layout::Auto;
        }
        else if (a.rfind("-psn_", 0) == 0) {} // older macOS adds this to an app started from the Finder
        else if (a[0] != '-' && romPath.empty()) romPath = a;
        else { Usage(); return 1; }
    }
    if (headless && frames < 0) { fprintf(stderr, "--headless needs --frames\n"); return 1; }

    // No ROM on the command line (a double click on the app): ask for one.
    const bool picked = romPath.empty();
    if (picked)
    {
        if (headless) { Usage(); return 1; }
        romPath = PickRom();
        if (romPath.empty()) return 0; // cancelled
    }
    const bool dialogs = picked; // started without a terminal, most likely
    // The game's save, the quick save state and the screenshot go beside the ROM: a program
    // started from the Finder or Explorer has no useful current folder.
    const std::string romBase = romPath.substr(0, romPath.rfind('.'));
    Host::SavePath = savePath.empty() ? romBase + ".sav" : savePath;
    const std::string quickState = romBase + ".quick.st", shotFile = romBase + "-shot.png";

    // The ROM and its save.
    std::ifstream rf(romPath, std::ios::binary);
    if (!rf) { Tell(dialogs, "Cannot read " + romPath); return 1; }
    std::vector<u8> rom((std::istreambuf_iterator<char>(rf)), {});
    melonDS::NDSCart::NDSCartArgs cartArgs {};
    {
        std::ifstream sf(Host::SavePath, std::ios::binary);
        if (sf)
        {
            std::vector<u8> sav((std::istreambuf_iterator<char>(sf)), {});
            cartArgs.SRAM = std::make_unique<u8[]>(sav.size());
            memcpy(cartArgs.SRAM.get(), sav.data(), sav.size());
            cartArgs.SRAMLength = (u32)sav.size();
            fprintf(stderr, "save: %s (%zu bytes)\n", Host::SavePath.c_str(), sav.size());
        }
    }
    auto cart = melonDS::NDSCart::ParseROM(rom.data(), (u32)rom.size(), nullptr, std::move(cartArgs));
    if (!cart) { Tell(dialogs, "Not a Nintendo DS ROM: " + romPath); return 1; }

    App app;
    // (For the checks' pictures: pinned, the navigator no longer moves them; it still steers.)
    app.consoleMenu = consoleMenu;
    app.consoleFocus = consoleFocus;
    app.consolePinned = consolePinned;
    app.uiDemo = uiDemo;
    app.quickState = quickState;
    app.shotFile = shotFile;
    // The settings file (not headless: those runs stay the same on every machine).
    const std::string settingsPath = headless ? "" : Settings::DefaultPath();
    if (!settingsPath.empty() && app.settings.Load(settingsPath)) fprintf(stderr, "settings: %s\n", settingsPath.c_str());
    if (headless)
    {
        // Settings default to CONSOLE's looks; headless never loads them, so it starts with the
        // looks off (the checks' output stays the same) unless the flags ask.
        app.settings.resolution3D = 1;
        app.settings.widescreen = false;
        app.settings.interpolate60 = false;
    }
    app.layout = layoutGiven || headless ? layout : app.settings.layout;
    app.shown = Layout::Side; // (the first frame logs the layout when it is not SIDE)
    if (res3d) app.settings.resolution3D = res3d;
    if (wide) app.settings.widescreen = true;
    if (interp) app.settings.interpolate60 = true;
    Looks looks;
    app.fast = fast;
    // --mic overrides the setting (keep this after the settings are loaded).
    if (micArg == "off") app.settings.mic = MicMode::Off;
    else if (micArg == "blow") app.settings.mic = MicMode::BlowKey;
    else if (micArg == "real") app.settings.mic = MicMode::Real;
    else if (!micArg.empty()) { Usage(); return 1; }
    if (rom.size() >= 0x10) app.romCode.assign((const char*)rom.data() + 0x0C, 4);
    app.portSupported = app.romCode == "ASJP";
    melonDS::NDSArgs args;
    args.JIT = std::nullopt;
    app.nds = std::make_unique<NDS>(std::move(args), nullptr);
    NDS& nds = *app.nds;
    nds.SetNDSCart(std::move(cart));
    nds.Reset();
    clock::Seed(nds); // the DS clock: the computer's local time (or --clock); LoadState re-seeds it
    nds.SetupDirectBoot("game.nds");
    nds.Start();

    // The game's text from the ROM (--strings-count, --dump-string ID): print it and stop.
    if (stringsCount || dumpString >= 0)
    {
        RomData data;
        if (!data.Open(rom)) { fprintf(stderr, "no rom.bin archive in this ROM\n"); return 1; }
        const int lang = language >= 0 ? language : GameLanguage(nds);
        if (!data.LoadStrings(lang)) { fprintf(stderr, "no strings for language %d\n", lang); return 1; }
        fprintf(stderr, "strings: language %d from file %d, %d strings\n", lang, data.StringsFile(), data.StringCount());
        if (dumpString >= data.StringCount()) { fprintf(stderr, "no string %d\n", dumpString); return 1; }
        if (stringsCount) printf("%d\n", data.StringCount());
        else
        {
            std::string s = data.String(dumpString);
            for (size_t p = 0; (p = s.find('\n', p)) != std::string::npos; p += 2) s.replace(p, 1, "\\n");
            printf("%s\n", s.c_str());
        }
        return 0;
    }
    // A resource of the game's archive, its compression undone, or a sprite's frame as a PNG
    // (--dump-resource, --dump-sprite; tests/romdata_check.sh): write it, say what it is, stop.
    if (dumpResource >= 0 || dumpSprite >= 0)
    {
        RomData data;
        if (!data.Open(rom)) { fprintf(stderr, "no rom.bin archive in this ROM\n"); return 1; }
        if (dumpResource >= 0)
        {
            if (dumpResource >= data.ResourceCount()) { fprintf(stderr, "no resource %d (the archive holds %d)\n", dumpResource, data.ResourceCount()); return 1; }
            const std::vector<u8> raw = data.Resource(dumpResource);
            std::vector<u8> out;
            const bool packed = Decompress(raw.data(), raw.size(), out);
            if (!packed) out = raw;
            FILE* f = fopen(dumpFile.c_str(), "wb");
            if (!f || (!out.empty() && fwrite(out.data(), 1, out.size(), f) != out.size())) { fprintf(stderr, "cannot write %s\n", dumpFile.c_str()); if (f) fclose(f); return 1; }
            fclose(f);
            if (packed) fprintf(stderr, "resource %d: kind %d, %zu bytes unpacked to %zu\n", dumpResource, (raw[0] >> 4) & 7, raw.size(), out.size());
            else fprintf(stderr, "resource %d: %zu bytes, not compressed\n", dumpResource, raw.size());
            return 0;
        }
        const int frames = data.SpriteFrames(dumpSprite);
        if (frames == 0) { fprintf(stderr, "resource %d is not a sprite descriptor\n", dumpSprite); return 1; }
        SpriteFrame f;
        if (dumpFrame >= frames || !data.Sprite(dumpSprite - 1, dumpSprite, dumpSprite + 1, dumpFrame, f))
        {
            fprintf(stderr, "sprite %d: %d frames; frame %d does not read\n", dumpSprite, frames, dumpFrame);
            return 1;
        }
        WriteRgba(dumpFile, f.argb.data(), f.w, f.h);
        fprintf(stderr, "sprite %d: %d frames; frame %d: %d x %d, %d pieces\n", dumpSprite, frames, dumpFrame, f.w, f.h, f.pieces);
        return 0;
    }
    // The game's words for the HD status bar and conversations, once, in the game's language
    // (--language: another, a layout check only): a ROM whose data does not read keeps
    // milestone 1's CONSOLE (the game's own HUD in the world, its own dialogue box). The
    // archive stays open: the HD menus' icons are decoded from it as they are needed.
    if (!noRomData)
    {
        auto data = std::make_unique<RomData>();
        if (data->Open(rom) && data->LoadStrings(language >= 0 ? language : GameLanguage(nds)))
        {
            app.hudStrings = HudStringsFrom(data.get());
            app.rom = std::move(data);
            app.icons = std::make_unique<IconCache>(app.rom.get());
        }
    }
    if (!app.hudStrings.ok) fprintf(stderr, "no status bar text in this ROM: CONSOLE shows the game's own\n");
    if (!app.rom) fprintf(stderr, "no conversation text in this ROM: CONSOLE shows the game's own box\n");
    // (A check) the HD Interact tab's words in their boxes: print and stop.
    if (measureMenu)
    {
        if (!app.rom) return 1;
        const MenuMeasure m = MeasureInteract(*app.rom);
        printf("interact: %d words, widest verb %d (%.0f px), %d shrunk, %d cut (row %d, hint %d, title %d, strip %d, strip sides %d)",
               m.checked, m.widestId, m.widestW, m.shrunk, m.cut, m.cutIn[0], m.cutIn[1], m.cutIn[2], m.cutIn[3], m.cutIn[4]);
        for (int id : m.cutIds) printf(" %d", id);
        printf("\n");
        // A neighbour's name beside the plate may end in an ellipsis (the Italian table has a
        // sentence at an object's name, 463); every other box must hold its words.
        return m.cut - m.cutIn[4] > 0 ? 2 : 0;
    }
    if (!stateLoad.empty() && !LoadState(nds, stateLoad)) { fprintf(stderr, "cannot load state %s\n", stateLoad.c_str()); return 1; }
    // (A check) a string, or every page of the conversations, laid out in the HD box: print and stop.
    if (!measure.empty())
    {
        if (!app.rom) return 1;
        std::vector<int> ids;
        if (measure == "all")
        {
            if (app.portSupported) ids = SpeechStringIds({nds.MainRAM, nds.MainRAMMask});
            if (ids.empty()) { fprintf(stderr, "no speech table in RAM (a state of the European ROM is needed)\n"); return 1; }
        }
        else ids.push_back(atoi(measure.c_str()));
        HudState hud;
        hud.valid = true; hud.hour = 22; hud.minute = 58; hud.simName = "WWWWWWWW"; // the widest name and clock
        int worst = -1, bad = 0, raw = 0;
        size_t worstLines = 0, worstLen = 0;
        for (int id : ids)
        {
            if (id < 0 || id >= app.rom->StringCount()) { fprintf(stderr, "no string %d\n", id); return 1; }
            const std::string text = DialogueText(app.rom->String(id), hud, app.rom->String(kDlgAmId), app.rom->String(kDlgPmId));
            // Nothing raw may be left: a placeholder or an escape in the shown text.
            if (text.find_first_of("@{}") != std::string::npos)
            {
                printf("string %d: a placeholder or escape left in the text\n", id);
                raw++;
            }
            for (bool portrait : {true, false})
            {
                const DialogueLayout L = LayoutDialogue(text, portrait);
                const bool inside = L.fits && L.panel.y >= 0 && L.panel.y + L.panel.h <= kConsoleH;
                if (!inside) bad++;
                if (ids.size() == 1)
                    printf("string %d portrait %d: %zu lines of %d, size %g, text box %d wide, panel %d,%d %dx%d, %s\n", id, portrait ? 1 : 0, L.lines.size(),
                           L.rows, L.size, L.textW, L.panel.x, L.panel.y, L.panel.w, L.panel.h, inside ? "fits" : "overflows");
                if (portrait && (L.lines.size() > worstLines || (L.lines.size() == worstLines && text.size() > worstLen)))
                {
                    worst = id;
                    worstLines = L.lines.size();
                    worstLen = text.size();
                }
            }
        }
        // (The longest is measured with the portrait's column, the narrower box; both are checked.)
        if (ids.size() > 1) printf("%zu pages, longest string %d (%zu lines), %d overflow%s, %d raw\n", ids.size(), worst, worstLines, bad, bad == 1 ? "" : "s", raw);
        return bad || raw ? 2 : 0;
    }

    std::vector<InputEvent> script;
    if (!inputPath.empty()) script = LoadScript(inputPath);
    bool winShots = false;
    for (const InputEvent& e : script) winShots |= e.winShot;
    if (!shotAt.empty() || shotEvery || winShots) { std::string cmd = "mkdir -p '" + shotDir + "'"; (void)system(cmd.c_str()); }

    // Window, sound, controllers, the save's backups.
    Video video;
    Audio audio;
    Input input;
    PauseMenu menu;
    if (!app.portSupported)
    {   // the pause menu's font: capitals, digits and a few signs only
        const char* msg = "THIS ROM IS NOT THE EUROPEAN ONE: CONSOLE USES ITS OLD SCREEN RULES";
        fprintf(stderr, "rom %s: %s\n", app.romCode.c_str(), msg);
        menu.Say(msg, 0);
    }
    Backups backups;
    if (!headless)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        // The keymap is known now: the keys asked by position have their values, so the swap
        // can move them out of the way of a key the settings file gave another action.
        app.settings.controls.ResolveKeys();
        if (!video.Open(app.settings)) return 1;
        audio.Open();
        input.OpenPads();
        backups.Start(Host::SavePath);
    }

    // frame counts the game's frames (shots, scripts); iter every turn of the loop, paused too.
    long frame = 0;
    std::unique_ptr<Recorder> recorder;
    if (!headless || !recordDir.empty()) recorder = std::make_unique<Recorder>(recordDir);
    // The window keeps the DS's own pace, 59.83 frames a second: vsync alone would follow the
    // screen, and on a 120 Hz screen (ProMotion, gaming monitors) the game ran twice too fast.
    const double dsFrame = 1.0 / (33513982.0 / (6.0 * 355.0 * 263.0)); // seconds
    const double perfFreq = (double)SDL_GetPerformanceFrequency();
    double nextFrameAt = 0; // in seconds of the performance counter; 0: not started
    bool wasPaused = false; // the pause menu was open: buttons held on leaving it are not presses
    bool lastMenu = app.consoleMenu;
    int lastFocus = app.consoleFocus;
    int disagree = 0; // frames the game's settled screen and the pixels' answer have disagreed
    auto keepPace = [&]() {
        if (headless || app.fast) { nextFrameAt = 0; return; }
        double now = SDL_GetPerformanceCounter() / perfFreq;
        if (nextFrameAt == 0 || now - nextFrameAt > 4 * dsFrame) nextFrameAt = now; // start, or fell behind
        nextFrameAt += dsFrame;
        double wait = nextFrameAt - now;
        if (wait > 0.002) SDL_Delay((Uint32)((wait - 0.001) * 1000)); // sleep most of it
        while (SDL_GetPerformanceCounter() / perfFreq < nextFrameAt) {} // then the last moment
    };
    for (long iter = 0; (frames < 0 || iter < frames) && !app.quit; iter++)
    {
        u32 keys = 0;
        bool blow = false, touching = false;
        int tx = 0, ty = 0;
        ApplyScript(script, frame, keys, blow, touching, tx, ty);
        ScriptPad(script, frame, input.scriptPad);
        if (const int l = ScriptLayout(script, frame); l >= 0) app.layout = (Layout)l;
        if (headless)
        {
            // CONSOLE's navigator from the script's "pad" events; its taps over the script's touch.
            input.Steer(app, input.scriptPad, keys);
            input.ScriptTouch(touching, tx, ty);
        }
        else
        {
            SDL_Event ev;
            input.PushScript(script, iter, video);
            while (SDL_PollEvent(&ev)) input.HandleEvent(ev, app, video, menu.open);
            if (ScriptWindowShot(script, iter))
            {
                char name[64];
                snprintf(name, sizeof name, "/w%06ld.png", iter);
                video.shotPath = shotDir + name;
            }
            if (app.quit) break;
            input.Poll(app, menu.open);
            if (menu.open) menu.Frame(app, video, input, settingsPath);
            else if (input.wantMenu) menu.Toggle(app, input);
            if (menu.open)
            {
                // Paused: the last picture with the menu over it; nothing is emulated.
                input.EndFrame();
                menu.Draw(app, video, input, frame);
                video.Present(app, &menu.canvas);
                keepPace();
                wasPaused = true;
                continue;
            }
            if (input.wantSave) menu.Say(SaveState(nds, quickState) ? "STATE SAVED" : "STATE NOT SAVED", frame);
            if (input.wantLoad) menu.Say(LoadState(nds, quickState) ? "STATE LOADED" : "NO STATE TO LOAD", frame);
            if (input.wantShot) { WriteShot(shotFile, app.Top(), app.Bottom()); menu.Say("SCREENSHOT SAVED", frame); }
            if (input.wantLayout >= 0) app.settings.layout = app.layout = (Layout)input.wantLayout;
            if (input.wantFullscreen)
            {
                app.settings.fullscreen = !app.settings.fullscreen;
                video.SetFullscreen(app.settings.fullscreen);
                app.settings.Save(settingsPath);
            }
            if (wasPaused) { input.navigator.Resync(); wasPaused = false; }
            input.Frame(app, video, frame, keys, touching, tx, ty);
            input.EndFrame();
            backups.Tick(SDL_GetTicks());
        }

        // Playable: what the DS hears this frame (the setting, the blow key or the script's
        // "blow") and, in a window, the DS clock held to real time (fast forward, pauses).
        Host::MicMode = app.settings.mic;
        Host::MicBlow = app.micBlow || blow;
        Host::MicForce = app.micForce;
        if (!headless || app.fast) clock::Frame(nds, app.fast);

        nds.SetKeyMask(~keys & 0xFFF);
        if (touching) nds.TouchScreen((melonDS::u16)tx, (melonDS::u16)ty); else nds.ReleaseScreen();
        looks.Apply(app);
        nds.RunFrame();
        looks.Frame(app);

        const ScreenState consoleBefore = app.ConsoleScreen();
        {
            GameState before = app.game;
            app.game = app.portSupported ? ReadGameState({nds.MainRAM, nds.MainRAMMask}) : GameState{};
            if (app.portSupported && unknownAfter >= 0 && frame >= unknownAfter)
            {   // the check's fake: a new unmapped pair every 200 frames
                app.game.valid = true; app.game.between = false; app.game.settled = true;
                app.game.screen = GScreen::Unknown; app.game.overlay = 90; app.game.vtable = 0x02000000 + (u32)((frame - unknownAfter) / 200);
            }
            if (dialogueString >= 0 && app.game.dialogue.valid) app.game.dialogue.stringId = dialogueString;
            if (app.portSupported && printHud)
            {
                const HudState& h = app.game.hud;
                if (h.valid != before.hud.valid || (h.valid && !(h == before.hud)))
                {
                    if (h.valid) fprintf(stderr, "frame %ld: hud money=%d time=%02d:%02d sanity=%d/%d tier=%d name=%s\n", frame, h.money, h.hour, h.minute, h.sanity, h.sanityMax, h.tier, h.simName.c_str());
                    else fprintf(stderr, "frame %ld: hud none\n", frame);
                }
                const DialogueState& d = app.game.dialogue;
                if (d.valid != before.dialogue.valid || (d.valid && !(d == before.dialogue)))
                {
                    if (d.valid)
                    {
                        fprintf(stderr, "frame %ld: dialogue speech=%d page=%d/%d id=%d speaker=%d name=%d last=%d\n", frame, d.speech, d.page, d.pages, d.stringId, d.speaker, d.speakerNameId, d.last ? 1 : 0);
                        // What the HD box shows from this frame on (CONSOLE builds it from the same
                        // reading; the line counts are for the box with the portrait's column).
                        if (app.rom && d.stringId < app.rom->StringCount())
                        {
                            const DialogueLayout L = LayoutDialogue(DialogueText(app.rom->String(d.stringId), app.game.hud, app.rom->String(kDlgAmId), app.rom->String(kDlgPmId)), true);
                            fprintf(stderr, "frame %ld: hd dialogue id=%d lines=%zu size=%g\n", frame, d.stringId, L.lines.size(), L.size);
                        }
                    }
                    else fprintf(stderr, "frame %ld: dialogue none\n", frame);
                }
            }
            if (app.portSupported && printTown)
            {
                const TownState& t = app.game.town;
                const TownState& b = before.town;
                if (!(t.inventory == b.inventory) || frame == 0)
                {
                    if (!t.inventory.valid) fprintf(stderr, "frame %ld: inventory none\n", frame);
                    else
                    {
                        std::string s;
                        for (const ItemRec& i : t.inventory.slots) s += (s.empty() ? "" : ",") + std::to_string(i.id) + ":" + std::to_string(i.nameId) + "/" + std::to_string(i.descId) + "/" + std::to_string(i.iconRes);
                        fprintf(stderr, "frame %ld: inventory slots=%s counted=%d,%d,%d,%d keys=%d%d%d\n", frame, s.empty() ? "-" : s.c_str(),
                                t.inventory.counted[0].count, t.inventory.counted[1].count, t.inventory.counted[2].count, t.inventory.counted[3].count,
                                t.inventory.keys[0].count, t.inventory.keys[1].count, t.inventory.keys[2].count);
                    }
                }
                if (!(t.options == b.options) || frame == 0)
                {
                    if (t.options.valid) fprintf(stderr, "frame %ld: options music=%d sfx=%d\n", frame, t.options.music, t.options.sfx);
                    else fprintf(stderr, "frame %ld: options none\n", frame);
                }
                if (!(t.popup == b.popup) || frame == 0)
                {
                    if (t.popup.valid)
                        fprintf(stderr, "frame %ld: popup state=%d kind=%d pressed=%d,%d title=\"%s\" text=\"%s\"\n", frame, t.popup.state, t.popup.kind, t.popup.pressed, t.popup.pressing ? 1 : 0,
                                GameTextToUtf8(t.popup.title).c_str(), GameTextToUtf8(t.popup.text).c_str());
                    else fprintf(stderr, "frame %ld: popup none\n", frame);
                }
                if (!(t.saves == b.saves) || frame == 0)
                {
                    if (!t.saves.valid) fprintf(stderr, "frame %ld: saves none\n", frame);
                    else
                    {
                        std::string s;
                        for (int i = 0; i < kSaveSlotCount; i++)
                            s += std::string(" ") + std::to_string(i) + "=" + (t.saves.used[i] ? t.saves.slot[i].simName + "/" + std::to_string(t.saves.slot[i].money) + "/" + std::to_string(t.saves.slot[i].place) : "empty");
                        fprintf(stderr, "frame %ld: saves current=%d%s\n", frame, t.saves.current, s.c_str());
                    }
                }
            }
            if (app.portSupported && printTown)
            {   // the menus' lists (gamestate.h), one line per part when it changes
                const TownState& t = app.game.town;
                const TownState& b = before.town;
                auto tgt = [](const TargetRef& r) { return r.kind == TargetRef::None ? std::string("-") : std::string(r.kind == TargetRef::Sim ? "sim" : "obj") + std::to_string(r.index) + ":" + std::to_string(r.nameId) + ":" + std::to_string(r.portraitRes); };
                if (!(t.interact == b.interact) || frame == 0)
                {
                    const InteractState& v = t.interact;
                    if (!v.valid) fprintf(stderr, "frame %ld: interact none\n", frame);
                    else
                    {
                        std::string a;
                        for (const InteractAction& x : v.actions) a += (a.empty() ? "" : ",") + std::to_string(x.code) + ":" + std::to_string(x.labelId) + (x.greyed ? ":grey" : "");
                        fprintf(stderr, "frame %ld: interact target=%s prev=%s next=%s big=%d pad=%d hl=%d actions=%s\n", frame, tgt(v.target).c_str(), tgt(v.prev).c_str(), tgt(v.next).c_str(),
                                v.bigPortraitRes, v.padMode, v.highlighted, a.empty() ? "-" : a.c_str());
                    }
                }
                if (!(t.pockets == b.pockets) || frame == 0)
                {
                    if (t.pockets.valid) fprintf(stderr, "frame %ld: pockets slot=%d key=%d counted=%d\n", frame, t.pockets.slot, t.pockets.key, t.pockets.counted);
                    else fprintf(stderr, "frame %ld: pockets none\n", frame);
                }
                if (!(t.goals == b.goals) || frame == 0)
                {
                    const GoalsState& v = t.goals;
                    if (!v.valid) fprintf(stderr, "frame %ld: goals none\n", frame);
                    else
                    {
                        std::string e;
                        for (const GoalEntry& x : v.entries)
                            e += (e.empty() ? "" : ",") + std::string(x.mission ? "m" : "h") + std::to_string(x.id) + ":" + std::to_string(x.textId) + ":" + std::to_string(x.firstLine) + "+" + std::to_string(x.nLines) +
                                 (x.mission ? "" : ":" + std::to_string(x.nameId) + ":" + std::to_string(x.placeId));
                        fprintf(stderr, "frame %ld: goals sel=%d first=%d lines=%d portrait=%d footer=%d/%d/%d entries=%s\n", frame, v.selected, v.first, v.lines, v.portraitRes,
                                v.footer ? v.footerId : -1, v.footer ? v.scoreId : -1, v.footer ? v.score : -1, e.empty() ? "-" : e.c_str());
                    }
                }
                if (!(t.relations == b.relations) || frame == 0)
                {
                    const RelState& v = t.relations;
                    if (!v.valid) fprintf(stderr, "frame %ld: relations none\n", frame);
                    else
                    {
                        std::string e;
                        for (const RelEntry& x : v.sims)
                            e += (e.empty() ? "" : ",") + std::to_string(x.id) + ":" + std::to_string(x.nameId) + "/" + std::to_string(x.descId) + "/" + std::to_string(x.moodId) + "/" + std::to_string(x.levelId) + "/" + std::to_string(x.portraitRes) + (x.secret ? "/secret" : "");
                        fprintf(stderr, "frame %ld: relations first=%d sel=%d sims=%s\n", frame, v.first, v.selected, e.empty() ? "-" : e.c_str());
                    }
                }
                if (!(t.phone == b.phone) || frame == 0)
                {
                    const PhoneState& v = t.phone;
                    if (!v.valid) fprintf(stderr, "frame %ld: phone none\n", frame);
                    else
                    {
                        std::string e;
                        for (const InboxMsg& x : v.inbox)
                            e += (e.empty() ? "" : ",") + std::to_string(x.slot) + ":" + std::to_string(x.textId) + ":" + std::to_string(x.senderNameId) + (x.read ? ":read" : ":new");
                        fprintf(stderr, "frame %ld: phone page=%d row=%d scroll=%d shown=%d ring=%d contacts=%d/%d menu=%d,%d,%d,%d inbox=%s\n", frame, v.page, v.row, v.scroll, v.shownSlot, v.ringtone,
                                v.contacts, v.contactSim, v.menuIds[0], v.menuIds[1], v.menuIds[2], v.menuIds[3], e.empty() ? "-" : e.c_str());
                    }
                }
                if (!(t.gift == b.gift) || frame == 0)
                {
                    const GiftState& v = t.gift;
                    if (!v.valid) fprintf(stderr, "frame %ld: gift none\n", frame);
                    else fprintf(stderr, "frame %ld: gift recipient=%d name=%d slot=%d refused=%d%d%d%d%d%d\n", frame, v.recipient, v.recipientNameId, v.slot, v.refused[0], v.refused[1], v.refused[2], v.refused[3], v.refused[4], v.refused[5]);
                }
                if (!(t.note == b.note) || frame == 0)
                {
                    if (t.note.valid) fprintf(stderr, "frame %ld: note text=%d playername=%d\n", frame, t.note.textId, t.note.playerName ? 1 : 0);
                    else fprintf(stderr, "frame %ld: note none\n", frame);
                }
            }
            if (app.portSupported && printGame && (!(app.game == before) || frame == 0)) fprintf(stderr, "frame %ld: game %s\n", frame, GameStateName(app.game).c_str());
        }
        if (recorder) recorder->Frame(app, frame, nds);
        const u32* top = app.Top();
        const u32* bottom = app.Bottom();
        {
            bool autoMainIsTop = app.mainIsTop;
            Layout autoShown = app.autoLayout.Update(top, bottom, autoMainIsTop);
            Layout before = app.shown;
            bool bigBefore = app.mainIsTop;
            if (app.layout == Layout::Auto) { app.shown = autoShown; app.mainIsTop = autoMainIsTop; }
            else app.shown = app.layout;
            if (app.shown != before || (app.shown == Layout::Focus && app.mainIsTop != bigBefore))
                fprintf(stderr, "frame %ld: layout %s%s\n", frame, LayoutName(app.shown),
                        app.shown != Layout::Focus ? "" : app.mainIsTop ? " (top big)" : " (bottom big)");
        }
        {
            ScreenState before = app.screens.Current();
            ScreenState now = app.screens.Update(top, bottom);
            // The navigator ran before this frame's screen was known: a new screen (CONSOLE's, the
            // game's when it reads) starts without the old menu and focus.
            const ScreenState consoleNow = app.ConsoleScreen();
            if (!(consoleNow == consoleBefore) && !app.consolePinned && consoleNow.screen != Screen::Panel)
            {
                app.consoleMenu = false;
                app.consoleFocus = -1;
            }
            if (printScreen && !(now == before)) fprintf(stderr, "frame %ld: screen %s\n", frame, ScreenStateName(now).c_str());
            // The game's screen and the pixels' answer apart for more than 90 frames while the
            // game's screen is settled: said once (RAM leads the pixels by up to about 60 frames).
            // (Counted again from either side's change.)
            if (!(consoleNow == consoleBefore) || !(now == before)) disagree = 0;
            if (app.game.valid && app.game.settled && !PixelsAgree(app.game, now)) disagree++;
            else disagree = 0;
            if (printGame && disagree == 91) fprintf(stderr, "frame %ld: pixels disagree: %s\n", frame, ScreenStateName(now).c_str());
            if (printScreen && (app.consoleMenu != lastMenu || app.consoleFocus != lastFocus))
                fprintf(stderr, "frame %ld: console menu %s, focus %d\n", frame, app.consoleMenu ? "open" : "closed", app.consoleFocus);
            lastMenu = app.consoleMenu;
            lastFocus = app.consoleFocus;
        }
        if (shotAt.count(frame) || (shotEvery && frame % shotEvery == 0))
        {
            char name[64];
            snprintf(name, sizeof name, "/f%06ld.png", frame);
            int tw, th;
            const u32* big = app.TopImage(tw, th);
            // (The other layouts sized from the picture itself, as they always were here.)
            if (compose)
            {
                Composition c = app.shown == Layout::Console ? video.Composed(app) : Compose(app.shown, app.mainIsTop, (float)tw / th, tw, th);
                if (app.uiDemo) c.ui = ui::DemoItems(bottom);
                WriteCompose(shotDir + name, c, big, tw, th, bottom, composeScale);
            }
            else WriteShot(shotDir + name, top, bottom);
            // The looks' top picture at its size, beside the DS's screens.
            if (!compose && !app.topBig.empty())
            {
                snprintf(name, sizeof name, "/f%06ld-top.png", frame);
                WriteImage(shotDir + name, big, tw, th);
            }
        }
        frame++;

        if (headless) continue;
        audio.Frame(nds, app.fast, app.settings);
        if (app.fast && frame % 4) continue; // fast forward: one picture in four
        video.Present(app, menu.Draw(app, video, input, frame) ? &menu.canvas : nullptr);
        keepPace();
    }

    Looks::Shutdown();
    if (app.settings.interpolate60) fprintf(stderr, "looks: %u in-between frames\n", melonDS::Looks3D.InterpolatedFrames);
    if (!stateSave.empty()) fprintf(stderr, "state save %s: %s\n", stateSave.c_str(), SaveState(nds, stateSave) ? "ok" : "failed");
    if (headless)
    {
        // For the tests: what the DS clock says at the end, and whether the game listened.
        clock::Time t = clock::Read(nds);
        const clock::Stats& cs = clock::GetStats();
        fprintf(stderr, "clock: %04d-%02d-%02d %02d:%02d:%02d (held %ld times, at most %ld s ahead of the computer, "
                "a minute ahead on %ld frames)  mic: %ld samples read by the game\n",
                t.year, t.month, t.day, t.hour, t.minute, t.second, cs.writes, cs.maxAhead, cs.minuteAhead, mic::SamplesRead());
    }
    if (!headless)
    {
        video.RememberSize(app.settings);
        if (!app.settings.Save(settingsPath)) fprintf(stderr, "cannot write %s\n", settingsPath.c_str());
        input.ClosePads();
        SDL_Quit();
    }
    return 0;
}
