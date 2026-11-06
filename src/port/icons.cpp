#include "port/icons.h"

#include "ui/kit.h"

namespace sims2wide
{
int IconCache::Frames(SpriteRef s)
{
    if (!rom) return 0;
    return rom->SpriteFrames(s.desc);
}

const SpriteFrame* IconCache::Frame(SpriteRef s, int frame)
{
    const auto key = std::make_tuple(s, frame);
    auto it = frames.find(key);
    if (it == frames.end())
    {
        SpriteFrame f;
        if (!rom || !rom->Sprite(s.tiles, s.desc, s.pal, frame, f)) f = SpriteFrame {};
        it = frames.emplace(key, std::move(f)).first;
    }
    return it->second.w > 0 ? &it->second : nullptr;
}

const SpriteFrame* IconCache::Scaled(SpriteRef s, int frame, int w, int h)
{
    if (w <= 0 || h <= 0 || w > kMaxScaled || h > kMaxScaled) return nullptr;
    const auto key = std::make_tuple(s, frame, w, h);
    auto it = scaled.find(key);
    if (it == scaled.end())
    {
        SpriteFrame out;
        if (const SpriteFrame* src = Frame(s, frame))
        {
            out.w = w;
            out.h = h;
            out.argb.assign((size_t)w * h, 0);
            // The kit's scaler, as an image item of the whole frame drawn at w x h.
            // (Pixel's nearest copy, whatever the game pictures' setting: Image() gives Smooth the frame.)
            ui::UiItem im = ui::ImageItem({0, 0, w, h}, src->argb.data(), src->w, src->h, {0, 0, src->w, src->h});
            im.smooth = false;
            ui::Rasterise({im}, 1.0f, out.argb.data(), w, h);
        }
        it = scaled.emplace(key, std::move(out)).first;
    }
    return it->second.w > 0 ? &it->second : nullptr;
}

const SpriteFrame* IconCache::Scaled(SpriteRef s, int frame, int scale)
{
    const SpriteFrame* f = Frame(s, frame);
    if (!f || scale <= 0 || scale > 64) return nullptr;
    return Scaled(s, frame, f->w * scale, f->h * scale);
}

ui::UiItem IconCache::Image(SpriteRef s, int frame, const SpriteFrame& scaled, Rect to)
{
    if (ui::SmoothPictures())
        if (const SpriteFrame* f = Frame(s, frame); f && f->w > 0 && f->h > 0)
            return ui::ImageItem(to, f->argb.data(), f->w, f->h, {0, 0, f->w, f->h});
    return ui::ImageItem(to, scaled.argb.data(), scaled.w, scaled.h, {0, 0, scaled.w, scaled.h});
}
}
