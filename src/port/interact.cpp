#include "port/interact.h"

#include <algorithm>
#include <cctype>

#include "port/icons.h"
#include "port/ownwords.h"
#include "port/panelmenu.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

// The rows' look: the game's row bar (its frame colour), lighter when focused; the arrow is the
// game's own ▶ (the Interact tab's icon, frame 0: blue, green when focused, as the game lights
// the highlighted row's arrow).
constexpr u32 kGreyShadow = 0x40383869;
constexpr int kArrowIcon = 48;      // the ▶, 2x
constexpr u32 kPlateRim = 0xFFB2B2E3; // the game's light lavender

Rect RowRect(int i) { return {kInteractRowX, kInteractRowY + i * (kInteractRowH + kInteractRowGap), kInteractRowW, kInteractRowH}; }

// The strip's boxes: the current target's plate in the middle, the previous / next names beside
// it (the hints' D-pad chip says Left / Right switch: "Other target").
constexpr int kStripCurW = 340;
struct StripBoxes { Rect cur, prev, next; };
StripBoxes Strip()
{
    const int cx = kInteractStrip.x + kInteractStrip.w / 2, sy = kInteractStrip.y, sh = kInteractStrip.h;
    StripBoxes b;
    b.cur = {cx - kStripCurW / 2, sy, kStripCurW, sh};
    const int lx = kInteractStrip.x + 18;
    b.prev = {lx, sy, cx - kStripCurW / 2 - 16 - lx, sh};
    const int rx = cx + kStripCurW / 2 + 16;
    b.next = {rx, sy, kInteractStrip.x + kInteractStrip.w - 18 - rx, sh};
    return b;
}
// A neighbour's name beside the plate: one line from 24 px down to 18, else two lines at 18
// (the strip holds two), else cut with an ellipsis.
struct SideName { std::string text; float size; bool wrap, fits; };
SideName FitSide(const std::string& name, int width, float size = 24, Weight w = Weight::Bold)
{
    const FittedText one = FitLine(name, size, 18, width, w);
    if (one.fits) return {one.text, one.size, false, true};
    if (Wrap(name, 18, (float)width, w).size() <= 2) return {name, 18, true, true};
    return {one.text, one.size, false, false};
}
// The row label's box (after the row's rounded end, before the arrow).
constexpr int kLabelInset = 28;
int LabelW() { return kInteractRowW - kLabelInset - kArrowIcon - 28; }
}

std::string WithoutAmount(std::string raw)
{
    const size_t at = raw.find("@1");
    if (at == std::string::npos) return raw;
    const size_t open = raw.rfind('(', at), close = raw.find(')', at);
    if (open == std::string::npos || close == std::string::npos) return raw;
    size_t from = open;
    while (from > 0 && raw[from - 1] == ' ') from--;
    raw.erase(from, close + 1 - from);
    return raw;
}

std::string InteractLabel(const RomData& rom, int labelId, int amount)
{
    if (labelId < 0) return {};
    // An amount of 0 (a zeroed table entry) is not shown: "Pay" and not "Pay (§0)".
    if (amount == 0) return MenuText(WithoutAmount(rom.String(labelId)));
    return MenuWords(rom, labelId, amount);
}

void AddInteractView(MenuView& v, const InteractState& in, const RomData* rom)
{
    if (rom)
        v.title = MenuTitle(*rom, 0, in.target.kind != TargetRef::None && in.target.nameId >= 0 ? MenuWords(*rom, in.target.nameId) : "");
    const int n = std::min((int)in.actions.size(), kInteractRows);
    for (int i = 0; i < n; i++)
    {
        ViewItem it;
        it.rect = RowRect(i);
        it.enabled = !in.actions[i].greyed;
        it.action = {TapStep(kInteractArrowX, kInteractArrowY0 + i * kInteractArrowStep)};
        it.group = 1;
        it.id = kInteractRowId + ((u32)in.actions[i].code << 4) + (u32)i;
        it.closesMenu = InteractAnimation(in.actions[i].code);
        if (rom) it.word = InteractLabel(*rom, in.actions[i].labelId, in.actions[i].amount);
        // Left / Right are the strip's targets; with one target, nothing (never the phone by the
        // geometry: it is Up from the first row).
        it.next[2] = it.next[3] = kNoMove;
        v.items.push_back(it);
    }
    if (in.prev.kind != TargetRef::None) v.buttons[(int)MenuButton::Left] = {TapStep(kInteractPrevX, kInteractStripY)};
    if (in.next.kind != TargetRef::None) v.buttons[(int)MenuButton::Right] = {TapStep(kInteractNextX, kInteractStripY)};
}

void BuildInteract(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons,
                   const u32* bottom)
{
    const Theme& th = GameTheme();
    const InteractState& in = game.town.interact;

    // Layer 0: the portrait's well and the strip's (in the game's own panel colour, so the live
    // picture's background meets it).
    UiItem well = WellItem(kInteractPortrait);
    well.fill = th.frameDark;
    well.radius = 14;
    c.ui.push_back(well);
    UiItem strip = WellItem(kInteractStrip);
    strip.radius = 14;
    c.ui.push_back(strip);

    // The rows: focus ring, bar, label; greyed ones dim.
    const int n = std::min((int)in.actions.size(), kInteractRows);
    for (int i = 0; i < n && i < (int)v.items.size(); i++)
    {
        const Rect r = v.items[i].rect;
        const bool focused = i == focus && v.items[i].enabled, greyed = !v.items[i].enabled;
        if (focused) AddFocusRing(c, r, r.h / 2.0f);
        UiItem row = PanelItem(r);
        row.fill = focused ? kFillFocus : kFillRow;
        row.borderArgb = th.frame;
        row.border = 3;
        row.radius = r.h / 2.0f;
        row.raised = !greyed;
        row.layer = 1;
        c.ui.push_back(row);
        const int textX = r.x + kLabelInset, textW = LabelW();
        const FittedText t = FitLine(v.items[i].word, kPmTextSize, kPmTextMin, textW, Weight::ExtraBold);
        UiItem label = TextItem({textX, r.y, textW, r.h}, t.text, t.size, Align::Left, Weight::ExtraBold);
        label.wrap = false;
        label.middle = true;
        if (greyed) { label.colour = kTextInactive; label.shadowArgb = kGreyShadow; } // (3.23:1 on the row: inactive)
        label.layer = 1;
        c.ui.push_back(label);
        AddMousePiece(c, {kInteractArrowFrom.x, kInteractArrowFrom.y + i * kInteractArrowStep, kInteractArrowFrom.w, kInteractArrowFrom.h}, r);
    }

    // No action at all (no target, or one that offers none): the port's words in the rows' place.
    if (n == 0)
    {
        const Rect area {RowRect(0).x, RowRect(0).y, RowRect(0).w, RowRect(kInteractRows - 1).y + RowRect(0).h - RowRect(0).y};
        PanelText(c, area, OwnWord(&rom, OwnWordId::NoActions), kPmTextSize, kPmTextMin, Align::Centre, Weight::ExtraBold, 0xFFFFFFFF);
    }

    // The strip: the previous target at the left, the current one in the middle (on a plate with
    // a thin light rim), the next at the right.
    const StripBoxes sb = Strip();
    if (in.target.kind != TargetRef::None)
    {
        UiItem plate = PanelItem({sb.cur.x, sb.cur.y + 8, sb.cur.w, sb.cur.h - 16});
        plate.fill = th.frame;
        plate.borderArgb = kPlateRim; // (not green: only the focus is)
        plate.border = 2;
        plate.radius = (sb.cur.h - 16) / 2.0f;
        plate.layer = 1;
        c.ui.push_back(plate);
        const SideName t = FitSide(MenuWords(rom, in.target.nameId), sb.cur.w - 32, 26, Weight::ExtraBold);
        UiItem cur = TextItem({sb.cur.x + 16, sb.cur.y, sb.cur.w - 32, sb.cur.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
        cur.wrap = t.wrap;
        cur.middle = true;
        cur.layer = 1;
        c.ui.push_back(cur);
        AddMousePiece(c, kInteractCurFrom, sb.cur);
    }
    if (in.prev.kind != TargetRef::None || in.next.kind != TargetRef::None)
    {
        auto name = [&](const TargetRef& t, Rect box, Align a, Rect from) {
            if (t.kind == TargetRef::None) return;
            const SideName f = FitSide(MenuWords(rom, t.nameId), box.w);
            UiItem it = TextItem(box, f.text, f.size, a, Weight::Bold);
            it.wrap = f.wrap;
            it.middle = true;
            it.colour = kTextNote; // (small text on the strip's well: white, 4.58:1)
            it.layer = 1;
            c.ui.push_back(it);
            AddMousePiece(c, from, box);
        };
        name(in.prev, sb.prev, Align::Left, kInteractPrevFrom);
        name(in.next, sb.next, Align::Right, kInteractNextFrom);
    }

    // Images (above the layers): the arrows (the game's ▶), the portrait (live).
    if (icons)
        for (int i = 0; i < n && i < (int)v.items.size(); i++)
        {
            const bool focused = i == focus && v.items[i].enabled;
            const SpriteRef ref = SpriteRef::Of(focused ? kTabIconGreen : kTabIconBlue);
            if (icons->Frame(ref, 0))
            {
                // The panel's tab bar's tier (one for the ▶ wherever it is).
                const Rect r = v.items[i].rect;
                UiItem a = icons->Tiered(ref, 0, kTierS, {r.x + r.w - kArrowIcon - 10, r.y, kArrowIcon, r.h});
                if (!v.items[i].enabled) a.fill = 0x50000000; // greyed: faint
                c.ui.push_back(a);
            }
        }
    if (bottom)
    {
        const int w = kInteractPortraitFrom.w * 3, h = kInteractPortraitFrom.h * 3;
        c.ui.push_back(KeyedCrop(bottom, kInteractPortraitFrom, {kInteractPortrait.x + (kInteractPortrait.w - w) / 2, kInteractPortrait.y + (kInteractPortrait.h - h) / 2, w, h}));
    }
}
}

namespace sims2wide
{
MenuMeasure MeasureInteract(const RomData& rom)
{
    MenuMeasure m;
    auto unused = [&](int id) {
        const std::string t = rom.String(id);
        return t.empty() || (t.size() >= 3 && std::tolower((unsigned char)t[0]) == 'x' && std::tolower((unsigned char)t[1]) == 'x' &&
                             std::tolower((unsigned char)t[2]) == 'x') || t == "Dummy";
    };
    auto fit = [&](int box, int id, const std::string& text, float size, float minSize, int width, Weight w) {
        const FittedText f = FitLine(text, size, minSize, width, w);
        m.checked++;
        if (f.size < size) m.shrunk++;
        if (!f.fits)
        {
            m.cut++;
            m.cutIn[box]++;
            if (std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) m.cutIds.push_back(id);
        }
    };
    // The A hint's room: the hints' band less the B hint and the D-pad's "Other target" (as
    // BuildPanelMenu lays them out with a previous / next target).
    const Rect b = ChipRect(0, 0, Button::B, MenuWords(rom, kStrClose), kPmHintSize, 0, Weight::ExtraBold);
    const Rect d = ChipRect(0, 0, Button::DPadH, OwnWord(&rom, OwnWordId::PromptOther), kPmHintSize, 0, Weight::ExtraBold);
    const int hintW = kPmHints.w - 8 - b.w - 28 - 60 - d.w - 28;
    for (int id = 1845; id <= 1951; id++)
    {
        if (unused(id)) continue;
        // A verb with an amount, at the widest the game's money reaches.
        const std::string label = InteractLabel(rom, id, rom.String(id).find("@1") != std::string::npos ? 99999999 : -1);
        fit(0, id, label, kPmTextSize, kPmTextMin, LabelW(), Weight::ExtraBold);
        fit(1, id, label, kPmHintSize, kPmHintSize, hintW, Weight::ExtraBold);
        const float w = TextWidth(label, kPmTextSize, Weight::ExtraBold);
        if (w > m.widestW) { m.widestW = w; m.widestId = id; }
    }
    {
        // No action: the port's words over the rows' place (as BuildInteract).
        const Rect area {RowRect(0).x, RowRect(0).y, RowRect(0).w, RowRect(kInteractRows - 1).y + RowRect(0).h - RowRect(0).y};
        fit(0, 9000 + (int)OwnWordId::NoActions, OwnWord(&rom, OwnWordId::NoActions), kPmTextSize, kPmTextMin, area.w, Weight::ExtraBold);
    }
    const StripBoxes sb = Strip();
    std::vector<int> names {kSimNameUnknown};
    for (int id = kSimNameBase; id <= kSimNameLast; id++) names.push_back(id);
    for (int id = kItemNameBase; id <= kItemNameBase + kItemIdLast; id++) names.push_back(id);
    for (int id : names)
    {
        if (unused(id)) continue;
        const std::string name = MenuWords(rom, id);
        fit(2, id, MenuTitle(rom, 0, name), kPmTitleSize, kPmTextMin, kPmTitle.w - 48, Weight::ExtraBold);
        auto two = [&](int box, const SideName& f, float size) {
            m.checked++;
            if (f.size < size) m.shrunk++;
            if (f.fits) return;
            m.cut++;
            m.cutIn[box]++;
            if (std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) m.cutIds.push_back(id);
        };
        two(3, FitSide(name, sb.cur.w - 32, 26, Weight::ExtraBold), 26);
        two(4, FitSide(name, std::min(sb.prev.w, sb.next.w)), 24);
    }
    return m;
}
}
