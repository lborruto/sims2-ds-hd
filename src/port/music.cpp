#include "port/music.h"

#include <algorithm>

#include "port/goals.h"
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
constexpr u32 kPlayingColour = 0xFFFFE9A0;                         // a warm white (the money's)
constexpr u32 kSubColour = 0xFFD8DAF4;                             // a pale lavender
constexpr u32 kIconWell = 0xFF5961A2;                              // the game's button well

int Rows(const MusicState& m) { return std::min((int)m.list.size(), kMuRows); }

void Text(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, Weight w, u32 colour = 0) { PanelText(c, r, s, size, minSize, a, w, colour); }
}

std::string SongName(const RomData& rom, const SongRow& s)
{
    if (s.custom >= 0) return MenuText(GameTextToUtf8(s.customName));
    return s.nameId >= 0 ? MenuWords(rom, s.nameId) : std::string();
}

Rect MusicRowRect(int i) { return {kMuList.x + 16, kMuList.y + 12 + i * 86, kMuList.w - 32, 78}; }

MenuView MusicView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrMuTitle) return {};
    return MusicViewOf(s, *game, rom);
}

MenuView MusicViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const MusicState& m = game.music;
    if (s.screen != Screen::Unknown || !game.valid || !m.valid) return v;
    const int rows = Rows(m), count = (int)m.list.size();
    if (m.scroll < 0 || m.scroll + rows > count) return v;
    if (rom) v.title = MenuWords(*rom, kStrMuTitle);
    for (int i = 0; i < rows; i++)
    {
        const int at = m.scroll + i;
        ViewItem it;
        it.rect = MusicRowRect(i);
        it.id = kMuRowId + (u32)std::clamp(m.list[at].index, 0, 0xFF);
        if (at != m.playing)
        {
            it.action = {TapStep(kMuRowX, kMuRowY0 + i * kMuRowDy)};
            if (rom) it.word = MenuWords(*rom, kStrMuPlay);
        }
        else v.initial = i;
        it.next[0] = i == 0 ? kNoMove : i - 1;
        it.next[1] = i == rows - 1 ? kNoMove : i + 1;
        it.next[2] = it.next[3] = kNoMove;
        v.items.push_back(it);
    }
    if (v.initial < 0) v.initial = 0;
    // The game's own scroll: one row a tap, while it can.
    if (m.scroll > 0) v.scrollUp = {TapStep(kMuUpX, kMuUpY)};
    if (m.scroll < count - rows) v.scrollDown = {TapStep(kMuDownX, kMuDownY)};
    v.back = {TapStep(kMuCloseX, kMuCloseY)};
    v.backCloses = true;
    v.own = true;
    v.valid = true;
    return v;
}

void BuildMusic(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons)
{
    const MusicState& m = game.music;
    const int rows = Rows(m), count = (int)m.list.size();
    if ((int)v.items.size() != rows) return;
    const size_t first = c.ui.size();

    // Layer 0: the panel, the title's, the arrows', the list's and the info's wells.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    for (Rect r : {kMuTitle, kMuArrows, kMuList, kMuInfo})
    {
        UiItem w = WellItem(r);
        w.radius = 16;
        c.ui.push_back(w);
    }

    // Layer 1: the title, the rows, the song playing, the scroll bar, the hints.
    Text(c, {kMuTitle.x + 24, kMuTitle.y, kMuClose.x - 16 - (kMuTitle.x + 24), kMuTitle.h}, v.title, kPmTitleSize, kPmTextMin, Align::Left, Weight::ExtraBold);
    struct Icon { int desc, frame, scale; Rect box; u32 opacity; };
    std::vector<Icon> images;
    for (int i = 0; i < rows; i++)
    {
        const int at = m.scroll + i;
        const bool focused = focus == i, playing = at == m.playing;
        const Rect rr = v.items[i].rect;
        if (focused) AddFocusRing(c, rr, 22);
        UiItem row = PanelItem(rr);
        row.fill = focused ? kRowFocusFill : kRowFill;
        row.borderArgb = GameTheme().frame;
        row.border = 3;
        row.radius = 22;
        row.layer = 1;
        c.ui.push_back(row);
        const Rect icon {rr.x + 8, rr.y + 7, 64, 64}; // (the game's button: no well of its own)
        images.push_back({kSprMuPlay, playing ? kSprMuPlayingFrame : 0, 2, icon, 0});
        Text(c, {rr.x + 88, rr.y, rr.w - 88 - 16, rr.h}, SongName(rom, m.list[at]), kMuNameSize, kMuNameMin, Align::Left, Weight::ExtraBold,
             playing ? kPlayingColour : 0);
        AddMousePiece(c, {kMuRowX - 16, kMuRowY0 + i * kMuRowDy - 15, 32, 30}, rr);
    }
    // The arrows (dim where the game cannot scroll) and the scroll bar between them.
    images.push_back({kSprMuUp, 0, 2, kMuUp, v.scrollUp.empty() ? 0x60000000u : 0});
    images.push_back({kSprMuDown, 0, 2, kMuDown, v.scrollDown.empty() ? 0x60000000u : 0});
    AddScrollBar(c, {kMuArrows.x + kMuArrows.w / 2 - 5, kMuUp.y + kMuUp.h + 12, 10, kMuDown.y - 12 - (kMuUp.y + kMuUp.h + 12)}, m.scroll, rows, count);
    AddMousePiece(c, {kMuUpX - 16, kMuUpY - 5, 32, 30}, kMuUp);
    AddMousePiece(c, {kMuDownX - 16, kMuDownY - 25, 32, 30}, kMuDown);

    // The song playing (the game's green button 3x, its name) and the room it plays in.
    const int ix = kMuInfo.x + 20, iw = kMuInfo.w - 40;
    const Rect big {kMuInfo.x + (kMuInfo.w - 112) / 2, kMuInfo.y + 24, 112, 112};
    UiItem bw = PanelItem(big);
    bw.plain = true;
    bw.fill = kIconWell;
    bw.radius = 16;
    bw.layer = 1;
    c.ui.push_back(bw);
    if (m.playing >= 0 && m.playing < count)
    {
        images.push_back({kSprMuPlay, kSprMuPlayingFrame, 3, big, 0});
        const Rect nb {ix, kMuInfo.y + 152, iw, 120};
        const FittedBlock name = FitBlock(SongName(rom, m.list[m.playing]), kMuNameSize, kMuNameMin, nb);
        UiItem nt = TextItem(nb, name.text, name.size, Align::Centre, Weight::ExtraBold);
        nt.colour = kPlayingColour;
        nt.layer = 1;
        c.ui.push_back(nt);
    }
    else images.push_back({kSprMuPlay, 0, 3, big, 0x60000000u});
    if (m.place >= 0)
    {
        const Rect pb {ix, kMuInfo.y + 290, iw, 120};
        const FittedBlock place = FitBlock(MenuWords(rom, kStrMuPlaceBase + m.place), 24, 18, pb);
        UiItem pt = TextItem(pb, place.text, place.size, Align::Centre, Weight::Bold);
        pt.colour = kSubColour;
        pt.layer = 1;
        c.ui.push_back(pt);
    }
    AddMousePiece(c, {kMuCloseX - 16, kMuCloseY - 15, 32, 30}, kMuClose);

    // The hints: B "Close" (the game's X), A "Turn On" on a song not playing, the D-pad's Up / Down
    // when the list scrolls.
    int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) right = AddHint(c, right, Button::A, v.items[focus].word);
    if (count > rows) AddHint(c, right, Button::DPadV, "");

    // Images: the play buttons, the arrows, the X.
    for (const Icon& i : images) AddSpriteIcon(c, icons, i.desc, i.frame, i.scale, i.box, i.opacity);
    AddSpriteIcon(c, icons, kSprMuX, 0, 2, kMuClose);
    SortUi(c.ui, first);
}

void MeasureMusic(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    auto note = [&](int id, bool shrunk, bool fits, int lines = 0) { tally.Note(id, shrunk, fits, lines); };
    auto line = [&](int id, const std::string& text, float size, float minSize, int width, Weight w = Weight::ExtraBold) { tally.Line(id, text, size, minSize, width, w); };
    auto block = [&](int id, const std::string& text, float size, float minSize, Rect box) { tally.Block(id, text, size, minSize, box); };
    const Rect row = MusicRowRect(0);
    const int iw = kMuInfo.w - 40;
    line(kStrMuTitle, MenuWords(rom, kStrMuTitle), kPmTitleSize, kPmTextMin, kMuClose.x - 16 - (kMuTitle.x + 24));
    for (int id : kMuSongNames)
    {
        line(id, MenuWords(rom, id), kMuNameSize, kMuNameMin, row.w - 88 - 16);
        block(id, MenuWords(rom, id), kMuNameSize, kMuNameMin, {0, 0, iw, 120});
    }
    for (int place = 0; place <= 32; place++) block(kStrMuPlaceBase + place, MenuWords(rom, kStrMuPlaceBase + place), 24, 18, {0, 0, iw, 120});
    for (int id : {kStrMuPlay, kStrClose}) line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 300);
}
}
