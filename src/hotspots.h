// The touch targets of the game's screens, in DS bottom-screen pixels: where to tap each button
// and the area the focus ring frames. Measured on save states and checked by tapping each one
// headless (scripts/hotspots/, tests/hotspots_check.sh); CLAUDE.md lists what each tap does.
// CONSOLE's navigator moves a focus between them and taps the focused one.
#pragma once
#include <vector>

#include "layout.h"
#include "screens.h"

namespace sims2wide
{
struct Hotspot
{
    const char* name;
    int x, y;  // where the tap goes
    Rect area; // what the focus ring frames (holds x, y); navigation uses its centre
};

// The targets of a screen, the tab's own first (the navigator's first focus), then for the
// panel the five tab icons and the phone. Empty for None, Unknown and Vacuum (the stick).
const std::vector<Hotspot>& HotspotsFor(ScreenState s);

// The nearest target from `from` in the direction (dx, dy), by the distance between the areas'
// centres, among those within 60 degrees of that direction; `from` when there is none.
int Neighbour(const std::vector<Hotspot>& h, int from, int dx, int dy);

// The panel's tab icons, in Tab order (Interact, Pockets, Goals, Relationships, Options).
extern const Hotspot kTabIcon[5];
// The panel's phone icon (opens the phone).
extern const Hotspot kPhoneIcon;
// The conversation's blue arrow (next line; the last one ends the talk).
extern const Hotspot kDialogueArrow;
// The X of every popup kind: the goal box, the tips and Game Paused (their X sit 16 pixels
// apart; this point is inside both).
extern const Hotspot kPopupClose;
// The phone's X (closes it, back to the panel).
extern const Hotspot kPhoneClose;
}
