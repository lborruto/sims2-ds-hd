#include "video.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "shots.h"

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

void Video::Present(const App& app, const Canvas* over)
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
    const Composition comp = Composed(app);
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
    if (over && over->Dirty())
    {
        if (!overlay || overlayW != over->w || overlayH != over->h)
        {
            if (overlay) SDL_DestroyTexture(overlay);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
            overlay = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, over->w, over->h);
            SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
            overlayW = over->w;
            overlayH = over->h;
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "linear" : "nearest");
        }
        // Only the part drawn into is uploaded and shown (the canvas is as large as the output).
        const SDL_Rect r {over->x0d, over->y0d, over->x1d - over->x0d, over->y1d - over->y0d};
        SDL_UpdateTexture(overlay, &r, over->px.data() + (size_t)r.y * over->w + r.x, over->w * 4);
        SDL_RenderCopy(ren, overlay, &r, &r);
    }
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
               e.o.border == o.border && e.o.borderArgb == o.borderArgb;
    };
    for (const OverlayTex& e : overlayTex)
        if (same(e)) return e.tex;
    // A new one (a resize or a new ring makes a few): keep the cache small.
    if (overlayTex.size() >= 24)
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

bool Video::TouchAt(const App& app, int wx, int wy, int& tx, int& ty, int* pieceIndex) const
{
    const Composition c = Composed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return TouchFromComposition(c, CanvasFit::For(c.cw, c.ch, ow, oh), px, py, tx, ty, pieceIndex);
}

void Video::TouchClampedAt(const App& app, int pieceIndex, int wx, int wy, int& tx, int& ty) const
{
    const Composition c = Composed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    TouchClampedComposition(c, pieceIndex, CanvasFit::For(c.cw, c.ch, ow, oh), px, py, tx, ty);
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

SDL_Rect Video::BottomRect(const App& app) const
{
    const Composition c = Composed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_Rect r;
    CanvasFit::For(c.cw, c.ch, ow, oh).ToOutput(BottomPieceRect(c), r.x, r.y, r.w, r.h);
    return r;
}
}
