// What the game is showing, read from the screens' pixels: decides the AUTO layout.
#pragma once
#include "common.h"
#include "layout.h"
#include "screens.h" // the rules AUTO uses: HudVisible, SocialVisible, WarmMenuVisible, VacuumVisible, CardsVisible

namespace sims2hd
{
// A screen sampled on a coarse grid: its brightness, and how much of it changed since the last
// take (SIMS2HD_PROBE=N prints both every N frames, to study a situation).
struct Sample
{
    static constexpr int kW = 32, kH = 24, kN = kW * kH; // one sample every 8 pixels
    u32 px[kN] {};
    bool valid = false;
    void Take(const u32* screen, float& changed, float& luma);
};

// The layout AUTO shows, with hysteresis so that fades and one-frame flashes do not flicker.
struct AutoLayout
{
    bool hud = false;
    int streak = 0;      // frames the HUD detection has disagreed with the layout shown

    // The touch screen's turn: FOCUS with the bottom big.
    bool bottomBig = false;
    int bigStreak = 0;   // frames the detection has disagreed with bottomBig
    Sample top, bottom;
    float topMotion = 0, bottomMotion = 0, topLuma = 0, bottomLuma = 0; // smoothed, about a second
    long frame = 0;

    // Returns the layout to show; sets mainIsTop when the big screen changes (Tab still swaps
    // by hand in between).
    Layout Update(const u32* top, const u32* bottom, bool& mainIsTop);
};
}
