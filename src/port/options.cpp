#include "port/options.h"

#include <algorithm>
#include <tuple>

#include "hotspots.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kBtnFill = 0xFF6A72B8, kBtnFocusFill = 0xFF9AA2E3;
constexpr u32 kTrackFill = 0xFF51518A, kTrackOn = 0xFFB2B2E3, kNotch = 0xFF383869;
constexpr u32 kCurrent = 0xFF6BD36B;      // the game's green for its current slot's number
constexpr u32 kLabel = 0xFFD6DAF6;
constexpr float kKnob = 46;

Rect SlotRect(int i) { return {kPmContent.x, kSlotY0 + i * (kSlotH + kSlotGap), kPmContent.w, kSlotH}; }
int KnobX(int k) { return kOptTrackX + kOptTrackW * std::clamp(k, 0, 10) / 10; }
int TrackY(Rect well) { return well.y + 88; }

MenuAction KnobDrag(int k, int to, int y)
{
    if (to < 0 || to > 10) return {};
    return {DragStep(kOptKnobX0 + kOptKnobStep * k, y, kOptKnobX0 + kOptKnobStep * to, y)};
}

// A pill button with its word centred.
void AddButton(Composition& c, Rect r, const std::string& word, bool focused)
{
    const Theme& th = GameTheme();
    if (focused) AddFocusRing(c, r, r.h / 2.0f);
    UiItem b = PanelItem(r);
    b.fill = focused ? kBtnFocusFill : kBtnFill;
    b.borderArgb = th.frame;
    b.border = 3;
    b.radius = r.h / 2.0f;
    b.layer = 1;
    c.ui.push_back(b);
    const FittedText t = FitLine(word, kPmTextSize, kPmTextMin, r.w - 48, Weight::ExtraBold);
    UiItem tt = TextItem({r.x + 24, r.y, r.w - 48, r.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
    tt.wrap = false;
    tt.middle = true;
    tt.layer = 1;
    c.ui.push_back(tt);
}

// A volume: the label, the value in percent, the track with its eleven notches and the knob.
void AddSlider(Composition& c, Rect well, const std::string& label, int notch, bool focused, int dsY)
{
    const Theme& th = GameTheme();
    if (focused) AddFocusRing(c, well, 16);
    UiItem w = WellItem(well);
    w.radius = 16;
    w.layer = 1;
    c.ui.push_back(w);
    const std::string pct = std::to_string(notch * 10) + "%";
    const int pw = (int)TextWidth("100%", 28, Weight::ExtraBold) + 8;
    const FittedText l = FitLine(label, 28, 20, well.w - 64 - pw - 80, Weight::ExtraBold);
    UiItem lt = TextItem({well.x + 28, well.y + 10, well.w - 64 - pw - 80, 44}, l.text, l.size, Align::Left, Weight::ExtraBold);
    lt.wrap = false;
    lt.middle = true;
    lt.layer = 1;
    c.ui.push_back(lt);
    UiItem vt = TextItem({well.x + well.w - 28 - pw, well.y + 10, pw, 44}, pct, 28, Align::Right, Weight::ExtraBold);
    vt.wrap = false;
    vt.middle = true;
    vt.colour = kLabel;
    vt.layer = 1;
    c.ui.push_back(vt);
    if (focused)
    {   // The D-pad's chip: Left / Right move the knob.
        const Rect dp = ChipRect(0, 0, Button::DPadH, "", 24);
        UiItem chip = ChipItem(well.x + well.w - 28 - pw - 16 - dp.w, well.y + 10 + (44 - dp.h) / 2, Button::DPadH, "", 24);
        chip.layer = 1;
        c.ui.push_back(chip);
    }
    const int ty = TrackY(well);
    UiItem track = PanelItem({kOptTrackX - 6, ty - 7, kOptTrackW + 12, 14});
    track.plain = true;
    track.fill = kTrackFill;
    track.radius = 7;
    track.layer = 1;
    c.ui.push_back(track);
    UiItem on = PanelItem({kOptTrackX - 6, ty - 7, KnobX(notch) - kOptTrackX + 12, 14});
    on.plain = true;
    on.fill = kTrackOn;
    on.radius = 7;
    on.layer = 1;
    c.ui.push_back(on);
    for (int k = 0; k <= 10; k++)
    {
        UiItem d = PanelItem({KnobX(k) - 3, ty - 3, 6, 6});
        d.plain = true;
        d.fill = kNotch;
        d.radius = 3;
        d.layer = 1;
        c.ui.push_back(d);
    }
    UiItem knob = PanelItem({KnobX(notch) - (int)kKnob / 2, ty - (int)kKnob / 2, (int)kKnob, (int)kKnob});
    knob.fill = focused ? kBtnFocusFill : th.fill;
    knob.borderArgb = th.frame;
    knob.border = 4;
    knob.radius = kKnob / 2;
    knob.layer = 1;
    c.ui.push_back(knob);
    // The mouse: the track onto the game's (its knob's tenth to tenth), so a drag from the knob
    // drags the game's.
    const float s = (float)kOptTrackW / (10 * kOptKnobStep);
    AddMousePiece(c, {kOptKnobX0 - 8, dsY - 12, 10 * kOptKnobStep + 16, 24},
                  {kOptTrackX - (int)(8 * s), ty - 28, (int)((10 * kOptKnobStep + 16) * s), 56});
}
}

int VolumeNotch(int volume) { return std::clamp((volume * 10 + 2048) / 4096, 0, 10); }

void AddOptionsView(MenuView& v, const OptionsState& o, const RomData* rom)
{
    if (rom) v.title = MenuTitle(*rom, 4, "");
    // Explicit moves: the buttons' centres lie just outside the 60° cone from the music slider's.
    const int b = (int)v.items.size();
    ViewItem save;
    save.next[1] = b + 2;
    save.rect = kOptSaveBtn;
    save.id = kOptSaveId;
    save.action = {TapStep(kOptSaveX, kOptSaveY)};
    if (rom) save.word = MenuWords(*rom, kStrSave);
    v.items.push_back(save);
    ViewItem quit;
    quit.rect = kOptQuitBtn;
    quit.id = kOptQuitId;
    quit.next[1] = b + 2;
    quit.action = {TapStep(kOptQuitX, kOptQuitY)};
    if (rom) quit.word = MenuWords(*rom, kStrQuit);
    v.items.push_back(quit);
    const int notch[2] = {VolumeNotch(o.music), VolumeNotch(o.sfx)};
    const int dsY[2] = {kOptMusicY, kOptSfxY};
    for (int i = 0; i < 2; i++)
    {
        ViewItem s;
        s.rect = i == 0 ? kOptMusic : kOptSfx;
        s.id = i == 0 ? kOptMusicId : kOptSfxId;
        s.left = KnobDrag(notch[i], notch[i] - 1, dsY[i]);
        s.right = KnobDrag(notch[i], notch[i] + 1, dsY[i]);
        // (Left / Right at an end: nothing to drag; the focus stays, as a move would go nowhere.)
        if (i == 0) s.next[0] = b;
        if (s.left.empty()) s.next[2] = (int)v.items.size();
        if (s.right.empty()) s.next[3] = (int)v.items.size();
        v.items.push_back(s);
    }
}

void BuildOptions(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom)
{
    if (v.items.size() < 4) return;
    const OptionsState& o = game.town.options;
    const std::vector<Hotspot>& hs = HotspotsFor({Screen::Panel, Tab::Options});
    AddButton(c, kOptSaveBtn, v.items[0].word, focus == 0);
    AddButton(c, kOptQuitBtn, v.items[1].word, focus == 1);
    AddSlider(c, kOptMusic, MenuWords(rom, kStrMusic), VolumeNotch(o.music), focus == 2, kOptMusicY);
    AddSlider(c, kOptSfx, MenuWords(rom, kStrSfx), VolumeNotch(o.sfx), focus == 3, kOptSfxY);
    if (hs.size() >= 3)
    {
        AddMousePiece(c, hs[1].area, kOptSaveBtn);
        AddMousePiece(c, hs[2].area, kOptQuitBtn);
    }
}

MenuView SaveSlotsView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrSaveGame) return {};
    return SaveSlotsViewOf(s, *game, rom);
}

int SlotsPopup(const GameState& game)
{
    const PopupState& p = game.town.popup;
    return p.valid && p.state >= 1 && p.state <= 3 ? p.kind : -1;
}

MenuView SaveSlotsViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const SaveSlots& sv = game.town.saves;
    if (s.screen != Screen::SaveSlots || !game.valid || !sv.valid) return v;
    if (rom) v.title = MenuWords(*rom, kStrSaveGame);
    // A popup over the slots (the game's question before it overwrites a used slot, kind 2; its
    // "Saving", kind 3): the slots wait; the question's A is its tick, B its cross.
    const int popup = SlotsPopup(game);
    for (int i = 0; i < kSaveSlotCount; i++)
    {
        ViewItem it;
        it.rect = SlotRect(i);
        it.id = kSlotId + (u32)i;
        it.enabled = popup < 0;
        it.action = {TapStep(kSlotTickX, kSlotTickY[i])};
        if (rom) it.word = MenuWords(*rom, kStrSave);
        v.items.push_back(it);
    }
    if (popup == 2)
    {
        ViewItem yes;
        yes.rect = kSlotYes;
        yes.id = kSlotYesId;
        yes.action = {TapStep(kSlotAskTickX, kSlotAskY)};
        if (rom) yes.word = MenuWords(*rom, kStrYes);
        v.items.push_back(yes);
        v.back = {TapStep(kSlotAskCrossX, kSlotAskY)};
        v.initial = kSaveSlotCount;
    }
    else if (popup < 0)
    {
        v.back = {TapStep(kSlotCloseX, kSlotCloseY)};
        // The game's current slot; without one, the first empty slot (a save there asks nothing).
        v.initial = 0;
        if (sv.current >= 0 && sv.current < kSaveSlotCount) v.initial = sv.current;
        else
            for (int i = kSaveSlotCount - 1; i >= 0; i--)
                if (!sv.used[i]) v.initial = i;
    }
    // A press held while the game was not settled is dropped, never replayed: here a late A saves.
    v.replay = false;
    v.valid = true;
    return v;
}

void BuildSaveSlots(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom)
{
    const Theme& th = GameTheme();
    const SaveSlots& sv = game.town.saves;
    const size_t first = c.ui.size();
    if ((int)v.items.size() < kSaveSlotCount) return;

    // Layer 0: the panel and the title's well.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem title = WellItem(kSlotTitle);
    title.radius = 16;
    c.ui.push_back(title);

    // Layer 1: the title, the slots, the hints.
    const FittedText t = FitLine(v.title, kPmTitleSize, kPmTextMin, kSlotTitle.w - 48, Weight::ExtraBold);
    UiItem tt = TextItem({kSlotTitle.x + 24, kSlotTitle.y, kSlotTitle.w - 48, kSlotTitle.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
    tt.wrap = false;
    tt.middle = true;
    tt.layer = 1;
    c.ui.push_back(tt);
    for (int i = 0; i < kSaveSlotCount; i++)
    {
        const Rect r = SlotRect(i);
        const bool focused = i == focus;
        if (focused) AddFocusRing(c, r, 22);
        UiItem row = PanelItem(r);
        row.fill = focused ? kBtnFocusFill : kBtnFill;
        row.borderArgb = th.frame;
        row.border = 3;
        row.radius = 22;
        row.layer = 1;
        c.ui.push_back(row);
        // The number in its round well, green for the game's current slot.
        UiItem nw = PanelItem({r.x + 20, r.y + (r.h - 100) / 2, 100, 100});
        nw.plain = true;
        nw.fill = th.frameDark;
        nw.radius = 50;
        nw.layer = 1;
        c.ui.push_back(nw);
        UiItem n = TextItem({r.x + 20, r.y, 100, r.h}, std::to_string(i + 1), 64, Align::Centre, Weight::ExtraBold);
        n.wrap = false;
        n.middle = true;
        if (sv.current == i) n.colour = kCurrent;
        n.layer = 1;
        c.ui.push_back(n);
        const int tx = r.x + 148, tw = r.w - 148 - 32;
        if (!sv.used[i])
        {
            const FittedText nd = FitLine(MenuWords(rom, kStrNoData), 32, 22, tw, Weight::ExtraBold);
            UiItem e = TextItem({tx, r.y, tw, r.h}, nd.text, nd.size, Align::Left, Weight::ExtraBold);
            e.wrap = false;
            e.middle = true;
            e.colour = kLabel;
            e.layer = 1;
            c.ui.push_back(e);
        }
        else
        {
            const SaveSlot& s = sv.slot[i];
            const std::string money = MenuWords(rom, kStrMoney, std::max(0, s.money));
            const int mw = (int)TextWidth(money, 32, Weight::ExtraBold) + 8;
            const FittedText name = FitLine(s.simName, 34, 22, tw - mw - 24, Weight::ExtraBold);
            UiItem nt = TextItem({tx, r.y + 22, tw - mw - 24, 48}, name.text, name.size, Align::Left, Weight::ExtraBold);
            nt.wrap = false;
            nt.middle = true;
            nt.layer = 1;
            c.ui.push_back(nt);
            UiItem mt = TextItem({tx + tw - mw, r.y + 22, mw, 48}, money, 32, Align::Right, Weight::ExtraBold);
            mt.wrap = false;
            mt.middle = true;
            mt.layer = 1;
            c.ui.push_back(mt);
            if (s.place >= 0)
            {
                const FittedText pl = FitLine(MenuWords(rom, kStrPlaceFirst + s.place), 26, 20, tw, Weight::Bold);
                UiItem pt = TextItem({tx, r.y + 74, tw, 44}, pl.text, pl.size, Align::Left, Weight::Bold);
                pt.wrap = false;
                pt.middle = true;
                pt.colour = kLabel;
                pt.layer = 1;
                c.ui.push_back(pt);
            }
        }
        // The mouse: the row onto its tick (a click saves, as A), while no popup is up.
        if (v.items[i].enabled) AddMousePiece(c, {kSlotTickX - 12, kSlotTickY[i] - 12, 24, 24}, r);
    }
    const int popup = SlotsPopup(game);
    if (popup >= 0)
    {
        // The game's popup over the slots, small: its box, its text (kind 3 "Saving": its title
        // too) and, for the question, its two answers in the game's words with their buttons.
        UiItem dim = PanelItem({kPmPanel.x + 12, kPmPanel.y + 12, kPmPanel.w - 24, kPmPanel.h - 24});
        dim.plain = true;
        dim.fill = 0x70202448; // (the slots dimmed behind the game's box)
        dim.radius = 16;
        dim.layer = 1;
        c.ui.push_back(dim);
        UiItem box = PanelItem(kSlotAsk);
        box.radius = 22;
        box.layer = 1;
        c.ui.push_back(box);
        const bool ask = popup == 2;
        const Rect well {kSlotAsk.x + 24, kSlotAsk.y + 24, kSlotAsk.w - 48, ask ? kSlotYes.y - kSlotAsk.y - 48 : kSlotAsk.h - 48};
        UiItem w = WellItem(well);
        w.radius = 14;
        w.layer = 1;
        c.ui.push_back(w);
        const PopupState& p = game.town.popup;
        std::string text = MenuText(GameTextToUtf8(p.text));
        if (!ask && !p.title.empty()) text = MenuText(GameTextToUtf8(p.title)) + "\n" + text;
        const Rect tb {well.x + 18, well.y + 8, well.w - 36, well.h - 16};
        const FittedBlock b = FitBlock(text, 30, 22, tb);
        UiItem tt = TextItem(tb, b.text, b.size, Align::Centre, Weight::ExtraBold);
        tt.middle = true;
        tt.layer = 1;
        c.ui.push_back(tt);
        if (ask)
        {
            auto answer = [&](Rect r, Button chip, const std::string& word, bool focused, Rect from) {
                if (focused) AddFocusRing(c, r, r.h / 2.0f);
                UiItem bt = PanelItem(r);
                bt.fill = focused ? kBtnFocusFill : kBtnFill;
                bt.borderArgb = th.frame;
                bt.border = 3;
                bt.radius = r.h / 2.0f;
                bt.layer = 1;
                c.ui.push_back(bt);
                const Rect cr = ChipRect(0, 0, chip, word, 28, 0, Weight::ExtraBold);
                UiItem ch = ChipItem(r.x + (r.w - cr.w) / 2, r.y + (r.h - cr.h) / 2, chip, word, 28, 0, Weight::ExtraBold);
                ch.layer = 1;
                c.ui.push_back(ch);
                AddMousePiece(c, from, r);
            };
            const int yes = kSaveSlotCount;
            answer(kSlotNo, Button::B, MenuWords(rom, kStrNo), false, {kSlotAskCrossX - 12, kSlotAskY - 12, 24, 24});
            answer(kSlotYes, Button::A, MenuWords(rom, kStrYes), focus == yes, {kSlotAskTickX - 12, kSlotAskY - 12, 24, 24});
        }
    }
    if (popup < 0)
    {
        int right = AddHint(c, kPmHints.x + kPmHints.w - 8, Button::B, MenuWords(rom, kStrClose));
        if (MenuFocusable(v, focus))
        {
            const FittedText a = FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, right - kPmHints.x - 60, Weight::ExtraBold);
            AddHint(c, right, Button::A, a.text);
        }
    }

    SortUi(c.ui, first);
}

void MeasureOptions(const RomData& rom, BlockMeasure& m)
{
    auto line = [&](int id, const std::string& text, float size, float minSize, int width) {
        const FittedText f = FitLine(text, size, minSize, width, Weight::ExtraBold);
        m.checked++;
        if (f.size < size) m.shrunk++;
        if (!f.fits) { m.cut++; m.cutIds.push_back(id); }
    };
    const int pw = (int)TextWidth("100%", 28, Weight::ExtraBold) + 8;
    for (int id : {kStrSave, kStrQuit}) line(id, MenuWords(rom, id), kPmTextSize, kPmTextMin, kOptSaveBtn.w - 48);
    for (int id : {kStrSave, kStrQuit}) line(id, MenuWords(rom, id), kPmHintSize, kPmHintSize, 240);
    for (int id : {kStrMusic, kStrSfx}) line(id, MenuWords(rom, id), 28, 20, kOptMusic.w - 64 - pw - 80);
    line(kStrSaveGame, MenuWords(rom, kStrSaveGame), kPmTitleSize, kPmTextMin, kSlotTitle.w - 48);
    const int tw = kPmContent.w - 148 - 32;
    line(kStrNoData, MenuWords(rom, kStrNoData), 32, 22, tw);
    // The question over the slots and its answers; "Saving" with its line.
    for (int id : {kStrNo, kStrYes}) line(id, MenuWords(rom, id), 28, 28, kSlotYes.w - 90);
    const Rect qb {kSlotAsk.x + 42, kSlotAsk.y + 32, kSlotAsk.w - 84, kSlotYes.y - kSlotAsk.y - 64};
    const Rect sb {kSlotAsk.x + 42, kSlotAsk.y + 32, kSlotAsk.w - 84, kSlotAsk.h - 64};
    for (auto [id, box, text] : {std::tuple {kStrOverwrite, qb, MenuWords(rom, kStrOverwrite)},
                                 std::tuple {kStrSaving, sb, MenuWords(rom, kStrSaving) + "\n" + MenuWords(rom, kStrSaving + 1)}})
    {
        const FittedBlock b = FitBlock(text, 30, 22, box);
        m.checked++;
        if (b.size < 30) m.shrunk++;
        if (!b.fits) { m.cut++; m.cutIds.push_back(id); }
    }
    const int mw = (int)TextWidth(MenuWords(rom, kStrMoney, 99999999), 32, Weight::ExtraBold) + 8;
    line(kStrMoney, "WWWWWWWW", 34, 22, tw - mw - 24); // (the widest name the game's keyboard makes)
    for (int place = 0; place < 0x20; place++)
    {
        const std::string p = MenuWords(rom, kStrPlaceFirst + place);
        if (p.empty() || p == "Dummy" || p == "Empty String") continue;
        line(kStrPlaceFirst + place, p, 26, 20, tw);
    }
}
}
