// The player's settings: one place for every option, whoever uses it. The comfort work loads and
// saves them (a file in SDL's per-user folder) and shows them in the settings page; the other
// parts only read them. Add a field here with its default, then use it; never a setting of your
// own elsewhere.
#pragma once
#include <string>

namespace sims2wide
{
enum class MicMode { Off, BlowKey, Real }; // Real: the computer's microphone (asks permission)

struct Settings
{
    // Playable
    MicMode mic = MicMode::BlowKey;

    // Looks
    int resolution3D = 1;      // 1 = DS (192 lines), 2, 3… times sharper
    bool widescreen = false;   // 16:9 world when the game's 3D allows it
    bool interpolate60 = false; // 60 fps in-between frames

    // Comfort
    bool fullscreen = false;
    bool smoothScaling = false;
};
}
