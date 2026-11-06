// Checks the layouts' touch mapping: every DS touch pixel, drawn in each layout at several window
// sizes (HiDPI too), maps back to itself from the centre of where it was drawn; the top screen
// is never touchable, FOCUS's panel wins over the world under it, and a drag past the touch
// screen is held to its edges.
//   ninja -C build layout_test && ./build/layout_test
#include <climits>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "compose.h"
#include "console.h"
#include "hotspots.h"
#include "layout.h"
#include "port/build.h"
#include "port/catalogue.h"
#include "port/dialogue.h"
#include "port/gift.h"
#include "port/goals.h"
#include "port/note.h"
#include "port/options.h"
#include "port/phone.h"
#include "port/popup.h"
#include "port/relations.h"
#include "port/interact.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/kit.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void TestLayout(Layout l, bool mainIsTop, float aspect, int ow, int oh)
{
    const Placement p = Place(l, mainIsTop, aspect);
    const CanvasFit fit = CanvasFit::For(p, ow, oh);
    // Each DS pixel from the centre of its drawn area.
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            const float cx = p.bottom.x + (x + 0.5f) * p.bottom.w / SW, cy = p.bottom.y + (y + 0.5f) * p.bottom.h / SH;
            const float px = fit.ox + cx * fit.scale, py = fit.oy + cy * fit.scale;
            int tx = -1, ty = -1;
            const bool hit = TouchFromOutput(p, fit, px, py, tx, ty);
            if (!hit || tx != x || ty != y)
            {
                fprintf(stderr, "layout %s main %d aspect %.2f out %dx%d: pixel %d,%d -> %s %d,%d\n", LayoutName(l), mainIsTop, aspect, ow, oh, x, y, hit ? "hit" : "miss", tx, ty);
                failures++;
                return;
            }
            CHECK(!OverTop(p, fit, px, py));
            int cxT = -1, cyT = -1;
            TouchClamped(p, fit, px, py, cxT, cyT);
            CHECK(cxT == x && cyT == y);
        }
    // A drag past the touch screen's edges is held to them: corners and the middle of each side.
    {
        int tx, ty;
        const float l = fit.ox + (p.bottom.x - 50) * fit.scale, r = fit.ox + (p.bottom.x + p.bottom.w + 50) * fit.scale;
        const float t = fit.oy + (p.bottom.y - 50) * fit.scale, b = fit.oy + (p.bottom.y + p.bottom.h + 50) * fit.scale;
        const float mx = fit.ox + (p.bottom.x + p.bottom.w / 2.0f) * fit.scale, my = fit.oy + (p.bottom.y + p.bottom.h / 2.0f) * fit.scale;
        TouchClamped(p, fit, l, t, tx, ty); CHECK(tx == 0 && ty == 0);
        TouchClamped(p, fit, r, b, tx, ty); CHECK(tx == SW - 1 && ty == SH - 1);
        TouchClamped(p, fit, r, my, tx, ty); CHECK(tx == SW - 1 && abs(ty - SH / 2) <= 1);
        TouchClamped(p, fit, mx, t, tx, ty); CHECK(abs(tx - SW / 2) <= 1 && ty == 0);
    }
    // The first output pixel of the bottom screen, and the first one past its right edge.
    int tx, ty;
    CHECK(TouchFromOutput(p, fit, fit.ox + p.bottom.x * fit.scale + 0.5f, fit.oy + p.bottom.y * fit.scale + 0.5f, tx, ty) && tx == 0 && ty == 0);
    CHECK(!TouchFromOutput(p, fit, fit.ox + (p.bottom.x + p.bottom.w) * fit.scale + 0.5f, fit.oy + p.bottom.y * fit.scale + 0.5f, tx, ty));
    // The top screen's own area (outside the panel) is never a touch, and is "over top".
    const float topX = fit.ox + (p.top.x + 2) * fit.scale, topY = fit.oy + (p.top.y + 2) * fit.scale;
    if (!p.Contains(p.bottom, p.top.x + 2, p.top.y + 2))
    {
        CHECK(!TouchFromOutput(p, fit, topX, topY, tx, ty));
        CHECK(OverTop(p, fit, topX, topY));
    }
    // Outside the canvas: nothing.
    CHECK(!TouchFromOutput(p, fit, fit.ox - 1, fit.oy - 1, tx, ty));
    CHECK(!OverTop(p, fit, fit.ox - 1, fit.oy - 1));
}

// --- Compositions: pieces of the DS screens drawn at rectangles, and the mouse through them.

// Every DS bottom pixel's centre, drawn through the composition and mapped back, is itself.
static void TestComposeRoundtrip(Layout l, bool mainIsTop, float aspect, int ow, int oh)
{
    const int topW = (int)(SH * aspect + 0.5f);
    const Composition c = FromPlacement(Place(l, mainIsTop, aspect), topW, SH);
    const CanvasFit fit = CanvasFit::For(c.cw, c.ch, ow, oh);
    const Piece* b = nullptr;
    for (const Piece& q : c.pieces) if (q.src == Src::Bottom) b = &q;
    CHECK(b);
    if (!b) return;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            const float cx = b->to.x + (x + 0.5f) * b->to.w / b->from.w, cy = b->to.y + (y + 0.5f) * b->to.h / b->from.h;
            const float px = fit.ox + cx * fit.scale, py = fit.oy + cy * fit.scale;
            int tx = -1, ty = -1;
            if (!TouchFromComposition(c, fit, px, py, tx, ty) || tx != x || ty != y)
            {
                fprintf(stderr, "compose %s main %d aspect %.2f out %dx%d: pixel %d,%d -> %d,%d\n", LayoutName(l), mainIsTop, aspect, ow, oh, x, y, tx, ty);
                failures++;
                return;
            }
        }
}

static Composition TwoBottomPieces()
{
    Composition c {1400, 600, {}, {}};
    c.pieces.push_back({Src::Bottom, {0, 0, SW, 96}, {0, 0, SW * 3, 96 * 3}});
    c.pieces.push_back({Src::Bottom, {0, 96, SW, 96}, {900, 0, SW * 2, 96 * 2}});
    return c;
}

// --- CONSOLE: every DS pixel of every touchable bottom piece of every composition maps back to
// itself from the centre of where it is drawn (click and drag), never as the camera.
static std::vector<ScreenState> AllStates()
{
    std::vector<ScreenState> v;
    for (Screen sc : {Screen::None, Screen::Panel, Screen::Phone, Screen::Dialogue, Screen::Popup, Screen::Gift, Screen::Social,
                      Screen::Vacuum, Screen::Cards, Screen::Catalogue, Screen::Build, Screen::FrontEnd,
                      Screen::CreateASim, Screen::SaveSlots, Screen::ItemHand, Screen::Unknown, Screen::Loading})
        if (sc == Screen::Panel)
            for (Tab t : {Tab::None, Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options}) v.push_back({sc, t});
        else v.push_back({sc, Tab::None});
    return v;
}

// The status bar's words, made up (no game text here): the game's placeholders in its formats.
static HudStrings TestStrings()
{
    HudStrings h;
    h.ok = true;
    h.clock = "@hh:@mm @am";
    h.am = "am";
    h.pm = "pm";
    h.money = "$@1";
    h.label = "Meter";
    return h;
}
static HudState TestHud()
{
    HudState h;
    h.valid = true;
    h.money = 14843;
    h.hour = 15;
    h.minute = 37;
    h.sanity = 150;
    h.sanityMax = 225;
    h.tier = 2;
    return h;
}

// given: test that composition (the HD conversation's, built apart) instead of BuildConsole's.
static void TestConsoleRoundtrip(ScreenState s, bool menuOpen, int topW, int topH, int ow, int oh, bool withHud, const Composition* given = nullptr)
{
    const HudState hud = TestHud();
    const HudStrings strings = TestStrings();
    const Composition c = given ? *given : withHud ? BuildConsole(s, menuOpen, -1, topW, topH, &hud, &strings) : BuildConsole(s, menuOpen, -1, topW, topH);
    const CanvasFit fit = CanvasFit::For(c.cw, c.ch, ow, oh);
    int bottoms = 0;
    for (int i = 0; i < (int)c.pieces.size(); i++)
    {
        const Piece& q = c.pieces[i];
        if (q.src != Src::Bottom || !q.touch) continue;
        bottoms++;
        CHECK(q.from.x >= 0 && q.from.y >= 0 && q.from.x + q.from.w <= SW && q.from.y + q.from.h <= SH);
        CHECK(q.to.x >= 0 && q.to.y >= 0 && q.to.x + q.to.w <= c.cw && q.to.y + q.to.h <= c.ch);
        for (int y = q.from.y; y < q.from.y + q.from.h; y++)
            for (int x = q.from.x; x < q.from.x + q.from.w; x++)
            {
                const float cx = q.to.x + (x - q.from.x + 0.5f) * q.to.w / q.from.w, cy = q.to.y + (y - q.from.y + 0.5f) * q.to.h / q.from.h;
                const float px = fit.ox + cx * fit.scale, py = fit.oy + cy * fit.scale;
                int tx = -1, ty = -1, idx = -1, dx = -1, dy = -1;
                const bool hit = TouchFromComposition(c, fit, px, py, tx, ty, &idx);
                TouchClampedComposition(c, i, fit, px, py, dx, dy);
                if (!hit || tx != x || ty != y || idx != i || dx != x || dy != y || OverTopComposition(c, fit, px, py))
                {
                    fprintf(stderr, "console %s menu %d top %dx%d out %dx%d: piece %d pixel %d,%d -> %s %d,%d (piece %d), drag %d,%d\n",
                            ScreenStateName(s).c_str(), menuOpen, topW, topH, ow, oh, i, x, y, hit ? "hit" : "miss", tx, ty, idx, dx, dy);
                    failures++;
                    return;
                }
            }
        // A drag that began on the piece and left it is held to the piece's own rectangle.
        int tx, ty;
        TouchClampedComposition(c, i, fit, -1000, -1000, tx, ty);
        CHECK(tx == q.from.x && ty == q.from.y);
        TouchClampedComposition(c, i, fit, 1e6f, 1e6f, tx, ty);
        CHECK(tx == q.from.x + q.from.w - 1 && ty == q.from.y + q.from.h - 1);
    }
    // A black load (Loading) shows nothing at all: no piece to touch.
    CHECK(s.screen == Screen::Loading ? bottoms == 0 : bottoms > 0);
}

// Every enabled item of v reachable from every other by the pad's moves, as the navigator makes
// them (MenuMove; Left / Right that run an action instead are no moves).
static bool AllReachable(const MenuView& v)
{
    const int n = (int)v.items.size();
    for (int from = 0; from < n; from++)
    {
        if (!v.items[from].enabled) continue;
        std::vector<bool> seen(n, false);
        std::vector<int> todo {from};
        seen[from] = true;
        while (!todo.empty())
        {
            const int i = todo.back();
            todo.pop_back();
            for (int d = 0; d < 4; d++)
            {
                if (d >= 2 && (!(d == 2 ? v.items[i].left : v.items[i].right).empty() ||
                               !v.buttons[(int)(d == 2 ? MenuButton::Left : MenuButton::Right)].empty()))
                    continue;
                const int to = MenuMove(v, i, d);
                if (to >= 0 && !seen[to]) { seen[to] = true; todo.push_back(to); }
            }
        }
        for (int i = 0; i < n; i++)
            if (v.items[i].enabled && !seen[i])
            {
                fprintf(stderr, "menu: item %d not reachable from %d\n", i, from);
                return false;
            }
    }
    return true;
}

int main()
{
    // CONSOLE's compositions.
    for (ScreenState s : AllStates())
        for (bool menuOpen : {false, true})
            for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                    for (bool withHud : {false, true})
                        TestConsoleRoundtrip(s, menuOpen, top.first, top.second, o.first, o.second, withHud);
    // Where the mockup puts things.
    {
        const Composition town = BuildConsole({Screen::Panel, Tab::Goals}, false, -1, 341, 192);
        CHECK(town.cw == kConsoleW && town.ch == kConsoleH);
        CHECK(town.pieces.size() == 3 && town.pieces[0].src == Src::Top && town.pieces[0].to.w == kConsoleW && town.pieces[0].to.h == kConsoleH);
        CHECK(town.pieces[2].from.x == 0 && town.pieces[2].from.y == 6 && town.pieces[2].from.w == 35 && town.pieces[2].to.w == 70 && town.pieces[2].to.h == 368);
        CHECK(town.overlays.size() == 1 && town.overlays[0].argb == kFrameArgb && town.overlays[0].borderArgb == kOutlineArgb && town.overlays[0].border == 6);
        const Rect f = town.overlays[0].to;
        CHECK(f.y + f.h / 2 == kConsoleH / 2 && f.x + f.w <= kConsoleW);
        // The DS's 4:3 world: 960 wide, centred.
        const Composition t43 = BuildConsole({Screen::Panel, Tab::Interact}, false, -1, SW, SH);
        CHECK(t43.pieces[0].to.x == 160 && t43.pieces[0].to.w == 960);
        // The phone is a menu: the touch screen 3x over the dimmed world.
        const Composition phone = BuildConsole({Screen::Phone, Tab::None}, false, -1, SW, SH);
        CHECK(phone.pieces.size() == 2 && !phone.pieces[0].touch && phone.pieces[1].to.x == 256 && phone.pieces[1].to.w == SW * 3);
        const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, -1, SW, SH);
        CHECK(dlg.pieces[0].from.h == 176);
        CHECK(dlg.overlays[0].to.x == 110 && dlg.overlays[0].to.y == 456 && dlg.overlays[0].to.w == 1060);
        CHECK(dlg.pieces.size() == 5);
        const Composition act = BuildConsole({Screen::Vacuum, Tab::None}, false, -1, SW, SH);
        CHECK(act.pieces[0].src == Src::Bottom && act.pieces[0].to.x == 36 && act.pieces[0].to.y == 68 && act.pieces[0].to.w == SW * 3);
        CHECK(act.pieces[1].src == Src::Top && act.pieces[1].to.x == 858 && act.pieces[1].to.w == 384 && act.pieces[1].to.h == 288);
    }
    // The HD status bar: the game's bar cropped off the world (its last 16 of 192 rows, whatever
    // the picture's size), the world keeping its shape; the bar at the bottom left of the world,
    // inside it and the canvas, below the menu's frame; under the small world of the activities
    // that keep the HUD, not the cards'; nothing without the game's words or values.
    {
        const HudState hud = TestHud();
        const HudStrings strings = TestStrings();
        for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
        {
            const int rows = HudCropRows(top.second);
            for (ScreenState st : {ScreenState {Screen::Panel, Tab::Goals}, ScreenState {Screen::Phone, Tab::None}, ScreenState {Screen::Popup, Tab::None},
                                   ScreenState {Screen::Gift, Tab::None}, ScreenState {Screen::SaveSlots, Tab::None}})
                for (bool menuOpen : {false, true})
                {
                    const Composition c = BuildConsole(st, menuOpen, -1, top.first, top.second, &hud, &strings);
                    const Piece& w = c.pieces[0];
                    // The whole width of the picture (the crop never cuts its sides), at its shape
                    // (within a pixel's rounding): 720 high at 4:3, the canvas's width and
                    // top-aligned when wider (widescreen: 1280 x 661), the bar below it.
                    CHECK(w.src == Src::Top && w.from.x == 0 && w.from.y == 0 && w.from.w == top.first && w.from.h == rows);
                    CHECK(std::abs(w.to.w * w.from.h - w.to.h * w.from.w) <= w.from.h + w.from.w);
                    CHECK(w.to.y == 0 && w.to.x >= 0 && w.to.x + w.to.w <= kConsoleW && w.to.h <= kConsoleH);
                    CHECK(top.first * 3 == top.second * 4 ? w.to.h == kConsoleH : (w.to.w == kConsoleW && w.to.h == 661));
                    CHECK(!c.ui.empty());
                    const Rect b = c.ui[0].to;
                    CHECK(b.y == kStatusBarY && b.h == kStatusBarH && b.x >= std::max(w.to.x, 0) && b.x + b.w <= std::min(w.to.x + w.to.w, kConsoleW));
                    CHECK(b.y > kMenuTo.y + kMenuTo.h + 6);
                    for (const ui::UiItem& it : c.ui)
                        CHECK(it.to.x >= b.x && it.to.y >= b.y && it.to.x + it.to.w <= b.x + b.w && it.to.y + it.to.h <= b.y + b.h);
                }
            for (Screen sc : {Screen::Social, Screen::Vacuum, Screen::Catalogue, Screen::Build, Screen::Cards})
            {
                const Composition c = BuildConsole({sc, Tab::None}, false, -1, top.first, top.second, &hud, &strings);
                const Piece& w = c.pieces[1];
                CHECK(w.src == Src::Top && w.from.h == (sc == Screen::Cards ? top.second : rows));
                bool bar = false;
                for (const ui::UiItem& it : c.ui)
                    if (it.kind == ui::UiItem::Panel && it.to.h == kStatusBarH)
                    {
                        bar = true;
                        CHECK(it.to.y >= w.to.y + w.to.h + 6 && it.to.y + it.to.h + 8 <= kHintBox.y && it.to.x + it.to.w <= kConsoleW);
                    }
                CHECK(bar == (sc != Screen::Cards));
            }
            // The conversation as before (its own box hides the bar), and without the words or
            // without RAM's values, milestone 1's world, the game's HUD in it.
            CHECK(BuildConsole({Screen::Dialogue, Tab::None}, false, -1, top.first, top.second, &hud, &strings).ui.empty());
            HudStrings none;
            const Composition m1 = BuildConsole({Screen::Panel, Tab::Goals}, false, -1, top.first, top.second, &hud, &none);
            CHECK(m1.pieces[0].from.h == top.second && m1.ui.empty());
            CHECK(BuildConsole({Screen::Panel, Tab::Goals}, false, -1, top.first, top.second, nullptr, &strings).pieces[0].from.h == top.second);
        }
        // The values: 12-hour clock, the money grouped, the simoleon sign for the game's '$'.
        std::string clock, money;
        float fill = 0;
        CHECK(FormatHud(hud, strings, clock, money, fill) && clock == "3:37 pm" && money == "\xC2\xA7" "14,843" && std::abs(fill - 150.0f / 225) < 1e-6f);
        HudState h = hud;
        h.hour = 0; h.minute = 5;
        CHECK(FormatHud(h, strings, clock, money, fill) && clock == "12:05 am");
        h.hour = 12;
        CHECK(FormatHud(h, strings, clock, money, fill) && clock == "12:05 pm");
        CHECK(GroupThousands(0) == "0" && GroupThousands(999) == "999" && GroupThousands(1000) == "1,000" && GroupThousands(12345678) == "12,345,678");
        // Escapes and unknown placeholders in the words never show.
        HudStrings odd = strings;
        odd.clock = "{F012}@hh:@mm @am @zz";
        odd.money = "@1\xC2\xA0$";
        CHECK(FormatHud(hud, odd, clock, money, fill) && clock == "3:37 pm" && money == "14,843\xC2\xA0\xC2\xA7");
        // Garbage (a load's): not valid, out of range, max 0, huge: nothing drawn, no division.
        std::vector<HudState> bad;
        for (int i = 0; i < 9; i++) bad.push_back(hud);
        bad[0].valid = false;
        bad[1].hour = 24;
        bad[2].minute = -1;
        bad[3].tier = 7;
        bad[4].sanityMax = 0;
        bad[5].money = -5;
        bad[6].money = INT_MAX;
        bad[7].sanity = -1;
        bad[8].hour = INT_MIN;
        for (const HudState& g : bad)
        {
            Composition c {kConsoleW, kConsoleH, {}, {}};
            const Rect r = AddStatusBar(c, g, strings, 100, 600, 600);
            CHECK(c.ui.empty() && r.w == 0);
        }
        // Valid but extreme (sanity over max, the most money, long words, a narrow box): every
        // item stays inside the bar's box, every drawn pixel inside it and its soft drop shadow
        // (the kit's panel shadow: 3 + 4 canvas pixels down, its 4-pixel blur around).
        HudState x = hud;
        x.sanity = 100000; x.sanityMax = 1; x.money = 99999999; x.tier = 3;
        HudStrings longWords = strings;
        longWords.label = "Averyveryverylongmeterlabelthatcannotfit";
        longWords.am = longWords.pm = "o'clock in the afternoon";
        for (int maxW : {600, 300, 200, 120, 104})
        {
            Composition c {kConsoleW, kConsoleH, {}, {}};
            const Rect b = AddStatusBar(c, x, longWords, 40, 40, maxW);
            CHECK(!c.ui.empty() && b.w <= maxW && b.w > 0);
            for (const ui::UiItem& it : c.ui)
                CHECK(it.to.x >= b.x && it.to.y >= b.y && it.to.x + it.to.w <= b.x + b.w && it.to.y + it.to.h <= b.y + b.h);
            const int W = 800, H = 160;
            std::vector<u32> px((size_t)W * H);
            ui::Rasterise(c.ui, 1.0f, px.data(), W, H);
            int outside = 0;
            for (int y = 0; y < H; y++)
                for (int xx = 0; xx < W; xx++)
                    if ((px[(size_t)y * W + xx] >> 24) && (xx < b.x - 5 || xx >= b.x + b.w + 5 || y < b.y - 2 || y >= b.y + b.h + 8)) outside++;
            CHECK(outside == 0);
        }
        Composition tiny {kConsoleW, kConsoleH, {}, {}};
        CHECK(AddStatusBar(tiny, hud, strings, 0, 0, 50).w == 0 && tiny.ui.empty());
    }
    // No HUD (title, main menu, Create-a-Sim, black loads): never the two screens side by side.
    // The top screen framed at the left at its shape, the touch screen 2.5x framed at the right,
    // the only touchable piece; a hint row at the bottom.
    for (Screen sc : {Screen::FrontEnd, Screen::CreateASim, Screen::None})
        for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
        {
            const Composition c = BuildConsole({sc, Tab::None}, false, -1, top.first, top.second);
            CHECK(c.cw == kConsoleW && c.ch == kConsoleH);
            const Composition side = FromPlacement(Place(Layout::Side, true, (float)top.first / top.second), top.first, top.second);
            int touchBottoms = 0;
            for (const Piece& q : c.pieces)
            {
                for (const Piece& sq : side.pieces)
                    CHECK(!(q.to.x == sq.to.x && q.to.y == sq.to.y && q.to.w == sq.to.w && q.to.h == sq.to.h));
                if (q.src != Src::Bottom || !q.touch) continue;
                touchBottoms++;
                CHECK(q.from.x == 0 && q.from.y == 0 && q.from.w == SW && q.from.h == SH);
                CHECK(q.to.w * 2 >= SW * 5 && q.to.h * 2 >= SH * 5 && q.to.w * SH == q.to.h * SW);
                CHECK(q.to.x >= c.pieces[0].to.x + c.pieces[0].to.w);
            }
            CHECK(touchBottoms == 1);
            CHECK(c.pieces.size() == 2 && c.pieces[0].src == Src::Top && c.pieces[0].to.x + c.pieces[0].to.w <= kConsoleW / 2);
            // The top screen at its own shape (within a pixel), never stretched.
            const Rect t = c.pieces[0].to;
            CHECK(std::abs(t.w * top.second - t.h * top.first) <= top.first);
            bool hint = false;
            for (const Overlay& o : c.overlays) hint |= o.text != nullptr && o.to.y > 600;
            CHECK(hint);
            float cx, cy, sc2;
            CHECK(CanvasFromTouch(c, 10, 20, cx, cy, sc2) && sc2 == 2.5f && cx == c.pieces[1].to.x + 25 && cy == c.pieces[1].to.y + 50);
            // Both pieces framed (6 pixels outside each, drawn before the top piece); the top
            // screen is the camera (wheel, right drag), as SIDE's was.
            CHECK(c.pieces[0].touch);
            int frames = 0;
            for (const Overlay& o : c.overlays)
                for (const Piece& q : c.pieces)
                    if (!o.text && o.argb == kFrameArgb && o.border == 6 && o.before == 0 && o.to.x == q.to.x - 6 && o.to.y == q.to.y - 6 &&
                        o.to.w == q.to.w + 12 && o.to.h == q.to.h + 12)
                        frames++;
            CHECK(frames == 2);
        }
    // A black load: the bare backdrop, nothing drawn, nothing touchable, no cursor, no camera.
    for (bool menuOpen : {false, true})
    {
        const Composition c = BuildConsole({Screen::Loading, Tab::None}, menuOpen, 0, SW, SH);
        CHECK(c.cw == kConsoleW && c.ch == kConsoleH && c.pieces.empty() && c.overlays.empty());
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 1280, 720);
        int tx, ty;
        float cx, cy, sc2;
        CHECK(!TouchFromComposition(c, fit, 640, 360, tx, ty));
        CHECK(!OverTopComposition(c, fit, 640, 360));
        CHECK(!CanvasFromTouch(c, 128, 96, cx, cy, sc2));
    }
    // SaveSlots, ItemHand, Unknown (HUD up): the menu composition, framed, with the stick cursor.
    for (Screen sc : {Screen::SaveSlots, Screen::ItemHand, Screen::Unknown})
    {
        const Composition m = BuildConsole({sc, Tab::None}, false, -1, SW, SH);
        CHECK(m.pieces.size() == 2 && !m.pieces[0].touch && m.pieces[1].src == Src::Bottom && m.pieces[1].to.x == kMenuTo.x && m.pieces[1].to.y == kMenuTo.y && m.pieces[1].to.w == SW * 3);
        CHECK(m.overlays.size() == 2 && m.overlays[0].argb == kDimArgb && m.overlays[1].argb == kFrameArgb && m.overlays[1].to.x == kMenuTo.x - 6);
    }
    // The menu: the dimmed world swallows clicks and the camera; the touch screen 3x is the stylus.
    for (ScreenState s : {ScreenState {Screen::Panel, Tab::Goals}, ScreenState {Screen::Popup, Tab::None}, ScreenState {Screen::Gift, Tab::None}})
    {
        const Composition m = BuildConsole(s, s.screen == Screen::Panel, -1, SW, SH);
        const CanvasFit fit = CanvasFit::For(m.cw, m.ch, 1280, 720);
        CHECK(m.pieces.size() == 2 && !m.pieces[0].touch && m.pieces[1].to.x == 256 && m.pieces[1].to.y == 72 && m.pieces[1].to.w == SW * 3);
        CHECK(m.overlays.size() == 1 && m.overlays[0].argb == kDimArgb && m.overlays[0].before == 1);
        int tx, ty;
        CHECK(!TouchFromComposition(m, fit, 200, 300, tx, ty));
        CHECK(!OverTopComposition(m, fit, 200, 300));
        CHECK(TouchFromComposition(m, fit, 256 + 3 * 10 + 1, 72 + 3 * 20 + 1, tx, ty) && tx == 10 && ty == 20);
    }
    // In town the world is the camera except under the rail's pieces.
    {
        const Composition town = BuildConsole({Screen::Panel, Tab::Interact}, false, -1, SW, SH);
        const CanvasFit fit = CanvasFit::For(town.cw, town.ch, 1280, 720);
        CHECK(OverTopComposition(town, fit, 600, 300));
        CHECK(!OverTopComposition(town, fit, 1200, 300));
    }
    // The focus ring: the hotspot's area, through the piece that shows it, 4 pixels outside.
    {
        const ScreenState goals {Screen::Panel, Tab::Goals};
        const Composition m = BuildConsole(goals, true, 0, SW, SH);
        const Rect a = HotspotsFor(goals)[0].area;
        CHECK(m.overlays.size() == 2);
        const Overlay& r = m.overlays.back();
        CHECK(r.argb == 0 && r.border == 4 && r.radius == 8 && r.borderArgb == kRingArgb && r.before == -1);
        CHECK(r.to.x == 256 + 3 * a.x - 4 && r.to.y == 72 + 3 * a.y - 4 && r.to.w == 3 * a.w + 8 && r.to.h == 3 * a.h + 8);
        // In town the tab icons are on the rail (2x); a goal line is not shown: no ring.
        int tab = -1;
        const std::vector<Hotspot>& h = HotspotsFor(goals);
        for (int i = 0; i < (int)h.size(); i++) if (std::string(h[i].name) == "pockets") tab = i;
        CHECK(tab >= 0);
        const Composition town = BuildConsole(goals, false, tab, SW, SH);
        CHECK(town.overlays.size() == 2);
        const Rect ta = h[tab].area;
        CHECK(town.overlays[1].to.x == kRailTabsTo.x + 2 * (ta.x - kRailTabsFrom.x) - 4);
        CHECK(town.overlays[1].to.y == kRailTabsTo.y + 2 * (ta.y - kRailTabsFrom.y) - 4);
        CHECK(BuildConsole(goals, false, 0, SW, SH).overlays.size() == 1);
        CHECK(BuildConsole(goals, false, 999, SW, SH).overlays.size() == 1);
        // The dialogue's arrow: its area clipped to the arrow piece.
        const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, 0, SW, SH);
        const Rect ra = dlg.overlays.back().to;
        CHECK(ra.x >= kDialogueArrowTo.x - 4 && ra.x + ra.w <= kDialogueArrowTo.x + kDialogueArrowTo.w + 4);
    }
    // A drag whose piece is gone (a stale index): the touchable bottom piece under the pointer,
    // else the largest; always on the DS screen.
    {
        const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, -1, SW, SH);
        const CanvasFit fit = CanvasFit::For(dlg.cw, dlg.ch, 1280, 720);
        int tx, ty;
        // Over the portrait (piece 3) with a stale index.
        TouchClampedComposition(dlg, 99, fit, kDialoguePortraitTo.x + 2 * 10 + 1, kDialoguePortraitTo.y + 2 * 5 + 1, tx, ty);
        CHECK(tx == kDialoguePortraitFrom.x + 10 && ty == kDialoguePortraitFrom.y + 5);
        // Over the world (piece 0, a top piece): the largest, the text.
        TouchClampedComposition(dlg, 0, fit, 5, 5, tx, ty);
        CHECK(tx == kDialogueTextFrom.x && ty == kDialogueTextFrom.y);
        // A piece reaching past the DS screen: held to the screen, never an empty range.
        Composition odd {600, 400, {}, {}};
        odd.pieces.push_back({Src::Bottom, {250, 186, 40, 40}, {0, 0, 80, 80}});
        odd.pieces.push_back({Src::Bottom, {-20, -20, 10, 10}, {100, 0, 20, 20}});
        const CanvasFit f2 = CanvasFit::For(odd.cw, odd.ch, 600, 400);
        for (int idx : {0, 1})
            for (float p : {-500.0f, 10.0f, 110.0f, 5000.0f})
            {
                TouchClampedComposition(odd, idx, f2, p, p, tx, ty);
                CHECK(tx >= 0 && tx < SW && ty >= 0 && ty < SH);
            }
    }
    // The stick cursor: drawn through the piece that shows its point, hidden where none does.
    {
        float cx, cy, sc;
        const Composition town = BuildConsole({Screen::Panel, Tab::Goals}, false, -1, SW, SH);
        CHECK(CanvasFromTouch(town, 16, 22, cx, cy, sc) && sc == 2 && cx == kRailTabsTo.x + 32 && cy == kRailTabsTo.y + 32);
        CHECK(CanvasFromTouch(town, 230, 20, cx, cy, sc) && cx == kRailPhoneTo.x + 16);
        CHECK(!CanvasFromTouch(town, 128, 96, cx, cy, sc));
        const Composition unk = BuildConsole({Screen::Unknown, Tab::None}, false, -1, SW, SH);
        CHECK(CanvasFromTouch(unk, 10, 20, cx, cy, sc) && sc == 3 && cx == kMenuTo.x + 30 && cy == kMenuTo.y + 60);
        const Composition side = FromPlacement(Place(Layout::Side, true, kDSAspect), SW, SH);
        CHECK(CanvasFromTouch(side, 0, 0, cx, cy, sc) && cx == SW && cy == 0 && sc == 1);
        CHECK(!CanvasFromTouch(side, SW, 0, cx, cy, sc));
    }
    // The overlay's pixels: border, fill, corners left empty.
    {
        std::vector<u32> px;
        RasteriseOverlay({{0, 0, 100, 50}, 0xFF112233, 10, 4, 0xFF445566}, 200, 100, px);
        CHECK(px.size() == 200 * 100);
        CHECK(px[0] == 0);                       // outside the rounded corner
        CHECK(px[50 * 200 + 2] == 0xFF445566);   // the left border, 8 output pixels wide
        CHECK(px[50 * 200 + 7] == 0xFF445566);
        CHECK(px[50 * 200 + 9] == 0xFF112233);   // the fill
        CHECK(px[50 * 200 + 100] == 0xFF112233);
    }

    for (Layout l : {Layout::Stacked, Layout::Side, Layout::Focus})
        for (bool mainIsTop : {true, false})
            for (float aspect : {kDSAspect, 16.0f / 9.0f})
                for (const auto& s : {std::pair<int, int> {1280, 720}, {2560, 1440}}) TestComposeRoundtrip(l, mainIsTop, aspect, s.first, s.second);

    // Two pieces of the touch screen at different scales: each maps back through its own.
    {
        const Composition c = TwoBottomPieces();
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 1400, 600);
        int tx = -1, ty = -1, idx = -1;
        CHECK(TouchFromComposition(c, fit, 3 * 10 + 1, 3 * 20 + 1, tx, ty, &idx) && tx == 10 && ty == 20 && idx == 0);
        CHECK(TouchFromComposition(c, fit, 900 + 2 * 30, 2 * 5, tx, ty, &idx) && tx == 30 && ty == 101 && idx == 1);
        CHECK(!TouchFromComposition(c, fit, 850, 100, tx, ty)); // the gap between them
        // A drag that started on the first piece stays on it: past its right edge it is held.
        TouchClampedComposition(c, 0, fit, 1300, 500, tx, ty);
        CHECK(tx == SW - 1 && ty == 95);
        TouchClampedComposition(c, 1, fit, 0, -50, tx, ty);
        CHECK(tx == 0 && ty == 96);
    }
    // A drag begun on FOCUS top-big's panel (piece 1) when the layout flips to bottom-big (where
    // piece 1 is the world): it stays on the touch screen, in DS coordinates.
    for (float aspect : {kDSAspect, 16.0f / 9.0f})
    {
        const int tw = (int)(SH * aspect + 0.5f);
        const Composition big = FromPlacement(Place(Layout::Focus, false, aspect), tw, SH);
        const CanvasFit fit = CanvasFit::For(big.cw, big.ch, 1280, 720);
        for (int idx : {1, 0, 5, -1})
            for (float px : {-100.0f, 0.0f, 1000.0f, 1270.0f, 5000.0f})
                for (float py : {-100.0f, 360.0f, 5000.0f})
                {
                    int tx = -1, ty = -1;
                    TouchClampedComposition(big, idx, fit, px, py, tx, ty);
                    CHECK(tx >= 0 && tx < SW && ty >= 0 && ty < SH);
                }
        int tx, ty;
        TouchClampedComposition(big, 1, fit, 480, 360, tx, ty); // the middle of the big touch screen
        CHECK(abs(tx - SW / 2) <= 1 && abs(ty - SH / 2) <= 1);
    }
    // Overlapping pieces: the later one is hit.
    {
        Composition c {600, 400, {}, {}};
        c.pieces.push_back({Src::Bottom, {0, 0, SW, SH}, {0, 0, SW, SH}});
        c.pieces.push_back({Src::Bottom, {0, 0, 64, 48}, {100, 100, 256, 192}});
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 600, 400);
        int tx, ty, idx;
        CHECK(TouchFromComposition(c, fit, 100 + 4 * 10, 100 + 4 * 20, tx, ty, &idx) && idx == 1 && tx == 10 && ty == 20);
        CHECK(TouchFromComposition(c, fit, 50, 50, tx, ty, &idx) && idx == 0 && tx == 50 && ty == 50);
    }
    // A piece with touch off is never hit, and does not let a click through to what is under it.
    {
        Composition c {600, 400, {}, {}};
        c.pieces.push_back({Src::Bottom, {0, 0, SW, SH}, {0, 0, SW, SH}, 255, false});
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 600, 400);
        int tx, ty;
        CHECK(!TouchFromComposition(c, fit, 50, 50, tx, ty));
        c.pieces.insert(c.pieces.begin(), {Src::Bottom, {0, 0, SW, SH}, {0, 0, SW, SH}});
        CHECK(!TouchFromComposition(c, fit, 50, 50, tx, ty));
    }
    // The camera: over a top piece unless a later touchable bottom piece covers the point.
    {
        const Composition c = FromPlacement(Place(Layout::Focus, true, kDSAspect), SW, SH);
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, c.cw, c.ch);
        CHECK(OverTopComposition(c, fit, 100, 100));
        CHECK(!OverTopComposition(c, fit, c.cw - 10, c.ch / 2));
        CHECK(!OverTopComposition(c, fit, -5, 100));
    }
    // A placement drawn second keeps its opacity on its piece; the first is opaque.
    {
        const Composition c = FromPlacement(Place(Layout::Focus, true, kDSAspect), SW, SH);
        CHECK(c.pieces.size() == 2 && c.pieces[0].src == Src::Top && c.pieces[0].alpha == 255);
        CHECK(c.pieces[1].src == Src::Bottom && c.pieces[1].alpha == 232);
        CHECK(c.pieces[0].from.w == SW && c.pieces[0].from.h == SH);
        const Composition w = FromPlacement(Place(Layout::Side, true, 16.0f / 9.0f), 427, 240);
        CHECK(w.pieces[0].from.w == 427 && w.pieces[0].from.h == 240);
    }
    const int sizes[][2] = {{1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}, {1920, 1080}, {640, 480}};
    const float aspects[] = {kDSAspect, 16.0f / 9.0f};
    for (Layout l : {Layout::Stacked, Layout::Side, Layout::Focus})
        for (bool mainIsTop : {true, false})
            for (float aspect : aspects)
                for (const auto& s : sizes) TestLayout(l, mainIsTop, aspect, s[0], s[1]);

    // FOCUS with the world big: the panel lies over the world's right edge and wins there; the
    // world's status bar (its bottom rows) stays clear of it; the world keeps the canvas's height.
    for (float aspect : aspects)
    {
        const Placement p = Place(Layout::Focus, true, aspect);
        CHECK(p.topFirst && p.alpha < 255);
        CHECK(p.top.h == p.ch && p.top.y == 0);
        CHECK(p.bottom.x + p.bottom.w == p.cw);
        CHECK(p.bottom.y + p.bottom.h < p.top.y + p.top.h - 40);
        CHECK(p.bottom.w == SW * 2 && p.bottom.h == SH * 2);
        const CanvasFit fit = CanvasFit::For(p, 1280, 720);
        int tx, ty;
        const float px = fit.ox + (p.bottom.x + 1) * fit.scale, py = fit.oy + (p.bottom.y + 1) * fit.scale;
        CHECK(p.Contains(p.top, p.bottom.x + 1, p.bottom.y + 1)); // under the panel there is world
        CHECK(TouchFromOutput(p, fit, px, py, tx, ty));
        CHECK(!OverTop(p, fit, px, py));
    }
    // FOCUS with the touch screen big: the world is drawn last, small, beside it, never over it.
    {
        const Placement p = Place(Layout::Focus, false, kDSAspect);
        CHECK(!p.topFirst && p.alpha == 255);
        CHECK(p.top.x >= p.bottom.x + p.bottom.w);
        const Placement w = Place(Layout::Focus, false, 16.0f / 9.0f);
        CHECK(w.top.w == 320 && w.top.h == 180);
    }
    // SIDE and STACKED follow the top picture's shape.
    CHECK(Place(Layout::Side, true, 16.0f / 9.0f).top.w == 341);
    CHECK(Place(Layout::Stacked, true, 16.0f / 9.0f).cw == 341);
    CHECK(Place(Layout::Side, true, kDSAspect).cw == SW * 2);

    // The HD conversation's text (port/dialogue.h): placeholders and escapes never shown raw.
    {
        HudState hud;
        hud.valid = true; hud.hour = 15; hud.minute = 7; hud.simName = "Jun";
        CHECK(DialogueText("There's a reason, @1. Go!", hud) == "There's a reason, Jun. Go!");
        CHECK(DialogueText("the @2 was repossessed", hud) == "the was repossessed");
        CHECK(DialogueText("Looks like a @2! Amazing.", hud) == "Looks like a! Amazing.");
        CHECK(DialogueText("by @3.", hud) == "by.");
        CHECK(DialogueText("at @hh:@mm @am today", hud, "am", "pm") == "at 3:07 pm today");
        CHECK(DialogueText("day:@DD @ss @ll end", hud) == "day: end");
        CHECK(DialogueText("{F012}Hello{0b} there{F0}", hud) == "Hello there");
        CHECK(DialogueText("lone @ sign @", hud) == "lone sign");
        CHECK(DialogueText("  @2 starts\n@1\n\n", hud) == "starts\nJun");
        CHECK(DialogueText("Bonjour !", hud) == "Bonjour !"); // the string's own space before '!' stays
        HudState none; // no clock, no name: dropped
        CHECK(DialogueText("@1, it is @hh:@mm.", none) == ", it is:.");
        // Wrapped to the box: a word longer than a line breaks; a long page grows the panel up,
        // never past the canvas; every line within the text box.
        const DialogueLayout a = LayoutDialogue("Short.", true);
        CHECK(a.fits && a.lines.size() == 1 && a.size == kDlgTextSize && a.rows == kDlgRows);
        CHECK(a.panel.x == kDlgPanelX && a.panel.y + a.panel.h == kConsoleH - kDlgBottomGap && a.panel.y > 0);
        const DialogueLayout b = LayoutDialogue(std::string(150, 'W'), true);
        CHECK(b.fits && b.lines.size() > 4 && b.panel.y >= 0 && b.panel.h > a.panel.h);
        std::string longText;
        for (int i = 0; i < 60; i++) longText += "word ";
        const DialogueLayout c = LayoutDialogue(longText, false);
        CHECK(c.fits && c.size >= kDlgTextMin && c.textW > a.textW);
        for (const std::string& l : c.lines) CHECK(ui::TextWidth(l, c.size) <= c.textW);
        const DialogueLayout huge = LayoutDialogue(std::string(4000, 'W'), true); // clipped, never past the panel
        CHECK(!huge.fits && huge.rows == kDlgMaxRows && huge.panel.y >= 0);
        CHECK(!PortraitShown(nullptr));
        std::vector<u32> flat(SW * SH, 0xFF5961A2), black(SW * SH, 0xFF000000), face(SW * SH, 0xFFC08060);
        CHECK(!PortraitShown(flat.data()) && !PortraitShown(black.data()) && PortraitShown(face.data()));

        // BuildDialogue (a RomData without strings: no text, the player's name on the plate):
        // the world whole, every UI item inside the canvas, a click on the panel taps the game's
        // arrow; BuildConsole keeps milestone 1's box without the game's strings.
        RomData rom;
        GameState g;
        g.valid = true;
        g.hud = hud;
        g.dialogue.valid = true; g.dialogue.page = 1; g.dialogue.pages = 2; g.dialogue.stringId = 5; g.dialogue.speaker = 0x40;
        for (int topW : {256, 341})
        {
            Composition comp {kConsoleW, kConsoleH, {}, {}};
            BuildDialogue(comp, g, rom, face.data(), topW, 192);
            CHECK(comp.pieces.size() == 2 && comp.pieces[0].src == Src::Top && comp.pieces[0].from.x == 0 && comp.pieces[0].from.w == topW);
            CHECK(comp.pieces[0].from.h == HudCropRows(192));
            bool plate = false;
            for (const ui::UiItem& it : comp.ui)
            {
                CHECK(it.to.x >= 0 && it.to.y >= 0 && it.to.x + it.to.w <= kConsoleW && it.to.y + it.to.h <= kConsoleH);
                plate |= it.kind == ui::UiItem::Text && it.text == "Jun";
            }
            CHECK(plate);
            const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
            const Rect P = comp.pieces[1].to;
            int tx, ty;
            CHECK(TouchFromComposition(comp, fit, P.x + 5.5f, P.y + 5.5f, tx, ty) && tx >= 219 && tx < 250 && ty >= 155 && ty < 187);
            CHECK(TouchFromComposition(comp, fit, P.x + P.w - 2.5f, P.y + P.h - 2.5f, tx, ty) && tx >= 219 && tx < 250 && ty >= 155 && ty < 187);
            CHECK(OverTopComposition(comp, fit, 640.5f, 100.5f));
            // Every pixel of its touchable pieces (the invisible tap piece) round trips, at every size.
            for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                TestConsoleRoundtrip({Screen::Dialogue}, false, topW, 192, o.first, o.second, true, &comp);
        }
        const Composition m1 = BuildConsole({Screen::Dialogue}, false, -1, 256, 192, &hud, nullptr, &g, &rom, face.data());
        CHECK(m1.ui.empty() && m1.pieces.size() == 5);
    }


    // The panel's HD menu (port/panelmenu.h, port/interact.h) on a made-up Interact state: four
    // actions (the third greyed), a previous and a next target. Its mouse pieces (invisible,
    // touchable) map each HD item onto the game's own target: every pixel round trips at every
    // size; a click on a row lands on its arrow, on a tab on its icon, on the phone on the phone;
    // a click on the panel elsewhere, or on the dimmed world, touches nothing. Every UI item lies
    // inside the panel; the still items come in ascending layers (the frame on 0, the rest above).
    {
        RomData rom;
        GameState g;
        g.valid = true;
        InteractState& in = g.town.interact;
        in.valid = true;
        in.target = {TargetRef::Object, 29, 492, 6638, false};
        in.prev = {TargetRef::Sim, 3, 326, 1, false};
        in.next = {TargetRef::Object, 15, 478, 5744, false};
        for (int i = 0; i < 4; i++) in.actions.push_back({i, 1845 + i, i == 2});
        CHECK(WithoutAmount("Pay (\x24@1)") == "Pay" && WithoutAmount("Buy (@1) now") == "Buy now" && WithoutAmount("Open") == "Open" && WithoutAmount("Pay @1") == "Pay @1");
        const ScreenState it {Screen::Panel, Tab::Interact};
        const MenuView v = PanelMenuViewOf(it, g);
        CHECK(v.valid && v.items.size() == 5);
        CHECK(!v.items[2].enabled && v.items[0].enabled && v.items[3].enabled);
        CHECK(!v.buttons[(int)MenuButton::Left].empty() && !v.buttons[(int)MenuButton::Right].empty());
        CHECK(MenuFirst(v) == 0 && MenuNeighbour(v, 1, 0, 1) == 3); // the greyed row skipped

        // Reachability on the real views: every enabled item from every other, the phone from
        // the rows and back (Up from the top row, Down from the phone), with and without a
        // target, greyed rows, neighbours (Left / Right then switch targets, not moves).
        for (int rows : {0, 1, 3, 4})
            for (int greyed : {-1, 0, 1})
                for (bool side : {false, true})
                {
                    GameState h = g;
                    InteractState& hi = h.town.interact;
                    hi.actions.clear();
                    for (int i = 0; i < rows; i++) hi.actions.push_back({i, 1845 + i, i == greyed});
                    if (!side) hi.prev = hi.next = TargetRef {};
                    if (rows == 0) hi.target = TargetRef {};
                    const MenuView w = PanelMenuViewOf(it, h);
                    CHECK(w.valid && AllReachable(w));
                    const int phone = (int)w.items.size() - 1;
                    const int top = MenuFirst(w);
                    if (top != phone)
                    {
                        CHECK(MenuMove(w, top, 0) == phone);
                        CHECK(MenuMove(w, phone, 1) == top);
                    }
                }
        CHECK(!PanelMenuView(it, &g, &rom).valid);                  // no strings: milestone 1's
        CHECK(!PanelMenuViewOf({Screen::Panel, Tab::Goals}, g).valid);
        GameState bad = g;
        bad.town.interact.valid = false;
        CHECK(!PanelMenuViewOf(it, bad).valid);
        std::vector<u32> face(SW * SH, 0xFFC08060);
        for (int topW : {256, 341})
            for (int focus : {-1, 0, 3, 4})
            {
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                Piece world = ConsoleWorld(topW, HudCropRows(192), true);
                world.touch = false;
                comp.pieces.push_back(world);
                BuildPanelMenu(comp, it, v, focus, g, rom, nullptr, face.data());
                int layer = 0;
                for (const ui::UiItem& u : comp.ui)
                {
                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                    if (u.kind == ui::UiItem::Image) continue;
                    CHECK(u.layer >= layer);
                    layer = u.layer;
                }
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                    TestConsoleRoundtrip(it, true, topW, 192, o.first, o.second, true, &comp);
                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                int tx, ty;
                for (int i = 0; i < 4; i++)
                {
                    const Rect r = v.items[i].rect;
                    CHECK(TouchFromComposition(comp, fit, r.x + 3.5f, r.y + r.h / 2.0f, tx, ty));
                    CHECK(tx >= 219 && tx <= 251 && ty >= 46 + 34 * i && ty <= 78 + 34 * i);
                }
                for (int t = 0; t < 5; t++)
                {
                    CHECK(TouchFromComposition(comp, fit, kPmTabsX + t * kPmTabCell + kPmTabCell / 2.0f, kPmTabBar.y + kPmTabBar.h / 2.0f, tx, ty));
                    const Rect a = kTabIcon[t].area;
                    CHECK(tx >= a.x && tx < a.x + a.w && ty >= a.y && ty < a.y + a.h);
                }
                const Rect ph = v.items[4].rect;
                CHECK(TouchFromComposition(comp, fit, ph.x + ph.w / 2.0f, ph.y + ph.h / 2.0f, tx, ty));
                CHECK(tx >= kPhoneIcon.area.x && tx < kPhoneIcon.area.x + kPhoneIcon.area.w);
                CHECK(!TouchFromComposition(comp, fit, kPmTitle.x + 10.5f, kPmTitle.y + 10.5f, tx, ty)); // the title: nothing
                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                          // the dimmed world: nothing
                CHECK(!OverTopComposition(comp, fit, 20.5f, 360.5f));
            }
        // Pockets (port/pockets.h) and Give Gift (port/gift.h) on made-up inventories: empty slots
        // disabled and never focused, every enabled item reachable from every other and the phone
        // Up from the top row; A on a slot taps it then the hand / the bow; X on a slot the game
        // has not selected taps the slot (the question), on the one it has the bin. Their mouse
        // pieces round trip and land on the game's own targets; every item inside the panel.
        auto inventory = [](int n) {
            GameState h;
            h.valid = true;
            InventoryState& inv = h.town.inventory;
            inv.valid = true;
            for (int i = 0; i < n; i++) inv.slots.push_back({100 + i, 563 + i, 922 + i, 1991, 1});
            for (int k = 0; k < 3; k++) inv.keys[k] = {k, 1968 + k, 1972 + k, -1, k == 1 ? 1 : 0};
            for (int i = 0; i < 4; i++) inv.counted[i] = {i, 1154 + i, 845 + i, -1, i * 3};
            h.town.pockets.valid = true;
            h.town.gift.valid = true;
            h.town.gift.recipient = 22;
            h.town.gift.recipientNameId = 345;
            return h;
        };
        const ScreenState pk {Screen::Panel, Tab::Pockets}, gs {Screen::Gift, Tab::None};
        for (int n = 0; n <= 6; n++)
            for (int sel : {-1, 0, 5})
            {
                GameState h = inventory(n);
                h.town.pockets.slot = sel < n ? sel : -1;
                h.town.gift.slot = sel < n ? sel : -1;
                if (n == 4) h.town.gift.refused[2] = true;
                const MenuView p = PanelMenuViewOf(pk, h);
                CHECK(p.valid && p.items.size() == 6 + 3 + 4 + 1 && AllReachable(p));
                for (int i = 0; i < 6; i++)
                {
                    CHECK(p.items[i].enabled == (i < n));
                    if (i >= n) continue;
                    CHECK(p.items[i].action.size() == 2 && p.items[i].action[1].x == kPkHandX && p.items[i].action[1].y == kPkHandY);
                    // The bin asks, whatever the game's selection: arming taps the slot, the
                    // second X the slot and the bin.
                    const ViewItem& it = p.items[i];
                    CHECK(it.xAsks && it.xArm.size() == 1 && it.xArm[0].x == it.action[0].x && it.xArm[0].y == it.action[0].y);
                    CHECK(it.buttonX.size() == 2 && it.buttonX[0].x == it.action[0].x && it.buttonX[1].x == kPkBinX && it.buttonX[1].y == kPkBinY);
                }
                CHECK(p.items[6].action.empty() && !p.items[7].action.empty() && p.items[9].action.empty()); // a key item not owned, owned; counted
                CHECK(MenuMove(p, 13, 1) == MenuFirst(p));
                if (h.town.pockets.slot >= 0) CHECK(MenuFirst(p) == h.town.pockets.slot);
                const MenuView g = GiftMenuViewOf(gs, h);
                CHECK(g.valid && g.items.size() == 6 && g.back.size() == 1 && g.back[0].x == kGiftCloseX);
                for (int i = 0; i < 6; i++)
                {
                    CHECK(g.items[i].enabled == (i < n && !(n == 4 && i == 2)));
                    if (g.items[i].enabled) CHECK(g.items[i].action.size() == 2 && g.items[i].action[1].x == kGiftBowX);
                }
                bool any = false;
                for (const ViewItem& it : g.items) any |= it.enabled;
                if (any) CHECK(AllReachable(g));
                std::vector<u32> pic(SW * SH, 0xFF808080);
                for (int focus : {-1, 0, 7, 9, 13})
                {
                    for (bool gift : {false, true})
                    {
                        Composition comp {kConsoleW, kConsoleH, {}, {}};
                        Piece world = ConsoleWorld(256, HudCropRows(192), true);
                        world.touch = false;
                        comp.pieces.push_back(world);
                        if (gift) BuildGiftMenu(comp, g, focus < 6 ? focus : -1, h, rom, nullptr, pic.data());
                        else BuildPanelMenu(comp, pk, p, focus, h, rom, nullptr, pic.data());
                        int layer = 0;
                        for (const ui::UiItem& u : comp.ui)
                        {
                            CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                  u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                            if (u.kind == ui::UiItem::Image) continue;
                            CHECK(u.layer >= layer);
                            layer = u.layer;
                        }
                        if (n != 2 || focus != 0) continue;
                        for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                            TestConsoleRoundtrip(gift ? gs : pk, true, 256, 192, o.first, o.second, true, &comp);
                        const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                        const std::vector<Hotspot>& hs = HotspotsFor(gift ? gs : pk);
                        const MenuView& w = gift ? g : p;
                        int tx, ty;
                        for (int i = 0; i < 6; i++)
                        {
                            const Rect r = w.items[i].rect;
                            CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                            const Rect a = hs[i].area;
                            CHECK(tx >= a.x && tx < a.x + a.w && ty >= a.y && ty < a.y + a.h);
                        }
                        const Rect act = gift ? kGiftBow : kPkHandBtn;
                        const Rect a = hs[6].area; // (the hand, or Give Item's bow: both 6th)
                        CHECK(TouchFromComposition(comp, fit, act.x + act.w / 2.0f, act.y + act.h / 2.0f, tx, ty));
                        CHECK(tx >= a.x && tx < a.x + a.w && ty >= a.y && ty < a.y + a.h);
                        if (!gift)
                        {
                            CHECK(TouchFromComposition(comp, fit, kPkBinBtn.x + kPkBinBtn.w / 2.0f, kPkBinBtn.y + kPkBinBtn.h / 2.0f, tx, ty));
                            CHECK(tx >= hs[10].area.x && tx < hs[10].area.x + hs[10].area.w && ty >= hs[10].area.y && ty < hs[10].area.y + hs[10].area.h);
                        }
                        CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty)); // the dimmed world: nothing
                    }
                }
            }
        {
            GameState bad = inventory(2);
            bad.town.inventory.valid = false;
            CHECK(!PanelMenuViewOf(pk, bad).valid && !GiftMenuViewOf(gs, bad).valid);
            bad = inventory(2);
            bad.town.gift.valid = false;
            CHECK(!GiftMenuViewOf(gs, bad).valid && PanelMenuViewOf(pk, bad).valid);
            CHECK(!GiftMenuView(gs, &bad, &rom).valid); // no strings: milestone 1's
        }
        // The shop's catalogue (port/catalogue.h) on made-up lists: the store's twelve items at
        // every scroll, 0-6 owned, Buy or Sell or nothing selected, money enough or not; a
        // vendor's six. Cells past the list and empty places disabled; Pay always focusable, its
        // tap only when the game would pay (the money, the pockets' room, the quest items, a
        // vendor's sale); every enabled item reachable; the game's arrows as the list's scroll;
        // A on a cell / place its own tap, B the X, never replayed. Every item inside the panel,
        // layers ascending, the mouse pieces round trip and land on the game's own hit boxes.
        auto shop = [](bool vendor, int count, int scroll, int owned, bool sell, int sel, int money) {
            GameState h;
            h.valid = true;
            h.screen = GScreen::Catalogue;
            CatalogueState& ca = h.catalogue;
            ca.valid = true;
            ca.shop = vendor ? 0x30 : kCaStore;
            ca.vendor = vendor;
            for (int i = 0; i < count; i++) ca.items.push_back({100 + i + 0x3B, 100 + i, 563 + i, 922 + i, 1991, 2 * (50 + 10 * i), 50 + 10 * i});
            for (int i = 0; i < owned; i++) ca.owned.push_back({i == 1 ? 0x22 + 0x3B : 106 + 0x3B, i == 1 ? 0x22 : 106, 569, 928, 1991, 160, 80});
            ca.scroll = scroll;
            ca.sell = sell;
            ca.selected = sel;
            ca.money = money;
            ca.payEnabled = true;
            ca.upEnabled = !vendor && scroll > 0;
            ca.downEnabled = !vendor && scroll < count - 3;
            return h;
        };
        const ScreenState cs {Screen::Catalogue, Tab::None};
        for (bool vendor : {false, true})
            for (int scroll : {0, 3, 6, 9})
                for (int owned : {0, 2, 6})
                    for (int mode : {0, 1, 2}) // nothing selected, Buy, Sell
                        for (int money : {0, 5000})
                        {
                            if (vendor && (scroll > 0 || owned > 0 || mode == 2)) continue;
                            const int count = vendor ? 6 : 12;
                            const bool sell = mode == 2;
                            const int sel = mode == 0 ? -1 : sell ? std::min(1, owned - 1) : scroll + 1;
                            if (sell && owned == 0) continue;
                            const GameState h = shop(vendor, count, scroll, owned, sell, sel, money);
                            const MenuView v = CatalogueViewOf(cs, h);
                            const size_t n = vendor ? 7 : 13;
                            CHECK(v.valid && v.own && !v.replay && v.backCloses && v.items.size() == n && AllReachable(v));
                            CHECK(v.back.size() == 1 && v.back[0].x == kCaCloseX && v.back[0].y == kCaCloseY);
                            for (int c = 0; c < 6; c++)
                            {
                                CHECK(v.items[c].enabled == (scroll + c < count) && v.items[c].group == v.scrollGroup);
                                if (!v.items[c].enabled) continue;
                                const int x = vendor ? kCaVendorCellX0 + (c % 2) * kCaVendorCellDx : kCaStoreCellX0 + (c % 3) * kCaStoreCellDx;
                                const int y = kCaCellY0 + (c / (vendor ? 2 : 3)) * kCaCellDy;
                                CHECK(v.items[c].action.size() == 1 && v.items[c].action[0].x == x && v.items[c].action[0].y == y);
                            }
                            // Pay: focusable always; its tap only when the game would pay.
                            const ViewItem& pay = v.items[6];
                            CHECK(pay.enabled && pay.id == kCaPayId);
                            bool pays = false;
                            if (mode == 1) pays = money >= h.catalogue.items[sel].buyPrice && owned < 6;
                            if (mode == 2) pays = sel != 1; // (place 1 holds a quest item: the game will not buy it back)
                            CHECK(CataloguePays(h.catalogue) == pays && pay.action.empty() == !pays);
                            if (pays) CHECK(pay.action.size() == 1 && pay.action[0].x == (vendor ? kCaVendorPayX : kCaPayX) && pay.action[0].y == kCaPayY);
                            for (int i = 0; !vendor && i < 6; i++)
                            {
                                CHECK(v.items[7 + i].enabled == (i < owned));
                                if (i < owned) CHECK(v.items[7 + i].action.size() == 1 && v.items[7 + i].action[0].x == kCaOwnedX0 + (i % 2) * kCaOwnedDx);
                            }
                            // The game's arrows: the list's scroll; Right from the grid's last column: Pay.
                            CHECK(v.scrollUp.empty() == !h.catalogue.upEnabled && v.scrollDown.empty() == !h.catalogue.downEnabled);
                            if (!v.scrollDown.empty()) CHECK(v.scrollDown[0].x == kCaDownX && v.scrollDown[0].y == kCaArrowY);
                            if (!v.scrollUp.empty()) CHECK(v.scrollUp[0].x == kCaUpX);
                            CHECK(MenuMove(v, vendor ? 1 : 2, 3) == 6 && MenuMove(v, 0, 0) == 0 && MenuMove(v, 6, 0) == 6 && MenuMove(v, 6, 1) == 6);
                            if (v.items[5].enabled) CHECK(MenuMove(v, 5, 1) == 5); // (past the last row: the game's scroll, never Pay)
                            if (!vendor && owned > 0) CHECK(MenuMove(v, 7, 2) == 6);
                            if (mode == 1) CHECK(MenuFirst(v) == 1);
                            if (mode == 2) CHECK(MenuFirst(v) == 7 + sel);
                            // The drawing, for a few focuses.
                            for (int focus : {-1, 0, 6, 7})
                            {
                                Composition comp {kConsoleW, kConsoleH, {}, {}};
                                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                                world.touch = false;
                                comp.pieces.push_back(world);
                                BuildCatalogue(comp, v, focus, h, rom, nullptr);
                                int layer = 0;
                                for (const ui::UiItem& u : comp.ui)
                                {
                                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                                    if (u.kind == ui::UiItem::Image) continue;
                                    CHECK(u.layer >= layer);
                                    layer = u.layer;
                                }
                                if (focus != 0 || money != 5000 || (owned != 2 && !vendor)) continue;
                                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                    TestConsoleRoundtrip(cs, false, 256, 192, o.first, o.second, true, &comp);
                                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                                int tx, ty;
                                // Every cell, place and Pay lands in the game's hit box around its point.
                                for (int i = 0; i < (int)v.items.size(); i++)
                                {
                                    if (v.items[i].action.empty()) continue;
                                    const Rect r = v.items[i].rect;
                                    CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - v.items[i].action[0].x) <= 15 && std::abs(ty - v.items[i].action[0].y) <= 15);
                                }
                                const CatalogueGeometry g = CatalogueLayout(vendor);
                                if (!vendor)
                                {
                                    CHECK(TouchFromComposition(comp, fit, g.down.x + g.down.w / 2.0f, g.down.y + g.down.h / 2.0f, tx, ty));
                                    CHECK(tx >= 62 && tx <= 92 && ty >= 153 && ty <= 183);
                                    CHECK(TouchFromComposition(comp, fit, g.up.x + g.up.w / 2.0f, g.up.y + g.up.h / 2.0f, tx, ty));
                                    CHECK(tx >= 30 && tx <= 60 && ty >= 153 && ty <= 183);
                                }
                                CHECK(TouchFromComposition(comp, fit, kCaClose.x + kCaClose.w / 2.0f, kCaClose.y + kCaClose.h / 2.0f, tx, ty));
                                CHECK(tx >= 220 && tx <= 250 && ty >= 4 && ty <= 34);
                                CHECK(!TouchFromComposition(comp, fit, kCaTitle.x + 10.5f, kCaTitle.y + 10.5f, tx, ty)); // the title: nothing
                                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                          // the dimmed world: nothing
                            }
                        }
        {
            // A vendor never buys back; nothing selected (just after a sale): Pay does nothing;
            // the pockets full: no purchase; no view off its screen, without RAM's part or the strings.
            GameState h = shop(true, 6, 0, 0, true, 0, 100);
            h.catalogue.owned.push_back({106 + 0x3B, 106, 569, 928, 1991, 160, 80});
            CHECK(!CataloguePays(h.catalogue));
            h = shop(false, 12, 0, 3, true, 3, 100);
            CHECK(!CataloguePays(h.catalogue) && CatalogueViewOf(cs, h).items[6].action.empty());
            h = shop(false, 12, 0, 6, false, 0, 99999);
            CHECK(!CataloguePays(h.catalogue));
            h = shop(false, 12, 0, 5, false, 0, 99999);
            CHECK(CataloguePays(h.catalogue));
            h.catalogue.money = h.catalogue.items[0].buyPrice - 1;
            CHECK(!CataloguePays(h.catalogue));
            CHECK(!CatalogueViewOf({Screen::Panel, Tab::Interact}, h).valid);
            h.catalogue.valid = false;
            CHECK(!CatalogueViewOf(cs, h).valid);
            h.catalogue.valid = true;
            CHECK(!CatalogueView(cs, &h, &rom).valid); // no strings: milestone 1's
            // The ids: a re-rolled list is other items (the focus does not stay on a new item by its place).
            GameState k = shop(false, 12, 0, 0, false, 0, 100);
            const u32 before = CatalogueViewOf(cs, k).items[0].id;
            k.catalogue.items[0].row += 1;
            CHECK(CatalogueViewOf(cs, k).items[0].id != before);
        }
        // Build Rooms (port/build.h) on made-up lists of 0-11 rooms at every scroll, money enough or
        // not, and page 1 for each room: the rows the game shows (three from its scroll), each
        // focusable, A its knob only when the money covers the permit; the game's arrows as the
        // list's scroll while it can move; B the X (the list) or the cross (page 1), the tick the
        // first focus there; never replayed. Every item inside the panel, layers ascending, the
        // mouse pieces round trip and land on the game's own buttons.
        {
            auto build = [](int count, int scroll, int page, int chosen, int money) {
                GameState h;
                h.valid = true;
                h.screen = GScreen::Build;
                BuildState& b = h.build;
                b.valid = true;
                const int places[] = {0, 1, 8, 12, 15, 16, 18, 21, 26, 28, 31};
                const int prices[] = {2000, 1000, 1500, 2500, 1000, 1500, 1000, 3500, 1000, 2000, 2500};
                for (int i = 0; i < count; i++) b.places.push_back({places[i], prices[i], 0x43 + 2, prices[i] <= money});
                b.scroll = scroll;
                b.page = page;
                b.chosen = chosen;
                b.money = money;
                return h;
            };
            const ScreenState bs {Screen::Build, Tab::None};
            for (int count : {0, 1, 2, 3, 4, 11})
                for (int money : {0, 2000, 999999})
                    for (int scroll = 0; scroll <= std::max(0, count - 3); scroll++)
                        for (int chosen = -1; chosen < count; chosen++)
                        {
                            const int page = chosen < 0 ? 0 : 1;
                            if (page == 1 && scroll > 0) continue; // (page 1 does not depend on the scroll)
                            const GameState h = build(count, scroll, page, chosen, money);
                            const MenuView v = BuildRoomsViewOf(bs, h);
                            CHECK(v.valid && v.own && !v.replay && v.back.size() == 1);
                            if (page == 0)
                            {
                                const int shown = std::min(3, count - scroll);
                                CHECK((int)v.items.size() == shown && v.backCloses && v.back[0].x == kBdCloseX && v.back[0].y == kBdCloseY);
                                CHECK(AllReachable(v));
                                for (int i = 0; i < shown; i++)
                                {
                                    const BuildEntry& e = h.build.places[scroll + i];
                                    CHECK(v.items[i].enabled && v.items[i].id == kBdRowId + (u32)e.place && v.items[i].group == v.scrollGroup);
                                    CHECK(v.items[i].action.empty() == !e.affordable);
                                    if (e.affordable) CHECK(v.items[i].action.size() == 1 && v.items[i].action[0].x == kBdRowX && v.items[i].action[0].y == kBdRowY0 + i * kBdRowDy);
                                    CHECK(MenuMove(v, i, 2) == i && MenuMove(v, i, 3) == i);
                                }
                                if (shown > 0) CHECK(MenuMove(v, 0, 0) == 0 && MenuMove(v, shown - 1, 1) == shown - 1);
                                if (shown > 1) CHECK(MenuMove(v, 0, 1) == 1 && MenuMove(v, 1, 0) == 0);
                                CHECK(v.scrollUp.empty() == (scroll == 0) && v.scrollDown.empty() == (scroll + 3 >= count));
                                if (!v.scrollUp.empty()) CHECK(v.scrollUp[0].x == kBdUpX && v.scrollUp[0].y == kBdArrowY);
                                if (!v.scrollDown.empty()) CHECK(v.scrollDown[0].x == kBdDownX && v.scrollDown[0].y == kBdArrowY);
                            }
                            else
                            {
                                CHECK(v.items.size() == 2 && !v.backCloses && v.initial == 1 && MenuFirst(v) == 1);
                                CHECK(v.items[1].action.size() == 1 && v.items[1].action[0].x == kBdTickX && v.items[1].action[0].y == kBdConfirmY);
                                CHECK(v.items[0].action[0].x == kBdCrossX && v.back[0].x == kBdCrossX && v.back[0].y == kBdConfirmY);
                                CHECK(MenuMove(v, 1, 2) == 0 && MenuMove(v, 0, 3) == 1 && MenuMove(v, 1, 0) == 1);
                            }
                            for (int focus : {-1, 0, 1, 2})
                            {
                                Composition comp {kConsoleW, kConsoleH, {}, {}};
                                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                                world.touch = false;
                                comp.pieces.push_back(world);
                                BuildBuildRooms(comp, v, focus, h, rom, nullptr);
                                CHECK(!comp.ui.empty());
                                int layer = 0;
                                for (const ui::UiItem& u : comp.ui)
                                {
                                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                                    if (u.kind == ui::UiItem::Image) continue;
                                    CHECK(u.layer >= layer);
                                    layer = u.layer;
                                }
                                if (focus != 0 || money != 2000 || scroll != 0 || (chosen > 1)) continue;
                                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                    TestConsoleRoundtrip(bs, false, 256, 192, o.first, o.second, true, &comp);
                                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                                int tx, ty;
                                // Every row, the cross and the tick land within the game's circle around its point.
                                for (const ViewItem& it : v.items)
                                {
                                    const Rect r = it.rect;
                                    CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                    const int px = page == 0 ? kBdRowX : it.id == kBdTickId ? kBdTickX : kBdCrossX;
                                    const int py = page == 0 ? kBdRowY0 + (r.y - BuildRowRect(0).y) / (BuildRowRect(1).y - BuildRowRect(0).y) * kBdRowDy : kBdConfirmY;
                                    CHECK(std::abs(tx - px) <= 15 && std::abs(ty - py) <= 15);
                                }
                                if (page == 0)
                                {
                                    CHECK(TouchFromComposition(comp, fit, kBdUp.x + kBdUp.w / 2.0f, kBdUp.y + kBdUp.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - kBdUpX) <= 15 && std::abs(ty - kBdArrowY) <= 15);
                                    CHECK(TouchFromComposition(comp, fit, kBdDown.x + kBdDown.w / 2.0f, kBdDown.y + kBdDown.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - kBdDownX) <= 15 && std::abs(ty - kBdArrowY) <= 15);
                                }
                                CHECK(TouchFromComposition(comp, fit, kBdClose.x + kBdClose.w / 2.0f, kBdClose.y + kBdClose.h / 2.0f, tx, ty));
                                CHECK(std::abs(tx - kBdCloseX) <= 15 && std::abs(ty - kBdCloseY) <= 15);
                                CHECK(!TouchFromComposition(comp, fit, kBdTitle.x + 10.5f, kBdTitle.y + 10.5f, tx, ty)); // the title: nothing
                                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                          // the dimmed world: nothing
                            }
                        }
            // No view off its screen, without RAM's part or the strings; a row's id follows its room.
            GameState h = build(4, 0, 0, -1, 2000);
            CHECK(!BuildRoomsViewOf({Screen::Panel, Tab::Interact}, h).valid);
            CHECK(!BuildRoomsView(bs, &h, &rom).valid); // no strings: milestone 1's
            const u32 first = BuildRoomsViewOf(bs, h).items[0].id;
            h.build.scroll = 1;
            CHECK(BuildRoomsViewOf(bs, h).items[0].id != first);
            h.build.valid = false;
            CHECK(!BuildRoomsViewOf(bs, h).valid);
        }
        // Goals, Relationships, Options (port/goals.h, relations.h, options.h) and the Save slots
        // on made-up states: the game's selection held (Up / Down its arrows), Options' moves
        // explicit and its sliders' Left / Right the knob's drag by a tenth, the Save slots' A its
        // tick and B its X; every enabled item reachable (the phone too, but where the game owns
        // the selection: the focus stays on it); every item inside the panel, layers ascending,
        // the mouse pieces round trip and land on the game's own targets.
        {
            auto check = [&](ScreenState st, const MenuView& v, const GameState& h, int focus) {
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                world.touch = false;
                comp.pieces.push_back(world);
                std::vector<u32> pic(SW * SH, 0xFF808080);
                if (st.screen == Screen::SaveSlots) BuildSaveSlots(comp, v, focus, h, rom);
                else BuildPanelMenu(comp, st, v, focus, h, rom, nullptr, pic.data());
                int layer = 0;
                for (const ui::UiItem& u : comp.ui)
                {
                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                    if (u.kind == ui::UiItem::Image) continue;
                    CHECK(u.layer >= layer);
                    layer = u.layer;
                }
                for (const ViewItem& it : v.items)
                    CHECK(it.rect.x >= kPmPanel.x && it.rect.y >= kPmPanel.y && it.rect.x + it.rect.w <= kPmPanel.x + kPmPanel.w &&
                          it.rect.y + it.rect.h <= kPmPanel.y + kPmPanel.h);
                bool touchable = false; // ("Saving" over the slots: nothing to touch)
                for (const Piece& q : comp.pieces) touchable |= q.src == Src::Bottom;
                if (touchable)
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(st, true, 256, 192, o.first, o.second, true, &comp);
                return comp;
            };
            auto inside = [](int x, int y, Rect a) { return x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h; };
            const CanvasFit fit = CanvasFit::For(kConsoleW, kConsoleH, 1280, 720);
            int tx, ty;
            // Goals: 8 goals of 1-3 DS lines (14 lines), the selection and the game's scroll.
            const ScreenState gs {Screen::Panel, Tab::Goals};
            for (int n : {0, 1, 8})
                for (int sel : {-1, 0, 3, 7})
                    for (bool footer : {false, true})
                    {
                        if (sel >= n) continue;
                        GameState h;
                        h.valid = true;
                        GoalsState& g = h.town.goals;
                        g.valid = true;
                        int line = 0;
                        for (int i = 0; i < n; i++)
                        {
                            const int lines = 1 + i % 3;
                            g.entries.push_back({true, i, 1627 + i, -1, -1, line, lines});
                            line += lines;
                        }
                        g.lines = line;
                        g.selected = sel;
                        g.first = sel <= 0 ? 0 : std::max(0, g.entries[sel].firstLine + g.entries[sel].nLines - 7);
                        g.footer = footer;
                        g.footerId = 1843;
                        g.score = 100;
                        const MenuView v = PanelMenuViewOf(gs, h);
                        CHECK(v.valid && AllReachable(v));
                        // The game's arrows, none past the list's ends (Up from the first goal: the phone).
                        CHECK(v.scrollUp.empty() == (sel == 0) && (sel == 0 || v.scrollUp[0].x == kGoalsUpX));
                        CHECK(v.scrollDown.empty() == (sel >= 0 && sel == n - 1) && (v.scrollDown.empty() || v.scrollDown[0].y == kGoalsDownY));
                        if (sel == 0) CHECK(MenuMove(v, v.selected, 0) == (int)v.items.size() - 1);
                        if (sel >= 0)
                        {
                            CHECK(MenuFocusable(v, v.selected) && v.items[v.selected].id == kGoalsId + (u32)sel);
                        }
                        const Rect list = footer ? kGoalsListFooter : kGoalsList;
                        for (int i = 0; i + 1 < (int)v.items.size(); i++)
                            CHECK(v.items[i].rect.y >= list.y && v.items[i].rect.y + v.items[i].rect.h <= list.y + list.h);
                        const Composition comp = check(gs, v, h, v.selected >= 0 ? v.selected : 0);
                        // A goal the touch screen shows: its bar lands on its lines.
                        for (int i = 0; i + 1 < (int)v.items.size(); i++)
                        {
                            const GoalEntry& e = g.entries[v.items[i].id - kGoalsId];
                            if (e.firstLine < g.first || e.firstLine >= g.first + 7) continue;
                            const Rect r = v.items[i].rect;
                            CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + 2.5f, tx, ty));
                            CHECK(inside(tx, ty, {kGoalsLineX, kGoalsLineY0 + 16 * (e.firstLine - g.first), kGoalsLineW, 16}));
                        }
                    }
            // Relationships: 0, 1, 5 or 23 Sims, the game's first shown and selection.
            const ScreenState rs {Screen::Panel, Tab::Relationships};
            for (int n : {0, 1, 5, 23})
                for (int sel : {0, 2, 4, 22})
                {
                    if (sel >= std::max(n, 1)) continue;
                    GameState h;
                    h.valid = true;
                    RelState& r = h.town.relations;
                    r.valid = true;
                    for (int i = 0; i < n; i++) r.sims.push_back({i, 323 + i, 389 + i, 184, 195, -1, false});
                    r.selected = n ? sel : -1;
                    r.first = std::max(0, std::min(sel - 3, n - 4));
                    const MenuView v = PanelMenuViewOf(rs, h);
                    CHECK(v.valid && (int)v.items.size() == std::min(4, n) + 1);
                    if (n)
                    {
                        CHECK(MenuFocusable(v, v.selected) && v.selected == sel - r.first);
                        CHECK(v.scrollUp.empty() == (sel == 0) && v.scrollDown.empty() == (sel == n - 1));
                        if (sel > 0) CHECK(v.scrollUp[0].x == kRelPrevX && v.scrollUp[0].y == kRelPrevY);
                        if (sel + 1 < n) CHECK(v.scrollDown[0].y == kRelNextY);
                        if (sel == 0) CHECK(MenuMove(v, v.selected, 0) == (int)v.items.size() - 1); // the phone
                    }
                    const Composition comp = check(rs, v, h, v.selected);
                    for (int i = 0; i < std::min(4, n); i++)
                    {
                        const Rect q = v.items[i].rect;
                        CHECK(TouchFromComposition(comp, fit, q.x + q.w / 2.0f, q.y + q.h / 2.0f, tx, ty));
                        CHECK(inside(tx, ty, HotspotsFor(rs)[1 + i].area));
                    }
                }
            // Options: both volumes at each end and between.
            const ScreenState os {Screen::Panel, Tab::Options};
            for (int vol : {0, 1229, 4096})
            {
                GameState h;
                h.valid = true;
                h.town.options = {true, vol, 4096 - vol};
                const MenuView v = PanelMenuViewOf(os, h);
                CHECK(v.valid && v.items.size() == 5 && AllReachable(v));
                CHECK(v.items[0].action[0].x == kOptSaveX && v.items[1].action[0].y == kOptQuitY);
                CHECK(MenuMove(v, 0, 1) == 2 && MenuMove(v, 1, 1) == 2 && MenuMove(v, 2, 0) == 0 && MenuMove(v, 2, 1) == 3);
                CHECK(MenuMove(v, 0, 0) == 4 && MenuMove(v, 4, 1) == 0); // the phone above Save
                const int k = VolumeNotch(vol);
                const ViewItem& m = v.items[2];
                CHECK(m.left.empty() == (k == 0) && m.right.empty() == (k == 10));
                if (k > 0) CHECK(m.left[0].kind == MenuStep::Drag && m.left[0].x == 72 + 16 * k && m.left[0].x2 == 72 + 16 * (k - 1) && m.left[0].y == 121);
                if (k < 10) CHECK(m.right[0].x2 == 72 + 16 * (k + 1));
                if (VolumeNotch(4096 - vol) < 10) CHECK(v.items[3].right[0].y == 171);
                const Composition comp = check(os, v, h, 2);
                CHECK(TouchFromComposition(comp, fit, kOptSaveBtn.x + kOptSaveBtn.w / 2.0f, kOptSaveBtn.y + kOptSaveBtn.h / 2.0f, tx, ty));
                CHECK(inside(tx, ty, HotspotsFor(os)[1].area));
            }
            CHECK(VolumeNotch(0) == 0 && VolumeNotch(1229) == 3 && VolumeNotch(3686) == 9 && VolumeNotch(4096) == 10);
            // The Save slots: used, empty, the current one.
            const ScreenState ss {Screen::SaveSlots, Tab::None};
            for (int cur : {-1, 1})
            {
                GameState h;
                h.valid = true;
                SaveSlots& sv = h.town.saves;
                sv.valid = true;
                sv.current = cur;
                sv.used[1] = sv.slot[1].used = true;
                sv.slot[1] = {true, 9150, 0x14, 0, "Jun"};
                const MenuView v = SaveSlotsViewOf(ss, h);
                CHECK(v.valid && v.items.size() == 3 && AllReachable(v) && MenuFirst(v) == (cur < 0 ? 0 : cur) && !v.replay);
                CHECK(v.back.size() == 1 && v.back[0].x == kSlotCloseX && v.back[0].y == kSlotCloseY);
                for (int i = 0; i < 3; i++) CHECK(v.items[i].action[0].x == kSlotTickX && v.items[i].action[0].y == kSlotTickY[i]);
                const Composition comp = check(ss, v, h, 1);
                const Rect q = v.items[2].rect;
                CHECK(TouchFromComposition(comp, fit, q.x + q.w / 2.0f, q.y + q.h / 2.0f, tx, ty));
                CHECK(inside(tx, ty, {kSlotTickX - 12, kSlotTickY[2] - 12, 24, 24}));
                // No current slot, the first used: the first empty one.
                GameState e = h;
                e.town.saves.current = -1;
                e.town.saves.used[0] = true;
                CHECK(MenuFirst(SaveSlotsViewOf(ss, e)) == 2);
                // The game's question over a used slot (popup kind 2) or "Saving" (kind 3): the slots
                // wait (disabled, no B) under the popup (port/popup.h: its view takes the input).
                GameState pq = h;
                pq.town.popup = {true, 2, 2, "Overwrite this save?", "", -1, false, 48, 160, 6};
                const MenuView a = SaveSlotsViewOf(ss, pq);
                CHECK(a.valid && a.items.size() == 3 && MenuFirst(a) == -1 && a.back.empty());
                const MenuView ap = PopupViewOf(ss, pq);
                CHECK(ap.valid && ap.popup && ap.own && ap.items.size() == 1 && !ap.replay);
                CHECK(ap.items[0].action[0].x == 164 && ap.items[0].action[0].y == 108 && ap.back[0].x == 92 && ap.back[0].y == 108);
                pq.town.popup = {true, 2, 3, "Do not turn off", "Saving", -1, false, 48, 160, 6};
                const MenuView w = SaveSlotsViewOf(ss, pq);
                CHECK(w.valid && MenuFirst(w) == -1 && w.back.empty());
                check(ss, w, pq, -1);
                CHECK(!SaveSlotsViewOf({Screen::Panel, Tab::Options}, h).valid);
                CHECK(!SaveSlotsView(ss, &h, &rom).valid); // no strings: the stick cursor's screen
                h.town.saves.valid = false;
                CHECK(!SaveSlotsViewOf(ss, h).valid);
            }
        }
        // Popups, the phone and notes (port/popup.h, phone.h, note.h) on made-up states.
        {
            const CanvasFit fit = CanvasFit::For(kConsoleW, kConsoleH, 1280, 720);
            auto inside = [](int x, int y, Rect a) { return x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h; };
            int tx, ty, bx, by;
            // The game's buttons from its box (0x02057314): Game Paused / tips, the goal, the question.
            PopupState pp {true, 2, 0, "Press START", "Game Paused", -1, false, 40, 160, 10};
            CHECK(PopupButton(pp, 0, bx, by) && bx == 132 && by == 132 && !PopupButton(pp, 1, bx, by));
            pp.boxY = 56; pp.boxTiles = 6;
            CHECK(PopupButton(pp, 0, bx, by) && bx == 132 && by == 116);
            pp.kind = 2;
            CHECK(PopupButton(pp, 0, bx, by) && bx == 92 && by == 116 && PopupButton(pp, 1, bx, by) && bx == 164 && by == 116);
            pp.kind = 3;
            CHECK(!PopupButton(pp, 0, bx, by));
            pp.kind = 0; pp.boxW = 0;
            CHECK(!PopupButton(pp, 0, bx, by));
            // Over the panel (its open menu drawn beneath), Options > Quit's (Unknown), over the
            // phone; never over a screen the port does not redraw, nor without the box.
            for (int kind : {0, 2, 3})
                for (ScreenState on : {ScreenState {Screen::Popup, Tab::None}, ScreenState {Screen::Unknown, Tab::None}})
                {
                    GameState h;
                    h.valid = true;
                    h.screen = GScreen::Popup;
                    h.tab = on.screen == Screen::Unknown ? 4 : 0;
                    h.town.popup = {true, 2, kind, std::string(300, 'w'), kind == 2 ? "stale" : "Title", -1, false, 56, 160, 6};
                    h.town.options = {true, 4096, 2048};
                    const MenuView v = PopupViewOf(on, h);
                    CHECK(v.valid && v.own && v.popup && (int)v.items.size() == (kind == 3 ? 0 : 1) && v.back.empty() == (kind == 3));
                    CHECK(UnderScreen(on, h).screen == Screen::Panel);
                    Composition comp {kConsoleW, kConsoleH, {}, {}};
                    Piece world = ConsoleWorld(256, HudCropRows(192), true);
                    world.touch = false;
                    comp.pieces.push_back(world);
                    const ScreenState under = UnderScreen(on, h);
                    const MenuView u = PanelMenuViewOf(under, h);
                    if (u.valid) BuildPanelMenu(comp, under, u, 0, h, rom, nullptr, face.data());
                    std::vector<u32> crop(SW * SH, 0xFF808080);
                    comp.ui.push_back(ui::ImageItem({kPopBox.x + 10, kPopBox.y + 10, 60, 60}, crop.data(), SW, SH, {0, 0, 20, 20})); // (a picture under the box)
                    ui::UiItem barItem = ui::PanelItem({16, 8, 200, 40}); // (the status bar's panel: never under the veil)
                    barItem.layer = kStatusBarLayer;
                    comp.ui.push_back(barItem);
                    BuildPopup(comp, v, 0, h, rom);
                    SortUi(comp.ui);
                    int bars = 0;
                    for (const ui::UiItem& it : comp.ui) bars += it.kind != ui::UiItem::Image && it.layer == kPopLayer + 1 && it.to.x == 16 && it.to.w == 200;
                    CHECK(bars == 1);
                    int layer = 0;
                    for (const ui::UiItem& it : comp.ui)
                    {
                        if (it.kind == ui::UiItem::Image)
                        {
                            CHECK(!(it.to.x < kPopBox.x + kPopBox.w && kPopBox.x < it.to.x + it.to.w && it.to.y < kPopBox.y + kPopBox.h && kPopBox.y < it.to.y + it.to.h));
                            continue;
                        }
                        CHECK(it.layer >= layer);
                        layer = it.layer;
                        if (it.layer == kPopLayer && it.to.w < kConsoleW)
                            CHECK(it.to.x >= kPopBox.x - 8 && it.to.x + it.to.w <= kPopBox.x + kPopBox.w + 8 && it.to.y >= kPopTitle.y - 8 &&
                                  it.to.y + it.to.h <= kPopBox.y + kPopBox.h + 8);
                    }
                    // The mouse: the HD buttons onto the game's; the menu beneath takes none.
                    if (kind == 0)
                    {
                        CHECK(TouchFromComposition(comp, fit, kPopClose.x + kPopClose.w / 2.0f, kPopClose.y + kPopClose.h / 2.0f, tx, ty));
                        CHECK(inside(tx, ty, {120, 104, 24, 24}));
                    }
                    if (kind == 2)
                    {
                        CHECK(TouchFromComposition(comp, fit, kPopYes.x + kPopYes.w / 2.0f, kPopYes.y + kPopYes.h / 2.0f, tx, ty) && inside(tx, ty, {152, 104, 24, 24}));
                        CHECK(TouchFromComposition(comp, fit, kPopNo.x + kPopNo.w / 2.0f, kPopNo.y + kPopNo.h / 2.0f, tx, ty) && inside(tx, ty, {80, 104, 24, 24}));
                    }
                    CHECK(!TouchFromComposition(comp, fit, kPmPhoneWell.x + 10.5f, kPmPhoneWell.y + 10.5f, tx, ty)); // the menu's phone beneath
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        if (kind != 3) TestConsoleRoundtrip(on, true, 256, 192, o.first, o.second, true, &comp);
                    GameState off = h;
                    off.town.popup.state = 0;
                    CHECK(!PopupViewOf(on, off).valid);
                    off = h;
                    off.screen = GScreen::Vacuum; // (a screen the port does not redraw: milestone 1's popup)
                    CHECK(!PopupViewOf({Screen::Vacuum, Tab::None}, off).valid);
                    off.screen = GScreen::Build; // (over the HD Build Rooms: the HD popup)
                    CHECK(PopupViewOf({Screen::Build, Tab::None}, off).valid == PopupViewOf(on, h).valid);
                    off.screen = GScreen::Catalogue; // (over the HD catalogue: the HD popup)
                    CHECK(PopupViewOf({Screen::Catalogue, Tab::None}, off).valid == PopupViewOf(on, h).valid);
                    off = h;
                    off.town.popup.boxW = 0;
                    CHECK(PopupViewOf(on, off).valid == (kind == 3));
                    CHECK(!PopupView(on, &h, &rom).valid); // (no strings: milestone 1's)
                }
            // A popup the game shows over Save slots, Give Gift or the phone whose HD view does not
            // read (a box without its place): no view at all, not the screen's (milestone 1's picture
            // holds the game's own popup). (This rom has no strings: the guard itself is PopupOver's.)
            for (ScreenState on : {ScreenState {Screen::SaveSlots, Tab::None}, ScreenState {Screen::Gift, Tab::None}, ScreenState {Screen::Phone, Tab::None}})
            {
                GameState h;
                h.valid = true;
                h.screen = on.screen == Screen::Gift ? GScreen::ItemHand : GScreen::Phone;
                h.town.popup = {true, 2, 0, "Text", "Title", -1, false, 56, 0, 6}; // (boxW 0: no buttons)
                CHECK(PopupOver(on, h) && !PopupView(on, &h, &rom).valid);
                CHECK(!ConsoleView(on, &h, &rom).valid);
            }
            // The phone: every page as rows the game counts, the selection the game's, Up / Down its
            // DS keys (as many presses as rows between), A the keys then OK; B its X.
            auto phone = [&](int page, int row, int scroll, int rows) {
                GameState h;
                h.valid = true;
                h.screen = GScreen::Phone;
                PhoneState& p = h.town.phone;
                p.valid = true;
                p.page = page; p.row = row; p.scroll = scroll;
                p.month = 11; p.day = 5;
                for (int i = 0; i < 4; i++) p.menuIds[i] = 0x812 + i;
                for (int i = 0; i < rows && page == 2; i++) p.inbox.push_back({i, 0x907, 45, 368, i % 2 == 0, {}, {}});
                if (page == 3) { p.inbox.push_back({0, 0x907, 45, 368, true, {}, {}}); p.shownSlot = 0; p.textEnd = 10; }
                if (page == 7) { for (int i = 0; i < rows; i++) p.contactList.push_back(i); p.contacts = rows; p.contactsKnown = true; }
                if (page == 9 || page == 6 || page == 8) { p.textEnd = 5; p.caller = 2; p.reply = 0x81F; p.callMood = 1; p.callPlace = 3; }
                if (page == 5) p.maidCost = 2270;
                return h;
            };
            auto presses = [](const MenuAction& a, int key) {
                int n = 0;
                for (const MenuStep& st : a) n += st.kind == MenuStep::Keys && st.keys == (1u << key);
                return n;
            };
            const ScreenState ps {Screen::Phone, Tab::None};
            for (auto [page, row, scroll, rows] : std::vector<std::tuple<int, int, int, int>> {
                     {0, 0, 0, 0}, {0, 3, 0, 0}, {2, 1, 0, 3}, {2, 10, 7, 10}, {3, 0, 0, 0}, {3, 4, 3, 0}, {3, 12, 10, 0}, {4, 5, 2, 0}, {5, 1, 0, 0},
                     {7, 20, 18, 24}, {7, 0, 0, 0}, {9, 0, 0, 0}, {6, 5, 2, 0}, {8, 1, 0, 0}, {10, 0, 0, 0}})
            {
                const GameState h = phone(page, row, scroll, rows);
                const MenuView v = PhoneViewOf(ps, h, &rom);
                CHECK(v.valid && v.own && !v.popup && v.back.size() == 1 && v.back[0].x == kPhoneClose.x && v.backCloses);
                if (page == 10) { CHECK(v.items.empty()); continue; }
                CHECK(MenuFocusable(v, v.selected));
                const ViewItem& sel = v.items[v.selected];
                CHECK(sel.action.back().kind == MenuStep::Keys && sel.action.back().keys == (1u << kA));
                const PhonePage pg = PhonePageOf(h, rom);
                CHECK(pg.valid && !pg.crop && !pg.rows.empty());
                // The selection's row: the game's (or, in a text, the first row after it).
                const int selRow = (int)(sel.id - kPhRowId);
                CHECK(selRow == row || (row < pg.rows.front().row && selRow == pg.rows.front().row));
                CHECK(presses(sel.action, kDown) == std::max(0, selRow - row) && presses(sel.action, kUp) == std::max(0, row - selRow));
                if (!v.scrollDown.empty()) CHECK(presses(v.scrollDown, kDown) >= 1 && presses(v.scrollDown, kUp) == 0);
                CHECK(v.scrollDown.empty() == (selRow == pg.rows.back().row));
                CHECK((int)v.items.size() <= kPhRows);
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                BuildPhone(comp, v, v.selected, h, rom, nullptr, face.data(), nullptr);
                for (const ui::UiItem& it : comp.ui)
                    CHECK(it.to.x >= kPhPanel.x && it.to.y >= kPhPanel.y && it.to.x + it.to.w <= kPhPanel.x + kPhPanel.w && it.to.y + it.to.h <= kPhPanel.y + kPhPanel.h);
                for (const ViewItem& it : v.items) CHECK(it.rect.y >= kPhList.y && it.rect.y + it.rect.h <= kPhList.y + kPhList.h);
                CHECK(TouchFromComposition(comp, fit, kPhOk.x + kPhOk.w / 2.0f, kPhOk.y + kPhOk.h / 2.0f, tx, ty) && inside(tx, ty, {kPhOkX - 12, kPhBtnY - 12, 24, 24}));
                CHECK(TouchFromComposition(comp, fit, kPhDown.x + kPhDown.w / 2.0f, kPhDown.y + kPhDown.h / 2.0f, tx, ty) && inside(tx, ty, {kPhDownX - 12, kPhBtnY - 12, 24, 24}));
                CHECK(TouchFromComposition(comp, fit, kPhX.x + kPhX.w / 2.0f, kPhX.y + kPhX.h / 2.0f, tx, ty) && inside(tx, ty, kPhoneClose.area));
            }
            // A list the game's own count disagrees with: its LCD, live, the same keys.
            GameState odd = phone(7, 2, 0, 5);
            odd.town.phone.contactsKnown = false;
            const MenuView ov = PhoneViewOf(ps, odd, &rom);
            CHECK(ov.valid && ov.items.size() == 1 && ov.selected == 0 && presses(ov.scrollDown, kDown) == 1 && presses(ov.scrollUp, kUp) == 1);
            CHECK(PhonePageOf(odd, rom).crop);
            odd.town.phone.contacts = 0; // (the list being built: nothing yet)
            CHECK(!PhonePageOf(odd, rom).crop && PhoneViewOf(ps, odd, &rom).items.empty());
            odd.town.phone.contactsWait = kPhContactsGrace + 1; // (never agreeing: after a few frames, its LCD)
            CHECK(PhonePageOf(odd, rom).crop && PhoneViewOf(ps, odd, &rom).items.size() == 1);
            CHECK(!PhoneViewOf({Screen::Panel, Tab::Interact}, phone(0, 0, 0, 0), &rom).valid);
            CHECK(!PhoneView(ps, nullptr, &rom).valid && !PhoneView(ps, &odd, &rom).valid); // (no strings)
            // A note: A and B press the DS's A (the game closes it so); its X onto the game's.
            GameState n;
            n.valid = true;
            n.town.note = {true, 0x4A8, true};
            const MenuView nv = NoteViewOf({Screen::Unknown, Tab::None}, n);
            CHECK(nv.valid && nv.own && nv.items.size() == 1 && nv.items[0].action[0].kind == MenuStep::Keys && nv.back == nv.items[0].action);
            CHECK(!NoteViewOf({Screen::Panel, Tab::Interact}, n).valid);
            Composition nc {kConsoleW, kConsoleH, {}, {}};
            BuildNote(nc, nv, 0, n, rom, nullptr);
            for (const ui::UiItem& it : nc.ui)
                CHECK(it.to.x >= kNotePanel.x - 6 && it.to.y >= kNotePanel.y - 6 && it.to.x + it.to.w <= kNotePanel.x + kNotePanel.w && it.to.y + it.to.h <= kNotePanel.y + kNotePanel.h);
            CHECK(TouchFromComposition(nc, fit, kNoteX.x + kNoteX.w / 2.0f, kNoteX.y + kNoteX.h / 2.0f, tx, ty) && inside(tx, ty, {kNoteCloseX - 14, kNoteCloseY - 14, 28, 28}));
            n.town.note.valid = false;
            CHECK(!NoteViewOf({Screen::Unknown, Tab::None}, n).valid);
        }
        // BuildConsole: without the game's strings, milestone 1's menu (the touch screen 3x).
        const HudState hud = TestHud();
        const HudStrings strings = TestStrings();
        const Composition m1 = BuildConsole(it, true, 0, 256, 192, &hud, &strings, &g, &rom, face.data());
        bool whole = false;
        for (const Piece& q : m1.pieces) whole |= q.src == Src::Bottom && q.from.w == SW && q.from.h == SH && q.to.w == SW * 3;
        CHECK(whole);
        // FitLine: shrinks, then cuts with an ellipsis, never wider than asked.
        const FittedText f1 = FitLine("Hotseat", 30, 22, 400);
        CHECK(f1.fits && f1.size == 30 && f1.text == "Hotseat");
        const FittedText f2 = FitLine(std::string(60, 'W'), 30, 22, 300);
        CHECK(!f2.fits && f2.size == 22 && ui::TextWidth(f2.text, 22) <= 300 && f2.text.size() > 3);
        const FittedText f3 = FitLine("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 30, 30, 40); // accents never split
        CHECK(!f3.fits && f3.text.size() % 2 == 1);
        // Mixed letters: only whole letters go, never more than needed (an ASCII letter cut alone,
        // not the accented one before it).
        const std::string mixed = "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9";
        for (int w = 20; w < 140; w += 7)
        {
            const FittedText f = FitLine(mixed, 30, 30, w);
            if (f.fits) continue;
            const std::string kept = f.text.substr(0, f.text.size() - 3);
            CHECK(mixed.compare(0, kept.size(), kept) == 0);
            CHECK(kept.empty() || ((unsigned char)kept.back() & 0xC0) != 0xC0); // no letter cut in two
            // One more letter of the original would not fit.
            size_t next = kept.size();
            if (next < mixed.size()) { next++; while (next < mixed.size() && ((unsigned char)mixed[next] & 0xC0) == 0x80) next++; }
            CHECK(ui::TextWidth(mixed.substr(0, next) + "\xE2\x80\xA6", 30) > w || next == mixed.size());
        }
    }

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("layout_test: ok\n");
    return 0;
}
