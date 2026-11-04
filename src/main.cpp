// sims2wide: The Sims 2 (DS, 2005) in one window.
//
// The melonDS core runs the game; this frontend shows the two DS screens in one window with a
// choice of layouts, and has a headless mode (input scripts, shots, save states) to study which
// screen carries what in each situation of the game. main.cpp reads the command line, starts the
// DS and runs the frame loop; the parts live beside it:
//   app.h      shared state          input.*   keyboard and mouse     video.*   the window
//   audio.*    sound                 layout.*  where the screens go   scene.*   AUTO's detection
//   script.*   headless input        shots.*   PNG pictures           state.*   save states
//   picker.*   ROM file picker and error dialogs
#include <SDL.h>

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
#include "host.h"
#include "input.h"
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
        "  --layout auto|focus|side|stacked  window layout (default auto)\n"
        "  --compose                     shots show the window picture (the layout), not the two screens\n"
        "  --headless                    no window; with --frames and the shot options\n"
        "  --frames N                    stop after N frames\n"
        "  --input FILE                  input script (frame len key[+key] | frame len touch x y)\n"
        "  --shot-dir DIR                where shots go (default shots)\n"
        "  --shot-every N | --shot-at a,b,c\n"
        "  --state-load FILE / --state-save FILE (saved at the end)\n"
        "  --save FILE                   the game's save (default beside the ROM, .sav)\n"
        "  --mic off|blow|real           what the DS hears: silence, the blow key, the computer's microphone\n"
        "  --version, --help\n"
        "keys: arrows, X=A Z=B S=X A=Y Q=L W=R Enter=Start RShift=Select\n"
        "      Tab swaps the big screen (focus), 0 auto, 1/2/3 layouts, Space fast forward\n"
        "      F5/F7 save/load state (<rom>.quick.st), F12 screenshot (<rom>-shot.png), Esc quit\n");
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
    bool headless = false;
    long frames = -1;
    int shotEvery = 0;
    std::set<long> shotAt;
    Layout layout = Layout::Auto;
    bool compose = false;
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { Usage(); exit(1); } return argv[++i]; };
        if (a == "--help" || a == "-h") { Usage(); return 0; }
        else if (a == "--version") { printf("sims2wide %s\n", SIMS2WIDE_VERSION); return 0; }
        else if (a == "--headless") headless = true;
        else if (a == "--compose") compose = true;
        else if (a == "--frames") frames = atol(next().c_str());
        else if (a == "--input") inputPath = next();
        else if (a == "--shot-dir") shotDir = next();
        else if (a == "--shot-every") shotEvery = atoi(next().c_str());
        else if (a == "--shot-at") { std::stringstream ss(next()); std::string t; while (std::getline(ss, t, ',')) shotAt.insert(atol(t.c_str())); }
        else if (a == "--state-load") stateLoad = next();
        else if (a == "--state-save") stateSave = next();
        else if (a == "--save") savePath = next();
        else if (a == "--mic") micArg = next();
        else if (a == "--layout")
        {
            std::string l = next();
            layout = l == "side" ? Layout::Side : l == "stacked" ? Layout::Stacked : l == "focus" ? Layout::Focus : Layout::Auto;
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
    app.layout = layout;
    app.shown = layout == Layout::Auto ? Layout::Side : layout;
    app.quickState = quickState;
    app.shotFile = shotFile;
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
    nds.SetupDirectBoot("game.nds");
    nds.Start();
    if (!stateLoad.empty() && !LoadState(nds, stateLoad)) { fprintf(stderr, "cannot load state %s\n", stateLoad.c_str()); return 1; }

    std::vector<InputEvent> script;
    if (!inputPath.empty()) script = LoadScript(inputPath);
    if (!shotAt.empty() || shotEvery) { std::string cmd = "mkdir -p '" + shotDir + "'"; (void)system(cmd.c_str()); }

    // Window and sound.
    Video video;
    Audio audio;
    Input input;
    if (!headless)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        if (!video.Open()) return 1;
        audio.Open();
    }

    for (long frame = 0; (frames < 0 || frame < frames) && !app.quit; frame++)
    {
        u32 keys = 0;
        bool blow = false, touching = false;
        int tx = 0, ty = 0;
        ApplyScript(script, frame, keys, blow, touching, tx, ty);
        if (!headless)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) input.HandleEvent(ev, app);
            if (app.quit) break;
            input.Frame(app, video, keys, touching, tx, ty);
        }

        // Playable: what the DS hears this frame (the setting, the blow key or the script's "blow").
        Host::MicMode = app.settings.mic;
        Host::MicBlow = app.micBlow || blow;

        nds.SetKeyMask(~keys & 0xFFF);
        if (touching) nds.TouchScreen((melonDS::u16)tx, (melonDS::u16)ty); else nds.ReleaseScreen();
        nds.RunFrame();

        const u32* top = app.Top();
        const u32* bottom = app.Bottom();
        {
            bool autoMainIsTop = app.mainIsTop;
            Layout autoShown = app.autoLayout.Update(top, bottom, autoMainIsTop);
            Layout before = app.shown;
            if (app.layout == Layout::Auto) { app.shown = autoShown; app.mainIsTop = autoMainIsTop; }
            else app.shown = app.layout;
            if (app.shown != before) fprintf(stderr, "frame %ld: layout %s\n", frame, LayoutName(app.shown));
        }
        if (shotAt.count(frame) || (shotEvery && frame % shotEvery == 0))
        {
            char name[64];
            snprintf(name, sizeof name, "/f%06ld.png", frame);
            if (compose) WriteCompose(shotDir + name, app.shown, app.mainIsTop, top, bottom);
            else WriteShot(shotDir + name, top, bottom);
        }

        if (headless) continue;
        audio.Frame(nds, app.fast);
        if (app.fast && frame % 4) continue; // fast forward: one picture in four
        video.Present(app);
    }

    if (!stateSave.empty()) fprintf(stderr, "state save %s: %s\n", stateSave.c_str(), SaveState(nds, stateSave) ? "ok" : "failed");
    if (headless)
    {
        // For the tests: whether the game listened.
        fprintf(stderr, "mic: %ld samples read by the game\n", mic::SamplesRead());
    }
    if (!headless) SDL_Quit();
    return 0;
}
