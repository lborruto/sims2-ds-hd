// Which screen the game shows on the touch screen, read from the two screens' pixels. Every rule
// is a flat row or fixed samples measured on save states of the situation (CLAUDE.md, traps),
// never motion. AUTO (scene.*) uses the activity rules; CONSOLE uses the whole answer.
#pragma once
#include <string>

#include "common.h"

namespace sims2wide
{
// None: no HUD and no known screen (title, Create-a-Sim, loading): both screens matter alike.
// Unknown: the HUD is up but the touch screen is none of the others (fades, slides, screens not
// measured yet).
enum class Screen { None, Panel, Phone, Dialogue, Popup, Gift, Social, Vacuum, Cards, Catalogue, Build, Unknown };
// The panel's tab (only with Screen::Panel).
enum class Tab { None, Interact, Pockets, Goals, Relationships, Options };

struct ScreenState
{
    Screen screen = Screen::None;
    Tab tab = Tab::None;
    bool operator==(const ScreenState&) const = default;
};

const char* ScreenName(Screen s);
const char* TabName(Tab t);
// "Panel/Pockets", "Phone", "None"…
std::string ScreenStateName(ScreenState s);

// One frame's answer.
ScreenState Recognise(const u32* top, const u32* bottom);

// Recognise with hysteresis: a change is accepted after 6 frames that agree on it; a Popup or a
// Dialogue after 3 (a goal popup must not be missed); Unknown only after 60, since every fade
// and slide between two known screens reads Unknown for up to about 30 frames.
class ScreenTracker
{
public:
    ScreenState Update(const u32* top, const u32* bottom);
    ScreenState Current() const { return current; }

private:
    ScreenState current, candidate;
    int streak = 0;
};

// The rules AUTO shares (scene.cpp).
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
// The casino's card game (Keelhaulin' Cards): its score bar on the top screen.
bool CardsVisible(const u32* top);
}
