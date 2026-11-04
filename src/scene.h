// What the game is showing, read from the screens' pixels: decides the AUTO layout.
#pragma once
#include "common.h"
#include "layout.h"

namespace sims2wide
{
// The status bar under the world view (clock, money, motive bar) is on the top screen: free roam
// or a conversation in town, where the bottom screen is only a menu panel.
bool HudVisible(const u32* top);

// A social reaction (Impress, Cheer Up, Calm…): the top screen keeps the status bar and gets a
// name and relationship box on its first rows; the bottom screen is the timed three-icon game.
bool SocialVisible(const u32* top);

// A full-screen touch menu over the town: the shop's catalogue, Build Rooms (warm, bright
// corners; the panel, the phone, dialogues and popups are purple or grey).
bool WarmMenuVisible(const u32* bottom);

// The hotel's vacuum: the bowl on the touch screen (dust, items to drag into the bag, the filter
// to blow clear); the world and the HUD stay on top.
bool VacuumVisible(const u32* bottom);

// A screen sampled on a coarse grid: its brightness, and how much of it changed since the last
// take (SIMS2WIDE_PROBE=N prints both every N frames, to study a situation).
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
    int darkStreak = 0;  // frames in a row without the HUD, with a dark top and a lit bottom
    Sample top, bottom;
    float topMotion = 0, bottomMotion = 0, topLuma = 0, bottomLuma = 0; // smoothed, about a second
    long frame = 0;

    // Returns the layout to show; sets mainIsTop when the big screen changes (Tab still swaps
    // by hand in between).
    Layout Update(const u32* top, const u32* bottom, bool& mainIsTop);
};
}
