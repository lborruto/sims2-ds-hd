#include "video.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "shots.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2wide
{
static void MakeScreenTextures(SDL_Renderer* ren, SDL_Texture* tex[2], bool smooth)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "linear" : "nearest");
    for (int i = 0; i < 2; i++)
    {
        if (tex[i]) SDL_DestroyTexture(tex[i]);
        tex[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
        SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
    }
}

bool Video::Open(const Settings& s)
{
    win = SDL_CreateWindow("The Sims 2 DS - one screen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           s.windowW, s.windowH, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "SDL window: %s\n", SDL_GetError()); return false; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "SDL renderer: %s\n", SDL_GetError()); return false; }
    smooth = s.smoothScaling;
    MakeScreenTextures(ren, tex, smooth);
    if (s.fullscreen) SetFullscreen(true);
    return true;
}

void Video::SetFullscreen(bool on)
{
    // (The pointer stays visible: the mouse is the stylus.)
    SDL_SetWindowFullscreen(win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Video::SetSmooth(bool on)
{
    if (on == smooth) return;
    smooth = on;
    MakeScreenTextures(ren, tex, smooth);
}

void Video::RememberSize(Settings& s) const
{
    if (!win || (SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED))) return;
    SDL_GetWindowSize(win, &s.windowW, &s.windowH);
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
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "linear" : "nearest");
            tex[0] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tw, th);
            SDL_SetTextureBlendMode(tex[0], SDL_BLENDMODE_BLEND);
        }
        SDL_UpdateTexture(tex[0], nullptr, top, tw * 4);
    }
    // --- end looks
    SDL_UpdateTexture(tex[1], nullptr, app.Bottom(), SW * 4);
    Composition comp = Composed(app);
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
    for (int pi = 0; pi < (int)comp.pieces.size(); pi++)
    {
        overlaysBefore(pi);
        const Piece& q = comp.pieces[pi];
        SDL_Rect d, f {q.from.x, q.from.y, q.from.w, q.from.h};
        fit.ToOutput(q.to, d.x, d.y, d.w, d.h);
        SDL_Texture* t = tex[q.src == Src::Top ? 0 : 1];
        SDL_SetTextureAlphaMod(t, q.alpha);
        SDL_RenderCopy(ren, t, &f, &d);
    }
    overlaysBefore((int)comp.pieces.size());
    DrawUi(comp.ui, fit, uiCache, app.Bottom());
    DrawUi(over, CanvasFit::For(1280, 720, ow, oh), shellCache, nullptr);
    Finish(ow, oh);
    lastShown = std::move(comp);
    haveLastShown = true;
}

void Video::PresentUi(const std::vector<ui::UiItem>& shell)
{
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_SetRenderDrawColor(ren, 16, 18, 24, 255);
    SDL_RenderClear(ren);
    DrawUi(shell, CanvasFit::For(1280, 720, ow, oh), shellCache, nullptr);
    Finish(ow, oh);
    haveLastShown = false; // (no game shown)
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

namespace
{
// A live crop of the touch screen the GPU draws as the kit would (ui::WholeFactorCrop).
struct CropCheck
{
    const u32* bottom;
    float scale;
};
bool DirectCrop(const ui::UiItem& im, const ui::LayerAt& at, void* user)
{
    const CropCheck& c = *(const CropCheck*)user;
    return c.bottom && im.src == c.bottom && ui::WholeFactorCrop(im, at, c.scale);
}
}

void Video::DrawUi(const std::vector<ui::UiItem>& items, const CanvasFit& fit, UiCache& cache, const u32* bottom)
{
    CropCheck check {bottom, fit.scale};
    cache.layers.Update(items, fit.scale, fit.ox, fit.oy, bottom ? &DirectCrop : nullptr, &check);
    // The textures follow the cache's entries (by id); the others go.
    std::vector<std::pair<u64, UiTexture>> kept;
    bool crops = false;
    for (const ui::LayerCache::Entry& e : cache.layers.entries)
    {
        if (e.direct)
        {
            crops = true;
            continue;
        }
        UiTexture t;
        for (auto& [id, old] : cache.tex)
            if (id == e.id) { t = old; old.tex = nullptr; break; }
        bool whole = false;
        if (!t.tex || t.w != e.w || t.h != e.h)
        {
            if (t.tex) SDL_DestroyTexture(t.tex);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
            t.tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, e.w, e.h);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "linear" : "nearest");
            if (t.tex) SDL_SetTextureBlendMode(t.tex, SDL_BLENDMODE_BLEND);
            t.w = e.w;
            t.h = e.h;
            whole = true;
        }
        if (t.tex)
        {
            // What was drawn again, alone (a focus move: the old and the new ring's pixels).
            if (whole) SDL_UpdateTexture(t.tex, nullptr, e.px.data(), e.w * 4);
            else
                for (const Rect& r : e.dirty)
                {
                    const SDL_Rect d {r.x, r.y, r.w, r.h};
                    SDL_UpdateTexture(t.tex, &d, &e.px[(size_t)r.y * e.w + r.x], e.w * 4);
                }
        }
        kept.push_back({e.id, t});
    }
    for (auto& [id, old] : cache.tex)
        if (old.tex) SDL_DestroyTexture(old.tex);
    cache.tex = std::move(kept);
    if (crops)
    {
        if (!cropTex)
        {
            cropTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
            if (cropTex)
            {
                SDL_SetTextureScaleMode(cropTex, SDL_ScaleModeNearest);
                SDL_SetTextureBlendMode(cropTex, SDL_BLENDMODE_BLEND);
            }
        }
        if (cropTex) SDL_UpdateTexture(cropTex, nullptr, bottom, SW * 4);
    }
    // Drawn in the layers' order: the still layers, then the pictures.
    size_t ti = 0;
    for (const ui::LayerCache::Entry& e : cache.layers.entries)
    {
        if (e.direct)
        {
            if (!cropTex) continue;
            const ui::UiItem& im = e.image;
            const SDL_Rect f {im.from.x, im.from.y, im.from.w, im.from.h};
            const SDL_Rect d {e.x, e.y, (int)(im.to.w * fit.scale), (int)(im.to.h * fit.scale)};
            SDL_SetTextureAlphaMod(cropTex, im.fill ? (Uint8)(im.fill >> 24) : 255);
            SDL_RenderCopy(ren, cropTex, &f, &d);
            continue;
        }
        const UiTexture& t = cache.tex[ti++].second;
        if (!t.tex) continue;
        const SDL_Rect d {e.x, e.y, e.w, e.h};
        SDL_RenderCopy(ren, t.tex, nullptr, &d);
    }
}

void Video::Event(const SDL_Event& ev)
{
    if (ev.type == SDL_RENDER_TARGETS_RESET || ev.type == SDL_RENDER_DEVICE_RESET) ForgetTextures();
}

void Video::ForgetTextures()
{
    for (UiCache* c : {&uiCache, &shellCache})
    {
        for (auto& [id, t] : c->tex)
            if (t.tex) SDL_DestroyTexture(t.tex);
        c->tex.clear();
        c->layers.entries.clear(); // (every layer drawn and uploaded whole again)
    }
    if (cropTex) SDL_DestroyTexture(cropTex);
    cropTex = nullptr;
    for (OverlayTex& e : overlayTex) SDL_DestroyTexture(e.tex);
    overlayTex.clear();
}

const Composition& Video::ShownComposition(const App& app, Composition& scratch) const
{
    if (haveLastShown) return lastShown;
    scratch = Composed(app);
    return scratch;
}

bool Video::TouchAt(const App& app, int wx, int wy, int& tx, int& ty, int* pieceIndex) const
{
    Composition scratch;
    const Composition& c = ShownComposition(app, scratch);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return TouchFromComposition(c, CanvasFit::For(c.cw, c.ch, ow, oh), px, py, tx, ty, pieceIndex);
}

void Video::TouchClampedAt(const App& app, int pieceIndex, int wx, int wy, int& tx, int& ty) const
{
    Composition scratch;
    const Composition& c = ShownComposition(app, scratch);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    TouchClampedComposition(c, pieceIndex, CanvasFit::For(c.cw, c.ch, ow, oh), px, py, tx, ty);
}

bool Video::OverTop(const App& app, int wx, int wy) const
{
    Composition scratch;
    const Composition& c = ShownComposition(app, scratch);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return OverTopComposition(c, CanvasFit::For(c.cw, c.ch, ow, oh), px, py);
}

}
