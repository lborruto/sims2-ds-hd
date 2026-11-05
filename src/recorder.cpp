#include "recorder.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

#include "shots.h"
#include "state.h"

namespace sims2wide
{
namespace
{
constexpr int kUnknownFrames = 30;   // a settled unmapped screen, this long
constexpr int kInvalidFrames = 300;  // RAM not reading (not a load, not black), this long
constexpr long kInvalidFrom = 600;   // ... after the boot
constexpr int kMaxPerRun = 20;
constexpr int kMaxInFolder = 200;
}

Recorder::Recorder(std::string dir) : dir_(std::move(dir))
{
    if (dir_.empty())
    {
        if (char* p = SDL_GetPrefPath("", "sims2-ds-wide")) { dir_ = std::string(p) + "captures"; SDL_free(p); }
    }
}

void Recorder::Frame(const App& app, long frame, melonDS::NDS& nds)
{
    if (dir_.empty() || count_ >= kMaxPerRun || !app.portSupported) return; // (no RAM map on other ROMs)
    const GameState& g = app.game;
    // The pair to watch, or none: loads (between, or black) are never unmapped screens.
    bool watch = false; int need = 0;
    std::pair<int, u32> pair{-1, 0};
    if (g.valid && g.settled && g.screen == GScreen::Unknown) { watch = true; need = kUnknownFrames; pair = {g.overlay, g.vtable}; }
    else if (!g.valid && !g.between && frame >= kInvalidFrom && !ScreensBlack(app.Top(), app.Bottom())) { watch = true; need = kInvalidFrames; }
    if (!watch) { cur_ = {-2, 0}; run_ = 0; return; }
    if (pair != cur_) { cur_ = pair; run_ = 0; }
    if (++run_ != need || done_.count(pair)) return;
    done_.insert(pair);

    // Once per screen across sessions: a capture of this pair already in the folder, or a full folder.
    std::error_code ec;
    char tag[48];
    snprintf(tag, sizeof tag, "-ov%d-vt%08X.st", pair.first, pair.second);
    int inFolder = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir_, ec))
    {
        const std::string f = e.path().filename().string();
        if (e.path().extension() != ".st") continue;
        inFolder++;
        if (f.size() > strlen(tag) && f.compare(f.size() - strlen(tag), strlen(tag), tag) == 0) return;
    }
    if (inFolder >= kMaxInFolder) return;
    std::filesystem::create_directories(dir_, ec);
    char stamp[32];
    const time_t t = time(nullptr);
    struct tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &lt);
    char name[96];
    snprintf(name, sizeof name, "%s-ov%d-vt%08X", stamp, pair.first, pair.second);
    const std::string base = (std::filesystem::path(dir_) / name).string();
    if (!SaveState(nds, base + ".st"))
    {
        std::filesystem::remove(base + ".st", ec);
        fprintf(stderr, "recorder: cannot save %s.st\n", base.c_str());
        return;
    }
    WriteShot(base + ".png", app.Top(), app.Bottom());
    if (!std::filesystem::exists(base + ".png", ec))
    {
        std::filesystem::remove(base + ".st", ec);
        fprintf(stderr, "recorder: cannot save %s.png\n", base.c_str());
        return;
    }
    count_++;
    fprintf(stderr, "recorded: %s.st\n", base.c_str());
}
}
