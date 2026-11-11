#include "port/statpanel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "port/icons.h"
#include "port/ownwords.h"
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

constexpr u32 kButtonFill = 0xFF9AA2E3, kButtonFocusFill = 0xFFB4BBF0;
constexpr u32 kTabPlateFill = 0xFF9AA2E3;            // (the panel's selected tab)
// The game's stat sprites are drawn for its white paper (their specks, rims and brush strokes
// glow on a purple well): the rows and the aspiration's picture sit on paper, in dark ink (10.8:1),
// the values a dark amber (6.4:1). The light buttons and the tab's plate take the ink too (5:1).
constexpr u32 kPaper = 0xFFF4F1E8, kInk = 0xFF2E3063, kValueInk = 0xFF7A4E00;
constexpr float kLineSize = 28, kLineMin = 18, kButtonSize = 28, kTextSize = 26, kTextMin = 18, kTabSize = 22, kTabMin = 18;

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
    PanelText(c, r, s, size, minSize, a, w, colour);
    if (colour == kInk || colour == kValueInk) c.ui.back().shadowArgb = 0; // (dark ink: no shadow)
}
// A receipt line's value width: its own (at most half the line).
int ValueWidth(const std::string& value, int w) { return value.empty() ? 0 : std::min(w / 2, (int)TextWidth(value, kLineSize, Weight::ExtraBold) + 4); }
// A label and its value in `r` (the label at the left, the value right-aligned), in ink on paper.
void Receipt(Composition& c, Rect r, const std::string& label, const std::string& value)
{
    const int vw = ValueWidth(value, r.w);
    Text(c, {r.x, r.y, r.w - vw - (vw ? 16 : 0), r.h}, label, kLineSize, kLineMin, Align::Left, Weight::ExtraBold, kInk);
    if (vw) Text(c, {r.x + r.w - vw, r.y, vw, r.h}, value, kLineSize, kLineMin, Align::Right, Weight::ExtraBold, kValueInk);
}

// A game picture: a small one (at most ui::kTierMaxSource) at its tier (ui::kTierS, kTierL), a
// larger one (the Main page's buttons, the gauge, the skills' bars) `scale` times its size, centred in box.
struct Icon
{
    SpriteRef ref;
    int frame, tier, scale;
    Rect box;
};
Icon Small(SpriteRef ref, int frame, int tier, Rect box) { return {ref, frame, tier, 0, box}; }
Icon Large(SpriteRef ref, int frame, int scale, Rect box) { return {ref, frame, 0, scale, box}; }
void AddIcon(Composition& c, IconCache* icons, const Icon& i)
{
    if (!icons) return;
    if (i.tier > 0) { AddTiered(c.ui, icons, i.ref, i.frame, i.tier, i.box); return; }
    if (const SpriteFrame* f = icons->Scaled(i.ref, i.frame, i.scale))
        c.ui.push_back(icons->Image(i.ref, i.frame, *f, {i.box.x + (i.box.w - f->w) / 2, i.box.y + (i.box.h - f->h) / 2, f->w, f->h}));
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

// A tab's name in `box`, two lines at most: the largest size from `size` down to `minSize` at which
// no word is wider than the box (a word is never broken) and the lines fit.
FittedBlock TabName(const std::string& name, float size, float minSize, Rect box)
{
    for (float s = size; s > minSize; s -= 1)
    {
        bool words = true;
        size_t a = 0;
        while (words && a <= name.size())
        {
            const size_t b = std::min(name.find(' ', a), name.size());
            words = TextWidth(name.substr(a, b - a), s, Weight::ExtraBold) <= box.w;
            a = b + 1;
        }
        if (words)
            if (const FittedBlock f = FitBlock(name, s, s, box); f.fits) return f;
    }
    return FitBlock(name, minSize, minSize, box);
}
// The pages' rows inside kPmContent.
Rect HotelRow(int i) { return {kPmContent.x + 16, kPmContent.y + 8 + i * 72, kPmContent.w - 32, 64}; }
Rect StatsRow(int i) { return {kPmContent.x + 16, kPmContent.y + 8 + i * 90, kPmContent.w - 32, 82}; }
constexpr Rect kAspIcon {kPmContent.x + 16, kPmContent.y + 8, 144, 144}; // (the 64-pixel picture at tier S, 8 of paper round it)
constexpr Rect kAspName {kPmContent.x + 176, kPmContent.y + 8, kPmContent.w - 192, 60};
constexpr Rect kAspGauge {kPmContent.x + 176, kPmContent.y + 84, 432, 56};
constexpr Rect kAspStars {kPmContent.x + 640, kPmContent.y + 84, 224, 56};
constexpr Rect kAspBonus {kPmContent.x + 16, kPmContent.y + 160, kPmContent.w - 32, 96};
constexpr Rect kAspHow {kPmContent.x + 16, kPmContent.y + 268, kPmContent.w - 32, 96};
constexpr int kSkillBarW = 320;
}

Rect StatTabRect(int page, const RomData* rom)
{
    constexpr int chips = 64, x0 = kPmTabBar.x + 16 + chips, x1 = kPmTabBar.x + kPmTabBar.w - 16 - chips, h = 56;
    int need[kStPages] {}, sum = 0;
    for (int p = 0; p < kStPages; p++)
    {
        // (a name on two lines at most: its widest word at the smallest size, a margin each side)
        if (rom)
        {
            const std::string name = PageName(*rom, p);
            size_t a = 0;
            while (a <= name.size())
            {
                const size_t b = std::min(name.find(' ', a), name.size());
                need[p] = std::max(need[p], (int)std::ceil(TextWidth(name.substr(a, b - a), kTabMin, Weight::ExtraBold)) + 22);
                a = b + 1;
            }
        }
        sum += need[p];
    }
    const int spare = std::max(0, x1 - x0 - sum) / kStPages;
    page = std::clamp(page, 0, kStPages - 1);
    int x = x0;
    for (int p = 0; p < page; p++) x += need[p] + spare;
    return {x + 3, kPmTabBar.y + (kPmTabBar.h - h) / 2, need[page] + spare - 6, h};
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
            // (running on until the game shows the new page: the page left stays drawn till then)
            a.push_back(WaitStep(kStShowWait));
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

// The stat panel as CONSOLE shows it: the game's, but while the pad goes from one page to another
// (by Main, which the game shows for about 48 frames between), the page it left (StatState::held).
StatState Shown(const StatState& s)
{
    StatState t = s;
    if (s.held > 0) { t.page = s.held; t.transition = 0; }
    return t;
}

MenuView StatPanelViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const StatState st = Shown(game.stat);
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
        it.rect = StatTabRect(st.page, rom);
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
    const StatState st = Shown(game.stat);
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
    const Rect cell = StatTabRect(page, &rom);
    UiItem title = WellItem(kPmTitle);
    title.radius = 14;
    c.ui.push_back(title);
    {
        const Rect lb = ChipRect(0, 0, Button::LB, "", 26), rb = ChipRect(0, 0, Button::RB, "", 26);
        c.ui.push_back(ChipItem(kPmTabBar.x + 16, kPmTabBar.y + (kPmTabBar.h - lb.h) / 2, Button::LB, "", 26));
        c.ui.push_back(ChipItem(kPmTabBar.x + kPmTabBar.w - 16 - rb.w, kPmTabBar.y + (kPmTabBar.h - rb.h) / 2, Button::RB, "", 26));
    }

    // Layer 1: the title (the X at its right end), the page, the focus, the hints.
    Text(c, {kPmTitle.x + 64, kPmTitle.y, kPmTitle.w - 128, kPmTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Centre, Weight::ExtraBold);
    // The page's raised plate in the tab bar (on a page the focus rests there: its halo first).
    if (page != 0 && focus == 0) AddFocusRing(c, cell, 18, 1, kTabPlateFill);
    {
        UiItem plate = PanelItem(cell);
        plate.fill = kTabPlateFill;
        plate.borderArgb = th.fill;
        plate.border = 3;
        plate.radius = 18;
        plate.layer = 1;
        c.ui.push_back(plate);
    }
    // (one size for every tab: the largest at which each name holds, words whole)
    float tabSize = kTabSize;
    for (int q = 0; q < kStPages; q++)
    {
        const Rect r = StatTabRect(q, &rom);
        tabSize = std::min(tabSize, TabName(PageName(rom, q), kTabSize, kTabMin, {r.x + 8, r.y + 2, r.w - 16, r.h - 4}).size);
    }
    for (int p = 0; p < kStPages; p++)
    {
        // The pages by their names (the game's pictures for them are a 24-pixel one and four 72 x
        // 56 brush strokes: no one size of pixel for the row), the page's own in ink on its plate.
        const Rect r = StatTabRect(p, &rom), in {r.x + 8, r.y + 2, r.w - 16, r.h - 4};
        const FittedBlock f = TabName(PageName(rom, p), tabSize, tabSize, in);
        UiItem t = TextItem(in, f.text, f.size, Align::Centre, Weight::ExtraBold);
        t.middle = true;
        if (p == page) { t.colour = kInk; t.shadowArgb = 0; }
        t.layer = 1;
        c.ui.push_back(t);
    }

    if (page == 0)
    {
        for (int p = 1; p < kStPages; p++)
        {
            const Rect r = StatButtonRect(p);
            const bool focused = focus == p - 1;
            if (focused) AddFocusRing(c, r, 24, 1, kButtonFocusFill);
            UiItem b = PanelItem(r);
            b.fill = focused ? kButtonFocusFill : kButtonFill;
            b.borderArgb = th.frame;
            b.border = 3;
            b.radius = 24;
            b.layer = 1;
            c.ui.push_back(b);
            images.push_back(Large(SpriteRef::Of(kSprStNav[p]), 0, 2, {r.x, r.y + 8, r.w, 112}));
            Text(c, {r.x + 16, r.y + 124, r.w - 32, 44}, PageName(rom, p), kButtonSize, kLineMin, Align::Centre, Weight::ExtraBold, kInk);
            AddMousePiece(c, kStButtonFrom[p], r);
            AddMousePiece(c, kStButtonFrom[p], StatTabRect(p, &rom));
        }
    }
    else if (page == 1)
    {
        const int asp = std::clamp(st.aspiration, 0, 3), level = std::clamp(st.level, 0, 3), j = kStLevelPal[level];
        Well(c, kAspIcon, kPaper, 20);
        images.push_back(Small(SpriteRef::Of(kSprStAspiration), kStAspFrame[asp], kTierS, kAspIcon));
        Text(c, kAspName, MenuWords(rom, kStrStAspName + asp), 44, 26, Align::Left, Weight::ExtraBold);
        images.push_back(Large({kSprStGauge - 1, kSprStGauge, kStGaugePal[j]}, std::clamp(st.gaugeFrame, 0, 20), 3, kAspGauge));
        images.push_back(Small({kSprStStars - 1, kSprStStars, kStStarsPal[j]}, level, kTierL, kAspStars));
        for (auto [r, id] : {std::pair {kAspBonus, kStrStAspBonus + asp}, std::pair {kAspHow, kStrStAspHow + asp}})
        {
            Well(c, r, kFillRow, 16);
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
            Well(c, r, kPaper, 18);
            const Rect icon {r.x + 8, r.y + (r.h - 64) / 2, 64, 64};
            images.push_back(Small(SpriteRef::Of(page == 2 ? kSprStHotel[i] : kSprStStats[i]), 0, kTierS, icon));
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
            Well(c, r, kPaper, 18);
            images.push_back(Small(SpriteRef::Of(kSprStSkill[i]), 0, kTierS, {r.x + 8, r.y, 64, 64}));
            const Rect barBox {r.x + r.w - 16 - kSkillBarW, r.y, kSkillBarW, r.h};
            Text(c, {r.x + 88, r.y, barBox.x - 16 - (r.x + 88), r.h}, MenuWords(rom, kStrStSkill + i), kLineSize, kLineMin, Align::Left, Weight::ExtraBold, kInk);
            images.push_back(Large(SpriteRef::Of(kSprStSkillBar[i]), std::clamp(st.skills[i], 0, 10), 2, barBox));
        }
    }
    // The X: closes from Main, back to Main from a page (and the Main tab is that X too).
    AddMousePiece(c, kStCloseFrom, kStClose);
    if (page != 0) AddMousePiece(c, kStCloseFrom, StatTabRect(0, &rom));

    // The hints: B "Close" on Main ("Back" on a page: back there, a level up; the game's table has
    // no "Back", the pause menu's word), A the focused button's page; the D-pad: on Main its cross
    // moves the focus over the four buttons ("Choose"), on a page its Left / Right the pages.
    int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, page == 0 ? MenuWords(rom, kStrClose) : std::string(OwnWord(&rom, OwnWordId::ShellBack)));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) right = AddHint(c, right, Button::A, v.items[focus].word);
    if (page != 0) AddHint(c, right, Button::DPadH, OwnWord(&rom, OwnWordId::HintPages));
    else AddHint(c, right, Button::DPad, OwnWord(&rom, OwnWordId::HintChoose));

    for (const Icon& i : images) AddIcon(c, icons, i);
    AddIcon(c, icons, Small(SpriteRef::Of(kSprStX), 0, kTierS, kStClose));
    SortUi(c.ui, first);
}

void MeasureStatPanel(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) { tally.Note(id, shrunk, fits, lines); };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) { tally.Line(id, text, size, minSize, width, w); };
    auto block = [&](int id, const std::string& text, float size, float minSize, Rect box) { tally.Block(id, text, size, minSize, box); };
    auto receipt = [&](int id, const std::string& label, const std::string& value, int w) {
        const int vw = ValueWidth(value, w);
        line(id, label, kLineSize, kLineMin, w - vw - (vw ? 16 : 0));
        if (vw) line(id, value, kLineSize, kLineMin, vw);
    };
    for (int p = 0; p < kStPages; p++) line(p == 0 ? kStrStTitle : kStrStPage + p, PageName(rom, p), kPmTitleSize, kPmTextMin, kPmTitle.w - 128);
    for (int p = 1; p < kStPages; p++) line(kStrStPage + p, PageName(rom, p), kButtonSize, kLineMin, StatButtonRect(p).w - 32);
    for (int p = 0; p < kStPages; p++)   // (the tabs: two lines at most)
    {
        const Rect r = StatTabRect(p, &rom);
        block(p == 0 ? kStrStTitle : kStrStPage + p, PageName(rom, p), kTabSize, kTabMin, {0, 0, r.w - 16, r.h - 4});
    }
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
