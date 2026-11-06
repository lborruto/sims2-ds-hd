// Checks the screen manager reader (gamestate.*) on a synthetic main RAM: the pointer chain
// is written by hand with the values of the RAM map, no game data and no melonDS NDS.
//   ninja -C build gamestate_test && ./build/gamestate_test
#include <cstdio>
#include <cstring>
#include <random>
#include <tuple>
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
        {0x021509B0, 3, GScreen::CreateASim}, {0x0214B1C4, 8, GScreen::SaveSlots}, {0x0214B8BC, 18, GScreen::Drencher},
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
        {GScreen::SaveSlots, Screen::SaveSlots}, {GScreen::Unknown, Screen::Unknown}};
    for (auto [gs, sc] : same)
    {
        g.screen = gs;
        CHECK(ToScreenState(g) == (ScreenState{sc}));
        CHECK(std::string(ScreenName(sc)) == GScreenName(gs));
    }
    // The Drencher is CONSOLE's ItemHand; the minigames are the stick cursor's Unknown.
    g.screen = GScreen::Drencher;
    CHECK(ToScreenState(g) == (ScreenState{Screen::ItemHand}));
    for (GScreen m : {GScreen::Autopsy, GScreen::Detector, GScreen::MooGoo, GScreen::Scratch, GScreen::Crime, GScreen::Mixer, GScreen::MixerSave, GScreen::ArtGallery})
    {
        g.screen = m;
        CHECK(ToScreenState(g) == (ScreenState{Screen::Unknown}));
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


// ---- The hotel and the shops: made-up RAM with the research's layouts.
static Ram hotel_ram(u32 vt, int ov)
{
    Ram r = menus_ram(vt, ov, 0);
    for (u32 k = 0; k < 33; k++) { r.W16(0x0211F9EC + 4 * k, 0); r.W8(0x0211F9EC + 4 * k + 2, 0); r.W8(0x0211F9EC + 4 * k + 3, 0x21); }
    r.W16(0x0211F9EC + 4 * 4, 1500); r.W8(0x0211F9EC + 4 * 4 + 2, 6); r.W8(0x0211F9EC + 4 * 4 + 3, 2);     // casino
    r.W16(0x0211F9EC + 4 * 15, 1000); r.W8(0x0211F9EC + 4 * 15 + 2, 8); r.W8(0x0211F9EC + 4 * 15 + 3, 27);  // Jungle
    r.W32(kGb + 0x4410, (1u << 29) | (1u << 4)); r.W32(kGb + 0x4414, 0);
    for (u32 j = 0; j < 5; j++) { r.W32(kGb + 0x2BC0 + 8 * j, 0x21); r.W8(kGb + 0x2BC4 + 8 * j, 0x3F); }
    r.W32(kGb + 0x2F60, 2000); r.W32(kGb + 0x84, 3); r.W32(kGb + 0x2F64, 500); r.W32(kGb + 0x2F68, 300);
    return r;
}
static void hotel_block()
{
    Ram r = hotel_ram(0x0213203C, -1);
    r.W32(kGb + 0x2BC0, 29); r.W8(kGb + 0x2BC4, 45); r.W8(kGb + 0x2BC5, 2);
    r.W8(kGb + 0x449C + 4, 3);
    r.W32(0x0213E580, 0x021A1000); r.W32(0x021A1000 + 0xF4, 13);
    HotelState h = ReadGameState(r.View()).hotel;
    CHECK(h.valid && h.built == ((1u << 29) | (1u << 4)) && h.score == 6 && h.rooms[15].permit == 1000 && h.rooms[15].parent == 27 && h.slots[0].place == 29 && h.slots[0].sim == 45 && h.slots[0].days == 2);
    CHECK(h.guestsStayed == 3 && h.money == 2000 && h.earned == 500 && h.spent == 300 && h.songs[4] == 3 && h.songs[5] == 0 && h.place == 13);
    // Rooms being built: their completion time (G+0x4418 + 4 place) not yet past the clock (G+0x480C); never set 0x80000000.
    r.W32(kGb + 0x480C, 100000);
    for (u32 k = 0; k < 33; k++) r.W32(kGb + 0x4418 + 4 * k, k == 15 ? 100000 + 0x7080 : k == 4 ? 99999 : 0x80000000u);
    CHECK(ReadGameState(r.View()).hotel.building == (1ull << 15));
    r.W32(kGb + 0x4418 + 4 * 32, 100000); CHECK(ReadGameState(r.View()).hotel.building == ((1ull << 15) | (1ull << 32)));
    r.W32(kGb + 0x4414, 2); CHECK(!ReadGameState(r.View()).hotel.valid);   // a bit past place 32
    r.W32(kGb + 0x4414, 1); CHECK(ReadGameState(r.View()).hotel.valid);
    r.W32(kGb + 0x4414, 0); r.W8(0x0211F9EC + 4 * 4 + 2, 101); CHECK(!ReadGameState(r.View()).hotel.valid);   // a weight over 100
    r.W8(0x0211F9EC + 4 * 4 + 2, 6); r.W32(kGb + 0x2BC0, 0x22); CHECK(!ReadGameState(r.View()).hotel.valid);
    r.W32(kGb + 0x2BC0, 29); r.W32(kGb + 0x2F68, 1000000); CHECK(!ReadGameState(r.View()).hotel.valid);
    // the hotel block reads on its own and the screens' parts stay empty on the panel
    GameState g = ReadGameState(hotel_ram(0x0213203C, -1).View());
    CHECK(g.hotel.valid && !g.checkIn.valid && !g.roomInfo.valid && !g.music.valid && !g.build.valid && !g.catalogue.valid && !g.stat.valid && !g.placement.valid);
}
static const u32 kGuestRooms[6] = {8, 29, 17, 15, 18, 20};   // the furniture records' place ids
static const u32 kSongCodes[4] = {0x2B, 1, 3, 0x2A};

static void registry_and_room()
{
    Ram r = hotel_ram(0x0214A804, 4);
    const u8 rooms[5] = {0x1D, 0x08, 0x0F, 0x12, 0x14};
    for (int i = 0; i < 5; i++) r.W8(0x0214A7A0 + i, rooms[i]);
    r.W8(kS + 0x26, 1);
    r.W32(kGb + 0x4410, (1u << 29) | (1u << 15) | (1u << 20));
    r.W32(kGb + 0x2BC0, 15); r.W8(kGb + 0x2BC4, 7);
    CheckInState c = ReadGameState(r.View()).checkIn;
    CHECK(c.valid && c.mode == 1 && c.rows[0].place == 29 && c.rows[0].built && c.rows[0].available && c.rows[1].place == 8 && !c.rows[1].built && !c.rows[1].available);
    CHECK(c.rows[2].built && c.rows[2].occupant == 7 && !c.rows[2].available && c.rows[4].built && !c.rows[4].available);   // Penthouse: flag 0x15 clear
    CHECK(c.rows[0].infoOk && !c.rows[1].infoOk && c.rows[2].infoOk && !c.rows[4].infoOk);   // room info: built (an occupied room too), the Penthouse's flag
    r.W32(kGb + 0x4814 + 0, 1u << 21); CHECK(ReadGameState(r.View()).checkIn.rows[4].available && ReadGameState(r.View()).checkIn.rows[4].infoOk);
    r.W8(kS + 0x26, 0); r.W8(kS + 0x25, 45);
    c = ReadGameState(r.View()).checkIn; CHECK(c.valid && c.mode == 0 && c.guest == 45);
    r.W8(kS + 0x25, 46); CHECK(!ReadGameState(r.View()).checkIn.valid);
    r.W8(kS + 0x26, 2); CHECK(!ReadGameState(r.View()).checkIn.valid);
    r.W8(kS + 0x26, 1); r.W8(0x0214A7A0, 40); CHECK(!ReadGameState(r.View()).checkIn.valid);
    Ram w = hotel_ram(0x0214A804, 5); w.W8(kS + 0x26, 1); CHECK(!ReadGameState(w.View()).checkIn.valid);   // another overlay

    // Room info: Small Guest Room (29) occupied by Sim 45 until day 3 of a made-up date, furniture 557 -> 2
    Ram q = hotel_ram(0x0214A9BC, 12);
    for (u32 i = 0; i < 6; i++) q.W32(0x0211F9D4 + 4 * i, kGuestRooms[i]);
    const u32 room1 = kGb + 0x41F8 + 0x58 * 1;
    q.W32(room1 + 0x54, 2); q.W8(room1, 86); q.W8(room1 + 4, 114);
    const u32 p86 = 0x02122A00 + 0x12 * (86 + 0x3B), p114 = 0x02122A00 + 0x12 * (114 + 0x3B);
    q.W16(p86 + 0xE, 500); q.W16(p114 + 0xE, 57);
    q.W16(kS + 0x30, 29); q.W16(kS + 0x32, 45); q.W8(kS + 0x25, 1);
    q.W32(kGb + 0x68, 1);   // not Fortune: no x 1.05
    q.W32(kGb + 0x21DC + 0x2C * 45 + 0x20, 0x1000 + 86400 + 1000); q.W8(kGb + 0x21DC + 0x2C * 45 + 0x1A, 2);
    q.W32(kGb + 0x480C, 0x1000); q.W32(kGb + 0x47F4, 1); q.W32(kGb + 0x47F8, 31);
    RoomInfoState v = ReadGameState(q.View()).roomInfo;
    CHECK(v.valid && v.place == 29 && v.occupied && v.occupant == 45 && v.moodId == 2088 && v.checkOutKnown && v.days == 2 && v.month == 2 && v.day == 2);
    CHECK(v.baseValue == 400 && v.serviceCharge == 2 && v.scoreBonus == 12 && v.total == 414);   // the casino is built: score 6
    q.W32(kGb + 0x68, 0); CHECK(ReadGameState(q.View()).roomInfo.total == 434);
    for (u32 k = 0; k < 4; k++) q.W8(kS + 0x34 + k, 1);   // a hint box up: which one is not in RAM (the frame loop's, -1 here)
    v = ReadGameState(q.View()).roomInfo; CHECK(v.valid && v.hint && v.hintId == -1);
    for (u32 k = 0; k < 4; k++) q.W8(kS + 0x34 + k, 0);
    q.W8(kS + 0x25, 0); q.W16(kS + 0x32, 45);
    v = ReadGameState(q.View()).roomInfo; CHECK(v.valid && !v.occupied && !v.checkOutKnown && v.moodId == -1);
    q.W16(kS + 0x30, 8); q.W32(0x0211F9D4, 9); CHECK(!ReadGameState(q.View()).roomInfo.valid);   // not a guest room
    q.W32(0x0211F9D4, 8); q.W16(kS + 0x30, 33); CHECK(!ReadGameState(q.View()).roomInfo.valid);
    q.W16(kS + 0x30, 29); q.W16(kS + 0x32, 46); CHECK(!ReadGameState(q.View()).roomInfo.valid);
}
static void music_build_placement()
{
    Ram r = hotel_ram(0x0214ABA8, 5);
    for (u32 i = 0; i < 4; i++) { r.W8(0x0214ABBC + 4 * i, kSongCodes[i]); r.W16(0x0214ABBC + 4 * i + 2, 100 + i); }
    r.W32(kS + 0x6C, 3); r.W32(kS + 0x74, 3); r.W32(kS + 0x28, 0); r.W32(kS + 0x70, 2);
    r.W8(kS + 0x2C, 0); r.W8(kS + 0x2D, 1); r.W8(kS + 0x2E, 2);
    r.W32(0x0213E580, 0x021A1000); r.W32(0x021A1000 + 0xF4, 4); r.W8(kGb + 0x449C + 4, 3);
    MusicState m = ReadGameState(r.View()).music;
    CHECK(m.valid && m.list.size() == 3 && m.list[0].custom == 0 && m.list[0].nameId == -1 && m.list[1].nameId == 101 && m.playing == 2 && m.place == 4 && m.roomSong == 3);
    CHECK(m.list[0].customName.empty() && m.list[1].customName.empty());
    // A recorded song's name: the mixer's char[16] in G, up to its NUL, never past 16 bytes.
    for (u32 k = 0; k < 16; k++) r.W8(kGb + 0x46AC + k, k < 5 ? (u32)"Tune1"[k] : 0);
    CHECK(ReadGameState(r.View()).music.list[0].customName == "Tune1");
    for (u32 k = 0; k < 17; k++) r.W8(kGb + 0x46AC + k, 'A');
    CHECK(ReadGameState(r.View()).music.list[0].customName == std::string(16, 'A'));
    r.W32(kS + 0x70, 0xFF); CHECK(ReadGameState(r.View()).music.playing == -1);
    r.W32(kS + 0x70, 3); CHECK(!ReadGameState(r.View()).music.valid);
    r.W32(kS + 0x70, 2); r.W8(kS + 0x2E, 3); CHECK(!ReadGameState(r.View()).music.valid);   // the table's end marker
    r.W8(kS + 0x2E, 2); r.W32(kS + 0x6C, 28); CHECK(!ReadGameState(r.View()).music.valid);

    Ram b = hotel_ram(0x0214B1D0, 1);
    b.W32(kGb + 0x2F60, 1200); b.W32(kS + 0x40, 2); b.W32(kS + 0x4C, 4); b.W32(kS + 0x50, 15); b.W32(kS + 0x3C, 0); b.W32(kS + 0x48, 0); b.W32(kS + 0x44, 1);
    BuildState v = ReadGameState(b.View()).build;
    CHECK(v.valid && v.places.size() == 2 && v.places[0].place == 4 && v.places[0].price == 1500 && !v.places[0].affordable && v.places[1].affordable && v.places[1].parentNameId == 0x43 + 27 && v.chosen == -1 && v.money == 1200);
    b.W32(kS + 0x48, 1); CHECK(ReadGameState(b.View()).build.chosen == 1);
    b.W32(kS + 0x44, 2); CHECK(!ReadGameState(b.View()).build.valid);
    b.W32(kS + 0x44, 1); b.W32(kS + 0x3C, 1); CHECK(!ReadGameState(b.View()).build.valid);   // scroll past the end
    b.W32(kS + 0x3C, 0);
    b.W32(kS + 0x4C, 33); CHECK(!ReadGameState(b.View()).build.valid);
    b.W32(kS + 0x4C, 4); b.W32(kS + 0x40, 34); CHECK(!ReadGameState(b.View()).build.valid);
    b.W32(kS + 0x40, 2); b.W16(0x0211F9EC + 4 * 4, 0); CHECK(!ReadGameState(b.View()).build.valid);   // a listed room always has a permit
    b.W16(0x0211F9EC + 4 * 4, 1500); CHECK(ReadGameState(b.View()).build.valid);

    Ram p = hotel_ram(0x021351A8, -1);
    p.W32(kS + 8, 1); p.W32(kS + 0x24, 1); p.W32(kS + 0x5C, 1); p.W8(kS + 0x59, 0); p.W8(kS + 0x5A, 1); p.W32(kS + 0x6C, 3);
    const u32 rec = kGb + 0x41F8 + 0x58;
    p.W32(rec + 0x54, 2); p.W8(rec, 86); p.W8(rec + 1, 5); p.W8(rec + 2, 14); p.W8(rec + 3, 0); p.W8(rec + 4, 114); p.W8(rec + 5, 71); p.W8(rec + 6, 58); p.W8(rec + 7, 1);
    PlacementState pl = ReadGameState(p.View()).placement;
    CHECK(pl.valid && pl.mode == 1 && pl.room == 1 && pl.phase == 1 && pl.object == 1 && pl.rotation == 3 && pl.objects.size() == 2 && pl.objects[1].x == 71 && pl.objects[1].rotation == 1);
    p.W32(rec + 0x54, 15); CHECK(!ReadGameState(p.View()).placement.valid);
    p.W32(rec + 0x54, 2); p.W32(kS + 0x24, 6); CHECK(!ReadGameState(p.View()).placement.valid);
    Ram o = hotel_ram(0x021351A8, 3); o.W32(kS + 8, 1); CHECK(!ReadGameState(o.View()).placement.valid);   // an overlay is loaded: not this screen
}
static void catalogue_and_stat()
{
    Ram r = hotel_ram(0x0214C194, 2);
    put_item_row(r, 106, 1991); put_item_row(r, 132, 2276);
    // put_item_row names the row's item 463 + id; the rows hold id + 0x3B
    r.W32(kS + 0x260, 0x33); r.W8(kS + 0x26C, 0); r.W8(kS + 0x264, 2);
    r.W32(kS + 0x3C, 106 + 0x3B); r.W32(kS + 0x40, 132 + 0x3B);
    r.W32(kS + 0x254, 1); r.W32(kS + 0x258, 1); r.W32(kS + 0x268, 0); r.W8(kS + 0x25D, 1); r.W8(kS + 0x26D, 0); r.W8(kS + 0x26E, 1);
    r.W32(kGb + 0x215C, 1); r.W16(kGb + 0x28, 106);
    r.W32(kS + 0x214, kS + 0x400); r.W8(kS + 0x404, 1);
    CatalogueState c = ReadGameState(r.View()).catalogue;
    CHECK(c.valid && c.shop == 0x33 && !c.vendor && c.items.size() == 2 && c.items[0].id == 106 && c.items[0].nameId == 569 && c.selected == 1 && !c.sell && c.payEnabled && !c.upEnabled && c.downEnabled && c.ring);
    CHECK(c.owned.size() == 1 && c.owned[0].sellPrice == c.items[0].sellPrice && c.items[0].buyPrice == 2 * c.items[0].sellPrice && c.money == 2000);
    r.W32(kS + 0x258, 2); r.W32(kS + 0x254, 0); c = ReadGameState(r.View()).catalogue; CHECK(c.valid && c.sell && c.selected == 0);
    r.W32(kS + 0x254, 1); c = ReadGameState(r.View()).catalogue; CHECK(c.valid && c.selected == -1);   // past the owned list after a sale
    r.W32(kS + 0x258, 3); CHECK(!ReadGameState(r.View()).catalogue.valid);
    r.W32(kS + 0x258, 1); r.W32(kS + 0x40, 5 + 0x3B); CHECK(!ReadGameState(r.View()).catalogue.valid);   // the row names another item
    r.W32(kS + 0x40, 132 + 0x3B); r.W8(kS + 0x264, 13); CHECK(!ReadGameState(r.View()).catalogue.valid);
    r.W8(kS + 0x264, 2); r.W8(kS + 0x25D, 2); CHECK(!ReadGameState(r.View()).catalogue.valid);
    r.W8(kS + 0x25D, 1);
    // The keeper's record index: Sim record rows 0..56 (0x260 = 57 is past them).
    r.W32(kS + 0x260, 56); CHECK(ReadGameState(r.View()).catalogue.valid);
    r.W32(kS + 0x260, 57); CHECK(!ReadGameState(r.View()).catalogue.valid);
    // A vendor: layout 1, six fixed items.
    r.W32(kS + 0x260, 0x30); r.W8(kS + 0x26C, 1); r.W8(kS + 0x264, 6);
    for (u32 i = 0; i < 6; i++) r.W32(kS + 0x3C + 4 * i, (i % 2 ? 132 : 106) + 0x3B);
    c = ReadGameState(r.View()).catalogue;
    CHECK(c.valid && c.vendor && c.shop == 0x30 && c.items.size() == 6 && c.items[5].id == 132);
    r.W8(kS + 0x26C, 2); CHECK(!ReadGameState(r.View()).catalogue.valid);
    // A negative record price (s16, as the game reads it): never ~131,000.
    r.W8(kS + 0x26C, 1); r.W16(kRec + 0x12 * (132 + 0x3B) + 0xE, 0xFFF6);
    c = ReadGameState(r.View()).catalogue;
    CHECK(c.valid && c.items[1].sellPrice == -10 && c.items[1].buyPrice == -20);
    CHECK(ReadItemRow(r.View(), 132).price == -10);

    Ram s = hotel_ram(0x0214B5A4, 14);
    s.W32(kS + 0x38, 2); s.W32(kS + 0x3C, 0);
    s.W32(kGb + 0x68, 0); s.W32(kGb + 0x6C, 2); s.W32(kGb + 0x70, 29); s.W32(0x02121DD8 + 8, 30);
    for (u32 i = 0; i < 5; i++) s.W8(kGb + 0x5B + i, 1 + i);
    s.W32(kGb + 0x74, 22); s.W32(kGb + 0x78, 12); s.W32(kGb + 0x7C, 359); s.W32(kGb + 0x80, 2020);
    StatState v = ReadGameState(s.View()).stat;
    CHECK(v.valid && v.page == 2 && v.level == 2 && v.points == 29 && v.gaugeFrame == 19 && v.skills[4] == 5 && v.aliens == 22 && v.metal == 2020 && v.earned == 500 && v.spent == 300 && v.guestsStayed == 3);
    s.W32(kGb + 0x6C, 3); CHECK(ReadGameState(s.View()).stat.gaugeFrame == 20);
    s.W32(kS + 0x38, 5); CHECK(!ReadGameState(s.View()).stat.valid);
    s.W32(kS + 0x38, 4); s.W8(kGb + 0x5B, 11); CHECK(!ReadGameState(s.View()).stat.valid);
    s.W8(kGb + 0x5B, 1); s.W32(kGb + 0x68, 4); CHECK(!ReadGameState(s.View()).stat.valid);
}
static void hotel_random()
{
    std::mt19937 rng(23);
    struct Kind { u32 vt; int ov; };
    const Kind kinds[] = {{0x0214A804, 4}, {0x0214A9BC, 12}, {0x0214ABA8, 5}, {0x0214B1D0, 1}, {0x021351A8, -1}, {0x0214C194, 2}, {0x0214B5A4, 14}};
    for (int round = 0; round < 1400; round++)
    {
        const Kind k = kinds[round % 7];
        Ram r = hotel_ram(k.vt, k.ov);
        for (u32 i = 0; i < 6; i++) r.W32(0x0211F9D4 + 4 * i, kGuestRooms[i]);
        for (int i = 0; i < 300; i++)
        {
            const u32 areas[] = {kS, kS + 0x200, kGb + 0x41F8, kGb + 0x2BC0, kGb + 0x4400, kGb, kGb + 0x21DC, 0x0211F9EC, 0x0214ABBC, 0x0214A7A0};
            r.W8(areas[rng() % 10] + rng() % 0x300, (u32)rng());
        }
        const GameState g = ReadGameState(r.View());   // no crash, no overrun; lists stay capped
        CHECK(g.music.list.size() <= 27 && g.build.places.size() <= 33 && g.catalogue.items.size() <= 12 && g.catalogue.owned.size() <= 6 && g.placement.objects.size() <= 14);
    }
}

// ---- The minigames (milestone 6): made-up RAM with the research's layouts.
constexpr u32 kMg = 0x021670A4;   // G
static Ram mg_ram(u32 vt, int ov)
{
    Ram r = Make(vt, ov);
    r.W32(0x0213E434, kMg);
    return r;
}

static void minigame_screens()
{
    // Every minigame's (overlay, vtable) is its own screen kind; the arm9 ones ignore the overlay.
    struct { u32 vt; int ov; GScreen s; } t[] = {
        {0x0214EBC4, 0, GScreen::Autopsy}, {0x0214C124, 9, GScreen::Detector}, {0x02152E0C, 10, GScreen::MooGoo}, {0x02152EC8, 10, GScreen::MooGoo},
        {0x0214AFC4, 13, GScreen::Scratch}, {0x0214B8BC, 18, GScreen::Drencher}, {0x02134EC8, -1, GScreen::Crime},
        {0x02134F10, -1, GScreen::Mixer}, {0x02134FA0, -1, GScreen::MixerSave}, {0x02134D70, -1, GScreen::ArtGallery},
    };
    for (auto& c : t)
    {
        const GameState g = ReadGameState(mg_ram(c.vt, c.ov).View());
        CHECK(g.valid && g.screen == c.s);
        CHECK(std::string(GScreenName(c.s)) == GameStateName(g));
        // CONSOLE does not know them: the stick cursor's Unknown, except the Drencher (ItemHand, a cursor screen too).
        CHECK(ToScreenState(g) == (ScreenState{c.s == GScreen::Drencher ? Screen::ItemHand : Screen::Unknown}));
    }
    // The overlay's classes need their overlay; the arm9 classes do not care.
    CHECK(ReadGameState(mg_ram(0x0214EBC4, 1).View()).screen == GScreen::Unknown);
    CHECK(ReadGameState(mg_ram(0x02152E0C, 11).View()).screen == GScreen::Unknown);
    CHECK(ReadGameState(mg_ram(0x02134EC8, 15).View()).screen == GScreen::Crime);
    CHECK(GScreen::ItemHand == GScreen::Drencher && std::string(GScreenName(GScreen::ItemHand)) == "Drencher");
    // A part is read only on its own screen: the autopsy's bytes under the cards' vtable read nothing.
    Ram r = mg_ram(0x0214EBC4, 0);
    const GameState a = ReadGameState(r.View());
    CHECK(a.minigame.autopsy.valid && !a.minigame.scratch.valid && !a.minigame.mooGoo.valid && !a.minigame.art.valid && !a.minigame.mixer.valid);
    r.W32(kS, 0x0212F98C);
    const GameState c = ReadGameState(r.View());
    CHECK(c.screen == GScreen::Cards && !c.minigame.autopsy.valid && c.minigame == MinigameState{});
    CHECK(MinigameLine(c).empty());
}

static Ram autopsy_ram()
{
    Ram r = mg_ram(0x0214EBC4, 0);
    for (u32 i = 0; i < 3; i++) { const u32 o = kS + 0x28 + 0x10 * i; r.W16(o + 0xC, 20); r.W8(o + 0xE, i); }
    r.W8(kS + 0xCA0C, 208); r.W8(kS + 0xCA0D, 16);   // stale: neither "none"
    return r;
}
static void autopsy()
{
    Ram r = autopsy_ram();
    GameState g = ReadGameState(r.View());
    const AutopsyState& v = g.minigame.autopsy;
    CHECK(v.valid && v.phase == 0 && v.sub == 0 && v.tool == 0 && v.earnings == 0 && v.parasites.empty());
    CHECK(v.organs[0].kind == 0 && v.organs[2].kind == 2 && v.organs[1].damage == 20 && v.organs[2].present() && !v.handAlert && !v.tweezersAlert);
    CHECK(v.dragged == -1 && v.held == -1 && v.tearX == -1);   // the stale "none" bytes are not read outside their sub-phase
    // An organ freed (the hand flashes), one removed (kind 5, freed stays): not an alert.
    r.W8(kS + 0x28 + 0xF, 1);
    CHECK(ReadGameState(r.View()).minigame.autopsy.handAlert);
    r.W8(kS + 0x28 + 0xE, 5);
    CHECK(!ReadGameState(r.View()).minigame.autopsy.handAlert);
    // Parasites: a sprite with 20.12 positions (one off the screen on the left), the eating one flashes the tweezers.
    r.W32(kS + 0x58, 0x021A0000); r.W32(0x021A0000 + 0x50, (u32)(-22 << 12)); r.W32(0x021A0000 + 0x54, 85u << 12); r.W8(kS + 0x58 + 0x11, 2);
    r.W32(kS + 0x58 + 0x18 * 5, 0x021A0100); r.W32(0x021A0100 + 0x50, 140u << 12); r.W32(0x021A0100 + 0x54, 37u << 12); r.W8(kS + 0x58 + 0x18 * 5 + 0x11, 3); r.W8(kS + 0x58 + 0x18 * 5 + 0x14, 1);
    g = ReadGameState(r.View());
    CHECK(g.minigame.autopsy.parasites.size() == 2 && g.minigame.autopsy.tweezersAlert);
    CHECK(g.minigame.autopsy.parasites[0] == (AutopsyParasite{0, -22, 85, 2, false}) && g.minigame.autopsy.parasites[1] == (AutopsyParasite{5, 140, 37, -1, true}));
    CHECK(MinigameLine(g).find("parasites=0:-22:85:2,5:140:37:-1:eat") != std::string::npos);
    // The dragged organ and the held parasite count under their sub-phase.
    r.W8(kS + 0xC9F4, 1); r.W8(kS + 0xC9F5, 3); r.W8(kS + 0xCA0C, 1);
    CHECK(ReadGameState(r.View()).minigame.autopsy.dragged == 1);
    r.W8(kS + 0xCA0C, 3);
    CHECK(ReadGameState(r.View()).minigame.autopsy.dragged == -1);
    r.W8(kS + 0xC9F5, 6); r.W8(kS + 0xCA0D, 5);
    CHECK(ReadGameState(r.View()).minigame.autopsy.held == 5);
    r.W8(kS + 0xC9F4, 6); r.W8(kS + 0xC9F5, 0); r.W16(kS + 0xCA16, 125); r.W16(kS + 0xCA18, 100);
    g = ReadGameState(r.View());
    CHECK(g.minigame.autopsy.phase == 6 && g.minigame.autopsy.tearX == 125 && g.minigame.autopsy.tearY == 100);
    // Out-of-range fields: the part is invalid, the screen stays the autopsy's.
    for (auto [off, val, w] : {std::tuple<u32, u32, int>{0xC9F4, 7, 8}, {0xC9F5, 8, 8}, {0xC9F6, 2, 8}, {0xC9F7, 3, 8}, {0x28 + 0xC, 0xA01, 16}, {0x28 + 0xE, 6, 8}, {0x28 + 0xF, 2, 8}, {0xC9EC, 0xFFFFFFFF, 32}})
    {
        Ram q = r;
        if (w == 8) q.W8(kS + off, val); else if (w == 16) q.W16(kS + off, val); else q.W32(kS + off, val);
        g = ReadGameState(q.View());
        CHECK(g.screen == GScreen::Autopsy && !g.minigame.autopsy.valid);
    }
    { Ram q = r; q.W32(kS + 0x58 + 0x18 * 3, 0x021A0300); q.W32(0x021A0300 + 0x50, 5000u << 12); CHECK(!ReadGameState(q.View()).minigame.autopsy.valid); }
    { Ram q = r; q.W32(kS + 0x58 + 0x18 * 3, 0x02500000); CHECK(!ReadGameState(q.View()).minigame.autopsy.valid); }   // a sprite outside RAM
}

static void detector()
{
    Ram r = mg_ram(0x0214C124, 9);
    r.W32(kS + 0x9C, 3); r.W32(kS + 0xD0, 0x99480); r.W32(kS + 0xD8, 0x021A0000); r.W32(0x021A0000 + 0x90, 0x800);
    r.W32(kMg + 0x80, 2020); r.W32(kMg + 0x31C8, 0);
    GameState g = ReadGameState(r.View());
    CHECK(g.screen == GScreen::Detector && g.minigame.detector.valid);
    DetectorState v = g.minigame.detector;
    CHECK(v.state == 3 && v.charge == 0x99480 && !v.inRange && v.slider == 0x800 && v.mismatch == 0 && !v.card && v.findType == -1 && v.earnings == 2020 && v.findCount == 0);
    // In range: the sensed find's type is read (a stale pointer out of range is not).
    r.W8(kS + 0xD4, 1); r.W32(kS + 0x54, 128); r.W32(kS + 0x58, 0x021A0200); r.W8(0x021A0200 + 8, 7); r.W8(kS + 0x4E, 1); r.W32(kMg + 0x31C8, 12);
    v = ReadGameState(r.View()).minigame.detector;
    CHECK(v.valid && v.inRange && v.mismatch == 128 && v.findType == 7 && v.card && v.findCount == 12);
    r.W8(0x021A0200 + 8, 9);
    CHECK(ReadGameState(r.View()).minigame.detector.findType == -1);
    r.W8(kS + 0xD4, 0); r.W8(0x021A0200 + 8, 7);
    CHECK(ReadGameState(r.View()).minigame.detector.findType == -1);
    for (auto [off, val] : {std::pair<u32, u32>{0x9C, 6}, {0xD0, 0xFFFFFFF0}, {0xD0, 0x300000}, {0x54, 129}})
    {
        Ram q = r;
        q.W32(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::Detector && !ReadGameState(q.View()).minigame.detector.valid);
    }
    { Ram q = r; q.W32(0x021A0000 + 0x90, 0x1001); CHECK(!ReadGameState(q.View()).minigame.detector.valid); }
    { Ram q = r; q.W32(kS + 0xD8, 0); CHECK(!ReadGameState(q.View()).minigame.detector.valid); }
    { Ram q = r; q.W32(0x0213E434, 0); CHECK(!ReadGameState(q.View()).minigame.detector.valid); }
}

static void moogoo()
{
    // The intro is the arrow alone, valid on its own.
    GameState g = ReadGameState(mg_ram(0x02152EC8, 10).View());
    CHECK(g.screen == GScreen::MooGoo && g.minigame.mooGoo.valid && g.minigame.mooGoo.intro && MinigameLine(g) == "moogoo intro");
    Ram r = mg_ram(0x02152E0C, 10);
    r.W32(kS + 0x1C, 0xD); r.W32(kS + 0xC, 1); r.W32(kS + 0x18, 0xFFFFFFFF); r.W8(kS + 0x1E6, 0); r.W8(kS + 0x1E7, 1); r.W8(kS + 0x1E9, 1); r.W8(kS + 0x1EA, 1 | 4);
    r.W8(kS + 0x1E4, 19); r.W8(kS + 0x1E5, 54); r.W16(kS + 0x1F2, 0xFFF6);
    // Monkey 2 has two cards under it and bets 2/0/0 in 2 slots; monkey 4 is out.
    const u32 m2 = kS + 0x30 + 0x2C * 2;
    r.W8(m2 + 0x18, 20); r.W8(m2 + 0x19, 48); r.W8(m2 + 0x26, 2); r.W8(m2 + 0x23, 2); r.W8(m2 + 0x27, 2);
    r.W8(kS + 0x30 + 0x2C * 4 + 0x28, 1);
    // Players: 0 a human with a full hand, the third the AI with an empty slot.
    const u32 p0 = kS + 0x138, p2 = kS + 0x138 + 0x28 * 2;
    r.W32(p0 + 0x18, 1758); for (u32 i = 0; i < 5; i++) r.W8(p0 + 0x1C + i, 45 - i); r.W8(p0 + 0x21, 0x30); r.W8(p0 + 0x22, 2); r.W8(p0 + 0x23, 8);
    r.W32(p2 + 0x18, 326); for (u32 i = 0; i < 5; i++) r.W8(p2 + 0x1C + i, i == 2 ? 0xFF : 12 + i); r.W8(p2 + 0x23, 9 | 4);
    g = ReadGameState(r.View());
    const MooGooState& v = g.minigame.mooGoo;
    CHECK(v.valid && !v.intro && v.phase == 0xD && v.sub == 1 && v.timer == -1 && v.current == 0 && v.humans == 2 && v.round == 1);
    CHECK(v.hotSeat && !v.turnDone && v.betPlaced && !v.stale && !v.quit && !v.quitAsk && v.winnings == -10 && v.deckNext == 19 && v.deckSize == 54);
    CHECK(v.monkeys[2].count == 2 && v.monkeys[2].cards[0] == 20 && v.monkeys[2].cards[1] == 48 && v.monkeys[2].bets[0] == 2 && v.monkeys[2].slots == 2 && !v.monkeys[2].eliminated);
    CHECK(v.monkeys[4].eliminated && v.monkeys[0].count == 0);
    CHECK(v.players[0].nameId == 1758 && v.players[0].hand[0] == 45 && v.players[0].hand[4] == 41 && v.players[0].selected == 3 && v.players[0].score == 2 && !v.players[0].ai && v.players[0].bet == true);
    CHECK(v.players[2].hand[2] == kMooGooHandEmpty && v.players[2].ai && v.players[2].outOfCards && v.players[2].bet);
    // Before the deal the hands are zeros, not cards.
    r.W32(kS + 0x1C, 7);
    CHECK(ReadGameState(r.View()).minigame.mooGoo.players[0].hand[0] == kMooGooHandEmpty);
    r.W32(kS + 0x1C, 0xD);
    // Bounds: a card id, a bet, the count, the phase, the current player.
    for (auto [off, val, w] : {std::tuple<u32, u32, int>{0x1C, 0x18, 32}, {0xC, 10, 32}, {0x1E6, 3, 8}, {0x1E7, 3, 8}, {0x1E9, 4, 8}, {0x1E5, 55, 8}, {0x1E4, 60, 8}, {0x1F1, 2, 8},
                               {0x30 + 0x26, 9, 8}, {0x30 + 0x23, 5, 8}, {0x30 + 0x27, 5, 8}, {0x138 + 0x1C, 54, 8}, {0x138 + 0x21, 0x50, 8}, {0x138 + 0x18, 0x10000, 32}})
    {
        Ram q = r;
        if (w == 8) q.W8(kS + off, val); else q.W32(kS + off, val);
        g = ReadGameState(q.View());
        CHECK(g.screen == GScreen::MooGoo && !g.minigame.mooGoo.valid);
    }
    { Ram q = r; q.W8(m2 + 0x19, 54); CHECK(!ReadGameState(q.View()).minigame.mooGoo.valid); }
    // The quit box and a quit in progress.
    r.W8(kS + 0x1F1, 1);
    CHECK(ReadGameState(r.View()).minigame.mooGoo.quitAsk);
}

static void scratch()
{
    Ram r = mg_ram(0x0214AFC4, 13);
    r.W32(kS + 0xE0, 1); r.W32(kS + 0x10, 50); r.W32(kS + 0xC8, 4); r.W32(kS + 0xCC, 5); r.W32(kS + 0xD0, 1); r.W16(kS + 0xD8, 2); r.W8(kS + 0xDC, 1); r.W16(kS + 0xDE, 3); r.W8(kS + 0xE4, 1);
    for (u32 i = 0; i < 8; i++)
    {
        r.W32(kS + 0x24 + 0xC * i, i % 7); r.W32(kS + 0xA4 + 4 * i, 0x021A0000 + 0x500 * i); r.W32(0x021A0000 + 0x500 * i + 0x98, 1024);
    }
    r.W8(kS + 0x24 + 0xC * 1 + 9, 2); r.W32(0x021A0000 + 0x500 + 0x98, 339);   // box 1 revealed
    r.W32(0x021A0000 + 0x500 * 3 + 0x98, 700);                                  // box 3 started: 324 erased
    GameState g = ReadGameState(r.View());
    const ScratchState& v = g.minigame.scratch;
    CHECK(g.screen == GScreen::Scratch && v.valid && v.tier == 1 && v.prize == 50 && v.state == 1 && v.revealed == 2 && v.winA == 4 && v.winB == 5 && v.active == 3 && v.hint);
    CHECK(v.boxes[0].picture == 0 && !v.boxes[0].revealed && v.boxes[0].erased == 0 && v.boxes[1].revealed && v.boxes[1].erased == 685 && v.boxes[3].erased == 324 && v.boxes[7].picture == 0);
    r.W8(kS + 0xDC, 0);
    CHECK(ReadGameState(r.View()).minigame.scratch.active == -1);
    r.W8(kS + 0xDC, 1); r.W16(kS + 0xDE, 0xFFFF);
    CHECK(ReadGameState(r.View()).minigame.scratch.active == -1);
    // A popup (the hint, the result) is the popup part's: the screen stays Scratch.
    r.W8(kP + 0x8, 2);
    CHECK(ReadGameState(r.View()).screen == GScreen::Scratch && ReadGameState(r.View()).popup == 2);
    for (auto [off, val] : {std::pair<u32, u32>{0xE0, 3}, {0xC8, 7}, {0xD0, 0}, {0xD0, 4}, {0xD8, 9}, {0x24 + 0xC * 2, 7}, {0x24 + 0xC * 2 + 9, 3}})
    {
        Ram q = r;
        if (off == 0xD8) q.W16(kS + off, val); else if (off == 0x24 + 0xC * 2 + 9) q.W8(kS + off, val); else q.W32(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::Scratch && !ReadGameState(q.View()).minigame.scratch.valid);
    }
    { Ram q = r; q.W32(0x021A0000 + 0x500 * 6 + 0x98, 1025); CHECK(!ReadGameState(q.View()).minigame.scratch.valid); }
    { Ram q = r; q.W32(kS + 0xA4 + 4 * 6, 0); CHECK(!ReadGameState(q.View()).minigame.scratch.valid); }
}

static void drencher_and_crime()
{
    Ram r = mg_ram(0x0214B8BC, 18);
    r.W32(kS + 0x24, 90u << 12); r.W32(kS + 0x28, (u32)-1); r.W32(kS + 0x2C, 0); r.W32(kMg + 0x18, (65u << 12) + 1000); r.W32(kMg + 0x1C, (125u << 12));
    r.W32(kS + 0x70, 0x2229800);   // the byte at +0x70 is the mode, the rest of the word is not
    GameState g = ReadGameState(r.View());
    DrencherState v = g.minigame.drencher;
    CHECK(g.screen == GScreen::Drencher && v.valid && v.pump == 90 && v.target == -1 && v.targetType == -1 && !v.firing && !v.targetMode && v.water == 65 && v.pressure == 125);
    r.W32(kS + 0x28, 29); r.W32(kS + 0x2C, 0xB); r.W8(kS + 0x35, 1); r.W8(kS + 0x34, 1); r.W8(kS + 0x70, 1); r.W8(kS + 0x78, 1); r.W8(kS + 0x79, 1);
    v = ReadGameState(r.View()).minigame.drencher;
    CHECK(v.valid && v.target == 29 && v.targetType == 0xB && v.firing && v.pumping && v.targetMode && v.help && v.helpText);
    for (auto [off, val] : {std::pair<u32, u32>{0x24, 181u << 12}, {0x24, (u32)-1}, {0x28, 0x200}, {0x2C, 0x100}, {0x35, 2}})
    {
        Ram q = r;
        if (off == 0x35) q.W8(kS + off, val); else q.W32(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::Drencher && !ReadGameState(q.View()).minigame.drencher.valid);
    }
    { Ram q = r; q.W32(kMg + 0x1C, 130u << 12); CHECK(!ReadGameState(q.View()).minigame.drencher.valid); }
    { Ram q = r; q.W32(kMg + 0x18, (u32)-1); CHECK(!ReadGameState(q.View()).minigame.drencher.valid); }

    r = mg_ram(0x02134EC8, -1);
    r.W8(kS + 0x61, 0x10);   // unset before the first throw
    CrimeState c = ReadGameState(r.View()).minigame.crime;
    CHECK(c.valid && !c.help && c.phase == 0 && c.angle == 0 && !c.hit && !c.canScare);
    r.W8(kS + 0x4C, 1); r.W8(kS + 0x52, 1); r.W8(kS + 0x58, 2); r.W16(kS + 0x5A, 82); r.W8(kS + 0x5C, 1); r.W8(kS + 0x5D, 1); r.W8(kS + 0x5E, 1); r.W8(kS + 0x5F, 1); r.W8(kS + 0x60, 1); r.W8(kS + 0x61, 1);
    c = ReadGameState(r.View()).minigame.crime;
    CHECK(c.valid && c.help && c.dragging && c.phase == 2 && c.angle == 82 && c.canScare && c.armed && c.canSubdue && c.scarePressed && c.subduePressed && c.hit);
    r.W16(kS + 0x5A, (u32)(-90 & 0xFFFF));
    CHECK(ReadGameState(r.View()).minigame.crime.angle == -90);
    for (auto [off, val] : {std::pair<u32, u32>{0x4C, 2}, {0x58, 4}, {0x5C, 2}, {0x60, 3}})
    {
        Ram q = r;
        q.W8(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::Crime && !ReadGameState(q.View()).minigame.crime.valid);
    }
    r.W16(kS + 0x5A, 91);
    CHECK(!ReadGameState(r.View()).minigame.crime.valid);
}

static void mixer_and_art()
{
    Ram r = mg_ram(0x02134F10, -1);
    r.W8(kS + 0xA55, 2); r.W8(kS + 0x8E2, 0xFE); r.W8(kS + 0x8E3, 0); r.W8(kS + 0x8E4, 0xD); r.W8(kS + 0x8E5, 12); r.W8(kS + 0x8E6, 0xFF); r.W8(kS + 0x8E7, 5); r.W8(kS + 0x8E9, 1);
    r.W8(kS + 0x8EA, 25); r.W8(kS + 0x8EB, 3); r.W8(kS + 0x8EC, 110); r.W8(kS + 0x8ED, 1); r.W8(kS + 0xA5C, 8); r.W16(kS + 0xA4C, 4); r.W16(kS + 0xA4E, 24); r.W8(kS + 0xA52, 0x34); r.W8(kS + 0xA6C, 2);
    r.W8(kS + 0xA6B, 6); for (u32 i = 0; i < 6; i++) r.W8(kS + 0xA63 + i, i);
    GameState g = ReadGameState(r.View());
    const MixerState& v = g.minigame.mixer;
    CHECK(g.screen == GScreen::Mixer && v.valid && v.phase == 2 && v.tracks == 0xFE && v.keys[0] == 0 && v.keys[1] == -1 && v.keys[2] == 12 && v.octave == -1 && v.instrument == 5 && v.sustain);
    CHECK(v.sliderA == 25 && v.sliderB == 3 && v.tempo == 110 && v.variation == 1 && v.song == 8 && v.controlEvents == 4 && v.noteEvents == 24 && v.prep && v.previewing && v.micFull);
    CHECK(v.unlocked.size() == 6 && v.unlocked[5] == 5);
    for (auto [off, val] : {std::pair<u32, u32>{0xA55, 0xB}, {0x8E7, 4}, {0x8E6, 2}, {0x8EA, 27}, {0x8E3, 0xE}, {0xA5C, 9}, {0xA6B, 9}, {0xA63, 8}})
    {
        Ram q = r;
        q.W8(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::Mixer && !ReadGameState(q.View()).minigame.mixer.valid);
    }
    r.W32(kS + 8, 3);
    CHECK(!ReadGameState(r.View()).minigame.mixer.valid);

    // The save menu: valid with the mixer's context; names from G by mode, byte 15 the used flag.
    r = mg_ram(0x02134FA0, -1);
    r.W32(kS + 0xC, 0x021A0000); r.W8(kS + 0x28, 0xFF);
    r.W8(kS + 0x88, 'H'); r.W8(kS + 0x89, 'i');
    r.W8(kMg + 0x46AC + 16 + 15, 1); r.W8(kMg + 0x46AC + 16, 'A'); r.W8(kMg + 0x46AC + 17, 'b');
    g = ReadGameState(r.View());
    const MixerSaveState& s = g.minigame.mixerSave;
    CHECK(g.screen == GScreen::MixerSave && s.valid && s.mode == 0 && s.slot == -1 && s.name == "Hi" && !s.used[0] && s.used[1] && s.slotName[1] == "Ab" && !s.used[2]);
    r.W32(kS + 8, 1); r.W8(kS + 0x28, 2); r.W8(kMg + 0x46DC + 64 * 2 + 15, 1); r.W8(kMg + 0x46DC + 64 * 2, 'R');
    const MixerSaveState s1 = ReadGameState(r.View()).minigame.mixerSave;
    CHECK(s1.valid && s1.mode == 1 && s1.slot == 2 && !s1.used[1] && s1.used[2] && s1.slotName[2] == "R");
    { Ram q = r; q.W32(kS + 0xC, 0); CHECK(!ReadGameState(q.View()).minigame.mixerSave.valid); }   // no context: the black screen
    { Ram q = r; q.W8(kS + 0x28, 3); CHECK(!ReadGameState(q.View()).minigame.mixerSave.valid); }
    { Ram q = r; q.W32(kS + 8, 2); CHECK(!ReadGameState(q.View()).minigame.mixerSave.valid); }

    // Art Gallery: the 48 x 32 working canvas, the tool state, the stylus's cell only while it is on the canvas.
    r = mg_ram(0x02134D70, -1);
    for (u32 i = 0; i < 48 * 32; i++) r.W8(kS + 0x650 + i, 1);
    r.W8(kS + 0x650 + 17 * 48 + 21, 4);
    r.W32(kS + 0x2C, 1); r.W32(kS + 0x30, 7); r.W32(kS + 0x34, 2); r.W32(kS + 0x38, 5); r.W32(kS + 0x3C, 1); r.W32(kS + 0x40, 2);
    r.W8(kS + 0xC68, 52); r.W32(kS + 0xC60, 0xFFFFFFFF); r.W32(kS + 0xC64, 77);   // garbage while off the canvas
    g = ReadGameState(r.View());
    const ArtState& a = g.minigame.art;
    CHECK(g.screen == GScreen::ArtGallery && a.valid && a.stroke == 1 && a.button == 7 && a.tool == 2 && a.swatch == 5 && a.page == 1 && a.brush == 2 && !a.inCanvas && a.cellRow == -1);
    CHECK(a.cells.size() == 48 * 32 && a.cells[17 * 48 + 21] == 4 && a.cells[0] == 1);
    r.W8(kS + 0xC68, 1); r.W32(kS + 0xC60, 17); r.W32(kS + 0xC64, 21);
    CHECK(ReadGameState(r.View()).minigame.art.inCanvas && ReadGameState(r.View()).minigame.art.cellRow == 17 && ReadGameState(r.View()).minigame.art.cellCol == 21);
    r.W32(kS + 0xC64, 48);
    CHECK(ReadGameState(r.View()).minigame.art.valid && !ReadGameState(r.View()).minigame.art.inCanvas);
    for (auto [off, val] : {std::pair<u32, u32>{0x2C, 3}, {0x30, 10}, {0x34, 6}, {0x38, 15}, {0x3C, 16}, {0x40, 3}})
    {
        Ram q = r;
        q.W32(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::ArtGallery && !ReadGameState(q.View()).minigame.art.valid);
    }
    { Ram q = r; q.W8(kS + 0x650 + 100, 16); CHECK(!ReadGameState(q.View()).minigame.art.valid); }
}

// Random bytes in every minigame object: the readers stay in bounds, the lists capped, and a part
// that reads is valid in itself (no part invalid with its fields half filled).
// Keelhaulin' Cards and the vacuum (milestone 1's activities, framed in HD by port/minigame.*).
static void cards_and_vacuum()
{
    Ram r = mg_ram(0x0212F98C, -1);
    r.W16(kS + 0x4C, 60); r.W16(kS + 0x4E, 39);
    for (u32 i = 0; i < 9; i++) r.W32(kS + 0xC0 + 0x14 * i, kS + 0x250 + i);       // nine cards in hand
    for (u32 i = 0; i < 10; i += 2) r.W32(kS + 0x188 + 0x14 * i, kS + 0x290 + i);  // five for the opponent, every other place
    r.W32(kS + 0x2CC, (u32)-10); r.W32(kS + 0x2D0, 30); r.W32(kS + 0x2E4, 10);
    r.W8(kS + 0x308, 'J'); r.W8(kS + 0x309, 'u'); r.W8(kS + 0x30A, 'n');
    GameState g = ReadGameState(r.View());
    CardsState k = g.minigame.cards;
    CHECK(g.screen == GScreen::Cards && k.valid && k.deck == 39 && k.total == 60 && k.hand == 9 && k.opponent == 5 && k.score == -10 && k.opponentScore == 30 &&
          k.treasure == 10 && k.name == "Jun" && !k.box && !k.message && !k.quitAsk);
    CHECK(MinigameLine(g) == "cards deck=39/60 hand=9 opponent=5 score=-10,30 treasure=10 box=0 message=0 quit=0 name=\"Jun\"");
    r.W8(kS + 0x306, 1); r.W8(kS + 0x307, 1);
    k = ReadGameState(r.View()).minigame.cards;
    CHECK(k.valid && k.box && k.quitAsk && !k.message);
    r.W8(kS + 0x307, 0); r.W8(kS + 0x2F7, 1);
    k = ReadGameState(r.View()).minigame.cards;
    CHECK(k.valid && k.box && k.message && !k.quitAsk);
    // A name that is not text: none (the board's "Player" instead), the rest still reads.
    { Ram q = r; q.W8(kS + 0x308, 0x07); k = ReadGameState(q.View()).minigame.cards; CHECK(k.valid && k.name.empty()); }
    // Garbage: a deck past the total, no total, a card pointer out of the object, flags past 1, scores out of range.
    for (auto [off, val, w] : {std::tuple<u32, u32, int>{0x4E, 61, 2}, {0x4C, 0, 2}, {0x4C, 61, 2}, {0xC0, 0x02000000, 4}, {0x188 + 0x14 * 9, kS + 0x300, 4},
                               {0x306, 2, 1}, {0x307, 2, 1}, {0x2F7, 2, 1}, {0x2CC, 10000, 4}, {0x2E4, (u32)-1, 4}})
    {
        Ram q = r;
        if (w == 1) q.W8(kS + off, val); else if (w == 2) q.W16(kS + off, val); else q.W32(kS + off, val);
        CHECK(ReadGameState(q.View()).screen == GScreen::Cards && !ReadGameState(q.View()).minigame.cards.valid);
    }

    r = mg_ram(0x0214C658, 17);
    r.W32(kMg + 0x7C, 120);
    GameState v = ReadGameState(r.View());
    CHECK(v.screen == GScreen::Vacuum && v.minigame.vacuum.valid && !v.minigame.vacuum.clogged && v.minigame.vacuum.earnings == 120);
    r.W8(kS + 0x158, 1);
    v = ReadGameState(r.View());
    CHECK(v.minigame.vacuum.clogged && MinigameLine(v) == "vacuum clogged=1 earnings=120");
    { Ram q = r; q.W8(kS + 0x158, 2); CHECK(!ReadGameState(q.View()).minigame.vacuum.valid); }
    { Ram q = r; q.W32(kMg + 0x7C, (u32)-5); CHECK(!ReadGameState(q.View()).minigame.vacuum.valid); }
    // Each part only on its own screen.
    CHECK(!ReadGameState(mg_ram(0x0214C658, 17).View()).minigame.cards.valid && !ReadGameState(mg_ram(0x0212F98C, -1).View()).minigame.vacuum.valid);
}

static void minigame_random()
{
    std::mt19937 rng(77);
    const struct { u32 vt; int ov; } k[] = {{0x0214EBC4, 0}, {0x0214C124, 9}, {0x02152E0C, 10}, {0x02152EC8, 10}, {0x0214AFC4, 13}, {0x0214B8BC, 18}, {0x02134EC8, -1}, {0x02134F10, -1}, {0x02134FA0, -1}, {0x02134D70, -1}, {0x0212F98C, -1}, {0x0214C658, 17}};
    for (int round = 0; round < 720; round++)
    {
        Ram r = mg_ram(k[round % 12].vt, k[round % 12].ov);
        for (int i = 0; i < 300; i++) r.W8(kS + rng() % 0xCA40, (u32)rng() & (i % 3 ? 0x3F : 0xFF));
        for (int i = 0; i < 100; i++) r.W8(kMg + 0x30 + rng() % 0x4700, (u32)rng());
        const GameState g = ReadGameState(r.View());
        const MinigameState& m = g.minigame;
        CHECK(m.autopsy.parasites.size() <= 8 && m.mixer.unlocked.size() <= 8 && (m.art.cells.empty() || m.art.cells.size() == 48 * 32));
        CHECK(!m.autopsy.valid || g.screen == GScreen::Autopsy);
        CHECK(!m.detector.valid || g.screen == GScreen::Detector);
        CHECK(!m.mooGoo.valid || g.screen == GScreen::MooGoo);
        CHECK(!m.scratch.valid || g.screen == GScreen::Scratch);
        CHECK(!m.drencher.valid || g.screen == GScreen::Drencher);
        CHECK(!m.crime.valid || g.screen == GScreen::Crime);
        CHECK(!m.mixer.valid || g.screen == GScreen::Mixer);
        CHECK(!m.mixerSave.valid || g.screen == GScreen::MixerSave);
        CHECK(!m.art.valid || g.screen == GScreen::ArtGallery);
        CHECK(!m.cards.valid || (g.screen == GScreen::Cards && m.cards.deck <= m.cards.total && m.cards.hand <= 10 && m.cards.opponent <= 10));
        CHECK(!m.vacuum.valid || g.screen == GScreen::Vacuum);
        (void)MinigameLine(g);
    }
}

int main()
{
    hotel_block();
    registry_and_room();
    music_build_placement();
    catalogue_and_stat();
    hotel_random();
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
    minigame_screens();
    autopsy();
    detector();
    moogoo();
    scratch();
    drencher_and_crime();
    mixer_and_art();
    cards_and_vacuum();
    minigame_random();
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
