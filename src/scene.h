// What the game is showing, read from the screens' pixels: decides the AUTO layout.
#pragma once
#include "common.h"
#include "layout.h"

namespace sims2wide
{
// The status bar under the world view (clock, money, motive bar) is on the top screen: free roam
// or a conversation in town, where the bottom screen is only a menu panel.
bool HudVisible(const u32* top);

// The layout AUTO shows, with hysteresis so that fades and one-frame flashes do not flicker.
struct AutoLayout
{
    bool hud = false;
    int streak = 0; // frames the detection has disagreed with the layout shown
    // Returns the layout to show, and which screen FOCUS makes big.
    Layout Update(const u32* top, const u32* bottom, bool& mainIsTop);
};
}
