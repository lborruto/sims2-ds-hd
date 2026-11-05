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
| `src/app.h` | Shared state: the `NDS`, layout chosen/shown, `mainIsTop`, fast forward, quit, `micBlow`, `topAspect`, F5/F12 paths |
| `src/settings.*` | `Settings`: every option with its default (CONSOLE's: `layout=console`, widescreen on, 3D resolution 2, 60 fps on); `settings.ini` load/save (SDL pref path), not headless; `layout=` round-trips (`tests/controls_test.cpp`) |
| `src/controls.*` | Actions, key and controller bindings, defaults, names (from FullDex) |
| `src/input.*` | Keyboard, mouse (stylus; wheel and right drag as the camera), SDL_GameController, stick cursor, script window events |
| `src/video.*` | The SDL window: draws the shown layout and the overlay, fullscreen, scaling, maps window points to touch positions |
| `src/ui.*`, `src/menu.*` | `Canvas` (ARGB overlay, 5 × 7 font) and `Menu` (from FullDex); `PauseMenu` (pages, bindings capture, toasts, cursor) |
| `src/audio.*`, `src/audiostretch.h` | SDL sound queue, WSOLA stretching when slow (from FullDex), volume, fast forward |
| `src/backup.*` | Copies of the `.sav` in `backups/`, at start and every half hour, eight kept |
| `src/layout.*` | `Place()` (canvas per layout, `topAspect`, FOCUS's overlaid panel), `CanvasFit`, `TouchFromOutput`, `OverTop` |
| `src/compose.*` | `Composition`: pieces (a rectangle of one DS screen at a canvas rectangle, opacity, touchable or not) in drawing order, and overlays (rounded frames, boxes, the dimmed backdrop, the focus ring; `RasteriseOverlay`, each drawn before the piece it names); `FromPlacement`/`Compose`, `TouchFromComposition`, `TouchClampedComposition`, `OverTopComposition`; what `Video`, `--compose` and the mouse use |
| `src/console.*` | CONSOLE (`--layout console`): `BuildConsole(ScreenState, menuOpen, focus, topW, topH)`, the 1280 × 720 composition per screen (`App::ConsoleScreen()`: the game's from RAM, `ConsoleScreenOf(App::game, …)`, the pixel tracker's only when RAM does not read; town and its rail, the menu over the dimmed world (framed for SaveSlots / ItemHand / Unknown), the dialogue rebuilt wide, activities, the front end's view for None, FrontEnd, CreateASim; never SIDE), its geometry and the game's colours; `App::consoleMenu` / `consoleFocus` (the navigator drives them; `--console-menu` / `--console-focus N` pin them for checks' pictures, `App::consolePinned`) |
| `src/navigator.*` | CONSOLE's navigator: `PadState` (the actions held: DS buttons, MENU = the PANEL action, sticks), `Navigator::Update(ScreenState, pad, TouchScript&, dsKeys, settled)`: per screen the DS's buttons passed on, or a focus over `HotspotsFor` moved by the D-pad and tapped with A; the gate (a press that would tap waits for `settled`, `App::Settled()`); `Leave` when CONSOLE goes; `Input::Steer` runs it (window and headless) |
| `src/activities.*`, `src/padstate.h` | CONSOLE's activities on buttons: `ActivityInput(Screen, PadState, TouchScript&, dsKeys, ActivityState&)` (social answers, the cards' draw / lift / play / discard / quit question, the vacuum's drive, hand, grab and blow), `ActivityHints` (the hints' box rows); `PadState` (shared with the navigator) |
| `tests/navigator_test.cpp` | The navigator on synthetic screen states: menu, focus moves, taps, tabs, popup, phone, town keys, Unknown's cursor, leaving CONSOLE, the activities, the gate (unsettled presses held, replayed, dropped after 90 or on a screen or mode change; DS buttons not held), the front end's screens, save slots and the quit question's cursor (`ninja -C build navigator_test`) |
| `tests/controls_test.cpp` | The bindings: one action per input, by default and after loading an older settings file (`ninja -C build controls_test`) |
| `scripts/console/*.txt` | `pad` scripts for `tests/console_check.sh`'s navigator cases |
| `tests/console_check.sh` | CONSOLE's pictures from save states: the DS pixels where each piece puts them, the frames, the dimmed world, the focus ring, the front end's view (`s2`, `s3`, `s4`), the save slots framed over the dimmed world (`sit-store`, Options > Save); then the navigator from `pad` scripts (`--print-game` and `--print-screen`, no `pixels disagree`: tabs from `s7`, the popup from `s6`, CONSOLE → FOCUS → CONSOLE) and the activities' (a social answer, the cards' draw, discard, quit and its cancel, the vacuum's blow), the hints' box (ROM, `shots/`, the complete save and Pillow: `PYTHON=…`) |
| `src/touchscript.*` | `TouchScript` (`Input::touchScript`): taps, holds and drags over game frames, queued (8 at most), one released frame between; wins over the mouse and the stick in `Input::Frame` |
| `tests/layout_test.cpp` | Touch mapping of every pixel in every layout and size, and of every piece of every CONSOLE composition (`ninja -C build layout_test`) |
| `tests/touchscript_test.cpp` | The synthesised touches (`ninja -C build touchscript_test`) |
| `src/looks.*` | Sharper / widescreen 3D and 60 fps: applies the Looks settings, builds `App::TopImage` |
| `src/melonds_ext/GPU3D_Wide.*` | FullDex's wide 3D rasteriser (CPU), built into melonDS's `core` |
| `patches/melonDS/*.patch` | Hooks in melonDS 1.1 for the looks, applied at configure time |
| `src/gamestate.*` | `ReadGameState(RamView)`: the game's own screen manager read from main RAM (EU addresses, `kEU`) → `GameState` (`GScreen`, panel tab, selections, `settled`); `ToScreenState` (CONSOLE's screen: Panel + tab, the rest by name, tab -1, unmapped or Options > Quit's popup → Unknown), `ConsoleScreenOf` (that, an Unknown without the HUD → None; the pixels without RAM), `PixelsAgree`; `App::game` every frame, `--print-game` logs changes (`frame N: game X`) and a pixel tracker that disagrees over 90 settled frames (`frame N: pixels disagree: X`); the map is in `docs/research/ram-map.md` |
| `tests/gamestate_test.cpp`, `tests/gamestate_check.sh` | The reader on a synthetic RAM, `ToScreenState`, `ConsoleScreenOf` (`ninja -C build gamestate_test`); each save state's game state and the panel's tabs from `s6` (ROM, `shots/`, the complete save, as `screens_check.sh`) |
| `src/screens.*` | Which bottom screen the game shows: `Recognise(top, bottom)` → `ScreenState` (`Screen`, panel `Tab`), `ScreenTracker` (hysteresis; `App::screens`, `--print-screen`; CONSOLE uses it only when RAM does not read), the `Screen` values (FrontEnd, CreateASim, SaveSlots, ItemHand come only from RAM), and the pixel rules AUTO shares (`HudVisible`, `SocialVisible`, `WarmMenuVisible`, `VacuumVisible`, `CardsVisible`) |
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

- melonDS framebuffers are `0xAARRGGBB`, 256 × 192 each, `GPU.Framebuffer[FrontBuffer][0]` =
  top, `[1]` = bottom.
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
  brightness of each screen); `--print-screen` prints the recognised bottom screen on change.
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
  232 on rows 121/171, until moved); Save opens the slot screen and Quit a confirmation, both
  read Unknown in pixels; CONSOLE (from RAM) sees SaveSlots and, for Quit, a popup over the
  Options tab that it takes as Unknown: both give A to the stick cursor; the button by the title (193,17) shows no effect. Pockets: the bin (67,167)
  throws the selected slot's item away; the hand uses the selected item (the Super Drencher
  starts its game; a key item that cannot be used here gets a red ring). Catalogue: "Buy" and
  "Sell" are only the mode, set by selecting a shop item or a bought one; the simoleon button
  (147,128) buys or sells; buying the same item again fills the six owned places. Cards: hand
  cards lift on a tap only after the draw; the play arrow (shown after the draw) takes a drag,
  not a tap; X asks "Are you sure you want to quit?" (tick 93,92, cross 163,92) and still reads
  Cards. Relationships: the up/down arrows select the previous/next Sim. Placed from shots only
  (a tap shows nothing in every state we have): Pockets slots 2-6 (empty), the Goals lines (one
  goal at a time seen; a goal takes one to three 16-pixel lines), the Interact strip (one target
  near), Give Item's slots and bow (empty pockets), the cards' discard pile and tenth place
  (empty), Build Rooms' rows and arrows (no room to build), the Options button by the title.
- CONSOLE (`console.cpp`): the world is drawn 720 high, centred (the DS's 4:3: 960 wide, dark
  sides) or, wider than 16:9, cropped at its sides (a conversation's world without its status
  bar: 176 of 192 rows); stretched only when within 8 pixels of the canvas's width (under 1 %:
  the widescreen world, 341 x 192, is 1279 wide at 720 high). The small world of activities is 384 wide, its height
  from its shape (288 at 4:3, 216 widescreen). The dialogue frame is 256 high, not the
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
  side as SIDE does: without a HUD (FrontEnd, CreateASim, None: title, main menu, Create-a-Sim,
  black loads) the front end's view, the top screen fitted at its own shape in `kFrontTopBox`
  (24,72, 568 × 480: 568 × 426 at 4:3, 568 × 320 widescreen, centred; touchable as the camera,
  as SIDE's was) and the touch screen 2.5x at `kFrontBottomTo` (616,72, 640 × 480, the only
  stylus piece; 3x would not fit beside the top), both framed on the dark canvas, and a box at
  y 600 with `kFrontHint` at 3x ("A / B / D-PAD: GAME BUTTONS - STICK: CURSOR": the font has no
  apostrophe and no middle dot). The stick cursor is drawn where the piece showing its point draws it
  (`CanvasFromTouch`), hidden where no piece shows it.
- The navigator (`navigator.cpp`, CONSOLE only; `Input::Steer`, before the mouse, and the touch
  script last as ever). Its screen is the game's (`App::ConsoleScreen()`, from RAM; the pixel
  tracker only when RAM does not read). RAM leads the pixels by up to about 60 frames (6 to 24
  seen from `s7` and `s6`) and the bottom slot reads not settled (`P+0xE42` != 3) while it
  switches; a panel tab of -1 (between screens, `Panel/?`) is Unknown, and an Unknown without the
  HUD (the black loads: CAS → town, the title → CAS, quitting the cards; `game Unknown(ov=-1,vt=0x00000000)`)
  is None (`ConsoleScreenOf`: the front end's view, the DS's buttons). Options > Quit is a popup over the Options
  tab in RAM: CONSOLE takes it as Unknown (its yes / no are not `kPopupClose`). The gate: on a focus
  screen or a popup, a press that would tap (A; B / MENU where they tap a close; LB / RB on the
  panel) while not settled is held, the latest one winning, and replayed on the first settled
  frame, or dropped after 90 frames or when the screen changes; focus moves and the DS's own
  buttons never wait; activities are not gated. The 90-frame wait after a closing tap still
  applies to a replayed press. FrontEnd, CreateASim: every button is the DS's (no table yet);
  SaveSlots, ItemHand: as Unknown (the stick cursor). Panel with the menu closed, Dialogue, None: the buttons are the
  DS's (LB / RB the camera's L / R; A advances a conversation itself). MENU (the PANEL action:
  E, the pad's Back) opens the panel's menu only; Esc / Guide / Start + Back stay the pause
  menu; the pad's TOUCH MENU acts on Back's release, and not at all if Start was held meanwhile
  (`ReleasePulse`: Start + Back in either order is the pause menu); the key E acts on its press.
  Focus screens (the panel's menu, Phone, Gift, Catalogue, Build): the
  D-pad or left stick moves the focus (`Neighbour`; held: again after 18 frames, then every 6),
  A taps, on the panel LB / RB tap the previous / next tab icon (in a ring) and B / MENU close
  the menu (no tap); on the Phone B / MENU tap `kPhoneClose` and close the menu; elsewhere B taps
  the screen's `close` target if it has one. Popup: A / B tap `kPopupClose`, Start stays the
  DS's (Game Paused). Unknown: the DS's buttons but A, which presses the stick cursor when one
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
