// The window: draws the two screens in the shown layout, and maps the mouse back to the touch
// screen through the same placement.
#pragma once
#include <SDL.h>

#include "app.h"

namespace sims2wide
{
struct Video
{
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex[2] = {};

    bool Open(); // after SDL_Init(SDL_INIT_VIDEO)
    void Present(const App& app);
    // A window position (in window points) on the touch screen, as a DS touch position.
    bool TouchAt(const App& app, int wx, int wy, int& tx, int& ty) const;
};
}
