#include "port/minigame.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "port/menuview.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
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

// The prompt bar's hints: the scheme of activities.cpp, and what the game says now.
std::vector<HdHint> Hints(Screen s, const GameState& game, const RomData& rom)
{
    switch (s)
    {
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

// The vacuum's values: the stat panel's "Vacuum Earnings: $@1", label and value.
int VacuumValues(Composition& c, int y, const VacuumState& v, const RomData& rom)
{
    const int x = kMgInnerX, w = kMgInnerW;
    StatState st;
    st.vacuum = v.earnings;
    std::string label, value;
    StatStatsLine(st, rom, 2, label, value);
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

bool MinigameFramed(ScreenState s, const GameState* game, const RomData* rom)
{
    return (s.screen == Screen::Cards || s.screen == Screen::Vacuum || s.screen == Screen::Social) && game && game->valid && rom &&
           rom->StringCount() > kStrMgMax;
}

void BuildMinigame(Composition& c, ScreenState s, int topW, int topH, const HudState* hud, const HudStrings* strings, const GameState& game,
                   const RomData& rom, const u32* bottom)
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
    if (content > 0) c.ui[panel].to.h = std::min(content + 2 * kMgPad, kMgSideBottom - y);
    else c.ui.erase(c.ui.begin() + (long)panel);
    // The prompt bar: the controller's hints.
    AddPrompts(c, Hints(s.screen, game, rom));
    SortUi(c.ui);
}

void MeasureMinigames(const RomData& rom, BlockMeasure& m)
{
    const Measurer me {m};
    for (int id : {kStrMgCards, kStrMgVacuum}) me.Line(id, Words(rom, id), kMgTitleSize, kMgTitleMin, kMgTitleMaxW - 48);
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
