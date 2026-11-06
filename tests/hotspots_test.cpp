// Checks the touch targets of the game's screens (hotspots.*): Neighbour moves the focus to the
// nearest target in a direction, within 60 degrees of it; every table lies on the DS screen and
// the focus can reach each of its targets.
// Where each target is and what tapping it does was measured headless (scripts/hotspots/).
//   ninja -C build hotspots_test && ./build/hotspots_test
#include <cstdio>
#include <vector>

#include "hotspots.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Hotspot At(int x, int y) { return {"p", x, y, {x - 5, y - 5, 11, 11}}; }

// 0 1 2
// 3 4 5
// 6 7 8
static std::vector<Hotspot> Grid()
{
    std::vector<Hotspot> h;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) h.push_back(At(40 + c * 40, 40 + r * 40));
    return h;
}

static void neighbour_moves_in_direction()
{
    std::vector<Hotspot> g = Grid();
    CHECK(Neighbour(g, 4, 1, 0) == 5);
    CHECK(Neighbour(g, 4, -1, 0) == 3);
    CHECK(Neighbour(g, 4, 0, -1) == 1);
    CHECK(Neighbour(g, 4, 0, 1) == 7);
    CHECK(Neighbour(g, 0, 1, 0) == 1);
    CHECK(Neighbour(g, 3, 0, 1) == 6);
    // Nothing further: the focus stays.
    CHECK(Neighbour(g, 1, 0, -1) == 1);
    CHECK(Neighbour(g, 0, 0, -1) == 0);
    CHECK(Neighbour(g, 5, 1, 0) == 5);
    CHECK(Neighbour(g, 8, 0, 1) == 8);
    // No targets, or a focus out of the table: unchanged.
    CHECK(Neighbour({}, 0, 1, 0) == 0);
    CHECK(Neighbour(g, -1, 1, 0) == -1);
}

static void neighbour_cone()
{
    // From the origin going right: a point far right but 70 degrees off is not taken; one 50
    // degrees off is, and a nearer one 20 degrees off wins over it.
    std::vector<Hotspot> h = {At(20, 20), At(20 + 200, 20 + 549)}; // tan 70° ≈ 2.747
    CHECK(Neighbour(h, 0, 1, 0) == 0);
    h.push_back(At(20 + 100, 20 + 119)); // 50°
    CHECK(Neighbour(h, 0, 1, 0) == 2);
    h.push_back(At(20 + 120, 20 + 44)); // 20°, nearer
    CHECK(Neighbour(h, 0, 1, 0) == 3);
    // Navigation goes by the areas' centres, not the tap points: a row whose button sits at
    // its far end is still straight below.
    std::vector<Hotspot> rows = {{"a", 20, 10, {10, 0, 200, 20}}, {"b", 200, 40, {10, 30, 200, 20}}};
    CHECK(Neighbour(rows, 0, 0, 1) == 1);
    CHECK(Neighbour(rows, 1, 0, -1) == 0);
}

static bool Inside(const Rect& r) { return r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 && r.x + r.w <= SW && r.y + r.h <= SH; }
static bool Holds(const Rect& r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

static void tables_inside_screen()
{
    std::vector<ScreenState> all;
    for (Screen s : {Screen::None, Screen::Phone, Screen::Dialogue, Screen::Popup, Screen::Gift, Screen::Social,
                     Screen::Vacuum, Screen::Cards, Screen::Catalogue, Screen::Build, Screen::FrontEnd,
                     Screen::CreateASim, Screen::SaveSlots, Screen::Drencher, Screen::Unknown, Screen::MixerSave})
        all.push_back({s, Tab::None});
    for (Tab t : {Tab::None, Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options})
        all.push_back({Screen::Panel, t});
    for (ScreenState s : all)
        for (const Hotspot& h : HotspotsFor(s))
        {
            if (!(Inside(h.area) && Holds(h.area, h.x, h.y)))
                fprintf(stderr, "%s %s: %d,%d in %d,%d %dx%d\n", ScreenStateName(s).c_str(), h.name, h.x, h.y,
                        h.area.x, h.area.y, h.area.w, h.area.h);
            CHECK(Inside(h.area));
            CHECK(Holds(h.area, h.x, h.y));
            CHECK(h.name && h.name[0]);
        }
    for (const Hotspot& h : kTabIcon) { CHECK(Inside(h.area)); CHECK(Holds(h.area, h.x, h.y)); }
    for (const Hotspot* h : {&kPhoneIcon, &kDialogueArrow, &kPopupClose, &kPhoneClose})
    {
        CHECK(Inside(h->area));
        CHECK(Holds(h->area, h->x, h->y));
    }
}

static int Count(ScreenState s) { return (int)HotspotsFor(s).size(); }
static bool Has(ScreenState s, const Hotspot& k)
{
    for (const Hotspot& h : HotspotsFor(s))
        if (h.x == k.x && h.y == k.y) return true;
    return false;
}

static void tables_have_their_targets()
{
    CHECK(HotspotsFor({Screen::None, Tab::None}).empty());
    CHECK(HotspotsFor({Screen::Unknown, Tab::None}).empty());
    CHECK(HotspotsFor({Screen::Vacuum, Tab::None}).empty()); // the stick's hand (activities.h)
    // Every tab: its own targets, then the five tab icons and the phone.
    for (Tab t : {Tab::None, Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options})
    {
        for (const Hotspot& k : kTabIcon) CHECK(Has({Screen::Panel, t}, k));
        CHECK(Has({Screen::Panel, t}, kPhoneIcon));
    }
    CHECK(Count({Screen::Panel, Tab::None}) == 6);
    CHECK(Count({Screen::Panel, Tab::Pockets}) == 6 + 9 + 1 + 4 + 1);   // 9 slots, hand, 4 side items, bin
    CHECK(Count({Screen::Panel, Tab::Relationships}) == 6 + 4 + 2);     // portraits, up and down
    CHECK(Count({Screen::Phone, Tab::None}) == 4);
    CHECK(Has({Screen::Phone, Tab::None}, kPhoneClose));
    CHECK(Count({Screen::Dialogue, Tab::None}) == 1);
    CHECK(Count({Screen::MixerSave, Tab::None}) == 26 + 1 + 3 + 3 + 1);   // the keys, backspace, the pages, the slots, the X
    CHECK(HotspotsFor({Screen::Scratch, Tab::None}).empty() && HotspotsFor({Screen::Mixer, Tab::None}).empty());   // (their pointer)
    CHECK(Has({Screen::Dialogue, Tab::None}, kDialogueArrow));
    CHECK(Count({Screen::Popup, Tab::None}) == 1);
    CHECK(Has({Screen::Popup, Tab::None}, kPopupClose));
    CHECK(Count({Screen::Social, Tab::None}) == 3);
    CHECK(Count({Screen::Cards, Tab::None}) == 1 + 1 + 1 + 10 + 1 + 1); // deck, ?, X, hand, discard, play arrow
    CHECK(Count({Screen::Gift, Tab::None}) == 6 + 1 + 1);
    // The measured spots the rest of the frontend relies on.
    CHECK(kTabIcon[1].x == 16 && kTabIcon[1].y == 57);
    CHECK(kPopupClose.x == 131 && kPopupClose.y == 124);
}

// From any target of any table, the four directions reach every other one.
static void every_target_reachable()
{
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, -1}, {0, 1}};
    std::vector<ScreenState> all;
    for (Screen s : {Screen::Phone, Screen::Dialogue, Screen::Popup, Screen::Gift, Screen::Social, Screen::Cards,
                     Screen::Catalogue, Screen::Build})
        all.push_back({s, Tab::None});
    for (Tab t : {Tab::None, Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options})
        all.push_back({Screen::Panel, t});
    for (ScreenState s : all)
    {
        const std::vector<Hotspot>& h = HotspotsFor(s);
        for (int start = 0; start < (int)h.size(); start++)
        {
            std::vector<bool> seen(h.size(), false);
            std::vector<int> todo = {start};
            seen[start] = true;
            for (size_t k = 0; k < todo.size(); k++)
                for (const auto& d : dirs)
                {
                    int j = Neighbour(h, todo[k], d[0], d[1]);
                    if (!seen[j]) { seen[j] = true; todo.push_back(j); }
                }
            if (todo.size() != h.size()) fprintf(stderr, "%s: not all reachable from %s\n", ScreenStateName(s).c_str(), h[start].name);
            CHECK(todo.size() == h.size());
        }
    }
}

int main()
{
    neighbour_moves_in_direction();
    neighbour_cone();
    tables_inside_screen();
    tables_have_their_targets();
    every_target_reachable();
    if (failures) { fprintf(stderr, "hotspots_test: %d failure(s)\n", failures); return 1; }
    puts("hotspots_test: ok");
    return 0;
}
