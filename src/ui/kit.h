// The UI kit: the game's look (purple rounded panels, white HD text with a soft dark shadow,
// controller button chips, live pictures) as a list of items in canvas pixels, rasterised in
// software at any scale. The window and --compose both draw that raster, so they show the same
// pixels, and the same items always give the same pixels (no clock, no animation here).
// UI items are drawn after a composition's pieces and overlays and never take the mouse.
#pragma once
#include <string>
#include <vector>

#include "common.h"
#include "layout.h"
#include "ui/font.h"

namespace sims2wide::ui
{
// The game's palette (0xAARRGGBB) and the panels' shape, canvas pixels.
struct Theme
{
    u32 frame, frameDark, fill, fillDark, text, shadow, backdrop;
    float radius, border;
};
const Theme& GameTheme();

enum class Align { Left, Centre, Right };
// The controller's buttons a chip can show: A, B, X, Y as round chips, the others as pills
// (the D-pad as a cross).
// DPadV / DPadH: the cross with its up and down (left and right) arms lit and the others faint:
// Up / Down (Left / Right) do something there.
// DPadL / DPadU / DPadR / DPadD: one arm lit (that direction alone). LStick / RStick: the sticks
// (a round chip with a ring, "L" / "R"). LT: the left trigger (a pill, as LB).
enum class Button { A, B, X, Y, LB, RB, Start, DPad, DPadV, DPadH, DPadL, DPadU, DPadR, DPadD, LStick, RStick, LT };
constexpr int kButtonCount = (int)Button::LT + 1;

// How the chips are printed: the device the player used last (input.cpp sets it every frame;
// ChipItem / ChipRect copy the current look into the item, so the raster stays a function of
// the items). Xbox: A / B / X / Y in green / red / blue / yellow, LB, LT, START (the look the
// kit always had); PlayStation: the cross, circle, square and triangle drawn, L1 / L2, OPTIONS;
// Nintendo: A / B / X / Y in the SNES colours, L / ZL, +; Keyboard: a keycap with the key's name.
enum class Face : u8 { Xbox, PlayStation, Nintendo, Keyboard };
struct ChipLook
{
    Face face = Face::Xbox;
    // Per Button, what is printed on it. A controller: the SDL input its action is bound to,
    // by its settings name ("a", "b", "x", "y", "lb", "rb", "lt", "rt", "start", "back", "guide",
    // "ls", "rs", "up", "down", "left", "right"), drawn in the face's own way; the keyboard: the
    // key's name. Empty: the button's own (A's "a", LB's "lb"…; the D-pad and the sticks keep
    // their drawing on every face).
    std::string glyph[kButtonCount];
};
void SetChipLook(const ChipLook& look);
const ChipLook& CurrentChipLook();

struct UiItem
{
    enum Kind { Panel, Text, Chip, Image } kind = Panel;
    Rect to {};         // canvas pixels (Text: the box it is laid out and clipped in)
    // The still layer it belongs to (Panel, Text, Chip): the window keeps each layer's raster
    // apart and redraws only the layers whose items changed (a menu's frame on layer 0, its list
    // and focus on layer 1: a focus move does not raster the frame again). Layers are drawn in
    // ascending order, the items of one in list order; give items in ascending layer order so
    // Rasterise (all at once, list order) draws what the layers draw. Images ignore it.
    int layer = 0;

    // Panel: a rounded box, `border` wide band of `borderArgb` inside `to`, a dark one-pixel
    // outline and, raised, a soft drop shadow under it.
    u32 fill = 0, borderArgb = 0;
    float radius = 0, border = 0;
    bool raised = true;
    bool plain = false;  // just the rounded box in `fill`: no outline, band, bevel or shadow (a meter's fill)

    // Text (and a chip's label): UTF-8, size in canvas pixels per em, wrapped to to.w when
    // `wrap`, aligned in `to`; a shadow one canvas pixel down and right, softened when scaled.
    std::string text;
    float size = 24;
    Weight weight = Weight::Bold;
    Align align = Align::Left;
    bool wrap = true;
    bool middle = false; // the lines centred in to's height (else from its top)
    u32 colour = 0xFFFFFFFF, shadowArgb = 0;

    // Chip: the button's chip, `size` its text size (the chip is as tall as a line), then the
    // label in `text` (none: the chip alone). `to` from ChipItem (grows with the label).
    Button button = Button::A;
    Face face = Face::Xbox; // and its glyph (ChipLook::glyph; "" the button's own): from the current ChipLook
    std::string glyph;
    float labelAt = 0;   // where the label starts, from to.x (0: right after the chip): a column of hints

    // Image: a rectangle of an ARGB picture (not copied: it must outlive the raster; a live crop
    // of a DS screen), scaled to `to` (nearest by the whole factor, then bilinear to the size),
    // `fill` as its opacity's alpha (0: opaque). Images are drawn above every other item, in
    // their own order, whatever their place in the list: the still items are one layer that
    // the window keeps while they do not change, the images (live, they change every frame)
    // are drawn over it on their own (Layers). Nothing of the UI may lie over a picture.
    const u32* src = nullptr;
    int srcW = 0, srcH = 0; // the picture's size (srcW is its stride)
    Rect from {};
};

UiItem PanelItem(Rect to, const Theme& t = GameTheme());          // the light panel (fill, frame)
UiItem WellItem(Rect to, const Theme& t = GameTheme());           // a darker inset box inside a panel
UiItem TextItem(Rect to, const std::string& utf8, float size, Align align = Align::Left, Weight w = Weight::Bold);
// A chip at (x, y) with its label, printed as the current ChipLook says; its rectangle is the
// chip and the label together.
// labelAt: the label's start from x (a column of hints lines its labels up), 0: after the chip.
// w: the label's weight.
UiItem ChipItem(int x, int y, Button b, const std::string& label, float size, float labelAt = 0, Weight w = Weight::Bold);
Rect ChipRect(int x, int y, Button b, const std::string& label, float size, float labelAt = 0, Weight w = Weight::Bold);
UiItem ImageItem(Rect to, const u32* src, int srcW, int srcH, Rect from);

// The items' bounds on the canvas (shadows included), empty when none draws anything.
Rect Bounds(const std::vector<UiItem>& items);
// A hash of everything that changes the still items' raster (images are left out: they are
// drawn every frame, apart).
u64 Hash(const std::vector<UiItem>& items);
// An image item's hash, its pixels included (the window redraws a picture only when it changes).
u64 HashImage(const UiItem& image);
// The still items (Panel, Text, Chip, in order) and the images (in order), as drawn: the images
// above the still items. (LayersAt splits the still items further by their `layer`.)
void Layers(const std::vector<UiItem>& items, std::vector<UiItem>& still, std::vector<UiItem>& images);

// The items drawn into argb (w x h, straight alpha, cleared first): canvas point (cx, cy) lands
// on argb pixel ((cx - ox) * scale, (cy - oy) * scale). scale: output pixels per canvas pixel.
// The still items first, then the images (Layers).
void Rasterise(const std::vector<UiItem>& items, float scale, u32* argb, int w, int h, float ox = 0, float oy = 0);

// What the window and --compose both draw: each still layer (by `layer`, ascending) over its
// bounds, then each image over its own, as whole output pixels (x, y, w, h) for a canvas whose origin is output pixel
// (originX, originY); Rasterise(items, scale, …, w, h, ox, oy) draws one. Empty ones are left out.
// A layer's pixel origin (x, y) is rounded down like a piece's (CanvasFit::ToOutput): its bounds' top left
// is drawn on it exactly (ox, oy are the bounds' canvas position), never between two pixels.
struct LayerAt
{
    std::vector<UiItem> items;
    bool still;          // still items of one layer (else one image)
    int layer = 0;       // that layer
    int x, y, w, h;
    float ox, oy;
};
std::vector<LayerAt> LayersAt(const std::vector<UiItem>& items, float scale, float originX, float originY);

// A demonstration of the kit (--ui-demo): a panel, text, every chip and a live crop of `bottom`
// (the DS's touch screen, 256 x 192; null: none).
std::vector<UiItem> DemoItems(const u32* bottom);

}
