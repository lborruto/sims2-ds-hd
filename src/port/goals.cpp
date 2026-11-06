#include "port/goals.h"

#include <algorithm>

#include "port/ownwords.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kMissionMark = 0xFF6BD36B;  // the game's bullet: a light green dot
constexpr u32 kRequestMark = 0xFFB2B2E3;  // the game's light lavender
constexpr u32 kBarFill = 0xFF6A72B8, kBarSelFill = 0xFF9AA2E3;
constexpr u32 kTrack = 0xFF51518A, kThumb = 0xFFB2B2E3;

// The goals' texts and heights as the HD list lays them out (at kGoalsSize, shrunk all together
// to kGoalsMin when the selected goal alone does not fit the well).
struct GoalBox { std::string text; int lines = 1; int h = 0; };

int TextW(Rect list) { return list.w - 2 * kGoalsPad - kGoalsMark - 18 - 12; } // (the scroll bar's margin)

std::vector<GoalBox> Boxes(const GoalsState& g, const RomData* rom, Rect list, float& size)
{
    std::vector<GoalBox> b(g.entries.size());
    for (size = kGoalsSize;; size -= 1)
    {
        const float lh = Metrics(size).lineHeight;
        int tallest = 0;
        for (size_t i = 0; i < g.entries.size(); i++)
        {
            b[i].text = rom ? GoalText(*rom, g.entries[i]) : std::string();
            b[i].lines = rom ? std::max(1, (int)Wrap(b[i].text, size, (float)TextW(list)).size()) : std::max(1, g.entries[i].nLines);
            b[i].h = (int)(b[i].lines * lh + 0.999f) + 16;
            tallest = std::max(tallest, b[i].h);
        }
        if (tallest <= list.h - 2 * kGoalsPad || size <= kGoalsMin) return b;
    }
}

// The goals the HD list shows: from the goal on the game's first visible line, moved on until the
// selected one fits; [first, last).
void Window(const GoalsState& g, const std::vector<GoalBox>& b, Rect list, int& first, int& last)
{
    const int n = (int)b.size(), room = list.h - 2 * kGoalsPad;
    first = 0;
    for (int i = 0; i < n; i++)
        if (g.first >= g.entries[i].firstLine && g.first < g.entries[i].firstLine + std::max(1, g.entries[i].nLines)) first = i;
    auto end = [&](int from) {
        int y = 0, i = from;
        for (; i < n && y + b[i].h <= room; i++) y += b[i].h + kGoalsGap;
        return std::max(i, std::min(from + 1, n));
    };
    if (g.selected >= 0 && g.selected < first) first = g.selected;
    last = end(first);
    while (g.selected >= last && first < g.selected) last = end(++first);
}

Rect ListRect(const GoalsState& g) { return g.footer ? kGoalsListFooter : kGoalsList; }
}

std::string GoalText(const RomData& rom, const GoalEntry& e)
{
    std::string f = rom.String(e.textId >= 0 ? e.textId : 0);
    if (!e.mission)
    {
        // A request: @1 the Sim's name, @2 the place (the rest dropped by MenuText).
        auto fill = [&](const char* at, int id) {
            const std::string w = id >= 0 ? MenuWords(rom, id) : std::string();
            for (size_t p = 0; (p = f.find(at, p)) != std::string::npos; p += w.size()) f.replace(p, 2, w);
        };
        fill("@1", e.nameId);
        fill("@2", e.placeId);
    }
    std::string s = MenuText(f), out;
    // The game's '$' is its simoleon glyph ("Pay GIUSEPPI MEZZOALTO $250.").
    for (char ch : s) out += ch == '$' ? std::string("\xC2\xA7") : std::string(1, ch);
    return out;
}

void AddGoalsView(MenuView& v, const GoalsState& g, const RomData* rom)
{
    if (rom) v.title = MenuTitle(*rom, 2, "");
    const Rect list = ListRect(g);
    float size = kGoalsSize;
    const std::vector<GoalBox> b = Boxes(g, rom, list, size);
    int first = 0, last = 0;
    Window(g, b, list, first, last);
    const int base = (int)v.items.size();
    int y = list.y + kGoalsPad;
    for (int i = first; i < last; i++)
    {
        const GoalEntry& e = g.entries[i];
        ViewItem it;
        it.rect = {list.x + kGoalsPad, y, list.w - 2 * kGoalsPad - 18, b[i].h};
        it.group = 1;
        it.id = kGoalsId + (e.mission ? 0 : 0x100) + (u32)std::max(0, e.id);
        // No action: a goal is only read. (Without the game's selection, A selects it by its
        // first line on the touch screen while the game shows it.)
        const int line = e.firstLine - g.first;
        if (g.selected < 0 && line >= 0 && line < kGoalsLinesShown) it.action = {TapStep(kGoalsLineX + kGoalsLineW / 2, kGoalsLineY0 + kGoalsLineH * line + kGoalsLineH / 2)};
        v.items.push_back(it);
        y += b[i].h + kGoalsGap;
    }
    if (g.selected >= first && g.selected < last) v.selected = base + g.selected - first;
    v.scrollGroup = 1;
    // (The game's arrow is not there at the list's ends: none, so Up from the first goal goes to
    // the phone.)
    if (g.selected != 0) v.scrollUp = {TapStep(kGoalsUpX, kGoalsUpY)};
    if (g.selected < 0 || g.selected + 1 < (int)g.entries.size()) v.scrollDown = {TapStep(kGoalsDownX, kGoalsDownY)};
}

void AddScrollBar(Composition& c, Rect track, int first, int shown, int count)
{
    if (count <= shown || count <= 0) return;
    UiItem t = PanelItem(track);
    t.plain = true;
    t.fill = kTrack;
    t.radius = track.w / 2.0f;
    t.layer = 1;
    c.ui.push_back(t);
    const int h = std::max(track.w * 2, track.h * shown / count);
    const int y = track.y + (track.h - h) * first / std::max(1, count - shown);
    UiItem th = PanelItem({track.x, y, track.w, h});
    th.plain = true;
    th.fill = kThumb;
    th.radius = track.w / 2.0f;
    th.layer = 1;
    c.ui.push_back(th);
}

void BuildGoals(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, const u32* bottom)
{
    const Theme& th = GameTheme();
    const GoalsState& g = game.town.goals;
    const Rect list = ListRect(g);
    float size = kGoalsSize;
    const std::vector<GoalBox> b = Boxes(g, &rom, list, size);
    // The goals the view shows (its items before the phone): from the entry with its first id.
    int shown = 0;
    while (shown < (int)v.items.size() && v.items[shown].id >= kGoalsId && v.items[shown].id < kGoalsId + 0x200) shown++;
    int first = 0;
    for (int i = 0; shown > 0 && i < (int)g.entries.size(); i++)
        if (kGoalsId + (g.entries[i].mission ? 0u : 0x100u) + (u32)std::max(0, g.entries[i].id) == v.items[0].id) { first = i; break; }
    const int last = std::min(first + shown, (int)g.entries.size());

    // Layer 0: the portrait's well (the game's colour behind the live picture), the list's, the footer's.
    UiItem pw = WellItem(kGoalsPortrait);
    pw.fill = th.frameDark;
    pw.radius = 16;
    c.ui.push_back(pw);
    UiItem lw = WellItem(list);
    lw.radius = 16;
    c.ui.push_back(lw);
    if (g.footer)
    {
        UiItem fw = WellItem(kGoalsFooter);
        fw.radius = 16;
        c.ui.push_back(fw);
    }

    // Layer 1: the goals (bar, mark, text), the scroll bar, the footer's words.
    for (int i = first, k = 0; i < last && k < (int)v.items.size(); i++, k++)
    {
        const Rect r = v.items[k].rect;
        const bool sel = i == g.selected;
        if (k == focus) AddFocusRing(c, r, 14);
        UiItem bar = PanelItem(r);
        bar.fill = sel ? kBarSelFill : kBarFill;
        bar.borderArgb = sel ? th.fill : th.frame;
        bar.border = 2;
        bar.radius = 14;
        bar.raised = sel;
        bar.layer = 1;
        c.ui.push_back(bar);
        const float lh = Metrics(size).lineHeight;
        UiItem mark = PanelItem({r.x + 12, r.y + 8 + (int)(lh / 2) - 7, 14, 14});
        mark.plain = true;
        mark.fill = g.entries[i].mission ? kMissionMark : kRequestMark;
        mark.radius = 7;
        mark.layer = 1;
        c.ui.push_back(mark);
        UiItem t = TextItem({r.x + kGoalsMark + 8, r.y + 8, TextW(list), r.h - 16}, b[i].text, size, Align::Left, Weight::Bold);
        t.layer = 1;
        c.ui.push_back(t);
        // The mouse: the goal's lines the touch screen shows.
        const int l0 = std::max(g.entries[i].firstLine, g.first), l1 = std::min(g.entries[i].firstLine + g.entries[i].nLines, g.first + kGoalsLinesShown);
        if (l1 > l0) AddMousePiece(c, {kGoalsLineX, kGoalsLineY0 + kGoalsLineH * (l0 - g.first), kGoalsLineW, kGoalsLineH * (l1 - l0)}, r);
    }
    AddScrollBar(c, {list.x + list.w - kGoalsPad - 10, list.y + kGoalsPad, 10, list.h - 2 * kGoalsPad}, first, last - first, (int)g.entries.size());
    // No goal: the port's words in the list's well (the game's table has none for it).
    if (g.entries.empty())
        PanelText(c, {list.x + 24, list.y, list.w - 48, list.h}, OwnWord(&rom, OwnWordId::NoGoals), kGoalsSize, 20, Align::Centre, Weight::ExtraBold);
    if (g.footer)
    {
        const Rect f {kGoalsFooter.x + 20, kGoalsFooter.y, kGoalsFooter.w - 40, kGoalsFooter.h};
        const std::string score = MenuWordsWith(rom, kStrFooterScore, std::to_string(g.score));
        const FittedText s = FitLine(score, 26, 20, f.w / 3, Weight::ExtraBold);
        const int sw = (int)TextWidth(s.text, s.size, Weight::ExtraBold) + 4;
        UiItem st = TextItem({f.x + f.w - sw, f.y, sw, f.h}, s.text, s.size, Align::Right, Weight::ExtraBold);
        st.wrap = false;
        st.middle = true;
        st.layer = 1;
        c.ui.push_back(st);
        const FittedText w = FitLine(MenuWords(rom, g.footerId >= 0 ? g.footerId : kStrFooterImprove), 26, 20, f.w - sw - 24, Weight::ExtraBold);
        UiItem wt = TextItem({f.x, f.y, f.w - sw - 24, f.h}, w.text, w.size, Align::Left, Weight::ExtraBold);
        wt.wrap = false;
        wt.middle = true;
        wt.layer = 1;
        c.ui.push_back(wt);
    }
    // The giver's portrait (live, 3x) while the game lists a goal.
    if (bottom && !g.entries.empty())
    {
        const int w = kGoalsPortraitFrom.w * 3, h = kGoalsPortraitFrom.h * 3;
        c.ui.push_back(ImageItem({kGoalsPortrait.x + (kGoalsPortrait.w - w) / 2, kGoalsPortrait.y + (kGoalsPortrait.h - h) / 2, w, h}, bottom, SW, SH,
                                 kGoalsPortraitFrom));
    }
}

void MeasureGoals(const RomData& rom, BlockMeasure& m)
{
    {
        // No goal: the port's words in the list's well (as BuildGoals: one line, 24 px in from each side).
        const FittedText f = FitLine(OwnWord(&rom, OwnWordId::NoGoals), kGoalsSize, 20, kGoalsList.w - 48, Weight::ExtraBold);
        m.checked++;
        if (f.size < kGoalsSize) m.shrunk++;
        if (!f.fits) { m.cut++; m.cutIds.push_back(9000 + (int)OwnWordId::NoGoals); }
    }
    // Each goal alone in the list (with the footer: the smaller well) at the smallest size.
    auto block = [&](int id, const std::string& text) {
        const float lh = Metrics(kGoalsMin).lineHeight;
        const int lines = (int)Wrap(text, kGoalsSize, (float)TextW(kGoalsListFooter)).size();
        const int linesMin = (int)Wrap(text, kGoalsMin, (float)TextW(kGoalsListFooter)).size();
        const bool fits = linesMin * lh + 16 <= kGoalsListFooter.h - 2 * kGoalsPad;
        m.checked++;
        if (lines * Metrics(kGoalsSize).lineHeight + 16 > kGoalsListFooter.h - 2 * kGoalsPad) m.shrunk++;
        if (!fits) { m.cut++; m.cutIds.push_back(id); }
        if (lines > m.tallestLines) { m.tallestLines = lines; m.tallestId = id; }
    };
    for (int id = 0; id < 34; id++)
    {
        GoalEntry e;
        e.mission = true;
        e.id = id;
        e.textId = kStrGoalFirst + id;
        block(e.textId, GoalText(rom, e));
    }
    // The requests (2110-2128: their texts, the longest Sim and place names filled).
    for (int id = 2110; id <= 2128; id++)
    {
        GoalEntry e;
        e.textId = id;
        e.nameId = 324; // (Emperor Xizzle, a long one)
        e.placeId = 0x43;
        block(id, GoalText(rom, e));
    }
    for (int id : {kStrFooterImprove, kStrFooterAmazing})
    {
        const FittedText f = FitLine(MenuWords(rom, id), 26, 20, (kGoalsFooter.w - 40) * 2 / 3 - 24, Weight::ExtraBold);
        m.checked++;
        if (f.size < 26) m.shrunk++;
        if (!f.fits) { m.cut++; m.cutIds.push_back(id); }
    }
    const FittedText s = FitLine(MenuWordsWith(rom, kStrFooterScore, "100"), 26, 20, (kGoalsFooter.w - 40) / 3, Weight::ExtraBold);
    m.checked++;
    if (!s.fits) { m.cut++; m.cutIds.push_back(kStrFooterScore); }
}
}
