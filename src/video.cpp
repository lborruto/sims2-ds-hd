#include "video.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "port/cas.h"
#include "port/prompt.h"
#include "shots.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2wide
{
// The screens' textures: raw pixels drawn nearest (Sharp and Nearest) or bilinear (Smooth).
static void ScreenScaleMode(SDL_Texture* t, Scaling c)
{
    if (t) SDL_SetTextureScaleMode(t, c == Scaling::Smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
}

static void MakeScreenTextures(SDL_Renderer* ren, SDL_Texture* tex[2], Scaling c)
{
    for (int i = 0; i < 2; i++)
    {
        if (tex[i]) SDL_DestroyTexture(tex[i]);
        tex[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
        SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
        ScreenScaleMode(tex[i], c);
    }
}

bool Video::Open(const Settings& s)
{
    // The size: the remembered one, or (a first start) one from the screen's usable area; never
    // larger than that area (a size remembered on a larger screen). The place: the remembered one
    // when it still lies on a screen, else centred.
    int w = s.windowW, h = s.windowH, x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED;
    SDL_Rect usable;
    int display = 0;
    const int n = SDL_GetNumVideoDisplays();
    if (s.windowX != -1 || s.windowY != -1)
        for (int d = 0; d < n; d++)
        {
            SDL_Rect b;
            if (SDL_GetDisplayUsableBounds(d, &b) == 0 && s.windowX >= b.x && s.windowY >= b.y && s.windowX < b.x + b.w - 64 && s.windowY < b.y + b.h - 64)
            {
                display = d;
                x = s.windowX;
                y = s.windowY;
            }
        }
    if (SDL_GetDisplayUsableBounds(display, &usable) == 0)
    {
        FitWindow(usable.w, usable.h, s.windowKnown, w, h);
        // A remembered place with a size made here (or shrunk): the whole window on that screen.
        if (x != SDL_WINDOWPOS_CENTERED) FitPlace(usable.x, usable.y, usable.w, usable.h, w, h, x, y);
    }
    win = SDL_CreateWindow("The Sims 2 DS - one screen", x, y, w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "SDL window: %s\n", SDL_GetError()); return false; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "SDL renderer: %s\n", SDL_GetError()); return false; }
    scaling = s.scaling;
    MakeScreenTextures(ren, tex, scaling);
    if (s.fullscreen) SetFullscreen(true);
    return true;
}

void Video::SetFullscreen(bool on)
{
    // (The pointer follows Settings::hidePointer as in a window: the mouse is the stylus.)
    SDL_SetWindowFullscreen(win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Video::FreeSharp()
{
    for (int i = 0; i < 2; i++)
    {
        if (sharpTex[i]) SDL_DestroyTexture(sharpTex[i]);
        sharpTex[i] = nullptr;
        sharpW[i] = sharpH[i] = 0;
    }
}

void Video::SetScaling(Scaling c)
{
    if (c == scaling) return;
    scaling = c;
    if (scaling != Scaling::Sharp) FreeSharp();
    for (SDL_Texture* t : tex) ScreenScaleMode(t, scaling);
}

void Video::Close()
{
    FreeSharp();
    if (ren) SDL_DestroyRenderer(ren); // (its textures with it)
    if (win) SDL_DestroyWindow(win);
    ren = nullptr;
    win = nullptr;
    for (SDL_Texture*& t : tex) t = nullptr;
    overlayTex.clear();
    uiStill.clear();
    uiImages.clear();
    shellStill.clear();
    shellImages.clear();
}

void Video::TextInput(bool on)
{
    if (on == textInput || !win) return;
    textInput = on;
    if (!on) { SDL_StopTextInput(); return; }
    // An input method's editor by the name well (CONSOLE's canvas -> output pixels -> window points).
    int ow, oh, ww, wh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_GetWindowSize(win, &ww, &wh);
    if (ow > 0 && oh > 0)
    {
        int x, y, w, h;
        CanvasFit::For(kConsoleW, kConsoleH, ow, oh).ToOutput(kCasTitle, x, y, w, h);
        SDL_Rect r {x * ww / ow, y * wh / oh, std::max(1, w * ww / ow), std::max(1, h * wh / oh)};
        SDL_SetTextInputRect(&r);
    }
    SDL_StartTextInput();
}

void Video::RememberSize(Settings& s) const
{
    if (!win || (SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))) return;
    SDL_GetWindowSize(win, &s.windowW, &s.windowH);
    SDL_GetWindowPosition(win, &s.windowX, &s.windowY);
    s.windowKnown = true;
}

void Video::OutputSize(int& ow, int& oh) const { SDL_GetRendererOutputSize(ren, &ow, &oh); }

void Video::ToOutput(int wx, int wy, float& px, float& py) const
{
    int ow, oh, ww, wh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_GetWindowSize(win, &ww, &wh);
    px = wx * (float)ow / ww;
    py = wy * (float)oh / wh;
}

void Video::Present(const App& app, const Marks& marks, const std::vector<ui::UiItem>* shell)
{
    // --- looks: the top screen at its real size (App::TopImage: sharper / widescreen 3D). The
    // texture follows the picture's size (SetSmooth makes it 256 x 192 again; it is re-made here).
    {
        int tw, th, cw, ch;
        const u32* top = app.TopImage(tw, th);
        SDL_QueryTexture(tex[0], nullptr, nullptr, &cw, &ch);
        if (cw != tw || ch != th)
        {
            SDL_DestroyTexture(tex[0]);
            tex[0] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tw, th);
            SDL_SetTextureBlendMode(tex[0], SDL_BLENDMODE_BLEND);
            ScreenScaleMode(tex[0], scaling);
        }
        SDL_UpdateTexture(tex[0], nullptr, top, tw * 4);
    }
    // --- end looks
    SDL_UpdateTexture(tex[1], nullptr, app.Bottom(), SW * 4);
    Composition comp = Composed(app);
    // Paused under the shell: the game's picture without the world's prompt (port/prompt.h).
    if (shell) std::erase_if(comp.ui, [](const ui::UiItem& it) { return it.kind != ui::UiItem::Image && it.layer == kPromptLayer; });
    // The shell's items, and a toast over the shell when it is open (else over the game).
    std::vector<ui::UiItem> over;
    if (shell) over = *shell;
    if (!toast.empty() && SDL_GetTicks() < toastUntil)
    {
        // On a layer of its own above every other, so it rasters alone.
        constexpr float kSize = 26;
        constexpr int kLayer = 9;
        std::vector<ui::UiItem>& to = shell ? over : comp.ui;
        const int cw = shell ? 1280 : comp.cw;
        const int tw = (int)std::ceil(ui::TextWidth(toast, kSize)), w = std::min(tw + 48, cw - 32), h = 56;
        const Rect box {(cw - w) / 2, shell ? 4 : 24, w, h};
        ui::UiItem panel = ui::PanelItem(box);
        ui::UiItem text = ui::TextItem({box.x + 24, box.y, box.w - 48, box.h}, toast, kSize, ui::Align::Centre);
        text.wrap = false;
        text.middle = true;
        panel.layer = text.layer = kLayer;
        to.push_back(panel);
        to.push_back(text);
    }
    else toast.clear();
    // The stick cursor: a cross where the piece showing its point draws it (yellow while it
    // presses), on a layer of its own; hidden where no piece shows it.
    float cx, cy, cs;
    if (marks.cursor && CanvasFromTouch(comp, marks.x, marks.y, cx, cy, cs))
    {
        const float arm = SW * cs / 40 + 3, t = std::max(1.5f, comp.ch / 360.0f);
        const u32 col = marks.press ? 0xFFFFD75E : 0xFFFFFFFF;
        for (int pass = 0; pass < 2; pass++)
            for (int v = 0; v < 2; v++)
            {
                const float o = pass ? 0 : 1.5f, w = v ? t : arm, h = v ? arm : t;
                ui::UiItem bar = ui::PanelItem({(int)std::lround(cx - w - o), (int)std::lround(cy - h - o), (int)std::lround(2 * (w + o)), (int)std::lround(2 * (h + o))});
                bar.plain = true;
                bar.fill = pass ? col : 0xFF000000;
                bar.radius = 1;
                bar.layer = 8;
                comp.ui.push_back(bar);
            }
    }
    // Fast forward: a small » plate at the top right.
    if (marks.fast)
    {
        const Rect r {comp.cw - 88, 16, 72, 48};
        ui::UiItem p = ui::PanelItem(r);
        p.plain = true;
        p.fill = 0xB0202436;
        p.radius = 24;
        p.layer = 8;
        ui::UiItem t = ui::TextItem(r, "\xC2\xBB", 40, ui::Align::Centre, ui::Weight::ExtraBold);
        t.middle = true;
        t.wrap = false;
        t.layer = 8;
        comp.ui.push_back(p);
        comp.ui.push_back(t);
    }
    // Paused while the window is not in front: two bars on a plate at the top right.
    if (marks.paused)
    {
        const Rect r {comp.cw - 88, 16, 72, 48};
        ui::UiItem p = ui::PanelItem(r);
        p.plain = true;
        p.fill = 0xB0202436;
        p.radius = 24;
        p.layer = 8;
        comp.ui.push_back(p);
        for (int b = 0; b < 2; b++)
        {
            ui::UiItem bar = ui::PanelItem({r.x + 25 + b * 14, r.y + 13, 8, 22});
            bar.plain = true;
            bar.fill = 0xFFFFFFFF;
            bar.radius = 2;
            bar.layer = 8;
            comp.ui.push_back(bar);
        }
    }
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, ow, oh);
    SDL_SetRenderDrawColor(ren, 16, 18, 24, 255);
    SDL_RenderClear(ren);
    // The pieces in order: a later one lies over an earlier one, with its opacity; each overlay
    // just before the piece it names, or after them all.
    auto overlaysBefore = [&](int i) {
        for (const Overlay& o : comp.overlays)
        {
            if (!(o.before == i || (i == (int)comp.pieces.size() && (o.before < 0 || o.before >= i)))) continue;
            SDL_Rect d;
            fit.ToOutput(o.to, d.x, d.y, d.w, d.h);
            if (SDL_Texture* t = OverlayTexture(o, d.w, d.h)) SDL_RenderCopy(ren, t, nullptr, &d);
        }
    };
    // Sharp: each screen nearest into a target a whole factor larger (the factor the largest piece
    // showing it needs, at most 6), drawn smooth from there; a factor of 1: the screen itself.
    int k[2] = {1, 1};
    if (scaling == Scaling::Sharp && SDL_RenderTargetSupported(ren))
    {
        for (const Piece& q : comp.pieces)
        {
            if (q.from.w <= 0 || q.from.h <= 0) continue;
            SDL_Rect d;
            fit.ToOutput(q.to, d.x, d.y, d.w, d.h);
            const int i = q.src == Src::Top ? 0 : 1;
            const int need = (int)std::ceil(std::min((double)d.w / q.from.w, (double)d.h / q.from.h) - 0.01);
            k[i] = std::clamp(std::max(k[i], need), 1, 6);
        }
        for (int i = 0; i < 2; i++)
        {
            if (k[i] < 2) continue;
            int sw, sh;
            SDL_QueryTexture(tex[i], nullptr, nullptr, &sw, &sh);
            while (k[i] > 1 && (sw * k[i] > 8192 || sh * k[i] > 8192)) k[i]--;
            if (k[i] < 2) continue;
            if (!sharpTex[i] || sharpW[i] != sw * k[i] || sharpH[i] != sh * k[i])
            {
                if (sharpTex[i]) SDL_DestroyTexture(sharpTex[i]);
                sharpTex[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, sw * k[i], sh * k[i]);
                sharpW[i] = sw * k[i];
                sharpH[i] = sh * k[i];
                if (sharpTex[i])
                {
                    SDL_SetTextureBlendMode(sharpTex[i], SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(sharpTex[i], SDL_ScaleModeLinear);
                }
            }
            if (!sharpTex[i] || SDL_SetRenderTarget(ren, sharpTex[i]) != 0) { k[i] = 1; continue; }
            // (A plain copy: the screens are opaque, their pieces' opacity is the target's.)
            SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_NONE);
            SDL_SetTextureAlphaMod(tex[i], 255);
            SDL_RenderCopy(ren, tex[i], nullptr, nullptr);
            SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(ren, nullptr);
        }
    }
    for (int pi = 0; pi < (int)comp.pieces.size(); pi++)
    {
        overlaysBefore(pi);
        const Piece& q = comp.pieces[pi];
        const int i = q.src == Src::Top ? 0 : 1;
        SDL_Rect d, f {q.from.x * k[i], q.from.y * k[i], q.from.w * k[i], q.from.h * k[i]};
        fit.ToOutput(q.to, d.x, d.y, d.w, d.h);
        SDL_Texture* t = k[i] > 1 ? sharpTex[i] : tex[i];
        SDL_SetTextureAlphaMod(t, q.alpha);
        SDL_RenderCopy(ren, t, &f, &d);
    }
    overlaysBefore((int)comp.pieces.size());
    if (!comp.ui.empty()) DrawUi(comp.ui, fit, uiStill, uiImages);
    if (!over.empty()) DrawUi(over, CanvasFit::For(1280, 720, ow, oh), shellStill, shellImages);
    Finish(ow, oh);
}

void Video::PresentUi(const std::vector<ui::UiItem>& shell)
{
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_SetRenderDrawColor(ren, 16, 18, 24, 255);
    SDL_RenderClear(ren);
    DrawUi(shell, CanvasFit::For(1280, 720, ow, oh), shellStill, shellImages);
    Finish(ow, oh);
}

void Video::Finish(int ow, int oh)
{
    if (!shotPath.empty())
    {
        std::vector<u8> rgb((size_t)ow * oh * 3);
        if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_RGB24, rgb.data(), ow * 3) == 0) WriteRgb(shotPath, ow, oh, rgb.data());
        else fprintf(stderr, "window shot: %s\n", SDL_GetError());
        shotPath.clear();
    }
    SDL_RenderPresent(ren);
}

SDL_Texture* Video::OverlayTexture(const Overlay& o, int w, int h)
{
    if (w <= 0 || h <= 0) return nullptr;
    auto same = [&](const OverlayTex& e) {
        return e.w == w && e.h == h && e.o.to.w == o.to.w && e.o.to.h == o.to.h && e.o.argb == o.argb && e.o.radius == o.radius &&
               e.o.border == o.border && e.o.borderArgb == o.borderArgb && e.o.text == o.text && e.o.textScale == o.textScale;
    };
    for (const OverlayTex& e : overlayTex)
        if (same(e)) return e.tex;
    // A new one (a resize or a new ring makes a few; an activity's hints are some twenty): keep the cache small.
    if (overlayTex.size() >= 64)
    {
        for (OverlayTex& e : overlayTex) SDL_DestroyTexture(e.tex);
        overlayTex.clear();
    }
    std::vector<u32> px;
    RasteriseOverlay(o, w, h, px);
    SDL_Texture* t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return nullptr;
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
    overlayTex.push_back({o, w, h, t});
    return t;
}

void Video::DrawUi(const std::vector<ui::UiItem>& items, const CanvasFit& fit, std::vector<StillTex>& uiStill, std::vector<ImageTex>& uiImages)
{
    auto texture = [&](SDL_Texture*& t, int& tw, int& th, int w, int h) {
        if (t && tw == w && th == h) return t;
        if (t) SDL_DestroyTexture(t);
        t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (t)
        {
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest); // (drawn at its own size)
        }
        tw = w;
        th = h;
        return t;
    };
    std::vector<u32> px;
    size_t image = 0, still = 0;
    for (const ui::LayerAt& l : ui::LayersAt(items, fit.scale, fit.ox, fit.oy))
    {
        SDL_Texture* t = nullptr;
        bool draw = true;
        if (l.still)
        {
            if (still == uiStill.size()) uiStill.push_back({nullptr, 0, 0, 0});
            StillTex& e = uiStill[still++];
            u64 hash = ui::Hash(l.items);
            for (u64 v : {(u64)(u32)l.x, (u64)(u32)l.y, (u64)l.w, (u64)l.h, (u64)std::lround(fit.scale * 65536)}) hash = (hash ^ v) * 0x100000001B3ull;
            draw = !e.tex || hash != e.hash || e.w != l.w || e.h != l.h;
            t = texture(e.tex, e.w, e.h, l.w, l.h);
            e.hash = t ? hash : 0;
        }
        else
        {
            if (image == uiImages.size()) uiImages.push_back({nullptr, 0, 0, 0});
            ImageTex& e = uiImages[image++];
            u64 hash = ui::HashImage(l.items[0]);
            for (u64 v : {(u64)(u32)l.x, (u64)(u32)l.y, (u64)l.w, (u64)l.h, (u64)std::lround(fit.scale * 65536)}) hash = (hash ^ v) * 0x100000001B3ull;
            draw = !e.tex || hash != e.hash || e.w != l.w || e.h != l.h;
            t = texture(e.tex, e.w, e.h, l.w, l.h);
            e.hash = t ? hash : 0;
        }
        if (!t) continue;
        if (draw)
        {
            px.assign((size_t)l.w * l.h, 0);
            ui::Rasterise(l.items, fit.scale, px.data(), l.w, l.h, l.ox, l.oy);
            SDL_UpdateTexture(t, nullptr, px.data(), l.w * 4);
        }
        const SDL_Rect d {l.x, l.y, l.w, l.h};
        SDL_RenderCopy(ren, t, nullptr, &d);
    }
}

bool Video::TouchAt(const App& app, int wx, int wy, int& tx, int& ty, int* pieceIndex) const
{
    const Composition c = Composed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return TouchFromComposition(c, CanvasFit::For(c.cw, c.ch, ow, oh), px, py, tx, ty, pieceIndex);
}

bool Video::TouchClampedAt(const App& app, int pieceIndex, int wx, int wy, int& tx, int& ty) const
{
    const Composition c = Composed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return TouchClampedComposition(c, pieceIndex, CanvasFit::For(c.cw, c.ch, ow, oh), px, py, tx, ty);
}

bool Video::OverTop(const App& app, int wx, int wy) const
{
    const Composition c = Composed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return OverTopComposition(c, CanvasFit::For(c.cw, c.ch, ow, oh), px, py);
}

}
