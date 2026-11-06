#include "port/checkin.h"

#include <algorithm>

#include "port/icons.h"
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

constexpr u32 kRowFill = 0xFF6A72B8, kRowFocusFill = 0xFF9AA2E3;   // (Interact's rows)
constexpr u32 kSubColour = 0xFFD8DAF4;                             // a pale lavender (Build Rooms' places)
constexpr u32 kPinkPlate = 0xE0302F5C;                             // the pink names' plate: dark, the rows' hue
constexpr u32 kScoreColour = 0xFFFFE9A0;                           // a warm white (the money's)
constexpr u32 kIconWell = 0xFF5961A2;                              // the game's row button

std::string RoomName(const RomData& rom, int place) { return MenuWords(rom, kStrCiPlaceBase + place); }
std::string SimName(const RomData& rom, int k) { return k >= 0 ? MenuWords(rom, kSimNameBase + k) : std::string(); }

// The game's "@1: @2" (1840) with its two parts.
std::string Pair(const RomData& rom, const std::string& a, const std::string& b)
{
    const std::string f = rom.String(kStrTitle);
    std::string out;
    for (size_t i = 0; i < f.size(); i++)
    {
        if (f[i] == '@' && i + 1 < f.size() && (f[i + 1] == '1' || f[i + 1] == '2'))
        {
            out += f[i + 1] == '1' ? a : b;
            i++;
        }
        else out += f[i];
    }
    return MenuText(out);
}

std::string Title(const GameState& game, const RomData& rom)
{
    const CheckInState& ci = game.checkIn;
    return ci.mode == 1 ? MenuWords(rom, kStrCiTitle) : Pair(rom, MenuWords(rom, kStrCiCheckIn), SimName(rom, ci.guest));
}
std::string Score(const RomData& rom, int score) { return MenuWordsWith(rom, kStrCiScore, std::to_string(score)); }

// What stays in a room: its occupant's line ("Guest : @1"), "Vacant" for a built room, none.
std::string Status(const RomData& rom, const RegistryRow& r)
{
    if (r.occupant >= 0) return MenuWordsWith(rom, kStrCiGuest, SimName(rom, r.occupant));
    return r.built ? MenuWords(rom, kStrCiVacant) : std::string();
}
// Whether A performs on the row: the game's own rule per mode.
bool Takes(const CheckInState& ci, const RegistryRow& r) { return ci.mode == 1 ? r.infoOk : r.available; }

void Text(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, Weight w, u32 colour = 0) { PanelText(c, r, s, size, minSize, a, w, colour); }
// The game's pink (a room not built or taken) on a dark plate as wide as the words: read on the
// lavender rows (one line, left-aligned in r, fitted as Text fits it).
void PinkPlate(Composition& c, Rect r, const std::string& s, float size, float minSize)
{
    const FittedText f = FitLine(s, size, minSize, r.w, Weight::ExtraBold);
    const int w = (int)TextWidth(f.text, f.size, Weight::ExtraBold) + 20, h = (int)f.size + 12;
    UiItem p = PanelItem({r.x - 10, r.y + (r.h - h) / 2, std::min(w, r.w + 20), h});
    p.plain = true;
    p.fill = kPinkPlate;
    p.radius = h / 2.0f;
    p.layer = 1;
    c.ui.push_back(p);
}

// A room's picture in `box`: the occupant's portrait (the game's own row button, live) at `scale`,
// else the game's key (frame 0) or its silhouette (frame 1, not built).
void RoomPicture(Composition& c, IconCache* icons, const u32* bottom, const RegistryRow& r, int row, int scale, Rect box)
{
    if (r.occupant >= 0)
    {
        if (!bottom) return;
        const Rect from {kCiPortraitFrom.x, kCiPortraitFrom.y + kCiRowDy * row, kCiPortraitFrom.w, kCiPortraitFrom.h};
        const int w = from.w * scale, h = from.h * scale;
        c.ui.push_back(ImageItem({box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h}, bottom, SW, SH, from));
        return;
    }
    AddSpriteIcon(c, icons, kSprCiKey, r.built ? 0 : 1, scale, box);
}
}

Rect CheckInRowRect(int i) { return {kCiList.x + 16, kCiList.y + 12 + i * 86, kCiList.w - 32, 78}; }

MenuView CheckInView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrCiTitle) return {};
    return CheckInViewOf(s, *game, rom);
}

MenuView CheckInViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const CheckInState& ci = game.checkIn;
    if (s.screen != Screen::Unknown || !game.valid || !ci.valid || (ci.mode != 0 && ci.mode != 1)) return v;
    if (rom) v.title = Title(game, *rom);
    for (int i = 0; i < kCiRows; i++)
    {
        const RegistryRow& r = ci.rows[i];
        ViewItem it;
        it.rect = CheckInRowRect(i);
        it.id = kCiRowId + (u32)std::clamp(r.place, 0, 0xFF);
        if (Takes(ci, r))
        {
            it.action = {TapStep(kCiRowX, kCiRowY0 + i * kCiRowDy)};
            // The check-in hands over to the guest's speech (another screen); the Registry's room
            // info is another overlay on the same (Unknown) screen: no closing wait there.
            it.closes = ci.mode == 0;
            if (rom) it.word = MenuWords(*rom, ci.mode == 1 ? kStrCiExamine : kStrCiCheckIn);
            if (v.initial < 0) v.initial = i;
        }
        it.next[0] = i == 0 ? kNoMove : i - 1;
        it.next[1] = i == kCiRows - 1 ? kNoMove : i + 1;
        it.next[2] = it.next[3] = kNoMove;
        v.items.push_back(it);
    }
    if (v.initial < 0) v.initial = 0;
    v.back = {TapStep(kCiCloseX, kCiCloseY)};
    v.backCloses = true;
    v.replay = ci.mode == 1; // (a late A could check a guest in: never replayed)
    v.own = true;
    v.valid = true;
    return v;
}

void BuildCheckIn(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom)
{
    const Theme& th = GameTheme();
    const CheckInState& ci = game.checkIn;
    if ((int)v.items.size() != kCiRows) return;
    const size_t first = c.ui.size();

    // Layer 0: the panel, the title's well, the ledger's and the focused room's wells.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    for (Rect r : {kCiTitle, kCiList, kCiInfo})
    {
        UiItem w = WellItem(r);
        w.radius = 16;
        c.ui.push_back(w);
    }

    // Layer 1: the title (and the Registry's score on a plate), the rooms, the focused room, the hints.
    int titleRight = kCiClose.x - 16;
    if (ci.mode == 1)
    {
        const std::string score = Score(rom, ci.score);
        const int sw = std::min(360, (int)TextWidth(score, kPmTextSize, Weight::ExtraBold) + 32);
        const Rect plateBox {kCiClose.x - 20 - sw, kCiTitle.y + 10, sw, kCiTitle.h - 20};
        UiItem plate = PanelItem(plateBox);
        plate.fill = th.frameDark;
        plate.raised = false;
        plate.border = 0;
        plate.radius = plateBox.h / 2.0f;
        plate.layer = 1;
        c.ui.push_back(plate);
        Text(c, {plateBox.x + 12, plateBox.y, plateBox.w - 24, plateBox.h}, score, kPmTextSize, 20, Align::Centre, Weight::ExtraBold, kScoreColour);
        titleRight = plateBox.x - 16;
    }
    Text(c, {kCiTitle.x + 24, kCiTitle.y, titleRight - (kCiTitle.x + 24), kCiTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Left, Weight::ExtraBold);

    std::vector<std::pair<Rect, int>> pictures; // (the picture's box, its row): images last
    for (int i = 0; i < kCiRows; i++)
    {
        const RegistryRow& r = ci.rows[i];
        const Rect rr = v.items[i].rect;
        const bool focused = focus == i;
        if (focused) AddFocusRing(c, rr, 22);
        UiItem row = PanelItem(rr);
        row.fill = focused ? kRowFocusFill : kRowFill;
        row.borderArgb = th.frame;
        row.border = 3;
        row.radius = 22;
        row.raised = r.built;
        row.layer = 1;
        c.ui.push_back(row);
        const Rect icon {rr.x + 8, rr.y + 7, 64, 64};
        UiItem iw = PanelItem(icon);
        iw.plain = true;
        iw.fill = kIconWell;
        iw.radius = 12;
        iw.layer = 1;
        c.ui.push_back(iw);
        pictures.push_back({icon, i});
        const int x = rr.x + 88, w = rr.w - 88 - 16;
        const std::string sub = Status(rom, r);
        const u32 colour = r.available ? kCiNameOk : kCiNamePink;
        const Rect nameAt = sub.empty() ? Rect {x, rr.y, w, rr.h} : Rect {x, rr.y + 4, w, 40};
        if (!r.available) PinkPlate(c, nameAt, RoomName(rom, r.place), kCiNameSize, kCiNameMin);
        if (sub.empty()) Text(c, nameAt, RoomName(rom, r.place), kCiNameSize, kCiNameMin, Align::Left, Weight::ExtraBold, colour);
        else
        {
            Text(c, nameAt, RoomName(rom, r.place), kCiNameSize, kCiNameMin, Align::Left, Weight::ExtraBold, colour);
            Text(c, {x, rr.y + 42, w, 30}, sub, kCiSubSize, kCiSubMin, Align::Left, Weight::Bold, focused ? 0xFFFFFFFF : kSubColour);
        }
        AddMousePiece(c, {kCiRowX - 15, kCiRowY0 + i * kCiRowDy - 15, 30, 30}, rr);
    }

    // The focused room (none focused: the first): its picture 3x, its name, who stays there.
    const int f = focus >= 0 && focus < kCiRows ? focus : 0;
    const RegistryRow& fr = ci.rows[f];
    const int ix = kCiInfo.x + 20, iw = kCiInfo.w - 40;
    const Rect big {kCiInfo.x + (kCiInfo.w - 104) / 2, kCiInfo.y + 20, 104, 104};
    UiItem bw = PanelItem(big);
    bw.plain = true;
    bw.fill = kIconWell;
    bw.radius = 16;
    bw.layer = 1;
    c.ui.push_back(bw);
    const FittedBlock name = FitBlock(RoomName(rom, fr.place), kCiNameSize, 22, {ix, kCiInfo.y + 140, iw, 84});
    if (!fr.available)
    {
        // (the pink on its dark plate, the block's lines tall)
        const int h = std::min(84, (int)(name.lines * Metrics(name.size).lineHeight) + 16);
        UiItem p = PanelItem({ix, kCiInfo.y + 140 + (84 - h) / 2, iw, h});
        p.plain = true;
        p.fill = kPinkPlate;
        p.radius = 16;
        p.layer = 1;
        c.ui.push_back(p);
    }
    UiItem nt = TextItem({ix, kCiInfo.y + 140, iw, 84}, name.text, name.size, Align::Centre, Weight::ExtraBold);
    nt.colour = fr.available ? kCiNameOk : kCiNamePink;
    nt.layer = 1;
    c.ui.push_back(nt);
    const FittedBlock st = FitBlock(Status(rom, fr), 24, kCiSubMin, {ix, kCiInfo.y + 236, iw, 96});
    UiItem stt = TextItem({ix, kCiInfo.y + 236, iw, 96}, st.text, st.size, Align::Centre, Weight::Bold);
    stt.colour = kSubColour;
    stt.layer = 1;
    c.ui.push_back(stt);
    if (ci.mode == 0)
    {
        // Who is checking in, where: the room's line as it would read with the guest in it.
        const FittedBlock g = FitBlock(MenuWordsWith(rom, kStrCiGuest, SimName(rom, ci.guest)), 24, kCiSubMin, {ix, kCiInfo.y + 350, iw, 84});
        UiItem gt = TextItem({ix, kCiInfo.y + 350, iw, 84}, g.text, g.size, Align::Centre, Weight::ExtraBold);
        gt.colour = Takes(ci, fr) ? kScoreColour : 0x80FFFFFF;
        gt.layer = 1;
        c.ui.push_back(gt);
    }
    pictures.push_back({big, -1 - f});
    AddMousePiece(c, {kCiCloseX - 15, kCiCloseY - 15, 30, 30}, kCiClose);

    // The hints: B "Close" (the game's X), A with the focused room's word (none when the game refuses it).
    int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) right = AddHint(c, right, Button::A, v.items[focus].word);
    AddHint(c, right, Button::DPadV, "");

    // Images: the rooms' pictures (2x in a row, 3x for the focused room), the X.
    for (auto [box, i] : pictures)
    {
        const int row = i >= 0 ? i : -1 - i;
        RoomPicture(c, icons, bottom, ci.rows[row], row, i >= 0 ? 2 : 3, box);
    }
    AddSpriteIcon(c, icons, kSprCiX, 0, 2, kCiClose);
    SortUi(c.ui, first);
}

void MeasureCheckIn(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) { tally.Note(id, shrunk, fits, lines); };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) { tally.Line(id, text, size, minSize, width, w); };
    auto block = [&](int id, const std::string& text, float size, float minSize, Rect box) { tally.Block(id, text, size, minSize, box); };
    const Rect row = CheckInRowRect(0);
    const int rowW = row.w - 88 - 16, ix = kCiInfo.w - 40;
    // The title beside the widest score (100 %); a check-in's title with every guest's name.
    const std::string score = Score(rom, 100);
    const int sw = std::min(360, (int)TextWidth(score, kPmTextSize, Weight::ExtraBold) + 32);
    line(kStrCiScore, score, kPmTextSize, 20, sw - 24);
    line(kStrCiTitle, MenuWords(rom, kStrCiTitle), kPmTitleSize, kPmTextMin, kCiClose.x - 20 - sw - 16 - (kCiTitle.x + 24));
    for (int k = 0; k <= kSimNameLast - kSimNameBase; k++)
    {
        const std::string t = Pair(rom, MenuWords(rom, kStrCiCheckIn), SimName(rom, k));
        line(kSimNameBase + k, t, kPmTitleSize, kPmTextMin, kCiClose.x - 16 - (kCiTitle.x + 24));
        const std::string g = MenuWordsWith(rom, kStrCiGuest, SimName(rom, k));
        line(kStrCiGuest, g, kCiSubSize, kCiSubMin, rowW, Weight::Bold);
        block(kStrCiGuest, g, 24, kCiSubMin, {0, 0, ix, 96});
    }
    // The five guest rooms' names (the game's table: Small, Deluxe, Jungle, Modern, Penthouse) in
    // a row and in the focused room's well; "Vacant".
    for (int place : {29, 8, 15, 18, 20})
    {
        line(kStrCiPlaceBase + place, RoomName(rom, place), kCiNameSize, kCiNameMin, rowW);
        block(kStrCiPlaceBase + place, RoomName(rom, place), kCiNameSize, 22, {0, 0, ix, 84});
    }
    line(kStrCiVacant, MenuWords(rom, kStrCiVacant), kCiSubSize, kCiSubMin, rowW, Weight::Bold);
    block(kStrCiVacant, MenuWords(rom, kStrCiVacant), 24, kCiSubMin, {0, 0, ix, 96});
    for (int id : {kStrCiExamine, kStrCiCheckIn, kStrClose}) line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 300);
}
}
