// A composition: what is drawn in the window as a list of pieces, each a rectangle of one DS
// screen drawn at a rectangle of the canvas, in order (a later piece lies over an earlier one),
// with an opacity. Drawing, the shots' --compose picture and the mouse's mapping to the stylus
// all go through it, so they always agree. The layouts (layout.h) are compositions of two
// pieces; richer ones (the console layout) are more.
#pragma once
#include <vector>

#include "layout.h"

namespace sims2wide
{
enum class Src { Top, Bottom };

struct Piece
{
    Src src;
    Rect from;        // in the source's own pixels (for Src::Top: the top picture's size, App::TopImage)
    Rect to;          // in canvas pixels
    u8 alpha = 255;
    bool touch = true; // false: the mouse is not a stylus here and does not reach what lies under
};

// A frame drawn in the game's colours around or behind pieces (filled in by the console layout).
struct Overlay
{
    Rect to;
    u32 argb;
    int radius;
    int border;
    u32 borderArgb;
};

struct Composition
{
    int cw, ch;
    std::vector<Piece> pieces;
    std::vector<Overlay> overlays;
};

// A two-screen placement as pieces, in drawing order (FOCUS's panel keeps its opacity).
// topW x topH: the top picture's size.
Composition FromPlacement(const Placement& p, int topW, int topH);

// The shown layout's composition: topAspect shapes the top screen (Place()), topW x topH is the
// top picture's size.
Composition Compose(Layout l, bool mainIsTop, float topAspect, int topW, int topH);

// An output pixel position as a DS touch position: the topmost piece under it decides, and it
// must be a touchable Src::Bottom piece (the top screen, or an untouchable piece, lies over
// what is under it and takes the click). pieceIndex, when given, receives the piece hit.
bool TouchFromComposition(const Composition& c, const CanvasFit& fit, float px, float py, int& tx, int& ty, int* pieceIndex = nullptr);
// The same, held to one piece's source rectangle wherever the point is: a drag stays on the
// piece it started on. A stale index (the layout changed meanwhile) falls back to the last
// touchable bottom piece; the result is always within the DS screen.
void TouchClampedComposition(const Composition& c, int pieceIndex, const CanvasFit& fit, float px, float py, int& tx, int& ty);
// Whether an output pixel position is on a top piece and no later touchable bottom piece
// covers it (the mouse camera's area).
bool OverTopComposition(const Composition& c, const CanvasFit& fit, float px, float py);
// The first Src::Bottom piece's destination (where the stylus cursor is drawn); a zero Rect
// without one.
Rect BottomPieceRect(const Composition& c);
}
