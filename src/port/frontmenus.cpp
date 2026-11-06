#include "port/frontmenus.h"

#include <algorithm>
#include <cmath>

#include "console.h"
#include "port/front.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kRowFill = 0xFF6A72B8, kRowFocusFill = 0xFF9AA2E3, kTabPlateFill = 0xFF9AA2E3;
constexpr u32 kTrackFill = 0xFF51518A, kTrackOn = 0xFFB2B2E3, kNotch = 0xFF383869;
constexpr u32 kCurrent = 0xFF6BD36B;   // the game's green for its current slot's number
constexpr u32 kLabel = 0xFFD6DAF6;
constexpr int kCreditsLayer = 2;       // the credits' lines: they move every other frame, the rest stays
constexpr float kKnob = 44;

bool FrontScreenState(ScreenState s) { return s.screen == Screen::FrontEnd || s.screen == Screen::None; }

int TabTapY(int i) { return kSeTabY0 + i * kSeTabDy; }
Rect CellFrom(int i) { return {kSeCellX0 + (i & 3) * kSeCellDx, kSeCellY0 + (i >> 2) * kSeCellDy, kSeCellW, kSeCellH}; }
int SliderY(int page) { return page == 0 ? kSeMusicY : kSeSfxY; }
int HdKnobX(int volume) { return kSeHdTrackX + kSeHdTrackW * std::clamp(volume, 0, 4096) / 4096; }
int Percent(int volume) { return std::clamp((volume * 100 + 2048) / 4096, 0, 100); }

void SeHint(Composition& c, int& right, Button b, const std::string& label)
{
    const Rect r = ChipRect(0, 0, b, label, kPmHintSize, 0, Weight::ExtraBold);
    UiItem it = ChipItem(right - r.w, kSeHints.y + (kSeHints.h - r.h) / 2, b, label, kPmHintSize, 0, Weight::ExtraBold);
    it.layer = 1;
    c.ui.push_back(it);
    right -= r.w + 28;
}
}

int SettingsKnobX(int volume) { return kSeTrackX0 + (kSeTrackW * std::clamp(volume, 0, 4096) + 2048) / 4096; }

int SettingsNotchX(int volume, int dir)
{
    const double x = std::clamp(volume, 0, 4096) * 10.0 / 4096.0;
    const int r = (int)std::lround(x);
    const bool near = std::fabs(x - r) < 0.15;
    const int k = dir < 0 ? (near ? r - 1 : (int)std::floor(x)) : (near ? r + 1 : (int)std::ceil(x));
    if (k < 0 || k > 10) return -1;
    // The game moves its knob by the stylus's own move from where it took it (32 a pixel: a drag
    // of dx pixels changes the volume by 32 dx, measured), so the drag is that tenth's distance
    // from the volume, the ends a little past (the game stops there).
    const int target = (4096 * k + 5) / 10;
    int dx = (int)std::lround((target - std::clamp(volume, 0, 4096)) / 32.0);
    if (k == 0) dx -= 2;
    if (k == 10) dx += 2;
    return SettingsKnobX(volume) + dx;
}

std::string SlotTimeText(const std::string& format, const LoadRow& r, const std::string& am, const std::string& pm)
{
    if (!r.timeKnown) return {};
    const int h24 = (r.hour12 % 12) + (r.pm ? 12 : 0);
    auto two = [](int v) { char b[8]; snprintf(b, sizeof b, "%02d", v); return std::string(b); };
    std::string out;
    for (size_t i = 0; i < format.size(); i++)
    {
        if (format[i] != '%' || i + 1 >= format.size()) { out += format[i]; continue; }
        switch (format[++i])
        {
        case 'n': out += std::to_string(r.month); break;
        case 'm': out += two(r.month); break;
        case 'j': out += std::to_string(r.day); break;
        case 'd': out += two(r.day); break;
        case 'g': out += std::to_string(r.hour12); break;
        case 'h': out += two(r.hour12); break;
        case 'G': out += std::to_string(h24); break;
        case 'H': out += two(h24); break;
        case 'i': out += two(r.minute); break;
        case 'a': out += r.pm ? pm : am; break;
        case 'A':
        {
            std::string s = r.pm ? pm : am;
            for (char& ch : s) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
            out += s;
            break;
        }
        case '%': out += '%'; break;
        default: break; // (an unknown code: dropped, never shown raw)
        }
    }
    return out;
}

std::string CreditsLine(const RomData& rom, int line, u32& colour)
{
    colour = 0;
    if (line < 1) return {};
    const int id = kStrCredits0 + ((line - 1) % kCreditsLines);
    if (id >= rom.StringCount()) return {};
    std::string s = rom.String(id);
    if (s.size() >= 2 && s[0] == '^' && (s[1] == '1' || s[1] == '2'))
    {
        colour = s[1] == '1' ? kCreditsPink : kCreditsGreen;
        s.erase(0, 2);
    }
    return MenuText(s);
}

// ---- Settings ----

MenuView SettingsView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrCredits0 + kCreditsLines - 1) return {};
    return SettingsViewOf(s, *game, rom);
}

MenuView SettingsViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const SettingsState& st = game.front.settings;
    if (!FrontScreenState(s) || !game.valid || !st.valid || st.page < 0 || st.page >= kSeTabs) return v;
    if (st.page == 2 && !st.conceptKnown) return v;
    if (st.page == 3 && !st.creditsKnown) return v;
    if (rom) v.title = MenuWords(*rom, kStrSeTitle);
    if (st.page <= 1)
    {
        // The volume: Left / Right drag the game's knob to the next tenth.
        const int vol = st.page == 0 ? st.music : st.sfx, y = SliderY(st.page), from = SettingsKnobX(vol);
        ViewItem it;
        it.rect = kSeSlider;
        it.id = kSeSliderId + (u32)st.page;
        const int l = SettingsNotchX(vol, -1), r = SettingsNotchX(vol, +1);
        if (l >= 0) it.left = {DragStep(from, y, l, y)};
        if (r >= 0) it.right = {DragStep(from, y, r, y)};
        for (int& n : it.next) n = kNoMove;
        v.items.push_back(it);
    }
    else if (st.page == 2)
    {
        for (int i = 0; i < kSeCells; i++)
        {
            ViewItem it;
            it.rect = SettingsCellRect(i);
            it.id = kSeConceptId + (u32)i;
            // A locked picture: focusable (the grid keeps its shape under the thumb), nothing to do
            // (the game only plays a sound).
            if (st.conceptBits & (1u << i))
            {
                const Rect f = CellFrom(i);
                it.action = {TapStep(f.x + f.w / 2, f.y + f.h / 2)};
            }
            // The grid's edges: no move past them.
            if (i < 4) it.next[0] = kNoMove;
            if (i >= 8) it.next[1] = kNoMove;
            if ((i & 3) == 0) it.next[2] = kNoMove;
            if ((i & 3) == 3) it.next[3] = kNoMove;
            v.items.push_back(it);
        }
        v.initial = 0;
        for (int i = kSeCells - 1; i >= 0; i--)
            if (st.conceptBits & (1u << i)) v.initial = i;
    }
    // (Credits: nothing to press; the game scrolls them.)
    v.buttons[(int)MenuButton::LB] = {TapStep(kSeTabX, TabTapY((st.page + kSeTabs - 1) % kSeTabs))};
    v.buttons[(int)MenuButton::RB] = {TapStep(kSeTabX, TabTapY((st.page + 1) % kSeTabs))};
    v.back = {TapStep(kSeCloseX, kSeCloseY)};
    v.backCloses = true;
    v.page = 0x10 + st.page;
    v.replay = false;
    v.own = true;
    v.valid = true;
    return v;
}

void BuildSettings(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, const u32* bottom, IconCache* icons, int topW, int topH)
{
    const Theme& th = GameTheme();
    const SettingsState& st = game.front.settings;
    if (!st.valid || st.page < 0 || st.page >= kSeTabs) return;
    const size_t first = c.ui.size();
    // The top screen (the game's backdrop, or the concept art it shows) at the left, framed.
    const Rect from = FrontTopFrom(topW, topH);
    FrontFramed(c, {Src::Top, from, FrontFit(kFrTopBox, from.w, from.h), 255, false});

    // Layer 0: the panel, the title's well, the tab bar and its LB / RB.
    UiItem panel = PanelItem(kFrPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem title = WellItem(kSeTitle);
    title.radius = 14;
    c.ui.push_back(title);
    UiItem bar = WellItem(kSeTabBar);
    bar.fill = th.frameDark;
    bar.radius = 16;
    c.ui.push_back(bar);
    {
        const Rect lb = ChipRect(0, 0, Button::LB, "", 24);
        c.ui.push_back(ChipItem(kSeTabsX - 10 - lb.w, kSeTabBar.y + (kSeTabBar.h - lb.h) / 2, Button::LB, "", 24));
        c.ui.push_back(ChipItem(kSeTabsX + kSeTabs * kSeTabCell + 10, kSeTabBar.y + (kSeTabBar.h - lb.h) / 2, Button::RB, "", 24));
    }

    // Layer 1: the title, the page's plate, the page's name, the page, the hints.
    PanelText(c, {kSeTitle.x + 20, kSeTitle.y, kSeTitle.w - 40, kSeTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Centre, Weight::ExtraBold);
    {
        const Rect cell = SettingsTabRect(st.page);
        UiItem plate = PanelItem({cell.x + 1, cell.y, cell.w - 2, cell.h});
        plate.fill = kTabPlateFill;
        plate.borderArgb = th.fill;
        plate.border = 3;
        plate.radius = 16;
        plate.layer = 1;
        c.ui.push_back(plate);
    }
    PanelText(c, kSeLabel, MenuWords(rom, kStrSePage0 + st.page), 30, 20, Align::Centre, Weight::ExtraBold, kLabel);

    std::vector<UiItem> images;
    // The game's own tabs, live (its selected one on its green plate).
    for (int i = 0; i < kSeTabs && bottom; i++)
    {
        const Rect cell = SettingsTabRect(i);
        const int w = kSeTabFrom.w * 2 - 4, h = kSeTabFrom.h * 2 - 4; // (2x less a pixel a side: the cell's rim stays)
        images.push_back(ImageItem({cell.x + (cell.w - w) / 2, cell.y + (cell.h - h) / 2, w, h}, bottom, SW, SH,
                                   {kSeTabFrom.x + 1, kSeTabFrom.y + i * kSeTabDy + 1, kSeTabFrom.w - 2, kSeTabFrom.h - 2}));
        AddMousePiece(c, {kSeTabX - 12, TabTapY(i) - 12, 24, 24}, cell);
    }

    if (st.page <= 1 && !v.items.empty())
    {
        const int vol = st.page == 0 ? st.music : st.sfx;
        const bool focused = focus == 0;
        const Rect w = kSeSlider;
        if (focused) AddFocusRing(c, w, 16);
        UiItem well = WellItem(w);
        well.radius = 16;
        well.layer = 1;
        c.ui.push_back(well);
        UiItem pct = TextItem({w.x, w.y + 18, w.w, 64}, std::to_string(Percent(vol)) + "%", 48, Align::Centre, Weight::ExtraBold);
        pct.wrap = false;
        pct.middle = true;
        pct.layer = 1;
        c.ui.push_back(pct);
        if (focused)
        {   // The D-pad's chip: Left / Right move the knob.
            const Rect dp = ChipRect(0, 0, Button::DPadH, "", 26);
            UiItem chip = ChipItem(w.x + 28, w.y + 18 + (64 - dp.h) / 2, Button::DPadH, "", 26);
            chip.layer = 1;
            c.ui.push_back(chip);
        }
        const int ty = w.y + 124;
        UiItem track = PanelItem({kSeHdTrackX - 6, ty - 7, kSeHdTrackW + 12, 14});
        track.plain = true;
        track.fill = kTrackFill;
        track.radius = 7;
        track.layer = 1;
        c.ui.push_back(track);
        UiItem on = PanelItem({kSeHdTrackX - 6, ty - 7, HdKnobX(vol) - kSeHdTrackX + 12, 14});
        on.plain = true;
        on.fill = kTrackOn;
        on.radius = 7;
        on.layer = 1;
        c.ui.push_back(on);
        for (int k = 0; k <= 10; k++)
        {
            UiItem d = PanelItem({kSeHdTrackX + kSeHdTrackW * k / 10 - 3, ty - 3, 6, 6});
            d.plain = true;
            d.fill = kNotch;
            d.radius = 3;
            d.layer = 1;
            c.ui.push_back(d);
        }
        UiItem knob = PanelItem({HdKnobX(vol) - (int)kKnob / 2, ty - (int)kKnob / 2, (int)kKnob, (int)kKnob});
        knob.fill = focused ? kRowFocusFill : th.fill;
        knob.borderArgb = th.frame;
        knob.border = 4;
        knob.radius = kKnob / 2;
        knob.layer = 1;
        c.ui.push_back(knob);
        // The mouse: the HD track onto the game's (a drag from the knob drags the game's).
        const float s = (float)kSeHdTrackW / kSeTrackW;
        const int y = SliderY(st.page);
        AddMousePiece(c, {kSeTrackX0 - 8, y - 12, kSeTrackW + 16, 24}, {kSeHdTrackX - (int)std::lround(8 * s), ty - 30, (int)std::lround((kSeTrackW + 16) * s), 60});
    }
    else if (st.page == 2)
    {
        for (int i = 0; i < kSeCells && i < (int)v.items.size(); i++)
        {
            const Rect r = SettingsCellRect(i);
            if (focus == i) AddFocusRing(c, r, 12);
            UiItem cell = WellItem(r);
            cell.radius = 12;
            cell.layer = 1;
            c.ui.push_back(cell);
            const Rect f = CellFrom(i);
            if (bottom) images.push_back(ImageItem({r.x + (r.w - 2 * f.w) / 2, r.y + (r.h - 2 * f.h) / 2, 2 * f.w, 2 * f.h}, bottom, SW, SH, f));
            AddMousePiece(c, f, r);
        }
    }
    else if (st.page == 3)
    {
        UiItem well = WellItem(kSeCredits);
        well.radius = 16;
        well.layer = 1;
        c.ui.push_back(well);
        // The game's lines up to its current one, scrolling up with its position (16.16, in lines):
        // the newest at the foot, each a row higher a line later; only whole rows inside the well.
        const int line = st.creditsLine;
        const double pos = std::clamp(st.creditsPos / 65536.0, (double)line, line + 0.999);
        const int pad = 10, bottomY = kSeCredits.y + kSeCredits.h - pad;
        for (int l = line; l >= 1 && l > line - 16; l--)
        {
            const int y = bottomY - (int)std::lround(kSeCreditsRow * (pos - l + 1));
            if (y < kSeCredits.y + pad) break;
            u32 colour = 0;
            const std::string text = CreditsLine(rom, l, colour);
            if (text.empty()) continue;
            const FittedText t = FitLine(text, 26, 18, kSeCredits.w - 40, colour ? Weight::ExtraBold : Weight::Bold);
            UiItem tt = TextItem({kSeCredits.x + 20, y, kSeCredits.w - 40, kSeCreditsRow}, t.text, t.size, Align::Centre, colour ? Weight::ExtraBold : Weight::Bold);
            tt.wrap = false;
            tt.middle = true;
            if (colour) tt.colour = colour;
            tt.layer = kCreditsLayer;
            c.ui.push_back(tt);
        }
    }

    // The hints: A (an unlocked picture), B "Close" (the title).
    int right = kSeHints.x + kSeHints.w - 8;
    SeHint(c, right, Button::B, MenuWords(rom, kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) SeHint(c, right, Button::A, "");
    for (UiItem& im : images) c.ui.push_back(im);
    // The game's X (B), the mouse onto its own.
    AddSpriteIcon(c, icons, kSprX, 0, 2, kSeX);
    AddMousePiece(c, {kSeCloseX - 14, kSeCloseY - 14, 28, 28}, kSeX);
    SortUi(c.ui, first);
}

// ---- Load-a-Sim / DS Wireless ----

MenuView LoadView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrLdTime) return {};
    return LoadViewOf(s, *game, rom);
}

MenuView LoadViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const LoadScreen& ld = game.front.load;
    // DS Wireless (the same slot list) keeps milestone 1's view: its tick opens the wireless lobby.
    if (!FrontScreenState(s) || !game.valid || !ld.valid || ld.wireless) return v;
    if (rom) v.title = MenuWords(*rom, kStrLdTitle);
    // A popup over the slots (the game's "Loading"): the slots wait under it (port/popup.h).
    const bool popup = game.town.popup.valid && game.town.popup.state >= 1 && game.town.popup.state <= 3;
    for (int i = 0; i < kLoadRows; i++)
    {
        ViewItem it;
        it.rect = LoadRowRect(i);
        it.id = kLdRowId + (u32)i;
        // The game shows a slot's tick only when the slot is used.
        it.enabled = ld.rows[i].used && !popup;
        if (it.enabled) it.action = {TapStep(kLdTickX, kLdTickY[i])};
        it.closes = true; // (the game loads: the screen goes)
        it.next[2] = it.next[3] = kNoMove;
        if (rom) it.word = MenuWords(*rom, kStrLdTitle);
        v.items.push_back(it);
    }
    if (!popup)
    {
        v.back = {TapStep(kLdCloseX, kLdCloseY)};
        v.backCloses = true;
        // The game's current slot when used, else the first used one.
        v.initial = -1;
        if (ld.green >= 0 && ld.green < kLoadRows && ld.rows[ld.green].used) v.initial = ld.green;
        else
            for (int i = kLoadRows - 1; i >= 0; i--)
                if (ld.rows[i].used) v.initial = i;
    }
    v.page = 0x20;
    // A press held while the game was not settled is dropped: here a late A loads.
    v.replay = false;
    v.own = true;
    v.valid = true;
    return v;
}

void BuildLoad(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, int topW, int topH)
{
    const Theme& th = GameTheme();
    const LoadScreen& ld = game.front.load;
    if (!ld.valid || (int)v.items.size() < kLoadRows) return;
    const size_t first = c.ui.size();
    // The game's top screen (the title's backdrop) over the canvas, dimmed.
    const Rect from = FrontTopFrom(topW, topH);
    Piece top {Src::Top, from, FrontFit({0, 0, kConsoleW, kConsoleH}, from.w, from.h)};
    top.touch = false;
    c.pieces.push_back(top);
    c.overlays.push_back({{0, 0, kConsoleW, kConsoleH}, kDimArgb, 0, 0, 0});

    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem title = WellItem(kLdTitle);
    title.radius = 16;
    c.ui.push_back(title);

    PanelText(c, {kLdTitle.x + 24, kLdTitle.y, kLdTitle.w - 48, kLdTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Centre, Weight::ExtraBold);
    const std::string format = rom.String(kStrLdTime), am = MenuWords(rom, kStrLdAm), pm = MenuWords(rom, kStrLdPm);
    for (int i = 0; i < kLoadRows; i++)
    {
        const Rect r = LoadRowRect(i);
        const LoadRow& row = ld.rows[i];
        const bool focused = i == focus && v.items[i].enabled;
        if (focused) AddFocusRing(c, r, 22);
        UiItem bg = PanelItem(r);
        bg.fill = focused ? kRowFocusFill : kRowFill;
        bg.borderArgb = th.frame;
        bg.border = 3;
        bg.radius = 22;
        bg.layer = 1;
        c.ui.push_back(bg);
        // The number in its round well, green for the game's current slot.
        UiItem nw = PanelItem({r.x + 20, r.y + (r.h - 100) / 2, 100, 100});
        nw.plain = true;
        nw.fill = th.frameDark;
        nw.radius = 50;
        nw.layer = 1;
        c.ui.push_back(nw);
        UiItem n = TextItem({r.x + 20, r.y, 100, r.h}, std::to_string(i + 1), 64, Align::Centre, Weight::ExtraBold);
        n.wrap = false;
        n.middle = true;
        if (ld.green == i) n.colour = kCurrent;
        n.layer = 1;
        c.ui.push_back(n);
        const int tx = r.x + 148, tw = r.w - 148 - 32;
        if (!row.used) PanelText(c, {tx, r.y, tw, r.h}, MenuWords(rom, kStrLdNoData), 32, 22, Align::Left, Weight::ExtraBold, kLabel);
        else
        {
            // Line 1: the name and the money; line 2: the place and the time it was saved.
            const std::string money = MenuWords(rom, kStrLdMoney, std::max(0, row.money));
            const int mw = (int)TextWidth(money, 32, Weight::ExtraBold) + 8;
            PanelText(c, {tx, r.y + 22, tw - mw - 24, 48}, row.name, 34, 22, Align::Left, Weight::ExtraBold);
            PanelText(c, {tx + tw - mw, r.y + 22, mw, 48}, money, 32, 32, Align::Right, Weight::ExtraBold);
            const std::string time = SlotTimeText(format, row, am, pm);
            const int timeW = time.empty() ? 0 : (int)TextWidth(time, 28, Weight::Bold) + 8;
            if (row.placeNameId >= 0) PanelText(c, {tx, r.y + 74, tw - timeW - 24, 44}, MenuWords(rom, row.placeNameId), 28, 20, Align::Left, Weight::Bold, kLabel);
            if (!time.empty()) PanelText(c, {tx + tw - timeW, r.y + 74, timeW, 44}, time, 28, 28, Align::Right, Weight::Bold, kLabel);
        }
        // The mouse: the row onto its tick (a click loads, as A), while it can.
        if (v.items[i].enabled) AddMousePiece(c, {kLdTickX - 12, kLdTickY[i] - 12, 24, 24}, r);
    }
    const bool popup = game.town.popup.valid && game.town.popup.state >= 1 && game.town.popup.state <= 3;
    if (!popup)
    {
        int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
        if (MenuFocusable(v, focus))
        {
            const FittedText a = FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, right - kPmHints.x - 60, Weight::ExtraBold);
            AddHint(c, right, Button::A, a.text);
        }
    }
    // The game's X (B, while no popup is up), the mouse onto its own.
    if (!popup)
    {
        AddSpriteIcon(c, icons, kSprX, 0, 2, kLdX);
        AddMousePiece(c, {kLdCloseX - 14, kLdCloseY - 14, 28, 28}, kLdX);
    }
    SortUi(c.ui, first);
}

void MeasureFrontMenus(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    tally.Line(kStrSeTitle, MenuWords(rom, kStrSeTitle), kPmTitleSize, kPmTextMin, kSeTitle.w - 40);
    for (int p = 0; p < kSeTabs; p++) tally.Line(kStrSePage0 + p, MenuWords(rom, kStrSePage0 + p), 30, 20, kSeLabel.w);
    for (int l = 1; l <= kCreditsLines; l++)
    {
        u32 colour = 0;
        const std::string t = CreditsLine(rom, l, colour);
        if (!t.empty()) tally.Line(kStrCredits0 + l - 1, t, 26, 18, kSeCredits.w - 40, colour ? Weight::ExtraBold : Weight::Bold);
    }
    tally.Line(kStrLdTitle, MenuWords(rom, kStrLdTitle), kPmTitleSize, kPmTextMin, kLdTitle.w - 48);
    const int tw = 880 - 148 - 32;
    tally.Line(kStrLdNoData, MenuWords(rom, kStrLdNoData), 32, 22, tw);
    const int mw = (int)TextWidth(MenuWords(rom, kStrLdMoney, 99999999), 32, Weight::ExtraBold) + 8;
    tally.Line(-1, "WWWWWWW", 34, 22, tw - mw - 24); // (the widest name the game's keyboard makes: no string)
    // The widest time: two-digit month and day, 12:59 or 23:59, am / pm.
    LoadRow r;
    r.timeKnown = true;
    r.month = 12; r.day = 28; r.hour12 = 12; r.minute = 58; r.pm = true;
    const std::string time = SlotTimeText(rom.String(kStrLdTime), r, MenuWords(rom, kStrLdAm), MenuWords(rom, kStrLdPm));
    tally.Line(kStrLdTime, time, 28, 28, 220);
    const int timeW = (int)TextWidth(time, 28, Weight::Bold) + 8;
    for (int place = 0; place < 0x20; place++)
    {
        const std::string p = MenuWords(rom, kStrLdPlace0 + place);
        if (p.empty() || p == "Dummy" || p == "Empty String") continue;
        tally.Line(kStrLdPlace0 + place, p, 28, 20, tw - timeW - 24, Weight::Bold);
    }
    for (int id : {kStrLdTitle, kStrClose}) tally.Line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 300);
}
}
