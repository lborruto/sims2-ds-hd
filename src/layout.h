// Where the two DS screens go in the window. Each layout places them on a logical canvas, which
// the window scales to fit (letterboxed); drawing and mouse-to-touch mapping both go through
// the layout's composition (compose.h, made from Place()) and CanvasFit, so they always agree.
#pragma once
#include "common.h"

namespace sims2wide
{
// Console: one 16:9 screen built per recognised bottom screen (console.h); Place() gives it
// FOCUS's placement, which nothing draws.
enum class Layout { Stacked, Side, Focus, Auto, Console };
struct Rect { int x, y, w, h; };
struct Placement
{
    int cw, ch;
    Rect top, bottom;
    // The screens may overlap (FOCUS: the panel over the world's edge): the one drawn second is
    // on top, with this opacity (255: opaque).
    bool topFirst = true;
    u8 alpha = 255;
    bool Contains(const Rect& r, float cx, float cy) const { return cx >= r.x && cy >= r.y && cx < r.x + r.w && cy < r.y + r.h; }
};

// The shape of the DS's top screen, width / height.
constexpr float kDSAspect = 4.0f / 3.0f;
// The side panel of FOCUS: the touch screen at twice the DS's size, over the world's right edge.
constexpr int kPanelW = SW * 2, kPanelH = SH * 2;

// The canvas for a layout (not Auto: resolve it first). mainIsTop: which screen FOCUS makes big.
// topAspect: the top picture's shape (App::topAspect; wider than the DS's when the widescreen
// world is on): the top screen is sized from it.
Placement Place(Layout l, bool mainIsTop, float topAspect = kDSAspect);

// How a canvas sits in an output of ow x oh pixels: uniform scale, centred.
struct CanvasFit
{
    float scale, ox, oy;
    static CanvasFit For(const Placement& p, int ow, int oh) { return For(p.cw, p.ch, ow, oh); }
    static CanvasFit For(int cw, int ch, int ow, int oh);
    // Canvas rectangle -> output pixels.
    void ToOutput(const Rect& r, int& x, int& y, int& w, int& h) const;
    // Output pixels -> canvas position.
    void ToCanvas(float px, float py, float& cx, float& cy) const { cx = (px - ox) / scale; cy = (py - oy) / scale; }
};

// An output pixel position on the bottom screen, as a DS touch position; false when outside it.
bool TouchFromOutput(const Placement& p, const CanvasFit& fit, float px, float py, int& tx, int& ty);
// The same, held to the bottom screen's edges wherever the point is: a stylus drag that started
// on the touch screen (dragging an item, tracing) stays on it when the mouse overshoots.
void TouchClamped(const Placement& p, const CanvasFit& fit, float px, float py, int& tx, int& ty);
// Whether an output pixel position is on the top screen's picture (and not under the panel).
bool OverTop(const Placement& p, const CanvasFit& fit, float px, float py);

const char* LayoutName(Layout l);
}
