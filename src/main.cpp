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
#include "script.h"
#include "shots.h"
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
        "  --print-screen                print the recognised bottom screen when it changes (frame N: screen X)\n"
        "                                and CONSOLE's menu and focus (frame N: console menu open|closed, focus F)\n"
        "  --console-menu, --console-focus N  (checks) CONSOLE's menu shown, the focus ring on target N\n"
        "                                (pinned: the navigator no longer moves them)\n"
        "  --fast                        start with fast forward held (headless: the clock's hold is tested)\n"
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
    std::string romPath, inputPath, shotDir = "shots", stateLoad, stateSave, savePath, micArg;
    bool headless = false, fast = false;
    long frames = -1;
    int shotEvery = 0;
    std::set<long> shotAt;
    Layout layout = Layout::Console;
    bool layoutGiven = false;
    bool compose = false, printScreen = false, consoleMenu = false, consolePinned = false;
    int consoleFocus = -1;
    int res3d = 0;
    bool wide = false, interp = false;
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { Usage(); exit(1); } return argv[++i]; };
        if (a == "--help" || a == "-h") { Usage(); return 0; }
        else if (a == "--version") { printf("sims2wide %s\n", SIMS2WIDE_VERSION); return 0; }
        else if (a == "--headless") headless = true;
        else if (a == "--compose") compose = true;
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
        else if (a == "--console-menu") consoleMenu = consolePinned = true;
        else if (a == "--console-focus") { consoleFocus = atoi(next().c_str()); consolePinned = true; }
        else if (a == "--clock") { if (!clock::SetFixed(next())) { fprintf(stderr, "--clock wants \"YYYY-MM-DD HH:MM\"\n"); return 1; } }
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
    melonDS::NDSArgs args;
    args.JIT = std::nullopt;
    app.nds = std::make_unique<NDS>(std::move(args), nullptr);
    NDS& nds = *app.nds;
    nds.SetNDSCart(std::move(cart));
    nds.Reset();
    clock::Seed(nds); // the DS clock: the computer's local time (or --clock); LoadState re-seeds it
    nds.SetupDirectBoot("game.nds");
    nds.Start();
    if (!stateLoad.empty() && !LoadState(nds, stateLoad)) { fprintf(stderr, "cannot load state %s\n", stateLoad.c_str()); return 1; }

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
    // The window keeps the DS's own pace, 59.83 frames a second: vsync alone would follow the
    // screen, and on a 120 Hz screen (ProMotion, gaming monitors) the game ran twice too fast.
    const double dsFrame = 1.0 / (33513982.0 / (6.0 * 355.0 * 263.0)); // seconds
    const double perfFreq = (double)SDL_GetPerformanceFrequency();
    double nextFrameAt = 0; // in seconds of the performance counter; 0: not started
    bool wasPaused = false; // the pause menu was open: buttons held on leaving it are not presses
    bool lastMenu = app.consoleMenu;
    int lastFocus = app.consoleFocus;
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
            // The navigator ran before the tracker: a new screen starts without the old menu and focus.
            if (!(now == before) && !app.consolePinned && now.screen != Screen::Panel)
            {
                app.consoleMenu = false;
                app.consoleFocus = -1;
            }
            if (printScreen && !(now == before)) fprintf(stderr, "frame %ld: screen %s\n", frame, ScreenStateName(now).c_str());
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
            if (compose) WriteCompose(shotDir + name, app.shown == Layout::Console ? video.Composed(app) : Compose(app.shown, app.mainIsTop, (float)tw / th, tw, th), big, tw, th, bottom);
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
