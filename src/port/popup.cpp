#include "port/popup.h"

#include <algorithm>

#include "port/build.h"
#include "port/checkin.h"
#include "port/roominfo.h"
#include "port/music.h"
#include "port/statpanel.h"
#include "port/catalogue.h"
#include "port/gift.h"
#include "port/front.h"
#include "port/frontmenus.h"
#include "port/cas.h"
#include "port/note.h"
#include "port/options.h"
#include "port/panelmenu.h"
#include "port/phone.h"
#include "port/pockets.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kBtnFill = 0xFF6A72B8, kBtnFocusFill = 0xFF9AA2E3, kBtnPressed = 0xFF309A41; // the game's green while held
constexpr u32 kVeil = 0x8C181C38;
constexpr Rect kCanvas {0, 0, 1280, 720};

bool Overlaps(Rect a, Rect b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

// The popup's text as shown: the title (kinds 0 and 3; the question's is stale: the game draws
// none) and the text, decoded, its escapes and placeholders dropped.
void PopupWords(const PopupState& p, std::string& title, std::string& text)
{
    title = p.kind == 2 ? "" : MenuText(GameTextToUtf8(p.title));
    text = MenuText(GameTextToUtf8(p.text));
}
}

bool PopupButton(const PopupState& p, int index, int& x, int& y)
{
    if (!p.valid || p.boxW <= 0 || p.boxTiles <= 0) return false;
    int r;
    if (p.kind == 0 && index == 0) r = p.boxW / 16 - 1;
    else if (p.kind == 2 && index == 0) r = p.boxW / 32 - 1;
    else if (p.kind == 2 && index == 1) r = 3 * p.boxW / 32 - 2;
    else return false;
    const int x0 = 128 - p.boxW / 2 + 8 * r, y0 = p.boxY + 8 * p.boxTiles;
    if (x0 < 0 || x0 + 24 > SW || y0 < 0 || y0 + 24 > SH) return false;
    x = x0 + 12;
    y = y0 + 12;
    return true;
}

bool PopupOver(ScreenState s, const GameState& game)
{
    const PopupState& p = game.town.popup;
    if (!game.valid || !p.valid || p.state < 1 || p.state > 3) return false;
    if (game.screen == GScreen::Popup) return s.screen == Screen::Popup || s.screen == Screen::Unknown; // (Options > Quit: Unknown)
    return s.screen == Screen::Gift || s.screen == Screen::SaveSlots || s.screen == Screen::Phone || s.screen == Screen::Catalogue || s.screen == Screen::Build ||
           (s.screen == Screen::Unknown && (NoteUp(game) || game.checkIn.valid || game.roomInfo.valid || game.music.valid || game.stat.valid)) ||
           // The front end's Load-a-Sim ("Loading", kind 3; port/frontmenus.h).
           ((s.screen == Screen::None || s.screen == Screen::FrontEnd) && game.front.load.valid && !game.front.load.wireless);
}

MenuView PopupView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrPopClose) return {};
    return PopupViewOf(s, *game, rom);
}

MenuView PopupViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    if (!PopupOver(s, game)) return v;
    const PopupState& p = game.town.popup;
    int x0, y0, x1, y1;
    // A box whose buttons are not placed yet (its slide-in: the box's place not in RAM yet): the
    // HD popup with nothing to press until they are, never the game's own screen.
    const bool placed = p.kind == 0 ? PopupButton(p, 0, x0, y0) : p.kind == 2 ? PopupButton(p, 0, x0, y0) && PopupButton(p, 1, x1, y1) : true;
    if (p.kind == 0 && placed)
    {
        ViewItem it;
        it.rect = kPopClose;
        it.id = kPopCloseId;
        it.action = {TapStep(x0, y0)};
        it.closes = true;
        v.items.push_back(it);
        v.back = it.action;
        v.backCloses = true;
        v.passKeys = 1u << kStart; // (Game Paused resumes on Start too)
    }
    else if (p.kind == 2 && placed)
    {
        // No first, and the focus opens there (a question's safe answer: two quick A never say
        // Yes); Yes one Right away.
        ViewItem no;
        no.rect = kPopNo;
        no.id = kPopNoId;
        no.action = {TapStep(x0, y0)};
        no.closes = true;
        if (rom) no.word = MenuWords(*rom, kStrPopNo);
        v.items.push_back(no);
        ViewItem yes;
        yes.rect = kPopYes;
        yes.id = kPopYesId;
        yes.action = {TapStep(x1, y1)};
        yes.closes = true;
        if (rom) yes.word = MenuWords(*rom, kStrPopYes);
        v.items.push_back(yes);
        v.initial = 0;
        v.question = true;
        v.back = {TapStep(x0, y0)};
        v.backCloses = true;
        v.replay = false; // (a late A answers Yes: never replayed)
    }
    else if (p.kind != 0 && p.kind != 2 && p.kind != 3) return v;
    v.own = v.popup = true;
    v.valid = true;
    return v;
}

MenuView ConsoleView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (MenuView v = PopupView(s, game, rom); v.valid) return v;
    // The game's own popup is up over a screen but its HD view does not read (no strings, a box that
    // does not fit): not the screen's view (milestone 1's picture, the popup in it).
    if (game && PopupOver(s, *game)) return {};
    return ScreenView(s, game, rom);
}

MenuView ScreenView(ScreenState s, const GameState* game, const RomData* rom)
{
    switch (s.screen)
    {
    case Screen::Panel: return PanelMenuView(s, game, rom);
    case Screen::Gift: return GiftMenuView(s, game, rom);
    case Screen::SaveSlots: return SaveSlotsView(s, game, rom);
    case Screen::Phone: return PhoneView(s, game, rom);
    case Screen::Catalogue: return CatalogueView(s, game, rom);
    case Screen::Build: return BuildRoomsView(s, game, rom);
    case Screen::Unknown:
        // The hotel's Registry, room info, music and stat panel (no GScreen of their own), else a note.
        if (game && game->checkIn.valid) return CheckInView(s, game, rom);
        if (game && game->roomInfo.valid) return RoomInfoView(s, game, rom);
        if (game && game->music.valid) return MusicView(s, game, rom);
        if (game && game->stat.valid) return StatPanelView(s, game, rom);
        return NoteView(s, game, rom);
    // The front end's boot, title and main menu (port/front.h), Settings and Load-a-Sim (port/frontmenus.h).
    // Create-a-Sim (port/cas.h).
    case Screen::CreateASim: return CasView(s, game, rom);
    case Screen::FrontEnd: case Screen::None: case Screen::Loading:
        if (MenuView v = FrontView(s, game, rom); v.valid) return v;
        if (game && game->front.settings.valid) return SettingsView(s, game, rom);
        if (game && game->front.load.valid) return LoadView(s, game, rom);
        return {};
    default: return {};
    }
}

ScreenState UnderScreen(ScreenState s, const GameState& game)
{
    if (game.screen == GScreen::Popup && game.tab >= 0 && game.tab <= 4) return {Screen::Panel, (Tab)((int)Tab::Interact + game.tab)};
    return s;
}

namespace
{
// The popups' look over what c holds: what is beneath takes no click, its pictures under the box
// go (they would be drawn over it) and the others are dimmed with the rest; the veil, the box,
// the title on its plate (none when empty), the text in its well (`buttons`: the well leaves
// their row free). Everything on kPopLayer.
void BoxBase(Composition& c, const std::string& title, const std::string& text, bool buttons, Rect boxAt = kPopBox)
{
    // (A title that does not hold on the plate at its smallest gets a plate almost the box's width:
    // a minigame's long help title, Spanish's "RELLENOS DE PERCUSIÓN / VARIACIONES".)
    const int inset = FitLine(title, 30, 22, boxAt.w - 240 - 40, Weight::ExtraBold).fits ? 120 : 40;
    const Rect titleAt {boxAt.x + inset, boxAt.y - 26, boxAt.w - 2 * inset, 56};
    const Theme& th = GameTheme();
    for (Piece& q : c.pieces) q.touch = false;
    std::vector<UiItem> kept;
    for (const UiItem& u0 : c.ui)
    {
        UiItem u = u0;
        // The status bar stays clear: above the veil, never dimmed.
        if (u.layer == kStatusBarLayer) { u.layer = kPopLayer + 1; kept.push_back(u); continue; }
        if (u.kind != UiItem::Image) { kept.push_back(u); continue; }
        if (Overlaps(u.to, {boxAt.x - 8, titleAt.y - 8, boxAt.w + 16, boxAt.y + boxAt.h - titleAt.y + 24})) continue;
        UiItem d = u;
        d.fill = 0x50000000; // (its opacity)
        kept.push_back(d);
    }
    c.ui = kept;
    auto add = [&](UiItem u) { u.layer = kPopLayer; c.ui.push_back(u); };

    UiItem veil = PanelItem(kCanvas);
    veil.plain = true;
    veil.fill = kVeil;
    add(veil);
    UiItem box = PanelItem(boxAt);
    box.radius = 26;
    add(box);
    const Rect well = buttons ? Rect {boxAt.x + 28, boxAt.y + 42, boxAt.w - 56, boxAt.h - 162} : Rect {boxAt.x + 28, boxAt.y + 42, boxAt.w - 56, boxAt.h - 70};
    UiItem w = WellItem(well);
    w.radius = 16;
    add(w);
    if (!title.empty())
    {
        UiItem plate = PanelItem(titleAt);
        plate.fill = th.frame;
        plate.borderArgb = 0xFFB2B2E3;
        plate.border = 2;
        plate.radius = titleAt.h / 2.0f;
        add(plate);
        const FittedText t = FitLine(title, 30, 22, titleAt.w - 40, Weight::ExtraBold);
        UiItem tt = TextItem({titleAt.x + 20, titleAt.y, titleAt.w - 40, titleAt.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
        tt.wrap = false;
        tt.middle = true;
        add(tt);
    }
    const Rect tb {well.x + 24, well.y + 10, well.w - 48, well.h - 20};
    const FittedBlock b = FitBlock(text, 30, 20, tb);
    UiItem tt = TextItem(tb, b.text, b.size, Align::Centre, Weight::ExtraBold);
    tt.middle = true;
    add(tt);
}

// A box's button: its fill (pressed: the game's green), the focus's halo, its chips centred, the
// mouse's piece onto the game's DS rectangle `from` (none when empty).
void BoxButton(Composition& c, Rect r, bool pressed, bool focused, const std::vector<BoxChip>& chips, Rect from)
{
    const Theme& th = GameTheme();
    auto add = [&](UiItem u) { u.layer = kPopLayer; c.ui.push_back(u); };
    if (focused)
    {
        AddFocusRing(c, r, r.h / 2.0f);
        c.ui.back().layer = kPopLayer;
    }
    UiItem bt = PanelItem(r);
    bt.fill = pressed ? kBtnPressed : focused ? kBtnFocusFill : kBtnFill;
    bt.borderArgb = th.frame;
    bt.border = 3;
    bt.radius = r.h / 2.0f;
    add(bt);
    int total = 0;
    std::vector<Rect> rs;
    // (wordOnly: the word alone, no chip: an answer A does not press until it has the focus.)
    for (const BoxChip& ch : chips)
    {
        if (ch.wordOnly) rs.push_back({0, 0, (int)TextWidth(ch.word, 28, Weight::ExtraBold) + 2, 36});
        else rs.push_back(ChipRect(0, 0, ch.button, ch.word, 28, 0, Weight::ExtraBold));
        total += rs.back().w;
    }
    total += 12 * ((int)chips.size() - 1);
    int x = r.x + (r.w - total) / 2;
    for (size_t i = 0; i < chips.size(); i++)
    {
        if (chips[i].wordOnly)
        {
            UiItem t = TextItem({x, r.y, rs[i].w, r.h}, chips[i].word, 28, Align::Left, Weight::ExtraBold);
            t.wrap = false;
            t.middle = true;
            add(t);
        }
        else add(ChipItem(x, r.y + (r.h - rs[i].h) / 2, chips[i].button, chips[i].word, 28, 0, Weight::ExtraBold));
        x += rs[i].w + 12;
    }
    if (from.w > 0) AddMousePiece(c, from, r);
}
}

void BuildPopup(Composition& c, const MenuView&, int focus, const GameState& game, const RomData& rom)
{
    const PopupState& p = game.town.popup;
    std::string title, text;
    PopupWords(p, title, text);
    BoxBase(c, title, text, p.kind != 3);
    // The buttons: the game's held one green (P+0xD8C, its index P+0xD88), the focus's halo.
    auto button = [&](Rect r, int index, bool focused, std::vector<BoxChip> chips) {
        int bx, by;
        const Rect from = PopupButton(p, index, bx, by) ? Rect {bx - 12, by - 12, 24, 24} : Rect {};
        BoxButton(c, r, p.pressing && p.pressed == index, focused, chips, from);
    };
    if (p.kind == 0)
        button(kPopClose, 0, focus == 0, {{Button::A, ""}, {Button::B, MenuWords(rom, kStrPopClose)}});
    else if (p.kind == 2)
    {
        // A answers the focused button (No first), B is always No.
        const std::string no = MenuWords(rom, kStrPopNo), yes = MenuWords(rom, kStrPopYes);
        if (focus == 1) button(kPopNo, 0, false, {{Button::B, no}});
        else button(kPopNo, 0, true, {{Button::A, ""}, {Button::B, no}});
        if (focus == 1) button(kPopYes, 1, true, {{Button::A, yes}});
        else button(kPopYes, 1, false, {{Button::A, yes, true}});
    }
}

void BuildHintBox(Composition& c, int focus, const std::string& title, const std::string& text, const RomData& rom, Rect from)
{
    BoxBase(c, title, text, true);
    BoxButton(c, kPopClose, false, focus == 0, {{Button::A, ""}, {Button::B, MenuWords(rom, kStrPopClose)}}, from);
}

Rect GameBoxAt(Rect cover, Rect area, int buttons)
{
    // At least the popups' box, as large as the game's box it hides, centred on the area, held inside it.
    const int w = std::min(area.w - 16, std::max(kPopBox.w, cover.w + 48));
    const int h = std::min(area.h - 40, std::max(buttons > 0 ? kPopBox.h : 240, cover.h + 48));
    // (Level with the box it hides, held below the title plate's room and above the area's foot.)
    const int y = std::clamp(cover.y + cover.h / 2 - h / 2, area.y + 30, area.y + area.h - h - 8);
    return {area.x + (area.w - w) / 2, y, w, h};
}

void BuildGameBox(Composition& c, Rect box, const std::string& title, const std::string& text, const std::vector<GameBoxButton>& buttons)
{
    BoxBase(c, title, text, !buttons.empty(), box);
    const int n = (int)buttons.size(), y = box.y + box.h - 104;
    for (int i = 0; i < n; i++)
    {
        Rect r;
        if (n == 1) r = {box.x + (box.w - 320) / 2, y, 320, kPopBtnH};
        else if (n == 2) r = i == 0 ? Rect {box.x + 48, y, 260, kPopBtnH} : Rect {box.x + box.w - 308, y, 260, kPopBtnH};
        else
        {
            const int gap = 20, bw = (box.w - 72 - gap * (n - 1)) / n;
            r = {box.x + 36 + i * (bw + gap), y, bw, kPopBtnH};
        }
        BoxButton(c, r, buttons[i].pressed, buttons[i].focused, buttons[i].chips, buttons[i].from);
    }
}

void MeasurePopups(const RomData& rom, BlockMeasure& m)
{
    // The popups the game opens (0x0205871C's callers, research m3 §3.4) and the Save slots'.
    static const int kTexts[] = {23, 65, 313, 413, 1158, 1159, 1191, 1761, 2423, 2427, 2429, 2430, 2431, 2435, 2436, 2437, 2438,
                                 2440, 2441, 2443, 2444, 2445, 2446, 2447, 2448, 2449, 2450, 2586};
    static const int kTitles[] = {22, 2424, 2425, 2426, 2428, 2434, 2442};
    const Rect tb {kPopWell.x + 24, kPopWell.y + 10, kPopWell.w - 48, kPopWell.h - 20};
    for (int id : kTexts)
    {
        // @1 a long name (a Sim's: the game's keyboard makes 8 letters at most).
        std::string t = rom.String(id);
        for (size_t q; (q = t.find("@1")) != std::string::npos;) t.replace(q, 2, "WWWWWWWW");
        const FittedBlock b = FitBlock(MenuText(t), 30, 20, tb);
        m.checked++;
        if (b.size < 30) m.shrunk++;
        if (b.lines > m.tallestLines) { m.tallestLines = b.lines; m.tallestId = id; }
        if (!b.fits) { m.cut++; m.cutIds.push_back(id); }
    }
    for (int id : kTitles)
    {
        const FittedText f = FitLine(MenuWords(rom, id), 30, 22, kPopTitle.w - 40, Weight::ExtraBold);
        m.checked++;
        if (f.size < 30) m.shrunk++;
        if (!f.fits) { m.cut++; m.cutIds.push_back(id); }
    }
    // The buttons' chips and words (Close beside the A chip too).
    for (int id : {kStrPopClose, kStrPopYes, kStrPopNo})
    {
        const Rect r = ChipRect(0, 0, Button::B, MenuWords(rom, id), 28, 0, Weight::ExtraBold);
        const Rect a = ChipRect(0, 0, Button::A, "", 28, 0, Weight::ExtraBold);
        m.checked++;
        // (Close and the focused No beside the A chip.)
        const bool withA = id == kStrPopClose || id == kStrPopNo;
        const int room = (id == kStrPopClose ? kPopClose.w : kPopNo.w) - 24;
        if ((withA ? r.w + 12 + a.w : r.w) > room) { m.cut++; m.cutIds.push_back(id); }
    }
}
}
