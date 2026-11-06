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

struct DataDir
{
    std::string root; // ends in a separator; "" none (headless without --data-dir)

    bool Ok() const { return !root.empty(); }
    // The folder of one game's states (by its code, ASJP: letters and digits only), made on demand.
    std::string GameDir(const std::string& code) const;
    std::string Slot(const std::string& code, int i) const;       // slot i (0..kStateSlots-1): .st
    std::string SlotThumb(const std::string& code, int i) const;  // its .thumb
    std::string Undo(const std::string& code) const;              // the state before the last load
    std::string Suspend(const std::string& code) const;           // written on quitting
    std::string SuspendThumb(const std::string& code) const;
    std::string SuspendRom(const std::string& code) const;        // the ROM the suspend state belongs to

    // The recent ROMs, newest first (only files that still exist), at most kRecents.
    static constexpr int kRecents = 5;
    std::vector<std::string> Recents() const;
    void PushRecent(const std::string& romPath) const;
};

bool FileExists(const std::string& path);
// A path's file name (after the last / or \).
std::string BaseName(const std::string& path);

// What a ROM's header says about it (the first 0x10 bytes: the title, the game code).
enum class RomKind { Unreadable, NotSims2, Sims2Other, Sims2EU };
RomKind ClassifyRom(const std::string& path, std::string* code = nullptr);
RomKind ClassifyHeader(const u8* header16, size_t size, std::string* code = nullptr);
}
