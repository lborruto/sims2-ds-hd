#include "port/statusbar.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
// The game's string ids (the same in every language's table): the clock's format, am, pm, the
// money's format, the HUD's "Sanity:" (research: the HUD draw routine asks for them each frame).
constexpr int kClockId = 30, kAmId = 178, kPmId = 179, kMoneyId = 28, kSanityId = 2421;

// Sizes, canvas pixels.
constexpr float kValueSize = 25, kLabelSize = 19;
constexpr int kPad = 16, kGap = 13, kDivider = 2, kMeterH = 20, kMeterW = 200, kMeterMin = 72, kLabelGap = 10;

void Replace(std::string& s, const std::string& from, const std::string& to)
{
    for (size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
}

bool StartsWith(const std::string& s, size_t at, const char* p)
{
    const size_t n = strlen(p);
    return s.size() >= at + n && s.compare(at, n, p) == 0;
}

// A string from the table without what is not text: RomData's {xx} / {F0xx} escapes dropped,
// spaces (and no-break spaces) trimmed at both ends.
std::string Clean(const std::string& in)
{
    std::string s;
    for (size_t i = 0; i < in.size(); i++)
    {
        if (in[i] == '{')
        {
            const size_t e = in.find('}', i);
            if (e != std::string::npos) { i = e; continue; }
        }
        s += in[i];
    }
    for (;;)
    {
        if (!s.empty() && (s.back() == ' ' || s.back() == '\n')) s.pop_back();
        else if (s.size() >= 2 && StartsWith(s, s.size() - 2, "\xC2\xA0")) s.resize(s.size() - 2);
        else break;
    }
    while (!s.empty() && (s[0] == ' ' || s[0] == '\n' || StartsWith(s, 0, "\xC2\xA0"))) s.erase(0, s[0] == ' ' || s[0] == '\n' ? 1 : 2);
    return s;
}

// '@' and the letters or digits after it (a placeholder this bar does not fill) dropped.
std::string DropPlaceholders(const std::string& in)
{
    std::string s;
    for (size_t i = 0; i < in.size(); i++)
    {
        if (in[i] == '@')
        {
            while (i + 1 < in.size() && std::isalnum((unsigned char)in[i + 1])) i++;
            continue;
        }
        s += in[i];
    }
    return s;
}

ui::UiItem Plain(Rect to, u32 argb, float radius)
{
    ui::UiItem it;
    it.kind = ui::UiItem::Panel;
    it.plain = true;
    it.to = to;
    it.fill = argb;
    it.radius = std::min(radius, std::min(to.w, to.h) / 2.0f);
    it.raised = false;
    return it;
}

std::string Clock(const HudStrings& s, int hour, int minute)
{
    std::string c = s.clock;
    const int h12 = hour % 12 == 0 ? 12 : hour % 12;
    char mm[8];
    snprintf(mm, sizeof mm, "%02d", minute);
    Replace(c, "@hh", std::to_string(h12));
    Replace(c, "@mm", mm);
    Replace(c, "@am", hour < 12 ? s.am : s.pm);
    return Clean(DropPlaceholders(c));
}

std::string Money(const HudStrings& s, int money)
{
    std::string m = s.money;
    Replace(m, "@1", GroupThousands(money));
    Replace(m, "$", "\xC2\xA7"); // the game's '$' is its simoleon sign
    return Clean(DropPlaceholders(m));
}
}

HudStrings HudStringsFrom(const RomData* rom)
{
    HudStrings s;
    if (!rom || rom->StringCount() <= kSanityId) return s;
    s.clock = rom->String(kClockId);
    s.am = Clean(rom->String(kAmId));
    s.pm = Clean(rom->String(kPmId));
    s.money = rom->String(kMoneyId);
    std::string label = Clean(rom->String(kSanityId));
    while (!label.empty() && (label.back() == ':' || label.back() == ' ')) label.pop_back();
    label = Clean(label);
    s.label = label.find('@') == std::string::npos ? label : "";
    s.ok = s.clock.find("@hh") != std::string::npos && s.clock.find("@mm") != std::string::npos && s.money.find("@1") != std::string::npos;
    return s;
}

std::string GroupThousands(int value)
{
    std::string d = std::to_string(std::max(value, 0)), out;
    for (size_t i = 0; i < d.size(); i++)
    {
        if (i > 0 && (d.size() - i) % 3 == 0) out += ',';
        out += d[i];
    }
    return out;
}

TierColours MeterColours(int tier)
{
    // Sampled from the game's bar (rows 181, 183, 185 of the top screen; tiers 1 and 3 from
    // states patched to them).
    static const TierColours k[4] = {
        {0xFFA20010, 0xFFEB0000, 0xFFFB92CB},
        {0xFF189218, 0xFF38BA38, 0xFF61F361},
        {0xFFBA8228, 0xFFDBA230, 0xFFFBFB79},
        {0xFF7979B2, 0xFFB2B2E3, 0xFFF3F3FB},
    };
    return k[std::clamp(tier, 0, 3)];
}

bool FormatHud(const HudState& hud, const HudStrings& s, std::string& clock, std::string& money, float& fill)
{
    if (!s.ok || !hud.valid) return false;
    if (hud.hour < 0 || hud.hour > 23 || hud.minute < 0 || hud.minute > 59 || hud.tier < 0 || hud.tier > 3) return false;
    if (hud.sanityMax <= 0 || hud.sanity < 0 || hud.money < 0 || hud.money > 99999999) return false;
    clock = Clock(s, hud.hour, hud.minute);
    money = Money(s, hud.money);
    fill = std::clamp((float)hud.sanity / (float)hud.sanityMax, 0.0f, 1.0f);
    return true;
}

Rect AddStatusBar(Composition& c, const HudState& hud, const HudStrings& s, int x, int y, int maxW)
{
    std::string clock, money;
    float fill = 0;
    if (!FormatHud(hud, s, clock, money, fill) || maxW < 2 * kPad + kMeterMin) return {x, y, 0, 0};
    using namespace ui;
    // Columns as wide as their widest value, so the bar keeps its size as the clock turns and
    // the money changes (up to six digits; more widens it).
    const float clockW = std::max(TextWidth(Clock(s, 10, 58), kValueSize, Weight::ExtraBold), TextWidth(Clock(s, 22, 58), kValueSize, Weight::ExtraBold));
    const float moneyW = std::max(TextWidth(Money(s, 888888), kValueSize, Weight::ExtraBold), TextWidth(money, kValueSize, Weight::ExtraBold));
    const int cw = (int)std::ceil(clockW), mw = (int)std::ceil(moneyW);
    std::string label = s.label;
    int lw = label.empty() ? 0 : (int)std::ceil(TextWidth(label, kLabelSize, Weight::Bold));
    const int fixed = kPad + cw + 2 * kGap + kDivider + mw + 2 * kGap + kDivider + kPad;
    int meterW = kMeterW;
    if (fixed + (lw ? lw + kLabelGap : 0) + meterW > maxW) meterW = std::max(kMeterMin, maxW - fixed - (lw ? lw + kLabelGap : 0));
    if (lw && fixed + lw + kLabelGap + meterW > maxW) { label.clear(); lw = 0; meterW = std::max(kMeterMin, std::min(kMeterW, maxW - fixed)); }
    const int w = std::min(maxW, fixed + (lw ? lw + kLabelGap : 0) + meterW);
    const Rect bar {x, y, w, kStatusBarH};
    const Theme& t = GameTheme();
    UiItem panel = PanelItem(bar);
    panel.radius = 14;
    panel.border = 3;
    c.ui.push_back(panel);

    // Inside the band: everything is held to it (a value too wide for a narrow bar is clipped).
    const int ix0 = x + kPad, ix1 = x + w - kPad, iy = y + 5, ih = kStatusBarH - 10;
    auto clip = [&](Rect r) {
        const int a = std::clamp(r.x, ix0, ix1), b = std::clamp(r.x + r.w, ix0, ix1);
        return Rect {a, r.y, b - a, r.h};
    };
    auto text = [&](Rect r, const std::string& str, float size, Weight wt) {
        UiItem it = TextItem(clip(r), str, size, Align::Centre, wt);
        it.wrap = false;
        it.middle = true;
        if (it.to.w > 0) c.ui.push_back(it);
    };
    auto divider = [&](int dx) {
        const Rect r = clip({dx, y + 11, kDivider, kStatusBarH - 22});
        if (r.w > 0) c.ui.push_back(Plain(r, t.frameDark, 1));
    };
    int cx = ix0;
    text({cx, iy, cw, ih}, clock, kValueSize, Weight::ExtraBold);
    cx += cw + kGap;
    divider(cx);
    cx += kDivider + kGap;
    text({cx, iy, mw, ih}, money, kValueSize, Weight::ExtraBold);
    cx += mw + kGap;
    divider(cx);
    cx += kDivider + kGap;
    if (lw)
    {
        text({cx, iy, lw, ih}, label, kLabelSize, Weight::Bold);
        cx += lw + kLabelGap;
    }
    // The meter: a well, and in it the bar filled cur / max in the tier's colours (rim, body, a
    // highlight along its top), as the game draws it.
    const Rect well = clip({cx, y + (kStatusBarH - kMeterH) / 2, meterW, kMeterH});
    if (well.w >= 12)
    {
        UiItem wi = WellItem(well);
        wi.radius = kMeterH / 2.0f;
        c.ui.push_back(wi);
        const Rect track {well.x + 3, well.y + 3, well.w - 6, well.h - 6};
        const int fw = (int)std::lround(track.w * fill);
        const TierColours tc = MeterColours(hud.tier);
        if (fw >= 2)
        {
            c.ui.push_back(Plain({track.x, track.y, fw, track.h}, tc.rim, track.h / 2.0f));
            c.ui.push_back(Plain({track.x + 1, track.y + 1, fw - 2, track.h - 3}, tc.body, (track.h - 3) / 2.0f));
            if (fw >= 8) c.ui.push_back(Plain({track.x + 3, track.y + 2, fw - 6, 3}, (tc.light & 0xFFFFFF) | 0xD9000000, 1.5f));
        }
    }
    return bar;
}

Rect AddStatusBar(Composition& c, const HudState& hud, const HudStrings& s)
{
    for (const Piece& p : c.pieces)
        if (p.src == Src::Top)
        {
            const int left = std::max(p.to.x, 0), right = std::min(p.to.x + p.to.w, c.cw);
            return AddStatusBar(c, hud, s, left + 16, kStatusBarY, right - left - 32);
        }
    return {0, 0, 0, 0};
}

void AddStatusBar(Composition& c, const HudState& hud, const RomData* rom)
{
    AddStatusBar(c, hud, HudStringsFrom(rom));
}
}
