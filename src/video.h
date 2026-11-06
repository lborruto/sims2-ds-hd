// The window: draws the two screens in the shown layout (and the overlay: menu, cursor,
// messages), and maps the mouse back to the touch screen through the same placement.
#pragma once
#include <SDL.h>

#include "app.h"
#include "compose.h"
#include "console.h"
#include "ui.h"

namespace sims2wide
{
struct Video
{
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex[2] = {};
    SDL_Texture* overlay = nullptr;
    // The composition's overlays (frames, the backdrop, the ring) as textures at output size,
    // made again only when one changes.
    struct OverlayTex { Overlay o; int w, h; SDL_Texture* tex; };
    std::vector<OverlayTex> overlayTex;
    SDL_Texture* OverlayTexture(const Overlay& o, int w, int h);
    int overlayW = 0, overlayH = 0;
    // The composition's UI (ui/kit.h) at output size: the still items' layer over their bounds,
    // made again only when its hash (items, size, place) changes; the pictures (live crops) each
    // over their own bounds in their own texture, made again when that picture's pixels change.
    // The UI's still layers (ui::LayersAt), each kept until its own items change.
    struct StillTex { SDL_Texture* tex; int w, h; u64 hash; };
    std::vector<StillTex> uiStill;
    struct ImageTex { SDL_Texture* tex; int w, h; u64 hash; };
    std::vector<ImageTex> uiImages;
    void DrawUi(const Composition& c, const CanvasFit& fit);
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

    // What the window shows now: the shown layout's pieces, for the top picture's current size
    // (CONSOLE: for the game's screen, the menu and the focus). No SDL needed: --compose too.
    Composition Composed(const App& app) const
    {
        int tw, th;
        app.TopImage(tw, th);
        Composition c = app.shown == Layout::Console ? BuildConsole(app.ConsoleScreen(), app.consoleMenu, app.consoleFocus, tw, th, app.Hud(), &app.hudStrings,
                                                                    app.portSupported ? &app.game : nullptr, app.rom.get(), app.Bottom(), app.icons.get())
                                                     : Compose(app.shown, app.mainIsTop, app.topAspect, tw, th);
        if (app.uiDemo) c.ui = ui::DemoItems(app.Bottom());
        return c;
    }
    void OutputSize(int& ow, int& oh) const;
    // Window points -> output pixels (they differ on HiDPI screens).
    void ToOutput(int wx, int wy, float& px, float& py) const;
    // A window position on the touch screen, as a DS touch position.
    // pieceIndex (when given) receives the piece of the composition it is on.
    bool TouchAt(const App& app, int wx, int wy, int& tx, int& ty, int* pieceIndex = nullptr) const;
    // A window position as a DS touch position held to the edges of the piece (from TouchAt) a
    // drag started on.
    void TouchClampedAt(const App& app, int pieceIndex, int wx, int wy, int& tx, int& ty) const;
    // Whether a window position is over the top (world) screen's picture.
    bool OverTop(const App& app, int wx, int wy) const;
    // Where a DS touch position is drawn, in output pixels, and the drawn size of the whole touch
    // screen's width at that scale (the stick cursor's); false when no piece shows that point.
    bool TouchToOutput(const App& app, float tx, float ty, int& x, int& y, int& screenW) const;
};
}
