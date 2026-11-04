// sims2wide: The Sims 2 (DS, 2005) on one screen. Proof of concept.
//
// The melonDS core runs the game; this frontend shows the two DS screens in one window with a
// choice of layouts, and has a headless mode that dumps both screens to PNG, to study which
// screen carries what in each situation of the game.
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "NDS.h"
#include "NDSCart.h"
#include "Args.h"
#include "GPU.h"
#include "SPU.h"
#include "Savestate.h"
#include "host.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "tinyfiledialogs.h"

using namespace melonDS;

static constexpr int SW = 256, SH = 192;

// DS key bits, in the order of melonDS's key mask.
enum DSKey { kA, kB, kSelect, kStart, kRight, kLeft, kUp, kDown, kR, kL, kX, kY };
static const std::map<std::string, int> kKeyNames = {
    {"a", kA}, {"b", kB}, {"select", kSelect}, {"start", kStart}, {"right", kRight}, {"left", kLeft},
    {"up", kUp}, {"down", kDown}, {"r", kR}, {"l", kL}, {"x", kX}, {"y", kY}};

// ---------------------------------------------------------------- input script
// One event per line: "<frame> <frames held> <what>", what is a key name, several joined by
// '+', or "touch <x> <y>". '#' starts a comment.
struct InputEvent { int frame, len; u32 keys; bool touch; int tx, ty; };

static std::vector<InputEvent> LoadScript(const std::string& path)
{
    std::vector<InputEvent> evs;
    std::ifstream in(path);
    if (!in) { fprintf(stderr, "cannot read input script %s\n", path.c_str()); exit(1); }
    std::string line;
    while (std::getline(in, line))
    {
        if (auto h = line.find('#'); h != std::string::npos) line.resize(h);
        std::istringstream ss(line);
        InputEvent e {};
        std::string what;
        if (!(ss >> e.frame >> e.len >> what)) continue;
        if (what == "touch") { e.touch = true; ss >> e.tx >> e.ty; }
        else
        {
            std::stringstream ks(what);
            std::string k;
            while (std::getline(ks, k, '+'))
            {
                auto it = kKeyNames.find(k);
                if (it == kKeyNames.end()) { fprintf(stderr, "unknown key %s\n", k.c_str()); exit(1); }
                e.keys |= 1u << it->second;
            }
        }
        evs.push_back(e);
    }
    return evs;
}

// ---------------------------------------------------------------- layouts
// Each layout places the top and bottom screens in a logical canvas; the window scales it.
enum class Layout { Stacked, Side, Focus, Auto };
struct Rect { int x, y, w, h; };
struct Placement { int cw, ch; Rect top, bottom; };

// Focus: the main screen fills a 4:3 area, the other one sits in a column on the right (16:9
// canvas overall). mainIsTop says which screen is the big one.
static Placement Place(Layout l, bool mainIsTop)
{
    switch (l)
    {
    case Layout::Stacked: return {SW, SH * 2, {0, 0, SW, SH}, {0, SH, SW, SH}};
    case Layout::Side:    return {SW * 2, SH, {0, 0, SW, SH}, {SW, 0, SW, SH}};
    case Layout::Focus:
    default:
    {
        Rect big {0, 0, 960, 720}, small {960, 240, 320, 240};
        return mainIsTop ? Placement {1280, 720, big, small} : Placement {1280, 720, small, big};
    }
    }
}

// Auto: the world view with its HUD strip (clock, money, motive bar) means free roam or a
// conversation in town, where the bottom screen is only a menu panel: Focus on the top screen.
// Anything else (title, Create-a-Sim, loading) needs both screens alike: Side.
static bool HudVisible(const u32* top)
{
    // The strip's bottom border, one flat colour from edge to edge; the rows above it carry
    // the clock's text and the motive bar. Checked against every situation shot so far: only
    // gameplay matches.
    for (int x = 8; x < 250; x += 8)
        if ((top[190 * SW + x] & 0xFFFFFF) != 0x415969) return false;
    return true;
}

struct AutoLayout
{
    bool hud = false;
    int streak = 0; // frames the detection has disagreed with the layout shown (hysteresis)
    Layout Update(const u32* top)
    {
        bool now = HudVisible(top);
        streak = now == hud ? 0 : streak + 1;
        if (streak >= 10) { hud = now; streak = 0; }
        return hud ? Layout::Focus : Layout::Side;
    }
};

// The window's picture on the CPU (nearest scaling), for headless previews of a layout.
static void WriteCompose(const std::string& path, Layout l, bool mainIsTop, const u32* top, const u32* bottom)
{
    Placement p = Place(l, mainIsTop);
    std::vector<u8> rgb(p.cw * p.ch * 3, 0);
    for (int i = 0; i < p.cw * p.ch; i++) { rgb[i * 3] = 16; rgb[i * 3 + 1] = 18; rgb[i * 3 + 2] = 24; }
    for (int s = 0; s < 2; s++)
    {
        const Rect& r = s ? p.bottom : p.top;
        const u32* src = s ? bottom : top;
        for (int y = 0; y < r.h; y++)
            for (int x = 0; x < r.w; x++)
            {
                u32 px = src[(y * SH / r.h) * SW + x * SW / r.w];
                u8* d = &rgb[((r.y + y) * p.cw + r.x + x) * 3];
                d[0] = (px >> 16) & 0xFF; d[1] = (px >> 8) & 0xFF; d[2] = px & 0xFF;
            }
    }
    stbi_write_png(path.c_str(), p.cw, p.ch, 3, rgb.data(), p.cw * 3);
}

// ---------------------------------------------------------------- shots
static void WriteShot(const std::string& path, const u32* top, const u32* bottom)
{
    std::vector<u8> rgb(SW * SH * 2 * 3);
    for (int s = 0; s < 2; s++)
    {
        const u32* src = s ? bottom : top;
        for (int i = 0; i < SW * SH; i++)
        {
            u32 p = src[i]; // 0xAARRGGBB
            u8* d = &rgb[(s * SW * SH + i) * 3];
            d[0] = (p >> 16) & 0xFF; d[1] = (p >> 8) & 0xFF; d[2] = p & 0xFF;
        }
    }
    stbi_write_png(path.c_str(), SW, SH * 2, 3, rgb.data(), SW * 3);
}

// ---------------------------------------------------------------- savestates
static bool SaveState(NDS& nds, const std::string& path)
{
    Savestate st;
    if (st.Error || !nds.DoSavestate(&st)) return false;
    st.Finish();
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(st.Buffer(), 1, st.Length(), f);
    fclose(f);
    return true;
}

static bool LoadState(NDS& nds, const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<char> buf((std::istreambuf_iterator<char>(in)), {});
    Savestate st(buf.data(), (u32)buf.size(), false);
    return !st.Error && nds.DoSavestate(&st);
}

// ---------------------------------------------------------------- main
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
        "  --version, --help\n"
        "keys: arrows, X=A Z=B S=X A=Y Q=L W=R Enter=Start RShift=Select\n"
        "      Tab swaps the big screen (focus), 0 auto, 1/2/3 layouts, Space fast forward\n"
        "      F5/F7 save/load state (<rom>.quick.st), F12 screenshot (<rom>-shot.png), Esc quit\n");
}

// A message for whoever started the program: on the terminal always, and in a dialog box too
// when there may be no terminal (started by a double click).
static void Tell(bool dialog, const char* type, const std::string& msg)
{
    fprintf(stderr, "%s\n", msg.c_str());
    if (dialog) tinyfd_messageBox("The Sims 2 DS, one screen", msg.c_str(), "ok", type, 1);
}

// The ROM chosen last time, kept in the per-user settings folder (SDL's pref path).
static std::string LastRomFile()
{
    char* dir = SDL_GetPrefPath("", "sims2-ds-wide");
    if (!dir) return "";
    std::string f = std::string(dir) + "last-rom.txt";
    SDL_free(dir);
    return f;
}

static std::string PickRom()
{
    std::string last, file = LastRomFile();
    if (std::ifstream in(file); in) std::getline(in, last);
    const char* patterns[] = {"*.nds"};
    const char* chosen = tinyfd_openFileDialog("Choose your copy of The Sims 2 (DS)", last.empty() ? nullptr : last.c_str(),
                                               1, patterns, "Nintendo DS ROM (.nds)", 0);
    if (!chosen) return "";
    std::string rom = chosen;
    if (!file.empty()) { std::ofstream out(file); out << rom << "\n"; }
    return rom;
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
    std::string romPath, inputPath, shotDir = "shots", stateLoad, stateSave, savePath;
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
    if (!rf) { Tell(dialogs, "error", "Cannot read " + romPath); return 1; }
    std::vector<u8> rom((std::istreambuf_iterator<char>(rf)), {});
    NDSCart::NDSCartArgs cartArgs {};
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
    auto cart = NDSCart::ParseROM(rom.data(), (u32)rom.size(), nullptr, std::move(cartArgs));
    if (!cart) { Tell(dialogs, "error", "Not a Nintendo DS ROM: " + romPath); return 1; }

    NDSArgs args;
    args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args), nullptr);
    nds->SetNDSCart(std::move(cart));
    nds->Reset();
    nds->SetupDirectBoot("game.nds");
    nds->Start();
    if (!stateLoad.empty() && !LoadState(*nds, stateLoad)) { fprintf(stderr, "cannot load state %s\n", stateLoad.c_str()); return 1; }

    std::vector<InputEvent> script;
    if (!inputPath.empty()) script = LoadScript(inputPath);
    if (!shotAt.empty() || shotEvery) { std::string cmd = "mkdir -p '" + shotDir + "'"; (void)system(cmd.c_str()); }

    // Window.
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex[2] = {};
    SDL_AudioDeviceID audio = 0;
    if (!headless)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        win = SDL_CreateWindow("The Sims 2 DS - one screen (POC)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        for (auto& t : tex) t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        SDL_AudioSpec want {}, have {};
        want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
        audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (audio) SDL_PauseAudioDevice(audio, 0);
    }

    bool mainIsTop = true, fast = false, mouseDown = false;
    AutoLayout autoLayout;
    Layout shown = layout == Layout::Auto ? Layout::Side : layout; // what the window shows this frame
    int mouseX = 0, mouseY = 0;
    u32 heldKeys = 0;
    const std::map<SDL_Keycode, int> keymap = {
        {SDLK_x, kA}, {SDLK_z, kB}, {SDLK_s, kX}, {SDLK_a, kY}, {SDLK_q, kL}, {SDLK_w, kR},
        {SDLK_RETURN, kStart}, {SDLK_RSHIFT, kSelect}, {SDLK_UP, kUp}, {SDLK_DOWN, kDown},
        {SDLK_LEFT, kLeft}, {SDLK_RIGHT, kRight}};
    std::vector<s16> abuf(4096 * 2);

    // Maps a window position to the canvas, then to a DS touch-screen position (or -1).
    auto touchAt = [&](int wx, int wy, int& tx, int& ty) -> bool {
        Placement p = Place(shown, mainIsTop);
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        float px = wx * (float)ow / ww, py = wy * (float)oh / wh; // window points -> pixels
        float scale = std::min((float)ow / p.cw, (float)oh / p.ch);
        float ox = (ow - p.cw * scale) / 2, oy = (oh - p.ch * scale) / 2;
        float cx = (px - ox) / scale, cy = (py - oy) / scale;
        const Rect& b = p.bottom;
        if (cx < b.x || cy < b.y || cx >= b.x + b.w || cy >= b.y + b.h) return false;
        tx = (int)((cx - b.x) * SW / b.w);
        ty = (int)((cy - b.y) * SH / b.h);
        return true;
    };

    for (long frame = 0; frames < 0 || frame < frames; frame++)
    {
        u32 keys = heldKeys;
        bool touching = false;
        int tx = 0, ty = 0;
        for (const auto& e : script)
            if (frame >= e.frame && frame < e.frame + e.len)
            {
                keys |= e.keys;
                if (e.touch) { touching = true; tx = e.tx; ty = e.ty; }
            }

        if (!headless)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
            {
                if (ev.type == SDL_QUIT) frames = frame;
                else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
                {
                    bool down = ev.type == SDL_KEYDOWN;
                    SDL_Keycode k = ev.key.keysym.sym;
                    if (auto it = keymap.find(k); it != keymap.end())
                    {
                        if (down) heldKeys |= 1u << it->second; else heldKeys &= ~(1u << it->second);
                    }
                    else if (down && !ev.key.repeat)
                    {
                        if (k == SDLK_TAB) mainIsTop = !mainIsTop;
                        else if (k == SDLK_0) layout = Layout::Auto;
                        else if (k == SDLK_1) layout = Layout::Focus;
                        else if (k == SDLK_2) layout = Layout::Side;
                        else if (k == SDLK_3) layout = Layout::Stacked;
                        else if (k == SDLK_F5) fprintf(stderr, "state save: %s\n", SaveState(*nds, quickState) ? "ok" : "failed");
                        else if (k == SDLK_F7) fprintf(stderr, "state load: %s\n", LoadState(*nds, quickState) ? "ok" : "failed");
                        else if (k == SDLK_F12) { WriteShot(shotFile, nds->GPU.Framebuffer[nds->GPU.FrontBuffer][0].get(), nds->GPU.Framebuffer[nds->GPU.FrontBuffer][1].get()); }
                        else if (k == SDLK_ESCAPE) frames = frame;
                    }
                    if (k == SDLK_SPACE) fast = down;
                }
                else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) { mouseDown = true; mouseX = ev.button.x; mouseY = ev.button.y; }
                else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) mouseDown = false;
                else if (ev.type == SDL_MOUSEMOTION) { mouseX = ev.motion.x; mouseY = ev.motion.y; }
            }
            keys |= heldKeys;
            int mx, my;
            if (mouseDown && touchAt(mouseX, mouseY, mx, my)) { touching = true; tx = mx; ty = my; }
        }

        nds->SetKeyMask(~keys & 0xFFF);
        if (touching) nds->TouchScreen((u16)tx, (u16)ty); else nds->ReleaseScreen();
        nds->RunFrame();

        const u32* top = nds->GPU.Framebuffer[nds->GPU.FrontBuffer][0].get();
        const u32* bottom = nds->GPU.Framebuffer[nds->GPU.FrontBuffer][1].get();
        {
            Layout auto_ = autoLayout.Update(top);
            Layout before = shown;
            shown = layout == Layout::Auto ? auto_ : layout;
            if (shown != before) fprintf(stderr, "frame %ld: layout %s\n", frame, shown == Layout::Focus ? "focus" : shown == Layout::Side ? "side" : "stacked");
        }
        if (shotAt.count(frame) || (shotEvery && frame % shotEvery == 0))
        {
            char name[64];
            snprintf(name, sizeof name, "/f%06ld.png", frame);
            if (compose) WriteCompose(shotDir + name, shown, mainIsTop, top, bottom);
            else WriteShot(shotDir + name, top, bottom);
        }

        if (headless) continue;

        int n = nds->SPU.ReadOutput(abuf.data(), 4096);
        if (audio && !fast && SDL_GetQueuedAudioSize(audio) < 48000 * 4 / 5) SDL_QueueAudio(audio, abuf.data(), n * 4);
        if (fast && frame % 4) continue;

        SDL_UpdateTexture(tex[0], nullptr, top, SW * 4);
        SDL_UpdateTexture(tex[1], nullptr, bottom, SW * 4);
        Placement p = Place(shown, mainIsTop);
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        float scale = std::min((float)ow / p.cw, (float)oh / p.ch);
        float ox = (ow - p.cw * scale) / 2, oy = (oh - p.ch * scale) / 2;
        auto dst = [&](const Rect& r) { return SDL_Rect {(int)(ox + r.x * scale), (int)(oy + r.y * scale), (int)(r.w * scale), (int)(r.h * scale)}; };
        SDL_SetRenderDrawColor(ren, 16, 18, 24, 255);
        SDL_RenderClear(ren);
        SDL_Rect dt = dst(p.top), db = dst(p.bottom);
        SDL_RenderCopy(ren, tex[0], nullptr, &dt);
        SDL_RenderCopy(ren, tex[1], nullptr, &db);
        SDL_RenderPresent(ren);
    }

    if (!stateSave.empty()) fprintf(stderr, "state save %s: %s\n", stateSave.c_str(), SaveState(*nds, stateSave) ? "ok" : "failed");
    if (!headless) SDL_Quit();
    return 0;
}
