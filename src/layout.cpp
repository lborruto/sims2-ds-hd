#include "layout.h"

#include <algorithm>
#include <cmath>

namespace sims2wide
{
static int Wide(int h, float aspect) { return (int)std::lround(h * aspect); }

// FOCUS: the main screen fills the canvas's height (720); the other one is the side panel. With
// the world big, the panel (kPanelW x kPanelH, twice the DS screen) sits at the right
// edge, over the world's edge, a little see-through: the DS's 4:3 world leaves a 320-wide column
// beside it, which is too narrow for a readable panel, and a wider world leaves none. The world's
// status bar is at its bottom, which the panel (centred) leaves clear. With the touch screen big
// (Tab) nothing covers it: the world goes small into the column.
Placement Place(Layout l, bool mainIsTop, float topAspect)
{
    const int topW = Wide(SH, topAspect);
    switch (l)
    {
    case Layout::Stacked:
    {
        const int cw = std::max(topW, SW);
        return {cw, SH * 2, {(cw - topW) / 2, 0, topW, SH}, {(cw - SW) / 2, SH, SW, SH}};
    }
    case Layout::Side: return {topW + SW, SH, {0, 0, topW, SH}, {topW, 0, SW, SH}};
    case Layout::Focus:
    default:
    {
        if (mainIsTop)
        {
            const int worldW = Wide(720, topAspect), cw = std::max(1280, worldW);
            Placement p {cw, 720, {0, 0, worldW, 720}, {cw - kPanelW, (720 - kPanelH) / 2, kPanelW, kPanelH}};
            p.alpha = 232;
            return p;
        }
        const int smallH = Wide(320, 1.0f / topAspect);
        Placement p {1280, 720, {960, (720 - smallH) / 2, 320, smallH}, {0, 0, 960, 720}};
        p.topFirst = false;
        return p;
    }
    }
}

CanvasFit CanvasFit::For(int cw, int ch, int ow, int oh)
{
    float scale = std::min((float)ow / cw, (float)oh / ch);
    return {scale, (ow - cw * scale) / 2, (oh - ch * scale) / 2};
}

void CanvasFit::ToOutput(const Rect& r, int& x, int& y, int& w, int& h) const
{
    x = (int)(ox + r.x * scale);
    y = (int)(oy + r.y * scale);
    w = (int)(r.w * scale);
    h = (int)(r.h * scale);
}

bool TouchFromOutput(const Placement& p, const CanvasFit& fit, float px, float py, int& tx, int& ty)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    const Rect& b = p.bottom;
    if (!p.Contains(b, cx, cy)) return false;
    // (The top screen drawn over the bottom one: not touchable there.)
    if (p.topFirst == false && p.Contains(p.top, cx, cy)) return false;
    tx = (int)((cx - b.x) * SW / b.w);
    ty = (int)((cy - b.y) * SH / b.h);
    return true;
}

void TouchClamped(const Placement& p, const CanvasFit& fit, float px, float py, int& tx, int& ty)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    const Rect& b = p.bottom;
    const float x = (cx - b.x) * SW / b.w, y = (cy - b.y) * SH / b.h;
    tx = x < 0 ? 0 : x > SW - 1 ? SW - 1 : (int)x;
    ty = y < 0 ? 0 : y > SH - 1 ? SH - 1 : (int)y;
}

bool OverTop(const Placement& p, const CanvasFit& fit, float px, float py)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    if (!p.Contains(p.top, cx, cy)) return false;
    return !(p.topFirst && p.Contains(p.bottom, cx, cy));
}

const char* LayoutName(Layout l)
{
    switch (l)
    {
    case Layout::Stacked: return "stacked";
    case Layout::Side: return "side";
    case Layout::Focus: return "focus";
    default: return "auto";
    }
}
}
