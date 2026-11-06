# The Sims 2 DS (European release) — UI state in main RAM

What the game says is on screen, read from main RAM without looking at pixels. `src/gamestate.*`
reads it every frame (`App::game`; `--print-game` logs changes as `frame N: game <name>`).

Addresses are for the European release only (`kEU` table in `src/gamestate.cpp`). All values are
little-endian; main RAM is `nds->MainRAM[(addr - 0x02000000) & MainRAMMask]` (the DS uses 4 MB).
"Confidence" is how well a field held on the states it was checked on.

## How it was found

1. Save states of each situation were run headless; main RAM was dumped per state and labelled
   with the screen the pixels showed.
2. Bytes constant within a label and different across labels gave the first candidates (the
   loaded overlay id, a sub-engine layer record).
3. The tab field was followed across places through pointers until it led to one manager object,
   whose bottom-screen object's first word (a vtable pointer) names the screen.
4. Disassembling (capstone, ARM9) the code that writes the overlay id located the manager's
   "set screen" routine, the module-to-overlay table and the per-frame loop. This confirmed the
   fields below rather than guessing from dumps.
5. Selections were found by diffing the screen object between states differing by one cursor
   move.

Verified on 511 states: 179 for discovery, 84 per-place tab and phone states, 243 time series
and 5 held out. RAM leads the pixels by up to about 60 frames; a closing popup is still drawn
while RAM already says it is closing. Where RAM and pixels disagree (the "goal completed"
popups), RAM was right.

## The screen manager

`P` is the manager: `P = u32 @ 0x02139FA4` (always 0x0219BE3C in the states seen).
The screen manager's "set screen" routine is at 0x02046C24 and its per-frame loop near
0x02047F5C; slot 0 is the top screen, 1 the bottom.

| Field | Address / offset | Type | Meaning |
| --- | --- | --- | --- |
| bottom screen object `S` | `*(P+0xDAC)` | ptr | its first word is a vtable pointer that names the screen; 0 during the black loads between screens (title → Create-a-Sim, Create-a-Sim → town, quitting the cards), both screens then exactly black |
| top screen object | `*(P+0xDA8)` | ptr | world, title/menu or Create-a-Sim object |
| loaded overlay | `P+0xE34` | s32 | -1 none; needed because overlay vtables collide |
| popup | `P+0x8` | u8 | 0xFF front end, 0 none, 1 opening, 2 shown, 3 closing (still drawn) |
| bottom slot state | `P+0xE42` | u8 | 3 settled, 4 switching (bottom blank) |
| panel tab | `S+0x64` | u32 | 0 Interact, 1 Pockets, 2 Goals, 3 Relationships, 4 Options |

`S` is on the heap: read it again every frame and check its vtable; never cache it.

Decoding: read `P`, `S`, `vt = *S`, `ov = P+0xE34`, `popup = P+0x8`; the screen is looked up from
`vt` (or from `(ov, vt)` where overlay vtables collide). A Panel with popup in 1..3 is a Popup.
Options > Quit is a popup (popup = 2 over the Options tab).

| Vtable | Overlay | Screen |
| --- | --- | --- |
| 0x0213203C | -1 | Panel (tab at `S+0x64`); Popup when `P+0x8` is 1..3 |
| 0x02131F08 | -1 | Phone |
| 0x02135004 | -1 | Social (reaction game) |
| 0x0212F98C | -1 | Cards |
| 0x02131960 | -1 | Title / main menu (`P+0x8` = 0xFF) |
| 0x0214AA18 | 15 | Dialogue |
| 0x0214ABA4 | 6 | Gift |
| 0x0214C194 | 2 | Catalogue |
| 0x0214B1D0 | 1 | Build |
| 0x0214C658 | 17 | Vacuum |
| 0x021509B0 | 3 | Create-a-Sim and its confirmation |
| 0x0214B1C4 | 8 | Options > Save slots |
| 0x0214B8BC | 18 | Pockets > item in hand |

Not separable: title versus main menu, Create-a-Sim versus its confirmation. A screen not in the
table is reported as `Unknown(ov=N,vt=0x...)`: log it, it is a new screen to map.

## Selections (offsets from `S`)

| Screen | Field | Type | Meaning | Confidence |
| --- | --- | --- | --- | --- |
| Relationships | `S+0xD0` (also `+0xD2`) | u16 | selected Sim id | high |
| Pockets | `*(S+0x38)+0xE0` | s32 | key item 0..2, -1 none | medium |
| Catalogue | `S+0x254` | | selected item, absolute index (3 x 2 view) | high |
| Catalogue | `S+0x268` | | scroll: first visible item, step 3 | high |
| Catalogue | `S+0x258` | | list: 1 shop, 2 owned | high |
| Phone | `S+0x89` | u8 | highlighted entry 0..3 | high |
| Phone | `S+0x88` | u8 | page (0 menu, 2 when Messages opened, 0x0A leaving) | medium |
| Dialogue | `S+0x34` | u16 | speech index into the table at `0x0211F16C` (216 entries `{u32 pages ptr, u8 count}`); in one-off mode (`S+0x41` = 1) a bare string id | high |
| Dialogue | `S+0x36` | u16 | page counter, 1-based while a page is shown (0 before the first draw); last page = counter == count | high |
| Dialogue | `S+0x0C` | u8 | copy of the speech index (not read) | high |
| Dialogue | `S+0x3C` | u8 | speaker in one-off mode | medium |
| Dialogue | `pages[S+0x36-1]` | `{u16 string id, u8 speaker}` | text id and speaker of the shown page; speaker < 46 = Sim record, 0x3F none, 0x40 the player; name string id = 323 + u16 at `G+0x21DC+0x2C*k+0x18` | high |
| Cards | `S+0x2E0` | s32 | lifted card 0..8, -1 none | medium |
| Cards | `S+0xD0 + 0x14*i` | | card i lifted flag | medium |
| Cards | `S+0x4C` | u8 | deck count | medium |
| Cards | `S+0x306` / `S+0x2F7` | | 1 when help or quit is up / quit is up | low-medium |
| Social | `Q = *(0x0213E70C)`; `Q+4/+8/+0xC` | | answers left/middle/right; byte `+2` = 1 when chosen; stale after the game | low-medium |

## Status bar and conversation (`HudState`, `DialogueState`)

`G` = `*(0x0213E434)` = 0x021670A4 (static, the game-state block). Read by `ReadGameState` into
`GameState::hud` (any screen) and `GameState::dialogue` (screen Dialogue only), every field
range-checked (else `valid` false); `--print-hud` logs changes. Offsets are in `kEU`.

| Field | Address | Type | Meaning |
| --- | --- | --- | --- |
| money | `G+0x2F60` (0x0216A004) | s32 | plain binary |
| hour / minute | `G+0x4800` / `G+0x4804` (0x0216B8A4 / A8) | u32 | 0-23 / 0-59, the DS RTC the game shows (`--clock` drives it; a fresh state load shows the old time for a few frames) |
| sanity cur / max | `G+0x2164` / `G+0x2168` (0x02169208 / 0C) | u32, 20.12 fixed | bar = cur*104/max px |
| sanity tier | `G+0x216C` (0x02169210) | u32 0..3 | bar colour red, green, yellow, lavender |
| Sim name | `G+0x53` | ASCII, NUL within 8 | the `@1` text substitution |

Not mapped: motives and the mood meter; the progressive text reveal (the page is complete as soon as its counter shows).

## The town's records (`TownState`, `RecordRow`, `SimRecord`)

Read by `ReadGameState` into `GameState::town` (any screen; each part valid on its own, left out of
`GameState ==`, logged by `--print-town`) and by `ReadSimRow` / `ReadItemRow` / `ReadSimRecord`. Offsets in `kEU.town`.
`P` = `*(0x02139FA4)`, `G` = `*(0x0213E434)`.

| Part | Address | Meaning |
| --- | --- | --- |
| Record table | `0x02122A00 + 0x12 * row` (static) | rows of 9 u16: Sim k = row 2 + k (k < 46), item id = row 0x3B + id; +0/+2/+4 sprite tiles / descriptor / palette resources, +8 name string id (Sims 323 + k, items 463 + id), +0xA portrait / second resource, +0xE price (items) |
| Sim record | `G + 0x21DC + 0x2C * k` | u16 +0x18 Sim id, u32 +0x14 flags (bit 0 met, bit 1 secret info), u8 +0x1A mood (0..6), u8 +0x1B relationship level |
| Pocket slots | `G + 0x215C` s32 count (0..6), `G + 0x28 + 2i` u16 item ids | name = the row's +8 (must equal 463 + id, else the inventory is invalid), description 822 + id, icon = the row's descriptor |
| Counted items | `G + 0x24 + i` u8 (4) | names 1154 + i, descriptions 845 + i |
| Key items | bits 1..3 of the u32 at `G` | Vacuum, Super Drencher, Metal Detector: names 1968 + k, descriptions 1972 + k |
| Options | `G + 0x2F70` music, `G + 0x2F74` SFX | u32 20.12, 0..0x1000 |
| Popup | `P + 0x8` state (1 opening, 2 shown, 3 closing), `P + 0x10` kind (0 one X, 2 cross + tick, 3 none) | text `P + 0x34` (0x400 bytes), title `P + 0x434` (0x100), both NUL-terminated game text bytes (`GameTextToUtf8` in `romdata.h` decodes them); `P + 0xD88` pressed button, `P + 0xD8C` stylus down |
| Save slots | used `0x0213D848 + i` u8, current `0x0213D84C` s32 (-1 none) | headers `0x0213D7B8 + 0x30 * i`: +0x14 time word, +0x18 money s32, +0x1C name char[16], +0x2C place (string 0x43 + place) |

Held-out states for the checks are listed in `CLAUDE.md`; the full field map is in the research notes (milestone 3, town).

## The menus' lists (`TownState::interact` ... `::note`)

Read on the screen that shows them (a tab object is read only while its tab is up: its fields are stale otherwise), each part
`valid` on its own and `--print-town` logs each. Tab objects hang off `S` (heap, vtable checked); offsets in `kEU.menus`.

| Part | Path | Meaning |
| --- | --- | --- |
| Interact | `T = *(S+0x34)` (vtable `0x021321C4`); `E = *(T+0x300)` target (0 none); `E+0` actor (`+0xA` record index, `+0xC` type 2 Sim / 0xB object, `+0x130` its interface), `E+4` kind (2: name 321 "???") | name from the record table (Sim k < 57: 323 + k; object: 463 + id); interface: `+8` action count (0..4), `+0x10 + i` codes (label 1845 + code, at most 106); greyed from the row's selector `*(T+0x6C+4i)+0x94` (codes 2, 0x3A..0x3C by the code); `T+0x308` next / `T+0x30C` previous target (stale when no target: not read); `T+0x314` mode (1 stylus, 0 pad mode, 2 after an action), `T+0x318` highlighted row; `u16 S+0xD0` big portrait (0x1A5A the plumbob) |
| Pockets selection | `Pk = *(S+0x38)` (vtable `0x0213232C`) | `+0x74` slot (-1..count-1), `+0xE0` key item (-1..2), counted item = the ring `*(Pk+0x34)` (`+4` bit 0 shown, x `+0x50` 20.12: (x - 109) / 29) while both are -1 |
| Goals | `TG = *(S+0x3C)` (vtable `0x02132480`) | `+0x50C` entries (at most 44), `+0x510` lines (132), entry n at `+0x18 + 16n` {id, first line, lines, u8 mission}; `+0x514` selected, `+0x518` first line; mission text 1627 + id; hotel request i: record `G+0x2BE8+0x4C*i` (`+0x48` text, `+0x30` Sim, `+0x32` place; code only); giver portrait from `0x0211FB6C + 0x114*stage + 8*done`; footer when game flag 17 (`G+0x4814`): score = the weights (`0x0211F9EC[4k+2]`) of the 33 set bits of `G+0x4410` (bit 32 is the next word's bit 0; 100 on the complete save), ids 1841 / 1843 and 1842 |
| Relationships | `TR = *(S+0x40)` (vtable `0x02132580`) | `+0x1C` count (24), `+0x20` first, **`+0x24` selected** (the old `u16 S+0xD0` is the big portrait, not a Sim id: `GameState::selection` reads `TR+0x24`), `+0x28` the selected Sim's id (cross-checked), `+0x2C + 4i` pointers into the Sim records (met only); name 323 + id, description 389 + id, mood 184 + mood, level 192 + 7 * mood + level, portrait the Sim row's `+0xA` |
| Phone | `S` (Phone vtable): page `+0x88` (0..10), row `+0x89`, scroll `+0x8A`; `+0x56` shown slot (page 3 only), `+0x86C` contacts / `+0x870` Sim under the highlight (page 7 only) | inbox `G+0x303C+0x24i` (10): `+0x1A` present, `+0x1B` read, `+0x14` text id, `+0x18` sender Sim, `+0` args, `+0x10` arg flags; page 0's four string ids at `0x0211E99C` (u32); ringtone `G+0x31C5` |
| Give Gift | `S` (overlay 6): `+0x24` recipient actor (`+0xA` Sim index), `+0x4C` slot, `+0x34+4i` slot buttons (`+0x94` = cannot be given) | title 2422 with name 323 + Sim id |
| Note | `S` (vtable `0x0214A040`, overlay 11): `u16 +0x2C` text id | `@1` the player's name for 0x4A8, 0x509, 0x657; code only, no state reaches it |

## The hotel and the shops (`GameState::hotel` ... `::stat`)

`H = G + 0x41F8` is the hotel block. `GameState::hotel` reads on any screen; each screen's part is read only on its own
screen (vtable and overlay checked), valid on its own, every list capped. Places 0..32 name rooms (string `0x43 + place`).

- **Hotel block** (`HotelState`): rooms built = u32[2] at `G+0x4410` (bit = place, nothing above bit 32), permits
  purchasable = `G+0x4408`; static room table at `0x0211F9EC + 4 * place` {u16 permit price, u8 score weight, u8 parent place
  (0x21 none)}; hotel score = the weights of the built rooms (at most 100); guest slots `G+0x2BC0 + 8j`, j < 5 {u32 room
  (0x21 none), u8 Sim (0x3F none), u8 days booked}; guests stayed `G+0x84`; money `G+0x2F60`, earned `G+0x2F64`, spent
  `G+0x2F68` (each 0..999999); the song code of each room `u8 G+0x449C + place` (0x2A: default); the hero's place
  `u32 *(0x0213E580) + 0xF4`.
- **Registry / check-in** (overlay 4, vtable `0x0214A804`): `S+0x26` mode (1 registry, 0 guest), `S+0x25` guest Sim; the five
  rooms are the u8 table `0x0214A7A0` {29, 8, 15, 18, 20}. A row is usable when built, vacant and (Penthouse) game flag 0x15 is set;
  occupant = the slot holding the room, or the story Sim (6 / 4 / 14 for mission stage `u8 G+0x2F78` 1..3) in the Penthouse.
- **Room info** (overlay 12, `0x0214A9BC`): `u16 S+0x30` room, `u16 S+0x32` occupant (45, the Concierge, when vacant), `u8 S+0x25`
  occupied, `S+0x34..37` hint flags. Values: base 400 / 600 / 700 / 800 / 1000 (Small / Jungle / Modern / Deluxe / Penthouse);
  service charge `(sum of the furniture's record prices * 20) >> 12` (truncated, as the game's 20.12 product: 557 gives 2; the furniture
  of guest room `r` of the table `0x0211F9D4` is `u32 H + 0x58r + 0x54` items of `{id, x, y, rotation}` at `H + 0x58r + 4i`);
  score bonus `score * {0x2199, 0x2333, 0x24CC, 0x2666, 0x2800} / 4096` (truncated); total = sum, times 0x10CC / 4096 when
  `G+0x68` (aspiration) is Fortune (0). Check out: `u32 R_k+0x20` (R = `G + 0x21DC + 0x2C k`), days = (it - `G+0x480C`) / 86400 + 1,
  date = today (`G+0x47F4` / `+0x47F8`) + days, counted on a 365-day year (medium: the game's calendar was not decoded).
- **Music** (overlay 5, `0x0214ABA8`): `S+0x6C` count (at most 27), `S+0x2C + i` indices into the song table `0x0214ABBC` (4 bytes
  {code, gate, name id}, ended by code 0x2A; codes 0x2B..0x2D are recorded songs, no name id), `S+0x28` scroll,
  `S+0x70` playing (0xFF none), `S+0x74` rows shown.
- **Build Rooms** (overlay 1, `0x0214B1D0`): `S+0x40` count (at most 33), `S+0x4C + 4i` places, `S+0x3C` scroll (at most count - 3),
  `S+0x48` page (0 list, 1 confirm), `S+0x44` chosen (read on page 1). Price and parent from the room table; "too dear" is price > money.
- **Placement** (module 0x14, `0x021351A8`, no overlay): `S+8` mode, `S+0x24` room index, `S+0x5C` phase, `S+0x59` pocket slot,
  `S+0x5A` object, `S+0x6C` rotation; the room's furniture (at most 14) from the block above. Stylus only; dropping an object is not decoded.
- **Catalogue** (overlay 2, `0x0214C194`): `S+0x260` shop (0x33 the General Store), `u8 S+0x26C` layout, `u8 S+0x264` count (at most
  12), `S+0x3C + 4i` record rows (id + 0x3B; the record must name its own item), `S+0x254` selected, `S+0x258` mode (1 buy, 2 sell),
  `S+0x268` scroll, `u8 S+0x25D` pay enabled, `S+0x26D` / `S+0x26E` arrows, `S+0x214` the ring (its `+4` bit 0 shown); buy price twice the record's
  `+0xE`, sell price once; the owned places are the pockets (`G+0x215C`, `G+0x28`). A selection past the list (after a sale) reads as none.
- **Stat panel** (overlay 14, `0x0214B5A4`): `S+0x38` page 0..4, `S+0x3C` transition; aspiration `G+0x68`, level `G+0x6C`, points `G+0x70`
  (gauge = round(20 * points / `u32 0x02121DD8[level]`), 20 at level 3), skills `u8 G+0x5B + i` (0..10), aliens / goons / vacuum / metal `G+0x74..0x80`.

`--print-hotel` logs each part when it changes (`hotel`, `checkin`, `roominfo`, `music`, `build`, `placement`, `catalogue`, `stat`).
Checked by `tests/gamestate_test.cpp` (made-up RAM, garbage per part) and `tests/gamestate_check.sh` (held-out states).

## Backup signal

Sub-engine BG layer records at 0x02139618, 0x0213968C, 0x02139700 and 0x02139774 (stride 0x74)
change with the screen; the u16 at 0x02139700 reads 0x0009/0x010B/0x020B/0x030B/0x040B for the
panel tabs, 0x1A1A phone, 0x0187 dialogue, 0x1F02 catalogue, 0x1E0B build, 0x020E vacuum,
0x020A cards, 0x0213/0x0317 social, 0x1D06 Create-a-Sim, 0x0007 main menu, 0x0005 title. Gift and
Popup share 0x0009. Kept as a cross-check; the manager above is the source.

## Unknown

Not seen in any state: other shops, other minigames, later towns, the phonebook, the messages'
own pages (only the phone's page value as Messages opened), Goals and Relationships sub-pages, save slot and Quit choices, Build and Gift selections,
Create-a-Sim pages, game over, credits. Not found: the highlighted Social answer, the dialogue's
page count, the Pockets counted item or slot, the Relationships list index, the title/menu
cursor, the Options sliders. `P+0x54C` (0x0219C388) is 8 with a popup and 7 between popups;
undecoded.
