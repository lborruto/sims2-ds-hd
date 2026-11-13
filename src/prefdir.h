// The per-user folder (SDL's pref path for "sims2-ds-hd": %APPDATA%\sims2-ds-hd on Windows,
// ~/Library/Application Support/sims2-ds-hd on a Mac, ~/.local/share/sims2-ds-hd on Linux): the
// settings, the controller mappings, the last ROM, the save states' files and the captures. The
// one place that names it; everything else asks PrefDir().
#pragma once
#include <filesystem>
#include <string>

namespace sims2hd
{
// The folder with a trailing separator, created by SDL ("" when SDL has none). On its first call
// in a run that allowed it (AllowPrefMove), the folder of the project's old name (sims2-ds-wide,
// beside it) is moved over first (MoveOldPrefs).
std::string PrefDir();
// Lets PrefDir's first call move the old folder over: main.cpp, for a window run alone (headless
// and tool runs, --measure-menu and the like, which the checks make, never move the player's files).
void AllowPrefMove();

struct PrefMove
{
    int moved = 0;  // entries renamed into the new folder
    int copied = 0; // entries copied (the rename failed; the old one is kept)
    int kept = 0;   // entries left in the old folder: the new folder has one of that name
    int failed = 0; // entries neither moved nor copied (left in the old folder)
    bool removedOld = false; // the old folder, empty after the move, removed
};
// The old folder's entries (files and folders) moved into newDir, when oldDir is a folder and
// newDir holds no settings.ini yet: each one renamed, else copied (a partial copy taken back, the
// old entry always kept); an entry newDir already has stays where it is. oldDir is removed only
// when it ends up empty. Nothing is ever overwritten; nothing old is deleted on a failure.
PrefMove MoveOldPrefs(const std::filesystem::path& oldDir, const std::filesystem::path& newDir);
}
