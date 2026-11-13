// The window: draws the two screens in the shown layout (and over them the stick cursor, the fast
// forward mark, a toast and the HD shell, all on the UI kit), and maps the mouse back to the touch
// screen through the same placement.
#pragma once
#include <SDL.h>

#include "app.h"
#include "compose.h"
#include "console.h"
#include "ui/kit.h"

namespace sims2hd
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
    // The renderer lost its textures' contents (SDL_RENDER_TARGETS_RESET / SDL_RENDER_DEVICE_RESET:
    // a GPU switch, a display change on Windows): the UI's layers, the crops' texture and the
    // overlays are forgotten, so the next picture makes and uploads them whole. main.cpp passes
    // every event.
    void Event(const SDL_Event& ev);
    void ForgetTextures();
    // The composition last shown (Present): what the mouse reads (TouchAt, TouchClampedAt,
    // OverTop) until the next one, rather than composing it again per event. A held picture
    // (ConsoleHold) is what was shown then: no piece touchable, so the mouse takes nothing.
    Composition lastShown;
    bool haveLastShown = false;
    const Composition& ShownComposition(const App& app, Composition& scratch) const;
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
    void SetScaling(Scaling c);   // (Sharp's targets freed when another is chosen)
    void FreeSharp();
    // Everything let go (the window closed), before SDL_Quit.
    void Close();
    // The keyboard's text (SDL_TEXTINPUT) on only while Create-a-Sim's Name page takes it, an
    // input method's editor placed by the name well; off everywhere else (the menus, the game).
    bool textInput = false;
    void TextInput(bool on);
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
        Composition c = app.shown == Layout::Console ? app.consoleHold.Shown(app, tw, th) // (consolehold.h)
                                                     : Compose(app.shown, app.mainIsTop, app.topAspect, tw, th);
        if (app.uiDemo) c.ui = ui::DemoItems(app.Bottom());
        return c;
    }
    void OutputSize(int& ow, int& oh) const;
    // What Auto's UI size reads (settings.h's AutoUiScale): the millimetres one output pixel spans on
    // the window's display and that display's diagonal, from the system's physical size (macOS:
    // CGDisplayScreenSize; elsewhere SDL_GetDisplayDPI: Windows' scaling DPI, which Windows picks
    // from the panel's size, X11's physical one); false when the system does not say.
    bool DisplayMetrics(float& mmPerOutputPixel, float& diagonalMm) const;
    // The UI's size Auto gives for the window now (1 when the system does not say); asked of the
    // system again only when the window's display, its size or the display's mode changes.
    float AutoUiScale() const;
    // (Auto's last inputs: the display, the window's size in units and pixels, the display's
    // bounds' size)
    static constexpr int kAutoKey = 7;
    mutable int autoKey[kAutoKey] = {-1, 0, 0, 0, 0, 0, 0};
    mutable float autoScale = 1;
    // Window points -> output pixels (they differ on HiDPI screens).
    void ToOutput(int wx, int wy, float& px, float& py) const;
    // A window position on the touch screen, as a DS touch position.
    // pieceIndex (when given) receives the piece of the composition it is on.
    bool TouchAt(const App& app, int wx, int wy, int& tx, int& ty, int* pieceIndex = nullptr) const;
    // A window position as a DS touch position held to the edges of the piece (from TouchAt) a
    // drag started on; false when no piece takes a touch (compose.h: TouchClampedComposition).
    bool TouchClampedAt(const App& app, int pieceIndex, int wx, int wy, int& tx, int& ty) const;
    // Whether a window position is over the top (world) screen's picture.
    bool OverTop(const App& app, int wx, int wy) const;
};
}
