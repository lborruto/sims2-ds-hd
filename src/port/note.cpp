#include "port/note.h"

#include "port/panelmenu.h"
#include "port/pockets.h"
#include "romdata.h"
#include "ui/kit.h"

namespace sims2wide
{
namespace
{
using namespace ui;

// Parchment: a warm cream, a brown rim and ink.
constexpr u32 kPaper = 0xFFF0E2B8, kPaperRim = 0xFF8A6A3A, kPaperEdge = 0xFFD8C08A, kInk = 0xFF3A2A18;
constexpr float kNoteSize = 32, kNoteMin = 20;
}

bool NoteUp(const GameState& game) { return game.valid && game.town.note.valid; }

std::string NoteText(const GameState& game, const RomData& rom)
{
    const NoteState& n = game.town.note;
    if (!n.valid) return {};
    return n.playerName ? MenuWordsWith(rom, n.textId, game.hud.simName) : MenuWords(rom, n.textId);
}

MenuView NoteView(ScreenState s, const GameState* game, const RomData* rom)
{
    if (!game || !rom || rom->StringCount() <= kStrClose) return {};
    return NoteViewOf(s, *game, rom);
}

MenuView NoteViewOf(ScreenState s, const GameState& game, const RomData* rom)
{
    MenuView v;
    if (s.screen != Screen::Unknown || !NoteUp(game)) return v;
    // The game closes a note on its own A (or a tap on its X): A and B both press it.
    ViewItem close;
    close.rect = kNoteX;
    close.id = kNoteCloseId;
    close.action = {KeysStep(1u << kA, 2)};
    close.closes = true;
    if (rom) close.word = MenuWords(*rom, kStrClose);
    v.items.push_back(close);
    v.back = close.action;
    v.backCloses = true;
    v.own = true;
    v.valid = true;
    return v;
}

void BuildNote(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons)
{
    const size_t first = c.ui.size();
    UiItem paper = PanelItem(kNotePanel);
    paper.fill = kPaper;
    paper.borderArgb = kPaperEdge;
    paper.border = 10;
    paper.radius = 18;
    c.ui.push_back(paper);
    UiItem rim = PanelItem({kNotePanel.x + 22, kNotePanel.y + 22, kNotePanel.w - 44, kNotePanel.h - 44});
    rim.plain = true;
    rim.fill = 0x208A6A3A; // (a faint inner rule)
    rim.radius = 10;
    c.ui.push_back(rim);
    UiItem in = PanelItem({kNotePanel.x + 26, kNotePanel.y + 26, kNotePanel.w - 52, kNotePanel.h - 52});
    in.plain = true;
    in.fill = kPaper;
    in.radius = 8;
    c.ui.push_back(in);

    const FittedBlock b = FitBlock(NoteText(game, rom), kNoteSize, kNoteMin, kNoteText);
    UiItem t = TextItem(kNoteText, b.text, b.size, Align::Centre, Weight::Bold);
    t.middle = true;
    t.colour = kInk;
    t.shadowArgb = 0;
    t.layer = 1;
    c.ui.push_back(t);
    if (focus == 0)
    {
        AddFocusRing(c, kNoteX, kNoteX.w / 2.0f);
    }
    // The hints: A / B close (the game's word), at the paper's foot, ink on paper.
    const Rect cr = ChipRect(0, 0, Button::B, MenuWords(rom, kStrClose), kPmHintSize, 0, Weight::ExtraBold);
    const Rect ar = ChipRect(0, 0, Button::A, "", kPmHintSize, 0, Weight::ExtraBold);
    const int hy = kNotePanel.y + kNotePanel.h - 70;
    UiItem a = ChipItem(kNoteX.x - 24 - cr.w - 12 - ar.w, hy + (40 - ar.h) / 2, Button::A, "", kPmHintSize, 0, Weight::ExtraBold);
    a.layer = 1;
    c.ui.push_back(a);
    UiItem bb = ChipItem(kNoteX.x - 24 - cr.w, hy + (40 - cr.h) / 2, Button::B, MenuWords(rom, kStrClose), kPmHintSize, 0, Weight::ExtraBold);
    bb.layer = 1;
    bb.colour = kInk;
    bb.shadowArgb = 0;
    c.ui.push_back(bb);

    AddMousePiece(c, {kNoteCloseX - 14, kNoteCloseY - 14, 28, 28}, kNoteX);
    AddSpriteIcon(c, icons, kSprNoteX, 0, 2, kNoteX);
    (void)v;
    SortUi(c.ui, first);
}

void MeasureNotes(const RomData& rom, BlockMeasure& m)
{
    for (int id : {0x4A8, 0x509, 0x657})
    {
        const FittedBlock b = FitBlock(MenuWordsWith(rom, id, "WWWWWWWW"), kNoteSize, kNoteMin, kNoteText);
        m.checked++;
        if (b.size < kNoteSize) m.shrunk++;
        if (b.lines > m.tallestLines) { m.tallestLines = b.lines; m.tallestId = id; }
        if (!b.fits) { m.cut++; m.cutIds.push_back(id); }
    }
}
}
