#include "picker.h"

#include <SDL.h>

#include <cstdio>
#include <fstream>

#include "prefdir.h"
#include "tinyfiledialogs.h"

namespace sims2hd
{
// A message for whoever started the program: on the terminal always, and in a dialog box too
// when there may be no terminal (started by a double click). SDL's message box, not
// tinyfiledialogs': on a Mac and on Linux tinyfd builds a shell command from its strings, and the
// message carries a file path.
void Tell(bool dialog, const std::string& msg)
{
    fprintf(stderr, "%s\n", msg.c_str());
    if (dialog) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sims 2 DS HD", msg.c_str(), nullptr);
}

// The ROM chosen last time, kept in the per-user settings folder (SDL's pref path).
static std::string LastRomFile()
{
    const std::string dir = PrefDir();
    return dir.empty() ? "" : dir + "last-rom.txt";
}

// Whether a path may go to the file picker. tinyfiledialogs runs osascript, zenity or kdialog
// through a shell on a Mac and on Linux, with the default path inside the command, and copies it
// into fixed-size buffers: only short paths without control characters or anything a shell gives
// a meaning to inside quotes are passed (on Windows it calls the common dialog directly, and the
// backslash is the path separator).
static bool SafeForPicker(const std::string& path)
{
    if (path.empty() || path.size() > 400) return false;
    for (unsigned char c : path)
    {
        if (c < 0x20 || c == 0x7F) return false;
        if (c == '$' || c == '`' || c == '"' || c == '\'' || c == '!') return false;
#ifndef _WIN32
        if (c == '\\') return false;
#endif
    }
    return true;
}

std::string PickRom()
{
    std::string last, file = LastRomFile();
    if (std::ifstream in(file); in) std::getline(in, last);
    // Only the remembered ROM's folder, and only if it still exists and passes the check.
    std::string startDir;
    if (size_t slash = last.find_last_of("/\\"); slash != std::string::npos)
    {
        std::string dir = last.substr(0, slash + 1);
        if (SafeForPicker(dir) && std::ifstream(last).good()) startDir = dir;
    }
    const char* patterns[] = {"*.nds"};
    const char* chosen = tinyfd_openFileDialog("Choose your copy of The Sims 2 (DS)", startDir.empty() ? nullptr : startDir.c_str(),
                                               1, patterns, "Nintendo DS ROM (.nds)", 0);
    if (!chosen) return "";
    std::string rom = chosen;
    // Remembered only if it could be offered again safely next time.
    if (!file.empty() && SafeForPicker(rom)) { std::ofstream out(file); out << rom << "\n"; }
    return rom;
}
}
