#include "shots.h"

#include <vector>

#include "compose.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace sims2wide
{
void WriteShot(const std::string& path, const u32* top, const u32* bottom)
{
    std::vector<u8> rgb(SW * SH * 2 * 3);
    for (int s = 0; s < 2; s++)
    {
        const u32* src = s ? bottom : top;
        for (int i = 0; i < SW * SH; i++)
        {
            u32 p = src[i]; // 0xAARRGGBB
            u8* d = &rgb[(s * SW * SH + i) * 3];
            d[0] = (p >> 16) & 0xFF; d[1] = (p >> 8) & 0xFF; d[2] = p & 0xFF;
        }
    }
    stbi_write_png(path.c_str(), SW, SH * 2, 3, rgb.data(), SW * 3);
}

void WriteRgb(const std::string& path, int w, int h, const u8* rgb)
{
    stbi_write_png(path.c_str(), w, h, 3, rgb, w * 3);
}

void WriteImage(const std::string& path, const u32* px, int w, int h)
{
    std::vector<u8> rgb((size_t)w * h * 3);
    for (size_t i = 0; i < (size_t)w * h; i++)
    {
        rgb[i * 3] = (px[i] >> 16) & 0xFF; rgb[i * 3 + 1] = (px[i] >> 8) & 0xFF; rgb[i * 3 + 2] = px[i] & 0xFF;
    }
    stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3);
}

void WriteCompose(const std::string& path, Layout l, bool mainIsTop, const u32* top, int topW, int topH, const u32* bottom)
{
    const Composition c = Compose(l, mainIsTop, (float)topW / topH, topW, topH);
    std::vector<u8> rgb(c.cw * c.ch * 3, 0);
    for (int i = 0; i < c.cw * c.ch; i++) { rgb[i * 3] = 16; rgb[i * 3 + 1] = 18; rgb[i * 3 + 2] = 24; }
    // The pieces in order: a later one may overlap an earlier one, with its opacity.
    for (const Piece& q : c.pieces)
    {
        const Rect& r = q.to;
        const u32* src = q.src == Src::Top ? top : bottom;
        const int iw = q.src == Src::Top ? topW : SW;
        const int a = q.alpha;
        for (int y = 0; y < r.h; y++)
            for (int x = 0; x < r.w; x++)
            {
                u32 px = src[(q.from.y + y * q.from.h / r.h) * iw + q.from.x + x * q.from.w / r.w];
                u8* d = &rgb[((r.y + y) * c.cw + r.x + x) * 3];
                d[0] = (u8)((((px >> 16) & 0xFF) * a + d[0] * (255 - a)) / 255);
                d[1] = (u8)((((px >> 8) & 0xFF) * a + d[1] * (255 - a)) / 255);
                d[2] = (u8)(((px & 0xFF) * a + d[2] * (255 - a)) / 255);
            }
    }
    stbi_write_png(path.c_str(), c.cw, c.ch, 3, rgb.data(), c.cw * 3);
}
}
