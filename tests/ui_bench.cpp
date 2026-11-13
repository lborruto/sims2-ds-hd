// Measures what a press costs on the HD panel menu (never part of a normal run, not a check):
// CONSOLE's real composition (BuildConsole: the five tabs of the panel's menu from made-up
// states, the way layout_test builds them, the game's words a made-up string table) rastered at
// the window's scale through the window's own cache (ui::LayerCache, what Video::DrawUi keeps),
// press after press: focus moves on each tab, then a tab change, three rounds (the first cold:
// no text drawn before). Prints the milliseconds per press and per idle frame, and a digest of
// every picture so two builds can be compared pixel for pixel.
// The window draws the live crops with the GPU (ui::WholeFactorCrop); --images-cpu rasters them
// as --compose does (the digest of two builds compares then).
// --threads N: the raster's threads (1: one; default: the machine's). --ui-scale N (percent) and
// --game-pictures smooth|pixel: the UI's size and the pictures' look (default 100, pixel).
//   ninja -C build ui_bench && ./build/ui_bench [scale] [--images-cpu] [--threads N] [--ui-scale N] [--game-pictures smooth]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "compose.h"
#include "console.h"
#include "gamestate.h"
#include "layout.h"
#include "port/menuview.h"
#include "port/panelmenu.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/kit.h"

using namespace sims2hd;

using Bytes = std::vector<u8>;
static void W16(Bytes& b, size_t at, u32 v) { b[at] = (u8)v; b[at + 1] = (u8)(v >> 8); }
static void W32(Bytes& b, size_t at, u32 v) { W16(b, at, v); W16(b, at + 2, v >> 16); }
static void Put16(Bytes& b, u32 v) { b.push_back((u8)v); b.push_back((u8)(v >> 8)); }
static void Put32(Bytes& b, u32 v) { Put16(b, v); Put16(b, v >> 16); }
static void PutName(Bytes& b, u8 type, const char* name) { b.push_back(type); while (*name) b.push_back((u8)*name++); }

// A string table in the game's format (as tests/romdata_test.cpp writes one): a balanced tree
// over the symbols, offsets, then each string's bits.
static Bytes StringFile(const std::vector<std::string>& strings)
{
    std::vector<u8> syms = {0, 'a'};
    for (const std::string& s : strings)
        for (char c : s) if (std::find(syms.begin(), syms.end(), (u8)c) == syms.end()) syms.push_back((u8)c);
    std::vector<u16> pairs;
    std::map<u8, std::vector<int>> codes;
    std::vector<int> path;
    auto build = [&](auto& self, size_t lo, size_t hi) -> int {
        if (hi - lo == 1) { codes[syms[lo]] = path; return syms[lo]; }
        const size_t k = pairs.size() / 2;
        pairs.push_back(0);
        pairs.push_back(0);
        const size_t mid = (lo + hi) / 2;
        path.push_back(0);
        const int left = self(self, lo, mid);
        path.back() = 1;
        const int right = self(self, mid, hi);
        path.pop_back();
        pairs[2 * k] = (u16)left;
        pairs[2 * k + 1] = (u16)right;
        return 0x100 + (int)k;
    };
    build(build, 0, syms.size());
    Bytes streams;
    std::vector<u32> starts;
    for (const std::string& s : strings)
    {
        starts.push_back((u32)streams.size());
        size_t nbits = 0;
        auto put = [&](const std::vector<int>& code) {
            for (int bit : code)
            {
                if (nbits % 8 == 0) streams.push_back(0);
                streams.back() |= (u8)(bit << (nbits % 8));
                nbits++;
            }
        };
        for (char c : s) put(codes[(u8)c]);
        put(codes[0]);
    }
    const u32 n = 4 + 2 * (u32)pairs.size();
    Bytes file;
    Put32(file, n);
    for (u16 p : pairs) Put16(file, p);
    for (u32 start : starts) Put32(file, n + 4 * (u32)starts.size() + start);
    file.insert(file.end(), streams.begin(), streams.end());
    return file;
}

// A cartridge holding rom.bin with the string file as resource 123.
static Bytes MakeRom(const Bytes& stringFile)
{
    std::vector<Bytes> resources(124);
    resources[123] = stringFile;
    Bytes b(0x50, 0);
    Bytes fnt;
    Put32(fnt, 8);
    Put16(fnt, 0);
    Put16(fnt, 1);
    PutName(fnt, 0x80 | 6, "Movies");
    Put16(fnt, 0xF001);
    PutName(fnt, 5, "a.bin");
    PutName(fnt, 7, "rom.bin");
    fnt.push_back(0);
    const size_t fntAt = b.size();
    b.insert(b.end(), fnt.begin(), fnt.end());
    while (b.size() % 4) b.push_back(0);
    Bytes arch;
    u32 at = 4 * (u32)resources.size();
    for (const Bytes& r : resources) { Put32(arch, at); at += (u32)r.size(); }
    for (const Bytes& r : resources) arch.insert(arch.end(), r.begin(), r.end());
    const size_t fat = b.size();
    const u32 start = (u32)(fat + 16);
    Put32(b, start); Put32(b, start);
    Put32(b, start); Put32(b, start + (u32)arch.size());
    b.insert(b.end(), arch.begin(), arch.end());
    W32(b, 0x40, (u32)fntAt); W32(b, 0x44, (u32)fnt.size()); W32(b, 0x48, (u32)fat); W32(b, 0x4C, 16);
    return b;
}

// Made-up words of a menu's lengths: names and verbs short, descriptions and goals long.
static std::vector<std::string> MadeUpStrings()
{
    static const char* words[] = {"Talk",  "about", "the",    "weather", "Give",  "a",       "gift",   "Hotel",  "garden",
                                  "Visit", "town",  "square", "Mayor",   "quiet", "lobby",   "Bring",  "three",  "golden",
                                  "rods",  "to",    "basement", "Check", "every", "room",    "Sanity", "Relax",  "music"};
    std::vector<std::string> v(3127);
    u32 seed = 12345;
    for (size_t id = 0; id < v.size(); id++)
    {
        const bool lng = (id >= 389 && id < 460) || (id >= 822 && id < 930) || (id >= 845 && id < 850) || (id >= 1627 && id < 1700) ||
                         (id >= 1972 && id < 1976);
        const int n = lng ? 10 + (int)(id % 9) : 1 + (int)(id % 4);
        std::string s;
        for (int i = 0; i < n; i++)
        {
            seed = seed * 1103515245u + 12345u;
            s += (i ? " " : "") + std::string(words[(seed >> 16) % (sizeof(words) / sizeof(words[0]))]);
        }
        v[id] = s;
    }
    v[28] = "$@1";
    v[1840] = "@1: @2";
    return v;
}

static GameState MadeUpGame()
{
    GameState g;
    g.valid = true;
    InteractState& in = g.town.interact;
    in.valid = true;
    in.target = {TargetRef::Object, 29, 492, 6638, false};
    in.prev = {TargetRef::Sim, 3, 326, 1, false};
    in.next = {TargetRef::Object, 15, 478, 5744, false};
    for (int i = 0; i < 4; i++) in.actions.push_back({i, 1845 + i, false});
    InventoryState& inv = g.town.inventory;
    inv.valid = true;
    for (int i = 0; i < 4; i++) inv.slots.push_back({100 + i, 563 + i, 922 + i, 1991, 1});
    for (int k = 0; k < 3; k++) inv.keys[k] = {k, 1968 + k, 1972 + k, -1, k == 1 ? 1 : 0};
    for (int i = 0; i < 4; i++) inv.counted[i] = {i, 1154 + i, 845 + i, -1, i * 3};
    g.town.pockets.valid = true;
    g.town.pockets.slot = 0;
    GoalsState& gs = g.town.goals;
    gs.valid = true;
    int line = 0;
    for (int i = 0; i < 8; i++)
    {
        const int lines = 1 + i % 3;
        gs.entries.push_back({true, i, 1627 + i, -1, -1, line, lines});
        line += lines;
    }
    gs.lines = line;
    gs.selected = 0;
    gs.first = 0;
    RelState& r = g.town.relations;
    r.valid = true;
    for (int i = 0; i < 5; i++) r.sims.push_back({i, 323 + i, 389 + i, 184, 195, -1, false});
    r.selected = 0;
    r.first = 0;
    g.town.options = {true, 4096, 2048};
    return g;
}

static u64 Mix(u64 h, u64 v) { return (h ^ v) * 0x100000001B3ull; }

int main(int argc, char** argv)
{
    float scale = 3;
    bool imagesCpu = false;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--images-cpu")) imagesCpu = true;
        else if (!strcmp(argv[i], "--threads") && i + 1 < argc) ui::SetRasterThreads(atoi(argv[++i]));
        else if (!strcmp(argv[i], "--ui-scale") && i + 1 < argc) ui::SetUiScale(atoi(argv[++i]) / 100.0f);
        else if (!strcmp(argv[i], "--game-pictures") && i + 1 < argc) ui::SetGamePictures(!strcmp(argv[++i], "smooth"));
        else scale = (float)atof(argv[i]);
    }
    RomData rom;
    if (!rom.Open(MakeRom(StringFile(MadeUpStrings()))) || !rom.LoadStrings(1)) { fprintf(stderr, "made-up strings refused\n"); return 1; }
    const GameState game = MadeUpGame();
    HudStrings strings;
    strings.ok = true;
    strings.clock = "@hh:@mm @am";
    strings.am = "am";
    strings.pm = "pm";
    strings.money = "$@1";
    strings.label = "Sanity";
    HudState hud;
    hud.valid = true;
    hud.money = 14843;
    hud.hour = 15;
    hud.minute = 37;
    hud.sanity = 150;
    hud.sanityMax = 225;
    hud.tier = 2;
    std::vector<u32> bottom(SW * SH), top(SW * SH);
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) bottom[y * SW + x] = top[y * SW + x] = 0xFF000000u | (u32)(x * 7 % 256) << 16 | (u32)(y * 5 % 256) << 8 | (u32)((x ^ y) & 0xFF);

    const int ow = (int)std::lround(1280 * scale), oh = (int)std::lround(720 * scale);
    const CanvasFit fit = CanvasFit::For(kConsoleW, kConsoleH, ow, oh);
    printf("ui_bench: output %d x %d, scale %.3f\n", ow, oh, fit.scale);

    if (getenv("UI_BENCH_PROFILE"))
    {
        // The menu's panel alone, over and over on one thread (for a profiler).
        ui::SetRasterThreads(1);
        const Composition c = BuildConsole({Screen::Panel, Tab::Pockets}, true, 0, SW, SH, &hud, &strings, &game, &rom, bottom.data(), nullptr);
        const ui::LayerAt l = ui::LayersAt(c.ui, fit.scale, fit.ox, fit.oy)[1];
        std::vector<u32> px((size_t)l.w * l.h);
        for (int k = 0; k < atoi(getenv("UI_BENCH_PROFILE")); k++)
        {
            ui::ClearTextMasks();
            ui::Rasterise(l.items, fit.scale, px.data(), l.w, l.h, l.ox, l.oy);
        }
        return 0;
    }
    ui::LayerCache cache;
    std::vector<u32> texture((size_t)ow * oh); // the upload's copy (SDL_UpdateTexture's memcpy)
    using Clock = std::chrono::steady_clock;
    auto ms = [](Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); };
    struct Sum { double total = 0, max = 0; int n = 0; void Add(double v) { total += v; max = std::max(max, v); n++; } };
    std::map<std::string, Sum> sums;
    u64 digest = 0xCBF29CE484222325ull;

    // One frame: compose, raster through the cache, copy what changed (the upload).
    auto frame = [&](ScreenState s, int focus, const char* kind) {
        const auto t0 = Clock::now();
        const Composition c = BuildConsole(s, true, focus, SW, SH, &hud, &strings, &game, &rom, bottom.data(), nullptr);
        const auto t1 = Clock::now();
        // The window's crops (Video::DrawUi): drawn by the GPU when the kit would draw them nearest.
        struct Ctx { const u32* bottom; float scale; } ctx {bottom.data(), fit.scale};
        auto direct = [](const ui::UiItem& im, const ui::LayerAt& at, void* u) {
            const Ctx& c = *(const Ctx*)u;
            return im.src == c.bottom && ui::WholeFactorCrop(im, at, c.scale);
        };
        if (imagesCpu) cache.Update(c.ui, fit.scale, fit.ox, fit.oy);
        else cache.Update(c.ui, fit.scale, fit.ox, fit.oy, direct, &ctx);
        const auto t2 = Clock::now();
        size_t pixels = 0;
        for (const ui::LayerCache::Entry& e : cache.entries)
            if (e.changed && !e.direct)
                for (const Rect& r : e.dirty)
                    for (int y = r.y; y < r.y + r.h; y++)
                    {
                        // (into the output-sized buffer, a layer wider than it cut: the copy's cost)
                        memcpy(&texture[(size_t)(e.y + y) % oh * ow], &e.px[(size_t)y * e.w + r.x], (size_t)std::min(r.w, ow) * 4);
                        pixels += r.w;
                    }
        const auto t3 = Clock::now();
        const double total = ms(t3 - t0);
        if (getenv("UI_BENCH_VERBOSE") && pixels)
        {
            printf("  %s drew %zu px in %.2f ms:", kind, pixels, ms(t2 - t1));
            for (const ui::LayerCache::Entry& e : cache.entries)
                if (e.changed)
                {
                    long long a = 0;
                    for (const Rect& r : e.dirty) a += (long long)r.w * r.h;
                    printf(" [%s %d at %d,%d %dx%d: %zu rects, %lld px]", e.still ? "layer" : "image", e.layer, e.x, e.y, e.w, e.h, e.dirty.size(), a);
                }
            printf("\n");
        }
        sums[std::string(kind) + " total"].Add(total);
        sums[std::string(kind) + " compose"].Add(ms(t1 - t0));
        sums[std::string(kind) + " raster"].Add(ms(t2 - t1));
        sums[std::string(kind) + " upload"].Add(ms(t3 - t2));
        for (const ui::LayerCache::Entry& e : cache.entries)
        {
            for (int v : {e.x, e.y, e.w, e.h}) digest = Mix(digest, (u64)(u32)v);
            if (e.direct) digest = Mix(digest, ui::HashImage(e.image));
            else
                for (u32 p : e.px) digest = Mix(digest, p);
        }
        return std::make_pair(total, pixels);
    };

    const Tab tabs[5] = {Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options};
    for (int round = 0; round < 3; round++)
    {
        const std::string pre = round == 0 ? "cold " : "warm ";
        for (int t = 0; t < 5; t++)
        {
            const ScreenState s {Screen::Panel, tabs[t]};
            const MenuView v = PanelMenuView(s, &game, &rom);
            if (!v.valid) { fprintf(stderr, "no view on tab %d\n", t); return 1; }
            int focus = MenuFirst(v);
            const auto [tt, tp] = frame(s, focus, round == 0 && t == 0 ? "open" : (pre + "tab change").c_str());
            printf("round %d tab %d: tab change %.2f ms (%zu px drawn)\n", round, t, tt, tp);
            frame(s, focus, "idle");
            // The world moving under the menu: the live crops (portrait, phone) change every frame.
            for (u32& p : bottom) p ^= 0x00010101;
            frame(s, focus, "idle, live crops");
            // Focus moves: Down, else Right, Up, Left: the next item the pad reaches.
            for (int m = 0; m < 5; m++)
            {
                int next = focus;
                for (int dir : {1, 3, 0, 2})
                    if ((next = MenuMove(v, focus, dir)) != focus && next >= 0) break;
                if (next < 0 || next == focus) break;
                focus = next;
                const auto [ft, fp] = frame(s, focus, (pre + "focus move").c_str());
                if (round == 0) printf("round %d tab %d: focus move to %d %.2f ms (%zu px drawn)\n", round, t, focus, ft, fp);
                frame(s, focus, "idle");
            }
        }
    }
    if (getenv("UI_BENCH_VERBOSE"))
    {
        // One full raster of each still layer of the last frame, warm, on all threads and on one;
        // and the same size with no item (the buffer and the straight-alpha pass alone).
        const Composition c = BuildConsole({Screen::Panel, Tab::Pockets}, true, 0, SW, SH, &hud, &strings, &game, &rom, bottom.data(), nullptr);
        for (int threads : {0, 1})
        {
            ui::SetRasterThreads(threads);
            for (const ui::LayerAt& l : ui::LayersAt(c.ui, fit.scale, fit.ox, fit.oy))
            {
                if (!l.still) continue;
                std::vector<u32> px((size_t)l.w * l.h);
                ui::Rasterise(l.items, fit.scale, px.data(), l.w, l.h, l.ox, l.oy);
                auto a = Clock::now();
                for (int k = 0; k < 5; k++) ui::Rasterise(l.items, fit.scale, px.data(), l.w, l.h, l.ox, l.oy);
                auto b = Clock::now();
                for (int k = 0; k < 5; k++) ui::Rasterise({}, fit.scale, px.data(), l.w, l.h, l.ox, l.oy);
                auto d = Clock::now();
                printf("threads %d layer %d (%dx%d, %zu items): %.2f ms, empty %.2f ms\n", threads, l.layer, l.w, l.h, l.items.size(), ms(b - a) / 5, ms(d - b) / 5);
                if (threads == 1)
                    for (const ui::UiItem& it : l.items)
                    {
                        auto e = Clock::now();
                        ui::Rasterise({it}, fit.scale, px.data(), l.w, l.h, l.ox, l.oy);
                        printf("    kind %d %dx%d at %d,%d: %.2f ms\n", (int)it.kind, it.to.w, it.to.h, it.to.x, it.to.y, ms(Clock::now() - e) - ms(d - b) / 5);
                    }
            }
        }
        ui::SetRasterThreads(0);
    }
    printf("\n%-26s %8s %8s %4s\n", "", "mean ms", "max ms", "n");
    for (const auto& [k, v] : sums) printf("%-26s %8.2f %8.2f %4d\n", k.c_str(), v.total / v.n, v.max, v.n);

    // The kit's own pictures beside the menu's (every chip look, several scales): the digest
    // covers them too, so a raster change shows here pixel for pixel.
    {
        std::vector<u32> pic(SW * SH);
        for (int i = 0; i < SW * SH; i++) pic[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
        for (int face = 0; face < 4; face++)
        {
            ui::ChipLook look;
            look.face = (ui::Face)face;
            if (face == 3)
                for (int b = 0; b < ui::kButtonCount; b++) look.glyph[b] = b % 2 ? "Esc" : "W";
            ui::SetChipLook(look);
            for (float sc : {1.0f, 1.5f, 2.0f, 2.5f, 3.0f})
            {
                const std::vector<ui::UiItem> items = ui::DemoItems(pic.data());
                for (const ui::LayerAt& l : ui::LayersAt(items, sc, 0.25f, 0.5f))
                {
                    std::vector<u32> px((size_t)l.w * l.h);
                    ui::Rasterise(l.items, sc, px.data(), l.w, l.h, l.ox, l.oy);
                    for (u32 p : px) digest = Mix(digest, p);
                }
            }
        }
        ui::SetChipLook({});
    }
    printf("\ndigest %016llx\n", (unsigned long long)digest);
    return 0;
}
