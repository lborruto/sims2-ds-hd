// The UI kit: the game's look (purple rounded panels, white HD text with a soft dark shadow,
// controller button chips, live pictures) as a list of items in canvas pixels, rasterised in
// software at any scale. The window and --compose both draw that raster, so they show the same
// pixels, and the same items always give the same pixels (no clock, no animation here).
// UI items are drawn after a composition's pieces and overlays and never take the mouse.
#pragma once
#include <memory>
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

// The HD screens' shared palette, measured with WCAG's contrast formula (research checklist B1-B4;
// XAG 102: 4.5:1 for text, 3:1 for inactive text): a row, button or well kFillRow (white text on it
// 4.58:1; GameTheme().fillDark is the same), the focused row kFillFocus (white 7.15:1), notes and
// subtitles kTextNote (white: 4.58:1 on a well), inactive text kTextInactive (3.23:1 on kFillRow).
// The panel's own fill (#828ACB, the game's) takes white at 3.26:1: large text only (titles).
constexpr u32 kFillRow = 0xFF6870B6, kFillFocus = 0xFF4A5296, kTextNote = 0xFFFFFFFF, kTextInactive = 0xFFD4D7F2;
// The focus ring: the game's green outside (kFocusOuterW wide around the item), a white line
// inside it (kFocusInnerW): it reads without its colour (white against the panel 3.26:1, against a
// well 4.58:1, a focused fill against an unfocused one besides).
constexpr u32 kFocusOuter = 0xFF309A41, kFocusInner = 0xFFFFFFFF;
constexpr int kFocusOuterW = 7, kFocusInnerW = 4;
// The smallest text at 100 % (canvas pixels, a shrunk-to-fit line and a chip's glyph included):
// 27 px at 1080p full screen, past the console's minimum of 26 (XAG 101; the PC's is 18).
constexpr float kMinText = 18;

enum class Align { Left, Centre, Right };
// The controller's buttons a chip can show: A, B, X, Y as round chips, the others as pills
// (the D-pad as a cross).
// DPadV / DPadH: the cross with its up and down (left and right) arms lit and the others faint:
// Up / Down (Left / Right) do something there.
// DPadL / DPadU / DPadR / DPadD: one arm lit (that direction alone). LStick / RStick: the sticks
// (a round chip with a ring, "L" / "R"). LT: the left trigger (a pill, as LB).
// Back: the Select-type button (Xbox's Back / View, PlayStation's Share, Nintendo's -: a pill),
// the TOUCH MENU's on a pad. Pause: the pause menu's buttons, one pill (START + BACK, OPTIONS +
// SHARE, + / -; the keyboard's key, Esc).
enum class Button { A, B, X, Y, LB, RB, Start, DPad, DPadV, DPadH, DPadL, DPadU, DPadR, DPadD, LStick, RStick, LT, Back, Pause };
constexpr int kButtonCount = (int)Button::Pause + 1;

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
    // Image: Smooth (SetGamePictures, copied by ImageItem): the game's pixel art enlarged by the
    // pixel-art scaler (xBRZ, src/thirdparty/xbrz) to the whole factor at or above the size it is
    // drawn at, then bilinear to it; false: nearest by the whole factor, then bilinear (Pixel).
    bool smooth = false;
    // Image: its tier (kTierS, kTierL; 0: none, drawn to fill `to` as above). Tiered, every pixel of
    // the picture is a k x k block of whole output pixels, k = TierFactor(tier, scale x zoom),
    // never resampled: Pixel nearest, Smooth the pixel-art scaler at exactly k (past its 6: at
    // k's largest divisor 2-6, then nearest). The block of pictures is centred on `to` (TierRect),
    // which a builder makes the picture `tier` times its size (TierImage): at 100 % it fills it.
    int tier = 0;

    // The UI's size (SetUiScale; console.cpp, the shell and the window's marks set it per group of
    // items about the group's anchor): every canvas point p of the item is drawn at
    // zoom * p + (dx, dy), its sizes (text, radius, border, shadows, a chip's) zoom times; the
    // items' own layout (wrapping, fitting) is the one at 1, so a zoomed item is that item made
    // larger or smaller, never laid out again. 1, 0, 0: as always (the hash too).
    float zoom = 1, dx = 0, dy = 0;
};

// The game pictures' two sizes (one tier per component; a game pixel = TierFactor(tier, the
// window's scale x the UI's size) output pixels): S the controls' (arrows, chips' icons, small
// buttons), L the content's (items, portraits, tabs, the cells a player looks at).
constexpr int kTierS = 2, kTierL = 3;
// The whole output pixels a picture pixel takes at tier `tier` drawn at s output pixels per canvas
// pixel: tier x s rounded (a half down: never larger than its layout when it can be smaller), 1 at least.
int TierFactor(int tier, float s);

UiItem PanelItem(Rect to, const Theme& t = GameTheme());          // the light panel (fill, frame)
// The focus ring around `r` (`radius` its corners'): the green, then the white line inside it, two
// plain boxes to give before the item they ring (the item covers their middle).
struct FocusRingItems { UiItem outer, inner; };
FocusRingItems FocusRing(Rect r, float radius);
UiItem WellItem(Rect to, const Theme& t = GameTheme());           // a darker inset box inside a panel
UiItem TextItem(Rect to, const std::string& utf8, float size, Align align = Align::Left, Weight w = Weight::Bold);
// A chip at (x, y) with its label, printed as the current ChipLook says; its rectangle is the
// chip and the label together.
// labelAt: the label's start from x (a column of hints lines its labels up), 0: after the chip.
// w: the label's weight.
UiItem ChipItem(int x, int y, Button b, const std::string& label, float size, float labelAt = 0, Weight w = Weight::Bold);
Rect ChipRect(int x, int y, Button b, const std::string& label, float size, float labelAt = 0, Weight w = Weight::Bold);
// A chip printed with a given face and glyph rather than the current look's (a binding shown as
// what it is: the keyboard's key, or a pad input by its settings name, "lb", "rt", "start"…).
UiItem ChipItemAs(int x, int y, Face face, const std::string& glyph, const std::string& label, float size, Weight w = Weight::Bold);
UiItem ImageItem(Rect to, const u32* src, int srcW, int srcH, Rect from);
// The same at a tier: `from` `tier` times its size, centred in `box` (canvas pixels), tier set.
UiItem TierImage(Rect box, const u32* src, int srcW, int srcH, Rect from, int tier);
// A tiered picture's output pixels in a raster at `scale` whose canvas origin is (ox, oy)
// (Rasterise's): from's size times TierFactor, centred on `to` as drawn (zoom applied), its top left
// rounded to a whole pixel. Empty for an image without a tier.
Rect TierRect(const UiItem& image, float scale, float ox, float oy);

// The UI's size (the player's setting, Auto or 60-150 %, the window's; 1 headless unless
// --ui-scale): what the HD screens' builders zoom their groups of items by (console.cpp, the
// shell, the window's toast and marks). The game's pictures in the HD UI: Smooth (the pixel-art
// scaler) or Pixel (nearest, the default: every check's picture as it always was); ImageItem
// copies it into the item, so the raster stays a function of the items.
void SetUiScale(float scale);
float UiScale();
void SetGamePictures(bool smooth);
bool SmoothPictures();
// The chips' look now (SetChipLook's): a builder that prints another button for another device
// (the keyboard's turn keys rather than the right stick).
const ChipLook& ChipLookNow();
// An item's `to` as drawn (its zoom and shift applied), rounded outward to whole canvas pixels.
Rect Placed(const UiItem& it);
// The item zoomed by z about the canvas point (ax, ay) (on top of the zoom it already has).
void ZoomItem(UiItem& it, float z, float ax, float ay);
// The largest zoom up to z at which `box` zoomed about (ax, ay) does not reach past `limit` on any
// side it grows towards (z itself when it fits; never below 0).
float FitZoom(Rect box, float z, float ax, float ay, Rect limit);
// The pixel-art scaler's picture for an image item (Smooth): its `from` rectangle enlarged k times
// (2-6), straight alpha; an icon's (a picture of its own) kept in a cache by its pixels and k
// (64 MB at most, the oldest out first): the same pixels give the same picture. A live crop of a
// DS screen (a 256 × 192 picture) is made again each time and not kept. Null for k outside 2-6 or a picture that does not read.
struct Enlarged { std::vector<u32> px; int w = 0, h = 0; };
std::shared_ptr<const Enlarged> Enlarge(const UiItem& image, int k);
void ClearEnlarged();

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
// The same raster's pixels inside `clip` alone (the others left as they are): byte for byte what
// the whole raster gives there. Drawn in bands on a few threads (the bands are independent: the
// same bytes on any number of threads); the font's part (text masks, kept by what makes them) on
// the calling thread first.
void Rasterise(const std::vector<UiItem>& items, float scale, u32* argb, int w, int h, float ox, float oy, Rect clip);
// The raster's threads: 0 as many as the machine has (8 at most), 1 the calling thread alone (the
// checks compare the two).
void SetRasterThreads(int n);
// Forgets the text masks kept (the checks: a mask drawn again is the same bytes).
void ClearTextMasks();

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

// What the window keeps of LayersAt between frames (Video::DrawUi; tests/ui_bench.cpp measures
// it): each layer's pixels, made again only when what draws them changes. No SDL here.
struct LayerCache
{
    struct Entry
    {
        u64 id = 0;                 // this layer's, for its life (a new one: drawn whole)
        bool still = true;
        int layer = 0;              // a still layer's number (UiItem::layer)
        int x = 0, y = 0, w = 0, h = 0;
        u64 hash = 0;               // its items, place, size and scale (0: nothing drawn yet)
        std::vector<u32> px;        // w x h, straight alpha (empty when `direct`)
        bool changed = false;       // drawn again by the last Update: the window uploads `dirty`
        std::vector<Rect> dirty;    // what the last Update drew again (layer pixels)
        bool direct = false;        // a picture the caller draws itself (`image`; no pixels here)
        UiItem image;               // (a picture's item)
        float ox = 0, oy = 0;       // the layer's canvas origin (LayerAt::ox / oy)
        float scale = 0;
        std::vector<u64> itemHash;  // each item's hash as drawn: what the next Update compares
        std::vector<Rect> itemBox;  // each item's pixels (layer pixels, with room for its shadow)
    };
    std::vector<Entry> entries;     // in drawing order, as LayersAt gives them
    // direct: whether the caller draws this picture itself (the window: a live crop at a whole
    // factor, from its own texture); null: every picture rastered here.
    using Direct = bool (*)(const UiItem& image, const LayerAt& at, void* user);
    void Update(const std::vector<UiItem>& items, float scale, float originX, float originY, Direct direct = nullptr, void* user = nullptr);
};

// Whether a picture (an image layer of LayersAt at `scale`) is drawn by the kit's scaler as
// nearest alone: opaque, its rectangle inside the picture, scaled by a whole factor to whole
// pixels (a tiered one in Pixel: always, at TierRect). The window draws such a live crop with the
// GPU (nearest) rather than rastering it, at CropRect.
bool WholeFactorCrop(const UiItem& image, const LayerAt& at, float scale);
// Where such a crop lands, output pixels: its layer's whole pixel (x, y) and canvas origin (ox, oy)
// (LayerAt's, LayerCache::Entry's) at `scale`.
Rect CropRect(const UiItem& image, float scale, int x, int y, float ox, float oy);

// A demonstration of the kit (--ui-demo): a panel, text, every chip and a live crop of `bottom`
// (the DS's touch screen, 256 x 192; null: none).
std::vector<UiItem> DemoItems(const u32* bottom);

}
