# sims2-ds-wide — notes for Claude

The Sims 2 (Nintendo DS, 2005) in one window, in the spirit of
[Pokémon FullDex](https://github.com/mamesj/pokemon-fulldex). The melonDS 1.1 core (submodule,
plus the small looks hooks in `patches/melonDS/`) runs the game; a small SDL2 frontend (`src/`) lays out the two screens and
picks the layout from what the screens show (CONSOLE by default: the game played like a console game). No decompilation or recompiler: none exists for
this game and plain melonDS is fast enough. `origin` = `lborruto/sims2-ds-wide` (private until
the owner makes it public; write everything as if it were public already).

## Permanent rules

- NEVER commit game content: ROMs (`.nds`), saves (`.sav`), save states (`.st`), dumps of the
  game's files, code, graphics or sound. `roms/` and `shots/` are git-ignored; never force-add.
  Screenshots in `docs/screenshots` are fine. Never write where to get a ROM, or the file names
  of ROM sets.
- No personal paths, host names or work addresses in anything committed.
- Commit as `luca.borruto@gmail.com` (set in the local git config), never the work address.
- Never push `main` without the owner's go-ahead.
- Keep the README short and for players; technical detail goes here and in CONTRIBUTING.md. A PR
  that changes what README.md or this file says updates them.
- CONSOLE stays the default layout (the owner plays it all the time; AUTO, FOCUS, SIDE and STACKED are chosen by hand). melonDS stays the upstream 1.1 tag: changes go in
  `patches/melonDS/` (applied by CMake) and only with the owner's agreement; never a fork.
- One task = one clear commit; `ninja -C build sims2wide` and a headless run before each commit.

## Layout

| Path | Role |
| --- | --- |
| `src/main.cpp` | Command line, starting the DS, the frame loop (input → `RunFrame` → layout → shots → sound → picture; paused: menu only) |
| `src/app.h` | Shared state: the `NDS`, layout chosen/shown, `mainIsTop`, fast forward, quit, `micBlow`, `topAspect`, F5/F12 paths; `hudStrings` (the status bar's words, read from the ROM once at start by `main.cpp`: a ROM whose data does not read logs "no status bar text" and keeps milestone 1's CONSOLE), `rom` (the `RomData`'s strings, in the firmware's language or `--language`'s, for the conversations, and its archive, kept open for the run (38 MB: the icons are decoded from it as the menus ask; `DropArchive` is for a program that wants the words alone); null with `--no-romdata` or data that does not read: "no conversation text", milestone 1's box), `icons` (the `IconCache` over that archive, `port/icons.*`; null without `rom`), `Hud()` (the RAM's `GameState::hud` on the European ROM, else none) |
| `src/settings.*` | `Settings`: every option with its default (CONSOLE's: `layout=console`, widescreen on, 3D resolution 2, 60 fps on); `settings.ini` load/save (SDL pref path), not headless; `layout=` round-trips (`tests/controls_test.cpp`); `settings_version=2` is written on save, and a file without it that says `layout=auto` (from a build before version 2: `layout=` came with CONSOLE as the default, v0.1.1 never wrote it) loads as CONSOLE, once, by the owner's choice |
| `src/controls.*` | Actions, key and controller bindings, defaults, names (from FullDex) |
| `src/input.*` | Keyboard, mouse (stylus; wheel and right drag as the camera), SDL_GameController, stick cursor, script window events |
| `src/video.*` | The SDL window: draws the shown layout and the overlay, fullscreen, scaling, maps window points to touch positions |
| `src/ui.*`, `src/menu.*` | `Canvas` (ARGB overlay, 5 × 7 font) and `Menu` (from FullDex); `PauseMenu` (pages, bindings capture, toasts, cursor) |
| `src/audio.*`, `src/audiostretch.h` | SDL sound queue, WSOLA stretching when slow (from FullDex), volume, fast forward |
| `src/backup.*` | Copies of the `.sav` in `backups/`, at start and every half hour, eight kept |
| `src/layout.*` | `Place()` (canvas per layout, `topAspect`, FOCUS's overlaid panel), `CanvasFit`, `TouchFromOutput`, `OverTop` |
| `src/compose.*` | `Composition`: pieces (a rectangle of one DS screen at a canvas rectangle, opacity, touchable or not) in drawing order, and overlays (rounded frames, boxes, the dimmed backdrop, the focus ring; `RasteriseOverlay`, each drawn before the piece it names), then its UI items (`ui`, `src/ui/kit.*`); `FromPlacement`/`Compose`, `TouchFromComposition`, `TouchClampedComposition`, `OverTopComposition`; what `Video`, `--compose` and the mouse use |
| `src/ui/kit.*` | The UI kit (`sims2wide::ui`): `UiItem`s (Panel, Text, Chip, Image) in canvas pixels, `GameTheme()` (the game's purple: frame `#6171B2`/`#5961A2`, fill `#828ACB`, wells `#6A72B8`, outline and text shadow `#383869`), `PanelItem` (dark outline, frame band, fill, a light bevel and a soft drop shadow), `WellItem` (sunk: a dark top edge), `TextItem` (white, wrapped to its box, clipped to it, `middle` centres it, a `#383869` shadow one canvas pixel down-right, blurred by 0.6 × the scale), `ChipItem` (A/B/X/Y round with their usual colours, LB/RB/START pills, the D-pad a drawn cross, `DPadV` / `DPadH` with only its vertical / horizontal arms lit; `labelAt` lines a column of hints up; `ChipRect` grows with the label), `PanelItem`'s `plain` (just the rounded box in its fill: a meter's fill), `ImageItem` (a live crop, not copied: nearest by the whole factor then bilinear, the picture's own alpha weighing the mix (premultiplied: an icon's transparent pixels lend no colour to its edge) and multiplying the opacity; four opaque texels take exactly the path they always took, so a DS crop draws as before; images are drawn above every other item whatever their place in the list, so nothing of the UI may lie over a picture); `Rasterise(items, scale, argb, w, h, ox, oy)` (premultiplied floats, out as straight alpha; anti-aliased rounded rectangles by signed distance; the still items, then the images), `Bounds`, `Hash` (the still items only; an item's `layer` other than 0 hashes in), `Layers` / `LayersAt` (each still layer, by `UiItem::layer` ascending, over its bounds, then each image over its own, as whole output pixels; items are given in ascending layers so `Rasterise` draws the same; one layer: exactly as before); `DemoItems` (`--ui-demo`). Composed after a composition's pieces and overlays (`Composition::ui`); never touchable. The window (`Video::DrawUi`) and `--compose` both draw `LayersAt`: the window keeps each still layer's texture until its hash changes (an HD menu's frame on layer 0, its list and focus on layer 1: a focus move rasters layer 1 only), and each image's until its pixels change (`HashImage`; a live crop then redraws only its own small bounds: about 1.7 ms a frame with the demo's crop of the moving world on a Retina window, against 41 ms when the whole layer was redrawn); `--compose` at the picture's size (`--compose-scale N`, 1-4: the pieces nearest, the overlays and the UI at N×) |
| `src/ui/font.*` | HD text: Nunito Bold / ExtraBold through stb_truetype (`Weight`), sizes in pixels per em, UTF-8 (a bad byte U+FFFD, a missing letter '?'), kerning (GPOS), glyphs cached per size, weight and quarter-pixel phase; `Wrap` (spaces, '\n', an over-long word broken between letters, accents never split), `TextWidth`, `Metrics`, `DrawLine`. Never the DS's font, never the 5 × 7 menu font |
| `assets/fonts/` | Nunito Bold and ExtraBold (SIL OFL 1.1, `OFL.txt` beside them): static instances of upstream's variable `Nunito[wght].ttf` (googlefonts/nunito at 8c6a9bb), made with fontTools (`fonttools varLib.instancer … wght=700` / `800`, then `pyftsubset` to Latin-1, Latin Extended-A and punctuation, kern and liga kept, `--name-IDs='*'` so the copyright, licence and licence URL names (0, 13, 14) stay in the files, no hinting), 39 KB each; the release archives carry `OFL.txt` as `OFL-Nunito.txt`. Embedded in the program at build time: `cmake/embed.cmake` writes `fonts_embedded.cpp` (byte arrays `nunito_bold`, `nunito_extrabold`) into the build folder, plain CMake (no xxd), linked into the `uikit` library |
| `src/thirdparty/stb_truetype.h` | TrueType rasteriser (v1.26, public domain, nothings/stb at f58f558), compiled once in `font.cpp` (`STBTT_STATIC`) |
| `tests/uikit_test.cpp` | The kit: `Wrap` (fits, wraps, a long word broken, accents kept, empty), the raster's determinism at 1, 1.5 and 2 and its hash, still layers (two `LayerAt`s, a layer-1 change leaving layer 0's hash, one layer as before, the layers over each other within 2 of the whole raster), a panel's outline / frame / fill / anti-aliased corner and shadow at scale 1 and 2, text only inside its box, chips growing with their label, the scaler (`ninja -C build uikit_test`; CI runs it) |
| `src/port/statusbar.*` | The HD status bar: `HudStringsFrom(RomData*)` (the game's own words, ids the same in every language: 30 the clock's format `@hh:@mm @am`, 178 / 179 am / pm, 28 the money's `$@1` (the game's `$` is its simoleon glyph: drawn §), 2421 the HUD's "Sanity:" (its colon dropped; an `@` in it drops the label); `{xx}` escapes and unknown `@` placeholders never shown; `ok` false without them), `FormatHud` (12-hour clock as the game prints it, "3:37 pm", 12 am / 12 pm; money grouped by `,`: the game's formatter has no separator in its string table, so the English style for every language; false, nothing drawn, when `hud` is not valid or out of range: hour, minute, tier 0-3, max ≤ 0, money < 0 or > 99,999,999), `MeterColours` (the game's bar per tier: rim, body, highlight, sampled from its rows 181-185: red `#EB0000`, green `#38BA38`, yellow `#DBA230`, pale lavender `#B2B2E3`), `AddStatusBar` (its items on their own still layer, `kStatusBarLayer` 2, so a minute turning never rasters a menu's frame; a 48-high kit panel: clock, a divider, money, a divider, the label, the meter: a well, filled `round(track × cur / max)` of its track, the well less 3 pixels a side; columns as wide as their widest value so the bar keeps its size; in a narrow box the meter shrinks to 72, then the label goes; every item held inside the panel) |
| `src/port/dialogue.*` | The HD conversation: `DialogueText` (a page as shown: `@1` the Sim's name, `@hh`/`@mm`/`@am` the HUD's clock, any other `@` placeholder and a lone `@` dropped, RomData's `{xx}`/`{F0xx}` dropped, the spaces a drop leaves folded and none left before `. , ! ? ; :`, the game's `\n` kept), `LayoutDialogue` (Nunito 30 px, 28 when 4 rows do not hold it, then 28 px with up to 9 rows: the panel grows upwards; the well never lower than four rows at 30 px, so pages of four rows or fewer keep one box size; the A chip's corner kept free on the last row; `fits`), `PortraitShown` (a fifth of the crop not the frame's flat colours, named in `dialogue.cpp` and the theme, or black), `BuildDialogue(c, game, rom, bottom, topW, topH)`: the world whole (`ConsoleWorld(topW, HudCropRows(topH), keepSides)`), a 1060-wide kit panel from x 110 ending 14 above the canvas's foot (254 high for 4 rows), the name plate tab over its top edge (the speaker's string `speakerNameId`; the player 0x40: the Sim's name; none 0x3F: no plate), the text in a well, an A chip alone in the well's bottom right (the game's table has no conversation "next" word: its only "next" strings are the phone's), the portrait at the right (DS x 174-240, rows 38-121 inside its frame, the same on `s5`, `sit-dialogue`, `sit-concierge`; 2x in a well; the well kept while a speaker talks so the box does not jump, the picture only when shown), and an invisible (alpha 0) touchable piece over the panel mapped onto the game's arrow (x 223-245, rows 159-182): a click anywhere on the panel turns the page. No page drawn yet (`dialogue.valid` false while the game says Dialogue: a speech starting, the 2 frames between two) → the panel alone. Never in git: the strings; checks find the longest at run time |
| `src/console.*` | CONSOLE (`--layout console`): `BuildConsole(ScreenState, menuOpen, focus, topW, topH, hud, strings, game, rom, bottom, icons)` (the panel's menu is `BuildPanelMenu`'s when its tab's `PanelMenuView` is valid, Give Gift `BuildGiftMenu`'s when its `GiftMenuView` is, the Save slots `BuildSaveSlots`'s when its `SaveSlotsView` is, the phone `BuildPhone`'s, a note `BuildNote`'s, and a popup (`ConsoleView`: `port/popup.*`) `BuildPopup`'s over the screen beneath it (the panel's HD menu while it was open, its focus none), `focus` then an index in that view, else milestone 1's touch screen 3x; a conversation is `BuildDialogue`'s when the RAM reads and `rom` has strings, else milestone 1's box from pieces; `ConsoleWorld` is its world for other modules), the 1280 × 720 composition per screen (`App::ConsoleScreen()`: the game's from RAM, `ConsoleScreenOf(App::game, …)`, the pixel tracker's only when RAM does not read; town and its rail, the menu over the dimmed world (framed for SaveSlots / ItemHand / Unknown), the dialogue rebuilt wide, activities, the front end's view for None, FrontEnd, CreateASim, the bare backdrop for Loading; never SIDE), its geometry and the game's colours; `App::consoleMenu` / `consoleFocus` (the navigator drives them; `--console-menu` / `--console-focus N` pin them for checks' pictures, `App::consolePinned`; on an HD tab N is an index in its `MenuView`: Interact's rows, then the phone) |
| `src/navigator.*` | CONSOLE's navigator: `PadState` (the actions held: DS buttons, MENU = the PANEL action, sticks), `Navigator::Update(ScreenState, pad, TouchScript&, dsKeys, settled, view)`: per screen the DS's buttons passed on, or a focus over `HotspotsFor` moved by the D-pad and tapped with A, or (the panel's menu open and `view`, a `MenuView`, valid) a focus over the view's enabled items (`ViewFocus`; `Focus` is then a view index) whose action A runs (`Run` / `Step`: its steps in order over frames, a tap or drag when the stylus is free, keys held their frames, waits; one action at a time, a press meanwhile dropped), a direction's action (`DirAction`: a selection the game owns, its Up / Down the game's scroll; the focused item's own Left / Right; the view's Left / Right; the list's own scroll past its ends with the focus following the rebuilt view, `pendingDir_`) or else a focus move (`MenuMove`), X / Y and back actions, the focus kept on its item's id when the content changes (`MenuKeep`); the gate (a press that would tap waits for `settled`, `App::Settled()`; a direction whose action taps is held the same way; with a view, a held press is dropped when the view's `Hash` changed meanwhile); the view is the caller's for one `Update` (`view_` null after it); `Leave` when CONSOLE goes; `Input::Steer` runs it (window and headless), building the view with `ConsoleView` (`port/popup.h`) from the same state CONSOLE draws; a popup's view (`MenuView::popup`) wins the focus over the panel's open menu, which stays open beneath, and when it goes and the screen under it is back the focus returns to the item it was on (`MenuKeep` on the id saved when it came: the resume) |
| `src/port/menuview.*` | The HD menus' shared model: `MenuView` (`valid`; `items`: `ViewItem` {HD `rect` on the canvas, `action` (a `MenuAction`: `MenuStep`s `TapStep` DS point, `DragStep`, `KeysStep` DS keys for N frames, `WaitStep`), `enabled` (greyed / empty: drawn dim, never focused, never run), `group`, stable `id`, its own `left` / `right` actions (a slider: before the view's and a focus move), its own X (`buttonX`, before the view's X, with its hint's `xWord`; `xAsks` / `xArm`: an X that asks first, the first press arming the item and running `xArm`, the second running `buttonX`: Pockets' bin), explicit moves `next[4]` (up, down, left, right item indices; -1 the geometry), its A hint's `word`}; `buttons[Left/Right/X/Y]` screen-wide actions; `back` (B; empty: the menu closes); `scrollUp` / `scrollDown` / `scrollGroup` (the game's own scroll: the list shows the rows the game shows); `initial`; `selected` (a selection the game owns, Relationships' Sim or the phone's row: the focus held there, every Up / Down the game's scroll); `title` (the tab's title in the game's words); `armed` (drawing only, not hashed: the item whose X is armed); `own` (a screen of its own: Give Gift, the Save slots, the phone, a popup, a note: the navigator's focus screen whatever the screen state); `popup` (a popup over the screen: the resume); `closes` (an item's A) / `backCloses` (B) close the screen: A and B then wait as after the phone's X; `passKeys` (DS buttons passed on while held: Game Paused's Start); `Hash` of all of it), `SortUi` (the UI items in ascending still layers, the images last: every HD screen's last step), `MenuFirst`, `MenuNeighbour` (nearest enabled item within 60°, as `Neighbour`), `MenuMove` (the explicit move, else `MenuNeighbour`), `MenuKeep` (the enabled item with the id, else the enabled one at the old index clamped, then the nearest, the one before first), `MenuFocusable`. Screen modules build one from `GameState` every frame; the navigator and CONSOLE use the same |
| `src/port/panelmenu.*` | The panel's menu in HD: `PanelMenuView(ScreenState, game, rom)` (valid on a tab whose part of `GameState` reads, with the game's strings: all five tabs; the tab's items, then the phone button (tap `kPhoneIcon`, id 1, `kPmPhoneWell`), linked explicitly: Up from each top item (nothing enabled above it) to the phone, Down from the phone to the tab's first item (the cone alone leaves it to chance); the title and A words are the view's; `PanelMenuViewOf(s, game, rom)` without the strings' check, for tests, rom null leaving the words empty) and `BuildPanelMenu` (a 928 × 616 kit panel at 176,30 over the dimmed world, above the HD status bar: the tab bar (LB, the five tab icons of the ROM (2247 green for the tab shown, on a raised plate; 2250 blue), 3x, RB, centred; the phone button at the right, a live crop of the game's own (DS 218,10, 30 × 34, 2x: its ROM sprite is not known) in a well of the game's #5961A2), the title in a well (the game's `"@1: @2"` 1840 with the tab's name 1835 + tab and the target's name, fitted), the tab's content, the hints at the bottom right (X with the focused item's `xWord` when it has its own X; A with the focused item's word, the phone's "Cellphone" 716, none when the item performs nothing; B with "Close" 1866: the game has no "back"), the focus a green halo (#309A41, the game's selection green) around the item; frame on layer 0, everything else on layer 1, images above; invisible touchable pieces map each HD item onto the game's own target, so the mouse does what A does). Helpers: `MenuWords` (a string as shown in a menu: escapes and placeholders dropped, a `$` or empty brackets left before one, one line; `MenuText` the same on a text, `MenuWordsWith` with its `@1` filled first: "Give Item to @1"), `MenuTitle`, `FitLine` (one line, shrunk from its size to a minimum, then an ellipsis), `AddMousePiece`, `AddFocusRing`, `AddHint`. A tab without a view keeps milestone 1's menu (the touch screen 3x) |
| `src/port/interact.*` | The Interact tab in HD: `AddInteractView` (a row per action the game lists, max 4, greyed ones disabled; A taps the row's arrow, 236, 59 + 34i, the game's own touch target being the arrow only, x 219-251; Left / Right tap the strip's previous / next slot, 55 / 122, 167, when there is one), `BuildInteract` (the portrait a live crop of the game's well, DS 63,47 65 × 81, 3x in a well of its colour: Sims' and objects' pictures are texture bundles, not sprites; the rows: a rounded bar, the verb 1845 + code at 30 px (to 22, then an ellipsis), the game's ▶ (the Interact tab's icon, frame 0, 2x: blue, green when focused, as the game lights its highlighted arrow); greyed rows flat, their text and arrow faint; the verbs that carry an amount show it as the game does (`InteractAction::amount`: Buy 2 × the object's record price, Payoff 1500, Bail 200, Pay the object's entry in `G+0x301C`; "Buy (§1,200)", the game's `$` drawn §); the strip: the current target on a plate with a thin light lavender rim (only the focus is green), the neighbours' names beside it (one line 24 to 18 px, else two lines at 18), the D-pad's chip when Left / Right switch), `MeasureInteract` (`--measure-menu`). Not the game's pad mode (A held, Up / Down, release A: it holds and turns the hero, and switching targets in it performs an action on the release) |
| `src/port/pockets.*` | The Pockets tab in HD: `AddPocketsView` (the six slots, empty ones disabled; the three key items; the four counted items; the first focus the game's own selection; a key item's Left explicit: the last enabled slot of its row, the third's the last counted item; a slot's Right: the next enabled slot of its row, else its row's key item) and `BuildPockets` (a 3 × 2 grid of 104-pixel cells with the record table's item icon 3x (`ItemRec::iconRes`, the descriptor), the key items' column (the game's tool buttons 6670 / 6655 / 6652: frame 1 owned, frame 0 the silhouette), the counted row (their icons a live crop of the game's buttons, DS 99 + 29i, 155, 23 × 23, 2x on the button's own `#51518A`: the research's sprite 2506 is a ▶, their real one is not known; the count in white, dim at 0), the info well (the focused item's name, 463 + id / 1968 + k / 1154 + i, its description 822 + id / 1972 + k / 845 + i wrapped and shrunk 24 → 18 px, `FitBlock`; the phone focused: the game's selection's), and the hand and the bin as buttons (the game's sprites 6667 / 6664 and 6661 / 6658 2x, "Use" 1845 and "Trash" 469: the game has no word for its bin, so its trash cans' name); a cell the game holds selected has a lavender rim (a key item not owned: red, the game's ring), the focus the green halo). A on a slot or an owned key item taps it then the hand (191,107); X on a slot asks (`ViewItem::xAsks`: the game's bin throws at once, with no question of its own): one X never throws, it arms the slot (`Navigator::Armed`, `App::consoleArmed`, drawn through `MenuView::armed`: the HD bin green; it taps the slot, so the game's ring is there too), a second X on it taps the slot and the bin (67,167); a focus move, a tab change, the item going (a slot's id carries its item id) or the menu closing disarm it; counted items and tools not owned: A does nothing (their description only). `MeasurePockets` (`--measure-menu`): every item's, key item's and counted item's name and description in the Pockets and Give Gift boxes, the buttons' words, the Give Gift title and plate for every Sim; an item's name may end in an ellipsis (Italian 463, a sentence), nothing else |
| `src/port/gift.*` | Give Gift in HD (`GiftMenuView`, `BuildGiftMenu`; CONSOLE whenever the game shows it, no menu to open): a panel like the menu's without the tab bar, the title "Give Item to @1" (2422, the recipient's name 323 + k), the pockets as a 2 × 3 grid (empty slots drawn, never focused; an item the game marks as refused for this Sim, `GiftState::refused`, greyed and skipped), the focused item's name and description, the bow bar (the game's bow sprite 3631 2x: the bow actor's definition `0x0214AACC` {0xE2E, 0xE2F, 0xE30}, read from RAM; and "Give Gift" 1850), the recipient's portrait (a live crop, DS 167,47 65 × 81, 3x: the Interact well's shape 104 to the right) over a name plate; A on a slot taps it then the bow (127,85): the game gives it and goes back to the panel (giving is what the screen is for, as the game's own bow: no question); B (`MenuView::back`) taps the game's X (235,19); the first focus is the game's selected slot. `Navigator::UsesView` takes Gift (and the Save slots) as it takes the open panel; LB / RB tap tabs on the panel only |
| `src/port/goals.*` | The Goals tab in HD: `AddGoalsView` (one item per goal the HD list shows: from the goal on the game's first visible line, `TG+0x518`, moved on until the selected one fits the well; the game's selection held, `MenuView::selected`; Up / Down tap the game's arrows, 62,59 / 62,145; no A: a goal is only read) and `BuildGoals` (the giver's portrait a live crop, DS 47,88 30 × 30, 3x in a well; the list well: each goal a bar with its mark (a mission's green dot, a hotel request's lavender one) and its text wrapped at 26 px, all shrunk together to 20 when the tallest does not fit, the game's selection raised; a thin scroll bar (`AddScrollBar`, shared); the hotel footer when the game shows it: 1843 / 1841 and "Score: @1%" 1842), `GoalText` (a mission's 1627 + id; a request's own string with @1 the Sim, @2 the place; the game's `$` drawn §), `MeasureGoals`. A goal's bar maps onto its lines on the touch screen while the game shows them |
| `src/port/relations.*` | The Relationships tab in HD: `AddRelationsView` (the four rows the game shows from `TR+0x20`, its selection held; Up / Down its arrows 56,10 / 56,180) and `BuildRelations` (a row: the game's small portrait a live crop, DS 43,28 + the row's step, 28 × 28, 2x, and the name, one line 24-18 px or two at 18; the selected Sim: the big portrait a live crop, DS 84,37 69 × 84, 3x, its name on a plate, its mood 181 / 184 + mood and relationship 182 / 192 + 7 mood + level in two wells, its description 389 + id wrapped and shrunk 24 → 18), `MeasureRelations`. A row maps onto its portrait (a tap selects that Sim) |
| `src/port/options.*` | The Options tab in HD (`AddOptionsView`, `BuildOptions`: Save 64 and Quit 61 as pill buttons, A their taps 57,44 / 234,69; the music 62 and SFX 63 volumes as sliders read back from RAM, `VolumeNotch` the game's tenth, 11 notches, Left / Right a `DragStep` of the game's knob from 72 + 16 k to the next tenth on row 121 / 171 (none past an end), the D-pad's chip on the focused one; explicit moves: Save / Quit Down to the music, Right / Left to each other, the music Up to Save) and the Save slots (`SaveSlotsView`, `BuildSaveSlots`: CONSOLE whenever the game shows SaveSlots, a panel without the tab bar, "Save Game" 12, three rows from `GameState::town.saves`: the number in a round well, green for the game's current slot, the Sim's name, the money 28, the place 0x43 + place, or "No Data" 11; A taps the slot's tick, 232, 67 / 113 / 159: an empty slot is saved at once, a used one makes the game ask "Overwrite this save?" (2423, popup kind 2; `SlotsPopup`): the slots then wait, disabled, under the HD popup (`port/popup.*`: "No" 49 B, "Yes" 48 A), "Saving" (kind 3) the same way; B its X 232,22; the first focus the current slot, else the first empty one; `replay` false: a press held while the game was not settled is dropped; a row maps onto its tick), `MeasureOptions` |
| `src/port/popup.*` | The game's popups in HD: `PopupButton` (the game's own button rectangles, 0x02057314: 24 × 24, y0 = box y `P+0x544` + 8 × tiles `P+0x554`, x0 = 128 − width `P+0x550` / 2 + 8 r, r = w / 16 − 1 for kind 0's X, w / 32 − 1 the cross, 3 w / 32 − 2 the tick; their centres), `PopupOver` (a popup up, `P+0x8` 1-3, over the panel (`GScreen::Popup`, any tab: Options > Quit's screen is Unknown) or an HD screen: Give Gift, the Save slots, the phone, a note), `PopupView` (own and popup: kind 0 one item "Close" 1866, A and B tap the X, Start passed on (`passKeys`); kind 2 "Yes" 48 (A: the tick), B "No" 49 (the cross), `replay` false; kind 3 nothing; `closes` / `backCloses`: the closing wait), `ConsoleView` (the view CONSOLE draws and the navigator drives: the popup's, else `ScreenView`, the screen's own: the panel's tab, Give Gift, the Save slots, the phone, a note), `UnderScreen` (the panel and its tab under a popup), `BuildPopup` (over what the composition already holds: a veil (`kVeil`) on everything, the box (`kPopBox`, 680 × 330 at 300,182) with its title on a plate (kinds 0 and 3; a question's is stale in RAM: not drawn), the text in a well (FitBlock 30 → 20), the buttons with their chips (A B "Close"; B "No", A "Yes"), the game's held button green (`P+0xD8C`), the focus's halo, the mouse's pieces on the game's buttons; all on `kPopLayer` 3; the pieces beneath stop taking the mouse, the pictures beneath that the box would cover go and the rest are dimmed), `MeasurePopups` |
| `src/port/phone.*` | The phone in HD (`PhonePageOf`: every page's rows as the game counts them (its highlight's index), its title and text: 0 the menu (the game's table, "Messages-@1" with the count or "FULL"), 2 the messages (unread: a dot, the game's "*"), 3 a message (the sender, its text with @1..@4 as the game fills them, Next / Delete / Exit from the row `textEnd`), 4 the ringtones, 5 the phonebook (the maid's price), 7 the contacts, 6 / 8 / 9 the answers, 1 an incoming call, 10 nothing; `crop` when a page does not read as the game's: its own LCD then, `kPhLcdFrom` 2x); `PhoneView` (own; the game's highlight held, `selected`, the rows shown up to `kPhRows` from the game's scroll; Up / Down and A as the DS's own keys: as many presses as rows between, then A; B its X `kPhoneClose`, `backCloses`); `BuildPhone` (a kit panel, the LCD in the game's green `#9AB29A` with dark letters, the highlight its dark bar with the focus's halo, the header: signal bars 2419, the date 0x83C and the status bar's clock; the game's Up / OK / Down 8448 / 5806 / 2755 and X 3068 sprites mapped by the mouse onto its buttons (43 / 127 / 212, 166; `kPhoneClose`); the hints D-pad, A with the row's word, B "Close"), `MeasurePhone` |
| `src/port/note.*` | A note (overlay 11) in HD: `NoteView` (own on the Unknown screen while `GameState::town.note` reads; A and B press the DS's A, the game's way to close it), `NoteText` (the string `S+0x2C`, @1 the player's name for 0x4A8 / 0x509 / 0x657), `BuildNote` (a parchment panel in the note's colours, the text centred 32 → 20, the game's X 7683 2x mapped onto 234,171), `MeasureNotes` |
| `src/activities.*`, `src/padstate.h` | CONSOLE's activities on buttons: `ActivityInput(Screen, PadState, TouchScript&, dsKeys, ActivityState&)` (social answers, the cards' draw / lift / play / discard / quit question, the vacuum's drive, hand, grab and blow), `ActivityHints` (the hints' box rows); `PadState` (shared with the navigator) |
| `tests/navigator_test.cpp` | The navigator on synthetic screen states: menu, focus moves, taps, tabs, popup, phone, town keys, Unknown's cursor, leaving CONSOLE, the activities, the gate (unsettled presses held, replayed, dropped after 90 or on a screen or mode change; DS buttons not held), a popup opening held, the front end's screens, save slots and the quit question on the cursor; synthetic `MenuView`s: a selection the game owns (the focus held, Up / Down its scroll, held while unsettled and dropped when the view changed), an item's own Left / Right before the view's, Left / Right held while unsettled, explicit moves, greyed rows skipped and never run, A's tap, the phone above the list, Left / Right actions, DS keys held over frames then a tap, a press during an action dropped, B's back action, a drag, the focus kept on its id when rows go or grey, a held press dropped when the view changed and replayed when not, the view going invalid, the list's scroll with the focus following, an item's own X before the view's (held while unsettled), an asking X (armed by the first press, performed by the second, disarmed by a move, the menu closing, the item going), Give Gift's view (A's two taps, B its back, LB / RB nothing); the HD Save slots (a focus screen, not the cursor's: A the slot's tick, B the X; without a view the cursor's) and a slider's Left (the knob's drag, no move where nothing is to drag); a popup's view over the open menu (the focus its, the menu open beneath, A its tap then the closing wait, Start passed on; the menu back on the same row after it), over the closed panel (closed after), over the Save slots (the slot back), the phone's view (Up its DS key over frames, B its X) (`ninja -C build navigator_test`) |
| `tests/controls_test.cpp` | The bindings: one action per input, by default and after loading an older settings file (`ninja -C build controls_test`) |
| `scripts/console/*.txt` | `pad` scripts for `tests/console_check.sh`'s navigator cases |
| `tests/console_check.sh` | CONSOLE's pictures from save states: the DS pixels where each piece puts them, the frames, the dimmed world, the focus ring, the front end's view (`s2`, `s3`, `s4`), the HD save slots over the dimmed world (`sit-store`, Options > Save: three "No Data" rows; `slots-hd`, `B-cas-opt-save`: the current slot's number green, the focus on it), the bare backdrop in the cards' quit load (`act-cards`, frame 119), the HD status bar (`s7` at 15:37, `sit-cityhall`, the menu: the world without the game's bar rows, the bar's panel found by its band and text inside it; `sit-cityhall --wide`: the world 1280 × 661 with its sides, against the run's `-top.png`, the backdrop below it, the bar from x 16; the complete save's `sit-casino` at 1x and `--compose-scale 2`: the meter's body in the yellow tier's colour, filled cur / max from `--print-hud` within 2 pixels; `act-social`, `act-vacuum`: the small world 264 high, the bar under it; `act-cards`: the whole world, no bar); the panel's HD menu (`sit-cityhall --console-menu`: no target, no row; `casino-menu` at 1x, 2x and `--wide`: the portrait = the touch screen's pixels 3x, three labelled rows, the focus ring on the second only, the tab bar's Interact icon the only green one, the world dimmed, never the touch screen 3x; `menu-m1`: `--no-romdata`, milestone 1's menu; `pockets-menu` (`A-pk-cnt3-slot1`): the Pockets icon the green one, the focus around slot 1, the counted items' icons = the touch screen's pixels 2x, counts and the name in white; `gift` (`sit-gift`, empty pockets: no focus) and `gift-items` (`A-gift2-base`) at 1x, 2x and `--wide`: the HD Give Gift, the portrait 3x and the bow 2x = the touch screen's pixels, the focus around slot 1 and its icon; never the touch screen 3x); then the navigator from `pad` scripts (`--print-game` and `--print-screen`, no `pixels disagree`: tabs from `s7`, the HD Interact tab (`interact-hotseat`: from `sit-casino` MENU, Down, A → Cards; `interact-target`: from `D-cas-right2` Left → the cards, Right → the panel, `--print-town`; `interact-buy`: from `sit-store` A → Catalogue; `pockets-drencher`: from `A-pk-slot1-key2` MENU, A → the Super Drencher's game, ItemHand (overlay 18); `pockets-bin`: from the same, MENU, Left, X → the slot selected, nothing thrown, X → the pockets empty (after the second X only); `pockets-bin-once` / `-twice` / `-cancel` from `A-pk-cnt3-slot1` (the slot already the game's selection): one X throws nothing, two do, X, Right, Left, X throws nothing; the picture `pockets-armed` (the bin's button green after one X, plain in `pockets-menu`); `gift-give`: from `A-gift2-base` Right, A → the bed given, the pockets keep the arcade, Panel/Interact; `gift-close`: from `sit-gift` B → Panel/Interact; `goals-scroll`: from `B-pg8-goals` MENU, Down x3, Up → the game's goal 1, 2, 3 (scrolled), 2; `relations-select`: from `B-cas-rel` Down x5, Up → Sim 1-5 (scrolled), 4; `options-volume`: from `B-cas-opt` Down, Left, Left, Right, Down, Left → music 90, 80, 90 %, SFX 90 % read back from RAM; `options-save`: A on Save → SaveSlots, B → the panel; `saveslots-save`: from `B-st-opt-save` Down, A → "Saving", slot 2 used and current, on a scratch save; `saveslots-ask-no` / `-yes`: from `B-cas-opt-save` (a copy of the complete save) A → "Overwrite this save?" in HD, B → closed, nothing saved / A → saved; popups and the phone: `popup-pause-a` / `-start` (from `B-pause`, A taps the HD popup's button, Start is passed on: the panel), `popup-quit-no` (from `B-cas-opt` MENU, Right, A: the game's question over the open HD menu, B its cross: the menu open again on Quit, never the title), `phone-pages` (from `s7`: the phonebook, the contacts, a friend's answer, Exit by the game's Down five times then A, B the panel), `phone-message` (from `C-cas-msg1`: A, ten Downs past the text, the next message); pictures `popup-goal` (`s6`), `popup-pause` (the title plate), `popup-quit` (`B-ch-opt-quit`), `popup-menu` (over the HD Options), `phone-msg`, `phone-contacts` (the scroll bar), `phone-wide`, `s7` and `s7-menu` (the HD phone: its LCD, the highlight and halo, never the touch screen 3x); pictures `goals-menu`, `rel-menu` (and `--wide`), `opt-menu`, `slots-hd`: the giver's and the Sims' portraits the touch screen's pixels 3x / 2x, the focus on the game's selection, the knobs at 100 %), the popup from `s6`, from `s6` again A pressed while the next popup opens (held, then it closes it), CONSOLE → FOCUS → CONSOLE) and the activities' (a social answer, the cards' draw, discard, quit and its cancel, the vacuum's blow), the hints' box; the conversation: `s5` with `--no-romdata` (milestone 1's box), `s5-hd` (the panel, white text and name, the portrait = the touch screen's pixels 2x, the world whole), `s5-hd-wide` (`--wide`: the world's sides kept), from `s5` through `scripts/dlg-taps.txt` the HD text and the page change logged on the same frame (`--print-hud`: `dialogue … page=` and `hd dialogue id=…`) and the text changing between the frame before and it, `scripts/console/dialogue-a.txt` (A twice: exactly two pages), the same taps with `--no-romdata` (`--print-hud` still logs every page, `dialogue none` only between speeches, no HD line), each language 1-5's every conversation page (`--measure-string all`, from the speech table in RAM; FR/DE/IT/ES over the EN game: a layout check only) fitting the box, its longest one (`--measure-string ID`) and its picture (`--dialogue-string ID`) with no white text pixel outside the well; each language's HD Interact, Pockets, Give Gift, Goals, Relationships, Options and Save slots words in their boxes (`--measure-menu`) (ROM, `shots/` with `D-cas-right2`, `A-pk-cnt3-slot1`, `A-pk-slot1-key2`, `A-gift2-base`, `B-pg8-goals`, `B-cas-rel`, `B-cas-opt`, `B-cas-opt-save`, `B-st-opt-save`, `B-pause`, `B-ch-opt-quit`, `C-cas-msg1`, `C-cas-msgs`, `C-cas-pb1` (copies of the research's held-out states), the complete save and Pillow: `PYTHON=…`) |
| `src/touchscript.*` | `TouchScript` (`Input::touchScript`): taps, holds and drags over game frames, queued (8 at most), one released frame between; wins over the mouse and the stick in `Input::Frame` |
| `tests/layout_test.cpp` | Touch mapping of every pixel in every layout and size, and of every piece of every CONSOLE composition, with and without the status bar; the HD menu on a made-up Interact state (its view: greyed row disabled and skipped, Left / Right; no strings or an invalid tab: no view; its mouse pieces round trip at every size, a row's click on its arrow, a tab's on its icon, the phone's on the phone, the title and the dimmed world nothing; every item inside the panel; the still items in ascending layers; on the real views (0-4 rows, a greyed one, neighbours or none) every enabled item reachable from every other by the pad's moves, the phone Up from the top row and back; `FitLine` cutting whole letters only; milestone 1's menu without the strings; `FitLine`; Pockets and Give Gift on made-up inventories of 0-6 items, with or without a selection and a refused gift: empty slots disabled, every enabled item reachable, A's taps (slot then hand / bow), X's (the slot, or the bin on the game's selection), their mouse pieces onto the game's slots, hand, bin and bow, every item inside the panel, layers ascending, no view without the strings or the RAM's part); Goals (0, 1, 8 goals of 1-3 lines, a selection or none, the footer or not), Relationships (0-23 Sims, the game's first and selection), Options (the volumes at 0, 30 and 100 %) and the Save slots (the popups over them `port/popup.*`'s: the slots disabled); popups (`PopupButton` on the game's boxes; kinds 0 / 2 / 3 over the panel and Options > Quit's Unknown: the view, the menu beneath drawn, the box's items inside it, layers ascending, the pictures under the box gone, the mouse on the game's buttons and not on the menu beneath, no view without the box, the strings or over a screen the port does not redraw), the phone (every page: the game's highlight held, A and Up / Down as many DS key presses as rows between, the rows inside the LCD, the mouse on Up / OK / Down / X; a list that disagrees: the game's LCD), a note (A / B the DS's A, the X on the game's); the game's selection held and Up / Down its arrows, Options' explicit moves (Save / Quit Down to the music, the phone above Save), the sliders' Left / Right the knob's drag by a tenth (none past an end), the slots' ticks and X, every enabled item reachable, the items inside their list and the panel, layers ascending, the mouse pieces round trip and land on the game's targets (a goal's bar on its lines, a row on its portrait, Save on Save, a slot on its tick); the status bar's place (the world cropped at its shape, the bar inside it below the menu's frame, under the small world but the cards'), its values (clock, grouping, escapes) and garbage (invalid, out of range, max 0, huge: nothing drawn; extreme but valid values and long words: every item and pixel inside the bar and its shadow) on made-up words (`ninja -C build layout_test`) |
| `tests/touchscript_test.cpp` | The synthesised touches (`ninja -C build touchscript_test`) |
| `src/looks.*` | Sharper / widescreen 3D and 60 fps: applies the Looks settings, builds `App::TopImage` |
| `src/melonds_ext/GPU3D_Wide.*` | FullDex's wide 3D rasteriser (CPU), built into melonDS's `core` |
| `patches/melonDS/*.patch` | Hooks in melonDS 1.1 for the looks, applied at configure time |
| `src/gamestate.*` | `ReadGameState(RamView)`: the game's own screen manager read from main RAM (EU addresses, `kEU`) → `GameState` (`GScreen`, panel tab, selections, `settled`: the bottom slot in place and no popup sliding, decided there and nowhere else); `GameState::hud` (money, clock, sanity, tier, Sim name from the block `G`) and `::dialogue` (screen Dialogue: speech, page of pages, text string id, speaker and its name id, last page) with `--print-hud`, and `::town` (`TownState`: `inventory` = pocket slots with name / description string ids and sprite descriptor, the four counted items, the three key items; `options` = music and SFX volume 0..4096; `popup` = state, kind, the game's raw text and title bytes, pressed button; `saves` = the three slots' used flag, current slot, Sim name, money, place, time; `ReadSimRow` / `ReadItemRow` / `ReadSimRecord` the shared record table and the Sim records; `popup` also its box's place (`boxY`, `boxW`, `boxTiles`: its buttons, `PopupButton`); the panel's tab lists are read under a popup too (the menu drawn beneath it); `::interact` / `::pockets` / `::goals` / `::relations` / `::phone` / `::gift` / `::note` (the menus' lists: Interact's target, actions, pad mode and strip; the Pockets selection; Goals' entries, scroll and footer; Relationships' Sims; the phone's page, row and inbox, its date, the rows its texts end at, the maid's price and answer, the Sim called and their mood and place, the contacts' list (checked against the game's), a recorded custom ringtone (`kEUX`); Give Gift's recipient and slot; a note's text id; each read only on its own screen or tab, vtable-checked, capped, `GameState::selection` for Relationships = `TR+0x24`); `--print-town` logs each part when it changes; popup text decodes with `GameTextToUtf8`, `romdata.h`), all range-checked, each part `valid` false on garbage on its own, `valid` false on garbage, and left out of `GameState ==` (logged apart); `ToScreenState` (CONSOLE's screen: Panel + tab, the rest by name, tab -1, unmapped or Options > Quit's popup → Unknown), `ConsoleScreenOf` (that; without the HUD and both screens black, `GameState::between` or an Unknown → Loading, an Unknown not black → None; the pixels without RAM), `PixelsAgree`; `App::game` every frame, `--print-game` logs changes (`frame N: game X`) and a pixel tracker that disagrees over 90 settled frames (`frame N: pixels disagree: X`); the map is in `docs/research/ram-map.md` |
| `src/recorder.*` | `Recorder::Frame` (window runs; headless only with `--record-dir`): a settled unmapped (overlay, vtable) pair for 30 frames, or RAM not reading (not `between`, not black) for 300 frames after frame 600, saves a state + PNG once per pair (max 20 per run, skipped when the folder already holds that pair or 200 captures) to `<SDL pref path>/captures/<YYYYMMDD-HHMMSS>-ov<ov>-vt<vt>.st/.png` and logs `recorded: <path>`; `--record-dir DIR` overrides the folder; `--game-unknown-after N` (check only) fakes a new unmapped pair every 200 frames from frame N (checked in `tests/gamestate_check.sh`) |
| `tests/gamestate_test.cpp`, `tests/gamestate_check.sh` | The reader on a synthetic RAM, `ToScreenState`, `ConsoleScreenOf` (`ninja -C build gamestate_test`); each save state's game state, the status bar and the conversation's series (`--print-hud`, `scripts/dlg-taps.txt`) the panel's tabs from `s6` and the town's records (`--print-town`, an action's amount as `:$N`; popup texts compared with the ROM's own strings via `--dump-string`) from copies of research states put in `shots/`: `A-gift2-base`, `A-pk-cnt3-slot1`, `B-st-opt-music`, `B-ch-opt-quit`, `B-pause`, `B-cas-opt-save` (and `s6`), and the menus' lists (`--print-town`) from `L-sit-store-none-250`, `L-sit-casino-none-250`, `L-sit-cityhall-none-250`, `D-hotel-down`, `D-cas-right2`, `D-cas-hold`, `D-cas-hold-down2`, `A-pk-slot1-key2`, `A-pk-town-cnt1`, `B-pg8-goals-l7`, `B-cas-goals`, `B-hot-goals`, `B-cas-rel-next5`, `B-st-rel`, `C-cas-msg1`, `C-cas-msg1-dn4`, `C-cas-pb1`, `C-s7-msgs`, `A-gift2-slot2` (ROM, `shots/`, the complete save, as `screens_check.sh`) |
| `src/romdata.*` | `RomData`: the game's own data read from the player's ROM at run time (nothing of it in git): `Open(rom)` finds `rom.bin` in the cartridge's file system (the game's archive: a u32 offset table, resource id = index, 8867 entries), `Resource(id)`; `LoadStrings(language)` (0 JP → the EN file, 1 EN, 2 FR, 3 DE, 4 IT, 5 ES: the game's own table, files 123, 123, 3496, 3593, 4184, 7599; each u32 N, a Huffman tree of u16 pairs to N, u32 offsets, LSB-first bit streams, no compression header), `StringCount()` (3127), `String(id)` as UTF-8 (the game's 1-byte charset, `kCharset`: Latin-1's order of the accented letters less a few, worked out from FR/DE/IT/ES words; an unknown byte stays visible as `{xx}`, a 2-byte escape ≥ 0xF0 as `{F0xx}`). Every size and offset is checked before use and a table that does not fit is refused (a string over 4 KB, a table over 4 MB too); nothing in `romdata.cpp` touches melonDS: `GameLanguage(nds)` = the firmware's user language (bits 0-2 of its settings word; melonDS's built-in firmware: English) is in `romdata_nds.cpp`. `--language N`, `--strings-count`, `--dump-string ID` (whole numbers, else the usage) print and stop (`--language` also picks the HD interface's words, a layout check: the game keeps the firmware's language); `--measure-string ID|all` (the HD conversation's layout of a string, or of every page of the speech table: `SpeechStringIds(RamView)`, `gamestate.*`, needs a state; with the ROM's am / pm and the widest name and clock; the longest is reported for the box with the portrait's column, both boxes checked) prints and stops (exit 2 when one overflows or a page's shown text still holds `@`, `{` or `}`: their ids printed); `--language` above 5 is a usage error; `--no-romdata` (milestone 1's CONSOLE) and `--dialogue-string ID` (the HD box shows that string) are for checks. The archive's compression (`Decompress(data, len, out)`, or at an offset of a resource): a 4-byte header (byte 0 bits 4-6 the kind, bit 7 a 16-bit delta undone after, the low nibble 0 in every compressed resource of the game, bytes 1-3 the unpacked size), kind 0 stored, 1 LZ10 (the BIOS's), 6 the game's own, ported instruction by instruction from the decoder in its ITCM (`0x01FF8000`; the research ran that code under Unicorn and `romdata_check.sh` compares against its outputs): a first word (N table bytes, the prefix K, B2 extra offset bits, the prefix's width B3), the table, then codes read from 32-bit little-endian words top bit first: B3 bits not K start a literal; K then a number (n ones, a zero under 7, n bits: 1 to 255) G ≥ 2 a copy of G + 1 bytes (offset from a second number, 255 the end of the stream, B2 bits, a byte; distance 1 + it), G == 1 then bits 0 a 2-byte copy (a byte: distance 1 + it), 1 0 an escaped literal (the new K before its low bits, the old K its top bits), 1 1 a run (a count code, a byte index: under 32 the table, else spelt in 3 more bits); the output must end at the header's size with the end code, every copy from inside the output, every run byte inside the table, else refused (`out` empty); `kMaxUnpacked` 4 MB. The game's data: 5464 streams of kind 6 and 16 stored, nothing else: B2 always 0, B3 1 to 4, every stream ending exactly at its size, no delta and no LZ10 at all (the research's four "delta" entries are 32-byte palettes beginning `E0 03`, green, and its 32 "LZ10" ones sprite descriptors 16 pixels wide, `10 NN 00 00`, that its heuristic took for streams; the port refuses the palettes, their stream running out, and kind 1 stays for the loader's sake, checked on synthetic streams only). Palettes (`ReadPalette`: 32 or 512 bytes of BGR555, `Bgr555ToArgb` spreading 5 bits as melonDS shows them, `(c << 3) | (c >> 3)`: white is `#FBFBFB`, the frame `#6171B2`). Sprites (`SpriteFrames(desc)`, `DecodeSpriteFrame(tiles, desc, pal, frame, out)`, `RomData::Sprite(tiles, desc, pal, frame, out)` → `SpriteFrame` w × h ARGB straight alpha, colour 0 transparent, `pieces`): a descriptor (u8 w, u8 h, u16 0, u16 ?, u16 frames at byte 6, u32 tile bytes, u16 frame offsets from byte 12; a frame u16 pieces 1-64, u8 w, u8 h, u16 the byte offset of its own compressed tile stream in the tiles resource, s16 x, s16 y, then pieces u16 position (x 9 bits signed, y 7) and u16 attribute (bits 2-3 size, 4-5 shape, 6 up the first tile in 32-byte tiles), the DS's OBJ sizes, 1D tile order), each piece drawn at (its x − the frame's x, …), clipped, every offset and size checked. `--dump-resource ID FILE` (the resource unpacked when its header reads, else raw; a multi-frame tiles resource gives its first frame's stream) and `--dump-sprite ID FRAME FILE` (frame FRAME of the sprite whose descriptor is ID, tiles ID − 1 and palette ID + 1, as a PNG with alpha; `sprite ID: N frames; frame F: w x h, P pieces` on stderr) write and stop, for `romdata_check.sh` |
| `src/port/icons.*` | `IconCache(rom)` over the open archive: `SpriteRef` {tiles, desc, pal} (`Of(desc)`: the game's definitions put the three side by side), `Frames`, `Frame(ref, frame)` (decoded once and kept; a bad one remembered as null), `Scaled(ref, frame, w, h)` / `(ref, frame, scale)` (through the kit's scaler, `Rasterise` of one `ImageItem`: a whole factor is nearest alone, alpha kept); pointers stable for the cache's life. The HD menus ask it for the tab icons (2247 green / 2250 blue, 5 frames: Interact ▶, Pockets, Goals, Relationships, Options), the phone button (2230, frames 2-5 the 32 × 32 round one), the phone's buttons, and item icons by the record table's descriptor id |
| `tests/romdata_test.cpp` | `RomData` on a synthetic ROM written by hand (header, name and file tables, archive, a tiny string table of made-up text; no game data): the valid one decodes (the charset, `{xx}`, `{F0xx}`, the bit order), and every truncated, oversize, out-of-range, non-monotonic or cyclic table is refused without a crash; `HudStringsFrom` on such a table at the game's ids (the label's colon, no-break space and spaces trimmed, a label with `@` dropped, not ok without `@hh` / `@mm` / `@1` or with too few strings); the compression on streams written by an encoder in the test (`Game6`: every code, the widths' ends, extra offset bits, the delta; every truncation, the header and the stream's word lying, copies from before the start or past the size, a run byte past the table, other kinds, stored and LZ10 streams); sprites built by hand (`Sprite`: pieces of each shape, placed, clipped, at the most negative positions; every broken descriptor, tile stream and palette refused) and the icon cache over them (frames kept, scaled copies pixel for pixel, bad ids remembered, nothing from a dropped archive) (`ninja -C build romdata_test`; CI runs it; run it once under `-fsanitize=undefined` with `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG` after a change: it found a test bug ASan would have, and ASan deadlocks on the owner's Mac) |
| `tests/romdata_check.sh` | 3127 strings in each language 1-5, in language 0 (the EN file) and in the firmware's; language 6, string 3127, `--language fr` and `--dump-string -1` refused; every frame of the HD menus' icons decoded (`--dump-sprite`: the tab icons, the phone button and buttons, the rings, items), a string table, a missing frame, a missing resource and bad numbers refused; three English strings and a French one with "é" against the research's decoded lines, read at run time from `RESEARCH=` (a folder holding `text/strings_00123.txt`, `strings_03496.txt`; never in git: skipped with a message without it); the compression against the research's outputs (`extract/decomp_index.json`, `extract/dec/NNNNN.dec`: every LZ10 and delta resource and every 100th of the game's own kind, byte for byte); the icons' pixels against the research's rendered sheets (`m3/frames/spr_NNNNN_all.png` beside the folder, `RESEARCH_FRAMES=`; Pillow, `PYTHON=`): the research drew each frame's first piece only (its tile unit was 128 bytes, the game's is 32), so its pixels must all be in the port's frame (colours within 4: it spread 5-bit colours as c × 255 / 31) and a frame of several pieces must have more. Needs the ROM |
| `src/screens.*` | Which bottom screen the game shows: `Recognise(top, bottom)` → `ScreenState` (`Screen`, panel `Tab`), `ScreenTracker` (hysteresis; `App::screens`, `--print-screen`; CONSOLE uses it only when RAM does not read), the `Screen` values (FrontEnd, CreateASim, SaveSlots, ItemHand come only from RAM; Loading from RAM and `ScreensBlack`), and the pixel rules AUTO shares (`HudVisible`, `SocialVisible`, `WarmMenuVisible`, `VacuumVisible`, `CardsVisible`; `ScreensBlack` is CONSOLE's) |
| `tests/screens_check.sh` | Each save state's recognised screen, the panel's tabs from `s6` with `scripts/tabs.txt`, two-line catalogue entries from `sit-storebuy` (needs the ROM, `shots/` and the complete save: `ROM=… SHOTS=… MORE_SHOTS=… COMPLETE_SAVE=… bash tests/screens_check.sh`; `MORE_SHOTS`, default `shots/`, is looked in for states `SHOTS` lacks) |
| `src/hotspots.*` | The touch targets of each recognised screen (`HotspotsFor`: tap point and the area the focus ring frames, DS bottom pixels), `Neighbour` (focus moves: nearest area centre within 60° of the direction), the constants the navigator uses (`kTabIcon`, `kPhoneIcon`, `kDialogueArrow`, `kPopupClose`, `kPhoneClose`) |
| `tests/screens_test.cpp` | The HUD rule on synthetic top screens: any Sanity bar tint, no strip no HUD (`ninja -C build screens_test`) |
| `tests/hotspots_test.cpp` | The tables lie on the screen, every target reachable by focus moves, `Neighbour`'s cone (`ninja -C build hotspots_test`) |
| `tests/hotspots_check.sh`, `scripts/hotspots/*.txt` | Each target tapped in the game from a save state: the screens printed, or a last picture that differs from the same run without the tap (ROM, `shots/` and the complete save, as `screens_check.sh`) |
| `src/scene.*` | AUTO's decision from those rules: `AutoLayout::Update(top, bottom, mainIsTop)` |
| `src/script.*`, `src/shots.*`, `src/state.*`, `src/picker.*` | Headless input scripts (keys, `touch`, `blow`, CONSOLE's `pad <action>`, `layout <name>`); PNG shots and `--compose`; save states (a load re-seeds the clock); ROM file picker and `Tell()` |
| `src/mic.*` | What the DS hears: the blow key's signal, the computer's microphone (SDL capture, resampled), or silence |
| `src/clock.*` | The DS clock: the host's time at boot and after a state load, held to real time during fast forward; `--clock` |
| `src/platform.cpp` | melonDS Platform interface (files, threads, save, the microphone through `mic.*`), adapted from pokemon-fulldex |
| `src/host.h` | What platform.cpp needs from the frontend: the save path and hook, the mic mode and blow key |
| `src/thirdparty/stb_image_write.h` | PNG writer |
| `src/thirdparty/tinyfiledialogs.*` | File picker and message boxes (3.22.1, zlib), for starting without a ROM |
| `packaging/` | Icon (`tools/make_icon.py` draws it, original art: never EA's plumbob), Mac `Info.plist.in`, `make_icns.sh`, Windows `.rc` |
| `scripts/*.txt` | Input scripts used to walk from the title to the town |
| `tools/sheet.py` | Contact sheet of shots (Pillow) |
| `melonDS/` | melonDS 1.1, submodule |

## Build and test

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # CMake 3.21 to 4.x; Mac: cmake + ninja from pip work
    ninja -C build sims2wide
    ./build/sims2wide roms/<rom>.nds --headless --frames 400 --state-load shots/s7.st \
        --compose --shot-at 399 --shot-dir shots/c --save shots/c.sav

Always pass `--headless --frames N` in automated runs (a run without it opens a window and
waits) and a scratch `--save`. The ROM lives in git-ignored `roms/` (tested: European, `ASJP`).
Save states `shots/s2.st` (main menu), `s3` (Create-a-Sim), `s4` (Keep this Sim?), `s5`
(first conversation), `s6` (after it), `s7` (free roam in town, the phone open: Exit is at
126,126) are in git-ignored `shots/`; from s7: `act-social` (the Impress reaction game),
`sit-dialogue`, `sit-gift` (the inventory popup), `sit-cityhall` (inside, with the HUD),
`sit-build` (Build Rooms), `sit-store` and `sit-storebuy` (the shop floor, its catalogue),
`sit-pause`; the hotel (made on the DS clock 2026-01-01 12:03 to 12:46: run them with
`--clock "2026-01-01 14:00"` or later, never earlier): `sit-hotel` (the lobby, free roam, after
the concierge's first talk), `sit-concierge` (that talk), `sit-basement` (the dark reactor
room), `act-vacuum` (the vacuum just switched on in the lobby), `act-vacuum-clog` (the same,
filter clogged). Pass `--clock "2026-01-01
12:00"` to runs from the town states whose output is compared.

Headless runs default to CONSOLE (the first frame logs `frame 0: layout console`; looks off: settings
are never loaded there, so the looks-on defaults are the window's only). Every check that relied
on AUTO names its layout: `--layout auto` for the layout check, `--layout auto` (or `focus`) for
the 24-shot check. The layout check: from each state, `--headless --frames 200 --layout auto` must log `focus (top big)` for
s5, s6, s7, sit-dialogue, sit-gift, sit-cityhall, sit-store, sit-pause, and (clock 2026-01-01
14:00) sit-hotel, sit-concierge, sit-basement; `focus (bottom big)` for act-social, sit-build,
sit-storebuy, and (14:00) act-vacuum, act-vacuum-clog; nothing (SIDE) for s2, s3, s4; on the complete save (clock 2026-11-05 12:05) `focus (top big)` for sit-casino, sit-hotel-late. From act-social, 2400
frames go back to `top big` around frame 1570, when the game returns to town.

## Known traps

- The RAM map is the European ROM's (game code `ASJP`, header 0x0C): `App::portSupported`. On any
  other ROM `App::game` stays default (not valid, not between), CONSOLE uses the pixel tracker,
  the recorder records nothing, `--print-game` prints nothing, and one toast / stderr line says
  so. `tests/gamestate_check.sh` runs a copy with `XXXX` as game code (melonDS does not check
  the header CRC for direct boot).
- The game's screen object `S` lives on the heap: `ReadGameState` reads it again every frame and
  checks its vtable, never cached (`docs/research/ram-map.md`). Settled (`GameState::settled`,
  `ReadGameState`): the bottom slot `P+0xE42` is 3 and the popup state `P+0x8` is 0, 2 or 0xFF.
  A popup opening (1) reads Popup about 13 frames before its box is drawn (`s6`, the second goal
  popup: Popup from frame 48, drawn from 61) and closing (3) still draws it: a tap then would land
  on the panel underneath. Not settled is mid-switch: CONSOLE holds taps until it settles (the gate, below); the recorder only records a
  settled pair.
- The recorder writes to the player's pref folder: once per (overlay, vtable) pair across
  sessions, 20 a run, 200 in all. It writes nothing on another ROM and, headless, nothing without
  `--record-dir`. A capture's `.st` holds the game's memory: never commit or attach one; the
  `.png` is what a player sends.
- Known limits of CONSOLE's RAM-led screen: (1) quitting the cards shows about 10 frames of the
  cards' layout over black (the pixels go black before RAM leaves Cards); (2) a goal popup over
  the Options tab reads Unknown (the stick cursor) only without its HD view (`--no-romdata`): with it, every popup over
  the panel is `port/popup.*`'s; (3) the frame's
  corner radius is a literal (16, `Frame()` in `console.cpp`), not derived from the piece.
- Known limits of the HD status bar and conversations: (1) in German and Spanish the status bar's
  Sanity label keeps the game's own abbreviation (the string table has no longer one); (2) money's
  thousands separator is "," in every language (the game's table has none, `FormatHud`); (3) Sim
  names are assumed to be at most 7 characters (the game's name keyboard), so the plate and the
  bar's money column are sized for that; to check in Create-a-Sim with a full-width name.
- Known limits of the HD menu (milestone 3): (1) no save state has a greyed
  Interact row (the research found them in code only: "Can't pick up", "Detector full", …): drawn
  dim and skipped in `layout_test` / `navigator_test` only; (2) a popup over the open menu keeps
  it open beneath (drawn dimmed, no focus) and wins the focus; when the popup goes and the panel is back on the same tab,
  the menu is open again on the item it was on (the navigator's resume: `MenuView::popup`; checked by `popup-quit-no`);
  (3)
  the phone button and the portraits are live crops of the touch screen (their ROM pictures are
  not sprites the port decodes); (4) the Italian table has a sentence at the first object's name
  (463): beside the strip's plate it ends in an ellipsis; (5) "Pay ($@1)" 1859 and "Take $@1"
  1868 (codes 14 and 23) have no label method of their own setting @1 (the game's code sets it
  for Buy, Payoff, Bail and the other Pay only, `0x0208B708`, `0x02093588`, `0x02097554`,
  `0x020960E8`): the DS shows whatever its @1 buffer last held, so the HD row drops the amount;
  no save state shows any of these verbs, the amounts come from the code alone; (6) a tab whose part of `GameState` does not read keeps milestone 1's
  menu, and `--console-focus N` means a `MenuView` index on an HD tab (and on Give Gift and the Save slots); (7) the counted items'
  icons are live crops too (no sprite: the counted buttons hold none, only the ring's `0x0211F8F8`; the research's
  2506 is a ▶; they are likely drawn in the panel's background layer); (8) no state has
  an item Give Gift refuses (`refused`, from the code only: drawn dim and skipped in `layout_test`), nor a Sim whose
  reaction popup follows a gift into the HD menu; (9) the bin's word is the trash cans' name (469: Trash, Ordures, Müll,
  Rifiuti, Basura), the game having none; (10) what the hand does with a slot item that is not a tool (furniture) was not
  seen: A taps the slot and the hand as the stylus would; (11) Goals and Relationships hold the focus on the game's own
  selection (Up / Down its arrows) while it is in the list; the phone is reached by Up from the first entry only (the
  game has no arrow there: `scrollUp` empty), the game's words having none for its arrows the hint is the D-pad with its
  vertical arms lit (`Button::DPadV`; Options' sliders `DPadH`); (12) no state shows a hotel request goal (their texts, 2110-2128, and the records are from the code
  only), and their time (@3) and item (@4) are dropped; a goal done leaves the game's list, so none is drawn done;
  (13) Relationships' Secret Info button (a Sim with secret info) was seen in code only: not offered; (14) the Save
  slots do not show when a slot was saved (the header's time word was not decoded; its differences are seconds, its
  epoch unknown); the popups over them (its "Overwrite this save?" question, kind 2, and "Saving", kind 3) are
  `port/popup.*`'s, drawn over the waiting slots, the question's cross and tick from the box's place in RAM; (15) the slot screen's X goes back to the panel's Interact tab (the game's own doing);
  an empty slot's A saves at once (the game's tick), a used one's asks first (the game's question: A Yes, B No); the
  slots' view never replays a press the gate held (`MenuView::replay` false); (16) popups (`port/popup.*`): the
  buttons are where the game's own code puts them (0x02057314: 24 x 24 from the box's y `P+0x544`, width `P+0x550`,
  height in tiles `P+0x554`; Game Paused / tips X at 132,132, the goal's at 132,116, Quit's cross / tick 92,116 /
  164,116); a question's title in RAM is stale ("Loading" on the complete save): not drawn; a popup over a screen the port
  does not redraw (the catalogue, Build, the activities) keeps milestone 1's composition (the game's own box in the touch
  screen); the pictures of an HD screen under the popup's box are left out (images are drawn above every still item);
  the tips' ids are not all known (`--measure-menu` measures the call sites' 23, 65, 313, 413, 1158-1159, 1191, 1761,
  2423-2450, 2586 and the titles); (17) the phone (`port/phone.*`): its rows are moved by the game's own DS Up / Down
  (2 frames held, 4 released, one press per row: `phone-pages`, `phone-message`), never by touch (the rows are not
  touch targets); the maid's price is the static `0x0213E430` (set on page 5), the answers' Exit row the statics
  `0x0213E41C`-`0x0213E42C`, page 3's "Next Message" row `0x0213E428`, the maid's answer `S+0x862`; the contacts'
  list is worked out from the Sim records (guests checked in, then the Sims met and in town) and kept only when it
  agrees with the game's count and highlighted Sim (the game asks one more function, `0x02077E08`, not decoded):
  else, as for a recorded custom ringtone (its name not decoded; none in any save), the HD phone shows the game's own
  LCD as a live crop 2x; page 1 (an incoming call) and "You Have No Contacts" are from the code only; Exit from a
  friend's answer goes back to the contacts (page 9 → 7, seen); the header's clock is the status bar's; (18) notes
  (`port/note.*`): no save state reaches one, so the view and the panel are checked on made-up states only, the
  game's X place (234,171) is from the code only, A and B press the DS's A (the game's own way to close a note),
  and the parchment backgrounds (4161 / 4162: BG images the port does not decode) are drawn in their colours instead.
- UI kit traps: (1) the HD crops are taken from the DS framebuffer at fixed rows (`HudCropRows`:
  176 of 192, the world without the status bar's rows; the HD bar is drawn in the strip below); a change to a crop's rows moves every picture and the `console_check.sh` hashes;
  (2) the font is embedded at build time (`cmake/embed.cmake`), never read from disk: a new
  weight means a new byte array and an `OFL` line in the releases; (3) the raster must stay
  deterministic (premultiplied floats, one thread): `uikit_test`
  pins a hash of the still layer, and `console_check.sh` compares composed pictures byte for
  byte, so a rasteriser change is a deliberate re-baseline, never a side effect.
- melonDS framebuffers are `0xAARRGGBB`, 256 × 192 each, `GPU.Framebuffer[FrontBuffer][0]` =
  top, `[1]` = bottom.
- The game's sprites (`romdata.*`): a piece's tile index counts 32-byte tiles, not the 128 of
  the research's renderer, so the research's sprite PNGs show only each frame's first piece
  (the right and bottom strips of a 24 × 24 icon are missing there); the frame count is the
  u16 at byte 6 of the descriptor, and a frame's tile offset is a byte offset into the tiles resource, each frame its own
  compressed stream with its own header (`--dump-resource` of such a resource unpacks the
  first stream only). A sprite descriptor 24 × 24 begins `18 18`: as a compression header
  that is LZ10 with a size, which is why `Decompress` wants the low nibble of byte 0 clear
  (every compressed resource of the game has it so). The icons' colours are melonDS's
  spreading of 5 bits (`(c << 3) | (c >> 3)`, the same values as its frames' pixels), not
  `c × 255 / 31`: a check against another renderer allows 4 a channel.
- HUD detection uses the status bar's frame, never what lies under the Sanity bar: row 176
  `#597192` (the strip's top edge) and row 191 `#384961` (under the bottom border), both flat
  x 8-248, and row 190 `#415969` x 8-136 (the bottom border under the clock and the money).
  Rows 177 to 190 under the bar's box (from x 144) take the bar's colour: row 190 there is
  `#415969` with the red bar of the town's states, `#495969` with the yellow one of the complete
  save (the casino, the late hotel lobby); the green and silver levels were not seen, so nothing
  from x 144 on those rows may be used (`tests/screens_test.cpp`, synthetic screens). The three
  rows are on or off together on every frame of every state. Row 186 crosses the clock's white
  text; row 189 is not flat.
- The social reaction games keep the HUD: they are told by row 0 of the top screen (`#597192`
  from x 5 to 252, the top edge of the name and relationship box), not by the red bar, whose
  fill may change. The shop's catalogue and Build Rooms are told by warm, bright corners of
  the bottom screen (r > b + 40, luma > 100); the panel, the phone, popups and the paused
  panel are cool. The social game's bottom barely moves (motion ≤ 0.002 on the sampling grid):
  do not detect activities by motion. The vacuum keeps the HUD: told by row 1 of the bottom
  screen (`#30A2D3` x 4-44, `#304951` x 60-100; clogged, the sepia tint's fixed samples of
  the same row). The casino's card game has no HUD: told by its score bar on the top screen
  (row 17 flat `#594120`, row 19 `#CB9A38` from x 40; `CardsVisible`). Only rules checked on a
  save state of the activity: a guessed "dark top, lit bottom" rule made the touch screen big in
  dim places (the casino, as the owner played) and was removed.
- The hotel route (from s7, goal "Check out the HOTEL"): close the phone (X at 237,15), L 94
  frames then up: the hotel is the building with the blue bed sign; enter by walking up into
  its red door. The concierge's talk gives "nuclear rods in the BASEMENT": rods lie on the
  floor (lobby corridor, the lounge right of the desk, the basement corridor; Pick Up at
  234,61); the basement door is the dark door at the back of the lounge, then stairs down, a
  corridor and the reactor room's door; the furnace takes six ("Add 1 Fuel Rod" at 234,61).
  Back in the lobby a cutscene: Jebediah wants the hotel vacuumed. The vacuum is the red upright
  at the desk's end (Pick Up); Pockets tab (16,59), the vacuum (232,88), the hand (194,107)
  switches it on. Its goal ("clean up 5 DUST piles") did not complete in ~5000 frames.
- In the vacuum up walks forward, left/right turn, down backs up; dust and finds swirl in the
  bowl and go down the hole; a find dragged (successive `touch` events) into the box at the
  bottom right is kept. Touching dust with the stylus clogs the filter: the whole touch screen
  turns sepia (dithered), a mouth and an arrow blink bottom left, dust stops draining
  (`act-vacuum-clog`). Holding `blow` 7 frames or more clears it (gone 17 frames after the blow
  starts); 6 frames do not; without it the clog stays. melonDS's recorded blow
  (`melonDS/src/frontend/mic_blow.h`) gives exactly the same thresholds, so the synthesised
  wave stays. The blow key is verified in game. CONSOLE's Y holds the blow 8 frames from the press (a tap is 4-6) and blows in every mic mode (`Host::MicForce`); the M key only with mic=blow.
- Navigation headless is slow: the camera follows, L/R turn it (about 156 frames a full turn),
  walls stop the Sim silently. Branch from saved states and try several scripts in parallel
  (background runs), comparing their last shots.
- melonDS ticks the RTC on emulated time and keeps it in the save state; a direct boot leaves
  it at 2000-01-01. Never set it back (the game punishes that): `clock::Seed` after `Reset()`
  and in `LoadState`, `clock::Frame` holds it during fast forward (headless: only with
  `--fast`). `--clock` fixes it and turns the holding off. `nds.RTC.SetDateTime` keeps the
  12/24-hour flag and the sub-second phase; it is fine mid-transfer.
- The game opens the mic only when it samples the TSC's AUX channel: in free roam
  `Mic_ReadInput` is never called (0 samples), so a mic test needs a mic situation (the
  vacuum, the music mixer). `--mic real` needs `NSMicrophoneUsageDescription` in the Mac app's
  plist (there) and the terminal's permission on a Mac for the CLI program.
- `SIMS2WIDE_PROBE=N` prints AUTO's signals every N frames (HUD, social, warm menu, motion and
  brightness of each screen); `--print-screen` prints the recognised bottom screen on change, and
  `--print-game` what the game's RAM says (also at frame 0; `pixels disagree` when the two part for 90 settled frames).
- Touch mapping goes through the same composition (`compose.h`, from `Place()` or `BuildConsole`) as drawing: change one, the other follows. The topmost piece under the pointer decides, for a click and for the camera alike: a touchable bottom piece is the stylus, a touchable top piece the camera (wheel, right drag), an untouchable piece (`touch=false`: CONSOLE's dimmed world behind the menu) neither, and it hides what lies under it. Overlays never take the mouse. A drag keeps the piece it began on; if that piece is gone (the layout or the screen changed mid-drag), the touchable bottom piece under the pointer, else the largest; always held to the piece's source rectangle and the DS screen.
  The stylus goes down only when the left button is pressed on the touch screen; until release
  it follows the pointer clamped to that screen (`TouchClamped`), wherever the pointer goes.
  Window points and renderer pixels differ on HiDPI; `Video::ToOutput` converts. FOCUS's panel
  lies over the world: it is drawn second (`Placement::topFirst`, `alpha`; a later `Piece`) and wins the mouse.
- The game zooms in with X and out with Y; L and R turn the camera continuously while held
  (checked with `scripts`-style runs from `s7`). The wheel and the right-button drag press them.
- Settings are never loaded or saved headless (runs must be the same everywhere), and headless
  starts with the looks off (`main.cpp` resets resolution 1, widescreen and 60 fps off, though
  `Settings`' defaults are on for CONSOLE); only `--res3d`, `--wide`, `--interp` turn them on; the menu
  never opens there. Window events in a script (`press`, `click`, `winshot`) count loop turns
  (`iter`), not game frames: the game stands still while the menu is open.
- The menu's font has capitals, digits and `:-+./<>!(),` only: write its text in capitals.
- A real pointer resting on the menu sends motion events; hover selects a row only when the
  pointer moves onto another one, otherwise it overrides keyboard navigation.
- Fullscreen keeps the pointer visible: the mouse is the stylus.
- In town the bottom screen often has a popup to close (the X near 126,110 or 126,126) before
  the D-pad moves the Sim.
- The bottom screen (`Recognise`, `screens.cpp`; HUD first: without it only Cards or None).
  Measured on every frame of the save states (1200 frames each), each rule matching nothing
  else: the panel's tab, by the selected icon's green ring `#309A41` in the tab column (x 0-34,
  a 35 × 35 square around rows 22, 57, 92, 127, 162; 26 to 51 ring pixels, 16 needed; no other
  screen has that colour there); the phone, row 105 flat `#9AB29A` (its LCD) x 34-221; a popup
  (goal unlocked, tips, Game Paused), by the dimmed panel behind its box: rows 0 and 191 flat
  `#303859` x 36-252 (`#6171B2` undimmed), wherever the box sits, and the box's fill, row 120
  `#828ACB` x 49-113 and 144-200 (the same in all three boxes): opening the phone fades the
  panel to black through the very same dim for 2 frames, with no box. The X is not used (it
  turns green when touched, and the box slides; while it slides the screen reads Unknown). A
  conversation, row 171's fixed samples every 3 pixels from x 214 (the blue arrow); Give Item,
  row 86's every 3 from x 112 (the bow button); the catalogue, the bottom edge of the item's
  description box, row 62 flat `#51518A` and row 63 flat `#302008` x 12-244, the same whether
  the description takes one line or two (row 55, used before, crosses a second line: Red
  Shower, Crimson Toilet); Build Rooms, rows 10 `#695128` and 23
  `#302008` x 36-219 (both also warm). Fades and slides between screens read `Unknown` for up
  to about 30 frames (the dim fading over 14 frames, the phone opening over 36): the tracker
  takes Unknown only after 60 agreeing frames, Popup and Dialogue after 3, the rest after 6.
  `sit-pause` reads Popup.
- Touch targets (`hotspots.cpp`, all checked by `tests/hotspots_check.sh` unless said). Popups:
  the goal box's X spans y 104-127, the tips' and Game Paused's y 120-143 (x 120-143 in all);
  `kPopupClose` (131,124) closes all three (Game Paused also resumes on Start, A or B); a tap
  in the box elsewhere does nothing. Options: a volume slider moves only by a drag that
  starts on its knob (taps on the track or the knob do nothing; the knob sits at full volume,
  232 on rows 121/171, until moved; the game keeps a volume in tenths: the knob at x 72 + 16 k, a
  drag from it lands on the nearest tenth when let go, `options-volume` checks 100 → 90 → 80 %
  from RAM); Save opens the slot screen and Quit a confirmation, both
  read Unknown in pixels; CONSOLE (from RAM) sees SaveSlots (in HD: its rows' ticks at 232, 67 /
  113 / 159 save, its X at 232,22) and, for Quit, a popup over the
  Options tab that it takes as Unknown, its HD popup's (A Yes: the tick; B No: the cross); the button by the title (193,17) shows no effect. Pockets: the bin (67,167)
  throws the selected slot's item away; the hand uses the selected item (the Super Drencher
  starts its game; a key item that cannot be used here gets a red ring). Catalogue: "Buy" and
  "Sell" are only the mode, set by selecting a shop item or a bought one; the simoleon button
  (147,128) buys or sells; buying the same item again fills the six owned places. Cards: hand
  cards lift on a tap only after the draw; the play arrow (shown after the draw) takes a drag,
  not a tap; X asks "Are you sure you want to quit?" (tick 93,92, cross 163,92) and still reads
  Cards. Relationships: the up/down arrows select the previous/next Sim (`relations-select`). Goals: the
  arrows by the giver's portrait, up (62,59) and down (62,145), select the previous / next goal and
  scroll the list so it shows (`goals-scroll`, from `B-pg8-goals`). Pockets slot 1, the hand and the bin
  and Give Item's slots 1-2 and bow are checked in game by `console_check.sh` (`pockets-*`, `gift-give` on
  `A-pk-slot1-key2` / `A-gift2-base`). Placed from shots only
  (a tap shows nothing in every state we have): Pockets slots 2-6 (empty), the Goals lines (one
  goal at a time seen; a goal takes one to three 16-pixel lines), the Interact strip (one target
  near), Give Item's slots 3-6 (two items at most), the cards' discard pile and tenth place
  (empty), Build Rooms' rows and arrows (no room to build), the Options button by the title.
- CONSOLE (`console.cpp`): the world is drawn 720 high, centred (the DS's 4:3: 960 wide, dark
  sides) or, wider than 16:9, cropped at its sides (a conversation's world without its status
  bar: 176 of 192 rows); stretched only when within 8 pixels of the canvas's width (under 1 %:
  the widescreen world, 341 x 192, is 1279 wide at 720 high). The small world of activities is 384 wide, its height
  from its shape (288 at 4:3, 216 widescreen). The HD status bar (`port/statusbar.*`, with the
  game's words and RAM's values: `App::hudStrings`, `App::Hud()`) replaces the game's: the world's
  source rectangle drops the top picture's last 16 of 192 rows (`HudCropRows(topH)` = `topH × 176 / 192`, whatever the
  looks' size; the conversation's crop uses it too) in town, the menu composition (Phone, Popup, Gift, SaveSlots, ItemHand, Unknown,
  the menu, the HD conversation) and the activities that keep the HUD (Social, Vacuum, Catalogue, Build; the cards
  have none), the world keeping its shape and its whole width: never cut at its sides to make
  room for the crop (4:3 town: 1047 × 720 from x 116; wider than the canvas, the widescreen town
  341 × 176, fitted to the canvas's width and top-aligned, 1280 × 661, the backdrop in the strip
  below; the HD conversation's world too; only milestone 1's conversation (no game text) still
  crops its sides). The bar sits at the
  world's bottom left (16 in from its left, y 660, 48 high: below the menu's frame, which ends
  at 654, so it does not move when the menu opens, and undimmed over the dimmed world, as the
  game keeps its own up), and in the activities under the small world (6 frame + 12 below it,
  as wide as its frame: rows 344-391 at 4:3, above the hints' box). A conversation keeps the
  crop and shows no bar (its box takes the bottom; `port/dialogue.*`, above). Without the words (another game's data) or
  RAM (another ROM), milestone 1's compositions: the game's own HUD in the world. A load's
  garbage (`hud.valid` false, values out of range) hides the bar for that frame; the crop stays
  (no jump). The dialogue frame is 256 high, not the
  mockup's 236: name and text at 2x take 228 rows (the mockup's own picture hangs the text out
  of its frame). The rail (the panel only) shows the tab column x 0-34 (35 columns: the selected
  icon's ring takes them all) and the phone icon; the phone (Phone, `s7`) is the menu
  composition, the touch screen 3x over the dimmed world. Dialogue pieces: the name bar whole,
  x 10-165 rows 6-25 (the name's last pixel row is 24; its right part opens downwards into the
  text box, so its bottom border stops at x 153); the text x 8-159 rows 26-123; the arrow x
  219-249 rows 155-186 (its dark outline ends at x 249; column 250 holds only a one-pixel fringe
  of `#6171B2` on rows 168-175, and row 187 seven such pixels, both left out because the screen
  frame's rounded corner starts at 250,186 and 248,187). The hints' box (`kHintBox`, 850,400, 400 wide, as high
  as its rows, 252 at most): `AddHints`, one row per `Hint` (a chip in `#5961A2` with the
  button, round for one letter at 3x, a word at 2x; the text at 3x from x 972, white with a
  `#383869` shadow one font pixel off), drawn as text overlays (`Overlay::text`: the 5 × 7
  font, literal strings, lines split by hand with `\n`: 14 characters beside a chip, 20 in a
  note). The window's overlay texture cache holds 64 (an activity's hints are some twenty). `menuOpen` shows the menu composition on any screen with a HUD; Phone, Popup
  and Gift always do, and SaveSlots, ItemHand, Unknown (the stick cursor's screens) with a frame
  around the touch screen (`FrameAround(kMenuTo)`). CONSOLE never shows the two screens side by
  side as SIDE does: a black load (Loading) is the bare backdrop (no piece, no overlay: nothing to
  touch, no cursor); without a HUD (FrontEnd, CreateASim, None: title, main menu, Create-a-Sim,
  an unmapped front-end screen, the pixels' None when RAM does not read) the front end's view, the top screen fitted at its own shape in `kFrontTopBox`
  (24,72, 568 × 480: 568 × 426 at 4:3, 568 × 320 widescreen, centred; touchable as the camera,
  as SIDE's was) and the touch screen 2.5x at `kFrontBottomTo` (616,72, 640 × 480, the only
  stylus piece; 3x would not fit beside the top), both framed on the dark canvas, and a box at
  y 600 with `kFrontHint` at 3x ("STICK: CURSOR - A: TOUCH": these screens are touch-only, the
  DS's A, Start and D-pad change nothing on the title, `s2`, `s3`, `s4`; the font has no
  apostrophe and no middle dot). The stick cursor is drawn where the piece showing its point draws it
  (`CanvasFromTouch`), hidden where no piece shows it.
- The navigator (`navigator.cpp`, CONSOLE only; `Input::Steer`, before the mouse, and the touch
  script last as ever). Its screen is the game's (`App::ConsoleScreen()`, from RAM; the pixel
  tracker only when RAM does not read). RAM leads the pixels by up to about 60 frames (6 to 24
  seen from `s7` and `s6`) and the game reads not settled while the bottom slot switches
  (`P+0xE42` != 3) or a popup slides (`P+0x8` 1 or 3); a panel tab of -1 (between screens, `Panel/?`) is Unknown. In the black loads (CAS →
  town, the title → CAS, quitting the cards; `game Unknown(ov=-1,vt=0x00000000)`) the manager
  reads but its bottom object pointer `P+0xDAC` is 0, so the chain is not valid
  (`GameState::between`); both screens are exactly black there (a fade of 60, 28, 0 over 4
  frames before; `ScreensBlack`, at most 8 a channel). Between screens or Unknown, without the
  HUD and black: Loading (the bare backdrop, the DS's buttons); an Unknown without the HUD that
  is not black: None (`ConsoleScreenOf`: the front end's view). Measured: cards quit Loading on
  frames 102-119, title → CAS 29-30, CAS → town 90-125 (124-125 are `Panel/?`). Options > Quit is a popup over the Options
  tab in RAM: CONSOLE takes it as Unknown (its yes / no are not `kPopupClose`), and its HD popup view
  (`port/popup.*`, `MenuView::own`) makes it a focus screen. The gate: on a focus
  screen or a popup, a press that would tap (A; B / MENU where they tap a close; LB / RB on the
  panel) while not settled is held, the latest one winning, and replayed on the first settled
  frame, or dropped after 90 frames or when the screen changes; focus moves and the DS's own
  buttons never wait; activities are not gated. The 90-frame wait after a closing tap still
  applies to a replayed press. SaveSlots with its HD view (`SaveSlotsView`, the game's strings and
  RAM's slots): a focus screen like Give Gift (A the focused slot's tick, B the game's X, no cursor).
  FrontEnd, CreateASim, None (touch-only, no table yet), SaveSlots without it,
  ItemHand: as Unknown (A presses the stick cursor). Panel with the menu closed, Dialogue, Loading: the buttons are the
  DS's (LB / RB the camera's L / R; A advances a conversation itself). MENU (the PANEL action:
  E, the pad's Back) opens the panel's menu only; Esc / Guide / Start + Back stay the pause
  menu; the pad's TOUCH MENU acts on Back's release, and not at all if Start was held meanwhile
  (`ReleasePulse`: Start + Back in either order is the pause menu); the key E acts on its press.
  Focus screens (the panel's menu, Phone, Gift, Catalogue, Build): the
  D-pad or left stick moves the focus (`Neighbour`; held: again after 18 frames, then every 6),
  A taps, on the panel LB / RB tap the previous / next tab icon (in a ring) and B / MENU close
  the menu (no tap); on the Phone B / MENU tap `kPhoneClose` and close the menu (with its HD view, `port/phone.*`: B
  its X, Up / Down / A the game's own keys); elsewhere B taps
  the screen's `close` target if it has one. Popup (without its HD view): A / B tap `kPopupClose`, Start stays the
  DS's (Game Paused); with it (`port/popup.*`): A the focused button, B the X or the cross, Start passed on. Unknown: the DS's buttons but A, which presses the stick cursor when one
  exists (a controller and the setting on: `PadState::cursor`; otherwise A stays the DS's, so
  the keyboard alone still answers Save and Quit); the stick
  cursor works in CONSOLE only on screens without a table (None, Vacuum, Unknown, and the RAM's
  FrontEnd, CreateASim, SaveSlots, ItemHand). The menu
  stays open only while the panel changes tab; any other screen change closes it and puts the
  focus back on the screen's first target. A tap is queued only when the touch script does not
  own the stylus (presses meanwhile are dropped); after a closing tap (popup, phone, X) A and B
  wait for the screen to change (90 frames at most): the popup box slides away still reading
  Popup and a second tap would land on the panel. Volume sliders and the cards' play arrow take
  a drag only (`Hotspot::dragOnly`): the focus skips them (the mouse does the sliders, the
  cards' A the arrow). Presses are edges against the previous `PadState`; after `Leave` (another layout) or
  the pause menu, buttons still held are not presses. The DS's Select moved to the left stick's
  click (Back is the touch menu's). `Bindings::Apply` loads settings through `SetKey`/`SetPad`
  (the swap), so an older file giving an action Back or E (now TOUCH MENU's defaults) still binds
  each input to one action (`tests/controls_test.cpp`). Keys need one more step: settings load
  before SDL's video is up, when the keys asked by position (A, B, X, Y, L, R, TOUCH MENU's E)
  are still 0, so the window gives the keys again through the swap right after `SDL_Init`
  (`Bindings::ResolveKeys`; a 0 left by the load counts as a default). The test gets the keymap
  from SDL's dummy video driver. From `s7`, `pad b` closes the phone (Panel/Interact about 50
  frames later, the phone slides).
- The activities (`activities.cpp`, CONSOLE: Social, Cards, Vacuum go to `ActivityInput`; Start
  and Select stay the DS's). Social: Left / Up / Right (or the stick) tap the left / middle /
  right answer, A the focused one. The tapped answer lights green about 4 to 12 frames after the
  touch (`act-social`, a tap at frame 30: green on frames 34-42), then the game plays on: it
  leaves the social screen around frame 1600 whatever the answers (no answer at all: 1606), so
  a check cannot wait for it to leave. Cards: the focus as on a focus screen, starting on card 1;
  A on a card taps it (lifted) and A again on a card it lifted drags it onto the play arrow
  (`Drag`: 4 frames on the card, 8 steps of 2 frames, 4 on the arrow; a lone card snaps back,
  seen headless; a real play of three alike was not reached); X taps the deck; Y drags the
  focused card onto the discard pile (9 → 8, the end of the turn); B taps X. The question "Are
  you sure you want to quit?" still reads Cards: `ActivityState::confirm`, set by the quit tap,
  makes A tap its tick (93,92: Cards → None → Panel/Interact) and B its cross (163,92: the game
  goes on); the other buttons wait. Answered with the mouse, the flag stays until the next A or
  B (which then lands on card 2's or card 4's place) or a screen change. Jun's card count is on
  the top screen at x 83-86, rows 3-12 (the checks compare x 82-87, rows 2-13). Vacuum: the
  left stick drives (the DS's D-pad), the D-pad too unless X or A is held; the hand is the stick
  cursor, moved by the right stick (a controller, the setting on or off) or by the D-pad 2 pixels
  a frame while X or A is held (the keyboard's way), shown without a controller; A held keeps the
  stylus down there (dust touched clogs, as with the stylus); Y is the microphone
  (`Navigator::Blow`, ORed into `App::micBlow`; held 10 frames from `act-vacuum-clog`, the clog is
  gone at frame 30: row 1 back to `#30A2D3`); B, L, R stay the DS's. The mockup's RT for the grab
  is the pad's FAST FORWARD (RT), so the grab is A (and the TOUCH binding, LT, presses the hand
  too).
- The complete save's states (in git-ignored `shots/`; run them with
  `--save` a copy of `saves/complete-play.sav` and `--clock "2026-11-05 12:05"`): `sit-casino`
  (`act-cards` quit to the casino's panel, `scripts/hotspots/casino-base.txt`) and
  `sit-hotel-late` (`saves/complete-late.st`, the hotel lobby): both read `Panel/Interact`, and
  AUTO shows `focus (top big)` there (SIDE before, the HUD's yellow-bar tint not being
  recognised). Quitting the card game goes Cards → None (a black load, about 50 frames) →
  Panel/Interact; Hotseat goes back through None to Cards. Hotel rooms and the penthouse were
  not reached.
- `act-cards` runs whose pictures are compared need `--clock` after the state's time
  (`2026-11-05 12:00`): without it the HUD clock is the host's and two runs differ by it.
- Looks: with all looks off the headless output must stay byte for byte the same (24-shot
  shasum check in CONTRIBUTING, run with `--layout auto`: the default is CONSOLE now). Frame 0 after `--state-load` shows melonDS's uninitialised 3D
  buffer, so even the memory layout of melonDS's objects matters: patch state goes in globals
  (`Looks3D`, `Looks2D`), never in new members.
- Looks: a patch that no longer applies fails the configure with a message. To edit one, change
  the submodule's files, then `git -C melonDS diff -- <files> > patches/melonDS/000N-….patch`.
- Looks: the wide picture is published one frame after it is drawn; `Looks::Frame` waits three
  frames after the looks are switched on. `--wide` changes the box test, so it changes what
  the game draws (and its output); `--res3d` and `--interp` change only what is shown.
- Looks: seams in the big 3D. `GPU3D_Wide.cpp` grows each triangle by 0.34 output pixel to
  close gaps between polygons; a sample in that margin must go back to the triangle's *nearest*
  point (`edge[]`, square to the edge). Clamping its weights instead moved it along slivers
  (polygons seen edge-on, often made by clipping at the wide view's edge) to a far corner's
  depth and texels: the two cracks of light on Create-a-Sim's curtain (`--wide`, any `--res3d`).
  To find what draws a stray pixel, log the triangle and weights at that pixel in the write loop.
- A press on the touch screen keeps the stylus down at least 3 game frames (`tapFrames`): a
  click or a trackpad tap sends down and up within milliseconds, before the next frame, and the
  DS never saw it (the owner's "the mouse doesn't work on the cards").
- `act-cards.st` (shots/) is Keelhaulin' Cards, "Draw a card!", from the owner's play on the
  downloaded complete save (not in git: `saves/complete-play.sav`, pass it as `--save`); the
  deck is at touch 127,30. Layout check: `focus (bottom big)`.
- The window paces itself (`keepPace` in main.cpp, 59.83 fps from the performance counter, not
  during fast forward): vsync alone follows the screen, and a 120 Hz screen ran the game at 2x.
- The DS buttons' default keys are asked by position (`SDL_GetKeyFromScancode`), so AZERTY and
  QWERTZ get the QWERTY places; the other defaults (M, Enter, arrows, F keys) are by name.
- JIT is off (`ENABLE_JIT OFF`): macOS arm64 needs MAP_JIT entitlements.
- Running `sims2wide` with no ROM opens a file picker and waits: in automated runs always pass a
  ROM (or `--help`/`--version`).
- The Mac app is opt-in (`-DSIMS2WIDE_MAC_APP=ON`) so `build/sims2wide` stays the CLI program;
  the bundle's binary is `Contents/MacOS/Sims 2 DS Wide`. Apple silicon refuses unsigned code:
  the release ad-hoc signs (`codesign -s -`) after copying.
- tinyfiledialogs builds shell commands (osascript, zenity) on Mac and Linux: never pass it
  outside data (paths, ROM text) unchecked. `SafeForPicker()` gates the default folder; message
  boxes go through `SDL_ShowSimpleMessageBox`.
- Windows: GUI subsystem; `AttachConsole` only when stdout is not already redirected (CI reads
  `--version` through a pipe). `windows.h` is not included (its macros clash); the two kernel32
  functions are declared by hand.

## Tasks

✅ done · 🔄 in progress · ⏳ to do

| # | Task | Status |
| --- | --- | --- |
| 1 | POC: melonDS + SDL frontend, headless study of the screens, AUTO layout (CONSOLE is the default now) | ✅ |
| 2 | Repository template (docs, CI, release, issue templates); portfolio page in `lborruto/docs` waits on a local branch until the repo is public | ✅ / 🔄 |
| 3 | Bigger side panel: the touch screen at 2x over the world's right edge | ✅ |
| 4 | Looks: widescreen 3D from FullDex's `GPU3D_Wide.cpp`, sharper 3D, 60 fps (on by default in CONSOLE) | ✅ |
| 5 | Survey the minigames and the other situations: social reactions, the shop, Build Rooms, City Hall, pause, the hotel's lobby, basement and vacuum done; the vacuum's goal, hotel rooms, cards, painting, mixer… not reached | 🔄 |
| 6 | Desktop apps: file picker, universal Mac app, Windows .exe (CI and release) | ✅ |
| 7 | Comfort: pause menu, settings file, controllers, mouse camera, fullscreen, sound stretching, save backups | ✅ (controller untested on hardware) |
| 8 | Playable: DS microphone (blow key, real mic), DS clock = the computer's, touch screen big for its activities, mouse drags held to the touch screen | ✅ (blow verified in the vacuum's clog) |
