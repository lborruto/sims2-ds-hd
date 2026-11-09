#include "shell.h"

#include <algorithm>
#include <cmath>

#include "port/ownwords.h"
#include "ui/font.h"

namespace sims2wide::shell
{
using namespace ui;
using W = OwnWordId;

namespace
{
constexpr u32 kVeil = 0xB8101218;            // over the game
// The rows' fills, notes and inactive text: the kit's palette (ui::kFillRow, kFillFocus,
// kTextNote, kTextInactive: 4.5:1 and more for text, 3:1 for inactive). kWarm only on the focused fill.
constexpr u32 kWarm = 0xFFFFF1C9;
constexpr Rect kPanel {200, 32, 880, 656};
constexpr Rect kContent {kPanel.x + 32, kPanel.y + 84, kPanel.w - 64, kPanel.h - 84 - 80};
constexpr int kHintsY = kPanel.y + kPanel.h - 66, kHintsH = 44;
constexpr float kHint = 24;
constexpr int kRowH = 52, kRowStep = 56;     // Keys: a row
int ListRows(Page) { return 7; }             // (Keys: its columns' heads above)
int ListTop(Page) { return 40; }
// Settings: a row, the Advanced heading, the list's height (every tab fits: no scrolling; the UI's
// size zooms the whole panel, held to the canvas), the note under it, the tab bar in the title band.
constexpr int kSetRowH = 50, kSetRowStep = 54, kSetHeadH = 32, kSetListH = 410;
constexpr int kTabBarY = kPanel.y + 20, kTabBarH = 48;
constexpr Rect kAskBox {280, 200, 720, 320};
// The TV's safe area (the outer 5 % left clear: x 64-1216, y 36-684); the launch panel inside it
// at every UI size (Zoom).
constexpr Rect kSafe {64, 36, 1152, 648};
constexpr Rect kLaunchList {64, 104, 664, 580}, kLaunchCard {752, 104, 464, 580};

const char* Wd(int lang, W id) { return OwnWordIn(lang, id); }
std::string Fill(std::string s, const std::string& v)
{
    if (size_t at = s.find("@1"); at != std::string::npos) s.replace(at, 2, v);
    return s;
}

// One line, shrunk from size down to min, then cut with an ellipsis.
UiItem Line(Rect r, const std::string& text, float size, float min, Align a = Align::Left, Weight w = Weight::Bold)
{
    min = std::max(min, kMinText); // (never below the smallest text: ui::kMinText)
    float s = std::max(size, min);
    std::string t = text;
    while (s > min && TextWidth(t, s, w) > r.w) s -= 1;
    if (TextWidth(t, s, w) > r.w)
    {
        const std::string dots = "\xE2\x80\xA6";
        while (!t.empty() && TextWidth(t + dots, s, w) > r.w)
        {
            while (!t.empty() && ((unsigned char)t.back() & 0xC0) == 0x80) t.pop_back();
            if (!t.empty()) t.pop_back();
        }
        t += dots;
    }
    UiItem it = TextItem(r, t, s, a, w);
    it.wrap = false;
    it.middle = true;
    return it;
}

// The items by layer (the kit draws layers in ascending order; give them so).
struct Items
{
    std::vector<UiItem> l[6];
    void Add(int layer, UiItem it)
    {
        it.layer = layer;
        l[layer].push_back(std::move(it));
    }
    std::vector<UiItem> All()
    {
        std::vector<UiItem> v;
        for (auto& x : l) v.insert(v.end(), x.begin(), x.end());
        return v;
    }
};

// The focus ring (ui::FocusRing: the green, a white line inside it) on `layer`, before the item.
void Ring(Items& it, int layer, Rect r, float radius)
{
    const FocusRingItems g = FocusRing(r, radius);
    it.Add(layer, g.outer);
    it.Add(layer, g.inner);
}

UiItem Button(Rect r, bool focused, bool enabled = true)
{
    UiItem b = PanelItem(r);
    b.fill = !enabled ? GameTheme().fillDark : focused ? kFillFocus : kFillRow;
    b.borderArgb = GameTheme().frame;
    b.border = 3;
    b.radius = std::min(r.h / 2.0f, 24.0f);
    return b;
}

// The hints at the panel's foot, right to left; returns the x left of them.
int Hint(Items& it, int right, int y, ui::Button b, const std::string& label)
{
    const Rect r = ChipRect(0, 0, b, label, kHint, 0, Weight::ExtraBold);
    it.Add(1, ChipItem(right - r.w, y + (kHintsH - r.h) / 2, b, label, kHint, 0, Weight::ExtraBold));
    return right - r.w - 28;
}

bool Inside(const Rect& r, float x, float y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

constexpr Layout kLayouts[] = {Layout::Console, Layout::Auto, Layout::Focus, Layout::Side, Layout::Stacked};

// The controls card's entries: chips (a button of the current look, or a binding's glyph) and a word.
struct CardEntry { std::vector<ui::Button> chips; int action; W word; };
const std::vector<CardEntry>& Card()
{
    static const std::vector<CardEntry> c = {
        {{ui::Button::DPad, ui::Button::LStick}, -1, W::CtlWalk},
        {{ui::Button::A}, -1, W::CtlTouch},
        {{ui::Button::B}, -1, W::CtlBack},
        {{ui::Button::X, ui::Button::Y}, -1, W::CtlZoom},
        {{ui::Button::RStick}, -1, W::CtlCamera}, // (in the world: it turns, and zooms up / down)
        {{ui::Button::Back}, -1, W::Menu},
        {{ui::Button::Pause}, -1, W::ShellPauseMenu},
        {{ui::Button::RStick}, -1, W::Cursor},
        {{}, Controls::FAST, W::CtlFast},
    };
    return c;
}

// The controls card in a box: one or two columns of entries.
void AddCard(Items& it, Rect box, int columns, const Model& m)
{
    const std::vector<CardEntry>& c = Card();
    const int rows = ((int)c.size() + columns - 1) / columns, colW = box.w / columns;
    const int rowH = std::min(56, box.h / std::max(rows, 1));
    for (int i = 0; i < (int)c.size(); i++)
    {
        const int col = i / rows, row = i % rows;
        const int x = box.x + col * colW, y = box.y + row * rowH;
        int cx = x + 8;
        for (ui::Button b : c[i].chips)
        {
            const Rect r = ChipRect(0, 0, b, "", 26, 0, Weight::ExtraBold);
            it.Add(1, ChipItem(cx, y + (rowH - r.h) / 2, b, "", 26, 0, Weight::ExtraBold));
            cx += r.w + 6;
        }
        if (c[i].action >= 0)
        {
            const std::string& g = m.keyboard ? m.keyGlyph[c[i].action] : m.padGlyph[c[i].action];
            if (!g.empty())
            {
                UiItem chip = ChipItemAs(cx, 0, m.keyboard ? Face::Keyboard : m.padFace, g, "", 26, Weight::ExtraBold);
                chip.to.y = y + (rowH - chip.to.h) / 2;
                cx += chip.to.w + 6;
                it.Add(1, chip);
            }
        }
        const int lx = std::max(x + 132, cx + 8);
        it.Add(1, Line({lx, y, x + colW - lx - 8, rowH}, Wd(m.language, c[i].word), 24, 17));
    }
}

void AddThumb(Items& it, std::vector<UiItem>& images, const Thumb& t, Rect r)
{
    UiItem well = WellItem({r.x - 4, r.y - 4, r.w + 8, r.h + 8});
    well.radius = 8;
    it.Add(1, well);
    if (t.w > 0)
    {
        UiItem im = ImageItem(r, t.px.data(), t.w, t.h, {0, 0, t.w, t.h});
        im.smooth = false; // (a picture of the screen made small: never the pixel-art scaler's)
        images.push_back(im);
    }
}
}

// ---- the settings' rows

void ChangeRow(Row r, int dir, Settings& s)
{
    const int d = dir < 0 ? -1 : 1;
    auto cycle = [&](int v, int n) { return (v + n + d) % n; };
    switch (r)
    {
    case Row::Layout:
    {
        int i = 0;
        while (i < 5 && kLayouts[i] != s.layout) i++;
        if (i == 5) i = 0;
        s.layout = kLayouts[(i + 5 + d) % 5];
        break;
    }
    case Row::Fullscreen: s.fullscreen = !s.fullscreen; break;
    case Row::Scaling: s.scaling = (Scaling)cycle((int)s.scaling, 3); break;
    case Row::UiSize:
        // Auto (0), then 60 .. 150 % by 5; Left from Auto to 150, Right from 150 to Auto.
        if (s.uiScale == 0) s.uiScale = d > 0 ? 60 : 150;
        else if ((d > 0 && s.uiScale >= 150) || (d < 0 && s.uiScale <= 60)) s.uiScale = 0;
        else s.uiScale = std::clamp(s.uiScale + 5 * d, 60, 150);
        break;
    case Row::Pictures: s.smoothPictures = !s.smoothPictures; break;
    case Row::Res3D: s.resolution3D = std::clamp(s.resolution3D + d, 1, 4); break;
    case Row::Wide: s.widescreen = !s.widescreen; break;
    case Row::Fps60: s.interpolate60 = !s.interpolate60; break;
    case Row::Battery: s.batterySaver = !s.batterySaver; break;
    case Row::Volume: s.volume = std::clamp(s.volume + 5 * d, 0, 100); break;
    case Row::Mute: s.mute = !s.mute; break;
    case Row::FastSound: s.fastSound = !s.fastSound; break;
    case Row::Background: s.runInBackground = !s.runInBackground; break;
    case Row::FastSpeed:
    {
        // Maximum (0), then 2, 3, 4 times.
        static const int speeds[] = {0, 2, 3, 4};
        int i = 0;
        while (i < 4 && speeds[i] != s.fastSpeed) i++;
        s.fastSpeed = speeds[cycle(i == 4 ? 0 : i, 4)];
        break;
    }
    case Row::FastKey: s.fastToggle = !s.fastToggle; break;
    case Row::FastBoot: s.fastBoot = !s.fastBoot; break;
    case Row::HidePointer: s.hidePointer = !s.hidePointer; break;
    case Row::StickCursor: s.stickCursor = !s.stickCursor; break;
    case Row::CursorSpeed: s.cursorSpeed = std::clamp(s.cursorSpeed + 25 * d, 50, 200); break;
    case Row::DeadZone: s.stickDeadZone = std::clamp(s.stickDeadZone + 5 * d, 5, 60); break;
    case Row::LeftHanded: s.leftHanded = !s.leftHanded; break;
    case Row::Trigger: s.triggerThreshold = std::clamp(s.triggerThreshold + 10 * d, 10, 90); break;
    case Row::Rumble: s.rumble = !s.rumble; break;
    case Row::Mic: s.mic = (MicMode)cycle((int)s.mic, 3); break;
    case Row::Language: s.language = cycle(s.language, 6); break;
    case Row::Keys: case Row::Count: break;
    }
}

const TabLayout& TabRows(int tab)
{
    static const TabLayout tabs[kTabs] = {
        {{Row::Fullscreen, Row::UiSize, Row::Scaling, Row::Pictures, Row::HidePointer, Row::Layout}, 5},
        {{Row::Battery, Row::Res3D, Row::Fps60, Row::Wide}, 3},
        {{Row::Volume, Row::Mute, Row::Background, Row::FastSound}, 3},
        {{Row::Keys, Row::StickCursor, Row::CursorSpeed, Row::LeftHanded, Row::DeadZone, Row::Trigger, Row::Rumble}, 4},
        {{Row::Language, Row::Mic, Row::FastSpeed, Row::FastKey, Row::FastBoot}, 4},
    };
    return tabs[std::clamp(tab, 0, kTabs - 1)];
}

std::string TabName(int tab, int lang)
{
    static const W words[kTabs] = {W::TabDisplay, W::TabGraphics, W::TabSound, W::ShellControls, W::TabGame};
    return tab >= 0 && tab < kTabs ? Wd(lang, words[tab]) : "";
}

bool RowHeld(Row r, const Settings& s) { return s.batterySaver && (r == Row::Res3D || r == Row::Fps60); }

void ResetTabRows(int tab, Settings& s)
{
    const Settings d;
    for (Row r : TabRows(tab).rows)
        switch (r)
        {
        case Row::Layout: s.layout = d.layout; break;
        case Row::Fullscreen: s.fullscreen = d.fullscreen; break;
        case Row::Scaling: s.scaling = d.scaling; break;
        case Row::UiSize: s.uiScale = d.uiScale; break;
        case Row::Pictures: s.smoothPictures = d.smoothPictures; break;
        case Row::Res3D: s.resolution3D = d.resolution3D; break;
        case Row::Wide: s.widescreen = d.widescreen; break;
        case Row::Fps60: s.interpolate60 = d.interpolate60; break;
        case Row::Battery: s.batterySaver = d.batterySaver; break;
        case Row::Volume: s.volume = d.volume; break;
        case Row::Mute: s.mute = d.mute; break;
        case Row::FastSound: s.fastSound = d.fastSound; break;
        case Row::Background: s.runInBackground = d.runInBackground; break;
        case Row::FastSpeed: s.fastSpeed = d.fastSpeed; break;
        case Row::FastKey: s.fastToggle = d.fastToggle; break;
        case Row::FastBoot: s.fastBoot = d.fastBoot; break;
        case Row::HidePointer: s.hidePointer = d.hidePointer; break;
        case Row::StickCursor: s.stickCursor = d.stickCursor; break;
        case Row::CursorSpeed: s.cursorSpeed = d.cursorSpeed; break;
        case Row::DeadZone: s.stickDeadZone = d.stickDeadZone; break;
        case Row::LeftHanded: s.leftHanded = d.leftHanded; break;
        case Row::Trigger: s.triggerThreshold = d.triggerThreshold; break;
        case Row::Rumble: s.rumble = d.rumble; break;
        case Row::Mic: s.mic = d.mic; break;
        case Row::Language: s.language = d.language; break;
        case Row::Keys: case Row::Count: break; // (the bindings keep their own reset)
        }
}

std::string RowLabel(Row r, int lang)
{
    static const W words[kRows] = {W::SetLayout, W::SetFullscreen, W::SetScaling, W::SetUiSize, W::SetPictures, W::SetRes3D, W::SetWide, W::SetFps60, W::SetBattery,
                                   W::SetVolume, W::SetMute, W::SetFastSound, W::SetBackground,
                                   W::SetFastSpeed, W::SetFastKey, W::SetFastBoot,
                                   W::SetHidePointer, W::SetStickCursor, W::SetCursorSpeed, W::SetDeadZone, W::SetLeftHanded, W::SetTrigger, W::SetRumble,
                                   W::SetMic, W::SetLanguage, W::CtlKeys};
    return (int)r >= 0 && (int)r < kRows ? Wd(lang, words[(int)r]) : "";
}

int RowNote(Row r)
{
    static const W notes[kRows] = {W::LayoutNote, W::FullscreenNote, W::ScalingNote, W::UiSizeNote, W::PicturesNote, W::Res3DNote, W::WideNote, W::Fps60Note, W::BatteryNote,
                                   W::VolumeNote, W::MuteNote, W::FastSoundNote, W::BackgroundNote,
                                   W::FastSpeedNote, W::FastKeyNote, W::FastBootNote,
                                   W::HidePointerNote, W::StickCursorNote, W::CursorSpeedNote, W::DeadZoneNote, W::LeftHandedNote, W::TriggerNote, W::RumbleNote,
                                   W::MicNote, W::LangNote, W::KeysNote};
    return (int)r >= 0 && (int)r < kRows ? (int)notes[(int)r] : (int)W::Count;
}

std::string RowValue(Row r, const Settings& s, int lang)
{
    auto onOff = [&](bool b) { return std::string(Wd(lang, b ? W::ShellOn : W::ShellOff)); };
    switch (r)
    {
    case Row::Layout:
    {
        const W w[] = {W::LayoutConsole, W::LayoutAuto, W::LayoutFocus, W::LayoutSide, W::LayoutStacked};
        for (int i = 0; i < 5; i++)
            if (kLayouts[i] == s.layout) return Wd(lang, w[i]);
        return "";
    }
    case Row::Fullscreen: return onOff(s.fullscreen);
    case Row::Scaling: return Wd(lang, s.scaling == Scaling::Smooth ? W::ScaleSmooth : s.scaling == Scaling::Nearest ? W::ScaleNearest : W::ScaleSharp);
    case Row::UiSize: return s.uiScale == 0 ? std::string(Wd(lang, W::UiAuto)) : std::to_string(s.uiScale) + " %";
    case Row::Pictures: return Wd(lang, s.smoothPictures ? W::ScaleSmooth : W::ScaleNearest);
    case Row::Res3D: return std::to_string(s.Res3D()) + "\xC3\x97"; // "2×" (Battery saver's 1× while it holds the row)
    case Row::Wide: return onOff(s.widescreen);
    case Row::Fps60: return onOff(s.Interpolate60());
    case Row::Battery: return onOff(s.batterySaver);
    case Row::Volume: return std::to_string(s.volume) + " %";
    case Row::Mute: return onOff(s.mute);
    case Row::FastSound: return onOff(s.fastSound);
    case Row::Background: return onOff(!s.runInBackground); // ("Pause in the background": the setting's opposite)
    case Row::FastSpeed: return s.fastSpeed >= 2 ? std::to_string(s.fastSpeed) + "\xC3\x97" : Wd(lang, W::FastMax);
    case Row::FastKey: return Wd(lang, s.fastToggle ? W::FastPress : W::FastHold);
    case Row::FastBoot: return onOff(s.fastBoot);
    case Row::HidePointer: return onOff(s.hidePointer);
    case Row::StickCursor: return onOff(s.stickCursor);
    case Row::CursorSpeed: return std::to_string(s.cursorSpeed) + " %";
    case Row::DeadZone: return std::to_string(s.stickDeadZone) + " %";
    case Row::LeftHanded: return onOff(s.leftHanded);
    case Row::Trigger: return std::to_string(s.triggerThreshold) + " %";
    case Row::Rumble: return onOff(s.rumble);
    case Row::Mic: return s.mic == MicMode::Off ? Wd(lang, W::ShellOff) : s.mic == MicMode::Real ? Wd(lang, W::MicReal) : Wd(lang, W::MicBlowKey);
    case Row::Language:
    {
        // Each language by its own name.
        static const char* names[] = {"English", "Français", "Deutsch", "Italiano", "Español"};
        return s.language >= 1 && s.language <= 5 ? names[s.language - 1] : Wd(lang, W::LangAuto);
    }
    case Row::Keys: case Row::Count: break;
    }
    return "";
}

std::string ActionLabel(int a, int lang)
{
    using namespace Controls;
    switch (a)
    {
    case A: return "DS A";
    case B: return "DS B";
    case SELECT: return "DS Select";
    case START: return "DS Start";
    case RIGHT: return Wd(lang, W::ActRight);
    case LEFT: return Wd(lang, W::ActLeft);
    case UP: return Wd(lang, W::ActUp);
    case DOWN: return Wd(lang, W::ActDown);
    case R: return "DS R";
    case L: return "DS L";
    case X: return "DS X";
    case Y: return "DS Y";
    case MENU: return Wd(lang, W::ShellPauseMenu);
    case FAST: return Wd(lang, W::CtlFast);
    case SWAP: return Wd(lang, W::ActSwap);
    case SAVESTATE: return Wd(lang, W::ActSaveState);
    case LOADSTATE: return Wd(lang, W::ActLoadState);
    case BLOW: return Wd(lang, W::ActBlow);
    case TOUCH: return Wd(lang, W::ActTouch);
    case PANEL: return Wd(lang, W::Menu);
    default: return "";
    }
}

std::vector<MainItem> MainEntries(const Model& m)
{
    std::vector<MainItem> e {MainItem::Resume, MainItem::Settings, MainItem::Controls, MainItem::States, MainItem::ChangeGame};
    if (m.canOpenFolder) e.push_back(MainItem::OpenFolder);
    e.push_back(MainItem::Quit);
    return e;
}

PadIns ShellPadIns(const Controls::Bindings& b)
{
    using namespace Controls;
    PadIns r;
    r.accept = b.Pad(A) >= 0 ? b.Pad(A) : b.Pad(B) == IN_A ? IN_B : IN_A;
    if (b.Pad(B) >= 0) r.back = b.Pad(B) != r.accept ? b.Pad(B) : -1;
    else if (r.accept != IN_B) r.back = IN_B;
    else if (r.accept != IN_A && b.ActionOfPad(IN_A) < 0) r.back = IN_A;
    return r;
}

PadNav ShellPadNav(const Controls::Bindings& b, const bool* now, const bool* before)
{
    using namespace Controls;
    const PadIns ins = ShellPadIns(b);
    auto pressed = [&](int in) { return in >= 0 && in < IN_COUNT && now[in] && !before[in]; };
    // X / Y only on an input that is neither accept nor back: one press, one shell action.
    auto own = [&](int a) { const int in = b.Pad(a); return in != ins.accept && in != ins.back && pressed(in); };
    PadNav n;
    n.a = pressed(ins.accept);
    n.b = pressed(ins.back);
    n.x = own(X);
    n.y = own(Y);
    n.lb = own(L);
    n.rb = own(R);
    return n;
}

std::vector<LaunchEntry> LaunchEntries(const Model& m)
{
    std::vector<LaunchEntry> e;
    if (!m.recents.empty()) e.push_back({LaunchItem::Continue, 0});
    e.push_back({LaunchItem::Choose, 0});
    for (int i = 1; i < (int)m.recents.size() && i < 3; i++) e.push_back({LaunchItem::Recent, i});
    e.push_back({LaunchItem::Language, 0});
    e.push_back({LaunchItem::Quit, 0});
    return e;
}

// ---- navigation

int Shell::Count(const Model& m) const
{
    switch (page)
    {
    case Page::Main: return (int)MainEntries(m).size();
    case Page::Settings: return (int)TabRows(tab).rows.size();
    case Page::Controls: return 1;
    case Page::Keys: return Controls::COUNT + 1;
    case Page::States: return kStateSlots + 1;
    case Page::Launch: return (int)LaunchEntries(m).size();
    }
    return 0;
}

bool Shell::Focusable(const Model& m, int i) const
{
    if (i < 0 || i >= Count(m)) return false;
    if (page == Page::States) return m.statesOk && (i < kStateSlots || m.canUndo);
    return true;
}

void Shell::Open(Page p)
{
    open = true;
    ask = Ask::None;
    sequence = false;
    hover_ = -2;
    Go(p, 0);
}

void Shell::Go(Page p, int f)
{
    page = p;
    focus = f;
    scroll = 0;
    rects_.clear();
}

void Shell::Move(const Model& m, int dir)
{
    const int n = Count(m);
    if (n <= 0) return;
    if (page == Page::States)
    {
        // The 2 x 2 grid, then Undo under it: 0 1 / 2 3 / 4.
        int f = focus;
        if (dir == 0) f = f == 4 ? 2 : f >= 2 ? f - 2 : f;      // up
        else if (dir == 1) f = f < 2 ? f + 2 : 4;               // down
        else if (dir == 2) f = f == 1 || f == 3 ? f - 1 : f;    // left
        else f = f == 0 || f == 2 ? f + 1 : f;                  // right
        if (Focusable(m, f)) focus = f;
        return;
    }
    if (dir == 2 || dir == 3) return;
    const int step = dir == 0 ? -1 : 1;
    for (int k = 1; k <= n; k++)
    {
        const int f = (focus + step * k + n * 4) % n;
        if (Focusable(m, f)) { focus = f; return; }
    }
}

Result Shell::Update(const Input& in0, Model& m)
{
    Input in = in0;
    Result res;
    if (!open || m.capturing >= 0) return res;

    // The mouse: a move onto another item focuses it, a click presses it (on a value row's left
    // third: Left, else Right); a click outside the panel is B.
    const std::vector<Rect>& rects = ask != Ask::None || !m.message.empty() ? askRects_ : rects_;
    // A click on a tab of the settings' tab bar shows it.
    if (in.click && page == Page::Settings && ask == Ask::None && m.message.empty())
        for (int t = 0; t < (int)tabRects_.size(); t++)
            if (Inside(tabRects_[t], in.mx, in.my))
            {
                tab = t;
                focus = 0;
                return res;
            }
    if (in.hover || in.click)
    {
        int at = -1;
        for (int i = 0; i < (int)rects.size(); i++)
            if (Inside(rects[i], in.mx, in.my)) at = i;
        int& f = ask != Ask::None || !m.message.empty() ? askFocus : focus;
        if (at >= 0 && (in.click || at != hover_) && (ask != Ask::None || !m.message.empty() || Focusable(m, at))) f = at;
        hover_ = at;
        if (in.click)
        {
            if (at >= 0)
            {
                const bool valueRow = (page == Page::Settings && ask == Ask::None && at < Count(m) && TabRows(tab).rows[at] != Row::Keys) ||
                                      (page == Page::Launch && ask == Ask::None && at < (int)LaunchEntries(m).size() && LaunchEntries(m)[at].kind == LaunchItem::Language);
                if (valueRow && m.message.empty()) (in.mx < rects[at].x + rects[at].w / 3 ? in.left : in.right) = true;
                else in.a = true;
            }
            else if (!Inside(panel_, in.mx, in.my)) in.b = true;
        }
    }

    // A message: only A closes it.
    if (!m.message.empty())
    {
        if (in.a) m.message.clear();
        return res;
    }

    if (ask != Ask::None)
    {
        if (in.left || in.up) askFocus = 0;
        if (in.right || in.down) askFocus = 1;
        if (in.b || (in.a && askFocus == 0)) { ask = Ask::None; return res; }
        if (in.a && askFocus == 1)
        {
            const Ask was = ask;
            ask = Ask::None;
            if (was == Ask::Quit) res.act = Act::Quit;
            else if (was == Ask::Load) res = {Act::LoadSlot, askSlot};
            else if (was == Ask::Overwrite) res = {Act::SaveSlot, askSlot};
            else if (was == Ask::ResetTab)
            {
                ResetTabRows(tab, *m.settings);
                res = {Act::ResetTab, tab};
            }
        }
        return res;
    }

    if (in.up) Move(m, 0);
    if (in.down) Move(m, 1);
    if (page == Page::States)
    {
        if (in.left) Move(m, 2);
        if (in.right) Move(m, 3);
    }

    switch (page)
    {
    case Page::Main:
        if (in.b) res.act = Act::Resume;
        else if (in.a)
        {
            const std::vector<MainItem> e = MainEntries(m);
            switch (e[std::clamp(focus, 0, (int)e.size() - 1)])
            {
            case MainItem::Resume: res.act = Act::Resume; break;
            case MainItem::Settings: Go(Page::Settings, 0); break;
            case MainItem::Controls: Go(Page::Controls, 0); break;
            case MainItem::States: Go(Page::States, 0); break;
            case MainItem::ChangeGame: res.act = Act::ChangeGame; break;
            case MainItem::OpenFolder: res.act = Act::OpenFolder; break;
            case MainItem::Quit: AskQuit(); break;
            }
        }
        break;
    case Page::Settings:
    {
        if (in.b) { Go(Page::Main, 1); break; }
        if (in.lb != in.rb)
        {
            // LB / RB: the tab before / after (wrapping), its first row focused.
            tab = (tab + kTabs + (in.rb ? 1 : -1)) % kTabs;
            focus = 0;
            break;
        }
        const std::vector<Row>& rows = TabRows(tab).rows;
        focus = std::clamp(focus, 0, (int)rows.size() - 1);
        const Row r = rows[focus];
        if (in.y) { ask = Ask::ResetTab; askFocus = 0; break; }
        if (r == Row::Keys)
        {
            if (in.a) { keysFrom = Page::Settings; Go(Page::Keys, 0); }
            break;
        }
        if ((in.left || in.right || in.a) && !RowHeld(r, *m.settings))
        {
            ChangeRow(r, in.left ? -1 : 1, *m.settings);
            res = {Act::Setting, (int)r};
        }
        break;
    }
    case Page::Controls:
        if (in.b) Go(Page::Main, 2);
        else if (in.a) { keysFrom = Page::Controls; Go(Page::Keys, 0); }
        break;
    case Page::Keys:
        if (in.b)
        {
            // Back to the door it came by: the Controls page, or Settings' Controls tab on its row.
            if (keysFrom == Page::Settings)
            {
                tab = (int)Tab::Controls;
                const std::vector<Row>& rows = TabRows(tab).rows;
                Go(Page::Settings, (int)(std::find(rows.begin(), rows.end(), Row::Keys) - rows.begin()));
            }
            else Go(Page::Controls, 0);
            break;
        }
        if (focus == Controls::COUNT)
        {
            if (in.a) res.act = Act::ResetBindings;
            break;
        }
        if (in.a) res = {Act::Capture, focus};
        else if (in.x) res = {Act::ClearBinding, focus};
        else if (in.y) { sequence = true; res = {Act::Capture, focus}; }
        break;
    case Page::States:
        if (in.b) { Go(Page::Main, 3); break; }
        if (!m.statesOk) break;
        if (focus == kStateSlots)
        {
            if (in.a && m.canUndo) res.act = Act::Undo;
            break;
        }
        if (in.a)
        {
            if (m.used[focus]) { ask = Ask::Load; askSlot = focus; askFocus = 0; }
            else res = {Act::SaveSlot, focus};
        }
        else if (in.x)
        {
            if (m.used[focus]) { ask = Ask::Overwrite; askSlot = focus; askFocus = 0; }
            else res = {Act::SaveSlot, focus};
        }
        break;
    case Page::Launch:
    {
        const std::vector<LaunchEntry> e = LaunchEntries(m);
        if (focus >= (int)e.size()) focus = 0;
        const LaunchEntry& it = e[focus];
        if (in.b) { focus = (int)e.size() - 1; break; } // B: onto Quit (never quits by itself)
        if (it.kind == LaunchItem::Language && (in.left || in.right || in.a))
        {
            ChangeRow(Row::Language, in.left ? -1 : 1, *m.settings);
            res = {Act::Setting, (int)Row::Language};
        }
        else if (in.a)
        {
            if (it.kind == LaunchItem::Continue) res.act = Act::Continue;
            else if (it.kind == LaunchItem::Choose) res.act = Act::ChooseFile;
            else if (it.kind == LaunchItem::Recent) res = {Act::OpenRecent, it.arg};
            else if (it.kind == LaunchItem::Quit) res.act = Act::Exit;
        }
        break;
    }
    }
    // The list's scroll follows the focus.
    if (page == Page::Keys)
    {
        if (focus < scroll) scroll = focus;
        if (focus >= scroll + ListRows(page)) scroll = focus - ListRows(page) + 1;
    }
    return res;
}

Result Shell::CaptureDone(bool cancelled)
{
    if (!sequence || cancelled || page != Page::Keys || focus + 1 >= Controls::COUNT)
    {
        sequence = false;
        return {};
    }
    focus++;
    if (focus >= scroll + ListRows(page)) scroll = focus - ListRows(page) + 1;
    return {Act::Capture, focus};
}

// ---- the pictures

std::vector<UiItem> Shell::Build(const Model& m) const
{
    Items it;
    std::vector<UiItem> images;
    const int lang = m.language;
    const Theme& th = GameTheme();
    rects_.assign(Count(m), Rect {0, 0, 0, 0});
    askRects_.clear();
    tabRects_.clear();
    const bool asking = ask != Ask::None || !m.message.empty() || m.capturing >= 0;

    if (page == Page::Launch)
    {
        // The whole canvas: the backdrop, the title, the list at the left, the card at the right.
        UiItem bg = PanelItem({0, 0, kW, kH});
        bg.plain = true;
        bg.fill = th.backdrop;
        bg.radius = 0;
        it.Add(0, bg);
        it.Add(0, Line({0, 28, kW, 64}, Wd(lang, W::LaunchTitle), 44, 30, Align::Centre, Weight::ExtraBold));
        panel_ = {0, 0, kW, kH};
        UiItem lp = PanelItem(kLaunchList);
        lp.radius = 22;
        it.Add(0, lp);
        UiItem cp = PanelItem(kLaunchCard);
        cp.radius = 22;
        it.Add(0, cp);
        it.Add(0, Line({kLaunchCard.x + 24, kLaunchCard.y + 14, kLaunchCard.w - 48, 44}, Wd(lang, W::ShellControls), 30, 22, Align::Centre, Weight::ExtraBold));
        AddCard(it, {kLaunchCard.x + 20, kLaunchCard.y + 70, kLaunchCard.w - 40, 420}, 1, m);
        UiItem drop = TextItem({kLaunchCard.x + 24, kLaunchCard.y + kLaunchCard.h - 76, kLaunchCard.w - 48, 60}, Wd(lang, W::LaunchDrop), 20, Align::Centre);
        drop.middle = true;
        drop.colour = kTextNote;
        it.Add(1, drop);

        const std::vector<LaunchEntry> e = LaunchEntries(m);
        int y = kLaunchList.y + 24;
        const int x = kLaunchList.x + 28, w = kLaunchList.w - 56;
        bool recentHeader = false;
        for (int i = 0; i < (int)e.size(); i++)
        {
            const bool f = !asking && focus == i;
            Rect r {x, y, w, 64};
            if (e[i].kind == LaunchItem::Continue) r.h = m.hasSuspend ? 150 : 96;
            if (e[i].kind == LaunchItem::Recent && !recentHeader)
            {
                recentHeader = true;
                UiItem h = Line({x + 8, y, w, 32}, Wd(lang, W::LaunchRecent), 22, 18);
                h.colour = kTextNote;
                it.Add(1, h);
                r.y = y += 34;
            }
            if (e[i].kind == LaunchItem::Recent) r.h = 48;
            if (e[i].kind == LaunchItem::Language || e[i].kind == LaunchItem::Quit) r.h = 56;
            if (f) Ring(it, 1, r, std::min(r.h / 2.0f, 24.0f));
            it.Add(1, Button(r, f));
            rects_[i] = r;
            switch (e[i].kind)
            {
            case LaunchItem::Continue:
            {
                int tx = r.x + 24;
                if (m.hasSuspend)
                {
                    AddThumb(it, images, m.suspend, {r.x + 14, r.y + 14, 216, 122});
                    tx = r.x + 14 + 216 + 20;
                }
                const int tw = r.x + r.w - 20 - tx;
                it.Add(1, Line({tx, r.y + 8, tw, 44}, Wd(lang, W::LaunchContinue), 34, 24, Align::Left, Weight::ExtraBold));
                UiItem name = Line({tx, r.y + 50, tw, 34}, m.recents.empty() ? "" : m.recents[0], 22, 16);
                name.colour = f ? kWarm : kTextNote;
                it.Add(1, name);
                if (m.hasSuspend)
                {
                    UiItem when = TextItem({tx, r.y + 86, tw, 56}, Fill(Wd(lang, W::LaunchWhere), FormatTime(m.suspend.time)), 19);
                    when.colour = kTextNote;
                    it.Add(1, when);
                }
                break;
            }
            case LaunchItem::Choose:
                it.Add(1, Line({r.x + 24, r.y, r.w - 48, r.h}, Wd(lang, W::LaunchChoose), 28, 20, Align::Left, Weight::ExtraBold));
                break;
            case LaunchItem::Recent:
                it.Add(1, Line({r.x + 24, r.y, r.w - 48, r.h}, m.recents[e[i].arg], 24, 16));
                break;
            case LaunchItem::Language:
            {
                const Rect pill {r.x + r.w - 24 - 240, r.y + 8, 240, r.h - 16};
                it.Add(1, Line({r.x + 24, r.y, r.w - 48 - 260, r.h}, Wd(lang, W::SetLanguage), 26, 18, Align::Left, Weight::ExtraBold));
                UiItem p = PanelItem(pill);
                p.plain = true;
                p.fill = th.frameDark;
                p.radius = pill.h / 2.0f;
                it.Add(1, p);
                it.Add(1, Line({pill.x + 12, pill.y, pill.w - 24, pill.h}, RowValue(Row::Language, *m.settings, lang), 24, 16, Align::Centre));
                break;
            }
            case LaunchItem::Quit:
                it.Add(1, Line({r.x + 24, r.y, r.w - 48, r.h}, Wd(lang, W::ShellQuit), 26, 18, Align::Left, Weight::ExtraBold));
                break;
            }
            y += r.h + 10;
        }
        // The hints under the list.
        if (!asking)
        {
            int right = kLaunchList.x + kLaunchList.w - 24;
            const int hy = kLaunchList.y + kLaunchList.h - 56;
            const LaunchItem k = e[std::clamp(focus, 0, (int)e.size() - 1)].kind;
            const W aw = k == LaunchItem::Continue ? W::LaunchContinue : k == LaunchItem::Language ? W::ShellChange
                       : k == LaunchItem::Quit ? W::ShellQuit : k == LaunchItem::Choose ? W::ShellOpen : W::LaunchStart;
            right = Hint(it, right, hy, ui::Button::A, Wd(lang, aw));
            Hint(it, right, hy, ui::Button::DPadV, "");
        }
    }
    else
    {
        // Over the game: the veil, the panel, the page's title.
        UiItem veil = PanelItem({0, 0, kW, kH});
        veil.plain = true;
        veil.fill = kVeil;
        veil.radius = 0;
        it.Add(0, veil);
        UiItem p = PanelItem(kPanel);
        p.radius = 24;
        it.Add(0, p);
        panel_ = kPanel;
        static const W titles[] = {W::ShellPaused, W::ShellSettings, W::ShellControls, W::KeysKeyboard, W::ShellStates};
        const std::string title = page == Page::Keys ? Wd(lang, W::CtlKeys) : Wd(lang, titles[(int)page]);
        if (page != Page::Settings) it.Add(0, Line({kPanel.x + 40, kPanel.y + 16, kPanel.w - 80, 56}, title, 36, 24, Align::Centre, Weight::ExtraBold));
        UiItem well = WellItem(kContent);
        well.radius = 16;
        it.Add(0, well);

        const Rect c {kContent.x + 16, kContent.y + 16, kContent.w - 32, kContent.h - 32};
        // At the left of the hints, always: the pause buttons resume (Start + Back, Esc).
        {
            const Rect r = ChipRect(0, 0, ui::Button::Pause, Wd(lang, W::ShellResume), kHint, 0, Weight::ExtraBold);
            it.Add(1, ChipItem(kPanel.x + 32, kHintsY + (kHintsH - r.h) / 2, ui::Button::Pause, Wd(lang, W::ShellResume), kHint, 0, Weight::ExtraBold));
        }
        int right = kPanel.x + kPanel.w - 32;

        switch (page)
        {
        case Page::Main:
        {
            static const W words[] = {W::ShellResume, W::ShellSettings, W::ShellControls, W::ShellStates, W::ShellChangeGame, W::ShellOpenFolder, W::ShellQuit};
            const std::vector<MainItem> e = MainEntries(m);
            // The banner (the save written elsewhere) over the buttons, warm, kept while it lasts.
            const int bannerH = m.banner.empty() ? 0 : 48;
            if (bannerH)
            {
                UiItem b = TextItem({c.x + 8, c.y - 6, c.w - 16, bannerH + 4}, m.banner, 18, Align::Centre);
                b.middle = true;
                b.colour = kTextNote;
                it.Add(1, b);
            }
            const int n = (int)e.size(), step = bannerH ? 58 : 64, top = c.y + bannerH + (c.h - bannerH - (n * step - 8)) / 2;
            for (int i = 0; i < n; i++)
            {
                const Rect r {c.x + 120, top + i * step, c.w - 240, step - 8};
                const bool f = !asking && focus == i;
                if (f) Ring(it, 1, r, 24);
                it.Add(1, Button(r, f));
                it.Add(1, Line({r.x + 24, r.y, r.w - 48, r.h}, Wd(lang, words[(int)e[i]]), 30, 20, Align::Centre, Weight::ExtraBold));
                rects_[i] = r;
            }
            right = Hint(it, right, kHintsY, ui::Button::A, Wd(lang, W::Select));
            Hint(it, right, kHintsY, ui::Button::B, Wd(lang, W::ShellResume));
            break;
        }
        case Page::Settings:
        {
            // The tab bar in the title band: LB, the five tabs (the shown one on a button), RB.
            {
                const Rect lb = ChipRect(0, 0, ui::Button::LB, "", 24, 0, Weight::ExtraBold);
                const Rect rb = ChipRect(0, 0, ui::Button::RB, "", 24, 0, Weight::ExtraBold);
                const int cy = kTabBarY + kTabBarH / 2;
                it.Add(0, ChipItem(kPanel.x + 40, cy - lb.h / 2, ui::Button::LB, "", 24, 0, Weight::ExtraBold));
                it.Add(0, ChipItem(kPanel.x + kPanel.w - 40 - rb.w, cy - rb.h / 2, ui::Button::RB, "", 24, 0, Weight::ExtraBold));
                const int x0 = kPanel.x + 40 + lb.w + 16, x1 = kPanel.x + kPanel.w - 40 - rb.w - 16;
                constexpr int pad = 18, gap = 6;
                float size = 26;
                auto total = [&](float sz) {
                    float w = 0;
                    for (int t = 0; t < kTabs; t++) w += TextWidth(TabName(t, lang), sz, Weight::ExtraBold) + 2 * pad;
                    return w + gap * (kTabs - 1);
                };
                while (size > 16 && total(size) > x1 - x0) size -= 1;
                float x = x0 + std::max(0.0f, (x1 - x0 - total(size)) / 2);
                for (int t = 0; t < kTabs; t++)
                {
                    const std::string name = TabName(t, lang);
                    const int w = (int)std::ceil(TextWidth(name, size, Weight::ExtraBold)) + 2 * pad;
                    const Rect r {(int)x, kTabBarY, w, kTabBarH};
                    if (t == tab) it.Add(0, Button(r, true));
                    it.Add(0, Line(r, name, size, size, Align::Centre, Weight::ExtraBold));
                    tabRects_.push_back(r);
                    x += w + gap;
                }
            }
            // The tab's rows, the Advanced heading above its group; no scrolling (every tab fits).
            const TabLayout& tl = TabRows(tab);
            const Settings& st = *m.settings;
            int y = c.y;
            for (int i = 0; i < (int)tl.rows.size(); i++)
            {
                if (i == tl.advanced)
                {
                    UiItem h = Line({c.x + 12, y, c.w - 24, kSetHeadH}, Wd(lang, W::SetAdvanced), 20, 16);
                    h.colour = kTextNote;
                    it.Add(1, h);
                    y += kSetHeadH;
                }
                const Row row = tl.rows[i];
                const Rect r {c.x, y, c.w, kSetRowH};
                y += kSetRowStep;
                const bool f = !asking && focus == i, held = RowHeld(row, st);
                if (f) Ring(it, 1, r, 16);
                UiItem b = Button(r, f, !held);
                b.radius = 16;
                it.Add(1, b);
                rects_[i] = r;
                const Rect pill {r.x + r.w - 14 - 250, r.y + 6, 250, r.h - 12};
                const int tagW = row == Row::Language ? 200 : 0;
                UiItem label = Line({r.x + 20, r.y, pill.x - 16 - tagW - (r.x + 20), r.h}, RowLabel(row, lang), 24, 17);
                if (held) label.colour = kTextInactive;
                it.Add(1, label);
                if (row == Row::Keys)
                {
                    // Opens a page: a "›" at the right, no pill.
                    it.Add(1, Line({r.x + r.w - 60, r.y, 40, r.h}, "\xE2\x80\xBA", 40, 40, Align::Right, Weight::ExtraBold));
                    continue;
                }
                if (tagW)
                {
                    // Language applies at the game's next start: said on the row.
                    UiItem tag = Line({pill.x - 12 - tagW, r.y, tagW, r.h}, Wd(lang, W::NextStart), 17, 13, Align::Right);
                    tag.colour = f ? kWarm : kTextNote;
                    it.Add(1, tag);
                }
                UiItem pl = PanelItem(pill);
                pl.plain = true;
                pl.fill = th.frameDark;
                pl.radius = pill.h / 2.0f;
                it.Add(1, pl);
                if (!held)
                {
                    // ‹ › : Left / Right change it.
                    it.Add(1, Line({pill.x + 6, pill.y, 26, pill.h}, "\xE2\x80\xB9", 34, 34, Align::Centre, Weight::ExtraBold));
                    it.Add(1, Line({pill.x + pill.w - 32, pill.y, 26, pill.h}, "\xE2\x80\xBA", 34, 34, Align::Centre, Weight::ExtraBold));
                }
                UiItem v = Line({pill.x + 34, pill.y, pill.w - 68, pill.h}, RowValue(row, st, lang), 22, 15, Align::Centre);
                if (held) v.colour = kTextInactive;
                it.Add(1, v);
            }
            // The focused row's note under the list (Battery saver's hold on a row it holds).
            if (focus >= 0 && focus < (int)tl.rows.size())
            {
                const Row row = tl.rows[focus];
                UiItem note = TextItem({c.x + 8, c.y + kSetListH, c.w - 16, c.h - kSetListH}, Wd(lang, RowHeld(row, st) ? W::SaverHolds : (W)RowNote(row)), 18);
                note.middle = true;
                note.colour = kTextNote;
                it.Add(1, note);
            }
            const bool keysRow = focus >= 0 && focus < (int)tl.rows.size() && tl.rows[focus] == Row::Keys;
            right = keysRow ? Hint(it, right, kHintsY, ui::Button::A, Wd(lang, W::ShellOpen)) : Hint(it, right, kHintsY, ui::Button::DPadH, Wd(lang, W::ShellChange));
            right = Hint(it, right, kHintsY, ui::Button::Y, Wd(lang, W::ResetTab));
            Hint(it, right, kHintsY, ui::Button::B, Wd(lang, W::ShellBack));
            break;
        }
        case Page::Keys:
        {
            const int n = Count(m), kListRows = ListRows(page), top = c.y + ListTop(page);
            for (int i = scroll; i < n && i < scroll + kListRows; i++)
            {
                const Rect r {c.x, top + (i - scroll) * kRowStep, c.w - 20, kRowH};
                const bool f = !asking && focus == i;
                if (f) Ring(it, 1, r, 16);
                UiItem b = Button(r, f);
                b.radius = 16;
                it.Add(1, b);
                rects_[i] = r;
                if (i == Controls::COUNT)
                    it.Add(1, Line({r.x + 20, r.y, r.w - 40, r.h}, Wd(lang, W::KeysReset), 26, 18, Align::Left, Weight::ExtraBold));
                else
                {
                    it.Add(1, Line({r.x + 20, r.y, 300, r.h}, ActionLabel(i, lang), 24, 16));
                    for (int col = 0; col < 2; col++)
                    {
                        const std::string& g = col == 0 ? m.keyGlyph[i] : m.padGlyph[i];
                        const int x = r.x + 360 + col * 190;
                        if (g.empty())
                        {
                            UiItem none = Line({x, r.y, 160, r.h}, "\xE2\x80\x94", 24, 24); // "—"
                            none.colour = kTextInactive;
                            it.Add(1, none);
                            continue;
                        }
                        UiItem chip = ChipItemAs(x, 0, col == 0 ? Face::Keyboard : m.padFace, g, "", 24, Weight::ExtraBold);
                        chip.to.y = r.y + (r.h - chip.to.h) / 2;
                        if (chip.to.w > 170) chip.to.w = 170;
                        it.Add(1, chip);
                    }
                }
            }
            // A thin scroll bar at the right.
            if (n > kListRows)
            {
                const int trackH = kListRows * kRowStep - (kRowStep - kRowH);
                UiItem track = PanelItem({c.x + c.w - 8, top, 8, trackH});
                track.plain = true;
                track.fill = th.frameDark;
                track.radius = 4;
                it.Add(1, track);
                const int h = std::max(24, trackH * kListRows / n), y = top + (trackH - h) * scroll / std::max(1, n - kListRows);
                UiItem thumb = PanelItem({c.x + c.w - 8, y, 8, h});
                thumb.plain = true;
                thumb.fill = th.text;
                thumb.radius = 4;
                it.Add(1, thumb);
            }
            {
                // The columns' heads, above the list.
                UiItem kh = Line({c.x + 360, c.y, 180, 34}, Wd(lang, W::KeysKeyboard), 20, 16);
                UiItem ph = Line({c.x + 550, c.y, 180, 34}, Wd(lang, W::KeysController), 20, 16);
                kh.colour = ph.colour = kTextNote;
                it.Add(1, kh);
                it.Add(1, ph);
                if (focus < Controls::COUNT)
                {
                    right = Hint(it, right, kHintsY, ui::Button::A, Wd(lang, W::ShellChange));
                    right = Hint(it, right, kHintsY, ui::Button::X, Wd(lang, W::KeysClear));
                    right = Hint(it, right, kHintsY, ui::Button::Y, Wd(lang, W::KeysAll));
                }
                else right = Hint(it, right, kHintsY, ui::Button::A, Wd(lang, W::KeysReset));
                Hint(it, right, kHintsY, ui::Button::B, Wd(lang, W::ShellBack));
            }
            break;
        }
        case Page::Controls:
        {
            AddCard(it, {c.x + 8, c.y + 4, c.w - 16, 5 * 56}, 2, m);
            const Rect r {c.x + 160, c.y + c.h - 76, c.w - 320, 64};
            {
                // What Keys and buttons can do (one button alone for the pause menu, too).
                UiItem kn = TextItem({c.x + 8, r.y - 60, c.w - 16, 50}, Wd(lang, W::KeysNote), 18, Align::Centre);
                kn.middle = true;
                kn.colour = kTextNote;
                it.Add(1, kn);
            }
            const bool f = !asking && focus == 0;
            if (f) Ring(it, 1, r, 24);
            it.Add(1, Button(r, f));
            it.Add(1, Line({r.x + 24, r.y, r.w - 48, r.h}, Wd(lang, W::CtlKeys), 28, 20, Align::Centre, Weight::ExtraBold));
            rects_[0] = r;
            right = Hint(it, right, kHintsY, ui::Button::A, Wd(lang, W::ShellOpen));
            Hint(it, right, kHintsY, ui::Button::B, Wd(lang, W::ShellBack));
            break;
        }
        case Page::States:
        {
            if (!m.statesOk)
            {
                UiItem t = Line({c.x, c.y, c.w, c.h}, Wd(lang, W::StNoFolder), 28, 20, Align::Centre);
                t.colour = kTextNote;
                it.Add(1, t);
                Hint(it, right, kHintsY, ui::Button::B, Wd(lang, W::ShellBack));
                break;
            }
            for (int i = 0; i < kStateSlots; i++)
            {
                const Rect r {c.x + (c.w - 696) / 2 + (i % 2) * 360, c.y + (i / 2) * 206, 336, 196};
                const bool f = !asking && focus == i;
                if (f) Ring(it, 1, r, 18);
                UiItem b = Button(r, f);
                b.radius = 18;
                it.Add(1, b);
                rects_[i] = r;
                const Rect pic {r.x + 32, r.y + 12, 272, 136};
                if (m.used[i]) AddThumb(it, images, m.slots[i], {pic.x + (pic.w - 242) / 2, pic.y, 242, 136});
                else
                {
                    UiItem w = WellItem(pic);
                    w.radius = 8;
                    it.Add(1, w);
                    UiItem e = Line(pic, Wd(lang, W::StEmpty), 24, 18, Align::Centre);
                    e.colour = kTextInactive;
                    it.Add(1, e);
                }
                std::string label = Fill(Wd(lang, W::SaveSlot), std::to_string(i + 1));
                if (m.used[i] && m.slots[i].time) label += "  \xC2\xB7  " + FormatTime(m.slots[i].time);
                it.Add(1, Line({r.x + 12, r.y + 152, r.w - 24, 36}, label, 22, 15, Align::Centre));
            }
            {
                const Rect r {c.x + 170, c.y + 2 * 206 + 2, c.w - 340, 50};
                const bool f = !asking && focus == kStateSlots;
                if (f) Ring(it, 1, r, 22);
                it.Add(1, Button(r, f, m.canUndo));
                UiItem t = Line({r.x + 20, r.y, r.w - 40, r.h}, Wd(lang, W::StUndo), 24, 18, Align::Centre, Weight::ExtraBold);
                if (!m.canUndo) t.colour = kTextInactive;
                it.Add(1, t);
                rects_[kStateSlots] = r;
            }
            if (focus < kStateSlots)
            {
                right = Hint(it, right, kHintsY, ui::Button::A, Wd(lang, m.used[focus] ? W::StLoad : W::StSave));
                if (m.used[focus]) right = Hint(it, right, kHintsY, ui::Button::X, Wd(lang, W::StSave));
            }
            else if (m.canUndo) right = Hint(it, right, kHintsY, ui::Button::A, Wd(lang, W::StUndo));
            Hint(it, right, kHintsY, ui::Button::B, Wd(lang, W::ShellBack));
            break;
        }
        case Page::Launch: break;
        }
    }

    // A question, a message or a capture over it all: its own veil and box.
    if (asking)
    {
        UiItem veil = PanelItem({0, 0, kW, kH});
        veil.plain = true;
        veil.fill = 0x90101218;
        veil.radius = 0;
        it.Add(2, veil);
        UiItem box = PanelItem(kAskBox);
        box.radius = 24;
        it.Add(2, box);
        const Rect textBox {kAskBox.x + 32, kAskBox.y + 28, kAskBox.w - 64, 160};
        UiItem well = WellItem(textBox);
        well.radius = 14;
        it.Add(2, well);
        std::string text, note;
        if (m.capturing >= 0)
        {
            text = Fill(Wd(lang, W::KeysPress), ActionLabel(m.capturing, lang));
            note = std::string(Wd(lang, W::KeysCancel)) + "  (" + std::to_string(m.captureLeft) + ")";
        }
        else if (!m.message.empty()) text = m.message;
        else if (ask == Ask::Quit) { text = Wd(lang, W::QuitAsk); if (m.statesOk) note = Wd(lang, W::QuitNote); } // (no folder: nothing is kept)
        else if (ask == Ask::Load)
        {
            text = Fill(Wd(lang, W::StLoadAsk), std::to_string(askSlot + 1));
            if (askSlot >= 0 && askSlot < kStateSlots && !m.otherRom[askSlot].empty()) note = Fill(Wd(lang, W::StOtherRom), m.otherRom[askSlot]);
        }
        else if (ask == Ask::Overwrite) text = Fill(Wd(lang, W::StOverwriteAsk), std::to_string(askSlot + 1));
        else if (ask == Ask::ResetTab) text = Fill(Wd(lang, W::ResetTabAsk), TabName(tab, lang));
        // The words, as large as the well holds them (30 down to 20).
        float size = 30;
        while (size > 20 && (float)Wrap(text, size, (float)textBox.w - 32).size() * size * 1.3f + (note.empty() ? 0 : 60) > textBox.h) size -= 1;
        UiItem t = TextItem({textBox.x + 16, textBox.y + 10, textBox.w - 32, textBox.h - 20 - (note.empty() ? 0 : 56)}, text, size, Align::Centre);
        t.middle = true;
        it.Add(3, t);
        if (!note.empty())
        {
            UiItem n = TextItem({textBox.x + 16, textBox.y + textBox.h - 62, textBox.w - 32, 54}, note, 19, Align::Centre);
            n.middle = true;
            n.colour = kTextNote;
            it.Add(3, n);
        }
        const int by = kAskBox.y + kAskBox.h - 96;
        if (m.capturing < 0 && m.message.empty())
        {
            // No (focused first) and Yes; B is always No.
            const W words[2] = {W::ShellNo, W::ShellYes};
            for (int i = 0; i < 2; i++)
            {
                const Rect r {kAskBox.x + 110 + i * 270, by, 230, 64};
                const bool f = askFocus == i;
                if (f) Ring(it, 3, r, 24);
                it.Add(3, Button(r, f));
                const std::string w = Wd(lang, words[i]);
                const ui::Button chip = f ? ui::Button::A : ui::Button::B;
                if (f || i == 0)
                {
                    const Rect cr = ChipRect(0, 0, chip, w, 28, 0, Weight::ExtraBold);
                    it.Add(3, ChipItem(r.x + (r.w - cr.w) / 2, r.y + (r.h - cr.h) / 2, chip, w, 28, 0, Weight::ExtraBold));
                }
                else it.Add(3, Line({r.x + 12, r.y, r.w - 24, r.h}, w, 28, 20, Align::Centre, Weight::ExtraBold));
                askRects_.push_back(r);
            }
        }
        else if (m.capturing < 0)
        {
            const Rect r {kAskBox.x + (kAskBox.w - 230) / 2, by, 230, 64};
            Ring(it, 3, r, 24);
            it.Add(3, Button(r, true));
            const std::string w = Wd(lang, W::ShellOk);
            const Rect cr = ChipRect(0, 0, ui::Button::A, w, 28, 0, Weight::ExtraBold);
            it.Add(3, ChipItem(r.x + (r.w - cr.w) / 2, r.y + (r.h - cr.h) / 2, ui::Button::A, w, 28, 0, Weight::ExtraBold));
            askRects_.push_back(r);
        }
        // The pictures under the box would lie over it (the kit draws images above everything).
        images.clear();
    }
    std::vector<UiItem> out = it.All();
    out.insert(out.end(), images.begin(), images.end());
    Zoom(out);
    return out;
}

void Shell::Zoom(std::vector<UiItem>& items) const
{
    // The UI's size (ui::UiScale): everything but what covers the canvas's width (the backdrop, the
    // veils) about the canvas's centre, no larger than the canvas holds; the mouse's rectangles
    // with it.
    const float z = ui::UiScale();
    if (z == 1) return;
    auto fixed = [](const UiItem& u) { return u.to.x <= 0 && u.to.x + u.to.w >= kW; };
    int x0 = kW, y0 = kH, x1 = 0, y1 = 0;
    for (const UiItem& u : items)
        if (!fixed(u) && u.to.w > 0 && u.to.h > 0)
        {
            x0 = std::min(x0, u.to.x);
            y0 = std::min(y0, u.to.y);
            x1 = std::max(x1, u.to.x + u.to.w);
            y1 = std::max(y1, u.to.y + u.to.h);
        }
    if (x1 <= x0) return;
    constexpr float ax = kW / 2.0f, ay = kH / 2.0f;
    float f = ui::FitZoom({x0, y0, x1 - x0, y1 - y0}, z, ax, ay, page == Page::Launch ? kSafe : Rect {8, 8, kW - 16, kH - 16});
    if (z > 1) f = std::max(f, 1.0f);
    if (f == 1) return;
    for (UiItem& u : items)
        if (!fixed(u)) ui::ZoomItem(u, f, ax, ay);
    auto zoom = [&](Rect& r) {
        if (r.w <= 0 || r.h <= 0 || (r.x <= 0 && r.x + r.w >= kW)) return;
        const int a = (int)std::lround(ax + f * (r.x - ax)), b = (int)std::lround(ay + f * (r.y - ay));
        const int c = (int)std::lround(ax + f * (r.x + r.w - ax)), d = (int)std::lround(ay + f * (r.y + r.h - ay));
        r = {a, b, c - a, d - b};
    };
    for (Rect& r : rects_) zoom(r);
    for (Rect& r : askRects_) zoom(r);
    for (Rect& r : tabRects_) zoom(r);
    zoom(panel_);
}
}
