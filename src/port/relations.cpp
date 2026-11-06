#include "port/relations.h"

#include <algorithm>

#include "port/goals.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kRowFill = 0xFF6A72B8, kRowSelFill = 0xFF9AA2E3;
constexpr u32 kLabel = 0xFFD6DAF6;      // a well's small label: a pale lavender
constexpr u32 kPlateRim = 0xFFB2B2E3;
constexpr int kRowPortrait = 68;        // a row's portrait well (the crop 2x inside)
constexpr float kRowName = 24, kRowNameMin = 18;

Rect RowRect(int i) { return {kRelRowX, 206 + i * (kRelRowH + kRelRowGap), kRelRowW, kRelRowH}; }
Rect RowPortrait(Rect r) { return {r.x + 8, r.y + (r.h - kRowPortrait) / 2, kRowPortrait, kRowPortrait}; }
Rect RowNameBox(Rect r) { return {r.x + 8 + kRowPortrait + 12, r.y + 4, r.w - kRowPortrait - 36, r.h - 8}; }

// A name in a row: one line from 24 px to 18, else two lines at 18 (the row holds two).
struct RowName { std::string text; float size; bool wrap, fits; };
RowName FitRowName(const std::string& name, int width)
{
    const FittedText one = FitLine(name, kRowName, kRowNameMin, width, Weight::ExtraBold);
    if (one.fits) return {one.text, one.size, false, true};
    if (Wrap(name, kRowNameMin, (float)width, Weight::ExtraBold).size() <= 2) return {name, kRowNameMin, true, true};
    return {one.text, one.size, false, false};
}

// A small well: its label above, the value below.
void AddPair(Composition& c, Rect r, const std::string& label, const std::string& value)
{
    const FittedText l = FitLine(label, 20, 16, r.w - 36, Weight::Bold);
    UiItem lt = TextItem({r.x + 18, r.y + 6, r.w - 36, 26}, l.text, l.size, Align::Left, Weight::Bold);
    lt.wrap = false;
    lt.middle = true;
    lt.colour = kLabel;
    lt.layer = 1;
    c.ui.push_back(lt);
    const FittedText v = FitLine(value, 28, 20, r.w - 36, Weight::ExtraBold);
    UiItem vt = TextItem({r.x + 18, r.y + 32, r.w - 36, r.h - 38}, v.text, v.size, Align::Left, Weight::ExtraBold);
    vt.wrap = false;
    vt.middle = true;
    vt.layer = 1;
    c.ui.push_back(vt);
}
}

void AddRelationsView(MenuView& v, const RelState& r, const RomData* rom)
{
    if (rom) v.title = MenuTitle(*rom, 3, "");
    const int base = (int)v.items.size();
    const int n = (int)r.sims.size();
    for (int i = 0; i < kRelRowsShown && r.first + i < n; i++)
    {
        const RelEntry& e = r.sims[r.first + i];
        ViewItem it;
        it.rect = RowRect(i);
        it.group = 1;
        it.id = kRelId + (u32)std::max(0, e.id);
        v.items.push_back(it); // (no action: the game's selection is moved by Up / Down)
    }
    if (r.selected >= r.first && r.selected < r.first + kRelRowsShown && r.selected < n) v.selected = base + r.selected - r.first;
    v.scrollGroup = 1;
    // (The game's arrow is not there at the list's ends: none, so Up from the first Sim goes to
    // the phone.)
    if (r.selected > 0) v.scrollUp = {TapStep(kRelPrevX, kRelPrevY)};
    if (r.selected + 1 < n) v.scrollDown = {TapStep(kRelNextX, kRelNextY)};
}

void BuildRelations(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, const u32* bottom)
{
    const Theme& th = GameTheme();
    const RelState& r = game.town.relations;
    const int n = (int)r.sims.size();
    const RelEntry* sel = r.selected >= 0 && r.selected < n ? &r.sims[r.selected] : nullptr;

    // Layer 0: the big portrait's well, the name's plate, the mood / relationship / description wells.
    UiItem pw = WellItem(kRelPortrait);
    pw.fill = th.frameDark;
    pw.radius = 16;
    c.ui.push_back(pw);
    UiItem plate = PanelItem(kRelName);
    plate.fill = th.frame;
    plate.borderArgb = kPlateRim;
    plate.border = 2;
    plate.radius = kRelName.h / 2.0f;
    c.ui.push_back(plate);
    for (Rect w : {kRelMood, kRelLevel, kRelDescWell})
    {
        UiItem it = WellItem(w);
        it.radius = 16;
        c.ui.push_back(it);
    }

    // Layer 1: the rows (bar, the portrait's well, the name), the scroll bar, the selected Sim.
    for (int i = 0; i < (int)v.items.size() && i < kRelRowsShown && r.first + i < n; i++)
    {
        const Rect row = v.items[i].rect;
        const bool selected = r.first + i == r.selected;
        if (i == focus) AddFocusRing(c, row, 16);
        UiItem bar = PanelItem(row);
        bar.fill = selected ? kRowSelFill : kRowFill;
        bar.borderArgb = selected ? th.fill : th.frame;
        bar.border = 2;
        bar.radius = 16;
        bar.raised = selected;
        bar.layer = 1;
        c.ui.push_back(bar);
        UiItem well = PanelItem(RowPortrait(row));
        well.plain = true;
        well.fill = th.frameDark;
        well.radius = 10;
        well.layer = 1;
        c.ui.push_back(well);
        const Rect nb = RowNameBox(row);
        const RowName nm = FitRowName(MenuWords(rom, r.sims[r.first + i].nameId), nb.w);
        UiItem t = TextItem(nb, nm.text, nm.size, Align::Left, Weight::ExtraBold);
        t.wrap = nm.wrap;
        t.middle = true;
        t.layer = 1;
        c.ui.push_back(t);
        AddMousePiece(c, {40, kRelSimY[i] - 16, 33, 33}, row); // (hotspots.cpp's "Sim i" area)
    }
    AddScrollBar(c, kRelScroll, r.first, std::min(kRelRowsShown, n), n);
    if (sel)
    {
        const FittedText name = FitLine(MenuWords(rom, sel->nameId), 26, 18, kRelName.w - 32, Weight::ExtraBold);
        UiItem nt = TextItem({kRelName.x + 16, kRelName.y, kRelName.w - 32, kRelName.h}, name.text, name.size, Align::Centre, Weight::ExtraBold);
        nt.wrap = false;
        nt.middle = true;
        nt.layer = 1;
        c.ui.push_back(nt);
        AddPair(c, kRelMood, MenuWords(rom, kStrMood), sel->moodId >= 0 ? MenuWords(rom, sel->moodId) : "");
        AddPair(c, kRelLevel, MenuWords(rom, kStrRelationship), sel->levelId >= 0 ? MenuWords(rom, sel->levelId) : "");
        const FittedBlock d = FitBlock(MenuWords(rom, sel->descId), kPkDescSize, kPkDescMin, kRelDesc);
        UiItem dt = TextItem(kRelDesc, d.text, d.size, Align::Left, Weight::Bold);
        dt.layer = 1;
        c.ui.push_back(dt);
    }

    // Images: the rows' portraits (2x) and the big one (3x), live.
    if (bottom)
    {
        for (int i = 0; i < kRelRowsShown && r.first + i < n; i++)
        {
            const Rect from {kRelSimFrom.x, kRelSimFrom.y + kRelSimY[i] - kRelSimY[0], kRelSimFrom.w, kRelSimFrom.h};
            const Rect pw2 = RowPortrait(RowRect(i));
            const int w = from.w * 2, h = from.h * 2;
            c.ui.push_back(ImageItem({pw2.x + (pw2.w - w) / 2, pw2.y + (pw2.h - h) / 2, w, h}, bottom, SW, SH, from));
        }
        if (sel)
        {
            const int w = kRelBigFrom.w * 3, h = kRelBigFrom.h * 3;
            c.ui.push_back(ImageItem({kRelPortrait.x + (kRelPortrait.w - w) / 2, kRelPortrait.y + (kRelPortrait.h - h) / 2, w, h}, bottom, SW, SH, kRelBigFrom));
        }
    }
}

void MeasureRelations(const RomData& rom, BlockMeasure& m)
{
    auto note = [&](int id, bool shrunk, bool fits) {
        m.checked++;
        if (shrunk) m.shrunk++;
        if (!fits) { m.cut++; m.cutIds.push_back(id); }
    };
    const Rect nb = RowNameBox(RowRect(0));
    for (int id = 0; id < 24; id++) // (the list holds Sims 0-23)
    {
        const std::string name = MenuWords(rom, 323 + id);
        if (name.empty()) continue;
        const RowName rn = FitRowName(name, nb.w);
        note(323 + id, rn.size < kRowName, rn.fits);
        const FittedText p = FitLine(name, 26, 18, kRelName.w - 32, Weight::ExtraBold);
        note(323 + id, p.size < 26, p.fits);
        const FittedBlock d = FitBlock(MenuWords(rom, 389 + id), kPkDescSize, kPkDescMin, kRelDesc);
        note(389 + id, d.size < kPkDescSize, d.fits);
        if (d.lines > m.tallestLines) { m.tallestLines = d.lines; m.tallestId = 389 + id; }
    }
    for (int id : {kStrMood, kStrRelationship})
    {
        const FittedText f = FitLine(MenuWords(rom, id), 20, 16, kRelMood.w - 36, Weight::Bold);
        note(id, f.size < 20, f.fits);
    }
    for (int id = 184; id <= 192 + 7 * 6 + 6; id++)
    {
        if (id == 191) continue; // ("Dummy")
        const FittedText f = FitLine(MenuWords(rom, id), 28, 20, kRelMood.w - 36, Weight::ExtraBold);
        note(id, f.size < 28, f.fits);
    }
}
}
