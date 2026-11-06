// The game's own screen manager, read from main RAM (EU table): which screen each DS screen
// shows and the selections on it, without looking at pixels. The RAM map is in docs/research/ram-map.md.
#pragma once
#include <string>
#include <vector>

#include "common.h"
#include "screens.h"

namespace sims2wide
{
enum class GScreen { Unknown, FrontEnd /*title or main menu*/, CreateASim, Panel, Popup, Phone, Dialogue, Gift, Catalogue, Build, Vacuum, Social, Cards, SaveSlots, Drencher,
               // The minigames (milestone 6; MinigameState): CONSOLE knows none of them yet (the stick cursor's screens).
               Autopsy, Detector, MooGoo /*intro and game*/, Scratch, Crime, Mixer, MixerSave, ArtGallery,
               ItemHand = Drencher /*the old name: Pockets > the hand with the Super Drencher, overlay 18*/ };

// The status bar (top screen rows 176-191), from the game-state block G. valid: G reads and every
// field is in range (hour 0..23, minute 0..59, sanity <= max, tier 0..3, a printable name).
struct HudState
{
    bool valid = false;
    int money = 0;
    int hour = -1, minute = -1;     // the DS clock the game shows (24 h)
    int sanity = 0, sanityMax = 0;  // whole units (the game keeps them 20.12 fixed)
    int tier = -1;                  // 0 red, 1 green, 2 yellow, 3 lavender bar
    std::string simName;            // the current Sim, ASCII, at most 8 characters
    bool operator==(const HudState&) const = default;
};

// The conversation on the bottom screen (only read when the screen is Dialogue). stringId is the
// text id of the shown page (strings_EN numbering); the text itself is not here.
struct DialogueState
{
    bool valid = false;
    int speech = -1;                // index into the game's speech table (S+0x34); a bare string id in one-off mode
    int page = -1, pages = 0;       // page shown (1-based) of pages
    int stringId = -1;
    int speaker = -1;               // Sim record index, 0x3F none, 0x40 the player
    int speakerNameId = -1;         // string id of the speaker's name; -1 for none / the player
    bool last = false;              // the shown page is the last one (a tap closes the conversation)
    bool operator==(const DialogueState&) const = default;
};

// A row of the game's shared record table (names, icons, prices of the Sims and the items).
// Resources are rom.bin ids; -1 where the row does not read.
struct RecordRow
{
    bool valid = false;
    int nameId = -1;                  // string id of the name
    int iconRes = -1;                 // +0xA: the portrait / second picture resource
    int tilesRes = -1, descRes = -1, palRes = -1;   // +0 / +2 / +4: the sprite triple (descRes names the sprite, spr_NNNNN)
    int price = 0;                    // items
    bool operator==(const RecordRow&) const = default;
};

// A Sim record of the game block G (relationships): who is met, mood and relationship level.
struct SimRecord
{
    bool valid = false;
    int simId = -1;                   // name = string 323 + simId
    int mood = 0, level = 0;          // strings 184 + mood, 192 + 7 * mood + level
    bool met = false, secret = false; // flags bit 0 / bit 1
    bool operator==(const SimRecord&) const = default;
};

// One inventory entry: a pocket slot (id = item id, count 1), a counted item (id = 0..3) or a key
// item (id = 0..2, count 1 when owned). Strings are ids (strings_EN numbering).
struct ItemRec
{
    int id = -1;
    int nameId = -1, descId = -1;
    int iconRes = -1;                 // slot items: the sprite descriptor resource (RecordRow::descRes); -1 for the others
    int count = 0;
    int tilesRes = -1, palRes = -1;   // slot items: the record's own tiles and palette (the Large Plant's palette is not the descriptor's neighbour)
    bool operator==(const ItemRec&) const = default;
};

struct InventoryState
{
    bool valid = false;
    std::vector<ItemRec> slots;       // the pockets, in slot order (at most 6)
    ItemRec counted[4];               // Nuclear Fuel Rods, License Plates, Strange Gourds, Alien Ship Parts
    ItemRec keys[3];                  // Vacuum, Super Drencher, Metal Detector (count 1 = owned)
    bool operator==(const InventoryState&) const = default;
};

// The popup the manager runs (P+0x8 in 1..3). text and title are the game's raw text bytes
// (GameTextToUtf8, romdata.h, decodes them); title is empty when the popup has none.
struct PopupState
{
    bool valid = false;
    int state = 0;                    // 1 opening, 2 shown, 3 closing
    int kind = -1;                    // 0 one X, 2 cross + tick, 3 no button
    std::string text, title;
    int pressed = -1;                 // button under the stylus (0 X / cross, 1 tick)
    bool pressing = false;
    // The box as the game places it (P+0x544 its y when shown, P+0x550 its width in pixels,
    // P+0x554 its height in 8-pixel tiles): where its buttons are (PopupButton).
    int boxY = 0, boxW = 0, boxTiles = 0;
    bool operator==(const PopupState&) const = default;
};

struct OptionsState
{
    bool valid = false;
    int music = 0, sfx = 0;           // 0..4096 (0x1000 = 100 %)
    bool operator==(const OptionsState&) const = default;
};

constexpr int kSaveSlotCount = 3;
struct SaveSlot
{
    bool used = false;
    int money = 0, place = -1;        // place: string 0x43 + place
    u32 time = 0;                     // the header's raw time word
    std::string simName;              // ASCII (blank when it does not read)
    bool operator==(const SaveSlot&) const = default;
};
struct SaveSlots
{
    bool valid = false;
    bool used[kSaveSlotCount] = {};   // the UI keys off this (slot[i] is read only when used)
    int current = -1;                 // the game's current slot, -1 none
    SaveSlot slot[kSaveSlotCount];
    bool operator==(const SaveSlots&) const = default;
};

// The menus' lists, each read apart from the game's tab / screen objects (heap: re-read every frame,
// vtable checked) and from the block G. Ids are strings_EN numbering or rom.bin resource ids; the
// words and pictures come from RomData. -1 = none.
// An Interact target: a Sim or an object of the shared record table.
struct TargetRef
{
    enum Kind { None, Sim, Object };
    int kind = None;
    int index = -1;                   // record index k (Sim k, object id)
    int nameId = -1;                  // string id of the name (321 "???" for an unknown Sim)
    int portraitRes = -1;             // the record's +0xA picture
    bool unknown = false;
    bool operator==(const TargetRef&) const = default;
};
struct InteractAction
{
    int code = 0;
    int labelId = -1;                 // 1845 + code
    bool greyed = false;              // disabled: the D-pad skips it
    int amount = -1;                  // the label's @1 as the game sets it (Buy, Payoff, Bail, Pay); -1: none
    bool operator==(const InteractAction&) const = default;
};
struct InteractState
{
    bool valid = false;
    TargetRef target, prev, next;     // kind None = no target (the title is just "Interact"); prev / next: the strip's neighbours
    int bigPortraitRes = -1;          // S+0xD0: the big picture (0x1A5A the plumbob, -1 none)
    std::vector<InteractAction> actions;   // 0..4 rows
    int padMode = 1;                  // T+0x314: 1 stylus, 0 pad mode (A held), 2 after an action until A is released
    int highlighted = -1;             // T+0x318 as the game holds it (the first enabled row, 0, outside pad mode); lit only when padMode is 0
    bool operator==(const InteractState&) const = default;
};

struct PocketsSel
{
    bool valid = false;
    int slot = -1;                    // Pk+0x74: selected pocket slot, -1 none
    int key = -1;                     // Pk+0xE0: selected key item 0..2, -1 none
    int counted = -1;                 // the selected counted item 0..3 (its ring), -1 none
    bool operator==(const PocketsSel&) const = default;
};

struct GoalEntry
{
    bool mission = false;             // a mission goal (id 0..33) or a hotel request (id 0..9)
    int id = -1;
    int textId = -1;                  // mission: 1627 + id; request: the record's own string
    int nameId = -1, placeId = -1;    // request: @1 / @2 (323 + Sim, 0x43 + place)
    int firstLine = 0, nLines = 0;    // lines of the 152 px box, 7 fit
    bool operator==(const GoalEntry&) const = default;
};
struct GoalsState
{
    bool valid = false;
    std::vector<GoalEntry> entries;
    int lines = 0;                    // total text lines
    int selected = -1, first = 0;     // selected entry, first visible line
    int portraitRes = -1;             // the mission giver's portrait (-1 when it does not read)
    bool footer = false;              // the hotel footer is shown (game flag 17)
    int footerId = -1, scoreId = -1, score = 0;   // 1841 / 1843, 1842 and the hotel score in percent
    bool operator==(const GoalsState&) const = default;
};

struct RelEntry
{
    int id = -1;                      // Sim id: name 323 + id, description 389 + id
    int nameId = -1, descId = -1;
    int moodId = -1, levelId = -1;    // 184 + mood, 192 + 7 * mood + level
    int portraitRes = -1;
    bool secret = false;              // the Secret Info button is shown
    bool operator==(const RelEntry&) const = default;
};
struct RelState
{
    bool valid = false;
    std::vector<RelEntry> sims;       // met Sims, in the game's order (at most 24)
    int first = 0, selected = -1;     // the top of the 4 portraits, the selected entry
    bool operator==(const RelState&) const = default;
};

struct InboxMsg
{
    int slot = -1;                    // 0..9
    int textId = -1;
    int sender = -1, senderNameId = -1;
    bool read = false;
    u32 args[4] = {};                 // @1..@4 (0 in @1: the player's name), argNum: 1 = a number
    int argNum[4] = {};
    bool operator==(const InboxMsg&) const = default;
};
struct PhoneState
{
    bool valid = false;
    int page = -1;                    // 0 menu, 1 incoming, 2 messages, 3 message, 4 options, 5 phonebook, 6..9 replies, 10 leaving
    int row = -1, scroll = -1;        // highlighted row, first visible row (4 fit)
    std::vector<InboxMsg> inbox;      // the messages present, in slot order
    int menuIds[4] = {};              // page 0's rows (the game's table)
    int shownSlot = -1;               // page 3: the inbox slot shown
    int ringtone = -1;                // the current one, 0..6
    int contacts = 0, contactSim = -1;   // page 7: rows and the Sim under the highlight
    int month = 0, day = 0;           // the header's date (G+0x47F4 / +0x47F8)
    // Pages 1, 3, 6, 8, 9: the game's row of page 3's "Next Message" (1 + its text's lines), of
    // the Exit of pages 1, 6, 8, 9 (the statics 0x0213E41C..0x0213E42C; stale off their page).
    int textEnd = -1;
    int maidCost = -1;                // page 5: "Call Maid - $@1" (the static 0x0213E430)
    int reply = -1;                   // page 6 the maid's answer (S+0x862), page 8 the prank's (S+0x868): a string id
    int caller = -1;                  // pages 8, 9: the Sim called (S+0x864); page 1: the caller (S+0x44)
    // Page 9: the Sim's mood (R+0x1A) and place (R+0x4), or away (R+0x10 == 0 or place 0x21:
    // "Sorry, @1, …").
    int callMood = -1, callPlace = -1;
    bool callAway = false;
    bool prank = false;               // page 7 lists for a prank call (S+0x866)
    // Page 7: the contacts in the game's order (the hotel's guests checked in, then the Sims met
    // and in town), as worked out from the Sim records; empty when that list disagrees with the
    // game's own count (S+0x86C) or highlighted Sim (S+0x870).
    std::vector<int> contactList;
    bool contactsKnown = false;       // contactList is the game's (it agreed)
    int contactsWait = 0;             // frames page 7 has shown 0 contacts the records do not agree with (the front end counts: the game builds its list over a few)
    InboxMsg incoming;                // page 1: the call's record (S+0x2C, the inbox entry's layout)
    bool customTones = false;         // page 4: a recorded ringtone slot (its name is not decoded)
    bool operator==(const PhoneState&) const = default;
};

struct GiftState
{
    bool valid = false;
    int recipient = -1;               // Sim record index
    int recipientNameId = -1;         // title: string 2422 "Give Item to @1"
    int slot = -1;                    // selected pocket slot, -1 none
    bool refused[6] = {};             // slot i cannot be given to this Sim
    bool operator==(const GiftState&) const = default;
};

struct NoteState
{
    bool valid = false;
    int textId = -1;
    bool playerName = false;          // @1 = the player's name
    bool operator==(const NoteState&) const = default;
};

// The town's menus' content, each part read apart (garbage in one leaves only that one invalid).
struct TownState
{
    InventoryState inventory;
    PopupState popup;
    OptionsState options;
    SaveSlots saves;
    InteractState interact;           // Panel / Interact
    PocketsSel pockets;               // Panel / Pockets
    GoalsState goals;                 // Panel / Goals
    RelState relations;               // Panel / Relationships
    PhoneState phone;                 // Phone
    GiftState gift;                   // Give Gift
    NoteState note;                   // a note or letter (overlay 11)
    bool operator==(const TownState&) const = default;
};

// ---- The hotel and the shops (milestone 4). The hotel block is readable on any screen; each menu's
// part is read only on its own screen (vtable and overlay checked), apart from the others.
constexpr int kPlaces = 33;           // place ids 0..32 name the rooms (string 0x43 + place)
constexpr int kNoPlace = 0x21;        // a guest slot's room when it holds none
constexpr int kNoSim = 0x3F;
constexpr int kSongNone = 0x2A;       // a room's song code when none is set
struct RoomRow
{
    int permit = 0;                   // the build permit's price (0: not a buildable room)
    int weight = 0;                   // its share of the hotel score, percent
    int parent = 0;                   // the place it is built in (0x21: none)
    bool operator==(const RoomRow&) const = default;
};
struct GuestSlot
{
    int place = kNoPlace;             // the room the guest holds
    int sim = kNoSim;                 // Sim record index
    int days = 0;                     // days booked
    bool operator==(const GuestSlot&) const = default;
};
struct HotelState
{
    bool valid = false;
    u64 built = 0, unlocked = 0;      // bit = place id: the room is built / its permit can be bought
    u64 building = 0;                 // bit = place id: its permit bought, its building not finished (G+0x4418 + 4 place >= the clock)
    RoomRow rooms[kPlaces];           // the static room table
    int score = 0;                    // the hotel score in percent: the weights of the built rooms
    GuestSlot slots[5];
    int guestsStayed = 0;
    int money = 0, earned = 0, spent = 0;   // the simoleons, and the stat panel's totals (0..999999)
    int songs[kPlaces] = {};          // the song code each room plays (kSongNone: the default)
    int place = -1;                   // the place the hero is in, -1 when it does not read
    bool operator==(const HotelState&) const = default;
};

// The Registry (mode 1) and the guest check-in (mode 0), overlay 4.
struct RegistryRow
{
    int place = -1;                   // name = string 0x43 + place
    bool built = false;
    int occupant = -1;                // the Sim in the room, -1 vacant
    bool available = false;           // built and vacant (the Penthouse also needs game flag 0x15): the row can be used
    bool infoOk = false;              // built (the Penthouse also needs game flag 0x15): the Registry opens its room info
    bool operator==(const RegistryRow&) const = default;
};
struct CheckInState
{
    bool valid = false;
    int mode = -1;                    // 1 Registry, 0 a guest checking in
    int guest = -1;                   // mode 0: the guest's Sim record index (the title is its name, 323 + guest)
    RegistryRow rows[5];
    int score = 0;                    // the hotel score (mode 1 shows it)
    bool operator==(const CheckInState&) const = default;
};

// The room's page (overlay 12): the strings 2458..2467 with their values.
struct RoomInfoState
{
    bool valid = false;
    int place = -1;                   // the room, name 0x43 + place
    bool occupied = false;
    int occupant = -1;                // the Sim in it, or the Concierge (45) when vacant (his portrait fills the vacant page)
    int moodId = -1;                  // string 2086 + the guest's mood; -1 vacant
    bool checkOutKnown = false;       // false for the Penthouse while the mission runs ("?/? (? days)")
    int days = 0, month = 0, day = 0; // the check out date and the days left; the date counts a 365-day year
    int baseValue = 0, serviceCharge = 0, scoreBonus = 0, total = 0;   // simoleons per day (total: the daily cost)
    bool hint = false;                // a hint box is up (the next tap closes it)
    // The hint's string (2594..2603), or -1: which line was tapped is not in RAM, so the frame loop
    // (main.cpp) fills it from the stylus's last point when `hint` rises (RoomInfoHintAt,
    // port/roominfo.h) and keeps it while the box is up; ReadGameState leaves it -1.
    int hintId = -1;
    bool operator==(const RoomInfoState&) const = default;
};

// The hotel's music panel (overlay 5).
constexpr int kMaxSongs = 27;
struct SongRow
{
    int index = -1;                   // the song table's index
    int code = -1;                    // the code G's per-room song byte holds
    int nameId = -1;                  // string id; -1 for a custom (recorded) song
    int custom = -1;                  // 0..2 for a recorded song, -1 otherwise
    std::string customName;           // a recorded song's name, the game's raw text (GameTextToUtf8)
    bool operator==(const SongRow&) const = default;
};
struct MusicState
{
    bool valid = false;
    std::vector<SongRow> list;        // the songs listed, in order (at most 27)
    int scroll = 0;                   // the first row shown (5 are)
    int playing = -1;                 // list index of the song playing, -1 none
    int place = -1, roomSong = kSongNone;   // the room and the song code its slot holds
    bool operator==(const MusicState&) const = default;
};

// Build Rooms (overlay 1), the City Hall clerk's list.
struct BuildEntry
{
    int place = -1;                   // name 0x43 + place
    int price = 0;                    // the permit
    int parentNameId = -1;            // string of the place it is built in
    bool affordable = false;          // price <= the money
    bool operator==(const BuildEntry&) const = default;
};
struct BuildState
{
    bool valid = false;
    std::vector<BuildEntry> places;   // the list the menu shows (at most 33)
    int scroll = 0;                   // the first of the 3 rows
    int page = 0;                     // 0 the list, 1 "Are you sure you want to buy this room?"
    int chosen = -1;                  // the entry on page 1, -1 on page 0
    int money = 0;
    bool operator==(const BuildState&) const = default;
};

// The furniture editor of a hotel room (module 0x14, no overlay), stylus only: what its objects are.
struct PlaceObject
{
    int id = -1;                      // item id, name 463 + id
    int x = 0, y = 0, rotation = 0;
    bool operator==(const PlaceObject&) const = default;
};
struct PlacementState
{
    bool valid = false;
    int mode = -1;                    // S+8: 1 a pocket item, 2 moving a placed one
    int room = -1;                    // index in the guest rooms table
    int phase = -1;                   // 0 fade-in, 1 interactive, 2 closing, 3 fade-out
    int pocketSlot = -1, object = -1; // chosen pocket slot / room object
    int rotation = 0;                 // of the carried object, 0..3
    std::vector<PlaceObject> objects; // the room's furniture (at most 14)
    bool operator==(const PlacementState&) const = default;
};

// The shop catalogue (overlay 2): the General Store's six cells and the owned places beside them.
struct ShopItem
{
    int row = -1;                     // the record row (id + 0x3B)
    int id = -1;                      // item id: name 463 + id, description 822 + id
    int nameId = -1, descId = -1;
    int iconRes = -1;                 // the sprite descriptor resource (RecordRow::descRes)
    int buyPrice = 0, sellPrice = 0;  // two times the record's price, once
    int tilesRes = -1, palRes = -1;   // the record's tiles and palette (not always the descriptor's neighbours: the Large Plant's palette is not 2484)
    bool operator==(const ShopItem&) const = default;
};
struct CatalogueState
{
    bool valid = false;
    int shop = -1;                    // the keeper's record index (0x33 the General Store, 0x29 / 0x2E / 0x2F / 0x30 food vendors)
    bool vendor = false;              // layout 1: a 2 x 3 grid, no arrows, no owned places
    std::vector<ShopItem> items;      // the list (12 in the store, 6 for a vendor)
    std::vector<ShopItem> owned;      // the pockets, as sell prices
    int selected = -1;                // into items (buy) or owned (sell)
    bool sell = false;                // an owned item is selected
    int scroll = 0;                   // the list index of grid cell 0 (0, 3, 6, 9)
    bool payEnabled = false, upEnabled = false, downEnabled = false;
    bool ring = false;                // the selection ring is shown
    int money = 0;
    bool operator==(const CatalogueState&) const = default;
};

// The Stat Panel (overlay 14): the page and what each page shows, read from G.
struct StatState
{
    bool valid = false;
    int page = 0;                     // 0 Main, 1 Aspirations, 2 Hotel Info, 3 Skills, 4 Game Stats
    int transition = 0;               // 0 settled, 1 fading out, 2 fading in
    int aspiration = 0, level = 0, points = 0, gaugeFrame = 0;   // Fortune 0, Romance 1, Popularity 2, Party 3; the gauge 0..20
    int skills[5] = {};               // Creativity, Business, Body, Charisma, Mechanical: 0..10
    int score = 0, guestsStayed = 0, earned = 0, spent = 0;
    int aliens = 0, goons = 0, vacuum = 0, metal = 0;
    bool operator==(const StatState&) const = default;
};

// ---- The minigames (milestone 6). Each minigame's part is read only on its own screen (vtable and
// overlay checked), apart from the others, valid on its own; the lists are capped. All of them are
// the game's own fields (docs/research/ram-map.md, "The minigames"); the port reads RAM only.

// Alien Autopsy (overlay 0): three organ slots, up to 8 parasites, the laser / hand / tweezers.
constexpr int kAutopsyOrgans = 3, kAutopsyParasites = 8;
constexpr int kAutopsyKindGone = 5;        // an organ slot's kind once the organ is removed
constexpr int kAutopsyDestroyed = 0xA00;   // an organ's damage when it is destroyed
struct AutopsyOrgan
{
    int kind = -1;                    // 0..4 the organ (the game's table of five), 5 removed
    int damage = 0;                   // 0..0xA00 (destroyed)
    bool freed = false;               // the cut has enclosed it: the hand can lift it
    bool present() const { return kind >= 0 && kind < kAutopsyKindGone; }
    bool operator==(const AutopsyOrgan&) const = default;
};
struct AutopsyParasite
{
    int slot = -1;                    // 0..7
    int x = 0, y = 0;                 // the sprite's position on the touch screen (off the screen while it walks in)
    int organ = -1;                   // the organ it is heading for / eating (0..2), -1 none
    bool eating = false;              // +5 damage per tick
    bool operator==(const AutopsyParasite&) const = default;
};
struct AutopsyState
{
    bool valid = false;
    int phase = 0;                    // 0 laser, 1 hand, 2 tweezers (the tool), 3 specimen done, 4 fade, 5 wait for the tear, 6 tearing
    int sub = 0;                      // 0 idle, 1 lasering, 3 dragging an organ, 4 organ into the tray, 6 holding a parasite, 7 parasite into the flask
    int specimen = 0;                 // 0 or 1 (the second is behind the closed membrane)
    int tool = 0;                     // 0 laser, 1 hand, 2 tweezers
    int earnings = 0;                 // $ banked this autopsy (paid when the second specimen is done; B / Start quit and forfeit it)
    AutopsyOrgan organs[kAutopsyOrgans];
    std::vector<AutopsyParasite> parasites;   // the live ones, in slot order (at most 8)
    int dragged = -1;                 // the organ the hand holds (sub 3 / 4), else -1
    int held = -1;                    // the parasite the tweezers hold (sub 6 / 7), else -1
    bool handAlert = false;           // an organ is freed: the hand's button flashes
    bool tweezersAlert = false;       // a parasite is eating: the tweezers' button flashes
    int tearX = -1, tearY = -1;       // phase 6: where the tearing drag is now
    bool operator==(const AutopsyState&) const = default;
};

// Metal Detector (overlay 9): walks the hero on the top screen; the bottom is the instrument.
constexpr int kDetectorFull = 0x100000;   // charge 256.0 (20.12)
constexpr int kDetectorSliderMax = 0x1000;
struct DetectorState
{
    bool valid = false;
    int state = 0;                    // 0 switching off, 1 power-on, 2 opening, 3 active, 4 closing, 5 "found" card
    int charge = 0;                   // 0..kDetectorFull (-0x40 per tick active, -48.0 per dig)
    bool inRange = false;             // a find is within reach of the hero
    int slider = 0;                   // the depth slider, 0..kDetectorSliderMax
    int mismatch = 0;                 // 0..128: |slider - the find's depth| (the scope's noise)
    bool card = false;                // the "Found:" card is up
    int findType = -1;                // the sensed find's type (0 lead rock ... 8 sarcophagus) when inRange, else -1
    int findCount = 0;                // the finds the place loaded
    int earnings = 0;                 // "Metal Detector Earnings" (G)
    bool operator==(const DetectorState&) const = default;
};

// MooGoo Monkey (overlay 10): the intro (one arrow) and the game.
constexpr int kMooGooMonkeys = 6, kMooGooPlayers = 3, kMooGooHand = 5, kMooGooCards = 8;
constexpr int kMooGooHandEmpty = 0xFF;
struct MooGooMonkey
{
    int cards[kMooGooCards] = {};     // the cards under it (v = 8 * monkey + rank; 0x30.. the specials), the first `count`
    int count = 0;
    int bets[kMooGooPlayers] = {};    // fruit each player put on it
    int slots = 0;                    // bet slots used (4 is full)
    bool eliminated = false;
    bool operator==(const MooGooMonkey&) const = default;
};
struct MooGooPlayer
{
    int nameId = -1;                  // string id ("Player 1", a monkey's name...)
    int hand[kMooGooHand] = {};       // card ids, kMooGooHandEmpty none
    int selected = -1;                // the hand index the cursor is on
    int score = 0;                    // its fruit on the monkeys still in
    bool ai = false, outOfCards = false, bet = false;
    bool operator==(const MooGooPlayer&) const = default;
};
struct MooGooState
{
    bool valid = false;
    bool intro = false;               // the intro: only its play arrow (238, 174) goes on
    int phase = 0;                    // 0 players prompt, 1 names, 2/3 hint, 4/5 betting banner, 6 first bets, 7 shuffle, 8 deal, 9/0xA round banner, 0xB/0xC turn banner, 0xD the turn, 0xE..0x11 elimination, 0x12/0x13 results, 0x14/0x15 Stale Bananas / Game Over, 0x17 out of cards
    int sub = 0;                      // the turn: 0 start, 1 pick a monkey to bet on, 2 bet placed, 3 play a card, 4 card picked, 5 pick a target monkey, 6 target picked, 7 draw, 8 end of turn, 9 animation
    int timer = 0;                    // frames left of the phase (30 = a half second), -1 none
    int current = 0;                  // the player whose turn it is (0 = you)
    int humans = 1;                   // 1..3 (the players prompt's choice + 1)
    int round = 0;                    // 1..3
    bool hotSeat = false, turnDone = false, betPlaced = false, stale = false;
    bool quit = false, quitAsk = false;   // quit chosen / the quit box is up
    int winnings = 0;
    int deckNext = 0, deckSize = 0;
    MooGooMonkey monkeys[kMooGooMonkeys];
    MooGooPlayer players[kMooGooPlayers];
    bool operator==(const MooGooState&) const = default;
};

// Scratch lottery (overlay 13): eight boxes, two winning pictures.
constexpr int kScratchBoxes = 8, kScratchPixels = 1024, kScratchNeeded = 684;
struct ScratchBox
{
    int picture = -1;                 // 0..6
    bool revealed = false;
    int erased = 0;                   // pixels scratched of kScratchPixels (kScratchNeeded reveals the box)
    bool operator==(const ScratchBox&) const = default;
};
struct ScratchState
{
    bool valid = false;
    int tier = 0;                     // 0 "Cash Mummy!" ($1), 1 "Milking For Moo-lah" ($5), 2 "The Emperor's Coffers" ($10)
    int prize = 0;                    // the tier's top prize (paid when it is won)
    int state = 0;                    // 1 playing, 2 won, 3 lost
    int revealed = 0;                 // boxes revealed (three of a winning picture win; five lose)
    int winA = -1, winB = -1;         // the two winning pictures (three boxes each)
    int active = -1;                  // the box being scratched, -1 none
    bool hint = false;                // the hint popup has been shown
    ScratchBox boxes[kScratchBoxes];
    bool operator==(const ScratchState&) const = default;
};

// Super Drencher (overlay 18): a tool screen over the world; the hero walks on the top screen.
constexpr int kDrencherPumpMax = 180;
struct DrencherState
{
    bool valid = false;
    int pump = 0;                     // the handle, 0..kDrencherPumpMax (stylus x - 50)
    int target = -1;                  // the locked target's record index, -1 none
    int targetType = -1;              // 2 a Sim (name 323 + target), 0xB an object (463 + target)
    bool pumping = false, firing = false;
    bool targetMode = false;          // the target button is lit
    bool help = false, helpText = false;   // help mode / a help text is up (the next tap closes it)
    int water = 0;                    // the tank, whole units of 100 (G)
    int pressure = 0;                 // the pressure, whole units of 125 (G)
    bool operator==(const DrencherState&) const = default;
};

// Crime (the Ratarang): scare, aim and throw, subdue.
struct CrimeState
{
    bool valid = false;
    bool help = false;                // help mode
    bool dragging = false;            // the aim slider is held
    int phase = 0;                    // the throw: 0 idle, 1 grabbed, 2 aiming, 3 released
    int angle = 0;                    // -90..90 degrees
    bool canScare = false;            // an unscared goon is within 30 units
    bool armed = false;               // a scared goon is within 50
    bool canSubdue = false;           // a clobbered goon is within 10
    bool scarePressed = false, subduePressed = false;
    bool hit = false;                 // the last throw hit
    bool operator==(const CrimeState&) const = default;
};

// Music Mixer: the keyboard and sequencer.
constexpr int kMixerKeys = 3, kMixerSongs = 8;
struct MixerState
{
    bool valid = false;
    int mode = 0;                     // 0 song, 1 ringtone, 2 resumed from the save menu
    int phase = 0;                    // 0 idle, 1 playing, 2 recording, 5 VOX (microphone) recording, 7 song select, 8 help, 9 a help text, 0xA the save prompt
    int tracks = 0xFF;                // bit t: track t+1 is audible
    int keys[kMixerKeys] = {-1, -1, -1};   // the keys held (0..12 C4..C5: note 60 + key + 12 * octave), -1 none
    int octave = 0;                   // -1..1
    int instrument = 1;               // 1, 2, 3 or 5 (VOX)
    bool sustain = false;
    int sliderA = 13, sliderB = 0;    // the filters, 0..26 (A 13 = off)
    int tempo = 0;                    // BPM
    int variation = 0;                // 0 I, 1 II
    int song = 0;                     // 0..7, 8 the ringtone
    int controlEvents = 0, noteEvents = 0;   // the recording's
    bool prep = false, previewing = false, micFull = false;
    std::vector<int> unlocked;        // the songs unlocked (at most 8), in the game's order
    bool operator==(const MixerState&) const = default;
};

// The mixer's save menu (module 0x13): a name and three slots.
constexpr int kMixerSaveSlots = 3;
struct MixerSaveState
{
    bool valid = false;
    int mode = 0;                     // 0 song, 1 ringtone
    int slot = -1;                    // the slot being burnt, -1 none
    std::string name;                 // the name typed so far (the game's raw text: GameTextToUtf8)
    bool used[kMixerSaveSlots] = {};
    std::string slotName[kMixerSaveSlots];   // raw text, as name
    bool operator==(const MixerSaveState&) const = default;
};

// Art Gallery's painting: a 48 x 32 canvas of palette indices.
constexpr int kArtCols = 48, kArtRows = 32;
struct ArtState
{
    bool valid = false;
    int stroke = 0;                   // 0 idle, 1 drawing, 2 touched outside
    int button = 0;                   // the tool button 0..9
    int tool = 0;                     // 0 pencil, 1 eraser, 2 line, 3 bucket, 4 rect, 5 filled rect
    int swatch = 0;                   // 0..14 (the colour is swatch + 1 of the page)
    int page = 0;                     // the palette page 0..15 (resource 6028: 16 colours each)
    int brush = 0;                    // 0..2
    bool inCanvas = false;            // the stylus is on the canvas
    int cellRow = -1, cellCol = -1;   // its cell (inCanvas)
    std::vector<u8> cells;            // kArtRows * kArtCols palette indices (1 = white), row by row: what is drawn, a stroke in progress included
    bool operator==(const ArtState&) const = default;
};

// Keelhaulin' Cards (module 0x10, no overlay; GScreen::Cards). The hands are ten slots each (a
// card pointer into the object's own card table, 0 when the place is empty); the counts are what
// the score board shows beside each player's name.
constexpr int kCardsSlots = 10, kCardsMax = 60;
struct CardsState
{
    bool valid = false;
    int deck = 0, total = 0;          // cards left in the draw deck, of the game's total
    int hand = 0, opponent = 0;       // the cards in the player's hand and the opponent's (0..10)
    int score = 0, opponentScore = 0; // the players' scores (the first to 200 wins; a cannon can take them below 0)
    int treasure = 0;                 // the treasure chest's points (the round's winner takes them)
    bool box = false;                 // a box is up over the table (help, the quit question, a message)
    bool message = false;             // the "End of Turn. Touch for @1's turn." message (any touch goes on)
    bool quitAsk = false;             // "Are you sure you want to quit?" (its tick and cross: kCardsQuitYes / No)
    std::string name;                 // the player's name as the board shows it (ASCII; empty: unreadable)
    bool operator==(const CardsState&) const = default;
};

// The vacuum (overlay 17; GScreen::Vacuum): the filter's clog (touching dust; a blow clears it) and
// the stat panel's "Vacuum Earnings" (G).
struct VacuumState
{
    bool valid = false;
    bool clogged = false;
    int earnings = 0;
    bool operator==(const VacuumState&) const = default;
};

struct MinigameState
{
    CardsState cards;
    VacuumState vacuum;
    AutopsyState autopsy;
    DetectorState detector;
    MooGooState mooGoo;
    ScratchState scratch;
    DrencherState drencher;
    CrimeState crime;
    MixerState mixer;
    MixerSaveState mixerSave;
    ArtState art;
    bool operator==(const MinigameState&) const = default;
};

struct GameState
{
    bool valid = false;            // the manager pointer chain read cleanly
    // The manager reads but its bottom-screen object pointer (P+0xDAC) is null: the game is
    // between screens, in a black load (valid stays false). Measured on every load seen: the
    // title -> Create-a-Sim, Create-a-Sim -> town, quitting the cards.
    bool between = false;
    GScreen screen = GScreen::Unknown;
    u32 vtable = 0; int overlay = -1;   // raw, for Unknown logging
    u8 popup = 0;                  // P+0x8 (0xFF front end, 0 none, 1 opening, 2 shown, 3 closing)
    bool settled = false;          // P+0xE42 == 3 and popup 0, 2 or 0xFF (none sliding)
    int tab = -1;                  // Panel: S+0x64 (0..4)
    int selection = -1;            // per screen: catalogue S+0x254, phone S+0x89, cards S+0x2E0, relationships *(S+0x40)+0x24 (list index), pockets *(S+0x38)+0xE0
    int scroll = -1, list = -1;    // catalogue S+0x268, S+0x258
    int page = -1, speech = -1;    // dialogue page counter S+0x36, speech index S+0x34; phone page S+0x88 in `page`
    HudState hud;                  // the status bar's values (any screen with the world's G block valid)
    DialogueState dialogue;        // filled only when screen == Dialogue
    TownState town;                // inventory, popup, options, save slots (any screen); logged apart (--print-town)
    // The hotel and the shops (logged apart: --print-hotel). hotel: any screen; the others only on their own.
    HotelState hotel;
    CheckInState checkIn;
    RoomInfoState roomInfo;
    MusicState music;
    BuildState build;
    PlacementState placement;
    CatalogueState catalogue;
    StatState stat;
    MinigameState minigame;        // the minigames (milestone 6): the screen's part only; logged apart (--print-minigame)
    // Screen identity only: hud and dialogue change every minute / page and are logged apart
    // (--print-hud), so --print-game stays one line per screen change.
    bool operator==(const GameState& o) const
    {
        return valid == o.valid && between == o.between && screen == o.screen && vtable == o.vtable && overlay == o.overlay &&
               popup == o.popup && settled == o.settled && tab == o.tab && selection == o.selection && scroll == o.scroll &&
               list == o.list && page == o.page && speech == o.speech;
    }
};

// Main RAM as melonDS holds it: nds.MainRAM and nds.MainRAMMask.
struct RamView { const u8* ram; u32 mask; };

GameState ReadGameState(RamView ram);           // EU table
// The shared record table's rows: Sim k (0..45) and item id (0..0x1FF). `valid` means the row
// was readable (in range, in RAM), not that it is a real record: a slot the game left zeroed
// reads valid with empty fields. Callers check the name id (a string id in the table) before
// they use a row.
RecordRow ReadSimRow(RamView ram, int k);
RecordRow ReadItemRow(RamView ram, int id);
// The record table's string ids (the same in every language): Sim k's name kSimNameBase + k (k
// 0..45: up to kSimNameLast; kSimNameUnknown "???" for a target not met), item id's name
// kItemNameBase + id and description kItemDescBase + id (ids 0..kItemIdLast: the names end at the
// game's "end marker" 794). What the menus' measures (--measure-menu) walk.
constexpr int kSimNameUnknown = 321, kSimNameBase = 323, kSimNameLast = 368;
constexpr int kItemNameBase = 463, kItemDescBase = 822, kItemIdLast = 330;
SimRecord ReadSimRecord(RamView ram, int k);    // k 0..45, from G
// Every page's string id in the game's speech table (static data, any state of the European
// ROM), in table order, repeats kept; empty when the table does not read. For checks: the
// conversations' texts in each language measured against the HD box (--measure-string all).
std::vector<int> SpeechStringIds(RamView ram);
// One line of the minigame the screen shows (the part's fields as k=v), empty when the screen is
// none or its part does not read: what --print-minigame logs when it changes.
std::string MinigameLine(const GameState&);
const char* GScreenName(GScreen);
std::string GameStateName(const GameState&);   // "Panel/Goals", "Popup", "Unknown(ov=8,vt=0x0214B1C4)"
// CONSOLE's screen from the game's: Panel with its tab, the rest by name; not valid, a Panel
// whose tab is not 0..4, or an unmapped screen: Unknown (the DS's buttons, the stick cursor).
// Options > Quit is a popup over the Options tab (tab 4): Unknown too, since its yes / no
// buttons are not kPopupClose (the stick cursor answers it, or the DS's A).
ScreenState ToScreenState(const GameState&);
// What CONSOLE shows and steers: ToScreenState when RAM reads, else the pixels' answer. Without
// the HUD and with both screens black (`black`: ScreensBlack), the game between screens
// (GameState::between) or an Unknown is Loading (the black loads: the bare backdrop); an Unknown
// without the HUD that is not black is None (an unmapped front-end screen: the front end's view).
// The pixels' None (RAM not read at all) stays None.
ScreenState ConsoleScreenOf(const GameState&, ScreenState pixels, bool hud, bool black);
// Whether the pixel tracker's answer (screens.h) is what the game's screen looks like:
// the same, or None for FrontEnd / CreateASim (no HUD), Unknown for SaveSlots / Drencher.
bool PixelsAgree(const GameState&, ScreenState pixels);
}
