#include "ui/kit.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

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
float PillText(float size) { return size * 0.56f; }

// What a chip's face shows, from its button, face and glyph (UiItem::face / glyph).
enum class Mark { Letter, Pill, Key, Stick, Cross, PsCross, PsCircle, PsSquare, PsTriangle };
struct Resolved
{
    Mark mark = Mark::Pill;
    std::string text;
    u32 colour = 0xFFFFFFFF;
};
const char* DefaultGlyph(Button b)
{
    switch (b)
    {
    case Button::A: return "a";
    case Button::B: return "b";
    case Button::X: return "x";
    case Button::Y: return "y";
    case Button::LB: return "lb";
    case Button::RB: return "rb";
    case Button::LT: return "lt";
    case Button::Start: return "start";
    case Button::Back: return "back";
    case Button::Pause: return "start+back";
    case Button::LStick: return "ls";
    case Button::RStick: return "rs";
    default: return "";
    }
}
Resolved Resolve(Button b, Face face, const std::string& glyph)
{
    Resolved r;
    if (Cross(b)) { r.mark = Mark::Cross; return r; } // the D-pad: drawn alike everywhere (the arrows' cross too)
    if (face == Face::Keyboard && !glyph.empty()) { r.mark = Mark::Key; r.text = glyph; return r; }
    if (face == Face::Keyboard) face = Face::Xbox; // no key: the controller's look
    const std::string g = glyph.empty() ? DefaultGlyph(b) : glyph;
    const bool ps = face == Face::PlayStation, nin = face == Face::Nintendo;
    auto letter = [&](const char* t, u32 xbox, u32 snes) { r.mark = Mark::Letter; r.text = t; r.colour = nin ? snes : xbox; };
    if (g == "a") { if (ps) { r.mark = Mark::PsCross; r.colour = 0xFF7DB4F0; } else letter("A", 0xFF6EDB5E, 0xFFFF6A5E); }
    else if (g == "b") { if (ps) { r.mark = Mark::PsCircle; r.colour = 0xFFFF6A6A; } else letter("B", 0xFFFF6A5E, 0xFFFFD34D); }
    else if (g == "x") { if (ps) { r.mark = Mark::PsSquare; r.colour = 0xFFF09CD2; } else letter("X", 0xFF5CB4FF, 0xFF5CB4FF); }
    else if (g == "y") { if (ps) { r.mark = Mark::PsTriangle; r.colour = 0xFF4FD6B0; } else letter("Y", 0xFFFFD34D, 0xFF6EDB5E); }
    else if (g == "ls" || g == "rs") { r.mark = Mark::Stick; r.text = g == "ls" ? "L" : "R"; }
    else if (g == "up" || g == "down" || g == "left" || g == "right") r.mark = Mark::Cross;
    else
    {
        r.mark = Mark::Pill;
        struct N { const char *key, *xbox, *ps, *nin; };
        static const N names[] = {{"lb", "LB", "L1", "L"}, {"rb", "RB", "R1", "R"}, {"lt", "LT", "L2", "ZL"}, {"rt", "RT", "R2", "ZR"},
                                  {"start", "START", "OPTIONS", "+"}, {"back", "BACK", "SHARE", "-"}, {"create", "BACK", "CREATE", "-"}, {"select", "BACK", "SELECT", "-"},
                                  {"start+back", "START + BACK", "OPTIONS + SHARE", "+ / -"}, {"start+create", "START + BACK", "OPTIONS + CREATE", "+ / -"},
                                  {"start+select", "START + BACK", "START + SELECT", "+ / -"}, {"guide", "GUIDE", "PS", "HOME"}};
        r.text = g;
        for (const N& n : names)
            if (g == n.key) r.text = ps ? n.ps : nin ? n.nin : n.xbox;
    }
    return r;
}
Resolved Resolve(const UiItem& it) { return Resolve(it.button, it.face, it.glyph); }
bool RoundMark(Mark m) { return m != Mark::Pill && m != Mark::Key; }
float KeyText(float size, const std::string& t) { return t.size() <= 2 ? size * 0.62f : PillText(size); }
float ChipWidth(const Resolved& r, float size)
{
    const float h = ChipHeight(size);
    if (RoundMark(r.mark)) return h;
    if (r.mark == Mark::Key) return std::max(std::round(TextWidth(r.text, KeyText(size, r.text), Weight::ExtraBold) + h * 0.6f), h);
    return std::max(std::round(TextWidth(r.text, PillText(size), Weight::ExtraBold) + h * 0.75f), std::round(h * 1.4f));
}
float LabelGap(float size) { return std::round(size * 0.38f); }

// The layer being drawn: premultiplied RGBA floats. w x h is the whole raster (what clips every
// item, as it always did); this pass holds and draws only the pixels x0..x1, y0..y1 of it (a band
// of the rectangle asked for). Every pixel is a function of the items over it alone, so a band
// (or a rectangle drawn again) gives the very bytes the whole raster gives there.
struct Layer
{
    int w, h;
    int x0, y0, x1, y1;
    float* p; // (x1 - x0) x (y1 - y0) x 4
    float* At(int x, int y) { return p + ((size_t)(y - y0) * (x1 - x0) + (x - x0)) * 4; }
    void Over(int x, int y, u32 argb, float cover)
    {
        const float a = ((argb >> 24) / 255.0f) * cover;
        if (a <= 0) return;
        float* d = At(x, y);
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

// A rounded rectangle (layer pixels) and how much of a pixel it covers, anti-aliased; soft > 0
// blurs its edge over that many pixels each side (shadows). `full`: a cover of exactly 1.
struct RoundRect
{
    float x0, y0, x1, y1, r, soft;
    float Cover(int x, int y, bool& full) const
    {
        const float d = RoundRectDistance(x + 0.5f, y + 0.5f, x0, y0, x1, y1, r);
        if (soft > 0)
        {
            const float t = std::clamp((soft - d) / (2 * soft), 0.0f, 1.0f);
            full = t == 1.0f;
            return t * t * (3 - 2 * t);
        }
        const float c = std::clamp(0.5f - d, 0.0f, 1.0f);
        full = c == 1.0f;
        return c;
    }
    bool Full(int x, int y) const { bool f; Cover(x, y, f); return f; }
    // The first pixel of X0..X1 whose centre is right of (or on) the centre.
    int Pivot(int X0, int X1) const
    {
        const float cx = (x0 + x1) / 2;
        int P = std::clamp((int)std::ceil(cx - 0.5f), X0, X1);
        while (P > X0 && (P - 1) + 0.5f - cx >= 0) P--;
        while (P < X1 && P + 0.5f - cx < 0) P++;
        return P;
    }
    // The run of row y (within X0..X1) where it covers pixels fully: `full` depends on the
    // distance alone, which grows with |x + 0.5 - cx| (every step of it monotonic, in floats
    // too), so the full pixels are one run around the centre, found by bisection on each side.
    // [a, b), empty when a == b.
    // (A guess from the shape's geometry first, checked and moved by a pixel or two; bisection
    // when the guess is far off: the run is what Full says, whatever the guess.)
    void FullRun(int y, int X0, int X1, int P, int& a, int& b) const
    {
        a = b = P;
        if (P > X0 && Full(P - 1, y))
        {
            int g = std::clamp(GuessLeft(y), X0, P - 1);
            int steps = 0;
            while (g > X0 && Full(g - 1, y) && ++steps < 4) g--;
            while (!Full(g, y) && ++steps < 4) g++;
            if (Full(g, y) && (g == X0 || !Full(g - 1, y))) a = g;
            else
            {
                int lo = X0, hi = P - 1; // hi full; the first full one in lo..hi
                while (lo < hi) { const int mid = lo + (hi - lo) / 2; if (Full(mid, y)) hi = mid; else lo = mid + 1; }
                a = lo;
            }
        }
        if (P < X1 && Full(P, y))
        {
            int g = std::clamp(GuessRight(y), P, X1 - 1);
            int steps = 0;
            while (g + 1 < X1 && Full(g + 1, y) && ++steps < 4) g++;
            while (!Full(g, y) && ++steps < 4) g--;
            if (Full(g, y) && (g + 1 == X1 || !Full(g + 1, y))) b = g + 1;
            else
            {
                int lo = P, hi = X1 - 1; // lo full; the last full one in lo..hi
                while (lo < hi) { const int mid = lo + (hi - lo + 1) / 2; if (Full(mid, y)) lo = mid; else hi = mid - 1; }
                b = lo + 1;
            }
        }
    }
    // Where the full run's ends lie by the geometry (fully covered: at least `e` inside).
    float HalfRun(int y) const
    {
        const float e = soft > 0 ? soft : 0.5f;
        const float cy = (y0 + y1) / 2, hw = (x1 - x0) / 2, hh = (y1 - y0) / 2;
        const float rr = std::clamp(r, 0.0f, std::min(hw, hh));
        const float qy = std::fabs(y + 0.5f - cy) - (hh - rr);
        if (qy <= 0) return hw - e;
        const float k = rr - e;
        if (k <= qy) return hw - rr; // (none, or a sliver: the check moves it)
        return hw - rr + std::sqrt(k * k - qy * qy);
    }
    int GuessLeft(int y) const { return (int)std::ceil((x0 + x1) / 2 - HalfRun(y) - 0.5f); }
    int GuessRight(int y) const { return (int)std::floor((x0 + x1) / 2 + HalfRun(y) - 0.5f); }
};

// A rounded rectangle in layer pixels, anti-aliased; soft > 0 blurs its edge over that many
// pixels each side (shadows). Its fully covered run of each row is drawn without the distance
// (a cover of 1: the very values the per-pixel path gives; an opaque colour over anything is the
// colour itself). `under`: an opaque rectangle drawn over it later (soft 0, nothing else between):
// the pixels it covers fully are left alone here, since it sets them whatever lies beneath.
void FillRoundRect(Layer& L, float x0, float y0, float x1, float y1, float r, u32 argb, float soft = 0, const RoundRect* under = nullptr)
{
    if (x1 <= x0 || y1 <= y0 || !(argb >> 24)) return;
    const float m = soft + 1;
    // (Clipped to the pass's pixels, which lie inside 0..w, 0..h.)
    const int X0 = std::max((int)std::floor(x0 - m), L.x0), X1 = std::min((int)std::ceil(x1 + m), L.x1);
    const int Y0 = std::max((int)std::floor(y0 - m), L.y0), Y1 = std::min((int)std::ceil(y1 + m), L.y1);
    if (X0 >= X1 || Y0 >= Y1) return;
    const RoundRect shape {x0, y0, x1, y1, r, soft};
    const int P = shape.Pivot(X0, X1), Q = under ? under->Pivot(X0, X1) : 0;
    const bool opaque = (argb >> 24) == 0xFF;
    const float cr = ((argb >> 16) & 0xFF) / 255.0f, cg = ((argb >> 8) & 0xFF) / 255.0f, cb = (argb & 0xFF) / 255.0f;
    for (int y = Y0; y < Y1; y++)
    {
        int a, b;
        shape.FullRun(y, X0, X1, P, a, b);
        auto span = [&](int from, int to) {
            for (int x = from; x < std::min(to, a); x++)
            {
                bool f;
                const float c = shape.Cover(x, y, f);
                if (c > 0) L.Over(x, y, argb, c);
            }
            const int fa = std::max(from, a), fb = std::min(to, b);
            if (opaque)
                for (int x = fa; x < fb; x++)
                {
                    float* d = L.At(x, y);
                    d[0] = cr;
                    d[1] = cg;
                    d[2] = cb;
                    d[3] = 1.0f;
                }
            else
                for (int x = fa; x < fb; x++) L.Over(x, y, argb, 1.0f);
            for (int x = std::max(from, std::max(a, b)); x < to; x++)
            {
                bool f;
                const float c = shape.Cover(x, y, f);
                if (c > 0) L.Over(x, y, argb, c);
            }
        };
        int oa = X0, ob = X0;
        if (under) under->FullRun(y, X0, X1, Q, oa, ob);
        if (oa < ob)
        {
            span(X0, oa);
            span(ob, X1);
        }
        else span(X0, X1);
    }
}

u32 WithAlpha(u32 argb, float a) { return (argb & 0xFFFFFF) | ((u32)std::lround(std::clamp(a, 0.0f, 1.0f) * (argb >> 24)) << 24); }

// A box blur of radius r (each way), in place: each pixel the rounded mean of the 2r + 1 around it
// (those inside the mask), across then down, by running sums (whole numbers: exact).
void Blur(std::vector<u8>& m, int w, int h, int r)
{
    if (r <= 0) return;
    std::vector<u8> t(m.size());
    const int n = 2 * r + 1;
    for (int y = 0; y < h; y++)
    {
        const u8* row = &m[(size_t)y * w];
        u8* out = &t[(size_t)y * w];
        int sum = 0;
        for (int X = 0; X <= r && X < w; X++) sum += row[X];
        for (int x = 0; x < w; x++)
        {
            out[x] = (u8)((sum + n / 2) / n);
            if (x + 1 + r < w) sum += row[x + 1 + r];
            if (x - r >= 0) sum -= row[x - r];
        }
    }
    std::vector<int> col((size_t)w, 0);
    for (int Y = 0; Y <= r && Y < h; Y++)
        for (int x = 0; x < w; x++) col[x] += t[(size_t)Y * w + x];
    for (int y = 0; y < h; y++)
    {
        u8* out = &m[(size_t)y * w];
        for (int x = 0; x < w; x++) out[x] = (u8)((col[x] + n / 2) / n);
        if (y + 1 + r < h)
        {
            const u8* add = &t[(size_t)(y + 1 + r) * w];
            for (int x = 0; x < w; x++) col[x] += add[x];
        }
        if (y - r >= 0)
        {
            const u8* sub = &t[(size_t)(y - r) * w];
            for (int x = 0; x < w; x++) col[x] -= sub[x];
        }
    }
}

// A text box's coverage (DrawTextBox's, as it always drew it): the lines' mask and their
// shadow's over the box clipped to the raster (X0, Y0, mw x mh). Made with the font on one thread
// (the font's caches are not shared) before the bands draw, kept by everything that makes it (the
// same key: the same bytes), so the window's labels are drawn once and every band reads them.
struct TextMask
{
    int mw = 0, mh = 0;
    std::vector<u8> text, shadow; // shadow empty: none
};
struct PlacedMask
{
    int X0 = 0, Y0 = 0;
    std::shared_ptr<const TextMask> m;
};

class MaskCache
{
    static constexpr size_t kMaxBytes = 96u << 20;
    std::mutex mu;
    std::unordered_map<std::string, std::shared_ptr<const TextMask>> map;
    std::deque<std::string> order; // oldest first: dropped past kMaxBytes
    size_t bytes = 0;

public:
    std::shared_ptr<const TextMask> Find(const std::string& key)
    {
        std::lock_guard<std::mutex> g(mu);
        const auto it = map.find(key);
        return it == map.end() ? nullptr : it->second;
    }
    void Clear()
    {
        std::lock_guard<std::mutex> g(mu);
        map.clear();
        order.clear();
        bytes = 0;
    }
    void Put(const std::string& key, std::shared_ptr<const TextMask> m)
    {
        std::lock_guard<std::mutex> g(mu);
        if (map.count(key)) return;
        bytes += m->text.size() + m->shadow.size();
        map.emplace(key, std::move(m));
        order.push_back(key);
        while (bytes > kMaxBytes && order.size() > 1)
        {
            const auto it = map.find(order.front());
            bytes -= it->second->text.size() + it->second->shadow.size();
            map.erase(it);
            order.pop_front();
        }
    }
};
MaskCache& Masks()
{
    static MaskCache* cache = new MaskCache; // (never destroyed: no order to keep at exit)
    return *cache;
}
template <class T> void KeyAdd(std::string& k, const T& v) { k.append((const char*)&v, sizeof v); }

// Text lines in a box of a raster of W x H (bx0..bx1, by0..by1, its pixels, clipped to it): the
// mask, and the shadow's when `shadow`. size: canvas pixels per em; s: pixels per canvas pixel.
PlacedMask MakeTextMask(int W, int H, const std::vector<std::string>& lines, float size, Weight w, Align align, float s, float bx0,
                        float by0, float bx1, float by1, bool shadow, float lineHeight, float firstBaseline)
{
    PlacedMask out;
    const int X0 = std::max((int)std::floor(bx0), 0), X1 = std::min((int)std::ceil(bx1), W);
    const int Y0 = std::max((int)std::floor(by0), 0), Y1 = std::min((int)std::ceil(by1), H);
    const int mw = X1 - X0, mh = Y1 - Y0;
    if (mw <= 0 || mh <= 0 || lines.empty()) return out;
    out.X0 = X0;
    out.Y0 = Y0;
    // Each line's pen and baseline in mask pixels: with the lines, the size, the weight and the
    // mask's size, all that makes the mask.
    std::vector<float> pens(lines.size()), bases(lines.size());
    for (size_t i = 0; i < lines.size(); i++)
    {
        const float lw = TextWidth(lines[i], size, w) * s;
        float x = bx0;
        if (align == Align::Centre) x = (bx0 + bx1 - lw) / 2;
        else if (align == Align::Right) x = bx1 - lw;
        const float base = by0 + (firstBaseline + i * lineHeight) * s;
        pens[i] = x - X0;
        bases[i] = base - Y0;
    }
    const float px = size * s;
    // One canvas pixel down and right, softened in proportion when scaled up.
    const int off = std::max(1, (int)std::lround(s)), blur = (int)std::floor(s * 0.6f);
    std::string key;
    KeyAdd(key, mw);
    KeyAdd(key, mh);
    KeyAdd(key, (int)w);
    KeyAdd(key, px);
    KeyAdd(key, shadow ? off : -1);
    KeyAdd(key, shadow ? blur : -1);
    for (size_t i = 0; i < lines.size(); i++)
    {
        KeyAdd(key, pens[i]);
        KeyAdd(key, bases[i]);
        KeyAdd(key, lines[i].size());
        key += lines[i];
    }
    if ((out.m = Masks().Find(key))) return out;
    auto m = std::make_shared<TextMask>();
    m->mw = mw;
    m->mh = mh;
    m->text.assign((size_t)mw * mh, 0);
    for (size_t i = 0; i < lines.size(); i++) DrawLine(lines[i], size * s, w, pens[i], bases[i], m->text.data(), mw, mh);
    if (shadow)
    {
        m->shadow.assign((size_t)mw * mh, 0);
        if (off < mw)
            for (int y = off; y < mh; y++) memcpy(&m->shadow[(size_t)y * mw + off], &m->text[(size_t)(y - off) * mw], (size_t)(mw - off));
        Blur(m->shadow, mw, mh, blur);
    }
    Masks().Put(key, m);
    out.m = std::move(m);
    return out;
}

// A text mask drawn into the pass's pixels: the shadow first (when it has one and `shadow` shows),
// then the text.
void DrawMask(Layer& L, const PlacedMask& pm, u32 colour, u32 shadow)
{
    if (!pm.m) return;
    const TextMask& m = *pm.m;
    const int xa = std::max(pm.X0, L.x0), xb = std::min(pm.X0 + m.mw, L.x1);
    const int ya = std::max(pm.Y0, L.y0), yb = std::min(pm.Y0 + m.mh, L.y1);
    if (xa >= xb || ya >= yb) return;
    if ((shadow >> 24) && !m.shadow.empty())
        for (int y = ya; y < yb; y++)
            for (int x = xa; x < xb; x++)
                if (const u8 c = m.shadow[(size_t)(y - pm.Y0) * m.mw + (x - pm.X0)]) L.Over(x, y, shadow, c / 255.0f);
    for (int y = ya; y < yb; y++)
        for (int x = xa; x < xb; x++)
            if (const u8 c = m.text[(size_t)(y - pm.Y0) * m.mw + (x - pm.X0)]) L.Over(x, y, colour, c / 255.0f);
}

// What an item needs of the font, made before the bands draw: a chip's width, its glyph's and
// its label's masks, a text's mask.
struct Prepared
{
    float chipW = 0;
    PlacedMask glyph, label;
};
bool PsMarkOf(Mark m) { return m == Mark::PsCross || m == Mark::PsCircle || m == Mark::PsSquare || m == Mark::PsTriangle; }

Prepared Prepare(const UiItem& it, float s, float ox, float oy, int W, int H)
{
    Prepared p;
    if (it.kind == UiItem::Text)
    {
        const std::vector<std::string> lines = it.wrap ? Wrap(it.text, it.size, (float)it.to.w, it.weight) : Wrap(it.text, it.size, 1e9f, it.weight);
        const LineMetrics m = Metrics(it.size, it.weight);
        const float lh = std::round(it.size * 1.3f);
        // The first line's baseline: its ascent, less what the font keeps above capitals' accents.
        float base = std::round((lh - (m.ascent + m.descent)) / 2 + m.ascent);
        if (it.middle) base += std::round((it.to.h - lh * lines.size()) / 2);
        p.label = MakeTextMask(W, H, lines, it.size, it.weight, it.align, s, (it.to.x - ox) * s, (it.to.y - oy) * s, (it.to.x + it.to.w - ox) * s,
                               (it.to.y + it.to.h - oy) * s, (it.shadowArgb >> 24) != 0, lh, base);
    }
    else if (it.kind == UiItem::Chip)
    {
        const Resolved rv = Resolve(it);
        const float size = it.size, h = ChipHeight(size), cw = ChipWidth(rv, size);
        p.chipW = cw;
        const float shift = it.labelAt > 0 ? std::max(0.0f, std::round((it.labelAt - LabelGap(size) - cw) / 2)) : 0;
        const float x0 = (it.to.x + shift - ox) * s, y0 = (it.to.y - oy) * s + (it.to.h - h) / 2 * s;
        const float x1 = x0 + cw * s, y1 = y0 + h * s;
        if (rv.mark != Mark::Cross && !PsMarkOf(rv.mark))
        {
            const float ts = rv.mark == Mark::Stick ? size * 0.6f : rv.mark == Mark::Letter ? size * 0.82f
                           : rv.mark == Mark::Key   ? KeyText(size, rv.text) : PillText(size);
            const LineMetrics m = Metrics(ts, Weight::ExtraBold);
            // Centred on the cap height (about 0.7 em).
            const float base = (h + ts * 0.70f) / 2;
            p.glyph = MakeTextMask(W, H, {rv.text}, ts, Weight::ExtraBold, Align::Centre, s, x0, y0, x1, y1, false, m.lineHeight, base);
        }
        if (!it.text.empty())
        {
            const float lx = (it.to.x - ox + std::max(cw + LabelGap(size), it.labelAt)) * s;
            const LineMetrics m = Metrics(size, it.weight);
            const float base = (it.to.h + size * 0.70f) / 2;
            p.label = MakeTextMask(W, H, {it.text}, size, it.weight, Align::Left, s, lx, (it.to.y - oy) * s, (it.to.x + it.to.w - ox) * s,
                                   (it.to.y + it.to.h - oy) * s, (it.shadowArgb >> 24) != 0, m.lineHeight, base);
        }
    }
    return p;
}

void DrawPanel(Layer& L, const UiItem& it, float s, float ox, float oy)
{
    const Theme& t = GameTheme();
    const float x0 = (it.to.x - ox) * s, y0 = (it.to.y - oy) * s, x1 = x0 + it.to.w * s, y1 = y0 + it.to.h * s;
    const float r = it.radius * s, line = std::max(1.0f, std::round(s));
    if (it.plain) { FillRoundRect(L, x0, y0, x1, y1, r, it.fill); return; }
    // The fill, drawn last: where it is opaque and covers a pixel fully, what lies beneath it there
    // is never seen, so the shapes before it leave those pixels alone (the same bytes, sooner).
    float in = line;
    if (it.border > 0)
    {
        in += it.border * s; // (the very steps below: the same floats)
        in += line * 0.75f;
    }
    const float fillRi = std::max(r - in, 3 * s);
    const RoundRect fillShape {x0 + in, y0 + in + line * (it.raised ? 1 : 2), x1 - in, y1 - in, fillRi, 0};
    const RoundRect* fill = (it.fill >> 24) == 0xFF ? &fillShape : nullptr;
    in = line;
    if (it.raised) FillRoundRect(L, x0, y0 + 3 * s, x1, y1 + 3 * s, r, 0x66000000, 4 * s, fill);
    // The dark outline, the frame's band, then the fill.
    FillRoundRect(L, x0, y0, x1, y1, r, t.shadow, 0, fill);
    if (it.border > 0)
    {
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, r - in, it.borderArgb, 0, fill);
        in += it.border * s;
        // The inner edge of the band, as the game draws it: a thin dark line.
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, std::max(r - in, 3 * s), WithAlpha(t.shadow, 0.55f), 0, fill);
        in += line * 0.75f;
    }
    const float ri = std::max(r - in, 3 * s);
    if (it.raised)
    {
        // A light bevel along the fill's top edge (the panel stands out)…
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, ri, 0x40FFFFFF, 0, fill);
        FillRoundRect(L, x0 + in, y0 + in + line, x1 - in, y1 - in, ri, it.fill);
    }
    else
    {
        // …or a dark one (a well sunk into its panel).
        FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, ri, WithAlpha(t.shadow, 0.7f), 0, fill);
        FillRoundRect(L, x0 + in, y0 + in + line * 2, x1 - in, y1 - in, ri, it.fill);
    }
}

// A shape given by its signed distance (layer pixels, negative inside), anti-aliased over the box.
template <class F> void FillDistance(Layer& L, float x0, float y0, float x1, float y1, u32 argb, F dist)
{
    const int X0 = std::max((int)std::floor(x0) - 1, L.x0), X1 = std::min((int)std::ceil(x1) + 1, L.x1);
    const int Y0 = std::max((int)std::floor(y0) - 1, L.y0), Y1 = std::min((int)std::ceil(y1) + 1, L.y1);
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++)
        {
            const float c = std::clamp(0.5f - dist(x + 0.5f, y + 0.5f), 0.0f, 1.0f);
            if (c > 0) L.Over(x, y, argb, c);
        }
}
float SegmentDistance(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    const float t = std::clamp((wx * vx + wy * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
    const float dx = wx - t * vx, dy = wy - t * vy;
    return std::sqrt(dx * dx + dy * dy);
}

// PlayStation's face symbols, outlined, centred at (cx, cy) in a chip of height hs (layer pixels).
void DrawPsMark(Layer& L, Mark m, float cx, float cy, float hs, u32 c)
{
    const float R = hs * 0.25f, w = std::max(1.0f, hs * 0.075f) / 2;
    switch (m)
    {
    case Mark::PsCircle:
        FillDistance(L, cx - R - w, cy - R - w, cx + R + w, cy + R + w, c,
                     [&](float x, float y) { return std::fabs(std::hypot(x - cx, y - cy) - R) - w; });
        break;
    case Mark::PsSquare:
    {
        const float q = R * 0.82f;
        FillDistance(L, cx - q - w, cy - q - w, cx + q + w, cy + q + w, c,
                     [&](float x, float y) { return std::fabs(RoundRectDistance(x, y, cx - q, cy - q, cx + q, cy + q, w)) - w; });
        break;
    }
    case Mark::PsTriangle:
    {
        const float tx = cx, ty = cy - R, lx = cx - R * 0.95f, ly = cy + R * 0.62f, rx = cx + R * 0.95f, ry = ly;
        FillDistance(L, lx - w, ty - w, rx + w, ly + w, c, [&](float x, float y) {
            return std::min({SegmentDistance(x, y, tx, ty, lx, ly), SegmentDistance(x, y, lx, ly, rx, ry), SegmentDistance(x, y, rx, ry, tx, ty)}) - w;
        });
        break;
    }
    default: // the cross
    {
        const float q = R * 0.8f;
        FillDistance(L, cx - q - w, cy - q - w, cx + q + w, cy + q + w, c, [&](float x, float y) {
            return std::min(SegmentDistance(x, y, cx - q, cy - q, cx + q, cy + q), SegmentDistance(x, y, cx - q, cy + q, cx + q, cy - q)) - w;
        });
        break;
    }
    }
}

void DrawChip(Layer& L, const UiItem& it, float s, float ox, float oy, const Prepared& p)
{
    const Resolved rv = Resolve(it);
    const float size = it.size, h = ChipHeight(size), cw = p.chipW;
    // In a column (labelAt), the chip is centred in the room before its label.
    const float shift = it.labelAt > 0 ? std::max(0.0f, std::round((it.labelAt - LabelGap(size) - cw) / 2)) : 0;
    const float x0 = (it.to.x + shift - ox) * s, y0 = (it.to.y - oy) * s + (it.to.h - h) / 2 * s;
    // A key is a keycap: a rounded square rather than a pill.
    const float x1 = x0 + cw * s, y1 = y0 + h * s, r = (rv.mark == Mark::Key ? h * 0.24f : h / 2) * s, line = std::max(1.0f, std::round(s * 1.25f));
    FillRoundRect(L, x0, y0 + 1.5f * s, x1, y1 + 1.5f * s, r, 0x59000000, 1.5f * s);
    FillRoundRect(L, x0, y0, x1, y1, r, 0xFFE4E7FA);                                // the light rim
    FillRoundRect(L, x0 + line, y0 + line, x1 - line, y1 - line, r - line, 0xFF2E3063); // the dark face
    // A soft sheen on the face's upper half.
    FillRoundRect(L, x0 + line * 2, y0 + line * 1.5f, x1 - line * 2, (y0 + y1) / 2, std::max(0.0f, r - line * 2), 0x1FFFFFFF);
    // The D-pad's look: the button's own, or the direction a glyph names.
    Button d = it.button;
    if (!Cross(d)) d = it.glyph == "up" ? Button::DPadU : it.glyph == "down" ? Button::DPadD : it.glyph == "left" ? Button::DPadL : Button::DPadR;
    if (rv.mark == Mark::Cross && (d == Button::DPad || d == Button::DPadV || d == Button::DPadH))
    {
        // A cross: two rounded bars (one faint when only the other's directions do something).
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, a = h * 0.29f * s, b = h * 0.095f * s;
        FillRoundRect(L, cx - a, cy - b, cx + a, cy + b, b * 0.5f, d == Button::DPadV ? 0x59FFFFFF : 0xFFFFFFFF);
        FillRoundRect(L, cx - b, cy - a, cx + b, cy + a, b * 0.5f, d == Button::DPadH ? 0x59FFFFFF : 0xFFFFFFFF);
    }
    else if (rv.mark == Mark::Cross)
    {
        // The cross faint, its one arm lit (from the centre out).
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, a = h * 0.29f * s, b = h * 0.095f * s;
        FillRoundRect(L, cx - a, cy - b, cx + a, cy + b, b * 0.5f, 0x38FFFFFF);
        FillRoundRect(L, cx - b, cy - a, cx + b, cy + a, b * 0.5f, 0x38FFFFFF);
        // The lit arm a little wider than the cross, from its centre out.
        const float w = b * 1.45f;
        const float l = d == Button::DPadL ? cx - a : d == Button::DPadR ? cx - w * 0.5f : cx - w;
        const float r = d == Button::DPadR ? cx + a : d == Button::DPadL ? cx + w * 0.5f : cx + w;
        const float t = d == Button::DPadU ? cy - a : d == Button::DPadD ? cy - w * 0.5f : cy - w;
        const float u = d == Button::DPadD ? cy + a : d == Button::DPadU ? cy + w * 0.5f : cy + w;
        FillRoundRect(L, l, t, r, u, w * 0.5f, 0xFFFFFFFF);
    }
    else if (rv.mark == Mark::PsCross || rv.mark == Mark::PsCircle || rv.mark == Mark::PsSquare || rv.mark == Mark::PsTriangle)
        DrawPsMark(L, rv.mark, (x0 + x1) / 2, (y0 + y1) / 2, h * s, rv.colour);
    else
    {
        // The face buttons in their face's colours; the others white.
        if (rv.mark == Mark::Stick)
        {
            // A stick: a light ring inside the face, the letter smaller.
            const float in = (y1 - y0) * 0.16f;
            FillRoundRect(L, x0 + in, y0 + in, x1 - in, y1 - in, r - in, 0xB3E4E7FA);
            FillRoundRect(L, x0 + in + line, y0 + in + line, x1 - in - line, y1 - in - line, r - in - line, 0xFF2E3063);
        }
        DrawMask(L, p.glyph, rv.colour, 0); // (Prepare: the letter centred on the cap height)
    }
    DrawMask(L, p.label, it.colour, it.shadowArgb);
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
    const int X0 = std::max((int)std::lround(x0), L.x0), X1 = std::min((int)std::lround(x0 + tw), L.x1);
    const int Y0 = std::max((int)std::lround(y0), L.y0), Y1 = std::min((int)std::lround(y0 + th), L.y1);
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

void DrawText(Layer& L, const UiItem& it, const Prepared& p) { DrawMask(L, p.label, it.colour, it.shadowArgb); }

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

namespace
{
ChipLook gLook;
// The current look's face and glyph for a button; the Xbox look of a button's own input is
// stored as none, so the items (and their hash) are exactly what they always were.
void LookOf(Button b, Face& face, std::string& glyph)
{
    face = gLook.face;
    glyph = gLook.glyph[(int)b];
    if (face == Face::Xbox && glyph == DefaultGlyph(b)) glyph.clear();
}
}

void SetChipLook(const ChipLook& look) { gLook = look; }

Rect ChipRect(int x, int y, Button b, const std::string& label, float size, float labelAt, Weight wt)
{
    const float h = ChipHeight(size);
    Face face;
    std::string glyph;
    LookOf(b, face, glyph);
    float w = ChipWidth(Resolve(b, face, glyph), size);
    if (!label.empty()) w = std::max(w + LabelGap(size), labelAt) + std::ceil(TextWidth(label, size, wt)) + std::max(1.0f, std::round(size / 24));
    return {x, y, (int)std::ceil(w), (int)h};
}

UiItem ChipItemAs(int x, int y, Face face, const std::string& glyph, const std::string& label, float size, Weight wt)
{
    // (Button A: its glyph decides what is drawn; the D-pad's names draw the cross.)
    const Button b = glyph == "up" || glyph == "down" || glyph == "left" || glyph == "right" ? Button::DPad : Button::A;
    const float h = ChipHeight(size);
    float w = ChipWidth(Resolve(b, face, glyph), size);
    if (!label.empty()) w = w + LabelGap(size) + std::ceil(TextWidth(label, size, wt)) + std::max(1.0f, std::round(size / 24));
    UiItem it = TextItem({x, y, (int)std::ceil(w), (int)h}, label, size, Align::Left, wt);
    it.kind = UiItem::Chip;
    it.button = b;
    it.face = face;
    it.glyph = glyph;
    if (b == Button::DPad) it.button = glyph == "up" ? Button::DPadU : glyph == "down" ? Button::DPadD : glyph == "left" ? Button::DPadL : Button::DPadR;
    it.wrap = false;
    return it;
}

UiItem ChipItem(int x, int y, Button b, const std::string& label, float size, float labelAt, Weight wt)
{
    UiItem it = TextItem(ChipRect(x, y, b, label, size, labelAt, wt), label, size, Align::Left, wt);
    it.labelAt = labelAt;
    it.kind = UiItem::Chip;
    it.button = b;
    LookOf(b, it.face, it.glyph);
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

namespace
{
// One still item into the hash (Hash's, item by item).
void MixItem(u64& h, const UiItem& it)
{
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
    if (it.face != Face::Xbox || !it.glyph.empty())
    {   // (a chip of the Xbox look hashes as before)
        Mix(h, 0x46414345ull ^ (u64)it.face);
        for (char c : it.glyph) Mix(h, (u8)c);
    }
}
}

u64 Hash(const std::vector<UiItem>& items)
{
    u64 h = 0xCBF29CE484222325ull;
    for (const UiItem& it : items)
        if (it.kind != UiItem::Image) MixItem(h, it); // (pictures: drawn apart, every frame)
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

namespace
{
// The raster's helpers: a few threads drawing bands of a raster side by side. Each band is drawn
// from the items alone (every pixel a function of the items over it), so the bands give the very
// bytes one thread gives, in any order, on any number of threads.
class Pool
{
public:
    Pool()
    {
        const unsigned hc = std::thread::hardware_concurrency();
        const int n = std::clamp((int)hc - 1, 0, 7);
        for (int i = 0; i < n; i++) threads.emplace_back([this] { Work(); });
    }
    int Size() const { return (int)threads.size() + 1; }
    // f(0) .. f(n - 1), the caller taking its share; returns when all are done.
    void Run(int n, const std::function<void(int)>& f, bool alone)
    {
        std::lock_guard<std::mutex> one(runMu);
        if (n <= 1 || threads.empty() || alone)
        {
            for (int i = 0; i < n; i++) f(i);
            return;
        }
        {
            std::lock_guard<std::mutex> g(mu);
            job = &f;
            count = n;
            next.store(0);
            busy = (int)threads.size();
            gen++;
        }
        wake.notify_all();
        for (int i; (i = next.fetch_add(1)) < n;) f(i);
        std::unique_lock<std::mutex> g(mu);
        done.wait(g, [&] { return busy == 0; });
        job = nullptr;
    }

private:
    void Work()
    {
        u64 seen = 0;
        for (;;)
        {
            const std::function<void(int)>* f;
            int n;
            {
                std::unique_lock<std::mutex> g(mu);
                wake.wait(g, [&] { return gen != seen; });
                seen = gen;
                f = job;
                n = count;
            }
            for (int i; (i = next.fetch_add(1)) < n;) (*f)(i);
            std::lock_guard<std::mutex> g(mu);
            if (--busy == 0) done.notify_one();
        }
    }
    std::vector<std::thread> threads;
    std::mutex runMu, mu;
    std::condition_variable wake, done;
    const std::function<void(int)>* job = nullptr;
    int count = 0, busy = 0;
    std::atomic<int> next {0};
    u64 gen = 0;
};
Pool& Threads()
{
    static Pool* pool = new Pool; // (never destroyed: its threads sleep until the program ends)
    return *pool;
}
std::atomic<int> gRasterThreads {0};

// An item's pixels on a raster (layer pixels, before the raster's own clip): what it may draw,
// with room to spare: a panel's shadow (4 canvas pixels around, 3 + 4 below), a chip's (1.5 + 1.5)
// and the chip itself, which a column's labelAt moves right of `to` (by half of it at most) and
// which keeps its own height whatever `to`'s; a text inside its box; a picture inside `to`.
// A still item with no size: everything.
Rect Footprint(const UiItem& it, float s, float ox, float oy, int w, int h)
{
    if (it.to.w <= 0 || it.to.h <= 0) return {0, 0, w, h};
    float l = 2, r = 2, t = 2, b = 2; // canvas pixels around `to`
    if (it.kind == UiItem::Panel) l = r = t = b = 12;
    else if (it.kind == UiItem::Chip)
    {
        const float ch = ChipHeight(it.size);
        l = 8;
        r = 8 + std::max(it.labelAt, 0.0f);
        t = b = 8 + ch;
    }
    const int x0 = std::max((int)std::floor((it.to.x - l - ox) * s) - 2, 0), x1 = std::min((int)std::ceil((it.to.x + it.to.w + r - ox) * s) + 2, w);
    const int y0 = std::max((int)std::floor((it.to.y - t - oy) * s) - 2, 0), y1 = std::min((int)std::ceil((it.to.y + it.to.h + b + 3 - oy) * s) + 2, h);
    return {x0, y0, std::max(x1 - x0, 0), std::max(y1 - y0, 0)};
}
bool Meets(const Rect& a, const Rect& b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
// Whether two rectangles that meet are drawn as one: their bounding box adds little beside them
// (else each alone, what they share drawn twice: the same bytes).
bool Merged(const Rect& a, const Rect& b)
{
    const long long x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y), x1 = std::max(a.x + a.w, b.x + b.w), y1 = std::max(a.y + a.h, b.y + b.h);
    return (x1 - x0) * (y1 - y0) * 4 <= ((long long)a.w * a.h + (long long)b.w * b.h) * 5;
}
}

void SetRasterThreads(int n) { gRasterThreads.store(std::max(n, 0)); }
void ClearTextMasks() { Masks().Clear(); }

void Rasterise(const std::vector<UiItem>& items, float scale, u32* argb, int w, int h, float ox, float oy, Rect clip)
{
    const int cx0 = std::max(clip.x, 0), cy0 = std::max(clip.y, 0);
    const int cx1 = std::min(clip.x + clip.w, w), cy1 = std::min(clip.y + clip.h, h);
    if (cx0 >= cx1 || cy0 >= cy1 || !argb) return;
    // The font's part first, on this thread (Prepare): the texts over the rectangle, the chips.
    // And each item's pixels: a pixel no item reaches is transparent, no need to draw it.
    std::vector<Prepared> prep(items.size());
    std::vector<Rect> foot(items.size());
    if (scale > 0)
        for (size_t i = 0; i < items.size(); i++)
        {
            const UiItem& it = items[i];
            foot[i] = Footprint(it, scale, ox, oy, w, h);
            if (it.kind == UiItem::Chip || (it.kind == UiItem::Text && Meets(foot[i], {cx0, cy0, cx1 - cx0, cy1 - cy0})))
                prep[i] = Prepare(it, scale, ox, oy, w, h);
        }
    // Then the bands, side by side.
    Pool& pool = Threads();
    const int limit = gRasterThreads.load(), threads = limit > 0 ? std::min(limit, pool.Size()) : pool.Size();
    const int cw = cx1 - cx0, rows = cy1 - cy0;
    int band = threads > 1 ? std::max(16, (rows + threads * 4 - 1) / (threads * 4)) : rows;
    band = std::max(1, std::min(band, std::max(16, (int)((8u << 20) / (16u * (unsigned)cw))))); // (8 MB of floats a band at most)
    const int bands = (rows + band - 1) / band;
    pool.Run(bands, [&](int b) {
        thread_local std::vector<float> buf;
        thread_local std::vector<int> spanLo, spanHi;
        const int y0 = cy0 + b * band, y1 = std::min(y0 + band, cy1);
        if (buf.size() < (size_t)cw * (y1 - y0) * 4) buf.resize((size_t)cw * (y1 - y0) * 4);
        Layer L {w, h, cx0, y0, cx1, y1, buf.data()};
        // Each row's span the items reach (outside it: transparent, never drawn into).
        spanLo.assign((size_t)(y1 - y0), cx1);
        spanHi.assign((size_t)(y1 - y0), cx0);
        for (const Rect& f : foot)
        {
            const int a = std::max(f.x, cx0), z = std::min(f.x + f.w, cx1);
            if (a >= z) continue;
            for (int y = std::max(f.y, y0); y < std::min(f.y + f.h, y1); y++)
            {
                spanLo[y - y0] = std::min(spanLo[y - y0], a);
                spanHi[y - y0] = std::max(spanHi[y - y0], z);
            }
        }
        for (int y = y0; y < y1; y++)
            if (spanLo[y - y0] < spanHi[y - y0]) std::fill(L.At(spanLo[y - y0], y), L.At(spanHi[y - y0] - 1, y) + 4, 0.0f);
        if (scale > 0)
        {
            // The still items in order, then the images above them (Layers).
            for (size_t i = 0; i < items.size(); i++)
                switch (items[i].kind)
                {
                case UiItem::Panel: DrawPanel(L, items[i], scale, ox, oy); break;
                case UiItem::Text: DrawText(L, items[i], prep[i]); break;
                case UiItem::Chip: DrawChip(L, items[i], scale, ox, oy, prep[i]); break;
                case UiItem::Image: break;
                }
            for (const UiItem& it : items)
                if (it.kind == UiItem::Image) DrawImage(L, it, scale, ox, oy);
        }
        // Back to straight alpha.
        // (A pixel equal to the one before it gives what that one gave: a fill's run converts once.)
        for (int y = y0; y < y1; y++)
        {
            const float* row = L.At(cx0, y);
            u32* out = argb + (size_t)y * w;
            const int lo = spanLo[y - y0], hi = std::max(spanHi[y - y0], lo);
            std::fill(out + cx0, out + std::min(lo, cx1), 0u);
            std::fill(out + std::max(hi, cx0), out + cx1, 0u);
            float last[4] = {-1, -1, -1, -1};
            u32 lastOut = 0;
            for (int x = lo; x < hi; x++)
            {
                const float* p = row + (size_t)(x - cx0) * 4;
                const float a = p[3];
                if (a <= 0) { out[x] = 0; continue; }
                if (p[0] == last[0] && p[1] == last[1] && p[2] == last[2] && a == last[3]) { out[x] = lastOut; continue; }
                auto ch = [&](float v) { return (u32)std::lround(std::clamp(v / a, 0.0f, 1.0f) * 255); };
                out[x] = lastOut = (u32)std::lround(std::min(a, 1.0f) * 255) << 24 | ch(p[0]) << 16 | ch(p[1]) << 8 | ch(p[2]);
                memcpy(last, p, sizeof last);
            }
        }
    }, threads <= 1);
}

void Rasterise(const std::vector<UiItem>& items, float scale, u32* argb, int w, int h, float ox, float oy)
{
    Rasterise(items, scale, argb, w, h, ox, oy, Rect {0, 0, w, h});
}

namespace
{
u64 HashItem(const UiItem& it)
{
    u64 h = 0xCBF29CE484222325ull;
    if (it.kind == UiItem::Image) Mix(h, HashImage(it));
    else MixItem(h, it);
    return h;
}

// The pixels a change of items draws again: the items one list has and the other has not, in
// order (the longest run they share, item by item, is kept: a pixel no other item touches is
// drawn by the same items in the same order in both, so it stays as it was).
std::vector<Rect> Changed(const std::vector<u64>& was, const std::vector<Rect>& wasBox, const std::vector<u64>& now, const std::vector<Rect>& nowBox)
{
    size_t lo = 0;
    while (lo < was.size() && lo < now.size() && was[lo] == now[lo]) lo++;
    size_t ea = was.size(), eb = now.size();
    while (ea > lo && eb > lo && was[ea - 1] == now[eb - 1]) { ea--; eb--; }
    const size_t n = ea - lo, m = eb - lo;
    std::vector<bool> keptA(n, false), keptB(m, false);
    if (n && m)
    {
        // The longest common subsequence of what is left.
        std::vector<u16> t((n + 1) * (m + 1), 0);
        auto T = [&](size_t i, size_t j) -> u16& { return t[i * (m + 1) + j]; };
        for (size_t i = n; i-- > 0;)
            for (size_t j = m; j-- > 0;)
                T(i, j) = was[lo + i] == now[lo + j] ? (u16)(T(i + 1, j + 1) + 1) : std::max(T(i + 1, j), T(i, j + 1));
        for (size_t i = 0, j = 0; i < n && j < m;)
        {
            if (was[lo + i] == now[lo + j]) { keptA[i++] = true; keptB[j++] = true; }
            else if (T(i + 1, j) >= T(i, j + 1)) i++;
            else j++;
        }
    }
    std::vector<Rect> out;
    for (size_t i = 0; i < n; i++)
        if (!keptA[i] && wasBox[lo + i].w > 0 && wasBox[lo + i].h > 0) out.push_back(wasBox[lo + i]);
    for (size_t j = 0; j < m; j++)
        if (!keptB[j] && nowBox[lo + j].w > 0 && nowBox[lo + j].h > 0) out.push_back(nowBox[lo + j]);
    // Rectangles that meet become one.
    for (bool merged = true; merged;)
    {
        merged = false;
        for (size_t i = 0; i < out.size() && !merged; i++)
            for (size_t j = i + 1; j < out.size() && !merged; j++)
                if (Meets(out[i], out[j]) && Merged(out[i], out[j]))
                {
                    const int x0 = std::min(out[i].x, out[j].x), y0 = std::min(out[i].y, out[j].y);
                    const int x1 = std::max(out[i].x + out[i].w, out[j].x + out[j].w), y1 = std::max(out[i].y + out[i].h, out[j].y + out[j].h);
                    out[i] = {x0, y0, x1 - x0, y1 - y0};
                    out.erase(out.begin() + (long)j);
                    merged = true;
                }
    }
    return out;
}
}

void LayerCache::Update(const std::vector<UiItem>& items, float scale, float originX, float originY, Direct direct, void* user)
{
    std::vector<LayerAt> layers = LayersAt(items, scale, originX, originY);
    std::vector<Entry> old = std::move(entries);
    std::vector<bool> taken(old.size(), false);
    entries.clear();
    entries.reserve(layers.size());
    size_t images = 0;
    for (LayerAt& l : layers)
    {
        // What this layer was last time: the still layer of the same number, the picture in the
        // same place among the pictures.
        Entry e;
        {
            size_t k = 0;
            for (size_t i = 0; i < old.size(); i++)
            {
                if (old[i].still != l.still) continue;
                const bool match = l.still ? old[i].layer == l.layer : k++ == images;
                if (match && !taken[i]) { e = std::move(old[i]); taken[i] = true; break; }
            }
        }
        if (!l.still) images++;
        static std::atomic<u64> ids {0};
        if (!e.id) e.id = ++ids;
        const bool same = e.hash && e.x == l.x && e.y == l.y && e.w == l.w && e.h == l.h && e.ox == l.ox && e.oy == l.oy && e.scale == scale &&
                          e.px.size() == (size_t)l.w * l.h;
        e.still = l.still;
        e.layer = l.layer;
        e.x = l.x;
        e.y = l.y;
        e.w = l.w;
        e.h = l.h;
        e.ox = l.ox;
        e.oy = l.oy;
        e.scale = scale;
        e.changed = false;
        e.dirty.clear();
        if (!l.still && direct && direct(l.items[0], l, user))
        {
            // Drawn by the caller: nothing kept here.
            e.direct = true;
            e.image = l.items[0];
            e.px.clear();
            e.px.shrink_to_fit();
            e.hash = 0;
            e.itemHash.clear();
            e.itemBox.clear();
            entries.push_back(std::move(e));
            continue;
        }
        e.direct = false;
        std::vector<u64> hashes(l.items.size());
        std::vector<Rect> boxes(l.items.size());
        u64 hash = 0xCBF29CE484222325ull;
        for (size_t i = 0; i < l.items.size(); i++)
        {
            hashes[i] = HashItem(l.items[i]);
            boxes[i] = Footprint(l.items[i], scale, l.ox, l.oy, l.w, l.h);
            Mix(hash, hashes[i]);
        }
        for (u64 v : {(u64)(u32)l.x, (u64)(u32)l.y, (u64)l.w, (u64)l.h, FloatBits(l.ox), FloatBits(l.oy), FloatBits(scale)}) Mix(hash, v);
        if (!same || hash != e.hash)
        {
            std::vector<Rect> redo;
            if (same) redo = Changed(e.itemHash, e.itemBox, hashes, boxes);
            long long area = 0;
            for (const Rect& r : redo) area += (long long)r.w * r.h;
            if (!same || area * 20 > (long long)l.w * l.h * 17) redo = {Rect {0, 0, l.w, l.h}}; // (most of it: the whole)
            if (!same) e.px.assign((size_t)l.w * l.h, 0);
            for (const Rect& r : redo) Rasterise(l.items, scale, e.px.data(), l.w, l.h, l.ox, l.oy, r);
            e.dirty = std::move(redo);
            e.changed = !e.dirty.empty() || !same;
        }
        e.hash = hash;
        e.itemHash = std::move(hashes);
        e.itemBox = std::move(boxes);
        entries.push_back(std::move(e));
    }
}

bool WholeFactorCrop(const UiItem& im, const LayerAt& at, float scale)
{
    if (im.kind != UiItem::Image || !im.src || im.srcW <= 0 || im.srcH <= 0) return false;
    const Rect f = im.from;
    if (f.w <= 0 || f.h <= 0 || f.x < 0 || f.y < 0 || f.x + f.w > im.srcW || f.y + f.h > im.srcH) return false;
    const float tw = im.to.w * scale, th = im.to.h * scale;
    if (tw != std::floor(tw) || th != std::floor(th) || at.ox != (float)im.to.x || at.oy != (float)im.to.y) return false;
    const int k = std::max(1, (int)std::floor(std::min(tw / f.w, th / f.h)));
    if ((int)tw != f.w * k || (int)th != f.h * k || at.w < (int)tw || at.h < (int)th) return false;
    for (int y = f.y; y < f.y + f.h; y++)
        for (int x = f.x; x < f.x + f.w; x++)
            if ((im.src[(size_t)y * im.srcW + x] >> 24) != 0xFF) return false;
    return true;
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
