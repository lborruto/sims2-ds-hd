// The looks: a sharper 3D world (drawn at 2, 3... times the DS's resolution), a widescreen 16:9
// world, and 60 fps in-between frames. None of it changes the game (the widescreen world's box
// test aside, see src/melonds_ext/GPU3D_Wide.h); all of it is off by default.
//
// After each frame, Looks::Frame builds the top screen's picture at its real size into App
// (App::TopImage): the sharp or wide 3D picture where the DS showed its 3D layer, the DS's own
// 2D (status bar, sprites, menus) scaled up on top of it.
#pragma once
#include "app.h"

namespace sims2hd
{
struct Looks
{
    // Applies app.settings (resolution3D, widescreen, interpolate60) to the DS before a frame.
    void Apply(App& app);
    // Builds app's top picture from the frame just run (nothing when the looks are off).
    void Frame(App& app);
    // Before the DS goes away: waits for the 3D picture drawn in the background.
    static void Shutdown();

    int wideFrames = 0;    // frames the wide renderer has been on (its first picture comes late)
    bool wasOn = false;
};
}
