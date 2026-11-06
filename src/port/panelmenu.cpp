#include "port/panelmenu.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "hotspots.h"
#include "port/icons.h"
#include "port/interact.h"
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

std::string MenuWords(const RomData& rom, int id)
{
    const std::string in = rom.String(id);
    std::string s;
    for (size_t i = 0; i < in.size(); i++)
    {
        const char c = in[i];
        if (c == '{')
        {   // RomData's escapes ({xx}, {F0xx}) dropped.
            const size_t e = in.find('}', i);
            if (e != std::string::npos && e - i <= 8) { i = e; continue; }
        }
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
        t.pop_back();
        while (!t.empty() && ((unsigned char)t.back() & 0xC0) == 0x80) t.pop_back();
        if (!t.empty() && ((unsigned char)t.back() & 0xC0) == 0xC0) t.pop_back();
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
    return PanelMenuViewOf(s, *game);
}

MenuView PanelMenuViewOf(ScreenState s, const GameState& game)
{
    MenuView v;
    if (s.screen != Screen::Panel || !game.valid) return v;
    switch (s.tab)
    {
    case Tab::Interact:
        if (!game.town.interact.valid) return v;
        AddInteractView(v, game.town.interact);
        break;
    default: return v; // (the other tabs: milestone 1's composition until they are redrawn)
    }
    // The phone button, after the tab's own items (the first focus is the tab's).
    ViewItem phone;
    phone.rect = {kPmPhone.x + 8, kPmPhone.y + 4, kPmPhone.w - 16, kPmPhone.h - 8};
    phone.action = {TapStep(kPhoneIcon.x, kPhoneIcon.y)};
    phone.id = kPhoneId;
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
    UiItem phoneCell = PanelItem({kPmPhone.x + 8, kPmPhone.y + 4, kPmPhone.w - 16, kPmPhone.h - 8});
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

    // The tab's title.
    std::string name;
    if (s.tab == Tab::Interact && game.town.interact.target.kind != TargetRef::None && game.town.interact.target.nameId >= 0)
        name = MenuWords(rom, game.town.interact.target.nameId);
    const FittedText t = FitLine(MenuTitle(rom, tab, name), kPmTitleSize, kPmTextMin, kPmTitle.w - 48, Weight::ExtraBold);
    UiItem tt = TextItem({kPmTitle.x + 24, kPmTitle.y, kPmTitle.w - 48, kPmTitle.h}, t.text, t.size, Align::Centre, Weight::ExtraBold);
    tt.wrap = false;
    tt.middle = true;
    tt.layer = 1;
    c.ui.push_back(tt);

    // The tab's content.
    if (s.tab == Tab::Interact) BuildInteract(c, v, focus, game, rom, icons, bottom);

    // The hints: A (the focused item's own word when it has one), B closes.
    int right = kPmHints.x + kPmHints.w - 8;
    right = AddHint(c, right, Button::B, MenuWords(rom, kStrClose));
    std::string aWord;
    if (focus >= 0 && focus < (int)v.items.size() && v.items[focus].enabled)
    {
        if (s.tab == Tab::Interact && focus < (int)game.town.interact.actions.size())
            aWord = InteractLabel(rom, game.town.interact.actions[focus].labelId);
        else if (v.items[focus].id == kPhoneId) aWord = MenuWords(rom, kStrPhone);
    }
    if (focus >= 0 && focus < (int)v.items.size() && v.items[focus].enabled)
    {
        const FittedText a = FitLine(aWord, kPmHintSize, kPmHintSize, right - kPmHints.x - 60, Weight::ExtraBold);
        AddHint(c, right, Button::A, a.text);
    }

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
    AddMousePiece(c, kPhoneIcon.area, {kPmPhone.x + 8, kPmPhone.y + 4, kPmPhone.w - 16, kPmPhone.h - 8});

    // The still items in ascending layers (the tab's wells on layer 0 come after the frame's
    // layer 1 items above), each layer in the order given; the images after them.
    std::stable_sort(c.ui.begin() + (long)first, c.ui.end(), [](const UiItem& a, const UiItem& b) {
        const int ka = a.kind == UiItem::Image ? 1 << 20 : a.layer, kb = b.kind == UiItem::Image ? 1 << 20 : b.layer;
        return ka < kb;
    });
}
}
