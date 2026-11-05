// A composition: what is drawn in the window as a list of pieces, each a rectangle of one DS
// screen drawn at a rectangle of the canvas, in order (a later piece lies over an earlier one),
// with an opacity. Drawing, the shots' --compose picture and the mouse's mapping to the stylus
// all go through it, so they always agree. The layouts (layout.h) are compositions of two
// pieces; richer ones (the console layout) are more.
#pragma once
#include <vector>

#include "layout.h"
#include "ui/kit.h"

namespace sims2wide
{
enum class Src { Top, Bottom };

struct Piece
{
    Src src;
    Rect from;        // in the source's own pixels (for Src::Top: the top picture's size, App::TopImage)
    Rect to;          // in canvas pixels
    u8 alpha = 255;
    // What the mouse does here. true: a Src::Bottom piece is the stylus, a Src::Top piece the
    // camera (wheel, right drag). false: nothing at all, and the mouse does not reach what lies
    // under it either (a click is swallowed, the camera does not turn): the console's dimmed
    // world behind its menu, which is not playable while the menu is up.
    bool touch = true;
};

// A rounded rectangle drawn in the game's colours over, between or behind pieces: a frame, a
// box, the dimmed backdrop, the focus ring. Never touchable: the mouse goes to the pieces.
struct Overlay
{
    Rect to;          // in canvas pixels
    u32 argb;         // the fill (0: none), with its opacity
    int radius;       // the corners' radius, canvas pixels
    int border;       // the border's width inside `to`, canvas pixels (0: none)
    u32 borderArgb;
    int before = -1;  // drawn just before pieces[before]; -1: after every piece
    // Text instead of a box (CONSOLE's hints): the 5 x 7 font (ui.h, capitals) in `argb`, each
    // font pixel textScale canvas pixels, from to's top left, lines split at '\n' 9 font pixels
    // apart, with a shadow in borderArgb one font pixel down and right (0: none). `to` holds the
    // text and its shadow (TextRect). The text is not copied: a string that outlives the
    // composition (the hints are literals).
    const char* text = nullptr;
    int textScale = 0;
};

// A text overlay's rectangle at (x, y): its longest line's characters of 5 font pixels and a gap
// of 1 (the last gap holds the shadow), by 9 font pixels a line (the last one's 7 and the shadow's).
Rect TextRect(int x, int y, const char* text, int scale);

// An overlay as w x h pixels of 0xAARRGGBB (straight alpha; 0 outside its rounded corners), for
// `to` drawn at w x h output pixels: radius and border scaled with it. What --compose and the
// window both draw.
void RasteriseOverlay(const Overlay& o, int w, int h, std::vector<u32>& px);

struct Composition
{
    int cw, ch;
    std::vector<Piece> pieces;
    std::vector<Overlay> overlays;
    // The HD interface (ui/kit.h), drawn after the pieces and the overlays; never touchable.
    std::vector<ui::UiItem> ui;
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
// Overlays are not looked at: the gaps between pieces (frames, the canvas) touch nothing.
bool TouchFromComposition(const Composition& c, const CanvasFit& fit, float px, float py, int& tx, int& ty, int* pieceIndex = nullptr);
// The same, held to one piece's source rectangle wherever the point is: a drag stays on the
// piece it started on. A stale index (the layout changed meanwhile: no longer a touchable
// bottom piece) falls back to the touchable bottom piece under the point, else the largest one
// on the canvas, else the whole touch screen; the result is always within the DS screen,
// whatever the piece's source rectangle.
void TouchClampedComposition(const Composition& c, int pieceIndex, const CanvasFit& fit, float px, float py, int& tx, int& ty);
// Whether an output pixel position is on a touchable top piece and no later piece covers it
// (the mouse camera's area): the topmost piece under it decides, as for a click.
bool OverTopComposition(const Composition& c, const CanvasFit& fit, float px, float py);
// Where a DS touch position (tx, ty, fractions allowed) is drawn: through the topmost touchable
// Src::Bottom piece whose source rectangle holds it, as a canvas position, with that piece's
// scale (canvas pixels per DS pixel). False when no piece shows it (the stick cursor is hidden).
bool CanvasFromTouch(const Composition& c, float tx, float ty, float& cx, float& cy, float& scale);
}
