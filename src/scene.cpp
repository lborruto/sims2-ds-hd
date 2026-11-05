#include "scene.h"

#include <cstdio>
#include <cstdlib>

namespace sims2wide
{
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

    // The touch screen's turn: with the HUD, a social reaction, a full-screen menu or the vacuum;
    // without it, the casino's card game. Only rules checked on a save state of the activity: a
    // guessed "dark top, lit bottom" rule made the touch screen big in dim places (the casino)
    // and was removed.
    bool bigNow = hudNow ? (SocialVisible(topPx) || WarmMenuVisible(bottomPx) || VacuumVisible(bottomPx))
                         : CardsVisible(topPx);
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
                frame, hudNow, SocialVisible(topPx), WarmMenuVisible(bottomPx), VacuumVisible(bottomPx) || CardsVisible(topPx), topMotion, topLuma, bottomMotion, bottomLuma);

    if (hud || bottomBig) return Layout::Focus;
    // Anything else (title, Create-a-Sim, loading) needs both screens alike: SIDE.
    return Layout::Side;
}
}
