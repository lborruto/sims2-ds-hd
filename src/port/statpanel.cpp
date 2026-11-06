#include "port/statpanel.h"

#include <algorithm>
#include <cstdlib>

#include "port/icons.h"
#include "port/pockets.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kRowFill = 0xFF6A72B8;                 // (Interact's rows)
constexpr u32 kButtonFill = 0xFF9AA2E3, kButtonFocusFill = 0xFFB4BBF0;
constexpr u32 kTabPlateFill = 0xFF9AA2E3;            // (the panel's selected tab)
constexpr u32 kValueColour = 0xFFFFE9A0;             // a warm white (the money's)
constexpr u32 kIconWell = 0xFF5961A2;
constexpr float kLineSize = 28, kLineMin = 18, kButtonSize = 28, kTextSize = 26, kTextMin = 18;

std::string PageName(const RomData& rom, int page) { return MenuWords(rom, page == 0 ? kStrStTitle : kStrStPage + page); }

// The game's placeholders @1..@9 filled with args, its '$' (the simoleon) drawn §.
std::string Fill(const std::string& t, const std::vector<std::string>& args)
{
    std::string out;
    for (size_t i = 0; i < t.size(); i++)
    {
        if (t[i] == '@' && i + 1 < t.size() && t[i + 1] >= '1' && t[i + 1] <= '9')
        {
            const size_t n = (size_t)(t[i + 1] - '1');
            if (n < args.size()) out += args[n];
            i++;
        }
        else if (t[i] == '$') out += "\xC2\xA7";
        else out += t[i];
    }
    return out;
}

// A line split as a receipt: the words up to its first placeholder (a '$' before it goes with the
// value) less their colon, and the rest filled.
void Split(const RomData& rom, int id, const std::vector<std::string>& args, std::string& label, std::string& value)
{
    const std::string t = rom.String(id);
    size_t at = t.find('@');
    if (at == std::string::npos) at = t.size();
    if (at > 0 && t[at - 1] == '$') at--;
    label = MenuText(t.substr(0, at));
    while (!label.empty() && (label.back() == ':' || label.back() == ' ')) label.pop_back();
    value = MenuText(Fill(t.substr(at), args));
    if (label.empty()) std::swap(label, value); // (a language whose value comes first: the line whole at the left)
}

std::string Lots(const RomData& rom, int v, bool lots) { return lots ? MenuWords(rom, kStrStLots) : GroupThousands(v); }

void Text(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, Weight w, u32 colour = 0)
{
    const FittedText f = FitLine(s, size, minSize, r.w, w);
    UiItem t = TextItem(r, f.text, f.size, a, w);
    t.wrap = false;
    t.middle = true;
    if (colour) t.colour = colour;
    t.layer = 1;
    c.ui.push_back(t);
}
// A receipt line's value width: its own (at most half the line).
int ValueWidth(const std::string& value, int w) { return value.empty() ? 0 : std::min(w / 2, (int)TextWidth(value, kLineSize, Weight::ExtraBold) + 4); }
// A label and its value in `r` (the label at the left, the value right-aligned in warm white).
void Receipt(Composition& c, Rect r, const std::string& label, const std::string& value)
{
    const int vw = ValueWidth(value, r.w);
    Text(c, {r.x, r.y, r.w - vw - (vw ? 16 : 0), r.h}, label, kLineSize, kLineMin, Align::Left, Weight::ExtraBold);
    if (vw) Text(c, {r.x + r.w - vw, r.y, vw, r.h}, value, kLineSize, kLineMin, Align::Right, Weight::ExtraBold, kValueColour);
}

struct Icon
{
    SpriteRef ref;
    int frame, scale;
    Rect box;
};
void AddIcon(Composition& c, IconCache* icons, const Icon& i)
{
    if (!icons) return;
    if (const SpriteFrame* f = icons->Scaled(i.ref, i.frame, i.scale))
        c.ui.push_back(ImageItem({i.box.x + (i.box.w - f->w) / 2, i.box.y + (i.box.h - f->h) / 2, f->w, f->h}, f->argb.data(), f->w, f->h, {0, 0, f->w, f->h}));
}
void Well(Composition& c, Rect r, u32 fill, float radius)
{
    UiItem w = PanelItem(r);
    w.plain = true;
    w.fill = fill;
    w.radius = radius;
    w.layer = 1;
    c.ui.push_back(w);
}

// The pages' rows inside kPmContent.
Rect HotelRow(int i) { return {kPmContent.x + 16, kPmContent.y + 8 + i * 72, kPmContent.w - 32, 64}; }
Rect StatsRow(int i) { return {kPmContent.x + 16, kPmContent.y + 8 + i * 90, kPmContent.w - 32, 82}; }
constexpr Rect kAspIcon {kPmContent.x + 16, kPmContent.y + 8, 140, 140};
constexpr Rect kAspName {kPmContent.x + 176, kPmContent.y + 8, kPmContent.w - 192, 60};
constexpr Rect kAspGauge {kPmContent.x + 176, kPmContent.y + 84, 432, 56};
constexpr Rect kAspStars {kPmContent.x + 640, kPmContent.y + 84, 224, 56};
constexpr Rect kAspBonus {kPmContent.x + 16, kPmContent.y + 160, kPmContent.w - 32, 96};
constexpr Rect kAspHow {kPmContent.x + 16, kPmContent.y + 268, kPmContent.w - 32, 96};
constexpr int kSkillBarW = 320;
}

Rect StatTabRect(int page)
{
    const int cellY = kPmTabBar.y + (kPmTabBar.h - kPmTabCell) / 2;
    return {kPmTabsX + std::clamp(page, 0, kStPages - 1) * kPmTabCell, cellY, kPmTabCell, kPmTabCell};
}

Rect StatButtonRect(int page)
{
    const int k = std::clamp(page, 1, 4) - 1;
    constexpr int w = 424, h = 176;
    return {kPmContent.x + 12 + (k % 2) * (kPmContent.w - 24 - w), kPmContent.y + 4 + (k / 2) * (h + 12), w, h};
}

MenuAction StatPageAction(const StatState& st, int to)
{
    MenuAction a;
    if (to < 0 || to >= kStPages || to == st.page) return a;
    // The old page still fading out takes the taps (its own X): wait until it has gone.
    if (st.transition == 1) a.push_back(WaitStep(kStFadeWait));
    if (st.page == 0) a.push_back(TapStep(kStButtonX[to], kStButtonY[to]));
    else
    {
        a.push_back(TapStep(kStCloseX, kStCloseY));
        if (to > 0)
        {
            a.push_back(WaitStep(kStSwitchWait));
            a.push_back(TapStep(kStButtonX[to], kStButtonY[to]));
        }
    }
    return a;
}

void StatHotelLine(const StatState& st, const RomData& rom, int i, std::string& label, std::string& value)
{
    const int net = st.earned - st.spent;
    switch (i)
    {
    case 0: Split(rom, kStrStHotel, {std::to_string(st.score)}, label, value); break;
    case 1: Split(rom, kStrStHotel + 1, {GroupThousands(st.guestsStayed)}, label, value); break;
    case 2: Split(rom, kStrStHotel + 2, {Lots(rom, st.earned, st.earned == 999999)}, label, value); break;
    case 3: Split(rom, kStrStHotel + 3, {Lots(rom, st.spent, st.spent == 999999)}, label, value); break;
    default: Split(rom, kStrStHotel + 4, {net < 0 ? "-" : "", GroupThousands(std::abs(net))}, label, value); break;
    }
}

void StatStatsLine(const StatState& st, const RomData& rom, int i, std::string& label, std::string& value)
{
    const int v[4] = {st.aliens, st.goons, st.vacuum, st.metal};
    const int k = std::clamp(i, 0, 3);
    Split(rom, kStrStStats + k, {Lots(rom, v[k], v[k] < 0 || v[k] > 2000000000)}, label, value);
}

MenuView StatPanelView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrStSkill + 4) return {};
    return StatPanelViewOf(s, *game, rom);
}

MenuView StatPanelViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const StatState& st = game.stat;
    if (s.screen != Screen::Unknown || !game.valid || !st.valid || st.page < 0 || st.page >= kStPages) return v;
    if (rom) v.title = PageName(*rom, st.page);
    if (st.page == 0)
    {
        for (int p = 1; p < kStPages; p++)
        {
            ViewItem it;
            it.rect = StatButtonRect(p);
            it.id = kStPageId + (u32)p;
            it.action = StatPageAction(st, p);
            if (rom) it.word = PageName(*rom, p);
            v.items.push_back(it);
        }
        v.back = st.transition == 1 ? MenuAction {WaitStep(kStFadeWait), TapStep(kStCloseX, kStCloseY)} : MenuAction {TapStep(kStCloseX, kStCloseY)};
        v.backCloses = true;
    }
    else
    {
        // The page's tab: where the focus rests (A does nothing), its id Main's button's for it.
        ViewItem it;
        it.rect = StatTabRect(st.page);
        it.id = kStPageId + (u32)st.page;
        for (int& n : it.next) n = kNoMove;
        v.items.push_back(it);
        v.back = StatPageAction(st, 0);
        v.buttons[(int)MenuButton::Left] = StatPageAction(st, st.page - 1);
        v.buttons[(int)MenuButton::Right] = StatPageAction(st, (st.page + 1) % kStPages);
    }
    v.buttons[(int)MenuButton::LB] = StatPageAction(st, (st.page + kStPages - 1) % kStPages);
    v.buttons[(int)MenuButton::RB] = StatPageAction(st, (st.page + 1) % kStPages);
    v.initial = 0;
    v.own = true;
    v.valid = true;
    return v;
}

void BuildStatPanel(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons)
{
    const Theme& th = GameTheme();
    const StatState& st = game.stat;
    const int page = st.page;
    if (page < 0 || page >= kStPages || v.items.empty()) return;
    const size_t first = c.ui.size();
    std::vector<Icon> images;

    // Layer 0: the panel, the tab bar, the title's well, the LB / RB chips.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem bar = WellItem(kPmTabBar);
    bar.fill = th.frameDark;
    bar.radius = 16;
    c.ui.push_back(bar);
    const Rect cell = StatTabRect(page);
    UiItem title = WellItem(kPmTitle);
    title.radius = 14;
    c.ui.push_back(title);
    {
        const Rect lb = ChipRect(0, 0, Button::LB, "", 26);
        c.ui.push_back(ChipItem(kPmTabsX - 14 - lb.w, kPmTabBar.y + (kPmTabBar.h - lb.h) / 2, Button::LB, "", 26));
        c.ui.push_back(ChipItem(kPmTabsX + kStPages * kPmTabCell + 14, kPmTabBar.y + (kPmTabBar.h - lb.h) / 2, Button::RB, "", 26));
    }

    // Layer 1: the X's cell, the title, the page, the focus, the hints.
    UiItem xCell = PanelItem(kStClose);
    xCell.fill = th.frameDark;
    xCell.border = 0;
    xCell.raised = false;
    xCell.radius = 14;
    xCell.layer = 1;
    c.ui.push_back(xCell);
    Text(c, {kPmTitle.x + 24, kPmTitle.y, kPmTitle.w - 48, kPmTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Centre, Weight::ExtraBold);
    // The page's raised plate in the tab bar (on a page the focus rests there: its halo first).
    if (page != 0 && focus == 0) AddFocusRing(c, {cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4}, 18);
    {
        UiItem plate = PanelItem({cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4});
        plate.fill = kTabPlateFill;
        plate.borderArgb = th.fill;
        plate.border = 3;
        plate.radius = 18;
        plate.layer = 1;
        c.ui.push_back(plate);
    }
    for (int p = 0; p < kStPages; p++)
    {
        // Main's own picture 3x (24 px), the pages' buttons 1x (72 x 56): each fills a cell.
        Icon i {SpriteRef::Of(kSprStNav[p]), 0, p == 0 ? 3 : 1, StatTabRect(p)};
        images.push_back(i);
    }

    if (page == 0)
    {
        for (int p = 1; p < kStPages; p++)
        {
            const Rect r = StatButtonRect(p);
            const bool focused = focus == p - 1;
            if (focused) AddFocusRing(c, r, 24);
            UiItem b = PanelItem(r);
            b.fill = focused ? kButtonFocusFill : kButtonFill;
            b.borderArgb = th.frame;
            b.border = 3;
            b.radius = 24;
            b.layer = 1;
            c.ui.push_back(b);
            images.push_back({SpriteRef::Of(kSprStNav[p]), 0, 2, {r.x, r.y + 8, r.w, 112}});
            Text(c, {r.x + 16, r.y + 124, r.w - 32, 44}, PageName(rom, p), kButtonSize, kLineMin, Align::Centre, Weight::ExtraBold);
            AddMousePiece(c, kStButtonFrom[p], r);
            AddMousePiece(c, kStButtonFrom[p], StatTabRect(p));
        }
    }
    else if (page == 1)
    {
        const int asp = std::clamp(st.aspiration, 0, 3), level = std::clamp(st.level, 0, 3), j = kStLevelPal[level];
        Well(c, kAspIcon, kIconWell, 20);
        images.push_back({SpriteRef::Of(kSprStAspiration), asp, 2, kAspIcon});
        Text(c, kAspName, MenuWords(rom, kStrStAspName + asp), 44, 26, Align::Left, Weight::ExtraBold);
        images.push_back({{kSprStGauge - 1, kSprStGauge, kStGaugePal[j]}, std::clamp(st.gaugeFrame, 0, 20), 3, kAspGauge});
        images.push_back({{kSprStStars - 1, kSprStStars, kStStarsPal[j]}, level, 3, kAspStars});
        for (auto [r, id] : {std::pair {kAspBonus, kStrStAspBonus + asp}, std::pair {kAspHow, kStrStAspHow + asp}})
        {
            Well(c, r, kRowFill, 16);
            const Rect in {r.x + 20, r.y + 8, r.w - 40, r.h - 16};
            const FittedBlock f = FitBlock(MenuWords(rom, id), kTextSize, kTextMin, in);
            UiItem t = TextItem(in, f.text, f.size, Align::Left, Weight::Bold);
            t.middle = true;
            t.layer = 1;
            c.ui.push_back(t);
        }
    }
    else if (page == 2 || page == 4)
    {
        const int n = page == 2 ? 5 : 4;
        for (int i = 0; i < n; i++)
        {
            const Rect r = page == 2 ? HotelRow(i) : StatsRow(i);
            Well(c, r, kRowFill, 18);
            const Rect icon {r.x + 8, r.y + (r.h - 64) / 2, 64, 64};
            images.push_back({SpriteRef::Of(page == 2 ? kSprStHotel[i] : kSprStStats[i]), 0, 2, icon});
            std::string label, value;
            if (page == 2) StatHotelLine(st, rom, i, label, value);
            else StatStatsLine(st, rom, i, label, value);
            Receipt(c, {r.x + 88, r.y, r.w - 88 - 20, r.h}, label, value);
        }
    }
    else
    {
        for (int i = 0; i < 5; i++)
        {
            const Rect r = HotelRow(i);
            Well(c, r, kRowFill, 18);
            images.push_back({SpriteRef::Of(kSprStSkill[i]), 0, 2, {r.x + 8, r.y, 64, 64}});
            const Rect barBox {r.x + r.w - 16 - kSkillBarW, r.y, kSkillBarW, r.h};
            Text(c, {r.x + 88, r.y, barBox.x - 16 - (r.x + 88), r.h}, MenuWords(rom, kStrStSkill + i), kLineSize, kLineMin, Align::Left, Weight::ExtraBold);
            images.push_back({SpriteRef::Of(kSprStSkillBar[i]), std::clamp(st.skills[i], 0, 10), 2, barBox});
        }
    }
    // The X: closes from Main, back to Main from a page (and the Main tab is that X too).
    AddMousePiece(c, kStCloseFrom, kStClose);
    if (page != 0) AddMousePiece(c, kStCloseFrom, StatTabRect(0));

    // The hints: B "Close" on Main (the page's name, Main's, on a page: back there), A the focused
    // button's page; on a page the D-pad's Left / Right (the pages).
    int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, page == 0 ? MenuWords(rom, kStrClose) : PageName(rom, 0));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) right = AddHint(c, right, Button::A, v.items[focus].word);
    if (page != 0) AddHint(c, right, Button::DPadH, "");

    for (const Icon& i : images) AddIcon(c, icons, i);
    AddIcon(c, icons, {SpriteRef::Of(kSprStX), 0, 2, kStClose});
    SortUi(c.ui, first);
}

void MeasureStatPanel(const RomData& rom, BlockMeasure& m)
{
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) {
        m.checked++;
        if (shrunk) m.shrunk++;
        if (lines > m.tallestLines) { m.tallestLines = lines; m.tallestId = id; }
        if (!fits && std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) { m.cut++; m.cutIds.push_back(id); }
    };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) {
        const FittedText f = FitLine(text, size, minSize, width, w);
        note(id, f.size < size, f.fits);
    };
    auto block = [&](int id, const std::string& text, float size, float minSize, Rect box) {
        const FittedBlock f = FitBlock(text, size, minSize, box);
        note(id, f.size < size, f.fits, f.lines);
    };
    auto receipt = [&](int id, const std::string& label, const std::string& value, int w) {
        const int vw = ValueWidth(value, w);
        line(id, label, kLineSize, kLineMin, w - vw - (vw ? 16 : 0));
        if (vw) line(id, value, kLineSize, kLineMin, vw);
    };
    for (int p = 0; p < kStPages; p++) line(p == 0 ? kStrStTitle : kStrStPage + p, PageName(rom, p), kPmTitleSize, kPmTextMin, kPmTitle.w - 48);
    for (int p = 1; p < kStPages; p++) line(kStrStPage + p, PageName(rom, p), kButtonSize, kLineMin, StatButtonRect(p).w - 32);
    for (int a = 0; a < 4; a++)
    {
        line(kStrStAspName + a, MenuWords(rom, kStrStAspName + a), 44, 26, kAspName.w);
        block(kStrStAspBonus + a, MenuWords(rom, kStrStAspBonus + a), kTextSize, kTextMin, {0, 0, kAspBonus.w - 40, kAspBonus.h - 16});
        block(kStrStAspHow + a, MenuWords(rom, kStrStAspHow + a), kTextSize, kTextMin, {0, 0, kAspHow.w - 40, kAspHow.h - 16});
    }
    // The lines beside their widest values: the largest numbers the game prints, and LOTS!.
    const int rw = HotelRow(0).w - 88 - 20;
    StatState big;
    big.score = 100; big.guestsStayed = 99999; big.earned = 999998; big.spent = 999998;
    big.aliens = big.goons = big.vacuum = big.metal = 2000000000;
    StatState lots = big;
    lots.earned = lots.spent = 999999; lots.aliens = lots.goons = lots.vacuum = lots.metal = -1;
    StatState loss = big;
    loss.earned = 0;
    for (const StatState* s : {&big, &lots, &loss})
    {
        std::string label, value;
        for (int i = 0; i < 5; i++) { StatHotelLine(*s, rom, i, label, value); receipt(kStrStHotel + i, label, value, rw); }
        for (int i = 0; i < 4; i++) { StatStatsLine(*s, rom, i, label, value); receipt(kStrStStats + i, label, value, rw); }
    }
    const Rect r = HotelRow(0);
    for (int i = 0; i < 5; i++) line(kStrStSkill + i, MenuWords(rom, kStrStSkill + i), kLineSize, kLineMin, r.w - 16 - kSkillBarW - 16 - 88);
    line(kStrClose, MenuWords(rom, kStrClose), kPmHintSize, kPmHintSize, 300);
}
}
