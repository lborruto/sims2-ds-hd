#include "port/dialogue.h"
#include "port/panelmenu.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>

#include "console.h"
#include "port/menuview.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
// The game's speaker codes (research: 0x3F no speaker, 0x40 the player) and its am / pm.
constexpr int kSpeakerNone = 0x3F, kSpeakerPlayer = 0x40;
// The game's own colours around the portrait on the touch screen: its frame's inner bevels (the
// rest are the panel's: console.h, the kit's theme).
constexpr u32 kBevelDark = 0x414979, kBevelLight = 0x51518A;
// The game's arrow on the touch screen (inside its outline): a click on the panel taps it.
constexpr Rect kArrowInside {223, 159, 23, 24};

bool IsPunct(char c) { return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':'; }

// Spaces folded (a drop leaves two), none at a line's ends, none before punctuation that a drop
// put there ("the @2." → "the."). A space before '!' / '?' that the string had (French) stays:
// only spaces doubled or left at an end go, and a space before punctuation when it was doubled.
std::string Tidy(const std::string& in, const std::vector<bool>& dropped)
{
    std::string s;
    for (size_t i = 0; i < in.size(); i++)
    {
        const char c = in[i];
        if (c == ' ' && (s.empty() || s.back() == ' ' || s.back() == '\n')) continue;
        if (c == '\n') while (!s.empty() && s.back() == ' ') s.pop_back();
        if (IsPunct(c) && !s.empty() && s.back() == ' ' && i > 0 && dropped[i - 1]) s.pop_back();
        s += c;
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n')) s.pop_back();
    while (!s.empty() && s[0] == '\n') s.erase(0, 1);
    return s;
}

void Push(std::string& out, std::vector<bool>& dropped, const std::string& add, bool wasDrop)
{
    out += add;
    dropped.insert(dropped.end(), add.size(), false);
    if (wasDrop && !dropped.empty() && add.empty()) dropped.back() = true;
}

ui::UiItem PortraitFrame(Rect r)
{
    ui::UiItem w = ui::WellItem(r);
    w.radius = 10;
    return w;
}
}

std::string DialogueText(const std::string& raw, const HudState& hud, const std::string& am, const std::string& pm)
{
    const std::string in = DropEscapes(raw);
    std::string out;
    std::vector<bool> dropped; // per byte of out: a placeholder was dropped right after it
    const bool clock = hud.valid && hud.hour >= 0 && hud.hour <= 23 && hud.minute >= 0 && hud.minute <= 59;
    for (size_t i = 0; i < in.size(); i++)
    {
        if (in[i] != '@') { Push(out, dropped, std::string(1, in[i]), false); continue; }
        // A placeholder: '@' and its digits, or '@' and up to two letters.
        size_t j = i + 1;
        if (j < in.size() && std::isdigit((unsigned char)in[j])) while (j < in.size() && std::isdigit((unsigned char)in[j])) j++;
        else while (j < in.size() && j < i + 3 && std::isalpha((unsigned char)in[j])) j++;
        const std::string tag = in.substr(i + 1, j - i - 1);
        std::string value;
        if (tag == "1") value = hud.simName;
        else if (clock && tag == "hh") value = std::to_string(hud.hour % 12 == 0 ? 12 : hud.hour % 12);
        else if (clock && tag == "mm") value = (hud.minute < 10 ? "0" : "") + std::to_string(hud.minute);
        else if (clock && tag == "am") value = hud.hour < 12 ? am : pm;
        // (The name is UTF-8 decoded from the game's charset in RAM.)
        value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == '@' || c == '{' || c == '}'; }), value.end());
        Push(out, dropped, value, value.empty());
        i = j - 1;
    }
    return Tidy(out, dropped);
}

DialogueLayout LayoutDialogue(const std::string& text, bool portrait)
{
    DialogueLayout L;
    L.portrait = portrait;
    const int portraitW = kDlgPortraitFrom.w * kDlgPortraitScale + 2 * kDlgPortraitPad;
    const int wellW = kDlgPanelW - 2 * kDlgPad - (portrait ? portraitW + kDlgPad : 0);
    L.textW = wellW - 2 * kDlgTextPad;
    // The A chip sits in the text well's bottom right corner: the last row's text must end
    // before it when it is on the bottom row.
    const int chipW = ui::ChipRect(0, 0, ui::Button::A, "", kDlgChipSize).w + 10;
    auto tryAt = [&](float size, int rows) {
        std::vector<std::string> lines = ui::Wrap(text, size, (float)L.textW);
        if ((int)lines.size() > rows) return false;
        if ((int)lines.size() == rows && rows > 0 && ui::TextWidth(lines.back(), size) > L.textW - chipW) return false;
        L.size = size;
        L.lines = std::move(lines);
        L.rows = rows;
        return true;
    };
    bool ok = tryAt(kDlgTextSize, kDlgRows) || tryAt(kDlgTextMin, kDlgRows);
    for (int rows = kDlgRows + 1; !ok && rows <= kDlgMaxRows; rows++) ok = tryAt(kDlgTextMin, rows);
    if (!ok)
    {   // Longer than the panel ever grows: clipped to its rows (never past the panel).
        L.size = kDlgTextMin;
        L.lines = ui::Wrap(text, kDlgTextMin, (float)L.textW);
        L.rows = kDlgMaxRows;
        L.fits = false;
    }
    L.lineH = (int)std::lround(L.size * 1.3f); // the kit's line height (TextItem)
    for (const std::string& l : L.lines) L.fits = L.fits && ui::TextWidth(l, L.size) <= L.textW;
    const int textH = L.rows * L.lineH;
    // At least the well of four rows at 30 px, so the box keeps its size from page to page (a page
    // at 28 px or with fewer lines does not shrink it).
    const int floorH = kDlgRows * (int)std::lround(kDlgTextSize * 1.3f) + 2 * kDlgTextPad - 4;
    const int wellH = std::max({textH + 2 * kDlgTextPad - 4, floorH, portrait ? kDlgPortraitFrom.h * kDlgPortraitScale + 2 * kDlgPortraitPad : 0});
    const int h = kDlgTopPad + wellH + kDlgPad;
    L.panel = {kDlgPanelX, kConsoleH - kDlgBottomGap - h, kDlgPanelW, h};
    return L;
}

bool PortraitShown(const u32* bottom)
{
    if (!bottom) return false;
    // The frame's own colours (fill, bevels, outline) and black do not count.
    int picture = 0, total = 0;
    for (int y = kDlgPortraitFrom.y; y < kDlgPortraitFrom.y + kDlgPortraitFrom.h; y += 2)
        for (int x = kDlgPortraitFrom.x; x < kDlgPortraitFrom.x + kDlgPortraitFrom.w; x += 2)
        {
            const u32 p = bottom[y * SW + x] & 0xFFFFFF;
            const int r = p >> 16 & 0xFF, g = p >> 8 & 0xFF, b = p & 0xFF;
            total++;
            const u32 theme[] = {kPanelArgb & 0xFFFFFF, kFrameArgb & 0xFFFFFF, kOutlineArgb & 0xFFFFFF, ui::GameTheme().frame & 0xFFFFFF, kBevelDark, kBevelLight};
            if (std::find(std::begin(theme), std::end(theme), p) != std::end(theme)) continue;
            if (r < 24 && g < 24 && b < 24) continue;
            picture++;
        }
    return picture * 5 >= total; // a fifth of the rectangle at least
}

void BuildDialogue(Composition& c, const GameState& game, const RomData& rom, const u32* bottom, int topW, int topH)
{
    using namespace ui;
    const DialogueState& d = game.dialogue;
    // The world whole: without the game's status bar rows, its sides kept (as the HD bar's).
    c.pieces.push_back(ConsoleWorld(topW, HudCropRows(topH), true));

    // No page drawn yet (a speech starting, the frames between two): the panel alone.
    const std::string text = d.valid ? DialogueText(rom.String(d.stringId), game.hud, rom.String(kDlgAmId), rom.String(kDlgPmId)) : "";
    std::string name;
    if (!d.valid) {}
    else if (d.speaker == kSpeakerPlayer) name = game.hud.simName;
    else if (d.speaker != kSpeakerNone && d.speakerNameId >= 0) name = DialogueText(rom.String(d.speakerNameId), game.hud);
    for (char& ch : name) if (ch == '\n') ch = ' ';
    // The portrait's frame stays while a speaker talks (and between pages), so the box does not
    // jump while the touch screen fades in; the picture is drawn when the game shows one.
    const bool picture = d.valid && PortraitShown(bottom);
    const bool portrait = picture || !d.valid || d.speaker != kSpeakerNone;
    const DialogueLayout L = LayoutDialogue(text, portrait);
    const Rect& P = L.panel;

    // A click anywhere on the panel taps the game's arrow (the piece is not drawn: alpha 0; the
    // UI over it never takes the mouse).
    Piece tap {Src::Bottom, kArrowInside, P};
    tap.alpha = 0;
    c.pieces.push_back(tap);

    const size_t uiFirst = c.ui.size();
    UiItem panel = PanelItem(P);
    c.ui.push_back(panel);
    const int wellY = P.y + kDlgTopPad, wellH = P.h - kDlgTopPad - kDlgPad;
    const int portraitW = kDlgPortraitFrom.w * kDlgPortraitScale + 2 * kDlgPortraitPad;
    const Rect well {P.x + kDlgPad, wellY, L.textW + 2 * kDlgTextPad, wellH};
    UiItem w = WellItem(well);
    c.ui.push_back(w);
    // The text: from the well's top, its rows centred in the well's height.
    const int textH = L.rows * L.lineH;
    const Rect textBox {well.x + kDlgTextPad, well.y + (well.h - textH) / 2, L.textW, textH};
    // The page's words and the chip on layer 1: a page turning rasters them alone, the panel,
    // well, plate and portrait's frame (layer 0) stay as they were while the box keeps its size.
    const size_t textAt = c.ui.size();
    UiItem t = TextItem(textBox, text, L.size, Align::Left, Weight::Bold);
    c.ui.push_back(t);
    // The A chip in the well's bottom right corner.
    const Rect chip = ChipRect(0, 0, Button::A, "", kDlgChipSize);
    c.ui.push_back(ChipItem(well.x + well.w - chip.w - 12, well.y + well.h - chip.h - 10, Button::A, "", kDlgChipSize));
    const size_t textEnd = c.ui.size();
    // The name plate: a tab over the panel's top edge, above the well's left.
    if (!name.empty())
    {
        const int nameW = (int)std::ceil(TextWidth(name, kDlgNameSize, Weight::ExtraBold));
        const int plateW = std::clamp(nameW + 56, 220, well.w);
        const Rect plate {well.x, P.y - 26, plateW, 54};
        UiItem p = PanelItem(plate);
        p.fill = GameTheme().frame;
        p.borderArgb = GameTheme().fill;
        p.border = 3;
        p.radius = 14;
        c.ui.push_back(p);
        UiItem n = TextItem({plate.x + 20, plate.y, plate.w - 40, plate.h}, name, kDlgNameSize, Align::Centre, Weight::ExtraBold);
        n.wrap = false;
        n.middle = true;
        c.ui.push_back(n);
    }
    // The portrait, live from the touch screen, framed at the panel's right.
    if (portrait)
    {
        const Rect frame {P.x + P.w - kDlgPad - portraitW, wellY, portraitW, wellH};
        c.ui.push_back(PortraitFrame(frame));
        const int iw = kDlgPortraitFrom.w * kDlgPortraitScale, ih = kDlgPortraitFrom.h * kDlgPortraitScale;
        if (picture) c.ui.push_back(KeyedCrop(bottom, kDlgPortraitFrom, {frame.x + (frame.w - iw) / 2, frame.y + (frame.h - ih) / 2, iw, ih}));
    }
    for (size_t i = textAt; i < textEnd; i++) c.ui[i].layer = 1;
    SortUi(c.ui, uiFirst);
}
}
