#include "ui/font.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

// The fonts' bytes (cmake/embed.cmake, from assets/fonts).
extern const unsigned char nunito_bold[];
extern const unsigned char nunito_extrabold[];

namespace sims2wide::ui
{
namespace
{
struct Face
{
    stbtt_fontinfo info;
    int ascent, descent, lineGap;
    int fallback; // '?'
};

const Face& FaceOf(Weight w)
{
    static Face faces[2];
    static bool ready = false;
    if (!ready)
    {
        const unsigned char* data[2] = {nunito_bold, nunito_extrabold};
        for (int i = 0; i < 2; i++)
        {
            Face& f = faces[i];
            stbtt_InitFont(&f.info, data[i], stbtt_GetFontOffsetForIndex(data[i], 0));
            stbtt_GetFontVMetrics(&f.info, &f.ascent, &f.descent, &f.lineGap);
            f.fallback = stbtt_FindGlyphIndex(&f.info, '?');
        }
        ready = true;
    }
    return faces[w == Weight::ExtraBold ? 1 : 0];
}

// Code points with the byte each starts at.
struct Cp { u32 cp; size_t at; };
std::vector<Cp> Decode(const std::string& s)
{
    std::vector<Cp> out;
    size_t i = 0;
    while (i < s.size())
    {
        const u8 c = (u8)s[i];
        int n = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        u32 cp = n == 0 ? c : n == 1 ? c & 0x1F : n == 2 ? c & 0x0F : c & 0x07;
        bool ok = n >= 0 && i + n < s.size();
        for (int k = 1; ok && k <= n; k++)
        {
            const u8 d = (u8)s[i + k];
            if ((d & 0xC0) != 0x80) ok = false;
            else cp = (cp << 6) | (d & 0x3F);
        }
        if (!ok) { out.push_back({0xFFFD, i}); i++; continue; }
        out.push_back({cp, i});
        i += n + 1;
    }
    return out;
}

int GlyphOf(const Face& f, u32 cp)
{
    const int g = stbtt_FindGlyphIndex(&f.info, (int)cp);
    return g ? g : f.fallback;
}

float ScaleOf(const Face& f, float sizePx) { return stbtt_ScaleForMappingEmToPixels(&f.info, sizePx); }

// The pen's advance over a run of code points, in pixels at sizePx.
float Advance(const Face& f, float scale, const Cp* cps, size_t n)
{
    float x = 0;
    int prev = -1;
    for (size_t i = 0; i < n; i++)
    {
        if (cps[i].cp == '\n') continue;
        const int g = GlyphOf(f, cps[i].cp);
        if (prev >= 0) x += stbtt_GetGlyphKernAdvance(&f.info, prev, g) * scale;
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&f.info, g, &adv, &lsb);
        x += adv * scale;
        prev = g;
    }
    return x;
}

struct Glyph { int x0, y0, w, h; std::vector<u8> px; };
const Glyph& Rendered(const Face& f, Weight w, float sizePx, int g, int sub)
{
    static std::unordered_map<u64, Glyph> cache;
    const u64 size64 = (u64)std::lround(sizePx * 64) & 0xFFFFF;
    const u64 key = ((u64)(w == Weight::ExtraBold) << 62) | (size64 << 40) | ((u64)(g & 0xFFFF) << 8) | (u64)sub;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 8192) cache.clear(); // many sizes: start again (glyphs come back the same)
    Glyph out {};
    const float scale = ScaleOf(f, size64 / 64.0f), shift = sub / 4.0f;
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBoxSubpixel(&f.info, g, scale, scale, shift, 0, &x0, &y0, &x1, &y1);
    out.x0 = x0; out.y0 = y0; out.w = std::max(x1 - x0, 0); out.h = std::max(y1 - y0, 0);
    out.px.assign((size_t)out.w * out.h, 0);
    if (out.w && out.h) stbtt_MakeGlyphBitmapSubpixel(&f.info, out.px.data(), out.w, out.h, out.w, scale, scale, shift, 0, g);
    return cache.emplace(key, std::move(out)).first->second;
}
}

LineMetrics Metrics(float sizePx, Weight w)
{
    const Face& f = FaceOf(w);
    const float s = ScaleOf(f, sizePx);
    return {f.ascent * s, -f.descent * s, (f.ascent - f.descent + f.lineGap) * s};
}

float TextWidth(const std::string& utf8, float sizePx, Weight w)
{
    const Face& f = FaceOf(w);
    const std::vector<Cp> cps = Decode(utf8);
    return Advance(f, ScaleOf(f, sizePx), cps.data(), cps.size());
}

std::vector<std::string> Wrap(const std::string& utf8, float sizePx, float widthPx, Weight w)
{
    std::vector<std::string> lines;
    auto fits = [&](const std::string& s) { return TextWidth(s, sizePx, w) <= widthPx; };
    size_t p = 0;
    while (p <= utf8.size())
    {
        size_t e = utf8.find('\n', p);
        if (e == std::string::npos) e = utf8.size();
        const std::string para = utf8.substr(p, e - p);
        std::string line;
        bool any = false; // a paragraph with no word still makes an empty line (between two '\n')
        size_t q = 0;
        while (q < para.size())
        {
            while (q < para.size() && para[q] == ' ') q++;
            if (q >= para.size()) break;
            size_t r = para.find(' ', q);
            if (r == std::string::npos) r = para.size();
            std::string word = para.substr(q, r - q);
            q = r;
            any = true;
            const std::string joined = line.empty() ? word : line + " " + word;
            if (fits(joined)) { line = joined; continue; }
            if (!line.empty()) { lines.push_back(line); line.clear(); }
            // The word alone: broken between letters while it is too wide.
            while (!fits(word))
            {
                // The most letters that fit (cps.size() would be the whole word, which does
                // not), at least one.
                const std::vector<Cp> cps = Decode(word);
                size_t keep = 1;
                while (keep + 1 < cps.size() && fits(word.substr(0, cps[keep + 1].at))) keep++;
                if (keep >= cps.size()) break;
                lines.push_back(word.substr(0, cps[keep].at));
                word = word.substr(cps[keep].at);
            }
            line = word;
        }
        if (!line.empty()) lines.push_back(line);
        else if (!any && e < utf8.size()) lines.push_back("");
        if (e >= utf8.size()) break;
        p = e + 1;
    }
    // Trailing empty lines are of no use.
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

void DrawLine(const std::string& utf8, float sizePx, Weight w, float x, float y, u8* mask, int mw, int mh)
{
    const Face& f = FaceOf(w);
    const float scale = ScaleOf(f, std::lround(sizePx * 64) / 64.0f);
    const std::vector<Cp> cps = Decode(utf8);
    const int baseline = (int)std::lround(y);
    float pen = x;
    int prev = -1;
    for (const Cp& c : cps)
    {
        if (c.cp == '\n') continue;
        const int g = GlyphOf(f, c.cp);
        if (prev >= 0) pen += stbtt_GetGlyphKernAdvance(&f.info, prev, g) * scale;
        // The pen to a quarter pixel: the glyph is rendered at that phase.
        const float q = std::floor(pen * 4 + 0.5f) / 4;
        const int ix = (int)std::floor(q), sub = (int)((q - ix) * 4 + 0.5f) & 3;
        const Glyph& gl = Rendered(f, w, sizePx, g, sub);
        for (int gy = 0; gy < gl.h; gy++)
        {
            const int Y = baseline + gl.y0 + gy;
            if (Y < 0 || Y >= mh) continue;
            for (int gx = 0; gx < gl.w; gx++)
            {
                const int X = ix + gl.x0 + gx;
                if (X < 0 || X >= mw) continue;
                u8& m = mask[(size_t)Y * mw + X];
                m = (u8)std::min(255, m + gl.px[(size_t)gy * gl.w + gx]);
            }
        }
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&f.info, g, &adv, &lsb);
        pen += adv * scale;
        prev = g;
    }
}
}
