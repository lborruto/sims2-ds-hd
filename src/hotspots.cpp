#include "hotspots.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace sims2hd
{
// Every point and area below was read off headless shots of the save states (European ROM) and
// checked by tapping it (scripts/hotspots/, tests/hotspots_check.sh); CLAUDE.md lists what each
// tap does and which ones could only be placed from shots.

const Hotspot kTabIcon[5] = {
    {"interact", 16, 22, {1, 7, 31, 31}},
    {"pockets", 16, 57, {1, 42, 31, 31}},
    {"goals", 16, 92, {1, 77, 31, 31}},
    {"relationships", 16, 127, {1, 112, 31, 31}},
    {"options", 16, 162, {1, 147, 31, 31}},
};
const Hotspot kPhoneIcon = {"phone", 232, 22, {216, 7, 33, 33}};
const Hotspot kDialogueArrow = {"next", 235, 171, {220, 156, 31, 31}};
// The goal box's X spans y 104-127, the tips' and Game Paused's y 120-143 (x 120-143 in all);
// the ring frames both places.
const Hotspot kPopupClose = {"close", 131, 124, {120, 104, 24, 40}};
const Hotspot kPhoneClose = {"close", 238, 18, {224, 3, 28, 30}};

namespace
{
using List = std::vector<Hotspot>;

// The tab's own targets, then the tab icons and the phone.
List Panel(List own)
{
    for (const Hotspot& h : kTabIcon) own.push_back(h);
    own.push_back(kPhoneIcon);
    return own;
}

// Interact: the three actions (each row's arrow; the row is the area) and the strip of nearby
// targets (the selected one in the middle).
const List kInteract = Panel({
    {"action 1", 236, 59, {147, 44, 104, 31}},
    {"action 2", 236, 93, {147, 78, 104, 31}},
    {"action 3", 236, 127, {147, 112, 104, 31}},
    {"target previous", 55, 167, {40, 152, 32, 32}},
    {"target", 89, 167, {72, 152, 34, 32}},
    {"target next", 122, 167, {106, 152, 33, 32}},
});

// Pockets: six slots, the three key items on the right (the vacuum…), the hand (use the selected
// item), the bin (throws the selected slot's item away) and the four counted items.
const List kPockets = Panel({
    {"slot 1", 67, 89, {51, 72, 34, 34}},
    {"slot 2", 106, 89, {90, 72, 34, 34}},
    {"slot 3", 145, 89, {129, 72, 34, 34}},
    {"slot 4", 67, 126, {51, 109, 34, 34}},
    {"slot 5", 106, 126, {90, 109, 34, 34}},
    {"slot 6", 145, 126, {129, 109, 34, 34}},
    {"hand", 191, 107, {174, 90, 35, 35}},
    {"key item 1", 234, 89, {222, 73, 25, 33}},
    {"key item 2", 234, 127, {222, 112, 25, 33}},
    {"key item 3", 234, 165, {222, 150, 25, 33}},
    {"bin", 67, 167, {50, 150, 36, 36}},
    {"counted 1", 110, 170, {97, 154, 26, 34}},
    {"counted 2", 139, 170, {126, 154, 26, 34}},
    {"counted 3", 168, 170, {155, 154, 26, 34}},
    {"counted 4", 197, 170, {184, 154, 26, 34}},
});

// Goals: the list's lines (16 pixels each; a goal takes one to three, any of them selects it).
const List kGoals = Panel({
    {"goal line 1", 166, 58, {86, 52, 160, 16}},
    {"goal line 2", 166, 74, {86, 68, 160, 16}},
    {"goal line 3", 166, 90, {86, 84, 160, 16}},
    {"goal line 4", 166, 106, {86, 100, 160, 16}},
    {"goal line 5", 166, 122, {86, 116, 160, 16}},
    {"goal line 6", 166, 138, {86, 132, 160, 16}},
    {"goal line 7", 166, 154, {86, 148, 160, 16}},
});

// Relationships: four portraits and the arrows that select the previous / next Sim (the up
// arrow shows once the first is not selected).
const List kRelationships = Panel({
    {"previous Sim", 56, 10, {41, 3, 31, 15}},
    {"Sim 1", 56, 41, {40, 25, 33, 33}},
    {"Sim 2", 56, 78, {40, 62, 33, 33}},
    {"Sim 3", 56, 114, {40, 98, 33, 33}},
    {"Sim 4", 56, 150, {40, 134, 33, 33}},
    {"next Sim", 56, 180, {41, 172, 31, 16}},
});

// Options: Save and Quit (the rows; the buttons at their ends), the volumes (the knob at full
// volume: a slider moves only by a drag that starts on its knob, a tap does nothing) and the
// button by the title (no visible effect).
const List kOptions = Panel({
    {"settings", 193, 17, {182, 6, 23, 23}},
    {"save", 57, 44, {44, 29, 167, 30}},
    {"quit", 234, 69, {45, 55, 204, 28}},
    {"music volume", 232, 121, {58, 109, 190, 25}, true},
    {"sound volume", 232, 171, {58, 159, 190, 25}, true},
});

const List kPanelNone = Panel({});

const List kPhone = {
    {"up", 43, 166, {12, 140, 68, 50}},
    {"ok", 127, 166, {96, 140, 63, 50}},
    {"down", 212, 166, {172, 140, 72, 50}},
    kPhoneClose,
};

const List kDialogue = {kDialogueArrow};
const List kPopup = {kPopupClose};

// Give Item: six item slots, the bow (give), the X.
const List kGift = {
    {"item 1", 37, 50, {20, 33, 34, 34}},
    {"item 2", 76, 50, {59, 33, 34, 34}},
    {"item 3", 37, 87, {20, 70, 34, 34}},
    {"item 4", 76, 87, {59, 70, 34, 34}},
    {"item 5", 37, 124, {20, 107, 34, 34}},
    {"item 6", 76, 124, {59, 107, 34, 34}},
    {"give", 127, 85, {110, 68, 35, 35}},
    {"close", 235, 19, {220, 4, 31, 31}},
};

// The social reaction: the three answers.
const List kSocial = {
    {"left", 48, 123, {26, 101, 45, 45}},
    {"middle", 128, 77, {106, 55, 45, 45}},
    {"right", 208, 123, {186, 101, 45, 45}},
};

// Keelhaulin' Cards: the deck, ? (help), X (quit, asks first), the discard pile, the ten hand
// places (a tap lifts the card) and the play arrow (shown after a draw; cards are dragged
// onto it, a tap does nothing).
const List kCards = {
    {"deck", 127, 30, {110, 6, 36, 53}},
    {"help", 178, 17, {155, 5, 46, 21}},
    {"quit", 178, 48, {155, 37, 46, 22}},
    {"discard", 227, 30, {208, 5, 40, 56}},
    {"card 1", 32, 101, {14, 75, 37, 54}},
    {"card 2", 80, 101, {62, 75, 37, 54}},
    {"card 3", 128, 101, {110, 75, 37, 54}},
    {"card 4", 176, 101, {158, 75, 37, 54}},
    {"card 5", 224, 101, {206, 75, 37, 54}},
    {"card 6", 32, 161, {14, 135, 37, 54}},
    {"card 7", 80, 161, {62, 135, 37, 54}},
    {"card 8", 128, 161, {110, 135, 37, 54}},
    {"card 9", 176, 161, {158, 135, 37, 54}},
    {"card 10", 224, 161, {206, 135, 37, 54}},
    {"play", 37, 28, {12, 6, 51, 46}, true},
};

// The shop's catalogue: its six items, the arrows that scroll them, the simoleon button (buys the
// selected item, or sells it when it is one of the six places of what was bought), those six
// places, the X. "Buy" and "Sell" only show the mode (selecting an item sets it): a tap does
// nothing, so they are not targets.
const List kCatalogue = {
    {"item 1", 23, 89, {6, 72, 35, 34}},
    {"item 2", 62, 89, {45, 72, 35, 34}},
    {"item 3", 101, 89, {84, 72, 35, 34}},
    {"item 4", 23, 128, {6, 112, 35, 34}},
    {"item 5", 62, 128, {45, 112, 35, 34}},
    {"item 6", 101, 128, {84, 112, 35, 34}},
    {"scroll up", 46, 166, {32, 153, 29, 29}},
    {"scroll down", 78, 166, {64, 153, 29, 29}},
    {"pay", 147, 128, {130, 111, 34, 34}},
    {"owned 1", 194, 89, {177, 72, 34, 34}},
    {"owned 2", 233, 89, {216, 72, 34, 34}},
    {"owned 3", 194, 128, {177, 112, 34, 34}},
    {"owned 4", 233, 128, {216, 112, 34, 34}},
    {"owned 5", 194, 168, {177, 152, 34, 34}},
    {"owned 6", 233, 168, {216, 152, 34, 34}},
    {"close", 236, 19, {222, 5, 29, 29}},
};

// Build Rooms: three rows (each one's round button), the arrows, the X.
const List kBuild = {
    {"room 1", 48, 68, {34, 54, 184, 30}},
    {"room 2", 48, 103, {34, 89, 184, 30}},
    {"room 3", 48, 138, {34, 124, 184, 30}},
    {"scroll up", 112, 171, {97, 157, 30, 30}},
    {"scroll down", 146, 171, {131, 157, 30, 30}},
    {"close", 236, 171, {221, 157, 30, 30}},
};

// The music mixer's save page (CMusicSaveMenu, the standard text entry, research/m6 code-D 1.6): the
// keyboard's 26 keys (19 x 30 cells from 6,74, nine a row; the page's letters, digits or signs),
// its backspace, the three pages (ABC, abc, 123), the three slots' save buttons (round, 32 x 32,
// their middles 235, 57 / 109 / 160 as drawn) and the X (235,18: back to the mixer).
const List kMixerSave = {
    {"key 1", 15, 89, {6, 74, 19, 30}},
    {"key 2", 34, 89, {25, 74, 19, 30}},
    {"key 3", 53, 89, {44, 74, 19, 30}},
    {"key 4", 72, 89, {63, 74, 19, 30}},
    {"key 5", 91, 89, {82, 74, 19, 30}},
    {"key 6", 110, 89, {101, 74, 19, 30}},
    {"key 7", 129, 89, {120, 74, 19, 30}},
    {"key 8", 148, 89, {139, 74, 19, 30}},
    {"key 9", 167, 89, {158, 74, 19, 30}},
    {"key 10", 15, 119, {6, 104, 19, 30}},
    {"key 11", 34, 119, {25, 104, 19, 30}},
    {"key 12", 53, 119, {44, 104, 19, 30}},
    {"key 13", 72, 119, {63, 104, 19, 30}},
    {"key 14", 91, 119, {82, 104, 19, 30}},
    {"key 15", 110, 119, {101, 104, 19, 30}},
    {"key 16", 129, 119, {120, 104, 19, 30}},
    {"key 17", 148, 119, {139, 104, 19, 30}},
    {"key 18", 167, 119, {158, 104, 19, 30}},
    {"key 19", 15, 149, {6, 134, 19, 30}},
    {"key 20", 34, 149, {25, 134, 19, 30}},
    {"key 21", 53, 149, {44, 134, 19, 30}},
    {"key 22", 72, 149, {63, 134, 19, 30}},
    {"key 23", 91, 149, {82, 134, 19, 30}},
    {"key 24", 110, 149, {101, 134, 19, 30}},
    {"key 25", 129, 149, {120, 134, 19, 30}},
    {"key 26", 148, 149, {139, 134, 19, 30}},
    {"backspace", 94, 174, {80, 163, 28, 22}},
    {"page ABC", 95, 53, {79, 37, 32, 32}},
    {"page abc", 129, 53, {113, 37, 32, 32}},
    {"page 123", 163, 53, {147, 37, 32, 32}},
    {"slot 1", 235, 57, {219, 41, 32, 32}},
    {"slot 2", 235, 109, {219, 93, 32, 32}},
    {"slot 3", 235, 160, {219, 144, 32, 32}},
    {"close", 235, 18, {219, 2, 32, 32}},
};

const List kEmpty;
}

const std::vector<Hotspot>& HotspotsFor(ScreenState s)
{
    switch (s.screen)
    {
    case Screen::Panel:
        switch (s.tab)
        {
        case Tab::Interact: return kInteract;
        case Tab::Pockets: return kPockets;
        case Tab::Goals: return kGoals;
        case Tab::Relationships: return kRelationships;
        case Tab::Options: return kOptions;
        case Tab::None: return kPanelNone;
        }
        return kPanelNone;
    case Screen::Phone: return kPhone;
    case Screen::Dialogue: return kDialogue;
    case Screen::Popup: return kPopup;
    case Screen::Gift: return kGift;
    case Screen::Social: return kSocial;
    case Screen::Cards: return kCards;
    case Screen::Catalogue: return kCatalogue;
    case Screen::Build: return kBuild;
    case Screen::MixerSave: return kMixerSave;
    case Screen::None: case Screen::Vacuum: case Screen::Unknown:
    case Screen::FrontEnd: case Screen::CreateASim: case Screen::SaveSlots: case Screen::Loading: case Screen::Drencher: case Screen::MooGoo: case Screen::Crime:
    case Screen::Autopsy: case Screen::Detector: case Screen::Art: case Screen::Scratch: case Screen::Mixer: case Screen::Trading: return kEmpty;
    }
    return kEmpty;
}

int Neighbour(const std::vector<Hotspot>& h, int from, int dx, int dy)
{
    if (from < 0 || from >= (int)h.size() || (dx == 0 && dy == 0)) return from;
    // Centres doubled, to stay in whole numbers.
    auto cx = [&](int i) { return 2 * h[i].area.x + h[i].area.w; };
    auto cy = [&](int i) { return 2 * h[i].area.y + h[i].area.h; };
    const double len = std::sqrt(double(dx * dx + dy * dy));
    const double cos60 = 0.5;
    int best = from;
    double bestDist = 0, bestAlong = 0;
    for (int i = 0; i < (int)h.size(); i++)
    {
        if (i == from) continue;
        const double vx = cx(i) - cx(from), vy = cy(i) - cy(from);
        const double dist = std::sqrt(vx * vx + vy * vy);
        if (dist == 0) continue;
        const double along = (vx * dx + vy * dy) / len; // the distance along the direction
        if (along <= dist * cos60 + 1e-9) continue;     // more than 60 degrees off (or behind)
        // The nearest; between two as near, the one more in line.
        if (best == from || dist < bestDist - 1e-9 || (std::abs(dist - bestDist) <= 1e-9 && along > bestAlong))
        {
            best = i;
            bestDist = dist;
            bestAlong = along;
        }
    }
    return best;
}

int SaveSlotAt(int focus)
{
    const std::vector<Hotspot>& h = HotspotsFor({Screen::MixerSave, Tab::None});
    return focus >= 0 && focus < (int)h.size() && !strncmp(h[focus].name, "slot ", 5) ? atoi(h[focus].name + 5) - 1 : -1;
}
}
