#include "compose.h"

#include <algorithm>
#include <cmath>

namespace sims2wide
{
Composition FromPlacement(const Placement& p, int topW, int topH)
{
    const Piece top {Src::Top, {0, 0, topW, topH}, p.top};
    const Piece bottom {Src::Bottom, {0, 0, SW, SH}, p.bottom};
    Composition c {p.cw, p.ch, {}, {}};
    // The one drawn second may lie over the first, with the placement's opacity.
    c.pieces = p.topFirst ? std::vector<Piece> {top, bottom} : std::vector<Piece> {bottom, top};
    c.pieces[1].alpha = p.alpha;
    // (The top screen drawn over the bottom one is not touchable through.)
    c.pieces[p.topFirst ? 0 : 1].touch = false;
    return c;
}

Composition Compose(Layout l, bool mainIsTop, float topAspect, int topW, int topH)
{
    return FromPlacement(Place(l, mainIsTop, topAspect), topW, topH);
}

static bool Inside(const Rect& r, float cx, float cy) { return cx >= r.x && cy >= r.y && cx < r.x + r.w && cy < r.y + r.h; }

bool TouchFromComposition(const Composition& c, const CanvasFit& fit, float px, float py, int& tx, int& ty, int* pieceIndex)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    for (int i = (int)c.pieces.size() - 1; i >= 0; i--)
    {
        const Piece& q = c.pieces[i];
        if (!Inside(q.to, cx, cy)) continue;
        if (q.src != Src::Bottom || !q.touch) return false;
        tx = q.from.x + (int)((cx - q.to.x) * q.from.w / q.to.w);
        ty = q.from.y + (int)((cy - q.to.y) * q.from.h / q.to.h);
        if (pieceIndex) *pieceIndex = i;
        return true;
    }
    return false;
}

void TouchClampedComposition(const Composition& c, int pieceIndex, const CanvasFit& fit, float px, float py, int& tx, int& ty)
{
    const Piece& q = c.pieces[pieceIndex];
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    const float x = q.from.x + (cx - q.to.x) * q.from.w / q.to.w, y = q.from.y + (cy - q.to.y) * q.from.h / q.to.h;
    tx = std::clamp((int)std::floor(x), q.from.x, q.from.x + q.from.w - 1);
    ty = std::clamp((int)std::floor(y), q.from.y, q.from.y + q.from.h - 1);
}

bool OverTopComposition(const Composition& c, const CanvasFit& fit, float px, float py)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    for (int i = (int)c.pieces.size() - 1; i >= 0; i--)
    {
        const Piece& q = c.pieces[i];
        if (!Inside(q.to, cx, cy)) continue;
        if (q.src == Src::Top) return true;
        if (q.touch) return false;
    }
    return false;
}

Rect BottomPieceRect(const Composition& c)
{
    for (const Piece& q : c.pieces)
        if (q.src == Src::Bottom) return q.to;
    return {0, 0, 0, 0};
}
}
