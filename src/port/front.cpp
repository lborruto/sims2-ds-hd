#include "port/front.h"

#include <algorithm>
#include <cmath>

#include "console.h"
#include "port/icons.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr u32 kButtonFill = 0xFF6A72B8, kButtonFocusFill = 0xFF9AA2E3;   // (the HD rows' lavender)
constexpr u32 kChipPlate = 0xB0202436;                                   // the title's chip plate: dark, see-through

enum class FrontScreen { None, Boot, Title, Menu };

FrontScreen ScreenOf(ScreenState s, const GameState& game)
{
    if (!game.valid) return FrontScreen::None;
    const FrontState& f = game.front;
    // The title and the main menu: the front end's GScreen (the title object in the top slot).
    if (s.screen == Screen::FrontEnd && f.title.valid && (f.kind == FrontKind::Title || f.kind == FrontKind::MainMenu)) return f.title.phase == 0 ? FrontScreen::Title : FrontScreen::Menu;
    // The boot: the splash object (the video's or not), no HUD (its fades between splashes black:
    // Loading); not its popup mode (4).
    if ((s.screen == Screen::None || s.screen == Screen::Loading) && f.boot.valid && f.boot.mode != 4 && (f.kind == FrontKind::Boot || f.kind == FrontKind::Video)) return FrontScreen::Boot;
    return FrontScreen::None;
}

// `w` x `h` fitted in box at its shape, centred.
Rect Fit(Rect box, int w, int h)
{
    if (w <= 0 || h <= 0) return box;
    const double k = std::min((double)box.w / w, (double)box.h / h);
    const int fw = std::min(box.w, (int)std::lround(w * k)), fh = std::min(box.h, (int)std::lround(h * k));
    return {box.x + (box.w - fw) / 2, box.y + (box.h - fh) / 2, fw, fh};
}

// A framed piece: its 6-pixel frame just outside it (as CONSOLE's front end), not touchable (the
// top screen: no camera here).
void FramedPiece(Composition& c, Piece p)
{
    c.overlays.push_back({{p.to.x - 6, p.to.y - 6, p.to.w + 12, p.to.h + 12}, kFrameArgb, 14, 6, kOutlineArgb, (int)c.pieces.size()});
    c.pieces.push_back(p);
}

// The top picture's 2D part: these screens are the game's BG layers alone, the DS's 4:3, which
// the widescreen looks draw centred with black sides (--wide): those sides left out.
Rect TopFrom(int topW, int topH)
{
    const int w = std::min(topW, (int)std::lround(topH * 4.0 / 3.0));
    return {(topW - w) / 2, 0, w, topH};
}

std::string Label(const RomData& rom, int i) { return MenuWords(rom, kStrFrMenu[i]); }
}

Rect FrontFit(Rect box, int w, int h) { return Fit(box, w, h); }
Rect FrontTopFrom(int topW, int topH) { return TopFrom(topW, topH); }
void FrontFramed(Composition& c, Piece p) { FramedPiece(c, p); }

MenuView FrontView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrFrMenu[2]) return {};
    return FrontViewOf(s, *game, rom);
}

MenuView FrontViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    const FrontScreen at = ScreenOf(s, game);
    switch (at)
    {
    case FrontScreen::None: return v;
    case FrontScreen::Boot:
        // Only the last splash takes the touch (it also ends the video): A or Start tap it.
        if (game.front.boot.mode == 0)
        {
            ViewItem it;
            it.rect = kFrSplash;
            it.id = kFrBootId;
            it.action = {TapStep(kFrSplashX, kFrSplashY)};
            v.items.push_back(it);
            v.buttons[(int)MenuButton::Start] = it.action;
        }
        // The splashes that take no touch: the DS's Start and A passed on (as before this view:
        // harmless where the game ignores them, any skip of its own kept).
        else v.passKeys = (1u << kStart) | (1u << kA);
        v.page = 1;
        break;
    case FrontScreen::Title:
    {
        // Any release on the touch screen begins: the game's big button.
        ViewItem it;
        it.rect = {0, 0, kConsoleW, kConsoleH};
        it.id = kFrTitleId;
        it.action = {TapStep(kFrBigX, kFrBigY)};
        v.items.push_back(it);
        v.buttons[(int)MenuButton::Start] = it.action;
        v.page = 2;
        break;
    }
    case FrontScreen::Menu:
    {
        // The game's buttons take a touch only once faded in (phase 3); before and after a choice
        // they are drawn but dead.
        const bool live = game.front.title.phase == 3;
        for (int i = 0; i < kFrMenuButtons; i++)
        {
            ViewItem it;
            it.rect = FrontButtonRect(i);
            it.id = kFrMenuId + (u32)i;
            it.enabled = live;
            if (live) it.action = {TapStep(kFrMenuBoxX + kFrMenuBoxW / 2 - 1, kFrMenuBoxY0 + i * kFrMenuBoxDy + kFrMenuBoxH / 2 - 1)};
            it.next[0] = i == 0 ? kNoMove : i - 1;
            it.next[1] = i == kFrMenuButtons - 1 ? kNoMove : i + 1;
            it.next[2] = it.next[3] = kNoMove;
            if (rom) it.word = Label(*rom, i);
            v.items.push_back(it);
        }
        v.page = 3;
        break;
    }
    }
    // A late tap would land on the next screen (a choice, a skip): never replayed.
    v.replay = false;
    v.own = true;
    v.valid = true;
    return v;
}

void BuildFront(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, int topW, int topH)
{
    const size_t first = c.ui.size();
    const FrontScreen at = v.page == 1 ? FrontScreen::Boot : v.page == 2 ? FrontScreen::Title : v.page == 3 ? FrontScreen::Menu : FrontScreen::None;
    if (at == FrontScreen::Title)
    {
        // The game's top screen (its backdrop and logo) as large as the canvas holds it.
        const Rect from = TopFrom(topW, topH);
        Piece top {Src::Top, from, Fit({0, 0, kConsoleW, kConsoleH}, from.w, from.h)};
        top.touch = false;
        c.pieces.push_back(top);
        // The whole canvas is the game's big button for the mouse.
        AddMousePiece(c, kFrBigButtonFrom, {0, 0, kConsoleW, kConsoleH});
        // At its foot, on a dark plate: the A chip and the game's blue ▶.
        const Rect chip = ChipRect(0, 0, Button::A, "", 40, 0, Weight::ExtraBold);
        const int gap = 16, arrow = 48, pw = 28 + chip.w + gap + arrow + 28, ph = 72;
        const Rect plate {(kConsoleW - pw) / 2, kConsoleH - ph - 14, pw, ph};
        UiItem p = PanelItem(plate);
        p.plain = true;
        p.fill = kChipPlate;
        p.radius = ph / 2.0f;
        c.ui.push_back(p);
        UiItem a = ChipItem(plate.x + 28, plate.y + (ph - chip.h) / 2, Button::A, "", 40, 0, Weight::ExtraBold);
        a.layer = 1;
        c.ui.push_back(a);
        AddSpriteIcon(c, icons, kSprFrArrow, 0, 2, {plate.x + 28 + chip.w + gap, plate.y + (ph - arrow) / 2, arrow, arrow});
    }
    else if (at == FrontScreen::Menu)
    {
        FramedPiece(c, {Src::Top, TopFrom(topW, topH), Fit(kFrTopBox, TopFrom(topW, topH).w, topH), 255, false});
        UiItem panel = PanelItem(kFrPanel);
        panel.radius = 22;
        c.ui.push_back(panel);
        struct Icon { int desc, frame, scale; Rect box; };
        std::vector<Icon> images;
        for (int i = 0; i < (int)v.items.size() && i < kFrMenuButtons; i++)
        {
            const ViewItem& it = v.items[i];
            const bool focused = focus == i && it.enabled;
            const Rect r = it.rect;
            if (focused) AddFocusRing(c, r, r.h / 2.0f);
            UiItem b = PanelItem(r);
            b.fill = focused ? kButtonFocusFill : kButtonFill;
            b.borderArgb = GameTheme().frame;
            b.border = 3;
            b.radius = r.h / 2.0f;
            b.layer = 1;
            c.ui.push_back(b);
            // The label (DS Wireless: after the game's icon), the game's ▶ at the right end.
            const int arrow = kFrArrow, iconW = i == 3 ? kFrIconW : 0;
            const int tx = r.x + 28 + iconW, tw = r.x + r.w - 12 - arrow - 12 - tx;
            PanelText(c, {tx, r.y, tw, r.h}, Label(rom, i), kFrLabelSize, kFrLabelMin, Align::Left, Weight::ExtraBold);
            if (i == 3) images.push_back({kSprFrWireless, 0, 2, {r.x + 24, r.y + (r.h - 48) / 2, 48, 48}});
            images.push_back({focused ? kSprFrArrowOn : kSprFrArrow, 0, 2, {r.x + r.w - 12 - arrow, r.y + (r.h - arrow) / 2, arrow, arrow}});
            AddMousePiece(c, {kFrMenuBoxX, kFrMenuBoxY0 + i * kFrMenuBoxDy, kFrMenuBoxW, kFrMenuBoxH}, r);
        }
        // The hints: the D-pad's Up / Down and A (none while the buttons fade in).
        if (MenuFocusable(v, focus))
        {
            const Rect ar = ChipRect(0, 0, Button::A, "", kPmHintSize, 0, Weight::ExtraBold);
            const Rect dr = ChipRect(0, 0, Button::DPadV, "", kPmHintSize, 0, Weight::ExtraBold);
            const int right = kFrHints.x + kFrHints.w;
            UiItem a = ChipItem(right - ar.w, kFrHints.y + (kFrHints.h - ar.h) / 2, Button::A, "", kPmHintSize, 0, Weight::ExtraBold);
            a.layer = 1;
            c.ui.push_back(a);
            UiItem d = ChipItem(right - ar.w - 28 - dr.w, kFrHints.y + (kFrHints.h - dr.h) / 2, Button::DPadV, "", kPmHintSize, 0, Weight::ExtraBold);
            d.layer = 1;
            c.ui.push_back(d);
        }
        for (const Icon& i : images) AddSpriteIcon(c, icons, i.desc, i.frame, i.scale, i.box);
    }
    else if (at == FrontScreen::Boot)
    {
        // The video (top) large at the left and the game's splash (the touch screen) beside it.
        FramedPiece(c, {Src::Top, TopFrom(topW, topH), Fit(kFrTopBox, TopFrom(topW, topH).w, topH), 255, false});
        FramedPiece(c, {Src::Bottom, {0, 0, SW, SH}, kFrSplash});
        // Where the game waits for a touch: A and START, under the splash.
        if (!v.items.empty())
        {
            const Rect ar = ChipRect(0, 0, Button::A, "", 32, 0, Weight::ExtraBold);
            const Rect sr = ChipRect(0, 0, Button::Start, "", 32, 0, Weight::ExtraBold);
            const int w = ar.w + 24 + sr.w, x = kFrSplash.x + (kFrSplash.w - w) / 2, y = kFrSplash.y + kFrSplash.h + 30;
            UiItem a = ChipItem(x, y + (std::max(ar.h, sr.h) - ar.h) / 2, Button::A, "", 32, 0, Weight::ExtraBold);
            UiItem s = ChipItem(x + ar.w + 24, y + (std::max(ar.h, sr.h) - sr.h) / 2, Button::Start, "", 32, 0, Weight::ExtraBold);
            a.layer = s.layer = 1;
            c.ui.push_back(a);
            c.ui.push_back(s);
        }
    }
    (void)game;
    SortUi(c.ui, first);
}

void MeasureFront(const RomData& rom, BlockMeasure& m)
{
    const Measurer tally {m};
    for (int i = 0; i < kFrMenuButtons; i++)
    {
        const Rect r = FrontButtonRect(i);
        const int arrow = kFrArrow, iconW = i == 3 ? kFrIconW : 0;
        tally.Line(kStrFrMenu[i], Label(rom, i), kFrLabelSize, kFrLabelMin, r.w - 28 - iconW - 12 - arrow - 12);
    }
}
}
