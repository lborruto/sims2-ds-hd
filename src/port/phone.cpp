#include "port/phone.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "hotspots.h"
#include "port/goals.h"
#include "port/icons.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

// The game's LCD: its screen, its letters, its highlight (sampled from the phone's pages).
constexpr u32 kLcd = 0xFF9AB29A, kLcdRim = 0xFF51695A, kInk = 0xFF203020, kBar = 0xFF384838, kBarInk = 0xFFC3D3BB;
constexpr int kPressHold = 2, kPressGap = 4; // a DS key press: frames held, then released

std::string Fill(std::string s, int n, const std::string& with)
{
    const std::string at = "@" + std::to_string(n);
    for (size_t p = 0; (p = s.find(at, p)) != std::string::npos;)
    {
        if (p + 2 < s.size() && std::isdigit((unsigned char)s[p + 2])) { p += 2; continue; }
        s.replace(p, 2, with);
        p += with.size();
    }
    return s;
}

// The game's '$' (its simoleon glyph) drawn §, then as a menu shows it.
std::string Shown(std::string s)
{
    std::string o;
    for (char ch : s)
        if (ch == '$') o += "\xC2\xA7";
        else o += ch;
    return MenuText(o);
}

// A message's text as the game fills it: @1..@4 from its arguments (@1 0: the player's name; a
// number when the argument says so; else a string id).
std::string MessageText(const InboxMsg& m, const GameState& game, const RomData& rom)
{
    std::string s = rom.String(m.textId);
    for (int k = 0; k < 4; k++)
    {
        std::string v;
        if (k == 0 && m.args[0] == 0) v = game.hud.simName;
        else if (m.argNum[k]) v = std::to_string(m.args[k]);
        else if (m.args[k] < (u32)rom.StringCount()) v = MenuWords(rom, (int)m.args[k]);
        s = Fill(s, k + 1, v);
    }
    return Shown(s);
}

// The DS's Up / Down pressed until the game's highlight goes from row `from` to `to`.
MenuAction Presses(int from, int to)
{
    MenuAction a;
    const u32 key = 1u << (to > from ? kDown : kUp);
    for (int i = 0; i < std::abs(to - from); i++)
    {
        a.push_back(KeysStep(key, kPressHold));
        a.push_back(WaitStep(kPressGap));
    }
    return a;
}

// The rows shown: the first index (the game's scroll, up to kPhRows), how many.
void Window(const PhonePage& pg, const PhoneState& ph, bool list, int& first, int& shown)
{
    const int n = (int)pg.rows.size();
    shown = list ? std::min(n, kPhRows) : n;
    first = list ? std::clamp(std::min(ph.scroll, n - shown), 0, std::max(0, n - shown)) : 0;
}

// Pages that are a plain list (their rows are the game's rows from 0).
// A sprite of the game's fitted to box (its own shape kept, centred).
void AddSprite(Composition& c, IconCache* icons, int desc, Rect box)
{
    if (!icons) return;
    const SpriteFrame* f0 = icons->Frame(SpriteRef::Of(desc), 0);
    if (!f0 || f0->w <= 0) return;
    const double k = std::min((double)box.w / f0->w, (double)box.h / f0->h);
    const int w = (int)(f0->w * k), h = (int)(f0->h * k);
    if (const SpriteFrame* f = icons->Scaled(SpriteRef::Of(desc), 0, w, h))
        c.ui.push_back(icons->Image(SpriteRef::Of(desc), 0, *f, {box.x + (box.w - f->w) / 2, box.y + (box.h - f->h) / 2, f->w, f->h}));
}

bool ListPage(int page) { return page == 0 || page == 2 || page == 4 || page == 5 || page == 7; }

Rect RowRect(int i, int shown, bool list)
{
    if (list) return {kPhList.x, kPhList.y + i * kPhRowH, kPhList.w, kPhRowH};
    return {kPhList.x, kPhList.y + kPhList.h - (shown - i) * kPhRowH, kPhList.w, kPhRowH};
}
}

PhonePage PhonePageOf(const GameState& game, const RomData& rom)
{
    PhonePage pg;
    const PhoneState& ph = game.town.phone;
    if (!game.valid || !ph.valid) return pg;
    auto words = [&](int id) { return MenuWords(rom, id); };
    auto add = [&](int row, std::string text) { pg.rows.push_back({row, std::move(text)}); };
    auto simName = [&](int k) { return k >= 0 && k <= kSimNameLast - kSimNameBase ? words(kSimNameBase + k) : std::string(); };
    switch (ph.page)
    {
    case 0:
        for (int i = 0; i < 4; i++)
        {
            const int n = (int)ph.inbox.size();
            add(i, MenuText(Fill(rom.String(ph.menuIds[i]), 1, n >= 10 ? words(kStrPhFull) : std::to_string(n))));
        }
        break;
    case 2:
        for (int i = 0; i < (int)ph.inbox.size(); i++)
        {
            add(i, words(ph.inbox[i].senderNameId));
            pg.rows.back().unread = !ph.inbox[i].read;
        }
        add((int)ph.inbox.size(), words(kStrPhExit));
        break;
    case 3:
    {
        const InboxMsg* m = nullptr;
        for (const InboxMsg& e : ph.inbox)
            if (e.slot == ph.shownSlot) m = &e;
        if (!m || ph.textEnd < 1) { pg.crop = true; break; }
        pg.title = words(m->senderNameId);
        pg.body = MessageText(*m, game, rom);
        add(ph.textEnd, words(kStrPhNext));
        add(ph.textEnd + 1, words(kStrPhDelete));
        add(ph.textEnd + 2, words(kStrPhExit));
        break;
    }
    case 4:
        if (ph.customTones) { pg.crop = true; break; }
        for (int i = 0; i < 7; i++)
        {
            add(i, i < 4 ? MenuText(Fill(rom.String(kStrPhRingtone), 1, std::to_string(i))) : words(kStrPhEmpty));
            pg.rows.back().marked = i == ph.ringtone;
        }
        add(7, words(kStrPhExit));
        break;
    case 5:
        for (int i = 0; i < 4; i++) add(i, kStrPhPhonebook[i] == kStrPhMaid ? Shown(Fill(rom.String(kStrPhMaid), 1, ph.maidCost >= 0 ? GroupThousands(ph.maidCost) : "")) : words(kStrPhPhonebook[i]));
        break;
    case 7:
        // (The game builds its list over a frame or two: a count of 0 that disagrees is that, for a few frames; after them its own LCD.)
        if (!ph.contactsKnown && ph.contacts == 0 && ph.contactsWait <= kPhContactsGrace) break;
        if (!ph.contactsKnown) { pg.crop = true; break; }
        if (ph.contactList.empty()) pg.body = words(kStrPhNoContacts);
        for (int i = 0; i < (int)ph.contactList.size(); i++) add(i, simName(ph.contactList[i]));
        add((int)ph.contactList.size(), words(kStrPhExit));
        break;
    case 6: case 8: case 9: case 1:
        if (ph.textEnd < 1) { pg.crop = true; break; }
        if (ph.page == 6) { pg.title = words(kStrPhMaidName); pg.body = ph.reply >= 0 ? words(ph.reply) : ""; }
        else if (ph.page == 8) { pg.title = simName(ph.caller); pg.body = ph.reply >= 0 ? Shown(rom.String(ph.reply)) : ""; }
        else if (ph.page == 9)
        {
            pg.title = simName(ph.caller);
            pg.body = ph.callAway ? Shown(Fill(rom.String(kStrPhAway), 1, game.hud.simName))
                                  : Shown(Fill(Fill(rom.String(kStrPhCall), 1, words(kStrPhMood + ph.callMood)), 2, words(kStrPhPlace + ph.callPlace)));
        }
        else { pg.title = simName(ph.incoming.sender); pg.body = MessageText(ph.incoming, game, rom); }
        add(ph.textEnd, words(kStrPhExit));
        break;
    case 10: break;
    default: pg.crop = true; break;
    }
    // The game's highlight on a row the HD page does not know: its own LCD instead.
    if (!pg.crop && !pg.rows.empty() && ph.row > pg.rows.back().row) pg.crop = true;
    if (pg.crop) { pg.rows.clear(); pg.title.clear(); pg.body.clear(); }
    pg.valid = true;
    return pg;
}

// The row B goes back one level by (the page's Exit, the game's own way up: a message to the list,
// the ringtones / phonebook / list to the menu, the contacts to the phonebook, an answer to the
// contacts), -1 none (the menu: B hangs up; the game's own LCD; no page).
static int PhoneBackRow(const PhonePage& pg, int page, const RomData& rom)
{
    if (page == 0 || page == 10 || !pg.valid || pg.crop || pg.rows.empty()) return -1;
    return pg.rows.back().text == MenuWords(rom, kStrPhExit) ? pg.rows.back().row : -1;
}

MenuView PhoneView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrPhDate) return {};
    return PhoneViewOf(s, *game, rom);
}

MenuView PhoneViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const PhoneState& ph = game.town.phone;
    if (s.screen != Screen::Phone || !rom || !game.valid || !ph.valid) return v;
    const PhonePage pg = PhonePageOf(game, *rom);
    if (!pg.valid) return v;
    const MenuStep ok = KeysStep(1u << kA, kPressHold);
    v.scrollGroup = 1;
    if (pg.crop)
    {
        // The game's own LCD: Up / Down / A its buttons, one press each.
        ViewItem it;
        it.rect = kPhList;
        it.id = kPhRowId;
        it.group = 1;
        it.action = {ok};
        v.items.push_back(it);
        v.selected = 0;
        v.scrollUp = {KeysStep(1u << kUp, kPressHold)};
        v.scrollDown = {KeysStep(1u << kDown, kPressHold)};
    }
    else if (!pg.rows.empty())
    {
        const bool list = ListPage(ph.page);
        int first, shown;
        Window(pg, ph, list, first, shown);
        // The game's highlight: its row, or (on a text page, the highlight in the text) the first
        // row after it.
        int sel = 0;
        for (int i = 0; i < (int)pg.rows.size(); i++)
            if (pg.rows[i].row <= ph.row) sel = i;
        for (int i = first; i < first + shown; i++)
        {
            ViewItem it;
            it.rect = RowRect(i - first, shown, list);
            it.id = kPhRowId + (u32)pg.rows[i].row;
            it.group = 1;
            it.action = Presses(ph.row, pg.rows[i].row);
            it.action.push_back(ok);
            it.word = pg.rows[i].text;
            v.items.push_back(it);
        }
        v.selected = sel - first;
        if (sel > 0) v.scrollUp = Presses(ph.row, pg.rows[sel - 1].row);
        if (sel + 1 < (int)pg.rows.size()) v.scrollDown = Presses(ph.row, pg.rows[sel + 1].row);
    }
    if (const int back = PhoneBackRow(pg, ph.page, *rom); back >= 0)
    {
        // B back one level: the game's highlight to Exit, then its A (as A on Exit).
        v.back = Presses(ph.row, back);
        v.back.push_back(ok);
    }
    else
    {
        v.back = {TapStep(kPhoneClose.x, kPhoneClose.y)};
        v.backCloses = true;
    }
    // MENU hangs up from any page: the phone's X (B goes back one level).
    v.menu = {TapStep(kPhoneClose.x, kPhoneClose.y)};
    v.menuCloses = true;
    v.title = MenuWords(*rom, kStrPhone);
    v.own = true;
    v.valid = true;
    return v;
}

void BuildPhone(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom,
                const HudStrings* strings)
{
    const PhoneState& ph = game.town.phone;
    const size_t first0 = c.ui.size();
    const PhonePage pg = PhonePageOf(game, rom);

    // Layer 0: the phone's body and its LCD.
    UiItem body = PanelItem(kPhPanel);
    body.radius = 48;
    c.ui.push_back(body);
    UiItem lcd = PanelItem(kPhLcd);
    lcd.fill = kLcd;
    lcd.borderArgb = kLcdRim;
    lcd.border = 6;
    lcd.radius = 36;
    lcd.raised = false;
    c.ui.push_back(lcd);

    auto text = [&](Rect r, const std::string& s, float size, float minSize, Align al, u32 ink, Weight w = Weight::ExtraBold) {
        const FittedText f = FitLine(s, size, minSize, r.w, w);
        UiItem t = TextItem(r, f.text, f.size, al, w);
        t.wrap = false;
        t.middle = true;
        t.colour = ink;
        t.shadowArgb = 0;
        t.layer = 1;
        c.ui.push_back(t);
    };
    // The phone's name (the game's "Cellphone") on its body.
    {
        const FittedText f = FitLine(v.title, 30, 22, kPhTitle.w, Weight::ExtraBold);
        UiItem t = TextItem(kPhTitle, f.text, f.size, Align::Left, Weight::ExtraBold);
        t.wrap = false;
        t.middle = true;
        t.layer = 1;
        c.ui.push_back(t);
    }
    // The header: the date as the game writes it ("@2/@1": the month, the day) and the clock.
    {
        std::string date = Fill(Fill(rom.String(kStrPhDate), 1, std::to_string(ph.day)), 2, std::to_string(ph.month));
        std::string clock, money;
        float fill;
        if (!strings || !FormatHud(game.hud, *strings, clock, money, fill)) clock.clear();
        const size_t at = c.ui.size();
        text({kPhHeader.x + 80, kPhHeader.y, kPhHeader.w - 80, kPhHeader.h}, MenuText(date) + "    " + clock, 28, 22, Align::Right, kInk);
        c.ui[at].layer = kPhClockLayer;   // (its own layer: the minute does not raster the LCD again)
        UiItem rule = PanelItem({kPhHeader.x, kPhHeader.y + kPhHeader.h + 4, kPhHeader.w, 2});
        rule.plain = true;
        rule.fill = kLcdRim;
        rule.layer = 1;
        c.ui.push_back(rule);
    }

    if (!pg.crop && pg.valid)
    {
        const bool list = ListPage(ph.page);
        int first = 0, shown = 0;
        Window(pg, ph, list, first, shown);
        // A text page: who on top, the text, then its rows.
        if (!list || !pg.body.empty())
        {
            int y = kPhList.y;
            if (!pg.title.empty())
            {
                text({kPhList.x + 8, y, kPhList.w - 16, 44}, pg.title, 30, 22, Align::Centre, kInk);
                y += 48;
            }
            const Rect tb {kPhList.x + 12, y, kPhList.w - 24, kPhList.y + kPhList.h - shown * kPhRowH - 6 - y};
            const FittedBlock b = FitBlock(pg.body, 26, 18, tb);
            UiItem t = TextItem(tb, b.text, b.size, Align::Centre, Weight::Bold);
            t.middle = true;
            t.colour = kInk;
            t.shadowArgb = 0;
            t.layer = 1;
            c.ui.push_back(t);
        }
        for (int i = 0; i < (int)v.items.size() && first + i < (int)pg.rows.size(); i++)
        {
            const PhoneRow& r = pg.rows[first + i];
            const Rect rr = v.items[i].rect;
            const bool on = i == focus;
            if (on)
            {
                // The game's highlight: a dark bar, its letters light; the focus's green halo.
                UiItem halo = PanelItem({rr.x - 4, rr.y, rr.w + 8, rr.h});
                halo.plain = true;
                halo.fill = 0xFF309A41;
                halo.radius = 14;
                halo.layer = 1;
                c.ui.push_back(halo);
                UiItem bar = PanelItem({rr.x, rr.y + 4, rr.w, rr.h - 8});
                bar.plain = true;
                bar.fill = kBar;
                bar.radius = 10;
                bar.layer = 1;
                c.ui.push_back(bar);
            }
            const u32 ink = on ? kBarInk : kInk;
            if (r.unread || r.marked)
            {
                UiItem dot = PanelItem({rr.x + 24, rr.y + rr.h / 2 - 7, 14, 14});
                dot.plain = true;
                dot.fill = ink;
                dot.radius = 7;
                dot.layer = 1;
                c.ui.push_back(dot);
            }
            text({rr.x + 48, rr.y, rr.w - 96, rr.h}, r.text, 30, 20, Align::Centre, ink);
        }
        if (list) AddScrollBar(c, {kPhList.x + kPhList.w + 2, kPhList.y + 4, 8, shown * kPhRowH - 8}, first, shown, (int)pg.rows.size());
    }

    // The hints, under the buttons.
    {
        const Rect hb {kPhPanel.x + 24, kPhPanel.y + kPhPanel.h - 46, kPhPanel.w - 48, 36};
        std::vector<std::pair<Button, std::string>> hs = {{Button::DPadV, ""}, {Button::A, MenuFocusable(v, focus) ? FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, 270, Weight::ExtraBold).text : ""},
                                                          {Button::B, MenuWords(rom, v.backCloses ? kStrClose : kStrPhExit)}};
        int total = 0;
        std::vector<Rect> rs;
        for (auto& [b, w] : hs) { rs.push_back(ChipRect(0, 0, b, w, kPmHintSize, 0, Weight::ExtraBold)); total += rs.back().w; }
        total += 28 * ((int)hs.size() - 1);
        int x = hb.x + (hb.w - total) / 2;
        for (size_t i = 0; i < hs.size(); i++)
        {
            UiItem ch = ChipItem(x, hb.y + (hb.h - rs[i].h) / 2, hs[i].first, hs[i].second, kPmHintSize, 0, Weight::ExtraBold);
            ch.layer = 1;
            c.ui.push_back(ch);
            x += rs[i].w + 28;
        }
    }

    // The mouse: the HD buttons onto the game's.
    AddMousePiece(c, {kPhUpX - 12, kPhBtnY - 12, 24, 24}, kPhUp);
    AddMousePiece(c, {kPhOkX - 12, kPhBtnY - 12, 24, 24}, kPhOk);
    AddMousePiece(c, {kPhDownX - 12, kPhBtnY - 12, 24, 24}, kPhDown);
    AddMousePiece(c, {kPhoneClose.x - 8, kPhoneClose.y - 8, 16, 16}, kPhX);

    // Images: the signal bars, the buttons, the X; the game's own LCD when its page did not read.
    AddTieredIcon(c, icons, kSprPhSignal, 0, kTierS, {kPhHeader.x, kPhHeader.y, 64, kPhHeader.h});
    AddSprite(c, icons, kSprPhUp, kPhUp);
    AddSprite(c, icons, kSprPhOk, kPhOk);
    AddSprite(c, icons, kSprPhDown, kPhDown);
    AddTieredIcon(c, icons, kSprPhX, 0, kTierS, kPhX);
    if (pg.crop && bottom)
    {
        const int w = kPhLcdFrom.w * 2, h = kPhLcdFrom.h * 2;
        c.ui.push_back(ImageItem({kPhList.x + (kPhList.w - w) / 2, kPhList.y + (kPhList.h - h) / 2, w, h}, bottom, SW, SH, kPhLcdFrom));
    }
    SortUi(c.ui, first0);
}

void MeasurePhone(const RomData& rom, BlockMeasure& m)
{
    auto line = [&](int id, const std::string& t, float size, float minSize, int width) {
        const FittedText f = FitLine(t, size, minSize, width, Weight::ExtraBold);
        m.checked++;
        if (f.size < size) m.shrunk++;
        if (!f.fits) { m.cut++; m.cutIds.push_back(id); }
    };
    auto block = [&](int id, const std::string& t, Rect box) {
        const FittedBlock b = FitBlock(t, 26, 18, box);
        m.checked++;
        if (b.size < 26) m.shrunk++;
        if (b.lines > m.tallestLines) { m.tallestLines = b.lines; m.tallestId = id; }
        if (!b.fits) { m.cut++; m.cutIds.push_back(id); }
    };
    const int rowW = kPhList.w - 96;
    // Every row word: the menu, the lists' fixed words, a full price, every Sim's name.
    for (int id = 0x812; id <= 0x81E; id++)
    {
        std::string t = Fill(rom.String(id), 1, id == kStrPhMaid ? "99,999,999" : "10");
        if (id == kStrPhMaid) t = Shown(t);
        line(id, MenuText(t), 30, 20, rowW);
    }
    line(kStrPhFull, MenuText(Fill(rom.String(0x812), 1, MenuWords(rom, kStrPhFull))), 30, 20, rowW);
    for (int k = kSimNameBase; k <= kSimNameLast; k++) line(k, MenuWords(rom, k), 30, 20, rowW);
    line(kStrPhMaidName, MenuWords(rom, kStrPhMaidName), 30, 22, kPhList.w - 16);
    line(kStrPhDate, MenuText(Fill(Fill(rom.String(kStrPhDate), 1, "28"), 2, "12")) + "    12:59 pm", 28, 22, kPhHeader.w - 80);
    // The texts: the answers (one row under them), a message (three rows).
    const Rect reply {kPhList.x + 12, kPhList.y + 48, kPhList.w - 24, kPhList.h - kPhRowH - 6 - 48};
    const Rect message {kPhList.x + 12, kPhList.y + 48, kPhList.w - 24, kPhList.h - 3 * kPhRowH - 6 - 48};
    for (int id = 0x81F; id <= 0x822; id++) block(id, MenuWords(rom, id), reply);
    for (int id = 0x831; id <= 0x83A; id++)
        if (id != 0x835 && id != 0x836) block(id, MenuWords(rom, id), reply);
    std::string longestMood, longestPlace;
    for (int i = 0; i < 5; i++)
        if (MenuWords(rom, kStrPhMood + i).size() > longestMood.size()) longestMood = MenuWords(rom, kStrPhMood + i);
    for (int p = 0; p < 0x20; p++)
        if (MenuWords(rom, kStrPhPlace + p).size() > longestPlace.size()) longestPlace = MenuWords(rom, kStrPhPlace + p);
    block(kStrPhCall, Shown(Fill(Fill(rom.String(kStrPhCall), 1, longestMood), 2, longestPlace)), reply);
    block(kStrPhAway, Shown(Fill(rom.String(kStrPhAway), 1, "WWWWWWWW")), reply);
    block(kStrPhNoContacts, MenuWords(rom, kStrPhNoContacts), reply);
    // The messages the research saw (0x907, 0x49E with an object's name).
    block(0x907, Shown(Fill(rom.String(0x907), 1, "WWWWWWWW")), message);
    block(0x49E, Shown(Fill(rom.String(0x49E), 2, MenuWords(rom, 0x2B8))), message);
}
}
