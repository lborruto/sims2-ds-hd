#include "port/build.h"

#include <algorithm>
#include <tuple>
#include <cmath>
#include <cstdlib>
#include <iterator>

#include "port/goals.h"
#include "port/icons.h"
#include "port/ownwords.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kWhereColour = 0xFFD8DAF4;                           // a room's place: a pale lavender
constexpr u32 kPriceColour = 0xFFFFE9A0;                           // a warm white: money (the catalogue's)
constexpr u32 kPoorColour = 0xFFFF6A6A;                            // the game's red for a permit the money does not cover
constexpr Rect kNoRect {};

// A button's hit area on the DS: the game's circles of radius 16 around its point.
Rect Around(int x, int y) { return {x - 16, y - 16, 32, 32}; }

std::string Money(const RomData& rom, int amount) { return amount >= 0 ? MenuWords(rom, kStrBdMoney, amount) : std::string(); }
std::string RoomName(const RomData& rom, int place) { return MenuWords(rom, kStrBdPlaceBase + place); }
std::string Where(const RomData& rom, int nameId) { return nameId >= 0 ? MenuWordsWith(rom, kStrBdWhere, MenuWords(rom, nameId)) : std::string(); }

// What is left after the permit, signed like InfoSum's receipt: "= §1,000", or "= -§500" when short.
std::string Left(const RomData& rom, int money, int price)
{
    const int left = money - price;
    return (left < 0 ? "= -" : "= ") + Money(rom, std::abs(left));
}

// The sum, one line: "§2,000 - §1,000 = §1,000" (the money, the permit, what is left, "= -§500"
// when short; numbers only: the game has no words for them).
std::string Sum(const RomData& rom, int money, int price) { return Money(rom, money) + " - " + Money(rom, price) + " " + Left(rom, money, price); }

void Text(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, Weight w, u32 colour = 0) { PanelText(c, r, s, size, minSize, a, w, colour); }

// A room's name over its place, left in `r` (a row or the chosen plate), its permit at the right.
void RoomLines(Composition& c, const RomData& rom, const BuildEntry& e, Rect r, int priceW, bool focused = false)
{
    const int x = r.x + 84, w = r.w - 84 - priceW - 16;
    Text(c, {x, r.y + r.h / 2 - 48, w, 52}, RoomName(rom, e.place), kBdNameSize, kBdNameMin, Align::Left, Weight::ExtraBold);
    Text(c, {x, r.y + r.h / 2 + 4, w, 40}, Where(rom, e.parentNameId), kBdWhereSize, kBdWhereMin, Align::Left, Weight::Bold, focused ? 0xFFFFFFFF : kWhereColour);
    Text(c, {r.x + r.w - priceW - 16, r.y, priceW, r.h}, Money(rom, e.price), kBdPriceSize, 20, Align::Right, Weight::ExtraBold,
         e.affordable ? kPriceColour : kPoorColour);
}

// A room's name wrapped in the info well, at the largest size (30 down to 22) where its lines fit
// and no word is broken between two letters; a hyphenated word may break after its hyphen
// ("Deluxe-" / "Hotelzimmer"). fits false: none did (the smallest, broken).
FittedBlock NameBlock(const std::string& name, Rect box)
{
    std::string split = name;
    for (size_t i = 0; (i = split.find('-', i)) != std::string::npos; i += 2) split.insert(i + 1, " ");
    auto wordsFit = [&](const std::string& t, float size) {
        for (size_t a = 0; a <= t.size();)
        {
            const size_t b = std::min(t.find(' ', a), t.size());
            if (TextWidth(t.substr(a, b - a), size, Weight::ExtraBold) > box.w) return false;
            a = b + 1;
        }
        return true;
    };
    for (float size = kBdNameSize; size >= 22; size -= 1)
        for (const std::string* t : {&name, (const std::string*)&split})
            if (wordsFit(*t, size))
                if (FittedBlock f = FitBlock(*t, size, size, box); f.fits) return f;
    FittedBlock f = FitBlock(split, 22, 22, box);
    f.fits = false;
    return f;
}

// The sum in the info well (page 0), as a receipt: the money, minus the permit, a rule, what is left.
void InfoSum(Composition& c, const RomData& rom, const BuildEntry& e, int money)
{
    const Theme& th = GameTheme();
    const int x = kBdInfo.x + 20, w = kBdInfo.w - 40;
    const FittedBlock name = NameBlock(RoomName(rom, e.place), {x, kBdInfo.y + 16, w, 84});
    UiItem nt = TextItem({x, kBdInfo.y + 16, w, 84}, name.text, name.size, Align::Left, Weight::ExtraBold);
    nt.layer = 1;
    c.ui.push_back(nt);
    const int wy = kBdInfo.y + 16 + std::min(84, (int)std::lround(name.lines * name.size * 1.3f)) + 4; // (under the name's lines)
    const FittedBlock where = FitBlock(Where(rom, e.parentNameId), kBdWhereSize, kBdWhereMin, {x, wy, w, 64});
    UiItem wt = TextItem({x, wy, w, 64}, where.text, where.size, Align::Left, Weight::Bold);
    wt.colour = kWhereColour;
    wt.layer = 1;
    c.ui.push_back(wt);
    const int y = kBdInfo.y + 200;
    Text(c, {x, y, w, 44}, Money(rom, money), kBdPriceSize, 20, Align::Right, Weight::ExtraBold);
    Text(c, {x, y + 48, w, 44}, "- " + Money(rom, e.price), kBdPriceSize, 20, Align::Right, Weight::ExtraBold, e.affordable ? kPriceColour : kPoorColour);
    UiItem rule = PanelItem({x, y + 100, w, 4});
    rule.plain = true;
    rule.fill = th.frameDark;
    rule.radius = 2;
    rule.layer = 1;
    c.ui.push_back(rule);
    // What is left; short (the game refuses the room): the shortfall, red.
    Text(c, {x, y + 112, w, 48}, Left(rom, money, e.price), 32, 20, Align::Right, Weight::ExtraBold, money < e.price ? kPoorColour : kPriceColour);
}

// A page-1 button: a raised plate, the game's sprite 2x and its word.
void AskButton(Composition& c, Rect r, bool focused, const std::string& word)
{
    const Theme& th = GameTheme();
    if (focused) AddFocusRing(c, r, 24);
    UiItem b = PanelItem(r);
    b.fill = focused ? kFillFocus : th.frame;
    b.borderArgb = th.frameDark;
    b.border = 3;
    b.radius = 24;
    b.layer = 1;
    c.ui.push_back(b);
    Text(c, {r.x + 80, r.y, r.w - 96, r.h}, word, kBdNameSize, 20, Align::Left, Weight::ExtraBold);
}
Rect AskIcon(Rect r) { return {r.x + 16, r.y + (r.h - 56) / 2, 56, 56}; }
}

Rect BuildRowRect(int i) { return {kBdList.x + 16, kBdList.y + 16 + i * 144, 512, 132}; }

MenuView BuildRoomsView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrBdAsk) return {};
    return BuildRoomsViewOf(s, *game, rom);
}

MenuView BuildRoomsViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const BuildState& b = game.build;
    if (s.screen != Screen::Build || !game.valid || !b.valid) return v;
    if (rom) v.title = MenuWords(*rom, kStrBdTitle);
    v.page = b.page;
    v.question = b.page == 1;
    const int count = (int)b.places.size();
    if (b.page == 0)
    {
        // The rows the game shows (three from its scroll); a room the money does not cover is
        // focusable (its sum is read) but A does nothing (the game refuses it).
        const int shown = std::clamp(count - b.scroll, 0, kBdRows);
        for (int i = 0; i < shown; i++)
        {
            const BuildEntry& e = b.places[b.scroll + i];
            ViewItem it;
            it.rect = BuildRowRect(i);
            it.group = 1;
            it.id = kBdRowId + (u32)e.place;
            if (e.affordable)
            {
                it.action = {TapStep(kBdRowX, kBdRowY0 + i * kBdRowDy)};
                if (rom) it.word = MenuWords(*rom, kStrBdBuy);
            }
            it.next[0] = i == 0 ? kNoMove : i - 1;
            it.next[1] = i == shown - 1 ? kNoMove : i + 1;
            it.next[2] = it.next[3] = kNoMove;
            v.items.push_back(it);
        }
        // The game's own scroll (its arrows), while it can move.
        if (b.scroll > 0) v.scrollUp = {TapStep(kBdUpX, kBdArrowY)};
        if (b.scroll + kBdRows < count) v.scrollDown = {TapStep(kBdDownX, kBdArrowY)};
        v.scrollGroup = 1;
        v.back = {TapStep(kBdCloseX, kBdCloseY)};
        v.backCloses = true;
    }
    else
    {
        // "Are you sure you want to buy this room?": the cross, the tick; the first focus the cross
        // (No): a spend is never one press from the question.
        ViewItem cross;
        cross.rect = kBdCross;
        cross.id = kBdCrossId;
        cross.action = {TapStep(kBdCrossX, kBdConfirmY)};
        cross.next[0] = cross.next[1] = kNoMove;
        if (rom) cross.word = MenuWords(*rom, kStrBdNo);
        v.items.push_back(cross);
        ViewItem tick;
        tick.rect = kBdTick;
        tick.id = kBdTickId;
        tick.action = {TapStep(kBdTickX, kBdConfirmY)};
        tick.next[0] = tick.next[1] = kNoMove;
        if (rom) tick.word = MenuWords(*rom, kStrBdYes);
        v.items.push_back(tick);
        v.initial = 0;
        v.back = cross.action; // (back to the list: not a close)
    }
    v.replay = false; // (a late press could buy: never replayed)
    v.own = true;
    v.valid = true;
    return v;
}

void BuildBuildRooms(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons)
{
    const Theme& th = GameTheme();
    const BuildState& b = game.build;
    const size_t first = c.ui.size();
    const bool ask = b.page == 1;
    if (ask && (v.items.size() != 2 || b.chosen < 0 || b.chosen >= (int)b.places.size())) return;

    // Layer 0: the panel, the title's well, the page's wells.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    for (Rect r : {kBdTitle, ask ? kBdAsk : kBdList, ask ? kNoRect : kBdInfo})
    {
        if (r.w <= 0) continue;
        UiItem w = WellItem(r);
        w.radius = 16;
        c.ui.push_back(w);
    }

    // Layer 1: the title and the money (the catalogue's plate), the page, the hints.
    const std::string money = Money(rom, b.money);
    const int moneyW = (int)TextWidth(money, kPmTitleSize, Weight::ExtraBold);
    const Rect moneyBox {kBdClose.x - 20 - moneyW - 32, kBdTitle.y + 10, moneyW + 32, kBdTitle.h - 20};
    {
        UiItem plate = PanelItem(moneyBox);
        plate.fill = th.frameDark;
        plate.raised = false;
        plate.border = 0;
        plate.radius = moneyBox.h / 2.0f;
        plate.layer = 1;
        c.ui.push_back(plate);
        Text(c, moneyBox, money, kPmTitleSize, kPmTitleSize, Align::Centre, Weight::ExtraBold, kPriceColour);
        Text(c, {kBdTitle.x + 24, kBdTitle.y, moneyBox.x - 16 - (kBdTitle.x + 24), kBdTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Left, Weight::ExtraBold);
    }

    std::vector<Rect> knobs; // (images: the rows' knobs)
    if (!ask)
    {
        const int shown = (int)v.items.size();
        for (int i = 0; i < shown; i++)
        {
            const BuildEntry& e = b.places[b.scroll + i];
            const Rect r = v.items[i].rect;
            const bool focused = focus == i;
            if (focused) AddFocusRing(c, r, 24);
            UiItem row = PanelItem(r);
            row.fill = focused ? kFillFocus : kFillRow;
            row.borderArgb = th.frame;
            row.border = 3;
            row.radius = 24;
            row.raised = e.affordable;
            row.layer = 1;
            c.ui.push_back(row);
            RoomLines(c, rom, e, r, 150, focused);
            knobs.push_back({r.x + 12, r.y + (r.h - 56) / 2, 56, 56});
            AddMousePiece(c, Around(kBdRowX, kBdRowY0 + i * kBdRowDy), r);
        }
        AddScrollBar(c, kBdScroll, b.scroll, kBdRows, (int)b.places.size());
        // The arrows (the kit's: every list's look), sunk when the game cannot scroll that way.
        for (auto [r, dir, on] : {std::tuple {kBdUp, UiItem::Sign::Up, !v.scrollUp.empty()}, std::tuple {kBdDown, UiItem::Sign::Down, !v.scrollDown.empty()}})
        {
            UiItem a = ScrollArrowItem(r, dir, on);
            a.layer = 1;
            c.ui.push_back(a);
        }
        AddMousePiece(c, Around(kBdUpX, kBdArrowY), kBdUp);
        AddMousePiece(c, Around(kBdDownX, kBdArrowY), kBdDown);
        // The sum of the focused room (none focused: the first row's).
        const int f = focus >= 0 && focus < shown ? focus : shown > 0 ? 0 : -1;
        if (f >= 0) InfoSum(c, rom, b.places[b.scroll + f], b.money);
    }
    else
    {
        const BuildEntry& e = b.places[b.chosen];
        const FittedBlock q = FitBlock(MenuWords(rom, kStrBdAsk), 30, 22, {kBdAsk.x + 40, kBdAsk.y + 18, kBdAsk.w - 80, 90});
        UiItem qt = TextItem({kBdAsk.x + 40, kBdAsk.y + 18, kBdAsk.w - 80, 90}, q.text, q.size, Align::Centre, Weight::ExtraBold);
        qt.middle = true;
        qt.layer = 1;
        c.ui.push_back(qt);
        UiItem plate = PanelItem(kBdChosen);
        plate.fill = kFillRow;
        plate.borderArgb = th.frame;
        plate.border = 3;
        plate.radius = 24;
        plate.layer = 1;
        c.ui.push_back(plate);
        RoomLines(c, rom, e, kBdChosen, 170);
        knobs.push_back({kBdChosen.x + 12, kBdChosen.y + (kBdChosen.h - 56) / 2, 56, 56});
        Text(c, {kBdAsk.x + 40, kBdChosen.y + kBdChosen.h + 20, kBdAsk.w - 80, 48}, Sum(rom, b.money, e.price), 30, 20, Align::Centre, Weight::ExtraBold,
             e.affordable ? kPriceColour : kPoorColour);
        AskButton(c, kBdCross, focus == 0, v.items[0].word);
        AskButton(c, kBdTick, focus == 1, v.items[1].word);
        AddMousePiece(c, Around(kBdCrossX, kBdConfirmY), kBdCross);
        AddMousePiece(c, Around(kBdTickX, kBdConfirmY), kBdTick);
    }
    AddMousePiece(c, Around(kBdCloseX, kBdCloseY), kBdClose);

    // The hints: B (the list: "Close", the game's X; page 1: "No", its cross); A with the focused
    // item's word (page 1's Yes / No; a row: the chip alone, none when the game refuses it); the
    // D-pad's vertical arms while the list scrolls.
    int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, ask ? kStrBdNo : kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) right = AddHint(c, right, Button::A, v.items[focus].word);
    if (!ask && !(v.scrollUp.empty() && v.scrollDown.empty())) AddHint(c, right, Button::DPadV, OwnWord(&rom, OwnWordId::HintChoose));

    // Images: the knobs, page 1's cross and tick, the X.
    for (Rect k : knobs) AddTiered(c.ui, icons, SpriteRef::Of(kSprBdKnob), 0, kTierS, k);
    if (ask)
    {
        AddTiered(c.ui, icons, SpriteRef::Of(kSprBdX), 0, kTierS, AskIcon(kBdCross));
        AddTiered(c.ui, icons, SpriteRef::Of(kSprBdTick), 0, kTierS, AskIcon(kBdTick));
    }
    AddTiered(c.ui, icons, SpriteRef::Of(kSprBdX), 0, kTierS, kBdClose);

    SortUi(c.ui, first);
}

void MeasureBuild(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) { tally.Note(id, shrunk, fits, lines); };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) { tally.Line(id, text, size, minSize, width, w); };
    auto block = [&](int id, const std::string& text, float size, float minSize, Rect box) { tally.Block(id, text, size, minSize, box); };
    // Every room the game can list (the static room table's fourteen with a permit, in RAM:
    // kBdBuildable) in a row, the chosen plate and the sum; "(@1)" with every place's name (the
    // table's parents are the Atrium, the Basement and the 2nd Floor Lobby; any place is measured).
    for (int place = 0; place < kPlaces; place++)
    {
        const int id = kStrBdPlaceBase + place;
        const bool room = std::find(std::begin(kBdBuildable), std::end(kBdBuildable), place) != std::end(kBdBuildable);
        const std::string name = RoomName(rom, place), where = Where(rom, id);
        for (int priceW : {150, 170})
        {
            const Rect r = priceW == 150 ? BuildRowRect(0) : kBdChosen;
            const int w = r.w - 84 - priceW - 16;
            if (room) line(id, name, kBdNameSize, kBdNameMin, w);
            line(id, where, kBdWhereSize, kBdWhereMin, w, Weight::Bold);   // (noted under the place: each "(@1)" differs)
        }
        if (room)
        {
            const FittedBlock f = NameBlock(name, {0, 0, kBdInfo.w - 40, 84});
            note(id, f.size < kBdNameSize, f.fits, f.lines);
        }
        block(id, where, kBdWhereSize, kBdWhereMin, {0, 0, kBdInfo.w - 40, 64});
    }
    // The title beside the widest money (999,999, the game's cap); the prices (a permit is a u16);
    // the sums; the question; the buttons and the hints.
    const int moneyW = (int)TextWidth(Money(rom, 999999), kPmTitleSize, Weight::ExtraBold) + 32;
    line(kStrBdTitle, MenuWords(rom, kStrBdTitle), kPmTitleSize, kPmTextMin, kBdClose.x - 20 - moneyW - 16 - (kBdTitle.x + 24));
    line(kStrBdMoney, Money(rom, 65535), kBdPriceSize, 20, 150);
    line(kStrBdMoney, "- " + Money(rom, 65535), kBdPriceSize, 20, kBdInfo.w - 40);
    line(kStrBdMoney, "= " + Money(rom, 999999), 32, 20, kBdInfo.w - 40);
    line(kStrBdMoney, "= -" + Money(rom, 65535), 32, 20, kBdInfo.w - 40);
    line(kStrBdMoney, Sum(rom, 999999, 65535), 30, 20, kBdAsk.w - 80);
    block(kStrBdAsk, MenuWords(rom, kStrBdAsk), 30, 22, {0, 0, kBdAsk.w - 80, 90});
    line(kStrBdBuy, MenuWords(rom, kStrBdBuy), kPmHintSize, kPmHintSize, 300);
    for (int id : {kStrBdYes, kStrBdNo})
    {
        line(id, MenuWords(rom, id), kBdNameSize, 20, kBdCross.w - 96);
        line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 300);
    }
}
}
