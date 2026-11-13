#include "console.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "activities.h"
#include "hotspots.h"
#include "port/build.h"
#include "port/checkin.h"
#include "port/roominfo.h"
#include "port/music.h"
#include "port/statpanel.h"
#include "port/catalogue.h"
#include "port/dialogue.h"
#include "port/front.h"
#include "port/frontmenus.h"
#include "port/cas.h"
#include "port/gift.h"
#include "port/note.h"
#include "port/options.h"
#include "port/panelmenu.h"
#include "port/minigame.h"
#include "port/phone.h"
#include "port/statusbar.h"
#include "port/popup.h"
#include "port/prompt.h"
#include "port/ownwords.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2hd
{
namespace
{
constexpr Rect kCanvas {0, 0, kConsoleW, kConsoleH};
// Create-a-Sim's fade in (the bottom's brightest channel: every page has a white of 251): black
// below kCasFadeBlack, a veil until kCasFadeWhite, on a layer above every still one.
constexpr int kCasFadeBlack = 16, kCasFadeWhite = 245, kCasFadeLayer = 9;

// The UI's size (ui::UiScale) per group of HD items: each group zoomed about its anchor and held
// inside its limit (compose.h's ZoomUi). The menus and the screens over the dimmed world about the
// space above the strip's centre, never larger than at 100 % (they fill it); the conversation
// about the canvas's bottom centre; the strip's status at its left and hints at its right, about
// the strip's content row, as large as the strip's height lets them (the strip itself, the world's
// foot, keeps its place).
// (Grown, a group stops kUiEdge from the canvas's sides and top, as far in as the 100 % screens'
// own outer margins (Create-a-Sim's panel, the front end's 20): a conversation at 150 % no longer
// runs to 4 pixels of the screen's edge.)
constexpr int kUiEdge = 16;
constexpr Rect kUiLimit {kUiEdge, kUiEdge, kConsoleW - 2 * kUiEdge, kConsoleH - 2 * kUiEdge};
constexpr Rect kMenuLimit {kUiEdge, kUiEdge, kConsoleW - 2 * kUiEdge, kStripY - 4 - kUiEdge};
constexpr float kMenuAnchorY = 338; // (the menu's 30..646, centred)
constexpr Rect kStripInside {0, kStripY + 1 + kStripBand, kConsoleW, kConsoleH - kStripY - 1 - kStripBand};
constexpr float kStripAnchorY = kStripContentY + kStatusBarH / 2.0f;
float StripZoom(float z) { return std::min(z, (kStripAnchorY - kStripInside.y) / (kStatusBarH / 2.0f)); }
float MenuZoom(float z) { return std::min(z, 1.0f); }
// The world's prompt (port/prompt.h): its plate's bottom and its drop shadow above the strip.
constexpr Rect kPromptLimit {kUiEdge, kUiEdge, kConsoleW - 2 * kUiEdge, kStripY - kPromptShadow - 4 - kUiEdge};
// The stick cursor's screens (the touch screen at kMenuTo in its frame, FrameAround: 6 pixels):
// the hints and the HD status bar below the frame (10 pixels below the touch screen, where they sit
// at 100 %), the pause panel right of it (6 pixels clear, above the hints).
constexpr int kCursorBelow = kMenuTo.y + kMenuTo.h + 10, kCursorFrameRight = kMenuTo.x + kMenuTo.w + 6;
constexpr Rect kCursorHintsLimit {4, kCursorBelow, kConsoleW - 8, kConsoleH - 4 - kCursorBelow};
constexpr Rect kCursorPauseLimit {kCursorFrameRight + 6, 4, kConsoleW - 4 - (kCursorFrameRight + 6), kCursorBelow - 4};

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

// The money's last change (port/statusbar.h's bar at `bar`): a small dark pill right of it (the
// theme's shadow, #383869), "+§12" in a light green (8.24:1 on it) or "-§1" in a light red
// (5.30:1; on the bar's own fill they were 2.48 and 1.59), on the bar's layer.
void AddPayout(Composition& c, Rect bar, int payout, const RomData& rom, int right = kConsoleW - 8)
{
    const std::string text = (payout > 0 ? "+" : "-") + MenuWords(rom, kStrMgMoney, std::abs(payout));
    const float size = 26;
    const int w = (int)std::ceil(ui::TextWidth(text, size, ui::Weight::ExtraBold)) + 36;
    if (bar.x + bar.w + 12 + w > right) return;
    ui::UiItem p = ui::PanelItem({bar.x + bar.w + 12, bar.y, w, bar.h});
    p.fill = ui::GameTheme().shadow;
    p.borderArgb = ui::GameTheme().frameDark;
    p.layer = kStatusBarLayer;
    c.ui.push_back(p);
    PanelText(c, {bar.x + bar.w + 12 + 18, bar.y, w - 36, bar.h}, text, size, size, ui::Align::Left, ui::Weight::ExtraBold,
              payout > 0 ? 0xFFB4F0A8 : 0xFFFF9A90, kStatusBarLayer);
}

// The focus ring around hotspot `focus`'s area, as the topmost touchable bottom piece showing the
// area's centre draws it (the area clipped to that piece), the kit's ring around it.
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
        AddKitRingOverlay(c, {rx0, ry0, rx1 - rx0, ry1 - ry0});
        return;
    }
}}

Piece ConsoleWorld(int topW, int rows, bool keepSides) { return World(topW, rows, keepSides); }

void AddKitRingOverlay(Composition& c, Rect r)
{
    constexpr int outer = ui::kFocusOuterW, inner = ui::kFocusInnerW;
    c.overlays.push_back({{r.x - outer, r.y - outer, r.w + 2 * outer, r.h + 2 * outer}, 0, 10 + outer, outer - inner, ui::kFocusOuter, -1});
    c.overlays.push_back({{r.x - inner, r.y - inner, r.w + 2 * inner, r.h + 2 * inner}, 0, 10 + inner, inner, ui::kFocusInner, -1});
}

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

// The stick cursor's hints at y, as the kit's chips in a panel: the right stick moves the cursor,
// A touches, and (close: the hotel's furniture editor) B taps the game's X; beside them a second
// panel, the pause menu's chip (Start + Back, as the device used last prints them; Esc on the
// keyboard). Centred side by side, or (right) at the canvas's right edge, clear of the HD status
// bar under the touch screen, the pause panel above it right of the touch screen's frame. Kept on
// the canvas.
static void AddCursorHint(Composition& c, int y, const RomData* rom, bool right = false, bool close = false)
{
    constexpr float kSize = 24;
    constexpr int kGap = 12, kPad = 14, kBetween = 22, kH = 58;
    struct CursorChip
    {
        ui::Button b;
        const char* word;
    };
    std::vector<CursorChip> hints {{ui::Button::RStick, OwnWord(rom, OwnWordId::Cursor)}, {ui::Button::A, OwnWord(rom, OwnWordId::MixerPoint)}};
    if (close) hints.push_back({ui::Button::B, OwnWord(rom, OwnWordId::Close)});
    const size_t first = c.ui.size();
    int bw = 2 * kPad - kBetween;
    for (const CursorChip& h : hints) bw += ui::ChipRect(0, 0, h.b, h.word, kSize).w + kBetween;
    const char* word = OwnWord(rom, OwnWordId::Pause);
    const Rect chip = ui::ChipRect(0, 0, ui::Button::Pause, word, kSize);
    const int pw = kPad + chip.w + kPad;
    const int bx = right ? kConsoleW - bw - 24 : (kConsoleW - (bw + kGap + pw)) / 2;
    y = std::min(y, kConsoleH - kH - 4);
    c.ui.push_back(ui::PanelItem({bx, y, bw, kH}));
    int x = bx + kPad;
    for (const CursorChip& h : hints)
    {
        const Rect r = ui::ChipRect(0, 0, h.b, h.word, kSize);
        c.ui.push_back(ui::ChipItem(x, y + (kH - r.h) / 2, h.b, h.word, kSize));
        x += r.w + kBetween;
    }
    // The UI's size: about the hints' bottom right (or bottom centre). At the right (the stick
    // cursor's framed touch screen above), the hints below the frame (kCursorHintsLimit), the pause
    // panel its own group about its bottom right, right of the frame (kCursorPauseLimit).
    // (A pause chip wider than the room right of the frame, its glyph at the smallest text size,
    // START + BACK: the pause panel in the hints' row instead, left of them, zoomed with them.)
    const bool beside = right && kConsoleW - 16 - pw < kCursorPauseLimit.x;
    if (right && !beside) ZoomUi(c, first, c.pieces.size(), ui::UiScale(), kConsoleW - 24.0f, (float)(y + kH), kCursorHintsLimit);
    const Rect panel = beside  ? Rect {bx - kGap - pw, y, pw, kH}
                       : right ? Rect {std::max(kCursorPauseLimit.x, kConsoleW - 16 - pw), y - kH - kGap, pw, kH}
                               : Rect {bx + bw + kGap, y, pw, kH};
    const size_t pause = c.ui.size();
    c.ui.push_back(ui::PanelItem(panel));
    c.ui.push_back(ui::ChipItem(panel.x + kPad, panel.y + (kH - chip.h) / 2, ui::Button::Pause, word, kSize));
    if (beside) ZoomUi(c, first, c.pieces.size(), ui::UiScale(), kConsoleW - 24.0f, (float)(y + kH), kCursorHintsLimit);
    else if (right) ZoomUi(c, pause, c.pieces.size(), ui::UiScale(), (float)(panel.x + panel.w), (float)(panel.y + panel.h), kCursorPauseLimit);
    else ZoomUi(c, first, c.pieces.size(), ui::UiScale(), kConsoleW / 2.0f, (float)(y + kH), kUiLimit);
}

TownHints TownHintsFor(const RomData* rom, const ui::ChipLook& look)
{
    TownHints h;
    h.menu = OwnWord(rom, OwnWordId::Menu);
    h.camera = OwnWord(rom, OwnWordId::HintCamera); // (the stick turns it and zooms: "Camera", not "Turn the camera")
    // The keyboard turns the camera with its own L / R keys (Input::Actions' turnL / turnR): their
    // keycaps rather than a stick it has not got.
    h.keys = look.face == ui::Face::Keyboard && !look.glyph[(int)ui::Button::LB].empty() && !look.glyph[(int)ui::Button::RB].empty();
    const Rect chip = ui::ChipRect(0, 0, ui::Button::Back, h.menu, kTownHintSize);
    int cam;
    if (h.keys) cam = ui::ChipRect(0, 0, ui::Button::LB, "", kTownHintSize).w + kTownKeysGap + ui::ChipRect(0, 0, ui::Button::RB, h.camera, kTownHintSize).w;
    else cam = ui::ChipRect(0, 0, ui::Button::RStick, h.camera, kTownHintSize).w;
    h.menuW = chip.w;
    h.w = cam + kTownHintsBetween + chip.w;
    return h;
}

void AddTownHints(Composition& c, const TownHints& h)
{
    const size_t first = c.ui.size();
    const int y = kStripContentY + kStatusBarH / 2;
    int x = kTownHintsRight - h.w;
    if (h.keys)
    {
        const Rect l = ui::ChipRect(0, 0, ui::Button::LB, "", kTownHintSize);
        c.ui.push_back(ui::ChipItem(x, y - l.h / 2, ui::Button::LB, "", kTownHintSize));
        x += l.w + kTownKeysGap;
        const Rect r = ui::ChipRect(0, 0, ui::Button::RB, h.camera, kTownHintSize);
        c.ui.push_back(ui::ChipItem(x, y - r.h / 2, ui::Button::RB, h.camera, kTownHintSize));
    }
    else
    {
        const Rect cam = ui::ChipRect(0, 0, ui::Button::RStick, h.camera, kTownHintSize);
        c.ui.push_back(ui::ChipItem(x, y - cam.h / 2, ui::Button::RStick, h.camera, kTownHintSize));
    }
    const Rect chip = ui::ChipRect(0, 0, ui::Button::Back, h.menu, kTownHintSize);
    c.ui.push_back(ui::ChipItem(kTownHintsRight - chip.w, y - chip.h / 2, ui::Button::Back, h.menu, kTownHintSize));
    for (size_t i = first; i < c.ui.size(); i++) c.ui[i].layer = kStatusBarLayer;
}

const char* ConsoleBranchName(ConsoleBranch b)
{
    static const char* const kNames[] = {"front", "cas", "cas-fade", "backdrop", "front-m1", "screen", "veiled", "world-only", "cursor-m1", "town",
                                         "dialogue", "dialogue-m1", "minigame", "activity-m1"};
    return kNames[(int)b];
}

Composition BuildConsole(ScreenState s, bool menuOpen, int focus, int topW, int topH, const HudState* hud, const HudStrings* strings,
                         const GameState* game, const RomData* rom, const u32* bottom, IconCache* icons, u32 armed, const PointerView* pointer,
                         int payout, bool pictureClosed, bool veiled, int worldOnlyFrames, ConsoleBranch* branch, const PromptSteady* prompt)
{
    ConsoleBranch dummy;
    ConsoleBranch& b = branch ? *branch : dummy;
    Composition c {kConsoleW, kConsoleH, {}, {}};
    // The HD status bar replaces the game's: the world without its last 16 of 192 rows.
    const bool bar = hud && strings && strings->ok;
    const int worldRows = bar ? HudCropRows(topH) : topH;
    // The front end's boot, title and main menu (port/front.h), Settings and Load-a-Sim
    // (port/frontmenus.h) in HD: the whole canvas theirs; Load's "Loading" popup (port/popup.h) over it.
    if (s.screen == Screen::FrontEnd || s.screen == Screen::None || s.screen == Screen::Loading)
        if (const MenuView v = ConsoleView(s, game, rom); v.valid)
        {
            // The module by the front end's kind (FrontView takes the boot, title and main menu,
            // which win over Settings and Load in FrontState::kind).
            const FrontKind kind = game->front.kind;
            auto screen = [&](const MenuView& view, int f) {
                if (kind == FrontKind::Settings) BuildSettings(c, view, f, *game, *rom, bottom, icons, topW, topH, view.picture && !pictureClosed);
                else if (kind == FrontKind::Load) BuildLoad(c, view, f, *game, *rom, icons, topW, topH);
                else BuildFront(c, view, f, s, *game, *rom, icons, topW, topH);
            };
            if (v.popup)
            {
                if (const MenuView u = ScreenView(s, game, rom); u.valid) screen(u, -1);
                BuildPopup(c, v, focus, *game, *rom);
                SortUi(c.ui);
            }
            else screen(v, focus);
            b = ConsoleBranch::Front;
            return c;
        }
    // Create-a-Sim in HD (port/cas.h): the whole canvas its.
    if (s.screen == Screen::CreateASim)
        if (const MenuView v = ConsoleView(s, game, rom); v.valid)
        {
            // The game fading in from black (from the main menu or a load, about 14 frames): the
            // bare backdrop while it is black, then the HD screen under a veil as dark as the
            // game's fade (every page has a white of 251 at full brightness), so the panel never
            // shows bright around black crops.
            int brightest = 255;
            if (bottom)
            {
                brightest = 0;
                for (int i = 0; i < SW * SH; i += 3)
                    brightest = std::max({brightest, (int)((bottom[i] >> 16) & 0xFF), (int)((bottom[i] >> 8) & 0xFF), (int)(bottom[i] & 0xFF)});
            }
            b = ConsoleBranch::CasFade;
            if (brightest < kCasFadeBlack) return c;
            b = ConsoleBranch::Cas;
            // The veil lies over every still item; the pictures lie above it, so the ROM's are drawn
            // as dark as the veil makes the rest (their colour, never their opacity: nothing under
            // them shows through); the touch screen's own are already as dark as the game's fade.
            const int dark = brightest < kCasFadeWhite ? 255 * (kCasFadeWhite - brightest) / kCasFadeWhite : 0;
            BuildCas(c, v, focus, *game, *rom, icons, bottom, topW, topH, 255 - dark);
            if (dark > 0)
            {
                ui::UiItem veil = ui::PanelItem(kCanvas);
                veil.plain = true;
                veil.radius = 0;
                veil.fill = (u32)dark << 24;
                veil.layer = kCasFadeLayer;
                c.ui.push_back(veil);
                SortUi(c.ui);
            }
            return c;
        }
    // A black load (both screens black, RAM between screens): the bare backdrop.
    if (s.screen == Screen::Loading) { b = ConsoleBranch::Backdrop; return c; }
    const bool cursorScreen = s.screen == Screen::SaveSlots || s.screen == Screen::Unknown;
    if (s.screen == Screen::None || s.screen == Screen::FrontEnd || s.screen == Screen::CreateASim)
    {
        // No HUD (title, main menu, Create-a-Sim, an unmapped front-end screen; None from the pixels
        // when RAM does not read): both screens matter alike. The top
        // screen fitted in its box at its own shape, centred; the touch screen 2.5x; both framed.
        const Rect topTo = FrontFit(kFrontTopBox, topW, topH);
        c.overlays.push_back(FrameAround(topTo, 0));
        c.overlays.push_back(FrameAround(kFrontBottomTo, 0));
        c.pieces.push_back({Src::Top, {0, 0, topW, topH}, topTo});
        c.pieces.push_back(Bottom({0, 0, SW, SH}, kFrontBottomTo));
        AddCursorHint(c, kFrontHintY, rom);
        b = ConsoleBranch::FrontM1;
    }
    else if (MenuView v = ConsoleView(s, game, rom); v.valid && (v.own || (s.screen == Screen::Panel && menuOpen)))
    {
        // An HD screen (the panel's menu, port/panelmenu.h; Give Gift, port/gift.h; the Save
        // slots, port/options.h; the phone, port/phone.h; a note, port/note.h) over the dimmed
        // world, the status bar on its own layer above; a popup (port/popup.h) over all of it.
        v.armed = armed;
        Piece world = World(topW, worldRows, bar);
        world.touch = false;
        c.pieces.push_back(world);
        c.overlays.push_back({kCanvas, kDimArgb, 0, 0, 0, 1});
        const size_t i0 = c.ui.size(), p0 = c.pieces.size();
        auto screen = [&](ScreenState at, const MenuView& view, int f) {
            if (at.screen == Screen::Panel) BuildPanelMenu(c, at, view, f, *game, *rom, icons, bottom);
            else if (at.screen == Screen::Gift) BuildGiftMenu(c, view, f, *game, *rom, icons, bottom);
            else if (at.screen == Screen::SaveSlots) BuildSaveSlots(c, view, f, *game, *rom);
            else if (at.screen == Screen::Catalogue) BuildCatalogue(c, view, f, *game, *rom, icons);
            else if (at.screen == Screen::Build) BuildBuildRooms(c, view, f, *game, *rom, icons);
            else if (at.screen == Screen::Phone) BuildPhone(c, view, f, *game, *rom, icons, bottom, strings);
            else if (game->checkIn.valid) BuildCheckIn(c, view, f, *game, *rom, icons, bottom);
            else if (game->roomInfo.valid) BuildRoomInfo(c, view, f, *game, *rom, icons, bottom);
            else if (game->music.valid) BuildMusic(c, view, f, *game, *rom, icons);
            else if (game->stat.valid) BuildStatPanel(c, view, f, *game, *rom, icons);
            else BuildNote(c, view, f, *game, *rom, icons);
        };
        bool lone = false; // (a popup over the dimmed world alone: no screen beneath)
        if (v.popup && PopupOver(s, *game))
        {
            // What the popup is over: its HD screen (the panel's menu while it was open) without a
            // focus, or the dimmed world.
            const ScreenState at = UnderScreen(s, *game);
            if (const MenuView u = ScreenView(at, game, rom); u.valid && (u.own || (at.screen == Screen::Panel && menuOpen))) screen(at, u, -1);
            else lone = true;
            if (bar) AddStatusStrip(c, hud, *strings, 0, StripZoom(ui::UiScale()));
            BuildPopup(c, v, focus, *game, *rom);
        }
        else
        {
            screen(s, v, focus);
            if (bar) AddStatusStrip(c, hud, *strings, 0, StripZoom(ui::UiScale()));
            // Room info's own hint box (a popup of the screen's: port/roominfo.h) and Build Rooms'
            // question (port/build.h), over the bar too.
            if (s.screen == Screen::Unknown && RoomHintUp(*game)) BuildRoomHint(c, v, focus, *game, *rom);
            if (s.screen == Screen::Build) BuildBuildQuestion(c, v, focus, *game, *rom);
        }
        // The UI's size: the screen and its popup together (the strip's own already done); a lone
        // popup (a goal, Game Paused, Quit over the world) as large as the UI's size, as far as the
        // canvas above the strip holds it.
        ZoomUi(c, i0, p0, lone ? ui::UiScale() : MenuZoom(ui::UiScale()), kConsoleW / 2.0f, kMenuAnchorY, kMenuLimit);
        SortUi(c.ui); // (the bar's layer after the screen's, a popup's after it, the images last)
        b = ConsoleBranch::Screen;
        return c;     // (the focus is the view's: no ring from HotspotsFor)
    }
    else if (veiled && !menuOpen && (s.screen == Screen::Panel || s.screen == Screen::Unknown))
    {
        // The panel's menu coming back (B from the Save slots or the phone opened from it): the
        // dimmed world it will open over, never the town's flash while the game passes Interact.
        Piece world = World(topW, worldRows, bar);
        world.touch = false;
        c.pieces.push_back(world);
        c.overlays.push_back({kCanvas, kDimArgb, 0, 0, 0, 1});
        if (bar) AddStatusStrip(c, hud, *strings, 0, StripZoom(ui::UiScale()));
        SortUi(c.ui);
        b = ConsoleBranch::Veiled;
        return c;
    }
    else if (s.screen == Screen::Unknown && !menuOpen && game && (game->valid || game->between) && rom && rom->StringCount() > 0 &&
             (game->between || game->screen == GScreen::Panel || game->vtable == 0) && worldOnlyFrames <= kWorldOnlyMax)
    {
        // The panel coming (its tab not read yet) or no bottom object at all (between two, after a
        // minigame): the town's world (and bar when it reads), never the cursor's framed black touch
        // screen; for kWorldOnlyMax frames in a row at most, then the cursor's screen below (a screen
        // that stays so is not a dead end: the stick cursor and its hints).
        c.pieces.push_back(World(topW, worldRows, bar));
        if (bar) AddStatusStrip(c, hud, *strings, 0, StripZoom(ui::UiScale()));
        b = ConsoleBranch::WorldOnly;
    }
    else if (menuOpen || cursorScreen || s.screen == Screen::Phone || s.screen == Screen::Popup || s.screen == Screen::Gift)
    {
        // The world dimmed behind the touch screen: not playable, the mouse does nothing there.
        // The stick cursor's screens (no table: SaveSlots, Unknown) get a frame.
        Piece world = World(topW, worldRows, bar);
        world.touch = false;
        c.pieces.push_back(world);
        c.overlays.push_back({kCanvas, kDimArgb, 0, 0, 0, 1});
        if (cursorScreen)
        {
            c.overlays.push_back(FrameAround(kMenuTo, 1));
            // (a controller player: how to leave, as on the title; the furniture editor's B its X)
            AddCursorHint(c, kCursorBelow, rom, true, s.screen == Screen::Unknown && game && game->placement.valid);
        }
        c.pieces.push_back(Bottom({0, 0, SW, SH}, kMenuTo));
        // Not dimmed: the game keeps its status bar up while the touch screen is used.
        const size_t i0 = c.ui.size();
        if (bar && AddStatusBar(c, *hud, *strings).w > 0)
            ZoomUi(c, i0, c.pieces.size(), ui::UiScale(), 16, kStatusBarY + kStatusBarH, kCursorHintsLimit);
        b = ConsoleBranch::CursorM1;
    }
    else if (s.screen == Screen::Panel)
    {
        b = ConsoleBranch::Town;
        c.pieces.push_back(World(topW, worldRows, bar));
        // (The HD town whenever RAM and the game's strings read: a frame whose tab does not read yet,
        // the panel coming back from a minigame, never flashes the framed-screen rail.)
        const bool hd = ConsoleView(s, game, rom).valid || (game && game->valid && rom && rom->StringCount() > 0 && bar);
        if (!hd)
        {
            // The framed screens (no strings, another ROM): the rail, the only way to see the tabs.
            c.overlays.push_back(Frame(kRailFrame, 1));
            c.pieces.push_back(Bottom(kRailPhoneFrom, kRailPhoneTo));
            c.pieces.push_back(Bottom(kRailTabsFrom, kRailTabsTo));
        }
        if (hd || bar)
        {
            // The strip under the world, the canvas's whole width (port/statusbar.h): the status at
            // the left; with the HD menu (it has the tabs: no rail) the hints at the right, in the
            // strip, no panel of their own: the camera's, the right stick (navigator.h's world: it
            // turns and zooms), and how to open the menu: the TOUCH MENU's button as the device used
            // last prints it (the pad's Back / View, Share, -; the keyboard's key, E by default:
            // ui::ChipLook) and the word.
            const TownHints h = TownHintsFor(rom, ui::ChipLookNow());
            const float zs = StripZoom(ui::UiScale());
            const int hintsW = hd ? h.w : 0, hintsWz = (int)std::ceil(hintsW * zs);
            static const HudStrings kNoWords;
            const Rect r = AddStatusStrip(c, bar ? hud : nullptr, strings ? *strings : kNoWords, hintsWz, zs);
            if (hd)
            {
                const size_t first = c.ui.size();
                AddTownHints(c, h);
                ZoomUi(c, first, c.pieces.size(), zs, (float)kTownHintsRight, kStripAnchorY, kStripInside);
            }
            // The money's last change (an animation's payout, a prize) beside the status, in the game's words
            // (zoomed with the status, about its left: its room ends where the zoomed hints begin).
            if (bar && payout != 0 && rom && rom->StringCount() > kStrMgMoney)
            {
                const size_t first = c.ui.size();
                AddPayout(c, r, payout, *rom, 16 + (int)std::floor((kTownHintsRight - hintsWz - 12 - 16) / zs));
                ZoomUi(c, first, c.pieces.size(), zs, 16, kStripAnchorY, kStripInside);
            }
            // What A does here (port/prompt.h), on its own layer above the strip's: its own group,
            // zoomed about its bottom centre (kPromptBottom, the canvas's centre), held above the
            // strip (its drop shadow too) at every UI size.
            if (bar && hd && game && rom && rom->StringCount() > 0 && prompt)
            {
                const size_t first = c.ui.size();
                AddWorldPrompt(c, s, *game, *rom, *prompt);
                ZoomUi(c, first, c.pieces.size(), ui::UiScale(), kConsoleW / 2.0f, (float)kPromptBottom, kPromptLimit);
            }
        }
    }
    else if (s.screen == Screen::Dialogue && game && game->valid && rom && rom->StringCount() > 0 && bottom)
    {
        // The HD conversation (port/dialogue.h): the game's words, its speaker and portrait (the
        // panel alone while no page is drawn yet: the frames between two speeches).
        BuildDialogue(c, *game, *rom, bottom, topW, topH);
        // The UI's size: the box about the canvas's bottom centre (its margin below zoomed too).
        ZoomUi(c, 0, 0, ui::UiScale(), kConsoleW / 2.0f, kConsoleH, kUiLimit);
        // The strip under the world (the world stops at it: no black band below), its panel alone
        // (the box lies over it), under the box: first, on layer 0.
        {
            static const HudStrings kNoWords;
            const size_t at = c.ui.size();
            AddStatusStrip(c, nullptr, kNoWords);
            for (size_t i = at; i < c.ui.size(); i++) c.ui[i].layer = 0;
            std::rotate(c.ui.begin(), c.ui.begin() + (long)at, c.ui.end());
        }
        b = ConsoleBranch::Dialogue;
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
        b = ConsoleBranch::DialogueM1;
    }
    else if (MinigameFramed(s, game, rom))
    {
        // The HD minigame frame (port/minigame.h): the playfield big, the top screen beside it, the
        // title, the game's values and the hints' chips.
        BuildMinigame(c, s, topW, topH, hud, strings, *game, *rom, bottom, icons, pointer, focus);
        b = ConsoleBranch::Minigame;
    }
    else
    {
        // Social, Vacuum, Cards, Catalogue, Build; the hints' box for the activities. All but the
        // cards keep the HUD: their small world without it, the HD bar under it.
        b = ConsoleBranch::ActivityM1;
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
        if (pointer && pointer->mark.w > 0)
        {
            // MooGoo's focus (activities.h): a ring around what A touches.
            const Rect m = pointer->mark, to = kActivityBottomTo;
            const int x0 = to.x + m.x * to.w / SW, y0 = to.y + m.y * to.h / SH, x1 = to.x + (m.x + m.w) * to.w / SW, y1 = to.y + (m.y + m.h) * to.h / SH;
            AddKitRingOverlay(c, {x0, y0, x1 - x0, y1 - y0});
        }
        if (hudHere) AddStatusBar(c, *hud, *strings, topTo.x - 6, topTo.y + topTo.h + kActivityBarGap, topTo.w + 12);
    }
    // (The cards' focus only while the player's turn takes a card: never on the opponent's hand.)
    const CardsState* k = game && s.screen == Screen::Cards && game->minigame.cards.valid ? &game->minigame.cards : nullptr;
    if (!k || (k->turn == 0 && k->phase <= 2)) AddRing(c, s, focus);
    return c;
}
}
