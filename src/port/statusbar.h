// The HD status bar: the clock, the money and the Sanity meter the game shows on the top
// screen's last 16 rows (176-191), redrawn with the UI kit from the values read in RAM
// (GameState::hud) and the game's own words for them (its string table, RomData). CONSOLE crops
// the game's own bar off the world wherever it draws this one.
#pragma once
#include <string>

#include "compose.h"
#include "gamestate.h"

namespace sims2wide
{
class RomData;

// The game's own text for the bar, in the game's language: the clock's format ("@hh:@mm @am"),
// its am / pm, the money's format (the game's '$' is its simoleon glyph, shown as §) and the
// meter's label (its trailing colon dropped; empty when the table has none that reads cleanly:
// the meter then goes without a label, never with made-up text). ok: the formats read (with
// @hh, @mm and @1); false without a string table (no ROM data, another game): no bar, and
// CONSOLE keeps the game's own HUD in the world.
struct HudStrings
{
    bool ok = false;
    std::string clock, am, pm, money, label;
};
HudStrings HudStringsFrom(const RomData* rom);

// The bar's texts and the meter's fill (0..1) for hud; false (draw nothing) when hud is not
// valid or a value is out of range (hour 0-23, minute 0-59, tier 0-3, sanity max > 0, money
// 0..99,999,999): a load's garbage hides the bar, it never divides by zero.
bool FormatHud(const HudState& hud, const HudStrings& s, std::string& clock, std::string& money, float& fill);
// The money as the game writes it: thousands grouped by ',' (the game's formatter has no
// separator in its string table; the English style for every language).
std::string GroupThousands(int value);

// The Sanity bar's colours per tier, as the game draws them (0 red, 1 green, 2 yellow, 3 pale
// lavender): the rim, the body and the highlight.
struct TierColours { u32 rim, body, light; };
TierColours MeterColours(int tier);

constexpr int kStatusBarH = 48;
// The bar's still layer (ui::UiItem::layer), above an HD menu's frame (0) and content (1).
constexpr int kStatusBarLayer = 2;
// The status bar (UI items appended to c.ui) with its top-left at (x, y), at most maxW wide: the
// meter shrinks to fit, then the label goes; every item stays inside the bar's panel. Returns
// the panel's rectangle ({x, y, 0, 0} when nothing is drawn).
Rect AddStatusBar(Composition& c, const HudState& hud, const HudStrings& s, int x, int y, int maxW);
// The bar at the bottom left of the composition's world (its first top-screen piece), at
// kStatusBarY: below CONSOLE's menu frame, so the same place in town and with the menu up.
constexpr int kStatusBarY = 660;
Rect AddStatusBar(Composition& c, const HudState& hud, const HudStrings& s);
void AddStatusBar(Composition& c, const HudState& hud, const RomData* rom);
}
