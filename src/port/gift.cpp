#include "port/gift.h"

#include <algorithm>

#include "hotspots.h"
#include "port/icons.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr int kSlots = 6;
constexpr u32 kPlateRim = 0xFFB2B2E3; // the game's light lavender

Rect SlotRect(int i) { return {kGiftGridX + (i % 2) * (kPkCell + kPkGap), kGiftGridY + (i / 2) * (kPkCell + kPkGap), kPkCell, kPkCell}; }
}

MenuView GiftMenuView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrGiftTitle) return {};
    return GiftMenuViewOf(s, *game, rom);
}

MenuView GiftMenuViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const GiftState& g = game.town.gift;
    const InventoryState& inv = game.town.inventory;
    if (s.screen != Screen::Gift || !game.valid || !g.valid || !inv.valid) return v;
    if (rom) v.title = MenuWordsWith(*rom, kStrGiftTitle, g.recipientNameId >= 0 ? MenuWords(*rom, g.recipientNameId) : "");
    const std::string give = rom ? MenuWords(*rom, kStrGive) : "";
    for (int i = 0; i < kSlots; i++)
    {
        ViewItem it;
        it.rect = SlotRect(i);
        it.id = kGiftSlotId + (u32)i;
        // An empty slot, or an item this Sim may not have (the game greys it and its bow refuses).
        it.enabled = i < (int)inv.slots.size() && !g.refused[i];
        if (it.enabled)
        {
            it.action = {TapStep(kGiftSlotX0 + (i % 2) * kGiftSlotDx, kGiftSlotY0 + (i / 2) * kGiftSlotDy), TapStep(kGiftBowX, kGiftBowY)};
            it.word = give;
        }
        v.items.push_back(it);
    }
    v.back = {TapStep(kGiftCloseX, kGiftCloseY)};
    v.initial = g.slot;
    v.valid = true;
    return v;
}

void BuildGiftMenu(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons,
                   const u32* bottom)
{
    const Theme& th = GameTheme();
    const GiftState& g = game.town.gift;
    const InventoryState& inv = game.town.inventory;
    const size_t first = c.ui.size();
    const std::vector<Hotspot>& hs = HotspotsFor({Screen::Gift, Tab::None});
    if ((int)v.items.size() < kSlots || hs.size() < 8) return;

    // Layer 0: the panel, the title's well, the info's, the portrait's (the game's well colour
    // behind the live picture) and the name plate.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem title = WellItem(kGiftTitle);
    title.radius = 16;
    c.ui.push_back(title);
    UiItem info = WellItem(kGiftInfo);
    info.radius = 16;
    c.ui.push_back(info);
    UiItem well = WellItem(kGiftPortrait);
    well.fill = th.frameDark;
    well.radius = 16;
    c.ui.push_back(well);
    UiItem plate = PanelItem(kGiftName);
    plate.fill = th.frame;
    plate.borderArgb = kPlateRim;
    plate.border = 2;
    plate.radius = kGiftName.h / 2.0f;
    c.ui.push_back(plate);

    // Layer 1: the title, the recipient's name, the cells, the info, the bow's button, the hints.
    const FittedText t = FitLine(v.title, kPmTitleSize, kPmTextMin, kGiftTitle.w - 48, Weight::ExtraBold);
    UiItem tt = TextItem({kGiftTitle.x + 24, kGiftTitle.y, kGiftTitle.w - 48, kGiftTitle.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
    tt.wrap = false;
    tt.middle = true;
    tt.layer = 1;
    c.ui.push_back(tt);
    if (g.recipientNameId >= 0)
    {
        const FittedText n = FitLine(MenuWords(rom, g.recipientNameId), 26, 18, kGiftName.w - 32, Weight::ExtraBold);
        UiItem nt = TextItem({kGiftName.x + 16, kGiftName.y, kGiftName.w - 32, kGiftName.h}, n.text, n.size, Align::Centre, Weight::ExtraBold);
        nt.wrap = false;
        nt.middle = true;
        nt.layer = 1;
        c.ui.push_back(nt);
    }
    for (int i = 0; i < kSlots; i++)
    {
        const bool used = i < (int)inv.slots.size();
        AddItemCell(c, SlotRect(i), focus == i && v.items[i].enabled, g.slot == i && used, !used);
        AddMousePiece(c, hs[i].area, SlotRect(i));
    }
    // The focused item's name and description (else the game's selection's).
    int shown = MenuFocusable(v, focus) ? focus : g.slot;
    int nameId = -1, descId = -1;
    if (shown >= 0 && PocketsItemText(inv, shown, nameId, descId))
    {
        const FittedText name = FitLine(MenuWords(rom, nameId), kPkNameSize, kPmTextMin, kGiftInfo.w - 40, Weight::ExtraBold);
        UiItem nt = TextItem({kGiftInfo.x + 20, kGiftInfo.y + 12, kGiftInfo.w - 40, 44}, name.text, name.size, Align::Left, Weight::ExtraBold);
        nt.wrap = false;
        nt.middle = true;
        nt.layer = 1;
        c.ui.push_back(nt);
        const FittedBlock d = FitBlock(MenuWords(rom, descId), kPkDescSize, kPkDescMin, kGiftDesc);
        UiItem dt = TextItem(kGiftDesc, d.text, d.size, Align::Left, Weight::Bold);
        dt.layer = 1;
        c.ui.push_back(dt);
    }
    // The bow: the game's own button (live) on a bar with A and the game's "Give Gift".
    const bool canGive = MenuFocusable(v, focus);
    {
        UiItem b = PanelItem(kGiftBow);
        b.fill = th.fill; // (the game's panel colour: the live button's corners meet it)
        b.borderArgb = th.frameDark;
        b.border = 3;
        b.radius = 24;
        b.raised = canGive;
        b.layer = 1;
        c.ui.push_back(b);
        const FittedText w = FitLine(MenuWords(rom, kStrGive), 30, 20, kGiftBow.w - 120, Weight::ExtraBold);
        UiItem wt = TextItem({kGiftBow.x + 104, kGiftBow.y, kGiftBow.w - 120, kGiftBow.h}, w.text, w.size, Align::Left, Weight::ExtraBold);
        wt.wrap = false;
        wt.middle = true;
        if (!canGive) { wt.colour = 0x80FFFFFF; wt.shadowArgb = 0x40383869; }
        wt.layer = 1;
        c.ui.push_back(wt);
        AddMousePiece(c, hs[6].area, kGiftBow);
    }
    // The hints: A gives (the focused slot's word), B closes (the game's X).
    const int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
    if (canGive)
    {
        const FittedText a = FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, right - kPmHints.x - 60, Weight::ExtraBold);
        AddHint(c, right, Button::A, a.text);
    }

    // Images: the items, the bow (live), the portrait (live).
    for (int i = 0; i < kSlots && i < (int)inv.slots.size(); i++)
        AddSpriteIcon(c, icons, inv.slots[i].iconRes, 0, 3, SlotRect(i), v.items[i].enabled ? 0 : 0x60000000);
    if (bottom)
    {
        const int bw = kGiftBowFrom.w * 2, bh = kGiftBowFrom.h * 2;
        c.ui.push_back(ImageItem({kGiftBow.x + 16, kGiftBow.y + (kGiftBow.h - bh) / 2, bw, bh}, bottom, SW, SH, kGiftBowFrom));
        const int w = kGiftPortraitFrom.w * 3, h = kGiftPortraitFrom.h * 3;
        c.ui.push_back(ImageItem({kGiftPortrait.x + (kGiftPortrait.w - w) / 2, kGiftPortrait.y + (kGiftPortrait.h - h) / 2, w, h}, bottom, SW,
                                 SH, kGiftPortraitFrom));
    }

    std::stable_sort(c.ui.begin() + (long)first, c.ui.end(), [](const UiItem& a, const UiItem& b) {
        const int ka = a.kind == UiItem::Image ? 1 << 20 : a.layer, kb = b.kind == UiItem::Image ? 1 << 20 : b.layer;
        return ka < kb;
    });
}
}
