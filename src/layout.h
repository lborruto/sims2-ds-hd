// Where the two DS screens go in the window. Each layout places them on a logical canvas, which
// the window scales to fit (letterboxed); drawing and mouse-to-touch mapping both go through
// Place() and CanvasFit, so they always agree.
#pragma once
#include "common.h"

namespace sims2wide
{
enum class Layout { Stacked, Side, Focus, Auto };
struct Rect { int x, y, w, h; };
struct Placement { int cw, ch; Rect top, bottom; };

// The canvas for a layout (not Auto: resolve it first). mainIsTop: which screen FOCUS makes big.
Placement Place(Layout l, bool mainIsTop);

// How a canvas sits in an output of ow x oh pixels: uniform scale, centred.
struct CanvasFit
{
    float scale, ox, oy;
    static CanvasFit For(const Placement& p, int ow, int oh);
    // Canvas rectangle -> output pixels.
    void ToOutput(const Rect& r, int& x, int& y, int& w, int& h) const;
};

// An output pixel position on the bottom screen, as a DS touch position; false when outside it.
bool TouchFromOutput(const Placement& p, const CanvasFit& fit, float px, float py, int& tx, int& ty);

const char* LayoutName(Layout l);
}
