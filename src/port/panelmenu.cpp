#include "port/panelmenu.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "hotspots.h"
#include "port/goals.h"
#include "port/icons.h"
#include "port/interact.h"
#include "port/options.h"
#include "port/pockets.h"
#include "port/relations.h"
#include "port/statusbar.h"
#include "romdata.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kPhoneId = 1;
// The focus ring's green (the game's own selection green, as its tab ring and arrows).
constexpr u32 kFocusGreen = 0xFF309A41;
// The selected tab's raised plate and the hints' text.
constexpr u32 kTabPlateFill = 0xFF9AA2E3;

bool IsPunct(char c) { return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' || c == ')'; }
}

std::string MenuWords(const RomData& rom, int id, int amount) { return MenuText(rom.String(id), amount); }

std::string MenuWordsWith(const RomData& rom, int id, const std::string& at1)
{
    std::string f = rom.String(id);
    for (size_t p = 0; (p = f.find("@1", p)) != std::string::npos;)
    {
        if (p + 2 < f.size() && std::isdigit((unsigned char)f[p + 2])) { p += 2; continue; }
        f.replace(p, 2, at1);
        p += at1.size();
    }
    return MenuText(f);
}

std::string MenuText(std::string in, int amount)
{
    in = DropEscapes(in);   // (RomData's {xx} / {F0xx})
    if (amount >= 0)
    {
        // The amount in @1, the simoleon glyph for '$' (as the HD status bar draws the money).
        std::string f;
        for (size_t i = 0; i < in.size(); i++)
        {
            if (in.compare(i, 2, "@1") == 0 && (i + 2 >= in.size() || !std::isdigit((unsigned char)in[i + 2]))) { f += GroupThousands(amount); i++; }
            else if (in[i] == '$') f += "\xC2\xA7";
            else f += in[i];
        }
        in = f;
    }
    std::string s;
    for (size_t i = 0; i < in.size(); i++)
    {
        const char c = in[i];
        if (c == '@')
        {   // A placeholder: '@' and its digits or up to two letters; a '$' (the simoleon) before it goes too.
            size_t j = i + 1;
            if (j < in.size() && std::isdigit((unsigned char)in[j])) while (j < in.size() && std::isdigit((unsigned char)in[j])) j++;
            else while (j < in.size() && j < i + 3 && std::isalpha((unsigned char)in[j])) j++;
            if (!s.empty() && s.back() == '$') s.pop_back();
            i = j - 1;
            continue;
        }
        s += c == '\n' ? ' ' : c;
    }
    // Empty brackets a drop left, spaces folded, none before punctuation, the ends trimmed.
    for (size_t p; (p = s.find("()")) != std::string::npos;) s.erase(p, 2);
    std::string out;
    for (char c : s)
    {
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        if (IsPunct(c) && !out.empty() && out.back() == ' ' && c != '!' && c != '?' && c != ':' && c != ';') out.pop_back();
        out += c;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string MenuTitle(const RomData& rom, int tab, const std::string& name)
{
    const std::string tabName = MenuWords(rom, kStrTabName + std::clamp(tab, 0, 4));
    if (name.empty()) return tabName;
    // The game's "@1: @2" (its punctuation per language), the placeholders filled.
    std::string f = rom.String(kStrTitle), out;
    for (size_t i = 0; i < f.size(); i++)
    {
        if (f[i] == '@' && i + 1 < f.size() && (f[i + 1] == '1' || f[i + 1] == '2'))
        {
            out += f[i + 1] == '1' ? tabName : name;
            i++;
        }
        else if (f[i] == '{')
        {
            const size_t e = f.find('}', i);
            if (e != std::string::npos && e - i <= 8) i = e;
        }
        else if (f[i] != '@') out += f[i] == '\n' ? ' ' : f[i];
    }
    return out.find(name) == std::string::npos ? tabName + ": " + name : out;
}

FittedText FitLine(const std::string& text, float size, float minSize, int width, Weight w)
{
    FittedText f {text, size, true};
    for (float s = size; s >= minSize - 1e-3f; s -= 1)
        if (TextWidth(text, s, w) <= width) { f.size = s; return f; }
    // Still too wide at the smallest size: cut, with an ellipsis (never past a UTF-8 letter).
    f.size = minSize;
    f.fits = false;
    const std::string dots = "\xE2\x80\xA6";
    std::string t = text;
    while (!t.empty() && TextWidth(t + dots, minSize, w) > width)
    {
        // One whole letter: its continuation bytes, then its first byte.
        while (!t.empty() && ((unsigned char)t.back() & 0xC0) == 0x80) t.pop_back();
        if (!t.empty()) t.pop_back();
    }
    while (!t.empty() && t.back() == ' ') t.pop_back();
    f.text = t + dots;
    return f;
}

void AddMousePiece(Composition& c, Rect from, Rect to)
{
    Piece p {Src::Bottom, from, to};
    p.alpha = 0;
    c.pieces.push_back(p);
}

void AddFocusRing(Composition& c, Rect r, float radius)
{
    constexpr int t = 5;
    UiItem g = PanelItem({r.x - t, r.y - t, r.w + 2 * t, r.h + 2 * t});
    g.plain = true;
    g.fill = kFocusGreen;
    g.radius = radius + t;
    g.layer = 1;
    c.ui.push_back(g);
}

int AddHint(Composition& c, int right, Button b, const std::string& label)
{
    const Rect r = ChipRect(0, 0, b, label, kPmHintSize, 0, Weight::ExtraBold);
    UiItem it = ChipItem(right - r.w, kPmHints.y + (kPmHints.h - r.h) / 2, b, label, kPmHintSize, 0, Weight::ExtraBold);
    it.layer = 1;
    c.ui.push_back(it);
    return right - r.w - 28;
}

MenuView PanelMenuView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrClose) return {};
    return PanelMenuViewOf(s, *game, rom);
}

MenuView PanelMenuViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    if (s.screen != Screen::Panel || !game.valid) return v;
    switch (s.tab)
    {
    case Tab::Interact:
        if (!game.town.interact.valid) return v;
        AddInteractView(v, game.town.interact, rom);
        break;
    case Tab::Pockets:
        if (!game.town.inventory.valid || !game.town.pockets.valid) return v;
        AddPocketsView(v, game.town.inventory, game.town.pockets, rom);
        break;
    case Tab::Goals:
        if (!game.town.goals.valid) return v;
        AddGoalsView(v, game.town.goals, rom);
        break;
    case Tab::Relationships:
        if (!game.town.relations.valid) return v;
        AddRelationsView(v, game.town.relations, rom);
        break;
    case Tab::Options:
        if (!game.town.options.valid) return v;
        AddOptionsView(v, game.town.options, rom);
        break;
    default: return v; // (no tab: milestone 1's composition)
    }
    // The phone button, after the tab's own items (the first focus is the tab's), linked to them
    // explicitly: the geometry alone puts it at the edge of the 60° cone from the rows.
    const int tabFirst = MenuFirst(v), phoneAt = (int)v.items.size();
    for (int i = 0; i < phoneAt; i++)
        if (v.items[i].enabled && v.items[i].next[0] < 0 && MenuNeighbour(v, i, 0, -1) == i) v.items[i].next[0] = phoneAt;
    ViewItem phone;
    phone.rect = kPmPhoneWell;
    phone.action = {TapStep(kPhoneIcon.x, kPhoneIcon.y)};
    phone.id = kPhoneId;
    phone.next[1] = tabFirst;
    if (rom) phone.word = MenuWords(*rom, kStrPhone);
    v.items.push_back(phone);
    v.valid = true;
    return v;
}

void BuildPanelMenu(Composition& c, ScreenState s, const MenuView& v, int focus, const GameState& game, const RomData& rom,
                    IconCache* icons, const u32* bottom)
{
    const Theme& th = GameTheme();
    const int tab = std::clamp((int)s.tab - (int)Tab::Interact, 0, 4);
    const size_t first = c.ui.size();

    // Layer 0, the frame (rastered again only when the tab changes): the panel, the tab bar's
    // plate, the selected tab's raised plate, the title's well, the phone's cell.
    UiItem panel = PanelItem(kPmPanel);
    panel.radius = 22;
    c.ui.push_back(panel);
    UiItem bar = WellItem(kPmTabBar);
    bar.fill = th.frameDark;
    bar.radius = 16;
    c.ui.push_back(bar);
    const int cellY = kPmTabBar.y + (kPmTabBar.h - kPmTabCell) / 2;
    {
        UiItem plate = PanelItem({kPmTabsX + tab * kPmTabCell + 2, cellY + 2, kPmTabCell - 4, kPmTabCell - 4});
        plate.fill = kTabPlateFill;
        plate.borderArgb = th.fill;
        plate.border = 3;
        plate.radius = 18;
        c.ui.push_back(plate);
    }
    UiItem phoneCell = PanelItem(kPmPhoneWell);
    phoneCell.fill = th.frameDark; // (the game's well colour behind the live phone)
    phoneCell.border = 0;
    phoneCell.raised = false;
    phoneCell.radius = 14;
    UiItem title = WellItem(kPmTitle);
    title.radius = 14;
    c.ui.push_back(title);

    // LB / RB at the tab bar's ends (the chips alone: the tabs' icons say where they go).
    {
        const Rect lb = ChipRect(0, 0, Button::LB, "", 26);
        UiItem l = ChipItem(kPmTabsX - 14 - lb.w, kPmTabBar.y + (kPmTabBar.h - lb.h) / 2, Button::LB, "", 26);
        c.ui.push_back(l);
        UiItem r = ChipItem(kPmTabsX + 5 * kPmTabCell + 14, kPmTabBar.y + (kPmTabBar.h - lb.h) / 2, Button::RB, "", 26);
        c.ui.push_back(r);
    }

    // Layer 1 and up: what changes with the content and the focus.
    // The phone (its focus ring first: it lies under the cell).
    for (int i = 0; i < (int)v.items.size(); i++)
        if (v.items[i].id == kPhoneId && i == focus) AddFocusRing(c, v.items[i].rect, phoneCell.radius);
    phoneCell.layer = 1;
    c.ui.push_back(phoneCell);

    // The tab's title (the view's, in the game's words; the tab's name without one).
    const FittedText t = FitLine(v.title.empty() ? MenuTitle(rom, tab, "") : v.title, kPmTitleSize, kPmTextMin, kPmTitle.w - 48, Weight::ExtraBold);
    UiItem tt = TextItem({kPmTitle.x + 24, kPmTitle.y, kPmTitle.w - 48, kPmTitle.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
    tt.wrap = false;
    tt.middle = true;
    tt.layer = 1;
    c.ui.push_back(tt);

    // The tab's content.
    if (s.tab == Tab::Interact) BuildInteract(c, v, focus, game, rom, icons, bottom);
    else if (s.tab == Tab::Pockets) BuildPockets(c, v, focus, game, rom, icons, bottom);
    else if (s.tab == Tab::Goals) BuildGoals(c, v, focus, game, rom, bottom);
    else if (s.tab == Tab::Relationships) BuildRelations(c, v, focus, game, rom, bottom);
    else if (s.tab == Tab::Options) BuildOptions(c, v, focus, game, rom);

    // The hints: A (the focused item's own word when it has one), B closes.
    int right = kPmHints.x + kPmHints.w - 8;
    right = AddHint(c, right, Button::B, MenuWords(rom, kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) // (nothing to perform: no A)
    {
        const FittedText a = FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, right - kPmHints.x - 60, Weight::ExtraBold);
        right = AddHint(c, right, Button::A, a.text);
    }
    if (MenuFocusable(v, focus) && !v.items[focus].buttonX.empty())
    {
        const FittedText x = FitLine(v.items[focus].xWord, kPmHintSize, kPmHintSize, right - kPmHints.x - 60, Weight::ExtraBold);
        right = AddHint(c, right, Button::X, x.text);
    }
    // The D-pad's Up / Down (its vertical arms lit: the game has no word for its arrows) while they
    // move a selection the game owns (Goals, Relationships: the game's own arrows).
    if (MenuFocusable(v, v.selected) && focus == v.selected && (!v.scrollUp.empty() || !v.scrollDown.empty())) AddHint(c, right, Button::DPadV, "");

    // The icons (images: drawn above the still layers): the five tabs, the selected one green.
    if (icons)
        for (int i = 0; i < 5; i++)
        {
            const SpriteRef ref = SpriteRef::Of(i == tab ? kTabIconGreen : kTabIconBlue);
            if (const SpriteFrame* f = icons->Scaled(ref, i, kPmTabIcon / 24))
            {
                const Rect to {kPmTabsX + i * kPmTabCell + (kPmTabCell - f->w) / 2, cellY + (kPmTabCell - f->h) / 2, f->w, f->h};
                c.ui.push_back(ImageItem(to, f->argb.data(), f->w, f->h, {0, 0, f->w, f->h}));
            }
        }
    // The phone: the game's own button, live from the touch screen, 2x.
    if (bottom)
    {
        const int w = kPmPhoneFrom.w * 2, h = kPmPhoneFrom.h * 2;
        c.ui.push_back(ImageItem({kPmPhone.x + (kPmPhone.w - w) / 2, kPmPhone.y + (kPmPhone.h - h) / 2, w, h}, bottom, SW, SH, kPmPhoneFrom));
    }

    // The mouse: the tab icons and the phone onto the game's own.
    for (int i = 0; i < 5; i++) AddMousePiece(c, kTabIcon[i].area, {kPmTabsX + i * kPmTabCell, cellY, kPmTabCell, kPmTabCell});
    AddMousePiece(c, kPhoneIcon.area, kPmPhoneWell);

    // The still items in ascending layers (the tab's wells on layer 0 come after the frame's
    // layer 1 items above), each layer in the order given; the images after them.
    SortUi(c.ui, first);
}
}
