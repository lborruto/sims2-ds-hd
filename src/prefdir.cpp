#include "prefdir.h"

#include <SDL.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

namespace sims2hd
{
namespace fs = std::filesystem;

namespace
{
std::atomic<bool> allowMove{false};

// SDL's paths are UTF-8: a path made from the u8 string, never the narrow one (on Windows that is
// the ANSI code page).
fs::path U8Path(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }
std::string U8String(const fs::path& p) { const std::u8string u = p.u8string(); return std::string(u.begin(), u.end()); }
}

void AllowPrefMove() { allowMove = true; }

PrefMove MoveOldPrefs(const fs::path& oldDir, const fs::path& newDir)
{
    PrefMove r;
    std::error_code ec;
    if (!fs::is_directory(oldDir, ec)) return r;
    if (fs::exists(newDir / "settings.ini", ec) || ec) return r;
    if (fs::exists(newDir, ec) && fs::equivalent(oldDir, newDir, ec)) return r;
    fs::create_directories(newDir, ec);
    if (!fs::is_directory(newDir, ec)) { r.failed = 1; return r; }

    // The entries first: renaming them out while iterating is unspecified.
    std::vector<fs::path> entries;
    for (fs::directory_iterator it(oldDir, ec), end; !ec && it != end; it.increment(ec)) entries.push_back(it->path());
    if (ec) { r.failed = 1; return r; }

    for (const fs::path& from : entries)
    {
        const fs::path to = newDir / from.filename();
        if (fs::exists(fs::symlink_status(to, ec))) { r.kept++; continue; }
        fs::rename(from, to, ec);
        if (!ec) { r.moved++; continue; }
        ec.clear();
        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
        if (!ec) { r.copied++; continue; }
        std::error_code ignore;
        fs::remove_all(to, ignore); // our partial copy only: the old entry stays
        r.failed++;
    }
    if (r.moved == (int)entries.size()) r.removedOld = fs::remove(oldDir, ec); // (empty only)
    return r;
}

std::string PrefDir()
{
    static std::once_flag once;
    static std::string dir;
    std::call_once(once, []() {
        char* p = SDL_GetPrefPath("", "sims2-ds-hd");
        if (!p) return;
        dir = p;
        SDL_free(p);
        if (!allowMove) return;
        // The project's old name: its folder beside this one (SDL's pref path is <base>/<app>/).
        const fs::path now = U8Path(dir).parent_path();
        const fs::path old = now.parent_path() / "sims2-ds-wide";
        const PrefMove m = MoveOldPrefs(old, now);
        if (m.moved + m.copied + m.kept + m.failed == 0) return;
        fprintf(stderr, "settings: moved from %s to %s (%d moved, %d copied, %d kept there, %d failed)\n",
            U8String(old).c_str(), U8String(now).c_str(), m.moved, m.copied, m.kept, m.failed);
    });
    return dir;
}
}
