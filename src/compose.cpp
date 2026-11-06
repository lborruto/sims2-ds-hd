#include "compose.h"

#include <algorithm>
#include <cmath>

#include "ui.h"

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
    // (The top screen is the camera; drawn over the bottom one, it takes the clicks there.)
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

bool TouchClampedComposition(const Composition& c, int pieceIndex, const CanvasFit& fit, float px, float py, int& tx, int& ty)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    auto usable = [&](int i) { return i >= 0 && i < (int)c.pieces.size() && c.pieces[i].src == Src::Bottom && c.pieces[i].touch; };
    // The piece the drag began on; if the layout changed since (the indices with it), the
    // touchable bottom piece under the point, else the largest; none (a held CONSOLE picture,
    // a black load): no touch.
    int at = pieceIndex;
    if (!usable(at))
    {
        int under = -1, largest = -1;
        long largestArea = -1;
        for (int i = 0; i < (int)c.pieces.size(); i++)
        {
            if (!usable(i)) continue;
            const Rect& r = c.pieces[i].to;
            if (Inside(r, cx, cy)) under = i; // the later one is on top
            if ((long)r.w * r.h > largestArea) { largestArea = (long)r.w * r.h; largest = i; }
        }
        at = under >= 0 ? under : largest;
        if (at < 0) return false;
    }
    const Piece& q = c.pieces[at];
    const float x = q.from.x + (cx - q.to.x) * q.from.w / q.to.w, y = q.from.y + (cy - q.to.y) * q.from.h / q.to.h;
    // Held to the piece's source rectangle, itself held to the DS screen (lo <= hi always).
    const int x0 = std::clamp(q.from.x, 0, SW - 1), x1 = std::clamp(q.from.x + q.from.w - 1, x0, SW - 1);
    const int y0 = std::clamp(q.from.y, 0, SH - 1), y1 = std::clamp(q.from.y + q.from.h - 1, y0, SH - 1);
    tx = (int)std::clamp(std::floor(x), (float)x0, (float)x1);
    ty = (int)std::clamp(std::floor(y), (float)y0, (float)y1);
    return true;
}

bool OverTopComposition(const Composition& c, const CanvasFit& fit, float px, float py)
{
    float cx, cy;
    fit.ToCanvas(px, py, cx, cy);
    for (int i = (int)c.pieces.size() - 1; i >= 0; i--)
    {
        const Piece& q = c.pieces[i];
        if (!Inside(q.to, cx, cy)) continue;
        return q.src == Src::Top && q.touch;
    }
    return false;
}

Rect TextRect(int x, int y, const char* text, int scale)
{
    int lines = 1, chars = 0, longest = 0;
    for (const char* c = text; *c; c++)
        if (*c == '\n') { lines++; chars = 0; }
        else longest = std::max(longest, ++chars);
    return {x, y, longest * 6 * scale, (lines * 9 - 1) * scale};
}

void RasteriseOverlay(const Overlay& o, int w, int h, std::vector<u32>& px)
{
    px.assign((size_t)std::max(w, 0) * std::max(h, 0), 0);
    if (w <= 0 || h <= 0 || o.to.w <= 0 || o.to.h <= 0) return;
    const float sx = (float)w / o.to.w, sy = (float)h / o.to.h, s = std::min(sx, sy);
    if (o.text)
    {
        // Each font pixel a block of the output's pixels: the shadow first, then the text.
        const int sc = std::max(o.textScale, 1);
        auto block = [&](int fx, int fy, u32 argb) {
            const int x0 = (int)(fx * sc * sx), x1 = (int)((fx + 1) * sc * sx);
            const int y0 = (int)(fy * sc * sy), y1 = (int)((fy + 1) * sc * sy);
            for (int y = std::max(y0, 0); y < std::min(y1, h); y++)
                for (int x = std::max(x0, 0); x < std::min(x1, w); x++) px[(size_t)y * w + x] = argb;
        };
        for (int pass = o.borderArgb ? 0 : 1; pass < 2; pass++)
        {
            int cx = 0, cy = 0;
            for (const char* c = o.text; *c; c++, cx += 6)
                if (*c == '\n') { cx = -6; cy += 9; }
                else if (const u8* g = Glyph(*c))
                    for (int row = 0; row < 7; row++)
                        for (int col = 0; col < 5; col++)
                            if (g[row] & (0x10 >> col)) block(cx + col + (pass ? 0 : 1), cy + row + (pass ? 0 : 1), pass ? o.argb : o.borderArgb);
        }
        return;
    }
    const float r = std::min(o.radius * s, std::min(w, h) / 2.0f), b = o.border * s;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            // The signed distance from the pixel's centre to the rounded rectangle's edge
            // (negative inside).
            const float qx = std::fabs(x + 0.5f - w / 2.0f) - (w / 2.0f - r), qy = std::fabs(y + 0.5f - h / 2.0f) - (h / 2.0f - r);
            const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
            const float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
            if (d > 0) continue;
            px[(size_t)y * w + x] = b > 0 && d > -b ? o.borderArgb : o.argb;
        }
}

bool CanvasFromTouch(const Composition& c, float tx, float ty, float& cx, float& cy, float& scale)
{
    for (int i = (int)c.pieces.size() - 1; i >= 0; i--)
    {
        const Piece& q = c.pieces[i];
        if (q.src != Src::Bottom || !q.touch || !Inside(q.from, tx, ty)) continue;
        scale = (float)q.to.w / q.from.w;
        cx = q.to.x + (tx - q.from.x) * scale;
        cy = q.to.y + (ty - q.from.y) * q.to.h / q.from.h;
        return true;
    }
    return false;
}

Rect ZoomRect(Rect r, float z, float ax, float ay)
{
    const int x0 = (int)std::lround(ax + z * (r.x - ax)), y0 = (int)std::lround(ay + z * (r.y - ay));
    const int x1 = (int)std::lround(ax + z * (r.x + r.w - ax)), y1 = (int)std::lround(ay + z * (r.y + r.h - ay));
    return {x0, y0, x1 - x0, y1 - y0};
}

namespace
{
bool Fixed(const ui::UiItem& it, int cw)
{
    return it.to.x <= 0 && it.to.x + it.to.w >= cw;
}
bool Zoomed(const ui::UiItem& it) { return it.zoom != 1 || it.dx != 0 || it.dy != 0; }
bool MousePiece(const Piece& p) { return p.src == Src::Bottom && p.alpha == 0 && p.touch; }
}

float ZoomUi(Composition& c, size_t item0, size_t piece0, float z, float ax, float ay, Rect limit)
{
    if (z == 1) return 1;
    // The group's box: its items' rectangles (and its mouse pieces').
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool any = false;
    auto add = [&](Rect r) {
        if (r.w <= 0 || r.h <= 0) return;
        if (!any) { x0 = r.x; y0 = r.y; x1 = r.x + r.w; y1 = r.y + r.h; any = true; return; }
        x0 = std::min(x0, r.x); y0 = std::min(y0, r.y); x1 = std::max(x1, r.x + r.w); y1 = std::max(y1, r.y + r.h);
    };
    for (size_t i = item0; i < c.ui.size(); i++)
        if (!Fixed(c.ui[i], c.cw) && !Zoomed(c.ui[i])) add(c.ui[i].to);
    for (size_t i = piece0; i < c.pieces.size(); i++)
        if (MousePiece(c.pieces[i])) add(c.pieces[i].to);
    if (!any) return 1;
    float f = ui::FitZoom({x0, y0, x1 - x0, y1 - y0}, z, ax, ay, limit);
    if (z > 1) f = std::max(f, 1.0f); // (larger only as far as it fits; never smaller for it)
    if (f == 1) return 1;
    for (size_t i = item0; i < c.ui.size(); i++)
        if (!Fixed(c.ui[i], c.cw) && !Zoomed(c.ui[i])) ui::ZoomItem(c.ui[i], f, ax, ay);
    for (size_t i = piece0; i < c.pieces.size(); i++)
        if (MousePiece(c.pieces[i])) c.pieces[i].to = ZoomRect(c.pieces[i].to, f, ax, ay);
    return f;
}
}
