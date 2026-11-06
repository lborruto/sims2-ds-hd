// sims2wide: The Sims 2 (DS, 2005) in one window.
//
// The melonDS core runs the game; this frontend shows the two DS screens in one window with a
// choice of layouts, and has a headless mode (input scripts, shots, save states) to study which
// screen carries what in each situation of the game. main.cpp reads the command line, starts the
// DS and runs the frame loop; the parts live beside it:
//   app.h      shared state          input.*   keyboard, mouse, controller   video.*   the window
//   audio.*    sound                 layout.*  where the screens go          scene.*   AUTO's detection
//   script.*   headless input        shots.*   PNG pictures                  state.*   save states
//   picker.*   ROM file picker       shell*.*  pause menu, launch panel       slots.*   save states' files
//   settings.* the settings file     controls.* key and button bindings      backup.*  save copies
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
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
#include "mic.h"
#include "picker.h"
#include "romdata.h"
#include "ui/font.h"
#include "port/front.h"
#include "port/frontmenus.h"
#include "port/cas.h"
#include "port/build.h"
#include "port/checkin.h"
#include "port/roominfo.h"
#include "port/music.h"
#include "port/statpanel.h"
#include "port/minigame.h"
#include "port/catalogue.h"
#include "port/dialogue.h"
#include "port/interact.h"
#include "port/goals.h"
#include "port/note.h"
#include "port/options.h"
#include "port/phone.h"
#include "port/popup.h"
#include "port/pockets.h"
#include "port/relations.h"
#include "script.h"
#include "shots.h"
#include "recorder.h"
#include "shellhost.h"
#include "slots.h"
#include "state.h"
#include "video.h"
#include "port/ownwords.h"

using namespace sims2wide;
using melonDS::NDS;

// A whole number from 0 to max, the whole text (anything else is a usage error, not a 0).
static bool WholeNumber(const std::string& v, long max, long& out)
{
    char* rest = nullptr;
    out = strtol(v.c_str(), &rest, 10);
    return !v.empty() && !*rest && out >= 0 && out <= max;
}

static void Usage()
{
    fprintf(stderr,
        "sims2wide " SIMS2WIDE_VERSION "\n"
        "usage: sims2wide [rom.nds] [options]   (no ROM: a file picker asks for one)\n"
        "  --layout console|auto|focus|side|stacked  window layout (default console)\n"
        "  --compose                     shots show the window picture (the layout), not the two screens\n"
        "  --compose-scale N             --compose pictures N times the canvas (1 to 4; the UI drawn at that size)\n"
        "  --ui-demo                     (checks) the HD interface kit's demonstration over the picture\n"
        "  --ui-scale N                  the HD interface's size, 75 to 150 percent by 5 (default: the settings', Auto; 100 headless)\n"
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
        "  --print-hotel                 print the hotel block (rooms, score, guests, songs) and the hotel's and shops' screens (registry, room info, music, build rooms, placement, catalogue, stat panel), read from RAM, when they change\n"
        "  --print-minigame              print the minigame the bottom screen shows (Alien Autopsy, Metal Detector, MooGoo Monkey, scratch lottery, Super Drencher, Crime, Music Mixer and its save menu, Art Gallery), read from RAM, when it changes\n"
        "  --print-front                 print the front end (title, Settings, Load-a-Sim, Create-a-Sim, boot screens, video), read from RAM, when it changes\n"
        "  --print-console               print CONSOLE's composition (which branch; milestone 1's marked m1, a held picture) when it changes, and the counts at the end\n"
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
        "  --measure-menu                (checks) every word the HD menu (Interact, Pockets, Give Gift, Goals, Relationships, Options, Save slots, popups, phone, notes, the shop's catalogue, Build Rooms, the Registry, room info, the main menu, Settings, Load-a-Sim, Create-a-Sim) can show fitted to its box, in --language's\n"
        "                                words: how many shrink, how many need an ellipsis or a character the font lacks (exit 2), and stop\n"
        "  --measure-string ID|all       (checks) string ID laid out in the HD conversation's box (lines, size, fits),\n"
        "                                or all: every page of the game's conversations (needs --state-load; exit 2 on an overflow or a character the font lacks), and stop\n"
        "  --language N                  the text's language for those and the HD interface's words: 0 Japanese (the\n"
        "                                English file), 1 English, 2 French, 3 German, 4 Italian, 5 Spanish (default:\n"
        "                                the firmware's; the game itself keeps the firmware's)\n"
        "  --game-language N             the game's own language (the DS firmware's: 1 English .. 5 Spanish; the HD\n"
        "                                words follow it); default: the settings' (automatic: the computer's) in the\n"
        "                                window, the built-in firmware's (English) headless\n"
        "  --fast-boot                   (checks) the boot's splashes at full speed, as the window does (logged)\n"
        "  --no-romdata                  (checks) do not read the game's text: milestone 1's CONSOLE\n"
        "  --launch                      the launch panel first (shown anyway in the window without a ROM)\n"
        "  --data-dir DIR                where the save states, the suspend state and the recent ROMs go (default: the\n"
        "                                settings folder in the window; none headless unless given)\n"
        "  --shell PAGE [--shell-at N]   (checks) the pause menu opened on PAGE (main, settings, controls, keys, states,\n"
        "                                quit) at frame N (default 60); headless, Start + Back held in a script open it too\n"
        "  --print-shell                 print what the pause menu and the launch panel do (frame N: shell ...)\n"
        "  --version, --help\n"
        "keys: arrows, X=A Z=B S=X A=Y Q=L W=R Enter=Start RShift=Select, M blow (all remappable)\n"
        "      Esc (a controller: Start + Back) the pause menu (settings, controls, save states, quit),\n"
        "      Space fast forward, Tab swaps the big screen\n"
        "      CONSOLE: E opens the touch screen's menu (arrows move, A taps, L/R tabs, B closes)\n"
        "      0 auto, 1/2/3/4 focus/side/stacked/console, F5/F7 save/load state, F11 fullscreen, F12 screenshot (the window's)\n"
        "      Create-a-Sim's name: type it\n"
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
    bool compose = false, printScreen = false, printGame = false, printHud = false, printTown = false, printHotel = false, printMinigame = false, printFront = false, printConsole = false, consoleMenu = false, consolePinned = false;
    int consoleFocus = -1;
    int composeScale = 1;
    int uiScaleArg = 0;     // --ui-scale N (75-150): the HD interface's size for this run; 0 the settings' (headless 100)
    bool uiDemo = false;
    int res3d = 0;
    bool wide = false, interp = false;
    int language = -1, dumpString = -1; // the game's text (--language, --dump-string ID)
    int gameLanguage = -1;              // the DS firmware's language (--game-language)
    bool fastBootFlag = false;
    bool stringsCount = false, noRomData = false;
    int dumpResource = -1, dumpSprite = -1, dumpFrame = -1; // the archive's data (--dump-resource, --dump-sprite)
    std::string dumpFile;
    std::string measure; // --measure-string ID|all
    bool measureMenu = false;
    int dialogueString = -1; // (a check) the HD conversation shows this string instead of the page's
    bool launchFlag = false, printShell = false;
    std::string dataDir, shellPageArg;
    long shellAt = 60;
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
        else if (a == "--ui-scale")
        {
            long n = 0;
            if (!WholeNumber(next(), 1000, n) || n < 75 || n > 150 || n % 5) { Usage(); return 1; }
            uiScaleArg = (int)n;
        }
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
        else if (a == "--print-hotel") printHotel = true;
        else if (a == "--print-minigame") printMinigame = true;
        else if (a == "--print-front") printFront = true;
        else if (a == "--print-console") printConsole = true;
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
            long number;
            if (!WholeNumber(next(), 1000000, number)) { Usage(); return 1; }
            if (a == "--language" && number > 5) { Usage(); return 1; }
            (a == "--language" ? language : dumpString) = (int)number;
        }
        else if (a == "--game-language")
        {
            long number;
            if (!WholeNumber(next(), 5, number) || number < 1) { Usage(); return 1; }
            gameLanguage = (int)number;
        }
        else if (a == "--fast-boot") fastBootFlag = true;
        else if (a == "--strings-count") stringsCount = true;
        else if (a == "--no-romdata") noRomData = true;
        // The archive's data to a file (tests/romdata_check.sh): whole numbers, else the usage.
        else if (a == "--dump-resource" || a == "--dump-sprite")
        {
            auto number = [&](long max) {
                long n;
                if (!WholeNumber(next(), max, n)) { Usage(); exit(1); }
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
            long number;
            if (!WholeNumber(next(), 1000000, number)) { Usage(); return 1; }
            dialogueString = (int)number;
        }
        else if (a == "--measure-menu") measureMenu = true;
        else if (a == "--launch") launchFlag = true;
        else if (a == "--print-shell") printShell = true;
        else if (a == "--data-dir") dataDir = next();
        else if (a == "--shell") shellPageArg = next();
        else if (a == "--shell-at") shellAt = atol(next().c_str());
        else if (a == "--measure-string")
        {
            measure = next();
            long number;
            if (measure != "all" && !WholeNumber(measure, 1000000, number)) { Usage(); return 1; }
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

    // The pause menu's page for --shell.
    int shellPage = -1;
    if (!shellPageArg.empty())
    {
        static const char* pages[] = {"main", "settings", "controls", "keys", "states", "quit"};
        for (int i = 0; i < 6; i++)
            if (shellPageArg == pages[i]) shellPage = i;
        if (shellPage < 0) { Usage(); return 1; }
    }
    // A run that prints or measures and stops opens no window.
    const bool tool = stringsCount || dumpString >= 0 || dumpResource >= 0 || dumpSprite >= 0 || measureMenu || !measure.empty();
    const bool window = !headless && !tool;

    App app;
    // (For the checks' pictures: pinned, the navigator no longer moves them; it still steers.)
    app.consoleMenu = consoleMenu;
    app.consoleFocus = consoleFocus;
    app.consolePinned = consolePinned;
    app.uiDemo = uiDemo;
    // The settings file (not headless: those runs stay the same on every machine).
    const std::string settingsPath = headless ? "" : Settings::DefaultPath();
    if (!settingsPath.empty() && app.settings.Load(settingsPath)) fprintf(stderr, "settings: %s\n", settingsPath.c_str());
    // The HD interface's size (ui/kit.h): headless 100 % unless the flag says (every check's picture
    // as it was); the window the settings' (Auto from the window's physical size, AutoUiScale), the
    // flag over them. Set again every frame (applyUi, below).
    ui::SetUiScale(uiScaleArg ? uiScaleArg / 100.0f : 1.0f);

    // The HD shell (shell.h): its files in the settings folder (--data-dir: a check's; none
    // headless without it).
    ShellHost host;
    host.headless = headless;
    host.print = printShell;
    host.settingsPath = settingsPath;
    if (!dataDir.empty()) host.data.root = dataDir + (dataDir.back() == '/' || dataDir.back() == '\\' ? "" : "/");
    else if (window)
    {
        const std::string sp = Settings::DefaultPath();
        if (const size_t slash = sp.find_last_of("/\\"); slash != std::string::npos) host.data.root = sp.substr(0, slash + 1);
    }

    // Window, sound, controllers: up before the launch panel.
    Video video;
    float lastAuto = 0;
    auto applyUi = [&]() {
        if (!window || !video.win) return;
        float z = uiScaleArg / 100.0f;
        if (!uiScaleArg)
        {
            const float a = app.settings.uiScale == 0 ? video.AutoUiScale() : 1;
            if (app.settings.uiScale == 0 && a != lastAuto)
            {
                float mm = 0, diag = 0;
                video.DisplayMetrics(mm, diag);
                fprintf(stderr, "ui: auto size %d %% (%.3f mm an output pixel, a %.1f\" display)\n", (int)std::lround(a * 100), mm, diag / 25.4f);
                lastAuto = a;
            }
            z = UiZoomOf(app.settings.uiScale, a);
        }
        ui::SetUiScale(z);
    };
    Audio audio;
    Input input;
    if (window)
    {
        // Controllers: SDL's hints first (pads.h), then the joystick and game controller parts
        // (SDL_GAMECONTROLLERCONFIG, which Steam sets for a non-Steam game, is read here).
        Pads::Hints();
#ifdef _WIN32
        // Windows: the window in real pixels on every screen (per-monitor DPI, v2), sized in
        // points as on a Mac (SDL scales the window's size by the screen's scale): never a blurry
        // picture stretched by the system.
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        // Haptics (DirectInput pads' rumble) only if the system has them: never a reason not to start.
        if (SDL_InitSubSystem(SDL_INIT_HAPTIC) != 0) fprintf(stderr, "SDL haptic: %s\n", SDL_GetError());
        Pads::LoadMappings(Pads::UserMappingsPath());
        // The keymap is known now: the keys asked by position have their values, so the swap
        // can move them out of the way of a key the settings file gave another action.
        app.settings.controls.ResolveKeys();
        if (!video.Open(app.settings)) return 1;
        input.window = video.win;
        // (SDL starts with text input on: off until Create-a-Sim's Name page, Video::TextInput.)
        SDL_StopTextInput();
        input.OpenPads(app.settings);
        // The words before the game's strings are read: the computer's language.
        if (SDL_Locale* loc = SDL_GetPreferredLocales())
        {
            if (loc[0].language) host.autoLanguage = LanguageOfLocale(loc[0].language);
            SDL_free(loc);
        }
    }
    std::vector<InputEvent> script;
    if (!inputPath.empty()) script = LoadScript(inputPath);
    bool winShots = false;
    for (const InputEvent& e : script) winShots |= e.winShot;
    if (!shotAt.empty() || shotEvery || winShots) { std::string cmd = "mkdir -p '" + shotDir + "'"; (void)system(cmd.c_str()); }

    // The launch panel, before the DS starts: in the window without a ROM (a double click on
    // the app), with a ROM that is not The Sims 2, or with --launch (headless too, from the
    // script's "pad" events). Its turns count as frames: a script and the shots go on from there.
    long launchFrames = 0;
    bool continueSuspend = false;
    auto padEdges = [](const PadState& p, const PadState& q) {
        shell::Input in;
        auto edge = [](bool now, bool before) { return now && !before; };
        in.up = edge(p.up || p.ly < -0.5f, q.up || q.ly < -0.5f);
        in.down = edge(p.down || p.ly > 0.5f, q.down || q.ly > 0.5f);
        in.left = edge(p.left || p.lx < -0.5f, q.left || q.lx < -0.5f);
        in.right = edge(p.right || p.lx > 0.5f, q.right || q.lx > 0.5f);
        in.a = edge(p.a, q.a);
        in.b = edge(p.b, q.b);
        in.x = edge(p.x, q.x);
        in.y = edge(p.y, q.y);
        return in;
    };
    // The window's mouse in output pixels -> the shell's 1280 x 720 canvas.
    auto shellInput = [&](shell::Input in) {
        if ((in.hover || in.click) && video.ren)
        {
            int ow, oh;
            video.OutputSize(ow, oh);
            CanvasFit::For(shell::kW, shell::kH, ow, oh).ToCanvas(in.mx, in.my, in.mx, in.my);
        }
        return in;
    };
    // Which save a ROM's start will load: the one beside it (or --save), or its copy in the data
    // folder's saves/ when that one leads (ResolveSave's rule, NewerSave); nothing is made or moved.
    auto SaveOf = [&](const std::string& rom) {
        const std::string beside = savePath.empty() ? rom.substr(0, rom.rfind('.')) + ".sav" : savePath;
        return NewerSave(beside, SaveFallbackAt(host.data, beside));
    };
    auto launch = [&](const std::string& message) -> bool {
        host.model = shell::Model();
        host.shell.Open(shell::Page::Launch);
        host.model.message = message;
        std::vector<std::string> paths = host.data.Recents();
        for (const std::string& p : paths) host.model.recents.push_back(BaseName(p));
        std::string firstCode;
        StateMeta suspendMeta;
        if (!paths.empty() && ClassifyRom(paths[0], &firstCode) != RomKind::Unreadable)
            host.model.hasSuspend = ReadMeta(host.data.SuspendMeta(firstCode), suspendMeta) && suspendMeta.rom == paths[0] &&
                                    FileExists(host.data.Suspend(firstCode)) && ReadThumb(host.data.SuspendThumb(firstCode), host.model.suspend);
        PadState prev;
        if (launchFrames > 0) ScriptPad(script, launchFrames - 1, prev); // (Change game: a button still held is no press)
        for (long iter = 0;; iter++)
        {
            applyUi();
            shell::Input in;
            std::string tryPath;
            if (headless)
            {
                if (frames >= 0 && launchFrames >= frames) return false;
                PadState p;
                ScriptPad(script, launchFrames, p);
                in = padEdges(p, prev);
                prev = p;
            }
            else
            {
                SDL_Event ev;
                input.PushScript(script, iter, video);
                while (SDL_PollEvent(&ev))
                {
                    video.Event(ev);
                    input.HandleEvent(ev, app, video, true);
                }
                input.Poll(app, true);
                if (input.wantQuit) return false;
                in = shellInput(input.nav);
                if (!input.droppedFile.empty()) { tryPath = input.droppedFile; input.droppedFile.clear(); }
                input.EndFrame();
            }
            host.frame = launchFrames;
            host.Refresh(app, input);
            const shell::Result r = host.shell.Update(in, host.model);
            if (printShell && r.act != shell::Act::None) fprintf(stderr, "frame %ld: shell launch act %d %d\n", launchFrames, (int)r.act, r.arg);
            switch (r.act)
            {
            case shell::Act::Continue:
                tryPath = paths[0];
                continueSuspend = host.model.hasSuspend;
                if (continueSuspend)
                {
                    // The game saved since the place was kept (its save is not the one the place
                    // left): loading the place would put the older save back. Said, and the place dropped.
                    const std::string sav = SaveOf(paths[0]);
                    if (suspendMeta.save != HashFile(sav))
                    {
                        std::error_code ec;
                        for (const std::string& f : {host.data.Suspend(firstCode), host.data.SuspendThumb(firstCode), host.data.SuspendMeta(firstCode)})
                            std::filesystem::remove(f, ec);
                        host.model.hasSuspend = continueSuspend = false;
                        host.model.message = OwnWordIn(host.model.language, OwnWordId::LaunchSaveNewer);
                        if (printShell) fprintf(stderr, "frame %ld: shell launch refused continue: saved since\n", launchFrames);
                        tryPath.clear();
                    }
                }
                break;
            case shell::Act::OpenRecent: tryPath = paths[r.arg]; break;
            case shell::Act::ChooseFile: if (window) tryPath = PickRom(); break;
            case shell::Act::Exit: return false;
            case shell::Act::Setting:
                if (!settingsPath.empty())
                {
                    if (window) video.RememberSize(app.settings);
                    app.settings.Save(settingsPath);
                }
                break;
            default: break;
            }
            if (!tryPath.empty())
            {
                const int lang = host.model.language;
                const RomKind k = ClassifyRom(tryPath);
                std::string msg;
                if (k == RomKind::Unreadable) msg = OwnWordIn(lang, OwnWordId::LaunchUnreadable);
                else if (k == RomKind::NotSims2) msg = OwnWordIn(lang, OwnWordId::LaunchWrong);
                if (msg.empty())
                {
                    romPath = tryPath;
                    host.shell.Close();
                    if (printShell) fprintf(stderr, "frame %ld: shell launch %s%s\n", launchFrames, BaseName(romPath).c_str(), continueSuspend ? " (continue)" : "");
                    return true;
                }
                if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, BaseName(tryPath));
                host.model.message = msg;
                continueSuspend = false;
                if (printShell) fprintf(stderr, "frame %ld: shell launch refused %s\n", launchFrames, BaseName(tryPath).c_str());
            }
            const std::vector<ui::UiItem> items = host.shell.Build(host.model);
            if (headless)
            {
                if (shotAt.count(launchFrames) || (shotEvery && launchFrames % shotEvery == 0))
                {
                    char name[64];
                    snprintf(name, sizeof name, "/f%06ld.png", launchFrames);
                    Composition c {shell::kW, shell::kH, {}, {}, items};
                    static const std::vector<u32> black(SW * SH, 0xFF000000);
                    WriteCompose(shotDir + name, c, black.data(), SW, SH, black.data(), composeScale);
                }
            }
            else
            {
                if (ScriptWindowShot(script, iter))
                {
                    char name[64];
                    snprintf(name, sizeof name, "/w%06ld.png", iter);
                    video.shotPath = shotDir + name;
                }
                video.PresentUi(items);
                SDL_Delay(15);
            }
            launchFrames++;
        }
    };
    bool relaunching = false; // Change game: the launch panel again, after the game before
relaunch:
    std::string launchMessage;
    if (window && !romPath.empty() && !relaunching)
    {
        // A ROM on the command line that is not The Sims 2: said on the launch panel.
        const RomKind k = ClassifyRom(romPath);
        const int lang = shell::WordsLanguage(app.settings, host.autoLanguage);
        if (k == RomKind::Unreadable) launchMessage = OwnWordIn(lang, OwnWordId::LaunchUnreadable);
        else if (k == RomKind::NotSims2) launchMessage = OwnWordIn(lang, OwnWordId::LaunchWrong);
        if (size_t at = launchMessage.find("@1"); at != std::string::npos) launchMessage.replace(at, 2, BaseName(romPath));
    }
    if ((window && (romPath.empty() || launchFlag || !launchMessage.empty())) || (headless && launchFlag) || relaunching)
    {
        continueSuspend = false;
        if (!launchMessage.empty()) romPath.clear();
        if (!launch(launchMessage))
        {
            if (window) { input.ClosePads(); SDL_Quit(); }
            return 0;
        }
    }
    if (romPath.empty()) { Usage(); return 1; }
    const bool dialogs = window; // started without a terminal, most likely
    // (Remembered whole: the next start may be from another folder.)
    {
        std::error_code ec;
        const std::filesystem::path whole = std::filesystem::absolute(romPath, ec);
        host.romPath = ec ? romPath : whole.lexically_normal().string();
    }
    host.data.PushRecent(host.romPath);
    host.romHeader = 0;
    // The game's save and the screenshot go beside the ROM: a program started from the Finder or
    // Explorer has no useful current folder.
    const std::string romBase = romPath.substr(0, romPath.rfind('.'));
    Host::SavePath = savePath.empty() ? romBase + ".sav" : savePath;
    // A save that cannot be written there (a read-only folder: a write-protected card, a shared
    // folder, Program Files): in the data folder's saves/ instead, the one beside the ROM copied
    // there first, said in the window and on the pause menu. A copy left there from an earlier
    // start goes back beside the ROM once it can be written again (the newer of the two leads:
    // ResolveSave), said in a toast.
    Host::SaveMoved = false;
    Host::SaveFallback = SaveFallbackAt(host.data, Host::SavePath);
    host.saveElsewhere.clear();
    bool saveMovedBack = false;
    {
        const std::string beside = Host::SavePath;
        const SaveStart start = ResolveSave(host.data, beside);
        if (start.movedBack)
            fprintf(stderr, "save: %s can be written again: the save is back there (the copy in saves/ set aside as .moved)\n", beside.c_str());
        else if (!start.elsewhere.empty() && CanWrite(beside))
            fprintf(stderr, "save: the copy in %s could not be put back beside the ROM: the game keeps saving there\n", start.elsewhere.c_str());
        else if (!start.elsewhere.empty())
            fprintf(stderr, "save: %s cannot be written: the game saves to %s%s\n", beside.c_str(), start.elsewhere.c_str(),
                    start.copyFailed ? " (the save beside it could not be copied)" : "");
        Host::SavePath = start.path;
        host.saveElsewhere = start.elsewhere;
        saveMovedBack = start.movedBack;
    }
    std::string quickState = romBase + ".quick.st";
    const std::string shotFile = romBase + "-shot.png";

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
    app.shotFile = shotFile;
    host.romHeader = HashBytes(rom.data(), std::min<size_t>(rom.size(), 0x200)); // (StateMeta: the cartridge's header)
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
    // The game's language: the DS firmware's user language, which the game reads when it starts
    // (--game-language; the window: the settings', automatic being the computer's). Headless runs
    // keep the built-in firmware's, English (every check's output depends on it).
    {
        int firm = gameLanguage;
        if (firm < 0 && window) firm = app.settings.language >= 1 && app.settings.language <= 5 ? app.settings.language : host.autoLanguage;
        if (firm >= 1 && firm <= 5)
        {
            for (melonDS::Firmware::UserData& u : args.Firmware.GetUserData())
                u.Settings = (u16)((u.Settings & ~melonDS::Firmware::Language::Reserved) | firm);
            args.Firmware.UpdateChecksums();
            fprintf(stderr, "firmware language: %d\n", firm);
        }
    }
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
            if (packed && ((raw[0] >> 4) & 7) == 0) fprintf(stderr, "resource %d: stored (kind 0), its 4-byte header taken off: %zu bytes to %zu\n", dumpResource, raw.size(), out.size());
            else if (packed) fprintf(stderr, "resource %d: kind %d, %zu bytes unpacked to %zu\n", dumpResource, (raw[0] >> 4) & 7, raw.size(), out.size());
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
        ui::ResetMissingGlyphs();
        const MenuMeasure m = MeasureInteract(*app.rom);
        printf("interact: %d words, widest verb %d (%.0f px), %d shrunk, %d cut (row %d, hint %d, title %d, strip %d, strip sides %d)",
               m.checked, m.widestId, m.widestW, m.shrunk, m.cut, m.cutIn[0], m.cutIn[1], m.cutIn[2], m.cutIn[3], m.cutIn[4]);
        for (int id : m.cutIds) printf(" %d", id);
        printf("\n");
        // Pockets and Give Gift: every item's name and description, the buttons, the titles.
        const BlockMeasure p = MeasurePockets(*app.rom);
        printf("pockets: %d words, tallest description %d (%d lines), %d shrunk, %d cut, %d names cut", p.checked, p.tallestId, p.tallestLines, p.shrunk, p.cut, p.namesCut);
        for (int id : p.cutIds) printf(" %d", id);
        printf("\n");
        // Goals, Relationships, Options and the Save slots: every goal, Sim, mood, relationship,
        // button, volume, slot and place word in its box.
        BlockMeasure g;
        MeasureGoals(*app.rom, g);
        MeasureRelations(*app.rom, g);
        MeasureOptions(*app.rom, g);
        printf("tabs: %d words, tallest text %d (%d lines), %d shrunk, %d cut", g.checked, g.tallestId, g.tallestLines, g.shrunk, g.cut);
        for (int id : g.cutIds) printf(" %d", id);
        printf("\n");
        // Popups, the phone and notes: every popup text the game opens, the phone's rows, answers
        // and messages, the notes the code names.
        BlockMeasure q;
        MeasurePopups(*app.rom, q);
        MeasurePhone(*app.rom, q);
        MeasureNotes(*app.rom, q);
        printf("popups: %d words, tallest text %d (%d lines), %d shrunk, %d cut", q.checked, q.tallestId, q.tallestLines, q.shrunk, q.cut);
        for (int id : q.cutIds) printf(" %d", id);
        printf("\n");
        // The shop's catalogue: every item's name and description, the shops' titles beside the
        // widest money, the headers, Pay at the widest price.
        BlockMeasure k;
        MeasureCatalogue(*app.rom, k);
        printf("catalogue: %d words, tallest description %d (%d lines), %d shrunk, %d cut, %d names cut", k.checked, k.tallestId, k.tallestLines, k.shrunk, k.cut, k.namesCut);
        for (int id : k.cutIds) printf(" %d", id);
        printf("\n");
        // Build Rooms: every room's name and place in its boxes, the title, the question, the sums.
        BlockMeasure bd;
        MeasureBuild(*app.rom, bd);
        printf("build: %d words, tallest %d (%d lines), %d shrunk, %d cut", bd.checked, bd.tallestId, bd.tallestLines, bd.shrunk, bd.cut);
        for (int id : bd.cutIds) printf(" %d", id);
        printf("\n");
        // A neighbour's name beside the plate may end in an ellipsis (the Italian table has a
        // sentence at an object's name, 463); every other box must hold its words.
        // The Registry and room info: the titles, the rooms, every guest, the lines, the hints.
        BlockMeasure ci;
        MeasureCheckIn(*app.rom, ci);
        printf("checkin: %d words, tallest %d (%d lines), %d shrunk, %d cut", ci.checked, ci.tallestId, ci.tallestLines, ci.shrunk, ci.cut);
        for (int id : ci.cutIds) printf(" %d", id);
        printf("\n");
        BlockMeasure ri;
        MeasureRoomInfo(*app.rom, ri);
        printf("roominfo: %d words, tallest %d (%d lines), %d shrunk, %d cut", ri.checked, ri.tallestId, ri.tallestLines, ri.shrunk, ri.cut);
        for (int id : ri.cutIds) printf(" %d", id);
        printf("\n");
        // The hotel's music panel and the stat panel: the titles, every song, the pages' words.
        BlockMeasure mu, sp;
        MeasureMusic(*app.rom, mu);
        printf("music: %d words, tallest %d (%d lines), %d shrunk, %d cut", mu.checked, mu.tallestId, mu.tallestLines, mu.shrunk, mu.cut);
        for (int id : mu.cutIds) printf(" %d", id);
        printf("\n");
        MeasureStatPanel(*app.rom, sp);
        printf("statpanel: %d words, tallest %d (%d lines), %d shrunk, %d cut", sp.checked, sp.tallestId, sp.tallestLines, sp.shrunk, sp.cut);
        for (int id : sp.cutIds) printf(" %d", id);
        printf("\n");
        // The minigame frame (port/minigame.*): the titles, the cards' board, the vacuum's line, the hints' game words.
        BlockMeasure mg;
        MeasureMinigames(*app.rom, mg);
        printf("minigame: %d words, %d shrunk, %d cut", mg.checked, mg.shrunk, mg.cut);
        for (int id : mg.cutIds) printf(" %d", id);
        printf("\n");
        // The front end's main menu: the four buttons' labels.
        BlockMeasure fe;
        MeasureFront(*app.rom, fe);
        printf("front: %d words, %d shrunk, %d cut", fe.checked, fe.shrunk, fe.cut);
        for (int id : fe.cutIds) printf(" %d", id);
        printf("\n");
        // Settings and Load-a-Sim: the titles, the pages, every credits line, the slots' words.
        BlockMeasure fm;
        MeasureFrontMenus(*app.rom, fm);
        printf("frontmenus: %d words, %d shrunk, %d cut", fm.checked, fm.shrunk, fm.cut);
        for (int id : fm.cutIds) printf(" %d", id);
        printf("\n");
        // Create-a-Sim: the pages' titles, the aspirations, Yes / No, the hints, every key.
        BlockMeasure ca;
        MeasureCas(*app.rom, ca);
        printf("cas: %d words, tallest %d (%d lines), %d shrunk, %d cut", ca.checked, ca.tallestId, ca.tallestLines, ca.shrunk, ca.cut);
        for (int id : ca.cutIds) printf(" %d", id);
        printf("\n");
        // Every character of those words must be in the font (else a '?' is drawn).
        const std::vector<u32> lacking = ui::MissingGlyphs();
        for (u32 cp : lacking) printf("the font lacks U+%04X\n", cp);
        return m.cut - m.cutIn[4] > 0 || p.cut > 0 || g.cut > 0 || q.cut > 0 || k.cut > 0 || bd.cut > 0 || ci.cut > 0 || ri.cut > 0 || mu.cut > 0 || sp.cut > 0 || mg.cut > 0 || fe.cut > 0 || fm.cut > 0 || ca.cut > 0 ||
                       !lacking.empty() ? 2 : 0;
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
        ui::ResetMissingGlyphs();
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
        // A character the font lacks would be drawn as '?'.
        const std::vector<u32> lacking = ui::MissingGlyphs();
        for (u32 cp : lacking) printf("the font lacks U+%04X\n", cp);
        return bad || raw || !lacking.empty() ? 2 : 0;
    }

    // A cartridge other than the European one: CONSOLE keeps its old rules; the window says so
    // over the game until A.
    if (!app.portSupported)
    {
        fprintf(stderr, "rom %s: THIS ROM IS NOT THE EUROPEAN ONE: CONSOLE USES ITS OLD SCREEN RULES\n", app.romCode.c_str());
        if (window)
        {
            std::string msg = OwnWord(app.rom.get(), OwnWordId::NotEU);
            if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, app.romCode);
            host.Message(app, input, msg);
        }
    }
    host.code = app.romCode;
    host.canUndo = FileExists(host.data.Undo(app.romCode)); // (a load from an earlier run can be undone too)
    host.Refresh(app, input); // (the toasts in the game's language before the menu is ever opened)
    if (window && (!host.saveElsewhere.empty() || saveMovedBack))
    {
        std::string msg = OwnWordIn(host.model.language, saveMovedBack ? OwnWordId::SaveMovedBack : OwnWordId::SaveElsewhere);
        if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, saveMovedBack ? Host::SavePath : host.saveElsewhere);
        video.Toast(msg, 10000);
    }
    // The quick save state (F5 / F7) in the settings folder with the slots, not beside the ROM.
    if (host.data.Ok()) quickState = host.data.GameDir(app.romCode) + "quick.st";
    Backups backups;
    if (window)
    {
        static bool audioOpen = false; // (once: Change game comes back here)
        if (!audioOpen) audio.Open();
        audioOpen = true;
        backups.Start(Host::SavePath);
    }
    // Every state load copies the game's save first (melonDS writes the cartridge's memory to disk
    // on a load): the backups' copy, which skips a copy equal to the newest.
    host.beforeLoad = [&backups]() { backups.Copy(); };
    // Continue: the place kept on quitting, once (then the game's own save leads again). Any other
    // start of the same ROM (the command line, a file chosen, a recent, a drop) drops its place:
    // the game's save leads from then on, and an older place never comes back over it.
    {
        StateMeta meta;
        const bool same = ReadMeta(host.data.SuspendMeta(app.romCode), meta) && meta.rom == host.romPath;
        bool drop = same && !continueSuspend;
        if (continueSuspend)
        {
            const bool ok = host.Load(app, host.data.Suspend(app.romCode));
            fprintf(stderr, "suspend state: %s\n", ok ? "loaded" : "cannot load");
            if (!ok) host.Say(video, OwnWord(app.rom.get(), host.restoreFailed ? OwnWordId::StFailed : OwnWordId::LaunchContinueFailed));
            drop = true;
        }
        if (drop)
        {
            std::error_code ec;
            for (const std::string& f : {host.data.Suspend(app.romCode), host.data.SuspendThumb(app.romCode), host.data.SuspendMeta(app.romCode)})
                std::filesystem::remove(f, ec);
            if (printShell && !continueSuspend) fprintf(stderr, "suspend state: dropped (a new start)\n");
        }
    }

    // frame counts the game's frames (shots, scripts); iter every turn of the loop, paused too.
    long frame = launchFrames; // (the launch panel's turns came first)
    bool shellChord = false;   // headless: the script's Start + Back held last frame
    PadState shellPrev;        // headless: the script's pad last frame (the shell's presses)
    bool saveFailed = false;   // the game's save did not write (said once while it lasts)
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
    int lastTouchX = -1, lastTouchY = -1; // the stylus's last point down (room info's hint: which line)
    bool wasTouching = false;
    // Fast boot (Settings::fastBoot; --fast-boot headless, logged only): from a fresh start, the
    // boot's splashes, which take no touch (port/front.h, BootPassesBy), run unpaced, silent and
    // seldom drawn, until the last splash ("Touch the Touch Screen"), the player's first press, or
    // kFastBootFrames. Nothing is pressed for the player.
    constexpr long kFastBootFrames = 1500;
    bool fastBoot = (window ? app.settings.fastBoot : fastBootFlag) && stateLoad.empty() && !continueSuspend && app.portSupported;
    // (Only a press that begins once it runs ends it: a key or button still held from the launch
    // panel counts once let go, after a frame with nothing held.)
    bool fastBootArmed = false;
    if (fastBoot) fprintf(stderr, "fast boot: on\n");
    std::string pendingShot;      // a window screenshot asked for: said once the picture is written
    bool pointerHidden = false;   // the mouse pointer hidden (still for a while: Settings::hidePointer)
    auto keepPace = [&]() {
        // Fast forward at its speed (Settings::fastSpeed; 0: as fast as it goes), fast boot unpaced.
        const int speed = app.fast ? app.settings.fastSpeed : 1;
        if (headless || fastBoot || speed < 1) { nextFrameAt = 0; return; }
        const double step = dsFrame / speed;
        double now = SDL_GetPerformanceCounter() / perfFreq;
        if (nextFrameAt == 0 || now - nextFrameAt > 4 * step) nextFrameAt = now; // start, or fell behind
        nextFrameAt += step;
        double wait = nextFrameAt - now;
        if (wait > 0.002) SDL_Delay((Uint32)((wait - 0.001) * 1000)); // sleep most of it
        while (SDL_GetPerformanceCounter() / perfFreq < nextFrameAt) {} // then the last moment
    };
    // This frame's shots (the pause menu's items over the picture when it is open headless).
    auto writeShots = [&](bool withShell) {
        const u32* top = app.Top();
        const u32* bottom = app.Bottom();
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
                if (withShell)
                {
                    // (Above every layer of the game's: the window draws it on a canvas of its own.)
                    std::vector<ui::UiItem> items = host.shell.Build(host.model);
                    for (ui::UiItem& it : items) it.layer += 20;
                    std::stable_partition(c.ui.begin(), c.ui.end(), [](const ui::UiItem& it) { return it.kind != ui::UiItem::Image; });
                    const auto firstImage = std::find_if(c.ui.begin(), c.ui.end(), [](const ui::UiItem& it) { return it.kind == ui::UiItem::Image; });
                    const auto shellImages = std::stable_partition(items.begin(), items.end(), [](const ui::UiItem& it) { return it.kind != ui::UiItem::Image; });
                    std::vector<ui::UiItem> merged(c.ui.begin(), firstImage);
                    merged.insert(merged.end(), items.begin(), shellImages);
                    // The game's pictures lie under the menu: dropped (images are drawn above every still item).
                    merged.insert(merged.end(), shellImages, items.end());
                    c.ui = std::move(merged);
                }
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
    };
    for (long iter = 0; (frames < 0 || iter < frames) && !app.quit; iter++)
    {
        applyUi();
        u32 keys = 0;
        bool blow = false, touching = false;
        int tx = 0, ty = 0;
        ApplyScript(script, frame, keys, blow, touching, tx, ty);
        ScriptPad(script, frame, input.scriptPad);
        if (const int l = ScriptLayout(script, frame); l >= 0) app.layout = (Layout)l;
        // (A script's typed text: Create-a-Sim's name, as from the keyboard.)
        if (const std::string t = ScriptText(script, frame); !t.empty() && Input::TypingName(app))
            for (size_t i = 0; i < t.size();)
            {
                size_t j = i + 1;
                while (j < t.size() && ((unsigned char)t[j] & 0xC0) == 0x80) j++;
                input.typed.push_back(t.substr(i, j - i));
                i = j;
            }
        if (headless)
        {
            // The pause menu: Start + Back held in the script (or --shell at its frame); while it
            // is open the game stands still and the frames still count (the script goes on).
            const PadState& sp = input.scriptPad;
            const bool chord = sp.start && sp.menu;
            host.frame = frame;
            if (!host.Open() && ((chord && !shellChord) || (shellPage >= 0 && frame == shellAt)))
            {
                host.OpenMenu(app, input, chord || shellPage == 5 ? shell::Page::Main : (shell::Page)shellPage);
                if (!chord && shellPage == 5) host.shell.AskQuit();
            }
            else if (host.Open() && chord && !shellChord && host.model.capturing < 0) { host.shell.Close(); if (printShell) fprintf(stderr, "frame %ld: shell closed\n", frame); }
            else if (host.Open()) host.Frame(app, video, input, padEdges(sp, shellPrev));
            shellChord = chord;
            shellPrev = sp;
            if (host.Open() || app.quit)
            {
                if (host.Open()) writeShots(true);
                frame++;
                wasPaused = true;
                continue;
            }
            if (wasPaused) { input.navigator.Resync(); wasPaused = false; }
            // CONSOLE's navigator from the script's "pad" events; its taps over the script's touch.
            input.Steer(app, input.scriptPad, keys);
            input.PointerTouch(app, touching, tx, ty);
            input.ScriptTouch(touching, tx, ty);
        }
        else
        {
            SDL_Event ev;
            input.PushScript(script, iter, video);
            while (SDL_PollEvent(&ev))
            {
                video.Event(ev);
                input.HandleEvent(ev, app, video, host.Open());
            }
            // A controller connected (at start too): how to pause, now that Guide is Steam's.
            for (const Pads::Connected& pc : input.pads.TakeConnected())
            {
                std::string msg = OwnWord(app.rom.get(), OwnWordId::PadConnected);
                const std::string name = pc.name.empty() ? "?" : pc.name.substr(0, 40);
                if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, name);
                if (size_t at = msg.find("@2"); at != std::string::npos) msg.replace(at, 2, PauseCombo(pc.style, pc.type));
                video.Toast(msg, 5000);
            }
            if (ScriptWindowShot(script, iter))
            {
                char name[64];
                snprintf(name, sizeof name, "/w%06ld.png", iter);
                video.shotPath = shotDir + name;
            }
            // The window's close: asked first (focus on No); asked again while the question is up: quit.
            if (input.wantQuit) host.CloseAsked(app, video, input);
            input.droppedFile.clear(); // (a file dropped mid-game: the launch panel's only)
            if (app.quit) break;
            host.frame = frame;
            input.Poll(app, host.Open());
            // The keyboard's text only on Create-a-Sim's Name page (not under the pause menu).
            video.TextInput(!host.Open() && Input::TypingName(app));
            // The mouse pointer hidden after three seconds still over the game (never over a menu).
            {
                const bool hide = app.settings.hidePointer && !host.Open() && !input.mouseDown && !input.rightDown &&
                                  SDL_GetTicks() - input.mouseMovedAt > 3000;
                if (hide != pointerHidden) SDL_ShowCursor(hide ? SDL_DISABLE : SDL_ENABLE);
                pointerHidden = hide;
            }
            if (host.Open())
            {
                // The pause buttons again (Start + Back, the MENU key): back to the game.
                if (input.wantMenu && host.model.capturing < 0 && host.model.message.empty()) host.shell.Close();
                else host.Frame(app, video, input, shellInput(input.nav));
            }
            else if (input.wantMenu || (shellPage >= 0 && frame == shellAt && iter == shellAt))
            {
                host.OpenMenu(app, input, input.wantMenu || shellPage == 5 ? shell::Page::Main : (shell::Page)shellPage);
                if (!input.wantMenu && shellPage == 5) host.shell.AskQuit();
            }
            if (host.Open() || app.quit)
            {
                // Paused: the last picture with the menu over it; nothing is emulated.
                input.EndFrame();
                if (app.quit) break;
                const std::vector<ui::UiItem> items = host.shell.Build(host.model);
                video.Present(app, Video::Marks {}, &items);
                keepPace();
                wasPaused = true;
                continue;
            }
            // The window not in front: the game stands still and is silent (unless it runs in the
            // background), the last picture shown with a pause mark; the computer waits for an event.
            if (!input.focused && !app.settings.runInBackground)
            {
                audio.Pause(true);
                input.EndFrame();
                Video::Marks pm;
                pm.paused = true;
                video.Present(app, pm, nullptr);
                SDL_WaitEventTimeout(nullptr, 100);
                nextFrameAt = 0;
                wasPaused = true;
                continue;
            }
            audio.Pause(false);
            const int lang = host.model.language;
            if (input.wantSave)
            {
                if (host.data.Ok()) host.QuickSave(app, video);
                else host.Say(video, OwnWordIn(lang, SaveState(nds, quickState) ? OwnWordId::ToastStateSaved : OwnWordId::StFailed));
            }
            if (input.wantLoad)
            {
                if (host.data.Ok()) host.QuickLoad(app, video);
                else
                {
                    const bool ok = host.Load(app, quickState);
                    host.Say(video, OwnWordIn(lang, ok ? OwnWordId::ToastStateLoaded : host.restoreFailed ? OwnWordId::StFailed : OwnWordId::ToastNoState));
                }
            }
            if (input.wantShot && pendingShot.empty())
            {
                // The window's picture as it is drawn (its own size: HD), in the data folder's
                // screenshots/ (else beside the ROM), named by the time; said once written.
                const std::string dir = host.data.Ok() ? host.data.Screenshots() : shotFile.substr(0, shotFile.find_last_of("/\\") + 1);
                const long long now = (long long)time(nullptr);
                int n = 1;
                while (FileExists(dir + ShotName(now, n)) && n < 100) n++;
                pendingShot = video.shotPath = dir + ShotName(now, n);
            }
            if (input.wantLayout >= 0) app.settings.layout = app.layout = (Layout)input.wantLayout;
            if (input.wantFullscreen)
            {
                video.RememberSize(app.settings); // (the window's, before it fills the screen)
                app.settings.fullscreen = !app.settings.fullscreen;
                video.SetFullscreen(app.settings.fullscreen);
                app.settings.Save(settingsPath);
            }
            if (wasPaused) { input.navigator.Resync(); wasPaused = false; }
            input.Frame(app, video, frame, keys, touching, tx, ty);
            input.EndFrame();
            if (backups.Tick(SDL_GetTicks())) host.Say(video, OwnWordIn(lang, OwnWordId::ToastBackup));
            // The game's save not written (a folder it cannot write to): said when it starts.
            if (Host::SaveFailed && !saveFailed)
            {
                std::string msg = OwnWordIn(lang, OwnWordId::ToastNotSaving);
                if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, Host::SavePath);
                video.Toast(msg, 8000);
            }
            saveFailed = Host::SaveFailed;
            // The save moved to the data folder mid-game (its folder became read-only): said, and
            // kept on the pause menu's main page.
            if (Host::SaveMoved && host.saveElsewhere != Host::SavePath)
            {
                host.saveElsewhere = Host::SavePath;
                backups.savePath = Host::SavePath; // (the copies follow the save: backups/ beside it)
                std::string msg = OwnWordIn(lang, OwnWordId::SaveElsewhere);
                if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, Host::SavePath);
                video.Toast(msg, 10000);
            }
        }

        // Playable: what the DS hears this frame (the setting, the blow key or the script's
        // "blow") and, in a window, the DS clock held to real time (fast forward, pauses).
        Host::MicMode = app.settings.mic;
        Host::MicBlow = app.micBlow || blow;
        Host::MicForce = app.micForce;
        if (!headless || app.fast) clock::Frame(nds, app.fast || (fastBoot && !headless));

        nds.SetKeyMask(~keys & 0xFFF);
        if (touching) nds.TouchScreen((melonDS::u16)tx, (melonDS::u16)ty); else nds.ReleaseScreen();
        if (touching && !wasTouching) { lastTouchX = tx; lastTouchY = ty; } // (the stylus's last point down: room info's hint)
        wasTouching = touching;
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
            {   // the phone's contact list the game is still building (or never agrees with): counted
                PhoneState& ph = app.game.town.phone;
                if (ph.valid && ph.page == 7 && !ph.contactsKnown && ph.contacts == 0)
                    ph.contactsWait = before.town.phone.valid ? before.town.phone.contactsWait + 1 : 1;
            }
            {   // the stat panel: the page left drawn while the pad's action goes by Main to another page
                StatState& st = app.game.stat;
                if (st.valid && st.page == 0 && before.stat.valid && input.navigator.Running())
                    st.held = before.stat.held > 0 ? before.stat.held : before.stat.page > 0 ? before.stat.page : 0;
            }
            {   // Load-a-Sim's "Loading" popup seen (kind 3): the list is done, never shown again after it
                LoadScreen& ld = app.game.front.load;
                const PopupState& p = app.game.town.popup;
                if (ld.valid)
                    ld.loading = (before.front.load.valid && before.front.load.loading) || (p.valid && p.kind == 3 && p.state >= 1 && p.state <= 3);
            }
            {   // room info's hint: which line the stylus opened it on (not in RAM), kept while it is up
                RoomInfoState& ri = app.game.roomInfo;
                // (a touch before the room info rose opened it, or something else: never a line's)
                if (ri.valid && !before.roomInfo.valid) lastTouchX = lastTouchY = -1;
                if (ri.valid && ri.hint)
                    ri.hintId = before.roomInfo.valid && before.roomInfo.hint && before.roomInfo.hintId >= 0 ? before.roomInfo.hintId
                                : lastTouchX >= 0 ? RoomInfoHintAt(lastTouchX, lastTouchY, ri.occupied) : -1;
            }
            {   // a minigame's help text (the Drencher's, the crime game's, the mixer's): which control the stylus
                // asked about (not in RAM), kept while it is up; a text up with no touch since the screen rose: none
                MinigameState& mg = app.game.minigame;
                if (app.game.screen != before.screen) lastTouchX = lastTouchY = -1;
                // the game's popup just gone (on any screen; only the minigames' box, BoxOf, reads
                // popupGone): its box still drawn this frame, as the game's slides out
                const PopupState& p0 = before.town.popup;
                const bool upBefore = p0.valid && p0.state >= 1 && p0.state <= 3, upNow = app.game.town.popup.valid && app.game.town.popup.state >= 1 && app.game.town.popup.state <= 3;
                if (upBefore && !upNow && app.game.screen == before.screen) mg.popupGone = p0;
                // (decided once, when the text rises: kept while it is up, even when it was not
                // known then (-1), never taken from a later touch)
                auto fill = [&](bool up, bool wasUp, int was, int& id) {
                    if (up) id = wasUp ? was : lastTouchX >= 0 ? MinigameHelpAt(app.ConsoleScreen().screen, lastTouchX, lastTouchY) : -1;
                };
                fill(mg.drencher.valid && mg.drencher.helpText, before.minigame.drencher.valid && before.minigame.drencher.helpText, before.minigame.drencher.helpId, mg.drencher.helpId);
                fill(mg.crime.valid && mg.crime.helpText, before.minigame.crime.valid && before.minigame.crime.helpText, before.minigame.crime.helpId, mg.crime.helpId);
                fill(mg.mixer.valid && mg.mixer.phase == 9, before.minigame.mixer.valid && before.minigame.mixer.phase == 9, before.minigame.mixer.helpId, mg.mixer.helpId);
            }
            if (dialogueString >= 0 && app.game.dialogue.valid) app.game.dialogue.stringId = dialogueString;
            if (fastBoot)
            {
                const bool held = keys || touching, pressed = held && fastBootArmed;
                if (!held) fastBootArmed = true;
                if (pressed || frame >= kFastBootFrames || !BootPassesBy(app.game))
                {
                    fastBoot = false;
                    fprintf(stderr, "frame %ld: fast boot: done (%s)\n", frame, pressed ? "a press" : frame >= kFastBootFrames ? "its time" : "the boot waits for a touch");
                }
            }
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
                        for (const InteractAction& x : v.actions) a += (a.empty() ? "" : ",") + std::to_string(x.code) + ":" + std::to_string(x.labelId) + (x.greyed ? ":grey" : "") + (x.amount >= 0 ? ":$" + std::to_string(x.amount) : "");
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
                        std::string l;
                        for (int k : v.contactList) l += (l.empty() ? "" : ",") + std::to_string(k);
                        fprintf(stderr, "frame %ld: phone page=%d row=%d scroll=%d shown=%d ring=%d contacts=%d/%d menu=%d,%d,%d,%d inbox=%s date=%d/%d end=%d maid=%d reply=%d caller=%d list=%s\n", frame, v.page, v.row, v.scroll, v.shownSlot, v.ringtone,
                                v.contacts, v.contactSim, v.menuIds[0], v.menuIds[1], v.menuIds[2], v.menuIds[3], e.empty() ? "-" : e.c_str(), v.month, v.day,
                                v.textEnd, v.maidCost, v.reply, v.caller, v.contactsKnown ? (l.empty() ? "-" : l.c_str()) : "?");
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
            if (app.portSupported && printMinigame)
            {   // the minigames (gamestate.h): one line when the screen's part changes
                const std::string now = MinigameLine(app.game), was = MinigameLine(before);
                if (now != was || frame == 0) fprintf(stderr, "frame %ld: minigame %s\n", frame, now.empty() ? "none" : now.c_str());
                // A pointing minigame's stylus at its pointer (activities.h): its press and release.
                static bool pointerDown = false;
                if (app.consolePointer.down != pointerDown)
                {
                    pointerDown = app.consolePointer.down;
                    fprintf(stderr, "frame %ld: pointer %s at %d,%d\n", frame, pointerDown ? "down" : "up", (int)app.consolePointer.x, (int)app.consolePointer.y);
                }
            }
            if (app.portSupported && printHotel)
            {   // the hotel and the shops (gamestate.h), one line per part when it changes
                const GameState& g = app.game;
                const GameState& b = before;
                auto num = [](int n) { return std::to_string(n); };
                auto join = [](const std::vector<std::string>& v) { std::string r; for (const std::string& x : v) r += (r.empty() ? "" : ",") + x; return r.empty() ? std::string("-") : r; };
                if (!(g.hotel == b.hotel) || frame == 0)
                {
                    const HotelState& v = g.hotel;
                    if (!v.valid) fprintf(stderr, "frame %ld: hotel none\n", frame);
                    else
                    {
                        std::vector<std::string> sl, so, sg;
                        for (const GuestSlot& x : v.slots) sl.push_back(num(x.place) + ":" + num(x.sim) + ":" + num(x.days));
                        for (int k = 0; k < kPlaces; k++) if (v.rooms[k].permit) so.push_back(num(k) + ":" + num(v.rooms[k].permit) + "/" + num(v.rooms[k].weight) + "/" + num(v.rooms[k].parent));
                        for (int k = 0; k < kPlaces; k++) sg.push_back(num(v.songs[k]));
                        fprintf(stderr, "frame %ld: hotel built=0x%llx unlocked=0x%llx building=0x%llx score=%d stayed=%d money=%d earned=%d spent=%d place=%d slots=%s rooms=%s songs=%s\n", frame, (unsigned long long)v.built,
                                (unsigned long long)v.unlocked, (unsigned long long)v.building, v.score, v.guestsStayed, v.money, v.earned, v.spent, v.place, join(sl).c_str(), join(so).c_str(), join(sg).c_str());
                    }
                }
                if (!(g.checkIn == b.checkIn) || frame == 0)
                {
                    const CheckInState& v = g.checkIn;
                    if (!v.valid) fprintf(stderr, "frame %ld: checkin none\n", frame);
                    else
                    {
                        std::vector<std::string> r;
                        for (const RegistryRow& x : v.rows) r.push_back(num(x.place) + ":" + (x.built ? "b" : "-") + (x.available ? "a" : "-") + ":" + num(x.occupant));
                        fprintf(stderr, "frame %ld: checkin mode=%d guest=%d score=%d rows=%s\n", frame, v.mode, v.guest, v.score, join(r).c_str());
                    }
                }
                if (!(g.roomInfo == b.roomInfo) || frame == 0)
                {
                    const RoomInfoState& v = g.roomInfo;
                    if (!v.valid) fprintf(stderr, "frame %ld: roominfo none\n", frame);
                    else
                        fprintf(stderr, "frame %ld: roominfo place=%d occupant=%d occupied=%d mood=%d checkout=%d/%d/%d days=%d base=%d service=%d bonus=%d total=%d hint=%d%s\n", frame, v.place, v.occupant, v.occupied ? 1 : 0,
                                v.moodId, v.checkOutKnown ? 1 : 0, v.month, v.day, v.days, v.baseValue, v.serviceCharge, v.scoreBonus, v.total, v.hint ? 1 : 0,
                                v.hint ? (":" + std::to_string(v.hintId)).c_str() : "");
                }
                if (!(g.music == b.music) || frame == 0)
                {
                    const MusicState& v = g.music;
                    if (!v.valid) fprintf(stderr, "frame %ld: music none\n", frame);
                    else
                    {
                        std::vector<std::string> l;
                        for (const SongRow& x : v.list) l.push_back(num(x.index) + ":" + num(x.code) + ":" + num(x.nameId));
                        fprintf(stderr, "frame %ld: music scroll=%d playing=%d place=%d roomsong=%d list=%s\n", frame, v.scroll, v.playing, v.place, v.roomSong, join(l).c_str());
                    }
                }
                if (!(g.build == b.build) || frame == 0)
                {
                    const BuildState& v = g.build;
                    if (!v.valid) fprintf(stderr, "frame %ld: build none\n", frame);
                    else
                    {
                        std::vector<std::string> l;
                        for (const BuildEntry& x : v.places) l.push_back(num(x.place) + ":" + num(x.price) + ":" + num(x.parentNameId) + (x.affordable ? "" : ":poor"));
                        fprintf(stderr, "frame %ld: build scroll=%d page=%d chosen=%d money=%d places=%s\n", frame, v.scroll, v.page, v.chosen, v.money, join(l).c_str());
                    }
                }
                if (!(g.placement == b.placement) || frame == 0)
                {
                    const PlacementState& v = g.placement;
                    if (!v.valid) fprintf(stderr, "frame %ld: placement none\n", frame);
                    else
                    {
                        std::vector<std::string> l;
                        for (const PlaceObject& x : v.objects) l.push_back(num(x.id) + ":" + num(x.x) + ":" + num(x.y) + ":" + num(x.rotation));
                        fprintf(stderr, "frame %ld: placement mode=%d room=%d phase=%d slot=%d object=%d rot=%d objects=%s\n", frame, v.mode, v.room, v.phase, v.pocketSlot, v.object, v.rotation, join(l).c_str());
                    }
                }
                if (!(g.catalogue == b.catalogue) || frame == 0)
                {
                    const CatalogueState& v = g.catalogue;
                    if (!v.valid) fprintf(stderr, "frame %ld: catalogue none\n", frame);
                    else
                    {
                        std::vector<std::string> l, o;
                        for (const ShopItem& x : v.items) l.push_back(num(x.id) + ":" + num(x.nameId) + ":" + num(x.iconRes) + ":" + num(x.buyPrice));
                        for (const ShopItem& x : v.owned) o.push_back(num(x.id) + ":" + num(x.nameId) + ":" + num(x.iconRes) + ":" + num(x.sellPrice));
                        fprintf(stderr, "frame %ld: catalogue shop=%d vendor=%d sel=%d sell=%d scroll=%d pay=%d up=%d down=%d ring=%d money=%d items=%s owned=%s\n", frame, v.shop, v.vendor ? 1 : 0, v.selected, v.sell ? 1 : 0, v.scroll,
                                v.payEnabled ? 1 : 0, v.upEnabled ? 1 : 0, v.downEnabled ? 1 : 0, v.ring ? 1 : 0, v.money, join(l).c_str(), join(o).c_str());
                    }
                }
                if (!(g.stat == b.stat) || frame == 0)
                {
                    const StatState& v = g.stat;
                    if (!v.valid) fprintf(stderr, "frame %ld: stat none\n", frame);
                    else
                        fprintf(stderr, "frame %ld: stat page=%d transition=%d aspiration=%d level=%d points=%d gauge=%d skills=%d,%d,%d,%d,%d score=%d stayed=%d earned=%d spent=%d aliens=%d goons=%d vacuum=%d metal=%d%s\n", frame, v.page,
                                v.transition, v.aspiration, v.level, v.points, v.gaugeFrame, v.skills[0], v.skills[1], v.skills[2], v.skills[3], v.skills[4], v.score, v.guestsStayed, v.earned, v.spent, v.aliens, v.goons, v.vacuum, v.metal,
                                v.held > 0 ? (" held=" + std::to_string(v.held)).c_str() : "");
                }
            }
            if (app.portSupported && printFront)
            {   // the front end (gamestate.h): one line per part when it changes
                const FrontState& v = app.game.front;
                const FrontState& b = before.front;
                auto num = [](int n) { return std::to_string(n); };
                if (!(v == b) || frame == 0)
                {
                    fprintf(stderr, "frame %ld: front %s\n", frame, FrontKindName(v.kind));
                    if (!(v.title == b.title) || frame == 0)
                    {
                        if (v.title.valid) fprintf(stderr, "frame %ld: title phase=%d menu=%d button=%d\n", frame, v.title.phase, v.title.menu ? 1 : 0, v.title.button);
                        else fprintf(stderr, "frame %ld: title none\n", frame);
                    }
                    if (!(v.settings == b.settings) || frame == 0)
                    {
                        const SettingsState& s = v.settings;
                        if (!s.valid) fprintf(stderr, "frame %ld: settings none\n", frame);
                        else fprintf(stderr, "frame %ld: settings page=%d music=%d sfx=%d art=%d concept=%s credits=%s\n", frame, s.page, s.music, s.sfx, s.art ? 1 : 0,
                                     s.conceptKnown ? num(s.conceptBits).c_str() : "?", s.creditsKnown ? (num(s.creditsPos) + "/" + num(s.creditsLine)).c_str() : "?");
                    }
                    if (!(v.load == b.load) || frame == 0)
                    {
                        const LoadScreen& l = v.load;
                        if (!l.valid) fprintf(stderr, "frame %ld: load none\n", frame);
                        else
                        {
                            std::string rows;
                            for (const LoadRow& r : l.rows)
                            {
                                std::string e;
                                if (!r.used) e = "-";
                                else
                                {
                                    char t[40] = "?";
                                    if (r.timeKnown) snprintf(t, sizeof t, "%d/%d %d:%02d%s", r.month, r.day, r.hour12, r.minute, r.pm ? "pm" : "am");
                                    e = r.name + ":" + num(r.money) + ":" + num(r.placeNameId) + ":" + t;
                                }
                                rows += (rows.empty() ? "" : ",") + e;
                            }
                            fprintf(stderr, "frame %ld: load wireless=%d green=%d tapped=%d rows=%s%s\n", frame, l.wireless ? 1 : 0, l.green, l.tapped, rows.c_str(), l.loading ? " loading" : "");
                        }
                    }
                    if (!(v.cas == b.cas) || frame == 0)
                    {
                        const CasState& c = v.cas;
                        if (!c.valid) fprintf(stderr, "frame %ld: cas none\n", frame);
                        else
                        {
                            fprintf(stderr, "frame %ld: cas page=%d gender=%d simname=%s skin=%d haircolour=%d", frame, c.page, c.gender, c.simName.c_str(), c.skin, c.hairColour);
                            if (c.nameKnown) fprintf(stderr, " name=%s len=%d keyboard=%d", c.name.c_str(), c.nameLength, c.keyboard);
                            if (c.hairKnown) fprintf(stderr, " hair=%d scroll=%d", c.hairStyle, c.hairScroll);
                            if (c.face >= 0) fprintf(stderr, " face=%d", c.face);
                            if (c.clothesKnown) fprintf(stderr, " category=%d clothespage=%d item=%d", c.category, c.clothesPage, c.item);
                            if (c.keepKnown) fprintf(stderr, " aspiration=%d answer=%d", c.aspiration, c.answer);
                            if (c.switching) fprintf(stderr, " switching");
                            fprintf(stderr, "\n");
                            if (c.arrowsKnown) fprintf(stderr, "frame %ld: cas-sprites left=%d right=%d cell=%d pos=%d\n", frame, c.canLeft ? 1 : 0, c.canRight ? 1 : 0, c.clothesCell, c.hairPos);
                        }
                    }
                    if (!(v.boot == b.boot) || frame == 0)
                    {
                        if (v.boot.valid) fprintf(stderr, "frame %ld: boot mode=%d count=%d duration=%d top=%d\n", frame, v.boot.mode, v.boot.countdown, v.boot.duration, v.boot.top ? 1 : 0);
                        else fprintf(stderr, "frame %ld: boot none\n", frame);
                    }
                    if (!(v.video == b.video) || frame == 0)
                    {
                        if (v.video.valid) fprintf(stderr, "frame %ld: video movie=%d\n", frame, v.video.movie);
                        else fprintf(stderr, "frame %ld: video none\n", frame);
                    }
                }
            }
            if (app.portSupported && printGame && (!(app.game == before) || frame == 0)) fprintf(stderr, "frame %ld: game %s\n", frame, GameStateName(app.game).c_str());
        }
        if (recorder && recorder->Frame(app, frame, nds) && window) host.Say(video, OwnWord(app.rom.get(), OwnWordId::ToastCapture));
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
            // game's when it reads) starts without the old menu and focus (a popup coming over the
            // panel keeps its menu beneath: port/popup.h).
            const ScreenState consoleNow = app.ConsoleScreen();
            if (!(consoleNow == consoleBefore) && !app.consolePinned && consoleNow.screen != Screen::Panel &&
                !(app.portSupported && PopupOver(consoleNow, app.game)))
            {
                app.consoleMenu = false;
                app.consoleFocus = -1;
            }
            input.FocusForDrawing(app, !(consoleNow == consoleBefore)); // (the focus by id on the view drawn now)
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
        {
            // What CONSOLE shows this frame (consolehold.h): its own composition, or the last HD
            // one held through a swap of the game's.
            const ConsoleBranch fresh = app.consoleHold.Branch(), shown = app.consoleHold.ShownBranch();
            const bool held = app.consoleHold.Holding();
            app.consoleHold.Frame(app);
            if (printConsole && app.shown == Layout::Console &&
                (frame == 0 || fresh != app.consoleHold.Branch() || shown != app.consoleHold.ShownBranch() || held != app.consoleHold.Holding()))
                fprintf(stderr, "frame %ld: console %s%s\n", frame, ConsoleBranchName(app.consoleHold.Branch()),
                        app.consoleHold.Holding() ? (std::string(" held ") + ConsoleBranchName(app.consoleHold.ShownBranch())).c_str() : "");
        }
        writeShots(false);
        frame++;

        if (headless) continue;
        audio.Frame(nds, app.fast || fastBoot, app.settings);
        // Fast forward: one picture in four (as fast as it goes) or in its speed; fast boot one in eight.
        const int every = fastBoot ? 8 : app.fast ? (app.settings.fastSpeed >= 2 ? app.settings.fastSpeed : 4) : 1;
        if (frame % every == 0)
        {
            Video::Marks marks;
            marks.cursor = frame < input.cursorShownUntil;
            marks.x = input.cursorX;
            marks.y = input.cursorY;
            marks.press = input.cursorPress;
            marks.fast = app.fast || fastBoot;
            video.Present(app, marks, nullptr);
            if (!pendingShot.empty() && video.shotPath.empty())
            {
                std::string msg = OwnWordIn(host.model.language, FileExists(pendingShot) ? OwnWordId::ToastShotAt : OwnWordId::StFailed);
                if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, BaseName(pendingShot));
                host.Say(video, msg);
                pendingShot.clear();
            }
        }
        keepPace();
    }

    Looks::Shutdown();
    if (app.settings.interpolate60) fprintf(stderr, "looks: %u in-between frames\n", melonDS::Looks3D.InterpolatedFrames);
    if (!stateSave.empty()) fprintf(stderr, "state save %s: %s\n", stateSave.c_str(), SaveState(nds, stateSave) ? "ok" : "failed");
    if (printConsole)
        fprintf(stderr, "console: %ld frames, %ld milestone-1 compositions, %ld held, %ld milestone-1 shown\n", app.consoleHold.frames,
                app.consoleHold.fresh1, app.consoleHold.held, app.consoleHold.shown1);
    if (headless)
    {
        // For the tests: what the DS clock says at the end, and whether the game listened.
        clock::Time t = clock::Read(nds);
        const clock::Stats& cs = clock::GetStats();
        fprintf(stderr, "clock: %04d-%02d-%02d %02d:%02d:%02d (held %ld times, at most %ld s ahead of the computer, "
                "a minute ahead on %ld frames)  mic: %ld samples read by the game\n",
                t.year, t.month, t.day, t.hour, t.minute, t.second, cs.writes, cs.maxAhead, cs.minuteAhead, mic::SamplesRead());
    }
    // Change game: everything of this game let go, then the launch panel again.
    if (host.relaunch)
    {
        host.relaunch = false;
        host.shell.Close();
        relaunching = true;
        launchFrames = frame;
        stateLoad.clear();
        shellPage = -1;
        if (window)
        {
            video.RememberSize(app.settings);
            video.TextInput(false);
        }
        app.nds.reset();
        // (The 60 fps frames of the game before: never blended into the next one's first frame.)
        melonDS::Looks3D.RawPrevious.clear();
        melonDS::Looks3D.RawCurrent.clear();
        app.rom.reset();
        app.icons.reset();
        app.hudStrings = HudStrings();
        app.game = GameState();
        app.consoleHold.Reset();
        app.topBig.clear();
        app.topBigW = app.topBigH = 0;
        app.screens = ScreenTracker();
        app.autoLayout = AutoLayout();
        app.consoleMenu = consoleMenu;
        app.consoleFocus = consoleFocus;
        app.quit = app.fast = app.micBlow = app.micForce = false;
        input.navigator = Navigator();
        input.touchScript = TouchScript();
        goto relaunch;
    }
    if (window)
    {
        video.RememberSize(app.settings);
        if (!app.settings.Save(settingsPath)) fprintf(stderr, "cannot write %s\n", settingsPath.c_str());
        input.ClosePads();
        video.Close();
        SDL_Quit();
    }
    return 0;
}
