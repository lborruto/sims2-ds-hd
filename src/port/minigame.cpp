#include "port/minigame.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "hotspots.h"

#include "port/menuview.h"
#include "port/ownwords.h"
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
constexpr u32 kNowFill = 0xFF8C5FB8;       // MooGoo: the row of the player whose turn it is
constexpr u32 kMarkColour = 0xFF309A41;    // MooGoo's focus: the game's selection green
constexpr u32 kMooPlate = 0xFF5A2038;      // MooGoo's line: a plate in its table's maroon

// A DS rectangle of the playfield on the canvas (3x at kActivityBottomTo), grown by `grow` pixels.
Rect ToCanvas(Rect d, int grow)
{
    const Rect to = kActivityBottomTo;
    return {to.x + d.x * to.w / SW - grow, to.y + d.y * to.h / SH - grow, d.w * to.w / SW + 2 * grow, d.h * to.h / SH + 2 * grow};
}

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

// The port's own words, where the game's table has none (port/ownwords.*: in the game's
// language), gathered per game.
const char* Own(const RomData& rom, OwnWordId id, int k = 0) { return OwnWord(&rom, (OwnWordId)((int)id + k)); }
struct AutopsyWords
{
    const char *tools[3], *use[3], *carry[2], *tear, *quit, *quitAgain, *specimen, *organ, *freed, *removed, *destroyed, *parasites, *eating;
    explicit AutopsyWords(const RomData& r)
    {
        for (int i = 0; i < 3; i++) { tools[i] = Own(r, OwnWordId::AutopsyLaser, i); use[i] = Own(r, OwnWordId::AutopsyCut, i); }
        for (int i = 0; i < 2; i++) carry[i] = Own(r, OwnWordId::AutopsyToTray, i);
        tear = Own(r, OwnWordId::AutopsyTear);
        quit = Own(r, OwnWordId::AutopsyQuit);
        quitAgain = Own(r, OwnWordId::AutopsyQuitAgain);
        specimen = Own(r, OwnWordId::AutopsySpecimen);
        organ = Own(r, OwnWordId::AutopsyOrgan);
        freed = Own(r, OwnWordId::AutopsyFreed);
        removed = Own(r, OwnWordId::AutopsyRemoved);
        destroyed = Own(r, OwnWordId::AutopsyDestroyed);
        parasites = Own(r, OwnWordId::AutopsyParasites);
        eating = Own(r, OwnWordId::AutopsyEating);
    }
};
struct DetectorWords
{
    const char *walk, *dig, *depth, *powerOff, *charge, *signal, *none;
    explicit DetectorWords(const RomData& r)
        : walk(Own(r, OwnWordId::DetectorWalk)), dig(Own(r, OwnWordId::DetectorDig)), depth(Own(r, OwnWordId::DetectorDepth)),
          powerOff(Own(r, OwnWordId::DetectorPowerOff)), charge(Own(r, OwnWordId::DetectorCharge)), signal(Own(r, OwnWordId::DetectorSignal)),
          none(Own(r, OwnWordId::DetectorNone))
    {
    }
};
struct ArtWords
{
    const char *paint, *tool, *colour, *undo, *done, *tools[6], *sizes[3], *colourOf, *page;   // tools: the game's tool 0..5
    explicit ArtWords(const RomData& r)
        : paint(Own(r, OwnWordId::ArtPaint)), tool(Own(r, OwnWordId::ArtTool)), colour(Own(r, OwnWordId::ArtNextColour)), undo(Own(r, OwnWordId::ArtUndo)),
          done(Own(r, OwnWordId::ArtDone)), colourOf(Own(r, OwnWordId::ArtColourOf)), page(Own(r, OwnWordId::ArtPage))
    {
        for (int i = 0; i < 6; i++) tools[i] = Own(r, OwnWordId::ArtPencil, i);
        for (int i = 0; i < 3; i++) sizes[i] = Own(r, OwnWordId::ArtSmall, i);
    }
};

// A port word with its @1 filled.
std::string With(const char* w, int n)
{
    std::string s = w;
    if (const size_t at = s.find("@1"); at != std::string::npos) s.replace(at, 2, std::to_string(n));
    return s;
}

// A port word with its @1 a word of the game's.
std::string With(const char* w, const std::string& at1)
{
    std::string s = w;
    if (const size_t at = s.find("@1"); at != std::string::npos) s.replace(at, 2, at1);
    return s;
}

// The pointer's hints, shared by the pointing games: a stick (either) points, LT slows it.
HdHint PointHint(const RomData& rom) { return {{C(Button::LStick), Sep("/"), C(Button::RStick)}, Own(rom, OwnWordId::Point)}; }
HdHint SlowHint(const RomData& rom) { return {{C(Button::LT)}, Own(rom, OwnWordId::Slow)}; }

// The prompt bar's hints: the scheme of activities.cpp, and what the game says now. quitArmed:
// the autopsy's first B / Start (a second quits).
// A popup of the game's is up (opening, shown or closing: it holds the stylus).
bool GamePopup(const GameState& game) { const PopupState& p = game.town.popup; return p.valid && p.state >= 1 && p.state <= 3; }

bool CardsPlayerActs(int phase) { return phase == 0 || phase == 1 || phase == 2; }

// A question's hints (its box shows the answers): the D-pad between them, A the focused one (No
// first), B No, the mixer's X Preview.
std::vector<HdHint> AskHints(const RomData& rom, int askFocus, bool preview = false)
{
    std::vector<HdHint> h {{{C(Button::DPadH)}, ""}, {{C(Button::A)}, Words(rom, askFocus == 1 ? kStrMgYes : kStrMgNo)}, {{C(Button::B)}, Words(rom, kStrMgNo)}};
    if (preview) h.push_back({{C(Button::X)}, Words(rom, kStrMgMixPreview)});
    return h;
}

std::vector<HdHint> Hints(Screen s, const GameState& game, const RomData& rom, bool quitArmed = false, int focus = -1, int askFocus = -1,
                          bool taking = false)
{
    auto own = [&](OwnWordId w) { return std::string(Own(rom, w)); };
    switch (s)
    {
    case Screen::Scratch:
    {
        if (game.minigame.scratch.valid && GamePopup(game)) return {{{C(Button::A)}, Words(rom, kStrMgClose)}};
        if (quitArmed) return {{{C(Button::B)}, own(OwnWordId::ScratchQuitAgain), true}};
        return {PointHint(rom), SlowHint(rom), {{C(Button::A)}, own(OwnWordId::ScratchHold)}, {{C(Button::X)}, own(OwnWordId::ScratchRub)},
                {{C(Button::B)}, Words(rom, kStrMgQuit)}};
    }
    case Screen::Mixer:
    {
        const MixerState& m = game.minigame.mixer;
        const int phase = m.valid ? m.phase : 0;
        const HdHint touch {{C(Button::A)}, own(OwnWordId::MixerTouch)}, octave {{C(Button::DPadV)}, own(OwnWordId::MixerOctave)};
        if (phase == 0xA)
            return AskHints(rom, askFocus, true);
        if (phase == 7)
            return {{{C(Button::DPadV)}, own(OwnWordId::MixerChoose)}, {{C(Button::A)}, Words(rom, kStrMgMixSelect)}, {{C(Button::B)}, own(OwnWordId::MixerBack)}};
        // Help: the pointer and A pick the control the game explains (8); its text, A closes it (9).
        if (phase == 8) return {PointHint(rom), {{C(Button::A)}, own(OwnWordId::HelpWhat)}, {{C(Button::B)}, own(OwnWordId::HelpLeave)}};
        if (phase == 9) return {{{C(Button::A)}, own(OwnWordId::Close)}, {{C(Button::B)}, own(OwnWordId::HelpLeave)}};
        if (phase == 5) return {PointHint(rom), touch, {{C(Button::Y)}, own(OwnWordId::MixerVoxStop), true}};
        if (phase == 1) return {PointHint(rom), touch, {{C(Button::X), Sep("/"), C(Button::B)}, own(OwnWordId::MixerStop), true}, octave};
        if (phase == 2) return {PointHint(rom), touch, {{C(Button::RB), Sep("/"), C(Button::B)}, own(OwnWordId::MixerStop), true}, octave};
        // (Idle: every button has a use; the pointer and A as one hint, the D-pad's three as one.)
        return {{{C(Button::LStick), Sep("+"), C(Button::A)}, own(OwnWordId::MixerPoint)}, {{C(Button::X)}, Words(rom, kStrMgMixPlay)},
                {{C(Button::RB)}, Words(rom, kStrMgMixRecord)}, {{C(Button::Y)}, Words(rom, kStrMgMixVox)}, {{C(Button::LB)}, Words(rom, kStrMgMixGenre)},
                {{C(Button::Start)}, own(OwnWordId::MixerHelp)}, {{C(Button::B)}, Words(rom, kStrMgMixQuit)}};
        // (the D-pad's octave, sustain and PREP: the game's own keys; the octave's hint shows while it plays)
    }
    case Screen::MixerSave:
    {
        if (quitArmed) return {{{C(Button::A)}, own(OwnWordId::SaveOverwrite), true}, {{C(Button::DPad)}, own(OwnWordId::MixerChoose)}};
        return {{{C(Button::DPad)}, own(OwnWordId::MixerChoose)}, {{C(Button::A)}, own(SaveSlotAt(focus) >= 0 ? OwnWordId::SaveHere : OwnWordId::SaveType)},
                {{C(Button::Y)}, own(OwnWordId::SaveErase)}, {{C(Button::B)}, own(OwnWordId::MixerBack)}};
    }
    case Screen::Trading: return {{{C(Button::B)}, Words(rom, kStrMgExit)}};
    case Screen::Autopsy:
    {
        const AutopsyState& a = game.minigame.autopsy;
        const AutopsyWords w(rom);
        if (quitArmed) return {{{C(Button::B)}, w.quitAgain, true}};
        const int tool = a.valid ? std::clamp(a.tool, 0, 2) : 0;
        std::vector<HdHint> h {PointHint(rom), SlowHint(rom)};
        // What A does now: a freed organ or a parasite to take (the hand, the tweezers: activities.h), else held the tool.
        bool organ = false, parasite = false;
        for (int i = 0; a.valid && i < kAutopsyOrgans; i++) organ = organ || (a.organs[i].freed && a.organs[i].present());
        for (const AutopsyParasite& p : a.parasites) parasite = parasite || (p.x >= 0 && p.x < SW && p.y >= 0 && p.y < SH);
        if (a.valid && (a.phase == 5 || a.phase == 6)) h.push_back({{C(Button::A)}, w.tear});
        else if (a.valid && (a.sub == 3 || a.sub == 6)) h.push_back({{C(taking ? Button::A : Button::LB)}, w.carry[a.sub == 3 ? 0 : 1], true});
        else
        {
            const bool take = a.valid && a.sub == 0 && a.phase <= 2 && ((tool == 1 && organ) || (tool == 2 && parasite));
            h.push_back({{C(Button::A)}, take ? Own(rom, tool == 1 ? OwnWordId::AutopsyTakeOrgan : OwnWordId::AutopsyTakeParasite) : w.use[tool]});
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
        const DetectorWords w(rom);
        return {{{C(Button::LStick), Sep("/"), C(Button::DPad)}, w.walk},
                {{C(Button::A)}, w.dig, game.minigame.detector.valid && game.minigame.detector.inRange},
                {{C(Button::LB), C(Button::RB), Sep("/"), C(Button::RStick)}, w.depth},
                {{C(Button::B)}, w.powerOff}};
    }
    case Screen::Art:
    {
        const ArtWords w(rom);
        const PopupState& p = game.town.popup;
        if (p.valid && p.state >= 1 && p.state <= 3 && p.kind == 2) return AskHints(rom, askFocus);
        // (the pointer and A as one hint, as the mixer's: the bar holds eight hints in no language)
        return {{{C(Button::LStick), Sep("+"), C(Button::A)}, w.paint}, {{C(Button::LB), C(Button::RB)}, w.tool},
                {{C(Button::X)}, w.colour}, {{C(Button::Back)}, Own(rom, OwnWordId::ArtNextPalette)}, {{C(Button::Y)}, w.undo}, {{C(Button::B)}, w.done}};
    }
    case Screen::MooGoo:
    {
        const MooGooState& g = game.minigame.mooGoo;
        const HdHint quit {{C(Button::B)}, Words(rom, kStrMgQuit)}, startQuit {{C(Button::Start)}, Words(rom, kStrMgQuit)};
        const HdHint choose {{C(Button::DPadH)}, Own(rom, OwnWordId::MooGooChooseMonkey)};
        if (!g.valid) return {{{C(Button::A)}, Own(rom, OwnWordId::CardsContinue)}, quit};
        if (g.intro) return {{{C(Button::A)}, Own(rom, OwnWordId::MooGooPlay)}, quit};
        if (g.quitAsk) return AskHints(rom, askFocus);
        const bool human = !g.players[std::clamp(g.current, 0, kMooGooPlayers - 1)].ai;
        if (human && (g.phase == 6 || g.phase == 0xD))
        {
            if (g.sub == 1) return {choose, {{C(Button::A)}, Own(rom, OwnWordId::MooGooBet)}, quit};
            if (g.sub == 3)
                return {{{C(Button::DPadH)}, Own(rom, OwnWordId::CardsChoose)}, {{C(Button::A)}, Own(rom, OwnWordId::MooGooPlayCard)},
                        {{C(Button::B)}, Own(rom, OwnWordId::MooGooUndoBet)}, startQuit};
            if (g.sub == 5) return {choose, {{C(Button::A)}, Own(rom, OwnWordId::MooGooPlayOn)}, {{C(Button::B)}, Own(rom, OwnWordId::MooGooTakeBack)}, startQuit};
        }
        if (g.phase == 0) return {{{C(Button::DPadH)}, Own(rom, OwnWordId::MooGooPlayers)}, {{C(Button::A)}, Own(rom, OwnWordId::MooGooConfirm)}, quit};
        return {{{C(Button::A)}, Own(rom, OwnWordId::CardsContinue)}, quit};
    }
    case Screen::Drencher:
    {
        const DrencherState& d = game.minigame.drencher;
        const bool low = d.valid && DrencherLow(d.pressure);
        // Help (Start, the game's "?"): its text closes on any press; in its mode each button asks
        // the game what that button does, B leaves.
        if (d.valid && d.helpText) return {{{C(Button::A)}, Own(rom, OwnWordId::Close)}};
        if (d.valid && d.help)
            return {{{C(Button::A), C(Button::X), C(Button::Y), C(Button::RB)}, Own(rom, OwnWordId::HelpWhat)}, {{C(Button::B)}, Own(rom, OwnWordId::HelpLeave)}};
        return {{{C(Button::LStick)}, Own(rom, OwnWordId::DetectorWalk)},
                {{C(Button::A)}, Own(rom, OwnWordId::DrencherFire)},
                {{C(Button::X)}, Own(rom, OwnWordId::DrencherPump), low},
                {{C(Button::Y)}, Own(rom, OwnWordId::DrencherTarget)},
                {{C(Button::RB)}, Own(rom, OwnWordId::DrencherRefill)},
                {{C(Button::LB)}, Own(rom, OwnWordId::DrencherRun)},
                {{C(Button::Start)}, Own(rom, OwnWordId::MixerHelp)},
                {{C(Button::B)}, Own(rom, OwnWordId::DrencherPutAway)}};
    }
    case Screen::Crime:
    {
        const CrimeState& k = game.minigame.crime;
        // Help (Start, the game's "?"): as the Drencher's.
        if (k.valid && k.helpText) return {{{C(Button::A)}, Own(rom, OwnWordId::Close)}};
        if (k.valid && k.help)
            return {{{C(Button::X), C(Button::A), C(Button::Y)}, Own(rom, OwnWordId::HelpWhat)}, {{C(Button::B)}, Own(rom, OwnWordId::HelpLeave)}};
        if (k.valid && (k.phase == 1 || k.phase == 2))
            return {{{C(Button::LStick), Sep("/"), C(Button::DPadH)}, Own(rom, OwnWordId::CrimeAimLabel)}, {{C(Button::A)}, Own(rom, OwnWordId::CrimeThrow), true}};
        return {{{C(Button::LStick), Sep("/"), C(Button::DPad)}, Own(rom, OwnWordId::DetectorWalk)},
                {{C(Button::X)}, Own(rom, OwnWordId::CrimeScare), k.valid && k.canScare},
                {{C(Button::A)}, Own(rom, OwnWordId::CrimeAim), k.valid && k.armed},
                {{C(Button::Y)}, Own(rom, OwnWordId::CrimeSubdue), k.valid && k.canSubdue},
                {{C(Button::LB)}, Own(rom, OwnWordId::DrencherRun)},
                {{C(Button::Start)}, Own(rom, OwnWordId::MixerHelp)},
                {{C(Button::B)}, Own(rom, OwnWordId::DrencherPutAway)}};
    }
    case Screen::Cards:
    {
        const CardsState& k = game.minigame.cards;
        if (k.valid && k.quitAsk) return AskHints(rom, askFocus);
        if (k.valid && k.message) return {{{C(Button::A)}, Own(rom, OwnWordId::CardsContinue)}};
        const HdHint quit {{C(Button::B)}, Words(rom, kStrMgQuit)};
        // The turn's step (what the game's line says): draw, then play or discard; the opponent's
        // turn and the game's animations take nothing but the quit.
        if (k.valid && (k.turn != 0 || !CardsPlayerActs(k.phase))) return {quit};
        if (k.valid && k.phase == 0) return {{{C(Button::X)}, Own(rom, OwnWordId::CardsDraw)}, quit};
        return {{{C(Button::DPad)}, Own(rom, OwnWordId::CardsChoose)}, {{C(Button::A)}, Own(rom, OwnWordId::CardsPickUp)}, {{C(Button::Y)}, Own(rom, OwnWordId::CardsDiscard)},
                quit};
    }
    case Screen::Vacuum:
    {
        const VacuumState& v = game.minigame.vacuum;
        const bool clog = v.valid && v.clogged;
        // A: what it does now (activities.h): take a find (one is in the bowl), its take running, nothing
        // (no find yet); the bowl unread: the stylus held at the hand.
        bool find = false;
        for (const VacuumItem& t : v.items) find = find || !t.Dust();
        const OwnWordId a = !v.bowl ? OwnWordId::VacuumGrab : taking ? OwnWordId::VacuumTaking : find ? OwnWordId::VacuumTake : OwnWordId::VacuumNoFind;
        return {{{C(Button::LStick)}, Own(rom, OwnWordId::VacuumDrive)}, {{C(Button::RStick)}, Own(rom, OwnWordId::VacuumHand)},
                {{C(Button::A)}, Own(rom, a)}, {{C(Button::Y)}, Own(rom, clog ? OwnWordId::VacuumBlowNow : OwnWordId::VacuumBlow), clog},
                {{C(Button::B)}, Own(rom, OwnWordId::DrencherPutAway)}};
    }
    case Screen::Social:
        return {{{C(Button::DPadL), C(Button::DPadU), C(Button::DPadR)}, Own(rom, OwnWordId::SocialAnswer)}, {{C(Button::A)}, Own(rom, OwnWordId::SocialAgain)}};
    default: return {};
    }
}

std::string Title(Screen s, const GameState& game, const RomData& rom)
{
    if (s == Screen::Cards) return Words(rom, kStrMgCards);
    if (s == Screen::Vacuum) return Words(rom, kStrMgVacuum);
    if (s == Screen::Autopsy) return Words(rom, kStrMgAutopsy);
    if (s == Screen::Detector) return Words(rom, kStrMgDetector);
    if (s == Screen::Art) return Words(rom, kStrMgArtGallery);
    if (s == Screen::MooGoo) return Words(rom, kStrMgMooGoo);
    if (s == Screen::Drencher) return Words(rom, kStrMgDrencher);
    if (s == Screen::Crime) return Words(rom, kStrMgRatARang);
    if (s == Screen::Scratch) return Words(rom, kStrMgTicket0 + (game.minigame.scratch.valid ? std::clamp(game.minigame.scratch.tier, 0, 2) : 0));
    if (s == Screen::Mixer) return Words(rom, kStrMgMixer);
    if (s == Screen::MixerSave) return Words(rom, kStrMgBurn);
    if (s == Screen::Trading) return Words(rom, kStrMgWireless);
    return "";
}

// The board's columns in the cards' well: the name, its cards, its score (from the right).
constexpr int kColW = 78;

// The cards' values: the board (a head row, the player's and the opponent's rows: name, cards,
// score), then the deck (a thumbnail of the game's own, its count) and the treasure chest.
int CardsValues(Composition& c, int y, const CardsState& k, const RomData& rom, const u32* bottom)
{
    // The game's line (its top screen's, 1x there): what the turn asks now, readable, over the board.
    const int lineId = CardsLineId(k);
    const int lineH = lineId >= 0 ? 40 : 0;
    if (lineId >= 0)
        PanelTextAny(c, {kMgInnerX + 4, y, kMgInnerW - 8, lineH - 6}, Words(rom, lineId), 26, kMgValueMin, Align::Centre, Weight::ExtraBold,
                  k.turn != 0 ? kHeadColour : kValueColour);
    y += lineH;
    const int x = kMgInnerX, w = kMgInnerW, head = 24, row = 34, last = 40, h = 10 + head + 2 * row + 6 + last + 10;
    UiItem well = WellItem({x, y, w, h});
    c.ui.push_back(well);
    const int ix = x + 12, iw = w - 24, cx = ix + iw - 2 * kColW, sx = ix + iw - kColW;
    int yy = y + 10;
    PanelTextAny(c, {cx, yy, kColW - 8, head}, Words(rom, kStrMgCardsCount), kMgHeadSize, 13, Align::Right, Weight::Bold, kHeadColour);
    PanelTextAny(c, {sx, yy, kColW - 8, head}, Words(rom, kStrMgScore), kMgHeadSize, 13, Align::Right, Weight::Bold, kHeadColour);
    yy += head;
    const std::string names[2] = {k.name.empty() ? Words(rom, kStrMgPlayer) : k.name, Words(rom, kStrMgOpponent)};
    const int cards[2] = {k.hand, k.opponent}, scores[2] = {k.score, k.opponentScore};
    for (int i = 0; i < 2; i++, yy += row)
    {
        UiItem bar = PanelItem({ix - 4, yy + 2, iw + 8, row - 4});
        bar.plain = true;
        bar.fill = kFillRow;
        bar.radius = 8;
        bar.layer = 0;
        c.ui.push_back(bar);
        PanelTextAny(c, {ix + 6, yy, cx - ix - 12, row}, names[i], kMgValueSize, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
        PanelTextAny(c, {cx, yy, kColW - 8, row}, std::to_string(cards[i]), kMgValueSize, kMgValueSize, Align::Right, Weight::ExtraBold, 0xFFFFFFFF);
        PanelTextAny(c, {sx, yy, kColW - 8, row}, std::to_string(scores[i]), kMgValueSize, kMgValueSize, Align::Right, Weight::ExtraBold, kValueColour);
    }
    yy += 6;
    // The deck: the game's own card back, a thumbnail, and how many are left.
    const int tw = 24, th = 35;
    // (Not while a box of the game's lies over it: the help, the question, the message.)
    if (bottom && !k.box) c.ui.push_back(ImageItem({ix + 4, yy + (last - th) / 2, tw, th}, bottom, SW, SH, kMgDeckFrom));
    PanelTextAny(c, {ix + 4 + tw + 10, yy, 70, last}, std::to_string(k.deck), kMgValueSize, kMgValueSize, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    // The treasure chest's points.
    const int tx = ix + 4 + tw + 10 + 70;
    PanelTextAny(c, {tx, yy, sx - tx - 4, last}, Words(rom, kStrMgTreasure), kMgHeadSize + 2, 13, Align::Right, Weight::Bold, kHeadColour);
    PanelTextAny(c, {sx, yy, kColW - 8, last}, std::to_string(k.treasure), kMgValueSize, kMgValueSize, Align::Right, Weight::ExtraBold, kValueColour);
    return lineH + h;
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
    const FittedText vf = FitLineAny(value, kMgValueSize, kMgValueMin, w / 2, Weight::ExtraBold);
    const int vw = (int)std::ceil(TextWidth(vf.text, vf.size, Weight::ExtraBold)) + 2;
    // The label beside the value; when it does not fit there (Italian's long one), above it.
    const bool beside = FitLineAny(label, kMgValueSize - 2, kMgValueMin, w - 28 - vw - 10).fits;
    const int h = beside ? 48 : 80;
    c.ui.push_back(WellItem({x, y, w, h}));
    if (beside) PanelTextAny(c, {x + 14, y, w - 28 - vw - 10, h}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    else PanelTextAny(c, {x + 14, y + 6, w - 28, 34}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    PanelTextAny(c, {x + w - 14 - vw, beside ? y : y + 40, vw, beside ? h : 34}, vf.text, vf.size, vf.size, Align::Right, Weight::ExtraBold, kValueColour);
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
constexpr int kArtLabelW = 124;   // the painting's "Tool" label

// One row: the label at the left, a meter (frac >= 0) after it, the value at the right.
void MeterRow(Composition& c, int x, int y, int w, const std::string& label, float frac, u32 colour, const std::string& value, u32 valueColour)
{
    const int vw = 96;
    PanelTextAny(c, {x, y, kMgLabelW - 8, kMgRow}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    if (frac >= 0) Meter(c, {x + kMgLabelW, y + 7, w - kMgLabelW - vw - 8, kMgRow - 14}, frac, colour);
    PanelTextAny(c, {x + w - vw, y, vw, kMgRow}, value, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold, valueColour);
}

// A percentage as shown.
std::string Percent(long long n, long long of) { return std::to_string(of > 0 ? (int)std::lround(100.0 * std::clamp(n, 0LL, of) / of) : 0) + "%"; }

// The autopsy's values: the specimen and the money banked ($@1, the game's box), each organ's
// health (or removed / destroyed; freed: the hand can lift it) and the parasites.
int AutopsyValues(Composition& c, int y, const AutopsyState& a, const RomData& rom)
{
    const AutopsyWords w(rom);
    const int organs = a.specimen == 0 ? 3 : 2, h = 10 + (2 + organs) * kMgRow + 10;
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    PanelTextAny(c, {ix, yy, iw - 110, kMgRow}, With(w.specimen, a.specimen + 1), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    PanelTextAny(c, {ix + iw - 110, yy, 110, kMgRow}, MenuWords(rom, kStrMgMoney, a.earnings), kMgValueSize, kMgValueMin, Align::Right, Weight::ExtraBold, kValueColour);
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
    PanelTextAny(c, {ix, yy, kMgLabelW + 20, kMgRow}, w.parasites, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    PanelTextAny(c, {ix + kMgLabelW + 20, yy, iw - kMgLabelW - 20, kMgRow}, pv, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold,
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

// The detector's values: the charge (the game's gauge), the signal (a find in range: how well
// the depth matches, the scope's calm), the depth (the slider), what the card found, the earnings.
int DetectorValues(Composition& c, int y, const DetectorState& d, const RomData& rom)
{
    const DetectorWords w(rom);
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
    PanelTextAny(c, {ix, yy, kMgLabelW - 8, kMgRow}, Words(rom, kStrMgFound), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    PanelTextAny(c, {ix + kMgLabelW, yy, iw - kMgLabelW, kMgRow}, found.empty() ? "\xE2\x80\x94" : found, kMgValueSize - 2, kMgValueMin, Align::Right,
         Weight::ExtraBold, found.empty() ? kHeadColour : kValueColour);
    return h + 8 + EarningsValues(c, y + h + 8, 3, d.earnings, rom);
}

// The painting's 256 colours (resource 6028), decoded once per ROM (the panel is built every frame).
const std::vector<u32>& ArtPalette(const RomData& rom)
{
    static const RomData* owner = nullptr;
    static std::vector<u32> pal;
    if (owner != &rom || pal.empty())
    {
        std::vector<u8> res = rom.Resource(kResArtColours), raw;
        if (Decompress(res, 0, raw)) res = raw;
        pal.clear();
        if (!ReadPalette(res, pal)) pal.clear();
        owner = &rom;
    }
    return pal;
}

// The painting's values: the tool (and its size), the colour (a swatch of it, from the page's
// colours) and the palette page.
int ArtValues(Composition& c, int y, const ArtState& a, const RomData& rom)
{
    const ArtWords w(rom);
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, h = 10 + 3 * kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    std::string tool = w.tools[std::clamp(a.tool, 0, 5)];
    if (a.tool == 0 || a.tool == 1) tool += std::string(" (") + w.sizes[std::clamp(a.brush, 0, 2)] + ")";
    PanelTextAny(c, {ix, yy, kArtLabelW, kMgRow}, w.tool, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    PanelTextAny(c, {ix + kArtLabelW, yy, iw - kArtLabelW, kMgRow}, tool, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold, kValueColour);
    yy += kMgRow;
    // The colour: the page's colour swatch + 1 (resource 6028: 16 pages of 16).
    const std::vector<u32>& pal = ArtPalette(rom);
    const int index = std::clamp(a.page, 0, 15) * 16 + std::clamp(a.swatch, 0, 14) + 1;
    int sx = ix;
    if (index < (int)pal.size())
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
    PanelTextAny(c, {sx, yy, ix + iw - sx, kMgRow}, With(w.colourOf, std::clamp(a.swatch, 0, 14) + 1), kMgValueSize - 2, kMgValueMin, Align::Left,
         Weight::ExtraBold, 0xFFFFFFFF);
    yy += kMgRow;
    PanelTextAny(c, {ix, yy, iw, kMgRow}, With(w.page, std::clamp(a.page, 0, 15) + 1), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::ExtraBold,
         0xFFFFFFFF);
    return h;
}

// A string of the game's when the table holds it (an id from RAM: a name), else empty.
std::string SafeWords(const RomData& rom, int id) { return id >= 0 && id < rom.StringCount() ? Words(rom, id) : std::string(); }

// The Drencher's locked target as the game names it (a Sim 323 + k, an object 463 + k), or empty.
std::string DrencherTargetName(const RomData& rom, const DrencherState& d)
{
    if (d.target < 0) return "";
    if (d.targetType == 2) return SafeWords(rom, kStrMgSimName0 + d.target);
    if (d.targetType == 0xB) return SafeWords(rom, kStrMgObjectName0 + d.target);
    return "";
}

// A player's fruit as the game words it (the plural: "coconuts"); in English one is singular.
std::string Fruit(const RomData& rom, int player, int count)
{
    std::string w = Words(rom, kStrMgMooFruit + player);
    if (count == 1 && rom.Language() <= 1 && w.size() > 1 && w.back() == 's') w.pop_back();
    return w;
}

// MooGoo's values: the round, the players (name, their fruit on the monkeys still in: the score
// the game shows; the player whose turn it is raised), the cards left in the deck.
int MooGooValues(Composition& c, int y, const MooGooState& g, const RomData& rom)
{
    if (g.intro) return 0;
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, head = 24, row = 34;
    const int h = 10 + kMgRow + head + kMooGooPlayers * row + 6 + kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    const std::string round = g.round >= 1 && g.round <= 3 ? With(Own(rom, OwnWordId::MooGooRound), g.round) : Words(rom, kStrMgMooGoo);
    PanelTextAny(c, {ix, yy, iw, kMgRow}, round, kMgValueSize, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    yy += kMgRow;
    const int scoreW = 150;
    PanelTextAny(c, {ix + iw - scoreW, yy, scoreW - 8, head}, Words(rom, kStrMgMooScore), kMgHeadSize, 13, Align::Right, Weight::Bold, kHeadColour);
    yy += head;
    for (int p = 0; p < kMooGooPlayers; p++, yy += row)
    {
        const MooGooPlayer& pl = g.players[p];
        const bool now = p == g.current && g.phase >= 4 && g.phase != 0x12 && g.phase != 0x13;
        UiItem bar = PanelItem({ix - 4, yy + 2, iw + 8, row - 4});
        bar.plain = true;
        bar.fill = now ? kNowFill : kFillRow;
        bar.radius = 8;
        bar.layer = 0;
        c.ui.push_back(bar);
        const std::string name = SafeWords(rom, pl.nameId);
        PanelTextAny(c, {ix + 6, yy, iw - scoreW - 12, row}, name.empty() ? Words(rom, kStrMgMooPlayer1 + p) : name, kMgValueSize - 2, kMgValueMin, Align::Left,
             Weight::ExtraBold, 0xFFFFFFFF);
        // The score and its fruit (the game's words: coconuts, watermelons, pineapples).
        PanelTextAny(c, {ix + iw - scoreW, yy, scoreW - 8, row}, std::to_string(pl.score) + " " + Fruit(rom, p, pl.score), kMgValueSize - 4,
             kMgValueMin - 2, Align::Right, Weight::ExtraBold, kValueColour);
    }
    yy += 6;
    PanelTextAny(c, {ix, yy, iw - 80, kMgRow}, Own(rom, OwnWordId::MooGooCardsLeft), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    PanelTextAny(c, {ix + iw - 80, yy, 80, kMgRow}, std::to_string(std::max(0, g.deckSize - g.deckNext)), kMgValueSize - 2, kMgValueMin, Align::Right,
         Weight::ExtraBold, 0xFFFFFFFF);
    return h;
}

// The Drencher's values: the water and the pressure (the game's gauges), the target locked.
int DrencherValues(Composition& c, int y, const DrencherState& d, const RomData& rom)
{
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, h = 10 + 4 * kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    const float water = std::clamp(d.water, 0, 100) / 100.0f, pressure = std::clamp(d.pressure, 0, kDrencherPressureMax) / (float)kDrencherPressureMax;
    MeterRow(c, ix, yy, iw, Own(rom, OwnWordId::DrencherWater), water, 0xFF4FA8E8, Percent(d.water, 100), 0xFFFFFFFF);
    yy += kMgRow;
    MeterRow(c, ix, yy, iw, Own(rom, OwnWordId::DrencherPressure), pressure, Health(pressure), Percent(d.pressure, kDrencherPressureMax),
             DrencherLow(d.pressure) ? kBadColour : 0xFFFFFFFF);
    yy += kMgRow;
    const std::string target = DrencherTargetName(rom, d);
    // (The target's name on a line of its own: objects' names are long.)
    PanelTextAny(c, {ix, yy, iw, kMgRow}, Own(rom, OwnWordId::DrencherTarget), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    yy += kMgRow;
    PanelTextAny(c, {ix, yy, iw, kMgRow}, target.empty() ? Own(rom, OwnWordId::DrencherNone) : target, kMgValueSize - 2, kMgValueMin,
         Align::Right, Weight::ExtraBold, target.empty() ? kHeadColour : kValueColour);
    return h;
}

// The crime game's values: which of the three moves the game lights now (scare, the Rat-A-Rang,
// subdue), the aim's angle and the last throw.
int CrimeValues(Composition& c, int y, const CrimeState& k, const RomData& rom)
{
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, h = 10 + 4 * kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    const std::string ready = Own(rom, OwnWordId::CrimeReady), dash = "\xE2\x80\x94";
    const std::string labels[3] = {Own(rom, OwnWordId::CrimeScare), Words(rom, kStrMgRatARang), Own(rom, OwnWordId::CrimeSubdue)};
    const bool on[3] = {k.canScare, k.armed, k.canSubdue};
    for (int i = 0; i < 3; i++, yy += kMgRow)
    {
        PanelTextAny(c, {ix, yy, iw - 110, kMgRow}, labels[i], kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
        PanelTextAny(c, {ix + iw - 110, yy, 110, kMgRow}, on[i] ? ready : dash, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold,
             on[i] ? kGoodColour : kHeadColour);
    }
    const bool aiming = k.phase == 1 || k.phase == 2;
    PanelTextAny(c, {ix, yy, iw - 110, kMgRow}, Own(rom, OwnWordId::CrimeAimLabel), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    const std::string aim = aiming ? std::to_string(std::clamp(k.angle, -90, 90)) + "\xC2\xB0" : k.hit ? std::string(Own(rom, OwnWordId::CrimeHit)) : dash;
    PanelTextAny(c, {ix + iw - 110, yy, 110, kMgRow}, aim, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold,
         aiming ? kValueColour : k.hit ? kGoodColour : kHeadColour);
    return h;
}

// The Scratch lottery's values: the ticket's top prize, the boxes scratched (of five), each box's
// scratching as a small meter (green once the game reveals it: its picture is the game's, in the
// playfield), and the game's result when it has one.
int ScratchValues(Composition& c, int y, const ScratchState& t, const RomData& rom)
{
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, cell = 34;
    const bool result = t.state == 2 || t.state == 3;
    const int h = 10 + 2 * kMgRow + 6 + 2 * cell + (result ? 6 + 2 * kMgRow : 0) + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    PanelTextAny(c, {ix, yy, iw - 130, kMgRow}, Own(rom, OwnWordId::ScratchTopPrize), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    PanelTextAny(c, {ix + iw - 130, yy, 130, kMgRow}, MenuWords(rom, kStrMgMoney, std::max(0, t.prize)), kMgValueSize, kMgValueMin, Align::Right,
              Weight::ExtraBold, kValueColour);
    yy += kMgRow;
    PanelTextAny(c, {ix, yy, iw, kMgRow}, With(Own(rom, OwnWordId::ScratchOpened), std::clamp(t.revealed, 0, 5)), kMgValueSize - 2, kMgValueMin,
              Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
    yy += kMgRow + 6;
    // The boxes as the ticket lays them out: two rows of four.
    const int cw = (iw - 3 * 10) / 4;
    for (int i = 0; i < kScratchBoxes; i++)
    {
        const ScratchBox& b = t.boxes[i];
        const Rect r {ix + (i % 4) * (cw + 10), yy + (i / 4) * cell + 6, cw, cell - 12};
        const float f = b.revealed ? 1.0f : std::clamp(b.erased, 0, kScratchNeeded) / (float)kScratchNeeded;
        Meter(c, r, f, b.revealed ? kMeterGreen : 0xFFB2B2E3);
    }
    yy += 2 * cell;
    if (result)
    {
        yy += 6;
        const std::string w = t.state == 2 ? MenuWords(rom, kStrMgWon, std::max(0, t.prize)) : Words(rom, kStrMgLost);
        const FittedBlock fb = FitBlock(w, kMgValueSize - 2, kMgValueMin, {ix, yy, iw, 2 * kMgRow});
        UiItem tx = TextItem({ix, yy, iw, 2 * kMgRow}, fb.text, fb.size, Align::Left, Weight::ExtraBold);
        tx.fill = t.state == 2 ? kGoodColour : kHeadColour;
        tx.layer = 1;
        c.ui.push_back(tx);
    }
    return h;
}

// The music mixer's values: the song, the tempo, the instrument and octave, the tracks on, and
// what it does now (the game's own words where it has them).
int MixerValues(Composition& c, int y, const MixerState& m, const RomData& rom)
{
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, h = 10 + 6 * kMgRow + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    auto row = [&](const std::string& label, const std::string& value, u32 colour) {
        PanelTextAny(c, {ix, yy, kMgLabelW + 20, kMgRow}, label, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
        PanelTextAny(c, {ix + kMgLabelW + 24, yy, iw - kMgLabelW - 24, kMgRow}, value, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold, colour);
        yy += kMgRow;
    };
    const std::string dash = "\xE2\x80\x94";
    row(Own(rom, OwnWordId::MixerSong), m.song >= 0 && m.song < kMixerSongs ? Words(rom, kStrMgMixSong0 + m.song) : dash, kValueColour);
    row(Words(rom, kStrMgMixTempo), std::to_string(m.tempo), 0xFFFFFFFF);
    row(Own(rom, OwnWordId::MixerInstrument), m.instrument == 5 ? "4 (" + Words(rom, kStrMgMixVox) + ")" : std::to_string(std::clamp(m.instrument, 1, 3)),
        0xFFFFFFFF);
    row(Own(rom, OwnWordId::MixerOctave), (m.octave > 0 ? "+" : "") + std::to_string(std::clamp(m.octave, -1, 1)) +
        (m.sustain ? std::string(" \xC2\xB7 ") + Own(rom, OwnWordId::MixerSustain) : std::string()), 0xFFFFFFFF);
    // The tracks: eight lights, lit while audible (the game's red lights).
    PanelTextAny(c, {ix, yy, kMgLabelW + 20, kMgRow}, Words(rom, kStrMgMixTracks), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, 0xFFFFFFFF);
    for (int t = 0; t < 8; t++)
    {
        UiItem l = PanelItem({ix + iw - 8 * 22 + t * 22 + 3, yy + 9, 16, kMgRow - 18});
        l.plain = true;
        l.fill = m.tracks >> t & 1 ? 0xFFEB3030 : 0xFF4A4F80;
        l.radius = 4;
        l.layer = 1;
        c.ui.push_back(l);
    }
    yy += kMgRow;
    // Now: ready, playing, recording, the microphone, the song's list, help, the save question.
    const int p = m.phase;
    const std::string now = p == 1 ? Own(rom, OwnWordId::MixerPlaying) : p == 2 ? Own(rom, OwnWordId::MixerRecording) : p == 5 ? Words(rom, kStrMgMixMic)
                          : p == 7 ? Words(rom, kStrMgMixSelect) : p == 8 || p == 9 ? Own(rom, OwnWordId::MixerHelp) : p == 0xA ? Words(rom, kStrMgMixSaveAsk)
                                                                                                                   : Own(rom, OwnWordId::MixerReady);
    PanelTextAny(c, {ix, yy, iw, kMgRow}, now, kMgValueSize - 2, kMgValueMin, Align::Left, Weight::ExtraBold, p == 2 || p == 5 ? kBadColour : p == 1 ? kGoodColour : kValueColour);
    return h;
}

// The save page's values: the title typed so far and the three slots (the game's own names, or
// "No Data"); the focused slot's row raised, the one asked to be saved over lit.
int MixerSaveValues(Composition& c, int y, const MixerSaveState& v, const RomData& rom, int focusSlot, bool asked)
{
    const int x = kMgInnerX, wd = kMgInnerW, ix = x + 12, iw = wd - 24, row = 36, h = 10 + 2 * kMgRow + 6 + kMixerSaveSlots * row + 10;
    c.ui.push_back(WellItem({x, y, wd, h}));
    int yy = y + 10;
    PanelTextAny(c, {ix, yy, iw, kMgRow}, Words(rom, kStrMgSongTitle), kMgValueSize - 2, kMgValueMin, Align::Left, Weight::Bold, kHeadColour);
    yy += kMgRow;
    const std::string name = GameTextToUtf8(v.name);
    PanelTextAny(c, {ix, yy, iw, kMgRow}, name.empty() ? "\xE2\x80\x94" : name, kMgValueSize, kMgValueMin, Align::Left, Weight::ExtraBold,
              name.empty() ? kHeadColour : kValueColour);
    yy += kMgRow + 6;
    for (int j = 0; j < kMixerSaveSlots; j++, yy += row)
    {
        UiItem bar = PanelItem({ix - 4, yy + 2, iw + 8, row - 4});
        bar.plain = true;
        bar.fill = j == focusSlot ? (asked ? kLitFill : kNowFill) : kFillRow;
        bar.radius = 8;
        bar.layer = 0;
        c.ui.push_back(bar);
        PanelTextAny(c, {ix + 6, yy, 130, row}, With(Own(rom, OwnWordId::SaveSlot), j + 1), kMgValueSize - 4, kMgValueMin - 2, Align::Left, Weight::Bold,
                  0xFFFFFFFF);
        const std::string n = v.used[j] ? GameTextToUtf8(v.slotName[j]) : Words(rom, kStrMgNoData);
        PanelTextAny(c, {ix + 140, yy, iw - 146, row}, n, kMgValueSize - 2, kMgValueMin, Align::Right, Weight::ExtraBold, v.used[j] ? kValueColour : kHeadColour);
    }
    return h;
}

// The wireless lobby: why nothing happens there.
int TradingValues(Composition& c, int y, const RomData& rom)
{
    const int x = kMgInnerX, wd = kMgInnerW, h = 120;
    c.ui.push_back(WellItem({x, y, wd, h}));
    const Rect box {x + 14, y + 10, wd - 28, h - 20};
    const FittedBlock fb = FitBlock(Own(rom, OwnWordId::TradingNote), kMgValueSize, kMgValueMin, box);
    UiItem t = TextItem(box, fb.text, fb.size, Align::Left, Weight::ExtraBold);
    t.layer = 1;
    c.ui.push_back(t);
    return h;
}

// MooGoo's focus: a ring in the game's selection green around what A touches (DS pixels, over the
// playfield), as the cards' focus ring (an overlay over the pieces).
void AddMark(Composition& c, Rect m)
{
    if (m.w <= 0) return;
    const Rect to = kActivityBottomTo;
    const int x0 = to.x + m.x * to.w / SW, y0 = to.y + m.y * to.h / SH, x1 = to.x + (m.x + m.w) * to.w / SW, y1 = to.y + (m.y + m.h) * to.h / SH;
    constexpr int t = 5;
    c.overlays.push_back({{x0 - t, y0 - t, x1 - x0 + 2 * t, y1 - y0 + 2 * t}, 0, 10, t, kMarkColour, -1});
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
int SocialAnswers(Composition& c, int y, const u32* bottom, const RomData& rom)
{
    const int x = kMgInnerX, w = kMgInnerW;
    PanelTextAny(c, {x + 4, y, w - 8, 34}, Own(rom, OwnWordId::SocialMatch), kMgValueSize, kMgValueMin, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
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
                PanelTextAny(c, {x, y, sw + 2, h}, t.sep, size, size, Align::Left, Weight::ExtraBold, kHeadColour);
                x += sw + 6;
            }
        }
        if (!hint.text.empty())
        {
            const int tw = (int)std::ceil(TextWidth(hint.text, size, Weight::ExtraBold));
            PanelTextAny(c, {x + 4, y, tw + 2, h}, hint.text, size, size, Align::Left, Weight::ExtraBold, 0xFFFFFFFF);
            x += 4 + tw;
        }
        x += kMgPromptGap;
    }
}
}

int CardsLineId(const CardsState& k)
{
    if (!k.valid) return -1;
    // (The opponent's own steps but its plays: "Other Player's Turn".)
    if (k.turn != 0 && k.phase != 3 && k.phase != 4 && k.phase != 5 && k.phase != 6) return kStrMgCardsOther;
    switch (k.phase)
    {
    case 0: case 1: case 2: case 3: case 4: case 5: return kStrMgCardsLine0 + k.phase;
    case 6: return kStrMgCardsSwitching;
    case 8: return kStrMgCardsDrawing;
    default: return -1;
    }
}

int MinigameHelpAt(Screen s, int x, int y)
{
    auto in = [&](int x0, int y0, int x1, int y1) { return x > x0 && x < x1 && y > y0 && y < y1; };   // (the game's strict bounds)
    if (s == Screen::Drencher)
    {
        if (in(35, 7, 81, 85)) return kStrMgDrHelp0;            // the water gauge
        if (in(96, 7, 110, 85)) return kStrMgDrHelp0 + 1;       // the pressure gauge
        if (in(137, 46, 171, 80)) return kStrMgDrHelp0 + 3;     // target
        if (in(13, 159, 51, 197)) return kStrMgDrHelp0 + 4;     // refill
        if (in(219, 159, 253, 193)) return kStrMgDrHelp0 + 5;   // fire
        if (y > 94 && y < 146) return kStrMgDrHelp0 + 2;        // the pump's band
        return -1;
    }
    if (s == Screen::Crime)
    {
        if (in(94, 13, 161, 56)) return kStrMgCrHelp0;          // scare
        if (in(94, 135, 161, 178)) return kStrMgCrHelp0 + 2;   // subdue
        if (in(40, 70, 215, 125)) return kStrMgCrHelp0 + 1;    // the Rat-A-Rang
        return -1;
    }
    if (s == Screen::Mixer)
    {
        // The game's regions (first match, research m6 §8) → its help page h: the title 1778 + 2h.
        struct R { int x, y, w, h, page; };
        static constexpr R kRegions[] = {
            {63, 31, 38, 18, 0}, {63, 59, 38, 18, 1}, {3, 4, 28, 18, 2}, {29, 3, 28, 18, 3}, {108, 4, 38, 18, 4}, {6, 138, 28, 18, 5}, {62, 88, 28, 18, 6},
            {3, 33, 24, 72, 7}, {31, 33, 24, 72, 7}, {109, 27, 35, 20, 8}, {109, 63, 35, 20, 8},
            {156, 16, 60, 90, 9}, {94, 88, 28, 18, 10}, {125, 88, 28, 18, 10}, {223, 16, 28, 90, 11}, {32, 121, 224, 71, 12},
        };
        for (const R& r : kRegions)
            if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return kStrMgMixHelp0 + 2 * r.page;
        return -1;
    }
    return -1;
}

namespace
{
// A box of the minigame's own, or the game's popup over it, as the HD port draws it: its words,
// its buttons (each the activity's own, activities.cpp), the game's box it hides (DS pixels).
struct MgBox
{
    Rect cover {};
    std::string title, text;
    std::vector<GameBoxButton> buttons;   // a question's: No, Yes (then the mixer's Preview)
    bool question = false;
};

// A question's buttons as its focus makes them (ActivityState::askFocus: No first): the focused
// answer takes the A chip and the halo, No keeps B, Yes unfocused its word alone.
void FocusQuestion(MgBox& b, int focus, const RomData& rom)
{
    if (!b.question || b.buttons.size() < 2) return;
    const int f = focus == 1 ? 1 : 0;
    const std::string no = Words(rom, kStrMgNo), yes = Words(rom, kStrMgYes);
    b.buttons[0].chips = f == 0 ? std::vector<BoxChip> {{Button::A, ""}, {Button::B, no}} : std::vector<BoxChip> {{Button::B, no}};
    b.buttons[1].chips = f == 1 ? std::vector<BoxChip> {{Button::A, yes}} : std::vector<BoxChip> {{Button::A, yes, true}};
    b.buttons[0].focused = f == 0;
    b.buttons[1].focused = f == 1;
}
Rect Around(Point p, int r) { return {p.x - r, p.y - r, 2 * r, 2 * r}; }
GameBoxButton Btn(Button b, const std::string& word, Rect from, bool pressed = false) { return {{{b, word}}, from, pressed}; }

// The game's first line of a string with its own line breaks ("\n\n\n End of Turn. \n\n Touch for…": "End of Turn.").
std::string FirstLine(const std::string& s)
{
    size_t a = 0;
    while (a < s.size())
    {
        size_t b = s.find('\n', a);
        if (b == std::string::npos) b = s.size();
        std::string l = s.substr(a, b - a);
        const size_t f = l.find_first_not_of(' '), e = l.find_last_not_of(' ');
        if (f != std::string::npos) return l.substr(f, e - f + 1);
        a = b + 1;
    }
    return "";
}

bool BoxOf(Screen s, const GameState& game, const RomData& rom, MgBox& b)
{
    const MinigameState& m = game.minigame;
    // The game's own popup (its manager's: the lottery's results and hint, the painting's question).
    // (Its box slides in and out from below: opening and closing, and the frame after it went, the
    // cover reaches the playfield's foot.)
    const bool gone = !GamePopup(game) && m.popupGone.valid;
    const PopupState& p = gone ? m.popupGone : game.town.popup;
    if ((GamePopup(game) || gone) && p.boxW > 0 && p.boxTiles > 0)
    {
        b.cover = {128 - p.boxW / 2, p.boxY, p.boxW, 8 * p.boxTiles + 32};
        if (gone || p.state != 2) b.cover.h = SH - p.boxY;
        b.text = MenuText(GameTextToUtf8(p.text));
        // (a question's title in RAM is stale: not drawn; a message's and a hint's are its own)
        b.title = p.kind == 2 ? "" : MenuText(GameTextToUtf8(p.title));
        for (size_t at; (at = b.text.find('$')) != std::string::npos;) b.text.replace(at, 1, "\xC2\xA7");   // (the game's simoleon)
        int x, y;
        auto from = [&](int i) { return PopupButton(p, i, x, y) ? Rect {x - 12, y - 12, 24, 24} : Rect {}; };
        if (p.kind == 0) b.buttons = {Btn(Button::A, Words(rom, kStrMgClose), from(0), p.pressing && p.pressed == 0)};
        else if (p.kind == 2)
        {
            b.buttons = {Btn(Button::B, Words(rom, kStrMgNo), from(0), p.pressing && p.pressed == 0), Btn(Button::A, Words(rom, kStrMgYes), from(1), p.pressing && p.pressed == 1)};
            b.question = true;
        }
        return !b.text.empty();
    }
    if (s == Screen::Cards && m.cards.valid && (m.cards.quitAsk || m.cards.message))
    {
        b.cover = kMgCardsBox;
        if (m.cards.quitAsk)
        {
            b.text = Words(rom, kStrMgQuitAsk);
            b.buttons = {Btn(Button::B, Words(rom, kStrMgNo), Around(kCardsQuitNo, 16)), Btn(Button::A, Words(rom, kStrMgYes), Around(kCardsQuitYes, 16))};
            b.question = true;
        }
        else
        {
            // "End of Turn." and whose turn comes, in the game's words (MooGoo's "@1's turn"): not its "Touch for…".
            const std::string next = m.cards.turn == 0 ? Words(rom, kStrMgOpponent) : m.cards.name.empty() ? Words(rom, kStrMgPlayer) : m.cards.name;
            b.text = MenuText(FirstLine(rom.String(kStrMgEndOfTurn))) + "\n" + MenuWordsWith(rom, kStrMgTurnOf, next);
            b.buttons = {Btn(Button::A, Own(rom, OwnWordId::CardsContinue), Around(kCardsMessage, 20))};
        }
        return true;
    }
    if (s == Screen::MooGoo && m.mooGoo.valid && !m.mooGoo.intro && m.mooGoo.quitAsk)
    {
        b.cover = kMgMooGooBox;
        b.text = Words(rom, kStrMgQuitAsk);
        b.buttons = {Btn(Button::B, Words(rom, kStrMgNo), Around(kMooGooNo, 16)), Btn(Button::A, Words(rom, kStrMgYes), Around(kMooGooYes, 16))};
        b.question = true;
        return true;
    }
    if (s == Screen::Mixer && m.mixer.valid)
    {
        if (m.mixer.phase == 0xA)
        {
            b.cover = kMgMixerBox;
            b.text = Words(rom, kStrMgMixSaveAsk);
            b.buttons = {Btn(Button::B, Words(rom, kStrMgNo), {111, 94, 34, 16}), Btn(Button::A, Words(rom, kStrMgYes), {69, 94, 34, 16}),
                         Btn(Button::X, Words(rom, kStrMgMixPreview), {153, 94, 34, 16})};
            b.question = true;
            return true;
        }
        if (m.mixer.phase == 5)
        {
            b.cover = kMgMicBox;
            b.text = Words(rom, kStrMgMixMic);
            b.buttons = {Btn(Button::Y, Own(rom, OwnWordId::MixerVoxStop), {6, 138, 28, 18})};
            return true;
        }
        if (m.mixer.phase == 9 && m.mixer.helpId >= kStrMgMixHelp0 && m.mixer.helpId < kStrMgMixHelpEnd)
        {
            b.cover = kMgHelpBox;
            b.title = Words(rom, m.mixer.helpId);
            b.text = MenuText(rom.String(m.mixer.helpId + 1));
            b.buttons = {Btn(Button::A, Own(rom, OwnWordId::Close), {118, 86, 20, 20})};
            return true;
        }
    }
    const int help = s == Screen::Drencher && m.drencher.valid && m.drencher.helpText ? m.drencher.helpId
                   : s == Screen::Crime && m.crime.valid && m.crime.helpText ? m.crime.helpId : -1;
    if (help >= kStrMgDrHelp0 && help < kStrMgCrHelp0 + 3)
    {
        b.cover = kMgHelpBox;
        b.text = MenuText(rom.String(help));
        b.buttons = {Btn(Button::A, Own(rom, OwnWordId::Close), {118, 86, 20, 20})};
        return true;
    }
    return false;
}

// MooGoo's line at the top of its table (the game's 1x words, "Place a Bet - Touch a Monkey!"): what
// the turn asks, in HD over it while the game shows one; the bet's in the controller's words.
std::string MooGooLine(const MooGooState& g, const RomData& rom)
{
    // (Measured: the line's letters against the phase, every 5 frames of a whole round.)
    if (!g.valid || g.intro) return "";
    if (g.phase == 7 || g.phase == 8) return Words(rom, kStrMgMooShuffling);
    if (g.phase == 0x16) return Words(rom, kStrMgMooDealing);
    if (g.phase == 0xD)
    {
        if (g.sub <= 2) return Own(rom, OwnWordId::MooGooBetLine);
        if (g.sub == 5 || g.sub == 6) return Words(rom, kStrMgMooTarget);
        return Words(rom, kStrMgMooPlayCard);   // (3, 4, and the card's animation, draw and end: 7, 8, 9)
    }
    if (g.phase >= 4 && g.phase <= 0xC) return Own(rom, OwnWordId::MooGooBetLine);   // (the bets, the banners)
    return "";
}

// Whether the game draws its line now: light letters on the table's top rows.
bool MooGooLineShown(const u32* bottom)
{
    if (!bottom) return false;
    int n = 0;
    for (int y = kMgMooLineFrom.y; y < kMgMooLineFrom.y + kMgMooLineFrom.h; y++)
        for (int x = kMgMooLineFrom.x; x < kMgMooLineFrom.x + kMgMooLineFrom.w; x++)
        {
            const u32 c = bottom[y * SW + x];
            if (((c >> 16) & 0xFF) > 200 && ((c >> 8) & 0xFF) > 200 && (c & 0xFF) > 180) n++;
        }
    return n >= 12;
}

// MooGoo's line in HD: a plate over the game's, its words centred.
void AddMooGooLine(Composition& c, const std::string& line)
{
    const Rect r = ToCanvas(kMgMooLineFrom, 0);   // (its shadow clear of the table's first row)
    UiItem plate = PanelItem(r);
    plate.fill = kMooPlate;
    plate.borderArgb = 0xFF2A0E1A;
    plate.radius = 14;
    plate.layer = 1;
    c.ui.push_back(plate);
    PanelTextAny(c, {r.x + 16, r.y, r.w - 32, r.h}, line, 30, 20, Align::Centre, Weight::ExtraBold, 0xFFFFFFFF, 1);
}

// MooGoo's turn banner ("@1's turn", the game's words) in HD over the game's.
void AddMooGooBanner(Composition& c, const MooGooState& g, const RomData& rom)
{
    const MooGooPlayer& pl = g.players[std::clamp(g.current, 0, kMooGooPlayers - 1)];
    std::string name = SafeWords(rom, pl.nameId);
    if (name.empty()) name = Words(rom, kStrMgMooPlayer1 + std::clamp(g.current, 0, kMooGooPlayers - 1));
    const Rect r = ToCanvas(kMgMooBannerFrom, 6);
    UiItem box = PanelItem(r);
    box.radius = 18;
    box.layer = 1;
    c.ui.push_back(box);
    PanelTextAny(c, {r.x + 20, r.y, r.w - 40, r.h}, MenuWordsWith(rom, kStrMgTurnOf, name), 32, 20, Align::Centre, Weight::ExtraBold, 0xFFFFFFFF, 1);
}
}

ActivityRam MinigameRam(const GameState* game)
{
    ActivityRam r;
    if (!game || !game->valid) return r;
    const MinigameState& m = game->minigame;
    if (game->screen == GScreen::Cards && m.cards.valid) { r.cards = true; r.quitAsk = m.cards.quitAsk; r.message = m.cards.message; }
    if (game->screen == GScreen::Autopsy && m.autopsy.valid)
    {
        r.autopsy = true;
        r.autopsyTool = m.autopsy.tool;
        r.autopsySub = m.autopsy.sub;
        r.autopsyPhase = m.autopsy.phase;
        for (int i = 0; i < kAutopsyOrgans && i < 3; i++) r.autopsyFreed[i] = m.autopsy.organs[i].freed && m.autopsy.organs[i].present();
        for (const AutopsyParasite& p : m.autopsy.parasites) r.autopsyParasites.push_back({p.slot, (float)p.x, (float)p.y, p.eating});
    }
    if (game->screen == GScreen::Detector && m.detector.valid) { r.detector = true; r.detectorSlider = m.detector.slider; }
    if (game->screen == GScreen::ArtGallery && m.art.valid) { r.art = true; r.artButton = m.art.button; r.artSwatch = m.art.swatch; }
    if (game->screen == GScreen::MooGoo && m.mooGoo.valid)
    {
        const MooGooState& g = m.mooGoo;
        r.mooGoo = true;
        r.mooGooIntro = g.intro;
        r.mooGooPhase = g.phase;
        r.mooGooSub = g.sub;
        r.mooGooQuitAsk = g.quitAsk;
        const int p = std::clamp(g.current, 0, kMooGooPlayers - 1);
        r.mooGooHuman = !g.players[p].ai;
        for (int i = 0; i < kMooGooMonkeys; i++)
        {
            r.mooGooAlive[i] = !g.monkeys[i].eliminated;
            r.mooGooBet[i] = r.mooGooAlive[i] && g.monkeys[i].slots < 4;
            r.mooGooPile[i] = g.monkeys[i].count > 0;
        }
        for (int i = 0; i < kMooGooHand; i++) r.mooGooCard[i] = g.players[p].hand[i] != kMooGooHandEmpty;
        r.mooGooSelected = g.players[p].selected;
    }
    if (game->screen == GScreen::Drencher && m.drencher.valid) { r.drencher = true; r.drencherPump = m.drencher.pump; r.drencherHelp = m.drencher.help; r.drencherHelpText = m.drencher.helpText; }
    if (game->screen == GScreen::Crime && m.crime.valid) { r.crime = true; r.crimePhase = m.crime.phase; r.crimeHelp = m.crime.help; r.crimeHelpText = m.crime.helpText; }
    r.placement = game->placement.valid;
    if (game->screen == GScreen::Scratch && m.scratch.valid)
    {
        r.scratch = true;
        r.scratchTier = std::clamp(m.scratch.tier, 0, 2);
        r.scratchPopup = GamePopup(*game);
        for (int i = 0; i < kScratchBoxes; i++) r.scratchOpen[i] = m.scratch.boxes[i].revealed || m.scratch.boxes[i].erased >= kScratchNeeded;
    }
    if (game->screen == GScreen::Mixer && m.mixer.valid) { r.mixer = true; r.mixerPhase = m.mixer.phase; r.mixerSongs = (int)m.mixer.unlocked.size(); }
    if (game->screen == GScreen::MixerSave && m.mixerSave.valid)
    {
        r.mixerSave = true;
        for (int j = 0; j < kMixerSaveSlots; j++) r.mixerSaveUsed[j] = m.mixerSave.used[j];
    }
    if (game->screen == GScreen::Vacuum && m.vacuum.bowl)
    {
        r.vacuumBowl = true;
        r.vacuumKeeping = m.vacuum.keeping;
        for (const VacuumItem& t : m.vacuum.items) r.vacuumThings.push_back({t.slot, t.x, t.y, t.half, t.Dust(), t.held});
    }
    const PopupState& p = game->town.popup;
    if (p.valid && p.kind == 2 && p.state >= 1 && p.state <= 3)
    {
        r.askUp = true;
        r.ask = p.state == 2 && PopupButton(p, 1, r.askYes.x, r.askYes.y) && PopupButton(p, 0, r.askNo.x, r.askNo.y);
    }
    return r;
}

bool MinigameFramed(ScreenState s, const GameState* game, const RomData* rom)
{
    return IsActivity(s.screen) && game && game->valid && rom &&
           rom->StringCount() > kStrMgMax;
}

void BuildMinigame(Composition& c, ScreenState s, int topW, int topH, const HudState* hud, const HudStrings* strings, const GameState& game,
                   const RomData& rom, const u32* bottom, IconCache*, const PointerView* pointer, int focus)
{
    // The playfield and the secondary panel as milestone 1 placed them (the stylus's piece, the
    // checks' pixels): the touch screen 3x at the left, the top screen 384 wide at the right.
    const bool bar = hud && strings && strings->ok && s.screen != Screen::Cards;
    // (The social game's own bar at the top screen's top, the Sim's name and the relationship's
    // meter at 1x, unreadable that small: left out, as its status bar is.)
    const int skip = s.screen == Screen::Social ? topH * kMgSocialBarRows / 192 : 0;
    const int rows = (bar ? HudCropRows(topH) : topH) - skip;
    const int h = (int)std::lround((double)kActivityTopW * rows / topW);
    const Rect topTo {kActivityTopX, kActivityTopY, kActivityTopW, h};
    c.overlays.push_back({{0, 0, kConsoleW, kConsoleH}, kPanelArgb, 0, 0, 0, 0});
    c.overlays.push_back({{kActivityBottomTo.x - 6, kActivityBottomTo.y - 6, kActivityBottomTo.w + 12, kActivityBottomTo.h + 12}, kFrameArgb, 14, 6, kOutlineArgb, 0});
    c.overlays.push_back({{topTo.x - 6, topTo.y - 6, topTo.w + 12, topTo.h + 12}, kFrameArgb, 14, 6, kOutlineArgb, 0});
    c.pieces.push_back({Src::Bottom, {0, 0, SW, SH}, kActivityBottomTo});
    c.pieces.push_back({Src::Top, {0, skip, topW, rows}, topTo});

    // The title plate, over the playfield's frame.
    if (const std::string title = Title(s.screen, game, rom); !title.empty())
    {
        const FittedText t = FitLineAny(title, kMgTitleSize, kMgTitleMin, kMgTitleMaxW - 48, Weight::ExtraBold);
        const int w = std::min(kMgTitleMaxW, (int)std::ceil(TextWidth(t.text, t.size, Weight::ExtraBold)) + 48);
        c.ui.push_back(PanelItem({kMgTitleX, kMgTitleY, w, kMgTitleH}));
        PanelTextAny(c, {kMgTitleX + 24, kMgTitleY, w - 48, kMgTitleH}, t.text, t.size, t.size, Align::Left, Weight::ExtraBold, 0xFFFFFFFF, 0);
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
    else if (s.screen == Screen::Social) content = SocialAnswers(c, y + kMgPad, bottom, rom);
    else if (s.screen == Screen::Autopsy && game.minigame.autopsy.valid) content = AutopsyValues(c, y + kMgPad, game.minigame.autopsy, rom);
    else if (s.screen == Screen::Detector && game.minigame.detector.valid) content = DetectorValues(c, y + kMgPad, game.minigame.detector, rom);
    else if (s.screen == Screen::Art && game.minigame.art.valid) content = ArtValues(c, y + kMgPad, game.minigame.art, rom);
    else if (s.screen == Screen::MooGoo && game.minigame.mooGoo.valid) content = MooGooValues(c, y + kMgPad, game.minigame.mooGoo, rom);
    else if (s.screen == Screen::Drencher && game.minigame.drencher.valid) content = DrencherValues(c, y + kMgPad, game.minigame.drencher, rom);
    else if (s.screen == Screen::Crime && game.minigame.crime.valid) content = CrimeValues(c, y + kMgPad, game.minigame.crime, rom);
    else if (s.screen == Screen::Scratch && game.minigame.scratch.valid) content = ScratchValues(c, y + kMgPad, game.minigame.scratch, rom);
    else if (s.screen == Screen::Mixer && game.minigame.mixer.valid) content = MixerValues(c, y + kMgPad, game.minigame.mixer, rom);
    else if (s.screen == Screen::MixerSave && game.minigame.mixerSave.valid)
        content = MixerSaveValues(c, y + kMgPad, game.minigame.mixerSave, rom, SaveSlotAt(focus), pointer && pointer->quitArmed);
    else if (s.screen == Screen::Trading) content = TradingValues(c, y + kMgPad, rom);
    if (content > 0) c.ui[panel].to.h = std::min(content + 2 * kMgPad, kMgSideBottom - y);
    else c.ui.erase(c.ui.begin() + (long)panel);
    // The prompt bar: the controller's hints.
    AddPrompts(c, Hints(s.screen, game, rom, pointer && pointer->quitArmed, focus, pointer ? pointer->askFocus : -1, pointer && pointer->taking));
    // MooGoo's line and turn banner in HD over the game's (its letters 1x: "Touch a Monkey!").
    if (s.screen == Screen::MooGoo && game.minigame.mooGoo.valid && !game.minigame.mooGoo.intro && !game.minigame.mooGoo.quitAsk)
    {
        const MooGooState& g = game.minigame.mooGoo;
        if (const std::string line = MooGooLine(g, rom); !line.empty() && MooGooLineShown(bottom)) AddMooGooLine(c, line);
        if (g.phase == 0xB || g.phase == 0xC) AddMooGooBanner(c, g, rom);
    }
    // A box of the game's (its popup, a question, a message, a help text) in HD over the playfield:
    // the hints under the veil, no pointer or focus over it.
    MgBox box;
    if (BoxOf(s.screen, game, rom, box))
    {
        FocusQuestion(box, pointer ? pointer->askFocus : -1, rom);
        BuildGameBox(c, GameBoxAt(ToCanvas(box.cover, 0), kMgBoxArea, (int)box.buttons.size()), box.title, box.text, box.buttons);
    }
    else
    {
        if (pointer && pointer->shown && OwnScheme(s.screen)) AddPointer(c, *pointer);
        if (pointer) AddMark(c, pointer->mark);   // (MooGoo's focus, the mixer's song list)
    }
    SortUi(c.ui);
}

// Every prompt bar each minigame can show (each state its hints follow), in rom's language, as
// AddPrompts lays it out: shrunk when it does not hold at kMgHintSize, cut when it does not hold
// at kMgHintMin either; and the port's own words of the side panels in their boxes.
static void MeasureOwnWords(const RomData& rom, BlockMeasure& m)
{
    const Measurer me {m};
    constexpr int kOwn = 9000;   // a port word's id in the tallies: kOwn + its OwnWordId
    auto id = [](OwnWordId w) { return kOwn + (int)w; };
    const int avail = kMgPrompt.w - 2 * kMgPromptPad;
    auto total = [](const std::vector<HdHint>& hs, float size) {
        int t = (int)(hs.size() - 1) * kMgPromptGap;
        for (const HdHint& h : hs) t += HintWidth(h, size);
        return t;
    };
    std::vector<std::pair<Screen, GameState>> states;
    auto add = [&](Screen sc, auto&& set) {
        GameState g;
        g.valid = true;
        set(g);
        states.push_back({sc, g});
    };
    add(Screen::Cards, [](GameState&) {});
    add(Screen::Cards, [](GameState& g) { g.minigame.cards.valid = g.minigame.cards.quitAsk = true; });
    add(Screen::Cards, [](GameState& g) { g.minigame.cards.valid = g.minigame.cards.message = true; });
    add(Screen::Vacuum, [](GameState&) {});
    add(Screen::Vacuum, [](GameState& g) { g.minigame.vacuum.valid = g.minigame.vacuum.clogged = true; });
    add(Screen::Vacuum, [](GameState& g) { g.minigame.vacuum.valid = g.minigame.vacuum.clogged = g.minigame.vacuum.bowl = true; });
    add(Screen::Vacuum, [](GameState& g) {
        g.minigame.vacuum.valid = g.minigame.vacuum.clogged = g.minigame.vacuum.bowl = true;
        g.minigame.vacuum.items.push_back({0, 100, 100, 13, 0x196, false});
    });
    add(Screen::Social, [](GameState&) {});
    for (int tool = 0; tool < 3; tool++)
        add(Screen::Autopsy, [tool](GameState& g) {
            g.minigame.autopsy.valid = g.minigame.autopsy.handAlert = g.minigame.autopsy.tweezersAlert = true;
            g.minigame.autopsy.tool = tool;
        });
    add(Screen::Autopsy, [](GameState& g) { g.minigame.autopsy.valid = true; g.minigame.autopsy.phase = 5; });
    add(Screen::Autopsy, [](GameState& g) { g.minigame.autopsy.valid = true; g.minigame.autopsy.sub = 3; });
    add(Screen::Autopsy, [](GameState& g) { g.minigame.autopsy.valid = true; g.minigame.autopsy.sub = 6; });
    for (int tool = 1; tool < 3; tool++)   // (A's take: a freed organ, a parasite on the body)
        add(Screen::Autopsy, [tool](GameState& g) {
            AutopsyState& a = g.minigame.autopsy;
            a.valid = a.handAlert = a.tweezersAlert = true;
            a.tool = a.phase = tool;
            a.organs[0].kind = 0;
            a.organs[0].freed = true;
            a.parasites.push_back({0, 100, 100, 0, true});
        });
    add(Screen::Detector, [](GameState& g) { g.minigame.detector.valid = g.minigame.detector.inRange = true; });
    add(Screen::Art, [](GameState& g) { g.minigame.art.valid = true; });
    add(Screen::Art, [](GameState& g) { g.town.popup.valid = true; g.town.popup.state = 2; g.town.popup.kind = 2; });
    add(Screen::MooGoo, [](GameState&) {});
    add(Screen::MooGoo, [](GameState& g) { g.minigame.mooGoo.valid = g.minigame.mooGoo.intro = true; });
    add(Screen::MooGoo, [](GameState& g) { g.minigame.mooGoo.valid = g.minigame.mooGoo.quitAsk = true; });
    add(Screen::MooGoo, [](GameState& g) { g.minigame.mooGoo.valid = true; });   // the players' prompt
    add(Screen::MooGoo, [](GameState& g) { g.minigame.mooGoo.valid = true; g.minigame.mooGoo.phase = 9; });
    for (int sub : {1, 3, 5})
        add(Screen::MooGoo, [sub](GameState& g) { g.minigame.mooGoo.valid = true; g.minigame.mooGoo.phase = 0xD; g.minigame.mooGoo.sub = sub; });
    add(Screen::Drencher, [](GameState& g) { g.minigame.drencher.valid = true; });
    add(Screen::Drencher, [](GameState& g) { g.minigame.drencher.valid = g.minigame.drencher.help = true; });
    add(Screen::Drencher, [](GameState& g) { g.minigame.drencher.valid = g.minigame.drencher.help = g.minigame.drencher.helpText = true; });
    add(Screen::Crime, [](GameState& g) { g.minigame.crime.valid = g.minigame.crime.help = true; });
    add(Screen::Crime, [](GameState& g) { g.minigame.crime.valid = g.minigame.crime.help = g.minigame.crime.helpText = true; });
    add(Screen::Crime, [](GameState& g) { g.minigame.crime.valid = g.minigame.crime.canScare = g.minigame.crime.armed = g.minigame.crime.canSubdue = true; });
    add(Screen::Crime, [](GameState& g) { g.minigame.crime.valid = true; g.minigame.crime.phase = 2; });
    add(Screen::Scratch, [](GameState& g) { g.minigame.scratch.valid = true; });
    add(Screen::Scratch, [](GameState& g) { g.minigame.scratch.valid = g.town.popup.valid = true; g.town.popup.state = 2; });
    for (int phase : {0, 1, 2, 5, 7, 8, 9, 0xA})
        add(Screen::Mixer, [phase](GameState& g) { g.minigame.mixer.valid = true; g.minigame.mixer.phase = phase; });
    add(Screen::Trading, [](GameState&) {});
    std::vector<std::vector<HdHint>> bars;
    for (const auto& [sc, g] : states) bars.push_back(Hints(sc, g, rom));
    bars.push_back(Hints(Screen::Autopsy, states.back().second, rom, true));   // the autopsy's quit asked
    bars.push_back(Hints(Screen::Scratch, states.back().second, rom, true));   // the lottery's
    {
        GameState g;
        g.valid = g.minigame.vacuum.valid = g.minigame.vacuum.clogged = g.minigame.vacuum.bowl = true;
        bars.push_back(Hints(Screen::Vacuum, g, rom, false, -1, -1, true));   // the vacuum's take running
        g.screen = GScreen::Autopsy;
        g.minigame.autopsy.valid = true;
        g.minigame.autopsy.sub = 3;
        bars.push_back(Hints(Screen::Autopsy, g, rom, false, -1, -1, true));   // the autopsy's take carrying
    }
    for (int f : {0, 30}) bars.push_back(Hints(Screen::MixerSave, states.back().second, rom, false, f));   // a key, a slot
    bars.push_back(Hints(Screen::MixerSave, states.back().second, rom, true, 30));   // a slot's overwrite asked
    for (const std::vector<HdHint>& hs : bars) me.Note(kOwn, total(hs, kMgHintSize) > avail, total(hs, kMgHintMin) <= avail);
    // The side panels' own words (their boxes as the Build functions give them).
    const float vs = kMgValueSize - 2;
    const int iw = kMgInnerW - 24;
    for (int i = 1; i <= 2; i++) me.LineAny(id(OwnWordId::AutopsySpecimen), With(Own(rom, OwnWordId::AutopsySpecimen), i), vs, kMgValueMin, iw - 110);
    me.LineAny(id(OwnWordId::AutopsyOrgan), With(Own(rom, OwnWordId::AutopsyOrgan), 3), vs, kMgValueMin, kMgLabelW - 8, Weight::Bold);
    for (OwnWordId w : {OwnWordId::AutopsyFreed, OwnWordId::AutopsyRemoved, OwnWordId::AutopsyDestroyed, OwnWordId::DetectorNone})
        me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, 96);
    me.LineAny(id(OwnWordId::AutopsyParasites), Own(rom, OwnWordId::AutopsyParasites), vs, kMgValueMin, kMgLabelW + 20, Weight::Bold);
    me.LineAny(id(OwnWordId::AutopsyEating), "8 (" + With(Own(rom, OwnWordId::AutopsyEating), 8) + ")", vs, kMgValueMin, iw - kMgLabelW - 20);
    for (OwnWordId w : {OwnWordId::DetectorCharge, OwnWordId::DetectorSignal, OwnWordId::DetectorDepth})
        me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, kMgLabelW - 8, Weight::Bold);
    me.LineAny(id(OwnWordId::ArtTool), Own(rom, OwnWordId::ArtTool), vs, kMgValueMin, kArtLabelW, Weight::Bold);
    for (int t = 0; t < 6; t++)
    {
        std::string tool = Own(rom, OwnWordId::ArtPencil, t);
        if (t < 2) tool += std::string(" (") + Own(rom, OwnWordId::ArtMedium) + ")";
        me.LineAny(id(OwnWordId::ArtPencil) + t, tool, vs, kMgValueMin, iw - kArtLabelW);
    }
    me.LineAny(id(OwnWordId::ArtColourOf), With(Own(rom, OwnWordId::ArtColourOf), 15), vs, kMgValueMin, iw - 52);
    me.LineAny(id(OwnWordId::ArtPage), With(Own(rom, OwnWordId::ArtPage), 16), vs, kMgValueMin, iw);
    me.LineAny(id(OwnWordId::SocialMatch), Own(rom, OwnWordId::SocialMatch), kMgValueSize, kMgValueMin, kMgInnerW - 8);
    // The timing games' panels: MooGoo's round, the players' names (the AI's are Sims' names) and
    // their fruit, the deck; the Drencher's labels and every name a target can have; the crime
    // game's moves and their state.
    me.LineAny(id(OwnWordId::MooGooRound), With(Own(rom, OwnWordId::MooGooRound), 3), kMgValueSize, kMgValueMin, iw);
    for (int n = kStrMgSimName0; n < kStrMgSimName0 + 56; n++) me.LineAny(n, Words(rom, n), vs, kMgValueMin, iw - 150 - 12);
    for (int p = 0; p < kMooGooPlayers; p++)
    {
        me.LineAny(kStrMgMooPlayer1 + p, Words(rom, kStrMgMooPlayer1 + p), vs, kMgValueMin, iw - 150 - 12);
        me.LineAny(kStrMgMooFruit + p, "99 " + Words(rom, kStrMgMooFruit + p), kMgValueSize - 4, kMgValueMin - 2, 150 - 8);
    }
    me.LineAny(kStrMgMooScore, Words(rom, kStrMgMooScore), kMgHeadSize, 13, 150 - 8, Weight::Bold);
    me.LineAny(id(OwnWordId::MooGooCardsLeft), Own(rom, OwnWordId::MooGooCardsLeft), vs, kMgValueMin, iw - 80, Weight::Bold);
    for (OwnWordId w : {OwnWordId::DrencherWater, OwnWordId::DrencherPressure})
        me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, kMgLabelW - 8, Weight::Bold);
    for (OwnWordId w : {OwnWordId::DrencherTarget, OwnWordId::DrencherNone}) me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, iw);
    // (An object's name may end in an ellipsis, as in Pockets: the Italian table has a sentence at
    // the first one, 463; counted apart.)
    for (int n = kStrMgObjectName0; n < kStrMgDrencherObjectEnd; n++)
    {
        const FittedText f = FitLineAny(Words(rom, n), vs, kMgValueMin, iw, Weight::ExtraBold);
        m.checked++;
        if (f.size < vs) m.shrunk++;
        if (!f.fits) { m.namesCut++; if (std::find(m.cutIds.begin(), m.cutIds.end(), n) == m.cutIds.end()) m.cutIds.push_back(n); }
    }
    for (OwnWordId w : {OwnWordId::CrimeScare, OwnWordId::CrimeSubdue, OwnWordId::CrimeAimLabel}) me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, iw - 110, Weight::Bold);
    me.LineAny(kStrMgRatARang, Words(rom, kStrMgRatARang), vs, kMgValueMin, iw - 110, Weight::Bold);
    for (OwnWordId w : {OwnWordId::CrimeReady, OwnWordId::CrimeHit}) me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, 110);
    // The lottery's, the mixer's, its save page's and the lobby's panels.
    me.LineAny(id(OwnWordId::ScratchTopPrize), Own(rom, OwnWordId::ScratchTopPrize), vs, kMgValueMin, iw - 130, Weight::Bold);
    me.LineAny(id(OwnWordId::ScratchOpened), With(Own(rom, OwnWordId::ScratchOpened), 5), vs, kMgValueMin, iw);
    for (OwnWordId w : {OwnWordId::MixerSong, OwnWordId::MixerInstrument, OwnWordId::MixerOctave})
        me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, kMgLabelW + 20, Weight::Bold);
    for (int k : {kStrMgMixTempo, kStrMgMixTracks}) me.LineAny(k, Words(rom, k), vs, kMgValueMin, kMgLabelW + 20, Weight::Bold);
    for (int sg = 0; sg < kMixerSongs; sg++) me.LineAny(kStrMgMixSong0 + sg, Words(rom, kStrMgMixSong0 + sg), vs, kMgValueMin, iw - kMgLabelW - 24);
    me.LineAny(kStrMgMixVox, "4 (" + Words(rom, kStrMgMixVox) + ")", vs, kMgValueMin, iw - kMgLabelW - 24);
    me.LineAny(id(OwnWordId::MixerSustain), std::string("+1 \xC2\xB7 ") + Own(rom, OwnWordId::MixerSustain), vs, kMgValueMin, iw - kMgLabelW - 24);
    for (OwnWordId w : {OwnWordId::MixerPlaying, OwnWordId::MixerRecording, OwnWordId::MixerHelp, OwnWordId::MixerReady}) me.LineAny(id(w), Own(rom, w), vs, kMgValueMin, iw);
    for (int k : {kStrMgMixMic, kStrMgMixSelect, kStrMgMixSaveAsk}) me.LineAny(k, Words(rom, k), vs, kMgValueMin, iw);
    me.LineAny(kStrMgSongTitle, Words(rom, kStrMgSongTitle), vs, kMgValueMin, iw, Weight::Bold);
    me.LineAny(-1, "WWWWWWWWWWWWWWW", kMgValueSize, kMgValueMin, iw);   // (a title of 15 letters, the page's longest)
    me.LineAny(id(OwnWordId::SaveSlot), With(Own(rom, OwnWordId::SaveSlot), 3), kMgValueSize - 4, kMgValueMin - 2, 130, Weight::Bold);
    me.LineAny(kStrMgNoData, Words(rom, kStrMgNoData), vs, kMgValueMin, iw - 146);
    me.Block(id(OwnWordId::TradingNote), Own(rom, OwnWordId::TradingNote), kMgValueSize, kMgValueMin, {0, 0, kMgInnerW - 28, 100});
}

// The minigames' own boxes (BoxOf) at GameBoxAt's places, as BuildGameBox lays them out (the
// popups' BoxBase: the title on its plate, the text in the well at 30 → 20 px), and the game's
// lines the HD frame redraws: the cards' turn line in the side panel, MooGoo's on its plate.
static void MeasureGameBoxes(const RomData& rom, BlockMeasure& m)
{
    const Measurer me {m};
    auto box = [&](int id, Rect cover, int buttons, const std::string& title, const std::string& text) {
        const Rect b = GameBoxAt(ToCanvas(cover, 0), kMgBoxArea, buttons);
        const Rect well = buttons > 0 ? Rect {b.x + 28, b.y + 42, b.w - 56, b.h - 162} : Rect {b.x + 28, b.y + 42, b.w - 56, b.h - 70};
        me.Block(id, text, 30, 20, {well.x + 24, well.y + 10, well.w - 48, well.h - 20});
        // (the plate widens when the title does not hold on it: BoxBase)
        if (!title.empty()) me.LineAny(id, title, 30, 22, FitLineAny(title, 30, 22, b.w - 240 - 40, Weight::ExtraBold).fits ? b.w - 240 - 40 : b.w - 80 - 40);
    };
    // The quit questions (the cards', MooGoo's), the end of a turn (the longest name), the mixer's
    // save question and its microphone, the help texts (the Drencher's, the crime game's, the mixer's
    // title and body).
    box(kStrMgQuitAsk, kMgCardsBox, 2, "", Words(rom, kStrMgQuitAsk));
    box(kStrMgQuitAsk, kMgMooGooBox, 2, "", Words(rom, kStrMgQuitAsk));
    for (const std::string& next : {Words(rom, kStrMgOpponent), Words(rom, kStrMgPlayer), std::string("WWWWWWW")})
        box(kStrMgEndOfTurn, kMgCardsBox, 1, "", MenuText(FirstLine(rom.String(kStrMgEndOfTurn))) + "\n" + MenuWordsWith(rom, kStrMgTurnOf, next));
    box(kStrMgMixSaveAsk, kMgMixerBox, 3, "", Words(rom, kStrMgMixSaveAsk));
    box(kStrMgMixMic, kMgMicBox, 1, "", Words(rom, kStrMgMixMic));
    for (int id = kStrMgDrHelp0; id < kStrMgCrHelp0 + 3; id++) box(id, kMgHelpBox, 1, "", MenuText(rom.String(id)));
    for (int id = kStrMgMixHelp0; id + 1 < kStrMgMixHelpEnd; id += 2) box(id, kMgHelpBox, 1, Words(rom, id), MenuText(rom.String(id + 1)));
    // The cards' turn line over the board (CardsValues) and MooGoo's on its plate (AddMooGooLine).
    for (int id = kStrMgCardsLine0; id <= kStrMgCardsOther; id++) me.LineAny(id, Words(rom, id), 26, kMgValueMin, kMgInnerW - 8);
    const int plateW = ToCanvas(kMgMooLineFrom, 0).w - 32;
    for (int id : {kStrMgMooShuffling, kStrMgMooDealing, kStrMgMooPlayCard, kStrMgMooTarget}) me.LineAny(id, Words(rom, id), 30, 20, plateW);
    me.LineAny(9000 + (int)OwnWordId::MooGooBetLine, Own(rom, OwnWordId::MooGooBetLine), 30, 20, plateW);
}

void MeasureMinigames(const RomData& rom, BlockMeasure& m)
{
    const Measurer me {m};
    for (int id : {kStrMgCards, kStrMgVacuum, kStrMgAutopsy, kStrMgDetector, kStrMgArtGallery, kStrMgMooGoo, kStrMgDrencher, kStrMgRatARang,
                   kStrMgTicket0, kStrMgTicket0 + 1, kStrMgTicket0 + 2, kStrMgMixer, kStrMgBurn, kStrMgWireless})
        me.LineAny(id, Words(rom, id), kMgTitleSize, kMgTitleMin, kMgTitleMaxW - 48);
    // The lottery's prize and result (the widest prize, 100), as its panel fits them.
    me.LineAny(kStrMgMoney, MenuWords(rom, kStrMgMoney, 100), kMgValueSize, kMgValueMin, 130);
    me.Block(kStrMgWon, MenuWords(rom, kStrMgWon, 100), kMgValueSize - 2, kMgValueMin, {0, 0, kMgInnerW - 24, 2 * kMgRow});
    me.Block(kStrMgLost, Words(rom, kStrMgLost), kMgValueSize - 2, kMgValueMin, {0, 0, kMgInnerW - 24, 2 * kMgRow});
    // The pointing games: the autopsy's money (the reader's widest), the detector's "Found:" and
    // every find's name beside it, its earnings' line; every prompt bar fitting at its smallest.
    me.LineAny(kStrMgMoney, MenuWords(rom, kStrMgMoney, 99999999), kMgValueSize, kMgValueMin, 110);
    me.LineAny(kStrMgFound, Words(rom, kStrMgFound), kMgValueSize - 2, kMgValueMin, kMgLabelW - 8, Weight::Bold);
    for (int t = 0; t < kMgFindTypes; t++) me.LineAny(kStrMgFind0 + 2 * t, FindName(rom, t), kMgValueSize - 2, kMgValueMin, kMgInnerW - 24 - kMgLabelW);
    {
        StatState mt;
        mt.metal = 99999999;
        std::string label, value;
        StatStatsLine(mt, rom, 3, label, value);
        const int vw = (int)std::ceil(TextWidth(value, kMgValueSize, Weight::ExtraBold)) + 2;
        me.LineAny(kStrMgDetector, value, kMgValueSize, kMgValueMin, kMgInnerW / 2, Weight::ExtraBold);
        if (!FitLineAny(label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28 - std::min(vw, kMgInnerW / 2) - 10).fits)
            me.LineAny(kStrMgDetector, label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28, Weight::Bold);
        else m.checked++;
    }
    for (int id : {kStrMgCardsCount, kStrMgScore}) me.LineAny(id, Words(rom, id), kMgHeadSize, 13, kColW - 8, Weight::Bold);
    // The names' column (the player's own name is at most 7 letters: the game's keyboard).
    const int nameW = kMgInnerW - 24 - 2 * kColW - 12;
    for (int id : {kStrMgPlayer, kStrMgOpponent}) me.LineAny(id, Words(rom, id), kMgValueSize, kMgValueMin, nameW);
    me.LineAny(-1, "WWWWWWW", kMgValueSize, kMgValueMin, nameW);
    // The treasure's label between the deck's count and the score column.
    const int tx = 12 + 4 + 24 + 10 + 70;
    me.LineAny(kStrMgTreasure, Words(rom, kStrMgTreasure), kMgHeadSize + 2, 13, kMgInnerW - 24 - kColW - tx - 4, Weight::Bold);
    // "Vacuum Earnings:" beside the widest value the reader lets through.
    StatState st;
    st.vacuum = 99999999;
    std::string label, value;
    StatStatsLine(st, rom, 2, label, value);
    const int vw = (int)std::ceil(TextWidth(value, kMgValueSize, Weight::ExtraBold)) + 2;
    me.LineAny(kStrMgVacuumEarnings, value, kMgValueSize, kMgValueMin, kMgInnerW / 2, Weight::ExtraBold);
    // (Beside the value, or above it on a line of its own.)
    if (!FitLineAny(label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28 - std::min(vw, kMgInnerW / 2) - 10).fits)
        me.LineAny(kStrMgVacuumEarnings, label, kMgValueSize - 2, kMgValueMin, kMgInnerW - 28, Weight::Bold);
    else m.checked++;
    MeasureOwnWords(rom, m);
    MeasureGameBoxes(rom, m);
}
}
