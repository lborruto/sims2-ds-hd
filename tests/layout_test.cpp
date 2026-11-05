// Checks the layouts' touch mapping: every DS touch pixel, drawn in each layout at several window
// sizes (HiDPI too), maps back to itself from the centre of where it was drawn; the top screen
// is never touchable, FOCUS's panel wins over the world under it, and a drag past the touch
// screen is held to its edges.
//   ninja -C build layout_test && ./build/layout_test
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "compose.h"
#include "console.h"
#include "hotspots.h"
#include "layout.h"

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
                      Screen::Vacuum, Screen::Cards, Screen::Catalogue, Screen::Build, Screen::Unknown})
        if (sc == Screen::Panel)
            for (Tab t : {Tab::None, Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options}) v.push_back({sc, t});
        else v.push_back({sc, Tab::None});
    return v;
}

static void TestConsoleRoundtrip(ScreenState s, bool menuOpen, int topW, int topH, int ow, int oh)
{
    const Composition c = BuildConsole(s, menuOpen, -1, topW, topH);
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
    CHECK(bottoms > 0);
}

int main()
{
    // CONSOLE's compositions.
    for (ScreenState s : AllStates())
        for (bool menuOpen : {false, true})
            for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                    TestConsoleRoundtrip(s, menuOpen, top.first, top.second, o.first, o.second);
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
        const Composition side = BuildConsole({Screen::None, Tab::None}, false, -1, SW, SH);
        CHECK(side.cw == SW * 2 && side.ch == SH);
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
        const Composition act = BuildConsole({Screen::Unknown, Tab::None}, false, -1, SW, SH);
        CHECK(CanvasFromTouch(act, 10, 20, cx, cy, sc) && sc == 3 && cx == 36 + 30 && cy == 68 + 60);
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

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("layout_test: ok\n");
    return 0;
}
