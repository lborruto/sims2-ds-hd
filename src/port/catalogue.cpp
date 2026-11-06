#include "port/catalogue.h"

#include <algorithm>

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

constexpr int kCells = 6, kOwnedPlaces = 6;
constexpr u32 kLitRim = 0xFFB2B2E3;                 // the game's light lavender (a selection it holds)
constexpr u32 kDim = 0x80FFFFFF, kDimShadow = 0x40383869;
constexpr u32 kPriceColour = 0xFFFFE9A0;            // a warm white: the money's line

Rect ShopCellRect(const CatalogueGeometry& g, int c)
{
    return {g.shopX + (c % g.shopCols) * (kCaCell + kCaGap), kCaGridY + (c / g.shopCols) * (kCaCell + kCaGap), kCaCell, kCaCell};
}
Rect OwnedRect(const CatalogueGeometry& g, int i) { return {g.ownedX + (i % 2) * (kCaCell + kCaGap), kCaGridY + (i / 2) * (kCaCell + kCaGap), kCaCell, kCaCell}; }

struct DsPoint { int x, y; };
DsPoint CellPoint(bool vendor, int c)
{
    if (vendor) return {kCaVendorCellX0 + (c % 2) * kCaVendorCellDx, kCaCellY0 + (c / 2) * kCaCellDy};
    return {kCaStoreCellX0 + (c % 3) * kCaStoreCellDx, kCaCellY0 + (c / 3) * kCaCellDy};
}
DsPoint OwnedPoint(int i) { return {kCaOwnedX0 + (i % 2) * kCaOwnedDx, kCaCellY0 + (i / 2) * kCaCellDy}; }
// The game's hit boxes: ±16 around a cell's centre, the buttons' 30 x 30.
Rect HitAround(DsPoint p) { return {p.x - 16, p.y - 16, 32, 32}; }
Rect ButtonHit(int x, int y) { return {x - 15, y - 15, 30, 30}; }

// The width the A hint's word is fitted to: what B's "Close" leaves of the hints' band.
int HintAWidth(const RomData& rom)
{
    const Rect b = ChipRect(0, 0, Button::B, MenuWords(rom, kStrClose), kPmHintSize, 0, Weight::ExtraBold);
    const int right = kPmHints.x + kPmHints.w - 8 - b.w - 28;
    return right - kPmHints.x - 60;
}

// An item's icon in its cell, from the record's own triple (tiles, descriptor, palette: a few
// items' palettes are not the descriptor's neighbour), the largest whole factor (3x at most) that
// leaves the cell's rim clear (the store's pictures are 24 to 32 pixels: a bed 2x, a chair 3x).
void AddItemIcon(Composition& c, IconCache* icons, const ShopItem& it, Rect cell)
{
    if (!icons || it.iconRes < 0) return;
    const SpriteRef ref = it.tilesRes >= 0 && it.palRes >= 0 ? SpriteRef {it.tilesRes, it.iconRes, it.palRes} : SpriteRef::Of(it.iconRes);
    const SpriteFrame* f = icons->Frame(ref, 0);
    if (!f || f->w <= 0 || f->h <= 0) return;
    const int scale = std::clamp((cell.w - 16) / std::max(f->w, f->h), 1, 3);
    if (const SpriteFrame* a = icons->Scaled(ref, 0, scale))
        c.ui.push_back(ImageItem({cell.x + (cell.w - a->w) / 2, cell.y + (cell.h - a->h) / 2, a->w, a->h}, a->argb.data(), a->w, a->h, {0, 0, a->w, a->h}));
}

// "§1,200" (the game's "$@1", its '$' the simoleon drawn §); a negative price is not shown.
std::string Price(const RomData& rom, int amount) { return amount >= 0 ? MenuWords(rom, kStrCaMoney, amount) : std::string(); }

// The shop's title: the store's own (2518), a vendor's name (the keeper's record name).
std::string ShopTitle(const RomData& rom, const CatalogueState& ca)
{
    if (!ca.vendor) return MenuWords(rom, kStrStore);
    const int id = kSimNameBase + ca.shop;
    return id <= kSimNameLast ? MenuWords(rom, id) : std::string();
}

// What the game's pay button acts on: the game's selection, its price as the game counts it.
const ShopItem* Selected(const CatalogueState& ca)
{
    if (ca.selected < 0) return nullptr;
    const std::vector<ShopItem>& l = ca.sell ? ca.owned : ca.items;
    return ca.selected < (int)l.size() ? &l[ca.selected] : nullptr;
}
int PriceOf(const ShopItem& it, bool sell) { return sell ? it.sellPrice : it.buyPrice; }

// The pay button's word: the game's mode and the price of its selection ("Buy §160").
std::string PayWord(const RomData* rom, const CatalogueState& ca)
{
    if (!rom) return {};
    std::string w = MenuWords(*rom, ca.sell ? kStrSell : kStrBuy);
    if (const ShopItem* s = Selected(ca))
        if (const std::string p = Price(*rom, PriceOf(*s, ca.sell)); !p.empty()) w += " " + p;
    return w;
}
// Under it, what it buys or sells: the selection's name (the info shows the focused item: the
// two may differ, so Pay names its own).
std::string PayName(const RomData* rom, const CatalogueState& ca)
{
    if (!rom) return {};
    const ShopItem* s = Selected(ca);
    return s ? MenuWords(*rom, s->nameId) : std::string();
}
}

CatalogueGeometry CatalogueLayout(bool vendor)
{
    CatalogueGeometry g {};
    g.shopX = kCaTitle.x;
    g.shopCols = vendor ? 2 : 3;
    const int shopW = g.shopCols * kCaCell + (g.shopCols - 1) * kCaGap;
    const int ownedW = 2 * kCaCell + kCaGap;
    g.ownedX = kCaTitle.x + kCaTitle.w - ownedW;
    g.shopHead = {g.shopX, kCaTop, shopW, kCaHeadH};
    g.ownedHead = vendor ? Rect {} : Rect {g.ownedX, kCaTop, ownedW, kCaHeadH};
    const int infoX = g.shopX + shopW + 16, infoR = vendor ? kCaTitle.x + kCaTitle.w : g.ownedX - 16;
    g.info = {infoX, kCaTop, infoR - infoX, kCaBottom - kCaTop};
    g.name = {g.info.x + 20, g.info.y + 12, g.info.w - 40, 44};
    g.price = {g.info.x + 20, g.info.y + 56, g.info.w - 40, 36};
    g.pay = {g.info.x + 16, g.info.y + g.info.h - 112, g.info.w - 32, 96};
    g.payIcon = {g.pay.x + 12, g.pay.y + 16, 64, 64};
    g.payText = {g.pay.x + 88, g.pay.y, g.pay.w - 100, g.pay.h};
    g.desc = {g.info.x + 20, g.info.y + 100, g.info.w - 40, g.pay.y - 12 - (g.info.y + 100)};
    // The game's arrows, under the store's grid.
    const int ay = kCaGridY + 2 * (kCaCell + kCaGap) + 6, cx = g.shopX + shopW / 2;
    g.up = vendor ? Rect {} : Rect {cx - 76, ay, 68, 64};
    g.down = vendor ? Rect {} : Rect {cx + 8, ay, 68, 64};
    return g;
}

bool CataloguePays(const CatalogueState& ca)
{
    const ShopItem* s = Selected(ca);
    if (!ca.valid || !s) return false;
    // (A price of 0 or less is never paid: a record the port cannot show a sum for.)
    if (!ca.sell) return s->buyPrice > 0 && ca.money >= s->buyPrice && (int)ca.owned.size() < kOwnedPlaces;
    return !ca.vendor && ca.shop == kCaStore && s->sellPrice > 0 && !CatalogueRefusesSale(s->id);
}

bool CatalogueRefusesSale(int id)
{
    switch (id)
    {
    case 0x22: case 0x24: case 0x26: case 0x4D: case 0x4E: case 0x4F: case 0x50: case 0x51: case 0x52: return true;
    default: return false;
    }
}

MenuView CatalogueView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrStore) return {};
    return CatalogueViewOf(s, *game, rom);
}

MenuView CatalogueViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const CatalogueState& ca = game.catalogue;
    if (s.screen != Screen::Catalogue || !game.valid || !ca.valid) return v;
    const CatalogueGeometry g = CatalogueLayout(ca.vendor);
    if (rom) v.title = ShopTitle(*rom, ca);
    const int count = (int)ca.items.size(), cols = g.shopCols;
    // The shop's cells: the list from the game's scroll (a vendor's six do not scroll).
    for (int c = 0; c < kCells; c++)
    {
        ViewItem it;
        it.rect = ShopCellRect(g, c);
        it.group = 1;
        const int i = ca.scroll + c;
        it.enabled = i < count;
        if (it.enabled)
        {
            it.id = CatalogueCellId(ca.items[i].row, i);
            const DsPoint p = CellPoint(ca.vendor, c);
            it.action = {TapStep(p.x, p.y)};
            it.word = OwnWord(rom, OwnWordId::Select); // (A selects it, sets the mode; Pay buys)
        }
        else it.id = kCaEmptyId + (u32)c;
        v.items.push_back(it);
    }
    // Pay: the game's button. Always focusable (the D-pad's way through the screen does not move
    // under the player's thumb); it performs only when the game would pay (CataloguePays), else it
    // is drawn greyed and A does nothing.
    const ShopItem* sel = Selected(ca);
    const int pay = kCaPayIndex;
    {
        ViewItem it;
        it.rect = g.pay;
        it.id = kCaPayId;
        if (CataloguePays(ca))
        {
            it.action = {TapStep(ca.vendor ? kCaVendorPayX : kCaPayX, kCaPayY)};
            it.word = PayWord(rom, ca);
        }
        it.next[0] = it.next[1] = kNoMove; // (nothing above or below it)
        v.items.push_back(it);
    }
    // The owned places (the store only): the pockets' six.
    const int owned0 = kCaOwned0;
    if (!ca.vendor)
        for (int i = 0; i < kOwnedPlaces; i++)
        {
            ViewItem it;
            it.rect = OwnedRect(g, i);
            it.enabled = i < (int)ca.owned.size();
            if (it.enabled)
            {
                it.id = CatalogueOwnedId(ca.owned[i].id, i);
                const DsPoint p = OwnedPoint(i);
                it.action = {TapStep(p.x, p.y)};
                it.word = OwnWord(rom, OwnWordId::Select);
            }
            else it.id = kCaEmptyId + 8 + (u32)i;
            v.items.push_back(it);
        }
    // Explicit moves in the shop's grid: Up from its top row and Down from its bottom row stay
    // (past them: the game's scroll), Right from its last column to Pay (when it can be focused).
    const int rows = kCells / cols;
    for (int c = 0; c < kCells; c++)
    {
        ViewItem& it = v.items[c];
        const int row = c / cols, col = c % cols;
        it.next[0] = row == 0 ? kNoMove : (v.items[c - cols].enabled ? c - cols : kNoMove);
        it.next[1] = row == rows - 1 ? kNoMove : (v.items[c + cols].enabled ? c + cols : kNoMove);
        if (col == cols - 1 || !v.items[c + 1].enabled) it.next[3] = pay;
    }
    // Left from the owned places' first column: Pay, else the shop's last column.
    if (!ca.vendor)
        for (int i = 0; i < kOwnedPlaces; i += 2) v.items[owned0 + i].next[2] = pay;
    // The game's own scroll (the store's arrows, while it has them enabled).
    if (!ca.vendor)
    {
        if (ca.upEnabled) v.scrollUp = {TapStep(kCaUpX, kCaArrowY)};
        if (ca.downEnabled) v.scrollDown = {TapStep(kCaDownX, kCaArrowY)};
    }
    v.scrollGroup = 1;
    // The first focus: the game's selection.
    if (sel && !ca.sell && ca.selected - ca.scroll >= 0 && ca.selected - ca.scroll < kCells) v.initial = ca.selected - ca.scroll;
    else if (sel && ca.sell && !ca.vendor) v.initial = owned0 + ca.selected;
    v.back = {TapStep(kCaCloseX, kCaCloseY)};
    v.backCloses = true;
    v.replay = false; // (a late press could spend: never replayed)
    v.own = true;
    v.valid = true;
    return v;
}

void BuildCatalogue(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons)
{
    const Theme& th = GameTheme();
    const CatalogueState& ca = game.catalogue;
    const CatalogueGeometry g = CatalogueLayout(ca.vendor);
    const int pay = kCaPayIndex, owned0 = kCaOwned0;
    if ((int)v.items.size() < kCells + 1 + (ca.vendor ? 0 : kOwnedPlaces)) return;
    const size_t first = c.ui.size();

    // Layer 0: the panel, the title's well, the info's.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem title = WellItem(kCaTitle);
    title.radius = 16;
    c.ui.push_back(title);
    UiItem info = WellItem(g.info);
    info.radius = 16;
    c.ui.push_back(info);

    // Layer 1: the title and the money, the headers (the game's mode lit), the cells, the info,
    // Pay, the arrows, the hints.
    const std::string money = Price(rom, ca.money);
    const int moneyW = (int)TextWidth(money, kPmTitleSize, Weight::ExtraBold);
    const Rect moneyBox {kCaClose.x - 20 - moneyW - 32, kCaTitle.y + 10, moneyW + 32, kCaTitle.h - 20};
    {
        UiItem plate = PanelItem(moneyBox);
        plate.fill = th.frameDark;
        plate.raised = false;
        plate.border = 0;
        plate.radius = moneyBox.h / 2.0f;
        plate.layer = 1;
        c.ui.push_back(plate);
        UiItem mt = TextItem(moneyBox, money, kPmTitleSize, Align::Centre, Weight::ExtraBold);
        mt.wrap = false;
        mt.middle = true;
        mt.colour = kPriceColour;
        mt.layer = 1;
        c.ui.push_back(mt);
        const FittedText t = FitLine(v.title, kPmTitleSize, kPmTextMin, moneyBox.x - 16 - (kCaTitle.x + 24), Weight::ExtraBold);
        UiItem tt = TextItem({kCaTitle.x + 24, kCaTitle.y, moneyBox.x - 16 - (kCaTitle.x + 24), kCaTitle.h}, t.text, t.size, Align::Left,
                             Weight::ExtraBold);
        tt.wrap = false;
        tt.middle = true;
        tt.layer = 1;
        c.ui.push_back(tt);
    }
    auto head = [&](Rect r, int id, bool lit) {
        UiItem h = PanelItem(r);
        h.fill = lit ? th.frame : th.fill;
        h.borderArgb = lit ? kLitRim : th.frameDark;
        h.border = lit ? 3 : 2;
        h.radius = r.h / 2.0f;
        h.raised = lit;
        h.layer = 1;
        c.ui.push_back(h);
        const FittedText f = FitLine(MenuWords(rom, id), 26, 18, r.w - 24, Weight::ExtraBold);
        UiItem t = TextItem(r, f.text, f.size, Align::Centre, Weight::ExtraBold);
        t.wrap = false;
        t.middle = true;
        if (!lit) { t.colour = kDim; t.shadowArgb = kDimShadow; }
        t.layer = 1;
        c.ui.push_back(t);
    };
    head(g.shopHead, kStrBuy, !ca.sell);
    if (!ca.vendor) head(g.ownedHead, kStrSell, ca.sell);

    for (int i = 0; i < kCells; i++)
    {
        const bool selected = !ca.sell && ca.selected >= 0 && ca.selected == ca.scroll + i;
        AddItemCell(c, ShopCellRect(g, i), focus == i && v.items[i].enabled, selected, !v.items[i].enabled);
        AddMousePiece(c, HitAround(CellPoint(ca.vendor, i)), ShopCellRect(g, i));
    }
    if (!ca.vendor)
        for (int i = 0; i < kOwnedPlaces; i++)
        {
            const bool selected = ca.sell && ca.selected == i;
            AddItemCell(c, OwnedRect(g, i), focus == owned0 + i && v.items[owned0 + i].enabled, selected, !v.items[owned0 + i].enabled);
            AddMousePiece(c, HitAround(OwnedPoint(i)), OwnedRect(g, i));
        }

    // The info: the focused item (a shop item at its buy price, an owned one at its sell price);
    // Pay focused, or nothing: the game's selection.
    const ShopItem* shown = nullptr;
    bool sell = false;
    if (focus >= 0 && focus < kCells && v.items[focus].enabled) shown = &ca.items[ca.scroll + focus];
    else if (!ca.vendor && focus >= owned0 && focus < owned0 + kOwnedPlaces && v.items[focus].enabled) { shown = &ca.owned[focus - owned0]; sell = true; }
    else if (const ShopItem* s = Selected(ca)) { shown = s; sell = ca.sell; }
    if (shown)
    {
        const FittedText name = FitLine(MenuWords(rom, shown->nameId), kCaNameSize, kCaNameMin, g.name.w, Weight::ExtraBold);
        UiItem nt = TextItem(g.name, name.text, name.size, Align::Left, Weight::ExtraBold);
        nt.wrap = false;
        nt.middle = true;
        nt.layer = 1;
        c.ui.push_back(nt);
        UiItem pt = TextItem(g.price, Price(rom, PriceOf(*shown, sell)), kCaPriceSize, Align::Left, Weight::ExtraBold);
        pt.wrap = false;
        pt.middle = true;
        pt.colour = kPriceColour;
        pt.layer = 1;
        c.ui.push_back(pt);
        const FittedBlock d = FitBlock(MenuWords(rom, shown->descId), kCaDescSize, kCaDescMin, g.desc);
        UiItem dt = TextItem(g.desc, d.text, d.size, Align::Left, Weight::Bold);
        dt.layer = 1;
        c.ui.push_back(dt);
    }

    // Pay: the game's simoleon button, its mode and the price of its selection; greyed when the
    // game would not pay.
    const bool canPay = !v.items[pay].action.empty();
    if (focus == pay) AddFocusRing(c, g.pay, 24);
    {
        UiItem b = PanelItem(g.pay);
        b.fill = th.frame;
        b.borderArgb = th.frameDark;
        b.border = 3;
        b.radius = 24;
        b.raised = canPay;
        b.layer = 1;
        c.ui.push_back(b);
        const FittedText w = FitLine(PayWord(&rom, ca), kCaPaySize, kCaPayMin, g.payText.w, Weight::ExtraBold);
        const std::string name = PayName(&rom, ca);
        // With a selection: the mode and price, then the selection's name under them.
        const Rect top = name.empty() ? g.payText : Rect {g.payText.x, g.payText.y + 10, g.payText.w, 46};
        UiItem wt = TextItem(top, w.text, w.size, Align::Left, Weight::ExtraBold);
        wt.wrap = false;
        wt.middle = true;
        if (!canPay) { wt.colour = kDim; wt.shadowArgb = kDimShadow; }
        wt.layer = 1;
        c.ui.push_back(wt);
        if (!name.empty())
        {
            const FittedText n = FitLine(name, kCaPayNameSize, kCaPayNameMin, g.payText.w, Weight::Bold);
            UiItem nt = TextItem({g.payText.x, g.payText.y + 54, g.payText.w, 32}, n.text, n.size, Align::Left, Weight::Bold);
            nt.wrap = false;
            nt.middle = true;
            nt.colour = canPay ? kPriceColour : kDim;
            if (!canPay) nt.shadowArgb = kDimShadow;
            nt.layer = 1;
            c.ui.push_back(nt);
        }
        AddMousePiece(c, ButtonHit(ca.vendor ? kCaVendorPayX : kCaPayX, kCaPayY), g.pay);
    }
    // The game's arrows (the store), lit while the game has them enabled; the D-pad scrolls.
    auto arrow = [&](Rect r, bool on) {
        UiItem b = PanelItem(r);
        b.fill = th.fill;
        b.borderArgb = th.frameDark;
        b.border = 2;
        b.radius = 20;
        b.raised = on;
        b.layer = 1;
        c.ui.push_back(b);
    };
    if (!ca.vendor)
    {
        arrow(g.up, ca.upEnabled);
        arrow(g.down, ca.downEnabled);
        AddMousePiece(c, ButtonHit(kCaUpX, kCaArrowY), g.up);
        AddMousePiece(c, ButtonHit(kCaDownX, kCaArrowY), g.down);
    }
    AddMousePiece(c, ButtonHit(kCaCloseX, kCaCloseY), kCaClose);

    // The hints: B closes (the game's X); A with the focused item's word (Pay: its mode and price);
    // the D-pad's vertical arms when the list scrolls.
    int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty())
    {
        const FittedText a = FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, HintAWidth(rom), Weight::ExtraBold);
        right = AddHint(c, right, Button::A, a.text);
    }
    if (!ca.vendor && (ca.upEnabled || ca.downEnabled)) AddHint(c, right, Button::DPadV, "");

    // Images: the items (the record table's sprites 3x), the pay button, the arrows, the X.
    for (int i = 0; i < kCells; i++)
        if (v.items[i].enabled) AddItemIcon(c, icons, ca.items[ca.scroll + i], ShopCellRect(g, i));
    if (!ca.vendor)
        for (int i = 0; i < kOwnedPlaces && i < (int)ca.owned.size(); i++) AddItemIcon(c, icons, ca.owned[i], OwnedRect(g, i));
    AddSpriteIcon(c, icons, kSprCaPay, 0, 2, g.payIcon, canPay ? 0 : 0x60000000);
    if (!ca.vendor)
    {
        AddSpriteIcon(c, icons, kSprCaUp, 0, 2, g.up, ca.upEnabled ? 0 : 0x60000000);
        AddSpriteIcon(c, icons, kSprCaDown, 0, 2, g.down, ca.downEnabled ? 0 : 0x60000000);
    }
    AddSpriteIcon(c, icons, kSprCaX, 0, 2, kCaClose);

    SortUi(c.ui, first);
}

void MeasureCatalogue(const RomData& rom, BlockMeasure& m)
{
    auto unused = [&](int id) {
        const std::string t = rom.String(id);
        return t.empty() || t.rfind("XXX", 0) == 0 || t.rfind("xxx", 0) == 0 || t == "Dummy" || t == "Empty String";
    };
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) {
        m.checked++;
        if (shrunk) m.shrunk++;
        if (lines > m.tallestLines) { m.tallestLines = lines; m.tallestId = id; }
        if (!fits && std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) { m.cut++; m.cutIds.push_back(id); }
    };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width) {
        const FittedText f = FitLine(text, size, minSize, width, Weight::ExtraBold);
        note(id, f.size < size, f.fits);
    };
    const CatalogueGeometry store = CatalogueLayout(false), vendor = CatalogueLayout(true);
    line(9000 + (int)OwnWordId::Select, OwnWord(&rom, OwnWordId::Select), kPmHintSize, kPmHintSize, HintAWidth(rom)); // (a cell's A)
    // Every item: any object may be on the list or owned. A name may end in an ellipsis (the
    // Italian table's sentence at 463, as Pockets): counted apart.
    for (int id = 0; id <= kItemIdLast; id++)
    {
        const int nameId = kItemNameBase + id, descId = kItemDescBase + id;
        if (unused(nameId)) continue;
        for (const CatalogueGeometry* g : {&store, &vendor})
        {
            const FittedText f = FitLine(MenuWords(rom, nameId), kCaNameSize, kCaNameMin, g->name.w, Weight::ExtraBold);
            m.checked++;
            if (f.size < kCaNameSize) m.shrunk++;
            // (Pay's line under its word: the selection's name; an ellipsis allowed, as the name's.)
            const FittedText pn = FitLine(MenuWords(rom, nameId), kCaPayNameSize, kCaPayNameMin, g->payText.w, Weight::Bold);
            m.checked++;
            if (pn.size < kCaPayNameSize) m.shrunk++;
            if (!f.fits) m.namesCut++;
            if (unused(descId)) continue;
            const FittedBlock b = FitBlock(MenuWords(rom, descId), kCaDescSize, kCaDescMin, g->desc);
            note(descId, b.size < kCaDescSize, b.fits, b.lines);
        }
    }
    // The titles beside the widest money (999,999, the game's cap); the headers; Pay at the widest
    // price the game charges (2 × a record's s16: §65,534) in both modes and layouts; the hints.
    const int moneyW = (int)ui::TextWidth(Price(rom, 999999), kPmTitleSize, Weight::ExtraBold) + 32;
    const int titleW = kCaClose.x - 20 - moneyW - 16 - (kCaTitle.x + 24);
    line(kStrStore, MenuWords(rom, kStrStore), kPmTitleSize, kPmTextMin, titleW);
    for (int k : {0x29, 0x2E, 0x2F, 0x30}) line(kSimNameBase + k, MenuWords(rom, kSimNameBase + k), kPmTitleSize, kPmTextMin, titleW);
    for (int id : {kStrBuy, kStrSell})
    {
        line(id, MenuWords(rom, id), 26, 18, store.shopHead.w - 24);
        line(id, MenuWords(rom, id), 26, 18, store.ownedHead.w - 24);
        line(id, MenuWords(rom, id), 26, 18, vendor.shopHead.w - 24);
        const std::string w = MenuWords(rom, id) + " " + Price(rom, 65534);
        line(id, w, kCaPaySize, kCaPayMin, store.payText.w);
        line(id, w, kCaPaySize, kCaPayMin, vendor.payText.w);
        line(id, w, kPmHintSize, kPmHintSize, HintAWidth(rom)); // (the A hint beside B's)
        line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, HintAWidth(rom));
    }
}
}
