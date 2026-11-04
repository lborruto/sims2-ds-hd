// What platform.cpp needs from the frontend.
#pragma once
#include <cstdint>
#include <string>

namespace Host
{
inline bool StopRequested = false;
inline int LogLevel = 2; // melonDS LogLevel: 0 debug, 1 info, 2 warn, 3 error
inline std::string SavePath;

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
