// Keyboard and mouse: the DS buttons, the stylus, and the frontend's own keys (layouts, save
// states, screenshot, fast forward, quit).
#pragma once
#include <SDL.h>

#include "app.h"
#include "video.h"

namespace sims2wide
{
struct Input
{
    u32 heldKeys = 0;   // DS keys held on the keyboard (bits of DSKey)
    bool mouseDown = false;
    int mouseX = 0, mouseY = 0;

    void HandleEvent(const SDL_Event& ev, App& app);
    // Adds this frame's keyboard keys and mouse touch.
    void Frame(const App& app, const Video& video, u32& keys, bool& touching, int& tx, int& ty) const;
};
}
