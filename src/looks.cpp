#include "looks.h"

#include <algorithm>
#include <cmath>

#include "GPU2D_Soft.h"
#include "GPU3D.h"
#include "GPU3D_Wide.h"

namespace sims2hd
{
namespace
{
// The DS's brightness effects on one 6-bit channel (GPU2D_Soft.h, ColorBrightnessUp/Down).
inline u32 Up(u32 c, u32 f, u32 bias) { return c + ((((63 - c) * f) + bias) >> 4); }
inline u32 Down(u32 c, u32 f, u32 bias) { return c - (((c * f) + bias) >> 4); }

// A 3D pixel (8 bits per channel, from 6) with what the DS did to its 3D layer on that line:
// effect 2 brightens, 3 darkens by evy (BLDCNT, BLDY), then the master brightness.
inline u32 Shade(u32 px, u32 effect, u32 evy, u32 master)
{
    const u32 mmode = master >> 14, mf = std::min<u32>(master & 0x1F, 16);
    if (evy > 16) evy = 16;
    if ((effect < 2 || !evy) && (mmode == 0 || mmode == 3 || !mf)) return px | 0xFF000000;
    u32 out = 0xFF000000;
    for (int sh = 0; sh <= 16; sh += 8)
    {
        u32 c = ((px >> sh) & 0xFF) >> 2;
        if (effect == 2) c = Up(c, evy, 0x8);
        else if (effect == 3) c = Down(c, evy, 0x7);
        if (mmode == 1) c = Up(c, mf, 0x0);
        else if (mmode == 2) c = Down(c, mf, 0xF);
        out |= ((c << 2) | (c >> 4)) << sh;
    }
    return out;
}
}

void Looks::Apply(App& app)
{
    const Settings& s = app.settings;
    melonDS::NDS& nds = *app.nds;
    const int n = std::clamp(s.Res3D(), 1, 12);
    const bool on = n > 1 || s.widescreen;
    melonDS::Looks3D.InterpolationEnabled = s.Interpolate60();
    if (!on)
    {
        if (wasOn)
        {
            melonDS::WideWait();
            melonDS::Wide.Owner = nullptr;
            melonDS::Looks2D.Record3D = false;
            app.topBig.clear();
            app.topAspect = 4.0f / 3.0f;
            wasOn = false;
        }
        wideFrames = 0;
        return;
    }
    const int h = SH * n;
    const int w = s.widescreen ? (int)std::lround(h * 16.0 / 9.0) : SW * n;
    melonDS::Wide.Owner = &nds.GPU.GPU3D;
    melonDS::Wide.Width = w;
    melonDS::Wide.Height = h;
    melonDS::Wide.ViewWidth = (float)w / n;
    // Continuing polygons that end short of the view's edge is for Pokemon's skies; Sims 2's
    // town has none, and it only stretched the edge of the ground.
    melonDS::Wide.ExtendEdges = false;
    melonDS::Looks2D.Record3D = true;
    app.topAspect = (float)w / h;
    wasOn = true;
}

void Looks::Frame(App& app)
{
    if (!wasOn) return;
    const melonDS::WideState& wide = melonDS::Wide;
    const int W = wide.Width, H = wide.Height, n = H / SH;
    // The DS's 256 columns in the middle of the picture, n output pixels each.
    const int ox = (W - SW * n) / 2;
    const u32* ds = app.Top();
    const u32* front = wide.Front;
    const int fw = wide.FrontW, fh = wide.FrontH;
    // The wide picture lags a few frames behind being switched on (it is published one frame
    // after it is drawn); until then, and if it has another size, the DS's picture scaled.
    const bool have = front && fw == W && fh == H && ++wideFrames > 3;
    // A picture with no 3D in it (menus): the wide picture is not drawn and would be an old
    // one, so the sides stay black.
    const melonDS::GPU2DLooks& r2d = melonDS::Looks2D;
    const bool any3D = std::any_of(r2d.Pixel3D, r2d.Pixel3D + SW * SH, [](u8 c) { return c != 0; });

    app.topBig.resize((size_t)W * H);
    app.topBigW = W;
    app.topBigH = H;
    u32* out = app.topBig.data();
    for (int y = 0; y < H; y++)
    {
        const int dy = y / n;
        const u32 evy = r2d.LineEVY[dy], master = r2d.LineMaster[dy], blend = r2d.LineBlendCnt[dy];
        // What the DS does to the 3D layer on this line, for the parts beside its picture.
        const u32 sideEffect = (blend & 0x01) ? (blend >> 6) & 3 : 0;
        const u8* codes = &r2d.Pixel3D[dy * SW];
        const u32* dsRow = ds + dy * SW;
        const u32* wRow = have ? front + (size_t)y * W : nullptr;
        u32* o = out + (size_t)y * W;
        for (int x = 0; x < W; x++)
        {
            const int sx = x - ox;
            if (sx >= 0 && sx < SW * n)
            {
                const int dx = sx / n;
                const u8 code = codes[dx];
                // The DS's own pixel where it did not show its 3D layer as it is (2D, or 3D
                // blended with 2D), and where the sharp picture has nothing.
                if (!code || !wRow || !(wRow[x] >> 24)) o[x] = dsRow[dx];
                else o[x] = Shade(wRow[x], code, evy, master);
                continue;
            }
            // Beside the DS's picture (widescreen): the wide 3D picture, or where it has
            // nothing, the DS's edge pixel of that line carried on (status bar, backdrop).
            const int edge = sx < 0 ? 0 : SW - 1;
            if (!any3D) o[x] = 0xFF000000;
            else if (wRow && ((wRow[x] >> 24) || codes[edge])) o[x] = Shade(wRow[x], sideEffect, evy, master);
            else o[x] = dsRow[edge];
        }
    }
}

void Looks::Shutdown()
{
    melonDS::WideWait();
    melonDS::WideShutdown();
    melonDS::Wide.Owner = nullptr;
}
}
