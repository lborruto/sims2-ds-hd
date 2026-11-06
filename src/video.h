// The window: draws the two screens in the shown layout (and over them the stick cursor, the fast
// forward mark, a toast and the HD shell, all on the UI kit), and maps the mouse back to the touch
// screen through the same placement.
#pragma once
#include <SDL.h>

#include "app.h"
#include "compose.h"
#include "console.h"
#include "ui/kit.h"

namespace sims2wide
{
struct Video
{
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex[2] = {};
    // The composition's overlays (frames, the backdrop, the ring) as textures at output size,
    // made again only when one changes.
    struct OverlayTex { Overlay o; int w, h; SDL_Texture* tex; };
    std::vector<OverlayTex> overlayTex;
    SDL_Texture* OverlayTexture(const Overlay& o, int w, int h);
    // The composition's UI (ui/kit.h) at output size: each layer of ui::LayersAt kept by a
    // ui::LayerCache (a still layer drawn again only where its items changed, a picture when its
    // pixels change) in a texture of its own, uploaded where it was drawn again. The live crops of
    // the touch screen at a whole factor (the HD menus' portraits, the phone) are drawn by the GPU
    // from `cropTex` (nearest: the same pixels the kit's scaler gives there), not rastered.
    struct UiTexture { SDL_Texture* tex = nullptr; int w = 0, h = 0; };
    struct UiCache
    {
        ui::LayerCache layers;
        std::vector<std::pair<u64, UiTexture>> tex; // by LayerCache::Entry::id
    };
    UiCache uiCache, shellCache; // the game's UI; the shell's, kept apart
    SDL_Texture* cropTex = nullptr;
    void DrawUi(const std::vector<ui::UiItem>& items, const CanvasFit& fit, UiCache& cache, const u32* bottom);
    // The composition last shown (Present): what the mouse reads (TouchAt, OverTop) until the
    // next one, rather than composing it again per event.
    Composition lastShown;
    bool haveLastShown = false;
    const Composition& ShownComposition(const App& app, Composition& scratch) const;
    bool smooth = false;
    // A short HD message over the picture (a controller connected: how to pause), at the top of
    // the canvas in the kit's panel and Nunito, until toastUntil (SDL ticks).
    std::string toast;
    Uint32 toastUntil = 0;
    void Toast(const std::string& text, Uint32 ms) { toast = text; toastUntil = SDL_GetTicks() + ms; }

    bool Open(const Settings& s); // after SDL_Init(SDL_INIT_VIDEO): the remembered size, fullscreen as set
    void SetFullscreen(bool on);  // borderless, the desktop's size
    void SetSmooth(bool on);      // linear instead of nearest scaling (the textures are made again)
    // What lies over the game besides its composition: the stick cursor (a DS touch point, drawn
    // where the piece showing it draws it), the fast forward mark.
    struct Marks
    {
        bool cursor = false, press = false, fast = false;
        float x = 0, y = 0;
    };
    // shell: the HD shell's items on its own 1280 x 720 canvas (null: none). With shotPath set,
    // the picture is also written there as a PNG (input scripts' winshot), then forgotten.
    std::string shotPath;
    void Present(const App& app, const Marks& marks, const std::vector<ui::UiItem>* shell);
    // The shell alone (the launch panel: no DS yet).
    void PresentUi(const std::vector<ui::UiItem>& shell);
    void Finish(int ow, int oh); // (the window shot, then the picture shown)
    // The window's size into the settings, when it is a window.
    void RememberSize(Settings& s) const;

    // What the window shows now: the shown layout's pieces, for the top picture's current size
    // (CONSOLE: for the game's screen, the menu and the focus). No SDL needed: --compose too.
    Composition Composed(const App& app) const
    {
        int tw, th;
        app.TopImage(tw, th);
        Composition c = app.shown == Layout::Console ? BuildConsole(app.ConsoleScreen(), app.consoleMenu, app.consoleFocus, tw, th, app.Hud(), &app.hudStrings,
                                                                    app.portSupported ? &app.game : nullptr, app.rom.get(), app.Bottom(), app.icons.get(), app.consoleArmed,
                                                                    &app.consolePointer, app.consolePayout, app.consolePictureClosed, app.consoleVeiled,
                                                                    app.consoleWorldOnly)
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
};
}
