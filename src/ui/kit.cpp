#include "ui/kit.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sims2wide::ui
{
const Theme& GameTheme()
{
    // The game's own colours (CLAUDE.md, the console's frames): frame #6171B2 / #5961A2, fills
    // #828ACB, the dark outline and text shadow #383869, the backdrop #101218.
    static const Theme t {0xFF6171B2, 0xFF5961A2, 0xFF828ACB, 0xFF6A72B8, 0xFFFFFFFF, 0xFF383869, 0xFF101218, 18, 5};
    return t;
}

namespace
{
// Chip geometry, canvas pixels, from the label's text size.
float ChipHeight(float size) { return std::round(size * 1.3f); }
bool Cross(Button b) { return b == Button::DPad || b == Button::DPadV || b == Button::DPadH || b == Button::DPadL || b == Button::DPadU ||
                        b == Button::DPadR || b == Button::DPadD; }
bool Round(Button b) { return b == Button::A || b == Button::B || b == Button::X || b == Button::Y || Cross(b) || b == Button::LStick ||
                        b == Button::RStick; }
const char* ButtonText(Button b)
{
    switch (b)
    {
    case Button::A: return "A";
    case Button::B: return "B";
    case Button::X: return "X";
    case Button::Y: return "Y";
    case Button::LB: return "LB";
    case Button::RB: return "RB";
    case Button::Start: return "START";
    case Button::LStick: return "L";
    case Button::RStick: return "R";
    case Button::DPad: case Button::DPadV: case Button::DPadH: case Button::DPadL: case Button::DPadU: case Button::DPadR: case Button::DPadD: return "";
    }
    return "";
}
float PillText(float size) { return size * 0.56f; }
float ChipWidth(Button b, float size)
{
    const float h = ChipHeight(size);
    if (Round(b)) return h;
    return std::max(std::round(TextWidth(ButtonText(b), PillText(size), Weight::ExtraBold) + h * 0.75f), std::round(h * 1.4f));
}
float LabelGap(float size) { return std::round(size * 0.38f); }

// The layer being drawn: premultiplied RGBA floats over the buffer's pixels.
struct Layer
{
    int w, h;
    std::vector<float> p;
    void Over(int x, int y, u32 argb, float cover)
    {
        const float a = ((argb >> 24) / 255.0f) * cover;
        if (a <= 0) return;
        float* d = &p[((size_t)y * w + x) * 4];
        const float k = 1 - a;
        d[0] = ((argb >> 16) & 0xFF) / 255.0f * a + d[0] * k;
        d[1] = ((argb >> 8) & 0xFF) / 255.0f * a + d[1] * k;
        d[2] = (argb & 0xFF) / 255.0f * a + d[2] * k;
        d[3] = a + d[3] * k;
    }
};

// Signed distance from (px, py) to a rounded rectangle (negative inside).
float RoundRectDistance(float px, float py, float x0, float y0, float x1, float y1, float r)
{
    const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hw = (x1 - x0) / 2, hh = (y1 - y0) / 2;
    r = std::clamp(r, 0.0f, std::min(hw, hh));
    const float qx = std::fabs(px - cx) - (hw - r), qy = std::fabs(py - cy) - (hh - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

// A rounded rectangle in layer pixels, anti-aliased; soft > 0 blurs its edge over that many
// pixels each side (shadows).
void FillRoundRect(Layer& L, float x0, float y0, float x1, float y1, float r, u32 argb, float soft = 0)
{
    if (x1 <= x0 || y1 <= y0 || !(argb >> 24)) return;
    const float m = soft + 1;
    const int X0 = std::max((int)std::floor(x0 - m), 0), X1 = std::min((int)std::ceil(x1 + m), L.w);
    const int Y0 = std::max((int)std::floor(y0 - m), 0), Y1 = std::min((int)std::ceil(y1 + m), L.h);
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++)
        {
            const float d = RoundRectDistance(x + 0.5f, y + 0.5f, x0, y0, x1, y1, r);
            float c;
            if (soft > 0)
            {
                const float t = std::clamp((soft - d) / (2 * soft), 0.0f, 1.0f);
                c = t * t * (3 - 2 * t);
            }
            else c = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (c > 0) L.Over(x, y, argb, c);
        }
}

u32 WithAlpha(u32 argb, float a) { return (argb & 0xFFFFFF) | ((u32)std::lround(std::clamp(a, 0.0f, 1.0f) * (argb >> 24)) << 24); }

// A box blur of radius r (each way), in place.
void Blur(std::vector<u8>& m, int w, int h, int r)
{
    if (r <= 0) return;
    std::vector<u8> t(m.size());
    const int n = 2 * r + 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int s = 0;
            for (int k = -r; k <= r; k++) { const int X = x + k; if (X >= 0 && X < w) s += m[(size_t)y * w + X]; }
            t[(size_t)y * w + x] = (u8)((s + n / 2) / n);
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int s = 0;
            for (int k = -r; k <= r; k++) { const int Y = y + k; if (Y >= 0 && Y < h) s += t[(size_t)Y * w + x]; }
            m[(size_t)y * w + x] = (u8)((s + n / 2) / n);
        }
}

// Text lines in a box of the layer (bx0..bx1, by0..by1, layer pixels, clipped to it): the
// shadow first, then the text. size: canvas pixels per em; s: layer pixels per canvas pixel.
void DrawTextBox(Layer& L, const std::vector<std::string>& lines, float size, Weight w, Align align, float s, float bx0, float by0,
                 float bx1, float by1, u32 colour, u32 shadow, float lineHeight, float firstBaseline)
{
    const int X0 = std::max((int)std::floor(bx0), 0), X1 = std::min((int)std::ceil(bx1), L.w);
    const int Y0 = std::max((int)std::floor(by0), 0), Y1 = std::min((int)std::ceil(by1), L.h);
    const int mw = X1 - X0, mh = Y1 - Y0;
    if (mw <= 0 || mh <= 0 || lines.empty()) return;
    std::vector<u8> mask((size_t)mw * mh, 0);
    for (size_t i = 0; i < lines.size(); i++)
    {
        const float lw = TextWidth(lines[i], size, w) * s;
        float x = bx0;
        if (align == Align::Centre) x = (bx0 + bx1 - lw) / 2;
        else if (align == Align::Right) x = bx1 - lw;
        const float base = by0 + (firstBaseline + i * lineHeight) * s;
        DrawLine(lines[i], size * s, w, x - X0, base - Y0, mask.data(), mw, mh);
    }
    if (shadow >> 24)
    {
        // One canvas pixel down and right, softened in proportion when scaled up.
        const int off = std::max(1, (int)std::lround(s));
        std::vector<u8> sh((size_t)mw * mh, 0);
        if (off < mw)
            for (int y = off; y < mh; y++) memcpy(&sh[(size_t)y * mw + off], &mask[(size_t)(y - off) * mw], (size_t)(mw - off));
        Blur(sh, mw, mh, (int)std::floor(s * 0.6f));
        for (int y = 0; y < mh; y++)
            for (int x = 0; x < mw; x++)
                if (const u8 c = sh[(size_t)y * mw + x]) L.Over(X0 + x, Y0 + y, shadow, c / 255.0f);
    }
    for (int y = 0; y < mh; y++)
        for (int x = 0; x < mw; x++)
            if (const u8 c = mask[(size_t)y * mw + x]) L.Over(X0 + x, Y0 + y, colour, c / 255.0f);
}

void DrawPanel(Layer& L, const UiItem& it, float s, float ox, float oy)
{
    const Theme& t = GameTheme();
    const float x0 = (it.to.x - ox) * s, y0 = (it.to.y - oy) * s, x1 = x0 + it.to.w * s, y1 = y0 + it.to.h * s;
    const float r = it.radius * s, line = std::max(1.0f, std::round(s));
    if (it.plain) { FillRoundRect(L, x0, y0, x1, y1, r, it.fill); return; }
    if (it.raised) FillRoundRect(L, x0, y0 + 3 * s, x1, y1 + 3 * s, r, 0x66000000, 4 * s);
    // The dark outline, the frame's band, then the fill.
    FillRoundRect(L, x0, y0, x1, y1, r, t.shadow);
    float in = line;
    if (it.border > 0)
    {
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, r - in, it.borderArgb);
        in += it.border * s;
        // The inner edge of the band, as the game draws it: a thin dark line.
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, std::max(r - in, 3 * s), WithAlpha(t.shadow, 0.55f));
        in += line * 0.75f;
    }
    const float ri = std::max(r - in, 3 * s);
    if (it.raised)
    {
        // A light bevel along the fill's top edge (the panel stands out)…
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, ri, 0x40FFFFFF);
        FillRoundRect(L, x0 + in, y0 + in + line, x1 - in, y1 - in, ri, it.fill);
    }
    else
    {
        // …or a dark one (a well sunk into its panel).
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, ri, WithAlpha(t.shadow, 0.7f));
        FillRoundRect(L, x0 + in, y0 + in + line * 2, x1 - in, y1 - in, ri, it.fill);
    }
}

void DrawChip(Layer& L, const UiItem& it, float s, float ox, float oy)
{
    const float size = it.size, h = ChipHeight(size), cw = ChipWidth(it.button, size);
    // In a column (labelAt), the chip is centred in the room before its label.
    const float shift = it.labelAt > 0 ? std::max(0.0f, std::round((it.labelAt - LabelGap(size) - cw) / 2)) : 0;
    const float x0 = (it.to.x + shift - ox) * s, y0 = (it.to.y - oy) * s + (it.to.h - h) / 2 * s;
    const float x1 = x0 + cw * s, y1 = y0 + h * s, r = h / 2 * s, line = std::max(1.0f, std::round(s * 1.25f));
    FillRoundRect(L, x0, y0 + 1.5f * s, x1, y1 + 1.5f * s, r, 0x59000000, 1.5f * s);
    FillRoundRect(L, x0, y0, x1, y1, r, 0xFFE4E7FA);                                // the light rim
    FillRoundRect(L, x0 + line, y0 + line, x1 - line, y1 - line, r - line, 0xFF2E3063); // the dark face
    // A soft sheen on the face's upper half.
    FillRoundRect(L, x0 + line * 2, y0 + line * 1.5f, x1 - line * 2, (y0 + y1) / 2, r - line * 2, 0x1FFFFFFF);
    if (it.button == Button::DPad || it.button == Button::DPadV || it.button == Button::DPadH)
    {
        // A cross: two rounded bars (one faint when only the other's directions do something).
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, a = h * 0.29f * s, b = h * 0.095f * s;
        FillRoundRect(L, cx - a, cy - b, cx + a, cy + b, b * 0.5f, it.button == Button::DPadV ? 0x59FFFFFF : 0xFFFFFFFF);
        FillRoundRect(L, cx - b, cy - a, cx + b, cy + a, b * 0.5f, it.button == Button::DPadH ? 0x59FFFFFF : 0xFFFFFFFF);
    }
    else if (Cross(it.button))
    {
        // The cross faint, its one arm lit (from the centre out).
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, a = h * 0.29f * s, b = h * 0.095f * s;
        FillRoundRect(L, cx - a, cy - b, cx + a, cy + b, b * 0.5f, 0x38FFFFFF);
        FillRoundRect(L, cx - b, cy - a, cx + b, cy + a, b * 0.5f, 0x38FFFFFF);
        // The lit arm a little wider than the cross, from its centre out.
        const Button d = it.button;
        const float w = b * 1.45f;
        const float l = d == Button::DPadL ? cx - a : d == Button::DPadR ? cx - w * 0.5f : cx - w;
        const float r = d == Button::DPadR ? cx + a : d == Button::DPadL ? cx + w * 0.5f : cx + w;
        const float t = d == Button::DPadU ? cy - a : d == Button::DPadD ? cy - w * 0.5f : cy - w;
        const float u = d == Button::DPadD ? cy + a : d == Button::DPadU ? cy + w * 0.5f : cy + w;
        FillRoundRect(L, l, t, r, u, w * 0.5f, 0xFFFFFFFF);
    }
    else
    {
        // The face buttons in their usual colours; the others white.
        u32 c = 0xFFFFFFFF;
        switch (it.button)
        {
        case Button::A: c = 0xFF6EDB5E; break;
        case Button::B: c = 0xFFFF6A5E; break;
        case Button::X: c = 0xFF5CB4FF; break;
        case Button::Y: c = 0xFFFFD34D; break;
        default: break;
        }
        const bool round = Round(it.button);
        if (it.button == Button::LStick || it.button == Button::RStick)
        {
            // A stick: a light ring inside the face, the letter smaller.
            const float in = (y1 - y0) * 0.16f;
            FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, r - in, 0xB3E4E7FA);
            FillRoundRect(L, x0 + in + line, y0 + in + line, x1 - in - line, y1 - in - line, r - in - line, 0xFF2E3063);
        }
        const float ts = it.button == Button::LStick || it.button == Button::RStick ? size * 0.6f : round ? size * 0.82f : PillText(size);
        const LineMetrics m = Metrics(ts, Weight::ExtraBold);
        // Centred on the cap height (about 0.7 em).
        const float base = (h + ts * 0.70f) / 2;
        DrawTextBox(L, {ButtonText(it.button)}, ts, Weight::ExtraBold, Align::Centre, s, x0, y0, x1, y1, c, 0, m.lineHeight, base);
    }
    if (!it.text.empty())
    {
        const float lx = (it.to.x - ox + std::max(cw + LabelGap(size), it.labelAt)) * s;
        const LineMetrics m = Metrics(size, it.weight);
        const float base = (it.to.h + size * 0.70f) / 2;
        DrawTextBox(L, {it.text}, size, it.weight, Align::Left, s, lx, (it.to.y - oy) * s, (it.to.x + it.to.w - ox) * s,
                    (it.to.y + it.to.h - oy) * s, it.colour, it.shadowArgb, m.lineHeight, base);
    }
}

// A picture's pixel after nearest scaling by k (clamped to the rectangle).
u32 Texel(const UiItem& it, int k, int i, int j)
{
    const int x = std::clamp(i / k, 0, it.from.w - 1), y = std::clamp(j / k, 0, it.from.h - 1);
    const int X = std::clamp(it.from.x + x, 0, it.srcW - 1), Y = std::clamp(it.from.y + y, 0, it.srcH - 1);
    return it.src[(size_t)Y * it.srcW + X];
}

void DrawImage(Layer& L, const UiItem& it, float s, float ox, float oy)
{
    if (!it.src || it.from.w <= 0 || it.from.h <= 0 || it.srcW <= 0 || it.srcH <= 0) return;
    const float x0 = (it.to.x - ox) * s, y0 = (it.to.y - oy) * s, tw = it.to.w * s, th = it.to.h * s;
    if (tw <= 0 || th <= 0) return;
    // Nearest by the whole factor keeps the pixels square; bilinear covers what is left.
    const int k = std::max(1, (int)std::floor(std::min(tw / it.from.w, th / it.from.h)));
    const int nw = it.from.w * k, nh = it.from.h * k;
    const float alpha = it.fill ? (it.fill >> 24) / 255.0f : 1.0f;
    const int X0 = std::max((int)std::lround(x0), 0), X1 = std::min((int)std::lround(x0 + tw), L.w);
    const int Y0 = std::max((int)std::lround(y0), 0), Y1 = std::min((int)std::lround(y0 + th), L.h);
    for (int y = Y0; y < Y1; y++)
    {
        const float v = std::clamp((y + 0.5f - y0) / th * nh - 0.5f, 0.0f, nh - 1.0f);
        const int j = (int)v;
        const float fy = v - j;
        for (int x = X0; x < X1; x++)
        {
            const float u = std::clamp((x + 0.5f - x0) / tw * nw - 0.5f, 0.0f, nw - 1.0f);
            const int i = (int)u;
            const float fx = u - i;
            const u32 a = Texel(it, k, i, j), b = Texel(it, k, i + 1, j), c = Texel(it, k, i, j + 1), d = Texel(it, k, i + 1, j + 1);
            // The picture's own alpha (an icon's transparent pixels) weighs each texel's colour
            // (premultiplied), so an edge takes no colour from what is transparent beside it.
            // All four opaque (a live crop of a DS screen), the weights are exactly 1: the
            // pixel is what it was before alpha was looked at.
            const bool opaque = ((a & b & c & d) >> 24) == 0xFF;
            const float wa = (a >> 24) / 255.0f, wb = (b >> 24) / 255.0f, wc = (c >> 24) / 255.0f, wd = (d >> 24) / 255.0f;
            const float cover = opaque ? 1.0f : (wa * (1 - fx) + wb * fx) * (1 - fy) + (wc * (1 - fx) + wd * fx) * fy;
            if (cover <= 0) continue;
            auto mix = [&](int sh) {
                const float top = ((a >> sh) & 0xFF) * wa * (1 - fx) + ((b >> sh) & 0xFF) * wb * fx;
                const float bot = ((c >> sh) & 0xFF) * wc * (1 - fx) + ((d >> sh) & 0xFF) * wd * fx;
                return (u32)std::lround(std::clamp((top * (1 - fy) + bot * fy) / cover, 0.0f, 255.0f));
            };
            L.Over(x, y, 0xFF000000 | mix(16) << 16 | mix(8) << 8 | mix(0), alpha * cover);
        }
    }
}

void DrawText(Layer& L, const UiItem& it, float s, float ox, float oy)
{
    const std::vector<std::string> lines = it.wrap ? Wrap(it.text, it.size, (float)it.to.w, it.weight) : Wrap(it.text, it.size, 1e9f, it.weight);
    const LineMetrics m = Metrics(it.size, it.weight);
    const float lh = std::round(it.size * 1.3f);
    // The first line's baseline: its ascent, less what the font keeps above capitals' accents.
    float base = std::round((lh - (m.ascent + m.descent)) / 2 + m.ascent);
    if (it.middle) base += std::round((it.to.h - lh * lines.size()) / 2);
    DrawTextBox(L, lines, it.size, it.weight, it.align, s, (it.to.x - ox) * s, (it.to.y - oy) * s, (it.to.x + it.to.w - ox) * s,
                (it.to.y + it.to.h - oy) * s, it.colour, it.shadowArgb, lh, base);
}

void Mix(u64& h, u64 v)
{
    for (int i = 0; i < 8; i++) { h ^= (v >> (i * 8)) & 0xFF; h *= 0x100000001B3ull; }
}
u64 FloatBits(float f) { u32 b; memcpy(&b, &f, 4); return b; }
}

UiItem PanelItem(Rect to, const Theme& t)
{
    UiItem it;
    it.kind = UiItem::Panel;
    it.to = to;
    it.fill = t.fill;
    it.borderArgb = t.frame;
    it.radius = t.radius;
    it.border = t.border;
    return it;
}

UiItem WellItem(Rect to, const Theme& t)
{
    UiItem it = PanelItem(to, t);
    it.fill = t.fillDark;
    it.borderArgb = t.frameDark;
    it.radius = t.radius * 0.6f;
    it.border = 0;
    it.raised = false;
    return it;
}

UiItem TextItem(Rect to, const std::string& utf8, float size, Align align, Weight w)
{
    UiItem it;
    it.kind = UiItem::Text;
    it.to = to;
    it.text = utf8;
    it.size = size;
    it.align = align;
    it.weight = w;
    it.colour = GameTheme().text;
    it.shadowArgb = WithAlpha(GameTheme().shadow, 0.9f);
    return it;
}

Rect ChipRect(int x, int y, Button b, const std::string& label, float size, float labelAt, Weight wt)
{
    const float h = ChipHeight(size);
    float w = ChipWidth(b, size);
    if (!label.empty()) w = std::max(w + LabelGap(size), labelAt) + std::ceil(TextWidth(label, size, wt)) + std::max(1.0f, std::round(size / 24));
    return {x, y, (int)std::ceil(w), (int)h};
}

UiItem ChipItem(int x, int y, Button b, const std::string& label, float size, float labelAt, Weight wt)
{
    UiItem it = TextItem(ChipRect(x, y, b, label, size, labelAt, wt), label, size, Align::Left, wt);
    it.labelAt = labelAt;
    it.kind = UiItem::Chip;
    it.button = b;
    it.wrap = false;
    return it;
}

UiItem ImageItem(Rect to, const u32* src, int srcW, int srcH, Rect from)
{
    UiItem it;
    it.kind = UiItem::Image;
    it.to = to;
    it.src = src;
    it.srcW = srcW;
    it.srcH = srcH;
    it.from = from;
    return it;
}

Rect Bounds(const std::vector<UiItem>& items)
{
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool any = false;
    for (const UiItem& it : items)
    {
        if (it.to.w <= 0 || it.to.h <= 0) continue;
        if (it.kind == UiItem::Image && (!it.src || it.from.w <= 0 || it.from.h <= 0 || it.srcW <= 0 || it.srcH <= 0)) continue;
        // Room for the drop shadows (a panel's reaches 3 + 4 canvas pixels down).
        const int m = it.kind == UiItem::Panel ? 8 : it.kind == UiItem::Chip ? 4 : 0;
        const int a = it.to.x - m, b = it.to.y - m, c = it.to.x + it.to.w + m, d = it.to.y + it.to.h + m + 3;
        if (!any) { x0 = a; y0 = b; x1 = c; y1 = d; any = true; }
        else { x0 = std::min(x0, a); y0 = std::min(y0, b); x1 = std::max(x1, c); y1 = std::max(y1, d); }
    }
    return {x0, y0, x1 - x0, y1 - y0};
}

u64 Hash(const std::vector<UiItem>& items)
{
    u64 h = 0xCBF29CE484222325ull;
    for (const UiItem& it : items)
    {
        if (it.kind == UiItem::Image) continue; // drawn apart, every frame
        Mix(h, it.kind);
        Mix(h, ((u64)(u32)it.to.x << 32) | (u32)it.to.y);
        Mix(h, ((u64)(u32)it.to.w << 32) | (u32)it.to.h);
        Mix(h, ((u64)it.fill << 32) | it.borderArgb);
        Mix(h, (FloatBits(it.radius) << 32) | FloatBits(it.border));
        Mix(h, (FloatBits(it.size) << 32) | ((u64)it.raised << 16) | ((u64)it.weight << 8) | (u64)it.align);
        Mix(h, ((u64)it.plain << 56) | ((u64)it.middle << 48) | ((u64)it.wrap << 40) | ((u64)it.button << 32));
        Mix(h, ((u64)it.colour << 32) | it.shadowArgb);
        Mix(h, FloatBits(it.labelAt));
        Mix(h, it.text.size());
        for (char c : it.text) Mix(h, (u8)c);
        if (it.layer != 0) Mix(h, 0x4C41594552ull ^ (u64)(u32)it.layer); // (layer 0 hashes as before)
    }
    return h;
}

u64 HashImage(const UiItem& it)
{
    u64 h = 0xCBF29CE484222325ull;
    Mix(h, ((u64)(u32)it.to.x << 32) | (u32)it.to.y);
    Mix(h, ((u64)(u32)it.to.w << 32) | (u32)it.to.h);
    Mix(h, ((u64)(u32)it.from.x << 32) | (u32)it.from.y);
    Mix(h, ((u64)(u32)it.from.w << 32) | (u32)it.from.h);
    Mix(h, ((u64)(u32)it.srcW << 32) | (u32)it.srcH);
    Mix(h, it.fill);
    if (it.src && it.srcW > 0 && it.srcH > 0)
        for (int y = 0; y < it.from.h; y++)
            for (int x = 0; x < it.from.w; x++)
            {
                const int X = std::clamp(it.from.x + x, 0, it.srcW - 1), Y = std::clamp(it.from.y + y, 0, it.srcH - 1);
                Mix(h, it.src[(size_t)Y * it.srcW + X]);
            }
    return h;
}

void Layers(const std::vector<UiItem>& items, std::vector<UiItem>& still, std::vector<UiItem>& images)
{
    still.clear();
    images.clear();
    for (const UiItem& it : items) (it.kind == UiItem::Image ? images : still).push_back(it);
}

std::vector<LayerAt> LayersAt(const std::vector<UiItem>& items, float scale, float originX, float originY)
{
    std::vector<LayerAt> out;
    if (!(scale > 0)) return out;
    std::vector<UiItem> still, images;
    Layers(items, still, images);
    auto add = [&](std::vector<UiItem> part, bool isStill, int layer) {
        const Rect b = Bounds(part);
        if (b.w <= 0 || b.h <= 0) return;
        const int x0 = (int)std::floor(originX + b.x * scale), y0 = (int)std::floor(originY + b.y * scale);
        const int x1 = (int)std::ceil(originX + (b.x + b.w) * scale), y1 = (int)std::ceil(originY + (b.y + b.h) * scale);
        if (x1 <= x0 || y1 <= y0) return;
        // The layer's top left lands on its whole output pixel (x0, y0), as a piece's does (CanvasFit::ToOutput
        // truncates): its edges are not anti-aliased across two pixels where a piece's are sharp.
        out.push_back({std::move(part), isStill, layer, x0, y0, x1 - x0, y1 - y0, (float)b.x, (float)b.y});
    };
    // The still items by layer, ascending (one layer: exactly as before).
    std::vector<int> layers;
    for (const UiItem& it : still)
        if (std::find(layers.begin(), layers.end(), it.layer) == layers.end()) layers.push_back(it.layer);
    std::sort(layers.begin(), layers.end());
    if (layers.size() <= 1) add(still, true, layers.empty() ? 0 : layers[0]);
    else
        for (int layer : layers)
        {
            std::vector<UiItem> part;
            for (const UiItem& it : still)
                if (it.layer == layer) part.push_back(it);
            add(std::move(part), true, layer);
        }
    for (const UiItem& im : images) add({im}, false, 0);
    return out;
}

void Rasterise(const std::vector<UiItem>& items, float scale, u32* argb, int w, int h, float ox, float oy)
{
    if (w <= 0 || h <= 0 || !argb) return;
    Layer L {w, h, std::vector<float>((size_t)w * h * 4, 0.0f)};
    if (scale > 0)
    {
        // The still items in order, then the images above them (Layers).
        for (const UiItem& it : items)
            switch (it.kind)
            {
            case UiItem::Panel: DrawPanel(L, it, scale, ox, oy); break;
            case UiItem::Text: DrawText(L, it, scale, ox, oy); break;
            case UiItem::Chip: DrawChip(L, it, scale, ox, oy); break;
            case UiItem::Image: break;
            }
        for (const UiItem& it : items)
            if (it.kind == UiItem::Image) DrawImage(L, it, scale, ox, oy);
    }
    // Back to straight alpha.
    for (size_t i = 0; i < (size_t)w * h; i++)
    {
        const float* p = &L.p[i * 4];
        const float a = p[3];
        if (a <= 0) { argb[i] = 0; continue; }
        auto ch = [&](float v) { return (u32)std::lround(std::clamp(v / a, 0.0f, 1.0f) * 255); };
        argb[i] = (u32)std::lround(std::min(a, 1.0f) * 255) << 24 | ch(p[0]) << 16 | ch(p[1]) << 8 | ch(p[2]);
    }
}

std::vector<UiItem> DemoItems(const u32* bottom)
{
    std::vector<UiItem> v;
    // The hints: a column of chips in a panel.
    v.push_back(PanelItem({40, 40, 300, 352}));
    v.push_back(TextItem({64, 58, 252, 40}, "Controls", 30, Align::Left, Weight::ExtraBold));
    const struct { Button b; const char* label; } hints[] = {
        {Button::A, "Talk"}, {Button::B, "Back"}, {Button::X, "Zoom in"}, {Button::Y, "Zoom out"},
        {Button::LB, "Turn left"}, {Button::RB, "Turn right"}, {Button::Start, "Pause"}, {Button::DPad, "Move"},
    };
    int y = 106;
    for (const auto& h : hints)
    {
        v.push_back(ChipItem(64, y, h.b, h.label, 24, 76));
        y += 35;
    }
    // A conversation's box: a name plate, the text in a well, a live picture, the next chip.
    v.push_back(PanelItem({110, 440, 1060, 266}));
    UiItem plate = PanelItem({134, 412, 340, 56});
    plate.fill = GameTheme().frame;
    plate.borderArgb = GameTheme().fill;
    plate.border = 3;
    v.push_back(plate);
    UiItem name = TextItem({134, 412, 340, 56}, "Ana Lucía Méndez", 28, Align::Centre, Weight::ExtraBold);
    name.middle = true;
    v.push_back(name);
    v.push_back(WellItem({140, 482, 808, 176}));
    v.push_back(TextItem({162, 494, 766, 156},
                         "Bienvenue! Crème brûlée, façade, Grüße, año: every accent drawn in HD. A word too long "
                         "for its line breaks safely: Pneumonoultramicroscopicsilicovolcanoconiosis.",
                         30));
    v.push_back(WellItem({968, 482, 176, 176}));
    if (bottom) v.push_back(ImageItem({976, 490, 160, 160}, bottom, SW, SH, {96, 64, 64, 64}));
    UiItem next = ChipItem(0, 0, Button::A, "Next", 24);
    next.to.x = 948 - next.to.w;
    next.to.y = 666;
    v.push_back(next);
    return v;
}
}
