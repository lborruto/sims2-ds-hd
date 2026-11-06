#include "console.h"

#include <algorithm>
#include <cmath>

#include "activities.h"
#include "hotspots.h"
#include "port/dialogue.h"
#include "port/gift.h"
#include "port/panelmenu.h"
#include "romdata.h"

namespace sims2wide
{
namespace
{
constexpr Rect kCanvas {0, 0, kConsoleW, kConsoleH};

Overlay Frame(Rect r, int before) { return {r, kFrameArgb, 16, 6, kOutlineArgb, before}; }
// A frame hugging a piece: its 6-pixel border just outside it.
Overlay FrameAround(Rect r, int before) { return {{r.x - 6, r.y - 6, r.w + 12, r.h + 12}, kFrameArgb, 14, 6, kOutlineArgb, before}; }

// The world (the top picture's first `rows` rows) 720 high: centred when narrower than the
// canvas (the DS's own 4:3), its sides cropped to 16:9 when wider (a conversation's rows of the
// widescreen world). A picture within 8 pixels of the canvas's width at that height fills it,
// stretched by under 1 % (the widescreen world: 341 x 192 is 1279 wide at 720). keepSides (the
// world cropped for the HD status bar): never cut its sides to make room for the crop; wider
// than the canvas, it is fitted to the canvas's width instead, top-aligned (the widescreen town
// without its bar: 1280 x 661), the bar in the strip freed below.
Piece World(int topW, int rows, bool keepSides = false)
{
    const int w = (int)std::lround(720.0 * topW / rows);
    if (w < kConsoleW - 8) return {Src::Top, {0, 0, topW, rows}, {(kConsoleW - w) / 2, 0, w, kConsoleH}};
    if (keepSides && w > kConsoleW + 8) return {Src::Top, {0, 0, topW, rows}, {0, 0, kConsoleW, (int)std::lround((double)kConsoleW * rows / topW)}};
    const int fw = w <= kConsoleW + 8 ? topW : std::min(topW, (int)std::lround(rows * 16.0 / 9.0));
    return {Src::Top, {(topW - fw) / 2, 0, fw, rows}, kCanvas};
}

Piece Bottom(Rect from, Rect to) { return {Src::Bottom, from, to}; }

// Text in white with the outline's shadow (each font pixel `scale` canvas pixels).
Overlay Text(int x, int y, const char* text, int scale)
{
    Overlay o {TextRect(x, y, text, scale), 0xFFFFFFFF, 0, 0, kOutlineArgb};
    o.text = text;
    o.textScale = scale;
    return o;
}

// The focus ring around hotspot `focus`'s area, as the topmost touchable bottom piece showing the
// area's centre draws it (the area clipped to that piece), 4 pixels outside it.
void AddRing(Composition& c, ScreenState s, int focus)
{
    const std::vector<Hotspot>& h = HotspotsFor(s);
    if (focus < 0 || focus >= (int)h.size()) return;
    const Rect a = h[focus].area;
    const int cx2 = 2 * a.x + a.w, cy2 = 2 * a.y + a.h; // the centre, doubled
    for (int i = (int)c.pieces.size() - 1; i >= 0; i--)
    {
        const Piece& q = c.pieces[i];
        if (q.src != Src::Bottom || !q.touch) continue;
        const Rect& f = q.from;
        if (cx2 < 2 * f.x || cy2 < 2 * f.y || cx2 >= 2 * (f.x + f.w) || cy2 >= 2 * (f.y + f.h)) continue;
        const int x0 = std::max(a.x, f.x), y0 = std::max(a.y, f.y);
        const int x1 = std::min(a.x + a.w, f.x + f.w), y1 = std::min(a.y + a.h, f.y + f.h);
        const int rx0 = q.to.x + (x0 - f.x) * q.to.w / f.w, ry0 = q.to.y + (y0 - f.y) * q.to.h / f.h;
        const int rx1 = q.to.x + (x1 - f.x) * q.to.w / f.w, ry1 = q.to.y + (y1 - f.y) * q.to.h / f.h;
        constexpr int t = 4;
        c.overlays.push_back({{rx0 - t, ry0 - t, rx1 - rx0 + 2 * t, ry1 - ry0 + 2 * t}, 0, 8, t, kRingArgb, -1});
        return;
    }
}}

Piece ConsoleWorld(int topW, int rows, bool keepSides) { return World(topW, rows, keepSides); }

void AddHints(Composition& c, const std::vector<Hint>& hints)
{
    if (hints.empty()) return;
    const size_t box = c.overlays.size();
    c.overlays.push_back({{kHintBox.x, kHintBox.y, kHintBox.w, 0}, kFrameArgb, 16, 4, kOutlineArgb});
    int y = kHintBox.y + kHintPad;
    for (const Hint& h : hints)
    {
        int x = kHintBox.x + kHintPad, rowH = TextRect(0, 0, h.text, 3).h - 3;
        if (h.button)
        {
            const bool round = !h.button[1];
            const int cw = round ? kHintChipH : TextRect(0, 0, h.button, 2).w + 14;
            c.overlays.push_back({{x, y, cw, kHintChipH}, kPanelArgb, round ? kHintChipH / 2 : 9, 3, kOutlineArgb});
            if (round) c.overlays.push_back(Text(x + (kHintChipH - 15) / 2, y + (kHintChipH - 21) / 2, h.button, 3));
            else c.overlays.push_back(Text(x + 8, y + (kHintChipH - 14) / 2, h.button, 2));
            x = kHintTextX;
            rowH += kHintChipH - 21;
        }
        c.overlays.push_back(Text(x, y + (h.button ? (kHintChipH - 21) / 2 : 0), h.text, 3));
        y += std::max(rowH, h.button ? kHintChipH : 0) + kHintGap;
    }
    c.overlays[box].to.h = std::min(y - kHintGap + kHintPad - kHintBox.y, kHintBox.h);
}

Composition BuildConsole(ScreenState s, bool menuOpen, int focus, int topW, int topH, const HudState* hud, const HudStrings* strings,
                         const GameState* game, const RomData* rom, const u32* bottom, IconCache* icons)
{
    Composition c {kConsoleW, kConsoleH, {}, {}};
    // The HD status bar replaces the game's: the world without its last 16 of 192 rows.
    const bool bar = hud && strings && strings->ok;
    const int worldRows = bar ? HudCropRows(topH) : topH;
    // A black load (both screens black, RAM between screens): the bare backdrop.
    if (s.screen == Screen::Loading) return c;
    const bool cursorScreen = s.screen == Screen::SaveSlots || s.screen == Screen::ItemHand || s.screen == Screen::Unknown;
    if (s.screen == Screen::None || s.screen == Screen::FrontEnd || s.screen == Screen::CreateASim)
    {
        // No HUD (title, main menu, Create-a-Sim, an unmapped front-end screen; None from the pixels
        // when RAM does not read): both screens matter alike. The top
        // screen fitted in its box at its own shape, centred; the touch screen 2.5x; both framed.
        const double k = std::min((double)kFrontTopBox.w / topW, (double)kFrontTopBox.h / topH);
        const int w = std::min(kFrontTopBox.w, (int)std::lround(topW * k)), h = std::min(kFrontTopBox.h, (int)std::lround(topH * k));
        const Rect topTo {kFrontTopBox.x + (kFrontTopBox.w - w) / 2, kFrontTopBox.y + (kFrontTopBox.h - h) / 2, w, h};
        c.overlays.push_back(FrameAround(topTo, 0));
        c.overlays.push_back(FrameAround(kFrontBottomTo, 0));
        c.pieces.push_back({Src::Top, {0, 0, topW, topH}, topTo});
        c.pieces.push_back(Bottom({0, 0, SW, SH}, kFrontBottomTo));
        // The hint's box: the text with its shadow (TextRect) and, on each side, the padding and
        // the box's 4-pixel border, centred.
        constexpr int border = 4;
        const Rect t = TextRect(0, 0, kFrontHint, 3);
        const int inset = border + kFrontHintPad, bw = t.w + 2 * inset, bh = t.h + 2 * inset, bx = (kConsoleW - bw) / 2;
        c.overlays.push_back({{bx, kFrontHintY, bw, bh}, kFrameArgb, 16, border, kOutlineArgb});
        c.overlays.push_back(Text(bx + inset, kFrontHintY + inset, kFrontHint, 3));
    }
    else if (MenuView v; s.screen == Screen::Panel && menuOpen && (v = PanelMenuView(s, game, rom)).valid)
    {
        // The panel's menu redrawn in HD (port/panelmenu.h) over the dimmed world.
        Piece world = World(topW, worldRows, bar);
        world.touch = false;
        c.pieces.push_back(world);
        c.overlays.push_back({kCanvas, kDimArgb, 0, 0, 0, 1});
        BuildPanelMenu(c, s, v, focus, *game, *rom, icons, bottom);
        if (bar) AddStatusBar(c, *hud, *strings);
        // The UI in layer order (the bar's layer after the menu's), the images last.
        std::stable_sort(c.ui.begin(), c.ui.end(), [](const ui::UiItem& a, const ui::UiItem& b) {
            return (a.kind == ui::UiItem::Image ? 1 << 20 : a.layer) < (b.kind == ui::UiItem::Image ? 1 << 20 : b.layer);
        });
        return c; // (the focus is the view's: no ring from HotspotsFor)
    }
    else if (MenuView v; s.screen == Screen::Gift && (v = GiftMenuView(s, game, rom)).valid)
    {
        // Give Gift redrawn in HD (port/gift.h) over the dimmed world.
        Piece world = World(topW, worldRows, bar);
        world.touch = false;
        c.pieces.push_back(world);
        c.overlays.push_back({kCanvas, kDimArgb, 0, 0, 0, 1});
        BuildGiftMenu(c, v, focus, *game, *rom, icons, bottom);
        if (bar) AddStatusBar(c, *hud, *strings);
        std::stable_sort(c.ui.begin(), c.ui.end(), [](const ui::UiItem& a, const ui::UiItem& b) {
            return (a.kind == ui::UiItem::Image ? 1 << 20 : a.layer) < (b.kind == ui::UiItem::Image ? 1 << 20 : b.layer);
        });
        return c;
    }
    else if (menuOpen || cursorScreen || s.screen == Screen::Phone || s.screen == Screen::Popup || s.screen == Screen::Gift)
    {
        // The world dimmed behind the touch screen: not playable, the mouse does nothing there.
        // The stick cursor's screens (no table: SaveSlots, ItemHand, Unknown) get a frame.
        Piece world = World(topW, worldRows, bar);
        world.touch = false;
        c.pieces.push_back(world);
        c.overlays.push_back({kCanvas, kDimArgb, 0, 0, 0, 1});
        if (cursorScreen) c.overlays.push_back(FrameAround(kMenuTo, 1));
        c.pieces.push_back(Bottom({0, 0, SW, SH}, kMenuTo));
        // Not dimmed: the game keeps its status bar up while the touch screen is used.
        if (bar) AddStatusBar(c, *hud, *strings);
    }
    else if (s.screen == Screen::Panel)
    {
        c.pieces.push_back(World(topW, worldRows, bar));
        c.overlays.push_back(Frame(kRailFrame, 1));
        c.pieces.push_back(Bottom(kRailPhoneFrom, kRailPhoneTo));
        c.pieces.push_back(Bottom(kRailTabsFrom, kRailTabsTo));
        if (bar) AddStatusBar(c, *hud, *strings);
    }
    else if (s.screen == Screen::Dialogue && game && game->valid && rom && rom->StringCount() > 0 && bottom)
    {
        // The HD conversation (port/dialogue.h): the game's words, its speaker and portrait (the
        // panel alone while no page is drawn yet: the frames between two speeches).
        BuildDialogue(c, *game, *rom, bottom, topW, topH);
    }
    else if (s.screen == Screen::Dialogue)
    {
        // Without the status bar (its last 16 of 192 rows).
        c.pieces.push_back(World(topW, HudCropRows(topH)));
        c.overlays.push_back(Frame(kDialogueFrame, 1));
        c.overlays.push_back({kDialogueTextBox, kPanelArgb, 8, 2, kOutlineArgb, 1});
        c.pieces.push_back(Bottom(kDialogueTextFrom, kDialogueTextTo));
        c.pieces.push_back(Bottom(kDialogueNameFrom, kDialogueNameTo));
        c.pieces.push_back(Bottom(kDialoguePortraitFrom, kDialoguePortraitTo));
        c.pieces.push_back(Bottom(kDialogueArrowFrom, kDialogueArrowTo));
    }
    else
    {
        // Social, Vacuum, Cards, Catalogue, Build; the hints' box for the activities. All but the
        // cards keep the HUD: their small world without it, the HD bar under it.
        const bool hudHere = bar && s.screen != Screen::Cards;
        const int rows = hudHere ? worldRows : topH;
        const int h = (int)std::lround((double)kActivityTopW * rows / topW);
        const Rect topTo {kActivityTopX, kActivityTopY, kActivityTopW, h};
        c.overlays.push_back({kCanvas, kPanelArgb, 0, 0, 0, 0});
        c.overlays.push_back(FrameAround(kActivityBottomTo, 0));
        c.overlays.push_back(FrameAround(topTo, 0));
        c.pieces.push_back(Bottom({0, 0, SW, SH}, kActivityBottomTo));
        c.pieces.push_back({Src::Top, {0, 0, topW, rows}, topTo});
        AddHints(c, ActivityHints(s.screen));
        if (hudHere) AddStatusBar(c, *hud, *strings, topTo.x - 6, topTo.y + topTo.h + kActivityBarGap, topTo.w + 12);
    }
    AddRing(c, s, focus);
    return c;
}
}
