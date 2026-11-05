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
| Phone | `S+0x88` | u8 | page (0 menu, 2 messages, 0x0A leaving) | medium |
| Dialogue | `S+0x0C` (= u16 `S+0x36`) | u8 | speech index | medium |
| Dialogue | `S+0x34` | u16 | page, 1-based | medium |
| Cards | `S+0x2E0` | s32 | lifted card 0..8, -1 none | medium |
| Cards | `S+0xD0 + 0x14*i` | | card i lifted flag | medium |
| Cards | `S+0x4C` | u8 | deck count | medium |
| Cards | `S+0x306` / `S+0x2F7` | | 1 when help or quit is up / quit is up | low-medium |
| Social | `Q = *(0x0213E70C)`; `Q+4/+8/+0xC` | | answers left/middle/right; byte `+2` = 1 when chosen; stale after the game | low-medium |

## Backup signal

Sub-engine BG layer records at 0x02139618, 0x0213968C, 0x02139700 and 0x02139774 (stride 0x74)
change with the screen; the u16 at 0x02139700 reads 0x0009/0x010B/0x020B/0x030B/0x040B for the
panel tabs, 0x1A1A phone, 0x0187 dialogue, 0x1F02 catalogue, 0x1E0B build, 0x020E vacuum,
0x020A cards, 0x0213/0x0317 social, 0x1D06 Create-a-Sim, 0x0007 main menu, 0x0005 title. Gift and
Popup share 0x0009. Kept as a cross-check; the manager above is the source.

## Unknown

Not seen in any state: other shops, other minigames, later towns, the phonebook and messages,
Goals and Relationships sub-pages, save slot and Quit choices, Build and Gift selections,
Create-a-Sim pages, game over, credits. Not found: the highlighted Social answer, the dialogue's
page count, the Pockets counted item or slot, the Relationships list index, the title/menu
cursor, the Options sliders. `P+0x54C` (0x0219C388) is 8 with a popup and 7 between popups;
undecoded.
