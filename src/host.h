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

// melonDS calls this whenever the game writes its save memory; written straight to disk.
inline void SaveDirty(const uint8_t* data, uint32_t len)
{
    if (SavePath.empty()) return;
    if (FILE* f = fopen(SavePath.c_str(), "wb"))
    {
        fwrite(data, 1, len, f);
        fclose(f);
    }
}
}
