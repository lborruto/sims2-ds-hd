#include "port/panelmenu.h"

#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

#include "hotspots.h"
#include "port/goals.h"
#include "port/icons.h"
#include "port/interact.h"
#include "port/ownwords.h"
#include "port/options.h"
#include "port/pockets.h"
#include "port/relations.h"
#include "port/statusbar.h"
#include "romdata.h"

namespace sims2hd
{
namespace
{
using namespace ui;

constexpr u32 kPhoneId = 1;
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
    minSize = std::max(minSize, kMinText); // (never below the smallest text: ui::kMinText)
    size = std::max(size, minSize);
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

namespace
{
// KeyedCrop's copies, per DS rectangle: kept until the next crop of the same one (KeptCrop).
std::map<std::tuple<int, int, int, int>, std::vector<u32>>& CropCopies()
{
    static std::map<std::tuple<int, int, int, int>, std::vector<u32>> copies;
    return copies;
}
}

UiItem KeptCrop(Rect from, Rect to, int tier)
{
    const auto it = CropCopies().find({from.x, from.y, from.w, from.h});
    if (it == CropCopies().end() || it->second.size() != (size_t)from.w * from.h) return ImageItem({to.x, to.y, 0, 0}, nullptr, 0, 0, {});
    const std::vector<u32>& px = it->second;
    if (tier > 0) return TierImage(to, px.data(), from.w, from.h, {0, 0, from.w, from.h}, tier);
    return ImageItem(to, px.data(), from.w, from.h, {0, 0, from.w, from.h});
}

UiItem KeyedCrop(const u32* bottom, Rect from, Rect to, int tier)
{
    // (No picture, or a rectangle off the screen: an image item without a source, which draws
    // nothing and takes no room: ImageItem's own empty item.)
    if (!bottom || from.w <= 0 || from.h <= 0 || from.x < 0 || from.y < 0 || from.x + from.w > SW || from.y + from.h > SH)
        return ImageItem({to.x, to.y, 0, 0}, nullptr, 0, 0, {});
    std::vector<u32>& px = CropCopies()[{from.x, from.y, from.w, from.h}];
    px.resize((size_t)from.w * from.h);
    for (int j = 0; j < from.h; j++)
        for (int i = 0; i < from.w; i++) px[(size_t)j * from.w + i] = bottom[(size_t)(from.y + j) * SW + from.x + i] | 0xFF000000;
    std::vector<int> todo;
    auto push = [&](int i, int j) {
        if (std::min({i, j, from.w - 1 - i, from.h - 1 - j}) >= kKeyBand) return; // (only the frame's band)
        u32& c = px[(size_t)j * from.w + i];
        if ((c >> 24) != 0 && KeyedColour(c)) { c = 0; todo.push_back(j * from.w + i); }
    };
    for (int i = 0; i < from.w; i++) { push(i, 0); push(i, from.h - 1); }
    for (int j = 0; j < from.h; j++) { push(0, j); push(from.w - 1, j); }
    while (!todo.empty())
    {
        const int k = todo.back(), i = k % from.w, j = k / from.w;
        todo.pop_back();
        if (i > 0) push(i - 1, j);
        if (i + 1 < from.w) push(i + 1, j);
        if (j > 0) push(i, j - 1);
        if (j + 1 < from.h) push(i, j + 1);
    }
    if (tier > 0) return TierImage(to, px.data(), from.w, from.h, {0, 0, from.w, from.h}, tier);
    return ImageItem(to, px.data(), from.w, from.h, {0, 0, from.w, from.h});
}

bool KeyedColour(u32 c)
{
    c &= 0xFFFFFF;
    return c == 0x383869 || c == 0x414979 || c == 0x51518A || c == 0x5961A2 || c == 0x6171B2 || c == 0x828ACB;
}

int PhoneButtonFrame(IconCache& icons, const u32* bottom)
{
    const SpriteRef ref = SpriteRef::Of(kSprPhoneButton);
    const int frames = icons.Frames(ref);
    if (!bottom || frames <= 0) return 0;
    // Mismatched opaque pixels of frame f placed at (x, y), counting up to `limit`.
    auto misses = [&](const SpriteFrame& f, int x, int y, int limit) {
        int n = 0;
        for (int j = 0; j < f.h; j++)
            for (int i = 0; i < f.w; i++)
            {
                const u32 q = f.argb[(size_t)j * f.w + i];
                if ((q >> 24) == 0) continue;
                const int X = x + i, Y = y + j;
                if (X < 0 || Y < 0 || X >= SW || Y >= SH || ((bottom[(size_t)Y * SW + X] ^ q) & 0xFFFFFF) != 0)
                    if (++n >= limit) return n;
            }
        return n;
    };
    const SpriteFrame* f0 = icons.Frame(ref, 0);
    if (!f0 || f0->w <= 0) return 0;
    int opaque0 = 0;
    for (u32 q : f0->argb) opaque0 += (q >> 24) != 0;
    const int rest = misses(*f0, kPmPhoneSprX, kPmPhoneSprY, opaque0 / 2 + 1);
    if (rest == 0) return 0;              // (at rest: the usual case, one pass)
    if (rest > opaque0 / 2) return 0;     // (not even half of it there: no button on screen, no search)
    // The search (every frame at every shift) runs once per distinct picture of the button: its
    // result is kept by a hash of the crop it reads (the frames' box, 8 pixels around), so a button
    // held lit or blinking between two pictures costs one hash a frame.
    int maxW = 0, maxH = 0;
    for (int k = 0; k < frames; k++)
        if (const SpriteFrame* f = icons.Frame(ref, k)) { maxW = std::max(maxW, f->w); maxH = std::max(maxH, f->h); }
    const int cx0 = std::max(0, kPmPhoneSprX - 8), cy0 = std::max(0, kPmPhoneSprY - 8);
    const int cx1 = std::min(SW, kPmPhoneSprX + 8 + maxW), cy1 = std::min(SH, kPmPhoneSprY + 8 + maxH);
    u64 hash = 14695981039346656037ull;
    for (int y = cy0; y < cy1; y++)
        for (int x = cx0; x < cx1; x++)
        {
            const u32 q = bottom[(size_t)y * SW + x] & 0xFFFFFF;
            for (int b = 0; b < 3; b++) { hash ^= (q >> (8 * b)) & 0xFF; hash *= 1099511628211ull; }
        }
    static const IconCache* cachedFor = nullptr;
    static std::unordered_map<u64, int> found;
    if (cachedFor != &icons || found.size() > 256) { found.clear(); cachedFor = &icons; }
    if (const auto it = found.find(hash); it != found.end()) return it->second;
    int best = 0;
    double bestShare = 1;
    for (int k = 0; k < frames; k++)
    {
        const SpriteFrame* f = icons.Frame(ref, k);
        if (!f || f->w <= 0) continue;
        int opaque = 0;
        for (u32 q : f->argb) opaque += (q >> 24) != 0;
        if (opaque == 0) continue;
        for (int dy = -8; dy <= 8; dy++)
            for (int dx = -8; dx <= 8; dx++)
            {
                const int limit = (int)(bestShare * opaque) + 1;
                const int n = misses(*f, kPmPhoneSprX + dx, kPmPhoneSprY + dy, limit);
                if ((double)n / opaque < bestShare) { bestShare = (double)n / opaque; best = k; }
            }
    }
    const int frame = bestShare <= 0.1 ? best : 0;
    found.emplace(hash, frame);
    return frame;
}

void AddFocusRing(Composition& c, Rect r, float radius, int layer, u32 behind)
{
    FocusRingItems g = FocusRing(r, radius, behind);
    g.outer.layer = g.inner.layer = layer;
    c.ui.push_back(g.outer);
    c.ui.push_back(g.inner);
}

int AddHint(Composition& c, int right, Button b, const std::string& label)
{
    const Rect r = ChipRect(0, 0, b, label, kPmHintSize, 0, Weight::ExtraBold);
    UiItem it = ChipItem(right - r.w, kPmHints.y + (kPmHints.h - r.h) / 2, b, label, kPmHintSize, 0, Weight::ExtraBold);
    it.layer = 1;
    c.ui.push_back(it);
    return right - r.w - 28;
}

std::vector<DPadHint> PanelDPadHints(const MenuView& v, int focus, const RomData& rom)
{
    std::vector<DPadHint> h;
    // The tab's items the focus can go to (the phone left out: it is one more stop above them).
    int items = 0, x0 = -1;
    bool column = true;
    for (const ViewItem& it : v.items)
        if (it.enabled && it.id != kPhoneId)
        {
            items++;
            if (x0 < 0) x0 = it.rect.x;
            else if (it.rect.x != x0) column = false;
        }
    const bool slider = MenuFocusable(v, focus) && (!v.items[focus].left.empty() || !v.items[focus].right.empty());
    const bool gameSel = MenuFocusable(v, v.selected) && focus == v.selected && (!v.scrollUp.empty() || !v.scrollDown.empty());
    const bool targets = !v.buttons[(int)MenuButton::Left].empty() || !v.buttons[(int)MenuButton::Right].empty();
    // (Left / Right do something else on a slider or with targets: the cross's vertical arms alone.)
    if (items >= 2 || gameSel) h.push_back({column || slider || targets ? Button::DPadV : Button::DPad, OwnWord(&rom, OwnWordId::HintChoose)});
    if (slider) h.push_back({Button::DPadH, OwnWord(&rom, OwnWordId::HintAdjust)});            // (as every slider's hint)
    else if (targets) h.push_back({Button::DPadH, OwnWord(&rom, OwnWordId::PromptOther)});     // (as the world's prompt says it)
    return h;
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
    default: return v; // (no tab: the framed-screen composition)
    }
    // The phone button, after the tab's own items (the first focus is the tab's), linked to them
    // explicitly: the geometry alone puts it at the edge of the 60° cone from the rows.
    const int tabFirst = MenuFirst(v), phoneAt = (int)v.items.size();
    for (int i = 0; i < phoneAt; i++)
        if (v.items[i].enabled && v.items[i].next[0] == -1 && MenuNeighbour(v, i, 0, -1) == i) v.items[i].next[0] = phoneAt;
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

    // The hints: A (the focused item's own word when it has one), B closes, X, and the D-pad's
    // (each with its word: PanelDPadHints) at the left of them; A's and X's words fit the room
    // the D-pad's leave.
    std::vector<DPadHint> dpad = PanelDPadHints(v, focus, rom);
    auto dpadWidth = [&] {
        int w = 0;
        for (const DPadHint& d : dpad) w += ChipRect(0, 0, d.button, d.word, kPmHintSize, 0, Weight::ExtraBold).w + 28;
        return w;
    };
    int dpadW = dpadWidth();
    // (Two D-pad hints and an A word they would cut, a long verb beside Interact's "Other target":
    // "Choose" goes, the hint of what Left / Right do stays; MeasureInteract measures that room.)
    if (dpad.size() == 2 && MenuFocusable(v, focus) && !v.items[focus].action.empty())
    {
        const int roomA = kPmHints.x + kPmHints.w - 8 - ChipRect(0, 0, Button::B, MenuWords(rom, kStrClose), kPmHintSize, 0, Weight::ExtraBold).w - 28;
        if (!FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, roomA - kPmHints.x - 60 - dpadW, Weight::ExtraBold).fits)
        {
            dpad.erase(dpad.begin());
            dpadW = dpadWidth();
        }
    }
    int right = kPmHints.x + kPmHints.w - 8;
    right = AddHint(c, right, Button::B, MenuWords(rom, kStrClose));
    if (MenuFocusable(v, focus) && !v.items[focus].action.empty()) // (nothing to perform: no A)
    {
        const FittedText a = FitLine(v.items[focus].word, kPmHintSize, kPmHintSize, right - kPmHints.x - 60 - dpadW, Weight::ExtraBold);
        right = AddHint(c, right, Button::A, a.text);
    }
    if (MenuFocusable(v, focus) && !v.items[focus].buttonX.empty())
    {
        // An asking X armed (Pockets' bin): the hint says what the second X does.
        const bool armed = v.items[focus].xAsks && v.armed != 0 && v.armed == v.items[focus].id;
        const std::string word = armed ? OwnWord(&rom, OwnWordId::TrashAgain) : v.items[focus].xWord;
        const FittedText x = FitLine(word, kPmHintSize, kPmHintSize, right - kPmHints.x - 60 - dpadW, Weight::ExtraBold);
        right = AddHint(c, right, Button::X, x.text);
    }
    for (const DPadHint& d : dpad) right = AddHint(c, right, d.button, d.word);

    // The icons (images: drawn above the still layers): the five tabs, the selected one green.
    if (icons)
        for (int i = 0; i < 5; i++)
        {
            const SpriteRef ref = SpriteRef::Of(i == tab ? kTabIconGreen : kTabIconBlue);
            if (icons->Frame(ref, i)) c.ui.push_back(icons->Tiered(ref, i, kTierL, {kPmTabsX + i * kPmTabCell, cellY, kPmTabCell, kPmTabCell}));
        }
    // The phone: the game's own button sprite, in the frame the touch screen shows (ringing too),
    // at the tabs' tier.
    if (icons)
    {
        const int frame = PhoneButtonFrame(*icons, bottom);
        if (icons->Frame(SpriteRef::Of(kSprPhoneButton), frame))
            c.ui.push_back(icons->Tiered(SpriteRef::Of(kSprPhoneButton), frame, kTierS, kPmPhoneWell));
    }

    // The mouse: the tab icons and the phone onto the game's own.
    for (int i = 0; i < 5; i++) AddMousePiece(c, kTabIcon[i].area, {kPmTabsX + i * kPmTabCell, cellY, kPmTabCell, kPmTabCell});
    AddMousePiece(c, kPhoneIcon.area, kPmPhoneWell);

    // The still items in ascending layers (the tab's wells on layer 0 come after the frame's
    // layer 1 items above), each layer in the order given; the images after them.
    SortUi(c.ui, first);
}

void PanelText(Composition& c, Rect r, const std::string& s, float size, float minSize, ui::Align a, ui::Weight w, u32 colour, int layer)
{
    const FittedText f = FitLine(s, size, minSize, r.w, w);
    ui::UiItem t = ui::TextItem(r, f.text, f.size, a, w);
    t.wrap = false;
    t.middle = true;
    if (colour) t.colour = colour;
    t.layer = layer;
    c.ui.push_back(t);
}

void Measurer::Note(int id, bool shrunk, bool fits, int lines) const
{
    m.checked++;
    if (shrunk) m.shrunk++;
    if (lines > m.tallestLines) { m.tallestLines = lines; m.tallestId = id; }
    if (!fits && std::find(m.cutIds.begin(), m.cutIds.end(), id) == m.cutIds.end()) { m.cut++; m.cutIds.push_back(id); }
}
void Measurer::Line(int id, const std::string& text, float size, float minSize, int width, ui::Weight w) const
{
    const FittedText f = FitLine(text, size, minSize, width, w);
    Note(id, f.size < size, f.fits);
}
void Measurer::Block(int id, const std::string& text, float size, float minSize, Rect box) const
{
    const FittedBlock f = FitBlock(text, size, minSize, box);
    Note(id, f.size < size, f.fits, f.lines);
}
}
