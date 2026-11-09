#include "port/prompt.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "port/interact.h"
#include "port/ownwords.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/font.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

constexpr int kPad = 22, kGap = 28, kChipGap = 12;
constexpr int kNameY = 8, kNameH = 30, kVerbY = 40, kVerbH = 42;
constexpr int kHintMaxW = 340;               // a hint chip with its words, at most
// The target's name and the hints in white: on the panel's fill (#828ACB, white 3.26:1) only text
// at 18.67 px bold or larger reads (kPromptNameMin, kPromptHintSize).
constexpr u32 kNameColour = 0xFFFFFFFF;
constexpr u32 kHintColour = 0xFFFFFFFF;

bool HasStrings(const RomData* rom) { return rom && rom->StringCount() > 0; }

struct Hint { Button button; std::string label; };
std::vector<Hint> HintsOf(const WorldPrompt& p, const RomData* rom)
{
    std::vector<Hint> h;
    if (p.more) h.push_back(p.held ? Hint {Button::DPadV, OwnWord(rom, OwnWordId::HintChoose)} : Hint {Button::A, OwnWord(rom, OwnWordId::PromptMore)});
    // (Left / Right switch targets with A held; without it the D-pad walks the hero.)
    if (p.others && p.held) h.push_back({Button::DPadH, OwnWord(rom, OwnWordId::PromptOther)});
    return h;
}

struct PromptLayout
{
    Rect plate;
    FittedText name, verb;
    Rect chip;
    std::vector<Hint> hints;
    std::vector<Rect> hintRects;
    int leftW = 0, hintsW = 0;
};
PromptLayout LayOut(const WorldPrompt& p, const RomData* rom)
{
    PromptLayout l;
    l.name = FitLine(p.name, kPromptNameSize, kPromptNameMin, kPromptNameW, Weight::Bold);
    l.verb = FitLine(p.verb, kPromptVerbSize, kPromptVerbMin, kPromptVerbW, Weight::ExtraBold);
    l.chip = ChipRect(0, 0, Button::A, "", kPromptVerbSize);
    const int nameW = l.name.text.empty() ? 0 : (int)std::ceil(TextWidth(l.name.text, l.name.size, Weight::Bold));
    const int verbW = l.verb.text.empty() ? 0 : (int)std::ceil(TextWidth(l.verb.text, l.verb.size, Weight::ExtraBold));
    l.hints = HintsOf(p, rom);
    int hintsW = 0;
    for (const Hint& h : l.hints)
    {
        const Rect r = ChipRect(0, 0, h.button, h.label, kPromptHintSize);
        l.hintRects.push_back(r);
        hintsW = std::max(hintsW, std::min(r.w, kHintMaxW));
    }
    l.hintsW = hintsW;
    l.leftW = std::min(std::max(nameW, l.chip.w + kChipGap + verbW), kPromptMaxW - 2 * kPad - (hintsW > 0 ? kGap + hintsW : 0));
    const int w = kPad + l.leftW + (hintsW > 0 ? kGap + hintsW : 0) + kPad;
    l.plate = {(1280 - w) / 2, kPromptBottom - kPromptH, w, kPromptH};
    return l;
}
}

int PromptSteady::Update(const InteractState* in)
{
    if (!in || !in->valid || in->target.kind == TargetRef::None)
    {
        *this = PromptSteady {};
        return 0;
    }
    if (had && in->target == target && in->actions == actions) frames = std::min(frames + 1, 1 << 20);
    else
    {
        target = in->target;
        actions = in->actions;
        had = true;
        frames = 1;
    }
    return frames;
}

WorldPrompt WorldPromptOf(const InteractState& in, const RomData* rom)
{
    WorldPrompt p;
    if (!in.valid || in.target.kind == TargetRef::None || in.padMode == 2) return p;
    int enabled = 0, first = -1;
    for (int i = 0; i < (int)in.actions.size(); i++)
        if (!in.actions[i].greyed) { enabled++; if (first < 0) first = i; }
    if (enabled == 0) return p;
    p.held = in.padMode == 0;
    // Pad mode (A held): the row the game has lit is what the release runs.
    int row = first;
    if (p.held && in.highlighted >= 0 && in.highlighted < (int)in.actions.size() && !in.actions[in.highlighted].greyed) row = in.highlighted;
    p.shown = true;
    p.more = enabled > 1;
    p.others = in.prev.kind != TargetRef::None || in.next.kind != TargetRef::None;
    if (HasStrings(rom))
    {
        p.verb = InteractLabel(*rom, in.actions[row].labelId, in.actions[row].amount);
        if (in.target.nameId >= 0) p.name = MenuWords(*rom, in.target.nameId);
    }
    return p;
}

bool PromptUp(ScreenState s, const GameState& game, const PromptSteady& steady)
{
    const InteractState& in = game.town.interact;
    return s.screen == Screen::Panel && s.tab == Tab::Interact && game.valid && game.screen == GScreen::Panel && game.tab == 0 &&
           game.settled && in.valid && steady.had && steady.frames >= kPromptSteadyFrames && steady.target == in.target &&
           steady.actions == in.actions;
}

void DropWorldPrompt(Composition& c)
{
    std::erase_if(c.ui, [](const UiItem& it) { return it.kind != UiItem::Image && it.layer == kPromptLayer; });
}

Rect WorldPromptRect(const WorldPrompt& p, const RomData* rom)
{
    if (!p.shown) return {};
    return LayOut(p, rom).plate;
}

void BuildWorldPrompt(Composition& c, const WorldPrompt& p, const RomData* rom)
{
    if (!p.shown) return;
    const PromptLayout l = LayOut(p, rom);
    const size_t first = c.ui.size();
    c.ui.push_back(PanelItem(l.plate));
    const int x0 = l.plate.x + kPad, y0 = l.plate.y;
    const int leftW = l.leftW;
    if (!l.name.text.empty())
    {
        UiItem name = TextItem({x0, y0 + kNameY, leftW, kNameH}, l.name.text, l.name.size, Align::Left, Weight::Bold);
        name.wrap = false;
        name.middle = true;
        name.colour = kNameColour;
        c.ui.push_back(name);
    }
    const int chipY = y0 + kVerbY + (kVerbH - l.chip.h) / 2;
    c.ui.push_back(ChipItem(x0, chipY, Button::A, "", kPromptVerbSize));
    const int vx = x0 + l.chip.w + kChipGap;
    UiItem verb = TextItem({vx, y0 + kVerbY, std::max(0, x0 + leftW - vx), kVerbH}, l.verb.text, l.verb.size, Align::Left, Weight::ExtraBold);
    verb.wrap = false;
    verb.middle = true;
    c.ui.push_back(verb);
    if (!l.hints.empty())
    {
        const int hx = x0 + leftW + kGap;
        // One hint: level with the verb; two: one a row.
        const int rows = (int)l.hints.size();
        for (int i = 0; i < rows; i++)
        {
            const Rect r = l.hintRects[i];
            const int rowY = rows == 1 ? y0 + kVerbY + (kVerbH - r.h) / 2 : y0 + 10 + i * 36 + (34 - r.h) / 2;
            UiItem chip = ChipItem(hx, rowY, l.hints[i].button, l.hints[i].label, kPromptHintSize);
            chip.colour = kHintColour;
            c.ui.push_back(chip);
        }
    }
    for (size_t i = first; i < c.ui.size(); i++) c.ui[i].layer = kPromptLayer;
}

void AddWorldPrompt(Composition& c, ScreenState s, const GameState& game, const RomData& rom, const PromptSteady& steady)
{
    if (!PromptUp(s, game, steady)) return;
    BuildWorldPrompt(c, WorldPromptOf(game.town.interact, &rom), &rom);
}

void MeasurePrompt(const RomData& rom, BlockMeasure& m)
{
    const Measurer me {m};
    auto unused = [&](int id) {
        const std::string t = rom.String(id);
        return t.empty() || (t.size() >= 3 && std::tolower((unsigned char)t[0]) == 'x' && std::tolower((unsigned char)t[1]) == 'x' &&
                             std::tolower((unsigned char)t[2]) == 'x') || t == "Dummy";
    };
    for (int id = 1845; id <= 1951; id++)
    {
        if (unused(id)) continue;
        const std::string label = InteractLabel(rom, id, rom.String(id).find("@1") != std::string::npos ? 99999999 : -1);
        me.Line(id, label, kPromptVerbSize, kPromptVerbMin, kPromptVerbW, Weight::ExtraBold);
    }
    std::vector<int> names {kSimNameUnknown};
    for (int id = kSimNameBase; id <= kSimNameLast; id++) names.push_back(id);
    for (int id = kItemNameBase; id <= kItemNameBase + kItemIdLast; id++) names.push_back(id);
    for (int id : names)
    {
        if (unused(id)) continue;
        // A name may end in an ellipsis (the Italian table's sentence at 463), as beside the strip's plate.
        const FittedText f = FitLine(MenuWords(rom, id), kPromptNameSize, kPromptNameMin, kPromptNameW, Weight::Bold);
        m.checked++;
        if (f.size < kPromptNameSize) m.shrunk++;
        if (!f.fits) m.namesCut++;
    }
    for (OwnWordId w : {OwnWordId::PromptMore, OwnWordId::HintChoose, OwnWordId::PromptOther})
        me.Note(9000 + (int)w, false, ChipRect(0, 0, Button::A, OwnWord(&rom, w), kPromptHintSize).w <= kHintMaxW);
}
}
