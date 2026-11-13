#include "port/roominfo.h"

#include <algorithm>

#include "port/icons.h"
#include "port/ownwords.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "port/popup.h"
#include "romdata.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2hd
{
namespace
{
using namespace ui;

constexpr u32 kValueColour = 0xFFFFE9A0;                              // a warm white (the money's)
constexpr u32 kPortraitWell = 0xFF5961A2;

std::string RoomName(const RomData& rom, int place) { return MenuWords(rom, kStrRiPlaceBase + place); }
std::string Money(const RomData& rom, int amount) { return MenuWords(rom, kStrRiMoney, std::max(0, amount)); }

std::string Trim(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == ':')) s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

// A line of the game's as a label and a value: the string up to its first placeholder is the
// label, the rest (its placeholders filled) the value; a string without one (2461) splits after
// its last colon.
void Split(const RomData& rom, int id, const std::vector<std::string>& args, std::string& label, std::string& value)
{
    const std::string t = rom.String(id);
    size_t at = t.find('@');
    if (at == std::string::npos)
    {
        const size_t colon = t.rfind(':');
        at = colon == std::string::npos ? t.size() : colon + 1;
    }
    std::string rest;
    for (size_t i = at; i < t.size(); i++)
    {
        if (t[i] == '@' && i + 1 < t.size() && t[i + 1] >= '1' && t[i + 1] <= '9')
        {
            const size_t n = (size_t)(t[i + 1] - '1');
            if (n < args.size()) rest += args[n];
            i++;
        }
        else rest += t[i];
    }
    label = MenuText(t.substr(0, at));
    value = MenuText(rest);
}

struct Line
{
    std::string label, value;
    bool shown = false;
};
// The game's lines for this room (index as kRiLineY); line 0 is the name (the title band).
void Lines(const RoomInfoState& ri, const RomData& rom, Line out[kRiLines])
{
    out[0] = {RoomName(rom, ri.place), "", true};
    if (ri.occupied)
    {
        Split(rom, kStrRiGuest, {MenuWords(rom, kSimNameBase + ri.occupant)}, out[1].label, out[1].value);
        // (the mood is the game's @2)
        Split(rom, kStrRiMood, {"", ri.moodId >= 0 ? MenuWords(rom, ri.moodId) : std::string()}, out[2].label, out[2].value);
        if (ri.checkOutKnown)
            Split(rom, kStrRiOut, {std::to_string(ri.month), std::to_string(ri.day), std::to_string(ri.days)}, out[3].label, out[3].value);
        else Split(rom, kStrRiOutUnknown, {}, out[3].label, out[3].value);
        out[1].shown = out[2].shown = out[3].shown = true;
    }
    else out[1] = {MenuWords(rom, kStrRiVacant), "", true};
    const int ids[4] = {kStrRiBase, kStrRiService, kStrRiBonus, kStrRiTotal};
    const int values[4] = {ri.baseValue, ri.serviceCharge, ri.scoreBonus, ri.total};
    for (int k = 0; k < 4; k++) out[4 + k] = {MenuWords(rom, ids[k]), Money(rom, values[k]), true};
}

int HintOf(const RoomInfoState& ri, int line) { return line == 1 && !ri.occupied ? kRiHintVacant : kRiHint[line]; }

// The lines' view (what the hint box is drawn over too).
MenuView LinesView(const GameState& game, const RomData* rom)
{
    MenuView v;
    const RoomInfoState& ri = game.roomInfo;
    if (rom) v.title = RoomName(*rom, ri.place);
    std::vector<int> shown;
    for (int i = 0; i < kRiLines; i++)
        if (i < 2 || i >= 4 || ri.occupied) shown.push_back(i);
    for (size_t k = 0; k < shown.size(); k++)
    {
        const int i = shown[k];
        ViewItem it;
        it.rect = RoomInfoLineRect(i, ri.occupied);
        it.id = RoomInfoLineId(ri.place, i);
        it.action = {TapStep(kRiLineX, kRiLineY[i] + 6)};
        if (rom) it.word = MenuWords(*rom, kStrRiHints);
        it.next[0] = k == 0 ? kNoMove : (int)k - 1;
        it.next[1] = k + 1 == shown.size() ? kNoMove : (int)k + 1;
        it.next[2] = it.next[3] = kNoMove;
        v.items.push_back(it);
    }
    v.initial = 1; // (the guest's line: what the room is about)
    v.back = {TapStep(kRiCloseX, kRiCloseY)};
    v.own = true;
    v.valid = true;
    return v;
}

void Text(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, Weight w, u32 colour = 0) { PanelText(c, r, s, size, minSize, a, w, colour); }
// A line's label and value share its width: the value's own width first (at most half).
int ValueWidth(const std::string& value, int w) { return value.empty() ? 0 : std::min(w / 2, (int)TextWidth(value, kRiValueSize, Weight::ExtraBold) + 4); }
}

Rect RoomInfoLineRect(int i, bool occupied)
{
    if (i <= 0) return {kRiTitle.x + 12, kRiTitle.y + 6, kRiClose.x - 16 - (kRiTitle.x + 12), kRiTitle.h - 12};
    const int slot = occupied || i < 2 ? i : i - 2;   // (a vacant room's costs follow "Vacant")
    return {kRiLinesWell.x + 16, kRiLinesWell.y + 12 + (slot - 1) * 62, kRiLinesWell.w - 32, 54};
}

int RoomInfoHintAt(int x, int y, bool occupied)
{
    if (x < 15) return -1;
    for (int i = 0; i < kRiLines; i++)
    {
        if (i >= 2 && i <= 3 && !occupied) continue;
        const int right = i < 4 ? 75 : 135;
        if (x <= right && y >= kRiLineY[i] && y <= kRiLineY[i] + 12) return i == 1 && !occupied ? kRiHintVacant : kRiHint[i];
    }
    return -1;
}

bool RoomHintUp(const GameState& game) { return game.valid && game.roomInfo.valid && game.roomInfo.hint; }

MenuView RoomInfoView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrRiHints || rom->StringCount() <= kRiHintMax) return {};
    return RoomInfoViewOf(s, *game, rom);
}

MenuView RoomInfoViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    const RoomInfoState& ri = game.roomInfo;
    if (s.screen != Screen::Unknown || !game.valid || !ri.valid || ri.place < 0 || ri.place >= kPlaces) return {};
    if (!ri.hint) return LinesView(game, rom);
    // The hint box: the game closes it on the next stylus-down anywhere; a tap away from the lines.
    MenuView v;
    if (ri.hintId < 0) return v; // (which hint is not known: the framed screens)
    ViewItem close;
    close.rect = kPopClose;
    close.id = kRiHintCloseId;
    close.action = {TapStep(kRiAwayX, kRiAwayY)};
    if (rom) close.word = MenuWords(*rom, kStrClose);
    close.next[0] = close.next[1] = close.next[2] = close.next[3] = kNoMove;
    v.items.push_back(close);
    v.back = close.action;
    v.own = v.popup = true;
    v.valid = true;
    return v;
}

void BuildRoomInfo(Composition& c, const MenuView& v0, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom)
{
    const Theme& th = GameTheme();
    const RoomInfoState& ri = game.roomInfo;
    if (!ri.valid) return;
    // A hint up: the lines beneath it, without a focus.
    const MenuView lv = ri.hint ? LinesView(game, &rom) : MenuView {};
    const MenuView& v = ri.hint ? lv : v0;
    if (ri.hint) focus = -1;
    const size_t first = c.ui.size();
    Line lines[kRiLines];
    Lines(ri, rom, lines);

    // Layer 0: the panel, the title's well, the portrait's and the lines' wells.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    for (Rect r : {kRiTitle, kRiLinesWell})
    {
        UiItem w = WellItem(r);
        w.radius = 16;
        c.ui.push_back(w);
    }
    UiItem pw = PanelItem(kRiPortrait);
    pw.plain = true;
    pw.fill = kPortraitWell;
    pw.radius = 16;
    c.ui.push_back(pw);

    // Layer 1: the lines (the name in the title band), the focus, the hints.
    for (size_t k = 0; k < v.items.size(); k++)
    {
        if ((v.items[k].id & 0xFFF00000u) != kRiLineId) continue;
        const int i = (int)(v.items[k].id & 0xFF);
        if (i >= kRiLines) continue;
        const Rect r = v.items[k].rect;
        const bool focused = focus == (int)k;
        if (i == 0)
        {
            if (focused) AddFocusRing(c, r, r.h / 2.0f);
            if (focused)
            {
                UiItem pl = PanelItem(r);
                pl.fill = kFillFocus;
                pl.border = 0;
                pl.radius = r.h / 2.0f;
                pl.layer = 1;
                c.ui.push_back(pl);
            }
            Text(c, {r.x + 12, r.y, r.w - 24, r.h}, lines[0].label, kPmTitleSize, kPmTextMin, Align::Left, Weight::ExtraBold);
        }
        else
        {
            if (focused) AddFocusRing(c, r, 18);
            UiItem row = PanelItem(r);
            row.fill = focused ? kFillFocus : kFillRow;
            row.borderArgb = th.frame;
            row.border = 2;
            row.radius = 18;
            row.raised = i == 7;
            row.layer = 1;
            c.ui.push_back(row);
            const int vw = ValueWidth(lines[i].value, r.w - 32);
            Text(c, {r.x + 16, r.y, r.w - 32 - vw - (vw ? 12 : 0), r.h}, lines[i].label, kRiLabelSize, kRiMin, Align::Left, i == 7 ? Weight::ExtraBold : Weight::Bold);
            if (vw) Text(c, {r.x + r.w - 16 - vw, r.y, vw, r.h}, lines[i].value, kRiValueSize, kRiMin, Align::Right, Weight::ExtraBold, kValueColour);
        }
        AddMousePiece(c, {kRiLineX - 25, kRiLineY[i], 50, 12}, r);
    }
    AddMousePiece(c, {kRiCloseX - 15, kRiCloseY - 15, 30, 30}, kRiClose);

    if (!ri.hint)
    {
        int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
        if (MenuFocusable(v, focus)) right = AddHint(c, right, Button::A, v.items[focus].word);
        AddHint(c, right, Button::DPadV, OwnWord(&rom, OwnWordId::HintChoose));
    }

    // Images: the portrait (the game's own, 3x; under the hint box, the game's box lies over it
    // on the touch screen: the picture from before it, dimmed with the screen), the X.
    if (bottom)
    {
        const int w = kRiPortraitFrom.w * 3, h = kRiPortraitFrom.h * 3;
        const Rect to {kRiPortrait.x + (kRiPortrait.w - w) / 2, kRiPortrait.y + (kRiPortrait.h - h) / 2, w, h};
        c.ui.push_back(RoomHintUp(game) ? KeptCrop(kRiPortraitFrom, to) : KeyedCrop(bottom, kRiPortraitFrom, to));
    }
    AddTiered(c.ui, icons, SpriteRef::Of(kSprRiX), 0, kTierS, kRiClose);
    SortUi(c.ui, first);
}

void BuildRoomHint(Composition& c, const MenuView&, int focus, const GameState& game, const RomData& rom)
{
    const RoomInfoState& ri = game.roomInfo;
    if (!RoomHintUp(game) || ri.hintId < 0) return;
    Line lines[kRiLines];
    Lines(ri, rom, lines);
    std::string title;
    for (int i = 0; i < kRiLines; i++)
        if (HintOf(ri, i) == ri.hintId) title = Trim(lines[i].label);
    BuildHintBox(c, focus, title, MenuWords(rom, ri.hintId), rom, {kRiAwayX - 12, kRiAwayY - 12, 24, 24});
}

void MeasureRoomInfo(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) { tally.Note(id, shrunk, fits, lines); };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) { tally.Line(id, text, size, minSize, width, w); };
    // Every line of every room in every state the game can put it in, at the widest values: every
    // guest's name, every mood, a check-out 12/31 in 400 days, §65,535 (the formulas' u16 range).
    const Rect r = RoomInfoLineRect(1);
    auto measure = [&](const RoomInfoState& ri) {
        Line lines[kRiLines];
        Lines(ri, rom, lines);
        const int ids[kRiLines] = {kStrRiPlaceBase + ri.place, ri.occupied ? kStrRiGuest : kStrRiVacant, kStrRiMood,
                                   ri.checkOutKnown ? kStrRiOut : kStrRiOutUnknown, kStrRiBase, kStrRiService, kStrRiBonus, kStrRiTotal};
        const Rect t = RoomInfoLineRect(0);
        line(ids[0], lines[0].label, kPmTitleSize, kPmTextMin, t.w - 24);
        for (int i = 1; i < kRiLines; i++)
        {
            if (!ri.occupied && (i == 2 || i == 3)) continue;
            const int vw = ValueWidth(lines[i].value, r.w - 32);
            line(ids[i], lines[i].label, kRiLabelSize, kRiMin, r.w - 32 - vw - (vw ? 12 : 0), i == 7 ? Weight::ExtraBold : Weight::Bold);
            if (vw)
            {
                const FittedText f = FitLine(lines[i].value, kRiValueSize, kRiMin, vw, Weight::ExtraBold);
                note(ids[i], f.size < kRiValueSize, f.fits);
            }
        }
    };
    for (int place : {29, 8, 15, 18, 20})
    {
        RoomInfoState ri;
        ri.valid = true;
        ri.place = place;
        ri.baseValue = ri.serviceCharge = ri.scoreBonus = ri.total = 65535;
        measure(ri); // (vacant)
        ri.occupied = true;
        for (int k = 0; k <= kSimNameLast - kSimNameBase; k++)
            for (int mood = 0; mood < 5; mood++)
                for (bool known : {true, false})
                {
                    ri.occupant = k;
                    ri.moodId = 2086 + mood;
                    ri.checkOutKnown = known;
                    ri.month = 12; ri.day = 31; ri.days = 400;
                    measure(ri);
                }
    }
    // The hint boxes: the text in the popups' well, the titles on their plate.
    const Rect tb {kPopWell.x + 24, kPopWell.y + 10, kPopWell.w - 48, kPopWell.h - 20};
    for (int id = 2594; id <= 2603; id++)
    {
        if (id == 2602) continue; // ("Fortune" bonus: never drawn)
        const FittedBlock b = FitBlock(MenuWords(rom, id), 30, 20, tb);
        note(id, b.size < 30, b.fits, b.lines);
    }
    for (int id : {kStrRiGuest, kStrRiMood, kStrRiOut, kStrRiVacant, kStrRiBase, kStrRiService, kStrRiBonus, kStrRiTotal})
    {
        std::string label, value;
        Split(rom, id, {"", "", ""}, label, value);
        line(id, Trim(label), 30, 22, kPopTitle.w - 40);
    }
    line(kStrRiHints, MenuWords(rom, kStrRiHints), kPmHintSize, kPmHintSize, 300);
}
}
