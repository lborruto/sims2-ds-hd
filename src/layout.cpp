#include "layout.h"

#include <algorithm>

namespace sims2wide
{
// FOCUS: the main screen fills a 4:3 area, the other one sits in a column on the right (16:9
// canvas overall).
Placement Place(Layout l, bool mainIsTop)
{
    switch (l)
    {
    case Layout::Stacked: return {SW, SH * 2, {0, 0, SW, SH}, {0, SH, SW, SH}};
    case Layout::Side:    return {SW * 2, SH, {0, 0, SW, SH}, {SW, 0, SW, SH}};
    case Layout::Focus:
    default:
    {
        Rect big {0, 0, 960, 720}, small {960, 240, 320, 240};
        return mainIsTop ? Placement {1280, 720, big, small} : Placement {1280, 720, small, big};
    }
    }
}

CanvasFit CanvasFit::For(const Placement& p, int ow, int oh)
{
    float scale = std::min((float)ow / p.cw, (float)oh / p.ch);
    return {scale, (ow - p.cw * scale) / 2, (oh - p.ch * scale) / 2};
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
    float cx = (px - fit.ox) / fit.scale, cy = (py - fit.oy) / fit.scale;
    const Rect& b = p.bottom;
    if (cx < b.x || cy < b.y || cx >= b.x + b.w || cy >= b.y + b.h) return false;
    tx = (int)((cx - b.x) * SW / b.w);
    ty = (int)((cy - b.y) * SH / b.h);
    return true;
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
