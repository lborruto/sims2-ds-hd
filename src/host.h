// What platform.cpp needs from the frontend.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "settings.h"

namespace Host
{
inline bool StopRequested = false;
inline int LogLevel = 2; // melonDS LogLevel: 0 debug, 1 info, 2 warn, 3 error
inline std::string SavePath;

// The microphone (mic.cpp reads these; main.cpp sets them every frame from the settings, the
// blow key and the script's "blow").
inline sims2wide::MicMode MicMode = sims2wide::MicMode::BlowKey;
inline bool MicBlow = false;
inline bool MicForce = false; // the CONSOLE vacuum's Y: a blow in any mic mode

// melonDS calls this whenever the game writes its save memory; written straight to disk.
// Whether the last write failed (the window says so in a toast while it does). A write that fails
// where the save is (a folder that became read-only, a drive pulled out) goes to SaveFallback (the
// settings folder's saves/, main.cpp) from then on: SaveMoved says so (the pause menu's banner).
inline bool SaveFailed = false;
inline std::string SaveFallback;
inline bool SaveMoved = false;
inline bool WriteSave(const std::string& path, const uint8_t* data, uint32_t len)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == len;
    ok &= fclose(f) == 0;
    return ok;
}
inline void SaveDirty(const uint8_t* data, uint32_t len)
{
    if (SavePath.empty()) return;
    SaveFailed = !WriteSave(SavePath, data, len);
    if (SaveFailed && !SaveFallback.empty() && SaveFallback != SavePath && WriteSave(SaveFallback, data, len))
    {
        fprintf(stderr, "save: %s could not be written: saving to %s\n", SavePath.c_str(), SaveFallback.c_str());
        SavePath = SaveFallback;
        SaveFailed = false;
        SaveMoved = true;
    }
}
}
