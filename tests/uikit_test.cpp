// Checks the UI kit (src/ui): word wrap, the raster's determinism, a panel's outline, frame and
// fill where they belong at scale 1 and 2, text kept inside its box, chips growing with their
// label, the picture scaler. Made-up text only.
//   ninja -C build uikit_test && ./build/uikit_test
#include <cstdio>
#include <string>
#include <vector>

#include "ui/kit.h"

using namespace sims2wide;
using namespace sims2wide::ui;

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

int main()
{
    wrap_cases();
    determinism();
    panel_pixels();
    text_inside();
    chips();
    image_scaler();
    edges();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("uikit_test: all passed\n");
    return 0;
}
