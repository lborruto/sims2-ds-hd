// Sound: the DS's output queued to the computer's sound device.
#pragma once
#include <SDL.h>

#include <vector>

#include "NDS.h"
#include "common.h"

namespace sims2wide
{
struct Audio
{
    SDL_AudioDeviceID dev = 0;
    std::vector<s16> buf = std::vector<s16>(4096 * 2);

    bool Open(); // after SDL_Init(SDL_INIT_AUDIO); false: play on without sound
    // After each emulated frame: takes the frame's samples and plays them (dropped while fast).
    void Frame(melonDS::NDS& nds, bool fast);
};
}
