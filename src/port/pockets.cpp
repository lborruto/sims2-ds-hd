#include "port/pockets.h"

#include <algorithm>

#include "hotspots.h"
#include "port/icons.h"
#include "port/panelmenu.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr int kSlots = 6, kKeys = 3, kCounted = 4;
// The game's light lavender (a selection the game holds: its ring) and a cell's colours.
constexpr u32 kSelRim = 0xFFB2B2E3, kRedRim = 0xFFE03030; // (red: a key item the game's ring marks not owned)
constexpr u32 kEmptyFill = 0xFF7078BC, kCellFill = 0xFF5961A2;
constexpr u32 kDim = 0x80FFFFFF, kDimShadow = 0x40383869;

Rect SlotRect(int i) { return {kPkGridX + (i % 3) * (kPkCell + kPkGap), kPkGridY + (i / 3) * (kPkCell + kPkGap), kPkCell, kPkCell}; }
Rect KeyRect(int k)
{
    const int h = (kPkKeys.h - 2 * kPkGap) / 3;
    return {kPkKeys.x, kPkKeys.y + k * (h + kPkGap), kPkKeys.w, h};
}
Rect CountRect(int i)
{
    const int w = (kPkCounted.w - 3 * kPkGap) / 4;
    return {kPkCounted.x + i * (w + kPkGap), kPkCounted.y, w, kPkCounted.h};
}
// A counted item's icon, 2x, near the top of its cell.
Rect CountIconRect(int i)
{
    const Rect r = CountRect(i);
    const int w = kPkCountIconFrom.w * 2, h = kPkCountIconFrom.h * 2;
    return {r.x + (r.w - w) / 2, r.y + 16, w, h};
}
struct DsPoint { int x, y; };
DsPoint SlotPoint(int i) { return {kPkSlotX0 + (i % 3) * kPkSlotDx, kPkSlotY0 + (i / 3) * kPkSlotDy}; }

}

// A cell: a well in the game's dark button colour, rimmed in lavender when the game holds it
// selected; empty slots flat and pale.
void AddItemCell(Composition& c, Rect r, bool focused, bool selected, bool empty, bool red)
{
    if (focused) AddFocusRing(c, r, 18);
    UiItem cell = PanelItem(r);
    cell.fill = empty ? kEmptyFill : kCellFill;
    cell.borderArgb = selected ? (red ? kRedRim : kSelRim) : GameTheme().frameDark;
    cell.border = selected ? 4 : 2;
    cell.radius = 18;
    cell.raised = !empty;
    cell.layer = 1;
    c.ui.push_back(cell);
}

void AddSpriteIcon(Composition& c, IconCache* icons, int desc, int frame, int scale, Rect box, u32 opacity)
{
    if (!icons || desc < 0) return;
    if (const SpriteFrame* f = icons->Scaled(SpriteRef::Of(desc), frame, scale))
    {
        UiItem a = ImageItem({box.x + (box.w - f->w) / 2, box.y + (box.h - f->h) / 2, f->w, f->h}, f->argb.data(), f->w, f->h, {0, 0, f->w, f->h});
        a.fill = opacity;
        c.ui.push_back(a);
    }
}

bool PocketsItemText(const InventoryState& inv, int item, int& nameId, int& descId)
{
    const ItemRec* r = nullptr;
    if (item >= 0 && item < kSlots) r = item < (int)inv.slots.size() ? &inv.slots[item] : nullptr;
    else if (item >= kSlots && item < kSlots + kKeys) r = &inv.keys[item - kSlots];
    else if (item >= kSlots + kKeys && item < kSlots + kKeys + kCounted) r = &inv.counted[item - kSlots - kKeys];
    if (!r || r->nameId < 0) return false;
    nameId = r->nameId;
    descId = r->descId;
    return true;
}

FittedBlock FitBlock(const std::string& text, float size, float minSize, Rect box)
{
    FittedBlock b {text, size, 0, true};
    for (float s = size; s >= minSize - 1e-3f; s -= 1)
    {
        const int n = (int)Wrap(text, s, (float)box.w).size();
        if (n * Metrics(s).lineHeight <= box.h) { b.size = s; b.lines = n; return b; }
    }
    b.size = minSize;
    b.lines = (int)Wrap(text, minSize, (float)box.w).size();
    b.fits = false;
    return b;
}

void AddPocketsView(MenuView& v, const InventoryState& inv, const PocketsSel& sel, const RomData* rom)
{
    if (rom) v.title = MenuTitle(*rom, 1, "");
    const int first = (int)v.items.size();
    const MenuStep hand = TapStep(kPkHandX, kPkHandY);
    const std::string use = rom ? MenuWords(*rom, kStrUse) : "";
    for (int i = 0; i < kSlots; i++)
    {
        ViewItem it;
        it.rect = SlotRect(i);
        it.id = kPkSlotId + (u32)i;
        it.enabled = i < (int)inv.slots.size();
        const DsPoint p = SlotPoint(i);
        if (it.enabled)
        {
            it.action = {TapStep(p.x, p.y), hand};
            it.word = use;
            // The bin: the game throws its selected slot's item away; a slot it has not selected
            // is selected first (the HD menu's question: the bin turns green).
            it.buttonX = {sel.slot == i ? TapStep(kPkBinX, kPkBinY) : TapStep(p.x, p.y)};
            if (rom) it.xWord = MenuWords(*rom, kStrTrash);
        }
        v.items.push_back(it);
    }
    for (int k = 0; k < kKeys; k++)
    {
        ViewItem it;
        it.rect = KeyRect(k);
        it.id = kPkKeyId + (u32)k;
        if (inv.keys[k].count > 0)
        {
            it.action = {TapStep(kPkKeyX, kPkKeyY0 + k * kPkKeyDy), hand};
            it.word = use;
        }
        v.items.push_back(it);
    }
    for (int i = 0; i < kCounted; i++)
    {
        ViewItem it;
        it.rect = CountRect(i);
        it.id = kPkCountId + (u32)i;
        v.items.push_back(it); // (shown for its name and description: the game's hand does nothing with it)
    }
    // Left from a key item: the last item of its row of slots (the row above when that one is
    // empty), the third key item's the last counted item: the geometry alone sends the middle
    // key item to the counted row when its slots are empty.
    auto lastIn = [&](int row) {
        for (int i = row * 3 + 2; i >= row * 3; i--)
            if (v.items[first + i].enabled) return first + i;
        return -1;
    };
    const int k0 = first + kSlots;
    v.items[k0].next[2] = lastIn(0);
    v.items[k0 + 1].next[2] = lastIn(1) >= 0 ? lastIn(1) : lastIn(0);
    v.items[k0 + 2].next[2] = first + kSlots + kKeys + kCounted - 1;
    // The first focus: the game's own selection, else the first item.
    if (sel.slot >= 0 && sel.slot < (int)inv.slots.size()) v.initial = first + sel.slot;
    else if (sel.key >= 0 && sel.key < kKeys) v.initial = first + kSlots + sel.key;
    else if (sel.counted >= 0 && sel.counted < kCounted) v.initial = first + kSlots + kKeys + sel.counted;
}

void BuildPockets(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons,
                  const u32* bottom)
{
    const Theme& th = GameTheme();
    const InventoryState& inv = game.town.inventory;
    const PocketsSel& sel = game.town.pockets;
    if ((int)v.items.size() < kSlots + kKeys + kCounted) return;
    // The game's own targets (hotspots.cpp): the slots, the hand, the key items, the bin, the counted items.
    const std::vector<Hotspot>& hs = HotspotsFor({Screen::Panel, Tab::Pockets});
    if (hs.size() < 15) return;

    // Layer 0: the info panel's well (the cells are on layer 1: they change with the content).
    UiItem info = WellItem(kPkInfo);
    info.radius = 16;
    c.ui.push_back(info);

    // The cells.
    for (int i = 0; i < kSlots; i++)
    {
        AddItemCell(c, SlotRect(i), focus == i && v.items[i].enabled, sel.slot == i, !v.items[i].enabled);
        AddMousePiece(c, hs[i].area, SlotRect(i));
    }
    for (int k = 0; k < kKeys; k++)
    {
        AddItemCell(c, KeyRect(k), focus == kSlots + k, sel.key == k && sel.slot < 0, false, inv.keys[k].count <= 0);
        AddMousePiece(c, hs[7 + k].area, KeyRect(k));
    }
    for (int i = 0; i < kCounted; i++)
    {
        const Rect r = CountRect(i);
        AddItemCell(c, r, focus == kSlots + kKeys + i, sel.counted == i, false);
        // The count, as the game prints it ("%d").
        UiItem n = TextItem({r.x, r.y + r.h - 44, r.w, 36}, std::to_string(inv.counted[i].count), 28, Align::Centre, Weight::ExtraBold);
        n.wrap = false;
        n.middle = true;
        if (inv.counted[i].count == 0) { n.colour = kDim; n.shadowArgb = kDimShadow; }
        // The live icon's own dark well (the game's button colour around it).
        UiItem well = PanelItem(CountIconRect(i));
        well.plain = true;
        well.fill = kPkCountWell;
        well.radius = 8;
        well.layer = 1;
        c.ui.push_back(well);
        n.layer = 1;
        c.ui.push_back(n);
        AddMousePiece(c, hs[11 + i].area, r);
    }

    // The info: the focused item's name and description (the phone focused: the game's own
    // selection's), the hand and the bin.
    int nameId = -1, descId = -1;
    int shown = MenuFocusable(v, focus) && focus < kSlots + kKeys + kCounted ? focus : -1;
    if (shown < 0)
        shown = sel.slot >= 0 ? sel.slot : sel.key >= 0 ? kSlots + sel.key : sel.counted >= 0 ? kSlots + kKeys + sel.counted : -1;
    if (shown >= 0 && PocketsItemText(inv, shown, nameId, descId))
    {
        const FittedText name = FitLine(MenuWords(rom, nameId), kPkNameSize, kPmTextMin, kPkInfo.w - 40, Weight::ExtraBold);
        UiItem t = TextItem({kPkInfo.x + 20, kPkInfo.y + 12, kPkInfo.w - 40, 44}, name.text, name.size, Align::Left, Weight::ExtraBold);
        t.wrap = false;
        t.middle = true;
        t.layer = 1;
        c.ui.push_back(t);
        const FittedBlock d = FitBlock(MenuWords(rom, descId), kPkDescSize, kPkDescMin, kPkDesc);
        UiItem dt = TextItem(kPkDesc, d.text, d.size, Align::Left, Weight::Bold);
        dt.layer = 1;
        c.ui.push_back(dt);
    }
    // The hand: usable for the focused item (a slot, an owned key item); the bin: for a slot,
    // green when the game has that slot selected (one more X throws it away).
    const bool canUse = MenuFocusable(v, focus) && !v.items[focus].action.empty();
    const bool canBin = focus >= 0 && focus < kSlots && MenuFocusable(v, focus);
    const bool armed = canBin && sel.slot == focus;
    auto button = [&](Rect r, const std::string& label, bool on, bool green) {
        UiItem b = PanelItem(r);
        b.fill = green ? 0xFF8ACB9A : th.frame;
        b.borderArgb = green ? 0xFF309A41 : th.frameDark;
        b.border = 3;
        b.radius = r.h / 2.0f;
        b.raised = on;
        b.layer = 1;
        c.ui.push_back(b);
        const FittedText f = FitLine(label, 26, 18, r.w - 92, Weight::ExtraBold);
        UiItem t = TextItem({r.x + 82, r.y, r.w - 92, r.h}, f.text, f.size, Align::Left, Weight::ExtraBold);
        t.wrap = false;
        t.middle = true;
        if (!on) { t.colour = kDim; t.shadowArgb = kDimShadow; }
        t.layer = 1;
        c.ui.push_back(t);
    };
    button(kPkHandBtn, MenuWords(rom, kStrUse), canUse, false);
    button(kPkBinBtn, MenuWords(rom, kStrTrash), canBin, armed);
    AddMousePiece(c, hs[6].area, kPkHandBtn);
    AddMousePiece(c, hs[10].area, kPkBinBtn);

    // Images: the items, the tools, the counted items (live), the hand and the bin.
    for (int i = 0; i < kSlots && i < (int)inv.slots.size(); i++) AddSpriteIcon(c, icons, inv.slots[i].iconRes, 0, 3, SlotRect(i));
    for (int k = 0; k < kKeys; k++) AddSpriteIcon(c, icons, kSprTool[k], inv.keys[k].count > 0 ? 1 : 0, 3, KeyRect(k));
    if (bottom)
        for (int i = 0; i < kCounted; i++)
        {
            const Rect from {kPkCountIconFrom.x + i * kPkCountDx, kPkCountIconFrom.y, kPkCountIconFrom.w, kPkCountIconFrom.h};
            c.ui.push_back(ImageItem(CountIconRect(i), bottom, SW, SH, from));
        }
    const Rect handIcon {kPkHandBtn.x + 10, kPkHandBtn.y, 64, kPkHandBtn.h}, binIcon {kPkBinBtn.x + 10, kPkBinBtn.y, 64, kPkBinBtn.h};
    AddSpriteIcon(c, icons, kSprHand, 0, 2, handIcon, canUse ? 0 : 0x60000000);
    AddSpriteIcon(c, icons, armed ? kSprBinGreen : kSprBin, 0, 2, binIcon, canBin ? 0 : 0x60000000);
}
}

#include "port/gift.h"

namespace sims2wide
{
BlockMeasure MeasurePockets(const RomData& rom)
{
    using namespace ui;
    BlockMeasure m;
    auto unused = [&](int id) {
        const std::string t = rom.String(id);
        return t.empty() || t.rfind("XXX", 0) == 0 || t.rfind("xxx", 0) == 0 || t == "Dummy" || t == "Empty String";
    };
    auto note = [&](int id, bool shrunk, bool fits) {
        m.checked++;
        if (shrunk) m.shrunk++;
        if (!fits)
        {
            m.cut++;
            if (std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) m.cutIds.push_back(id);
        }
    };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width) {
        const FittedText f = FitLine(text, size, minSize, width, Weight::ExtraBold);
        note(id, f.size < size, f.fits);
    };
    // An item's name in the info may end in an ellipsis (the Italian table has a sentence at the
    // first object's name, 463, as Interact's strip shows): counted apart.
    auto name = [&](int id, const std::string& text, int width) {
        const FittedText f = FitLine(text, kPkNameSize, kPmTextMin, width, Weight::ExtraBold);
        m.checked++;
        if (f.size < kPkNameSize) m.shrunk++;
        if (!f.fits) { m.namesCut++; if (std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) m.cutIds.push_back(id); }
    };
    auto block = [&](int id) {
        const std::string text = MenuWords(rom, id);
        for (Rect box : {kPkDesc, kGiftDesc})
        {
            const FittedBlock b = FitBlock(text, kPkDescSize, kPkDescMin, box);
            note(id, b.size < kPkDescSize, b.fits);
            if (b.lines > m.tallestLines) { m.tallestLines = b.lines; m.tallestId = id; }
        }
    };
    // Every name and description: the items (any object may be in the pockets), the key items,
    // the counted items; in the Pockets info and the Give Gift info.
    std::vector<std::pair<int, int>> items;
    for (int id = 0; id <= 330; id++) items.push_back({463 + id, 822 + id});
    for (int k = 0; k < 3; k++) items.push_back({1968 + k, 1972 + k});
    for (int i = 0; i < 4; i++) items.push_back({1154 + i, 845 + i});
    for (auto [nameId, desc] : items)
    {
        if (unused(nameId)) continue;
        const std::string n = MenuWords(rom, nameId);
        name(nameId, n, kPkInfo.w - 40);
        name(nameId, n, kGiftInfo.w - 40);
        if (!unused(desc)) block(desc);
    }
    // The buttons' and hints' words, the Give Gift title for every Sim and its name plate.
    line(kStrUse, MenuWords(rom, kStrUse), 26, 18, kPkHandBtn.w - 92);
    line(kStrTrash, MenuWords(rom, kStrTrash), 26, 18, kPkBinBtn.w - 92);
    line(kStrGive, MenuWords(rom, kStrGive), 30, 20, kGiftBow.w - 120);
    for (int id : {kStrUse, kStrTrash, kStrGive}) line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 240);
    for (int id = 321; id <= 368; id++)
    {
        if (id == 322 || unused(id)) continue;
        const std::string n = MenuWords(rom, id);
        line(id, MenuWordsWith(rom, kStrGiftTitle, n), kPmTitleSize, kPmTextMin, kGiftTitle.w - 48);
        line(id, n, 26, 18, kGiftName.w - 32);
    }
    return m;
}
}
