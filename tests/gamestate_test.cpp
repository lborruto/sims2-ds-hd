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
    r.W32(kS + 0x40, 0x0219E800);
    r.W32(0x0219E800, 0x02132580);
    r.W32(0x0219E800 + 0x24, 3);
    r.W16(kS + 0xD0, 0x204);   // the big portrait: not the selection
    g = ReadGameState(r.View());
    CHECK(g.tab == 3 && g.selection == 3 && GameStateName(g) == "Panel/Relationships");
    r.W32(0x0219E800, 0x1234);   // not the tab's vtable
    CHECK(ReadGameState(r.View()).selection == -1);
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
    // a bad name byte or no NUL within the 8-byte field blanks the name only
    auto blank = [&](Ram q) { HudState x = ReadGameState(q.View()).hud; return x.valid && x.simName.empty() && x.money == -120 && x.hour == 15 && x.sanityMax == 225; };
    { Ram q = r; q.W8(kG + 0x53, 0x01); CHECK(blank(q)); }
    { Ram q = r; q.W8(kG + 0x54, 0xC3); CHECK(blank(q)); }
    { Ram q = r; for (int i = 0; i < 8; i++) q.W8(kG + 0x53 + i, 'A'); CHECK(blank(q)); }
    { Ram q = r; for (int i = 0; i < 7; i++) q.W8(kG + 0x53 + i, 'A'); q.W8(kG + 0x53 + 7, 0); CHECK(ReadGameState(q.View()).hud.simName == "AAAAAAA"); }
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

// The town's shared records, inventory, options, popup and save slots on a synthetic RAM.
constexpr u32 kRec = 0x02122A00;
static void put_item_row(Ram& r, int id, u32 desc)
{
    const u32 a = kRec + 0x12 * (id + 0x3B);
    r.W16(a, desc - 1); r.W16(a + 2, desc); r.W16(a + 4, desc + 1); r.W16(a + 8, 463 + id); r.W16(a + 0xA, 7); r.W16(a + 0xE, 80);
}

static void records()
{
    Ram r = Make(0x0213203C, -1);
    CHECK(ReadItemRow(r.View(), 5).valid);   // an all-zero row reads; only the bounds matter
    put_item_row(r, 106, 1991);
    RecordRow row = ReadItemRow(r.View(), 106);
    CHECK(row.valid && row.nameId == 569 && row.descRes == 1991 && row.tilesRes == 1990 && row.palRes == 1992 && row.iconRes == 7 && row.price == 80);
    const u32 s = kRec + 0x12 * (2 + 22);
    r.W16(s + 8, 345); r.W16(s + 0xA, 4359);
    row = ReadSimRow(r.View(), 22);
    CHECK(row.valid && row.nameId == 345 && row.iconRes == 4359);
    CHECK(!ReadSimRow(r.View(), 46).valid && !ReadSimRow(r.View(), -1).valid);
    CHECK(!ReadItemRow(r.View(), 0x200).valid && !ReadItemRow(r.View(), -1).valid);
    CHECK(!ReadSimRow(RamView{nullptr, 0}, 0).valid);
    // Sim records
    put_hud(r);
    const u32 R = kG + 0x21DC + 0x2C * 4;
    r.W16(R + 0x18, 4); r.W8(R + 0x1A, 2); r.W8(R + 0x1B, 5); r.W32(R + 0x14, 3);
    SimRecord sim = ReadSimRecord(r.View(), 4);
    CHECK(sim.valid && sim.simId == 4 && sim.mood == 2 && sim.level == 5 && sim.met && sim.secret);
    r.W8(R + 0x1A, 7);
    CHECK(!ReadSimRecord(r.View(), 4).valid);
    CHECK(!ReadSimRecord(r.View(), 46).valid);
}

static Ram town_ram()
{
    Ram r = Make(0x0213203C, -1);
    put_hud(r);
    put_item_row(r, 171, 321); put_item_row(r, 106, 1991);
    r.W32(kG + 0x215C, 2);
    r.W16(kG + 0x28, 171); r.W16(kG + 0x2A, 106);
    r.W8(kG + 0x24, 27); r.W8(kG + 0x26, 2);
    r.W32(kG, 0x0E);   // bits 1..3
    r.W32(kG + 0x2F70, 0x4CD); r.W32(kG + 0x2F74, 0x1000);
    return r;
}

static void inventory()
{
    Ram r = town_ram();
    InventoryState v = ReadGameState(r.View()).town.inventory;
    CHECK(v.valid && v.slots.size() == 2);
    CHECK(v.slots[0].id == 171 && v.slots[0].nameId == 634 && v.slots[0].descId == 993 && v.slots[0].iconRes == 321 && v.slots[0].count == 1);
    CHECK(v.slots[1].id == 106 && v.slots[1].nameId == 569 && v.slots[1].descId == 928 && v.slots[1].iconRes == 1991);
    CHECK(v.counted[0].count == 27 && v.counted[1].count == 0 && v.counted[2].count == 2 && v.counted[3].count == 0);
    CHECK(v.counted[0].nameId == 1154 && v.counted[3].descId == 848);
    CHECK(v.keys[0].count == 1 && v.keys[1].count == 1 && v.keys[2].count == 1 && v.keys[2].nameId == 1970 && v.keys[2].descId == 1974);
    r.W32(kG, 0x04);
    v = ReadGameState(r.View()).town.inventory;
    CHECK(v.keys[0].count == 0 && v.keys[1].count == 1 && v.keys[2].count == 0);
    r.W32(kG + 0x215C, 0);
    v = ReadGameState(r.View()).town.inventory;
    CHECK(v.valid && v.slots.empty());
    // garbage: the inventory only
    auto bad = [&](auto f) { Ram q = town_ram(); f(q); GameState g = ReadGameState(q.View()); return !g.town.inventory.valid && g.hud.valid && g.town.options.valid; };
    CHECK(bad([](Ram& q) { q.W32(kG + 0x215C, 7); }));
    CHECK(bad([](Ram& q) { q.W32(kG + 0x215C, (u32)-1); }));
    CHECK(bad([](Ram& q) { q.W16(kG + 0x28, 0x200); }));
    CHECK(bad([](Ram& q) { q.W16(kG + 0x28, 5); }));            // the row does not name item 5
    CHECK(bad([](Ram& q) { q.W16(kRec + 0x12 * (171 + 0x3B) + 8, 1); }));
    { Ram q = town_ram(); q.W32(0x0213E434, 0xFFFFFFF0); CHECK(!ReadGameState(q.View()).town.inventory.valid); }
    { Ram q = town_ram(); CHECK(ReadGameState(q.View()) == ReadGameState(q.View())); q.W8(kG + 0x24, 3); CHECK(ReadGameState(q.View()) == ReadGameState(town_ram().View())); }   // town is not screen identity
}

static void options()
{
    Ram r = town_ram();
    OptionsState o = ReadGameState(r.View()).town.options;
    CHECK(o.valid && o.music == 0x4CD && o.sfx == 0x1000);
    r.W32(kG + 0x2F70, 0); r.W32(kG + 0x2F74, 0);
    o = ReadGameState(r.View()).town.options;
    CHECK(o.valid && o.music == 0 && o.sfx == 0);
    auto bad = [&](u32 off, u32 v) { Ram q = town_ram(); q.W32(kG + off, v); GameState g = ReadGameState(q.View()); return !g.town.options.valid && g.town.inventory.valid; };
    CHECK(bad(0x2F70, 0x1001) && bad(0x2F74, 0x1001) && bad(0x2F74, 0xFFFFFFFF));
}

static void put_text(Ram& r, u32 a, const char* s) { for (; *s; s++) r.W8(a++, (u8)*s); r.W8(a, 0); }

static void popup()
{
    Ram r = town_ram();
    CHECK(!ReadGameState(r.View()).town.popup.valid);   // none
    r.W8(kP + 0x8, 2); r.W32(kP + 0x10, 2);
    put_text(r, kP + 0x34, "Quit?"); put_text(r, kP + 0x434, "");
    r.W32(kP + 0xD88, 1); r.W8(kP + 0xD8C, 1);
    PopupState p = ReadGameState(r.View()).town.popup;
    CHECK(p.valid && p.state == 2 && p.kind == 2 && p.text == "Quit?" && p.title.empty() && p.pressed == 1 && p.pressing);
    CHECK(p.boxW == 0 && p.boxTiles == 0); // (no box read: no button place)
    r.W32(kP + 0x544, 56); r.W32(kP + 0x550, 160); r.W32(kP + 0x554, 6);
    p = ReadGameState(r.View()).town.popup;
    CHECK(p.valid && p.boxY == 56 && p.boxW == 160 && p.boxTiles == 6);
    r.W32(kP + 0x554, 40);
    CHECK(ReadGameState(r.View()).town.popup.valid && ReadGameState(r.View()).town.popup.boxW == 0); // a box off the screen
    r.W32(kP + 0x554, 6);
    put_text(r, kP + 0x434, "T"); r.W8(kP + 0x8, 1); r.W32(kP + 0x10, 3);
    p = ReadGameState(r.View()).town.popup;
    CHECK(p.valid && p.state == 1 && p.kind == 3 && p.title == "T");
    // raw bytes are kept as they are (decoded by GameTextToUtf8)
    r.W8(kP + 0x34, 0x7B); r.W8(kP + 0x35, 0);
    CHECK(ReadGameState(r.View()).town.popup.text == std::string("\x7B"));
    // the popup over the panel is still Popup; the screen's identity is unchanged by the text
    auto bad = [&](auto f) { Ram q = town_ram(); q.W8(kP + 0x8, 2); q.W32(kP + 0x10, 0); put_text(q, kP + 0x34, "A"); f(q); GameState g = ReadGameState(q.View()); return !g.town.popup.valid && g.hud.valid; };
    CHECK(!bad([](Ram&) {}));
    CHECK(bad([](Ram& q) { q.W32(kP + 0x10, 1); }));
    CHECK(bad([](Ram& q) { q.W32(kP + 0x10, 4); }));
    CHECK(bad([](Ram& q) { q.W8(kP + 0x8, 4); }));
    CHECK(bad([](Ram& q) { q.W8(kP + 0x8, 0xFF); }));
    CHECK(bad([](Ram& q) { for (int i = 0; i < 0x400; i++) q.W8(kP + 0x34 + i, 'x'); }));     // no NUL in the text
    CHECK(bad([](Ram& q) { for (int i = 0; i < 0x100; i++) q.W8(kP + 0x434 + i, 'x'); }));    // no NUL in the title
    CHECK(bad([](Ram& q) { q.W8(kP + 0xD8C, 2); }));
}

static void saves()
{
    Ram r = town_ram();
    SaveSlots s = ReadGameState(r.View()).town.saves;
    CHECK(s.valid && s.current == 0 && !s.used[0] && !s.used[1] && !s.used[2]);   // zeros: current slot 0
    r.W32(0x0213D84C, (u32)-1);
    s = ReadGameState(r.View()).town.saves;
    CHECK(s.valid && s.current == -1);
    r.W8(0x0213D848 + 1, 1);
    const u32 h = 0x0213D7B8 + 0x30;
    r.W32(h + 0x18, 1055); r.W32(h + 0x14, 0x1234); put_text(r, h + 0x1C, "Clare"); r.W8(h + 0x2C, 19);
    r.W32(0x0213D84C, 1);
    s = ReadGameState(r.View()).town.saves;
    CHECK(s.valid && s.current == 1 && s.used[1] && s.slot[1].money == 1055 && s.slot[1].simName == "Clare" && s.slot[1].place == 19 && s.slot[1].time == 0x1234);
    r.W8(h + 0x1C, 1);   // a bad name blanks the name only
    s = ReadGameState(r.View()).town.saves;
    CHECK(s.valid && s.slot[1].simName.empty() && s.slot[1].money == 1055);
    auto bad = [&](auto f) { Ram q = r; f(q); GameState g = ReadGameState(q.View()); return !g.town.saves.valid && g.hud.valid; };
    CHECK(bad([](Ram& q) { q.W32(0x0213D84C, 3); }));
    CHECK(bad([](Ram& q) { q.W32(0x0213D84C, (u32)-2); }));
    CHECK(bad([](Ram& q) { q.W8(0x0213D848 + 2, 2); }));
}

static void town_random()
{
    std::mt19937 rng(7);
    for (int round = 0; round < 200; round++)
    {
        Ram r = town_ram();
        r.W8(kP + 0x8, 2); r.W32(kP + 0x10, 0);
        for (int i = 0; i < 300; i++)
        {
            const u32 areas[] = {kG, kP, 0x0213D7B8, kRec};
            const u32 a = areas[rng() % 4] + rng() % 0x4900;
            r.W8(a, (u32)rng());
        }
        GameState g = ReadGameState(r.View());
        CHECK(g.town.inventory.slots.size() <= 6);
    }
}

// ---- The menus' lists on a synthetic RAM: hand-written chains with made-up values.
constexpr u32 kT = 0x0219E000, kTG = 0x0219E800, kTR = 0x0219F000, kE = 0x021A0000, kActor = 0x021A0100, kIface = 0x021A0200;
constexpr u32 kGb = 0x021670A4;
static Ram menus_ram(u32 vt, int ov, u32 tab)
{
    Ram r = Make(vt, ov);
    r.W32(0x0213E434, kGb);
    r.W32(kS + 0x64, tab);
    return r;
}
static void put_sim_row(Ram& r, int k)
{
    const u32 a = kRec + 0x12 * (k + 2);
    r.W16(a + 8, 323 + k); r.W16(a + 0xA, 1000 + k);
}
static void interact_list()
{
    Ram r = menus_ram(0x0213203C, -1, 0);
    r.W32(kS + 0x34, kT); r.W32(kT, 0x021321C4);
    put_item_row(r, 29, 100); put_sim_row(r, 51);
    r.W32(kT + 0x300, kE); r.W32(kE, kActor); r.W32(kE + 4, 1);
    r.W16(kActor + 0xA, 29); r.W16(kActor + 0xC, 0xB); r.W32(kActor + 0x130, kIface);
    r.W32(kIface + 8, 3); r.W8(kIface + 0x10, 0x62); r.W8(kIface + 0x11, 0x02); r.W8(kIface + 0x12, 0x5D);
    r.W32(kT + 0x314, 0); r.W32(kT + 0x318, 1);
    r.W16(kS + 0xD0, 0x1A5A);
    GameState g = ReadGameState(r.View());
    const InteractState& v = g.town.interact;
    CHECK(v.valid && v.target.kind == TargetRef::Object && v.target.nameId == 463 + 29 && v.padMode == 0 && v.highlighted == 1);
    CHECK(v.actions.size() == 3 && v.actions[0].labelId == 1845 + 0x62 && !v.actions[0].greyed && v.actions[1].greyed && v.bigPortraitRes == 0x1A5A);
    CHECK(v.prev.kind == TargetRef::None && v.next.kind == TargetRef::None);
    // a Sim target, an unknown one, and the neighbours
    r.W16(kActor + 0xA, 51); r.W16(kActor + 0xC, 2); r.W32(kE + 4, 2);
    CHECK(ReadGameState(r.View()).town.interact.target.nameId == 321);
    r.W32(kE + 4, 1);
    CHECK(ReadGameState(r.View()).town.interact.target.nameId == 323 + 51);
    r.W32(kT + 0x308, kE);
    CHECK(ReadGameState(r.View()).town.interact.next.index == 51);
    // garbage: each part invalid on its own
    r.W32(kT + 0x308, 0);
    r.W16(kActor + 0xA, 57); CHECK(!ReadGameState(r.View()).town.interact.valid);       // Sim index out of the table
    r.W16(kActor + 0xA, 51); r.W32(kIface + 8, 5); CHECK(!ReadGameState(r.View()).town.interact.valid);   // too many actions
    r.W32(kIface + 8, 3); r.W8(kIface + 0x10, 0x70); CHECK(!ReadGameState(r.View()).town.interact.valid);   // label out of the verb list
    r.W8(kIface + 0x10, 0x62); r.W32(kT + 0x314, 3); CHECK(!ReadGameState(r.View()).town.interact.valid);
    r.W32(kT + 0x314, 1); r.W32(kT + 0x318, 7); CHECK(ReadGameState(r.View()).town.interact.valid == false);
    r.W32(kT + 0x318, 0); r.W32(kT, 0x1234); CHECK(!ReadGameState(r.View()).town.interact.valid);   // wrong vtable
    r.W32(kT, 0x021321C4); r.W32(kT + 0x300, 0); r.W32(kT + 0x308, 0x90000); r.W32(kT + 0x30C, 0x616);   // no target (stale strip pointers): valid, kind None
    const InteractState n = ReadGameState(r.View()).town.interact;
    CHECK(n.valid && n.target.kind == TargetRef::None && n.actions.empty());
    CHECK(ReadGameState(r.View()).town.goals.valid == false);   // read only on its own tab
}

// The amounts the game's own label methods put in @1: Buy 2 x the object's record price, Payoff
// 1500, Bail 200, Pay the object's entry in G's three {record, -, amount}; none for other verbs.
static void interact_amounts()
{
    Ram r = menus_ram(0x0213203C, -1, 0);
    r.W32(kS + 0x34, kT); r.W32(kT, 0x021321C4);
    put_item_row(r, 29, 100);
    r.W32(kT + 0x300, kE); r.W32(kE, kActor); r.W32(kE + 4, 1);
    r.W16(kActor + 0xA, 29); r.W16(kActor + 0xC, 0xB); r.W32(kActor + 0x130, kIface);
    r.W32(kIface + 4, kActor);
    r.W32(kIface + 8, 4); r.W8(kIface + 0x10, 0x5E); r.W8(kIface + 0x11, 0x4F); r.W8(kIface + 0x12, 0x5F); r.W8(kIface + 0x13, 7);
    r.W32(kT + 0x314, 1); r.W32(kT + 0x318, 0);
    r.W8(kGb + 0x301C + 4, 29); r.W16(kGb + 0x301C + 6, 350);   // Pay: the second entry is this object's
    InteractState v = ReadGameState(r.View()).town.interact;
    CHECK(v.valid && v.actions.size() == 4);
    if (v.actions.size() == 4)
    {
        CHECK(v.actions[0].amount == 160);   // 2 x the row's 80
        CHECK(v.actions[1].amount == 1500 && v.actions[2].amount == 200 && v.actions[3].amount == 350);
    }
    r.W8(kGb + 0x301C + 4, 30);              // no entry for it: no amount
    r.W8(kIface + 0x10, 0x62);               // another verb: none
    v = ReadGameState(r.View()).town.interact;
    CHECK(v.valid && v.actions[0].amount == -1 && v.actions[3].amount == -1);
}
static void goals_list()
{
    Ram r = menus_ram(0x0213203C, -1, 2);
    r.W32(kS + 0x3C, kTG); r.W32(kTG, 0x02132480);
    r.W32(kTG + 0x50C, 2); r.W32(kTG + 0x510, 4);
    r.W32(kTG + 0x18, 3); r.W32(kTG + 0x1C, 0); r.W32(kTG + 0x20, 1); r.W8(kTG + 0x24, 1);
    r.W32(kTG + 0x28, 5); r.W32(kTG + 0x2C, 1); r.W32(kTG + 0x30, 3); r.W8(kTG + 0x34, 1);
    r.W32(kTG + 0x514, 1); r.W32(kTG + 0x518, 0);
    GameState g = ReadGameState(r.View());
    const GoalsState& v = g.town.goals;
    CHECK(v.valid && v.entries.size() == 2 && v.entries[0].textId == 1627 + 3 && v.entries[1].textId == 1627 + 5 && v.entries[1].nLines == 3 && v.selected == 1 && v.lines == 4 && !v.footer);
    r.W32(kTG + 0x30, 4); CHECK(!ReadGameState(r.View()).town.goals.valid);   // lines past the total
    r.W32(kTG + 0x30, 3); r.W32(kTG + 0x28, 34); CHECK(!ReadGameState(r.View()).town.goals.valid);   // mission id out of range
    r.W32(kTG + 0x28, 5); r.W32(kTG + 0x514, 2); CHECK(!ReadGameState(r.View()).town.goals.valid);   // selection past the count
    r.W32(kTG + 0x514, 1); r.W32(kTG + 0x50C, 45); CHECK(!ReadGameState(r.View()).town.goals.valid);
    // the footer: game flag 17 set, the score from the weights' table (made-up weights 7)
    r.W32(kTG + 0x50C, 2);
    r.W32(kGb + 0x4814, 1u << 17); r.W32(kGb + 0x4410, 0x3);
    r.W8(0x0211F9EC + 2, 7); r.W8(0x0211F9EC + 6, 9);
    const GoalsState f = ReadGameState(r.View()).town.goals;
    CHECK(f.valid && f.footer && f.score == 16 && f.footerId == 1841 && f.scoreId == 1842);
}
static void relations_list()
{
    Ram r = menus_ram(0x0213203C, -1, 3);
    r.W32(kS + 0x40, kTR); r.W32(kTR, 0x02132580);
    put_sim_row(r, 4); put_sim_row(r, 9);
    const u32 R4 = kGb + 0x21DC + 0x2C * 4, R9 = kGb + 0x21DC + 0x2C * 9;
    r.W16(R4 + 0x18, 4); r.W32(R4 + 0x14, 1); r.W8(R4 + 0x1A, 2); r.W8(R4 + 0x1B, 5);
    r.W16(R9 + 0x18, 9); r.W32(R9 + 0x14, 3); r.W8(R9 + 0x1A, 0); r.W8(R9 + 0x1B, 1);
    r.W32(kTR + 0x1C, 2); r.W32(kTR + 0x2C, R4); r.W32(kTR + 0x30, R9);
    r.W32(kTR + 0x24, 1); r.W32(kTR + 0x28, 9); r.W32(kTR + 0x20, 0);
    GameState g = ReadGameState(r.View());
    const RelState& v = g.town.relations;
    CHECK(g.selection == 1 && v.valid && v.sims.size() == 2 && v.selected == 1);
    CHECK(v.sims[0].nameId == 327 && v.sims[0].descId == 393 && v.sims[0].moodId == 186 && v.sims[0].levelId == 192 + 14 + 5 && v.sims[0].portraitRes == 1004);
    CHECK(v.sims[1].secret && !v.sims[0].secret);
    r.W32(kTR + 0x28, 4); CHECK(!ReadGameState(r.View()).town.relations.valid);        // the Sim id beside the index disagrees
    r.W32(kTR + 0x28, 9); r.W32(kTR + 0x30, R9 + 1); CHECK(!ReadGameState(r.View()).town.relations.valid);   // not a record
    r.W32(kTR + 0x30, kGb + 0x21DC + 0x2C * 30); CHECK(!ReadGameState(r.View()).town.relations.valid);        // beyond the 24 listed
    r.W32(kTR + 0x30, R9); r.W32(kTR + 0x1C, 25); CHECK(!ReadGameState(r.View()).town.relations.valid);
    r.W32(kTR + 0x1C, 2); r.W32(R9 + 0x14, 2); CHECK(!ReadGameState(r.View()).town.relations.valid);          // not met
}
static void phone_gift_note()
{
    Ram r = menus_ram(0x02131F08, -1, 0);
    r.W8(kS + 0x88, 3); r.W8(kS + 0x89, 4); r.W8(kS + 0x8A, 3); r.W8(kS + 0x56, 1);
    const u32 E1 = kGb + 0x303C + 0x24;
    r.W8(E1 + 0x1A, 1); r.W8(E1 + 0x1B, 0); r.W32(E1 + 0x14, 2311); r.W16(E1 + 0x18, 45); r.W32(E1 + 4 * 0, 0);
    for (int i = 0; i < 4; i++) r.W32(0x0211E99C + 4 * i, 2066 + i);
    r.W8(kGb + 0x31C5, 2);
    r.W32(kGb + 0x47F4, 11); r.W32(kGb + 0x47F8, 5); r.W8(0x0213E428, 10);
    PhoneState p = ReadGameState(r.View()).town.phone;
    CHECK(p.valid && p.page == 3 && p.row == 4 && p.scroll == 3 && p.shownSlot == 1 && p.ringtone == 2 && p.menuIds[3] == 2069);
    CHECK(p.month == 11 && p.day == 5 && p.textEnd == 10 && p.maidCost == -1);
    r.W32(kGb + 0x47F4, 13); CHECK(!ReadGameState(r.View()).town.phone.valid);   // no 13th month
    r.W32(kGb + 0x47F4, 11); r.W8(0x0213E428, 0); CHECK(!ReadGameState(r.View()).town.phone.valid);   // Next Message on the sender's row
    r.W8(0x0213E428, 10);
    CHECK(p.inbox.size() == 1 && p.inbox[0].slot == 1 && p.inbox[0].textId == 2311 && p.inbox[0].senderNameId == 323 + 45 && !p.inbox[0].read);
    r.W8(kS + 0x56, 0xFF); r.W8(kS + 0x88, 2);   // stale fields off their pages are ignored
    CHECK(ReadGameState(r.View()).town.phone.valid && ReadGameState(r.View()).town.phone.shownSlot == -1);
    r.W8(kS + 0x88, 3); CHECK(ReadGameState(r.View()).town.phone.valid);   // -1 after a delete
    r.W8(kS + 0x56, 10); CHECK(!ReadGameState(r.View()).town.phone.valid);
    r.W8(kS + 0x56, 1); r.W8(kS + 0x88, 11); CHECK(!ReadGameState(r.View()).town.phone.valid);
    r.W8(kS + 0x88, 3); r.W8(E1 + 0x1A, 2); CHECK(!ReadGameState(r.View()).town.phone.valid);
    r.W8(E1 + 0x1A, 1); r.W16(E1 + 0x18, 60); CHECK(!ReadGameState(r.View()).town.phone.valid);   // sender not a Sim
    r.W16(E1 + 0x18, 45); r.W8(kGb + 0x31C5, 9); CHECK(!ReadGameState(r.View()).town.phone.valid);
    r.W8(kGb + 0x31C5, 2); r.W8(kS + 0x88, 7); r.W8(kS + 0x86C, 22); r.W32(kS + 0x870, 2);
    p = ReadGameState(r.View()).town.phone;
    CHECK(p.valid && p.contacts == 22 && p.contactSim == 2 && !p.contactsKnown); // (the records list none: it disagrees)
    // The records met (flags bit 0) and in town: Sims 2 and 5; the game's count and highlighted Sim agree.
    r.W8(kGb + 0x21DC + 0x2C * 2 + 0x14, 1); r.W8(kGb + 0x21DC + 0x2C * 5 + 0x14, 1);
    r.W8(kS + 0x86C, 2); r.W8(kS + 0x89, 1); r.W32(kS + 0x870, 5); r.W8(kS + 0x8A, 0);
    for (int j = 0; j < 5; j++) r.W8(kGb + 0x2BC0 + 8 * j + 4, 0x3F);
    p = ReadGameState(r.View()).town.phone;
    CHECK(p.valid && p.contactsKnown && p.contactList == std::vector<int>({2, 5}));
    r.W8(kGb + 0x21DC + 0x2C * 5 + 0x10, 8); // (Sim 5 away: the list disagrees)
    CHECK(!ReadGameState(r.View()).town.phone.contactsKnown);
    r.W8(kGb + 0x21DC + 0x2C * 5 + 0x10, 0);
    // Page 5: the maid's price; page 9: the Sim called, mood and place (place 0x21: away).
    r.W8(kS + 0x88, 5); r.W32(0x0213E430, 2270);
    CHECK(ReadGameState(r.View()).town.phone.maidCost == 2270);
    r.W8(kS + 0x88, 9); r.W8(0x0213E42C, 4); r.W16(kS + 0x864, 2); r.W8(kGb + 0x21DC + 0x2C * 2 + 0x10, 1);
    r.W8(kGb + 0x21DC + 0x2C * 2 + 0x4, 13); r.W8(kGb + 0x21DC + 0x2C * 2 + 0x1A, 2);
    p = ReadGameState(r.View()).town.phone;
    CHECK(p.valid && p.textEnd == 4 && p.caller == 2 && p.callMood == 2 && p.callPlace == 13 && !p.callAway);
    r.W8(kGb + 0x21DC + 0x2C * 2 + 0x4, 0x21);
    CHECK(ReadGameState(r.View()).town.phone.callAway);
    r.W16(kS + 0x864, 46); CHECK(!ReadGameState(r.View()).town.phone.valid);   // not a Sim
    r.W16(kS + 0x864, 2); r.W8(kGb + 0x21DC + 0x2C * 2 + 0x4, 0x25); CHECK(!ReadGameState(r.View()).town.phone.valid);   // a place the strings have none of
    r.W8(kGb + 0x21DC + 0x2C * 2 + 0x4, 0x1F); CHECK(ReadGameState(r.View()).town.phone.valid);
    r.W8(kS + 0x88, 7); r.W8(kS + 0x86C, 22);
    r.W8(kS + 0x86C, 65); CHECK(!ReadGameState(r.View()).town.phone.valid);

    // Give Gift
    Ram q = menus_ram(0x0214ABA4, 6, 0);
    put_item_row(q, 106, 200);
    q.W32(kGb + 0x215C, 1); q.W16(kGb + 0x28, 106);
    q.W32(kS + 0x24, kActor); q.W16(kActor + 0xA, 22); q.W16(kGb + 0x21DC + 0x2C * 22 + 0x18, 22);
    q.W32(kS + 0x4C, 0); q.W32(kS + 0x34, kE); q.W8(kE + 0x94, 1);
    GiftState gf = ReadGameState(q.View()).town.gift;
    CHECK(gf.valid && gf.recipient == 22 && gf.recipientNameId == 345 && gf.slot == 0 && gf.refused[0]);
    q.W32(kS + 0x4C, 1); CHECK(!ReadGameState(q.View()).town.gift.valid);   // slot past the pockets
    q.W32(kS + 0x4C, 0); q.W16(kActor + 0xA, 46); CHECK(!ReadGameState(q.View()).town.gift.valid);
    q.W16(kActor + 0xA, 22); q.W8(kE + 0x94, 2); CHECK(!ReadGameState(q.View()).town.gift.valid);

    // Notes: overlay 11
    Ram n = menus_ram(0x0214A040, 11, 0);
    n.W16(kS + 0x2C, 0x509);
    NoteState nt = ReadGameState(n.View()).town.note;
    CHECK(nt.valid && nt.textId == 0x509 && nt.playerName);
    n.W16(kS + 0x2C, 3127); CHECK(!ReadGameState(n.View()).town.note.valid);
    n.W16(kS + 0x2C, 100);
    CHECK(ReadGameState(n.View()).town.note.valid && !ReadGameState(n.View()).town.note.playerName);
    Ram w = menus_ram(0x0214A040, 10, 0); w.W16(kS + 0x2C, 100);   // another overlay with that vtable: not a note
    CHECK(!ReadGameState(w.View()).town.note.valid);
}
static void menus_random()
{
    std::mt19937 rng(11);
    // The tabs' screen and the Phone, Gift and Note screens (overlays 6 and 11) with their pages.
    struct Kind { u32 vt; int overlay; };
    const Kind kinds[] = {{0x0213203C, -1}, {0x0213203C, -1}, {0x0213203C, -1}, {0x0213203C, -1}, {0x02131F08, -1}, {0x0214ABA4, 6}, {0x0214A040, 11}};
    for (int round = 0; round < 700; round++)
    {
        const Kind k = kinds[round % 7];
        Ram r = menus_ram(k.vt, k.overlay, (u32)(round % 4));
        r.W32(kS + 0x34, kT); r.W32(kT, 0x021321C4); r.W32(kS + 0x38, 0x0219E400); r.W32(0x0219E400, 0x0213232C);
        r.W32(kS + 0x3C, kTG); r.W32(kTG, 0x02132480); r.W32(kS + 0x40, kTR); r.W32(kTR, 0x02132580);
        if (k.vt == 0x02131F08) r.W8(kS + 0x88, (u32)(round / 7 % 12));
        for (int i = 0; i < 400; i++)
        {
            const u32 areas[] = {kT, kTG, kTR, 0x0219E400, kE, kActor, kIface, kGb + 0x2BE8, kGb + 0x21DC, kGb + 0x303C, kS};
            r.W8(areas[rng() % 11] + rng() % 0x600, (u32)rng());
        }
        const GameState g = ReadGameState(r.View());   // must not crash or run out of bounds; counts stay capped
        CHECK(g.town.goals.entries.size() <= 44 && g.town.relations.sims.size() <= 24 && g.town.interact.actions.size() <= 4 && g.town.phone.inbox.size() <= 10);
        CHECK(g.town.phone.contactList.size() <= 64);
    }
}

int main()
{
    records();
    inventory();
    options();
    popup();
    saves();
    town_random();
    interact_list();
    interact_amounts();
    goals_list();
    relations_list();
    phone_gift_note();
    menus_random();
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
