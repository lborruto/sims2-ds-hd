#include "scene.h"

namespace sims2wide
{
bool HudVisible(const u32* top)
{
    // The strip's bottom border, one flat colour from edge to edge; the rows above it carry
    // the clock's text and the motive bar. Checked against every situation shot so far: only
    // gameplay matches.
    for (int x = 8; x < 250; x += 8)
        if ((top[190 * SW + x] & 0xFFFFFF) != 0x415969) return false;
    return true;
}

Layout AutoLayout::Update(const u32* top, const u32* /*bottom*/, bool& mainIsTop)
{
    bool now = HudVisible(top);
    streak = now == hud ? 0 : streak + 1;
    if (streak >= 10) { hud = now; streak = 0; }
    (void)mainIsTop; // the town always has the world big; Tab still swaps by hand
    // Anything else (title, Create-a-Sim, loading) needs both screens alike: SIDE.
    return hud ? Layout::Focus : Layout::Side;
}
}
