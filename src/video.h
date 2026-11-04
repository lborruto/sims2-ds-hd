// The window: draws the two screens in the shown layout (and the overlay: menu, cursor,
// messages), and maps the mouse back to the touch screen through the same placement.
#pragma once
#include <SDL.h>

#include "app.h"
#include "ui.h"

namespace sims2wide
{
struct Video
{
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex[2] = {};
    SDL_Texture* overlay = nullptr;
    int overlayW = 0, overlayH = 0;
    bool smooth = false;

    bool Open(const Settings& s); // after SDL_Init(SDL_INIT_VIDEO): the remembered size, fullscreen as set
    void SetFullscreen(bool on);  // borderless, the desktop's size
    void SetSmooth(bool on);      // linear instead of nearest scaling (the textures are made again)
    // over: the overlay to put on top, if it has anything drawn. With shotPath set, the
    // picture is also written there as a PNG (input scripts' winshot), then forgotten.
    std::string shotPath;
    void Present(const App& app, const Canvas* over);
    // The window's size into the settings, when it is a window.
    void RememberSize(Settings& s) const;

    Placement Placed(const App& app) const { return Place(app.shown, app.mainIsTop, app.topAspect); }
    void OutputSize(int& ow, int& oh) const;
    // Window points -> output pixels (they differ on HiDPI screens).
    void ToOutput(int wx, int wy, float& px, float& py) const;
    // A window position on the touch screen, as a DS touch position.
    bool TouchAt(const App& app, int wx, int wy, int& tx, int& ty) const;
    // Whether a window position is over the top (world) screen's picture.
    bool OverTop(const App& app, int wx, int wy) const;
    // The bottom screen's rectangle in output pixels (where the touch cursor is drawn).
    SDL_Rect BottomRect(const App& app) const;
};
}
