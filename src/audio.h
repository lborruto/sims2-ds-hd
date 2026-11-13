// Sound: the DS's output queued to the computer's sound device, stretched (at its own pitch)
// when the emulation falls behind, at the chosen volume.
#pragma once
#include <SDL.h>

#include <vector>

#include "NDS.h"
#include "audiostretch.h"
#include "common.h"
#include "settings.h"

namespace sims2hd
{
struct Audio
{
    SDL_AudioDeviceID dev = 0;
    std::vector<s16> buf = std::vector<s16>(4096 * 2);
    std::vector<s16> out;
    AudioStretch stretch;
    Uint64 lastTick = 0;
    double slow = 1.0; // how much longer than a DS frame the frames take, averaged

    bool Open(); // after SDL_Init(SDL_INIT_AUDIO); false: play on without sound
    // After each emulated frame: takes the frame's samples and plays them. While fast
    // forwarding, muted or at normal pitch skipping what does not fit (Settings::fastSound).
    void Frame(melonDS::NDS& nds, bool fast, const Settings& s);
    // The window not in front (and not running in the background): silent, what was queued dropped.
    bool paused = false;
    void Pause(bool on);
};
}
