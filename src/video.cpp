#include "video.h"

#include <cstdio>

namespace sims2wide
{
bool Video::Open()
{
    win = SDL_CreateWindow("The Sims 2 DS - one screen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "SDL window: %s\n", SDL_GetError()); return false; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "SDL renderer: %s\n", SDL_GetError()); return false; }
    for (auto& t : tex) t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SW, SH);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    return true;
}

void Video::Present(const App& app)
{
    // --- looks: the top screen at its real size (App::TopImage: sharper / widescreen 3D). The
    // texture follows the picture's size. Layouts size the top screen from app.topAspect.
    {
        int tw, th, cw, ch;
        const u32* top = app.TopImage(tw, th);
        SDL_QueryTexture(tex[0], nullptr, nullptr, &cw, &ch);
        if (cw != tw || ch != th)
        {
            SDL_DestroyTexture(tex[0]);
            tex[0] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tw, th);
        }
        SDL_UpdateTexture(tex[0], nullptr, top, tw * 4);
    }
    // --- end looks
    SDL_UpdateTexture(tex[1], nullptr, app.Bottom(), SW * 4);
    Placement p = Place(app.shown, app.mainIsTop);
    int ow, oh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    CanvasFit fit = CanvasFit::For(p, ow, oh);
    SDL_SetRenderDrawColor(ren, 16, 18, 24, 255);
    SDL_RenderClear(ren);
    SDL_Rect dt, db;
    fit.ToOutput(p.top, dt.x, dt.y, dt.w, dt.h);
    fit.ToOutput(p.bottom, db.x, db.y, db.w, db.h);
    SDL_RenderCopy(ren, tex[0], nullptr, &dt);
    SDL_RenderCopy(ren, tex[1], nullptr, &db);
    SDL_RenderPresent(ren);
}

bool Video::TouchAt(const App& app, int wx, int wy, int& tx, int& ty) const
{
    Placement p = Place(app.shown, app.mainIsTop);
    int ow, oh, ww, wh;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    SDL_GetWindowSize(win, &ww, &wh);
    // Window points -> output pixels (they differ on HiDPI screens).
    float px = wx * (float)ow / ww, py = wy * (float)oh / wh;
    return TouchFromOutput(p, CanvasFit::For(p, ow, oh), px, py, tx, ty);
}
}
