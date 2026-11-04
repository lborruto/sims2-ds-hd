#include "shots.h"

#include <vector>

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

void WriteCompose(const std::string& path, Layout l, bool mainIsTop, const u32* top, const u32* bottom)
{
    Placement p = Place(l, mainIsTop);
    std::vector<u8> rgb(p.cw * p.ch * 3, 0);
    for (int i = 0; i < p.cw * p.ch; i++) { rgb[i * 3] = 16; rgb[i * 3 + 1] = 18; rgb[i * 3 + 2] = 24; }
    // The screen drawn second may overlap the first one, with the placement's opacity.
    for (int i = 0; i < 2; i++)
    {
        const bool isTop = (i == 0) == p.topFirst;
        const Rect& r = isTop ? p.top : p.bottom;
        const u32* src = isTop ? top : bottom;
        const int a = i ? p.alpha : 255;
        for (int y = 0; y < r.h; y++)
            for (int x = 0; x < r.w; x++)
            {
                u32 px = src[(y * SH / r.h) * SW + x * SW / r.w];
                u8* d = &rgb[((r.y + y) * p.cw + r.x + x) * 3];
                d[0] = (u8)((((px >> 16) & 0xFF) * a + d[0] * (255 - a)) / 255);
                d[1] = (u8)((((px >> 8) & 0xFF) * a + d[1] * (255 - a)) / 255);
                d[2] = (u8)(((px & 0xFF) * a + d[2] * (255 - a)) / 255);
            }
    }
    stbi_write_png(path.c_str(), p.cw, p.ch, 3, rgb.data(), p.cw * 3);
}
}
