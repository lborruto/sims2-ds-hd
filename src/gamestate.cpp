#include "gamestate.h"
#include "romdata.h"

#include <algorithm>
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
    OvScreen overlays[9];
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
        // The verbs whose label holds an amount (@1), as the game's own label methods set it
        // (0x0208B708, 0x02093588, 0x020960E8, 0x02097554): Buy (2 x the object's record price,
        // its row's +0xE, signed), Payoff 1500, Bail 200, Pay (the object's entry in the three
        // {u8 record, u8, s16 amount} at G + payTable; none: no amount, the game prints stale text).
        u32 ifActor, codeBuy, codePayoff, codeBail, codePay, payoffAmount, bailAmount, payTable, payEntries;
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
    // Milestone 3's popups' boxes and the phone's pages (research m3-town §3.4, §4.1).
    struct
    {
        u32 popBoxY, popBoxW, popBoxTiles;                       // from P
        u32 phMonth, phDay;                                      // from G
        u32 phEnd8, phEnd1, phEnd6, phNext3, phEnd9, phMaidCost; // absolute statics
        u32 phMaidText, phCaller, phPrank, phPrankText, phIncoming;   // from S
        u32 guests, guestStride, guestSim, guestN, simState, simPlace, guestState, awayPlace, noSim;
        u32 tones, toneStride, toneNamed, toneN;                 // from G
    } x;
    // Milestone 4: the hotel and the shops (research m4-hotel-shops §1-§7).
    struct
    {
        // The hotel block, from G
        u32 builtBits, unlockedBits;                         // u32[2] bitsets, bit = place id
        u32 roomTable, roomStride, roomPermit, roomWeight, roomParent;   // absolute table of {u16 permit, u8 weight, u8 parent place}
        u32 guestRooms, guestRoomN;                          // absolute: u32 place ids of the furniture records
        u32 slotDays, simCheckOut;                           // slotDays: from a guest slot (kEU.x.guests) the u8 days booked; simCheckOut: from a Sim record, u32 seconds
        u32 guestsStayed, earned, spent;                     // from G, s32
        u32 songs, songNone, customSongs, customStride, customN;   // from G: u8 per place, char[16] names
        u32 placePtr, placeOff;                              // absolute pointer to the world, the place id u32 in it
        u32 roomBlock, roomRec, roomCount, roomItems, roomItemStride, maxRoomItems;   // from G: furniture records (count at +0x54, {id, x, y, rot} each)
        u32 clock, noCheckOut, daySeconds, aspiration;       // from G (clock u32 seconds); aspiration u32 (0 Fortune)
        u32 penthouse, penthouseFlag, penthouseSims[3];      // place id, game flag (registry gate), the Sims of mission stages 1..3
        u32 baseValues[5][2], scoreFactors[5][2];            // {place, value}: room base value, bonus factor / 4096
        u32 serviceMul, serviceShift, fortuneMul, concierge, moodBase, maxMood;
        // Registry (overlay 4) and Room info (overlay 12), from S
        u32 vtRegistry, ovRegistry, regGuest, regMode, regRooms, regRoomN;   // regRooms: absolute u8[5] place ids
        u32 vtRoomInfo, ovRoomInfo, riPlace, riOccupant, riOccupied, riHint;
        // Music (overlay 5)
        u32 vtMusic, ovMusic, muScroll, muList, muCount, muPlaying, muRows, songTable, songStride, songN, songEnd;
        // Build (overlay 1; vtable in overlays[]) and Placement (module 0x14)
        u32 bdScroll, bdCount, bdPage, bdChosen, bdPlaces, bdMax, bdRows;
        u32 vtPlacement, plMode, plRoom, plPhase, plSlot, plObject, plRotation, plMax, plRoomMax;
        // Catalogue (overlay 2; vtable in overlays[])
        u32 caShop, caLayout, caCount, caList, caSel, caMode, caScroll, caPay, caUp, caDown, caRing, caMaxItems, caPriceMul, caRowMin;
        // Stat panel (overlay 14)
        u32 vtStat, ovStat, stPage, stTransition, stLevel, stPoints, stSkills, stAliens, stGoons, stVacuum, stMetal;
        u32 stThresholds, stSkillN, stMaxSkill, stGauge;
        u32 customSong0, muNone, muRowsMax;                  // the first recorded song's code; the playing index when none; rows shown
        u32 stMaxPage, stMaxTransition, stMaxLevel, stMaxPoints;   // stat panel bounds (the aspiration level 3 is the top)
        u32 buildTimers;                                     // from G: u32 per place, the time its building ends (vs `clock`; noCheckOut: never set)
    } hotel;
    // The minigames (milestone 6; docs/research/ram-map.md, "The minigames"): every screen's (overlay, vtable) and the offsets from S.
    struct Minigames
    {
        OvScreen screens[11];                                 // ov kAnyOverlay: the class has no overlay of its own (its vtable says it)
        u32 vtMooGooIntro;                                   // the intro's vtable (screens[] names the game's)
        // Alien Autopsy
        u32 auOrgans, auOrganStride, auDamage, auKind, auFreed;      // from S: organ slots {+0xC u16 damage, +0xE u8 kind, +0xF u8 freed}
        u32 auParasites, auParaStride, auParaTarget, auParaEating;   // from S: parasite slots {+0 sprite, +0x11 target organ, +0x14 eating}
        u32 auSpriteX, auSpriteY;                                    // from a sprite: 20.12 positions
        u32 auEarnings, auPhase, auSub, auSpecimen, auTool, auDragged, auHeld, auTearX, auTearY;
        // Metal Detector
        u32 mdState, mdCharge, mdInRange, mdSlider, mdSliderValue, mdCard, mdMismatch, mdFind, mdFindType, mdEarnings, mdFinds;   // earnings and finds from G
        // MooGoo Monkey
        u32 mgPhase, mgSub, mgTimer, mgCurrent, mgChoice, mgRound, mgFlags, mgQuit, mgQuitAsk, mgWinnings, mgDeckNext, mgDeckSize;
        u32 mgMonkeys, mgMonkeyStride, mgMCards, mgMBets, mgMCount, mgMSlots, mgMFlags;
        u32 mgPlayers, mgPlayerStride, mgPName, mgPHand, mgPSel, mgPScore, mgPFlags;
        // Scratch lottery
        u32 scTier, scPrize, scA, scB, scState, scRevealed, scActive, scWhich, scHint, scBoxes, scBoxStride, scCovers, scCoverLeft;
        // Super Drencher
        u32 dwPump, dwTarget, dwType, dwPumping, dwFiring, dwTargetMode, dwHelp, dwHelpText, dwWater, dwPressure;   // water and pressure from G
        // Crime
        u32 crHelp, crDrag, crPhase, crAngle, crScare, crArmed, crSubdue, crScareDown, crSubdueDown, crHit, crHelpText;
        // Music Mixer and its save menu
        u32 mxMode, mxPhase, mxTracks, mxKeys, mxOctave, mxInst, mxSustain, mxSliderA, mxSliderB, mxTempo, mxVariation, mxSong, mxCtl, mxNotes, mxFlags, mxMic, mxUnlocked, mxUnlockedN;
        u32 msMode, msSlot, msName, msContext, msSongs, msSongStride, msRingtones, msRingStride;   // msSongs / msRingtones from G; byte 15 of each entry = used
        // Art Gallery
        u32 arStroke, arButton, arTool, arSwatch, arPage, arBrush, arWork, arIn, arRow, arCol;
        // Keelhaulin' Cards (u16 total and deck; the hands' slots {+0 flags, +4 card pointer, ...}; u8 flags; the name's bytes)
        u32 cdTotal, cdDeck, cdHand, cdOpponent, cdSlotStride, cdCardFrom, cdCardTo, cdBox, cdMessage, cdQuitAsk, cdName, cdNameMax, cdSize;
        u32 cdScore, cdOppScore, cdTreasure, cdScoreMax;   // s32 each
        u32 cdPhase, cdTurn;   // u8: the turn's step (the top screen's line), whose turn (0 the player, 1 the opponent)
        // The vacuum (u8 clog; the earnings are the stat panel's hotel.stVacuum, from G)
        u32 vcClog, vcSize;
    } mg;
    // Milestone 5: the front end (research m5-front-end §0-§5). T = *(P+topObj) the top slot's object, S the bottom's.
    struct
    {
        u32 topObj;                                          // from P
        u32 vtTitle, vtSettings, vtLoad, vtWireless, vtBoot, vtVideo;   // CTitleSceenMenu (top), COptionsMenu, CLoadMenu, CWirelessLoadMenu, CBGDisplayMenu, CVideoScreenMenu
        u32 tPhase, tCount, tButton, maxPhase, maxButton;    // title, from T
        u32 oPage, oPages, oArt, vtPages[4], oSlotSize;      // Settings, from S: the current page object, the four (Music, SFX, Concept, Credits), size
        u32 volMax;                                          // 20.12: 100 %
        u32 cpButtons, cpFlag, cpN, crPos, crLine, crSize;   // Concept page: button i at page+cpButtons+4i, its unlocked byte; credits scroll / line
        u32 ldTapped, ldSize, ldPlace, ldNameDefault, ldTimeBase, ldYearMax;   // Load: tapped row, object size, the string id base 67 (place), the default name's address, the time's base, years kept
        u32 casPage, casGender, casObj, casHero, casSize, casMaxPage, vtCasBottom;   // CAS, from S: hero at S+casHero+4*gender
        u32 vtCas[6], sizeCas[6];                            // page objects: Name, Hair, Face, Clothes, Confirm, Tan, and their sizes
        u32 nameKb, nameBuf, hairSel, hairScroll, face, thCat, thPage, thItem, cfAsp, cfAnswer;
        u32 heroSkin, heroHair, heroSize, maxSkin, maxHairColour, maxHairStyle, maxHairScroll, maxFace, maxCat, maxThPage, maxThItem;
        u32 bgMode, bgCount, bgDuration, bgMaxMode, vdMovie, vdMaxMovie;   // boot, video
        u32 pgSprites, vtSprite, sprFlags, sprX, sprY, maxSprites;   // a page's CActorSprite list (null-ended), a sprite's vtable, visible bit 0, position 20.12
        u32 hairPos, hairArrowL, hairArrowR, thArrowL, thArrowR, thRing, thCellX0, thCellY0, thCellDx, thCellDy;   // sprite indices; the Clothes ring at (x0 + dx c, y0 + dy r)
        u32 sprLoading, sprFrame, sprRes, maxRes;            // a sprite's flags: graphics not uploaded yet (with bit 0: shown, not drawn); its frame (u8), its resources' u16 triplet's pointer; the archive's ids below
    } front;
};
constexpr int kAnyOverlay = -2;
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
        {8, 0x0214B1C4, GScreen::SaveSlots}, {18, 0x0214B8BC, GScreen::Drencher},
        {-1, 0x02131A70, GScreen::FrontEnd},   // the boot's legal splash (CBGDisplayMenu's vtable, overlay -1)
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
        4, 0x5E, 0x4F, 0x5F, 7, 1500, 200, 0x2FA4 + 0x78, 3,
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
    {
        0x544, 0x550, 0x554,
        0x47F4, 0x47F8,
        0x0213E41C, 0x0213E420, 0x0213E424, 0x0213E428, 0x0213E42C, 0x0213E430,
        0x862, 0x864, 0x866, 0x868, 0x2C,
        0x2BC0, 8, 4, 5, 0x10, 0x4, 5, 0x21, 0x3F,
        0x46DC, 0x40, 0xF, 3,
    },
    {
        0x4410, 0x4408,
        0x0211F9EC, 4, 0, 2, 3,
        0x0211F9D4, 6,
        5, 0x20,
        0x84, 0x2F64, 0x2F68,
        0x449C, 0x2A, 0x46AC, 16, 3,
        0x0213E580, 0xF4,
        0x41F8, 0x58, 0x54, 0, 4, 14,
        0x480C, 0x80000000, 86400, 0x68,
        20, 0x15, {6, 4, 14},
        {{29, 400}, {15, 600}, {18, 700}, {8, 800}, {20, 1000}},
        {{29, 0x2199}, {15, 0x2333}, {18, 0x24CC}, {8, 0x2666}, {20, 0x2800}},
        20, 12, 0x10CC, 45, 2086, 4,
        0x0214A804, 4, 0x25, 0x26, 0x0214A7A0, 5,
        0x0214A9BC, 12, 0x30, 0x32, 0x25, 0x34,
        0x0214ABA8, 5, 0x28, 0x2C, 0x6C, 0x70, 0x74, 0x0214ABBC, 4, 27, 0x2A,
        0x3C, 0x40, 0x48, 0x44, 0x4C, 33, 3,
        0x021351A8, 8, 0x24, 0x5C, 0x59, 0x5A, 0x6C, 14, 6,
        0x260, 0x26C, 0x264, 0x3C, 0x254, 0x258, 0x268, 0x25D, 0x26D, 0x26E, 0x214, 12, 2, 0x3B,
        0x0214B5A4, 14, 0x38, 0x3C, 0x6C, 0x70, 0x5B, 0x74, 0x78, 0x7C, 0x80,
        0x02121DD8, 5, 10, 20,
        0x2B, 0xFF, 5,
        4, 2, 3, 9999,
        0x4418,
    },
    {
        {
            {0, 0x0214EBC4, GScreen::Autopsy}, {9, 0x0214C124, GScreen::Detector}, {10, 0x02152E0C, GScreen::MooGoo}, {10, 0x02152EC8, GScreen::MooGoo},
            {13, 0x0214AFC4, GScreen::Scratch}, {kAnyOverlay, 0x02134EC8, GScreen::Crime}, {kAnyOverlay, 0x02134F10, GScreen::Mixer},
            {kAnyOverlay, 0x02134FA0, GScreen::MixerSave}, {kAnyOverlay, 0x02134D70, GScreen::ArtGallery},
            {kAnyOverlay, 0x02136AEC, GScreen::Trading}, {kAnyOverlay, 0x02136B7C, GScreen::Trading},
        },
        0x02152EC8,
        0x28, 0x10, 0xC, 0xE, 0xF,
        0x58, 0x18, 0x11, 0x14,
        0x50, 0x54,
        0xC9EC, 0xC9F4, 0xC9F5, 0xC9F6, 0xC9F7, 0xCA0C, 0xCA0D, 0xCA16, 0xCA18,
        0x9C, 0xD0, 0xD4, 0xD8, 0x90, 0x4E, 0x54, 0x58, 8, 0x80, 0x31C8,
        0x1C, 0xC, 0x18, 0x1E6, 0x1E7, 0x1E9, 0x1EA, 0x1F0, 0x1F1, 0x1F2, 0x1E4, 0x1E5,
        0x30, 0x2C, 0x18, 0x23, 0x26, 0x27, 0x28,
        0x138, 0x28, 0x18, 0x1C, 0x21, 0x22, 0x23,
        0xE0, 0x10, 0xC8, 0xCC, 0xD0, 0xD8, 0xDC, 0xDE, 0xE4, 0x24, 0xC, 0xA4, 0x98,
        0x24, 0x28, 0x2C, 0x34, 0x35, 0x70, 0x78, 0x79, 0x18, 0x1C,
        0x4C, 0x52, 0x58, 0x5A, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x4D,
        8, 0xA55, 0x8E2, 0x8E3, 0x8E6, 0x8E7, 0x8E9, 0x8EA, 0x8EB, 0x8EC, 0x8ED, 0xA5C, 0xA4C, 0xA4E, 0xA52, 0xA6C, 0xA63, 0xA6B,
        8, 0x28, 0x88, 0xC, 0x46AC, 16, 0x46DC, 64,
        0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x650, 0xC68, 0xC60, 0xC64,
        0x4C, 0x4E, 0xC0, 0x188, 0x14, 0x200, 0x300, 0x306, 0x2F7, 0x307, 0x308, 8, 1000,
        0x2CC, 0x2D0, 0x2E4, 9999,
        0x2F0, 0x2F1,
        0x158, 0x194,
    },
    {
        0xDA8,
        0x0214ABB8, 0x02131BF0, 0x0214B15C, 0x0214B190, 0x02131A70, 0x02131E20,
        0x25, 0x24, 0x26, 4, 3,
        0x28, 0x2C, 0x24, {0x02131CF4, 0x02131CB8, 0x02131C0C, 0x02131BD4}, 0x54,
        0x1000,
        0xC, 0x8D, 12, 0xC, 0x14, 0x18,
        0x70, 0x78, 67, 0x0214B1F4, 0x80000000, 100,
        0x74, 0x76, 0x58, 0x48, 0x84, 7, 0x021509B0,
        {0x02150B54, 0x02150B10, 0x02150AA8, 0x02150BF4, 0x02150A2C, 0x02150B9C}, {0xDC, 0x4C, 0x14, 0x24, 0x18, 0x18},
        0x12, 0x70, 0x3E, 0x38, 0x10, 0x14, 0x16, 0x18, 0x10, 0x11,
        0x232, 0x236, 0x244, 3, 7, 17, 7, 8, 5, 11, 127,
        0x28, 0x24, 0x26, 6, 0x2C, 3,
        0x8, 0x0212FED8, 0x4, 0x50, 0x54, 24,
        0x3A, 2, 3, 6, 7, 8, 88, 41, 48, 64,
        0x80, 0x1A, 0x20, 0x4000,
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
    // The name field: 8 bytes including the NUL (the CAS editor's 7 + 1): letters of the game's
    // charset (accented ones too: the keyboards of FR / DE / IT / ES have them) then a NUL within
    // it, as UTF-8. Anything else blanks the name only.
    std::string name;
    bool terminated = false;
    for (u32 i = 0; i < kEU.simNameMax && !terminated; i++)
    {
        u32 ch;
        if (!m.R8(G + kEU.simName + i, ch) || (ch != 0 && !GameTextByte((unsigned char)ch))) { name.clear(); break; }
        if (ch == 0) terminated = true; else name += (char)ch;
    }
    if (!terminated) name.clear();
    name = GameTextToUtf8(name);
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
    r.nameId = (int)name; r.iconRes = (int)icon; r.price = (int)(s16)price; // (signed: some records' prices are negative)
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
        v.slots.push_back({(int)id, row.nameId, (int)(t.itemDesc + id), row.descRes, 1, row.tilesRes, row.palRes});
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
    u32 by, bw, bt;
    if (!m.R32(P + kEU.x.popBoxY, by) || !m.R32(P + kEU.x.popBoxW, bw) || !m.R32(P + kEU.x.popBoxTiles, bt)) return PopupState{};
    // (Out of range: kept at 0, no button place: PopupButton says so.)
    if (by < 192 && bw >= 32 && bw <= 256 && bt >= 1 && bt <= 24) { p.boxY = (int)by; p.boxW = (int)bw; p.boxTiles = (int)bt; }
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
        for (char ch : name) if (!GameTextByte((unsigned char)ch)) ok = false;
        s.slot[i].simName = ok ? GameTextToUtf8(name) : std::string();
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

// The amount the game prints in an action's label (@1), -1 when it sets none (InteractAction).
int ActionAmount(const Reader& m, u32 iface, u32 code)
{
    const auto& q = kEU.menus;
    const auto& t = kEU.town;
    if (code == q.codePayoff) return (int)q.payoffAmount;
    if (code == q.codeBail) return (int)q.bailAmount;
    if (code != q.codeBuy && code != q.codePay) return -1;
    u32 actor, k;
    if (!m.R32(iface + q.ifActor, actor) || !m.In(actor, 0x10) || !m.R16(actor + q.actIndex, k)) return -1;
    if (code == q.codeBuy)
    {
        if (k > 0x1FF) return -1;
        u32 price;
        if (!m.R16(t.recTable + t.recSize * (t.itemRow0 + k) + t.recPrice, price)) return -1;
        return std::max(0, 2 * (int)(int16_t)price);
    }
    u32 G;
    if (!m.R32(kEU.gameBlock, G)) return -1;
    for (u32 i = 0; i < q.payEntries; i++)
    {
        u32 id, amount;
        if (!m.R8(G + q.payTable + 4 * i, id) || !m.R16(G + q.payTable + 4 * i + 2, amount)) return -1;
        if (id == (k & 0xFF)) return std::max(0, (int)(int16_t)amount);
    }
    return -1;
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
                a.amount = ActionAmount(m, iface, code);
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
    // An inbox entry's layout at e (also page 1's call record in S): false when it does not read.
    auto entry = [&](u32 e, InboxMsg& b) {
        u32 read, text, sender;
        if (!m.R8(e + q.inbRead, read) || read > 1 || !m.R32(e + q.inbText, text) || text >= q.stringMax || !m.R16(e + q.inbSender, sender) || sender >= t.maxSims) return false;
        b.textId = (int)text; b.sender = (int)sender; b.senderNameId = (int)(kEU.simNameBase + sender); b.read = read == 1;
        for (int a = 0; a < 4; a++)
        {
            u32 arg, num;
            if (!m.R32(e + q.inbArgs + 4 * a, arg) || !m.R8(e + q.inbArgNum + a, num)) return false;
            b.args[a] = arg; b.argNum[a] = (int)num;
        }
        return true;
    };
    for (u32 i = 0; i < q.inboxN; i++)
    {
        const u32 e = G + q.inboxBase + q.inboxStride * i;
        u32 present;
        if (!m.R8(e + q.inbPresent, present) || present > 1) return PhoneState{};
        if (!present) continue;
        InboxMsg b;
        if (!entry(e, b)) return PhoneState{};
        b.slot = (int)i;
        v.inbox.push_back(b);
    }
    u32 month, day;
    if (!m.R32(G + kEU.x.phMonth, month) || !m.R32(G + kEU.x.phDay, day) || month < 1 || month > 12 || day < 1 || day > 31) return PhoneState{};
    v.month = (int)month; v.day = (int)day;
    // The page's own fields (each read on its page only: off it they are stale).
    const u32 endOf[11] = {0, kEU.x.phEnd1, 0, kEU.x.phNext3, 0, 0, kEU.x.phEnd6, 0, kEU.x.phEnd8, kEU.x.phEnd9, 0};
    if (endOf[page])
    {
        u32 end;
        if (!m.R8(endOf[page], end) || end < 1 || end > q.maxPhoneRow) return PhoneState{};
        v.textEnd = (int)end;
    }
    if (page == 5)
    {
        u32 cost;
        if (!m.R32(kEU.x.phMaidCost, cost) || cost > 99999999) return PhoneState{};
        v.maidCost = (int)cost;
    }
    if (page == 6 || page == 8)
    {
        u32 r;
        if (page == 6 ? !m.R16(S + kEU.x.phMaidText, r) : !m.R32(S + kEU.x.phPrankText, r)) return PhoneState{};
        if (r >= q.stringMax) return PhoneState{};
        v.reply = (int)r;
    }
    auto simAt = [&](u32 k, u32 off, u32& out) { return m.R8(G + kEU.simRecords + kEU.simRecordSize * k + off, out); };
    if (page == 8 || page == 9)
    {
        u32 k;
        if (!m.R16(S + kEU.x.phCaller, k) || k >= t.maxSims) return PhoneState{};
        v.caller = (int)k;
        if (page == 9)
        {
            u32 state, place, mood;
            if (!simAt(k, kEU.x.simState, state) || !simAt(k, kEU.x.simPlace, place) || !simAt(k, t.simMood, mood) || mood > 4) return PhoneState{};
            if (place >= 0x20 && place != kEU.x.awayPlace) return PhoneState{};   // (a place the string table has none of: the crop)
            v.callAway = state == 0 || place == kEU.x.awayPlace;
            v.callMood = (int)mood; v.callPlace = (int)place;
        }
    }
    if (page == 1)
    {
        if (!entry(S + kEU.x.phIncoming, v.incoming)) return PhoneState{};
        v.caller = v.incoming.sender;
    }
    if (page == 7)
    {
        u32 prank;
        if (!m.R8(S + kEU.x.phPrank, prank) || prank > 1) return PhoneState{};
        v.prank = prank == 1;
        // The game's list (0x0205B…): the hotel's guests whose record says checked in (state 5),
        // then the Sims 0..23 met (flags bit 0) and not away (state 7, 8, 9). The game also asks
        // one more function (0x02077E08) not decoded: the list is kept only when it agrees with
        // the game's count and its highlighted Sim.
        std::vector<int> list;
        for (u32 j = 0; j < kEU.x.guestN; j++)
        {
            u32 sim, state;
            if (!m.R8(G + kEU.x.guests + kEU.x.guestStride * j + kEU.x.guestSim, sim)) return PhoneState{};
            if (sim == kEU.x.noSim || sim >= t.maxSims) continue;
            if (simAt(sim, kEU.x.simState, state) && state == kEU.x.guestState) list.push_back((int)sim);
        }
        for (u32 k = 0; k < 24; k++)
        {
            u32 flags, state;
            if (!m.R8(G + kEU.simRecords + kEU.simRecordSize * k + t.simFlags, flags) || !simAt(k, kEU.x.simState, state)) return PhoneState{};
            if ((flags & 1) && (state < 7 || state > 9)) list.push_back((int)k);
        }
        const bool agrees = (int)list.size() == (int)contacts &&
                            (row < (int)list.size() ? list[row] == (int)csim : row == (int)list.size() && csim == kEU.x.noSim);
        if (agrees) { v.contactList = list; v.contactsKnown = true; }
    }
    if (page == 4)
        for (u32 j = 0; j < kEU.x.toneN; j++)
        {
            u32 named;
            if (!m.R8(G + kEU.x.tones + kEU.x.toneStride * j + kEU.x.toneNamed, named)) return PhoneState{};
            if (named) v.customTones = true;
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
    int slot = m.S32(S + q.giftSlot);
    if (slot < -1) return v;
    // The slot just given (the screen closing: the pockets one item shorter, the selection not yet
    // moved, about 19 frames): no selection, the part still read.
    if (slot >= (int)count && slot < (int)t.maxSlots) slot = -1;
    if (slot >= (int)count) return v;
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

// ---- The hotel and the shops (milestone 4). Each part reads apart from the others, from G or from its screen object.
bool Bits(const Reader& m, u32 a, u64& out)
{
    u32 lo, hi;
    if (!m.R32(a, lo) || !m.R32(a + 4, hi) || (hi >> (kPlaces - 32)) != 0) return false;   // bits past the last place are never set
    out = lo | (u64)hi << 32;
    return true;
}

bool RoomRowAt(const Reader& m, u32 place, RoomRow& r)
{
    const auto& h = kEU.hotel;
    u32 permit, weight, parent;
    const u32 a = h.roomTable + h.roomStride * place;
    if (place >= (u32)kPlaces || !m.R16(a + h.roomPermit, permit) || !m.R8(a + h.roomWeight, weight) || !m.R8(a + h.roomParent, parent) || weight > 100 || parent > (u32)kNoPlace) return false;
    r.permit = (int)permit; r.weight = (int)weight; r.parent = (int)parent;
    return true;
}

// The hotel score: the weights of the built rooms (at most 100).
bool HotelScore(const Reader& m, u64 built, int& score)
{
    score = 0;
    for (u32 k = 0; k < (u32)kPlaces; k++)
    {
        RoomRow r;
        if (!RoomRowAt(m, k, r)) return false;
        if ((built >> k) & 1) score += r.weight;
    }
    return score <= 100;
}

bool ReadSlots(const Reader& m, u32 G, GuestSlot out[5])
{
    const auto& x = kEU.x; const auto& h = kEU.hotel;
    for (u32 j = 0; j < x.guestN; j++)
    {
        const u32 a = G + x.guests + x.guestStride * j;
        u32 place, sim, days;
        if (!m.R32(a, place) || !m.R8(a + x.guestSim, sim) || !m.R8(a + h.slotDays, days) || place > (u32)kNoPlace || (sim >= kEU.town.maxSims && sim != x.noSim)) return false;
        out[j] = {(int)place, (int)sim, (int)days};
    }
    return true;
}

// The Sim in a room, -1 vacant: the guest slot that holds it, or the story Sim in the Penthouse while the mission runs.
bool OccupantOf(const Reader& m, u32 G, const GuestSlot* slots, u32 place, int& sim)
{
    const auto& h = kEU.hotel; const auto& q = kEU.menus;
    sim = -1;
    if (place == h.penthouse)
    {
        u32 stage;
        if (!m.R8(G + q.missionBase + q.missionStage, stage)) return false;
        if (stage >= 1 && stage <= 3) { sim = (int)h.penthouseSims[stage - 1]; return true; }
    }
    for (u32 j = 0; j < kEU.x.guestN; j++)
        if (slots[j].place == (int)place) { sim = slots[j].sim == (int)kEU.x.noSim ? -1 : slots[j].sim; return true; }
    return true;
}

bool FlagSet(const Reader& m, u32 G, u32 flag)
{
    u32 w;
    return m.R32(G + kEU.menus.flags + 4 * (flag >> 5), w) && ((w >> (flag & 31)) & 1);
}

HotelState ReadHotel(const Reader& m)
{
    const auto& h = kEU.hotel;
    HotelState v;
    u32 G, w;
    if (!m.R32(kEU.gameBlock, G) || !Bits(m, G + h.builtBits, v.built) || !Bits(m, G + h.unlockedBits, v.unlocked)) return v;
    for (u32 k = 0; k < (u32)kPlaces; k++)
    {
        if (!RoomRowAt(m, k, v.rooms[k]) || !m.R8(G + h.songs + k, w)) return HotelState{};
        v.songs[k] = (int)w;
    }
    if (!HotelScore(m, v.built, v.score) || !ReadSlots(m, G, v.slots)) return HotelState{};
    u32 stayed, money, earned, spent;
    if (!m.R32(G + h.guestsStayed, stayed) || !m.R32(G + kEU.money, money) || !m.R32(G + h.earned, earned) || !m.R32(G + h.spent, spent) ||
        stayed > 99999 || money > 999999 || earned > 999999 || spent > 999999)
        return HotelState{};
    v.guestsStayed = (int)stayed; v.money = (int)money; v.earned = (int)earned; v.spent = (int)spent;
    // A room is being built while its completion time is not yet past the clock (Build Rooms' own
    // test leaves it out of the list then: listed when timer < clock).
    u32 clock;
    if (!m.R32(G + h.clock, clock)) return HotelState{};
    for (u32 k = 0; k < (u32)kPlaces; k++)
    {
        u32 t;
        if (!m.R32(G + h.buildTimers + 4 * k, t)) return HotelState{};
        if (t != h.noCheckOut && t >= clock) v.building |= 1ull << k;
    }
    u32 world, pl;
    if (m.R32(h.placePtr, world) && m.R32(world + h.placeOff, pl) && pl < (u32)kPlaces) v.place = (int)pl;
    v.valid = true;
    return v;
}

CheckInState ReadCheckIn(const Reader& m, u32 S, const HotelState& hotel)
{
    const auto& h = kEU.hotel;
    CheckInState v;
    u32 G, mode, guest, flagOk;
    if (!hotel.valid || !m.R32(kEU.gameBlock, G) || !m.R8(S + h.regMode, mode) || mode > 1 || !m.R8(S + h.regGuest, guest)) return v;
    const u64 built = hotel.built;
    const GuestSlot* slots = hotel.slots;
    v.score = hotel.score;
    if (mode == 0 && guest >= kEU.town.maxSims) return v;
    flagOk = FlagSet(m, G, h.penthouseFlag);
    for (u32 i = 0; i < h.regRoomN; i++)
    {
        u32 place;
        int sim;
        if (!m.R8(h.regRooms + i, place) || place >= (u32)kPlaces || !OccupantOf(m, G, slots, place, sim)) return CheckInState{};
        RegistryRow& r = v.rows[i];
        r.place = (int)place;
        r.built = (built >> place) & 1;
        r.occupant = sim;
        r.available = r.built && sim < 0 && (place != h.penthouse || flagOk);
        r.infoOk = r.built && (place != h.penthouse || flagOk);
    }
    v.mode = (int)mode; v.guest = mode == 0 ? (int)guest : -1;
    v.valid = true;
    return v;
}

// A guest room's daily cost: base + the furniture's service charge + the score's bonus (x 1.05 with the Fortune aspiration, 0).
bool RoomCost(const Reader& m, u32 G, u32 place, int score, RoomInfoState& v)
{
    const auto& h = kEU.hotel; const auto& t = kEU.town;
    int base = -1, factor = -1;
    for (const auto& b : h.baseValues) if (b[0] == place) base = (int)b[1];
    for (const auto& f : h.scoreFactors) if (f[0] == place) factor = (int)f[1];
    u32 r = h.guestRoomN;
    for (u32 i = 0; i < h.guestRoomN; i++) { u32 id; if (m.R32(h.guestRooms + 4 * i, id) && id == place) r = i; }
    u32 n, aspiration;
    if (base < 0 || factor < 0 || r == h.guestRoomN || !m.R32(G + h.roomBlock + h.roomRec * r + h.roomCount, n) || n > h.maxRoomItems || !m.R32(G + h.aspiration, aspiration)) return false;
    int sum = 0;
    for (u32 i = 0; i < n; i++)
    {
        u32 id;
        if (!m.R8(G + h.roomBlock + h.roomRec * r + h.roomItems + h.roomItemStride * i, id)) return false;
        RecordRow row = ReadRow(m, t.itemRow0 + id);
        if (!row.valid) return false;
        sum += row.price;
    }
    v.baseValue = base;
    v.serviceCharge = (sum * (int)h.serviceMul) >> h.serviceShift;   // truncated: the game's 20.12 product shifted back
    v.scoreBonus = score * factor / 4096;
    v.total = v.baseValue + v.serviceCharge + v.scoreBonus;
    if (aspiration == 0) v.total = v.total * (int)h.fortuneMul >> 12;
    return true;
}

RoomInfoState ReadRoomInfo(const Reader& m, u32 S, const HotelState& hotel)
{
    const auto& h = kEU.hotel; const auto& x = kEU.x; const auto& q = kEU.menus;
    RoomInfoState v;
    u32 G, place, occupant, occupied, a, b, c, d;
    if (!hotel.valid || !m.R32(kEU.gameBlock, G) || !m.R16(S + h.riPlace, place) || place >= (u32)kPlaces || !m.R16(S + h.riOccupant, occupant) || occupant >= kEU.town.maxSims ||
        !m.R8(S + h.riOccupied, occupied) || occupied > 1 || !RoomCost(m, G, place, hotel.score, v))
        return RoomInfoState{};
    if (!m.R8(S + h.riHint, a) || !m.R8(S + h.riHint + 1, b) || !m.R8(S + h.riHint + 2, c) || !m.R8(S + h.riHint + 3, d) || a > 1 || b > 1 || c > 1 || d > 1) return RoomInfoState{};
    v.hint = a && b && c;
    v.place = (int)place; v.occupant = (int)occupant; v.occupied = occupied == 1;
    if (v.occupied)
    {
        const u32 R = G + kEU.simRecords + kEU.simRecordSize * occupant;
        u32 mood, out, clock, month, day;
        if (!m.R8(R + kEU.town.simMood, mood) || mood > h.maxMood || !m.R32(R + h.simCheckOut, out) || !m.R32(G + h.clock, clock) || !m.R32(G + x.phMonth, month) || !m.R32(G + x.phDay, day))
            return RoomInfoState{};
        v.moodId = (int)(h.moodBase + mood);
        u32 stage;
        const bool story = place == h.penthouse && m.R8(G + q.missionBase + q.missionStage, stage) && stage <= 3;
        const int diff = (int)(out - clock);
        if (!story && out != h.noCheckOut && month >= 1 && month <= 12 && day >= 1 && day <= 31 && diff >= 0 && diff / (int)h.daySeconds < 400)
        {
            v.days = diff / (int)h.daySeconds + 1;
            static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            int mo = (int)month - 1, dd = (int)day + v.days;
            while (dd > len[mo]) { dd -= len[mo]; mo = (mo + 1) % 12; }
            v.month = mo + 1; v.day = dd;
            v.checkOutKnown = true;
        }
    }
    v.valid = true;
    return v;
}

MusicState ReadMusic(const Reader& m, u32 S, const HotelState& hotel)
{
    const auto& h = kEU.hotel;
    MusicState v;
    u32 count, scroll, playing, rows, G, song;
    if (!m.R32(S + h.muCount, count) || count > h.songN || !m.R32(S + h.muScroll, scroll) || !m.R32(S + h.muPlaying, playing) || !m.R32(S + h.muRows, rows) ||
        rows != (count < h.muRowsMax ? count : h.muRowsMax) || scroll > count - rows || (playing != h.muNone && playing >= count) || !m.R32(kEU.gameBlock, G))
        return v;
    for (u32 i = 0; i < count; i++)
    {
        u32 idx, code, name;
        if (!m.R8(S + h.muList + i, idx) || idx >= h.songN) return MusicState{};
        const u32 e = h.songTable + h.songStride * idx;
        if (!m.R8(e, code) || code == h.songEnd || !m.R16(e + 2, name)) return MusicState{};
        const int custom = code >= h.customSong0 && code < h.customSong0 + h.customN ? (int)code - (int)h.customSong0 : -1;
        SongRow row {(int)idx, (int)code, custom < 0 ? (int)name : -1, custom};
        // A recorded song's name: the mixer's char[16] in G (the game's own text, up to its NUL).
        for (u32 k = 0; custom >= 0 && k < h.customStride; k++)
        {
            u32 b;
            if (!m.R8(G + h.customSongs + h.customStride * (u32)custom + k, b) || b == 0) break;
            row.customName += (char)b;
        }
        v.list.push_back(row);
    }
    v.scroll = (int)scroll; v.playing = playing == h.muNone ? -1 : (int)playing;
    if (hotel.valid && hotel.place >= 0 && m.R8(G + h.songs + (u32)hotel.place, song)) { v.place = hotel.place; v.roomSong = (int)song; }
    v.valid = true;
    return v;
}

BuildState ReadBuild(const Reader& m, u32 S)
{
    const auto& h = kEU.hotel;
    BuildState v;
    u32 count, scroll, page, chosen, G, money;
    if (!m.R32(S + h.bdCount, count) || count > h.bdMax || !m.R32(S + h.bdScroll, scroll) || scroll > (count > h.bdRows ? count - h.bdRows : 0) || !m.R32(S + h.bdPage, page) || page > 1 ||
        !m.R32(S + h.bdChosen, chosen) || !m.R32(kEU.gameBlock, G) || !m.R32(G + kEU.money, money) || money > 999999)
        return v;
    for (u32 i = 0; i < count; i++)
    {
        u32 place;
        RoomRow r;
        if (!m.R32(S + h.bdPlaces + 4 * i, place) || !RoomRowAt(m, place, r) || r.permit == 0) return BuildState{};   // a listed room always has a permit
        v.places.push_back({(int)place, r.permit, (int)(kEU.menus.hotelPlaceBase + r.parent), (int)money >= r.permit});
    }
    if (page == 1 && chosen >= count) return BuildState{};
    v.scroll = (int)scroll; v.page = (int)page; v.chosen = page == 1 ? (int)chosen : -1; v.money = (int)money;
    v.valid = true;
    return v;
}

PlacementState ReadPlacement(const Reader& m, u32 S)
{
    const auto& h = kEU.hotel;
    PlacementState v;
    u32 G, mode, room, phase, slot, obj, rot, n;
    if (!m.R32(kEU.gameBlock, G) || !m.R32(S + h.plMode, mode) || mode < 1 || mode > 2 || !m.R32(S + h.plRoom, room) || room >= h.plRoomMax || !m.R32(S + h.plPhase, phase) || phase > 3 ||
        !m.R8(S + h.plSlot, slot) || slot >= kEU.town.maxSlots || !m.R8(S + h.plObject, obj) || obj >= h.plMax || !m.R32(S + h.plRotation, rot) || rot > 3)
        return v;
    const u32 R = G + h.roomBlock + h.roomRec * room;
    if (!m.R32(R + h.roomCount, n) || n > h.plMax) return v;
    for (u32 i = 0; i < n; i++)
    {
        u32 id, px, py, pr;
        const u32 a = R + h.roomItems + h.roomItemStride * i;
        if (!m.R8(a, id) || !m.R8(a + 1, px) || !m.R8(a + 2, py) || !m.R8(a + 3, pr) || pr > 3) return PlacementState{};
        v.objects.push_back({(int)id, (int)px, (int)py, (int)pr});
    }
    v.mode = (int)mode; v.room = (int)room; v.phase = (int)phase; v.pocketSlot = (int)slot; v.object = (int)obj; v.rotation = (int)rot;
    v.valid = true;
    return v;
}

// A shop row (record row = id + 0x3B): its record names its own item.
bool ShopItemAt(const Reader& m, u32 row, ShopItem& it)
{
    const auto& t = kEU.town; const auto& h = kEU.hotel;
    if (row < h.caRowMin || row - h.caRowMin >= t.maxItems) return false;
    const u32 id = row - h.caRowMin;
    RecordRow r = ReadRow(m, row);
    if (!r.valid || r.nameId != (int)(t.itemName + id)) return false;
    it = {(int)row, (int)id, r.nameId, (int)(t.itemDesc + id), r.descRes, r.price * (int)h.caPriceMul, r.price, r.tilesRes, r.palRes};
    return true;
}

CatalogueState ReadCatalogue(const Reader& m, u32 S)
{
    const auto& h = kEU.hotel; const auto& t = kEU.town;
    CatalogueState v;
    u32 G, shop, layout, count, mode, sel, scroll, pay, up, down, owned, money, ring;
    if (!m.R32(kEU.gameBlock, G) || !m.R32(S + h.caShop, shop) || shop >= kEU.menus.maxSimIndex || !m.R8(S + h.caLayout, layout) || layout > 1 || !m.R8(S + h.caCount, count) || count == 0 || count > h.caMaxItems ||
        !m.R32(S + h.caMode, mode) || mode < 1 || mode > 2 || !m.R32(S + h.caSel, sel) || !m.R32(S + h.caScroll, scroll) || scroll >= count || !m.R8(S + h.caPay, pay) || pay > 1 ||
        !m.R8(S + h.caUp, up) || up > 1 || !m.R8(S + h.caDown, down) || down > 1 || !m.R32(G + t.invCount, owned) || owned > t.maxSlots || !m.R32(G + kEU.money, money) || money > 999999)
        return v;
    for (u32 i = 0; i < count; i++)
    {
        u32 row;
        ShopItem it;
        if (!m.R32(S + h.caList + 4 * i, row) || !ShopItemAt(m, row, it)) return CatalogueState{};
        v.items.push_back(it);
    }
    for (u32 i = 0; i < owned; i++)
    {
        u32 id;
        ShopItem it;
        if (!m.R16(G + t.invIds + 2 * i, id) || id >= t.maxItems || !ShopItemAt(m, t.itemRow0 + id, it)) return CatalogueState{};
        v.owned.push_back(it);
    }
    // A sale leaves the selection past the list until the next touch: no selection then.
    v.sell = mode == 2;
    v.selected = sel < (v.sell ? owned : count) ? (int)sel : -1;
    v.shop = (int)shop; v.vendor = layout == 1; v.scroll = (int)scroll;
    v.payEnabled = pay; v.upEnabled = up; v.downEnabled = down;
    u32 rp, vis;
    v.ring = Ptr(m, S + h.caRing, 8, rp) && m.R8(rp + 4, vis) && (vis & 1);
    v.money = (int)money;
    v.valid = true;
    return v;
}

StatState ReadStat(const Reader& m, u32 S, const HotelState& hotel)
{
    const auto& h = kEU.hotel;
    StatState v;
    u32 G, page, tr, asp, level, points, thr = 0;
    if (!hotel.valid || !m.R32(kEU.gameBlock, G) || !m.R32(S + h.stPage, page) || page > h.stMaxPage || !m.R32(S + h.stTransition, tr) || tr > h.stMaxTransition || !m.R32(G + h.aspiration, asp) || asp > 3 ||
        !m.R32(G + h.stLevel, level) || level > h.stMaxLevel || !m.R32(G + h.stPoints, points) || points > h.stMaxPoints)
        return v;
    if (level < h.stMaxLevel && (!m.R32(h.stThresholds + 4 * level, thr) || thr == 0 || thr > h.stMaxPoints)) return v;
    for (u32 i = 0; i < h.stSkillN; i++)
    {
        u32 k;
        if (!m.R8(G + h.stSkills + i, k) || k > h.stMaxSkill) return StatState{};
        v.skills[i] = (int)k;
    }
    u32 al, go, va, me;
    if (!m.R32(G + h.stAliens, al) || !m.R32(G + h.stGoons, go) || !m.R32(G + h.stVacuum, va) || !m.R32(G + h.stMetal, me))
        return StatState{};
    v.page = (int)page; v.transition = (int)tr; v.aspiration = (int)asp; v.level = (int)level; v.points = (int)points;
    v.gaugeFrame = level < h.stMaxLevel ? (int)std::min<u32>(h.stGauge, (h.stGauge * points + thr / 2) / thr) : (int)h.stGauge;
    v.score = hotel.score; v.guestsStayed = hotel.guestsStayed; v.earned = hotel.earned; v.spent = hotel.spent;
    v.aliens = (int)al; v.goons = (int)go; v.vacuum = (int)va; v.metal = (int)me;
    v.valid = true;
    return v;
}

// ---- The minigames (milestone 6). Each reader is given the screen object S (vtable already checked)
// and answers an invalid part, never a partial one, when a field is out of its range.
int Fix12(u32 raw) { return (int)raw >> 12; }   // a 20.12 fixed value, whole units
// The game's raw text for a log line: printable ASCII as it is, the rest \xNN.
std::string TextEsc(const std::string& raw)
{
    std::string r;
    for (unsigned char c : raw)
    {
        if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') r += (char)c;
        else { char h[8]; snprintf(h, sizeof h, "\\x%02x", c); r += h; }
    }
    return r;
}

AutopsyState ReadAutopsy(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    AutopsyState v;
    u32 phase, sub, spec, tool, dragged, held, tx, ty;
    if (!m.In(S, 0xCA40) || !m.R8(S + q.auPhase, phase) || phase > 6 || !m.R8(S + q.auSub, sub) || sub > 7 || !m.R8(S + q.auSpecimen, spec) || spec > 1 ||
        !m.R8(S + q.auTool, tool) || tool > 2 || !m.R8(S + q.auDragged, dragged) || !m.R8(S + q.auHeld, held) || !m.R16(S + q.auTearX, tx) || !m.R16(S + q.auTearY, ty))
        return v;
    const int earn = m.S32(S + q.auEarnings);
    if (earn < 0 || earn > 999999) return AutopsyState{};
    v.phase = (int)phase; v.sub = (int)sub; v.specimen = (int)spec; v.tool = (int)tool; v.earnings = earn;
    for (int i = 0; i < kAutopsyOrgans; i++)
    {
        const u32 o = S + q.auOrgans + q.auOrganStride * (u32)i;
        u32 damage, kind, freed;
        if (!m.R16(o + q.auDamage, damage) || damage > (u32)kAutopsyDestroyed || !m.R8(o + q.auKind, kind) || kind > (u32)kAutopsyKindGone || !m.R8(o + q.auFreed, freed) || freed > 1)
            return AutopsyState{};
        v.organs[i] = {(int)kind, (int)damage, freed == 1};
        if (v.organs[i].present() && v.organs[i].freed) v.handAlert = true;
    }
    for (int j = 0; j < kAutopsyParasites; j++)
    {
        const u32 e = S + q.auParasites + q.auParaStride * (u32)j;
        u32 sprite, target, eating;
        if (!m.R32(e, sprite)) return AutopsyState{};
        if (sprite == 0) continue;
        if (!m.In(sprite, q.auSpriteY + 4) || !m.R8(e + q.auParaTarget, target) || target > 3 || !m.R8(e + q.auParaEating, eating) || eating > 1) return AutopsyState{};
        const int x = Fix12((u32)m.S32(sprite + q.auSpriteX)), y = Fix12((u32)m.S32(sprite + q.auSpriteY));
        if (x < -4096 || x > 4096 || y < -4096 || y > 4096) return AutopsyState{};
        v.parasites.push_back({j, x, y, target < 3 ? (int)target : -1, eating == 1});
        if (eating == 1) v.tweezersAlert = true;
    }
    // The "none" bytes are only written after a use (stale before it): read them under their sub-phase.
    if ((sub == 3 || sub == 4) && dragged < (u32)kAutopsyOrgans) v.dragged = (int)dragged;
    if ((sub == 6 || sub == 7) && held < (u32)kAutopsyParasites) v.held = (int)held;
    if (phase == 6) { v.tearX = (int)tx; v.tearY = (int)ty; }
    v.valid = true;
    return v;
}

DetectorState ReadDetector(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    DetectorState v;
    u32 G, state, range, slider, sliderVal, card, mismatch, earn, finds;
    if (!m.In(S, 0xE4) || !m.R32(kEU.gameBlock, G) || !m.R32(S + q.mdState, state) || state > 5 || !m.R8(S + q.mdInRange, range) || range > 1 ||
        !Ptr(m, S + q.mdSlider, q.mdSliderValue + 4, slider) || !m.R32(slider + q.mdSliderValue, sliderVal) || sliderVal > (u32)kDetectorSliderMax ||
        !m.R8(S + q.mdCard, card) || card > 1 || !m.R32(S + q.mdMismatch, mismatch) || mismatch > 128 || !m.R32(G + q.mdEarnings, earn) || earn > 99999999 ||
        !m.R32(G + q.mdFinds, finds))
        return v;
    // (The finds' count is the place's table: a dig with none in it, the casino's empty table in the
    // research's states, leaves it at -1. It says nothing of the screen: 0 then, the part still read.)
    if (finds == 0xFFFFFFFFu) finds = 0;
    else if (finds > 150) return v;
    const int charge = m.S32(S + q.mdCharge);
    if (charge < 0 || charge > 2 * kDetectorFull) return DetectorState{};
    v.state = (int)state; v.charge = charge; v.inRange = range == 1; v.slider = (int)sliderVal; v.mismatch = (int)mismatch;
    // The card flag stays set after the card is gone (the game clears it at the next dig): the card is up in state 5 only, which
    // lasts about 2 s and ends by itself.
    v.card = card == 1 && state == 5;
    v.earnings = (int)earn; v.findCount = (int)finds;
    u32 find, type;
    // The sensed find, in range; and while the "Found:" card is up, the find it shows (the record
    // dug: the game keeps its pointer, the hero no longer in range of a find taken).
    if ((v.inRange || v.card) && Ptr(m, S + q.mdFind, q.mdFindType + 1, find) && m.R8(find + q.mdFindType, type) && type <= 8) v.findType = (int)type;
    v.valid = true;
    return v;
}

MooGooState ReadMooGoo(const Reader& m, u32 S, bool intro)
{
    const auto& q = kEU.mg;
    MooGooState v;
    if (intro) { v.intro = v.valid = m.In(S, 8); return v; }   // (the object is in RAM)
    u32 phase, sub, cur, choice, round, flags, quit, ask, deckNext, deckSize;
    if (!m.In(S, 0x214) || !m.R32(S + q.mgPhase, phase) || phase > 0x17 || !m.R32(S + q.mgSub, sub) || sub > 9 || !m.R8(S + q.mgCurrent, cur) || cur >= (u32)kMooGooPlayers ||
        !m.R8(S + q.mgChoice, choice) || choice >= (u32)kMooGooPlayers || !m.R8(S + q.mgRound, round) || round > 3 || !m.R8(S + q.mgFlags, flags) || !m.R8(S + q.mgQuit, quit) || quit > 1 ||
        !m.R8(S + q.mgQuitAsk, ask) || ask > 1 || !m.R8(S + q.mgDeckNext, deckNext) || !m.R8(S + q.mgDeckSize, deckSize) || deckSize > 54 || deckNext > deckSize)
        return v;
    const int timer = m.S32(S + q.mgTimer), winnings = (int)(short)m.S16(S + q.mgWinnings);
    if (timer < -1 || timer > 0x10000) return MooGooState{};
    v.phase = (int)phase; v.sub = (int)sub; v.timer = timer; v.current = (int)cur; v.humans = (int)choice + 1; v.round = (int)round;
    v.hotSeat = flags & 1; v.turnDone = flags & 2; v.betPlaced = flags & 4; v.stale = flags & 8;
    v.quit = quit == 1; v.quitAsk = ask == 1; v.winnings = winnings; v.deckNext = (int)deckNext; v.deckSize = (int)deckSize;
    for (int k = 0; k < kMooGooMonkeys; k++)
    {
        const u32 e = S + q.mgMonkeys + q.mgMonkeyStride * (u32)k;
        u32 count, slots, fl;
        if (!m.R8(e + q.mgMCount, count) || count > (u32)kMooGooCards || !m.R8(e + q.mgMSlots, slots) || slots > 4 || !m.R8(e + q.mgMFlags, fl)) return MooGooState{};
        MooGooMonkey& mk = v.monkeys[k];
        mk.count = (int)count; mk.slots = (int)slots; mk.eliminated = fl & 1;
        for (u32 i = 0; i < count; i++)
        {
            u32 c;
            if (!m.R8(e + q.mgMCards + i, c) || c > 0x35) return MooGooState{};
            mk.cards[i] = (int)c;
        }
        for (int p = 0; p < kMooGooPlayers; p++)
        {
            u32 b;
            if (!m.R8(e + q.mgMBets + (u32)p, b) || b > 4) return MooGooState{};
            mk.bets[p] = (int)b;
        }
    }
    for (int p = 0; p < kMooGooPlayers; p++)
    {
        const u32 e = S + q.mgPlayers + q.mgPlayerStride * (u32)p;
        u32 name, sel, score, fl;
        if (!m.R32(e + q.mgPName, name) || name > 0xFFFF || !m.R8(e + q.mgPSel, sel) || (sel >> 4) > 4 || !m.R8(e + q.mgPScore, score) || score > 99 || !m.R8(e + q.mgPFlags, fl)) return MooGooState{};
        MooGooPlayer& pl = v.players[p];
        pl.nameId = (int)name; pl.selected = (int)(sel >> 4); pl.score = (int)score; pl.ai = fl & 1; pl.outOfCards = fl & 4; pl.bet = fl & 8;
        for (int i = 0; i < kMooGooHand; i++)
        {
            u32 c;
            if (!m.R8(e + q.mgPHand + (u32)i, c) || (c > 0x35 && c != (u32)kMooGooHandEmpty)) return MooGooState{};
            pl.hand[i] = phase < 8 ? kMooGooHandEmpty : (int)c;   // before the deal the bytes are zeros, not "empty"
        }
    }
    v.valid = true;
    return v;
}

ScratchState ReadScratch(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    ScratchState v;
    u32 tier, prize, a, b, state, revealed, active, which, hint;
    if (!m.In(S, 0xE8) || !m.R32(S + q.scTier, tier) || tier > 2 || !m.R32(S + q.scPrize, prize) || prize > 99999 || !m.R32(S + q.scA, a) || a > 6 || !m.R32(S + q.scB, b) || b > 6 ||
        !m.R32(S + q.scState, state) || state < 1 || state > 3 || !m.R16(S + q.scRevealed, revealed) || revealed > (u32)kScratchBoxes || !m.R8(S + q.scActive, active) || active > 1 ||
        !m.R16(S + q.scWhich, which) || (which != 0xFFFF && which >= (u32)kScratchBoxes) || !m.R8(S + q.scHint, hint) || hint > 1)
        return v;
    v.tier = (int)tier; v.prize = (int)prize; v.winA = (int)a; v.winB = (int)b; v.state = (int)state; v.revealed = (int)revealed;
    v.active = active == 1 && which != 0xFFFF ? (int)which : -1; v.hint = hint == 1;
    for (int i = 0; i < kScratchBoxes; i++)
    {
        const u32 e = S + q.scBoxes + q.scBoxStride * (u32)i;
        u32 pic, st, cover, left;
        if (!m.R32(e, pic) || pic > 6 || !m.R8(e + 9, st) || st > 2 || !Ptr(m, S + q.scCovers + 4 * (u32)i, q.scCoverLeft + 4, cover) || !m.R32(cover + q.scCoverLeft, left) || left > (u32)kScratchPixels)
            return ScratchState{};
        v.boxes[i] = {(int)pic, st == 2, kScratchPixels - (int)left};
    }
    v.valid = true;
    return v;
}

DrencherState ReadDrencher(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    DrencherState v;
    u32 G, pumping, firing, mode, help, text;
    if (!m.In(S, 0x90) || !m.R32(kEU.gameBlock, G) || !m.R8(S + q.dwPumping, pumping) || pumping > 1 || !m.R8(S + q.dwFiring, firing) || firing > 1 || !m.R8(S + q.dwTargetMode, mode) || mode > 1 ||
        !m.R8(S + q.dwHelp, help) || help > 1 || !m.R8(S + q.dwHelpText, text) || text > 1)
        return v;
    const int pump = Fix12((u32)m.S32(S + q.dwPump)), target = m.S32(S + q.dwTarget), type = m.S32(S + q.dwType);
    const int water = m.S32(G + q.dwWater), pressure = m.S32(G + q.dwPressure);
    if (pump < 0 || pump > kDrencherPumpMax || target < -1 || target > 0x1FF || water < 0 || water > (101 << 12) || pressure < 0 || pressure > (126 << 12)) return DrencherState{};
    if (target >= 0 && (type < 0 || type > 0xFF)) return DrencherState{};
    v.pump = pump; v.target = target; v.targetType = target >= 0 ? type : -1; v.pumping = pumping == 1; v.firing = firing == 1; v.targetMode = mode == 1;
    v.help = help == 1; v.helpText = text == 1; v.water = Fix12((u32)water); v.pressure = Fix12((u32)pressure);
    v.valid = true;
    return v;
}

CrimeState ReadCrime(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    CrimeState v;
    u32 help, text, drag, phase, scare, armed, subdue, sd, bd, hit;
    if (!m.In(S, 0x64) || !m.R8(S + q.crHelp, help) || help > 1 || !m.R8(S + q.crHelpText, text) || text > 1 || !m.R8(S + q.crDrag, drag) || drag > 1 || !m.R8(S + q.crPhase, phase) || phase > 3 ||
        !m.R8(S + q.crScare, scare) || scare > 1 || !m.R8(S + q.crArmed, armed) || armed > 1 || !m.R8(S + q.crSubdue, subdue) || subdue > 1 ||
        !m.R8(S + q.crScareDown, sd) || sd > 1 || !m.R8(S + q.crSubdueDown, bd) || bd > 1 || !m.R8(S + q.crHit, hit))
        return v;
    const int angle = (int)(short)m.S16(S + q.crAngle);
    if (angle < -90 || angle > 90) return CrimeState{};
    v.help = help == 1; v.helpText = text == 1; v.dragging = drag == 1; v.phase = (int)phase; v.angle = angle; v.canScare = scare == 1; v.armed = armed == 1; v.canSubdue = subdue == 1;
    v.scarePressed = sd == 1; v.subduePressed = bd == 1;
    v.hit = hit == 1;   // 0x10 before the first throw: unset
    v.valid = true;
    return v;
}

MixerState ReadMixer(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    MixerState v;
    u32 mode, phase, tracks, octave, inst, sus, sa, sb, tempo, var, song, ctl, notes, flags, mic, nUnlocked;
    if (!m.In(S, 0xA70) || !m.R32(S + q.mxMode, mode) || mode > 2 || !m.R8(S + q.mxPhase, phase) || phase > 0xA || !m.R8(S + q.mxTracks, tracks) || !m.R8(S + q.mxOctave, octave) ||
        !m.R8(S + q.mxInst, inst) || !(inst == 1 || inst == 2 || inst == 3 || inst == 5) || !m.R8(S + q.mxSustain, sus) || sus > 1 || !m.R8(S + q.mxSliderA, sa) || sa > 26 ||
        !m.R8(S + q.mxSliderB, sb) || sb > 26 || !m.R8(S + q.mxTempo, tempo) || !m.R8(S + q.mxVariation, var) || var > 1 || !m.R8(S + q.mxSong, song) || song > 8 ||
        !m.R16(S + q.mxCtl, ctl) || !m.R16(S + q.mxNotes, notes) || !m.R8(S + q.mxFlags, flags) || !m.R8(S + q.mxMic, mic) || !m.R8(S + q.mxUnlockedN, nUnlocked) || nUnlocked > (u32)kMixerSongs)
        return v;
    const int oct = (int)(signed char)octave;
    if (oct < -1 || oct > 1) return MixerState{};
    for (int i = 0; i < kMixerKeys; i++)
    {
        u32 k;
        if (!m.R8(S + q.mxKeys + (u32)i, k) || k > 0xD) return MixerState{};
        v.keys[i] = k == 0xD ? -1 : (int)k;
    }
    for (u32 i = 0; i < nUnlocked; i++)
    {
        u32 s;
        if (!m.R8(S + q.mxUnlocked + i, s) || s >= (u32)kMixerSongs) return MixerState{};
        v.unlocked.push_back((int)s);
    }
    v.mode = (int)mode; v.phase = (int)phase; v.tracks = (int)tracks; v.octave = oct; v.instrument = (int)inst; v.sustain = sus == 1; v.sliderA = (int)sa; v.sliderB = (int)sb;
    v.tempo = (int)tempo; v.variation = (int)var; v.song = (int)song; v.controlEvents = (int)ctl; v.noteEvents = (int)notes;
    v.prep = flags & 4; v.previewing = flags & 0x10; v.micFull = mic & 2;
    v.valid = true;
    return v;
}

// A name field of the save menu: a raw text of at most 15 bytes (byte 15 is the entry's used flag) ending in a NUL.
bool ReadName15(const Reader& m, u32 a, std::string& out)
{
    out.clear();
    for (u32 i = 0; i < 15; i++)
    {
        u32 ch;
        if (!m.R8(a + i, ch)) return false;
        if (ch == 0) return true;
        out += (char)ch;
    }
    return true;
}

MixerSaveState ReadMixerSave(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    MixerSaveState v;
    u32 G, mode, slot, ctx;
    if (!m.In(S, 0x13C) || !m.R32(kEU.gameBlock, G) || !m.R32(S + q.msMode, mode) || mode > 1 || !m.R8(S + q.msSlot, slot) || (slot > 2 && slot != 0xFF) || !Ptr(m, S + q.msContext, 0x108, ctx))
        return v;   // the context is the mixer's: without it the menu draws black (no buttons)
    if (!ReadName15(m, S + q.msName, v.name)) return MixerSaveState{};
    const u32 base = mode == 0 ? q.msSongs : q.msRingtones, stride = mode == 0 ? q.msSongStride : q.msRingStride;
    for (int j = 0; j < kMixerSaveSlots; j++)
    {
        u32 used;
        if (!m.R8(G + base + stride * (u32)j + 15, used)) return MixerSaveState{};
        v.used[j] = used != 0;
        if (v.used[j] && !ReadName15(m, G + base + stride * (u32)j, v.slotName[j])) return MixerSaveState{};
    }
    v.mode = (int)mode; v.slot = slot == 0xFF ? -1 : (int)slot;
    v.valid = true;
    return v;
}

ArtState ReadArt(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    ArtState v;
    u32 stroke, button, tool, swatch, page, brush, in;
    if (!m.In(S, 0xCE8) || !m.R32(S + q.arStroke, stroke) || stroke > 2 || !m.R32(S + q.arButton, button) || button > 9 || !m.R32(S + q.arTool, tool) || tool > 5 ||
        !m.R32(S + q.arSwatch, swatch) || swatch > 14 || !m.R32(S + q.arPage, page) || page > 15 || !m.R32(S + q.arBrush, brush) || brush > 2 || !m.R8(S + q.arIn, in))
        return v;
    v.cells.resize((size_t)kArtRows * kArtCols);
    for (u32 i = 0; i < v.cells.size(); i++)
    {
        u32 c;
        if (!m.R8(S + q.arWork + i, c) || c > 15) return ArtState{};
        v.cells[i] = (u8)c;
    }
    v.stroke = (int)stroke; v.button = (int)button; v.tool = (int)tool; v.swatch = (int)swatch; v.page = (int)page; v.brush = (int)brush;
    // The stylus's cell is stale (garbage) while it is off the canvas.
    const int row = m.S32(S + q.arRow), col = m.S32(S + q.arCol);
    if (in == 1 && row >= 0 && row < kArtRows && col >= 0 && col < kArtCols) { v.inCanvas = true; v.cellRow = row; v.cellCol = col; }
    v.valid = true;
    return v;
}

CardsState ReadCards(const Reader& m, u32 S)
{
    const auto& q = kEU.mg;
    CardsState v;
    u32 total, deck, box, msg, ask;
    if (!m.In(S, q.cdSize) || !m.R16(S + q.cdTotal, total) || total == 0 || total > (u32)kCardsMax || !m.R16(S + q.cdDeck, deck) || deck > total ||
        !m.R8(S + q.cdBox, box) || box > 1 || !m.R8(S + q.cdMessage, msg) || msg > 1 || !m.R8(S + q.cdQuitAsk, ask) || ask > 1)
        return v;
    // A hand: its places holding a card (a pointer into the object's card table, or 0).
    auto count = [&](u32 at, int& n) {
        n = 0;
        for (u32 i = 0; i < (u32)kCardsSlots; i++)
        {
            u32 p;
            if (!m.R32(S + at + q.cdSlotStride * i, p)) return false;
            if (p == 0) continue;
            if (p < S + q.cdCardFrom || p >= S + q.cdCardTo) return false;
            n++;
        }
        return true;
    };
    if (!count(q.cdHand, v.hand) || !count(q.cdOpponent, v.opponent)) return CardsState{};
    for (u32 i = 0; i < q.cdNameMax; i++)
    {
        u32 ch;
        if (!m.R8(S + q.cdName + i, ch) || (ch != 0 && (ch < 0x20 || ch > 0x7E))) { v.name.clear(); break; }
        if (ch == 0) break;
        v.name += (char)ch;
    }
    const int score = m.S32(S + q.cdScore), opp = m.S32(S + q.cdOppScore), chest = m.S32(S + q.cdTreasure), lim = (int)q.cdScoreMax;
    if (score < -lim || score > lim || opp < -lim || opp > lim || chest < 0 || chest > lim) return CardsState{};
    v.score = score; v.opponentScore = opp; v.treasure = chest;
    u32 phase, turn;
    if (!m.R8(S + q.cdPhase, phase) || phase > 15 || !m.R8(S + q.cdTurn, turn) || turn > 1) return CardsState{};
    v.phase = (int)phase; v.turn = (int)turn;
    v.total = (int)total; v.deck = (int)deck; v.box = box == 1; v.message = msg == 1; v.quitAsk = ask == 1;
    v.valid = true;
    return v;
}

VacuumState ReadVacuum(const Reader& m, u32 S)
{
    VacuumState v;
    u32 G, clog;
    if (!m.In(S, kEU.mg.vcSize) || !m.R8(S + kEU.mg.vcClog, clog) || clog > 1 || !m.R32(kEU.gameBlock, G)) return v;
    const int earnings = m.S32(G + kEU.hotel.stVacuum);
    if (earnings < 0 || earnings > 99999999) return v;
    v.clogged = clog == 1; v.earnings = earnings;
    v.valid = true;
    return v;
}

// The part of the screen the game shows (g.screen is set), apart from every other.
void ReadMinigame(const Reader& m, u32 S, u32 vt, GameState& g)
{
    MinigameState& a = g.minigame;
    switch (g.screen)
    {
    case GScreen::Autopsy: a.autopsy = ReadAutopsy(m, S); break;
    case GScreen::Detector: a.detector = ReadDetector(m, S); break;
    case GScreen::MooGoo: a.mooGoo = ReadMooGoo(m, S, vt == kEU.mg.vtMooGooIntro); break;
    case GScreen::Scratch: a.scratch = ReadScratch(m, S); break;
    case GScreen::Drencher: a.drencher = ReadDrencher(m, S); break;
    case GScreen::Crime: a.crime = ReadCrime(m, S); break;
    case GScreen::Mixer: a.mixer = ReadMixer(m, S); break;
    case GScreen::MixerSave: a.mixerSave = ReadMixerSave(m, S); break;
    case GScreen::ArtGallery: a.art = ReadArt(m, S); break;
    case GScreen::Cards: a.cards = ReadCards(m, S); break;
    case GScreen::Vacuum: a.vacuum = ReadVacuum(m, S); break;
    default: break;
    }
}

// ---- The front end (milestone 5). Heap objects: re-read every frame, vtable checked; each part reads apart from the others.
TitleState ReadTitle(const Reader& m, u32 T)
{
    const auto& f = kEU.front;
    TitleState t;
    u32 phase, btn;
    if (!m.In(T, 0x4C) || !m.R8(T + f.tPhase, phase) || phase > f.maxPhase || !m.R8(T + f.tButton, btn) || (btn != 0xFF && btn > f.maxButton)) return t;
    t.phase = (int)phase; t.menu = phase >= 2;
    t.button = btn == 0xFF ? -1 : (int)btn;
    t.valid = true;
    return t;
}

SettingsState ReadSettings(const Reader& m, u32 S)
{
    const auto& f = kEU.front;
    SettingsState v;
    u32 cur, vt, music, sfx, art, G;
    if (!m.In(S, f.oSlotSize) || !m.R32(S + f.oPage, cur) || cur == 0 || !m.In(cur, 0x18) || !m.R32(cur, vt)) return v;
    int page = -1;
    for (int i = 0; i < 4; i++) if (vt == f.vtPages[i]) page = i;
    u32 obj[4];
    // Only the current page's object must exist: the others are not built yet (null or stale).
    for (int i = 0; i < 4; i++) if (!m.R32(S + f.oPages + 4 * (u32)i, obj[i])) return v;
    if (page < 0 || obj[page] != cur) return v;
    if (!m.R32(kEU.gameBlock, G) || !m.R32(G + kEU.town.optMusic, music) || !m.R32(G + kEU.town.optSfx, sfx) || music > f.volMax || sfx > f.volMax || !m.R8(S + f.oArt, art) || art > 1) return v;
    v.page = page; v.music = (int)music; v.sfx = (int)sfx; v.art = art == 0;
    // The Concept page's buttons (their unlocked byte) and the Credits page's scroll: only while their page is up (the others are not built yet).
    u32 vc;
    if (page == 2 && m.R32(obj[2], vc) && vc == f.vtPages[2])
    {
        u16 bits = 0;
        bool ok = true;
        for (u32 i = 0; i < f.cpN && ok; i++)
        {
            u32 btn, flag;
            if (!m.R32(obj[2] + f.cpButtons + 4 * i, btn) || !m.In(btn, f.cpFlag + 1) || !m.R8(btn + f.cpFlag, flag) || flag > 1) ok = false;
            else if (flag) bits |= (u16)(1u << i);
        }
        if (ok) { v.conceptKnown = true; v.conceptBits = bits; }
    }
    u32 vr, pos, line;
    if (page == 3 && m.R32(obj[3], vr) && vr == f.vtPages[3] && m.In(obj[3], f.crSize) && m.R32(obj[3] + f.crPos, pos) && m.R32(obj[3] + f.crLine, line) && (int)line >= 0 && line < 100000)
    {
        v.creditsKnown = true; v.creditsPos = (int)pos; v.creditsLine = (int)line;
    }
    v.valid = true;
    return v;
}

// The slot time: 0x80000000 + the seconds since 2000-01-01 00:00 (the game's calendar: every fourth year leap).
bool DecodeSlotTime(u32 v, LoadRow& r)
{
    const auto& f = kEU.front;
    if (v < f.ldTimeBase) return false;
    const u32 t = v - f.ldTimeBase;
    u32 days = t / 86400, sec = t % 86400, year = 2000;
    for (;; year++)
    {
        const u32 len = year % 4 == 0 ? 366 : 365;
        if (days < len) break;
        days -= len;
        if (year >= 2000 + f.ldYearMax) return false;
    }
    static const u32 mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    u32 month = 0;
    for (; month < 12; month++)
    {
        const u32 len = mdays[month] + (month == 1 && year % 4 == 0 ? 1 : 0);
        if (days < len) break;
        days -= len;
    }
    if (month >= 12) return false;
    const u32 h = sec / 3600;
    r.month = (int)month + 1; r.day = (int)days + 1;
    r.pm = h >= 12; r.hour12 = h % 12 == 0 ? 12 : (int)(h % 12); r.minute = (int)(sec / 60 % 60);
    r.timeKnown = true;
    return true;
}

LoadScreen ReadLoad(const Reader& m, u32 S, bool wireless)
{
    const auto& f = kEU.front; const auto& t = kEU.town;
    LoadScreen v;
    u32 cur, tapped;
    if (!m.In(S, f.ldSize) || !m.R32(t.saveCurrent, cur) || (int)cur < -1 || (int)cur >= kLoadRows || !m.R32(S + f.ldTapped, tapped)) return v;
    for (int i = 0; i < kLoadRows; i++)
    {
        u32 used;
        if (!m.R8(t.saveUsed + (u32)i, used) || used > 1) return LoadScreen{};
        LoadRow& r = v.rows[i];
        r.used = used == 1;
        if (!r.used) continue;
        const u32 h = t.saveHeader + t.saveHeaderSize * (u32)i;
        u32 money, time, place;
        if (!m.R32(h + t.saveMoney, money) || money > 999999 || !m.R32(h + t.saveTime, time) || !m.R8(h + t.savePlace, place) || f.ldPlace + place >= kEU.menus.stringMax) return LoadScreen{};
        r.money = (int)money; r.place = (int)place; r.placeNameId = (int)(f.ldPlace + place);
        std::string name;
        bool ok = ReadCString(m, h + t.saveName, t.saveNameMax, name);
        if (ok && name.empty()) ok = ReadCString(m, f.ldNameDefault, t.saveNameMax, name);
        for (char ch : name) if (!GameTextByte((unsigned char)ch)) ok = false;
        r.name = ok ? GameTextToUtf8(name) : std::string();
        DecodeSlotTime(time, r);
    }
    v.green = (int)cur;
    v.tapped = tapped < (u32)kLoadRows ? (int)tapped : -1;   // stale garbage before the first tick
    v.wireless = wireless;
    v.valid = true;
    return v;
}

// Create-a-Sim: the bottom screen's page object by its vtable, then that page's fields.
CasState ReadCas(const Reader& m, u32 S)
{
    const auto& f = kEU.front;
    CasState c;
    u32 page, gender, N, vt, G;
    if (!m.In(S, f.casSize) || !m.R16(S + f.casPage, page) || page > f.casMaxPage || !m.R16(S + f.casGender, gender) || gender > 1 || !m.R32(S + f.casObj, N)) return c;
    // (A page object that does not read: as a page change in flight, the page and no fields.)
    if (N != 0 && !m.R32(N, vt)) vt = 0;
    static const int kObjOfPage[8] = {0, 1, 2, 3, 4, 5, 1, 3};   // the in-game pages 6 / 7 reuse the Hair and Clothes pages
    const int idx = kObjOfPage[page];
    // The page and its object disagree (or there is none) for the 2-4 frames a page change is in
    // flight (the old object closed, the new one made on a later update): the page reads, its
    // fields do not (`switching`).
    const bool switching = N == 0 || vt != f.vtCas[idx] || !m.In(N, f.sizeCas[idx]);
    // The committed name (G+0x53) is a field of its own: unreadable or not yet text (a fresh game's
    // block), it reads empty and the part stays valid.
    if (!m.R32(kEU.gameBlock, G) || !ReadCString(m, G + kEU.simName, kEU.simNameMax, c.simName) || c.simName.size() > (size_t)kCasName) c.simName.clear();
    for (char ch : c.simName) if (!GameTextByte((unsigned char)ch)) { c.simName.clear(); break; }
    c.simName = GameTextToUtf8(c.simName);
    u32 H;
    if (m.R32(S + f.casHero + 4 * gender, H) && H != 0 && m.In(H, f.heroSize))
    {
        u32 skin, hair;
        if (m.R16(H + f.heroSkin, skin) && skin <= f.maxSkin) c.skin = (int)skin;
        if (m.R16(H + f.heroHair, hair) && hair <= f.maxHairColour) c.hairColour = (int)hair;
    }
    u32 a, b, d;
    // A sprite of the page's list (null-ended, CActorSprite by its vtable): shown, and where.
    auto sprite = [&](u32 i, bool& shown, int& x, int& y) {
        u32 list, sp, sv, flags, sx, sy;
        if (i >= f.maxSprites || !m.R32(N + f.pgSprites, list) || list == 0) return false;
        for (u32 k = 0; k <= i; k++)
            if (!m.R32(list + 4 * k, sp) || sp == 0) return false;
        if (!m.R32(sp, sv) || sv != f.vtSprite || !m.R32(sp + f.sprFlags, flags) || !m.R32(sp + f.sprX, sx) || !m.R32(sp + f.sprY, sy)) return false;
        shown = flags & 1;
        x = (int)((int32_t)sx >> 12);
        y = (int)((int32_t)sy >> 12);
        return true;
    };
    // The page's arrows, shown only where the game can scroll.
    auto arrows = [&](u32 l, u32 r) {
        int x, y;
        if (sprite(l, c.canLeft, x, y) && sprite(r, c.canRight, x, y)) c.arrowsKnown = true;
        else c.canLeft = c.canRight = false;
    };
    // A shown sprite whose graphics are not there yet (the game uploads a page's pictures a frame
    // or more after its object is made, and again for the Clothes page's next cells).
    // Each sprite's resources and frame too (the HD draws the game's pictures from the ROM).
    if (!switching)
    {
        u32 list, sp, sv, flags;
        if (m.R32(N + f.pgSprites, list) && list != 0)
            for (u32 k = 0; k < f.maxSprites && m.R32(list + 4 * k, sp) && sp != 0; k++)
            {
                if (!m.R32(sp, sv) || sv != f.vtSprite || !m.R32(sp + f.sprFlags, flags)) continue;
                if ((flags & (f.sprLoading | 1)) == (f.sprLoading | 1)) c.loading = true;
                if (k >= (u32)kCasSprites) continue;
                c.spriteCount = (int)k + 1;
                CasSprite& s = c.sprites[k];
                s.shown = flags & 1;
                u32 fr, t, a, b, p;
                if (m.R8(sp + f.sprFrame, fr) && fr < 64) s.frame = (int)fr;
                if (m.R32(sp + f.sprRes, t) && m.In(t, 6) && m.R16(t, a) && m.R16(t + 2, b) && m.R16(t + 4, p) && a < f.maxRes && b < f.maxRes)
                {
                    s.tiles = (int)a; s.desc = (int)b;
                    s.pal = p < f.maxRes ? (int)p : -1;
                }
            }
    }
    switch (switching ? -1 : idx)
    {
    case 0:
    {
        // The Name page's editor is filled a frame or two after its object is made (its buffer
        // garbage until then, seen on the way back from another page): a buffer that does not
        // read (no NUL within the game's 7 + 1, a byte the game's charset does not draw) leaves
        // the page without its fields (nameKnown false), the rest of the part valid.
        bool ok = m.R16(N + f.nameKb, a) && a <= 2 && ReadCString(m, N + f.nameBuf, kCasName + 1, c.name);
        for (char ch : c.name) if (!GameTextByte((unsigned char)ch)) ok = false;
        if (!ok) { c.name.clear(); break; }
        c.nameLength = (int)c.name.size();
        c.name = GameTextToUtf8(c.name);
        c.keyboard = (int)a; c.nameKnown = true;
        break;
    }
    case 1:
        // A field out of its range (the page object still being filled): the page without its
        // fields (hairKnown false), as the Name page without its buffer; the rest valid.
        if (!m.R16(N + f.hairSel, a) || a > f.maxHairStyle || !m.R16(N + f.hairScroll, b) || b > f.maxHairScroll) break;
        if (!m.R16(N + f.hairPos, d) || d > f.maxHairStyle) break;
        c.hairStyle = (int)a; c.hairScroll = (int)b; c.hairPos = (int)d; c.hairKnown = true;
        arrows(f.hairArrowL, f.hairArrowR);
        break;
    case 2:
        if (!m.R16(N + f.face, a) || a > f.maxFace) break;   // (face -1: no fields)
        c.face = (int)a;
        break;
    case 3:
        if (!m.R16(N + f.thCat, a) || a > f.maxCat || !m.R16(N + f.thPage, b) || b > f.maxThPage || !m.R16(N + f.thItem, d) || d > f.maxThItem) break;
        c.category = (int)a; c.clothesPage = (int)b; c.item = (int)d; c.clothesKnown = true;
        arrows(f.thArrowL, f.thArrowR);
        {   // The game's ring: on the chosen item's cell when this page shows it.
            bool shown = false;
            int x, y;
            if (sprite(f.thRing, shown, x, y) && shown)
            {
                const int cx = x - (int)f.thCellX0, cy = y - (int)f.thCellY0;
                if (cx >= 0 && cy >= 0 && cx % (int)f.thCellDx == 0 && cy % (int)f.thCellDy == 0 && cx / (int)f.thCellDx < 3 && cy / (int)f.thCellDy < 2)
                    c.clothesCell = cy / (int)f.thCellDy * 3 + cx / (int)f.thCellDx;
            }
        }
        break;
    case 4:
        if (!m.R8(N + f.cfAsp, a) || a > 3 || !m.R8(N + f.cfAnswer, b) || b > 2) break;
        c.aspiration = (int)a; c.answer = (int)b; c.keepKnown = true;
        break;
    default: break;
    }
    c.page = (int)page; c.gender = (int)gender;
    c.switching = switching;
    c.valid = true;
    return c;
}

BootState ReadBoot(const Reader& m, u32 obj, bool top)
{
    const auto& f = kEU.front;
    BootState b;
    u32 mode, cnt, dur;
    if (!m.In(obj, 0x38) || !m.R8(obj + f.bgMode, mode) || mode > f.bgMaxMode || !m.R16(obj + f.bgCount, cnt) || !m.R16(obj + f.bgDuration, dur)) return b;
    b.mode = (int)mode; b.countdown = (int)(s16)cnt; b.duration = (int)(s16)dur; b.top = top;
    b.valid = true;
    return b;
}

FrontState ReadFront(const Reader& m, u32 P, u32 S, u32 sVt)
{
    const auto& f = kEU.front;
    FrontState v;
    u32 T = 0, tVt = 0;
    if (!m.R32(P + f.topObj, T) || T == 0 || !m.R32(T, tVt)) { T = 0; tVt = 0; }
    if (T && tVt == f.vtTitle) v.title = ReadTitle(m, T);
    if (T && tVt == f.vtVideo)
    {
        u32 movie;
        if (m.In(T, 0x30) && m.R32(T + f.vdMovie, movie) && movie <= f.vdMaxMovie) { v.video.movie = (int)movie; v.video.valid = true; }
    }
    if (sVt == f.vtSettings) v.settings = ReadSettings(m, S);
    else if (sVt == f.vtLoad || sVt == f.vtWireless) v.load = ReadLoad(m, S, sVt == f.vtWireless);
    else if (sVt == f.vtCasBottom) v.cas = ReadCas(m, S);
    if (sVt == f.vtBoot) v.boot = ReadBoot(m, S, false);
    else if (T && tVt == f.vtBoot) v.boot = ReadBoot(m, T, true);
    if (v.video.valid) v.kind = FrontKind::Video;
    else if (v.boot.valid) v.kind = FrontKind::Boot;
    else if (v.title.valid) v.kind = v.title.menu ? FrontKind::MainMenu : FrontKind::Title;
    else if (v.settings.valid) v.kind = FrontKind::Settings;
    else if (v.load.valid) v.kind = v.load.wireless ? FrontKind::Wireless : FrontKind::Load;
    else if (v.cas.valid) v.kind = v.cas.page == 4 ? FrontKind::KeepSim : FrontKind::CreateASim;
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
    g.hotel = ReadHotel(m);
    g.front = ReadFront(m, P, S, vt);
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
    if (g.screen == GScreen::Unknown)
        for (const auto& e : kEU.mg.screens) if (e.vt == vt && (e.ov == kAnyOverlay || e.ov == g.overlay)) { g.screen = e.screen; break; }
    // Settings, Load-a-Sim and DS Wireless (the front end's bottom objects, port/frontmenus.h): the
    // front end's GScreen, as the title and the main menu (CONSOLE's screen from RAM there).
    if (g.screen == GScreen::Unknown && (g.front.settings.valid || g.front.load.valid)) g.screen = GScreen::FrontEnd;
    // Create-a-Sim's bottom object and its page object both read (two vtables that agree: never an
    // overlay's colliding one) while the overlay number still lags behind the swap: Create-a-Sim,
    // as the RAM says, never None for a frame.
    if (g.screen == GScreen::Unknown && g.front.cas.valid && !g.front.cas.switching) g.screen = GScreen::CreateASim;

    switch (g.screen)
    {
    case GScreen::Panel:
    {
        int t = m.S32(S + kEU.panelTab);
        g.tab = (t >= 0 && t < 5) ? t : -1;
        // The tab's lists are read under a popup too (the HD menu stays drawn beneath it), the
        // selection only without one (the screen is the popup's then).
        const bool popupUp = pop >= 1 && pop <= 3;
        if (g.tab == 1) { u32 k; if (!popupUp && m.R32(S + kEU.pocketsObj, k)) g.selection = m.S32(k + kEU.pocketsSel); g.town.pockets = ReadPockets(m, S); }
        else if (g.tab == 3)
        {
            u32 k;
            if (!popupUp && Obj(m, S + kEU.relObj, kEU.menus.vtRel, 0xB4, k)) g.selection = m.S32(k + kEU.relSel);
            g.town.relations = ReadRelations(m, S);
        }
        else if (g.tab == 0) g.town.interact = ReadInteract(m, S);
        else if (g.tab == 2) g.town.goals = ReadGoals(m, S);
        if (popupUp) g.screen = GScreen::Popup;
        break;
    }
    case GScreen::Phone: g.selection = m.S8(S + kEU.phoneSel); g.page = m.S8(S + kEU.phonePage); g.town.phone = ReadPhone(m, S); break;
    case GScreen::Gift: g.town.gift = ReadGift(m, S); break;
    case GScreen::Dialogue: g.page = m.S16(S + kEU.dlgPage); g.speech = m.S16(S + kEU.dlgSpeech); g.dialogue = ReadDialogue(m, S); break;
    case GScreen::Catalogue: g.selection = m.S32(S + kEU.catSel); g.scroll = m.S32(S + kEU.catScroll); g.list = m.S32(S + kEU.catList); g.catalogue = ReadCatalogue(m, S); break;
    case GScreen::Build: g.build = ReadBuild(m, S); break;
    case GScreen::Cards: g.selection = m.S32(S + kEU.cardsSel); break;
    default: break;
    }
    // The hotel's own screens have no GScreen (the milestone-1 layouts do not know them): their vtable and overlay say it.
    {
        const auto& h = kEU.hotel;
        const int ov = g.overlay;
        if (vt == h.vtRegistry && ov == (int)h.ovRegistry) g.checkIn = ReadCheckIn(m, S, g.hotel);
        else if (vt == h.vtRoomInfo && ov == (int)h.ovRoomInfo) g.roomInfo = ReadRoomInfo(m, S, g.hotel);
        else if (vt == h.vtMusic && ov == (int)h.ovMusic) g.music = ReadMusic(m, S, g.hotel);
        else if (vt == h.vtStat && ov == (int)h.ovStat) g.stat = ReadStat(m, S, g.hotel);
        else if (vt == h.vtPlacement && ov == -1) g.placement = ReadPlacement(m, S);
    }
    ReadMinigame(m, S, vt, g);
    return g;
}

std::string MinigameLine(const GameState& g)
{
    const MinigameState& a = g.minigame;
    auto n = [](int v) { return std::to_string(v); };
    auto join = [](const std::vector<std::string>& v) { std::string r; for (const std::string& x : v) r += (r.empty() ? "" : ",") + x; return r.empty() ? std::string("-") : r; };
    char b[1024];
    switch (g.screen)
    {
    case GScreen::Autopsy:
    {
        const AutopsyState& v = a.autopsy;
        if (!v.valid) return "";
        std::vector<std::string> o, p;
        for (const AutopsyOrgan& x : v.organs) o.push_back(n(x.kind) + ":" + n(x.damage) + (x.freed ? ":freed" : ""));
        for (const AutopsyParasite& x : v.parasites) p.push_back(n(x.slot) + ":" + n(x.x) + ":" + n(x.y) + ":" + n(x.organ) + (x.eating ? ":eat" : ""));
        snprintf(b, sizeof b, "autopsy phase=%d sub=%d specimen=%d tool=%d earnings=%d dragged=%d held=%d alert=%d%d tear=%d,%d organs=%s parasites=%s", v.phase, v.sub, v.specimen, v.tool, v.earnings,
                 v.dragged, v.held, v.handAlert, v.tweezersAlert, v.tearX, v.tearY, join(o).c_str(), join(p).c_str());
        return b;
    }
    case GScreen::Detector:
    {
        const DetectorState& v = a.detector;
        if (!v.valid) return "";
        snprintf(b, sizeof b, "detector state=%d charge=%d range=%d slider=%d mismatch=%d card=%d find=%d finds=%d earnings=%d", v.state, v.charge, v.inRange, v.slider, v.mismatch, v.card, v.findType, v.findCount, v.earnings);
        return b;
    }
    case GScreen::MooGoo:
    {
        const MooGooState& v = a.mooGoo;
        if (!v.valid) return "";
        if (v.intro) return "moogoo intro";
        std::vector<std::string> mk, pl;
        for (const MooGooMonkey& x : v.monkeys)
        {
            std::string c;
            for (int i = 0; i < x.count; i++) c += (c.empty() ? "" : "/") + n(x.cards[i]);
            mk.push_back((x.eliminated ? "out:" : "") + (c.empty() ? std::string("-") : c) + ":" + n(x.bets[0]) + n(x.bets[1]) + n(x.bets[2]) + ":" + n(x.slots));
        }
        for (const MooGooPlayer& x : v.players)
        {
            std::string h;
            for (int c : x.hand) h += (h.empty() ? "" : "/") + (c == kMooGooHandEmpty ? std::string("_") : n(c));
            pl.push_back(n(x.nameId) + ":" + h + ":sel" + n(x.selected) + ":score" + n(x.score) + (x.ai ? ":ai" : "") + (x.outOfCards ? ":out" : "") + (x.bet ? ":bet" : ""));
        }
        snprintf(b, sizeof b, "moogoo phase=%d sub=%d timer=%d current=%d humans=%d round=%d hotseat=%d done=%d bet=%d stale=%d quit=%d%d winnings=%d deck=%d/%d monkeys=%s players=%s", v.phase, v.sub, v.timer,
                 v.current, v.humans, v.round, v.hotSeat, v.turnDone, v.betPlaced, v.stale, v.quit, v.quitAsk, v.winnings, v.deckNext, v.deckSize, join(mk).c_str(), join(pl).c_str());
        return b;
    }
    case GScreen::Scratch:
    {
        const ScratchState& v = a.scratch;
        if (!v.valid) return "";
        std::vector<std::string> bx;
        for (const ScratchBox& x : v.boxes) bx.push_back(n(x.picture) + ":" + n(x.erased) + (x.revealed ? ":open" : ""));
        snprintf(b, sizeof b, "scratch tier=%d prize=%d state=%d revealed=%d win=%d,%d active=%d hint=%d boxes=%s", v.tier, v.prize, v.state, v.revealed, v.winA, v.winB, v.active, v.hint, join(bx).c_str());
        return b;
    }
    case GScreen::Drencher:
    {
        const DrencherState& v = a.drencher;
        if (!v.valid) return "";
        snprintf(b, sizeof b, "drencher pump=%d target=%d/%d pumping=%d firing=%d targetmode=%d help=%d%d water=%d pressure=%d", v.pump, v.target, v.targetType, v.pumping, v.firing, v.targetMode, v.help, v.helpText, v.water, v.pressure);
        return v.helpId >= 0 ? std::string(b) + " helpid=" + n(v.helpId) : std::string(b);
    }
    case GScreen::Crime:
    {
        const CrimeState& v = a.crime;
        if (!v.valid) return "";
        snprintf(b, sizeof b, "crime help=%d text=%d drag=%d phase=%d angle=%d scare=%d armed=%d subdue=%d down=%d%d hit=%d", v.help, v.helpText, v.dragging, v.phase, v.angle, v.canScare, v.armed, v.canSubdue, v.scarePressed, v.subduePressed, v.hit);
        return v.helpId >= 0 ? std::string(b) + " helpid=" + n(v.helpId) : std::string(b);
    }
    case GScreen::Mixer:
    {
        const MixerState& v = a.mixer;
        if (!v.valid) return "";
        std::vector<std::string> u;
        for (int s : v.unlocked) u.push_back(n(s));
        snprintf(b, sizeof b, "mixer mode=%d phase=%d tracks=0x%02x keys=%d,%d,%d octave=%d instrument=%d sustain=%d sliders=%d,%d tempo=%d variation=%d song=%d events=%d/%d prep=%d preview=%d micfull=%d unlocked=%s", v.mode, v.phase, v.tracks,
                 v.keys[0], v.keys[1], v.keys[2], v.octave, v.instrument, v.sustain, v.sliderA, v.sliderB, v.tempo, v.variation, v.song, v.controlEvents, v.noteEvents, v.prep, v.previewing, v.micFull, join(u).c_str());
        return v.helpId >= 0 ? std::string(b) + " helpid=" + n(v.helpId) : std::string(b);
    }
    case GScreen::MixerSave:
    {
        const MixerSaveState& v = a.mixerSave;
        if (!v.valid) return "";
        std::vector<std::string> s;
        for (int j = 0; j < kMixerSaveSlots; j++) s.push_back(v.used[j] ? "\"" + TextEsc(v.slotName[j]) + "\"" : std::string("empty"));
        snprintf(b, sizeof b, "mixersave mode=%d slot=%d name=\"%s\" slots=%s", v.mode, v.slot, TextEsc(v.name).c_str(), join(s).c_str());
        return b;
    }
    case GScreen::Cards:
    {
        const CardsState& v = a.cards;
        if (!v.valid) return "";
        snprintf(b, sizeof b, "cards deck=%d/%d hand=%d opponent=%d score=%d,%d treasure=%d box=%d message=%d quit=%d phase=%d turn=%d name=\"%s\"", v.deck, v.total, v.hand, v.opponent, v.score, v.opponentScore, v.treasure, v.box, v.message, v.quitAsk, v.phase, v.turn, TextEsc(v.name).c_str());
        return b;
    }
    case GScreen::Vacuum:
    {
        const VacuumState& v = a.vacuum;
        if (!v.valid) return "";
        snprintf(b, sizeof b, "vacuum clogged=%d earnings=%d", v.clogged, v.earnings);
        return b;
    }
    case GScreen::ArtGallery:
    {
        const ArtState& v = a.art;
        if (!v.valid) return "";
        int painted = 0;
        u32 hash = 2166136261u;
        for (u8 c : v.cells) { if (c != 1) painted++; hash = (hash ^ c) * 16777619u; }
        snprintf(b, sizeof b, "art stroke=%d button=%d tool=%d swatch=%d page=%d brush=%d cell=%d,%d painted=%d hash=%08x", v.stroke, v.button, v.tool, v.swatch, v.page, v.brush, v.cellRow, v.cellCol, painted, hash);
        return b;
    }
    default: return "";
    }
}

const char* FrontKindName(FrontKind k)
{
    switch (k)
    {
    case FrontKind::Title: return "Title";
    case FrontKind::MainMenu: return "MainMenu";
    case FrontKind::Settings: return "Settings";
    case FrontKind::Load: return "Load";
    case FrontKind::Wireless: return "Wireless";
    case FrontKind::CreateASim: return "CreateASim";
    case FrontKind::KeepSim: return "KeepSim";
    case FrontKind::Boot: return "Boot";
    case FrontKind::Video: return "Video";
    default: return "None";
    }
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
    case GScreen::Drencher: return "Drencher";
    case GScreen::Autopsy: return "Autopsy";
    case GScreen::Detector: return "Detector";
    case GScreen::MooGoo: return "MooGoo";
    case GScreen::Scratch: return "Scratch";
    case GScreen::Crime: return "Crime";
    case GScreen::Mixer: return "Mixer";
    case GScreen::MixerSave: return "MixerSave";
    case GScreen::ArtGallery: return "ArtGallery";
    case GScreen::Trading: return "Trading";
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
    case GScreen::Drencher: return {Screen::Drencher};
    // The minigames with a scheme of their own; the others are the stick cursor's screens (Unknown).
    case GScreen::Autopsy: return {Screen::Autopsy};
    case GScreen::Detector: return {Screen::Detector};
    case GScreen::ArtGallery: return {Screen::Art};
    case GScreen::MooGoo: return {Screen::MooGoo};
    case GScreen::Crime: return {Screen::Crime};
    case GScreen::Scratch: return {Screen::Scratch};
    case GScreen::Mixer: return {Screen::Mixer};
    case GScreen::MixerSave: return {Screen::MixerSave};
    case GScreen::Trading: return {Screen::Trading};
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
    case Screen::SaveSlots: case Screen::Drencher: return pixels.screen == Screen::Unknown;
    // (The minigames: no rule of their own in the pixels.)
    case Screen::Autopsy: case Screen::Detector: case Screen::Art: case Screen::MooGoo: case Screen::Crime:
    case Screen::Scratch: case Screen::Mixer: case Screen::MixerSave: case Screen::Trading: return pixels.screen == Screen::Unknown || pixels.screen == Screen::None;
    // The boot's splashes (no GScreen of their own): Unknown in RAM, no HUD in the pixels.
    case Screen::Unknown: return pixels.screen == Screen::None && (g.front.kind == FrontKind::Boot || g.front.kind == FrontKind::Video);
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
