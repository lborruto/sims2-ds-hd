#include "port/pockets.h"
#include "port/ownwords.h"

#include <algorithm>

#include "hotspots.h"
#include "port/gift.h"
#include "port/icons.h"
#include "port/panelmenu.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2hd
{
namespace
{
using namespace ui;

constexpr int kSlots = 6, kKeys = 3, kCounted = 4;
// The game's light lavender (a selection the game holds: its ring) and a cell's colours.
constexpr u32 kSelRim = 0xFFB2B2E3, kRedRim = 0xFFE03030; // (red: a key item the game's ring marks not owned)
constexpr u32 kEmptyFill = 0xFF7078BC, kCellFill = 0xFF5961A2;
// A button that does nothing now (the hand, the bin; Give Gift's bow): the shops' inactive look, a
// sunk plate with ui::kTextInactive words on it (5.8:1), never a faded white.
constexpr u32 kInactivePlate = 0xFF444A85;

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
// A counted item's icon (its record's sprite, 32 x 32 at ui::kTierS: the row's cells are narrower
// than the slots'), above the count.
Rect CountIconRect(int i)
{
    const Rect r = CountRect(i);
    return {r.x, r.y + 8, r.w, 72};
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

static void AddSpriteIcon(Composition& c, IconCache* icons, SpriteRef ref, int frame, int scale, Rect box, u32 opacity)
{
    if (!icons) return;
    if (const SpriteFrame* f = icons->Scaled(ref, frame, scale))
    {
        UiItem a = icons->Image(ref, frame, *f, {box.x + (box.w - f->w) / 2, box.y + (box.h - f->h) / 2, f->w, f->h});
        a.fill = opacity;
        c.ui.push_back(a);
    }
}

void AddSpriteIcon(Composition& c, IconCache* icons, int desc, int frame, int scale, Rect box, u32 opacity)
{
    if (desc >= 0) AddSpriteIcon(c, icons, SpriteRef::Of(desc), frame, scale, box, opacity);
}

void AddSpriteIcon(Composition& c, IconCache* icons, const ItemRec& item, int scale, Rect box, u32 opacity)
{
    if (item.iconRes < 0) return;
    AddSpriteIcon(c, icons, item.tilesRes >= 0 && item.palRes >= 0 ? SpriteRef {item.tilesRes, item.iconRes, item.palRes} : SpriteRef::Of(item.iconRes), 0, scale, box, opacity);
}

void AddTieredIcon(Composition& c, IconCache* icons, int desc, int frame, int tier, Rect box, u32 opacity)
{
    if (desc >= 0) AddTiered(c.ui, icons, SpriteRef::Of(desc), frame, tier, box, opacity);
}

void AddTieredIcon(Composition& c, IconCache* icons, const ItemRec& item, int tier, Rect box, u32 opacity)
{
    if (item.iconRes < 0) return;
    AddTiered(c.ui, icons, item.tilesRes >= 0 && item.palRes >= 0 ? SpriteRef {item.tilesRes, item.iconRes, item.palRes} : SpriteRef::Of(item.iconRes), 0, tier,
              box, opacity);
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
    minSize = std::max(minSize, ui::kMinText); // (never below the smallest text: ui::kMinText)
    size = std::max(size, minSize);
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
        it.enabled = i < (int)inv.slots.size();
        // Its id carries the item (another item in the slot is another item: the focus and an
        // armed bin do not stay on it).
        it.id = kPkSlotId + (u32)i + (it.enabled ? (u32)(inv.slots[i].id & 0xFFFF) << 8 : 0);
        const DsPoint p = SlotPoint(i);
        if (it.enabled)
        {
            it.action = {TapStep(p.x, p.y), hand};
            it.word = use;
            // The bin: the game throws its selected slot's item away at once, so X asks: the first
            // X selects the slot in the game (its ring, the HD bin green), the second taps the
            // slot again and the bin.
            it.xAsks = true;
            it.xArm = {TapStep(p.x, p.y)};
            it.buttonX = {TapStep(p.x, p.y), TapStep(kPkBinX, kPkBinY)};
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
    // Right from a slot: the next enabled slot of its row, else its row's key item (the cone
    // alone sends the first slot to the counted row when the rest of its row is empty).
    for (int i = 0; i < kSlots; i++)
    {
        int to = k0 + i / 3;
        for (int j = i + 1; j < (i / 3 + 1) * 3; j++)
            if (v.items[first + j].enabled) { to = first + j; break; }
        v.items[first + i].next[3] = to;
    }
    v.items[k0].next[2] = lastIn(0);
    v.items[k0 + 1].next[2] = lastIn(1) >= 0 ? lastIn(1) : lastIn(0);
    v.items[k0 + 2].next[2] = first + kSlots + kKeys + kCounted - 1;
    // Right from a key item: nothing (the column's end; the phone is Up from the top row).
    for (int k = 0; k < kKeys; k++) v.items[k0 + k].next[3] = kNoMove;
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
        if (inv.counted[i].count == 0) n.colour = kTextInactive; // (none: inactive, never a faded white)
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
    // The hand: usable for the focused item (a slot, an owned key item; not the phone); the bin:
    // for a slot, green when its X is armed (one more X throws it away).
    const bool onItem = MenuFocusable(v, focus) && focus < kSlots + kKeys + kCounted;
    const bool canUse = onItem && !v.items[focus].action.empty();
    const bool canBin = onItem && focus < kSlots;
    const bool armed = canBin && v.armed != 0 && v.armed == v.items[focus].id;
    auto button = [&](Rect r, const std::string& label, bool on, bool green) {
        UiItem b = PanelItem(r);
        b.fill = green ? 0xFF8ACB9A : on ? th.frame : kInactivePlate;
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
        if (!on) t.colour = kTextInactive; // (5.8:1 on the sunk plate)
        t.layer = 1;
        c.ui.push_back(t);
    };
    button(kPkHandBtn, MenuWords(rom, kStrUse), canUse, false);
    button(kPkBinBtn, MenuWords(rom, kStrTrash), canBin, armed);
    AddMousePiece(c, hs[6].area, kPkHandBtn);
    AddMousePiece(c, hs[10].area, kPkBinBtn);

    // Images: the items, the tools, the counted items (live), the hand and the bin. What the Sim
    // has none of has one look: its picture faint (a tool not owned: its own picture, not the
    // game's black silhouette; a counted item at 0, its count inactive), as every inactive picture.
    constexpr u32 kNone = 0x60000000;
    for (int i = 0; i < kSlots && i < (int)inv.slots.size(); i++) AddTieredIcon(c, icons, inv.slots[i], kTierL, SlotRect(i));
    for (int k = 0; k < kKeys; k++) AddTieredIcon(c, icons, kSprTool[k], 1, kTierL, KeyRect(k), inv.keys[k].count > 0 ? 0 : kNone);
    for (int i = 0; i < kCounted; i++) AddTieredIcon(c, icons, inv.counted[i], kTierS, CountIconRect(i), inv.counted[i].count > 0 ? 0 : kNone);
    const Rect handIcon {kPkHandBtn.x + 10, kPkHandBtn.y, 64, kPkHandBtn.h}, binIcon {kPkBinBtn.x + 10, kPkBinBtn.y, 64, kPkBinBtn.h};
    AddTieredIcon(c, icons, kSprHand, 0, kTierS, handIcon, canUse ? 0 : 0x60000000);
    AddTieredIcon(c, icons, armed ? kSprBinGreen : kSprBin, 0, kTierS, binIcon, canBin ? 0 : 0x60000000);
}

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
    for (int id = 0; id <= kItemIdLast; id++) items.push_back({kItemNameBase + id, kItemDescBase + id});
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
    {
        // The armed bin's X hint ("Again: throw it away", the port's word, 9000 + its id) in the
        // room BuildPanelMenu leaves it: the hints' band less B "Close", A "Use" and the D-pad's
        // "Choose" (the grid's cross).
        const int bw = ChipRect(0, 0, Button::B, MenuWords(rom, kStrClose), kPmHintSize, 0, Weight::ExtraBold).w;
        const int aw = ChipRect(0, 0, Button::A, MenuWords(rom, kStrUse), kPmHintSize, 0, Weight::ExtraBold).w;
        const int dw = ChipRect(0, 0, Button::DPad, OwnWord(&rom, OwnWordId::HintChoose), kPmHintSize, 0, Weight::ExtraBold).w;
        const int room = kPmHints.w - 8 - bw - 28 - aw - 28 - 60 - dw - 28;
        line(9000 + (int)OwnWordId::TrashAgain, OwnWord(&rom, OwnWordId::TrashAgain), kPmHintSize, kPmHintSize, room);
    }
    for (int id = kSimNameUnknown; id <= kSimNameLast; id++)
    {
        if ((id > kSimNameUnknown && id < kSimNameBase) || unused(id)) continue;
        const std::string n = MenuWords(rom, id);
        line(id, MenuWordsWith(rom, kStrGiftTitle, n), kPmTitleSize, kPmTextMin, kGiftTitle.w - 48);
        line(id, n, 26, 18, kGiftName.w - 32);
    }
    return m;
}
}
