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
    struct Town
    {
        u32 recTable, recSize, simRow0, itemRow0;           // rows of 0x12 bytes: Sim k = row 2 + k, item id = row 0x3B + id
        u32 recTiles, recDesc, recPal, recName, recIcon, recPrice;   // offsets in a row
        u32 maxSims, maxItems;                              // bounds on k / item id
        u32 simFlags, simMood, simLevel;                    // Sim record (from G + simRecords + k * simRecordSize)
        u32 invCount, invIds, invCounted, keyBits;          // from G: s32 count, u16[6], u8[4], u32 bit array
        u32 maxSlots, countedN, keyN;
        u32 itemName, itemDesc, countedName, countedDesc, keyName, keyDesc;   // string ids: base + index
        u32 optMusic, optSfx;                               // from G, 20.12
        u32 popKind, popParam, popText, popTitle, popPressed, popPressing;   // from P
        u32 popTextMax, popTitleMax;
        u32 saveUsed, saveCurrent, saveHeader, saveHeaderSize;              // absolute: u8[3], s32, headers
        u32 saveMoney, saveTime, saveName, saveNameMax, savePlace;
    } town;
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
    {
        0x02122A00, 0x12, 2, 0x3B,
        0, 2, 4, 8, 0xA, 0xE,
        46, 0x200,
        0x14, 0x1A, 0x1B,
        0x215C, 0x28, 0x24, 0,
        6, 4, 3,
        463, 822, 1154, 845, 1968, 1972,
        0x2F70, 0x2F74,
        0x10, 0x14, 0x34, 0x434, 0xD88, 0xD8C,
        0x400, 0x100,
        0x0213D848, 0x0213D84C, 0x0213D7B8, 0x30,
        0x18, 0x14, 0x1C, 16, 0x2C,
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

namespace
{
RecordRow ReadRow(const Reader& m, u32 row)
{
    const auto& t = kEU.town;
    RecordRow r;
    const u32 a = t.recTable + t.recSize * row;
    u32 tiles, desc, pal, name, icon, price;
    if (!m.R16(a + t.recTiles, tiles) || !m.R16(a + t.recDesc, desc) || !m.R16(a + t.recPal, pal) || !m.R16(a + t.recName, name) ||
        !m.R16(a + t.recIcon, icon) || !m.R16(a + t.recPrice, price))
        return r;
    r.valid = true;
    r.tilesRes = (int)tiles; r.descRes = (int)desc; r.palRes = (int)pal;
    r.nameId = (int)name; r.iconRes = (int)icon; r.price = (int)price;
    return r;
}

SimRecord ReadSim(const Reader& m, int k)
{
    const auto& t = kEU.town;
    SimRecord s;
    u32 G, id, flags, mood, level;
    if (k < 0 || (u32)k >= t.maxSims || !m.R32(kEU.gameBlock, G)) return s;
    const u32 R = G + kEU.simRecords + kEU.simRecordSize * (u32)k;
    if (!m.R16(R + kEU.simRecordName, id) || !m.R32(R + t.simFlags, flags) || !m.R8(R + t.simMood, mood) || !m.R8(R + t.simLevel, level)) return s;
    if (mood > 6 || level > 6 || id >= t.maxSims) return s;
    s.valid = true;
    s.simId = (int)id; s.mood = (int)mood; s.level = (int)level;
    s.met = flags & 1; s.secret = flags & 2;
    return s;
}

// A NUL-terminated byte string of at most max - 1 bytes at a; false when it does not end in max or leaves RAM.
bool ReadCString(const Reader& m, u32 a, u32 max, std::string& out)
{
    out.clear();
    for (u32 i = 0; i < max; i++)
    {
        u32 ch;
        if (!m.R8(a + i, ch)) return false;
        if (ch == 0) return true;
        out += (char)ch;
    }
    return false;
}

InventoryState ReadInventory(const Reader& m)
{
    const auto& t = kEU.town;
    InventoryState v;
    u32 G, n, keys;
    if (!m.R32(kEU.gameBlock, G) || !m.R32(G + t.invCount, n) || n > t.maxSlots || !m.R32(G + t.keyBits, keys)) return v;
    for (u32 i = 0; i < n; i++)
    {
        u32 id;
        if (!m.R16(G + t.invIds + 2 * i, id) || id >= t.maxItems) return InventoryState{};
        RecordRow row = ReadRow(m, t.itemRow0 + id);
        if (!row.valid || row.nameId != (int)(t.itemName + id)) return InventoryState{};   // the row names its own item
        v.slots.push_back({(int)id, row.nameId, (int)(t.itemDesc + id), row.descRes, 1});
    }
    for (u32 i = 0; i < t.countedN; i++)
    {
        u32 c;
        if (!m.R8(G + t.invCounted + i, c)) return InventoryState{};
        v.counted[i] = {(int)i, (int)(t.countedName + i), (int)(t.countedDesc + i), -1, (int)c};
    }
    for (u32 k = 0; k < t.keyN; k++)
        v.keys[k] = {(int)k, (int)(t.keyName + k), (int)(t.keyDesc + k), -1, (int)((keys >> (k + 1)) & 1)};
    v.valid = true;
    return v;
}

OptionsState ReadOptions(const Reader& m)
{
    const auto& t = kEU.town;
    OptionsState o;
    u32 G, mu, sf;
    if (!m.R32(kEU.gameBlock, G) || !m.R32(G + t.optMusic, mu) || !m.R32(G + t.optSfx, sf) || mu > 0x1000 || sf > 0x1000) return o;
    o.valid = true; o.music = (int)mu; o.sfx = (int)sf;
    return o;
}

PopupState ReadPopup(const Reader& m, u32 P)
{
    const auto& t = kEU.town;
    PopupState p;
    u32 st, kind, pressed, pressing;
    if (!m.R8(P + kEU.popup, st) || st < 1 || st > 3 || !m.R32(P + t.popKind, kind) || (kind != 0 && kind != 2 && kind != 3)) return p;
    if (!m.R32(P + t.popPressed, pressed) || !m.R8(P + t.popPressing, pressing) || pressing > 1) return p;
    if (!ReadCString(m, P + t.popText, t.popTextMax, p.text) || !ReadCString(m, P + t.popTitle, t.popTitleMax, p.title)) return PopupState{};
    p.valid = true;
    p.state = (int)st; p.kind = (int)kind;
    p.pressed = (int)pressed; p.pressing = pressing != 0;
    return p;
}

SaveSlots ReadSaves(const Reader& m)
{
    const auto& t = kEU.town;
    SaveSlots s;
    u32 cur;
    if (!m.R32(t.saveCurrent, cur) || (int)cur < -1 || (int)cur >= kSaveSlotCount) return s;
    for (int i = 0; i < kSaveSlotCount; i++)
    {
        u32 used;
        if (!m.R8(t.saveUsed + i, used) || used > 1) return SaveSlots{};
        s.used[i] = s.slot[i].used = used == 1;
        if (!s.used[i]) continue;
        const u32 h = t.saveHeader + t.saveHeaderSize * (u32)i;
        u32 money, time, place;
        if (!m.R32(h + t.saveMoney, money) || !m.R32(h + t.saveTime, time) || !m.R8(h + t.savePlace, place)) return SaveSlots{};
        s.slot[i].money = (int)money; s.slot[i].time = time; s.slot[i].place = (int)place;
        std::string name;
        bool ok = ReadCString(m, h + t.saveName, t.saveNameMax, name);
        for (char ch : name) if (ch < 0x20 || ch > 0x7E) ok = false;
        s.slot[i].simName = ok ? name : std::string();
    }
    s.current = (int)cur;
    s.valid = true;
    return s;
}
}

RecordRow ReadSimRow(RamView ram, int k)
{
    if (!ram.ram || k < 0 || (u32)k >= kEU.town.maxSims) return {};
    return ReadRow(Reader{ram}, kEU.town.simRow0 + (u32)k);
}
RecordRow ReadItemRow(RamView ram, int id)
{
    if (!ram.ram || id < 0 || (u32)id >= kEU.town.maxItems) return {};
    return ReadRow(Reader{ram}, kEU.town.itemRow0 + (u32)id);
}
SimRecord ReadSimRecord(RamView ram, int k) { return ram.ram ? ReadSim(Reader{ram}, k) : SimRecord{}; }

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
    g.town.inventory = ReadInventory(m);
    g.town.options = ReadOptions(m);
    g.town.saves = ReadSaves(m);
    g.town.popup = ReadPopup(m, P);
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
