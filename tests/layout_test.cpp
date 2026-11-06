// Checks the layouts' touch mapping: every DS touch pixel, drawn in each layout at several window
// sizes (HiDPI too), maps back to itself from the centre of where it was drawn; the top screen
// is never touchable, FOCUS's panel wins over the world under it, and a drag past the touch
// screen is held to its edges.
//   ninja -C build layout_test && ./build/layout_test
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "compose.h"
#include "console.h"
#include "hotspots.h"
#include "layout.h"
#include "port/dialogue.h"
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

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("layout_test: ok\n");
    return 0;
}
