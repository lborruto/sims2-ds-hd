// Checks the layouts' touch mapping: every DS touch pixel, drawn in each layout at several window
// sizes (HiDPI too), maps back to itself from the centre of where it was drawn; the top screen
// is never touchable, FOCUS's panel wins over the world under it, and a drag past the touch
// screen is held to its edges.
//   ninja -C build layout_test && ./build/layout_test
#include <cstdio>
#include <cstdlib>

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

int main()
{
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
