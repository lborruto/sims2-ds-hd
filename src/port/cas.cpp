#include "port/cas.h"

#include <algorithm>
#include <cmath>

#include "console.h"
#include "port/front.h"
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

constexpr u32 kCellFill = 0xFF6A72B8, kCellFocusFill = 0xFF9AA2E3, kTabPlateFill = 0xFF9AA2E3;
constexpr u32 kChosenRim = 0xFFB2B2E3;   // the game's choice (the focus alone is green)
constexpr u32 kDim = 0x80FFFFFF;          // a key that would do nothing (the name is full)
constexpr u32 kWarm = 0xFFFFF2D6;         // the chosen aspiration's name
constexpr u32 kSlotFill = 0xFF51518A, kSlotNext = 0xFFB2B2E3;

bool CasScreen(ScreenState s) { return s.screen == Screen::CreateASim; }

MenuStep Tap(int x, int y) { return TapStep(x, y); }
Rect Plus(Rect r, int dx, int dy) { return {r.x + dx, r.y + dy, r.w, r.h}; }

ViewItem Item(Rect r, u32 id, MenuAction a, const std::string& word = "")
{
    ViewItem it;
    it.rect = r;
    it.id = id;
    it.action = std::move(a);
    it.word = word;
    return it;
}

// A live crop of the touch screen, as large as a whole factor `k` makes it, centred in box.
void Crop(std::vector<UiItem>& images, const u32* bottom, Rect from, int k, Rect box)
{
    if (!bottom) return;
    const int w = from.w * k, h = from.h * k;
    images.push_back(ImageItem({box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h}, bottom, SW, SH, from));
}

// A cell: its well (raised and lighter when focused; the game's choice rimmed), the focus's halo.
void Cell(Composition& c, Rect r, bool focused, bool chosen, float radius = 14)
{
    if (focused) AddFocusRing(c, r, radius);
    UiItem w = PanelItem(r);
    w.fill = focused ? kCellFocusFill : kCellFill;
    w.borderArgb = chosen ? kChosenRim : GameTheme().frame;
    w.border = chosen ? 4 : 3;
    w.radius = radius;
    w.layer = 1;
    c.ui.push_back(w);
}

void Text(Composition& c, Rect r, const std::string& s, float size, Align a = Align::Centre, u32 colour = 0xFFFFFFFF, Weight w = Weight::ExtraBold)
{
    UiItem t = TextItem(r, s, size, a, w);
    t.wrap = false;
    t.middle = true;
    t.colour = colour;
    t.layer = 1;
    c.ui.push_back(t);
}

std::string Title(const RomData& rom, int page) { return MenuWords(rom, page == 4 ? kStrCasKeep : kStrCasTitle0 + page); }
// Start's word: the next page's name (Hair Style, Face Style, Clothes, Confirm).
std::string NextWord(const RomData& rom, int page) { return page >= 0 && page < 4 ? MenuWords(rom, kStrCasTitle0 + page + 1) : ""; }
// A hint chip right-aligned at `right` in the panel's hints band; the next right edge.
int HintChip(Composition& c, int right, Button b, const std::string& label)
{
    const Rect r = ChipRect(0, 0, b, label, kPmHintSize, 0, Weight::ExtraBold);
    UiItem it = ChipItem(right - r.w, kCasHints.y + (kCasHints.h - r.h) / 2, b, label, kPmHintSize, 0, Weight::ExtraBold);
    it.layer = 1;
    c.ui.push_back(it);
    return right - r.w - 28;
}
int Slot(int aspiration) { for (int j = 0; j < 4; j++) if (kCasAspOfSlot[j] == aspiration) return j; return -1; }
}

std::string CasKeyText(const RomData& rom, int page, int i)
{
    if (page < 0 || page > 2 || i < 0 || i >= kCasKeyCount[page] || kStrCasKeys[page] + i >= rom.StringCount()) return {};
    return CasNameText(rom.String(kStrCasKeys[page] + i));
}

std::string CasNameText(const std::string& name)
{
    std::string o;
    for (char ch : name) o += ch == '$' ? std::string("\xC2\xA7") : std::string(1, ch);
    return o;
}

CasType CasTypeAction(const CasState& cs, const RomData& rom, const std::string& typed)
{
    CasType t;
    if (!cs.valid || cs.page != 0 || !cs.nameKnown || typed.empty()) return t;
    if (typed == "\b")
    {
        if (!cs.name.empty()) t.taps = {Tap(kCasBackX, kCasBackY)};
        return t;
    }
    if (cs.nameLength >= kCasName) return t;
    const std::string ch = typed == "$" ? "\xC2\xA7" : typed;
    const int kb = std::clamp(cs.keyboard, 0, 2);
    // The page shown first, then the others; the exact character, then a letter's other case
    // (the game's keyboards have both).
    for (int pass = 0; pass < 2; pass++)
        for (int n = 0; n < 3; n++)
        {
            const int page = (kb + n) % 3;
            for (int i = 0; i < kCasKeyCount[page]; i++)
            {
                const std::string key = CasKeyText(rom, page, i);
                if (key.empty() || (pass == 0 ? key != ch : GameLower(key) != GameLower(ch))) continue;
                if (page == kb) t.taps = {Tap(kCasKeyX0 + kCasKeyDx * (i % 9), kCasKeyY0 + kCasKeyDy * (i / 9))};
                else
                {
                    t.taps = {Tap(kCasKbX, kCasKbY[page])};
                    t.done = false;
                }
                return t;
            }
        }
    return t;
}

MenuView CasView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrCasKeep) return {};
    return CasViewOf(s, *game, rom);
}

MenuView CasViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const CasState& cs = game.front.cas;
    // The front end's pages only (5-7 are the in-game Tan / Hair / Clothes: not this screen).
    if (!CasScreen(s) || !game.valid || !cs.valid || cs.page < 0 || cs.page >= kCasPages) return v;
    v.valid = true;
    v.own = true;
    // Taps that land late would land on another page or start the game: never replayed.
    v.replay = false;
    v.stickTurns = true;
    if (rom) v.title = cs.page == 0 ? "" : Title(*rom, cs.page);
    // A page change in flight: the new page's tab, nothing to press until its object is there.
    if (cs.switching) { v.page = 0x7F; return v; }
    v.page = 0x40 + cs.page;
    // LB / RB the pages, ends included (no ring: LB on the Name page never lands on Keep this Sim?).
    if (cs.page > 0) v.buttons[(int)MenuButton::LB] = {Tap(kCasTabX, kCasTabY0 + kCasTabDy * (cs.page - 1))};
    if (cs.page < kCasPages - 1) v.buttons[(int)MenuButton::RB] = {Tap(kCasTabX, kCasTabY0 + kCasTabDy * (cs.page + 1))};
    if (cs.page < 4) v.buttons[(int)MenuButton::Start] = {Tap(kCasTabX, kCasTabY0 + kCasTabDy * (cs.page + 1))};
    // X / Y turn the model on every page but the Name page (there the keyboard's page and the
    // backspace): the keyboard's way, with no right stick.
    v.xyTurns = cs.page != 0;

    switch (cs.page)
    {
    case 0:
    {
        if (!cs.nameKnown) return v;
        for (int g = 0; g < 2; g++) v.items.push_back(Item(CasGenderRect(g), kCasGenderId + (u32)g, {Tap(kCasGenderX[g], kCasGenderY)}));
        for (int i = 0; i < 4; i++) v.items.push_back(Item(CasSkinRect(i), kCasSkinId + (u32)i, {Tap(kCasSkinX0 + kCasSkinDx * i, kCasSkinY)}));
        const int kb = std::clamp(cs.keyboard, 0, 2);
        for (int p = 0; p < 3; p++)
        {
            ViewItem it = Item(CasKbPageRect(p), kCasKbPageId + (u32)p, {});
            if (p != kb) it.action = {Tap(kCasKbX, kCasKbY[p])};   // (the page shown: nothing to do)
            v.items.push_back(it);
        }
        const bool full = cs.nameLength >= kCasName;
        v.initial = (int)v.items.size();
        for (int i = 0; i < kCasKeys; i++)
        {
            // Every place of the keyboard is an item (its id the place: the focus stays there
            // through a page change); past the page's keys, none to press (123! has 20).
            ViewItem it = Item(CasKeyRect(i), kCasKeyId + (u32)i, {});
            it.enabled = i < kCasKeyCount[kb];
            // A key the game would refuse (the name holds seven): focusable, nothing to do.
            if (it.enabled && !full) it.action = {Tap(kCasKeyX0 + kCasKeyDx * (i % 9), kCasKeyY0 + kCasKeyDy * (i / 9))};
            v.items.push_back(it);
        }
        ViewItem back = Item(kCasBackRect, kCasBackId, {});
        if (!cs.name.empty()) back.action = {Tap(kCasBackX, kCasBackY)};
        v.items.push_back(back);
        // Down from a key: the key under it, else the next row's last key (123!'s short last
        // row), and only from the last row the backspace: never by surprise from the middle.
        {
            const int base = v.initial, count = kCasKeyCount[kb], rows = (count + 8) / 9;
            for (int i = 0; i < count; i++)
                v.items[base + i].next[1] = i / 9 + 1 < rows ? base + std::min(i + 9, count - 1) : base + kCasKeys;
        }
        v.buttons[(int)MenuButton::Y] = back.action;
        v.buttons[(int)MenuButton::X] = {Tap(kCasKbX, kCasKbY[(kb + 1) % 3])};
        break;
    }
    case 1:
    {
        if (!cs.hairKnown) return v;
        for (int i = 0; i < 8; i++) v.items.push_back(Item(CasSwatchRect(i), kCasSwatchId + (u32)i, {Tap(kCasSwatchX0 + kCasSwatchDx * i, kCasSwatchY)}));
        const int first = (int)v.items.size();
        for (int cell = 0; cell < 4; cell++)
        {
            const int col = cell & 1, row = cell >> 1;
            // By place (the focus stays on its column while the game scrolls the styles under it).
            ViewItem it = Item(CasHairRect(cell), kCasHairId + (u32)cell, {Tap(kCasHairX0 + kCasHairDx * col, kCasHairY0 + kCasHairDy * row)});
            if (col == 0 && cs.canLeft) it.left = {Tap(kCasHairLeftX, kCasHairArrowY)};
            if (col == 1 && cs.canRight) it.right = {Tap(kCasHairRightX, kCasHairArrowY)};
            it.next[col == 0 ? 2 : 3] = kNoMove;
            v.items.push_back(it);
        }
        // The first focus: the game's style when shown, else its colour.
        const int col = cs.hairPos % 9 - cs.hairScroll, row = cs.hairPos / 9;
        v.initial = col >= 0 && col <= 1 ? first + row * 2 + col : std::clamp(cs.hairColour, 0, 7);
        break;
    }
    case 2:
        if (cs.face < 0) return v;   // (the face not read yet: the page's tab alone)
        for (int i = 0; i < 9; i++)
            v.items.push_back(Item(CasFaceRect(i), kCasFaceId + (u32)i, {Tap(kCasFaceX0 + kCasFaceDx * (i % 3), kCasFaceY0 + kCasFaceDy * (i / 3))}));
        v.initial = std::clamp(cs.face, 0, 8);
        break;
    case 3:
    {
        if (!cs.clothesKnown) return v;
        for (int i = 0; i < 6; i++)
        {
            ViewItem it = Item(CasCatRect(i), kCasCatId + (u32)i, {});
            if (i != cs.category) it.action = {Tap(kCasCatX, kCasCatY[i])};
            it.next[2] = kNoMove;
            v.items.push_back(it);
        }
        for (int cell = 0; cell < 6; cell++)
        {
            const int col = cell % 3, row = cell / 3;
            v.items.push_back(Item(CasClothRect(cell), kCasClothId + (u32)cell, {Tap(kCasClothX0 + kCasClothDx * col, kCasClothY0 + kCasClothDy * row)}));
        }
        // The game's arrows as buttons beside the cells (the categories stay at the left), only
        // while it shows them.
        ViewItem left = Item(kCasClothLeft, kCasClothId + 0x10, {Tap(kCasClothLeftX, kCasClothArrowY)});
        ViewItem right = Item(kCasClothRight, kCasClothId + 0x11, {Tap(kCasClothRightX, kCasClothArrowY)});
        left.enabled = cs.canLeft;
        right.enabled = cs.canRight;
        if (!left.enabled) left.action.clear();
        if (!right.enabled) right.action.clear();
        right.next[3] = kNoMove;
        v.items.push_back(left);
        v.items.push_back(right);
        v.initial = cs.clothesCell >= 0 ? 6 + cs.clothesCell : 6;
        break;
    }
    case 4:
    {
        if (!cs.keepKnown) return v;
        // Once the tick or the cross is pressed the game counts down: nothing more to press.
        const bool live = cs.answer == 0;
        for (int j = 0; j < 4; j++)
        {
            const int a = kCasAspOfSlot[j];
            ViewItem it = Item(CasAspRect(j), kCasAspId + (u32)a, {Tap(kCasAspX0 + kCasAspDx * j, kCasAspY)}, rom ? MenuWords(*rom, kStrAsp0 + a) : "");
            it.enabled = live;
            if (a == cs.aspiration) it.action.clear();   // (the game's choice already)
            it.next[0] = kNoMove;
            v.items.push_back(it);
        }
        ViewItem tick = Item(kCasTick, kCasTickId, {Tap(kCasTickX, kCasAnswerY)}, rom ? MenuWords(*rom, kStrCasYes) : "");
        ViewItem cross = Item(kCasCross, kCasCrossId, {Tap(kCasCrossX, kCasAnswerY)}, rom ? MenuWords(*rom, kStrCasNo) : "");
        tick.enabled = cross.enabled = live;
        // Both leave the screen (the town, the title): A and B then wait as after a close.
        tick.closes = cross.closes = true;
        tick.next[1] = cross.next[1] = kNoMove;
        v.items.push_back(tick);
        v.items.push_back(cross);
        if (live)
        {
            v.back = cross.action;
            v.backCloses = true;
            // One B only puts the focus on the cross ("No": the Sim is discarded); B again there
            // (or A) answers.
            v.backFocus = (int)v.items.size() - 1;
        }
        v.initial = live ? Slot(cs.aspiration) : -1;
        break;
    }
    }
    return v;
}

void BuildCas(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom,
              int topW, int topH)
{
    const Theme& th = GameTheme();
    const CasState& cs = game.front.cas;
    if (!cs.valid || cs.page < 0 || cs.page >= kCasPages) return;
    const size_t first = c.ui.size();
    std::vector<UiItem> images;

    // The model: the top screen at its shape, framed; the camera for the mouse (its right drag turns it).
    const Rect top = FrontFit(kCasTopBox, topW, topH);
    FrontFramed(c, {Src::Top, {0, 0, topW, topH}, top});
    {
        // What turns it: the right stick (RStick); off the Name page X (left) and Y (right) too, either side of it.
        // The keyboard (the device used last) has no stick: its X / Y keys alone, off the Name page (on
        // it, the mouse's right drag; its L / R keys are the pages here).
        const bool keys = ui::ChipLookNow().face == Face::Keyboard;
        const Rect rs = ChipRect(0, 0, Button::RStick, "", 28), xr = ChipRect(0, 0, Button::X, "", 28);
        const int y = top.y + top.h + 22, cx = top.x + top.w / 2;
        const int half = keys ? 0 : rs.w / 2;
        if (!keys) c.ui.push_back(ChipItem(cx - rs.w / 2, y, Button::RStick, "", 28));
        if (v.xyTurns)
        {
            c.ui.push_back(ChipItem(cx - half - (keys ? 8 : 16) - xr.w, y + (rs.h - xr.h) / 2, Button::X, "", 28));
            c.ui.push_back(ChipItem(cx + half + (keys ? 8 : 16), y + (rs.h - xr.h) / 2, Button::Y, "", 28));
        }
        // The game's own "L" / "R" pills on its arrows (the DS's shoulder buttons: LB / RB are the
        // pages here) covered by the pad's turn chips: X / Y off the Name page, else the right stick.
        // The top picture is SW x SH DS pixels at --res3d's n (n = topH / SH, the 3D resolution's
        // factor), wider with --wide (the 2D layer centred): a pill's DS rectangle is n times its size there.
        const float n = (float)topH / SH;
        const float sx = (float)top.w / topW * n, sy = (float)top.h / topH * n; // canvas pixels per DS pixel
        const float ox = (topW / n - SW) / 2;                                  // DS pixels left of the 2D layer
        for (int k = 0; k < 2; k++)
        {
            const Rect pill = kCasTurnPill[k];
            const Rect r {top.x + (int)((pill.x + ox) * sx) - 3, top.y + (int)(pill.y * sy) - 3, (int)(pill.w * sx) + 6, (int)(pill.h * sy) + 6};
            UiItem plate = PanelItem(r);
            plate.plain = true;
            plate.fill = th.frameDark;
            plate.radius = r.h / 2.0f;
            c.ui.push_back(plate);
            const Button b = v.xyTurns ? (k == 0 ? Button::X : Button::Y) : Button::RStick;
            if (keys && !v.xyTurns) continue; // (the keyboard on the Name page: the plate alone)
            const Rect cr = ChipRect(0, 0, b, "", 26);
            c.ui.push_back(ChipItem(r.x + (r.w - cr.w) / 2, r.y + (r.h - cr.h) / 2, b, "", 26));
        }
    }

    // Layer 0: the panel, the tab bar and its LB / RB, the title's well.
    UiItem panel = PanelItem(kCasPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem bar = WellItem(kCasTabBar);
    bar.fill = th.frameDark;
    bar.radius = 16;
    c.ui.push_back(bar);
    {
        const Rect lb = ChipRect(0, 0, Button::LB, "", 24);
        c.ui.push_back(ChipItem(kCasTabsX - 8 - lb.w, kCasTabBar.y + (kCasTabBar.h - lb.h) / 2, Button::LB, "", 24));
        c.ui.push_back(ChipItem(kCasTabsX + kCasPages * kCasTabCell + 8, kCasTabBar.y + (kCasTabBar.h - lb.h) / 2, Button::RB, "", 24));
    }
    UiItem title = WellItem(kCasTitle);
    title.radius = 14;
    c.ui.push_back(title);

    // The tabs: the game's own (live), the page's on a raised plate; the mouse onto the game's.
    for (int i = 0; i < kCasPages; i++)
    {
        const Rect cell {kCasTabsX + i * kCasTabCell, kCasTabBar.y + 4, kCasTabCell, kCasTabBar.h - 8};
        if (i == cs.page)
        {
            UiItem plate = PanelItem({cell.x + 2, cell.y, cell.w - 4, cell.h});
            plate.fill = kTabPlateFill;
            plate.borderArgb = th.fill;
            plate.border = 3;
            plate.radius = 16;
            plate.layer = 1;
            c.ui.push_back(plate);
        }
        Crop(images, bottom, Plus(kCasTabFrom, 0, kCasTabDy * i), 2, cell);
        if (!cs.switching) AddMousePiece(c, Plus(kCasTabFrom, 0, kCasTabDy * i), cell);
    }
    if (!v.title.empty()) PanelText(c, {kCasTitle.x + 20, kCasTitle.y, kCasTitle.w - 40, kCasTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Centre, Weight::ExtraBold);

    auto item = [&](int i) -> const ViewItem* { return i >= 0 && i < (int)v.items.size() ? &v.items[i] : nullptr; };
    auto focused = [&](int i) { return focus == i && MenuFocusable(v, i); };
    // An item's mouse piece: its HD rectangle onto the game's target box.
    auto mouse = [&](int i, Rect from) { if (item(i) && item(i)->enabled && !item(i)->action.empty()) AddMousePiece(c, from, v.items[i].rect); };

    if (!cs.switching) switch (cs.page)
    {
    case 0:
    {
        if (!cs.nameKnown || v.items.size() < 2 + 4 + 3 + kCasKeys + 1) break;
        // The name in the title's place: seven slots (the game's limit), the next one lit.
        const std::string shown = CasNameText(cs.name);
        std::vector<std::string> chars;
        for (size_t i = 0; i < shown.size(); i++)
        {
            const unsigned char b = (unsigned char)shown[i];
            const size_t n = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;   // (a UTF-8 letter whole)
            chars.push_back(shown.substr(i, n));
            i += n - 1;
        }
        for (int i = 0; i < kCasName; i++)
        {
            const Rect r {kCasSlotsX + i * kCasSlotStep, kCasTitle.y + 4, kCasSlotW, kCasTitle.h - 8};
            UiItem slot = PanelItem(r);
            slot.plain = true;
            slot.fill = kSlotFill;
            slot.radius = 8;
            slot.layer = 1;
            c.ui.push_back(slot);
            if (i == (int)chars.size())
            {
                UiItem caret = PanelItem({r.x + 8, r.y + r.h - 8, r.w - 16, 4});
                caret.plain = true;
                caret.fill = kSlotNext;
                caret.radius = 2;
                caret.layer = 1;
                c.ui.push_back(caret);
            }
            if (i < (int)chars.size()) Text(c, r, chars[i], 32);
        }
        // Gender and skin: the game's buttons and figures, live.
        for (int g = 0; g < 2; g++)
        {
            Cell(c, v.items[g].rect, focused(g), cs.gender == g, 42);
            Crop(images, bottom, kCasGenderFrom[g], 2, v.items[g].rect);
            mouse(g, kCasGenderFrom[g]);
        }
        for (int i = 0; i < 4; i++)
        {
            const int k = 2 + i;
            Cell(c, v.items[k].rect, focused(k), cs.skin == i);
            Crop(images, bottom, Plus(kCasSkinFrom, kCasSkinDx * i, 0), 2, v.items[k].rect);
            mouse(k, Plus(kCasSkinFrom, kCasSkinDx * i, 0));
        }
        // The keyboard's pages (the game's labels) and X's chip: the next one.
        const int kb = std::clamp(cs.keyboard, 0, 2);
        for (int p = 0; p < 3; p++)
        {
            const int k = 6 + p;
            Cell(c, v.items[k].rect, focused(k), kb == p, 16);
            Text(c, v.items[k].rect, kCasKbLabel[p], 28, Align::Centre, kb == p ? 0xFF8AE38A : 0xFFFFFFFF);
            mouse(k, {kCasKbX - 12, kCasKbY[p] - 12, 24, 24});
        }
        {
            const Rect xr = ChipRect(0, 0, Button::X, "", 26);
            UiItem x = ChipItem(CasKbPageRect(2).x + CasKbPageRect(2).w + 14, CasKbPageRect(2).y + (48 - xr.h) / 2, Button::X, "", 26);
            x.layer = 1;
            c.ui.push_back(x);
        }
        UiItem kbWell = WellItem({kCasKeysX - 10, kCasKeysY - 10, 9 * kCasKeyStep + 14, 4 * kCasKeyStep + 14});
        kbWell.radius = 16;
        kbWell.layer = 1;
        c.ui.push_back(kbWell);
        const bool full = cs.nameLength >= kCasName;
        for (int i = 0; i < kCasKeys; i++)
        {
            const int k = 9 + i;
            if (!v.items[k].enabled) continue;
            Cell(c, v.items[k].rect, focused(k), false, 12);
            Text(c, v.items[k].rect, CasKeyText(rom, kb, i), 30, Align::Centre, full ? kDim : 0xFFFFFFFF);
            mouse(k, Plus(kCasKeyBoxFrom, kCasKeyDx * (i % 9), kCasKeyDy * (i / 9)));
        }
        // Backspace: the game's ← (live) and Y's chip.
        const int k = 9 + kCasKeys;
        Cell(c, kCasBackRect, focused(k), false, 12);
        Crop(images, bottom, kCasBackFrom, 2, {kCasBackRect.x, kCasBackRect.y, kCasBackRect.w - 40, kCasBackRect.h});
        {
            const Rect yr = ChipRect(0, 0, Button::Y, "", 24);
            UiItem y = ChipItem(kCasBackRect.x + kCasBackRect.w - 12 - yr.w, kCasBackRect.y + (kCasBackRect.h - yr.h) / 2, Button::Y, "", 24);
            y.layer = 1;
            c.ui.push_back(y);
        }
        mouse(k, kCasBackFrom);
        break;
    }
    case 1:
    {
        if (!cs.hairKnown || v.items.size() < 12) break;
        for (int i = 0; i < 8; i++)
        {
            Cell(c, v.items[i].rect, focused(i), cs.hairColour == i, 12);
            Crop(images, bottom, Plus(kCasSwatchFrom, kCasSwatchDx * i, 0), 2, v.items[i].rect);
            mouse(i, Plus(kCasSwatchFrom, kCasSwatchDx * i, 0));
        }
        for (int cell = 0; cell < 4; cell++)
        {
            const int k = 8 + cell, col = cell & 1, row = cell >> 1;
            const bool chosen = cs.hairPos / 9 == row && cs.hairPos % 9 - cs.hairScroll == col;
            const Rect from = Plus(kCasHairFrom, kCasHairDx * col, kCasHairDy * row);
            Cell(c, v.items[k].rect, focused(k), chosen, 16);
            Crop(images, bottom, from, 3, v.items[k].rect);
            mouse(k, from);
        }
        // The game's arrows, where it shows them.
        if (cs.canLeft) { Crop(images, bottom, kCasHairLeftFrom, 2, kCasHairLeft); AddMousePiece(c, kCasHairLeftFrom, kCasHairLeft); }
        if (cs.canRight) { Crop(images, bottom, kCasHairRightFrom, 2, kCasHairRight); AddMousePiece(c, kCasHairRightFrom, kCasHairRight); }
        break;
    }
    case 2:
        if (v.items.size() < 9) break;
        for (int i = 0; i < 9; i++)
        {
            const Rect from = Plus(kCasFaceFrom, kCasFaceDx * (i % 3), kCasFaceDy * (i / 3));
            Cell(c, v.items[i].rect, focused(i), cs.face == i, 16);
            Crop(images, bottom, from, 3, v.items[i].rect);
            mouse(i, from);
        }
        break;
    case 3:
    {
        if (!cs.clothesKnown || v.items.size() < 14) break;
        for (int i = 0; i < 6; i++)
        {
            const Rect from {kCasCatFrom.x, kCasCatFromY[i], kCasCatFrom.w, kCasCatFrom.h};
            Cell(c, v.items[i].rect, focused(i), cs.category == i, 14);
            Crop(images, bottom, from, 2, v.items[i].rect);
            mouse(i, from);
        }
        for (int cell = 0; cell < 6; cell++)
        {
            const int k = 6 + cell;
            const Rect from = Plus(kCasClothFrom, kCasClothDx * (cell % 3), kCasClothDy * (cell / 3));
            Cell(c, v.items[k].rect, focused(k), cs.clothesCell == cell, 16);
            Crop(images, bottom, from, 3, v.items[k].rect);
            mouse(k, from);
        }
        for (int k = 12; k < 14 && k < (int)v.items.size(); k++)
        {
            if (!v.items[k].enabled) continue;
            const Rect from = k == 12 ? kCasClothLeftFrom : kCasClothRightFrom;
            Cell(c, v.items[k].rect, focused(k), false, 12);
            Crop(images, bottom, from, 2, v.items[k].rect);
            mouse(k, from);
        }
        break;
    }
    case 4:
    {
        if (!cs.keepKnown || v.items.size() < 6) break;
        for (int j = 0; j < 4; j++)
        {
            const Rect from = Plus(kCasAspFrom, kCasAspDx * j, 0);
            Cell(c, v.items[j].rect, focused(j), cs.aspiration == kCasAspOfSlot[j], 16);
            // The game's aspiration picture (362, its frames in this order), 1.5x; the others dimmed,
            // as the game shows them as silhouettes.
            if (icons)
                if (const SpriteFrame* f = icons->Scaled(SpriteRef::Of(kSprCasAsp), j, kCasAspIcon, kCasAspIcon))
                {
                    const Rect r = v.items[j].rect;
                    UiItem im = icons->Image(SpriteRef::Of(kSprCasAsp), j, *f, {r.x + (r.w - f->w) / 2, r.y + (r.h - f->h) / 2, f->w, f->h});
                    im.fill = cs.aspiration == kCasAspOfSlot[j] ? 0 : 0xA0000000;
                    images.push_back(im);
                }
            mouse(j, from);
        }
        // The chosen aspiration's name and what it gives.
        const int a = std::clamp(cs.aspiration, 0, 3);
        PanelText(c, kCasAspName, MenuWords(rom, kStrAsp0 + a), 32, 22, Align::Centre, Weight::ExtraBold, kWarm);
        UiItem well = WellItem(kCasAspText);
        well.radius = 16;
        well.layer = 1;
        c.ui.push_back(well);
        const Rect tb {kCasAspText.x + 20, kCasAspText.y + 14, kCasAspText.w - 40, kCasAspText.h - 28};
        const FittedBlock d = FitBlock(MenuWords(rom, kStrAspText0 + a), 28, 20, tb);
        UiItem dt = TextItem(tb, d.text, d.size, Align::Centre, Weight::Bold);
        dt.middle = true;
        dt.layer = 1;
        c.ui.push_back(dt);
        // The game's tick and cross, with its Yes / No.
        for (int k = 4; k < 6; k++)
        {
            const ViewItem& it = v.items[k];
            const Rect r = it.rect;
            if (focused(k)) AddFocusRing(c, r, r.h / 2.0f);
            UiItem b = PanelItem(r);
            b.fill = focused(k) ? kCellFocusFill : kCellFill;
            b.borderArgb = th.frame;
            b.border = 3;
            b.radius = r.h / 2.0f;
            b.layer = 1;
            c.ui.push_back(b);
            AddSpriteIcon(c, icons, k == 4 ? kSprCasTick : kSprCasCross, 0, 2, {r.x + 18, r.y + (r.h - 48) / 2, 48, 48}, it.enabled ? 0 : 0x80000000);
            PanelText(c, {r.x + 76, r.y, r.w - 92, r.h}, it.word, 32, 22, Align::Left, Weight::ExtraBold, it.enabled ? 0 : kDim);
            mouse(k, {(k == 4 ? kCasTickX : kCasCrossX) - 16, kCasAnswerY - 16, 32, 32});
        }
        break;
    }
    default: break;
    }

    // The hints: Start the next page; B "No" (Keep this Sim?: the cross); A with the item's word.
    if (!cs.switching)
    {
        int right = kCasHints.x + kCasHints.w - 4;
        if (!v.buttons[(int)MenuButton::Start].empty())
        {
            const FittedText w = FitLine(NextWord(rom, cs.page), kPmHintSize, 18, 220, Weight::ExtraBold);
            right = HintChip(c, right, Button::Start, w.text);
        }
        if (!v.back.empty()) right = HintChip(c, right, Button::B, MenuWords(rom, kStrCasNo));
        if (MenuFocusable(v, focus) && !v.items[focus].action.empty())
        {
            const FittedText w = FitLine(v.items[focus].word, kPmHintSize, 18, std::max(0, right - kCasHints.x - 60), Weight::ExtraBold);
            HintChip(c, right, Button::A, w.text);
        }
    }
    for (UiItem& im : images) c.ui.push_back(im);
    SortUi(c.ui, first);
}

void MeasureCas(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    for (int p = 1; p < kCasPages; p++)
    {
        const int id = p == 4 ? kStrCasKeep : kStrCasTitle0 + p;
        tally.Line(id, Title(rom, p), kPmTitleSize, kPmTextMin, kCasTitle.w - 40);
        tally.Line(kStrCasTitle0 + p, NextWord(rom, p - 1), kPmHintSize, 18, 220);   // (Start's hint: the next page)
    }
    for (int a = 0; a < 4; a++)
    {
        tally.Line(kStrAsp0 + a, MenuWords(rom, kStrAsp0 + a), 32, 22, kCasAspName.w);
        tally.Line(kStrAsp0 + a, MenuWords(rom, kStrAsp0 + a), kPmHintSize, 18, 200);   // (the A hint)
        tally.Block(kStrAspText0 + a, MenuWords(rom, kStrAspText0 + a), 28, 20, {0, 0, kCasAspText.w - 40, kCasAspText.h - 28});
    }
    for (int id : {kStrCasYes, kStrCasNo}) tally.Line(id, MenuWords(rom, id), 32, 22, kCasTick.w - 92);
    for (int p = 0; p < 3; p++)
        for (int i = 0; i < kCasKeyCount[p]; i++) tally.Line(kStrCasKeys[p] + i, CasKeyText(rom, p, i), 30, 30, kCasKeyW - 4);
    for (int id : {kStrCasYes, kStrCasNo}) tally.Line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 200);   // (the A / B hints)
}
}
