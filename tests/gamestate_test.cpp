// Checks the screen manager reader (gamestate.*) on a synthetic main RAM: the pointer chain
// is written by hand with the values of the RAM map, no game data and no melonDS NDS.
//   ninja -C build gamestate_test && ./build/gamestate_test
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "gamestate.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

constexpr u32 kBase = 0x02000000, kMask = 0x3FFFFF;
constexpr u32 kP = 0x0219BE3C, kS = 0x0219D000;

struct Ram
{
    std::vector<u8> b = std::vector<u8>(kMask + 1, 0);
    void W8(u32 a, u32 v) { b[a - kBase] = (u8)v; }
    void W16(u32 a, u32 v) { W8(a, v); W8(a + 1, v >> 8); }
    void W32(u32 a, u32 v) { W16(a, v); W16(a + 2, v >> 16); }
    RamView View() const { return RamView{b.data(), kMask}; }
};

// The chain: P, S, vtable, overlay (-1 = none), popup, slot state.
static Ram Make(u32 vt, int ov, u32 popup = 0, u32 slot = 3)
{
    Ram r;
    r.W32(0x02139FA4, kP);
    r.W32(kP + 0xDAC, kS);
    r.W32(kS, vt);
    r.W32(kP + 0xE34, (u32)ov);
    r.W8(kP + 0x8, popup);
    r.W8(kP + 0xE42, slot);
    return r;
}

static void panel_and_popup()
{
    Ram r = Make(0x0213203C, -1, 0);
    r.W32(kS + 0x64, 2);
    GameState g = ReadGameState(r.View());
    CHECK(g.valid && g.screen == GScreen::Panel && g.tab == 2 && g.settled);
    CHECK(g.vtable == 0x0213203C && g.overlay == -1);
    CHECK(GameStateName(g) == "Panel/Goals");
    for (u32 p : {1u, 2u, 3u})
    {
        r.W8(kP + 0x8, p);
        g = ReadGameState(r.View());
        CHECK(g.screen == GScreen::Popup && g.tab == 2 && g.popup == p);
        CHECK(GameStateName(g) == "Popup");
    }
    r.W8(kP + 0x8, 0);
    r.W32(kS + 0x64, 1);
    r.W32(kS + 0x38, 0x0219E000);
    r.W32(0x0219E000 + 0xE0, 2);
    CHECK(ReadGameState(r.View()).selection == 2);
    r.W32(0x0219E000 + 0xE0, (u32)-1);
    CHECK(ReadGameState(r.View()).selection == -1);
    r.W32(kS + 0x64, 3);
    r.W16(kS + 0xD0, 0x204);
    g = ReadGameState(r.View());
    CHECK(g.tab == 3 && g.selection == 0x204 && GameStateName(g) == "Panel/Relationships");
}

// The black loads: the manager reads, its bottom object pointer is null.
static void between_screens()
{
    Ram r = Make(0x0213203C, -1);
    r.W32(kP + 0xDAC, 0);
    const GameState g = ReadGameState(r.View());
    CHECK(!g.valid && g.between && g.screen == GScreen::Unknown);
    CHECK(GameStateName(g) == "Unknown(ov=-1,vt=0x00000000)");
    CHECK(!ReadGameState(Make(0x0213203C, -1).View()).between);
    Ram none;
    CHECK(!ReadGameState(none.View()).between);
}

static void settled_only_for_3()
{
    for (u32 s : {0u, 3u, 4u, 5u})
        CHECK(ReadGameState(Make(0x0213203C, -1, 0, s).View()).settled == (s == 3));
    // A popup opening (1) or closing (3) is not settled: the game reads Popup about 13 frames
    // before the box is drawn, and a tap then lands on the panel underneath.
    for (u32 p : {0u, 1u, 2u, 3u, 0xFFu})
        CHECK(ReadGameState(Make(0x0213203C, -1, p, 3).View()).settled == (p == 0 || p == 2 || p == 0xFF));
    CHECK(!ReadGameState(Make(0x0213203C, -1, 2, 0).View()).settled);
}

static void plain_screens()
{
    Ram r = Make(0x02131F08, -1);
    r.W8(kS + 0x89, 3);
    r.W8(kS + 0x88, 2);
    GameState g = ReadGameState(r.View());
    CHECK(g.screen == GScreen::Phone && g.selection == 3 && g.page == 2);

    r = Make(0x0212F98C, -1);
    r.W32(kS + 0x2E0, (u32)-1);
    g = ReadGameState(r.View());
    CHECK(g.screen == GScreen::Cards && g.selection == -1);
    r.W32(kS + 0x2E0, 7);
    CHECK(ReadGameState(r.View()).selection == 7);

    CHECK(ReadGameState(Make(0x02135004, -1).View()).screen == GScreen::Social);
    g = ReadGameState(Make(0x02131960, -1, 0xFF).View());
    CHECK(g.screen == GScreen::FrontEnd && g.popup == 0xFF);
}

static void overlay_screens()
{
    Ram r = Make(0x0214AA18, 15);
    r.W16(kS + 0x36, 3);
    r.W16(kS + 0x34, 1);
    GameState g = ReadGameState(r.View());
    CHECK(g.screen == GScreen::Dialogue && g.page == 3 && g.speech == 1);

    r = Make(0x0214C194, 2);
    r.W32(kS + 0x254, 4);
    r.W32(kS + 0x268, 3);
    r.W32(kS + 0x258, 2);
    g = ReadGameState(r.View());
    CHECK(g.screen == GScreen::Catalogue && g.selection == 4 && g.scroll == 3 && g.list == 2);

    struct { u32 vt; int ov; GScreen s; } t[] = {
        {0x0214ABA4, 6, GScreen::Gift}, {0x0214B1D0, 1, GScreen::Build}, {0x0214C658, 17, GScreen::Vacuum},
        {0x021509B0, 3, GScreen::CreateASim}, {0x0214B1C4, 8, GScreen::SaveSlots}, {0x0214B8BC, 18, GScreen::ItemHand},
    };
    for (auto& c : t)
    {
        CHECK(ReadGameState(Make(c.vt, c.ov).View()).screen == c.s);
        CHECK(ReadGameState(Make(c.vt, c.ov + 1).View()).screen == GScreen::Unknown);
    }
    // Overlay vtables collide between overlays: the wrong overlay is not Dialogue.
    g = ReadGameState(Make(0x0214AA18, 2).View());
    CHECK(g.valid && g.screen == GScreen::Unknown && GameStateName(g) == "Unknown(ov=2,vt=0x0214AA18)");
    g = ReadGameState(Make(0x0214B1C4, 8 + 1).View());
    CHECK(GameStateName(g) == "Unknown(ov=9,vt=0x0214B1C4)");
}

static void invalid_chains()
{
    std::vector<u8> zero(kMask + 1, 0);
    GameState g = ReadGameState(RamView{zero.data(), kMask});
    CHECK(!g.valid && g.screen == GScreen::Unknown);

    Ram r = Make(0x0213203C, -1);
    r.W32(0x02139FA4, 0x08000000);               // P outside RAM
    CHECK(!ReadGameState(r.View()).valid);
    r.W32(0x02139FA4, 0x02400000 - 2);           // P at the very end: P+0xDAC out
    CHECK(!ReadGameState(r.View()).valid);
    r = Make(0x0213203C, -1);
    r.W32(kP + 0xDAC, 0);                        // S = 0
    CHECK(!ReadGameState(r.View()).valid);
    r.W32(kP + 0xDAC, 0x08000000);               // S outside RAM
    CHECK(!ReadGameState(r.View()).valid);
    r.W32(kP + 0xDAC, 0x023FFFFE);               // S straddles the end
    CHECK(!ReadGameState(r.View()).valid);
    CHECK(!ReadGameState(RamView{nullptr, kMask}).valid);

    // Fields past the end read as -1, not a crash.
    r = Make(0x0214C194, 2);
    r.W32(kP + 0xDAC, 0x023FFFF0);
    r.W32(0x023FFFF0, 0x0214C194);
    g = ReadGameState(r.View());
    CHECK(g.valid && g.screen == GScreen::Catalogue && g.selection == -1 && g.scroll == -1);
}

static void random_ram()
{
    std::mt19937 rng(12345);
    Ram r;
    for (int i = 0; i < 1000; i++)
    {
        for (size_t k = 0; k + 4 <= r.b.size(); k += 4) { u32 v = rng(); memcpy(&r.b[k], &v, 4); }
        // Half of them reach deep into the chain.
        if (i & 1) r.W32(0x02139FA4, kBase + (rng() & kMask));
        GameState g = ReadGameState(r.View());
        (void)GameStateName(g);
        CHECK(g.screen == GScreen::Unknown || g.valid);
        CHECK(g.tab >= -1 && g.tab <= 4);
    }
}

static void to_screen_state()
{
    // CONSOLE's screen: the panel with its tab, the rest by name; a tab out of 0..4, an
    // unmapped screen or no reading: Unknown.
    GameState g;
    CHECK(ToScreenState(g) == (ScreenState{Screen::Unknown}));
    g.valid = true;
    g.screen = GScreen::Panel;
    for (int t = 0; t < 5; t++)
    {
        g.tab = t;
        CHECK(ToScreenState(g) == (ScreenState{Screen::Panel, (Tab)((int)Tab::Interact + t)}));
    }
    g.tab = -1;
    CHECK(ToScreenState(g) == (ScreenState{Screen::Unknown}));
    g.tab = 2;
    CHECK(ScreenStateName(ToScreenState(g)) == "Panel/Goals");
    const std::pair<GScreen, Screen> same[] = {
        {GScreen::Popup, Screen::Popup}, {GScreen::Phone, Screen::Phone}, {GScreen::Dialogue, Screen::Dialogue},
        {GScreen::Gift, Screen::Gift}, {GScreen::Catalogue, Screen::Catalogue}, {GScreen::Build, Screen::Build},
        {GScreen::Vacuum, Screen::Vacuum}, {GScreen::Social, Screen::Social}, {GScreen::Cards, Screen::Cards},
        {GScreen::FrontEnd, Screen::FrontEnd}, {GScreen::CreateASim, Screen::CreateASim},
        {GScreen::SaveSlots, Screen::SaveSlots}, {GScreen::ItemHand, Screen::ItemHand}, {GScreen::Unknown, Screen::Unknown}};
    for (auto [gs, sc] : same)
    {
        g.screen = gs;
        CHECK(ToScreenState(g) == (ScreenState{sc}));
        CHECK(std::string(ScreenName(sc)) == GScreenName(gs));
    }
    // The pixels' answers that go with the game's screens.
    g.screen = GScreen::FrontEnd;
    CHECK(PixelsAgree(g, {Screen::None}) && !PixelsAgree(g, {Screen::Unknown}));
    g.screen = GScreen::SaveSlots;
    CHECK(PixelsAgree(g, {Screen::Unknown}) && !PixelsAgree(g, {Screen::Panel, Tab::Options}));
    g.screen = GScreen::Phone;
    CHECK(PixelsAgree(g, {Screen::Phone}) && !PixelsAgree(g, {Screen::Unknown}));
    // Options > Quit: a popup over the Options tab, Unknown for CONSOLE (its buttons are not the
    // popup's X); a popup over any other tab stays Popup.
    g.screen = GScreen::Popup;
    g.tab = 4;
    CHECK(ToScreenState(g) == (ScreenState{Screen::Unknown}));
    CHECK(PixelsAgree(g, {Screen::Unknown}) && PixelsAgree(g, {Screen::Popup}));
    for (int t : {-1, 0, 1, 2, 3})
    {
        g.tab = t;
        CHECK(ToScreenState(g) == (ScreenState{Screen::Popup}));
    }
}

static void console_screen()
{
    // RAM when it reads; an Unknown without the HUD is Loading when both screens are black (the
    // black loads), None otherwise (an unmapped front-end screen); without RAM the pixels.
    GameState g;
    CHECK(ConsoleScreenOf(g, {Screen::Phone}, true, false) == (ScreenState{Screen::Phone}));
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, true) == (ScreenState{Screen::None}));
    // Between screens (the manager reads, no bottom object): Loading when black without the HUD.
    g.between = true;
    CHECK(ConsoleScreenOf(g, {Screen::Cards}, false, true) == (ScreenState{Screen::Loading}));
    CHECK(ConsoleScreenOf(g, {Screen::Cards}, false, false) == (ScreenState{Screen::Cards}));
    CHECK(ConsoleScreenOf(g, {Screen::Panel, Tab::Goals}, true, true) == (ScreenState{Screen::Panel, Tab::Goals}));
    g.between = false;
    g.valid = true;
    CHECK(ConsoleScreenOf(g, {Screen::Cards}, false, false) == (ScreenState{Screen::None}));
    CHECK(ConsoleScreenOf(g, {Screen::Cards}, false, true) == (ScreenState{Screen::Loading}));
    CHECK(ConsoleScreenOf(g, {Screen::None}, true, false) == (ScreenState{Screen::Unknown}));
    g.screen = GScreen::Panel;
    g.tab = -1;
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, false) == (ScreenState{Screen::None}));
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, true) == (ScreenState{Screen::Loading}));
    g.tab = 1;
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, false) == (ScreenState{Screen::Panel, Tab::Pockets}));
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, true) == (ScreenState{Screen::Panel, Tab::Pockets}));
    g.screen = GScreen::Cards;
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, false) == (ScreenState{Screen::Cards}));
    g.screen = GScreen::FrontEnd;
    CHECK(ConsoleScreenOf(g, {Screen::None}, false, true) == (ScreenState{Screen::FrontEnd}));
}

constexpr u32 kG = 0x021670A4, kTab = 0x0211F16C, kPages = 0x02200000;

static void put_hud(Ram& r)
{
    r.W32(0x0213E434, kG);
    r.W32(kG + 0x2F60, 14843);
    r.W32(kG + 0x4800, 15);
    r.W32(kG + 0x4804, 37);
    r.W32(kG + 0x2164, 50u << 12);
    r.W32(kG + 0x2168, 225u << 12);
    r.W32(kG + 0x216C, 2);
    const char* n = "Jun";
    for (int i = 0; i < 4; i++) r.W8(kG + 0x53 + i, n[i]);
    r.W8(kG + 0x53 + 4, 'x');   // junk after the NUL
}

static void hud()
{
    Ram r = Make(0x0213203C, -1);
    CHECK(!ReadGameState(r.View()).hud.valid);   // no G block: zeros (max 0)
    put_hud(r);
    HudState h = ReadGameState(r.View()).hud;
    CHECK(h.valid && h.money == 14843 && h.hour == 15 && h.minute == 37);
    CHECK(h.sanity == 50 && h.sanityMax == 225 && h.tier == 2 && h.simName == "Jun");
    r.W32(kG + 0x2F60, (u32)-120);
    CHECK(ReadGameState(r.View()).hud.money == -120);
    // garbage: each field out of range invalidates
    auto bad = [&](u32 off, u32 v) { Ram q = r; q.W32(kG + off, v); return !ReadGameState(q.View()).hud.valid; };
    CHECK(bad(0x4800, 24) && bad(0x4804, 60) && bad(0x216C, 4) && bad(0x2168, 0) && bad(0x2164, 226u << 12));
    { Ram q = r; q.W8(kG + 0x53, 0x01); CHECK(!ReadGameState(q.View()).hud.valid); }
    { Ram q = r; for (int i = 0; i < 9; i++) q.W8(kG + 0x53 + i, 'A'); CHECK(!ReadGameState(q.View()).hud.valid); }   // no NUL in 8
    { Ram q = r; q.W32(0x0213E434, 0xFFFFFF00); CHECK(!ReadGameState(q.View()).hud.valid); }
    { Ram q = r; q.W32(0x0213E434, 0); CHECK(!ReadGameState(q.View()).hud.valid); }
    // screen identity ignores the hud
    GameState a = ReadGameState(r.View()), b = a;
    b.hud.minute = 38;
    CHECK(a == b);
}

// A speech table with speech 2 = 3 pages {1183 spk 63, 1184 spk 22, 1185 spk 0x40}; Sim record 22 -> name id 345.
static Ram dialogue_ram()
{
    Ram r = Make(0x0214AA18, 15);
    put_hud(r);
    r.W32(kTab + 8 * 2, kPages);
    r.W8(kTab + 8 * 2 + 4, 3);
    const u32 ids[] = {1183, 1184, 1185}, spk[] = {0x3F, 22, 0x40};
    for (int i = 0; i < 3; i++) { r.W16(kPages + 4 * i, ids[i]); r.W8(kPages + 4 * i + 2, spk[i]); }
    r.W16(kG + 0x21DC + 0x2C * 22 + 0x18, 22);
    r.W16(kS + 0x34, 2);
    r.W16(kS + 0x36, 1);
    return r;
}

static void dialogue()
{
    Ram r = dialogue_ram();
    DialogueState d = ReadGameState(r.View()).dialogue;
    CHECK(d.valid && d.speech == 2 && d.page == 1 && d.pages == 3 && d.stringId == 1183 && d.speaker == 0x3F);
    CHECK(d.speakerNameId == -1 && !d.last);
    r.W16(kS + 0x36, 2);
    d = ReadGameState(r.View()).dialogue;
    CHECK(d.valid && d.page == 2 && d.stringId == 1184 && d.speaker == 22 && d.speakerNameId == 345 && !d.last);
    r.W16(kS + 0x36, 3);
    d = ReadGameState(r.View()).dialogue;
    CHECK(d.valid && d.stringId == 1185 && d.speaker == 0x40 && d.speakerNameId == -1 && d.last);
    // not read outside the Dialogue screen
    { Ram q = Make(0x0213203C, -1); put_hud(q); CHECK(!ReadGameState(q.View()).dialogue.valid); }
    // garbage
    auto bad = [&](auto f) { Ram q = r; f(q); return !ReadGameState(q.View()).dialogue.valid; };
    CHECK(bad([](Ram& q) { q.W16(kS + 0x36, 0); }));                 // nothing drawn yet
    CHECK(bad([](Ram& q) { q.W16(kS + 0x36, 4); }));                 // past the count
    CHECK(bad([](Ram& q) { q.W16(kS + 0x34, 216); }));               // speech index out of the table
    CHECK(bad([](Ram& q) { q.W8(kTab + 8 * 2 + 4, 17); }));          // count over 16
    CHECK(bad([](Ram& q) { q.W8(kTab + 8 * 2 + 4, 0); }));
    CHECK(bad([](Ram& q) { q.W32(kTab + 8 * 2, 0x10000000); }));     // pages pointer outside RAM
    CHECK(bad([](Ram& q) { q.W32(kTab + 8 * 2, 0); }));              // null (speech 67 style)
    CHECK(bad([](Ram& q) { q.W8(kPages + 4 * 2 + 2, 46); }));        // speaker not a Sim record, none or player
    CHECK(bad([](Ram& q) { q.W16(kS + 0x36, 2); q.W32(0x0213E434, 0); }));   // no G for a named speaker
    // one-off mode: S+0x34 is the string id, speaker S+0x3C
    Ram o = dialogue_ram();
    o.W8(kS + 0x41, 1); o.W16(kS + 0x34, 1190); o.W8(kS + 0x3C, 22); o.W16(kS + 0x36, 0);
    d = ReadGameState(o.View()).dialogue;
    CHECK(d.valid && d.stringId == 1190 && d.page == 1 && d.pages == 1 && d.last && d.speaker == 22 && d.speakerNameId == 345);
    o.W16(kS + 0x34, 0);
    CHECK(!ReadGameState(o.View()).dialogue.valid);
}

int main()
{
    hud();
    dialogue();
    between_screens();
    panel_and_popup();
    settled_only_for_3();
    plain_screens();
    overlay_screens();
    invalid_chains();
    random_ram();
    to_screen_state();
    console_screen();
    if (failures) { fprintf(stderr, "%d failed\n", failures); return 1; }
    printf("all passed\n");
    return 0;
}
