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
    // The composition's UI (ui/kit.h) at output size: the still items' layer over their bounds,
    // made again only when its hash (items, size, place) changes; the pictures (live crops) each
    // over their own bounds in their own texture, made again when that picture's pixels change.
    // The UI's still layers (ui::LayersAt), each kept until its own items change.
    struct StillTex { SDL_Texture* tex; int w, h; u64 hash; };
    std::vector<StillTex> uiStill;
    struct ImageTex { SDL_Texture* tex; int w, h; u64 hash; };
    std::vector<ImageTex> uiImages;
    // The shell's (shell.h) layers, kept apart from the game's.
    std::vector<StillTex> shellStill;
    std::vector<ImageTex> shellImages;
    void DrawUi(const std::vector<ui::UiItem>& items, const CanvasFit& fit, std::vector<StillTex>& still, std::vector<ImageTex>& images);
    // The DS pictures' scaling (Settings::scaling). Sharp: each screen drawn nearest into a target
    // a whole factor larger (sharpTex: the factor the largest piece showing it needs), then that
    // drawn smooth to the piece's size.
    Scaling scaling = Scaling::Sharp;
    SDL_Texture* sharpTex[2] = {};
    int sharpW[2] = {}, sharpH[2] = {};
    // A short HD message over the picture (a controller connected: how to pause), at the top of
    // the canvas in the kit's panel and Nunito, until toastUntil (SDL ticks).
    std::string toast;
    Uint32 toastUntil = 0;
    void Toast(const std::string& text, Uint32 ms) { toast = text; toastUntil = SDL_GetTicks() + ms; }

    // After SDL_Init(SDL_INIT_VIDEO): the remembered size and place (or a size from the screen's
    // usable area on a first start; never larger than it), fullscreen as set.
    bool Open(const Settings& s);
    void SetFullscreen(bool on);  // borderless, the desktop's size
    void SetScaling(Scaling c);
    // The window's size and place into the settings, when it is a window.
    // What lies over the game besides its composition: the stick cursor (a DS touch point, drawn
    // where the piece showing it draws it), the fast forward mark.
    struct Marks
    {
        bool cursor = false, press = false, fast = false, paused = false; // paused: the window is not in front
        float x = 0, y = 0;
    };
    // shell: the HD shell's items on its own 1280 x 720 canvas (null: none). With shotPath set,
    // the picture is also written there as a PNG (input scripts' winshot), then forgotten.
    std::string shotPath;
    void Present(const App& app, const Marks& marks, const std::vector<ui::UiItem>* shell);
    // The shell alone (the launch panel: no DS yet).
    void PresentUi(const std::vector<ui::UiItem>& shell);
    void Finish(int ow, int oh); // (the window shot, then the picture shown)
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
