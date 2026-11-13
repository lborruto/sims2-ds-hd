// Checks the UI kit (src/ui): word wrap, the raster's determinism, a panel's outline, frame and
// fill where they belong at scale 1 and 2, text kept inside its box, chips growing with their
// label, the picture scaler. Made-up text only.
//   ninja -C build uikit_test && ./build/uikit_test
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ui/kit.h"

using namespace sims2hd;
using namespace sims2hd::ui;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static std::string Join(const std::vector<std::string>& lines, const char* sep)
{
    std::string s;
    for (size_t i = 0; i < lines.size(); i++) s += (i ? sep : "") + lines[i];
    return s;
}

static void wrap_cases()
{
    // Fits on one line.
    CHECK(Wrap("Hello there", 24, 1000) == std::vector<std::string> {"Hello there"});
    // Wraps at spaces, every line within the width, nothing lost.
    const std::string text = "The quick brown fox jumps over the lazy dog and keeps on running";
    const std::vector<std::string> lines = Wrap(text, 24, 200);
    CHECK(lines.size() > 1);
    for (const std::string& l : lines) CHECK(TextWidth(l, 24) <= 200);
    CHECK(Join(lines, " ") == text);
    // A word longer than the line breaks between letters, each piece within the width.
    const std::string longWord = "Abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
    const std::vector<std::string> pieces = Wrap(longWord, 24, 120);
    CHECK(pieces.size() > 2);
    for (const std::string& l : pieces) CHECK(!l.empty() && TextWidth(l, 24) <= 120);
    CHECK(Join(pieces, "") == longWord);
    // Even a line too narrow for one letter keeps a letter a line.
    CHECK(Wrap("abc", 24, 1) == (std::vector<std::string> {"a", "b", "c"}));
    // UTF-8 accents are kept whole, never cut inside a letter.
    const std::string accents = "crème brûlée, façade, Grüße, año, naïveté";
    const std::vector<std::string> al = Wrap(accents, 24, 110);
    CHECK(al.size() > 1);
    CHECK(Join(al, " ") == accents);
    const std::vector<std::string> ab = Wrap("éééééééééééééééééééé", 24, 60);
    CHECK(ab.size() > 1);
    CHECK(Join(ab, "") == "éééééééééééééééééééé");
    for (const std::string& l : ab) CHECK(l.size() % 2 == 0); // whole two-byte letters
    // Empty text, or only spaces: no line. '\n' forces a break.
    CHECK(Wrap("", 24, 100).empty());
    CHECK(Wrap("   ", 24, 100).empty());
    CHECK(Wrap("one\ntwo", 24, 1000) == (std::vector<std::string> {"one", "two"}));
    // An accented letter has its own glyph: wider than nothing, as wide whatever the scale.
    CHECK(TextWidth("é", 24) > 0);
    CHECK(TextWidth("é", 48) > TextWidth("é", 24) * 1.9f);
}

static u64 HashOf(const std::vector<u32>& px)
{
    u64 h = 0xCBF29CE484222325ull;
    for (u32 v : px) { h ^= v; h *= 0x100000001B3ull; }
    return h;
}

static std::vector<UiItem> Sample(const u32* picture)
{
    std::vector<UiItem> v;
    v.push_back(PanelItem({20, 20, 300, 160}));
    v.push_back(WellItem({40, 60, 200, 100}));
    v.push_back(TextItem({50, 70, 180, 80}, "Déjà vu: the same pixels, every time.", 22));
    v.push_back(ChipItem(30, 30, Button::A, "Next", 20));
    v.push_back(ChipItem(150, 30, Button::Start, "Pause", 20));
    v.push_back(ImageItem({250, 60, 60, 60}, picture, 4, 4, {0, 0, 4, 4}));
    return v;
}

static void determinism()
{
    std::vector<u32> picture(16);
    for (int i = 0; i < 16; i++) picture[i] = 0xFF000000 | (u32)(i * 0x0F0F0F);
    const std::vector<UiItem> items = Sample(picture.data());
    for (float scale : {1.0f, 1.5f, 2.0f})
    {
        const int w = (int)(360 * scale), h = (int)(220 * scale);
        std::vector<u32> a((size_t)w * h, 0x12345678), b((size_t)w * h, 0x9ABCDEF0);
        Rasterise(items, scale, a.data(), w, h);
        Rasterise(items, scale, b.data(), w, h);
        CHECK(HashOf(a) == HashOf(b));
        CHECK(a == b);
    }
    // The content hash follows what changes the raster, and only that.
    CHECK(Hash(items) == Hash(Sample(picture.data())));
    std::vector<UiItem> other = items;
    other[2].text = "Something else";
    CHECK(Hash(other) != Hash(items));
    std::vector<u32> picture2 = picture;
    picture2[5] ^= 1;
    CHECK(Hash(Sample(picture2.data())) == Hash(items)); // a picture's pixels: drawn apart, not hashed
    CHECK(HashImage(Sample(picture2.data())[5]) != HashImage(items[5]));
    CHECK(HashImage(Sample(picture.data())[5]) == HashImage(items[5]));
}

static u32 Rgb(u32 p) { return p & 0xFFFFFF; }

static void panel_pixels()
{
    const Theme& t = GameTheme();
    for (int s : {1, 2})
    {
        UiItem p = PanelItem({10, 10, 100, 60});
        p.raised = false;
        const int w = 130 * s, h = 90 * s;
        std::vector<u32> px((size_t)w * h);
        Rasterise({p}, (float)s, px.data(), w, h);
        auto at = [&](int x, int y) { return px[(size_t)y * w + x]; };
        // The corner is cut by the radius; outside the panel nothing.
        CHECK((at(10 * s, 10 * s) >> 24) == 0);
        CHECK(at(5 * s, 40 * s) == 0);
        CHECK(at(10 * s - 1, 40 * s) == 0);
        // The left edge, middle: the dark outline (one canvas pixel), then the frame's band, then the fill.
        CHECK(at(10 * s, 40 * s) == t.shadow);
        CHECK(at(10 * s + s - 1, 40 * s) == t.shadow);
        CHECK(at(10 * s + s, 40 * s) == t.frame);
        CHECK(at(10 * s + s + (int)(t.border * s) - 1, 40 * s) == t.frame);
        CHECK(at(60 * s, 40 * s) == t.fill);
        // The top edge too, and the right one (symmetric).
        CHECK(at(60 * s, 10 * s) == t.shadow);
        CHECK(at(60 * s, 10 * s + s) == t.frame);
        CHECK(at(110 * s - 1, 40 * s) == t.shadow);
        // The corner's curve: a partly covered pixel, anti-aliased, of the outline's colour.
        bool soft = false;
        for (int d = 0; d < 8 * s; d++)
        {
            const u32 c = at(10 * s + d, 10 * s + d);
            if ((c >> 24) > 0 && (c >> 24) < 255) soft = true;
        }
        CHECK(soft);
    }
    // A raised panel throws a shadow under its bottom edge.
    std::vector<u32> px(130 * 90);
    Rasterise({PanelItem({10, 10, 100, 60})}, 1, px.data(), 130, 90);
    CHECK((px[(size_t)72 * 130 + 60] >> 24) > 0);
    CHECK(Rgb(px[(size_t)72 * 130 + 60]) == 0);
}

static void text_inside()
{
    for (float s : {1.0f, 2.0f, 1.25f})
    {
        const int w = (int)(400 * s), h = (int)(200 * s);
        std::vector<u32> px((size_t)w * h);
        // Text far longer than its box: wrapped, the lines that do not fit clipped.
        const UiItem t = TextItem({50, 40, 200, 60}, "Ýpsilon jumps quietly over every gap, and still more words here to overflow the box below.", 26);
        Rasterise({t}, s, px.data(), w, h);
        int drawn = 0, outside = 0, white = 0;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                const u32 p = px[(size_t)y * w + x];
                if (!(p >> 24)) continue;
                drawn++;
                if (p == 0xFFFFFFFF) white++;
                if (x < 50 * s || y < 40 * s || x >= 250 * s || y >= 100 * s) outside++;
            }
        CHECK(drawn > 200);
        CHECK(white > 50);
        CHECK(outside == 0);
    }
}

static void chips()
{
    const Rect a = ChipRect(0, 0, Button::A, "", 24), a1 = ChipRect(0, 0, Button::A, "Go", 24), a2 = ChipRect(0, 0, Button::A, "Go much further", 24);
    CHECK(a.w == a.h);           // a face button alone is round
    CHECK(a1.w > a.w && a2.w > a1.w);
    CHECK(a1.h == a.h && a2.h == a.h);
    CHECK(ChipRect(0, 0, Button::Start, "", 24).w > ChipRect(0, 0, Button::LB, "", 24).w);
    CHECK(ChipRect(0, 0, Button::LB, "", 24).w > a.w); // a pill
    CHECK(ChipRect(0, 0, Button::A, "Go", 48).h > a1.h);
    // The one-direction D-pads and the sticks are round like the face buttons; each draws
    // differently (its arm, its letter), inside its rectangle.
    {
        std::vector<std::vector<u32>> seen;
        for (Button b : {Button::DPad, Button::DPadL, Button::DPadU, Button::DPadR, Button::DPadD, Button::LStick, Button::RStick})
        {
            const Rect r = ChipRect(0, 0, b, "", 24);
            CHECK(r.w == a.w && r.h == a.h);
            std::vector<u32> px(48 * 48);
            Rasterise({ChipItem(8, 8, b, "", 24)}, 1, px.data(), 48, 48);
            for (const std::vector<u32>& o : seen) CHECK(o != px);
            seen.push_back(px);
        }
    }
    // Every face's chips one height (a row of hints shares its baseline whatever the device); a
    // Nintendo pad's + (Start) and - (Back) round like its face buttons, their sign white bars in
    // the chip's middle (the "-" was a dot in a pill).
    {
        for (Face f : {Face::Xbox, Face::PlayStation, Face::Nintendo, Face::Keyboard})
        {
            ChipLook look;
            look.face = f;
            if (f == Face::Keyboard) look.glyph[(int)Button::A] = "Enter", look.glyph[(int)Button::Start] = "Return";
            SetChipLook(look);
            for (Button b : {Button::A, Button::B, Button::X, Button::Y, Button::LB, Button::RB, Button::Start, Button::Back, Button::Pause, Button::LT,
                             Button::DPadV, Button::RStick})
                CHECK(ChipRect(0, 0, b, "Go", 24).h == a1.h);
        }
        ChipLook nin;
        nin.face = Face::Nintendo;
        SetChipLook(nin);
        for (Button b : {Button::Start, Button::Back})
        {
            const Rect r = ChipRect(0, 0, b, "", 24);
            CHECK(r.w == a.w && r.h == a.h);
            std::vector<u32> px(48 * 48);
            Rasterise({ChipItem(8, 8, b, "", 24)}, 1, px.data(), 48, 48);
            const int cx = 8 + r.w / 2, cy = 8 + r.h / 2;
            CHECK((px[(size_t)cy * 48 + cx] & 0xFFFFFF) == 0xFFFFFF);                              // the bar's middle
            CHECK(((px[(size_t)(cy - 7) * 48 + cx] & 0xFFFFFF) == 0xFFFFFF) == (b == Button::Start)); // the upright: + only
        }
        SetChipLook({});
    }
    // A column of hints: the label starts where asked, whatever the chip.
    CHECK(ChipRect(0, 0, Button::A, "Go", 24, 100).w == ChipRect(0, 0, Button::Start, "Go", 24, 100).w);
    CHECK(ChipRect(0, 0, Button::A, "Go", 24, 100).w > a1.w);
    // The chip's item holds its rectangle; drawn, it stays in it (with room for its shadow).
    const UiItem c = ChipItem(20, 20, Button::X, "Zoom", 24);
    CHECK(c.to.w == ChipRect(20, 20, Button::X, "Zoom", 24).w);
    std::vector<u32> px(300 * 100);
    Rasterise({c}, 1, px.data(), 300, 100);
    int drawn = 0;
    for (int y = 0; y < 100; y++)
        for (int x = 0; x < 300; x++)
            if (px[(size_t)y * 300 + x] >> 24)
            {
                drawn++;
                CHECK(x >= 16 && x < 20 + c.to.w + 4 && y >= 16 && y < 20 + c.to.h + 4);
            }
    CHECK(drawn > 100);
}

static void image_scaler()
{
    // 2 x 2 pixels to 8 x 8 (a whole factor): each source pixel a clean 4 x 4 block inside.
    const u32 src[4] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF};
    std::vector<u32> px(8 * 8);
    Rasterise({ImageItem({0, 0, 8, 8}, src, 2, 2, {0, 0, 2, 2})}, 1, px.data(), 8, 8);
    CHECK(px[0] == 0xFFFF0000);
    CHECK(px[2 * 8 + 2] == 0xFFFF0000);
    CHECK(px[1 * 8 + 6] == 0xFF00FF00);
    CHECK(px[6 * 8 + 1] == 0xFF0000FF);
    CHECK(px[7 * 8 + 7] == 0xFFFFFFFF);
    // A crop reads only its rectangle.
    Rasterise({ImageItem({0, 0, 8, 8}, src, 2, 2, {1, 1, 1, 1})}, 1, px.data(), 8, 8);
    for (u32 p : px) CHECK(p == 0xFFFFFFFF);
}

static void edges()
{
    std::vector<u32> picture(16, 0xFF336699);
    // Off the buffer (left, above, right, below, and huge): nothing drawn, no crash.
    const std::vector<UiItem> away = {
        PanelItem({-500, -500, 100, 100}), PanelItem({5000, 10, 100, 100}), TextItem({10, 9000, 200, 40}, "far", 24),
        ChipItem(-300, 20, Button::Start, "Gone", 24), ImageItem({-90, -90, 40, 40}, picture.data(), 4, 4, {0, 0, 4, 4}),
        PanelItem({-1000000, -1000000, 2000, 2000}),
    };
    std::vector<u32> px(200 * 100, 0xDEADBEEF);
    Rasterise(away, 1, px.data(), 200, 100);
    for (u32 p : px) CHECK(p == 0);
    // Zero and negative sizes, an empty buffer, no buffer, a zero scale: nothing, no crash.
    const std::vector<UiItem> empty = {PanelItem({10, 10, 0, 40}), TextItem({10, 10, -5, 20}, "x", 24), ImageItem({10, 10, 0, 0}, picture.data(), 4, 4, {0, 0, 4, 4}),
                                       ImageItem({10, 10, 20, 20}, picture.data(), 4, 4, {0, 0, 0, 3}), ImageItem({10, 10, 20, 20}, nullptr, 4, 4, {0, 0, 4, 4})};
    px.assign(200 * 100, 0xDEADBEEF);
    Rasterise(empty, 1, px.data(), 200, 100);
    for (u32 p : px) CHECK(p == 0);
    Rasterise(Sample(picture.data()), 1, px.data(), 0, 100);
    Rasterise(Sample(picture.data()), 1, px.data(), 200, 0);
    Rasterise(Sample(picture.data()), 1, nullptr, 200, 100);
    px.assign(200 * 100, 0);
    Rasterise(Sample(picture.data()), 0, px.data(), 200, 100);
    for (u32 p : px) CHECK(p == 0);
    CHECK(LayersAt({}, 1, 0, 0).empty());
    CHECK(LayersAt(empty, 1, 0, 0).empty());
    // A tiny text box (narrower than the shadow's offset) and a 1 x 1 buffer.
    Rasterise({TextItem({0, 0, 1, 1}, "W", 30)}, 2, px.data(), 1, 1);
    Rasterise({TextItem({0, 0, 3, 40}, "Wide", 30)}, 4, px.data(), 2, 50);

    // A half-pixel offset: the panel's left edge falls half way across a pixel.
    UiItem p = PanelItem({10, 10, 100, 60});
    p.raised = false;
    px.assign(200 * 100, 0);
    Rasterise({p}, 1, px.data(), 200, 100, 0.5f, 0);
    const u32 half = px[(size_t)40 * 200 + 9];
    CHECK((half >> 24) > 100 && (half >> 24) < 155);
    CHECK(px[(size_t)40 * 200 + 8] == 0);

    // The layers at a whole-pixel origin give exactly the whole raster's pixels; at any origin and
    // scale they stay within the output and draw the same every time.
    const std::vector<UiItem> items = Sample(picture.data());
    std::vector<UiItem> still, images;
    Layers(items, still, images);
    CHECK(still.size() == 5 && images.size() == 1);
    CHECK(Hash(items) == Hash(still)); // pictures are not in the still layer's hash
    std::vector<u32> whole(400 * 300);
    Rasterise(still, 1, whole.data(), 400, 300, -30, -20); // the canvas's origin at output (30, 20)
    for (const LayerAt& l : LayersAt(items, 1, 30, 20))
    {
        std::vector<u32> part((size_t)l.w * l.h);
        Rasterise(l.items, 1, part.data(), l.w, l.h, l.ox, l.oy);
        if (!l.still) continue;
        CHECK(l.x >= 0 && l.y >= 0 && l.x + l.w <= 400 && l.y + l.h <= 300);
        bool same = true;
        for (int y = 0; y < l.h; y++)
            for (int x = 0; x < l.w; x++) same = same && part[(size_t)y * l.w + x] == whole[(size_t)(l.y + y) * 400 + l.x + x];
        CHECK(same);
    }
    for (float origin : {0.37f, 13.61f})
    {
        const std::vector<LayerAt> ls = LayersAt(items, 1.37f, origin, origin * 0.5f);
        CHECK(ls.size() == 2);
        for (const LayerAt& l : ls)
        {
            // The bounds, at this scale and origin, hold every item.
            for (const UiItem& it : l.items)
            {
                CHECK(l.x <= origin + it.to.x * 1.37f + 0.001f);
                CHECK(l.x + l.w >= origin + (it.to.x + it.to.w) * 1.37f - 0.001f);
            }
            std::vector<u32> a((size_t)l.w * l.h), b((size_t)l.w * l.h, 7);
            Rasterise(l.items, 1.37f, a.data(), l.w, l.h, l.ox, l.oy);
            Rasterise(l.items, 1.37f, b.data(), l.w, l.h, l.ox, l.oy);
            CHECK(a == b);
            int drawn = 0;
            for (u32 v : a) drawn += (v >> 24) != 0;
            CHECK(drawn > 100);
        }
    }
    // Images draw above the still items, wherever they are in the list.
    std::vector<u32> first(100 * 100), last(100 * 100);
    const UiItem img = ImageItem({20, 20, 40, 40}, picture.data(), 4, 4, {0, 0, 4, 4});
    const UiItem box = PanelItem({10, 10, 80, 80});
    Rasterise({img, box}, 1, first.data(), 100, 100);
    Rasterise({box, img}, 1, last.data(), 100, 100);
    CHECK(first == last);
    CHECK(first[(size_t)40 * 100 + 40] == 0xFF336699);
}

// Still layers (UiItem::layer): each its own LayerAt, ascending, the items of one in list order;
// a change on layer 1 leaves layer 0's hash alone (the window keeps its raster); one layer is
// exactly the single still layer as before (layer 0 hashes as it always did).
static void still_layers()
{
    UiItem frame = PanelItem({10, 10, 300, 200});
    UiItem row = PanelItem({40, 40, 100, 30});
    row.layer = 1;
    UiItem label = TextItem({50, 40, 80, 30}, "Row", 20);
    label.layer = 1;
    const std::vector<UiItem> items {frame, row, label};
    const std::vector<LayerAt> ls = LayersAt(items, 1, 0, 0);
    CHECK(ls.size() == 2);
    CHECK(ls[0].still && ls[0].layer == 0 && ls[0].items.size() == 1);
    CHECK(ls[1].still && ls[1].layer == 1 && ls[1].items.size() == 2 && ls[1].items[1].kind == UiItem::Text);
    // The focus moved (layer 1 changed): layer 0's items and hash the same.
    std::vector<UiItem> moved = items;
    moved[1].to.y += 30;
    moved[2].to.y += 30;
    const std::vector<LayerAt> lm = LayersAt(moved, 1, 0, 0);
    CHECK(Hash(lm[0].items) == Hash(ls[0].items) && Hash(lm[1].items) != Hash(ls[1].items));
    // Layer 0 alone: one still layer, its hash as without the field.
    UiItem plain = PanelItem({10, 10, 300, 200});
    CHECK(LayersAt({plain}, 1, 0, 0).size() == 1);
    UiItem other = plain;
    other.layer = 2;
    CHECK(Hash({other}) != Hash({plain}));
    // The layers over each other draw what the whole raster draws, within rounding.
    std::vector<u32> whole(320 * 220), split(320 * 220, 0);
    Rasterise(items, 1, whole.data(), 320, 220);
    for (const LayerAt& l : ls)
    {
        std::vector<u32> part((size_t)l.w * l.h);
        Rasterise(l.items, 1, part.data(), l.w, l.h, l.ox, l.oy);
        for (int y = 0; y < l.h; y++)
            for (int x = 0; x < l.w; x++)
            {
                const int X = l.x + x, Y = l.y + y;
                if (X < 0 || Y < 0 || X >= 320 || Y >= 220) continue;
                const u32 src = part[(size_t)y * l.w + x], dst = split[(size_t)Y * 320 + X];
                const float a = (src >> 24) / 255.0f, b = (dst >> 24) / 255.0f, o = a + b * (1 - a);
                u32 out = 0;
                if (o > 0)
                    for (int sh : {16, 8, 0})
                    {
                        const float c = (((src >> sh) & 255) * a + ((dst >> sh) & 255) * b * (1 - a)) / o;
                        out |= (u32)std::lround(c) << sh;
                    }
                split[(size_t)Y * 320 + X] = out | (u32)std::lround(o * 255) << 24;
            }
    }
    int worst = 0;
    for (size_t i = 0; i < whole.size(); i++)
        for (int sh : {24, 16, 8, 0}) worst = std::max(worst, std::abs((int)((whole[i] >> sh) & 255) - (int)((split[i] >> sh) & 255)));
    CHECK(worst <= 2);
}

// Made-up items of every kind (panels raised or not, wells, plain fills, texts, chips of every
// button with and without a label or a column's labelAt, pictures opaque or not), for the checks
// below that compare one way of drawing with another.
struct Rng
{
    u64 v = 88172645463325252ull;
    u32 Next() { v ^= v << 13; v ^= v >> 7; v ^= v << 17; return (u32)v; }
    int In(int a, int b) { return a + (int)(Next() % (u32)(b - a + 1)); }
};
static bool gZoomed = false; // (raster_ways' second pass: items zoomed by the UI's size, pictures Smooth)
static UiItem RandomItem(Rng& g, const u32* pic, const u32* icon)
{
    static const char* words[] = {"Hello", "Crème brûlée", "Talk about the weather", "§1,200", "A", "Pneumonoultramicroscopicsilicovolcanoconiosis and more to wrap"};
    const Rect r {g.In(0, 600), g.In(0, 400), g.In(4, 500), g.In(4, 300)};
    UiItem it;
    switch (g.In(0, 6))
    {
    case 0: it = PanelItem(r); it.radius = (float)g.In(0, 40); it.border = (float)g.In(0, 6); it.raised = g.In(0, 1); break;
    case 1: it = WellItem(r); break;
    case 2: it = PanelItem(r); it.plain = true; it.fill = g.Next() | (g.In(0, 1) ? 0xFF000000u : 0); break;
    case 3: it = TextItem(r, words[g.In(0, 5)], (float)g.In(12, 40), (Align)g.In(0, 2), (Weight)g.In(0, 1)); it.middle = g.In(0, 1); it.wrap = g.In(0, 1); break;
    case 4: it = ChipItem(r.x, r.y, (Button)g.In(0, kButtonCount - 1), g.In(0, 1) ? words[g.In(0, 4)] : "", (float)g.In(14, 34), g.In(0, 1) ? (float)g.In(40, 90) : 0); break;
    case 5: it = ImageItem(r, pic, 256, 192, {g.In(0, 100), g.In(0, 100), g.In(1, 120), g.In(1, 90)}); if (g.In(0, 2) == 0) it.fill = (u32)g.In(1, 255) << 24; break;
    default: it = ImageItem({r.x, r.y, 24 * g.In(1, 4), 24 * g.In(1, 4)}, icon, 24, 24, {0, 0, 24, 24}); break;
    }
    it.layer = g.In(0, 2);
    if (gZoomed)
    {
        static const float zooms[] = {0.75f, 0.8f, 0.85f, 1.05f, 1.35f, 1.5f};
        if (g.In(0, 2)) ZoomItem(it, zooms[g.In(0, 5)], (float)g.In(0, 1280), (float)g.In(0, 720));
        if (it.kind == UiItem::Image) it.smooth = g.In(0, 1);
        if (it.kind == UiItem::Image && g.In(0, 2) == 0) it.tier = g.In(1, 3);
    }
    return it;
}
static void SortLayers(std::vector<UiItem>& items)
{
    std::stable_sort(items.begin(), items.end(), [](const UiItem& a, const UiItem& b) { return a.layer < b.layer; });
}

// The raster's ways are one picture: drawn on one thread or on all of them (in bands), a
// rectangle drawn again over a raster of other items gives the whole raster's bytes there, a text
// mask kept or drawn anew the same; and the window's cache (LayerCache), item changes after item
// changes, holds exactly what a whole raster of each layer gives.
static void raster_ways(bool zoomed)
{
    gZoomed = zoomed;
    Rng g;
    if (zoomed) g.v ^= 0x9E3779B97F4A7C15ull;
    std::vector<u32> pic(256 * 192), icon(24 * 24);
    for (size_t i = 0; i < pic.size(); i++) pic[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
    for (size_t i = 0; i < icon.size(); i++) icon[i] = i % 5 == 0 ? 0 : g.Next();
    const float scales[] = {1, 1.5f, 2, 2.5f, 2.6667f, 3};
    for (int set = 0; set < 60; set++)
    {
        ChipLook look;
        look.face = (Face)g.In(0, 3);
        if (look.face == Face::Keyboard) for (int b = 0; b < kButtonCount; b++) look.glyph[b] = b % 3 ? "Esc" : "W";
        SetChipLook(look);
        std::vector<UiItem> items;
        for (int k = g.In(1, 12); k > 0; k--) items.push_back(RandomItem(g, pic.data(), icon.data()));
        SortLayers(items);
        const float s = scales[set % 6];
        const int w = 700, h = 500;
        std::vector<u32> one(w * h), many(w * h), part(w * h, 0x12345678u), fresh(w * h);
        SetRasterThreads(1);
        Rasterise(items, s * 0.5f, one.data(), w, h, -3, -2);
        SetRasterThreads(0);
        Rasterise(items, s * 0.5f, many.data(), w, h, -3, -2);
        CHECK(one == many);
        // A rectangle drawn over the raster of other items: the whole raster's bytes inside it,
        // the other raster's outside.
        std::vector<UiItem> other = items;
        other.push_back(RandomItem(g, pic.data(), icon.data()));
        SortLayers(other);
        Rasterise(other, s * 0.5f, part.data(), w, h, -3, -2);
        const std::vector<u32> before = part;
        const Rect clip {g.In(0, 300), g.In(0, 200), g.In(1, 400), g.In(1, 300)};
        Rasterise(items, s * 0.5f, part.data(), w, h, -3, -2, clip);
        bool inside = true, outside = true;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                const bool in = x >= clip.x && x < clip.x + clip.w && y >= clip.y && y < clip.y + clip.h;
                const size_t i = (size_t)y * w + x;
                if (in) inside &= part[i] == one[i];
                else outside &= part[i] == before[i];
            }
        CHECK(inside && outside);
        ClearTextMasks();
        Rasterise(items, s * 0.5f, fresh.data(), w, h, -3, -2);
        CHECK(fresh == one);

        // The cache over a run of changes: one item changed, one added, one gone, two swapped,
        // nothing at all; each time every layer as a whole raster draws it.
        LayerCache cache;
        for (int step = 0; step < 6; step++)
        {
            if (step > 0)
            {
                const int what = g.In(0, 4);
                if (what == 0 && !items.empty()) items[g.In(0, (int)items.size() - 1)] = RandomItem(g, pic.data(), icon.data());
                else if (what == 1) items.push_back(RandomItem(g, pic.data(), icon.data()));
                else if (what == 2 && items.size() > 1) items.erase(items.begin() + g.In(0, (int)items.size() - 1));
                else if (what == 3 && items.size() > 1) std::swap(items[0], items[items.size() - 1]);
                else if (!items.empty())
                {
                    // A small move of one item (a focus ring going to its neighbour).
                    UiItem& it = items[g.In(0, (int)items.size() - 1)];
                    it.to.x += g.In(-30, 30);
                    it.to.y += g.In(-20, 20);
                }
                SortLayers(items);
            }
            const float ox = 0.25f * g.In(0, 3), oy = 0.5f * g.In(0, 1);
            cache.Update(items, s, ox, oy);
            const std::vector<LayerAt> layers = LayersAt(items, s, ox, oy);
            CHECK(cache.entries.size() == layers.size());
            for (size_t i = 0; i < layers.size() && i < cache.entries.size(); i++)
            {
                const LayerAt& l = layers[i];
                const LayerCache::Entry& e = cache.entries[i];
                CHECK(e.x == l.x && e.y == l.y && e.w == l.w && e.h == l.h && e.still == l.still && !e.direct);
                std::vector<u32> whole((size_t)l.w * l.h);
                Rasterise(l.items, s, whole.data(), l.w, l.h, l.ox, l.oy);
                CHECK(e.px == whole);
            }
        }
    }
    SetChipLook({});
    gZoomed = false;
    if (zoomed) return;
    // A focus ring moving inside a big layer: only the old and the new ring's pixels are drawn
    // again; nothing at all when nothing changed.
    std::vector<UiItem> menu = {PanelItem({100, 100, 900, 500}), TextItem({140, 140, 500, 60}, "Title", 30)};
    UiItem ring = PanelItem({200, 300, 120, 60});
    ring.plain = true;
    menu.push_back(ring);
    LayerCache cache;
    cache.Update(menu, 2, 0, 0);
    CHECK(cache.entries.size() == 1 && cache.entries[0].changed);
    cache.Update(menu, 2, 0, 0);
    CHECK(!cache.entries[0].changed && cache.entries[0].dirty.empty());
    menu[2].to.x += 300;
    cache.Update(menu, 2, 0, 0);
    long long area = 0;
    for (const Rect& r : cache.entries[0].dirty) area += (long long)r.w * r.h;
    CHECK(cache.entries[0].changed && area > 0 && area * 4 < (long long)cache.entries[0].w * cache.entries[0].h);
    // A chip whose `to` is cut shorter than the chip (the shell's Keys page caps a long key's at
    // 170): the chip still draws whole, and the cache draws it again whole when it changes.
    for (float s : {1.0f, 2.0f, 3.0f})
    {
        std::vector<UiItem> keys = {PanelItem({20, 20, 600, 200})};
        UiItem key = ChipItemAs(60, 60, Face::Keyboard, "KEYPAD MEMSUBTRACT", "", 24, Weight::ExtraBold);
        CHECK(key.to.w > 170);
        key.to.w = 170;
        keys.push_back(key);
        UiItem after = TextItem({400, 60, 200, 40}, "Right", 24);
        keys.push_back(after);
        LayerCache c;
        c.Update(keys, s, 0, 0);
        keys[1] = ChipItemAs(60, 60, Face::Keyboard, "THOUSANDSSEPARATOR", "", 24, Weight::ExtraBold);
        keys[1].to.w = 170;
        c.Update(keys, s, 0, 0);
        const std::vector<LayerAt> l = LayersAt(keys, s, 0, 0);
        std::vector<u32> whole((size_t)l[0].w * l[0].h);
        Rasterise(l[0].items, s, whole.data(), l[0].w, l[0].h, l[0].ox, l[0].oy);
        CHECK(c.entries.size() == 1 && c.entries[0].px == whole);
    }
    // A live crop at a whole factor is the GPU's (WholeFactorCrop); at another scale, or with a
    // transparent pixel, the kit's.
    const std::vector<UiItem> crop = {ImageItem({10, 10, 60, 40}, pic.data(), 256, 192, {5, 5, 30, 20})};
    CHECK(WholeFactorCrop(crop[0], LayersAt(crop, 3, 0, 0)[0], 3));
    CHECK(!WholeFactorCrop(crop[0], LayersAt(crop, 2.25f, 0, 0)[0], 2.25f)); // (4.5x: nearest 4x, then bilinear)
    std::vector<u32> holed = pic;
    holed[10 * 256 + 10] = 0;
    const std::vector<UiItem> crop2 = {ImageItem({10, 10, 60, 40}, holed.data(), 256, 192, {5, 5, 30, 20})};
    CHECK(!WholeFactorCrop(crop2[0], LayersAt(crop2, 3, 0, 0)[0], 3));
}

// The UI's size (UiItem::zoom, dx, dy): an item zoomed is that item drawn at a larger or smaller
// scale (byte for byte the raster of the unzoomed items at scale × zoom), placed and bounded where
// the zoom puts it, hashed as before at 1; FitZoom keeps a group inside its limit. Smooth pictures:
// the scaler's picture kept and the same for the same pixels, blended edges where Pixel has two
// colours, a crop opaque to its corners, the same bytes on any number of threads, never the GPU's.
static void ui_scale_and_smooth()
{
    // Placed, ZoomItem (composed), FitZoom.
    UiItem p = PanelItem({110, 456, 1060, 250});
    const u64 h0 = Hash({p});
    ZoomItem(p, 1, 640, 720);
    CHECK(Hash({p}) == h0 && Placed(p).x == 110 && Placed(p).w == 1060);
    ZoomItem(p, 0.75f, 640, 720);
    const Rect q = Placed(p);
    CHECK(q.x == 242 && q.x + q.w == 1038 && q.y == 522 && q.y + q.h == 710);
    CHECK(Hash({p}) != h0);
    ZoomItem(p, 1 / 0.75f, 640, 720);
    CHECK(std::abs(p.zoom - 1) < 1e-5f && std::abs(p.dx) < 1e-3f && std::abs(p.dy) < 1e-3f);
    CHECK(std::abs(FitZoom({110, 426, 1060, 280}, 1.5f, 640, 720, {4, 4, 1272, 712}) - 636.0f / 530) < 1e-4f);
    CHECK(FitZoom({110, 426, 1060, 280}, 0.75f, 640, 720, {4, 4, 1272, 712}) == 0.75f);
    CHECK(FitZoom({16, 667, 400, 48}, 1.5f, 16, 691, {0, 665, 1280, 55}) < 1.09f);

    // Zoomed by z about the origin at scale s: the raster of the items as they are at s × z.
    std::vector<u32> pic(256 * 192);
    for (size_t i = 0; i < pic.size(); i++) pic[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
    std::vector<UiItem> items = {PanelItem({20, 20, 300, 160}), WellItem({40, 60, 120, 80}),
                                 TextItem({40, 30, 260, 30}, "Crème brûlée, twice", 22), ChipItem(180, 70, Button::A, "Talk", 20),
                                 ImageItem({190, 110, 60, 45}, pic.data(), 256, 192, {10, 10, 20, 15})};
    for (float z : {0.75f, 1.5f})
    {
        std::vector<UiItem> zoomed = items;
        for (UiItem& it : zoomed) ZoomItem(it, z, 0, 0);
        const int w = 520, h = 300;
        std::vector<u32> a(w * h), b(w * h);
        Rasterise(zoomed, 1.0f, a.data(), w, h);
        Rasterise(items, z, b.data(), w, h);
        CHECK(a == b);
        const Rect bz = Bounds(zoomed), bi = Bounds(items);
        CHECK(bz.x <= (int)std::floor(bi.x * z) && bz.x + bz.w >= (int)std::ceil((bi.x + bi.w) * z) - 1);
    }

    // Smooth: a picture with a diagonal edge, 4x.
    std::vector<u32> tile(16 * 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) tile[y * 16 + x] = x > y ? 0xFFF0E0A0u : 0xFF203060u;
    UiItem px = ImageItem({0, 0, 64, 64}, tile.data(), 16, 16, {0, 0, 16, 16});
    UiItem sm = px;
    sm.smooth = true;
    CHECK(HashImage(px) != HashImage(sm));
    const auto e1 = Enlarge(sm, 4), e2 = Enlarge(sm, 4);
    CHECK(e1 && e1 == e2 && e1->w == 64 && e1->h == 64 && Enlarge(sm, 3) != e1 && !Enlarge(sm, 7) && !Enlarge(sm, 1));
    std::vector<u32> a(64 * 64), b(64 * 64), c(64 * 64);
    Rasterise({px}, 1, a.data(), 64, 64);
    SetRasterThreads(1);
    Rasterise({sm}, 1, b.data(), 64, 64);
    SetRasterThreads(0);
    Rasterise({sm}, 1, c.data(), 64, 64);
    CHECK(b == c);
    auto colours = [](const std::vector<u32>& v) {
        std::vector<u32> u(v);
        std::sort(u.begin(), u.end());
        return (int)(std::unique(u.begin(), u.end()) - u.begin());
    };
    CHECK(colours(a) == 2 && colours(b) > 2);
    bool opaque = true;
    for (u32 v : b) opaque &= (v >> 24) == 0xFF;
    CHECK(opaque); // (the crop's edges repeated: no corner rounded off into transparency)
    CHECK(!WholeFactorCrop(sm, LayersAt({sm}, 1, 0, 0)[0], 1) && WholeFactorCrop(px, LayersAt({px}, 1, 0, 0)[0], 1));
    UiItem zc = px;
    ZoomItem(zc, 0.5f, 0, 0);
    CHECK(!WholeFactorCrop(zc, LayersAt({zc}, 2, 0, 0)[0], 2));
    // Drawn at its own size or smaller: no scaler (Pixel's bytes).
    UiItem small = sm;
    small.to = {0, 0, 16, 16};
    UiItem smallPx = px;
    smallPx.to = small.to;
    std::vector<u32> d(16 * 16), e(16 * 16);
    Rasterise({small}, 1, d.data(), 16, 16);
    Rasterise({smallPx}, 1, e.data(), 16, 16);
    CHECK(d == e);
    // ImageItem follows SetGamePictures.
    SetGamePictures(true);
    CHECK(ImageItem({0, 0, 8, 8}, tile.data(), 16, 16, {0, 0, 8, 8}).smooth);
    SetGamePictures(false);
    CHECK(!ImageItem({0, 0, 8, 8}, tile.data(), 16, 16, {0, 0, 8, 8}).smooth);
    ClearEnlarged();
    CHECK(Enlarge(sm, 4) != e1 && *Enlarge(sm, 4)->px.data() == *e1->px.data());
    // A live crop of a DS screen (a 256 x 192 picture): made again each time, not kept, the same
    // pixels all the same.
    std::vector<u32> screen(256 * 192);
    for (size_t i = 0; i < screen.size(); i++) screen[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
    UiItem live = ImageItem({0, 0, 64, 64}, screen.data(), 256, 192, {40, 30, 16, 16});
    live.smooth = true;
    const auto l1 = Enlarge(live, 4), l2 = Enlarge(live, 4);
    CHECK(l1 && l2 && l1 != l2 && l1->px == l2->px);
    // The icons' cache (64 MB): full, the oldest out first; the newest kept.
    ClearEnlarged();
    std::vector<std::vector<u32>> bigs(8, std::vector<u32>(250 * 250));
    std::vector<std::shared_ptr<const Enlarged>> made;
    for (size_t n = 0; n < bigs.size(); n++)
    {
        for (size_t i = 0; i < bigs[n].size(); i++) bigs[n][i] = 0xFF000000u | (u32)((i + n * 7919) * 2654435761u >> 8);
        UiItem big = ImageItem({0, 0, 1500, 1500}, bigs[n].data(), 250, 250, {0, 0, 250, 250});
        big.smooth = true;
        made.push_back(Enlarge(big, 6)); // (9 MB each: the eighth goes past 64 MB)
        CHECK(made.back() && Enlarge(big, 6) == made.back());
    }
    for (size_t n = bigs.size(); n-- > 0;) // (newest first: a hit changes nothing; the first is out)
    {
        UiItem big = ImageItem({0, 0, 1500, 1500}, bigs[n].data(), 250, 250, {0, 0, 250, 250});
        big.smooth = true;
        CHECK((Enlarge(big, 6) == made[n]) == (n != 0));
    }
    ClearEnlarged();
}

// Tiers (UiItem::tier): a picture pixel is TierFactor(tier, scale x zoom) whole output pixels,
// never resampled, centred on `to`; at 100 % the same bytes as the untiered picture `tier` times
// its size; the layer has room for blocks that pass `to`; the GPU may draw a tiered crop.
static void tiers()
{
    CHECK(TierFactor(kTierS, 1) == 2 && TierFactor(kTierL, 1) == 3);
    CHECK(TierFactor(kTierS, 1.5f) == 3 && TierFactor(kTierL, 1.5f) == 4); // (4.5: a half down)
    CHECK(TierFactor(kTierL, 1.6f) == 5 && TierFactor(kTierL, 3) == 9 && TierFactor(kTierS, 0.3f) == 1);
    CHECK(TierFactor(kTierS, 1.3f * 1.111f) == 3 && TierFactor(0, 2) == 1);
    // Two colours, a transparent corner.
    std::vector<u32> icon(16 * 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) icon[y * 16 + x] = x + y < 4 ? 0 : x > y ? 0xFFF0E0A0u : 0xFF203060u;
    const UiItem t = TierImage({100, 100, 60, 60}, icon.data(), 16, 16, {0, 0, 16, 16}, kTierS);
    CHECK(t.tier == kTierS && t.to.x == 114 && t.to.y == 114 && t.to.w == 32 && t.to.h == 32);
    UiItem plain = t;
    plain.tier = 0;
    CHECK(HashImage(plain) != HashImage(t));
    {   // 100 %: the untiered picture's bytes.
        std::vector<u32> a(300 * 300), b(300 * 300);
        Rasterise({t}, 1, a.data(), 300, 300);
        Rasterise({plain}, 1, b.data(), 300, 300);
        CHECK(a == b);
    }
    auto colours = [](const std::vector<u32>& v) {
        std::vector<u32> u(v);
        std::sort(u.begin(), u.end());
        return (int)(std::unique(u.begin(), u.end()) - u.begin());
    };
    for (float s : {1.5f, 1.6f, 2.25f, 3.0f})
    {
        const int k = TierFactor(kTierL, s);
        UiItem l = TierImage({100, 100, 60, 60}, icon.data(), 16, 16, {0, 0, 16, 16}, kTierL);
        const Rect r = TierRect(l, s, 0, 0);
        CHECK(r.w == 16 * k && r.h == 16 * k);
        // Centred on `to` (canvas 100 + 6 .. + 48: its centre 130) within a pixel.
        CHECK(std::abs(r.x + r.w / 2.0f - 130 * s) <= 1 && std::abs(r.y + r.h / 2.0f - 130 * s) <= 1);
        const int W = (int)(400 * s), H = W;
        std::vector<u32> a((size_t)W * H);
        Rasterise({l}, s, a.data(), W, H);
        // Three values (transparent, two colours), each picture pixel a whole k x k block.
        CHECK(colours(a) == 3);
        bool blocks = true;
        int drawn = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                const u32 v = a[(size_t)y * W + x];
                const bool in = x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
                if (!in) { blocks &= v == 0; continue; }
                drawn += v != 0;
                const int i = (x - r.x) / k, j = (y - r.y) / k;
                blocks &= v == icon[j * 16 + i];
            }
        CHECK(blocks && drawn == (16 * 16 - 10) * k * k);
        // Its layer holds every block (5 x 16 = 80 output pixels where `to` is 76.8 at 1.6), drawn as the whole raster.
        const std::vector<LayerAt> at = LayersAt({l}, s, 0.25f, 0.5f);
        CHECK(at.size() == 1);
        std::vector<u32> lp((size_t)at[0].w * at[0].h);
        Rasterise(at[0].items, s, lp.data(), at[0].w, at[0].h, at[0].ox, at[0].oy);
        int n = 0;
        for (u32 v : lp) n += v != 0;
        CHECK(n == drawn);
        // Smooth at exactly k (9: the scaler's 3, then 3 x 3 blocks); the same on any number of threads.
        UiItem sm = l;
        sm.smooth = true;
        std::vector<u32> b((size_t)W * H), c((size_t)W * H);
        SetRasterThreads(1);
        Rasterise({sm}, s, b.data(), W, H);
        SetRasterThreads(0);
        Rasterise({sm}, s, c.data(), W, H);
        CHECK(b == c && (k == 7 ? b == a : colours(b) > 3)); // (7: no scaler divides it, nearest alone)
        bool inside = true;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (b[(size_t)y * W + x] && !(x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h)) inside = false;
        CHECK(inside);
        if (k == 9)
        {
            const auto e3 = Enlarge(sm, 3);
            CHECK(e3 && b[(size_t)(r.y + 4) * W + r.x + 40] == e3->px[(size_t)1 * e3->w + 13]);
        }
    }
    // Zoomed: the factor of scale x zoom.
    UiItem z = TierImage({100, 100, 60, 60}, icon.data(), 16, 16, {0, 0, 16, 16}, kTierS);
    ZoomItem(z, 1.5f, 0, 0);
    CHECK(TierRect(z, 1, 0, 0).w == 48 && TierRect(z, 2, 0, 0).w == 96);
    // An opaque small tiered picture is drawn by whole blocks at any scale (WholeFactorCrop), at
    // TierRect; transparent pixels are not.
    std::vector<u32> opaque(48 * 32);
    for (size_t i = 0; i < opaque.size(); i++) opaque[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
    const std::vector<UiItem> small = {TierImage({10, 10, 90, 60}, opaque.data(), 48, 32, {5, 5, 30, 20}, kTierL)};
    const LayerAt at = LayersAt(small, 1.6f, 0, 0)[0];
    CHECK(Tiered(small[0]) && WholeFactorCrop(small[0], at, 1.6f));
    const Rect cr = CropRect(small[0], 1.6f, at.x, at.y, at.ox, at.oy);
    CHECK(cr.w == 150 && cr.h == 100 && cr.x >= at.x && cr.x + cr.w <= at.x + at.w);
    CHECK(!WholeFactorCrop(t, LayersAt({t}, 1.5f, 0, 0)[0], 1.5f));
    // Large pictures ignore a tier (the ruling: tiers for sources of 64 pixels or less, never a
    // live crop of a DS screen): a touch-screen crop and an 80-pixel picture fill `to` the Sharp
    // way, the bytes and hash of the untiered item, at 100 % and at 1.6.
    std::vector<u32> pic(256 * 192);
    for (size_t i = 0; i < pic.size(); i++) pic[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
    std::vector<u32> big(80 * 80);
    for (size_t i = 0; i < big.size(); i++) big[i] = 0xFF000000u | (u32)(i * 40503u);
    for (UiItem tiered : {TierImage({10, 10, 90, 60}, pic.data(), 256, 192, {5, 5, 30, 20}, kTierL),
                          TierImage({10, 10, 240, 240}, big.data(), 80, 80, {0, 0, 80, 80}, kTierL)})
    {
        UiItem untiered = tiered;
        untiered.tier = 0;
        CHECK(!Tiered(tiered) && HashImage(tiered) == HashImage(untiered) && TierRect(tiered, 1.6f, 0, 0).w == 0);
        for (float s : {1.0f, 1.6f})
        {
            const int W = (int)(260 * s), H = W;
            std::vector<u32> a((size_t)W * H), b((size_t)W * H);
            Rasterise({tiered}, s, a.data(), W, H);
            Rasterise({untiered}, s, b.data(), W, H);
            CHECK(a == b);
        }
        CHECK(WholeFactorCrop(tiered, LayersAt({tiered}, 1.6f, 0, 0)[0], 1.6f) == WholeFactorCrop(untiered, LayersAt({untiered}, 1.6f, 0, 0)[0], 1.6f));
    }
    std::vector<u32> b64(64 * 40, 0xFF102030u);
    CHECK(Tiered(TierImage({0, 0, 128, 80}, b64.data(), 64, 40, {0, 0, 64, 40}, kTierS)));
    CHECK(!Tiered(TierImage({0, 0, 130, 80}, b64.data(), 65, 40, {0, 0, 65, 40}, kTierS)));
    // TierImage centres with floor (an odd room: the left gets the smaller half).
    CHECK(TierImage({0, 0, 33, 33}, b64.data(), 64, 40, {0, 0, 16, 16}, kTierS).to.x == 0);
    // An empty picture (IconCache::Tiered of a frame that does not read) draws nothing and has no footprint.
    UiItem none = ImageItem({10, 10, 0, 0}, nullptr, 0, 0, {});
    none.tier = kTierL;
    {
        LayerCache lc;
        const std::vector<UiItem> two = {PanelItem({20, 20, 200, 100}), none};
        lc.Update(two, 1, 0, 0);
        CHECK(lc.entries.size() == 1);
    }
}

// The focus ring: the game's green at least 5 wide outside, then a line chosen by what it rings:
// white on a dark fill (3:1 or more for white), the theme's shadow on a light one.
static void focus_ring()
{
    CHECK(kFocusOuterW - kFocusInnerW >= 5);
    CHECK(Contrast(0xFFFFFFFF, 0xFF000000) > 20.9f && std::abs(Contrast(0xFFFFFFFF, kFillRow) - 4.58f) < 0.02f);
    CHECK(std::abs(Contrast(0xFFFFFFFF, kFillFocus) - 7.15f) < 0.02f && std::abs(Contrast(0xFFFFFFFF, 0xFF9AA2E3) - 2.43f) < 0.02f);
    for (u32 dark : {kFillRow, kFillFocus, GameTheme().fill, GameTheme().frame})
    {
        const FocusRingItems g = FocusRing({100, 100, 200, 50}, 25, dark);
        CHECK(g.inner.fill == kFocusInner && g.outer.fill == kFocusOuter);
        CHECK(g.outer.to.x == 100 - kFocusOuterW && g.inner.to.x == 100 - kFocusInnerW && g.outer.radius == 25 + kFocusOuterW);
    }
    for (u32 light : {0xFF9AA2E3u, 0xFFB4BBF0u, 0xFFF0E2B8u}) // (the stat panel's plate and focused button, the note's paper)
    {
        const FocusRingItems g = FocusRing({100, 100, 200, 50}, 25, light);
        CHECK(g.inner.fill == GameTheme().shadow && Contrast(g.inner.fill, light) >= 3); // (WCAG 1.4.11: 3:1 for a UI part)
    }
    CHECK(FocusRing({0, 0, 10, 10}, 5).inner.fill == kFocusInner); // (by default: a row's fill)
    // Drawn: at the ring's middle row, the green band 5 wide, then the line, then the item.
    const FocusRingItems g = FocusRing({40, 40, 200, 60}, 10, 0xFF9AA2E3);
    UiItem item = PanelItem({40, 40, 200, 60});
    item.plain = true;
    item.fill = 0xFF9AA2E3;
    std::vector<u32> px(300 * 150);
    Rasterise({g.outer, g.inner, item}, 1, px.data(), 300, 150);
    int green = 0, line = 0;
    for (int x = 20; x < 40; x++)
    {
        const u32 v = px[70 * 300 + x] | 0xFF000000u;
        green += v == kFocusOuter;
        line += v == GameTheme().shadow;
    }
    CHECK(green >= 5 && line >= kFocusInnerW - 1);
}

// A panel's sign (the kit's scroll arrows, the wireless sign): drawn in its middle in its colour,
// pointing its way; a panel without one rasters and hashes as before.
static void signs()
{
    auto draw = [](const UiItem& it, int w, int h, float s) {
        std::vector<u32> px((size_t)w * h);
        Rasterise({it}, s, px.data(), w, h);
        return px;
    };
    for (int s : {1, 2})
    {
        const int w = 80 * s, h = 80 * s;
        auto at = [&](const std::vector<u32>& px, int x, int y) { return px[(size_t)y * w + x]; };
        UiItem up = ScrollArrowItem({10, 10, 60, 60}, UiItem::Sign::Up, true);
        UiItem down = ScrollArrowItem({10, 10, 60, 60}, UiItem::Sign::Down, true);
        const std::vector<u32> pu = draw(up, w, h, (float)s), pd = draw(down, w, h, (float)s);
        // Up: white near its apex, the button's fill past its base; Down the other way round.
        CHECK(Rgb(at(pu, 40 * s, 32 * s)) == 0xFFFFFF);
        CHECK(Rgb(at(pu, 40 * s, 56 * s)) == Rgb(kFillRow));
        CHECK(Rgb(at(pd, 40 * s, 48 * s)) == 0xFFFFFF);
        CHECK(Rgb(at(pd, 40 * s, 24 * s)) == Rgb(kFillRow));
        // Beside the apex (a triangle, not a box): the fill.
        CHECK(Rgb(at(pu, 26 * s, 34 * s)) == Rgb(kFillRow));
        // Off: sunk, the triangle faint (not white).
        const std::vector<u32> po = draw(ScrollArrowItem({10, 10, 60, 60}, UiItem::Sign::Up, false), w, h, (float)s);
        CHECK(Rgb(at(po, 40 * s, 36 * s)) != 0xFFFFFF);
        // The wireless sign: its dot low in the box, nothing at the box's corners.
        UiItem wl = PanelItem({10, 10, 60, 60});
        wl.plain = true;
        wl.fill = 0;
        wl.sign = UiItem::Sign::Wireless;
        const std::vector<u32> pw = draw(wl, w, h, (float)s);
        CHECK(Rgb(at(pw, 40 * s, 58 * s)) == 0xFFFFFF);
        CHECK(at(pw, 12 * s, 12 * s) == 0);
    }
    // The hash: a sign changes it; none hashes as a panel always did.
    UiItem a = PanelItem({0, 0, 40, 40}), b = a;
    CHECK(Hash({a}) == Hash({b}));
    b.sign = UiItem::Sign::Left;
    CHECK(Hash({a}) != Hash({b}));
}

int main()
{
    wrap_cases();
    determinism();
    panel_pixels();
    text_inside();
    chips();
    image_scaler();
    edges();
    still_layers();
    raster_ways(false);
    raster_ways(true);
    ui_scale_and_smooth();
    tiers();
    focus_ring();
    signs();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("uikit_test: all passed\n");
    return 0;
}
