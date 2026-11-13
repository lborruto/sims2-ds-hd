#include "port/icons.h"

#include <utility>

#include "ui/kit.h"

namespace sims2hd
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
            // (the nearest copy, whatever the game pictures' setting: Image() draws the frame itself.)
            ui::UiItem im = ui::ImageItem({0, 0, w, h}, src->argb.data(), src->w, src->h, {0, 0, src->w, src->h});
            im.smooth = false;
            ui::Rasterise({im}, 1.0f, out.argb.data(), w, h);
        }
        it = scaled.emplace(key, std::move(out)).first;
    }
    return it->second.w > 0 ? &it->second : nullptr;
}

const SpriteFrame* IconCache::Look(SpriteRef s, int frame, SpriteLook look, int scale, int light)
{
    if (scale < 1 || scale > 8 || light < 0 || light > 255) return nullptr;
    if (look == SpriteLook::Plain && light == 255) return scale == 1 ? Frame(s, frame) : Scaled(s, frame, scale);
    // Two fade slots, the light asked now and the one before it: a new light frees only the slot
    // two lights back, so the pictures of the composition built with the previous light (still
    // drawn, or held by a layer's cache, while the next one is built) never dangle. A light asked
    // again keeps its slot.
    if (light != 255 && light != fadedLight[0])
    {
        if (light == fadedLight[1]) { std::swap(faded[0], faded[1]); std::swap(fadedLight[0], fadedLight[1]); }
        else
        {
            faded[1] = std::move(faded[0]);
            fadedLight[1] = fadedLight[0];
            faded[0] = {};
            fadedLight[0] = light;
        }
    }
    auto& cache = light == 255 ? looks : faded[0];
    const auto key = std::make_tuple(s, frame, (int)look, scale);
    auto it = cache.find(key);
    if (it == cache.end())
    {
        SpriteFrame out;
        if (const SpriteFrame* f = Frame(s, frame))
        {
            out.w = f->w * scale;
            out.h = f->h * scale;
            out.argb.resize((size_t)out.w * out.h);
            for (int y = 0; y < out.h; y++)
                for (int x = 0; x < out.w; x++)
                {
                    const int sx = look == SpriteLook::Flip ? f->w - 1 - x / scale : x / scale;
                    const u32 c = f->argb[(size_t)(y / scale) * f->w + sx];
                    u32 r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
                    if (look == SpriteLook::Grey)
                    {
                        const u32 l = (r * 77 + g * 150 + b * 29) >> 8;
                        r = g = b = 0x40 + l * 3 / 4;   // (lifted: a grey button, not a black one)
                    }
                    else if (look == SpriteLook::Dim) { r = r * 2 / 5; g = g * 2 / 5; b = b * 2 / 5; }
                    r = r * (u32)light / 255; g = g * (u32)light / 255; b = b * (u32)light / 255;
                    out.argb[(size_t)y * out.w + x] = (c & 0xFF000000) | r << 16 | g << 8 | b;
                }
        }
        it = cache.emplace(key, std::move(out)).first;
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
    // The frame as it is, in both looks: the kit enlarges it at the window's own size (Pixel:
    // nearest by the whole factor; a copy scaled first would hide the real pixels behind bilinear
    // at a fractional size). At a whole factor the pixels are the copy's.
    if (const SpriteFrame* f = Frame(s, frame); f && f->w > 0 && f->h > 0)
        return ui::ImageItem(to, f->argb.data(), f->w, f->h, {0, 0, f->w, f->h});
    return ui::ImageItem(to, scaled.argb.data(), scaled.w, scaled.h, {0, 0, scaled.w, scaled.h});
}

ui::UiItem IconCache::Tiered(SpriteRef s, int frame, int tier, Rect box)
{
    if (const SpriteFrame* f = Frame(s, frame); f && f->w > 0 && f->h > 0)
        return ui::TierImage(box, f->argb.data(), f->w, f->h, {0, 0, f->w, f->h}, tier);
    ui::UiItem none = ui::ImageItem({box.x, box.y, 0, 0}, nullptr, 0, 0, {});
    none.tier = tier;
    return none;
}

void AddTiered(std::vector<ui::UiItem>& items, IconCache* icons, SpriteRef s, int frame, int tier, Rect box, u32 opacity)
{
    if (!icons) return;
    ui::UiItem it = icons->Tiered(s, frame, tier, box);
    if (!it.src) return;
    it.fill = opacity;
    items.push_back(it);
}
}
