// Checks the layouts' touch mapping: every DS touch pixel, drawn in each layout at several window
// sizes (HiDPI too), maps back to itself from the centre of where it was drawn; the top screen
// is never touchable, FOCUS's panel wins over the world under it, and a drag past the touch
// screen is held to its edges.
//   ninja -C build layout_test && ./build/layout_test
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "compose.h"
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

int main()
{
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
