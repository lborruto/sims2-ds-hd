#include "recorder.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

#include "prefdir.h"
#include "shots.h"
#include "state.h"

namespace sims2hd
{
namespace
{
constexpr int kUnknownFrames = 30;   // a settled unmapped screen, this long
constexpr int kInvalidFrames = 300;  // RAM not reading (not a load, not black), this long
constexpr long kInvalidFrom = 600;   // ... after the boot
constexpr int kMaxPerRun = 20;
constexpr int kMaxInFolder = 200;

// The folder is UTF-8 (SDL's pref path): a path made from the u8 string, never the narrow one
// (on Windows that is the ANSI code page, which breaks a user folder with non-ASCII letters).
std::filesystem::path U8Path(const std::string& s) { return std::filesystem::path(std::u8string(s.begin(), s.end())); }
std::string U8String(const std::filesystem::path& p) { const std::u8string u = p.u8string(); return std::string(u.begin(), u.end()); }
}

Recorder::Recorder(std::string dir) : dir_(std::move(dir))
{
    if (dir_.empty())
    {
        if (const std::string p = PrefDir(); !p.empty()) dir_ = p + "captures";
    }
}

bool Recorder::Frame(const App& app, long frame, melonDS::NDS& nds)
{
    if (dir_.empty() || count_ >= kMaxPerRun || !app.portSupported) return false; // (no RAM map on other ROMs)
    const GameState& g = app.game;
    // The pair to watch, or none: loads (between, or black) are never unmapped screens.
    bool watch = false; int need = 0;
    std::pair<int, u32> pair{-1, 0};
    if (g.valid && g.settled && g.screen == GScreen::Unknown) { watch = true; need = kUnknownFrames; pair = {g.overlay, g.vtable}; }
    else if (!g.valid && !g.between && frame >= kInvalidFrom && !ScreensBlack(app.Top(), app.Bottom())) { watch = true; need = kInvalidFrames; }
    if (!watch) { cur_ = {-2, 0}; run_ = 0; return false; }
    if (pair != cur_) { cur_ = pair; run_ = 0; }
    if (++run_ != need || done_.count(pair)) return false;
    done_.insert(pair);

    // Once per screen across sessions: a capture of this pair already in the folder, or a full folder.
    std::error_code ec;
    char tag[48];
    snprintf(tag, sizeof tag, "-ov%d-vt%08X.st", pair.first, pair.second);
    int inFolder = 0;
    const std::filesystem::path dir = U8Path(dir_);
    // increment(ec), not ++: an entry that cannot be read ends the count instead of throwing.
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        const std::string f = U8String(it->path().filename());
        if (it->path().extension() != ".st") continue;
        inFolder++;
        if (f.size() > strlen(tag) && f.compare(f.size() - strlen(tag), strlen(tag), tag) == 0) return false;
    }
    if (inFolder >= kMaxInFolder) return false;
    std::filesystem::create_directories(dir, ec);
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
    const std::string base = U8String(dir / name);
    if (!SaveState(nds, base + ".st"))
    {
        std::filesystem::remove(U8Path(base + ".st"), ec);
        fprintf(stderr, "recorder: cannot save %s.st\n", base.c_str());
        return false;
    }
    WriteShot(base + ".png", app.Top(), app.Bottom());
    if (!std::filesystem::exists(U8Path(base + ".png"), ec))
    {
        std::filesystem::remove(U8Path(base + ".st"), ec);
        fprintf(stderr, "recorder: cannot save %s.png\n", base.c_str());
        return false;
    }
    count_++;
    fprintf(stderr, "recorded: %s.st\n", base.c_str());
    return true;
}
}
