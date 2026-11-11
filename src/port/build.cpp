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

constexpr u32 kWhereColour = 0xFFFFFFFF;                           // a room's place: white, as every information (4.58:1 on a row)
constexpr u32 kPriceColour = 0xFFFFE9A0;                           // a warm white: money (the catalogue's; 3.80:1 on a row, large text)
// A permit the money does not cover: a light red (the status bar's loss) on a dark plate as wide as
// the words (the theme's shadow, #383869): 5.30:1 (the game's red #FF6A6A on a row was 1.64:1).
constexpr u32 kPoorColour = 0xFFFF9A90, kPoorPlate = 0xFF383869;
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

// A sum in money's colour, or (poor) in the loss's red on its dark plate, one line in r (ExtraBold).
void MoneyText(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, bool poor)
{
    if (poor)
    {
        const FittedText f = FitLine(s, size, minSize, r.w, Weight::ExtraBold);
        const int tw = (int)std::ceil(TextWidth(f.text, f.size, Weight::ExtraBold)), h = (int)f.size + 14;
        const int x = a == Align::Right ? r.x + r.w - tw : a == Align::Centre ? r.x + (r.w - tw) / 2 : r.x;
        UiItem p = PanelItem({x - 12, r.y + (r.h - h) / 2, tw + 24, h});
        p.plain = true;
        p.fill = kPoorPlate;
        p.radius = h / 2.0f;
        p.layer = 1;
        c.ui.push_back(p);
    }
    Text(c, r, s, size, minSize, a, Weight::ExtraBold, poor ? kPoorColour : kPriceColour);
}

// A room's name over its place, left in `r` (a row or the chosen plate), its permit at the right.
void RoomLines(Composition& c, const RomData& rom, const BuildEntry& e, Rect r, int priceW)
{
    const int x = r.x + 84, w = r.w - 84 - priceW - 16;
    Text(c, {x, r.y + r.h / 2 - 48, w, 52}, RoomName(rom, e.place), kBdNameSize, kBdNameMin, Align::Left, Weight::ExtraBold);
    Text(c, {x, r.y + r.h / 2 + 4, w, 40}, Where(rom, e.parentNameId), kBdWhereSize, kBdWhereMin, Align::Left, Weight::Bold, kWhereColour);
    MoneyText(c, {r.x + r.w - priceW - 16, r.y, priceW, r.h}, Money(rom, e.price), kBdPriceSize, 20, Align::Right, !e.affordable);
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
    MoneyText(c, {x, y + 48, w, 44}, "- " + Money(rom, e.price), kBdPriceSize, 20, Align::Right, !e.affordable);
    UiItem rule = PanelItem({x, y + 100, w, 4});
    rule.plain = true;
    rule.fill = th.frameDark;
    rule.radius = 2;
    rule.layer = 1;
    c.ui.push_back(rule);
    // What is left; short (the game refuses the room): the shortfall, red.
    MoneyText(c, {x, y + 112, w, 48}, Left(rom, money, e.price), 32, 20, Align::Right, money < e.price);
}

// Page 1's room line: its name and its place ("Art Gallery (Atrium)").
std::string RoomLine(const RomData& rom, const BuildEntry& e)
{
    const std::string w = Where(rom, e.parentNameId);
    return w.empty() ? RoomName(rom, e.place) : RoomName(rom, e.place) + " " + w;
}
// The question box's well (popup.cpp's BoxBase with buttons).
Rect AskWell() { return {kBdAskBox.x + 28, kBdAskBox.y + 42, kBdAskBox.w - 56, kBdAskBox.h - 162}; }
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
    // The rows the list shows (page 1: the list under the question, without a focus).
    const int count = (int)b.places.size(), shown = std::clamp(count - b.scroll, 0, kBdRows);
    const bool empty = count == 0; // (nothing left to build: the port's line in one wide well)

    // Layer 0: the panel, the title's well, the list's and the info's wells.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    for (Rect r : {kBdTitle, empty ? kBdEmpty : kBdList, empty ? kNoRect : kBdInfo})
    {
        if (r.w <= 0) continue;
        UiItem w = WellItem(r);
        w.radius = 16;
        c.ui.push_back(w);
    }

    // Layer 1: the title and the money (the catalogue's plate), the list, the hints.
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
    const int listFocus = ask ? -1 : focus;
    for (int i = 0; i < shown; i++)
    {
        const BuildEntry& e = b.places[b.scroll + i];
        const Rect r = BuildRowRect(i);
        const bool focused = listFocus == i;
        if (focused) AddFocusRing(c, r, 24);
        UiItem row = PanelItem(r);
        row.fill = focused ? kFillFocus : kFillRow;
        row.borderArgb = th.frame;
        row.border = 3;
        row.radius = 24;
        row.raised = e.affordable;
        row.layer = 1;
        c.ui.push_back(row);
        RoomLines(c, rom, e, r, 150);
        knobs.push_back({r.x + 12, r.y + (r.h - 56) / 2, 56, 56});
        AddMousePiece(c, Around(kBdRowX, kBdRowY0 + i * kBdRowDy), r);
    }
    if (empty)
        PanelText(c, {kBdEmpty.x + 24, kBdEmpty.y, kBdEmpty.w - 48, kBdEmpty.h}, OwnWord(&rom, OwnWordId::BuildNothingLeft), kBdNameSize, 22, Align::Centre,
                  Weight::ExtraBold, 0xFFFFFFFF);
    else
    {
        // The arrows and the bar (every list's column), sunk when the game cannot scroll that way.
        AddScrollColumn(c, {kBdUp.x, kBdUp.y, kBdUp.w, kBdDown.y + kBdDown.h - kBdUp.y}, b.scroll, kBdRows, count, !ask && !v.scrollUp.empty(),
                        !ask && !v.scrollDown.empty());
        AddMousePiece(c, Around(kBdUpX, kBdArrowY), kBdUp);
        AddMousePiece(c, Around(kBdDownX, kBdArrowY), kBdDown);
        // The sum of the focused room (none focused: the first row's; page 1: the chosen room's).
        const int f = listFocus >= 0 && listFocus < shown ? listFocus : shown > 0 ? 0 : -1;
        if (ask) InfoSum(c, rom, b.places[b.chosen], b.money);
        else if (f >= 0) InfoSum(c, rom, b.places[b.scroll + f], b.money);
    }
    AddMousePiece(c, Around(kBdCloseX, kBdCloseY), kBdClose);

    // The hints (the list's; page 1's are its buttons' chips, as every question's): B "Close"
    // (the game's X), A with the focused room's word (the chip alone, none when the game refuses
    // it), the D-pad's vertical arms ("Choose", as every list's).
    if (!ask)
    {
        int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
        if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) right = AddHint(c, right, Button::A, v.items[focus].word);
        if (shown > 1 || count > shown) AddHint(c, right, Button::DPadV, OwnWord(&rom, OwnWordId::HintChoose));
    }

    // Images: the knobs, the X.
    for (Rect k : knobs) AddTiered(c.ui, icons, SpriteRef::Of(kSprBdKnob), 0, kTierS, k);
    AddTiered(c.ui, icons, SpriteRef::Of(kSprBdX), 0, kTierS, kBdClose);
    SortUi(c.ui, first);
}

void BuildBuildQuestion(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom)
{
    // Page 1: "Are you sure you want to buy this room?" in the popups' look over the dimmed list:
    // the question, the room (its name and place) and the sum, then No (the first focus) and Yes.
    const BuildState& b = game.build;
    if (b.page == 1 && v.items.size() == 2 && b.chosen >= 0 && b.chosen < (int)b.places.size())
    {
        const BuildEntry& e = b.places[b.chosen];
        BuildQuestionBox(c, kBdAskBox, MenuWords(rom, kStrBdAsk), kBdAskTextH, focus, rom, Around(kBdCrossX, kBdConfirmY), Around(kBdTickX, kBdConfirmY));
        const Rect well = AskWell();
        const size_t at = c.ui.size();
        Text(c, {well.x + 24, well.y + 10 + kBdAskTextH + 4, well.w - 48, 44}, RoomLine(rom, e), 28, 20, Align::Centre, Weight::ExtraBold);
        Text(c, {well.x + 24, well.y + 10 + kBdAskTextH + 52, well.w - 48, 48}, Sum(rom, b.money, e.price), 30, 20, Align::Centre, Weight::ExtraBold,
             kPriceColour);
        for (size_t k = at; k < c.ui.size(); k++) c.ui[k].layer = kPopLayer;
    }
}

void MeasureBuild(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) { tally.Note(id, shrunk, fits, lines); };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) { tally.Line(id, text, size, minSize, width, w); };
    auto block = [&](int id, const std::string& text, float size, float minSize, Rect box) { tally.Block(id, text, size, minSize, box); };
    // Every room the game can list (the static room table's fourteen with a permit, in RAM:
    // kBdBuildable) in a row and the info, and in the question's room line beside the widest place;
    // "(@1)" with every place's name (the table's parents are the Atrium, the Basement and the 2nd
    // Floor Lobby; any place is measured).
    std::string widestWhere;
    for (int place = 0; place < kPlaces; place++)
    {
        const std::string w = Where(rom, kStrBdPlaceBase + place);
        if (TextWidth(w, 28, Weight::ExtraBold) > TextWidth(widestWhere, 28, Weight::ExtraBold)) widestWhere = w;
    }
    const int askW = AskWell().w - 48;
    for (int place = 0; place < kPlaces; place++)
    {
        const int id = kStrBdPlaceBase + place;
        const bool room = std::find(std::begin(kBdBuildable), std::end(kBdBuildable), place) != std::end(kBdBuildable);
        const std::string name = RoomName(rom, place), where = Where(rom, id);
        const int w = BuildRowRect(0).w - 84 - 150 - 16;
        if (room) line(id, name, kBdNameSize, kBdNameMin, w);
        line(id, where, kBdWhereSize, kBdWhereMin, w, Weight::Bold);   // (noted under the place: each "(@1)" differs)
        if (room) line(id, name + " " + widestWhere, 28, 20, askW);
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
    line(kStrBdMoney, Sum(rom, 999999, 65535), 30, 20, askW);
    block(kStrBdAsk, MenuWords(rom, kStrBdAsk), 30, 20, {0, 0, askW, kBdAskTextH}); // (as the popups' text: 30 down to 20)
    line(kStrBdBuy, MenuWords(rom, kStrBdBuy), kPmHintSize, kPmHintSize, 300);
    // (Yes / No: the popups' buttons, MeasurePopups.) Nothing left to build: the port's line.
    line(9000 + (int)OwnWordId::BuildNothingLeft, OwnWord(&rom, OwnWordId::BuildNothingLeft), kBdNameSize, 22, kBdEmpty.w - 48);
}
}
