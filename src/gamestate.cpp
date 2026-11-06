#include "gamestate.h"

#include <cstdio>

namespace sims2wide
{
namespace
{
struct VtScreen { u32 vt; GScreen screen; };
struct OvScreen { int ov; u32 vt; GScreen screen; };

// Every address of the EU game (ASJP). P is the manager, S the bottom screen object.
struct EUTable
{
    u32 ramBase, manager;                                   // P = u32 at `manager`
    u32 bottomObj, overlay, popup, slotState;               // offsets from P
    u32 panelTab, pocketsObj, pocketsSel, relSel;           // from S (pocketsSel from *(S+pocketsObj))
    u32 phoneSel, phonePage, dlgPage, dlgSpeech;
    u32 catSel, catScroll, catList, cardsSel;
    u32 gameBlock;                                          // G = u32 at `gameBlock` (0x021670A4 on every state)
    u32 money, hour, minute, sanCur, sanMax, sanTier, simName;   // offsets from G (money s32, the clock u32, sanity 20.12 fixed)
    u32 simRecords, simRecordSize, simRecordName, simNameBase;   // Sim records from G; name id = simNameBase + u16 at +simRecordName
    u32 speechTable, speechCount;                           // 8-byte entries {u32 pages ptr, u8 count}, 216 of them
    u32 dlgOneOff, dlgOneOffSpeaker;                        // from S: u8 flag (1 = one-off), u8 speaker (dlgPage is the u16 1-based counter)
    u32 dlgMaxPages, dlgSpeakers;                           // bounds: pages per speech, Sim records
    u32 simNameMax;                                         // name field bytes incl. the NUL (assumed)
    VtScreen plain[5];
    OvScreen overlays[8];
};
constexpr EUTable kEU = {
    0x02000000, 0x02139FA4,
    0xDAC, 0xE34, 0x8, 0xE42,
    0x64, 0x38, 0xE0, 0xD0,
    0x89, 0x88, 0x36, 0x34,
    0x254, 0x268, 0x258, 0x2E0,
    0x0213E434,
    0x2F60, 0x4800, 0x4804, 0x2164, 0x2168, 0x216C, 0x53,
    0x21DC, 0x2C, 0x18, 323,
    0x0211F16C, 216,
    0x41, 0x3C,
    16, 46,
    8,
    {
        {0x0213203C, GScreen::Panel}, {0x02131F08, GScreen::Phone}, {0x02135004, GScreen::Social},
        {0x0212F98C, GScreen::Cards}, {0x02131960, GScreen::FrontEnd},
    },
    {
        {15, 0x0214AA18, GScreen::Dialogue}, {6, 0x0214ABA4, GScreen::Gift},
        {2, 0x0214C194, GScreen::Catalogue}, {1, 0x0214B1D0, GScreen::Build},
        {17, 0x0214C658, GScreen::Vacuum},   {3, 0x021509B0, GScreen::CreateASim},
        {8, 0x0214B1C4, GScreen::SaveSlots}, {18, 0x0214B8BC, GScreen::ItemHand},
    },
};
const char* const kTabs[] = { "Interact", "Pockets", "Goals", "Relationships", "Options" };

struct Reader
{
    RamView r;
    bool In(u32 a, u32 n) const
    {
        if (a < kEU.ramBase) return false;
        u32 o = a - kEU.ramBase;
        return o <= r.mask && n - 1 <= r.mask - o;
    }
    bool R8(u32 a, u32& v) const { if (!In(a, 1)) return false; v = r.ram[a - kEU.ramBase]; return true; }
    bool R16(u32 a, u32& v) const { if (!In(a, 2)) return false; const u8* p = r.ram + (a - kEU.ramBase); v = p[0] | p[1] << 8; return true; }
    bool R32(u32 a, u32& v) const
    {
        if (!In(a, 4)) return false;
        const u8* p = r.ram + (a - kEU.ramBase);
        v = p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
        return true;
    }
    // -1 when unreadable.
    int S8(u32 a) const { u32 v; return R8(a, v) ? (int)v : -1; }
    int S16(u32 a) const { u32 v; return R16(a, v) ? (int)v : -1; }
    int S32(u32 a) const { u32 v; return R32(a, v) ? (int)v : -1; }
};

constexpr int kSpeakerNone = 0x3F, kSpeakerPlayer = 0x40;

HudState ReadHud(const Reader& m)
{
    HudState h;
    u32 G, money, hour, minute, cur, max, tier;
    if (!m.R32(kEU.gameBlock, G)) return h;
    if (!m.R32(G + kEU.money, money) || !m.R32(G + kEU.hour, hour) || !m.R32(G + kEU.minute, minute) || !m.R32(G + kEU.sanCur, cur) ||
        !m.R32(G + kEU.sanMax, max) || !m.R32(G + kEU.sanTier, tier))
        return h;
    if (hour > 23 || minute > 59 || tier > 3 || max == 0 || cur > max || max > (999u << 12)) return h;
    // The name field is assumed 8 bytes including the NUL (research: layout, not checked against
    // the CAS limit): printable ASCII then a NUL within it. Anything else blanks the name only.
    std::string name;
    bool terminated = false;
    for (u32 i = 0; i < kEU.simNameMax && !terminated; i++)
    {
        u32 ch;
        if (!m.R8(G + kEU.simName + i, ch) || (ch != 0 && (ch < 0x20 || ch > 0x7E))) { name.clear(); break; }
        if (ch == 0) terminated = true; else name += (char)ch;
    }
    if (!terminated) name.clear();
    h.valid = true;
    h.money = (int)money;
    h.hour = (int)hour; h.minute = (int)minute;
    h.sanity = (int)(cur >> 12); h.sanityMax = (int)(max >> 12);
    h.tier = (int)tier;
    h.simName = name;
    return h;
}

DialogueState ReadDialogue(const Reader& m, u32 S)
{
    DialogueState d;
    u32 G, speech, counter, oneOff, speaker = kSpeakerNone, sid;
    if (!m.R32(kEU.gameBlock, G) || !m.R16(S + kEU.dlgSpeech, speech) || !m.R16(S + kEU.dlgPage, counter) ||
        !m.R8(S + kEU.dlgOneOff, oneOff))
        return d;
    if (oneOff == 1)
    {   // a game-built page: S+0x34 is the string id, the speaker S+0x3C
        if (speech == 0 || !m.R8(S + kEU.dlgOneOffSpeaker, speaker)) return d;
        sid = speech;
        d.pages = 1; d.page = 1;
    }
    else
    {
        u32 entry = kEU.speechTable + 8 * speech, pages, count, pe;
        if (speech >= kEU.speechCount || !m.R32(entry, pages) || !m.R8(entry + 4, count)) return d;
        if (count == 0 || count > kEU.dlgMaxPages || counter < 1 || counter > count) return d;   // counter 0: no page drawn yet
        pe = pages + 4 * (counter - 1);
        if (!m.R16(pe, sid) || !m.R8(pe + 2, speaker)) return d;
        d.pages = (int)count; d.page = (int)counter;
    }
    d.speech = (int)speech;
    d.stringId = (int)sid;
    d.speaker = (int)speaker;
    d.last = d.page == d.pages;
    if (speaker < kEU.dlgSpeakers)
    {
        u32 nm;
        if (!m.R16(G + kEU.simRecords + kEU.simRecordSize * speaker + kEU.simRecordName, nm)) return DialogueState{};
        d.speakerNameId = (int)(kEU.simNameBase + nm);
    }
    else if (speaker != kSpeakerNone && speaker != kSpeakerPlayer) return DialogueState{};
    d.valid = true;
    return d;
}
}

std::vector<int> SpeechStringIds(RamView ram)
{
    const Reader m {ram};
    std::vector<int> ids;
    for (u32 speech = 0; speech < kEU.speechCount; speech++)
    {
        u32 entry = kEU.speechTable + 8 * speech, pages, count, sid;
        if (!m.R32(entry, pages) || !m.R8(entry + 4, count)) return {};
        if (pages == 0 || count == 0 || count > kEU.dlgMaxPages) continue; // a game-built one-off (entry 67)
        for (u32 p = 0; p < count; p++)
            if (m.R16(pages + 4 * p, sid)) ids.push_back((int)sid);
    }
    return ids;
}

GameState ReadGameState(RamView ram)
{
    GameState g;
    if (!ram.ram) return g;
    Reader m{ram};
    u32 P, S, vt, ovRaw, pop, slot = 0;
    if (!m.R32(kEU.manager, P) || !m.R32(P + kEU.bottomObj, S)) return g;
    if (S == 0)
    {
        g.between = true;
        return g;
    }
    if (!m.R32(S, vt) || !m.R32(P + kEU.overlay, ovRaw) || !m.R8(P + kEU.popup, pop)) return g;
    g.valid = true;
    g.hud = ReadHud(m);
    g.vtable = vt;
    g.overlay = (int)ovRaw;   // 0xFFFFFFFF is -1
    g.popup = (u8)pop;
    // Settled: the bottom slot is in place and no popup is sliding. A popup opening (1) reads
    // Popup about 13 frames before its box is drawn, and closing (3) still draws it: a tap then
    // lands on the panel underneath. 0xFF is the front end's (no popup there).
    g.settled = m.R8(P + kEU.slotState, slot) && slot == 3 && (pop == 0 || pop == 2 || pop == 0xFF);

    for (const auto& e : kEU.plain) if (e.vt == vt) { g.screen = e.screen; break; }
    if (g.screen == GScreen::Unknown)
        for (const auto& e : kEU.overlays) if (e.vt == vt && e.ov == g.overlay) { g.screen = e.screen; break; }

    switch (g.screen)
    {
    case GScreen::Panel:
    {
        int t = m.S32(S + kEU.panelTab);
        g.tab = (t >= 0 && t < 5) ? t : -1;
        if (pop >= 1 && pop <= 3) { g.screen = GScreen::Popup; break; }
        if (g.tab == 1) { u32 k; if (m.R32(S + kEU.pocketsObj, k)) g.selection = m.S32(k + kEU.pocketsSel); }
        else if (g.tab == 3) g.selection = m.S16(S + kEU.relSel);
        break;
    }
    case GScreen::Phone: g.selection = m.S8(S + kEU.phoneSel); g.page = m.S8(S + kEU.phonePage); break;
    case GScreen::Dialogue: g.page = m.S16(S + kEU.dlgPage); g.speech = m.S16(S + kEU.dlgSpeech); g.dialogue = ReadDialogue(m, S); break;
    case GScreen::Catalogue: g.selection = m.S32(S + kEU.catSel); g.scroll = m.S32(S + kEU.catScroll); g.list = m.S32(S + kEU.catList); break;
    case GScreen::Cards: g.selection = m.S32(S + kEU.cardsSel); break;
    default: break;
    }
    return g;
}

const char* GScreenName(GScreen s)
{
    switch (s)
    {
    case GScreen::FrontEnd: return "FrontEnd";
    case GScreen::CreateASim: return "CreateASim";
    case GScreen::Panel: return "Panel";
    case GScreen::Popup: return "Popup";
    case GScreen::Phone: return "Phone";
    case GScreen::Dialogue: return "Dialogue";
    case GScreen::Gift: return "Gift";
    case GScreen::Catalogue: return "Catalogue";
    case GScreen::Build: return "Build";
    case GScreen::Vacuum: return "Vacuum";
    case GScreen::Social: return "Social";
    case GScreen::Cards: return "Cards";
    case GScreen::SaveSlots: return "SaveSlots";
    case GScreen::ItemHand: return "ItemHand";
    default: return "Unknown";
    }
}

std::string GameStateName(const GameState& g)
{
    if (g.screen == GScreen::Unknown)
    {
        char b[64];
        snprintf(b, sizeof b, "Unknown(ov=%d,vt=0x%08X)", g.overlay, g.vtable);
        return b;
    }
    if (g.screen == GScreen::Panel)
        return std::string("Panel/") + (g.tab >= 0 && g.tab < 5 ? kTabs[g.tab] : "?");
    return GScreenName(g.screen);
}

ScreenState ToScreenState(const GameState& g)
{
    if (!g.valid) return {Screen::Unknown};
    switch (g.screen)
    {
    case GScreen::Panel:
        if (g.tab < 0 || g.tab > 4) return {Screen::Unknown};
        return {Screen::Panel, (Tab)((int)Tab::Interact + g.tab)};
    case GScreen::Popup: return {g.tab == 4 ? Screen::Unknown : Screen::Popup}; // 4: Options > Quit
    case GScreen::Phone: return {Screen::Phone};
    case GScreen::Dialogue: return {Screen::Dialogue};
    case GScreen::Gift: return {Screen::Gift};
    case GScreen::Catalogue: return {Screen::Catalogue};
    case GScreen::Build: return {Screen::Build};
    case GScreen::Vacuum: return {Screen::Vacuum};
    case GScreen::Social: return {Screen::Social};
    case GScreen::Cards: return {Screen::Cards};
    case GScreen::FrontEnd: return {Screen::FrontEnd};
    case GScreen::CreateASim: return {Screen::CreateASim};
    case GScreen::SaveSlots: return {Screen::SaveSlots};
    case GScreen::ItemHand: return {Screen::ItemHand};
    case GScreen::Unknown: break;
    }
    return {Screen::Unknown};
}

bool PixelsAgree(const GameState& g, ScreenState pixels)
{
    const ScreenState r = ToScreenState(g);
    if (r == pixels) return true;
    if (g.screen == GScreen::Popup && pixels.screen == Screen::Popup) return true; // Options > Quit
    switch (r.screen)
    {
    case Screen::FrontEnd: case Screen::CreateASim: return pixels.screen == Screen::None;
    case Screen::SaveSlots: case Screen::ItemHand: return pixels.screen == Screen::Unknown;
    default: return false;
    }
}

ScreenState ConsoleScreenOf(const GameState& g, ScreenState pixels, bool hud, bool black)
{
    if (!g.valid) return g.between && !hud && black ? ScreenState {Screen::Loading} : pixels;
    ScreenState s = ToScreenState(g);
    if (s.screen == Screen::Unknown && !hud) s = {black ? Screen::Loading : Screen::None};
    return s;
}
}
