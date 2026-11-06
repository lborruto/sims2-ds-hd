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
    u32 panelTab, pocketsObj, pocketsSel, relObj, relSel;   // from S (pocketsSel from *(S+pocketsObj), relSel from *(S+relObj))
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
    // The menus' lists (§1-§4 of the milestone-3 research). Tab objects hang off S; each is checked by its vtable.
    struct Menus
    {
        u32 tabInteract, tabGoals, bigPortrait;             // from S (the pockets and relationships tabs: pocketsObj, relObj)
        u32 vtInteract, vtPockets, vtGoals, vtRel;          // the tab objects' vtables
        // Interact, from T = *(S + tabInteract)
        u32 itTarget, itNext, itPrev, itMode, itHighlight, itSelectors, selDisabled;
        u32 entActor, entKind, actIndex, actType, actIface;  // entry E, its actor, the actor's fields
        u32 ifCount, ifCodes, maxActions, maxCode;           // IInteractable
        u32 actionLabel, unknownName, typeSim, typeObject, maxSimIndex;   // label 1845 + code, name 321, actor types, Sim record rows
        u32 greyCodes[4];
        // Pockets (Pk) and Goals (TG)
        u32 pkSlot, pkRing, ringShown, ringX, ringX0, ringStep;
        u32 tgCount, tgLines, tgEntries, tgEntryStride, tgSelected, tgFirst, maxGoals, maxLines, maxEntryLines;
        u32 gEntId, gEntFirst, gEntLines, gEntMission;
        u32 missionBase, missionGoals, missionStage, missionDone, missionText;    // from G; strings 1627 + id
        u32 hotelBase, hotelStride, hotelCount, hotelFlag, hotelFlag2, hotelText, hotelSim, hotelPlace, hotelPlaceBase;
        u32 giverTable, giverStage, giverStages, giverMaxDone;   // absolute table, 0x114 per stage, 8 per goal done
        u32 flags, footerFlag, scoreBits, scoreWeights, scoreWeightStep, scoreWeightOff;   // G+flags bitset, game flag, hotel score
        u32 footerImprove, footerAmazing, footerScore;
        // Relationships (TR)
        u32 trCount, trFirst, trSel, trSimId, trList, maxRel, descBase, moodBase, levelBase, levelPerMood, maxMood;
        // Phone, from S
        u32 phRow, phScroll, phShown, phContacts, phContactSim, maxPhonePage, maxPhoneRow;
        u32 inboxBase, inboxStride, inboxN, inbArgs, inbArgNum, inbText, inbSender, inbPresent, inbRead, ringtone, maxRingtone;
        u32 phoneMenu;                                       // absolute: 4 u32 string ids
        // Give Gift, from S
        u32 giftActor, giftSlot, giftButtons, btnRefused, recActor;
        // Note (overlay 11)
        u32 noteVt, noteOverlay, noteText, noteNameIds[3];
        u32 stringMax;                                       // string ids stay below the table's count (3127 in the EU ROM)
    } menus;
};
constexpr EUTable kEU = {
    0x02000000, 0x02139FA4,
    0xDAC, 0xE34, 0x8, 0xE42,
    0x64, 0x38, 0xE0, 0x40, 0x24,
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
    {
        0x34, 0x3C, 0xD0,
        0x021321C4, 0x0213232C, 0x02132480, 0x02132580,
        0x300, 0x308, 0x30C, 0x314, 0x318, 0x6C, 0x94,
        0, 4, 0xA, 0xC, 0x130,
        8, 0x10, 4, 106,
        1845, 321, 2, 0xB, 57,
        {2, 0x3A, 0x3B, 0x3C},
        0x74, 0x34, 4, 0x50, 109, 29,
        0x50C, 0x510, 0x18, 16, 0x514, 0x518, 44, 132, 8,
        0, 4, 8, 0xC,
        0x2F78, 34, 0, 0x24, 1627,
        0x2BE8, 0x4C, 10, 0x35, 0x44, 0x48, 0x30, 0x32, 0x43,
        0x0211FB6C, 0x114, 4, 33,
        0x4814, 17, 0x4410, 0x0211F9EC, 4, 2,
        1841, 1843, 1842,
        0x1C, 0x20, 0x24, 0x28, 0x2C, 24, 389, 184, 192, 7, 6,
        0x89, 0x8A, 0x56, 0x86C, 0x870, 10, 127,
        0x303C, 0x24, 10, 0, 0x10, 0x14, 0x18, 0x1A, 0x1B, 0x31C5, 6,
        0x0211E99C,
        0x24, 0x4C, 0x34, 0x94, 0xA,
        0x0214A040, 11, 0x2C, {0x4A8, 0x509, 0x657},
        3127,
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

// ---- The menus' lists. A heap object: its pointer must lie in RAM and its vtable must be the class's.
bool Obj(const Reader& m, u32 a, u32 vt, u32 size, u32& obj)
{
    u32 v;
    return m.R32(a, obj) && obj != 0 && m.In(obj, size) && m.R32(obj, v) && v == vt;
}
// A pointer field to a plain struct (no vtable check).
bool Ptr(const Reader& m, u32 a, u32 size, u32& p) { return m.R32(a, p) && p != 0 && m.In(p, size); }

// A target entry E = {actor, kind}: the record table row names it.
bool ReadTarget(const Reader& m, u32 entry, TargetRef& t)
{
    const auto& q = kEU.menus; const auto& tw = kEU.town;
    u32 actor, kind, idx, type;
    if (!Ptr(m, entry + q.entActor, 0x134, actor) || !m.R32(entry + q.entKind, kind) || !m.R16(actor + q.actIndex, idx) || !m.R16(actor + q.actType, type)) return false;
    RecordRow row;
    if (type == q.typeSim)
    {
        if (idx >= q.maxSimIndex) return false;
        row = ReadRow(m, tw.simRow0 + idx);
        if (!row.valid || row.nameId != (int)(kEU.simNameBase + idx)) return false;
        t.kind = TargetRef::Sim;
    }
    else if (type == q.typeObject)
    {
        if (idx >= tw.maxItems) return false;
        row = ReadRow(m, tw.itemRow0 + idx);
        if (!row.valid || row.nameId != (int)(tw.itemName + idx)) return false;
        t.kind = TargetRef::Object;
    }
    else return false;
    t.index = (int)idx;
    t.unknown = t.kind == TargetRef::Sim && kind == 2;
    t.nameId = t.unknown ? (int)q.unknownName : row.nameId;
    t.portraitRes = row.iconRes;
    return true;
}

InteractState ReadInteract(const Reader& m, u32 S)
{
    const auto& q = kEU.menus;
    InteractState v;
    u32 T, E, mode, hl;
    if (!Obj(m, S + q.tabInteract, q.vtInteract, 0x320, T)) return v;
    if (!m.R32(T + q.itMode, mode) || mode > 2 || !m.R32(T + q.itHighlight, hl)) return v;
    u32 big;
    if (!m.R16(S + q.bigPortrait, big)) return v;
    if (!m.R32(T + q.itTarget, E)) return v;
    int n = 0;
    if (E != 0)
    {
        u32 actor, iface, count;
        if (!m.In(E, 8) || !ReadTarget(m, E, v.target) || !m.R32(E + q.entActor, actor) || !m.R32(actor + q.actIface, iface)) return InteractState{};
        if (iface != 0)   // an actor with no IInteractable has no actions
        {
            if (!m.In(iface, 0x20) || !m.R32(iface + q.ifCount, count) || count > q.maxActions) return InteractState{};
            n = (int)count;
            for (int i = 0; i < n; i++)
            {
                u32 code, sel, dis;
                if (!m.R8(iface + q.ifCodes + i, code) || code > q.maxCode) return InteractState{};
                InteractAction a;
                a.code = (int)code; a.labelId = (int)(q.actionLabel + code);
                for (u32 c : q.greyCodes) if (c == code) a.greyed = true;
                // The row's own selector button says it too (+0x94): it wins when it reads.
                if (m.R32(T + q.itSelectors + 4 * i, sel) && sel != 0 && m.In(sel, 0x96) && m.R8(sel + q.selDisabled, dis) && dis <= 1) a.greyed = dis == 1;
                v.actions.push_back(a);
            }
        }
    }
    for (int w = 0; w < 2 && E != 0; w++)   // with no target the strip's pointers are stale (seen: 0x90000)
    {
        u32 e;
        if (!m.R32(T + (w ? q.itPrev : q.itNext), e)) return InteractState{};
        if (e != 0 && (!m.In(e, 8) || !ReadTarget(m, e, w ? v.prev : v.next))) return InteractState{};
    }
    if (E != 0 && ((int)hl < -1 || (int)hl >= (n > 0 ? n : 1))) return InteractState{};
    v.padMode = (int)mode;
    v.highlighted = E != 0 ? (int)hl : -1;   // raw: the lit row is drawn only in pad mode (padMode 0), which the reader reports apart
    v.bigPortraitRes = big == 0xFFFF ? -1 : (int)big;
    v.valid = true;
    return v;
}

PocketsSel ReadPockets(const Reader& m, u32 S)
{
    const auto& q = kEU.menus; const auto& t = kEU.town;
    PocketsSel v;
    u32 Pk, G, count, ring, shown, x;
    if (!Obj(m, S + kEU.pocketsObj, q.vtPockets, 0xE4, Pk) || !m.R32(kEU.gameBlock, G) || !m.R32(G + t.invCount, count) || count > t.maxSlots) return v;
    const int slot = m.S32(Pk + q.pkSlot), key = m.S32(Pk + kEU.pocketsSel);
    if (slot < -1 || slot >= (int)count || key < -1 || key >= (int)t.keyN) return v;
    v.counted = -1;
    if (slot == -1 && key == -1 && m.R32(Pk + q.pkRing, ring) && ring != 0 && m.R32(ring + q.ringShown, shown) && (shown & 1) && m.R32(ring + q.ringX, x))
    {
        const int px = (int)(x >> 12) - (int)q.ringX0;
        if (px < 0 || px % (int)q.ringStep != 0 || px / (int)q.ringStep >= (int)t.countedN) return v;
        v.counted = px / (int)q.ringStep;
    }
    v.valid = true; v.slot = slot; v.key = key;
    return v;
}

GoalsState ReadGoals(const Reader& m, u32 S)
{
    const auto& q = kEU.menus; const auto& t = kEU.town;
    GoalsState v;
    u32 TG, G, n, lines;
    if (!Obj(m, S + q.tabGoals, q.vtGoals, 0x638, TG) || !m.R32(kEU.gameBlock, G)) return v;
    if (!m.R32(TG + q.tgCount, n) || n > q.maxGoals || !m.R32(TG + q.tgLines, lines) || lines > q.maxLines) return v;
    const int sel = m.S32(TG + q.tgSelected), first = m.S32(TG + q.tgFirst);
    if (sel < -1 || sel >= (int)n || first < 0 || first > (int)lines) return v;
    for (u32 i = 0; i < n; i++)
    {
        const u32 e = TG + q.tgEntries + q.tgEntryStride * i;
        u32 id, fl, nl, mis;
        if (!m.R32(e + q.gEntId, id) || !m.R32(e + q.gEntFirst, fl) || !m.R32(e + q.gEntLines, nl) || !m.R8(e + q.gEntMission, mis) || mis > 1) return GoalsState{};
        if (nl == 0 || nl > q.maxEntryLines || fl > lines || fl + nl > lines) return GoalsState{};
        GoalEntry g;
        g.mission = mis == 1; g.id = (int)id; g.firstLine = (int)fl; g.nLines = (int)nl;
        if (g.mission)
        {
            if (id >= q.missionGoals || q.missionText + id >= q.stringMax) return GoalsState{};
            g.textId = (int)(q.missionText + id);
        }
        else
        {
            if (id >= q.hotelCount) return GoalsState{};
            const u32 R = G + q.hotelBase + q.hotelStride * id;
            u32 text, sim, place;
            if (!m.R32(R + q.hotelText, text) || text >= q.stringMax || !m.R16(R + q.hotelSim, sim) || sim >= t.maxSims || !m.R16(R + q.hotelPlace, place) || q.hotelPlaceBase + place >= q.stringMax)
                return GoalsState{};
            g.textId = (int)text; g.nameId = (int)(kEU.simNameBase + sim); g.placeId = (int)(q.hotelPlaceBase + place);
        }
        v.entries.push_back(g);
    }
    // The giver's portrait: the mission block's stage and goals done pick the NPC (medium confidence: -1 when it does not read).
    u32 stage, done, giver;
    if (m.R8(G + q.missionBase + q.missionStage, stage) && stage < q.giverStages && m.R8(G + q.missionBase + q.missionDone, done) && done <= q.giverMaxDone &&
        m.R32(q.giverTable + q.giverStage * stage + 8 * done, giver) && giver >= t.simRow0 && giver < t.itemRow0)
    {
        RecordRow row = ReadRow(m, giver);
        if (row.valid) v.portraitRes = row.iconRes;
    }
    // The footer: game flag 17; the hotel score is the weights of the set bits of a u32 bitset.
    u32 flags, bits;
    if (!m.R32(G + q.flags + 4 * (q.footerFlag >> 5), flags) || !m.R32(G + q.scoreBits, bits)) return GoalsState{};
    if ((flags >> (q.footerFlag & 31)) & 1)
    {
        int score = 0;
        u32 bits2;   // 33 flags: bit 32 is the next word's bit 0
        if (!m.R32(G + q.scoreBits + 4, bits2)) return GoalsState{};
        for (u32 k = 0; k < 33; k++)
        {
            u32 w;
            if (!(k < 32 ? (bits >> k) & 1 : bits2 & 1)) continue;
            if (!m.R8(q.scoreWeights + q.scoreWeightStep * k + q.scoreWeightOff, w)) return GoalsState{};
            score += (int)w;
        }
        if (score > 100) return GoalsState{};
        v.footer = true; v.score = score;
        v.footerId = (int)(score == 100 ? q.footerAmazing : q.footerImprove);
        v.scoreId = (int)q.footerScore;
    }
    v.lines = (int)lines; v.selected = sel; v.first = first;
    v.valid = true;
    return v;
}

RelState ReadRelations(const Reader& m, u32 S)
{
    const auto& q = kEU.menus; const auto& t = kEU.town;
    RelState v;
    u32 TR, G, n;
    if (!Obj(m, S + kEU.relObj, q.vtRel, 0xB4, TR) || !m.R32(kEU.gameBlock, G) || !m.R32(TR + q.trCount, n) || n > q.maxRel) return v;
    const int sel = m.S32(TR + q.trSel), first = m.S32(TR + q.trFirst);
    if (sel < -1 || sel >= (int)n || first < 0 || first > (int)(n ? n - 1 : 0)) return v;
    const u32 base = G + kEU.simRecords;
    for (u32 i = 0; i < n; i++)
    {
        u32 p;
        if (!m.R32(TR + q.trList + 4 * i, p) || p < base || (p - base) % kEU.simRecordSize != 0) return RelState{};
        const u32 k = (p - base) / kEU.simRecordSize;
        const SimRecord r = ReadSim(m, (int)k);
        if (k >= q.maxRel || !r.valid || !r.met) return RelState{};
        const RecordRow row = ReadRow(m, t.simRow0 + (u32)r.simId);
        if (!row.valid) return RelState{};
        RelEntry e;
        e.id = r.simId; e.nameId = (int)(kEU.simNameBase + r.simId); e.descId = (int)(q.descBase + r.simId);
        e.moodId = (int)(q.moodBase + r.mood); e.levelId = (int)(q.levelBase + q.levelPerMood * r.mood + r.level);
        e.portraitRes = row.iconRes; e.secret = r.secret;
        if (e.descId >= (int)q.stringMax || e.levelId >= (int)q.stringMax) return RelState{};
        v.sims.push_back(e);
    }
    if (sel >= 0)
    {   // the game keeps the selected Sim's id beside the index
        u32 id;
        if (!m.R32(TR + q.trSimId, id) || (int)id != v.sims[(size_t)sel].id) return RelState{};
    }
    v.first = first; v.selected = sel;
    v.valid = true;
    return v;
}

PhoneState ReadPhone(const Reader& m, u32 S)
{
    const auto& q = kEU.menus; const auto& t = kEU.town;
    PhoneState v;
    u32 G, page, shown, ring, contacts, csim;
    if (!m.R32(kEU.gameBlock, G) || !m.R8(S + kEU.phonePage, page) || page > q.maxPhonePage) return v;
    const int row = m.S8(S + q.phRow), scroll = m.S8(S + q.phScroll);
    if (row < 0 || row > (int)q.maxPhoneRow || scroll < 0 || scroll > (int)q.maxPhoneRow) return v;
    if (!m.R8(S + q.phShown, shown) || !m.R8(G + q.ringtone, ring) || ring > q.maxRingtone || !m.R8(S + q.phContacts, contacts) || !m.R32(S + q.phContactSim, csim)) return v;
    // These two fields hold stale values off their pages: checked and kept only on page 3 (slot, -1 after a delete) and page 7 (rows, Sim index or 0x3F).
    if (page == 3 && shown != 0xFF && shown >= q.inboxN) return v;
    if (page == 7 && (contacts > 64 || (csim >= t.maxSims && csim != 0x3F))) return v;
    for (u32 i = 0; i < q.inboxN; i++)
    {
        const u32 e = G + q.inboxBase + q.inboxStride * i;
        u32 present, read, text, sender;
        if (!m.R8(e + q.inbPresent, present) || present > 1) return PhoneState{};
        if (!present) continue;
        if (!m.R8(e + q.inbRead, read) || read > 1 || !m.R32(e + q.inbText, text) || text >= q.stringMax || !m.R16(e + q.inbSender, sender) || sender >= t.maxSims) return PhoneState{};
        InboxMsg b;
        b.slot = (int)i; b.textId = (int)text; b.sender = (int)sender; b.senderNameId = (int)(kEU.simNameBase + sender); b.read = read == 1;
        for (int a = 0; a < 4; a++)
        {
            u32 arg, num;
            if (!m.R32(e + q.inbArgs + 4 * a, arg) || !m.R8(e + q.inbArgNum + a, num)) return PhoneState{};
            b.args[a] = arg; b.argNum[a] = (int)num;
        }
        v.inbox.push_back(b);
    }
    for (int i = 0; i < 4; i++)
    {
        u32 id;
        if (!m.R32(q.phoneMenu + 4 * i, id) || id >= q.stringMax) return PhoneState{};
        v.menuIds[i] = (int)id;
    }
    v.page = (int)page; v.row = row; v.scroll = scroll;
    v.shownSlot = page == 3 && shown != 0xFF ? (int)shown : -1;
    v.ringtone = (int)ring;
    v.contacts = page == 7 ? (int)contacts : 0; v.contactSim = page == 7 ? (int)csim : -1;
    v.valid = true;
    return v;
}

GiftState ReadGift(const Reader& m, u32 S)
{
    const auto& q = kEU.menus; const auto& t = kEU.town;
    GiftState v;
    u32 G, actor, k, count, id;
    if (!m.R32(kEU.gameBlock, G) || !Ptr(m, S + q.giftActor, 0x14, actor) || !m.R16(actor + q.recActor, k) || k >= t.maxSims) return v;
    if (!m.R32(G + t.invCount, count) || count > t.maxSlots) return v;
    const int slot = m.S32(S + q.giftSlot);
    if (slot < -1 || slot >= (int)count) return v;
    if (!m.R16(G + kEU.simRecords + kEU.simRecordSize * k + kEU.simRecordName, id) || id >= t.maxSims) return v;
    for (u32 i = 0; i < count; i++)
    {
        u32 b, r;
        if (!m.R32(S + q.giftButtons + 4 * i, b)) return GiftState{};
        if (b != 0 && m.R8(b + q.btnRefused, r) && r <= 1) v.refused[i] = r == 1;
        else if (b != 0) return GiftState{};
    }
    v.recipient = (int)k; v.recipientNameId = (int)(kEU.simNameBase + id); v.slot = slot;
    v.valid = true;
    return v;
}

NoteState ReadNote(const Reader& m, u32 S)
{
    const auto& q = kEU.menus;
    NoteState v;
    u32 text;
    if (!m.R16(S + q.noteText, text) || text == 0 || text >= q.stringMax) return v;
    v.valid = true; v.textId = (int)text;
    for (u32 id : q.noteNameIds) if (id == text) v.playerName = true;
    return v;
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
    // The note (overlay 11) has no GScreen of its own: its vtable and overlay say it.
    if (vt == kEU.menus.noteVt && (int)ovRaw == (int)kEU.menus.noteOverlay) g.town.note = ReadNote(m, S);
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
        if (g.tab == 1) { u32 k; if (m.R32(S + kEU.pocketsObj, k)) g.selection = m.S32(k + kEU.pocketsSel); g.town.pockets = ReadPockets(m, S); }
        else if (g.tab == 3)
        {
            u32 k;
            if (Obj(m, S + kEU.relObj, kEU.menus.vtRel, 0xB4, k)) g.selection = m.S32(k + kEU.relSel);
            g.town.relations = ReadRelations(m, S);
        }
        else if (g.tab == 0) g.town.interact = ReadInteract(m, S);
        else if (g.tab == 2) g.town.goals = ReadGoals(m, S);
        break;
    }
    case GScreen::Phone: g.selection = m.S8(S + kEU.phoneSel); g.page = m.S8(S + kEU.phonePage); g.town.phone = ReadPhone(m, S); break;
    case GScreen::Gift: g.town.gift = ReadGift(m, S); break;
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
