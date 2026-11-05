// Checks the HUD rule (screens.*) on synthetic top screens: the status bar must be found
// whatever tint the Sanity bar gives the strip's right part (red, yellow seen in the game; green
// and silver not seen yet), and not on a screen without the strip.
// The rows' colours were measured headless (CLAUDE.md, traps); no game picture is used.
//   ninja -C build screens_test && ./build/screens_test
#include <cstdio>
#include <vector>

#include "screens.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void Row(std::vector<u32>& s, int y, int x0, int x1, u32 rgb)
{
    for (int x = x0; x <= x1; x++) s[y * SW + x] = 0xFF000000 | rgb;
}

// A top screen with the status bar's rows: the strip's top edge (row 176), the bottom border
// (row 190: #415969 under the clock and the money, `barTint` under the Sanity bar's box from x
// 144) and the row under it (191); the world above is a dull green.
static std::vector<u32> Top(u32 barTint)
{
    std::vector<u32> s(SW * 192, 0xFF6D8E59);
    Row(s, 176, 5, 252, 0x597192);
    Row(s, 190, 3, 143, 0x415969);
    Row(s, 190, 144, 251, barTint);
    Row(s, 191, 3, 251, 0x384961);
    return s;
}

static void hud_whatever_the_bar()
{
    CHECK(HudVisible(Top(0x415969).data()));   // red bar (the town)
    CHECK(HudVisible(Top(0x495969).data()));   // yellow bar (the complete save's casino)
    CHECK(HudVisible(Top(0x416169).data()));   // a green tint (not seen)
    CHECK(HudVisible(Top(0x515D6D).data()));   // a silver tint (not seen)
    CHECK(HudVisible(Top(0x302018).data()));   // any other
}

static void no_hud_without_the_strip()
{
    std::vector<u32> none(SW * 192, 0xFF6D8E59);
    CHECK(!HudVisible(none.data()));
    // Only the border's left part, as a dark screen's row might be.
    std::vector<u32> part(SW * 192, 0xFF000000);
    Row(part, 190, 0, 255, 0x415969);
    CHECK(!HudVisible(part.data()));
    // The strip without its top edge, or without the row under the border.
    std::vector<u32> s = Top(0x415969);
    Row(s, 176, 0, 255, 0x000000);
    CHECK(!HudVisible(s.data()));
    s = Top(0x415969);
    Row(s, 191, 0, 255, 0x000000);
    CHECK(!HudVisible(s.data()));
    // The clock's box broken (another screen's row 190).
    s = Top(0x415969);
    Row(s, 190, 40, 60, 0x283038);
    CHECK(!HudVisible(s.data()));
}

// Both screens black (the game's loads: every pixel exactly 0 after a fade of 60, 28, 0): any
// pixel brighter than the threshold on either screen is not a load.
static void black_screens()
{
    std::vector<u32> top(SW * 192, 0xFF000000), bottom(SW * 192, 0xFF000000);
    CHECK(ScreensBlack(top.data(), bottom.data()));
    top[100 * SW + 37] = 0xFF080808;
    CHECK(ScreensBlack(top.data(), bottom.data()));
    bottom[191 * SW + 255] = 0xFF001C00; // the fade's last step, one pixel
    CHECK(!ScreensBlack(top.data(), bottom.data()));
    bottom[191 * SW + 255] = 0xFF000000;
    top[3 * SW + 1] = 0xFF1C0000;
    CHECK(!ScreensBlack(top.data(), bottom.data()));
}

int main()
{
    hud_whatever_the_bar();
    no_hud_without_the_strip();
    black_screens();
    if (failures) { fprintf(stderr, "screens_test: %d failures\n", failures); return 1; }
    printf("screens_test: ok\n");
    return 0;
}
