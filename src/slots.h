// The HD shell's files (shell.h): the save-state slots, their thumbnails and dates, the state
// kept before a load (its undo), the suspend state written on quitting (the launch panel's
// Continue) and the recent ROMs. All in one folder of the player's (the SDL pref folder beside
// settings.ini; --data-dir for a check; none headless without it), never beside the ROM and never
// in git: a state holds the game's memory.
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace sims2wide
{
constexpr int kStateSlots = 4;
constexpr int kThumbW = 320; // a thumbnail's width (its height from the picture's shape)

// A slot's picture (0xAARRGGBB) and when it was saved (seconds since 1970, 0 unknown).
struct Thumb
{
    int w = 0, h = 0;
    std::vector<u32> px;
    long long time = 0;
};

// The composed picture (RGB, w x h) made small: kThumbW wide, an area average.
Thumb MakeThumb(const std::vector<u8>& rgb, int w, int h, long long time);
// A thumbnail file ("S2WT", version, w, h, time, then the pixels); false when it does not read.
bool WriteThumb(const std::string& path, const Thumb& t);
bool ReadThumb(const std::string& path, Thumb& t);
// "2026-11-06 14:32" in the computer's local time ("" for 0).
std::string FormatTime(long long t);

// Who made a state: the ROM's path and its header's hash (a state made with another ROM file
// asks before it loads into this one's save), and the hash of the game's save then (the suspend
// state: a save written since makes it stale).
struct StateMeta
{
    std::string rom;
    u64 header = 0, save = 0;
};
bool WriteMeta(const std::string& path, const StateMeta& m);
bool ReadMeta(const std::string& path, StateMeta& m);
// FNV-1a of bytes / of a whole file (0: none).
u64 HashBytes(const u8* p, size_t n);
u64 HashFile(const std::string& path);

struct DataDir
{
    std::string root; // ends in a separator; "" none (headless without --data-dir)

    bool Ok() const { return !root.empty(); }
    // The folder of one game's states (by its code, ASJP: letters and digits only), made the first
    // time it is asked for; "" for no code (an unreadable file makes no folder).
    std::string GameDir(const std::string& code) const;
    std::string Slot(const std::string& code, int i) const;       // slot i (0..kStateSlots-1): .st
    std::string SlotThumb(const std::string& code, int i) const;  // its .thumb
    std::string SlotMeta(const std::string& code, int i) const;   // its .meta (StateMeta)
    std::string Undo(const std::string& code) const;              // the state before the last load
    std::string Suspend(const std::string& code) const;           // written on quitting
    std::string SuspendThumb(const std::string& code) const;
    std::string SuspendMeta(const std::string& code) const;       // its StateMeta: the ROM it belongs to, the save's hash
    // The folders of the game's save when the ROM's folder cannot be written (saves/) and of the
    // window's screenshots (screenshots/), made the first time they are asked for ("" none).
    std::string Saves() const;
    std::string Screenshots() const;
    std::string Quick(const std::string& code) const;             // F5 / F7's state
    std::string QuickMeta(const std::string& code) const;         // its StateMeta (F7 refuses another file's)

    // The recent ROMs, newest first (only files that still exist), at most kRecents.
    static constexpr int kRecents = 5;
    std::vector<std::string> Recents() const;
    void PushRecent(const std::string& romPath) const;

private:
    mutable std::vector<std::string> made_; // the game folders made so far
};

bool FileExists(const std::string& path);
// Whether the program can write this file: an existing one opened for update, else one made and
// removed again (nothing is left behind, nothing is changed).
bool CanWrite(const std::string& path);
// The game's save when its own place cannot be written: saves/ in the data folder, the same file
// name ("" without a data folder). SaveFallback makes saves/ (the save is going there);
// SaveFallbackAt only names it (a check: nothing is made).
std::string SaveFallback(const DataDir& d, const std::string& savePath);
std::string SaveFallbackAt(const DataDir& d, const std::string& savePath);
// Of the save beside the ROM and its fallback copy, the one that leads: the fallback when it
// exists and holds something else than the save beside (by content) and is not older than it (by
// date; the save beside missing: the fallback); else the save beside. (The launch panel's
// Continue check and ResolveSave.)
std::string NewerSave(const std::string& beside, const std::string& fallback);
// A file copied and read back (through a temporary beside it, renamed over the target, or in
// place when no temporary can be made there); the copy left owner-writable. False: the target
// is as it was or the copy did not read back the same.
bool CopyVerified(const std::string& from, const std::string& to);
// Where the game's save goes at a start, its folders made and files moved as needed:
// - a fallback copy in saves/ and the save beside writable again: the newer one beside (the
//   fallback copied back, verified, when it leads), the fallback renamed to .moved (movedBack);
// - a fallback copy and the save beside still read-only: the fallback, the save beside copied
//   over it first when that one leads (the old copy kept as .old);
// - no fallback copy: the save beside when it can be written, else the fallback, the save beside
//   copied there first.
// elsewhere: the save's path when it is not beside the ROM (the toast, the pause menu's banner).
struct SaveStart
{
    std::string path, elsewhere;
    bool movedBack = false;
    bool copyFailed = false; // a copy that should have been made was not (said in the terminal)
};
SaveStart ResolveSave(const DataDir& d, const std::string& beside);
// A window screenshot's file name: "sims2-YYYYMMDD-HHMMSS.png" in the local time of t, "-2", "-3"…
// before ".png" for n > 1 (two in one second).
std::string ShotName(long long t, int n = 1);
// A path's file name (after the last / or \).
std::string BaseName(const std::string& path);

// What a ROM's header says about it (the first 0x10 bytes: the title, the game code).
enum class RomKind { Unreadable, NotSims2, Sims2Other, Sims2EU };
RomKind ClassifyRom(const std::string& path, std::string* code = nullptr);
RomKind ClassifyHeader(const u8* header16, size_t size, std::string* code = nullptr);
}
