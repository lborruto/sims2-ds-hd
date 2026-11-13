// Checks the layouts' touch mapping: every DS touch pixel, drawn in each layout at several window
// sizes (HiDPI too), maps back to itself from the centre of where it was drawn; the top screen
// is never touchable, FOCUS's panel wins over the world under it, and a drag past the touch
// screen is held to its edges.
//   ninja -C build layout_test && ./build/layout_test
#include <climits>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "compose.h"
#include "console.h"
#include "hotspots.h"
#include "layout.h"
#include "port/build.h"
#include "port/checkin.h"
#include "port/roominfo.h"
#include "port/music.h"
#include "port/statpanel.h"
#include "port/catalogue.h"
#include "port/cas.h"
#include "port/dialogue.h"
#include "port/front.h"
#include "port/frontmenus.h"
#include "port/gift.h"
#include "port/goals.h"
#include "port/note.h"
#include "port/options.h"
#include "port/ownwords.h"
#include "port/phone.h"
#include "port/popup.h"
#include "port/prompt.h"
#include "port/relations.h"
#include "port/interact.h"
#include "port/minigame.h"
#include "port/panelmenu.h"
#include "port/pockets.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "ui/kit.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void TestLayout(Layout l, bool mainIsTop, float aspect, int ow, int oh)
{
    const Placement p = Place(l, mainIsTop, aspect);
    const CanvasFit fit = CanvasFit::For(p, ow, oh);
    // Each DS pixel from the centre of its drawn area.
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            const float cx = p.bottom.x + (x + 0.5f) * p.bottom.w / SW, cy = p.bottom.y + (y + 0.5f) * p.bottom.h / SH;
            const float px = fit.ox + cx * fit.scale, py = fit.oy + cy * fit.scale;
            int tx = -1, ty = -1;
            const bool hit = TouchFromOutput(p, fit, px, py, tx, ty);
            if (!hit || tx != x || ty != y)
            {
                fprintf(stderr, "layout %s main %d aspect %.2f out %dx%d: pixel %d,%d -> %s %d,%d\n", LayoutName(l), mainIsTop, aspect, ow, oh, x, y, hit ? "hit" : "miss", tx, ty);
                failures++;
                return;
            }
            CHECK(!OverTop(p, fit, px, py));
            int cxT = -1, cyT = -1;
            TouchClamped(p, fit, px, py, cxT, cyT);
            CHECK(cxT == x && cyT == y);
        }
    // A drag past the touch screen's edges is held to them: corners and the middle of each side.
    {
        int tx, ty;
        const float l = fit.ox + (p.bottom.x - 50) * fit.scale, r = fit.ox + (p.bottom.x + p.bottom.w + 50) * fit.scale;
        const float t = fit.oy + (p.bottom.y - 50) * fit.scale, b = fit.oy + (p.bottom.y + p.bottom.h + 50) * fit.scale;
        const float mx = fit.ox + (p.bottom.x + p.bottom.w / 2.0f) * fit.scale, my = fit.oy + (p.bottom.y + p.bottom.h / 2.0f) * fit.scale;
        TouchClamped(p, fit, l, t, tx, ty); CHECK(tx == 0 && ty == 0);
        TouchClamped(p, fit, r, b, tx, ty); CHECK(tx == SW - 1 && ty == SH - 1);
        TouchClamped(p, fit, r, my, tx, ty); CHECK(tx == SW - 1 && abs(ty - SH / 2) <= 1);
        TouchClamped(p, fit, mx, t, tx, ty); CHECK(abs(tx - SW / 2) <= 1 && ty == 0);
    }
    // The first output pixel of the bottom screen, and the first one past its right edge.
    int tx, ty;
    CHECK(TouchFromOutput(p, fit, fit.ox + p.bottom.x * fit.scale + 0.5f, fit.oy + p.bottom.y * fit.scale + 0.5f, tx, ty) && tx == 0 && ty == 0);
    CHECK(!TouchFromOutput(p, fit, fit.ox + (p.bottom.x + p.bottom.w) * fit.scale + 0.5f, fit.oy + p.bottom.y * fit.scale + 0.5f, tx, ty));
    // The top screen's own area (outside the panel) is never a touch, and is "over top".
    const float topX = fit.ox + (p.top.x + 2) * fit.scale, topY = fit.oy + (p.top.y + 2) * fit.scale;
    if (!p.Contains(p.bottom, p.top.x + 2, p.top.y + 2))
    {
        CHECK(!TouchFromOutput(p, fit, topX, topY, tx, ty));
        CHECK(OverTop(p, fit, topX, topY));
    }
    // Outside the canvas: nothing.
    CHECK(!TouchFromOutput(p, fit, fit.ox - 1, fit.oy - 1, tx, ty));
    CHECK(!OverTop(p, fit, fit.ox - 1, fit.oy - 1));
}

// --- Compositions: pieces of the DS screens drawn at rectangles, and the mouse through them.

// Every DS bottom pixel's centre, drawn through the composition and mapped back, is itself.
static void TestComposeRoundtrip(Layout l, bool mainIsTop, float aspect, int ow, int oh)
{
    const int topW = (int)(SH * aspect + 0.5f);
    const Composition c = FromPlacement(Place(l, mainIsTop, aspect), topW, SH);
    const CanvasFit fit = CanvasFit::For(c.cw, c.ch, ow, oh);
    const Piece* b = nullptr;
    for (const Piece& q : c.pieces) if (q.src == Src::Bottom) b = &q;
    CHECK(b);
    if (!b) return;
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++)
        {
            const float cx = b->to.x + (x + 0.5f) * b->to.w / b->from.w, cy = b->to.y + (y + 0.5f) * b->to.h / b->from.h;
            const float px = fit.ox + cx * fit.scale, py = fit.oy + cy * fit.scale;
            int tx = -1, ty = -1;
            if (!TouchFromComposition(c, fit, px, py, tx, ty) || tx != x || ty != y)
            {
                fprintf(stderr, "compose %s main %d aspect %.2f out %dx%d: pixel %d,%d -> %d,%d\n", LayoutName(l), mainIsTop, aspect, ow, oh, x, y, tx, ty);
                failures++;
                return;
            }
        }
}

static Composition TwoBottomPieces()
{
    Composition c {1400, 600, {}, {}};
    c.pieces.push_back({Src::Bottom, {0, 0, SW, 96}, {0, 0, SW * 3, 96 * 3}});
    c.pieces.push_back({Src::Bottom, {0, 96, SW, 96}, {900, 0, SW * 2, 96 * 2}});
    return c;
}

// --- CONSOLE: every DS pixel of every touchable bottom piece of every composition maps back to
// itself from the centre of where it is drawn (click and drag), never as the camera.
static std::vector<ScreenState> AllStates()
{
    std::vector<ScreenState> v;
    for (Screen sc : {Screen::None, Screen::Panel, Screen::Phone, Screen::Dialogue, Screen::Popup, Screen::Gift, Screen::Social,
                      Screen::Vacuum, Screen::Cards, Screen::Catalogue, Screen::Build, Screen::FrontEnd,
                      Screen::CreateASim, Screen::SaveSlots, Screen::Drencher, Screen::Unknown, Screen::Loading, Screen::Autopsy,
                      Screen::Detector, Screen::Art})
        if (sc == Screen::Panel)
            for (Tab t : {Tab::None, Tab::Interact, Tab::Pockets, Tab::Goals, Tab::Relationships, Tab::Options}) v.push_back({sc, t});
        else v.push_back({sc, Tab::None});
    return v;
}

// The status bar's words, made up (no game text here): the game's placeholders in its formats.
static HudStrings TestStrings()
{
    HudStrings h;
    h.ok = true;
    h.clock = "@hh:@mm @am";
    h.am = "am";
    h.pm = "pm";
    h.money = "$@1";
    h.label = "Meter";
    return h;
}
static HudState TestHud()
{
    HudState h;
    h.valid = true;
    h.money = 14843;
    h.hour = 15;
    h.minute = 37;
    h.sanity = 150;
    h.sanityMax = 225;
    h.tier = 2;
    return h;
}

// given: test that composition (the HD conversation's, built apart) instead of BuildConsole's.
static void TestConsoleRoundtrip(ScreenState s, bool menuOpen, int topW, int topH, int ow, int oh, bool withHud, const Composition* given = nullptr)
{
    const HudState hud = TestHud();
    const HudStrings strings = TestStrings();
    const Composition c = given ? *given : withHud ? BuildConsole(s, menuOpen, -1, topW, topH, &hud, &strings) : BuildConsole(s, menuOpen, -1, topW, topH);
    const CanvasFit fit = CanvasFit::For(c.cw, c.ch, ow, oh);
    int bottoms = 0;
    for (int i = 0; i < (int)c.pieces.size(); i++)
    {
        const Piece& q = c.pieces[i];
        if (q.src != Src::Bottom || !q.touch) continue;
        bottoms++;
        CHECK(q.from.x >= 0 && q.from.y >= 0 && q.from.x + q.from.w <= SW && q.from.y + q.from.h <= SH);
        CHECK(q.to.x >= 0 && q.to.y >= 0 && q.to.x + q.to.w <= c.cw && q.to.y + q.to.h <= c.ch);
        for (int y = q.from.y; y < q.from.y + q.from.h; y++)
            for (int x = q.from.x; x < q.from.x + q.from.w; x++)
            {
                const float cx = q.to.x + (x - q.from.x + 0.5f) * q.to.w / q.from.w, cy = q.to.y + (y - q.from.y + 0.5f) * q.to.h / q.from.h;
                const float px = fit.ox + cx * fit.scale, py = fit.oy + cy * fit.scale;
                int tx = -1, ty = -1, idx = -1, dx = -1, dy = -1;
                const bool hit = TouchFromComposition(c, fit, px, py, tx, ty, &idx);
                const bool dragged = TouchClampedComposition(c, i, fit, px, py, dx, dy);
                if (!hit || !dragged || tx != x || ty != y || idx != i || dx != x || dy != y || OverTopComposition(c, fit, px, py))
                {
                    fprintf(stderr, "console %s menu %d top %dx%d out %dx%d: piece %d pixel %d,%d -> %s %d,%d (piece %d), drag %d,%d\n",
                            ScreenStateName(s).c_str(), menuOpen, topW, topH, ow, oh, i, x, y, hit ? "hit" : "miss", tx, ty, idx, dx, dy);
                    failures++;
                    return;
                }
            }
        // A drag that began on the piece and left it is held to the piece's own rectangle.
        int tx, ty;
        CHECK(TouchClampedComposition(c, i, fit, -1000, -1000, tx, ty));
        CHECK(tx == q.from.x && ty == q.from.y);
        CHECK(TouchClampedComposition(c, i, fit, 1e6f, 1e6f, tx, ty));
        CHECK(tx == q.from.x + q.from.w - 1 && ty == q.from.y + q.from.h - 1);
    }
    // A black load (Loading) shows nothing at all: no piece to touch.
    CHECK(s.screen == Screen::Loading ? bottoms == 0 : bottoms > 0);
}

// Every enabled item of v reachable from every other by the pad's moves, as the navigator makes
// them (MenuMove; Left / Right that run an action instead are no moves).
static bool AllReachable(const MenuView& v)
{
    const int n = (int)v.items.size();
    for (int from = 0; from < n; from++)
    {
        if (!v.items[from].enabled) continue;
        std::vector<bool> seen(n, false);
        std::vector<int> todo {from};
        seen[from] = true;
        while (!todo.empty())
        {
            const int i = todo.back();
            todo.pop_back();
            for (int d = 0; d < 4; d++)
            {
                if (d >= 2 && (!(d == 2 ? v.items[i].left : v.items[i].right).empty() ||
                               !v.buttons[(int)(d == 2 ? MenuButton::Left : MenuButton::Right)].empty()))
                    continue;
                const int to = MenuMove(v, i, d);
                if (to >= 0 && !seen[to]) { seen[to] = true; todo.push_back(to); }
            }
        }
        for (int i = 0; i < n; i++)
            if (v.items[i].enabled && !seen[i])
            {
                fprintf(stderr, "menu: item %d not reachable from %d\n", i, from);
                return false;
            }
    }
    return true;
}

// The HD minigame frame (port/minigame.*) on made-up states: the playfield and the top screen where
// milestone 1 put them (the mouse round trips onto the playfield), every UI item on the canvas and
// off both pictures, the side panel inside the column, the prompt bar under the playfield, layers
// ascending, the hints following what the game says (the cards' question and message, the clog).
static void minigame_frame()
{
    const HudState hud = TestHud();
    const HudStrings strings = TestStrings();
    RomData rom;   // no strings: the words empty, the layout the same
    static u32 bottom[SW * SH] = {};
    for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
        for (Screen sc : {Screen::Cards, Screen::Vacuum, Screen::Social, Screen::Autopsy, Screen::Detector, Screen::Art, Screen::MooGoo, Screen::Drencher,
                          Screen::Crime, Screen::Scratch, Screen::Mixer, Screen::MixerSave, Screen::Trading})
            for (int variant = 0; variant < 4; variant++)
            {
                GameState g;
                g.valid = true;
                CardsState& k = g.minigame.cards;
                k.valid = variant > 0;
                k.deck = 40; k.total = 60; k.hand = 10; k.opponent = 10; k.score = -9999; k.opponentScore = 9999; k.treasure = 9999; k.name = "WWWWWWW";
                k.quitAsk = variant == 2;
                k.message = k.box = variant == 3;
                g.minigame.vacuum.valid = variant > 0;
                g.minigame.vacuum.clogged = variant == 2;
                g.minigame.vacuum.earnings = 99999999;
                // The pointing games: 1 a hand alert, 2 the extremes (and the autopsy's quit asked, the
                // detector's find in range, the painting's question), 3 an organ carried.
                AutopsyState& a = g.minigame.autopsy;
                a.valid = variant > 0;
                a.specimen = variant == 2 ? 1 : 0;
                a.tool = variant == 2 ? 2 : 0;
                a.sub = variant == 3 ? 3 : 0;
                a.earnings = variant == 2 ? 99999999 : 84;
                a.handAlert = variant == 1;
                a.organs[0] = {5, 0, false};
                a.organs[1] = {1, variant == 2 ? kAutopsyDestroyed : 900, variant == 1};
                a.organs[2] = {2, 0, false};
                for (int i = 0; i < (variant == 2 ? kAutopsyParasites : 1); i++) a.parasites.push_back({i, 300, -20, 1, true});
                DetectorState& d = g.minigame.detector;
                d.valid = variant > 0;
                d.state = 3;
                d.charge = variant == 2 ? kDetectorFull : 0x99480;
                d.inRange = d.card = variant == 2;
                d.findType = variant == 2 ? 8 : -1;
                d.mismatch = variant == 2 ? 40 : 0;
                d.slider = variant == 2 ? kDetectorSliderMax : 0;
                d.earnings = 99999999;
                ArtState& art = g.minigame.art;
                art.valid = variant > 0;
                art.tool = variant == 2 ? 5 : 0;
                art.brush = 2;
                art.swatch = 14;
                art.page = 15;
                // The timing games: 1 the player's bet (the Drencher's pressure low, every crime move
                // lit), 2 the extremes (MooGoo's question, a target past the names, the aim), 3 the
                // intro (a full tank, a hit).
                MooGooState& mg = g.minigame.mooGoo;
                mg.valid = variant > 0;
                mg.intro = variant == 3;
                mg.quitAsk = variant == 2;
                mg.phase = 0xD;
                mg.sub = variant == 2 ? 5 : 1;
                mg.round = variant == 2 ? 3 : 1;
                mg.deckSize = 54;
                mg.deckNext = variant == 2 ? 0 : 19;
                for (int p = 0; p < kMooGooPlayers; p++)
                {
                    mg.players[p].nameId = variant == 2 ? 99999 : 323 + p;
                    mg.players[p].score = variant == 2 ? 99 : p;
                    mg.players[p].ai = p > 0;
                }
                DrencherState& dr = g.minigame.drencher;
                dr.valid = variant > 0;
                dr.water = variant == 3 ? 100 : 0;
                dr.pressure = variant == 1 ? 10 : 125;
                dr.target = variant == 2 ? 99999 : -1;
                dr.targetType = 0xB;
                CrimeState& cr = g.minigame.crime;
                cr.valid = variant > 0;
                cr.canScare = cr.armed = cr.canSubdue = variant == 1;
                cr.phase = variant == 2 ? 2 : 0;
                cr.angle = variant == 2 ? -90 : 0;
                cr.hit = variant == 3;
                // The lottery: 1 two boxes revealed, one half scratched, 2 won (the game's popup up), 3
                // lost (the quit asked). The mixer: 1 recording, 2 the song's list (its mark), 3 the
                // save question. The save page: 1 a used slot asked over, 2 the longest names.
                ScratchState& sc2 = g.minigame.scratch;
                sc2.valid = variant > 0;
                sc2.tier = variant == 3 ? 2 : 0;
                sc2.prize = variant == 2 ? 100 : 10;
                sc2.state = variant == 2 ? 2 : variant == 3 ? 3 : 1;
                sc2.revealed = variant == 3 ? 5 : variant == 2 ? 3 : 2;
                for (int i = 0; i < kScratchBoxes; i++) sc2.boxes[i] = {i % 7, i < sc2.revealed, i < sc2.revealed ? 1024 : i == 7 ? 400 : 0};
                MixerState& mx = g.minigame.mixer;
                mx.valid = variant > 0;
                mx.phase = variant == 1 ? 2 : variant == 2 ? 7 : variant == 3 ? 0xA : 0;
                mx.song = variant == 3 ? 8 : 7;
                mx.instrument = 5;
                mx.octave = -1;
                mx.sustain = true;
                mx.tracks = 0x5A;
                mx.tempo = 255;
                MixerSaveState& ms = g.minigame.mixerSave;
                ms.valid = variant > 0;
                ms.name = variant == 2 ? "WWWWWWWWWWWWWWW" : "AB";
                for (int j = 0; j < kMixerSaveSlots; j++) { ms.used[j] = j != 1; ms.slotName[j] = variant == 2 ? "WWWWWWWWWWWWWWW" : "Song"; }
                PopupState& pop = g.town.popup;
                pop.valid = (variant == 2 && sc == Screen::Art) || (variant == 2 && sc == Screen::Scratch);
                pop.state = 2;
                pop.kind = 2;
                pop.boxY = 80; pop.boxW = 160; pop.boxTiles = 6;
                PointerView pv;
                pv.shown = true;
                pv.down = variant == 1;
                pv.quitArmed = (variant == 2 && sc == Screen::Autopsy) || (variant == 3 && sc == Screen::Scratch) || (variant == 1 && sc == Screen::MixerSave);
                pv.x = variant == 2 ? SW - 1 : 0;
                pv.y = variant == 2 ? SH - 1 : 0;
                if (sc == Screen::MooGoo && variant == 1) pv.mark = MooGooMonkeyRect(5);
                if (sc == Screen::Mixer && variant == 2) pv.mark = MixerRowRect(7);
                // (the backdrop and the two pictures' frames; the cards: no top screen, its frame neither)
                const size_t overlays0 = sc == Screen::Cards ? 2 : 3;
                const size_t pieces0 = sc == Screen::Cards ? 1 : 2;
                Composition c {kConsoleW, kConsoleH, {}, {}};
                BuildMinigame(c, {sc, Tab::None}, top.first, top.second, &hud, &strings, g, rom, bottom, nullptr, &pv, sc == Screen::MixerSave ? (variant == 1 ? 30 : 27) : -1);
                // The playfield and the top screen; an HD box over the playfield (port/popup.h's
                // BuildGameBox: a minigame's question, message or help) adds its buttons' mouse
                // pieces, inside the box's area.
                CHECK(c.pieces.size() >= pieces0 && c.pieces[0].src == Src::Bottom && c.pieces[0].to.x == kActivityBottomTo.x && c.pieces[0].to.y == kActivityBottomTo.y &&
                      c.pieces[0].to.w == kActivityBottomTo.w && c.pieces[0].to.h == kActivityBottomTo.h);
                const Piece* topPiece = nullptr;
                std::vector<Piece> pictures {c.pieces[0]};
                for (size_t i = 1; i < c.pieces.size(); i++)
                {
                    if (c.pieces[i].src == Src::Top) { CHECK(!topPiece); topPiece = &c.pieces[i]; pictures.push_back(c.pieces[i]); continue; }
                    const Rect q = c.pieces[i].to;
                    CHECK(q.x >= kMgBoxArea.x && q.y >= kMgBoxArea.y && q.x + q.w <= kMgBoxArea.x + kMgBoxArea.w && q.y + q.h <= kMgBoxArea.y + kMgBoxArea.h);
                }
                // (The cards: the game's score board in the DS's font not shown, the side panel in its place.)
                CHECK((topPiece != nullptr) == (sc != Screen::Cards));
                if (!topPiece && sc != Screen::Cards) continue;
                const Rect topTo = topPiece ? topPiece->to : Rect {kActivityTopX, kActivityTopY - kMgSideGap, kActivityTopW, 0};
                if (topPiece)
                {
                    CHECK(topTo.x == kActivityTopX && topTo.y == kActivityTopY && topTo.w == kActivityTopW);
                    // (the social game's top screen without the game's own bar rows: kMgSocialBarRows)
                    const int skip = sc == Screen::Social ? top.second * kMgSocialBarRows / SH : 0;
                    CHECK(topPiece->from.y == skip && topPiece->from.h == HudCropRows(top.second) - skip);
                }
                bool box = false;
                int layer = -1, prompt = 0, lit = 0, chips = 0, pointer = 0;
                bool images = false;
                for (const ui::UiItem& it : c.ui)
                {
                    const Rect r = it.to;
                    CHECK(r.x >= 0 && r.y >= 0 && r.x + r.w <= kConsoleW && r.y + r.h <= kConsoleH);
                    if (it.layer == kMgPointerLayer && it.kind == ui::UiItem::Panel)
                    {
                        // The pointer: over the playfield (its frame), its own layer, the last still one.
                        pointer++;
                        CHECK(r.x >= kActivityBottomTo.x - 16 && r.y >= kActivityBottomTo.y - 16 && r.x + r.w <= kActivityBottomTo.x + kActivityBottomTo.w + 16 &&
                              r.y + r.h <= kActivityBottomTo.y + kActivityBottomTo.h + 16);
                        CHECK(!images && it.layer >= layer);
                        layer = it.layer;
                        continue;
                    }
                    // An HD box (and the status bar lifted above its veil): over the playfield by design.
                    if (it.layer >= kPopLayer) { box = true; continue; }
                    // Off both pictures (their frames' 6 pixels too).
                    for (const Piece& q : pictures)
                        CHECK(r.x + r.w <= q.to.x - 6 || r.x >= q.to.x + q.to.w + 6 || r.y + r.h <= q.to.y - 6 || r.y >= q.to.y + q.to.h + 6);
                    if (it.kind == ui::UiItem::Image) { images = true; CHECK(r.x >= kMgSideX && r.x + r.w <= kMgSideX + kMgSideW && r.y + r.h <= kMgSideBottom); continue; }
                    CHECK(!images && it.layer >= layer);   // still layers ascending, the images last
                    layer = it.layer;
                    if (r.y >= kMgPrompt.y) { prompt++; CHECK(r.y + r.h <= kMgPrompt.y + kMgPrompt.h && r.x >= kMgPrompt.x - 10 && r.x + r.w <= kMgPrompt.x + kMgPrompt.w); }
                    else if (r.x >= kMgSideX - 2 && r.y > topTo.y + topTo.h) CHECK(r.x + r.w <= kMgSideX + kMgSideW && r.y + r.h <= kMgSideBottom);
                    if (it.kind == ui::UiItem::Panel && it.plain && it.fill == 0xFFB8323A) lit++;
                    if (it.kind == ui::UiItem::Chip && r.y >= kMgPrompt.y) chips++;
                }
                // (The save page's part unread with the RAM reading: no hints.)
                CHECK((prompt > 0) == !(sc == Screen::MixerSave && variant == 0));
                // The playfield and the top screen alone, unless an HD box adds its buttons' pieces.
                CHECK(c.pieces.size() == pieces0 || box);
                // The clog lights the blow's hint; the cards' question has two chips (Yes, No), the message one.
                const bool litNow = (sc == Screen::Vacuum && variant == 2) || (sc == Screen::Autopsy && variant > 0) || (sc == Screen::Detector && variant == 2) ||
                                    (sc == Screen::Drencher && variant == 1) || (sc == Screen::Crime && variant == 2) ||
                                    (sc == Screen::Scratch && variant == 3) || (sc == Screen::Mixer && variant == 1) || (sc == Screen::MixerSave && variant == 1);
                // (The save page with a slot asked over: its row lit too, beside A's hint.)
                CHECK(lit == (sc == Screen::Crime && variant == 1 ? 3 : sc == Screen::MixerSave && variant == 1 ? 2 : litNow ? 1 : 0));
                // (shown: the navigator shows it on the pointing games and the crime game's aim; never
                // under an HD box, whose answers are the buttons')
                CHECK(pointer == (OwnScheme(sc) && !box ? 2 : 0));
                // MooGoo's focus: one ring over the playfield, around the monkey (an overlay).
                // (the kit ring's look: two outlines, the green and the white line)
                CHECK(c.overlays.size() == overlays0 + ((sc == Screen::MooGoo && variant == 1) || (sc == Screen::Mixer && variant == 2) ? 2 : 0));
                // The lottery's popup: A alone (the game's Close); the mixer's save question (an HD
                // box since task 3 of the polish, its focus on No): the D-pad, A, B, X; the wireless
                // lobby: B alone.
                if (sc == Screen::Scratch && variant == 2) CHECK(chips == 1);
                if (sc == Screen::Mixer && variant == 3) CHECK(box && chips == 4);
                if (sc == Screen::Trading) CHECK(chips == 1);
                if (sc == Screen::MooGoo && variant == 1)
                {
                    const Rect o = c.overlays.back().to;
                    CHECK(o.x >= kActivityBottomTo.x + (56 + 32 * 5) * 3 - 8 && o.x + o.w <= kActivityBottomTo.x + (80 + 32 * 5) * 3 + 8 && o.y >= kActivityBottomTo.y + 32 * 3 - 8);
                }
                // The questions (MooGoo's, the painting's, the cards'; No focused first): the D-pad, A
                // the focused answer, B No.
                if (sc == Screen::MooGoo && variant == 2) CHECK(box && chips == 3);
                if (sc == Screen::Art && variant == 2) CHECK(chips == 3);
                // The cards: nothing known (the table's prompts), the draw step (X, B), the question,
                // the end of a turn (A).
                if (sc == Screen::Cards) CHECK(chips == (variant == 0 ? 4 : variant == 1 ? 2 : variant == 2 ? 3 : 1));
                if (sc == Screen::Cards && variant >= 2) CHECK(box);
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                    TestConsoleRoundtrip({sc, Tab::None}, false, top.first, top.second, o.first, o.second, true, &c);
            }
    // Without the game's strings or RAM: milestone 1's activity layout (the 5 x 7 hints' box).
    CHECK(!MinigameFramed({Screen::Cards, Tab::None}, nullptr, nullptr));
    GameState g;
    g.valid = true;
    CHECK(!MinigameFramed({Screen::Cards, Tab::None}, &g, &rom));
}

// Task 5: Create-a-Sim's name typed on the keyboard (CasTypeAction; the key matching needs the
// game's strings: console_check's pad-cas-type) and fast boot's rule (BootPassesBy).
static void platform()
{
    RomData rom; // no strings: no key matches
    CasState c;
    c.valid = true;
    c.page = 0;
    c.nameKnown = true;
    c.name = "Sim";
    c.nameLength = 3;
    c.keyboard = 0;
    CasType t = CasTypeAction(c, rom, "\b");
    CHECK(t.done && t.taps.size() == 1 && t.taps[0].x == kCasBackX && t.taps[0].y == kCasBackY);
    t = CasTypeAction(c, rom, "a");   // (no key: dropped, nothing tapped)
    CHECK(t.done && t.taps.empty());
    CasState e = c;
    e.name.clear();
    e.nameLength = 0;
    CHECK(CasTypeAction(e, rom, "\b").taps.empty());   // nothing to erase
    CasState full = c;
    full.nameLength = kCasName;
    CHECK(CasTypeAction(full, rom, "a").taps.empty() && CasTypeAction(full, rom, "a").done);
    for (int page = 1; page <= 4; page++)
    {
        CasState o = c;
        o.page = page;
        CHECK(CasTypeAction(o, rom, "\b").taps.empty());   // only on the Name page
    }
    CasState sw = c;
    sw.nameKnown = false;
    CHECK(CasTypeAction(sw, rom, "\b").taps.empty());
    // The other case through the game's charset pairs (not ASCII only): É / é, Ä / ä, Ñ / ñ; a
    // capital the game has no small letter for, and the signs, stay as they are.
    CHECK(GameLower("ABCxyz19") == "abcxyz19");
    CHECK(GameLower("\xC3\x89") == "\xC3\xA9" && GameLower("\xC3\x84") == "\xC3\xA4" && GameLower("\xC3\x91") == "\xC3\xB1");
    CHECK(GameLower("\xC3\xA9") == "\xC3\xA9" && GameLower("\xC3\x9F") == "\xC3\x9F");   // é, ß
    CHECK(GameLower("\xC3\x9B") == "\xC3\x9B");   // Û: the game has û but not Û (no pair)
    CHECK(GameLower("\xC3\x97") == "\xC3\x97" && GameLower("\xC2\xA7") == "\xC2\xA7"); // ×, §
    // Fast boot: nothing up yet, the video, the legal / EA / Maxis splashes, the zoom and the blank
    // go by; the last splash, a popup splash, the title or any other screen stop it.
    GameState g;
    CHECK(BootPassesBy(g));
    g.front.kind = FrontKind::Video;
    CHECK(BootPassesBy(g));
    g.front.boot.valid = true;
    for (int mode = 0; mode <= 6; mode++)
    {
        g.front.boot.mode = mode;
        CHECK(BootPassesBy(g) == (mode != 0 && mode != 4));
    }
    g.front.boot.valid = false;
    for (FrontKind k : {FrontKind::Title, FrontKind::MainMenu, FrontKind::Settings, FrontKind::Load, FrontKind::CreateASim, FrontKind::KeepSim})
    {
        g.front.kind = k;
        CHECK(!BootPassesBy(g));
    }
}

// The UI's size (ui::UiScale, console.cpp's groups) at 60, 100 and 150 %, in five languages, on
// made-up states drawn by BuildConsole with a made-up string table: the town's strip (the status
// at its left, the hints at its right, a payout; the pad's look and the keyboard's, whose camera
// hint is its own L / R keys), the panel's HD menu, a popup over it, a lone popup over the world,
// the conversation, the stick cursor's screen. Every item (as drawn: ui::Placed) inside the canvas;
// the strip's inside the strip below its band, the status clear of the hints; the menus above the
// strip, never larger than at 100 %; a lone popup above the strip, as large as the UI's size; the
// stick cursor's hints, pause panel and status bar clear of its touch screen's frame; the mouse's
// pieces round trip at every window size; at 100 % the items as they always were (no zoom). The whole-canvas screens (the front end, Create-a-Sim) keep their layout.
static void ui_scale()
{
    std::vector<u32> face(SW * SH);
    for (size_t i = 0; i < face.size(); i++) face[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
    const HudStrings strings = TestStrings();
    HudState hud = TestHud();
    hud.money = 99999999; // (the widest the bar shows)
    for (int lang = 1; lang <= 5; lang++)
    {
        RomData rom;
        std::vector<std::string> words(3127);
        for (size_t i = 0; i < words.size(); i++) words[i] = "Word " + std::to_string(i) + (i % 7 == 0 ? " longer than the others" : "");
        rom.SetStringsForTest(words, lang);
        for (bool keys : {false, true})
        {
            ui::ChipLook look;
            if (keys)
            {
                look.face = ui::Face::Keyboard;
                look.glyph[(int)ui::Button::LB] = "Q";
                look.glyph[(int)ui::Button::RB] = "W";
                look.glyph[(int)ui::Button::Back] = "E";
                look.glyph[(int)ui::Button::A] = "Return";
                look.glyph[(int)ui::Button::B] = "Backspace";
                look.glyph[(int)ui::Button::Pause] = "Escape";
            }
            ui::SetChipLook(look);
            for (float z : {0.6f, 1.0f, 1.5f})
            {
                ui::SetUiScale(z);
                auto inCanvas = [&](const Composition& c, const char* what) {
                    for (const ui::UiItem& it : c.ui)
                    {
                        const Rect r = ui::Placed(it);
                        if (r.x < 0 || r.y < 0 || r.x + r.w > kConsoleW || r.y + r.h > kConsoleH)
                        {
                            fprintf(stderr, "ui %d%% lang %d keys %d: %s item at %d,%d %dx%d off the canvas\n", (int)(z * 100), lang, keys, what, r.x, r.y, r.w, r.h);
                            failures++;
                            return;
                        }
                        if (z == 1) CHECK(it.zoom == 1 && it.dx == 0 && it.dy == 0);
                    }
                };
                // The town: the strip, the status (and a payout) left, the hints right.
                GameState g;
                g.valid = true;
                g.screen = GScreen::Panel;
                g.hud = hud;
                for (int payout : {0, -1234567})
                {
                    const Composition town = BuildConsole({Screen::Panel, Tab::Interact}, false, -1, 341, 192, &hud, &strings, &g, &rom, face.data(),
                                                          nullptr, 0, nullptr, payout);
                    inCanvas(town, "town");
                    int statusRight = 0, hintsLeft = kConsoleW, chips = 0, keycaps = 0;
                    for (const ui::UiItem& it : town.ui)
                    {
                        if (it.to.x <= 0 && it.to.x + it.to.w >= kConsoleW) continue; // (the strip itself)
                        const Rect r = ui::Placed(it);
                        CHECK(r.y >= kStripY + 1 + kStripBand && r.y + r.h <= kConsoleH);
                        if (it.kind == ui::UiItem::Chip)
                        {
                            hintsLeft = std::min(hintsLeft, r.x);
                            chips++;
                            keycaps += it.face == ui::Face::Keyboard;
                        }
                        else statusRight = std::max(statusRight, r.x + r.w);
                    }
                    CHECK(chips == (keys ? 3 : 2) && keycaps == (keys ? 3 : 0));
                    if (statusRight + 12 > hintsLeft)
                    {
                        fprintf(stderr, "ui %d%% lang %d keys %d payout %d: the status (to %d) meets the hints (from %d)\n", (int)(z * 100), lang, keys,
                                payout, statusRight, hintsLeft);
                        failures++;
                    }
                }
                // The panel's HD menu (Interact) and a popup over it; the cursor's screen.
                GameState m = g;
                InteractState& in = m.town.interact;
                in.valid = true;
                in.target = {TargetRef::Object, 29, 492, 6638, false};
                in.prev = {TargetRef::Sim, 3, 326, 1, false};
                in.next = {TargetRef::Object, 15, 478, 5744, false};
                for (int i = 0; i < 4; i++) in.actions.push_back({i, 1845 + i, i == 2});
                GameState p = m;
                p.screen = GScreen::Popup;
                p.tab = 0; // (over the Interact tab's menu)
                p.town.popup = {true, 2, 2, std::string(300, 'w'), "stale", -1, false, 56, 160, 6};
                p.town.options = {true, 4096, 2048};
                const ScreenState pan {Screen::Panel, Tab::Interact}, pop {Screen::Popup, Tab::None};
                const Composition menu = BuildConsole(pan, true, 1, 341, 192, &hud, &strings, &m, &rom, face.data());
                const Composition popup = BuildConsole(pop, true, 0, 341, 192, &hud, &strings, &p, &rom, face.data());
                const Composition cursor = BuildConsole({Screen::Unknown, Tab::None}, false, -1, SW, SH, &hud, &strings);
                for (const auto* c : {&menu, &popup})
                {
                    inCanvas(*c, c == &menu ? "menu" : "popup");
                    bool zoomed = false;
                    for (const ui::UiItem& it : c->ui)
                    {
                        if (it.to.x <= 0 && it.to.x + it.to.w >= kConsoleW) continue;
                        const Rect r = ui::Placed(it);
                        if (it.layer != kStatusBarLayer && it.layer != kPopLayer + 1) CHECK(r.y + r.h <= kStripY);
                        CHECK(it.zoom <= 1.0f + 1e-6f || it.layer == kStatusBarLayer || it.layer == kPopLayer + 1);
                        if (it.layer != kStatusBarLayer && it.layer != kPopLayer + 1) zoomed |= it.zoom != 1; // (the menu's, not the strip's)
                    }
                    CHECK(zoomed == (z < 1));
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(c == &menu ? pan : pop, true, 341, 192, o.first, o.second, true, c);
                }
                inCanvas(cursor, "cursor");
                {
                    // The touch screen's frame (FrameAround kMenuTo: 6 pixels) and every item clear
                    // of it: the hints and the status bar below, the pause panel at its right.
                    const Rect frame {kMenuTo.x - 6, kMenuTo.y - 6, kMenuTo.w + 12, kMenuTo.h + 12};
                    bool framed = false;
                    for (const Overlay& o : cursor.overlays) framed |= o.to.x == frame.x && o.to.y == frame.y && o.to.w == frame.w && o.to.h == frame.h;
                    CHECK(framed);
                    int panels = 0;
                    for (const ui::UiItem& it : cursor.ui)
                    {
                        if (it.to.x <= 0 && it.to.x + it.to.w >= kConsoleW) continue;
                        const Rect r = ui::Placed(it);
                        panels += it.kind == ui::UiItem::Panel;
                        if (r.x < frame.x + frame.w && r.x + r.w > frame.x && r.y < frame.y + frame.h && r.y + r.h > frame.y)
                        {
                            fprintf(stderr, "ui %d%% lang %d keys %d: cursor item at %d,%d %dx%d over the frame\n", (int)(z * 100), lang, keys, r.x, r.y, r.w, r.h);
                            failures++;
                        }
                    }
                    CHECK(panels >= 3); // (the hints', the pause's, the status bar's)
                }
                // A lone popup (over the dimmed world, no menu beneath): its own group, as large as
                // the UI's size (as far as the canvas above the strip holds it).
                {
                    GameState q = g;
                    q.screen = GScreen::Popup;
                    q.tab = -1; // (no tab: nothing beneath)
                    q.town.popup = {true, 2, 2, "A goal reached.", "stale", -1, false, 56, 160, 6};
                    const Composition lone = BuildConsole(pop, false, 0, 341, 192, &hud, &strings, &q, &rom, face.data());
                    inCanvas(lone, "lone popup");
                    float most = 0, least = 9;
                    for (const ui::UiItem& it : lone.ui)
                    {
                        if (it.to.x <= 0 && it.to.x + it.to.w >= kConsoleW) continue;
                        if (it.layer == kStatusBarLayer || it.layer == kPopLayer + 1) continue; // (the strip's)
                        const Rect r = ui::Placed(it);
                        CHECK(r.y + r.h <= kStripY);
                        most = std::max(most, it.zoom);
                        least = std::min(least, it.zoom);
                    }
                    CHECK(least <= most);
                    if (z < 1) CHECK(std::abs(most - z) < 1e-4f && std::abs(least - z) < 1e-4f);
                    else if (z == 1) CHECK(most == 1 && least == 1);
                    else CHECK(least > 1.1f && most <= z + 1e-4f);
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(pop, false, 341, 192, o.first, o.second, true, &lone);
                }
                // The conversation: about the canvas's bottom centre, 60 % of its width at 60 %.
                GameState d = g;
                d.screen = GScreen::Dialogue;
                d.dialogue.valid = true; d.dialogue.page = 1; d.dialogue.pages = 2; d.dialogue.stringId = 7; d.dialogue.speaker = 0x40;
                const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, -1, 341, 192, &hud, &strings, &d, &rom, face.data());
                inCanvas(dlg, "dialogue");
                int x0 = kConsoleW, x1 = 0, strip = 0;
                for (const ui::UiItem& it : dlg.ui)
                {
                    // (The status strip's panel under the box, the canvas's width: the world stops at it, no black band.)
                    if (it.to.x == 0 && it.to.w == kConsoleW && it.to.y >= kStripY) { strip++; CHECK(it.layer == 0); continue; }
                    x0 = std::min(x0, ui::Placed(it).x);
                    x1 = std::max(x1, ui::Placed(it).x + ui::Placed(it).w);
                }
                CHECK(strip > 0);
                const float want = z < 1 ? 1060 * z : z == 1 ? 1060 : 1060 * (636.0f / 530);
                CHECK(std::abs((x1 - x0) - want) < 40 && std::abs((x0 + x1) / 2.0f - 640) < 4);
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                    TestConsoleRoundtrip({Screen::Dialogue, Tab::None}, false, 341, 192, o.first, o.second, true, &dlg);
            }
        }
    }
    ui::SetChipLook({});
    ui::SetUiScale(1);
}

int main()
{
    ui_scale();
    platform();
    minigame_frame();
    // CONSOLE's compositions.
    for (ScreenState s : AllStates())
        for (bool menuOpen : {false, true})
            for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                    for (bool withHud : {false, true})
                        TestConsoleRoundtrip(s, menuOpen, top.first, top.second, o.first, o.second, withHud);
    // Where the mockup puts things.
    {
        const Composition town = BuildConsole({Screen::Panel, Tab::Goals}, false, -1, 341, 192);
        CHECK(town.cw == kConsoleW && town.ch == kConsoleH);
        CHECK(town.pieces.size() == 3 && town.pieces[0].src == Src::Top && town.pieces[0].to.w == kConsoleW && town.pieces[0].to.h == kConsoleH);
        CHECK(town.pieces[2].from.x == 0 && town.pieces[2].from.y == 6 && town.pieces[2].from.w == 35 && town.pieces[2].to.w == 70 && town.pieces[2].to.h == 368);
        CHECK(town.overlays.size() == 1 && town.overlays[0].argb == kFrameArgb && town.overlays[0].borderArgb == kOutlineArgb && town.overlays[0].border == 6);
        const Rect f = town.overlays[0].to;
        CHECK(f.y + f.h / 2 == kConsoleH / 2 && f.x + f.w <= kConsoleW);
        // The DS's 4:3 world: 960 wide, centred.
        const Composition t43 = BuildConsole({Screen::Panel, Tab::Interact}, false, -1, SW, SH);
        CHECK(t43.pieces[0].to.x == 160 && t43.pieces[0].to.w == 960);
        // The phone is a menu: the touch screen 3x over the dimmed world.
        const Composition phone = BuildConsole({Screen::Phone, Tab::None}, false, -1, SW, SH);
        CHECK(phone.pieces.size() == 2 && !phone.pieces[0].touch && phone.pieces[1].to.x == 256 && phone.pieces[1].to.w == SW * 3);
        const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, -1, SW, SH);
        CHECK(dlg.pieces[0].from.h == 176);
        CHECK(dlg.overlays[0].to.x == 110 && dlg.overlays[0].to.y == 456 && dlg.overlays[0].to.w == 1060);
        CHECK(dlg.pieces.size() == 5);
        const Composition act = BuildConsole({Screen::Vacuum, Tab::None}, false, -1, SW, SH);
        CHECK(act.pieces[0].src == Src::Bottom && act.pieces[0].to.x == 36 && act.pieces[0].to.y == 68 && act.pieces[0].to.w == SW * 3);
        CHECK(act.pieces[1].src == Src::Top && act.pieces[1].to.x == 858 && act.pieces[1].to.w == 384 && act.pieces[1].to.h == 288);
    }
    // The HD status bar: the game's bar cropped off the world (its last 16 of 192 rows, whatever
    // the picture's size), the world keeping its shape; the bar at the bottom left of the world,
    // inside it and the canvas, below the menu's frame; under the small world of the activities
    // that keep the HUD, not the cards'; nothing without the game's words or values.
    {
        const HudState hud = TestHud();
        const HudStrings strings = TestStrings();
        for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
        {
            const int rows = HudCropRows(top.second);
            for (ScreenState st : {ScreenState {Screen::Panel, Tab::Goals}, ScreenState {Screen::Phone, Tab::None}, ScreenState {Screen::Popup, Tab::None},
                                   ScreenState {Screen::Gift, Tab::None}, ScreenState {Screen::SaveSlots, Tab::None}})
                for (bool menuOpen : {false, true})
                {
                    const Composition c = BuildConsole(st, menuOpen, -1, top.first, top.second, &hud, &strings);
                    const Piece& w = c.pieces[0];
                    // The whole width of the picture (the crop never cuts its sides), at its shape
                    // (within a pixel's rounding): 720 high at 4:3, the canvas's width and
                    // top-aligned when wider (widescreen: 1280 x 661), the bar below it.
                    CHECK(w.src == Src::Top && w.from.x == 0 && w.from.y == 0 && w.from.w == top.first && w.from.h == rows);
                    CHECK(std::abs(w.to.w * w.from.h - w.to.h * w.from.w) <= w.from.h + w.from.w);
                    CHECK(w.to.y == 0 && w.to.x >= 0 && w.to.x + w.to.w <= kConsoleW && w.to.h <= kConsoleH);
                    CHECK(top.first * 3 == top.second * 4 ? w.to.h == kConsoleH : (w.to.w == kConsoleW && w.to.h == 661));
                    // The bar's items (its layer); the stick cursor's screens also have the hint's
                    // chips panel at the bottom right (right of the bar) and the pause chip's panel
                    // above it, right of the touch screen's frame (layer 0).
                    size_t first = 0;
                    while (first < c.ui.size() && c.ui[first].layer != kStatusBarLayer) first++;
                    CHECK(first < c.ui.size());
                    if (first >= c.ui.size()) continue;
                    // The town (no menu open): the strip, the canvas's whole width from kStripY to its
                    // foot (its fill, then its outline and band along its top), inside the canvas; the
                    // world down to it (widescreen: no black between); the status's items in the
                    // strip below its band, at the left, no panel of their own; the hints (none here:
                    // no HD view without RAM) at the right clear of it.
                    if (st.screen == Screen::Panel && !menuOpen)
                    {
                        CHECK(c.ui.size() >= first + 3);
                        if (c.ui.size() < first + 3) continue;
                        const Rect sp = c.ui[first].to;
                        CHECK(c.ui[first].plain && sp.x == 0 && sp.w == kConsoleW && sp.y == kStripY && sp.y + sp.h == kConsoleH);
                        CHECK(c.ui[first + 1].to.y == kStripY && c.ui[first + 1].to.w == kConsoleW && c.ui[first + 2].to.y == kStripY + 1 && c.ui[first + 2].to.w == kConsoleW);
                        CHECK(top.first * 3 == top.second * 4 || w.to.y + w.to.h == kStripY);
                        int chips = 0, statusRight = 0, chipsLeft = kConsoleW;
                        for (size_t i = first + 3; i < c.ui.size(); i++)
                        {
                            const Rect r = c.ui[i].to;
                            CHECK(c.ui[i].layer == kStatusBarLayer);
                            CHECK(r.x >= 16 && r.x + r.w <= kConsoleW - 16 && r.y >= kStripY + 1 + kStripBand && r.y + r.h <= kConsoleH - 2);
                            if (c.ui[i].kind == ui::UiItem::Chip) { chips++; chipsLeft = std::min(chipsLeft, r.x); }
                            else statusRight = std::max(statusRight, r.x + r.w);
                            CHECK(c.ui[i].kind != ui::UiItem::Panel || c.ui[i].plain || r.h < kStatusBarH); // (no panel of its own: the strip is)
                        }
                        CHECK(chips == 0 || chips == 2);
                        CHECK(statusRight > 200 && statusRight < chipsLeft);
                        continue;
                    }
                    const Rect b = c.ui[first].to;
                    CHECK(b.y == kStatusBarY && b.h == kStatusBarH && b.x >= std::max(w.to.x, 0) && b.x + b.w <= std::min(w.to.x + w.to.w, kConsoleW));
                    CHECK(b.y > kMenuTo.y + kMenuTo.h + 6);
                    for (size_t i = 0; i < c.ui.size(); i++)
                    {
                        const Rect r = c.ui[i].to;
                        if (i >= first)
                            CHECK(r.x >= b.x && r.y >= b.y && r.x + r.w <= b.x + b.w && r.y + r.h <= b.y + b.h);
                        else
                        {
                            CHECK(st.screen == Screen::SaveSlots && c.ui[i].layer == 0);
                            CHECK(r.x + r.w <= kConsoleW && r.y + r.h <= kConsoleH);
                            // never over the bar or the frame
                            CHECK(r.x > b.x + b.w || (r.x > kMenuTo.x + kMenuTo.w && r.y + r.h < b.y));
                            CHECK(r.y >= kMenuTo.y + kMenuTo.h + 6 || r.x > kMenuTo.x + kMenuTo.w);
                        }
                    }
                }
            for (Screen sc : {Screen::Social, Screen::Vacuum, Screen::Catalogue, Screen::Build, Screen::Cards})
            {
                const Composition c = BuildConsole({sc, Tab::None}, false, -1, top.first, top.second, &hud, &strings);
                const Piece& w = c.pieces[1];
                CHECK(w.src == Src::Top && w.from.h == (sc == Screen::Cards ? top.second : rows));
                bool bar = false;
                for (const ui::UiItem& it : c.ui)
                    if (it.kind == ui::UiItem::Panel && it.to.h == kStatusBarH)
                    {
                        bar = true;
                        CHECK(it.to.y >= w.to.y + w.to.h + 6 && it.to.y + it.to.h + 8 <= kHintBox.y && it.to.x + it.to.w <= kConsoleW);
                    }
                CHECK(bar == (sc != Screen::Cards));
            }
            // The conversation as before (its own box hides the bar), and without the words or
            // without RAM's values, milestone 1's world, the game's HUD in it.
            CHECK(BuildConsole({Screen::Dialogue, Tab::None}, false, -1, top.first, top.second, &hud, &strings).ui.empty());
            HudStrings none;
            const Composition m1 = BuildConsole({Screen::Panel, Tab::Goals}, false, -1, top.first, top.second, &hud, &none);
            CHECK(m1.pieces[0].from.h == top.second && m1.ui.empty());
            CHECK(BuildConsole({Screen::Panel, Tab::Goals}, false, -1, top.first, top.second, nullptr, &strings).pieces[0].from.h == top.second);
        }
        // The values: 12-hour clock, the money grouped, the simoleon sign for the game's '$'.
        std::string clock, money;
        float fill = 0;
        CHECK(FormatHud(hud, strings, clock, money, fill) && clock == "3:37 pm" && money == "\xC2\xA7" "14,843" && std::abs(fill - 150.0f / 225) < 1e-6f);
        HudState h = hud;
        h.hour = 0; h.minute = 5;
        CHECK(FormatHud(h, strings, clock, money, fill) && clock == "12:05 am");
        h.hour = 12;
        CHECK(FormatHud(h, strings, clock, money, fill) && clock == "12:05 pm");
        CHECK(GroupThousands(0) == "0" && GroupThousands(999) == "999" && GroupThousands(1000) == "1,000" && GroupThousands(12345678) == "12,345,678");
        // Escapes and unknown placeholders in the words never show.
        HudStrings odd = strings;
        odd.clock = "{F012}@hh:@mm @am @zz";
        odd.money = "@1\xC2\xA0$";
        CHECK(FormatHud(hud, odd, clock, money, fill) && clock == "3:37 pm" && money == "14,843\xC2\xA0\xC2\xA7");
        // Garbage (a load's): not valid, out of range, max 0, huge: nothing drawn, no division.
        std::vector<HudState> bad;
        for (int i = 0; i < 9; i++) bad.push_back(hud);
        bad[0].valid = false;
        bad[1].hour = 24;
        bad[2].minute = -1;
        bad[3].tier = 7;
        bad[4].sanityMax = 0;
        bad[5].money = -5;
        bad[6].money = INT_MAX;
        bad[7].sanity = -1;
        bad[8].hour = INT_MIN;
        for (const HudState& g : bad)
        {
            Composition c {kConsoleW, kConsoleH, {}, {}};
            const Rect r = AddStatusBar(c, g, strings, 100, 600, 600);
            CHECK(c.ui.empty() && r.w == 0);
        }
        // Valid but extreme (sanity over max, the most money, long words, a narrow box): every
        // item stays inside the bar's box, every drawn pixel inside it and its soft drop shadow
        // (the kit's panel shadow: 3 + 4 canvas pixels down, its 4-pixel blur around).
        HudState x = hud;
        x.sanity = 100000; x.sanityMax = 1; x.money = 99999999; x.tier = 3;
        HudStrings longWords = strings;
        longWords.label = "Averyveryverylongmeterlabelthatcannotfit";
        longWords.am = longWords.pm = "o'clock in the afternoon";
        for (int maxW : {600, 300, 200, 120, 104})
        {
            Composition c {kConsoleW, kConsoleH, {}, {}};
            const Rect b = AddStatusBar(c, x, longWords, 40, 40, maxW);
            CHECK(!c.ui.empty() && b.w <= maxW && b.w > 0);
            for (const ui::UiItem& it : c.ui)
                CHECK(it.to.x >= b.x && it.to.y >= b.y && it.to.x + it.to.w <= b.x + b.w && it.to.y + it.to.h <= b.y + b.h);
            const int W = 800, H = 160;
            std::vector<u32> px((size_t)W * H);
            ui::Rasterise(c.ui, 1.0f, px.data(), W, H);
            int outside = 0;
            for (int y = 0; y < H; y++)
                for (int xx = 0; xx < W; xx++)
                    if ((px[(size_t)y * W + xx] >> 24) && (xx < b.x - 5 || xx >= b.x + b.w + 5 || y < b.y - 2 || y >= b.y + b.h + 8)) outside++;
            CHECK(outside == 0);
        }
        Composition tiny {kConsoleW, kConsoleH, {}, {}};
        CHECK(AddStatusBar(tiny, hud, strings, 0, 0, 50).w == 0 && tiny.ui.empty());
    }
    // No HUD (title, main menu, Create-a-Sim, black loads): never the two screens side by side.
    // The top screen framed at the left at its shape, the touch screen 2.5x framed at the right,
    // the only touchable piece; a hint row at the bottom.
    for (Screen sc : {Screen::FrontEnd, Screen::CreateASim, Screen::None})
        for (const auto& top : {std::pair<int, int> {SW, SH}, {341, 192}, {682, 384}})
        {
            const Composition c = BuildConsole({sc, Tab::None}, false, -1, top.first, top.second);
            CHECK(c.cw == kConsoleW && c.ch == kConsoleH);
            const Composition side = FromPlacement(Place(Layout::Side, true, (float)top.first / top.second), top.first, top.second);
            int touchBottoms = 0;
            for (const Piece& q : c.pieces)
            {
                for (const Piece& sq : side.pieces)
                    CHECK(!(q.to.x == sq.to.x && q.to.y == sq.to.y && q.to.w == sq.to.w && q.to.h == sq.to.h));
                if (q.src != Src::Bottom || !q.touch) continue;
                touchBottoms++;
                CHECK(q.from.x == 0 && q.from.y == 0 && q.from.w == SW && q.from.h == SH);
                CHECK(q.to.w * 2 >= SW * 5 && q.to.h * 2 >= SH * 5 && q.to.w * SH == q.to.h * SW);
                CHECK(q.to.x >= c.pieces[0].to.x + c.pieces[0].to.w);
            }
            CHECK(touchBottoms == 1);
            CHECK(c.pieces.size() == 2 && c.pieces[0].src == Src::Top && c.pieces[0].to.x + c.pieces[0].to.w <= kConsoleW / 2);
            // The top screen at its own shape (within a pixel), never stretched.
            const Rect t = c.pieces[0].to;
            CHECK(std::abs(t.w * top.second - t.h * top.first) <= top.first);
            // (the kit's chips: the right stick, A, the pause menu's; no 5 x 7 text)
            bool hint = false;
            for (const ui::UiItem& it : c.ui) hint |= it.kind == ui::UiItem::Chip && it.to.y > 600;
            CHECK(hint);
            for (const Overlay& o : c.overlays) CHECK(o.text == nullptr);
            float cx, cy, sc2;
            CHECK(CanvasFromTouch(c, 10, 20, cx, cy, sc2) && sc2 == 2.5f && cx == c.pieces[1].to.x + 25 && cy == c.pieces[1].to.y + 50);
            // Both pieces framed (6 pixels outside each, drawn before the top piece); the top
            // screen is the camera (wheel, right drag), as SIDE's was.
            CHECK(c.pieces[0].touch);
            int frames = 0;
            for (const Overlay& o : c.overlays)
                for (const Piece& q : c.pieces)
                    if (!o.text && o.argb == kFrameArgb && o.border == 6 && o.before == 0 && o.to.x == q.to.x - 6 && o.to.y == q.to.y - 6 &&
                        o.to.w == q.to.w + 12 && o.to.h == q.to.h + 12)
                        frames++;
            CHECK(frames == 2);
        }
    // A black load: the bare backdrop, nothing drawn, nothing touchable, no cursor, no camera.
    for (bool menuOpen : {false, true})
    {
        const Composition c = BuildConsole({Screen::Loading, Tab::None}, menuOpen, 0, SW, SH);
        CHECK(c.cw == kConsoleW && c.ch == kConsoleH && c.pieces.empty() && c.overlays.empty());
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 1280, 720);
        int tx, ty;
        float cx, cy, sc2;
        CHECK(!TouchFromComposition(c, fit, 640, 360, tx, ty));
        CHECK(!OverTopComposition(c, fit, 640, 360));
        CHECK(!CanvasFromTouch(c, 128, 96, cx, cy, sc2));
    }
    // SaveSlots, Unknown (HUD up): the menu composition, framed, with the stick cursor (the Drencher,
    // once one of them, is an activity of its own: milestone 6).
    for (Screen sc : {Screen::SaveSlots, Screen::Unknown})
    {
        const Composition m = BuildConsole({sc, Tab::None}, false, -1, SW, SH);
        CHECK(m.pieces.size() == 2 && !m.pieces[0].touch && m.pieces[1].src == Src::Bottom && m.pieces[1].to.x == kMenuTo.x && m.pieces[1].to.y == kMenuTo.y && m.pieces[1].to.w == SW * 3);
        // (and the cursor's hint panel under it, at the right clear of the HD status bar: how to
        // leave; the kit's chips, the 5 x 7 font gone)
        CHECK(m.overlays.size() == 2 && m.overlays[0].argb == kDimArgb && m.overlays[1].argb == kFrameArgb && m.overlays[1].to.x == kMenuTo.x - 6);
        CHECK(m.ui.size() >= 4 && m.ui[0].kind == ui::UiItem::Panel);
        const Rect hb = m.ui[0].to;
        CHECK(hb.x >= 720 && hb.x + hb.w <= kConsoleW && hb.y >= kMenuTo.y + kMenuTo.h && hb.y + hb.h <= kConsoleH);
        int chips = 0;
        for (const ui::UiItem& it : m.ui) chips += it.kind == ui::UiItem::Chip;
        CHECK(chips == 3);   // the right stick, A, the pause menu
        // The furniture editor's: B too (its X).
        if (sc == Screen::Unknown)
        {
            GameState g;
            g.valid = true;
            g.placement.valid = true;
            const Composition e = BuildConsole({sc, Tab::None}, false, -1, SW, SH, nullptr, nullptr, &g);
            int ec = 0;
            bool b = false;
            for (const ui::UiItem& it : e.ui)
            {
                ec += it.kind == ui::UiItem::Chip;
                b |= it.kind == ui::UiItem::Chip && it.button == ui::Button::B;
            }
            CHECK(ec == 4 && b);
            CHECK(e.ui[0].to.x >= 720 && e.ui[0].to.x + e.ui[0].to.w <= kConsoleW);
        }
    }
    // The menu: the dimmed world swallows clicks and the camera; the touch screen 3x is the stylus.
    for (ScreenState s : {ScreenState {Screen::Panel, Tab::Goals}, ScreenState {Screen::Popup, Tab::None}, ScreenState {Screen::Gift, Tab::None}})
    {
        const Composition m = BuildConsole(s, s.screen == Screen::Panel, -1, SW, SH);
        const CanvasFit fit = CanvasFit::For(m.cw, m.ch, 1280, 720);
        CHECK(m.pieces.size() == 2 && !m.pieces[0].touch && m.pieces[1].to.x == 256 && m.pieces[1].to.y == 72 && m.pieces[1].to.w == SW * 3);
        CHECK(m.overlays.size() == 1 && m.overlays[0].argb == kDimArgb && m.overlays[0].before == 1);
        int tx, ty;
        CHECK(!TouchFromComposition(m, fit, 200, 300, tx, ty));
        CHECK(!OverTopComposition(m, fit, 200, 300));
        CHECK(TouchFromComposition(m, fit, 256 + 3 * 10 + 1, 72 + 3 * 20 + 1, tx, ty) && tx == 10 && ty == 20);
    }
    // In town the world is the camera except under the rail's pieces.
    {
        const Composition town = BuildConsole({Screen::Panel, Tab::Interact}, false, -1, SW, SH);
        const CanvasFit fit = CanvasFit::For(town.cw, town.ch, 1280, 720);
        CHECK(OverTopComposition(town, fit, 600, 300));
        CHECK(!OverTopComposition(town, fit, 1200, 300));
    }
    // The focus ring: the hotspot's area, through the piece that shows it, the kit's ring (the green
    // kFocusOuterW outside, the white line kFocusInnerW) as two outlines.
    {
        const ScreenState goals {Screen::Panel, Tab::Goals};
        const Composition m = BuildConsole(goals, true, 0, SW, SH);
        const Rect a = HotspotsFor(goals)[0].area;
        CHECK(m.overlays.size() == 3);
        const Overlay& o = m.overlays[1];
        const Overlay& r = m.overlays[2];
        constexpr int ow = ui::kFocusOuterW, iw = ui::kFocusInnerW;
        CHECK(o.argb == 0 && o.border == ow - iw && o.radius == 10 + ow && o.borderArgb == ui::kFocusOuter && o.before == -1);
        CHECK(o.to.x == 256 + 3 * a.x - ow && o.to.y == 72 + 3 * a.y - ow && o.to.w == 3 * a.w + 2 * ow && o.to.h == 3 * a.h + 2 * ow);
        CHECK(r.argb == 0 && r.border == iw && r.radius == 10 + iw && r.borderArgb == ui::kFocusInner && r.before == -1);
        CHECK(r.to.x == 256 + 3 * a.x - iw && r.to.y == 72 + 3 * a.y - iw && r.to.w == 3 * a.w + 2 * iw && r.to.h == 3 * a.h + 2 * iw);
        // In town the tab icons are on the rail (2x); a goal line is not shown: no ring.
        int tab = -1;
        const std::vector<Hotspot>& h = HotspotsFor(goals);
        for (int i = 0; i < (int)h.size(); i++) if (std::string(h[i].name) == "pockets") tab = i;
        CHECK(tab >= 0);
        const Composition town = BuildConsole(goals, false, tab, SW, SH);
        CHECK(town.overlays.size() == 3);
        const Rect ta = h[tab].area;
        CHECK(town.overlays[1].to.x == kRailTabsTo.x + 2 * (ta.x - kRailTabsFrom.x) - ow);
        CHECK(town.overlays[1].to.y == kRailTabsTo.y + 2 * (ta.y - kRailTabsFrom.y) - ow);
        CHECK(BuildConsole(goals, false, 0, SW, SH).overlays.size() == 1);
        CHECK(BuildConsole(goals, false, 999, SW, SH).overlays.size() == 1);
        // The dialogue's arrow: its area clipped to the arrow piece.
        const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, 0, SW, SH);
        const Rect ra = dlg.overlays.back().to;
        CHECK(ra.x >= kDialogueArrowTo.x - 4 && ra.x + ra.w <= kDialogueArrowTo.x + kDialogueArrowTo.w + 4);
    }
    // A drag whose piece is gone (a stale index): the touchable bottom piece under the pointer,
    // else the largest; always on the DS screen.
    {
        const Composition dlg = BuildConsole({Screen::Dialogue, Tab::None}, false, -1, SW, SH);
        const CanvasFit fit = CanvasFit::For(dlg.cw, dlg.ch, 1280, 720);
        int tx, ty;
        // Over the portrait (piece 3) with a stale index.
        CHECK(TouchClampedComposition(dlg, 99, fit, kDialoguePortraitTo.x + 2 * 10 + 1, kDialoguePortraitTo.y + 2 * 5 + 1, tx, ty));
        CHECK(tx == kDialoguePortraitFrom.x + 10 && ty == kDialoguePortraitFrom.y + 5);
        // Over the world (piece 0, a top piece): the largest, the text.
        CHECK(TouchClampedComposition(dlg, 0, fit, 5, 5, tx, ty));
        CHECK(tx == kDialogueTextFrom.x && ty == kDialogueTextFrom.y);
        // A piece reaching past the DS screen: held to the screen, never an empty range.
        Composition odd {600, 400, {}, {}};
        odd.pieces.push_back({Src::Bottom, {250, 186, 40, 40}, {0, 0, 80, 80}});
        odd.pieces.push_back({Src::Bottom, {-20, -20, 10, 10}, {100, 0, 20, 20}});
        const CanvasFit f2 = CanvasFit::For(odd.cw, odd.ch, 600, 400);
        for (int idx : {0, 1})
            for (float p : {-500.0f, 10.0f, 110.0f, 5000.0f})
            {
                CHECK(TouchClampedComposition(odd, idx, f2, p, p, tx, ty));
                CHECK(tx >= 0 && tx < SW && ty >= 0 && ty < SH);
            }
        // No touchable piece (a held CONSOLE picture: consolehold.h; a black load): no touch at
        // all, never the whole touch screen under the canvas.
        Composition held = dlg;
        for (Piece& p : held.pieces) p.touch = false;
        for (int idx : {-1, 0, 3, 99})
        {
            tx = ty = -7;
            CHECK(!TouchClampedComposition(held, idx, fit, kDialoguePortraitTo.x + 21, kDialoguePortraitTo.y + 11, tx, ty));
            CHECK(tx == -7 && ty == -7);
        }
        const Composition black = BuildConsole({Screen::Loading, Tab::None}, false, -1, SW, SH);
        const CanvasFit fb = CanvasFit::For(black.cw, black.ch, 1280, 720);
        CHECK(!TouchClampedComposition(black, 0, fb, 640, 360, tx, ty));
    }
    // The stick cursor: drawn through the piece that shows its point, hidden where none does.
    {
        float cx, cy, sc;
        const Composition town = BuildConsole({Screen::Panel, Tab::Goals}, false, -1, SW, SH);
        CHECK(CanvasFromTouch(town, 16, 22, cx, cy, sc) && sc == 2 && cx == kRailTabsTo.x + 32 && cy == kRailTabsTo.y + 32);
        CHECK(CanvasFromTouch(town, 230, 20, cx, cy, sc) && cx == kRailPhoneTo.x + 16);
        CHECK(!CanvasFromTouch(town, 128, 96, cx, cy, sc));
        const Composition unk = BuildConsole({Screen::Unknown, Tab::None}, false, -1, SW, SH);
        CHECK(CanvasFromTouch(unk, 10, 20, cx, cy, sc) && sc == 3 && cx == kMenuTo.x + 30 && cy == kMenuTo.y + 60);
        const Composition side = FromPlacement(Place(Layout::Side, true, kDSAspect), SW, SH);
        CHECK(CanvasFromTouch(side, 0, 0, cx, cy, sc) && cx == SW && cy == 0 && sc == 1);
        CHECK(!CanvasFromTouch(side, SW, 0, cx, cy, sc));
    }
    // The overlay's pixels: border, fill, corners left empty.
    {
        std::vector<u32> px;
        RasteriseOverlay({{0, 0, 100, 50}, 0xFF112233, 10, 4, 0xFF445566}, 200, 100, px);
        CHECK(px.size() == 200 * 100);
        CHECK(px[0] == 0);                       // outside the rounded corner
        CHECK(px[50 * 200 + 2] == 0xFF445566);   // the left border, 8 output pixels wide
        CHECK(px[50 * 200 + 7] == 0xFF445566);
        CHECK(px[50 * 200 + 9] == 0xFF112233);   // the fill
        CHECK(px[50 * 200 + 100] == 0xFF112233);
    }

    for (Layout l : {Layout::Stacked, Layout::Side, Layout::Focus})
        for (bool mainIsTop : {true, false})
            for (float aspect : {kDSAspect, 16.0f / 9.0f})
                for (const auto& s : {std::pair<int, int> {1280, 720}, {2560, 1440}}) TestComposeRoundtrip(l, mainIsTop, aspect, s.first, s.second);

    // Two pieces of the touch screen at different scales: each maps back through its own.
    {
        const Composition c = TwoBottomPieces();
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 1400, 600);
        int tx = -1, ty = -1, idx = -1;
        CHECK(TouchFromComposition(c, fit, 3 * 10 + 1, 3 * 20 + 1, tx, ty, &idx) && tx == 10 && ty == 20 && idx == 0);
        CHECK(TouchFromComposition(c, fit, 900 + 2 * 30, 2 * 5, tx, ty, &idx) && tx == 30 && ty == 101 && idx == 1);
        CHECK(!TouchFromComposition(c, fit, 850, 100, tx, ty)); // the gap between them
        // A drag that started on the first piece stays on it: past its right edge it is held.
        CHECK(TouchClampedComposition(c, 0, fit, 1300, 500, tx, ty));
        CHECK(tx == SW - 1 && ty == 95);
        CHECK(TouchClampedComposition(c, 1, fit, 0, -50, tx, ty));
        CHECK(tx == 0 && ty == 96);
    }
    // A drag begun on FOCUS top-big's panel (piece 1) when the layout flips to bottom-big (where
    // piece 1 is the world): it stays on the touch screen, in DS coordinates.
    for (float aspect : {kDSAspect, 16.0f / 9.0f})
    {
        const int tw = (int)(SH * aspect + 0.5f);
        const Composition big = FromPlacement(Place(Layout::Focus, false, aspect), tw, SH);
        const CanvasFit fit = CanvasFit::For(big.cw, big.ch, 1280, 720);
        for (int idx : {1, 0, 5, -1})
            for (float px : {-100.0f, 0.0f, 1000.0f, 1270.0f, 5000.0f})
                for (float py : {-100.0f, 360.0f, 5000.0f})
                {
                    int tx = -1, ty = -1;
                    CHECK(TouchClampedComposition(big, idx, fit, px, py, tx, ty));
                    CHECK(tx >= 0 && tx < SW && ty >= 0 && ty < SH);
                }
        int tx, ty;
        CHECK(TouchClampedComposition(big, 1, fit, 480, 360, tx, ty)); // the middle of the big touch screen
        CHECK(abs(tx - SW / 2) <= 1 && abs(ty - SH / 2) <= 1);
    }
    // Overlapping pieces: the later one is hit.
    {
        Composition c {600, 400, {}, {}};
        c.pieces.push_back({Src::Bottom, {0, 0, SW, SH}, {0, 0, SW, SH}});
        c.pieces.push_back({Src::Bottom, {0, 0, 64, 48}, {100, 100, 256, 192}});
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 600, 400);
        int tx, ty, idx;
        CHECK(TouchFromComposition(c, fit, 100 + 4 * 10, 100 + 4 * 20, tx, ty, &idx) && idx == 1 && tx == 10 && ty == 20);
        CHECK(TouchFromComposition(c, fit, 50, 50, tx, ty, &idx) && idx == 0 && tx == 50 && ty == 50);
    }
    // A piece with touch off is never hit, and does not let a click through to what is under it.
    {
        Composition c {600, 400, {}, {}};
        c.pieces.push_back({Src::Bottom, {0, 0, SW, SH}, {0, 0, SW, SH}, 255, false});
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, 600, 400);
        int tx, ty;
        CHECK(!TouchFromComposition(c, fit, 50, 50, tx, ty));
        c.pieces.insert(c.pieces.begin(), {Src::Bottom, {0, 0, SW, SH}, {0, 0, SW, SH}});
        CHECK(!TouchFromComposition(c, fit, 50, 50, tx, ty));
    }
    // The camera: over a top piece unless a later touchable bottom piece covers the point.
    {
        const Composition c = FromPlacement(Place(Layout::Focus, true, kDSAspect), SW, SH);
        const CanvasFit fit = CanvasFit::For(c.cw, c.ch, c.cw, c.ch);
        CHECK(OverTopComposition(c, fit, 100, 100));
        CHECK(!OverTopComposition(c, fit, c.cw - 10, c.ch / 2));
        CHECK(!OverTopComposition(c, fit, -5, 100));
    }
    // A placement drawn second keeps its opacity on its piece; the first is opaque.
    {
        const Composition c = FromPlacement(Place(Layout::Focus, true, kDSAspect), SW, SH);
        CHECK(c.pieces.size() == 2 && c.pieces[0].src == Src::Top && c.pieces[0].alpha == 255);
        CHECK(c.pieces[1].src == Src::Bottom && c.pieces[1].alpha == 232);
        CHECK(c.pieces[0].from.w == SW && c.pieces[0].from.h == SH);
        const Composition w = FromPlacement(Place(Layout::Side, true, 16.0f / 9.0f), 427, 240);
        CHECK(w.pieces[0].from.w == 427 && w.pieces[0].from.h == 240);
    }
    const int sizes[][2] = {{1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}, {1920, 1080}, {640, 480}};
    const float aspects[] = {kDSAspect, 16.0f / 9.0f};
    for (Layout l : {Layout::Stacked, Layout::Side, Layout::Focus})
        for (bool mainIsTop : {true, false})
            for (float aspect : aspects)
                for (const auto& s : sizes) TestLayout(l, mainIsTop, aspect, s[0], s[1]);

    // FOCUS with the world big: the panel lies over the world's right edge and wins there; the
    // world's status bar (its bottom rows) stays clear of it; the world keeps the canvas's height.
    for (float aspect : aspects)
    {
        const Placement p = Place(Layout::Focus, true, aspect);
        CHECK(p.topFirst && p.alpha < 255);
        CHECK(p.top.h == p.ch && p.top.y == 0);
        CHECK(p.bottom.x + p.bottom.w == p.cw);
        CHECK(p.bottom.y + p.bottom.h < p.top.y + p.top.h - 40);
        CHECK(p.bottom.w == SW * 2 && p.bottom.h == SH * 2);
        const CanvasFit fit = CanvasFit::For(p, 1280, 720);
        int tx, ty;
        const float px = fit.ox + (p.bottom.x + 1) * fit.scale, py = fit.oy + (p.bottom.y + 1) * fit.scale;
        CHECK(p.Contains(p.top, p.bottom.x + 1, p.bottom.y + 1)); // under the panel there is world
        CHECK(TouchFromOutput(p, fit, px, py, tx, ty));
        CHECK(!OverTop(p, fit, px, py));
    }
    // FOCUS with the touch screen big: the world is drawn last, small, beside it, never over it.
    {
        const Placement p = Place(Layout::Focus, false, kDSAspect);
        CHECK(!p.topFirst && p.alpha == 255);
        CHECK(p.top.x >= p.bottom.x + p.bottom.w);
        const Placement w = Place(Layout::Focus, false, 16.0f / 9.0f);
        CHECK(w.top.w == 320 && w.top.h == 180);
    }
    // SIDE and STACKED follow the top picture's shape.
    CHECK(Place(Layout::Side, true, 16.0f / 9.0f).top.w == 341);
    CHECK(Place(Layout::Stacked, true, 16.0f / 9.0f).cw == 341);
    CHECK(Place(Layout::Side, true, kDSAspect).cw == SW * 2);

    // The HD conversation's text (port/dialogue.h): placeholders and escapes never shown raw.
    {
        HudState hud;
        hud.valid = true; hud.hour = 15; hud.minute = 7; hud.simName = "Jun";
        CHECK(DialogueText("There's a reason, @1. Go!", hud) == "There's a reason, Jun. Go!");
        CHECK(DialogueText("the @2 was repossessed", hud) == "the was repossessed");
        CHECK(DialogueText("Looks like a @2! Amazing.", hud) == "Looks like a! Amazing.");
        CHECK(DialogueText("by @3.", hud) == "by.");
        CHECK(DialogueText("at @hh:@mm @am today", hud, "am", "pm") == "at 3:07 pm today");
        CHECK(DialogueText("day:@DD @ss @ll end", hud) == "day: end");
        CHECK(DialogueText("{F012}Hello{0b} there{F0}", hud) == "Hello there");
        CHECK(DialogueText("lone @ sign @", hud) == "lone sign");
        CHECK(DialogueText("  @2 starts\n@1\n\n", hud) == "starts\nJun");
        CHECK(DialogueText("Bonjour !", hud) == "Bonjour !"); // the string's own space before '!' stays
        HudState none; // no clock, no name: dropped
        CHECK(DialogueText("@1, it is @hh:@mm.", none) == ", it is:.");
        // Wrapped to the box: a word longer than a line breaks; a long page grows the panel up,
        // never past the canvas; every line within the text box.
        const DialogueLayout a = LayoutDialogue("Short.", true);
        CHECK(a.fits && a.lines.size() == 1 && a.size == kDlgTextSize && a.rows == kDlgRows);
        CHECK(a.panel.x == kDlgPanelX && a.panel.y + a.panel.h == kConsoleH - kDlgBottomGap && a.panel.y > 0);
        const DialogueLayout b = LayoutDialogue(std::string(150, 'W'), true);
        CHECK(b.fits && b.lines.size() > 4 && b.panel.y >= 0 && b.panel.h > a.panel.h);
        std::string longText;
        for (int i = 0; i < 60; i++) longText += "word ";
        const DialogueLayout c = LayoutDialogue(longText, false);
        CHECK(c.fits && c.size >= kDlgTextMin && c.textW > a.textW);
        for (const std::string& l : c.lines) CHECK(ui::TextWidth(l, c.size) <= c.textW);
        const DialogueLayout huge = LayoutDialogue(std::string(4000, 'W'), true); // clipped, never past the panel
        CHECK(!huge.fits && huge.rows == kDlgMaxRows && huge.panel.y >= 0);
        CHECK(!PortraitShown(nullptr));
        std::vector<u32> flat(SW * SH, 0xFF5961A2), black(SW * SH, 0xFF000000), face(SW * SH, 0xFFC08060);
        CHECK(!PortraitShown(flat.data()) && !PortraitShown(black.data()) && PortraitShown(face.data()));

        // BuildDialogue (a RomData without strings: no text, the player's name on the plate):
        // the world whole, every UI item inside the canvas, a click on the panel taps the game's
        // arrow; BuildConsole keeps milestone 1's box without the game's strings.
        RomData rom;
        GameState g;
        g.valid = true;
        g.hud = hud;
        g.dialogue.valid = true; g.dialogue.page = 1; g.dialogue.pages = 2; g.dialogue.stringId = 5; g.dialogue.speaker = 0x40;
        for (int topW : {256, 341})
        {
            Composition comp {kConsoleW, kConsoleH, {}, {}};
            BuildDialogue(comp, g, rom, face.data(), topW, 192);
            CHECK(comp.pieces.size() == 2 && comp.pieces[0].src == Src::Top && comp.pieces[0].from.x == 0 && comp.pieces[0].from.w == topW);
            CHECK(comp.pieces[0].from.h == HudCropRows(192));
            bool plate = false;
            for (const ui::UiItem& it : comp.ui)
            {
                CHECK(it.to.x >= 0 && it.to.y >= 0 && it.to.x + it.to.w <= kConsoleW && it.to.y + it.to.h <= kConsoleH);
                plate |= it.kind == ui::UiItem::Text && it.text == "Jun";
            }
            CHECK(plate);
            const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
            const Rect P = comp.pieces[1].to;
            int tx, ty;
            CHECK(TouchFromComposition(comp, fit, P.x + 5.5f, P.y + 5.5f, tx, ty) && tx >= 219 && tx < 250 && ty >= 155 && ty < 187);
            CHECK(TouchFromComposition(comp, fit, P.x + P.w - 2.5f, P.y + P.h - 2.5f, tx, ty) && tx >= 219 && tx < 250 && ty >= 155 && ty < 187);
            CHECK(OverTopComposition(comp, fit, 640.5f, 100.5f));
            // Every pixel of its touchable pieces (the invisible tap piece) round trips, at every size.
            for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                TestConsoleRoundtrip({Screen::Dialogue}, false, topW, 192, o.first, o.second, true, &comp);
        }
        const Composition m1 = BuildConsole({Screen::Dialogue}, false, -1, 256, 192, &hud, nullptr, &g, &rom, face.data());
        CHECK(m1.ui.empty() && m1.pieces.size() == 5);
    }


    // The panel's HD menu (port/panelmenu.h, port/interact.h) on a made-up Interact state: four
    // actions (the third greyed), a previous and a next target. Its mouse pieces (invisible,
    // touchable) map each HD item onto the game's own target: every pixel round trips at every
    // size; a click on a row lands on its arrow, on a tab on its icon, on the phone on the phone;
    // a click on the panel elsewhere, or on the dimmed world, touches nothing. Every UI item lies
    // inside the panel; the still items come in ascending layers (the frame on 0, the rest above).
    {
        RomData rom;
        GameState g;
        g.valid = true;
        InteractState& in = g.town.interact;
        in.valid = true;
        in.target = {TargetRef::Object, 29, 492, 6638, false};
        in.prev = {TargetRef::Sim, 3, 326, 1, false};
        in.next = {TargetRef::Object, 15, 478, 5744, false};
        for (int i = 0; i < 4; i++) in.actions.push_back({i, 1845 + i, i == 2});
        CHECK(WithoutAmount("Pay (\x24@1)") == "Pay" && WithoutAmount("Buy (@1) now") == "Buy now" && WithoutAmount("Open") == "Open" && WithoutAmount("Pay @1") == "Pay @1");
        const ScreenState it {Screen::Panel, Tab::Interact};
        const MenuView v = PanelMenuViewOf(it, g);
        CHECK(v.valid && v.items.size() == 5);
        CHECK(!v.items[2].enabled && v.items[0].enabled && v.items[3].enabled);
        CHECK(!v.buttons[(int)MenuButton::Left].empty() && !v.buttons[(int)MenuButton::Right].empty());
        CHECK(MenuFirst(v) == 0 && MenuNeighbour(v, 1, 0, 1) == 3); // the greyed row skipped

        // Reachability on the real views: every enabled item from every other, the phone from
        // the rows and back (Up from the top row, Down from the phone), with and without a
        // target, greyed rows, neighbours (Left / Right then switch targets, not moves).
        for (int rows : {0, 1, 3, 4})
            for (int greyed : {-1, 0, 1})
                for (bool side : {false, true})
                {
                    GameState h = g;
                    InteractState& hi = h.town.interact;
                    hi.actions.clear();
                    for (int i = 0; i < rows; i++) hi.actions.push_back({i, 1845 + i, i == greyed});
                    if (!side) hi.prev = hi.next = TargetRef {};
                    if (rows == 0) hi.target = TargetRef {};
                    const MenuView w = PanelMenuViewOf(it, h);
                    CHECK(w.valid && AllReachable(w));
                    const int phone = (int)w.items.size() - 1;
                    const int top = MenuFirst(w);
                    if (top != phone)
                    {
                        CHECK(MenuMove(w, top, 0) == phone);
                        CHECK(MenuMove(w, phone, 1) == top);
                    }
                }
        CHECK(!PanelMenuView(it, &g, &rom).valid);                  // no strings: milestone 1's
        CHECK(!PanelMenuViewOf({Screen::Panel, Tab::Goals}, g).valid);
        GameState bad = g;
        bad.town.interact.valid = false;
        CHECK(!PanelMenuViewOf(it, bad).valid);
        std::vector<u32> face(SW * SH, 0xFFC08060);
        for (int topW : {256, 341})
            for (int focus : {-1, 0, 3, 4})
            {
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                Piece world = ConsoleWorld(topW, HudCropRows(192), true);
                world.touch = false;
                comp.pieces.push_back(world);
                BuildPanelMenu(comp, it, v, focus, g, rom, nullptr, face.data());
                int layer = 0;
                for (const ui::UiItem& u : comp.ui)
                {
                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                    if (u.kind == ui::UiItem::Image) continue;
                    CHECK(u.layer >= layer);
                    layer = u.layer;
                }
                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}, {800, 1000}})
                    TestConsoleRoundtrip(it, true, topW, 192, o.first, o.second, true, &comp);
                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                int tx, ty;
                for (int i = 0; i < 4; i++)
                {
                    const Rect r = v.items[i].rect;
                    CHECK(TouchFromComposition(comp, fit, r.x + 3.5f, r.y + r.h / 2.0f, tx, ty));
                    CHECK(tx >= 219 && tx <= 251 && ty >= 46 + 34 * i && ty <= 78 + 34 * i);
                }
                for (int t = 0; t < 5; t++)
                {
                    CHECK(TouchFromComposition(comp, fit, kPmTabsX + t * kPmTabCell + kPmTabCell / 2.0f, kPmTabBar.y + kPmTabBar.h / 2.0f, tx, ty));
                    const Rect a = kTabIcon[t].area;
                    CHECK(tx >= a.x && tx < a.x + a.w && ty >= a.y && ty < a.y + a.h);
                }
                const Rect ph = v.items[4].rect;
                CHECK(TouchFromComposition(comp, fit, ph.x + ph.w / 2.0f, ph.y + ph.h / 2.0f, tx, ty));
                CHECK(tx >= kPhoneIcon.area.x && tx < kPhoneIcon.area.x + kPhoneIcon.area.w);
                CHECK(!TouchFromComposition(comp, fit, kPmTitle.x + 10.5f, kPmTitle.y + 10.5f, tx, ty)); // the title: nothing
                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                          // the dimmed world: nothing
                CHECK(!OverTopComposition(comp, fit, 20.5f, 360.5f));
            }
        // Pockets (port/pockets.h) and Give Gift (port/gift.h) on made-up inventories: empty slots
        // disabled and never focused, every enabled item reachable from every other and the phone
        // Up from the top row; A on a slot taps it then the hand / the bow; X on a slot the game
        // has not selected taps the slot (the question), on the one it has the bin. Their mouse
        // pieces round trip and land on the game's own targets; every item inside the panel.
        auto inventory = [](int n) {
            GameState h;
            h.valid = true;
            InventoryState& inv = h.town.inventory;
            inv.valid = true;
            for (int i = 0; i < n; i++) inv.slots.push_back({100 + i, 563 + i, 922 + i, 1991, 1});
            for (int k = 0; k < 3; k++) inv.keys[k] = {k, 1968 + k, 1972 + k, -1, k == 1 ? 1 : 0};
            for (int i = 0; i < 4; i++) inv.counted[i] = {i, 1154 + i, 845 + i, -1, i * 3};
            h.town.pockets.valid = true;
            h.town.gift.valid = true;
            h.town.gift.recipient = 22;
            h.town.gift.recipientNameId = 345;
            return h;
        };
        const ScreenState pk {Screen::Panel, Tab::Pockets}, gs {Screen::Gift, Tab::None};
        for (int n = 0; n <= 6; n++)
            for (int sel : {-1, 0, 5})
            {
                GameState h = inventory(n);
                h.town.pockets.slot = sel < n ? sel : -1;
                h.town.gift.slot = sel < n ? sel : -1;
                if (n == 4) h.town.gift.refused[2] = true;
                const MenuView p = PanelMenuViewOf(pk, h);
                CHECK(p.valid && p.items.size() == 6 + 3 + 4 + 1 && AllReachable(p));
                for (int i = 0; i < 6; i++)
                {
                    CHECK(p.items[i].enabled == (i < n));
                    if (i >= n) continue;
                    CHECK(p.items[i].action.size() == 2 && p.items[i].action[1].x == kPkHandX && p.items[i].action[1].y == kPkHandY);
                    // The bin asks, whatever the game's selection: arming taps the slot, the
                    // second X the slot and the bin.
                    const ViewItem& it = p.items[i];
                    CHECK(it.xAsks && it.xArm.size() == 1 && it.xArm[0].x == it.action[0].x && it.xArm[0].y == it.action[0].y);
                    CHECK(it.buttonX.size() == 2 && it.buttonX[0].x == it.action[0].x && it.buttonX[1].x == kPkBinX && it.buttonX[1].y == kPkBinY);
                }
                CHECK(p.items[6].action.empty() && !p.items[7].action.empty() && p.items[9].action.empty()); // a key item not owned, owned; counted
                CHECK(MenuMove(p, 13, 1) == MenuFirst(p));
                if (h.town.pockets.slot >= 0) CHECK(MenuFirst(p) == h.town.pockets.slot);
                const MenuView g = GiftMenuViewOf(gs, h);
                // The six slots, then the bow (focusable: it gives the game's selection, when it may).
                CHECK(g.valid && g.items.size() == 7 && g.back.size() == 1 && g.back[0].x == kGiftCloseX && g.items[6].id == kGiftBowId);
                {
                    const int sl = h.town.gift.slot;
                    const bool may = sl >= 0 && sl < 6 && g.items[sl].enabled;
                    CHECK(g.items[6].action.empty() == !may);
                    if (may) CHECK(g.items[6].action.size() == 1 && g.items[6].action[0].x == kGiftBowX && g.items[6].action[0].y == kGiftBowY);
                }
                for (int i = 0; i < 6; i++)
                {
                    CHECK(g.items[i].enabled == (i < n && !(n == 4 && i == 2)));
                    if (g.items[i].enabled) CHECK(g.items[i].action.size() == 2 && g.items[i].action[1].x == kGiftBowX);
                }
                bool any = false;
                for (const ViewItem& it : g.items) any |= it.enabled;
                if (any) CHECK(AllReachable(g));
                std::vector<u32> pic(SW * SH, 0xFF808080);
                for (int focus : {-1, 0, 7, 9, 13})
                {
                    for (bool gift : {false, true})
                    {
                        Composition comp {kConsoleW, kConsoleH, {}, {}};
                        Piece world = ConsoleWorld(256, HudCropRows(192), true);
                        world.touch = false;
                        comp.pieces.push_back(world);
                        if (gift) BuildGiftMenu(comp, g, focus < 6 ? focus : -1, h, rom, nullptr, pic.data());
                        else BuildPanelMenu(comp, pk, p, focus, h, rom, nullptr, pic.data());
                        int layer = 0;
                        for (const ui::UiItem& u : comp.ui)
                        {
                            CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                  u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                            if (u.kind == ui::UiItem::Image) continue;
                            CHECK(u.layer >= layer);
                            layer = u.layer;
                        }
                        if (n != 2 || focus != 0) continue;
                        for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                            TestConsoleRoundtrip(gift ? gs : pk, true, 256, 192, o.first, o.second, true, &comp);
                        const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                        const std::vector<Hotspot>& hs = HotspotsFor(gift ? gs : pk);
                        const MenuView& w = gift ? g : p;
                        int tx, ty;
                        for (int i = 0; i < 6; i++)
                        {
                            const Rect r = w.items[i].rect;
                            CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                            const Rect a = hs[i].area;
                            CHECK(tx >= a.x && tx < a.x + a.w && ty >= a.y && ty < a.y + a.h);
                        }
                        const Rect act = gift ? kGiftBow : kPkHandBtn;
                        const Rect a = hs[6].area; // (the hand, or Give Item's bow: both 6th)
                        CHECK(TouchFromComposition(comp, fit, act.x + act.w / 2.0f, act.y + act.h / 2.0f, tx, ty));
                        CHECK(tx >= a.x && tx < a.x + a.w && ty >= a.y && ty < a.y + a.h);
                        if (!gift)
                        {
                            CHECK(TouchFromComposition(comp, fit, kPkBinBtn.x + kPkBinBtn.w / 2.0f, kPkBinBtn.y + kPkBinBtn.h / 2.0f, tx, ty));
                            CHECK(tx >= hs[10].area.x && tx < hs[10].area.x + hs[10].area.w && ty >= hs[10].area.y && ty < hs[10].area.y + hs[10].area.h);
                        }
                        CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty)); // the dimmed world: nothing
                    }
                }
            }
        {
            GameState bad = inventory(2);
            bad.town.inventory.valid = false;
            CHECK(!PanelMenuViewOf(pk, bad).valid && !GiftMenuViewOf(gs, bad).valid);
            bad = inventory(2);
            bad.town.gift.valid = false;
            CHECK(!GiftMenuViewOf(gs, bad).valid && PanelMenuViewOf(pk, bad).valid);
            CHECK(!GiftMenuView(gs, &bad, &rom).valid); // no strings: milestone 1's
        }
        // The shop's catalogue (port/catalogue.h) on made-up lists: the store's twelve items at
        // every scroll, 0-6 owned, Buy or Sell or nothing selected, money enough or not; a
        // vendor's six. Cells past the list and empty places disabled; Pay always focusable, its
        // tap only when the game would pay (the money, the pockets' room, the quest items, a
        // vendor's sale); every enabled item reachable; the game's arrows as the list's scroll;
        // A on a cell / place its own tap, B the X, never replayed. Every item inside the panel,
        // layers ascending, the mouse pieces round trip and land on the game's own hit boxes.
        auto shop = [](bool vendor, int count, int scroll, int owned, bool sell, int sel, int money) {
            GameState h;
            h.valid = true;
            h.screen = GScreen::Catalogue;
            CatalogueState& ca = h.catalogue;
            ca.valid = true;
            ca.shop = vendor ? 0x30 : kCaStore;
            ca.vendor = vendor;
            for (int i = 0; i < count; i++) ca.items.push_back({100 + i + 0x3B, 100 + i, 563 + i, 922 + i, 1991, 2 * (50 + 10 * i), 50 + 10 * i});
            for (int i = 0; i < owned; i++) ca.owned.push_back({i == 1 ? 0x22 + 0x3B : 106 + 0x3B, i == 1 ? 0x22 : 106, 569, 928, 1991, 160, 80});
            ca.scroll = scroll;
            ca.sell = sell;
            ca.selected = sel;
            ca.money = money;
            ca.payEnabled = true;
            ca.upEnabled = !vendor && scroll > 0;
            ca.downEnabled = !vendor && scroll < count - 3;
            return h;
        };
        const ScreenState cs {Screen::Catalogue, Tab::None};
        for (bool vendor : {false, true})
            for (int scroll : {0, 3, 6, 9})
                for (int owned : {0, 2, 6})
                    for (int mode : {0, 1, 2}) // nothing selected, Buy, Sell
                        for (int money : {0, 5000})
                        {
                            if (vendor && (scroll > 0 || owned > 0 || mode == 2)) continue;
                            const int count = vendor ? 6 : 12;
                            const bool sell = mode == 2;
                            const int sel = mode == 0 ? -1 : sell ? std::min(1, owned - 1) : scroll + 1;
                            if (sell && owned == 0) continue;
                            const GameState h = shop(vendor, count, scroll, owned, sell, sel, money);
                            const MenuView v = CatalogueViewOf(cs, h);
                            const size_t n = vendor ? 7 : 13;
                            CHECK(v.valid && v.own && !v.replay && v.backCloses && v.items.size() == n && AllReachable(v));
                            for (size_t a = 0; a < v.items.size(); a++)
                                for (size_t b = a + 1; b < v.items.size(); b++) CHECK(v.items[a].id != v.items[b].id);
                            CHECK(v.back.size() == 1 && v.back[0].x == kCaCloseX && v.back[0].y == kCaCloseY);
                            for (int c = 0; c < 6; c++)
                            {
                                CHECK(v.items[c].enabled == (scroll + c < count) && v.items[c].group == v.scrollGroup);
                                if (!v.items[c].enabled) continue;
                                const int x = vendor ? kCaVendorCellX0 + (c % 2) * kCaVendorCellDx : kCaStoreCellX0 + (c % 3) * kCaStoreCellDx;
                                const int y = kCaCellY0 + (c / (vendor ? 2 : 3)) * kCaCellDy;
                                CHECK(v.items[c].action.size() == 1 && v.items[c].action[0].x == x && v.items[c].action[0].y == y);
                            }
                            // Pay: focusable always; its tap only when the game would pay.
                            const ViewItem& pay = v.items[6];
                            CHECK(pay.enabled && pay.id == kCaPayId);
                            bool pays = false;
                            if (mode == 1) pays = money >= h.catalogue.items[sel].buyPrice && owned < 6;
                            if (mode == 2) pays = sel != 1; // (place 1 holds a quest item: the game will not buy it back)
                            CHECK(CataloguePays(h.catalogue) == pays && pay.action.empty() == !pays);
                            if (pays) CHECK(pay.action.size() == 1 && pay.action[0].x == (vendor ? kCaVendorPayX : kCaPayX) && pay.action[0].y == kCaPayY);
                            for (int i = 0; !vendor && i < 6; i++)
                            {
                                CHECK(v.items[7 + i].enabled == (i < owned));
                                if (i < owned) CHECK(v.items[7 + i].action.size() == 1 && v.items[7 + i].action[0].x == kCaOwnedX0 + (i % 2) * kCaOwnedDx);
                            }
                            // The game's arrows: the list's scroll; Right from the grid's last column: Pay.
                            CHECK(v.scrollUp.empty() == !h.catalogue.upEnabled && v.scrollDown.empty() == !h.catalogue.downEnabled);
                            if (!v.scrollDown.empty()) CHECK(v.scrollDown[0].x == kCaDownX && v.scrollDown[0].y == kCaArrowY);
                            if (!v.scrollUp.empty()) CHECK(v.scrollUp[0].x == kCaUpX);
                            CHECK(MenuMove(v, vendor ? 1 : 2, 3) == 6 && MenuMove(v, 0, 0) == 0 && MenuMove(v, 6, 0) == 6 && MenuMove(v, 6, 1) == 6);
                            if (v.items[5].enabled) CHECK(MenuMove(v, 5, 1) == 5); // (past the last row: the game's scroll, never Pay)
                            if (!vendor && owned > 0) CHECK(MenuMove(v, 7, 2) == 6);
                            if (mode == 1) CHECK(MenuFirst(v) == 1);
                            if (mode == 2) CHECK(MenuFirst(v) == 7 + sel);
                            // The drawing, for a few focuses.
                            for (int focus : {-1, 0, 6, 7})
                            {
                                Composition comp {kConsoleW, kConsoleH, {}, {}};
                                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                                world.touch = false;
                                comp.pieces.push_back(world);
                                BuildCatalogue(comp, v, focus, h, rom, nullptr);
                                int layer = 0;
                                for (const ui::UiItem& u : comp.ui)
                                {
                                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                                    if (u.kind == ui::UiItem::Image) continue;
                                    CHECK(u.layer >= layer);
                                    layer = u.layer;
                                }
                                if (focus != 0 || money != 5000 || (owned != 2 && !vendor)) continue;
                                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                    TestConsoleRoundtrip(cs, false, 256, 192, o.first, o.second, true, &comp);
                                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                                int tx, ty;
                                // Every cell, place and Pay lands in the game's hit box around its point.
                                for (int i = 0; i < (int)v.items.size(); i++)
                                {
                                    if (v.items[i].action.empty()) continue;
                                    const Rect r = v.items[i].rect;
                                    CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - v.items[i].action[0].x) <= 15 && std::abs(ty - v.items[i].action[0].y) <= 15);
                                }
                                const CatalogueGeometry g = CatalogueLayout(vendor);
                                if (!vendor)
                                {
                                    CHECK(TouchFromComposition(comp, fit, g.down.x + g.down.w / 2.0f, g.down.y + g.down.h / 2.0f, tx, ty));
                                    CHECK(tx >= 62 && tx <= 92 && ty >= 153 && ty <= 183);
                                    CHECK(TouchFromComposition(comp, fit, g.up.x + g.up.w / 2.0f, g.up.y + g.up.h / 2.0f, tx, ty));
                                    CHECK(tx >= 30 && tx <= 60 && ty >= 153 && ty <= 183);
                                }
                                CHECK(TouchFromComposition(comp, fit, kCaClose.x + kCaClose.w / 2.0f, kCaClose.y + kCaClose.h / 2.0f, tx, ty));
                                CHECK(tx >= 220 && tx <= 250 && ty >= 4 && ty <= 34);
                                CHECK(!TouchFromComposition(comp, fit, kCaTitle.x + 10.5f, kCaTitle.y + 10.5f, tx, ty)); // the title: nothing
                                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                          // the dimmed world: nothing
                            }
                        }
        {
            // A vendor never buys back; nothing selected (just after a sale): Pay does nothing;
            // the pockets full: no purchase; no view off its screen, without RAM's part or the strings.
            GameState h = shop(true, 6, 0, 0, true, 0, 100);
            h.catalogue.owned.push_back({106 + 0x3B, 106, 569, 928, 1991, 160, 80});
            CHECK(!CataloguePays(h.catalogue));
            h = shop(false, 12, 0, 3, true, 3, 100);
            CHECK(!CataloguePays(h.catalogue) && CatalogueViewOf(cs, h).items[6].action.empty());
            h = shop(false, 12, 0, 6, false, 0, 99999);
            CHECK(!CataloguePays(h.catalogue));
            h = shop(false, 12, 0, 5, false, 0, 99999);
            CHECK(CataloguePays(h.catalogue));
            h.catalogue.money = h.catalogue.items[0].buyPrice - 1;
            CHECK(!CataloguePays(h.catalogue));
            CHECK(!CatalogueViewOf({Screen::Panel, Tab::Interact}, h).valid);
            h.catalogue.valid = false;
            CHECK(!CatalogueViewOf(cs, h).valid);
            h.catalogue.valid = true;
            CHECK(!CatalogueView(cs, &h, &rom).valid); // no strings: milestone 1's
            // Every id distinct, where the first ids met: owned place 0 holding item 1 against Pay,
            // a cell's row against an owned item id at the same index.
            {
                GameState q = shop(false, 12, 0, 2, false, 0, 100);
                q.catalogue.owned[0].id = 1;
                q.catalogue.items[0].row = 1 + 1;
                q.catalogue.items[1].row = q.catalogue.owned[1].id + 1; // (the first ids: 0x100 + 1 + row << 8 == 0x200 + 1 + id << 8)
                const MenuView w = CatalogueViewOf(cs, q);
                for (size_t a = 0; a < w.items.size(); a++)
                    for (size_t b = a + 1; b < w.items.size(); b++) CHECK(w.items[a].id != w.items[b].id);
                CHECK(w.items[kCaPayIndex].id == kCaPayId && w.items[kCaOwned0].id != kCaPayId);
            }
            // A price of 0 (or less) is never paid: the port would spend a sum it does not show.
            {
                GameState q = shop(false, 12, 0, 0, false, 0, 100);
                q.catalogue.items[0].buyPrice = 0;
                CHECK(!CataloguePays(q.catalogue) && CatalogueViewOf(cs, q).items[kCaPayIndex].action.empty());
                q.catalogue.items[0].buyPrice = -20;
                CHECK(!CataloguePays(q.catalogue));
            }
            // The cells' A word says what A does there: it selects (Pay buys); the port's own word.
            CHECK(CatalogueViewOf(cs, shop(false, 12, 0, 1, false, 0, 100)).items[0].word == OwnWord(nullptr, OwnWordId::Select));
            // The ids: a re-rolled list is other items (the focus does not stay on a new item by its place).
            GameState k = shop(false, 12, 0, 0, false, 0, 100);
            const u32 before = CatalogueViewOf(cs, k).items[0].id;
            k.catalogue.items[0].row += 1;
            CHECK(CatalogueViewOf(cs, k).items[0].id != before);
        }
        // Build Rooms (port/build.h) on made-up lists of 0-11 rooms at every scroll, money enough or
        // not, and page 1 for each room: the rows the game shows (three from its scroll), each
        // focusable, A its knob only when the money covers the permit; the game's arrows as the
        // list's scroll while it can move; B the X (the list) or the cross (page 1), the tick the
        // first focus there; never replayed. Every item inside the panel, layers ascending, the
        // mouse pieces round trip and land on the game's own buttons.
        {
            auto build = [](int count, int scroll, int page, int chosen, int money) {
                GameState h;
                h.valid = true;
                h.screen = GScreen::Build;
                BuildState& b = h.build;
                b.valid = true;
                const int places[] = {0, 1, 8, 12, 15, 16, 18, 21, 26, 28, 31};
                const int prices[] = {2000, 1000, 1500, 2500, 1000, 1500, 1000, 3500, 1000, 2000, 2500};
                for (int i = 0; i < count; i++) b.places.push_back({places[i], prices[i], 0x43 + 2, prices[i] <= money});
                b.scroll = scroll;
                b.page = page;
                b.chosen = chosen;
                b.money = money;
                return h;
            };
            const ScreenState bs {Screen::Build, Tab::None};
            for (int count : {0, 1, 2, 3, 4, 11})
                for (int money : {0, 2000, 999999})
                    for (int scroll = 0; scroll <= std::max(0, count - 3); scroll++)
                        for (int chosen = -1; chosen < count; chosen++)
                        {
                            const int page = chosen < 0 ? 0 : 1;
                            if (page == 1 && scroll > 0) continue; // (page 1 does not depend on the scroll)
                            const GameState h = build(count, scroll, page, chosen, money);
                            const MenuView v = BuildRoomsViewOf(bs, h);
                            CHECK(v.valid && v.own && !v.replay && v.back.size() == 1);
                            if (page == 0)
                            {
                                const int shown = std::min(3, count - scroll);
                                CHECK((int)v.items.size() == shown && v.backCloses && v.back[0].x == kBdCloseX && v.back[0].y == kBdCloseY);
                                CHECK(AllReachable(v));
                                for (int i = 0; i < shown; i++)
                                {
                                    const BuildEntry& e = h.build.places[scroll + i];
                                    CHECK(v.items[i].enabled && v.items[i].id == kBdRowId + (u32)e.place && v.items[i].group == v.scrollGroup);
                                    CHECK(v.items[i].action.empty() == !e.affordable);
                                    if (e.affordable) CHECK(v.items[i].action.size() == 1 && v.items[i].action[0].x == kBdRowX && v.items[i].action[0].y == kBdRowY0 + i * kBdRowDy);
                                    CHECK(MenuMove(v, i, 2) == i && MenuMove(v, i, 3) == i);
                                }
                                if (shown > 0) CHECK(MenuMove(v, 0, 0) == 0 && MenuMove(v, shown - 1, 1) == shown - 1);
                                if (shown > 1) CHECK(MenuMove(v, 0, 1) == 1 && MenuMove(v, 1, 0) == 0);
                                CHECK(v.scrollUp.empty() == (scroll == 0) && v.scrollDown.empty() == (scroll + 3 >= count));
                                if (!v.scrollUp.empty()) CHECK(v.scrollUp[0].x == kBdUpX && v.scrollUp[0].y == kBdArrowY);
                                if (!v.scrollDown.empty()) CHECK(v.scrollDown[0].x == kBdDownX && v.scrollDown[0].y == kBdArrowY);
                            }
                            else
                            {
                                CHECK(v.items.size() == 2 && !v.backCloses && v.initial == 0 && MenuFirst(v) == 0);   // (the cross first: a spend is never one press away)
                                CHECK(v.items[1].action.size() == 1 && v.items[1].action[0].x == kBdTickX && v.items[1].action[0].y == kBdConfirmY);
                                CHECK(v.items[0].action[0].x == kBdCrossX && v.back[0].x == kBdCrossX && v.back[0].y == kBdConfirmY);
                                CHECK(MenuMove(v, 1, 2) == 0 && MenuMove(v, 0, 3) == 1 && MenuMove(v, 1, 0) == 1);
                            }
                            for (int focus : {-1, 0, 1, 2})
                            {
                                Composition comp {kConsoleW, kConsoleH, {}, {}};
                                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                                world.touch = false;
                                comp.pieces.push_back(world);
                                BuildBuildRooms(comp, v, focus, h, rom, nullptr);
                                // (Page 1's question in the popups' look, as console.cpp draws it over the status bar.)
                                BuildBuildQuestion(comp, v, focus, h, rom);
                                CHECK(!comp.ui.empty());
                                if (count == 0) // (nothing left: the port's line, no arrows)
                                    CHECK(std::none_of(comp.ui.begin(), comp.ui.end(), [](const ui::UiItem& u) { return u.sign == ui::UiItem::Sign::Up; }));
                                if (page == 1) // (the question's box over the dimmed list)
                                    CHECK(std::any_of(comp.ui.begin(), comp.ui.end(), [](const ui::UiItem& u) { return u.layer == kPopLayer && u.to.x == kBdAskBox.x && u.to.w == kBdAskBox.w; }));
                                int layer = 0;
                                for (const ui::UiItem& u : comp.ui)
                                {
                                    if (u.layer == kPopLayer && u.kind != ui::UiItem::Image) { CHECK(u.layer >= layer); layer = u.layer; continue; } // (the veil: the canvas)
                                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                                    if (u.kind == ui::UiItem::Image) continue;
                                    CHECK(u.layer >= layer);
                                    layer = u.layer;
                                }
                                if (focus != 0 || money != 2000 || scroll != 0 || (chosen > 1)) continue;
                                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                    TestConsoleRoundtrip(bs, false, 256, 192, o.first, o.second, true, &comp);
                                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                                int tx, ty;
                                // Every row, the cross and the tick land within the game's circle around its point.
                                for (const ViewItem& it : v.items)
                                {
                                    const Rect r = it.rect;
                                    CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                    const int px = page == 0 ? kBdRowX : it.id == kBdTickId ? kBdTickX : kBdCrossX;
                                    const int py = page == 0 ? kBdRowY0 + (r.y - BuildRowRect(0).y) / (BuildRowRect(1).y - BuildRowRect(0).y) * kBdRowDy : kBdConfirmY;
                                    CHECK(std::abs(tx - px) <= 15 && std::abs(ty - py) <= 15);
                                }
                                if (page == 0 && count > 0)
                                {
                                    CHECK(TouchFromComposition(comp, fit, kBdUp.x + kBdUp.w / 2.0f, kBdUp.y + kBdUp.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - kBdUpX) <= 15 && std::abs(ty - kBdArrowY) <= 15);
                                    CHECK(TouchFromComposition(comp, fit, kBdDown.x + kBdDown.w / 2.0f, kBdDown.y + kBdDown.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - kBdDownX) <= 15 && std::abs(ty - kBdArrowY) <= 15);
                                }
                                if (page == 0) // (page 1: the question's box alone takes the mouse, as a popup)
                                {
                                    CHECK(TouchFromComposition(comp, fit, kBdClose.x + kBdClose.w / 2.0f, kBdClose.y + kBdClose.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - kBdCloseX) <= 15 && std::abs(ty - kBdCloseY) <= 15);
                                }
                                else CHECK(!TouchFromComposition(comp, fit, kBdClose.x + kBdClose.w / 2.0f, kBdClose.y + kBdClose.h / 2.0f, tx, ty));
                                CHECK(!TouchFromComposition(comp, fit, kBdTitle.x + 10.5f, kBdTitle.y + 10.5f, tx, ty)); // the title: nothing
                                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                          // the dimmed world: nothing
                            }
                        }
            // No view off its screen, without RAM's part or the strings; a row's id follows its room.
            GameState h = build(4, 0, 0, -1, 2000);
            CHECK(!BuildRoomsViewOf({Screen::Panel, Tab::Interact}, h).valid);
            CHECK(!BuildRoomsView(bs, &h, &rom).valid); // no strings: milestone 1's
            const u32 first = BuildRoomsViewOf(bs, h).items[0].id;
            h.build.scroll = 1;
            CHECK(BuildRoomsViewOf(bs, h).items[0].id != first);
            h.build.valid = false;
            CHECK(!BuildRoomsViewOf(bs, h).valid);
        }
        // The Registry / a guest's check-in and room info (port/checkin.h, port/roominfo.h) on made-up
        // hotels: the five rooms built or not, vacant or occupied, the Penthouse's flag; both modes.
        // A on a room only where the game takes it (the Registry: built; a check-in: built and
        // vacant), its row button; B the X; the check-in never replayed, closing (the speech
        // follows). Room info: a line per line the game shows (6 vacant, 8 occupied), A its tap,
        // each tap read back as the hint the game opens there (RoomInfoHintAt); a hint up: one
        // Close item, A and B a tap away from every line; its string unknown: no view. Every id
        // distinct (within and across both), every item reachable, inside the panel, layers
        // ascending, the mouse round trips onto the game's buttons and lines.
        {
            static u32 fakeBottom[SW * SH];
            for (int i = 0; i < SW * SH; i++) fakeBottom[i] = 0xFF000000u | (u32)(i * 2654435761u >> 8);
            const ScreenState us {Screen::Unknown, Tab::None};
            const int places[5] = {29, 8, 15, 18, 20};
            auto hotel = [&](int mode, int mask, int occupiedMask, bool flag) {
                GameState h;
                h.valid = true;
                h.screen = GScreen::Unknown;
                CheckInState& ci = h.checkIn;
                ci.valid = true;
                ci.mode = mode;
                ci.guest = mode == 0 ? 7 : -1;
                ci.score = 42;
                for (int i = 0; i < 5; i++)
                {
                    RegistryRow& r = ci.rows[i];
                    r.place = places[i];
                    r.built = (mask >> i) & 1;
                    r.occupant = r.built && ((occupiedMask >> i) & 1) ? 3 + i : -1;
                    r.infoOk = r.built && (i != 4 || flag);
                    r.available = r.infoOk && r.occupant < 0;
                }
                return h;
            };
            std::set<u32> allIds;
            for (int mode : {0, 1})
                for (int mask : {0, 1, 0x13, 0x1F})
                    for (int occ : {0, 1, 0x1E})
                        for (bool flag : {false, true})
                        {
                            const GameState h = hotel(mode, mask, occ, flag);
                            const MenuView v = CheckInViewOf(us, h);
                            CHECK(v.valid && v.own && v.items.size() == 5 && v.replay == (mode == 1) && v.backCloses);
                            CHECK(v.back.size() == 1 && v.back[0].x == kCiCloseX && v.back[0].y == kCiCloseY);
                            CHECK(AllReachable(v) && MenuFocusable(v, MenuFirst(v)));
                            std::set<u32> ids;
                            for (int i = 0; i < 5; i++)
                            {
                                const RegistryRow& r = h.checkIn.rows[i];
                                const ViewItem& it = v.items[i];
                                ids.insert(it.id);
                                allIds.insert(it.id);
                                CHECK(it.enabled && (it.id >> 24) == (kModCheckIn >> 24));
                                const bool takes = mode == 1 ? r.infoOk : r.available;
                                CHECK(it.action.empty() == !takes && it.closes == (takes && mode == 0));
                                if (takes) CHECK(it.action.size() == 1 && it.action[0].x == kCiRowX && it.action[0].y == kCiRowY0 + i * kCiRowDy);
                                CHECK(MenuMove(v, i, 2) == i && MenuMove(v, i, 3) == i);
                                if (i > 0) CHECK(MenuMove(v, i, 0) == i - 1);
                            }
                            CHECK(ids.size() == 5 && MenuMove(v, 0, 0) == 0 && MenuMove(v, 4, 1) == 4);
                            if (mask == 0x13 && occ == 1 && flag) CHECK(MenuFirst(v) == (mode == 1 ? 0 : 1)); // (the first room it takes)
                            for (int focus : {-1, 0, 2, 4})
                            {
                                Composition comp {kConsoleW, kConsoleH, {}, {}};
                                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                                world.touch = false;
                                comp.pieces.push_back(world);
                                BuildCheckIn(comp, v, focus, h, rom, nullptr, fakeBottom);
                                CHECK(!comp.ui.empty());
                                int layer = 0, crops = 0;
                                for (const ui::UiItem& u : comp.ui)
                                {
                                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                                    if (u.kind == ui::UiItem::Image) { crops++; continue; }
                                    CHECK(u.layer >= layer);
                                    layer = u.layer;
                                }
                                // An occupied room's portrait: the game's own row button, live (2x in its row; 3x focused).
                                int occupied = 0;
                                for (const RegistryRow& r : h.checkIn.rows) occupied += r.occupant >= 0;
                                const int f = focus >= 0 ? focus : 0;
                                CHECK(crops == occupied + (h.checkIn.rows[f].occupant >= 0));
                                if (focus != 0 || mask != 0x1F || occ != 1 || flag) continue;
                                for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                    TestConsoleRoundtrip(us, false, 256, 192, o.first, o.second, true, &comp);
                                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                                int tx, ty;
                                for (int i = 0; i < 5; i++)
                                {
                                    const Rect r = v.items[i].rect;
                                    CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - kCiRowX) <= 15 && std::abs(ty - (kCiRowY0 + i * kCiRowDy)) <= 15);
                                }
                                CHECK(TouchFromComposition(comp, fit, kCiClose.x + kCiClose.w / 2.0f, kCiClose.y + kCiClose.h / 2.0f, tx, ty));
                                CHECK(std::abs(tx - kCiCloseX) <= 15 && std::abs(ty - kCiCloseY) <= 15);
                                CHECK(!TouchFromComposition(comp, fit, kCiInfo.x + 10.5f, kCiInfo.y + 200.5f, tx, ty)); // the focused room: nothing
                                CHECK(!TouchFromComposition(comp, fit, 20.5f, 360.5f, tx, ty));                    // the dimmed world: nothing
                            }
                        }
            // No view off its screen, without RAM's part, with a mode the game has not, or the strings.
            GameState h = hotel(1, 1, 0, false);
            CHECK(!CheckInViewOf({Screen::Panel, Tab::Interact}, h).valid);
            CHECK(!CheckInView(us, &h, &rom).valid); // no strings: milestone 1's
            h.checkIn.mode = 2;
            CHECK(!CheckInViewOf(us, h).valid);
            h.checkIn.valid = false;
            CHECK(!CheckInViewOf(us, h).valid);

            // Room info, vacant and occupied, a hint up or not.
            auto info = [&](bool occupied, bool hint, int hintId) {
                GameState g;
                g.valid = true;
                g.screen = GScreen::Unknown;
                RoomInfoState& ri = g.roomInfo;
                ri.valid = true;
                ri.place = 29;
                ri.occupied = occupied;
                ri.occupant = occupied ? 7 : 45;
                ri.moodId = occupied ? 2087 : -1;
                ri.checkOutKnown = occupied;
                ri.month = 1; ri.day = 3; ri.days = 2;
                ri.baseValue = 400; ri.serviceCharge = 2; ri.scoreBonus = 0; ri.total = 422;
                ri.hint = hint;
                ri.hintId = hintId;
                return g;
            };
            for (bool occupied : {false, true})
            {
                const GameState g = info(occupied, false, -1);
                const MenuView v = RoomInfoViewOf(us, g);
                const int n = occupied ? 8 : 6;
                CHECK(v.valid && v.own && !v.popup && (int)v.items.size() == n && !v.backCloses && v.back.size() == 1);
                CHECK(v.back[0].x == kRiCloseX && v.back[0].y == kRiCloseY && RoomInfoHintAt(kRiCloseX, kRiCloseY, occupied) < 0);
                CHECK(AllReachable(v) && MenuFirst(v) == 1);
                std::set<int> hints;
                std::set<u32> ids = allIds; // (the Registry's ids)
                for (int k = 0; k < n; k++)
                {
                    const ViewItem& it = v.items[k];
                    CHECK(ids.insert(it.id).second); // (distinct from each other and the Registry's)
                    CHECK(it.enabled && it.action.size() == 1 && it.action[0].x == kRiLineX);
                    const int hid = RoomInfoHintAt(it.action[0].x, it.action[0].y, occupied);
                    CHECK(hid >= 2594 && hid <= 2603 && hints.insert(hid).second);
                    CHECK(MenuMove(v, k, 2) == k && MenuMove(v, k, 3) == k);
                    if (k > 0) CHECK(MenuMove(v, k, 0) == k - 1);
                }
                // (the ids carry the room: another room's line is another item; a vacant room's lines close up)
                CHECK(v.items[1].id == RoomInfoLineId(29, 1) && v.items[1].id != RoomInfoLineId(8, 1));
                for (int k = 1; k < n; k++) CHECK(v.items[k].rect.y == v.items[k - 1].rect.y + 62 || k == 1);
                CHECK(v.items[1].rect.y == RoomInfoLineRect(1).y && (occupied || v.items[2].rect.y == RoomInfoLineRect(1).y + 62));
                CHECK(hints.count(occupied ? 2595 : 2596) == 1 && hints.count(2601) == (size_t)occupied && hints.count(2600) == 1);
                CHECK(RoomInfoHintAt(kRiAwayX, kRiAwayY, occupied) < 0 && RoomInfoHintAt(40, 63, false) < 0);
                for (int focus : {-1, 0, 1, n - 1})
                {
                    Composition comp {kConsoleW, kConsoleH, {}, {}};
                    Piece world = ConsoleWorld(256, HudCropRows(192), true);
                    world.touch = false;
                    comp.pieces.push_back(world);
                    BuildRoomInfo(comp, v, focus, g, rom, nullptr, fakeBottom);
                    int layer = 0, crops = 0;
                    for (const ui::UiItem& u : comp.ui)
                    {
                        CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                              u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                        if (u.kind == ui::UiItem::Image) { crops++; continue; }
                        CHECK(u.layer >= layer);
                        layer = u.layer;
                    }
                    CHECK(crops == 1); // (the portrait: the occupant's, or the concierge's when vacant)
                    if (focus != 1) continue;
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(us, false, 256, 192, o.first, o.second, true, &comp);
                    const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                    int tx, ty;
                    for (const ViewItem& it : v.items)
                    {
                        const Rect r = it.rect;
                        CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                        CHECK(RoomInfoHintAt(tx, ty, occupied) == RoomInfoHintAt(it.action[0].x, it.action[0].y, occupied));
                    }
                    CHECK(TouchFromComposition(comp, fit, kRiClose.x + kRiClose.w / 2.0f, kRiClose.y + kRiClose.h / 2.0f, tx, ty));
                    CHECK(std::abs(tx - kRiCloseX) <= 15 && std::abs(ty - kRiCloseY) <= 15);
                    CHECK(!TouchFromComposition(comp, fit, kRiPortrait.x + 10.5f, kRiPortrait.y + 10.5f, tx, ty)); // the portrait: nothing
                }
                // The hint box: one Close, A and B a tap on no line; over the lines (no focus there).
                const GameState gh = info(occupied, true, occupied ? 2601 : 2596);
                const MenuView hv = RoomInfoViewOf(us, gh);
                CHECK(hv.valid && hv.own && hv.popup && hv.items.size() == 1 && hv.items[0].id == kRiHintCloseId && ids.count(kRiHintCloseId) == 0);
                CHECK(hv.items[0].action.size() == 1 && RoomInfoHintAt(hv.items[0].action[0].x, hv.items[0].action[0].y, occupied) < 0 && hv.back == hv.items[0].action);
                CHECK(hv.items[0].action[0].x != kRiCloseX && RoomHintUp(gh) && !RoomHintUp(g));
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                BuildRoomInfo(comp, hv, 0, gh, rom, nullptr, fakeBottom);
                const size_t beneath = comp.ui.size();
                BuildRoomHint(comp, hv, 0, gh, rom);
                CHECK(comp.ui.size() > beneath);
                for (size_t i = beneath; i < comp.ui.size(); i++) CHECK(comp.ui[i].layer >= kPopLayer);
                for (const ui::UiItem& u : comp.ui) CHECK(u.kind != ui::UiItem::Image || u.to.x + u.to.w <= kPopBox.x); // (the portrait under the box: gone)
                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                int tx, ty;
                CHECK(TouchFromComposition(comp, fit, kPopClose.x + kPopClose.w / 2.0f, kPopClose.y + kPopClose.h / 2.0f, tx, ty));
                CHECK(RoomInfoHintAt(tx, ty, occupied) < 0 && std::abs(tx - kRiAwayX) <= 12 && std::abs(ty - kRiAwayY) <= 12);
                CHECK(!RoomInfoViewOf(us, info(occupied, true, -1)).valid); // (which hint unknown: milestone 1's)
            }
            GameState g = info(true, false, -1);
            CHECK(!RoomInfoViewOf({Screen::Panel, Tab::Interact}, g).valid);
            CHECK(!RoomInfoView(us, &g, &rom).valid); // no strings: milestone 1's
            g.roomInfo.place = 40;
            CHECK(!RoomInfoViewOf(us, g).valid);
        }
        // The music panel and the stat panel (port/music.h, port/statpanel.h) on made-up states.
        // Music: lists of 0, 1, 5, 21 and 27 songs at every scroll, the song playing in view, out
        // of it or none: a row per row the game shows, A its play button but on the song playing
        // (nothing), Up / Down past the rows the game's arrows only while it can scroll, B the X;
        // ids distinct. The stat panel: every page and transition: Main's four buttons (A their
        // taps), a page's one item (its tab, Main's button's id for it), the game's way between
        // pages (StatPageAction: a wait first while the old page fades out; the X, a wait and
        // the button from a page), B the X (closing from Main only). Both: every item reachable,
        // inside the panel, layers ascending, the mouse round trips onto the game's buttons.
        {
            const ScreenState us {Screen::Unknown, Tab::None};
            auto inside = [&](const Composition& comp) {
                int layer = 0;
                for (const ui::UiItem& u : comp.ui)
                {
                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                    if (u.kind == ui::UiItem::Image) continue;
                    CHECK(u.layer >= layer);
                    layer = u.layer;
                }
            };
            auto lands = [&](const Composition& comp, Rect hd, int x, int y, int tol) {
                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                int tx, ty;
                CHECK(TouchFromComposition(comp, fit, hd.x + hd.w / 2.0f, hd.y + hd.h / 2.0f, tx, ty));
                CHECK(std::abs(tx - x) <= tol && std::abs(ty - y) <= tol);
            };
            auto world = [&]() {
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                Piece w = ConsoleWorld(256, HudCropRows(192), true);
                w.touch = false;
                comp.pieces.push_back(w);
                return comp;
            };
            for (int count : {0, 1, 5, 21, 27})
            {
                const int rows = std::min(count, kMuRows);
                for (int scroll = 0; scroll <= count - rows; scroll += std::max(1, (count - rows) / 3))
                    for (int playing : {-1, 0, 2, count - 1})
                    {
                        if (playing >= count) continue;
                        GameState g;
                        g.valid = true;
                        MusicState& m = g.music;
                        m.valid = true;
                        for (int i = 0; i < count; i++) m.list.push_back({i, i + 1, 101 + i, -1});
                        m.scroll = scroll;
                        m.playing = playing;
                        m.place = 4;
                        const MenuView v = MusicViewOf(us, g);
                        CHECK(v.valid && v.own && v.backCloses && (int)v.items.size() == rows && AllReachable(v));
                        CHECK(v.back.size() == 1 && v.back[0].x == kMuCloseX && v.back[0].y == kMuCloseY);
                        CHECK(v.scrollUp.empty() == (scroll == 0) && v.scrollDown.empty() == (scroll >= count - rows));
                        if (!v.scrollUp.empty()) CHECK(v.scrollUp[0].x == kMuUpX && v.scrollUp[0].y == kMuUpY);
                        if (!v.scrollDown.empty()) CHECK(v.scrollDown[0].x == kMuDownX && v.scrollDown[0].y == kMuDownY);
                        std::set<u32> ids;
                        for (int i = 0; i < rows; i++)
                        {
                            const ViewItem& it = v.items[i];
                            ids.insert(it.id);
                            CHECK((it.id >> 24) == (kModMusic >> 24) && it.enabled && it.group == v.scrollGroup);
                            CHECK(it.action.empty() == (scroll + i == playing));
                            if (!it.action.empty()) CHECK(it.action.size() == 1 && it.action[0].x == kMuRowX && it.action[0].y == kMuRowY0 + i * kMuRowDy);
                            CHECK(MenuMove(v, i, 2) == i && MenuMove(v, i, 3) == i);
                        }
                        CHECK((int)ids.size() == rows);
                        if (playing >= scroll && playing < scroll + rows) CHECK(v.initial == playing - scroll);
                        for (int focus : {-1, 0, rows - 1})
                        {
                            Composition comp = world();
                            BuildMusic(comp, v, focus, g, rom, nullptr);
                            CHECK(!comp.ui.empty());
                            inside(comp);
                            if (focus != 0 || count != 21 || scroll == 0) continue;
                            for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                TestConsoleRoundtrip(us, false, 256, 192, o.first, o.second, true, &comp);
                            for (int i = 0; i < rows; i++) lands(comp, v.items[i].rect, kMuRowX, kMuRowY0 + i * kMuRowDy, 15);
                            lands(comp, kMuClose, kMuCloseX, kMuCloseY, 15);
                            lands(comp, kMuUp, kMuUpX, kMuUpY + 10, 15);
                            lands(comp, kMuDown, kMuDownX, kMuDownY - 10, 15);
                        }
                    }
            }
            {
                GameState g;
                g.valid = true;
                g.music.valid = true;
                g.music.list = {{3, 1, 101, -1}};
                CHECK(MusicViewOf(us, g).valid && !MusicViewOf({Screen::Panel, Tab::Interact}, g).valid);
                CHECK(!MusicView(us, &g, &rom).valid); // no strings: milestone 1's
                g.music.scroll = 1;
                CHECK(!MusicViewOf(us, g).valid);
                g.music.valid = false;
                CHECK(!MusicViewOf(us, g).valid);
            }
            for (int page = 0; page < kStPages; page++)
                for (int transition = 0; transition <= 2; transition++)
                {
                    GameState g;
                    g.valid = true;
                    StatState& st = g.stat;
                    st.valid = true;
                    st.page = page;
                    st.transition = transition;
                    st.aspiration = page % 4;
                    st.level = page % 4;
                    st.gaugeFrame = 7;
                    for (int i = 0; i < 5; i++) st.skills[i] = 2 * i;
                    // The game's way to every page.
                    for (int to = 0; to < kStPages; to++)
                    {
                        const MenuAction a = StatPageAction(st, to);
                        if (to == page) { CHECK(a.empty()); continue; }
                        size_t k = 0;
                        if (transition == 1) CHECK(a[k].kind == MenuStep::Wait && a[k++].frames == kStFadeWait);
                        if (page == 0) CHECK(a.size() == k + 1 && a[k].kind == MenuStep::Tap && a[k].x == kStButtonX[to] && a[k].y == kStButtonY[to]);
                        else
                        {
                            CHECK(a[k].kind == MenuStep::Tap && a[k].x == kStCloseX && a[k].y == kStCloseY);
                            if (to == 0) CHECK(a.size() == k + 1);
                            else CHECK(a.size() == k + 4 && a[k + 1].kind == MenuStep::Wait && a[k + 1].frames == kStSwitchWait && a[k + 2].x == kStButtonX[to] && a[k + 2].y == kStButtonY[to] &&
                                       a[k + 3].kind == MenuStep::Wait && a[k + 3].frames == kStShowWait);
                        }
                        // Every tap inside the game's own rectangle for it.
                        for (const MenuStep& s : a)
                            if (s.kind == MenuStep::Tap)
                            {
                                const Rect r = s.x == kStCloseX && s.y == kStCloseY ? kStCloseFrom : kStButtonFrom[to];
                                CHECK(s.x >= r.x && s.x < r.x + r.w && s.y >= r.y && s.y < r.y + r.h);
                            }
                    }
                    const MenuView v = StatPanelViewOf(us, g);
                    CHECK(v.valid && v.own && AllReachable(v) && v.backCloses == (page == 0));
                    CHECK(v.buttons[(int)MenuButton::LB] == StatPageAction(st, (page + 4) % 5) && v.buttons[(int)MenuButton::RB] == StatPageAction(st, (page + 1) % 5));
                    if (page == 0)
                    {
                        CHECK(v.items.size() == 4 && v.buttons[(int)MenuButton::Left].empty() && v.buttons[(int)MenuButton::Right].empty());
                        std::set<u32> ids;
                        for (int p = 1; p < kStPages; p++)
                        {
                            const ViewItem& it = v.items[p - 1];
                            ids.insert(it.id);
                            CHECK(it.id == kStPageId + (u32)p && it.action == StatPageAction(st, p));
                        }
                        CHECK(ids.size() == 4 && v.back.back().x == kStCloseX && v.back.back().y == kStCloseY);
                    }
                    else
                    {
                        CHECK(v.items.size() == 1 && v.items[0].id == kStPageId + (u32)page && v.items[0].action.empty() && v.back == StatPageAction(st, 0));
                        CHECK(v.buttons[(int)MenuButton::Left] == StatPageAction(st, page - 1) && v.buttons[(int)MenuButton::Right] == StatPageAction(st, (page + 1) % 5));
                    }
                    for (int focus : {-1, 0, (int)v.items.size() - 1})
                    {
                        Composition comp = world();
                        BuildStatPanel(comp, v, focus, g, rom, nullptr);
                        CHECK(!comp.ui.empty());
                        inside(comp);
                        if (focus != 0 || transition != 0) continue;
                        for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                            TestConsoleRoundtrip(us, false, 256, 192, o.first, o.second, true, &comp);
                        lands(comp, kStClose, kStCloseX, kStCloseY, 16);
                        if (page == 0)
                            for (int p = 1; p < kStPages; p++)
                            {
                                lands(comp, StatButtonRect(p), kStButtonX[p], kStButtonY[p], 30);
                                lands(comp, StatTabRect(p), kStButtonX[p], kStButtonY[p], 30);
                            }
                        else lands(comp, StatTabRect(0), kStCloseX, kStCloseY, 16);
                    }
                }
            {
                GameState g;
                g.valid = true;
                g.stat.valid = true;
                CHECK(StatPanelViewOf(us, g).valid && !StatPanelViewOf({Screen::Panel, Tab::Interact}, g).valid);
                CHECK(!StatPanelView(us, &g, &rom).valid); // no strings: milestone 1's
                g.stat.page = 5;
                CHECK(!StatPanelViewOf(us, g).valid);
                g.stat.page = 0;
                g.stat.valid = false;
                CHECK(!StatPanelViewOf(us, g).valid);
            }
            // Ids: the two modules' tags apart from each other and the others'.
            CHECK((kMuRowId >> 24) != (kStPageId >> 24) && (kMuRowId >> 24) != (kRiLineId >> 24) && (kStPageId >> 24) != (kCiRowId >> 24));
        }
        // Goals, Relationships, Options (port/goals.h, relations.h, options.h) and the Save slots
        // on made-up states: the game's selection held (Up / Down its arrows), Options' moves
        // explicit and its sliders' Left / Right the knob's drag by a tenth, the Save slots' A its
        // tick and B its X; every enabled item reachable (the phone too, but where the game owns
        // the selection: the focus stays on it); every item inside the panel, layers ascending,
        // the mouse pieces round trip and land on the game's own targets.
        {
            auto check = [&](ScreenState st, const MenuView& v, const GameState& h, int focus) {
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                world.touch = false;
                comp.pieces.push_back(world);
                std::vector<u32> pic(SW * SH, 0xFF808080);
                if (st.screen == Screen::SaveSlots) BuildSaveSlots(comp, v, focus, h, rom);
                else BuildPanelMenu(comp, st, v, focus, h, rom, nullptr, pic.data());
                int layer = 0;
                for (const ui::UiItem& u : comp.ui)
                {
                    CHECK(u.to.x >= kPmPanel.x && u.to.y >= kPmPanel.y && u.to.x + u.to.w <= kPmPanel.x + kPmPanel.w &&
                          u.to.y + u.to.h <= kPmPanel.y + kPmPanel.h);
                    if (u.kind == ui::UiItem::Image) continue;
                    CHECK(u.layer >= layer);
                    layer = u.layer;
                }
                for (const ViewItem& it : v.items)
                    CHECK(it.rect.x >= kPmPanel.x && it.rect.y >= kPmPanel.y && it.rect.x + it.rect.w <= kPmPanel.x + kPmPanel.w &&
                          it.rect.y + it.rect.h <= kPmPanel.y + kPmPanel.h);
                bool touchable = false; // ("Saving" over the slots: nothing to touch)
                for (const Piece& q : comp.pieces) touchable |= q.src == Src::Bottom;
                if (touchable)
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(st, true, 256, 192, o.first, o.second, true, &comp);
                return comp;
            };
            auto inside = [](int x, int y, Rect a) { return x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h; };
            const CanvasFit fit = CanvasFit::For(kConsoleW, kConsoleH, 1280, 720);
            int tx, ty;
            // Goals: 8 goals of 1-3 DS lines (14 lines), the selection and the game's scroll.
            const ScreenState gs {Screen::Panel, Tab::Goals};
            for (int n : {0, 1, 8})
                for (int sel : {-1, 0, 3, 7})
                    for (bool footer : {false, true})
                    {
                        if (sel >= n) continue;
                        GameState h;
                        h.valid = true;
                        GoalsState& g = h.town.goals;
                        g.valid = true;
                        int line = 0;
                        for (int i = 0; i < n; i++)
                        {
                            const int lines = 1 + i % 3;
                            g.entries.push_back({true, i, 1627 + i, -1, -1, line, lines});
                            line += lines;
                        }
                        g.lines = line;
                        g.selected = sel;
                        g.first = sel <= 0 ? 0 : std::max(0, g.entries[sel].firstLine + g.entries[sel].nLines - 7);
                        g.footer = footer;
                        g.footerId = 1843;
                        g.score = 100;
                        const MenuView v = PanelMenuViewOf(gs, h);
                        CHECK(v.valid && AllReachable(v));
                        // The game's arrows, none past the list's ends (Up from the first goal: the phone).
                        CHECK(v.scrollUp.empty() == (sel == 0) && (sel == 0 || v.scrollUp[0].x == kGoalsUpX));
                        CHECK(v.scrollDown.empty() == (sel >= 0 && sel == n - 1) && (v.scrollDown.empty() || v.scrollDown[0].y == kGoalsDownY));
                        if (sel == 0) CHECK(MenuMove(v, v.selected, 0) == (int)v.items.size() - 1);
                        if (sel >= 0)
                        {
                            CHECK(MenuFocusable(v, v.selected) && v.items[v.selected].id == kGoalsId + (u32)sel);
                        }
                        const Rect list = footer ? kGoalsListFooter : kGoalsList;
                        for (int i = 0; i + 1 < (int)v.items.size(); i++)
                            CHECK(v.items[i].rect.y >= list.y && v.items[i].rect.y + v.items[i].rect.h <= list.y + list.h);
                        const Composition comp = check(gs, v, h, v.selected >= 0 ? v.selected : 0);
                        // No goal: the port's words in the list (else none of them).
                        {
                            int said = 0;
                            for (const ui::UiItem& u : comp.ui) said += u.text == OwnWord(&rom, OwnWordId::NoGoals);
                            CHECK(said == (n == 0 ? 1 : 0));
                        }
                        // A goal the touch screen shows: its bar lands on its lines.
                        for (int i = 0; i + 1 < (int)v.items.size(); i++)
                        {
                            const GoalEntry& e = g.entries[v.items[i].id - kGoalsId];
                            if (e.firstLine < g.first || e.firstLine >= g.first + 7) continue;
                            const Rect r = v.items[i].rect;
                            CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + 2.5f, tx, ty));
                            CHECK(inside(tx, ty, {kGoalsLineX, kGoalsLineY0 + 16 * (e.firstLine - g.first), kGoalsLineW, 16}));
                        }
                    }
            // Relationships: 0, 1, 5 or 23 Sims, the game's first shown and selection.
            const ScreenState rs {Screen::Panel, Tab::Relationships};
            for (int n : {0, 1, 5, 23})
                for (int sel : {0, 2, 4, 22})
                {
                    if (sel >= std::max(n, 1)) continue;
                    GameState h;
                    h.valid = true;
                    RelState& r = h.town.relations;
                    r.valid = true;
                    for (int i = 0; i < n; i++) r.sims.push_back({i, 323 + i, 389 + i, 184, 195, -1, false});
                    r.selected = n ? sel : -1;
                    r.first = std::max(0, std::min(sel - 3, n - 4));
                    const MenuView v = PanelMenuViewOf(rs, h);
                    CHECK(v.valid && (int)v.items.size() == std::min(4, n) + 1);
                    if (n)
                    {
                        CHECK(MenuFocusable(v, v.selected) && v.selected == sel - r.first);
                        CHECK(v.scrollUp.empty() == (sel == 0) && v.scrollDown.empty() == (sel == n - 1));
                        if (sel > 0) CHECK(v.scrollUp[0].x == kRelPrevX && v.scrollUp[0].y == kRelPrevY);
                        if (sel + 1 < n) CHECK(v.scrollDown[0].y == kRelNextY);
                        if (sel == 0) CHECK(MenuMove(v, v.selected, 0) == (int)v.items.size() - 1); // the phone
                    }
                    const Composition comp = check(rs, v, h, v.selected);
                    for (int i = 0; i < std::min(4, n); i++)
                    {
                        const Rect q = v.items[i].rect;
                        CHECK(TouchFromComposition(comp, fit, q.x + q.w / 2.0f, q.y + q.h / 2.0f, tx, ty));
                        CHECK(inside(tx, ty, HotspotsFor(rs)[1 + i].area));
                    }
                }
            // Options: both volumes at each end and between.
            const ScreenState os {Screen::Panel, Tab::Options};
            for (int vol : {0, 1229, 4096})
            {
                GameState h;
                h.valid = true;
                h.town.options = {true, vol, 4096 - vol};
                const MenuView v = PanelMenuViewOf(os, h);
                CHECK(v.valid && v.items.size() == 5 && AllReachable(v));
                CHECK(v.items[0].action[0].x == kOptSaveX && v.items[1].action[0].y == kOptQuitY);
                CHECK(MenuMove(v, 0, 1) == 2 && MenuMove(v, 1, 1) == 2 && MenuMove(v, 2, 0) == 0 && MenuMove(v, 2, 1) == 3);
                CHECK(MenuMove(v, 0, 0) == 4 && MenuMove(v, 4, 1) == 0); // the phone above Save
                const int k = VolumeNotch(vol);
                const ViewItem& m = v.items[2];
                CHECK(m.left.empty() == (k == 0) && m.right.empty() == (k == 10));
                if (k > 0) CHECK(m.left[0].kind == MenuStep::Drag && m.left[0].x == 72 + 16 * k && m.left[0].x2 == 72 + 16 * (k - 1) && m.left[0].y == 121);
                if (k < 10) CHECK(m.right[0].x2 == 72 + 16 * (k + 1));
                if (VolumeNotch(4096 - vol) < 10) CHECK(v.items[3].right[0].y == 171);
                const Composition comp = check(os, v, h, 2);
                CHECK(TouchFromComposition(comp, fit, kOptSaveBtn.x + kOptSaveBtn.w / 2.0f, kOptSaveBtn.y + kOptSaveBtn.h / 2.0f, tx, ty));
                CHECK(inside(tx, ty, HotspotsFor(os)[1].area));
            }
            CHECK(VolumeNotch(0) == 0 && VolumeNotch(1229) == 3 && VolumeNotch(3686) == 9 && VolumeNotch(4096) == 10);
            // The Save slots: used, empty, the current one.
            const ScreenState ss {Screen::SaveSlots, Tab::None};
            for (int cur : {-1, 1})
            {
                GameState h;
                h.valid = true;
                SaveSlots& sv = h.town.saves;
                sv.valid = true;
                sv.current = cur;
                sv.used[1] = sv.slot[1].used = true;
                sv.slot[1] = {true, 9150, 0x14, 0, "Jun"};
                const MenuView v = SaveSlotsViewOf(ss, h);
                CHECK(v.valid && v.items.size() == 3 && AllReachable(v) && MenuFirst(v) == (cur < 0 ? 0 : cur) && !v.replay);
                CHECK(v.back.size() == 1 && v.back[0].x == kSlotCloseX && v.back[0].y == kSlotCloseY);
                for (int i = 0; i < 3; i++) CHECK(v.items[i].action[0].x == kSlotTickX && v.items[i].action[0].y == kSlotTickY[i]);
                const Composition comp = check(ss, v, h, 1);
                const Rect q = v.items[2].rect;
                CHECK(TouchFromComposition(comp, fit, q.x + q.w / 2.0f, q.y + q.h / 2.0f, tx, ty));
                CHECK(inside(tx, ty, {kSlotTickX - 12, kSlotTickY[2] - 12, 24, 24}));
                // No current slot, the first used: the first empty one.
                GameState e = h;
                e.town.saves.current = -1;
                e.town.saves.used[0] = true;
                CHECK(MenuFirst(SaveSlotsViewOf(ss, e)) == 2);
                // The game's question over a used slot (popup kind 2) or "Saving" (kind 3): the slots
                // wait (disabled, no B) under the popup (port/popup.h: its view takes the input).
                GameState pq = h;
                pq.town.popup = {true, 2, 2, "Overwrite this save?", "", -1, false, 48, 160, 6};
                const MenuView a = SaveSlotsViewOf(ss, pq);
                CHECK(a.valid && a.items.size() == 3 && MenuFirst(a) == -1 && a.back.empty());
                const MenuView ap = PopupViewOf(ss, pq);
                // No first and the first focus (the safe answer), Yes one Right away; B No.
                CHECK(ap.valid && ap.popup && ap.own && ap.items.size() == 2 && !ap.replay && ap.question && MenuFirst(ap) == 0);
                CHECK(ap.items[0].id == kPopNoId && ap.items[0].action[0].x == 92 && ap.items[0].action[0].y == 108 && ap.items[0].closes);
                CHECK(ap.items[1].id == kPopYesId && ap.items[1].action[0].x == 164 && ap.items[1].action[0].y == 108 && ap.items[1].closes);
                CHECK(ap.back[0].x == 92 && ap.back[0].y == 108 && MenuMove(ap, 0, 3) == 1 && MenuMove(ap, 1, 2) == 0);
                pq.town.popup = {true, 2, 3, "Do not turn off", "Saving", -1, false, 48, 160, 6};
                const MenuView w = SaveSlotsViewOf(ss, pq);
                CHECK(w.valid && MenuFirst(w) == -1 && w.back.empty());
                check(ss, w, pq, -1);
                CHECK(!SaveSlotsViewOf({Screen::Panel, Tab::Options}, h).valid);
                CHECK(!SaveSlotsView(ss, &h, &rom).valid); // no strings: the stick cursor's screen
                h.town.saves.valid = false;
                CHECK(!SaveSlotsViewOf(ss, h).valid);
            }
        }
        // Popups, the phone and notes (port/popup.h, phone.h, note.h) on made-up states.
        {
            const CanvasFit fit = CanvasFit::For(kConsoleW, kConsoleH, 1280, 720);
            auto inside = [](int x, int y, Rect a) { return x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h; };
            int tx, ty, bx, by;
            // The game's buttons from its box (0x02057314): Game Paused / tips, the goal, the question.
            PopupState pp {true, 2, 0, "Press START", "Game Paused", -1, false, 40, 160, 10};
            CHECK(PopupButton(pp, 0, bx, by) && bx == 132 && by == 132 && !PopupButton(pp, 1, bx, by));
            pp.boxY = 56; pp.boxTiles = 6;
            CHECK(PopupButton(pp, 0, bx, by) && bx == 132 && by == 116);
            pp.kind = 2;
            CHECK(PopupButton(pp, 0, bx, by) && bx == 92 && by == 116 && PopupButton(pp, 1, bx, by) && bx == 164 && by == 116);
            pp.kind = 3;
            CHECK(!PopupButton(pp, 0, bx, by));
            pp.kind = 0; pp.boxW = 0;
            CHECK(!PopupButton(pp, 0, bx, by));
            // Over the panel (its open menu drawn beneath), Options > Quit's (Unknown), over the
            // phone; never over a screen the port does not redraw, nor without the box.
            for (int kind : {0, 2, 3})
                for (ScreenState on : {ScreenState {Screen::Popup, Tab::None}, ScreenState {Screen::Unknown, Tab::None}})
                {
                    GameState h;
                    h.valid = true;
                    h.screen = GScreen::Popup;
                    h.tab = on.screen == Screen::Unknown ? 4 : 0;
                    h.town.popup = {true, 2, kind, std::string(300, 'w'), kind == 2 ? "stale" : "Title", -1, false, 56, 160, 6};
                    h.town.options = {true, 4096, 2048};
                    const MenuView v = PopupViewOf(on, h);
                    CHECK(v.valid && v.own && v.popup && (int)v.items.size() == (kind == 3 ? 0 : kind == 2 ? 2 : 1) && v.back.empty() == (kind == 3));
                    if (kind == 2) CHECK(MenuFirst(v) == 0 && v.items[0].id == kPopNoId);
                    CHECK(UnderScreen(on, h).screen == Screen::Panel);
                    Composition comp {kConsoleW, kConsoleH, {}, {}};
                    Piece world = ConsoleWorld(256, HudCropRows(192), true);
                    world.touch = false;
                    comp.pieces.push_back(world);
                    const ScreenState under = UnderScreen(on, h);
                    const MenuView u = PanelMenuViewOf(under, h);
                    if (u.valid) BuildPanelMenu(comp, under, u, 0, h, rom, nullptr, face.data());
                    std::vector<u32> crop(SW * SH, 0xFF808080);
                    comp.ui.push_back(ui::ImageItem({kPopBox.x + 10, kPopBox.y + 10, 60, 60}, crop.data(), SW, SH, {0, 0, 20, 20})); // (a picture under the box)
                    ui::UiItem barItem = ui::PanelItem({16, 8, 200, 40}); // (the status bar's panel: never under the veil)
                    barItem.layer = kStatusBarLayer;
                    comp.ui.push_back(barItem);
                    BuildPopup(comp, v, 0, h, rom);
                    SortUi(comp.ui);
                    int bars = 0;
                    for (const ui::UiItem& it : comp.ui) bars += it.kind != ui::UiItem::Image && it.layer == kPopLayer + 1 && it.to.x == 16 && it.to.w == 200;
                    CHECK(bars == 1);
                    int layer = 0;
                    for (const ui::UiItem& it : comp.ui)
                    {
                        if (it.kind == ui::UiItem::Image)
                        {
                            CHECK(!(it.to.x < kPopBox.x + kPopBox.w && kPopBox.x < it.to.x + it.to.w && it.to.y < kPopBox.y + kPopBox.h && kPopBox.y < it.to.y + it.to.h));
                            continue;
                        }
                        CHECK(it.layer >= layer);
                        layer = it.layer;
                        if (it.layer == kPopLayer && it.to.w < kConsoleW)
                            CHECK(it.to.x >= kPopBox.x - 8 && it.to.x + it.to.w <= kPopBox.x + kPopBox.w + 8 && it.to.y >= kPopTitle.y - 8 &&
                                  it.to.y + it.to.h <= kPopBox.y + kPopBox.h + 8);
                    }
                    // The mouse: the HD buttons onto the game's; the menu beneath takes none.
                    if (kind == 0)
                    {
                        CHECK(TouchFromComposition(comp, fit, kPopClose.x + kPopClose.w / 2.0f, kPopClose.y + kPopClose.h / 2.0f, tx, ty));
                        CHECK(inside(tx, ty, {120, 104, 24, 24}));
                    }
                    if (kind == 2)
                    {
                        CHECK(TouchFromComposition(comp, fit, kPopYes.x + kPopYes.w / 2.0f, kPopYes.y + kPopYes.h / 2.0f, tx, ty) && inside(tx, ty, {152, 104, 24, 24}));
                        CHECK(TouchFromComposition(comp, fit, kPopNo.x + kPopNo.w / 2.0f, kPopNo.y + kPopNo.h / 2.0f, tx, ty) && inside(tx, ty, {80, 104, 24, 24}));
                    }
                    CHECK(!TouchFromComposition(comp, fit, kPmPhoneWell.x + 10.5f, kPmPhoneWell.y + 10.5f, tx, ty)); // the menu's phone beneath
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        if (kind != 3) TestConsoleRoundtrip(on, true, 256, 192, o.first, o.second, true, &comp);
                    GameState off = h;
                    off.town.popup.state = 0;
                    CHECK(!PopupViewOf(on, off).valid);
                    off = h;
                    off.screen = GScreen::Vacuum; // (a screen the port does not redraw: milestone 1's popup)
                    CHECK(!PopupViewOf({Screen::Vacuum, Tab::None}, off).valid);
                    off.screen = GScreen::Build; // (over the HD Build Rooms: the HD popup)
                    CHECK(PopupViewOf({Screen::Build, Tab::None}, off).valid == PopupViewOf(on, h).valid);
                    off.screen = GScreen::Catalogue; // (over the HD catalogue: the HD popup)
                    CHECK(PopupViewOf({Screen::Catalogue, Tab::None}, off).valid == PopupViewOf(on, h).valid);
                    off = h;
                    off.town.popup.boxW = 0;   // (its box not placed yet, sliding in: the HD popup, nothing to press)
                    CHECK(PopupViewOf(on, off).valid && PopupViewOf(on, off).items.empty() && PopupViewOf(on, off).back.empty());
                    CHECK(!PopupView(on, &h, &rom).valid); // (no strings: milestone 1's)
                }
            {
                // A popup over the HD catalogue (port/catalogue.h): the popup's view, the catalogue
                // drawn beneath it without a focus, its cells taking no click, the popup's X on the game's.
                GameState h;
                h.valid = true;
                h.screen = GScreen::Catalogue;
                h.town.popup = {true, 2, 0, std::string(80, 'w'), "Title", -1, false, 56, 160, 6};
                CatalogueState& ca = h.catalogue;
                ca.valid = true;
                ca.shop = kCaStore;
                for (int i = 0; i < 12; i++) ca.items.push_back({100 + i + 0x3B, 100 + i, 563 + i, 922 + i, 1991, 200, 100});
                ca.selected = 0;
                ca.money = 1000;
                ca.downEnabled = true;
                const ScreenState on {Screen::Catalogue, Tab::None};
                const MenuView v = PopupViewOf(on, h);
                CHECK(v.valid && v.popup && v.items.size() == 1);
                CHECK(UnderScreen(on, h).screen == Screen::Catalogue);
                const MenuView u = CatalogueViewOf(on, h);
                CHECK(u.valid);
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                Piece world = ConsoleWorld(256, HudCropRows(192), true);
                world.touch = false;
                comp.pieces.push_back(world);
                BuildCatalogue(comp, u, -1, h, rom, nullptr);
                BuildPopup(comp, v, 0, h, rom);
                SortUi(comp.ui);
                int layer = 0;
                for (const ui::UiItem& it : comp.ui)
                {
                    if (it.kind == ui::UiItem::Image) continue;
                    CHECK(it.layer >= layer);
                    layer = it.layer;
                }
                CHECK(TouchFromComposition(comp, fit, kPopClose.x + kPopClose.w / 2.0f, kPopClose.y + kPopClose.h / 2.0f, tx, ty));
                CHECK(inside(tx, ty, {120, 104, 24, 24}));
                const Rect cell = u.items[0].rect;
                CHECK(!TouchFromComposition(comp, fit, cell.x + cell.w / 2.0f, cell.y + cell.h / 2.0f, tx, ty)); // the catalogue beneath
            }
            // A popup the game shows over Save slots, Give Gift or the phone whose HD view does not
            // read (a box without its place): no view at all, not the screen's (milestone 1's picture
            // holds the game's own popup). (This rom has no strings: the guard itself is PopupOver's.)
            for (ScreenState on : {ScreenState {Screen::SaveSlots, Tab::None}, ScreenState {Screen::Gift, Tab::None}, ScreenState {Screen::Phone, Tab::None}})
            {
                GameState h;
                h.valid = true;
                h.screen = on.screen == Screen::Gift ? GScreen::Drencher : GScreen::Phone;
                h.town.popup = {true, 2, 0, "Text", "Title", -1, false, 56, 0, 6}; // (boxW 0: no buttons)
                CHECK(PopupOver(on, h) && !PopupView(on, &h, &rom).valid);
                CHECK(!ConsoleView(on, &h, &rom).valid);
            }
            // The phone: every page as rows the game counts, the selection the game's, Up / Down its
            // DS keys (as many presses as rows between), A the keys then OK; B its X.
            auto phone = [&](int page, int row, int scroll, int rows) {
                GameState h;
                h.valid = true;
                h.screen = GScreen::Phone;
                PhoneState& p = h.town.phone;
                p.valid = true;
                p.page = page; p.row = row; p.scroll = scroll;
                p.month = 11; p.day = 5;
                for (int i = 0; i < 4; i++) p.menuIds[i] = 0x812 + i;
                for (int i = 0; i < rows && page == 2; i++) p.inbox.push_back({i, 0x907, 45, 368, i % 2 == 0, {}, {}});
                if (page == 3) { p.inbox.push_back({0, 0x907, 45, 368, true, {}, {}}); p.shownSlot = 0; p.textEnd = 10; }
                if (page == 7) { for (int i = 0; i < rows; i++) p.contactList.push_back(i); p.contacts = rows; p.contactsKnown = true; }
                if (page == 9 || page == 6 || page == 8) { p.textEnd = 5; p.caller = 2; p.reply = 0x81F; p.callMood = 1; p.callPlace = 3; }
                if (page == 5) p.maidCost = 2270;
                return h;
            };
            auto presses = [](const MenuAction& a, int key) {
                int n = 0;
                for (const MenuStep& st : a) n += st.kind == MenuStep::Keys && st.keys == (1u << key);
                return n;
            };
            const ScreenState ps {Screen::Phone, Tab::None};
            for (auto [page, row, scroll, rows] : std::vector<std::tuple<int, int, int, int>> {
                     {0, 0, 0, 0}, {0, 3, 0, 0}, {2, 1, 0, 3}, {2, 10, 7, 10}, {3, 0, 0, 0}, {3, 4, 3, 0}, {3, 12, 10, 0}, {4, 5, 2, 0}, {5, 1, 0, 0},
                     {7, 20, 18, 24}, {7, 0, 0, 0}, {9, 0, 0, 0}, {6, 5, 2, 0}, {8, 1, 0, 0}, {10, 0, 0, 0}})
            {
                const GameState h = phone(page, row, scroll, rows);
                const MenuView v = PhoneViewOf(ps, h, &rom);
                CHECK(v.valid && v.own && !v.popup);
                // B: back one level where the page ends on the game's Exit row (its highlight moved
                // there, then A), else the game's X; MENU hangs up from any page: the X.
                {
                    const PhonePage bp = PhonePageOf(h, rom);
                    const bool exitRow = page != 0 && page != 10 && bp.valid && !bp.crop && !bp.rows.empty() && bp.rows.back().text == MenuWords(rom, kStrPhExit);
                    if (exitRow) CHECK(!v.backCloses && v.back.back().kind == MenuStep::Keys && v.back.back().keys == (1u << kA));
                    else CHECK(v.back.size() == 1 && v.back[0].x == kPhoneClose.x && v.backCloses);
                }
                CHECK(v.menu.size() == 1 && v.menu[0].x == kPhoneClose.x && v.menu[0].y == kPhoneClose.y && v.menuCloses);
                if (page == 10) { CHECK(v.items.empty()); continue; }
                CHECK(MenuFocusable(v, v.selected));
                const ViewItem& sel = v.items[v.selected];
                CHECK(sel.action.back().kind == MenuStep::Keys && sel.action.back().keys == (1u << kA));
                const PhonePage pg = PhonePageOf(h, rom);
                CHECK(pg.valid && !pg.crop && !pg.rows.empty());
                // The selection's row: the game's (or, in a text, the first row after it).
                const int selRow = (int)(sel.id - kPhRowId);
                CHECK(selRow == row || (row < pg.rows.front().row && selRow == pg.rows.front().row));
                CHECK(presses(sel.action, kDown) == std::max(0, selRow - row) && presses(sel.action, kUp) == std::max(0, row - selRow));
                if (!v.scrollDown.empty()) CHECK(presses(v.scrollDown, kDown) >= 1 && presses(v.scrollDown, kUp) == 0);
                CHECK(v.scrollDown.empty() == (selRow == pg.rows.back().row));
                CHECK((int)v.items.size() <= kPhRows);
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                BuildPhone(comp, v, v.selected, h, rom, nullptr, face.data(), nullptr);
                for (const ui::UiItem& it : comp.ui)
                    CHECK(it.to.x >= kPhPanel.x && it.to.y >= kPhPanel.y && it.to.x + it.to.w <= kPhPanel.x + kPhPanel.w && it.to.y + it.to.h <= kPhPanel.y + kPhPanel.h);
                for (const ViewItem& it : v.items) CHECK(it.rect.y >= kPhList.y && it.rect.y + it.rect.h <= kPhList.y + kPhList.h);
                CHECK(TouchFromComposition(comp, fit, kPhOk.x + kPhOk.w / 2.0f, kPhOk.y + kPhOk.h / 2.0f, tx, ty) && inside(tx, ty, {kPhOkX - 12, kPhBtnY - 12, 24, 24}));
                CHECK(TouchFromComposition(comp, fit, kPhDown.x + kPhDown.w / 2.0f, kPhDown.y + kPhDown.h / 2.0f, tx, ty) && inside(tx, ty, {kPhDownX - 12, kPhBtnY - 12, 24, 24}));
                CHECK(TouchFromComposition(comp, fit, kPhX.x + kPhX.w / 2.0f, kPhX.y + kPhX.h / 2.0f, tx, ty) && inside(tx, ty, kPhoneClose.area));
            }
            // A list the game's own count disagrees with: its LCD, live, the same keys.
            GameState odd = phone(7, 2, 0, 5);
            odd.town.phone.contactsKnown = false;
            const MenuView ov = PhoneViewOf(ps, odd, &rom);
            CHECK(ov.valid && ov.items.size() == 1 && ov.selected == 0 && presses(ov.scrollDown, kDown) == 1 && presses(ov.scrollUp, kUp) == 1);
            CHECK(PhonePageOf(odd, rom).crop);
            odd.town.phone.contacts = 0; // (the list being built: nothing yet)
            CHECK(!PhonePageOf(odd, rom).crop && PhoneViewOf(ps, odd, &rom).items.empty());
            odd.town.phone.contactsWait = kPhContactsGrace + 1; // (never agreeing: after a few frames, its LCD)
            CHECK(PhonePageOf(odd, rom).crop && PhoneViewOf(ps, odd, &rom).items.size() == 1);
            CHECK(!PhoneViewOf({Screen::Panel, Tab::Interact}, phone(0, 0, 0, 0), &rom).valid);
            CHECK(!PhoneView(ps, nullptr, &rom).valid && !PhoneView(ps, &odd, &rom).valid); // (no strings)
            // A note: A and B press the DS's A (the game closes it so); its X onto the game's.
            GameState n;
            n.valid = true;
            n.town.note = {true, 0x4A8, true};
            const MenuView nv = NoteViewOf({Screen::Unknown, Tab::None}, n);
            CHECK(nv.valid && nv.own && nv.items.size() == 1 && nv.items[0].action[0].kind == MenuStep::Keys && nv.back == nv.items[0].action);
            CHECK(!NoteViewOf({Screen::Panel, Tab::Interact}, n).valid);
            Composition nc {kConsoleW, kConsoleH, {}, {}};
            BuildNote(nc, nv, 0, n, rom, nullptr);
            for (const ui::UiItem& it : nc.ui)
                CHECK(it.to.x >= kNotePanel.x - 6 && it.to.y >= kNotePanel.y - 6 && it.to.x + it.to.w <= kNotePanel.x + kNotePanel.w && it.to.y + it.to.h <= kNotePanel.y + kNotePanel.h);
            CHECK(TouchFromComposition(nc, fit, kNoteX.x + kNoteX.w / 2.0f, kNoteX.y + kNoteX.h / 2.0f, tx, ty) && inside(tx, ty, {kNoteCloseX - 14, kNoteCloseY - 14, 28, 28}));
            n.town.note.valid = false;
            CHECK(!NoteViewOf({Screen::Unknown, Tab::None}, n).valid);
        }
        // BuildConsole: without the game's strings, milestone 1's menu (the touch screen 3x).
        const HudState hud = TestHud();
        const HudStrings strings = TestStrings();
        const Composition m1 = BuildConsole(it, true, 0, 256, 192, &hud, &strings, &g, &rom, face.data());
        bool whole = false;
        for (const Piece& q : m1.pieces) whole |= q.src == Src::Bottom && q.from.w == SW && q.from.h == SH && q.to.w == SW * 3;
        CHECK(whole);
        // FitLine: shrinks, then cuts with an ellipsis, never wider than asked.
        const FittedText f1 = FitLine("Hotseat", 30, 22, 400);
        CHECK(f1.fits && f1.size == 30 && f1.text == "Hotseat");
        const FittedText f2 = FitLine(std::string(60, 'W'), 30, 22, 300);
        CHECK(!f2.fits && f2.size == 22 && ui::TextWidth(f2.text, 22) <= 300 && f2.text.size() > 3);
        const FittedText f3 = FitLine("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 30, 30, 40); // accents never split
        CHECK(!f3.fits && f3.text.size() % 2 == 1);
        // Mixed letters: only whole letters go, never more than needed (an ASCII letter cut alone,
        // not the accented one before it).
        const std::string mixed = "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9" "a\xC3\xA9";
        for (int w = 20; w < 140; w += 7)
        {
            const FittedText f = FitLine(mixed, 30, 30, w);
            if (f.fits) continue;
            const std::string kept = f.text.substr(0, f.text.size() - 3);
            CHECK(mixed.compare(0, kept.size(), kept) == 0);
            CHECK(kept.empty() || ((unsigned char)kept.back() & 0xC0) != 0xC0); // no letter cut in two
            // One more letter of the original would not fit.
            size_t next = kept.size();
            if (next < mixed.size()) { next++; while (next < mixed.size() && ((unsigned char)mixed[next] & 0xC0) == 0x80) next++; }
            CHECK(ui::TextWidth(mixed.substr(0, next) + "\xE2\x80\xA6", 30) > w || next == mixed.size());
        }
    }

    // The front end's boot, title and main menu (port/front.h) on made-up states: the title's one
    // item (A and Start its big button), the menu's four buttons (enabled and tapping their box
    // only at phase 3: drawn but dead while they fade in and after a choice), the boot's last
    // splash (A and Start its tap; the earlier splashes nothing, the popup splash no view); never
    // replayed; ids distinct and the module's; every item reachable; layers ascending; the mouse
    // round trips (the title's whole canvas onto the big button, a button onto its box, the
    // splash the touch screen itself); no view on another screen, without the RAM's part or the
    // strings.
    {
        RomData rom;
        auto front = [](int phase, int mode) {
            GameState g;
            g.valid = true;
            if (phase >= 0)
            {
                g.front.title.valid = true;
                g.front.title.phase = phase;
                g.front.title.menu = phase >= 2;
                g.front.kind = phase >= 2 ? FrontKind::MainMenu : FrontKind::Title;
            }
            if (mode >= 0)
            {
                g.front.boot.valid = true;
                g.front.boot.mode = mode;
                g.front.video.valid = true;
                g.front.video.movie = 3;
                g.front.kind = FrontKind::Video;
            }
            return g;
        };
        auto lands = [&](const Composition& comp, Rect hd, int x0, int y0, int w, int h) {
            for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
            {
                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, o.first, o.second);
                for (float fx : {0.1f, 0.5f, 0.9f})
                    for (float fy : {0.1f, 0.5f, 0.9f})
                    {
                        int tx, ty;
                        const float cx = hd.x + hd.w * fx, cy = hd.y + hd.h * fy;
                        CHECK(TouchFromComposition(comp, fit, fit.ox + cx * fit.scale, fit.oy + cy * fit.scale, tx, ty));
                        CHECK(tx >= x0 && tx < x0 + w && ty >= y0 && ty < y0 + h);
                    }
            }
        };
        auto layers = [&](const Composition& comp) {
            int layer = 0;
            for (const ui::UiItem& u : comp.ui)
            {
                CHECK(u.to.x >= 0 && u.to.y >= 0 && u.to.x + u.to.w <= kConsoleW && u.to.y + u.to.h <= kConsoleH);
                if (u.kind == ui::UiItem::Image) continue;
                CHECK(u.layer >= layer);
                layer = u.layer;
            }
        };
        const ScreenState fe {Screen::FrontEnd, Tab::None}, none {Screen::None, Tab::None};
        for (int phase = 0; phase <= 4; phase++)
        {
            const GameState g = front(phase, -1);
            const MenuView v = FrontViewOf(fe, g);
            CHECK(v.valid && v.own && !v.replay && AllReachable(v));
            CHECK(!FrontViewOf(none, g).valid && !FrontViewOf({Screen::Panel, Tab::Interact}, g).valid);
            CHECK(!FrontView(fe, &g, &rom).valid); // no strings: milestone 1's
            std::set<u32> ids;
            for (const ViewItem& it : v.items) { ids.insert(it.id); CHECK((it.id >> 24) == (kModFront >> 24)); }
            CHECK(ids.size() == v.items.size());
            for (int topW : {256, 341})
                for (int focus : {-1, 0, 3})
                {
                    Composition comp {kConsoleW, kConsoleH, {}, {}};
                    BuildFront(comp, v, focus, fe, g, rom, nullptr, topW, 192);
                    layers(comp);
                    for (const Piece& q : comp.pieces)
                        if (q.src == Src::Top) CHECK(!q.touch && q.from.w == 256 && q.from.x == (topW - 256) / 2); // (the 2D part)
                    if (focus != 0 || topW != 256) continue;
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(fe, false, 256, 192, o.first, o.second, false, &comp);
                    if (phase == 0) lands(comp, {0, 0, kConsoleW, kConsoleH}, kFrBigButtonFrom.x, kFrBigButtonFrom.y, kFrBigButtonFrom.w, kFrBigButtonFrom.h);
                    else
                        for (int i = 0; i < kFrMenuButtons; i++)
                        {
                            if (i == kFrWireless)
                            {
                                // DS Wireless (a second DS): the mouse takes it nowhere.
                                int tx, ty;
                                const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                                const Rect r = FrontButtonRect(i);
                                CHECK(!TouchFromComposition(comp, fit, fit.ox + (r.x + r.w / 2) * fit.scale, fit.oy + (r.y + r.h / 2) * fit.scale, tx, ty));
                                continue;
                            }
                            lands(comp, FrontButtonRect(i), kFrMenuBoxX, kFrMenuBoxY0 + i * kFrMenuBoxDy, kFrMenuBoxW, kFrMenuBoxH);
                        }
                }
            const MenuAction& start = v.buttons[(int)MenuButton::Start];
            if (phase == 0)
            {
                CHECK(v.items.size() == 1 && v.items[0].enabled && v.items[0].action.size() == 1);
                CHECK(v.items[0].action[0].x == kFrBigX && v.items[0].action[0].y == kFrBigY && start == v.items[0].action);
                CHECK(v.items[0].action[0].x >= kFrBigButtonFrom.x && v.items[0].action[0].x < kFrBigButtonFrom.x + kFrBigButtonFrom.w);
                continue;
            }
            CHECK((int)v.items.size() == kFrMenuButtons && start.empty());
            for (int i = 0; i < kFrMenuButtons; i++)
            {
                const ViewItem& it = v.items[i];
                // DS Wireless: focusable at phase 3 (its note), never tapped (a second DS).
                CHECK(it.enabled == (phase == 3) && it.action.empty() == (phase != 3 || i == kFrWireless));
                if (phase != 3 || i == kFrWireless) continue;
                CHECK(it.action.size() == 1 && it.action[0].kind == MenuStep::Tap);
                const int x = it.action[0].x, y = it.action[0].y, y0 = kFrMenuBoxY0 + i * kFrMenuBoxDy;
                CHECK(x >= kFrMenuBoxX && x < kFrMenuBoxX + kFrMenuBoxW && y >= y0 && y < y0 + kFrMenuBoxH);
                CHECK(MenuMove(v, i, 1) == std::min(i + 1, 3) && MenuMove(v, i, 0) == std::max(i - 1, 0));
            }
            if (phase != 3) CHECK(MenuFirst(v) < 0);
        }
        for (int mode = 0; mode <= 6; mode++)
            for (const ScreenState& at : {none, ScreenState {Screen::Loading, Tab::None}})
            {
                GameState g = front(-1, mode);
                const MenuView v = FrontViewOf(at, g);
                CHECK(v.valid == (mode != 4));
                if (!v.valid) continue;
                CHECK(v.own && !v.replay && v.items.size() == (mode == 0 ? 1u : 0u));
                // The splashes that take no touch pass the DS's own Start and A on; the last one takes them itself.
                CHECK(v.passKeys == (mode == 0 ? 0u : (1u << kStart) | (1u << kA)));
                if (mode == 0)
                {
                    CHECK(v.items[0].id == kFrBootId && v.items[0].action.size() == 1 && v.buttons[(int)MenuButton::Start] == v.items[0].action);
                    CHECK(v.items[0].action[0].x == kFrSplashX && v.items[0].action[0].y == kFrSplashY);
                    // (held past the splash's slowest poll, 6 frames: a 3-frame tap was lost)
                    CHECK(v.items[0].action[0].frames == kFrSplashHold && kFrSplashHold > 6);
                }
                Composition comp {kConsoleW, kConsoleH, {}, {}};
                BuildFront(comp, v, MenuFirst(v), at, g, rom, nullptr, 256, 192);
                layers(comp);
                int splash = 0;
                for (const Piece& q : comp.pieces) splash += q.src == Src::Bottom && q.touch && q.to.x == kFrSplash.x && q.to.w == kFrSplash.w;
                CHECK(splash == 1);
                // The splash's mouse piece lands anywhere in the splash on the DS's touch screen.
                if (mode == 0) lands(comp, kFrSplash, 0, 0, SW, SH);
                if (mode == 0 && at.screen == Screen::None)
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(none, false, 256, 192, o.first, o.second, false, &comp);
                g.front.boot.valid = false;
                CHECK(!FrontViewOf(at, g).valid);
            }
        // The boot's splash on the FrontEnd screen is the legal splash's (GScreen FrontEnd): its
        // view; the title's object on None is no view.
        CHECK(FrontViewOf(fe, front(-1, 0)).valid && !FrontViewOf(none, front(0, -1)).valid);
        GameState bad = front(3, -1);
        bad.valid = false;
        CHECK(!FrontViewOf(fe, bad).valid);
    }

    // Settings and Load-a-Sim (port/frontmenus.h) on made-up states: the sliders' Left / Right a
    // drag from the game's knob to the next tenth at every volume (none past an end; a volume
    // landed half a pixel off still moves a whole tenth), the concept grid (A only on the unlocked
    // pictures, a tap inside the game's cell; the first focus the first unlocked one), Credits (no
    // item); LB / RB the neighbouring tabs in a ring, B the X; Load's rows (enabled only when used
    // and no popup is up, A the tick, the first focus the game's current slot when used), the slot
    // time as the game's formats print it; ids distinct and the module's; every enabled item
    // reachable; inside the canvas, layers ascending; the mouse round trips onto the game's targets;
    // no view on another screen, without the RAM's part or the strings.
    {
        RomData rom;
        const ScreenState none {Screen::None, Tab::None}, fe {Screen::FrontEnd, Tab::None};
        std::vector<u32> bottom(SW * SH, 0xFF5961A2);
        auto layers = [&](const Composition& comp) {
            int layer = 0;
            for (const ui::UiItem& u : comp.ui)
            {
                CHECK(u.to.x >= 0 && u.to.y >= 0 && u.to.x + u.to.w <= kConsoleW && u.to.y + u.to.h <= kConsoleH);
                if (u.kind == ui::UiItem::Image) continue;
                CHECK(u.layer >= layer);
                layer = u.layer;
            }
        };
        auto settings = [](int page, int music, int sfx, u16 bits) {
            GameState g;
            g.valid = true;
            g.front.kind = FrontKind::Settings;
            SettingsState& st = g.front.settings;
            st.valid = true;
            st.page = page;
            st.music = music;
            st.sfx = sfx;
            st.conceptKnown = page == 2;
            st.conceptBits = bits;
            st.creditsKnown = page == 3;
            st.creditsPos = 0xBF000;
            st.creditsLine = 11;
            return g;
        };
        // The knob's drag: from the knob, the volume after it (32 a pixel) within a pixel of the tenth.
        for (int vol = 0; vol <= 4096; vol += 16)
            for (int dir : {-1, 1})
            {
                const int x = SettingsNotchX(vol, dir), k = (int)std::lround(vol * 10.0 / 4096);
                const bool near = std::fabs(vol * 10.0 / 4096 - k) < 0.15;
                const int want = dir < 0 ? (near ? k - 1 : (int)std::floor(vol * 10.0 / 4096)) : (near ? k + 1 : (int)std::ceil(vol * 10.0 / 4096));
                CHECK((x < 0) == (want < 0 || want > 10));
                if (x < 0) continue;
                const int after = std::clamp(vol + 32 * (x - SettingsKnobX(vol)), 0, 4096);
                CHECK(std::abs(after - 4096 * want / 10) <= 32);
                CHECK(x >= kSeTrackX0 - 4 && x <= kSeTrackX0 + kSeTrackW + 4);
            }
        CHECK(SettingsKnobX(4096) == 212 && SettingsKnobX(0) == 84);
        std::set<u32> ids;
        for (int page = 0; page < 4; page++)
            for (u16 bits : {(u16)0, (u16)0x8, (u16)0xFFF})
                for (int vol : {0, 1296, 3696, 4096})
                {
                    const GameState g = settings(page, vol, 4096 - vol, bits);
                    const MenuView v = SettingsViewOf(none, g);
                    CHECK(v.valid && v.own && !v.replay && v.backCloses && AllReachable(v));
                    CHECK(SettingsViewOf(fe, g).valid && !SettingsViewOf({Screen::Panel, Tab::Interact}, g).valid);
                    CHECK(!SettingsView(none, &g, &rom).valid); // no strings: milestone 1's
                    CHECK(v.back.size() == 1 && v.back[0].x == kSeCloseX && v.back[0].y == kSeCloseY);
                    const MenuAction& lb = v.buttons[(int)MenuButton::LB];
                    const MenuAction& rb = v.buttons[(int)MenuButton::RB];
                    CHECK(lb.size() == 1 && lb[0].y == kSeTabY0 + kSeTabDy * ((page + 3) % 4) && rb.size() == 1 && rb[0].y == kSeTabY0 + kSeTabDy * ((page + 1) % 4));
                    for (const ViewItem& it : v.items) { ids.insert(it.id); CHECK((it.id >> 24) == (kModFrontMenus >> 24)); }
                    if (page <= 1)
                    {
                        const int vv = page == 0 ? vol : 4096 - vol;
                        CHECK(v.items.size() == 1 && v.items[0].left.empty() == (vv == 0) && v.items[0].right.empty() == (vv == 4096));
                        for (const MenuAction* a : {&v.items[0].left, &v.items[0].right})
                            if (!a->empty()) CHECK(a->size() == 1 && (*a)[0].kind == MenuStep::Drag && (*a)[0].x == SettingsKnobX(vv) && (*a)[0].y == (page == 0 ? kSeMusicY : kSeSfxY));
                    }
                    else if (page == 2)
                    {
                        CHECK((int)v.items.size() == kSeCells);
                        int firstOn = -1;
                        for (int i = 0; i < kSeCells; i++)
                        {
                            const ViewItem& it = v.items[i];
                            const bool on = bits & (1u << i);
                            if (on && firstOn < 0) firstOn = i;
                            CHECK(it.enabled && it.action.empty() == !on);
                            if (on)
                            {
                                const int x = it.action[0].x, y = it.action[0].y, cx = kSeCellX0 + (i & 3) * kSeCellDx, cy = kSeCellY0 + (i >> 2) * kSeCellDy;
                                CHECK(x > cx + 4 && x < cx + kSeCellW - 4 && y > cy + 4 && y < cy + kSeCellH - 4);
                            }
                        }
                        CHECK(MenuFirst(v) == std::max(firstOn, 0));
                    }
                    else CHECK(v.items.empty());
                    for (int topW : {256, 341})
                        for (int focus : {-1, 0, 3})
                        {
                            Composition comp {kConsoleW, kConsoleH, {}, {}};
                            BuildSettings(comp, v, focus, g, rom, bottom.data(), nullptr, topW, 192);
                            layers(comp);
                            if (focus != 0 || topW != 256 || vol != 1296) continue;
                            for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                TestConsoleRoundtrip(none, false, 256, 192, o.first, o.second, false, &comp);
                            // Every tab's HD cell lands in the game's tab box, every cell in its picture.
                            const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                            for (int i = 0; i < kSeTabs; i++)
                            {
                                const Rect r = SettingsTabRect(i);
                                int tx, ty;
                                CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                CHECK(std::abs(tx - kSeTabX) <= 12 && std::abs(ty - (kSeTabY0 + kSeTabDy * i)) <= 12);
                            }
                            {   // The X onto the game's.
                                int tx, ty;
                                CHECK(TouchFromComposition(comp, fit, kSeX.x + kSeX.w / 2.0f, kSeX.y + kSeX.h / 2.0f, tx, ty));
                                CHECK(std::abs(tx - kSeCloseX) <= 14 && std::abs(ty - kSeCloseY) <= 14);
                            }
                            if (page == 2)
                                for (int i = 0; i < kSeCells; i++)
                                {
                                    const Rect r = SettingsCellRect(i);
                                    int tx, ty;
                                    CHECK(TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty));
                                    CHECK(std::abs(tx - (kSeCellX0 + (i & 3) * kSeCellDx + kSeCellW / 2)) <= 1 && std::abs(ty - (kSeCellY0 + (i >> 2) * kSeCellDy + kSeCellH / 2)) <= 1);
                                }
                        }
                }
        CHECK(ids.size() == 2 + kSeCells);
        {
            GameState g = settings(2, 4096, 4096, 1);
            g.front.settings.conceptKnown = false;
            CHECK(!SettingsViewOf(none, g).valid);
            g = settings(3, 4096, 4096, 0);
            g.front.settings.creditsKnown = false;
            CHECK(!SettingsViewOf(none, g).valid);
            g.front.settings.valid = false;
            CHECK(!SettingsViewOf(none, g).valid);
        }

        // The slot time in the game's formats (EN, FR, DE), midnight and noon in 12 hours.
        LoadRow t;
        t.timeKnown = true;
        t.month = 11; t.day = 21; t.hour12 = 12; t.minute = 52; t.pm = false;
        CHECK(SlotTimeText("%n/%j %g:%i%a", t, "am", "pm") == "11/21 12:52am");
        CHECK(SlotTimeText("%j/%n %H:%i", t, "am", "pm") == "21/11 00:52");
        t.hour12 = 1; t.minute = 3; t.pm = true; t.month = 2; t.day = 5;
        CHECK(SlotTimeText("%n/%j %g:%i%a", t, "am", "pm") == "2/5 1:03pm");
        CHECK(SlotTimeText("%j.%n %H:%i", t, "am", "pm") == "5.2 13:03");
        CHECK(SlotTimeText("%d/%m %G%A%q%%", t, "am", "pm") == "05/02 13PM%");
        t.timeKnown = false;
        CHECK(SlotTimeText("%n/%j", t, "am", "pm").empty());

        auto load = [](u8 used, int green, bool popup, bool wireless) {
            GameState g;
            g.valid = true;
            LoadScreen& l = g.front.load;
            l.valid = true;
            l.wireless = wireless;
            l.green = green;
            g.front.kind = wireless ? FrontKind::Wireless : FrontKind::Load;
            for (int i = 0; i < kLoadRows; i++)
            {
                LoadRow& r = l.rows[i];
                r.used = used & (1u << i);
                if (!r.used) continue;
                r.name = i == 1 ? "WWWWWWW" : "Chris";
                r.money = i == 2 ? 99999999 : 3226;
                r.place = 3; r.placeNameId = 70;
                r.timeKnown = true; r.month = 12; r.day = 28; r.hour12 = 12; r.minute = 58; r.pm = true;
            }
            if (popup)
            {
                g.town.popup.valid = true;
                g.town.popup.state = 3;
                g.town.popup.kind = 3;
            }
            return g;
        };
        std::set<u32> lids;
        for (int used = 0; used < 8; used++)
            for (int green : {-1, 0, 2})
                for (bool popup : {false, true})
                    for (bool wireless : {false, true})
                    {
                        const GameState g = load((u8)used, green, popup, wireless);
                        const MenuView v = LoadViewOf(none, g);
                        // DS Wireless keeps milestone 1's view (its tick opens the wireless lobby).
                        if (wireless) { CHECK(!v.valid && !LoadView(none, &g, &rom).valid); continue; }
                        CHECK(v.valid && v.own && !v.replay && AllReachable(v) && (int)v.items.size() == kLoadRows);
                        CHECK(!LoadViewOf({Screen::Panel, Tab::Interact}, g).valid && !LoadView(none, &g, &rom).valid);
                        CHECK(v.back.empty() == popup);
                        if (!popup) CHECK(v.back.size() == 1 && v.back[0].x == kLdCloseX && v.back[0].y == kLdCloseY && v.backCloses);
                        for (int i = 0; i < kLoadRows; i++)
                        {
                            const ViewItem& it = v.items[i];
                            lids.insert(it.id);
                            CHECK((it.id >> 24) == (kModFrontMenus >> 24));
                            CHECK(it.enabled == ((used & (1 << i)) && !popup));
                            CHECK(it.action.empty() != it.enabled);
                            if (it.enabled) CHECK(it.action.size() == 1 && it.action[0].x == kLdTickX && it.action[0].y == kLdTickY[i] && it.closes);
                        }
                        const int first = MenuFirst(v);
                        if (!popup && used)
                        {
                            const int want = green >= 0 && (used & (1 << green)) ? green : __builtin_ctz(used);
                            CHECK(first == want);
                        }
                        else CHECK(first < 0);
                        for (int topW : {256, 341})
                        {
                            Composition comp {kConsoleW, kConsoleH, {}, {}};
                            BuildLoad(comp, v, first, g, rom, nullptr, topW, 192);
                            layers(comp);
                            for (const Piece& q : comp.pieces)
                                if (q.src == Src::Top) CHECK(!q.touch);
                            if (topW != 256 || popup || !used) continue;
                            for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                                TestConsoleRoundtrip(none, false, 256, 192, o.first, o.second, false, &comp);
                            const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                            for (int i = 0; i < kLoadRows; i++)
                            {
                                const Rect r = LoadRowRect(i);
                                int tx, ty;
                                const bool hit = TouchFromComposition(comp, fit, r.x + r.w / 2.0f, r.y + r.h / 2.0f, tx, ty);
                                CHECK(hit == (bool)(used & (1 << i)));
                                if (hit) CHECK(std::abs(tx - kLdTickX) <= 12 && std::abs(ty - kLdTickY[i]) <= 12);
                            }
                            // The X onto the game's.
                            int tx, ty;
                            CHECK(TouchFromComposition(comp, fit, kLdX.x + kLdX.w / 2.0f, kLdX.y + kLdX.h / 2.0f, tx, ty));
                            CHECK(std::abs(tx - kLdCloseX) <= 14 && std::abs(ty - kLdCloseY) <= 14);
                        }
                    }
        CHECK(lids.size() == (size_t)kLoadRows);
        for (u32 id : lids) CHECK(!ids.count(id));
        // After the game's "Loading" popup (LoadScreen::loading): nothing to press, nothing but the
        // backdrop drawn (the list never flashes back before the town).
        {
            GameState g = load(7, 0, false, false);
            g.front.load.loading = true;
            const MenuView v = LoadViewOf(none, g);
            CHECK(v.valid && MenuFirst(v) < 0 && v.back.empty());
            Composition comp {kConsoleW, kConsoleH, {}, {}};
            BuildLoad(comp, v, -1, g, rom, nullptr, 256, 192);
            CHECK(comp.ui.empty() && comp.pieces.size() == 1);
        }
        GameState bad = load(7, 0, false, false);
        bad.front.load.valid = false;
        CHECK(!LoadViewOf(none, bad).valid);
    }

    // Create-a-Sim (port/cas.h) on made-up states, every page: the items' taps on the game's own
    // targets (inside the cell, key, button or arrow the game draws there), the keys' count per
    // keyboard page, none to press once the name holds seven and no backspace at empty, X the next
    // keyboard page, the hair grid's Left / Right the game's arrows only while it shows them, the
    // clothes' arrows focusable only then, the aspirations in the game's order, the tick and the
    // cross dead once one is pressed (B the cross), LB / RB the tabs in a ring, Start the next
    // page but on the last, the page in flight with nothing to press; ids distinct and the
    // module's; every enabled item reachable; inside the canvas, layers ascending; the mouse round
    // trips and lands each item on its game target; no view on another screen, on the in-game
    // pages, without the RAM's part or the strings.
    {
        RomData rom;
        const ScreenState cas {Screen::CreateASim, Tab::None};
        std::vector<u32> bottom(SW * SH, 0xFF5961A2);
        auto layers = [&](const Composition& comp) {
            int layer = 0;
            for (const ui::UiItem& u : comp.ui)
            {
                CHECK(u.to.x >= 0 && u.to.y >= 0 && u.to.x + u.to.w <= kConsoleW && u.to.y + u.to.h <= kConsoleH);
                if (u.kind == ui::UiItem::Image) continue;
                CHECK(u.layer >= layer);
                layer = u.layer;
            }
        };
        auto state = [](int page) {
            GameState g;
            g.valid = true;
            g.front.kind = page == 4 ? FrontKind::KeepSim : FrontKind::CreateASim;
            CasState& c = g.front.cas;
            c.valid = true;
            c.page = page;
            c.gender = 0;
            c.simName = "Sim";
            c.skin = 1;
            c.hairColour = 3;
            c.drawn = true;
            return g;
        };
        // The game's target box of an item (the picture or box its tap lies in), by its id.
        auto target = [](u32 id, int& x0, int& y0, int& w, int& h) {
            Rect r {};
            const u32 k = id & 0xFF;
            if (id >= kCasGenderId && id < kCasGenderId + 2) r = kCasGenderFrom[k];
            else if (id >= kCasSkinId && id < kCasSkinId + 4) r = {kCasSkinFrom.x + kCasSkinDx * (int)(k - 0x10), kCasSkinFrom.y, kCasSkinFrom.w, kCasSkinFrom.h};
            else if (id >= kCasKeyId && id < kCasKeyId + 26) r = {kCasKeyBoxFrom.x + kCasKeyDx * (int)(k % 9), kCasKeyBoxFrom.y + kCasKeyDy * (int)(k / 9), kCasKeyBoxFrom.w, kCasKeyBoxFrom.h};
            else if (id == kCasBackId) r = kCasBackFrom;
            else if (id >= kCasKbPageId && id < kCasKbPageId + 3) r = {kCasKbX - 12, kCasKbY[k - 0x30] - 12, 24, 24};
            else if (id >= kCasSwatchId && id < kCasSwatchId + 8) r = {kCasSwatchFrom.x + kCasSwatchDx * (int)k, kCasSwatchFrom.y, kCasSwatchFrom.w, kCasSwatchFrom.h};
            else if (id >= kCasHairId && id < kCasHairId + 4) r = {kCasHairFrom.x + kCasHairDx * (int)((k - 0x10) & 1), kCasHairFrom.y + kCasHairDy * (int)((k - 0x10) >> 1), kCasHairFrom.w, kCasHairFrom.h};
            else if (id >= kCasFaceId && id < kCasFaceId + 9) r = {kCasFaceFrom.x + kCasFaceDx * (int)(k % 3), kCasFaceFrom.y + kCasFaceDy * (int)(k / 3), kCasFaceFrom.w, kCasFaceFrom.h};
            else if (id >= kCasCatId && id < kCasCatId + 6) r = {kCasCatFrom.x, kCasCatFromY[k], kCasCatFrom.w, kCasCatFrom.h};
            else if (id >= kCasClothId && id < kCasClothId + 6) r = {kCasClothFrom.x + kCasClothDx * (int)((k - 0x10) % 3), kCasClothFrom.y + kCasClothDy * (int)((k - 0x10) / 3), kCasClothFrom.w, kCasClothFrom.h};
            else if (id == kCasClothId + 0x10) r = kCasClothLeftFrom;
            else if (id == kCasClothId + 0x11) r = kCasClothRightFrom;
            else if (id >= kCasAspId && id < kCasAspId + 4) r = {kCasAspFrom.x + kCasAspDx * (int)[&] { for (int j = 0; j < 4; j++) if (kCasAspOfSlot[j] == (int)k) return j; return 0; }(), kCasAspFrom.y, kCasAspFrom.w, kCasAspFrom.h};
            else if (id == kCasTickId || id == kCasCrossId) r = {(id == kCasTickId ? kCasTickX : kCasCrossX) - 16, kCasAnswerY - 16, 32, 32};
            else return false;
            x0 = r.x; y0 = r.y; w = r.w; h = r.h;
            return true;
        };
        std::set<u32> ids;
        auto check = [&](const GameState& g, const char* what) {
            const MenuView v = CasViewOf(cas, g);
            if (!v.valid || !v.own || v.replay || !v.stickTurns || !AllReachable(v)) { fprintf(stderr, "cas %s: view\n", what); failures++; return v; }
            CHECK(!CasView(cas, &g, &rom).valid);   // no strings: milestone 1's
            CHECK(!CasViewOf({Screen::FrontEnd, Tab::None}, g).valid && !CasViewOf({Screen::Panel, Tab::Interact}, g).valid);
            const int page = g.front.cas.page;
            std::set<u32> here;
            for (const ViewItem& it : v.items)
            {
                CHECK((it.id >> 24) == (kModCas >> 24));
                here.insert(it.id);
                ids.insert(it.id);
                CHECK(it.rect.x >= kCasPanel.x && it.rect.y >= kCasPanel.y && it.rect.x + it.rect.w <= kCasPanel.x + kCasPanel.w && it.rect.y + it.rect.h <= kCasPanel.y + kCasPanel.h);
                int x0, y0, w, h;
                CHECK(target(it.id, x0, y0, w, h));
                for (const MenuAction* a : {&it.action})
                    if (!a->empty())
                    {
                        CHECK(it.enabled && a->size() == 1 && (*a)[0].kind == MenuStep::Tap);
                        CHECK((*a)[0].x >= x0 && (*a)[0].x < x0 + w && (*a)[0].y >= y0 && (*a)[0].y < y0 + h);
                    }
            }
            CHECK(here.size() == v.items.size());
            // X / Y turn the model off the Name page (there the keyboard's page and the backspace).
            CHECK(v.xyTurns == (page != 0 && !g.front.cas.switching));
            if (page != 0) CHECK(v.buttons[(int)MenuButton::X].empty() && v.buttons[(int)MenuButton::Y].empty());
            if (g.front.cas.switching) CHECK(v.items.empty() && v.buttons[(int)MenuButton::LB].empty() && v.buttons[(int)MenuButton::Start].empty());
            else
            {
                const MenuAction& lb = v.buttons[(int)MenuButton::LB];
                const MenuAction& rb = v.buttons[(int)MenuButton::RB];
                const MenuAction& start = v.buttons[(int)MenuButton::Start];
                // The pages' ends (no ring: LB on the Name page never lands on Keep this Sim?).
                CHECK(page == 0 ? lb.empty() : lb.size() == 1 && lb[0].x == kCasTabX && lb[0].y == kCasTabY0 + kCasTabDy * (page - 1));
                CHECK(page == 4 ? rb.empty() : rb.size() == 1 && rb[0].x == kCasTabX && rb[0].y == kCasTabY0 + kCasTabDy * (page + 1));
                if (page < 4) CHECK(start == rb);
                // Keep this Sim?: one B lands on the cross; only B there (or A) discards; Start the tick.
                if (page == 4 && !v.back.empty()) CHECK(MenuFocusable(v, v.backFocus) && v.items[v.backFocus].id == kCasCrossId);
                if (page == 4) CHECK(v.back.empty() ? start.empty() : start.size() == 1 && start[0].x == kCasTickX && start[0].y == kCasAnswerY);
                if (page != 4) CHECK(v.backFocus == -1);
                // B: the previous page on Hair, Face and Clothes (LB's tap); on the Name page Keep this
                // Sim?'s tab, the focus landing on its cross there.
                if (page >= 1 && page <= 3) CHECK(v.back == lb && v.backLands == 0);
                if (page == 0) CHECK(v.back.size() == 1 && v.back[0].x == kCasTabX && v.back[0].y == kCasTabY0 + kCasTabDy * 4 && v.backLands == kCasCrossId);
            }
            for (int topW : {256, 341})
                for (int focus : {-1, MenuFirst(v), (int)v.items.size() - 1})
                {
                    Composition comp {kConsoleW, kConsoleH, {}, {}};
                    BuildCas(comp, v, focus, g, rom, nullptr, bottom.data(), topW, 192);
                    layers(comp);
                    int tops = 0;
                    for (const Piece& q : comp.pieces)
                        if (q.src == Src::Top) { tops++; CHECK(q.touch && q.from.w == topW && q.to.x + q.to.w <= kCasPanel.x); }
                    CHECK(tops == 1);
                    if (topW != 256 || focus != MenuFirst(v)) continue;
                    {   // The touch screen's pictures only once they are the page's (CasState::drawn).
                        GameState u = g;
                        u.front.cas.drawn = false;
                        Composition cu {kConsoleW, kConsoleH, {}, {}};
                        BuildCas(cu, v, focus, u, rom, nullptr, bottom.data(), topW, 192);
                        for (const ui::UiItem& it : cu.ui) CHECK(!(it.kind == ui::UiItem::Image && it.src == bottom.data()));
                    }
                    if (g.front.cas.switching)
                    {
                        for (const Piece& q : comp.pieces) CHECK(q.src != Src::Bottom);   // (nothing to click)
                        continue;
                    }
                    for (const auto& o : {std::pair<int, int> {1280, 720}, {2560, 1440}, {1000, 600}})
                        TestConsoleRoundtrip(cas, false, 256, 192, o.first, o.second, false, &comp);
                    // Each item with something to do: its HD rectangle onto the game's target.
                    const CanvasFit fit = CanvasFit::For(comp.cw, comp.ch, 1280, 720);
                    for (const ViewItem& it : v.items)
                    {
                        int x0, y0, w, h, tx, ty;
                        target(it.id, x0, y0, w, h);
                        const bool hit = TouchFromComposition(comp, fit, it.rect.x + it.rect.w / 2.0f, it.rect.y + it.rect.h / 2.0f, tx, ty);
                        CHECK(hit == !it.action.empty());
                        if (hit) CHECK(tx >= x0 && tx < x0 + w && ty >= y0 && ty < y0 + h);
                    }
                    // The tabs onto the game's.
                    for (int i = 0; i < 5 && !g.front.cas.switching; i++)
                    {
                        int tx, ty;
                        CHECK(TouchFromComposition(comp, fit, kCasTabsX + i * kCasTabCell + kCasTabCell / 2.0f, kCasTabBar.y + kCasTabBar.h / 2.0f, tx, ty));
                        CHECK(std::abs(tx - kCasTabX) <= 17 && std::abs(ty - (kCasTabY0 + kCasTabDy * i)) <= 15);
                    }
                }
            return v;
        };
        // Name: every keyboard page, an empty, short and full name.
        for (int kb = 0; kb < 3; kb++)
            for (const char* name : {"", "Sim", "SimABCD"})
                for (int gender : {0, 1})
                {
                    GameState g = state(0);
                    CasState& c = g.front.cas;
                    c.nameKnown = true;
                    c.name = name;
                    c.nameLength = (int)std::string(name).size();
                    c.keyboard = kb;
                    c.gender = gender;
                    const MenuView v = check(g, "name");
                    CHECK(v.items.size() == 2 + 4 + 3 + 26 + 1u);
                    int keys = 0, live = 0;
                    for (const ViewItem& it : v.items)
                        if (it.id >= kCasKeyId && it.id < kCasKeyId + 26) { keys += it.enabled; live += !it.action.empty(); }
                    const bool full = std::string(name).size() == 7;
                    CHECK(keys == kCasKeyCount[kb] && live == (full ? 0 : kCasKeyCount[kb]));
                    CHECK(v.items[MenuFirst(v)].id == kCasKeyId);
                    const MenuAction& y = v.buttons[(int)MenuButton::Y];
                    CHECK(y.empty() == (*name == 0) && y == v.items.back().action);
                    const MenuAction& x = v.buttons[(int)MenuButton::X];
                    CHECK(x.size() == 1 && x[0].x == kCasKbX && x[0].y == kCasKbY[(kb + 1) % 3]);
                    for (int p = 0; p < 3; p++) CHECK(v.items[6 + p].action.empty() == (p == kb));
                    // Down: the key under, else the next row's last key; the backspace only from the
                    // last row (123!'s middle row never lands on it).
                    const int base = 9, count = kCasKeyCount[kb], rows = (count + 8) / 9, bs = (int)v.items.size() - 1;
                    for (int i = 0; i < count; i++)
                    {
                        const int down = MenuMove(v, base + i, 1);
                        if (i / 9 + 1 < rows) CHECK(down == base + std::min(i + 9, count - 1));
                        else CHECK(down == bs);
                    }
                }
        // Hair: the arrows only where the game shows them; the first focus the game's style when in view.
        for (int scroll : {0, 3, 7})
            for (int pos : {0, 4, 13})
                for (int arrows = 0; arrows < 4; arrows++)
                {
                    GameState g = state(1);
                    CasState& c = g.front.cas;
                    c.hairKnown = true;
                    c.hairScroll = scroll;
                    c.hairPos = pos;
                    c.hairStyle = 2;
                    c.arrowsKnown = true;
                    c.canLeft = arrows & 1;
                    c.canRight = arrows & 2;
                    const MenuView v = check(g, "hair");
                    CHECK(v.items.size() == 12u);
                    for (int cell = 0; cell < 4; cell++)
                    {
                        const ViewItem& it = v.items[8 + cell];
                        CHECK(it.left.empty() == !((cell & 1) == 0 && c.canLeft) && it.right.empty() == !((cell & 1) == 1 && c.canRight));
                        if (!it.left.empty()) CHECK(it.left[0].x == kCasHairLeftX && it.left[0].y == kCasHairArrowY);
                        if (!it.right.empty()) CHECK(it.right[0].x == kCasHairRightX && it.right[0].y == kCasHairArrowY);
                    }
                    const int col = pos % 9 - scroll;
                    CHECK(MenuFirst(v) == (col >= 0 && col <= 1 ? 8 + (pos / 9) * 2 + col : 3));
                }
        // Face.
        for (int face = 0; face < 9; face++)
        {
            GameState g = state(2);
            g.front.cas.face = face;
            const MenuView v = check(g, "face");
            CHECK(v.items.size() == 9u && MenuFirst(v) == face);
        }
        // Clothes: the categories, the cells, the arrows only while shown.
        for (int cat = 0; cat < 6; cat++)
            for (int cell : {-1, 0, 5})
                for (int arrows = 0; arrows < 4; arrows++)
                {
                    GameState g = state(3);
                    CasState& c = g.front.cas;
                    c.clothesKnown = true;
                    c.category = cat;
                    c.clothesPage = arrows;
                    c.item = 3;
                    c.clothesCell = cell;
                    c.arrowsKnown = true;
                    c.canLeft = arrows & 1;
                    c.canRight = arrows & 2;
                    const MenuView v = check(g, "clothes");
                    CHECK(v.items.size() == 14u);
                    CHECK(v.items[12].enabled == c.canLeft && v.items[13].enabled == c.canRight);
                    for (int i = 0; i < 6; i++) CHECK(v.items[i].action.empty() == (i == cat));
                    CHECK(MenuFirst(v) == 6 + std::max(cell, 0));
                }
        // Keep this Sim?: the aspirations, the answer.
        for (int asp = 0; asp < 4; asp++)
            for (int answer = 0; answer < 3; answer++)
            {
                GameState g = state(4);
                CasState& c = g.front.cas;
                c.keepKnown = true;
                c.aspiration = asp;
                c.answer = answer;
                const MenuView v = CasViewOf(cas, g);
                CHECK(v.valid && v.items.size() == 6u);
                if (answer)
                {
                    for (const ViewItem& it : v.items) CHECK(!it.enabled);
                    CHECK(v.back.empty() && MenuFirst(v) < 0);
                    continue;
                }
                check(g, "keep");
                for (int j = 0; j < 4; j++) CHECK(v.items[j].id == kCasAspId + (u32)kCasAspOfSlot[j] && v.items[j].action.empty() == (kCasAspOfSlot[j] == asp));
                CHECK(v.items[MenuFirst(v)].id == kCasAspId + (u32)asp);
                CHECK(v.items[4].closes && v.items[5].closes && v.backCloses && v.back == v.items[5].action);
                CHECK(v.items[4].action[0].x == kCasTickX && v.items[5].action[0].x == kCasCrossX);
            }
        // A page change in flight; the in-game pages; nothing read.
        for (int page = 0; page < 5; page++)
        {
            GameState g = state(page);
            g.front.cas.switching = true;
            check(g, "switching");
        }
        for (int page = 5; page < 8; page++) CHECK(!CasViewOf(cas, state(page)).valid);
        {   // Clothes' cells: a sprite read hidden is an empty cell (nothing drawn); one that did not
            // read, or past the list, keeps the touch screen's picture (the gated crop)
            GameState g = state(3);
            CasState& c = g.front.cas;
            c.clothesKnown = true;
            c.category = 0; c.clothesPage = 0; c.item = 0;
            c.spriteCount = 15;
            for (int i = 0; i < 15; i++) c.sprites[i] = {true, true, 0, -1, -1, -1};
            c.sprites[9] = {true, false, 0, -1, -1, -1};   // cell 0: read, hidden
            c.sprites[10] = {};                           // cell 1: did not read
            const MenuView v = CasViewOf(cas, g);
            Composition comp {1280, 720, {}, {}};
            BuildCas(comp, v, -1, g, rom, nullptr, bottom.data(), 256, 192);
            auto crops = [&](int cell) {
                int n = 0;
                for (const ui::UiItem& it : comp.ui)
                    if (it.kind == ui::UiItem::Image && it.src == bottom.data() && it.from.x == kCasClothFrom.x + kCasClothDx * (cell % 3) && it.from.y == kCasClothFrom.y + kCasClothDy * (cell / 3)) n++;
                return n;
            };
            CHECK(crops(0) == 0 && crops(1) == 1 && crops(2) == 1);   // (no ROM here: a readable sprite falls back too)
        }
        // A page whose fields do not read yet (its object being filled): the page's tabs, nothing to press.
        for (int page = 0; page < 5; page++)
        {
            GameState g = state(page);
            CasState& c = g.front.cas;
            c.nameKnown = c.hairKnown = c.clothesKnown = c.keepKnown = false;
            c.face = -1;
            const MenuView v = CasViewOf(cas, g);
            CHECK(v.valid && v.items.empty());
            Composition comp {1280, 720, {}, {}};
            BuildCas(comp, v, -1, g, rom, nullptr, bottom.data(), 256, 192);
            CHECK(!comp.ui.empty());
        }
        {
            GameState g = state(0);
            g.front.cas.valid = false;
            CHECK(!CasViewOf(cas, g).valid);
            g = state(2);
            g.valid = false;
            CHECK(!CasViewOf(cas, g).valid);
        }
        CHECK(ids.size() == 2 + 4 + 26 + 1 + 3 + 8 + 4 + 9 + 6 + 6 + 2 + 4 + 2u);
    }


    // The world's prompt (port/prompt.h) on made-up Interact states: shown for a target with an
    // enabled action (the first one, the lit row while A is held), never for none, all greyed or
    // right after an action (pad mode 2); its hints; steady frames (a pad mode change keeps them, a
    // new target restarts them); only on the panel's Interact tab, settled, steady 6 frames; the
    // plate inside the canvas above row 650, every item inside it on its own layer, with the
    // longest words and every hint; under the shell's veil its layer is what the window drops.
    {
        auto act = [](int code, bool grey) { InteractAction a; a.code = code; a.labelId = 1845 + code; a.greyed = grey; return a; };
        InteractState in;
        in.valid = true;
        in.target.kind = TargetRef::Sim; in.target.index = 51; in.target.nameId = 374;
        in.actions = {act(43, false)};
        in.highlighted = 0;
        WorldPrompt p = WorldPromptOf(in, nullptr);
        CHECK(p.shown && !p.more && !p.others && !p.held && p.verb.empty());
        InteractState none = in; none.target = {}; none.actions.clear();
        CHECK(!WorldPromptOf(none, nullptr).shown);
        InteractState grey = in; grey.actions = {act(2, true), act(0x3A, true)};
        CHECK(!WorldPromptOf(grey, nullptr).shown);
        InteractState after = in; after.padMode = 2;
        CHECK(!WorldPromptOf(after, nullptr).shown);
        InteractState invalid = in; invalid.valid = false;
        CHECK(!WorldPromptOf(invalid, nullptr).shown);
        // A target the game names with nothing (the picker's third type): the plate, no name.
        InteractState unnamed = in; unnamed.target = {}; unnamed.target.kind = TargetRef::Unnamed; unnamed.target.index = 3;
        p = WorldPromptOf(unnamed, nullptr);
        CHECK(p.shown && p.name.empty());
        InteractState many = in; many.actions = {act(2, true), act(6, false), act(71, false)}; many.prev.kind = TargetRef::Object; many.prev.nameId = 492;
        p = WorldPromptOf(many, nullptr);
        CHECK(p.shown && p.more && p.others && !p.held);
        {
            // Left / Right switch targets with A held only (without it the D-pad walks): no hint before.
            Composition k {kConsoleW, kConsoleH, {}, {}};
            BuildWorldPrompt(k, p, nullptr);
            int chips = 0;
            for (const ui::UiItem& it : k.ui) chips += it.kind == ui::UiItem::Chip && it.button == ui::Button::DPadH;
            CHECK(chips == 0);
        }
        many.padMode = 0; many.highlighted = 2;
        p = WorldPromptOf(many, nullptr);
        CHECK(p.shown && p.held && p.more);
        InteractState one = many; one.actions = {act(2, true), act(6, false)};
        CHECK(!WorldPromptOf(one, nullptr).more);

        PromptSteady st;
        CHECK(st.Update(nullptr) == 0);
        for (int i = 1; i <= 7; i++) CHECK(st.Update(&in) == i);
        InteractState held = in; held.padMode = 0;
        CHECK(st.Update(&held) == 8);
        CHECK(st.Update(&many) == 1);
        CHECK(st.Update(&invalid) == 0 && st.Update(&in) == 1);
        CHECK(st.Update(&none) == 0 && !st.had && st.Update(&none) == 0);

        GameState g;
        g.valid = true; g.screen = GScreen::Panel; g.tab = 0; g.settled = true; g.town.interact = in;
        const ScreenState town {Screen::Panel, Tab::Interact};
        PromptSteady five, six, lots;
        for (int i = 0; i < kPromptSteadyFrames - 1; i++) five.Update(&in);
        for (int i = 0; i < kPromptSteadyFrames; i++) six.Update(&in);
        for (int i = 0; i < 100; i++) lots.Update(&in);
        CHECK(PromptUp(town, g, six) && !PromptUp(town, g, five) && !PromptUp(town, g, PromptSteady {}));
        CHECK(!PromptUp({Screen::Panel, Tab::Pockets}, g, lots));
        CHECK(!PromptUp({Screen::Popup, Tab::Interact}, g, lots));
        GameState h = g; h.settled = false; CHECK(!PromptUp(town, h, lots));
        h = g; h.screen = GScreen::Popup; CHECK(!PromptUp(town, h, lots));
        h = g; h.tab = 1; CHECK(!PromptUp(town, h, lots));
        h = g; h.town.interact.valid = false; CHECK(!PromptUp(town, h, lots));
        h = g; h.screen = GScreen::Dialogue; CHECK(!PromptUp({Screen::Dialogue, Tab::None}, h, lots));
        // The counter a frame behind the drawn state (Steer runs before the frame's RAM is read): a
        // state loaded with another target, or the target just switched, never flashes the old
        // one's words, nor the new one's before it is steady itself.
        h = g; h.town.interact = many; CHECK(!PromptUp(town, h, lots));
        h = g; h.town.interact.actions[0].greyed = true; CHECK(!PromptUp(town, h, lots));
        {
            Composition k {kConsoleW, kConsoleH, {}, {}};
            RomData norom;
            AddWorldPrompt(k, town, h, norom, lots);
            CHECK(k.ui.empty());
            AddWorldPrompt(k, town, g, norom, lots);
            CHECK(!k.ui.empty());
            DropWorldPrompt(k);
            CHECK(k.ui.empty());
        }

        Composition c {kConsoleW, kConsoleH, {}, {}};
        BuildWorldPrompt(c, WorldPromptOf(none, nullptr), nullptr);
        CHECK(c.ui.empty());
        const std::string longVerb = "Wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww (§99,999,999)", longName = "Wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww";
        for (const auto& [verb, name, more, others, held2] : {std::tuple<std::string, std::string, bool, bool, bool> {"Buy", "Jun", false, false, false},
                                                               {longVerb, longName, true, true, false}, {longVerb, "", true, false, true},
                                                               {"Talk", longName, false, true, true}})
        {
            WorldPrompt w;
            w.shown = true; w.verb = verb; w.name = name; w.more = more; w.others = others; w.held = held2;
            Composition k {kConsoleW, kConsoleH, {}, {}};
            BuildWorldPrompt(k, w, nullptr);
            const Rect r = WorldPromptRect(w, nullptr);
            CHECK(!k.ui.empty() && r.w > 0 && r.x >= 16 && r.x + r.w <= kConsoleW - 16 && r.y >= 0 && r.y + r.h + kPromptShadow < 650);
            CHECK(r.x + r.w / 2 >= 639 && r.x + r.w / 2 <= 641);
            int chips = 0;
            for (const ui::UiItem& it : k.ui)
            {
                CHECK(it.layer == kPromptLayer);
                CHECK(it.to.x >= r.x && it.to.y >= r.y && it.to.x + it.to.w <= r.x + r.w && it.to.y + it.to.h <= r.y + r.h);
                chips += it.kind == ui::UiItem::Chip;
            }
            CHECK(chips == 1 + (more ? 1 : 0) + (others && held2 ? 1 : 0));
        }
    }

    {
        // The panel's D-pad hints (ux task 6): "Choose" whenever Up / Down move the focus (the
        // vertical arms for a column, the cross for a grid), a slider's "Adjust", Interact's targets.
        RomData rom;
        auto item = [](Rect r, u32 id) { ViewItem it; it.rect = r; it.id = id; return it; };
        MenuView col;
        col.items = {item({200, 200, 400, 60}, 10), item({200, 280, 400, 60}, 11), item(kPmPhoneWell, 1)};
        std::vector<DPadHint> h = PanelDPadHints(col, 0, rom);
        CHECK(h.size() == 1 && h[0].button == ui::Button::DPadV && h[0].word == OwnWord(&rom, OwnWordId::HintChoose));
        MenuView grid = col;
        grid.items[1].rect.x = 640;
        h = PanelDPadHints(grid, 0, rom);
        CHECK(h.size() == 1 && h[0].button == ui::Button::DPad);
        MenuView slider = grid;
        slider.items[1].left = {TapStep(10, 10)};
        h = PanelDPadHints(slider, 1, rom);
        CHECK(h.size() == 2 && h[0].button == ui::Button::DPadV && h[1].button == ui::Button::DPadH && h[1].word == OwnWord(&rom, OwnWordId::HintAdjust));
        MenuView targets = col;
        targets.buttons[(int)MenuButton::Left] = {TapStep(10, 10)};
        h = PanelDPadHints(targets, 0, rom);
        CHECK(h.size() == 2 && h[0].button == ui::Button::DPadV && h[1].word == OwnWord(&rom, OwnWordId::PromptOther));
        MenuView one;
        one.items = {item({200, 200, 400, 60}, 10), item(kPmPhoneWell, 1)};
        CHECK(PanelDPadHints(one, 0, rom).empty()); // (one item: the D-pad moves nothing but to the phone)
        // Every scrolling list's column: two kit arrows (sunk where the list cannot go) and the bar.
        Composition c {kConsoleW, kConsoleH, {}, {}};
        Rect up, down;
        AddScrollColumn(c, {740, 140, 48, 422}, 0, 3, 7, false, true, &up, &down);
        int arrows = 0;
        for (const ui::UiItem& it : c.ui) arrows += it.sign == ui::UiItem::Sign::Up || it.sign == ui::UiItem::Sign::Down;
        CHECK(arrows == 2 && c.ui.size() == 4 && up.y == 140 && down.y + down.h == 562 && up.w == 48);
    }

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("layout_test: ok\n");
    return 0;
}
