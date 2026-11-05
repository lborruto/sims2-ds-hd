# The Sims 2 (Nintendo DS, 2005): catalogue of mechanics

Research notes for the single-screen, controller-first design. Every claim carries its source;
"(verified in game)" means it was checked on this project's headless melonDS build with the
European ROM (`ASJP`) and the project's save states, on 2026-11-04/05. Anything else is
reported, not verified. British spelling throughout.

What could not be read: the official instruction manual (no scan found at archive.org,
replacementdocs, gamesdatabase.org, Nintendo Europe or RegVault, whose page only has
screenshots); the three GameFAQs guides (40191, 44316, 43449) and the GameSpot and IGN reviews,
which refuse automated readers (HTTP 403); The Cutting Room Floor, which served no game content;
the Fandom wikis (HTTP 402), which are quoted here only through search-result snippets. The
StrategyWiki pages were read through the MediaWiki API; the guide's "Controls", "Sanity",
"Pockets", "Skills", "Items", "Alien Mummy Surgery", "Fighting Goons" and "Alien Fighting"
entries listed in its table of contents do not exist as pages.

Developer: Griptonite Games (an Amaze Entertainment studio), publisher Electronic Arts; released
24 Oct 2005 (USA), 4 Nov 2005 (Europe), 8 Dec 2005 (Japan); game codes ASJE/ASJP/ASJJ, 64 MB ROM,
256 KB flash save ([Sim2Team](https://sim2team.github.io/wiki/research/sims2nds),
[Wikipedia](https://en.wikipedia.org/wiki/The_Sims_2_(Nintendo_DS_video_game))). The publisher's
blurb counts "Seven creative, new Touch Screen mini-games"
([LaunchBox copy of the blurb](https://gamesdb.launchbox-app.com/games/details/77127-the-sims-2)).

---

## 1. Overview and main loop

**Premise.** The player's car breaks down in Strangetown; Jebediah S. Jerky offers to fix it and
sends the player to the only hotel, where a letter names them the new manager
([StrategyWiki, Arrival](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Arrival_in_Strangetown),
[Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)). The hotel is the base for the
whole game; the player directly controls one Sim in a behind-the-back third-person view
([Nintendo World Report](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds)).
It is "a reimagined linear progression game", not a port of the PC game ([Wikipedia](https://en.wikipedia.org/wiki/The_Sims_2_(Nintendo_DS_video_game))).

**The loop.** Keep the hotel clean, keep guests happy, keep your own sanity up, earn Simoleons,
build rooms, and complete the story goals handed out by the penthouse residents
([NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds),
[Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
"Most of the missions force you to build a specific room in your hotel, or to buy a new item"
([NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds)).

**Sanity** (one meter instead of the PC game's needs; [NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds)):
- Shown as a red bar in the HUD strip at the bottom of the top screen, next to the clock and the
  money (verified in game).
- Rises when the Sim uses beds, benches, showers, toilets; flashing red need icons above the Sim's
  head must be dealt with or sanity drains; it also drains from robots, criminal attacks, alien
  abduction, the desert and the freezer, whose hanging meat restores a lot of it. Four colour
  levels: red, green, yellow, silver. At zero the Sim may be jailed and lose §200 bail (Mamma
  Hogg pays if you cannot), or passes out and the Grim Cowboy returns them to the Manager's Suite
  ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)). StrategyWiki says aliens
  may abduct an empty-sanity Sim back to the suite; Sims cannot die
  ([StrategyWiki](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS))).
- Wrong answers in the social reaction game drain it; shower, nap or food restore it
  ([Pocket Gamer](https://www.pocketgamer.com/the-sims-2/review/)). Left unanswered, a reaction
  game drains the bar visibly within about 40 seconds (verified in game, `act-social` run to
  1,800 frames).
- A breakdown stops quests and dialogue until recovered ([Wikipedia](https://en.wikipedia.org/wiki/The_Sims_2_(Nintendo_DS_video_game))).
- Four aspirations (Fortune, Popularity, Romance, Party); each fills a meter through its own
  actions, and a fuller meter slows sanity loss ([StrategyWiki](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS))).
  A wall panel in the Managerial Suite shows aspirations and statistics
  ([Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel)).

**Money (Simoleons, §).** The early states show §2,000 (verified in game). Sources of income
([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS) unless noted): guests pay for
their stay, more for pricier furniture; neutral or happy guests pay in full and happy ones tip;
brochures sell for §15 each in the lobby and restock free; licence plates sell to the sheriff,
fuel rods to the City Hall clerk, gourds to the Saloon Chick, UFO parts, rocks and spare
furniture to Johnny Smith; paintings, music, gambling; copper, silver, gold and chocolate bars
sell in the Vault for more than in the shop; the Vault's money machine can get you arrested.
Restraining, calming or cheering up Sims yields gourds, fuel rods and sometimes Simoleons
([StrategyWiki, Socials](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Socials)).

**Hotel rating.** Shown at the bottom of the Goals tab; raised by building rooms and lounges,
checking guests in (they stay 1 to 3 real days) ([Sims Wiki walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough));
guest-room furniture improves it ([Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel));
building every room brings it to 100 % ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).

**Goals and missions.** Sims phone you or ask you to visit; penthouse residents (Frankie Fusilli,
Ava Cadavra, Optimum Alfred) give the story missions; some are compulsory, some optional;
optional ones pay money, aspiration, gourds or fuel rods, and refusing can anger the guest, who
then will not pay the full bill; some have deadlines (a time limit, a time of day, a room)
([StrategyWiki, Missions](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Missions)).
The current goal is a text line in the Goals tab with the giver's portrait, and a "Mission Goal
completed!" popup with an X closes when one is done (verified in game).

**The real-time clock.** "The DS Internal Clock dictates the date and time of day in the game"
([publisher blurb](https://gamesdb.launchbox-app.com/games/details/77127-the-sims-2);
[Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
What depends on it:
- Building a room takes 8 real hours ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS),
  [Pocket Gamer](https://www.pocketgamer.com/the-sims-2/review/)).
- Guests check out on set dates; the shop restocks weekly with a limited-edition item monthly;
  rain is random; some options exist only by day or night ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).
- Skill points sit in one place for 8 hours and move at 8 am, 4 pm and midnight
  ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS), [CheatHappens](https://www.cheathappens.com/11538-Nintendo_DS-Sims_2_The_cheats)).
- 24 licence plates, each tied to an hour and a place (e.g. midnight alien lab, 1 am Art Gallery,
  5 am cow shed, 6 pm penthouse) ([GIGA](https://www.giga.de/entertainment/sims-2-komplettloesung-fuer-nintendo-ds--01J5ASS535T438G934TQFBG5VR),
  SuperCheats snippet).
- Story timing: the buried-chest dig and the pyramid are at night; Optimum Alfred's room can only
  be entered 6 pm to 10 pm ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
- "Look for rewards when you play on your birthday" ([blurb](https://gamesdb.launchbox-app.com/games/details/77127-the-sims-2));
  what the reward is was not found.
- **Clock set back:** the concierge calls you a cheat and an alien invasion follows
  ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/));
  aliens appear at every alien spawn point in Strangetown ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).
  The project's emulator never moves the DS clock backwards for this reason (`CLAUDE.md`).

---

## 2. Controls

| Input | Effect | Source |
| --- | --- | --- |
| D-pad | Walks the Sim; "the d-pad works well enough" | [NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds); up walks forward (verified in game) |
| A | Triggers the first interaction offered on the bottom screen (walked to a chair: "Interact: Chair / Sit", A sat the Sim down; "Stand Up" then offered) and advances dialogue (verified in game); "Press A to shower", "Press A and spray" | [walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough) |
| B | No effect found in free roam, the store, Build Rooms, the gift popup or the phone; resumes from pause | verified in game; pause text |
| X / Y | Zoom in / zoom out | project notes (`CLAUDE.md`), checked on the emulator |
| L / R | Turn the camera continuously while held (about 156 frames for a full turn) | verified in game |
| Start | Pause: "Game Paused. Press START, A Button or B Button to resume play." | verified in game |
| Select | No visible effect in free roam | verified in game |
| Stylus | Every bottom-screen menu and every activity | see below |
| Microphone | Blow to clear the vacuum's clogged filter; record sounds for the music mixer | [Inside Games](https://www.inside-games.jp/article/2005/07/15/16689.html), [Nintendo UK](https://www.nintendo.com/en-gb/Games/Nintendo-DS/The-Sims-2-273355.html) |

Camera: follows behind the Sim; zooming in makes the wall in front fade out
([Inside Games](https://www.inside-games.jp/article/2005/07/15/16689.html)); walls stop the Sim
silently (project notes).

**Touch-only (verified in game):** the five tabs and the phone, the store catalogue, Build Rooms,
the Give Item popup, the social reaction game (A ignored) and Keelhaulin' Cards (A and B
ignored). Walking up to an object offers its interactions automatically on the bottom screen, so
A plus the D-pad covers plain object use without touching.

---

## 3. Screens

Top screen: the 3D world with the HUD strip at the bottom: time, money, Sanity bar. Bottom
screen: the panel. (All verified in game unless noted.)

**Town and hotel free roam.** Bottom: the panel with five tabs down the left edge (Interact,
Pockets, Goals, Relationships, Options), a phone icon top right, and three empty slots along the
bottom (purpose not seen; possibly quick slots). Interact shows a plumbob when nothing is near,
and "Interact: <thing>" with a list of actions, each with a play arrow, when something is: a
chair offers Sit; Johnny Smith in the store offers Buy; Jebediah in the street offers "What's
up?", "Impress", "Give Gift", with his mood shown as a thought bubble over his head on the top
screen. Popups ("Mission Goal completed!") must be closed with their X before the D-pad works
again (project notes).

**Conversations.** Bottom: the speaker's name, portrait and text, a play arrow bottom right;
A or the arrow advances.

**Pockets tab.** One wide slot at the top, a 3 x 2 grid of item slots, a hand button (drag an item
onto it to use it: the vacuum, the Super Drencher, the metal detector), what looks like a bin
bottom left, four counters for stackable collectables (icons resemble a fuel rod, a licence
plate, a gourd and a fourth item) and three silhouettes on the right (costumes or tools; the
Ratticator suit is "a costume for your sim to wear" per the
[fan wiki, Rat Cave](https://the-sims-2-ds.fandom.com/wiki/Rat_Cave), snippet). Six item slots
in total, which matters for the autopsy bug (section 4).

**Goals tab.** The current goal's text and the giver's portrait; the hotel score at the bottom
([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).

**Relationships tab.** A column of Sim portraits; for the selected one: Mood (e.g. Neutral),
Relationship (e.g. Acquaintance) and a secret line ("Jeb wants to revive his old farm..."). Best
friends unlock each Sim's secret ([StrategyWiki, Socials](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Socials)).

**Options tab.** Save, Quit, Music Volume slider, SFX Volume slider, a brightness icon top right.
Saving happens here, not through any button. Main menu: Create-a-Sim, Load-a-Sim, Settings,
DS Wireless.

**Phone.** Opened from the icon: date and time, Messages (count), Phonebook, Options, Exit, with
on-screen up/OK/down keys and an X. Missions arrive as calls or messages; the phone locates Sims
([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough),
[Pocket Gamer](https://www.pocketgamer.com/the-sims-2/review/)).

**Hotel status.** The wall panel in the Managerial Suite (aspirations, statistics); bills are paid
at the concierge's desk; the furnace must be fed nuclear rods "to keep the lights on"
([Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel)).

**City Hall, Build Rooms.** The clerk's window; "Build Rooms" lists rooms with prices on a wood
panel with up/down arrows and an X (the list is empty before the casino goal). Rooms are bought
here and Tristan Legend builds them in 8 hours ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).
Known rooms and prices: Snake Eyes Casino (first), The Vault §3,000, Government Lab §2,000,
Rat Cave §3,500 (free before the final fight if unbought), plus Jungle, Deluxe and Modern guest
rooms, Sax Lounge, Art Gallery, Asian Emporium, Spa and Gym, Lion Lounge (atrium), Bovine Shrine,
Secret Warehouse (basement) ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS),
[Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel), fan wiki snippets).

**Store (General Store, Johnny Smith).** Header with the shop name and your money, the selected
item's name, price and description, a 3 x 2 icon grid, Buy, Sell, up/down arrows, an X. Also
buys UFO parts, rocks and furniture; sells the metal detector, shovel, recharger, lipstick,
lottery cards, the Ghost Painting ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS),
[walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
Room items can be moved and rotated by touch ([Inside Games](https://www.inside-games.jp/article/2005/07/15/16689.html)).

**Give Item popup.** A 2 x 3 grid of your items, a gift icon, the recipient's portrait, an X.

**Travel.** No map found; the town is walked, the hotel floors (basement, lobby, 2nd floor,
penthouse, sun deck) use the lift ([Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel)).

---

## 4. Activities and minigames

### 4.1 Vacuum cleaner
- Start: pick up the red upright at the end of the desk, Pockets tab, drag it onto the hand
  (verified in game; [walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
- Top: the world; D-pad up walks forward, left/right turn, down backs up; dust piles are sucked up
  by walking into them. Bottom: the bowl: dust and finds swirl and drain through the hole; a
  counter top left ("50"), an X top right, a box bottom right; dragging a find (eraser, ring,
  gold ring seen) into the box keeps it (verified in game; [Pocket Gamer](https://www.pocketgamer.com/the-sims-2/review/)).
  Dust piles can hide sellable items ([NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds)).
- Clog: touching dust with the stylus clogs the filter: the bottom screen turns sepia, a mouth
  and arrow blink; blowing into the mic for 7 frames or more clears it; nothing else does
  (verified in game; [Inside Games](https://www.inside-games.jp/article/2005/07/15/16689.html);
  GameSpot's preview compared it to WarioWare's blowing).
- Also removes a ghost from a guest ([GIGA](https://www.giga.de/entertainment/sims-2-komplettloesung-fuer-nintendo-ds--01J5ASS535T438G934TQFBG5VR)).
- Story: goal 3 ("clean up 5 DUST piles"); dust is a standing chore afterwards (a maid can be
  called) ([Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel)).
- **Controller mapping considerations.** Movement is already D-pad. Needs: a "blow" button, a
  cursor or auto-target to drag finds to the box (a single "keep" button would do), and the
  stylus must never touch dust, so a cursor that only exists while "keep" is held avoids clogs.

### 4.2 Super Drencher (water pistol)
- Bought from Giuseppi Mezzoalto for §250; drag onto the hand; the "?" button and the other
  buttons explain it in game; A sprays ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
- Touch controls: "a pump to build up pressure, a targeting selector, and the fire button placed
  haphazardly across the screen"; fights felt luck-based ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/));
  "the touch pad and stylus pump up the Sim's plastic water gun" (Giant Bomb snippet).
- Refilled at the sun-deck pool, toilets, showers, sinks, the town-square well, the oasis
  ([fan wiki, Super Drencher](https://the-sims-2-ds.fandom.com/wiki/Super_Drencher), snippet).
- Works on aliens, Emperor Xizzle, Lord Mole, fires; not on robots or Jimmy the Neck
  ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
  Bug: shooting a skill point lets it be taken repeatedly ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).
  Player tip: the Sim walks twice as fast with it out; it is put away at doors (SuperCheats snippet).
- Story: goal to spray Xizzle, Honest Jackson's office fire, Lord Mole, and alien attacks throughout.
- **Controller mapping considerations.** Three touch widgets (pump, target, fire): map pump to a
  held trigger, target to the stick or D-pad with auto-aim, fire to A. The exact screen layout
  was not reached in the emulator and should be captured before mapping.

### 4.3 Metal detector (desert)
- Bought in the store; finds metal bars (copper, silver, gold, chocolate), UFO parts, mummified
  aliens, the buried chest ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS),
  [Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
- Player reports: a slider on the side sharpens the reading; limited battery (enough for most of
  the desert); the display reads "???" over the sarcophagus at the back of the oasis (SuperCheats
  snippets, unverified). "Players use the stylus to separate valuables from the detritus" (Giant
  Bomb snippet; may describe the vacuum).
- Desert hazards: aliens abduct you if you stay too long; Jimmy the Neck in daytime
  ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
- Story: three metal bars for Frankie, the sarcophagus for Horus, mummies for the lab.
- **Controller mapping considerations.** Walking is D-pad; the slider wants an analogue axis or
  two buttons; digging is likely one tap. Low precision.

### 4.4 Shovel
- Bought in the store; used once to bury Frankie's chest in the desert at night
  ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
  Input not described. One button.

### 4.5 Keelhaulin' Cards (Snake Eyes Casino)
- Verified in game from `act-cards`: top screen shows "Jun [cards] 8 [coins] 0 | Opponent
  [cards] 8 [coins] 0", a message line ("Draw a card!", then "Play your cards or discard!",
  "Switching Players"), a ship with stairs, and a treasure chest with its count. Bottom: the deck
  (tap to draw: hand 8 to 9), a "?" and an X beside it, a discard slot to the right, and the hand
  as two rows of up to five cards (pirate faces: captain, bearded, bandana, girl, skull; pale
  "ghost" versions; a "XXX" grog jug). After drawing, an orange arrow points at the stairs.
  Dragging a lone card to the stairs snaps it back; dragging a card to the discard slot discards
  it and ends the turn ("End of Turn. Touch for Opponent's turn."). Tapping a card raises it. A
  and B do nothing; "?" showed nothing when tapped or held.
- Rules: first to 200 coins wins; draw from the deck or the discard pile; bonus cards (cannons,
  grog) play immediately; three or more of one pirate go to the stairs and their points to the
  Treasure Chest; the round ends when you discard your last card and you collect the chest
  ([fan wiki, Keelhaulin'](https://the-sims-2-ds.fandom.com/wiki/Keelhaulin'), snippet).
  The GBA version's deck: 8 normal pirate types x 5, 8 ghost types x 1 (a ghost is playable once
  three of its pirate have been played), up to 3 special card types x 4 at higher levels
  ([Sim2Team, GBA internals](https://sim2team.github.io/wiki/research/sims2gba/internal/keelhaulincards));
  the DS deck is assumed similar, unverified.
- Casino also holds skill points by the table ([CheatHappens](https://www.cheathappens.com/11538-Nintendo_DS-Sims_2_The_cheats)).
- Story: optional money maker, available once the casino is built (goal 7).
- **Controller mapping considerations.** Pure drag-and-drop with few targets: a card cursor
  (left/right through the hand), A to raise, and "play" / "discard" buttons that move the card to
  the stairs or the discard slot, plus "draw" (deck) and "take discard". No timing.

### 4.6 Slot machine (Snake Eyes Casino)
- §1 per go, small rewards ([fan wiki, Snake-Eyes Casino](https://the-sims-2-ds.fandom.com/wiki/Snake-Eyes_Casino), snippet;
  [Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
  Input not described; presumably one tap. Trivial to map.

### 4.7 Scratch-off lottery
- Scratch-off tickets are one of three gambling options ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/));
  lottery cards are bought in the shop ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS));
  a player describes a lottery machine in the store, tickets at §1, §5 and §10, win by matching
  three pictures (SuperCheats snippet). Whether the scratching is a stylus rub was not confirmed.
- **Controller mapping considerations.** If it is a rub, a "scratch all" button is enough; no
  skill involved.

### 4.8 Mongoo Monkey cards (Snake Eyes Casino, GBA-slot unlock)
- Unlocked only by playing with The Sims 2 (GBA) in the DS's slot 2 (GameRevolution and
  SuperCheats snippets); listed among the casino's objects as "Moogoo Monkey Cards" (fan wiki
  snippet). The same card game sits on the Riverboat in The Urbz handheld
  ([Sims Wiki, Urbz walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Urbz:_Sims_in_the_City_(handheld)_walkthrough)).
  Rules not found. Cannot be reached on an emulator without a GBA cartridge image; low priority.

### 4.9 Painting (Art Gallery)
- Up to five paintings, four small canvases and one large, which guests buy
  ([NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds)); the gallery
  holds "5 x Blank Canvas" and a Music Panel (fan wiki objects snippet); buyers' reasons vary
  ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
- Drawn with the stylus; each stored painting is 48 x 32 pixels, 4 bits per pixel, from one of 16
  palettes of 15 colours; 20 paintings fit in the save; canvas index 0-4 plus a "favourite"
  slot ([Sim2Team, painting](https://sim2team.github.io/wiki/research/sims2nds/savefile/painting)).
- Tradable wirelessly ([blurb](https://gamesdb.launchbox-app.com/games/details/77127-the-sims-2)).
- Story: optional; build the gallery early for money ([GIGA](https://www.giga.de/entertainment/sims-2-komplettloesung-fuer-nintendo-ds--01J5ASS535T438G934TQFBG5VR)).
- **Controller mapping considerations.** Free drawing is the hardest case: a 48 x 32 canvas is
  small enough for a stick-driven cursor with A to paint, or mouse passthrough. The palette and
  tools are taps.

### 4.10 Music mixer (Sax Lounge, Lion Lounge)
- Keyboard-style mixers only in those two lounges, not buyable; a short sound recorded through
  the microphone can be put on the keys; three saved songs per game, replaceable
  ([fan wiki, Music Mixer](https://the-sims-2-ds.fandom.com/wiki/Music_Mixer), snippet).
  Songs become room music or phone ringtones; each room has a wall box to change its music
  ([NWR](https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds)); tracks can
  be swapped wirelessly ([Pocket Gamer preview](https://www.pocketgamer.com/the-sims-2/rooms-with-a-view)).
  The project's emulator confirms the game only opens the microphone in such situations.
- **Controller mapping considerations.** A keyboard of taps: map keys to buttons or a cursor;
  the mic record needs a real microphone or a sample file. Optional content.

### 4.11 Alien dissection (Government Lab)
- Build the lab (§2,000), dig up a mummified alien in the desert with the detector, choose the
  autopsy at the table (GameRevolution snippet; [Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS));
  "harvest alien organs for cash while keeping parasites from consuming the flesh"
  ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
  Needs at least one mummy in the six pocket slots; a counting bug can lock the game (TCRF bugs
  snippet). Payout about §150, or §365 for selling the alien (SuperCheats, user claim).
- **Controller mapping considerations.** Likely drag organs out under time pressure from
  parasites: needs a cursor plus grab button and some speed; unverified until reached.

### 4.12 Ratticator (Rat Cave)
- The rat suit unlocks with the Rat Cave; in it the Sim fights goons, robots and aliens by
  "hitting them with the batarang" and sim-fu "used by pressing the icon in the corner of the
  touch screen"; aliens and Jimmy the Neck flee on sight, robots still attack
  ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS), [Sims Wiki, Ratticator](https://www.thesimswiki.com/wiki/Ratticator)).
  Only the Ratticator beats robots, Jimmy's crew and Crime Spree Bob
  ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough));
  robots respawn after a kill (SuperCheats Q&A snippet).
- Story: compulsory: three robots in town, then Optimum Alfred and four named robots (PAL-8000,
  BAM-7000, POW-6000, EEP-5000) beaten three times each on the sun deck
  ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough);
  "3 to 5 times" per [CheatHappens](https://www.cheathappens.com/11538-Nintendo_DS-Sims_2_The_cheats)).
- **Controller mapping considerations.** Two touch icons (batarang, sim-fu) plus movement: two
  face buttons. Easy, if targeting is automatic.

### 4.13 Social reaction games (Impress, Calm Down, Cheer Up, Restrain, Romance)
- Verified in game (`act-social`): top screen shows the other Sim's name and relationship box and
  the Sim performing a gesture; bottom shows three icons in a pipe layout (High Five, Dance,
  Laugh), one lit; the HUD stays. Rules: each interaction has three gestures, each matched to the
  left, centre or right icon (Impress: one hand up = High Five, both up = Dance; Romance: Kiss,
  Serenade, Show off bod); pick the icon matching the gesture ([StrategyWiki, Socials](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Socials)).
  The first time only one icon flashes; later the prompts fade and you must remember the pairing
  ([StrategyWiki, Arrival](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Arrival_in_Strangetown),
  [walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
  Moods: heart = romance, red face = angry (calm down), broken heart = cheer up, purple heart =
  restrain; angry guests break furniture (debris), sad ones recolour it, loopy ones leave ick
  piles ([StrategyWiki, Socials](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Socials),
  [Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)). Wrong picks cost sanity
  ([Pocket Gamer](https://www.pocketgamer.com/the-sims-2/review/)).
- Story: the very first activity (calm Honest Jackson) and constant afterwards.
- **Controller mapping considerations.** Three fixed targets: map to left/up/right or three face
  buttons. Timing is lenient (the game waits), so this is the easiest conversion and the most
  used.

### 4.14 Cows (cow field) and chickens
- Three cows: tip, milk or give gifts ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS));
  milk = plain milk, tip then milk = milkshake; a chocolate bar turns a cow brown for chocolate
  milk (SuperCheats snippets); the mystic cowbell for Ava comes from a cow ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough));
  milkshakes feed Johnny Smith in the metal-bar goal ([GIGA](https://www.giga.de/entertainment/sims-2-komplettloesung-fuer-nintendo-ds--01J5ASS535T438G934TQFBG5VR)).
  These are Interact-menu actions, so A covers them. No chicken activity was found in the DS
  game; "Bigfoot Loves Chickens" belongs to the GBA version ([Sims Wiki, GBA walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(GBA)_walkthrough)).

### 4.15 Hotel chores and guests
- Fuel rods: pick up rods lying around and feed the basement furnace ("Add 1 Fuel Rod"; six fill
  it) (verified in game); 20 more for Optimum Alfred later.
- Brochures, bills at the desk, messes and ick piles, calling the maid
  ([Sims Wiki, hotel](https://www.thesimswiki.com/wiki/Strangetown_Hotel)).
- Check-in from the lobby: free rooms appear as keys and the guest names a checkout day; early
  checkout may tip ([StrategyWiki, Socials](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Socials)).
- Needs: shower (A or stylus), sleep, eat from the suite's fridge; the freezer's meat
  ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough),
  [Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)). No fishing or cooking
  minigame was found.
- Skill points: Body, Business, Charisma, Creativity, Mechanical, picked up where they float on
  the schedule; Mechanical 5 is required late ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).
- All of these are walk-up interactions: D-pad plus A.

### 4.16 Other
- "Simple Space Invaders-style" arcade games are mentioned once ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/));
  nothing else describes them; possibly the alien fights. Unverified.
- Ghost Painting (store): summons a ghost that drains sanity but helps socials; the vacuum
  removes it temporarily ([walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough)).
- Alien abduction: the Sim wakes in the suite, walks badly until sanity recovers
  ([Sims Wiki](https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS)).

---

## 5. Story outline (what the player hits, in order)

From the [Sims Wiki walkthrough](https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough),
cross-checked with [GIGA](https://www.giga.de/entertainment/sims-2-komplettloesung-fuer-nintendo-ds--01J5ASS535T438G934TQFBG5VR)
and [StrategyWiki](https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Arrival_in_Strangetown);
steps 1-4 verified in game.

1. Create-a-Sim; car trouble, Jebediah's talk (conversation, then the Impress reaction game);
   find the hotel (blue bed sign); the concierge's letter.
2. Fuel rods into the basement furnace (walk-up interactions).
3. **Vacuum**: clean five dust piles (first minigame, mic blow if clogged).
4. **Restrain** Honest Jackson (reaction game); the Manager's Suite; **shower**.
5. **Cheer up** Tristan Legend in the desert.
6. City Hall: **Build Rooms**, Snake Eyes Casino (first 8-hour build; casino unlocks
   **Keelhaulin' Cards** and the **slot machine**).
7. Alien tremor; buy the **Super Drencher** (§250) and spray Emperor Xizzle. Check in guests.
8. Frankie Fusilli: §1,000; deliver a cow head to Honest Jackson; **Drencher** on the office fire,
   **calm** Honest; hotel rating 20 % (rooms, lounges, **Art Gallery/painting**, guests); build
   The Vault (§3,000); **metal detector**, three metal bars; **shovel**, bury the chest at night;
   wire, calm Frankie; he is arrested.
9. Ava Cadavra: Bovine Shrine; **cow** (mystic cowbell); lipstick; pyramid at night; Horus's
   sarcophagus from the oasis (**detector**); branding iron from the sheriff; smash the Prime
   Heifer; gourds for the statue; Ava burns the penthouse.
10. Optimum Alfred: recharger; 20 fuel rods; satellite; **Drencher** on Lord Mole; **Ratticator**
    versus three robots; Mechanical 5 (skill points); break into his room 6-10 pm; potion at
    the satellite; final fight versus Alfred and four robots, three times each. Ending.

Optional throughout: gambling, lottery, painting, music, **dissection** (Government Lab), licence
plates, cows, guest moods.

---

## 6. Microphone, wireless, GBA slot

- **Microphone:** blow to unclog the vacuum (verified in game); record sounds for the music mixer
  and compositions ([Nintendo UK](https://www.nintendo.com/en-gb/Games/Nintendo-DS/The-Sims-2-273355.html));
  such sounds also serve as phone ringtones (GameSpot preview snippet). The game samples the mic
  only inside these situations (project notes).
- **Wireless:** "DS Wireless" is on the main menu (verified in game). "Trade characters or play
  mini-games wirelessly with your pals" ([Nintendo UK](https://www.nintendo.com/en-gb/Games/Nintendo-DS/The-Sims-2-273355.html));
  trade paintings and songs ([blurb](https://gamesdb.launchbox-app.com/games/details/77127-the-sims-2));
  "multiplayer competition" ([Worthplaying](https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/)).
  Which minigames are playable head-to-head was not found.
- **GBA slot:** The Sims 2 (GBA) in slot 2 unlocks Mongoo Monkey in the casino (GameRevolution,
  SuperCheats snippets). No other slot-2 bonus found.

---

## 7. Sources

Primary and in-game
- This project's emulator runs (states `s2`, `s7`, `sit-*`, `act-social`, `act-vacuum`,
  `act-vacuum-clog`, `act-cards`) and its `CLAUDE.md` notes.
- Nintendo UK product page: https://www.nintendo.com/en-gb/Games/Nintendo-DS/The-Sims-2-273355.html
- Publisher blurb (LaunchBox copy): https://gamesdb.launchbox-app.com/games/details/77127-the-sims-2
- Sim2Team research: https://sim2team.github.io/wiki/research/sims2nds ,
  https://sim2team.github.io/wiki/research/sims2nds/savefile ,
  https://sim2team.github.io/wiki/research/sims2nds/savefile/painting ,
  https://sim2team.github.io/wiki/research/sims2gba/internal/keelhaulincards

Wikis and guides
- The Sims Wiki: https://www.thesimswiki.com/wiki/The_Sims_2_for_NDS ,
  https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(Nintendo_DS)_walkthrough ,
  https://www.thesimswiki.com/wiki/Strangetown_Hotel , https://www.thesimswiki.com/wiki/Ratticator ,
  https://www.thesimswiki.com/wiki/Game_guide:The_Sims_2_(GBA)_walkthrough ,
  https://www.thesimswiki.com/wiki/Game_guide:The_Urbz:_Sims_in_the_City_(handheld)_walkthrough
- StrategyWiki (via API): https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS) ,
  https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Table_of_Contents ,
  https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Arrival_in_Strangetown ,
  https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Missions ,
  https://strategywiki.org/wiki/The_Sims_2_(Nintendo_DS)/Socials
- The Sims 2 DS fan wiki (search snippets only): https://the-sims-2-ds.fandom.com/wiki/Keelhaulin' ,
  https://the-sims-2-ds.fandom.com/wiki/Snake-Eyes_Casino , https://the-sims-2-ds.fandom.com/wiki/Music_Mixer ,
  https://the-sims-2-ds.fandom.com/wiki/Super_Drencher , https://the-sims-2-ds.fandom.com/wiki/Rat_Cave ,
  https://the-sims-2-ds.fandom.com/wiki/List_of_objects_by_location , https://the-sims-2-ds.fandom.com/wiki/Goals
- Wikipedia: https://en.wikipedia.org/wiki/The_Sims_2_(Nintendo_DS_video_game)
- GIGA (German walkthrough): https://www.giga.de/entertainment/sims-2-komplettloesung-fuer-nintendo-ds--01J5ASS535T438G934TQFBG5VR
- CheatHappens: https://www.cheathappens.com/11538-Nintendo_DS-Sims_2_The_cheats
- Snippet-only cheat sites (unverified user tips): https://www.supercheats.com/nintendods/thesims2hints.htm ,
  https://www.gamerevolution.com/guides/37350-the-sims-2-ds-cheats ,
  https://www.neoseeker.com/the-sims-2-handheld/cheats/ds/ , https://tcrf.net/Bugs:The_Sims_2_(Nintendo_DS)

Reviews and previews
- Nintendo World Report: https://www.nintendoworldreport.com/review/4491/the-sims-2-ds-nintendo-ds
- Worthplaying: https://worthplaying.com/article/2006/1/23/reviews/30427-nds-review-the-sims-2/
- Pocket Gamer review: https://www.pocketgamer.com/the-sims-2/review/ ; preview: https://www.pocketgamer.com/the-sims-2/rooms-with-a-view
- Inside Games (Japanese, 2005): https://www.inside-games.jp/article/2005/07/15/16689.html
- GameSpot preview (snippet): https://www.gamespot.com/articles/the-sims-2-updated-impressions-the-sims-on-the-ds/1100-6129181/
- Kotaku: https://kotaku.com/folks-think-the-sims-2-on-nintendo-ds-is-haunted-1847324114

Not reachable from here: GameFAQs guides https://gamefaqs.gamespot.com/ds/927468-the-sims-2/faqs
(40191, 44316, 43449), GameSpot and IGN reviews, TCRF main page, the official manual.
