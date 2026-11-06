#include "port/minigame.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "port/menuview.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "port/popup.h"
#include "port/statpanel.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
using namespace ui;

namespace
{
constexpr u32 kHeadColour = 0xFFD2D6F4;    // the board's column heads: a dim lavender white
constexpr u32 kValueColour = 0xFFFFE9A0;   // a warm white (the money's, room info's values)
constexpr u32 kLitFill = 0xFFB8323A;       // a hint the game asks for now (the vacuum's clog): the game's red, deepened
constexpr u32 kRowFill = 0xFF6A72B8;       // the board's rows (Interact's)

// One hint of the prompt bar: its chips (a separator between two: "/" or "+"), then its label.
// lit: the game asks for it now (a red plate behind it).
struct Token
{
    bool chip;
    Button b;
    const char* sep;
};
struct HdHint
{
    std::vector<Token> tokens;
    std::string text;
    bool lit = false;
};
Token C(Button b) { return {true, b, ""}; }
Token Sep(const char* s) { return {false, Button::A, s}; }

std::string Words(const RomData& rom, int id) { return MenuWords(rom, id); }

// The port's own words, where the game's table has none: one table per game, so that each can
// move into the five-language table whole. (English today, whatever --language says.)
struct PointWords
{
    const char* point = "Point";
    const char* slow = "Slow";
};
struct AutopsyWords
{
    const char* tools[3] = {"Laser", "Hand", "Tweezers"};
    const char* use[3] = {"Hold: cut", "Hold: lift an organ", "Hold: pick a parasite"};
    const char* carry[2] = {"To the tray", "To the flask"};
    const char* tear = "Hold and pull: tear it open";
    const char* quit = "Quit";
    const char* quitAgain = "Again: quit, the earnings lost";
    const char* specimen = "Specimen @1 of 2";
    const char* earnings = "Earnings";
    const char* organ = "Organ @1";
    const char* freed = "Freed";
    const char* removed = "Removed";
    const char* destroyed = "Destroyed";
    const char* parasites = "Parasites";
    const char* eating = "@1 eating";
};
struct DetectorWords
{
    const char* walk = "Walk";
    const char* dig = "Dig";
    const char* depth = "Depth";
    const char* powerOff = "Power off";
    const char* charge = "Charge";
    const char* signal = "Signal";
    const char* none = "None";
};
struct ArtWords
{
    const char* paint = "Hold: paint";
    const char* tool = "Tool";
    const char* colour = "Next color";
    const char* undo = "Undo";
    const char* done = "Done";
    const char* tools[6] = {"Pencil", "Eraser", "Line", "Bucket", "Rectangle", "Filled rectangle"};   // the game's tool 0..5
    const char* sizes[3] = {"small", "medium", "large"};
    const char* colourOf = "Color @1 of 15";
    const char* page = "Palette @1 of 16";
};
constexpr PointWords kPointWords;
constexpr AutopsyWords kAutopsyWords;
constexpr DetectorWords kDetectorWords;
constexpr ArtWords kArtWords;

// A port word with its @1 filled.
std::string With(const char* w, int n)
{
    std::string s = w;
    if (const size_t at = s.find("@1"); at != std::string::npos) s.replace(at, 2, std::to_string(n));
    return s;
}

// The pointer's hints, shared by the pointing games: a stick (either) points, LT slows it.
HdHint PointHint() { return {{C(Button::LStick), Sep("/"), C(Button::RStick)}, kPointWords.point}; }
HdHint SlowHint() { return {{C(Button::LT)}, kPointWords.slow}; }

// The prompt bar's hints: the scheme of activities.cpp, and what the game says now. quitArmed:
// the autopsy's first B / Start (a second quits).
std::vector<HdHint> Hints(Screen s, const GameState& game, const RomData& rom, bool quitArmed = false)
{
    switch (s)
    {
    case Screen::Autopsy:
    {
        const AutopsyState& a = game.minigame.autopsy;
        const AutopsyWords& w = kAutopsyWords;
        if (quitArmed) return {{{C(Button::B)}, w.quitAgain, true}};
        const int tool = a.valid ? std::clamp(a.tool, 0, 2) : 0;
        std::vector<HdHint> h {PointHint(), SlowHint()};
        if (a.valid && (a.phase == 5 || a.phase == 6)) h.push_back({{C(Button::A)}, w.tear});
        else if (a.valid && (a.sub == 3 || a.sub == 6)) h.push_back({{C(Button::LB)}, w.carry[a.sub == 3 ? 0 : 1], true});
        else
        {
            h.push_back({{C(Button::A)}, w.use[tool]});
            // The other tools (the one the game flashes for lit: an organ freed, a parasite eating).
            const Button key[3] = {Button::X, Button::Y, Button::RB};
            const bool alert[3] = {false, a.valid && a.handAlert, a.valid && a.tweezersAlert};
            for (int t = 0; t < 3; t++)
                if (t != tool) h.push_back({{C(key[t])}, w.tools[t], alert[t]});
        }
        h.push_back({{C(Button::B)}, w.quit});
        return h;
    }
    case Screen::Detector:
    {
        const DetectorWords& w = kDetectorWords;
        return {{{C(Button::LStick), Sep("/"), C(Button::DPad)}, w.walk},
                {{C(Button::A)}, w.dig, game.minigame.detector.valid && game.minigame.detector.inRange},
                {{C(Button::LB), C(Button::RB), Sep("/"), C(Button::RStick)}, w.depth},
                {{C(Button::B)}, w.powerOff}};
    }
    case Screen::Art:
    {
        const ArtWords& w = kArtWords;
        const PopupState& p = game.town.popup;
        if (p.valid && p.state == 2 && p.kind == 2) return {{{C(Button::A)}, Words(rom, kStrMgYes)}, {{C(Button::B)}, Words(rom, kStrMgNo)}};
        return {PointHint(), SlowHint(), {{C(Button::A)}, w.paint}, {{C(Button::LB), C(Button::RB)}, w.tool},
                {{C(Button::X)}, w.colour}, {{C(Button::Y)}, w.undo}, {{C(Button::B)}, w.done}};
    }
    case Screen::Cards:
    {
        const CardsState& k = game.minigame.cards;
        if (k.valid && k.quitAsk) return {{{C(Button::A)}, Words(rom, kStrMgYes)}, {{C(Button::B)}, Words(rom, kStrMgNo)}};
        if (k.valid && k.message) return {{{C(Button::A)}, "Continue"}};
        return {{{C(Button::DPad)}, "Choose a card"}, {{C(Button::A)}, "Pick up, again to play"}, {{C(Button::X)}, "Draw a card"},
                {{C(Button::Y)}, "Discard"}, {{C(Button::B)}, Words(rom, kStrMgQuit)}};
    }
    case Screen::Vacuum:
    {
        const bool clog = game.minigame.vacuum.valid && game.minigame.vacuum.clogged;
        return {{{C(Button::LStick)}, "Drive"}, {{C(Button::RStick), Sep("/"), C(Button::X), Sep("+"), C(Button::DPad)}, "Move the hand"},
                {{C(Button::A)}, "Grab, drag to the box"}, {{C(Button::Y)}, clog ? "Blow: unclog!" : "Blow: unclog", clog}};
    }
    case Screen::Social:
        return {{{C(Button::DPadL), C(Button::DPadU), C(Button::DPadR)}, "Answer"}, {{C(Button::A)}, "The focused answer"}};
    default: return {};
    }
}

std::string Title(Screen s, const RomData& rom)
{
    if (s == Screen::Cards) return Words(rom, kStrMgCards);
    if (s == Screen::Vacuum) return Words(rom, kStrMgVacuum);
    if (s == Screen::Autopsy) return Words(rom, kStrMgAutopsy);
    if (s == Screen::Detector) return Words(rom, kStrMgDetector);
    if (s == Screen::Art) return Words(rom, kStrMgArtGallery);
    return "";
}

void Line(Composition& c, Rect r, const std::string& s, float size, float minSize, Align a, Weight w, u32 colour, int layer = 1)
{
    const FittedText f = FitLine(s, size, minSize, r.w, w);
    UiItem t = TextItem(r, f.text, f.size, a, w);
    t.wrap = false;
    t.middle = true;
    t.colour = colour;
    t.layer = layer;
    c.ui.push_back(t);
}

// The board's columns in the cards' well: the name, its cards, its score (from the right).
constexpr int kColW = 78;

// The cards' values: the board (a head row, the player's and the opponent's rows: name, cards,
// score), then the deck (a thumbnail of the game's own, its count) and the treasure chest.
int CardsValues(Composition& c, int y, const CardsState& k, const RomData& rom, const u32* bottom)
{
    const int x = kMgInnerX, w = kMgInnerW, head = 24, row = 34, last = 40, h = 10 + head + 2 * row + 6 + last + 10;
    UiItem well = WellItem({x, y, w, h});
    c.ui.push_back(well);
    const int ix = x + 12, iw = w - 24, cx = ix + iw - 2 * kColW, sx = ix + iw - kColW;
    int yy = y + 10;
    Line(c, {cx, yy, kColW - 8, head}, Words(rom, kStrMgCardsCount), kMgHeadSize, 13, Align::Right, Weight::Bold, kHeadColour);
    Line(c, {sx, yy, kColW - 8, head}, Words(rom, kStrMgScore), kMgHeadSize, 13, Align::Right, Weight::Bold, kHeadColour);
    yy += head;
    const std::string names[2] = {k.name.empty() ? Words(rom, kStrMgPlayer) : k.name, Words(rom, kStrMgOpponent)};
    const int cards[2] = {k.hand, k.opponent}, scores[2] = {k.score, k.opponentScore};
    for (int i = 0; i < 2; i++, yy += row)
    {
        UiItem bar = PanelItem({ix - 4, yy + 2, iw + 8, row - 4});
        bar.plain = true;
        bar.fill = kRowFill;
        bar.radius = 8;
        bar.layer = 0;
        c.ui.push_back(bar);
        Line(c, {ix + 6, yy, cx - ix - 12, row}, names[i], kMgValueSize, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
        Line(c, {cx, yy, kColW - 8, row}, std::to_string(cards[i]), kMgValueSize, kMgValueSize, Align::Right, Weight::ExtraBold, 0xFFFFFFFF);
        Line(c, {sx, yy, kColW - 8, row}, std::to_string(scores[i]), kMgValueSize, kMgValueSize, Align::Right, Weight::ExtraBold, kValueColour);
    }
    yy += 6;
    // The deck: the game's own card back, a thumbnail, and how many are left.
    const int tw = 24, th = 35;
    // (Not while a box of the game's lies over it: the help, the question, the message.)
    if (bottom && !k.box) c.ui.push_back(ImageItem({ix + 4, yy + (last - th) / 2, tw, th}, bottom, SW, SH, kMgDeckFrom));
    Line(c, {ix + 4 + tw + 10, yy, 70, last}, std::to_string(k.deck), kMgValueSize, kMgValueSize, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    // The treasure chest's points.
    const int tx = ix + 4 + tw + 10 + 70;
    Line(c, {tx, yy, sx - tx - 4, last}, Words(rom, kStrMgTreasure), kMgHeadSize + 2, 13, Align::Right, Weight::Bold, kHeadColour);
    Line(c, {sx, yy, kColW - 8, last}, std::to_string(k.treasure), kMgValueSize, kMgValueSize, Align::Right, Weight::ExtraBold, kValueColour);
    return h;
}

int EarningsValues(Composition& c, int y, int index, int amount, const RomData& rom);

// The vacuum's values: the stat panel's "Vacuum Earnings: $@1", label and value.
int VacuumValues(Composition& c, int y, const VacuumState& v, const RomData& rom) { return EarningsValues(c, y, 2, v.earnings, rom); }

int EarningsValues(Composition& c, int y, int index, int amount, const RomData& rom)
{
    const int x = kMgInnerX, w = kMgInnerW;
    StatState st;
    (index == 3 ? st.metal : st.vacuum) = amount;
    std::string label, value;
    StatStatsLine(st, rom, index, label, value);
    const FittedText vf = FitLine(value, kMgValueSize, kMgValueMin, w / 2, Weight::ExtraBold);
    const int vw = (int)std::ceil(TextWidth(vf.text, vf.size, Weight::ExtraBold)) + 2;
    // The label beside the value; when it does not fit there (Italian's long one), above it.
    const bool beside = FitLine(label, kMgValueSize - 2, kMgValueMin, w - 28 - vw - 10).fits;
    const int h = beside ? 48 : 80;
    c.ui.push_back(WellItem({x, y, w, h}));
    if (beside) Line(c, {x + 14, y, w - 28 - vw - 10, h}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    else Line(c, {x + 14, y + 6, w - 28, 34}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    Line(c, {x + w - 14 - vw, beside ? y : y + 40, vw, beside ? h : 34}, vf.text, vf.size, vf.size, Align::Right, Weight::ExtraBold, kValueColour);
    return h;
}

// A meter: a well, its track filled frac in colour (the game's own bar colours).
constexpr u32 kMeterGreen = 0xFF38BA38, kMeterYellow = 0xFFDBA230, kMeterRed = 0xFFEB0000;
constexpr u32 kGoodColour = 0xFFB4F0A8;   // a word the game would light green (freed)
constexpr u32 kBadColour = 0xFFFF9A90;    // a red word (destroyed)
void Meter(Composition& c, Rect r, float frac, u32 colour)
{
    c.ui.push_back(WellItem(r));
    const int track = r.w - 6, w = (int)std::lround(track * std::clamp(frac, 0.0f, 1.0f));
    if (w <= 0) return;
    UiItem f = PanelItem({r.x + 3, r.y + 3, std::max(w, r.h - 6), r.h - 6});
    f.plain = true;
    f.fill = colour;
    f.radius = (r.h - 6) / 2.0f;
    f.layer = 1;
    c.ui.push_back(f);
}

// A health's colour: green, yellow under two thirds, red under a third.
u32 Health(float f) { return f > 2 / 3.0f ? kMeterGreen : f > 1 / 3.0f ? kMeterYellow : kMeterRed; }

constexpr int kMgRow = 30;        // a values' row
constexpr int kMgLabelW = 118;    // the label's column before a meter

// One row: the label at the left, a meter (frac >= 0) after it, the value at the right.
void MeterRow(Composition& c, int x, int y, int w, const std::string& label, float frac, u32 colour, const std::string& value, u32 valueColour)
{
    const int vw = 96;
    Line(c, {x, y, kMgLabelW - 8, kMgRow}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    if (frac >= 0) Meter(c, {x + kMgLabelW, y + 7, w - kMgLabelW - vw - 8, kMgRow - 14}, frac, colour);
    Line(c, {x + w - vw, y, vw, kMgRow}, value, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold, valueColour);
}

// A percentage as shown.
std::string Percent(long long n, long long of) { return std::to_string(of > 0 ? (int)std::lround(100.0 * std::clamp(n, 0LL, of) / of) : 0) + "%"; }

// The autopsy's values: the specimen and the money banked ($@1, the game's box), each organ's
// health (or removed / destroyed; freed: the hand can lift it) and the parasites.
int AutopsyValues(Composition& c, int y, const AutopsyState& a, const RomData& rom)
{
    const AutopsyWords& w = kAutopsyWords;
    const int organs = a.specimen == 0 ? 3 : 2, h = 10 + (2 + organs) * kMgRow + 10;
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    Line(c, {ix, yy, iw - 110, kMgRow}, With(w.specimen, a.specimen + 1), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    Line(c, {ix + iw - 110, yy, 110, kMgRow}, MenuWords(rom, kStrMgMoney, a.earnings), kMgValueSize, kMgValueMin, Align::Right, Weight::ExtraBold, kValueColour);
    yy += kMgRow;
    for (int i = 0; i < organs; i++, yy += kMgRow)
    {
        const AutopsyOrgan& o = a.organs[i];
        const float f = 1.0f - std::clamp(o.damage, 0, kAutopsyDestroyed) / (float)kAutopsyDestroyed;
        if (!o.present()) MeterRow(c, ix, yy, iw, With(w.organ, i + 1), -1, 0, w.removed, kHeadColour);
        else if (o.damage >= kAutopsyDestroyed) MeterRow(c, ix, yy, iw, With(w.organ, i + 1), 0, 0, w.destroyed, kBadColour);
        else MeterRow(c, ix, yy, iw, With(w.organ, i + 1), f, Health(f), o.freed ? w.freed : Percent((long long)(f * 1000), 1000), o.freed ? kGoodColour : 0xFFFFFFFF);
    }
    int eating = 0;
    for (const AutopsyParasite& p : a.parasites) eating += p.eating;
    const std::string pv = std::to_string(a.parasites.size()) + (eating ? " (" + With(w.eating, eating) + ")" : "");
    Line(c, {ix, yy, kMgLabelW + 20, kMgRow}, w.parasites, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    Line(c, {ix + kMgLabelW + 20, yy, iw - kMgLabelW - 20, kMgRow}, pv, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold,
         eating ? kBadColour : 0xFFFFFFFF);
    return h;
}

// A find's name as the game's card shows it (its two lines, "¹" an empty one), or empty.
std::string FindName(const RomData& rom, int type)
{
    if (type < 0 || type >= kMgFindTypes) return "";
    std::string a = Words(rom, kStrMgFind0 + 2 * type), b = Words(rom, kStrMgFind0 + 2 * type + 1);
    auto empty = [](const std::string& s) { return s.empty() || s == "\xC2\xB9"; };
    if (empty(a)) return empty(b) ? "" : b;
    return empty(b) ? a : a + " " + b;
}

// A stat panel line ("Vacuum Earnings: $@1", "Metal Detector Earnings: $@1"): the label beside
// its value, or above it when it does not fit there.
int EarningsValues(Composition& c, int y, int index, int amount, const RomData& rom);

// The detector's values: the charge (the game's gauge), the signal (a find in range: how well
// the depth matches, the scope's calm), the depth (the slider), what the card found, the earnings.
int DetectorValues(Composition& c, int y, const DetectorState& d, const RomData& rom)
{
    const DetectorWords& w = kDetectorWords;
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, h = 10 + 4 * kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    const float charge = std::clamp(d.charge, 0, kDetectorFull) / (float)kDetectorFull;
    MeterRow(c, ix, yy, iw, w.charge, charge, Health(charge), Percent(d.charge, kDetectorFull), 0xFFFFFFFF);
    yy += kMgRow;
    const float signal = d.inRange ? 1.0f - std::clamp(d.mismatch, 0, 128) / 128.0f : 0.0f;
    MeterRow(c, ix, yy, iw, w.signal, d.inRange ? signal : 0, kMeterGreen, d.inRange ? Percent(128 - std::clamp(d.mismatch, 0, 128), 128) : w.none,
             d.inRange ? kGoodColour : kHeadColour);
    yy += kMgRow;
    MeterRow(c, ix, yy, iw, w.depth, std::clamp(d.slider, 0, kDetectorSliderMax) / (float)kDetectorSliderMax, 0xFFB2B2E3,
             Percent(d.slider, kDetectorSliderMax), 0xFFFFFFFF);
    yy += kMgRow;
    // The game's card: "Found:" and the find (while it is up).
    const std::string found = d.card ? FindName(rom, d.findType) : "";
    Line(c, {ix, yy, kMgLabelW - 8, kMgRow}, Words(rom, kStrMgFound), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    Line(c, {ix + kMgLabelW, yy, iw - kMgLabelW, kMgRow}, found.empty() ? "\xE2\x80\x94" : found, kMgValueSize - 2, kMgValueMin, Align::Right,
         Weight::ExtraBold, found.empty() ? kHeadColour : kValueColour);
    return h + 8 + EarningsValues(c, y + h + 8, 3, d.earnings, rom);
}

// The painting's values: the tool (and its size), the colour (a swatch of it, from the page's
// colours) and the palette page.
int ArtValues(Composition& c, int y, const ArtState& a, const RomData& rom)
{
    const ArtWords& w = kArtWords;
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, h = 10 + 3 * kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    std::string tool = w.tools[std::clamp(a.tool, 0, 5)];
    if (a.tool == 0 || a.tool == 1) tool += std::string(" (") + w.sizes[std::clamp(a.brush, 0, 2)] + ")";
    Line(c, {ix, yy, 80, kMgRow}, w.tool, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    Line(c, {ix + 80, yy, iw - 80, kMgRow}, tool, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold, kValueColour);
    yy += kMgRow;
    // The colour: the page's colour swatch + 1 (resource 6028: 16 pages of 16).
    std::vector<u8> res = rom.Resource(kResArtColours), raw;
    if (Decompress(res, 0, raw)) res = raw;
    std::vector<u32> pal;
    const int index = std::clamp(a.page, 0, 15) * 16 + std::clamp(a.swatch, 0, 14) + 1;
    int sx = ix;
    if (ReadPalette(res, pal) && index < (int)pal.size())
    {
        UiItem sw = PanelItem({ix, yy + 3, 40, kMgRow - 6});
        sw.fill = pal[index] | 0xFF000000;
        sw.borderArgb = 0xFFFFFFFF;
        sw.border = 2;
        sw.radius = 6;
        sw.raised = false;
        sw.layer = 1;
        c.ui.push_back(sw);
        sx = ix + 52;
    }
    Line(c, {sx, yy, ix + iw - sx, kMgRow}, With(w.colourOf, std::clamp(a.swatch, 0, 14) + 1), kMgValueSize - 2, kMgValueMin, Align::Left,
         Weight::ExtraBold, 0xFFFFFFFF);
    yy += kMgRow;
    Line(c, {ix, yy, iw, kMgRow}, With(w.page, std::clamp(a.page, 0, 15) + 1), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::ExtraBold,
         0xFFFFFFFF);
    return h;
}

// The pointer over the playfield: a ring with a dot at its point, gold while the stylus is down.
void AddPointer(Composition& c, const PointerView& p)
{
    const float cx = kActivityBottomTo.x + (std::clamp(p.x, 0.0f, (float)(SW - 1)) + 0.5f) * kActivityBottomTo.w / SW;
    const float cy = kActivityBottomTo.y + (std::clamp(p.y, 0.0f, (float)(SH - 1)) + 0.5f) * kActivityBottomTo.h / SH;
    const u32 col = p.down ? 0xFFFFD75E : 0xFFFFFFFF;
    const int r = 15, x = (int)std::lround(cx), y = (int)std::lround(cy);
    UiItem ring = PanelItem({x - r, y - r, 2 * r, 2 * r});
    ring.fill = 0;
    ring.borderArgb = col;
    ring.border = 3;
    ring.radius = (float)r;
    ring.raised = false;
    ring.layer = kMgPointerLayer;
    c.ui.push_back(ring);
    UiItem dot = PanelItem({x - 2, y - 2, 4, 4});
    dot.plain = true;
    dot.fill = col;
    dot.radius = 2;
    dot.layer = kMgPointerLayer;
    c.ui.push_back(dot);
}

// The social answers (the side panel): "match the gesture", then the D-pad's direction and the
// game's own label for each (a live crop 2x).
int SocialAnswers(Composition& c, int y, const u32* bottom)
{
    const int x = kMgInnerX, w = kMgInnerW;
    Line(c, {x + 4, y, w - 8, 34}, "Match the gesture", kMgValueSize, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    int yy = y + 38;
    const Button dir[3] = {Button::DPadL, Button::DPadU, Button::DPadR};
    for (int i = 0; i < 3; i++, yy += kMgAnswerRow)
    {
        const Rect r = ChipRect(0, 0, dir[i], "", kMgChipBig);
        UiItem a = ChipItem(x + 10, yy + (kMgAnswerRow - r.h) / 2, dir[i], "", kMgChipBig);
        a.layer = 1;
        c.ui.push_back(a);
        const Rect f = kMgAnswerFrom[i];
        if (bottom) c.ui.push_back(ImageItem({x + 10 + r.w + 18, yy + (kMgAnswerRow - 2 * f.h) / 2, 2 * f.w, 2 * f.h}, bottom, SW, SH, f));
    }
    return yy - y;
}

// One hint's width at `size` (its chips, separators, label).
int HintWidth(const HdHint& h, float size)
{
    float w = 0;
    for (const Token& t : h.tokens) w += (t.chip ? ChipRect(0, 0, t.b, "", size).w : std::ceil(TextWidth(t.sep, size, Weight::ExtraBold))) + 6;
    if (!h.text.empty()) w += 4 + std::ceil(TextWidth(h.text, size, Weight::ExtraBold));
    return (int)w;
}

// The prompt bar under the playfield, across the frame: the hints in a row, centred, shrunk to fit.
void AddPrompts(Composition& c, const std::vector<HdHint>& hints)
{
    if (hints.empty()) return;
    c.ui.push_back(PanelItem(kMgPrompt));
    const int avail = kMgPrompt.w - 2 * kMgPromptPad;
    float size = kMgHintSize;
    auto total = [&](float sz) {
        int t = 0;
        for (const HdHint& h : hints) t += HintWidth(h, sz);
        return t + (int)(hints.size() - 1) * kMgPromptGap;
    };
    while (size > kMgHintMin && total(size) > avail) size -= 1;
    int x = kMgPrompt.x + kMgPromptPad + std::max(0, (avail - total(size)) / 2);
    const int y = kMgPrompt.y, h = kMgPrompt.h;
    for (const HdHint& hint : hints)
    {
        const int hw = HintWidth(hint, size);
        if (hint.lit)
        {
            UiItem plate = PanelItem({x - 10, y + 6, hw + 20, h - 12});
            plate.plain = true;
            plate.fill = kLitFill;
            plate.radius = (h - 12) / 2.0f;
            plate.layer = 1;
            c.ui.push_back(plate);
        }
        for (const Token& t : hint.tokens)
        {
            if (t.chip)
            {
                const Rect r = ChipRect(0, 0, t.b, "", size);
                UiItem it = ChipItem(x, y + (h - r.h) / 2, t.b, "", size);
                it.layer = 1;
                c.ui.push_back(it);
                x += r.w + 6;
            }
            else
            {
                const int sw = (int)std::ceil(TextWidth(t.sep, size, Weight::ExtraBold));
                Line(c, {x, y, sw + 2, h}, t.sep, size, size, Align::Left, Weight::ExtraBold, kHeadColour);
                x += sw + 6;
            }
        }
        if (!hint.text.empty())
        {
            const int tw = (int)std::ceil(TextWidth(hint.text, size, Weight::ExtraBold));
            Line(c, {x + 4, y, tw + 2, h}, hint.text, size, size, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
            x += 4 + tw;
        }
        x += kMgPromptGap;
    }
}
}

ActivityRam MinigameRam(const GameState* game)
{
    ActivityRam r;
    if (!game || !game->valid) return r;
    const MinigameState& m = game->minigame;
    if (game->screen == GScreen::Cards && m.cards.valid) { r.cards = true; r.quitAsk = m.cards.quitAsk; r.message = m.cards.message; }
    if (game->screen == GScreen::Autopsy && m.autopsy.valid) { r.autopsy = true; r.autopsyTool = m.autopsy.tool; r.autopsySub = m.autopsy.sub; }
    if (game->screen == GScreen::Detector && m.detector.valid) { r.detector = true; r.detectorSlider = m.detector.slider; }
    if (game->screen == GScreen::ArtGallery && m.art.valid) { r.art = true; r.artButton = m.art.button; r.artSwatch = m.art.swatch; }
    const PopupState& p = game->town.popup;
    if (p.valid && p.state == 2 && p.kind == 2 && PopupButton(p, 1, r.askYes.x, r.askYes.y) && PopupButton(p, 0, r.askNo.x, r.askNo.y))
        r.ask = true;
    return r;
}

bool MinigameFramed(ScreenState s, const GameState* game, const RomData* rom)
{
    return IsActivity(s.screen) && game && game->valid && rom &&
           rom->StringCount() > kStrMgMax;
}

void BuildMinigame(Composition& c, ScreenState s, int topW, int topH, const HudState* hud, const HudStrings* strings, const GameState& game,
                   const RomData& rom, const u32* bottom, IconCache*, const PointerView* pointer)
{
    // The playfield and the secondary panel as milestone 1 placed them (the stylus's piece, the
    // checks' pixels): the touch screen 3x at the left, the top screen 384 wide at the right.
    const bool bar = hud && strings && strings->ok && s.screen != Screen::Cards;
    const int rows = bar ? HudCropRows(topH) : topH;
    const int h = (int)std::lround((double)kActivityTopW * rows / topW);
    const Rect topTo {kActivityTopX, kActivityTopY, kActivityTopW, h};
    c.overlays.push_back({{0, 0, kConsoleW, kConsoleH}, kPanelArgb, 0, 0, 0, 0});
    c.overlays.push_back({{kActivityBottomTo.x - 6, kActivityBottomTo.y - 6, kActivityBottomTo.w + 12, kActivityBottomTo.h + 12}, kFrameArgb, 14, 6, kOutlineArgb, 0});
    c.overlays.push_back({{topTo.x - 6, topTo.y - 6, topTo.w + 12, topTo.h + 12}, kFrameArgb, 14, 6, kOutlineArgb, 0});
    c.pieces.push_back({Src::Bottom, {0, 0, SW, SH}, kActivityBottomTo});
    c.pieces.push_back({Src::Top, {0, 0, topW, rows}, topTo});

    // The title plate, over the playfield's frame.
    if (const std::string title = Title(s.screen, rom); !title.empty())
    {
        const FittedText t = FitLine(title, kMgTitleSize, kMgTitleMin, kMgTitleMaxW - 48, Weight::ExtraBold);
        const int w = std::min(kMgTitleMaxW, (int)std::ceil(TextWidth(t.text, t.size, Weight::ExtraBold)) + 48);
        c.ui.push_back(PanelItem({kMgTitleX, kMgTitleY, w, kMgTitleH}));
        Line(c, {kMgTitleX + 24, kMgTitleY, w - 48, kMgTitleH}, t.text, t.size, t.size, Align::Left, Weight::ExtraBold, 0xFFFFFFFF, 0);
    }

    int y = topTo.y + topTo.h + 6;
    if (bar)
    {
        // (Its place kept when a frame's values do not read and the bar is not drawn: no jump.)
        AddStatusBar(c, *hud, *strings, topTo.x - 6, topTo.y + topTo.h + kActivityBarGap, topTo.w + 12);
        y = topTo.y + topTo.h + kActivityBarGap + kStatusBarH;
    }
    // The side panel: the game's values from RAM (as tall as they are; none: no panel).
    y += kMgSideGap;
    const size_t panel = c.ui.size();
    c.ui.push_back(PanelItem({kMgSideX, y, kMgSideW, 0}));
    int content = 0;
    if (s.screen == Screen::Cards && game.minigame.cards.valid) content = CardsValues(c, y + kMgPad, game.minigame.cards, rom, bottom);
    else if (s.screen == Screen::Vacuum && game.minigame.vacuum.valid) content = VacuumValues(c, y + kMgPad, game.minigame.vacuum, rom);
    else if (s.screen == Screen::Social) content = SocialAnswers(c, y + kMgPad, bottom);
    else if (s.screen == Screen::Autopsy && game.minigame.autopsy.valid) content = AutopsyValues(c, y + kMgPad, game.minigame.autopsy, rom);
    else if (s.screen == Screen::Detector && game.minigame.detector.valid) content = DetectorValues(c, y + kMgPad, game.minigame.detector, rom);
    else if (s.screen == Screen::Art && game.minigame.art.valid) content = ArtValues(c, y + kMgPad, game.minigame.art, rom);
    if (content > 0) c.ui[panel].to.h = std::min(content + 2 * kMgPad, kMgSideBottom - y);
    else c.ui.erase(c.ui.begin() + (long)panel);
    // The prompt bar: the controller's hints.
    AddPrompts(c, Hints(s.screen, game, rom, pointer && pointer->quitArmed));
    if (pointer && pointer->shown && IsPointing(s.screen)) AddPointer(c, *pointer);
    SortUi(c.ui);
}

void MeasureMinigames(const RomData& rom, BlockMeasure& m)
{
    const Measurer me {m};
    for (int id : {kStrMgCards, kStrMgVacuum, kStrMgAutopsy, kStrMgDetector, kStrMgArtGallery})
        me.Line(id, Words(rom, id), kMgTitleSize, kMgTitleMin, kMgTitleMaxW - 48);
    // The pointing games: the autopsy's money (the reader's widest), the detector's "Found:" and
    // every find's name beside it, its earnings' line; every prompt bar fitting at its smallest.
    me.Line(kStrMgMoney, MenuWords(rom, kStrMgMoney, 99999999), kMgValueSize, kMgValueMin, 110);
    me.Line(kStrMgFound, Words(rom, kStrMgFound), kMgValueSize - 2, kMgValueMin, kMgLabelW - 8, Weight::Bold);
    for (int t = 0; t < kMgFindTypes; t++) me.Line(kStrMgFind0 + 2 * t, FindName(rom, t), kMgValueSize - 2, kMgValueMin, kMgInnerW - 24 - kMgLabelW);
    {
        StatState mt;
        mt.metal = 99999999;
        std::string label, value;
        StatStatsLine(mt, rom, 3, label, value);
        const int vw = (int)std::ceil(TextWidth(value, kMgValueSize, Weight::ExtraBold)) + 2;
        me.Line(kStrMgDetector, value, kMgValueSize, kMgValueMin, kMgInnerW / 2, Weight::ExtraBold);
        if (!FitLine(label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28 - std::min(vw, kMgInnerW / 2) - 10).fits)
            me.Line(kStrMgDetector, label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28, Weight::Bold);
        else m.checked++;
    }
    {
        GameState g;
        g.minigame.autopsy.valid = g.minigame.detector.valid = g.minigame.art.valid = true;
        g.minigame.autopsy.handAlert = g.minigame.autopsy.tweezersAlert = true;
        GameState ask = g;
        ask.town.popup.valid = true;
        ask.town.popup.state = 2;
        ask.town.popup.kind = 2;
        const int avail = kMgPrompt.w - 2 * kMgPromptPad;
        for (const std::vector<HdHint>& hs : {Hints(Screen::Autopsy, g, rom), Hints(Screen::Autopsy, g, rom, true), Hints(Screen::Detector, g, rom),
                                              Hints(Screen::Art, g, rom), Hints(Screen::Art, ask, rom)})
        {
            int t = (int)(hs.size() - 1) * kMgPromptGap;
            for (const HdHint& h : hs) t += HintWidth(h, kMgHintMin);
            me.Note(kStrMgYes, false, t <= avail);
        }
    }
    for (int id : {kStrMgCardsCount, kStrMgScore}) me.Line(id, Words(rom, id), kMgHeadSize, 13, kColW - 8, Weight::Bold);
    // The names' column (the player's own name is at most 7 letters: the game's keyboard).
    const int nameW = kMgInnerW - 24 - 2 * kColW - 12;
    for (int id : {kStrMgPlayer, kStrMgOpponent}) me.Line(id, Words(rom, id), kMgValueSize, kMgValueMin, nameW);
    me.Line(-1, "WWWWWWW", kMgValueSize, kMgValueMin, nameW);
    // The treasure's label between the deck's count and the score column.
    const int tx = 12 + 4 + 24 + 10 + 70;
    me.Line(kStrMgTreasure, Words(rom, kStrMgTreasure), kMgHeadSize + 2, 13, kMgInnerW - 24 - kColW - tx - 4, Weight::Bold);
    // "Vacuum Earnings:" beside the widest value the reader lets through.
    StatState st;
    st.vacuum = 99999999;
    std::string label, value;
    StatStatsLine(st, rom, 2, label, value);
    const int vw = (int)std::ceil(TextWidth(value, kMgValueSize, Weight::ExtraBold)) + 2;
    me.Line(kStrMgVacuumEarnings, value, kMgValueSize, kMgValueMin, kMgInnerW / 2, Weight::ExtraBold);
    // (Beside the value, or above it on a line of its own.)
    if (!FitLine(label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28 - std::min(vw, kMgInnerW / 2) - 10).fits)
        me.Line(kStrMgVacuumEarnings, label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28, Weight::Bold);
    else m.checked++;
    // The hints in the game's words.
    // The cards' prompts with the game's "Quit", and its question's "Yes" / "No", in the bar at full size.
    const int avail = kMgPrompt.w - 2 * kMgPromptPad;
    GameState ask;
    ask.minigame.cards.valid = ask.minigame.cards.quitAsk = true;
    for (const std::vector<HdHint>& hs : {Hints(Screen::Cards, GameState {}, rom), Hints(Screen::Cards, ask, rom)})
    {
        int t = (int)(hs.size() - 1) * kMgPromptGap;
        for (const HdHint& h : hs) t += HintWidth(h, kMgHintSize);
        me.Note(kStrMgQuit, false, t <= avail);
    }
}
}
