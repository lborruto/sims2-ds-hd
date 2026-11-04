#include "video.h"

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
    const Placement p = Placed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    const CanvasFit fit = CanvasFit::For(p, ow, oh);
    SDL_SetRenderDrawColor(ren, 16, 18, 24, 255);
    SDL_RenderClear(ren);
    // The screen drawn second may lie over the first one (FOCUS's panel), with its opacity.
    for (int i = 0; i < 2; i++)
    {
        const bool isTop = (i == 0) == p.topFirst;
        SDL_Rect d;
        fit.ToOutput(isTop ? p.top : p.bottom, d.x, d.y, d.w, d.h);
        SDL_Texture* t = tex[isTop ? 0 : 1];
        SDL_SetTextureAlphaMod(t, i ? p.alpha : 255);
        SDL_RenderCopy(ren, t, nullptr, &d);
    }
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

bool Video::TouchAt(const App& app, int wx, int wy, int& tx, int& ty) const
{
    const Placement p = Placed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return TouchFromOutput(p, CanvasFit::For(p, ow, oh), px, py, tx, ty);
}

bool Video::OverTop(const App& app, int wx, int wy) const
{
    const Placement p = Placed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float px, py;
    ToOutput(wx, wy, px, py);
    return sims2wide::OverTop(p, CanvasFit::For(p, ow, oh), px, py);
}

SDL_Rect Video::BottomRect(const App& app) const
{
    const Placement p = Placed(app);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_Rect r;
    CanvasFit::For(p, ow, oh).ToOutput(p.bottom, r.x, r.y, r.w, r.h);
    return r;
}
}
