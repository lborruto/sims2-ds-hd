#include "scene.h"

#include <cstdio>
#include <cstdlib>

namespace sims2wide
{
// A row of the screen is one flat colour from x0 to x1 (sampled every 8 pixels; by default from
// edge to edge, x 8 to 248).
static bool FlatRow(const u32* screen, int y, u32 rgb, int x0 = 8, int x1 = 248)
{
    for (int x = x0; x <= x1; x += 8)
        if ((screen[y * SW + x] & 0xFFFFFF) != rgb) return false;
    return true;
}

bool HudVisible(const u32* top)
{
    // The strip's bottom border, one flat colour from edge to edge; the rows above it carry
    // the clock's text and the motive bar. Checked against every situation shot so far: only
    // gameplay matches.
    return FlatRow(top, 190, 0x415969);
}

bool SocialVisible(const u32* top)
{
    // The top edge of the name and relationship box, from x 5 to 252, on every frame of the
    // Impress game (act-social); on no other screen seen (the HUD's row 190 is another colour).
    return HudVisible(top) && FlatRow(top, 0, 0x597192);
}

bool VacuumVisible(const u32* bottom)
{
    // The vacuum's bowl keeps the town's HUD on top; its frame on the touch screen is told by
    // row 1: the light blue top edge of the gauge (top left, x 4 to 44) and the dark teal frame
    // right of it (x 60 to 100). On every frame of the vacuum (act-vacuum and runs from it), on
    // no other screen seen.
    if (FlatRow(bottom, 1, 0x30A2D3, 4, 44) && FlatRow(bottom, 1, 0x304951, 60, 100)) return true;
    // Clogged (dust touched with the stylus, until the player blows): the game tints the whole
    // screen sepia, dithered, so the same row reads differently but just as steadily (act-vacuum-clog).
    static const struct { int x; u32 rgb; } kClogged[] = {
        {4, 0x51A2BA}, {12, 0x51A2BA}, {20, 0x51A2BA}, {28, 0x459AB6}, {36, 0x459AB6}, {44, 0x459AB6},
        {60, 0x515D59}, {68, 0x515D59}, {76, 0x455555}, {84, 0x455555}, {92, 0x515D59}};
    for (const auto& p : kClogged)
        if ((bottom[SW + p.x] & 0xFFFFFF) != p.rgb) return false;
    return true;
}

static u32 CornerMean(const u32* screen, int x0, int y0)
{
    int r = 0, g = 0, b = 0;
    for (int y = y0; y < y0 + 8; y++)
        for (int x = x0; x < x0 + 8; x++)
        {
            u32 p = screen[y * SW + x];
            r += (p >> 16) & 0xFF; g += (p >> 8) & 0xFF; b += p & 0xFF;
        }
    return ((u32)(r / 64) << 16) | ((u32)(g / 64) << 8) | (u32)(b / 64);
}

bool WarmMenuVisible(const u32* bottom)
{
    // The shop's catalogue (parchment) and Build Rooms (wood) fill the screen with warm, bright
    // colours up to its corners; the panel (#5961A2 to #606EAD), the phone (grey), dialogues
    // and the paused panel are cool.
    const int corners[4][2] = {{0, 0}, {248, 0}, {0, 184}, {248, 184}};
    for (auto& c : corners)
    {
        u32 m = CornerMean(bottom, c[0], c[1]);
        int r = (m >> 16) & 0xFF, g = (m >> 8) & 0xFF, b = m & 0xFF;
        if (r < b + 40 || (r * 2 + g * 5 + b) / 8 < 100) return false;
    }
    return true;
}

void Sample::Take(const u32* screen, float& changed, float& luma)
{
    int moved = 0;
    long sum = 0;
    for (int j = 0; j < kH; j++)
        for (int i = 0; i < kW; i++)
        {
            u32 p = screen[(j * 8 + 4) * SW + i * 8 + 4] & 0xFFFFFF;
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            sum += (r * 2 + g * 5 + b) / 8;
            u32 q = px[j * kW + i];
            int dr = r - (int)((q >> 16) & 0xFF), dg = g - (int)((q >> 8) & 0xFF), db = b - (int)(q & 0xFF);
            if (dr > 24 || dr < -24 || dg > 24 || dg < -24 || db > 24 || db < -24) moved++;
            px[j * kW + i] = p;
        }
    changed = valid ? (float)moved / kN : 0.0f;
    luma = (float)sum / kN;
    valid = true;
}

Layout AutoLayout::Update(const u32* topPx, const u32* bottomPx, bool& mainIsTop)
{
    frame++;
    bool hudNow = HudVisible(topPx);
    streak = hudNow == hud ? 0 : streak + 1;
    if (streak >= 10) { hud = hudNow; streak = 0; }

    float tc, tl, bc, bl;
    top.Take(topPx, tc, tl);
    bottom.Take(bottomPx, bc, bl);
    const float k = 1.0f / 30.0f; // smoothed over about a second
    topMotion += (tc - topMotion) * k;
    bottomMotion += (bc - bottomMotion) * k;
    topLuma += (tl - topLuma) * k;
    bottomLuma += (bl - bottomLuma) * k;

    // The touch screen's turn. With the HUD: a social reaction, a full-screen menu or the
    // vacuum. Without it, the fallback for the activities not reached yet (cards, painting…): the
    // game dims the world on top and plays on the touch screen. Unverified; the hotel's dark
    // rooms fade out the top before the panel (about 30 frames of dark top and lit bottom), so it
    // must hold for two seconds.
    const bool darkNow = !hudNow && topLuma < 40 && bottomLuma > 50;
    darkStreak = darkNow ? darkStreak + 1 : 0;
    bool bigNow = hudNow ? (SocialVisible(topPx) || WarmMenuVisible(bottomPx) || VacuumVisible(bottomPx))
                         : darkStreak >= 120;
    bigStreak = bigNow == bottomBig ? 0 : bigStreak + 1;
    if (bigStreak >= 10)
    {
        bottomBig = bigNow;
        bigStreak = 0;
        mainIsTop = !bottomBig;
    }

    static const char* probe = getenv("SIMS2WIDE_PROBE");
    static const int probeEvery = probe ? atoi(probe) : 0;
    if (probeEvery > 0 && frame % probeEvery == 0)
        fprintf(stderr, "probe frame %ld: hud %d social %d warm %d vacuum %d | top motion %.4f luma %.0f | bottom motion %.4f luma %.0f\n",
                frame, hudNow, SocialVisible(topPx), WarmMenuVisible(bottomPx), VacuumVisible(bottomPx), topMotion, topLuma, bottomMotion, bottomLuma);

    if (hud || bottomBig) return Layout::Focus;
    // Anything else (title, Create-a-Sim, loading) needs both screens alike: SIDE.
    return Layout::Side;
}
}
