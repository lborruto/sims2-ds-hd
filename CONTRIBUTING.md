# Contributing to sims2-ds-wide

Thanks for helping. sims2-ds-wide shows The Sims 2 (DS) in one window on the melonDS core. The
bar is: nothing of the game ever in the repository, the single window as the default, and every
layout change checked on real pictures of the game.

It is meant to stay small: one game, one window, the melonDS core as it is. A change has to fit
that.

Contents: [Set up](#set-up) · [Checks](#checks) · [Principles](#principles) ·
[Studying the screens](#studying-the-screens) · [How it works](#how-it-works) ·
[Commits and pull requests](#commits-and-pull-requests) · [Releasing](#releasing).

## Set up

You need CMake 3.21 or later (4.x works), Ninja, a C++20 compiler and git. SDL2 2.30.9 is downloaded
and built by CMake (its archive's hash is checked); melonDS 1.1 is a git submodule.

On Linux, SDL needs its development packages. What CI installs on Ubuntu 24.04:

    sudo apt-get install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
      libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libasound2-dev libpulse-dev \
      libudev-dev

(libudev-dev: SDL's HIDAPI controller drivers and hot-plug through udev; configuring warns without
it.) Configuring prints `SDL controllers:` with the joystick drivers SDL compiled in, and stops
when a platform's main ones are missing (Windows: XInput, RawInput, DirectInput, HIDAPI; a Mac:
IOKit, GameController.framework, HIDAPI; Linux: evdev, HIDAPI).

On a Mac, Xcode's command line tools are enough (plus CMake and Ninja, from Homebrew or
`pip install cmake ninja`; CMake 3.21 to 4.x work).

    git clone --recursive https://github.com/lborruto/sims2-ds-wide.git && cd sims2-ds-wide
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    ninja -C build sims2wide

The first build compiles melonDS and SDL (a few minutes); after that, changing `src/` rebuilds
in seconds. Configuring also applies `patches/melonDS/*.patch` to the submodule's files (see
[How it works](#how-it-works)): `git status` does not show that (`ignore = dirty` in
`.gitmodules`), and `git -C melonDS checkout -- .` gives the pristine files back until the next
configure. `build/sims2wide` is a plain program for the command line on every system.

- **The Mac app**: add `-DSIMS2WIDE_MAC_APP=ON` (and `"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` for both kinds of Mac, as the release does) to get
  `build/Sims 2 DS Wide.app`, with the icon made from `packaging/icon.png`.
- **Windows**: MinGW (UCRT64) from [MSYS2](https://www.msys2.org/), the toolchain melonDS itself
  uses there: `pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}`, then the same commands in the
  UCRT64 shell. The `.exe` is linked statically (no DLL beside it) and opens no console window;
  started from a terminal, it writes to that one. MSVC is not supported.

## Checks

Before opening a pull request:

    ninja -C build sims2wide                 # no warnings from src/
    ./build/sims2wide --help                 # prints the usage (with no ROM it opens the file picker)
    ./build/sims2wide <rom> --headless --frames 1800 --shot-every 120 --shot-dir shots/boot
    ninja -C build layout_test && ./build/layout_test   # every touch pixel maps back to itself, in every layout
    bash tests/screens_check.sh              # the bottom screen recognised in every save state (needs the ROM and shots/)
    ninja -C build touchscript_test && ./build/touchscript_test   # the synthesised touches
    ninja -C build screens_test && ./build/screens_test   # the HUD rule on synthetic top screens
    ninja -C build hotspots_test && ./build/hotspots_test   # the touch targets' tables and focus moves
    bash tests/hotspots_check.sh             # each touch target tapped in the game does what it says (ROM, shots/, the complete save)
    ninja -C build navigator_test && ./build/navigator_test   # CONSOLE's navigator on synthetic screens (the HD menu, popups and the phone included)
    ninja -C build controls_test && ./build/controls_test     # one action per key and input, settings files loaded through the swap (SDL's dummy video driver)
    ninja -C build pads_test && SDL_VIDEODRIVER=dummy ./build/pads_test   # the controllers on SDL's virtual joysticks: hot-plug, two pads, triggers, dead zones, a pad missing buttons, Steam's virtual pad, the chips' look per pad type, the mappings' order
    bash tests/console_check.sh              # CONSOLE's pictures hold the DS pixels where they belong (and the hints), the navigator's and the activities' pad scripts (the shop's buys and sells, Build Rooms' buy, cancel and double press, the Registry, check-in, room info, the music and stat pages, the front end: boot, title, main menu, Settings, Load-a-Sim and Create-a-Sim, including the Start and A pressed during the splashes that take no touch), the HD conversation (its page follows the game's, A turns two pages, every language's conversations fit the box, every language's HD menu words fit theirs, no character missing from the font, no game text: milestone 1's box) (ROM, shots/, the complete save, Pillow)
    ninja -C build gamestate_test && ./build/gamestate_test   # the RAM reader and its screen names on a synthetic RAM (the menus' lists and the hotel's and shops' screens too, fuzzed: the tabs, the phone, Give Gift, notes, the catalogue, Build Rooms, the Registry, room info, music, the stat panel never crash or run past their caps)
    bash tests/gamestate_check.sh            # each save state's game state read from RAM (the front end's included), the minigames' parts (`--print-minigame`), the recorder's captures (ROM, shots/, the complete save)
    ninja -C build romdata_test && ./build/romdata_test       # the ROM's archive and string tables, the compressions, the sprites and the icon cache on a synthetic ROM: decoded, and every broken table refused (also once under UBSan and libc++'s hardened mode, not ASan)
    ninja -C build uikit_test && ./build/uikit_test           # the UI kit: text wrapping, the raster's determinism and hash, panels, text and chips (no ROM, no window)
    bash tests/romdata_check.sh              # the game's strings read from the ROM in its five languages, its compressed resources and sprites (ROM; RESEARCH= the decoded lines to compare with, else that part is skipped)

and look at the shots (`tools/sheet.py`). A layout change is checked with `--compose` from save
states of the situations it touches (below). A change to the menu, the mouse or the keys is
checked in the window with a script of window events (`press`, `click`, `winshot`: see the
table below) and `--frames`, which closes the window by itself.

A change to the core's patches or to the looks keeps the looks-off output byte for byte the
same: from save states of Create-a-Sim, a conversation and free roam, `--layout auto --frames 200
--shot-every 50` with and without `--compose` (24 shots; headless runs start with the looks off
and default to CONSOLE, so name the layout), shasums compared before and after the change.

CI runs on every pull request and push to `main`: actionlint and shellcheck on the workflows,
gitleaks over the whole history, CodeQL on `src/` (once the repository is public: code
scanning needs it on the free plan), and Release builds the way the release makes them: Linux
(Ubuntu 24.04), the Mac app for both kinds of Mac (built on an Intel macOS 15 runner, as GitHub's Apple silicon
ones are often out of capacity; the Intel half is run, the other checked present) and Windows (MSYS2,
checked to need no DLL beyond Windows' own, and `pads_test` run on Windows' own SDL build), each checked with `--version`. It has no ROM, so it
cannot run the game. The one check to require
is "CI result"; a docs-only pull request skips the build and still passes it.

## Principles

These are permanent. A change that needs one of them relaxed needs a conversation first, in an
issue.

- **Nothing of the game in the repository.** No ROM (`.nds`), save (`.sav`), save state
  (`.st`), dump of its files, code, graphics or sound, in code, tests, issues or commit
  messages. `.gitignore` covers `roms/`, `shots/`, `*.sav` and `*.st`; never force-add what it
  ignores. Screenshots in `docs/screenshots` are fine. Nothing that tells people where to get
  a ROM.
- **The single window is the default.** CONSOLE is the default layout; the others are there by
  choice.
- **Learn from the picture, not from the game's code.** Situations are recognised from the
  screens' pixels (like the status bar). A rule comes with the shots that show it holds, in
  every situation surveyed so far.
- **melonDS as it is.** The core is the upstream 1.1 release. The only changes are the small
  hooks in `patches/melonDS/` for the looks, applied at configure time; with the looks off they
  change nothing (the check below). Another change to it needs an issue first; most belong
  upstream.
- **No new dependency without discussion.** Today: melonDS, SDL2, stb_image_write,
  stb_truetype, tinyfiledialogs, xBRZ (`src/thirdparty/xbrz`, 1.8 from its SourceForge release,
  unchanged: GPLv3 like the program, its licence beside it as `LICENSE.txt`; the release archives'
  `LICENSE` covers it), the Nunito font (and Pillow for `tools/sheet.py` only).
- **Measure performance claims**, more than once, on the same save state, with `--headless`.
- **Playable from the keyboard and mouse alone.**

## Studying the screens

The headless mode runs the game without a window, about four times faster than real time:

| Option | What it does |
|---|---|
| `--headless --frames N` | run N frames, no window |
| `--input FILE` | an input script, one event per line: `frame frames-held key[+key]` or `frame frames-held touch x y` (`scripts/` has examples); the key `blow` blows into the microphone |
| (in a script, CONSOLE) | `pad ACTION` (`a`, `b`, `x`, `y`, `l`/`lb`, `r`/`rb`, `start`, `select`, `menu`, `up`, `down`, `left`, `right`): the action held as on a controller, through the navigator; `layout NAME` (`auto`, `focus`, `side`, `stacked`, `console`) switches the layout at that frame. Headless and window alike; `--print-screen` also prints the navigator's menu and focus |
| (in a script, window only) | `press KEY` (SDL's key names: `Escape`, `Return`, `F11`, `X`), `move x y` and `click x y` (window points), `wheel n`, `winshot` (the window's picture, `w<frame>.png`): real SDL events for the frontend; their frame counts the loop's turns, paused ones too |
| `--shot-every N`, `--shot-at a,b,c`, `--shot-dir DIR` | both screens as PNG (256 × 384) |
| `--layout console\|auto\|focus\|side\|stacked` | the layout (default CONSOLE; the checks that need another name it) |
| `--compose` | the window's picture instead (the layout's) |
| `--res3d N`, `--wide`, `--interp` | the looks; shots then also write the top picture at its size (`fNNNNNN-top.png`) |
| `--state-load FILE`, `--state-save FILE` | start from a save state, save one at the end |
| `--save FILE` | the game's save (use a scratch one for tests) |
| `--clock "YYYY-MM-DD HH:MM"` | fix the DS clock's start, so that two runs show the same time |
| `--mic off\|blow\|real` | what the DS hears (the setting otherwise) |
| `--fast` | start with fast forward held; headless, it exercises the clock's hold |
| `--print-screen` | print the recognised bottom screen (`screens.h`) when it changes: `frame N: screen Panel/Pockets` |
| `--print-game` | print what the game says is up, read from its RAM (European ROM only), at frame 0 and whenever it changes: `frame N: game X` |
| `--record-dir DIR` | where unmapped screens are recorded (a window run records by default; headless only with this) |
| `--strings-count`, `--dump-string ID`, `--language N` | the game's own strings read from the ROM (`romdata.h`), printed; they stop the run; `--language` also picks the HD interface's words |
| `--print-town` | print the town's state read from RAM when it changes (pockets, options, the popup, the save slots, and the HD menu's lists: Interact's actions, Goals, Relationships, the phone, Give Gift, a note) |
| `--print-front` | print the front end read from RAM when it changes (`frame N: front Title\|MainMenu\|Settings\|Load\|Wireless\|CreateASim\|KeepSim\|Boot\|Video`, `title phase=..`, `boot mode=..`, the Settings page and volumes, the Load rows, the Create-a-Sim page, name and picks); the front end's pad scripts in `scripts/console/front-*.txt`, `boot-keys.txt`, `settings-*.txt`, `load-slot.txt` and `cas-*.txt` drive it from `tests/console_check.sh` |
| `--print-hotel` | print the hotel block read from RAM (built, unlocked and building rooms, the score, the five guest slots, the money, the stat totals, the songs, the place) and the hotel's and shops' screens when they change: `checkin` (the Registry, the guest's check-in), `roominfo`, `music`, `build`, `placement`, `catalogue`, `stat` |
| `--print-hud` | print the status bar's values and the conversation read from RAM when they change (`frame N: hud money=.. time=hh:mm sanity=cur/max tier=.. name=..`, `frame N: dialogue ..`) |
| `--measure-string ID\|all` | lay a string out in the HD conversation's box (lines, size, fits); `all` every page of the game's conversations (from a `--state-load`); exit 2 for an overflow, a placeholder left in the text or a character the font lacks; stops the run |
| `--measure-menu` | every word the HD menu (Interact, Pockets, Give Gift, Goals, Relationships, Options, Save slots, popups, phone, notes, the catalogue, Build Rooms, the Registry, room info, the music and stat panels) can show, in `--language`'s words, fitted to its box: how many shrink, how many are cut; exit 2 for a cut word (a neighbour's name in the strip may end in an ellipsis) or a character Nunito lacks (it would be drawn `?`); stops the run |
| `--dump-resource ID FILE` | write resource ID of the game's archive to FILE, its compression undone (it says when the header was a stored one, or the resource not compressed); stops the run (`tests/romdata_check.sh`) |
| `--dump-sprite ID FRAME FILE` | write frame FRAME of the sprite whose descriptor is resource ID (tiles ID - 1, palette ID + 1) as a PNG; stops the run |
| `--console-menu`, `--console-focus N` | (checks) pin CONSOLE's menu open and its focus on item N of the HD view, for a picture; `-1` none |
| `--dialogue-string ID` | (checks) the HD conversation shows string ID on every page (the longest strings' pictures) |
| `--game-unknown-after N` | (checks) fake a new unmapped screen every 200 frames from frame N, to drive the recorder |
| `--print-minigame` | print the minigame the bottom screen shows (the autopsy, detector, MooGoo, lottery, Drencher, crime game, mixer, save page, painting, cards, vacuum), read from RAM, when it changes |
| `--no-romdata` | do not read the game's text: CONSOLE as in milestone 1 (the old status bar and conversation) |
| `--compose-scale N` | `--compose` pictures N times the canvas (1 to 4), the UI drawn at that size |
| `--ui-demo` | the UI kit's demonstration drawn over the picture |
| `SIMS2WIDE_PROBE=N` (environment) | print AUTO's signals every N frames: HUD, social, warm menu, each screen's motion and brightness |

Every `*_check.sh` runs at most `MAX_SIMS` emulators at once (4 by default: set it lower on a
slow or hot machine). Their pictures and runs of the minigames use the research's `G-*` states in
`shots/` and copies of the complete save.

A headless run ends with a line that says what the DS clock reads, how often the hold wrote
it, and how many microphone samples the game read (0 unless it listened).

The way to explore: save a state at each situation, then work from it. `scripts/title.txt`
touches the title screen, `play.txt` presses Play, `cas.txt` opens Create-a-Sim, `ok.txt` and
`keep.txt` confirm the Sim, `dlg.txt` taps through the first conversation and `tabs.txt` opens
each tab of the panel in town. `tools/sheet.py OUT.png shots/…/*.png` puts many shots on one
contact sheet (needs Pillow).

Keep your ROM in `roms/` and your shots and states in `shots/`: both are ignored by git.

## How it works

`src/main.cpp` reads the command line, starts the DS and runs the frame loop: input, one
`RunFrame()`, AUTO's decision, shots, sound, picture (or, with the menu open, just the menu
over the last picture). Each part has its own file pair: `app.h` (the shared state: the DS,
the layout chosen and shown, fast forward), `settings.*` (every option, the file),
`controls.*` (the key and button bindings), `input.*` (keyboard, mouse, controller),
`video.*` (the window), `audio.*` (with `audiostretch.h`), `layout.*`, `screens.*` (which
bottom screen the game shows), `scene.*` (AUTO's detection), `shell*.*` and `slots.*` (the pause menu, the launch panel, the save states), `ui.*` (milestone 1's 5 × 7 glyphs), `ui/` (the HD UI kit and its font), `port/` (the HD status bar and conversations), `backup.*`, `script.*`,
`shots.*`, `state.*`, `picker.*`.

- **Input script** (`script.*`): parses `--input` files; the window events in it are pushed as
  SDL events by `Input::PushScript`, so the frontend is checked through its real path.
- **Layouts**: `Place()` puts the top and bottom screens on a canvas, sizing the top one from
  `App::topAspect` (4:3 on the DS, wider with the widescreen world). FOCUS is 1280 × 720 with
  the world 720 high at the left and the touch screen at twice its size (512 × 384) over the
  world's right edge, centred and a little see-through (`Placement::alpha`): the DS's 4:3 world
  leaves only a 320-wide column beside it, too narrow for a readable panel, and a wider world
  leaves none; centred, the panel leaves the status bar at the world's bottom clear. With the
  touch screen big (Tab), the world goes small into the column and nothing covers the touch
  screen. The window scales the canvas; mouse clicks go back through the same `Place()` to a
  touch-screen position (`tests/layout_test.cpp` checks every pixel in every layout). A press on
  the touch screen keeps the stylus down until the button is released, held to the screen's
  edges (`TouchClamped()`), so a drag that overshoots does not drop what it carries.
- **Compositions and CONSOLE** (`compose.*`, `console.*`): what the window shows is a
  `Composition`, a list of pieces (a rectangle of one DS screen drawn at a rectangle of the
  canvas: the touch screen 2x, 2.5x or 3x) and overlays (rounded frames and boxes in the
  game's colours, the dimmed backdrop, the focus ring), drawn in order by the window and by
  `--compose` alike; the mouse goes back through the same pieces (`TouchFromComposition`: the
  topmost piece under the pointer decides; `touch=false` swallows clicks and the camera). SIDE,
  STACKED and FOCUS are two-piece compositions made from `Place()`. CONSOLE (`--layout console`)
  builds one per screen (`App::ConsoleScreen()`: the game's from RAM, else `screens.*`) on
  1280 × 720, and never shows the two screens bare side by side as SIDE does: in town (the panel)
  the world with a rail of the touch screen's tab icons and phone (2x) at the right; the menu,
  the phone, a popup and Give Item the whole touch screen 3x over the dimmed world (framed for
  the save slots, the item in hand and unknown screens, where the stick cursor works); a
  conversation the world without its status bar and the dialogue's name, text, portrait and
  arrow (2x) in a wide frame; activities the touch screen 3x at the left, the world small at the
  top right and the hints' box; the front end (boot, title, main menu, Settings, Load-a-Sim,
  Create-a-Sim) its own HD screens (`port/front.*`, `frontmenus.*`, `cas.*`), and without them (no
  HUD, no view) the top screen framed at the left at its own shape and the touch screen 2.5x
  framed at the right, with the buttons' hint below; a black load between screens (Loading) only the dark backdrop. The focus ring frames a hotspot's area (`hotspots.*`) where the
  piece showing it draws it. The navigator (below) opens the menu and moves the ring;
  `--console-menu` and `--console-focus N` pin them for the checks' pictures (`tests/console_check.sh`); `tests/layout_test.cpp` maps every pixel of every piece
  of every CONSOLE composition back to itself.
- **The HD interface** (`ui/`, `port/`, `romdata.*`): the kit (`ui/kit.*`) draws items (panels, text, chips, live crops of the DS pictures) in canvas pixels, with the embedded Nunito (`ui/font.*`, `assets/fonts/`, SIL OFL: the releases carry its licence); a composition's `ui` list is drawn after its pieces, by the window and `--compose` alike, deterministically (`--compose-scale N` draws it at N times). `port/statusbar.*` and `port/dialogue.*` build the status bar and the conversation from `GameState` (read from RAM) and the game's own words, read from your ROM at run time by `romdata.*` (in the language `--language` or the firmware names); without them (another version of the game, `--no-romdata`) CONSOLE keeps milestone 1's pictures. Nothing of the game's text is in the repository.
  The hotel and the shops follow the same path: `port/catalogue.*` (the shop), `build.*` (Build Rooms), `checkin.*` and `roominfo.*` (the Registry, a guest's check-in, a room's page), `music.*`, `statpanel.*` build their pictures and `MenuView`s from `GameState` (`hotel`, `catalogue`, `build`, `checkIn`, `roomInfo`, `music`, `stat`: each read from RAM on its own screen, the hotel block from any screen); the furniture editor (`placement`) is stylus-only and has no HD page, only the stick cursor and its hint. The hotel's buttons are spends, so their views are never replayed and the question pages start on No. Shared drawing helpers (`PanelText`, `Measurer`) live in `port/panelmenu.*`.
- **The navigator** (`navigator.*`): in CONSOLE the buttons go through it (`Input::Steer`, from
  the keyboard, the controllers and a script's `pad` events alike, as a `PadState` of actions).
  For each recognised screen it either passes them on as the DS's (town, conversations) or keeps
  a focus on one of the screen's touch targets (`hotspots.*`), moved by the D-pad with
  `Neighbour` and tapped with A through the touch script; the panel's menu opens with the TOUCH
  MENU action (E on its press, Back on its release unless Start joined it: Start + Back is the
  pause menu), LB / RB tap the tabs there, B closes. Presses are edges against the
  previous frame; taps are dropped while the touch script owns the stylus. Leaving CONSOLE
  cancels the touch script and closes the menu. `tests/navigator_test.cpp` drives it on
  synthetic screen states; `tests/console_check.sh` from save states with `pad` scripts.
- **Activities** (`activities.*`): the navigator hands the social reaction, Keelhaulin' Cards
  and the vacuum to `ActivityInput`, which keeps its own `ActivityState` (the previous
  `PadState`, the focus, the cards' lifted places and quit question) and turns buttons into the
  game's touches: taps, and drags through `TouchScript::Drag` (a card onto the play arrow or the
  discard pile); the vacuum's hand is the stick cursor (`Navigator::Hand`, moved by the right
  stick or by the D-pad with X or A held, so the keyboard needs no stick; A held presses it), its
  Y blows (`Navigator::Blow` into `App::micBlow`). `ActivityHints` gives the hints' box its rows
  (`AddHints`, `console.*`: text overlays in the 5 × 7 font, `Overlay::text`, lines split by hand).
  `PadState` is in `padstate.h` (both use it).
- **Settings** (`settings.*`): one `Settings` struct for every option, whoever uses it; loaded
  from `settings.ini` (plain `key=value`) in SDL's per-user folder when the window opens, never
  headless, saved on every change from the menu and when the program closes (with the window's
  size). Add a field with its default, its two lines in `Load` and `Save`, its row in `shell.cpp` (`Row`, `ChangeRow`, `RowLabel`, `RowValue`, its words in `port/ownwords.*`). The defaults are CONSOLE's: `layout=console`, widescreen on,
  3D resolution 2, 60 fps on; `--layout` overrides the layout for one run. Headless never loads
  settings and starts with the looks off (main.cpp), so its output does not depend on them.
- **Controls** (`controls.*`, `input.*`): each action (the 12 DS buttons, MENU, FAST FORWARD,
  SWAP SCREENS, SAVE STATE, LOAD STATE, BLOW, CURSOR PRESS, TOUCH MENU) has a key (an SDL keycode, so the
  key's label on any layout) and a controller input; giving an action a key another one had
  swaps them. Controllers (`pads.*`) are read through SDL_GameController, labels by SDL's convention (A is
  the button printed A; with `SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0` a Nintendo pad's A is its
  bottom button, printed B: `PrintedInput`). `Pads` opens every controller (at start and on
  `SDL_CONTROLLERDEVICEADDED`, closed on `…REMOVED`), and one drives: the last one whose button
  went down or whose stick went past half way (a drifting stick never takes over), else the last
  one connected; while Steam Input's virtual pad is open (a game Steam launched: Steam sets
  `SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD`, and normally hides the physical pads with
  `SDL_GAMECONTROLLER_IGNORE_DEVICES`), the physical ones are not read, so nothing acts twice.
  `Pads::Read` gives the driving pad's buttons, its triggers as buttons past `trigger_threshold`
  and its sticks zeroed inside the round `stick_dead_zone`, rescaled from its edge. Steam's
  virtual pad is told by its GUID's ids (`SDL_GetJoystickGUIDInfo`), not `SDL_JoystickGetVendor`,
  which gives the real pad's when Steam passes them. A pad connected shows a toast (`Video::Toast`)
  with its pause buttons; the stick cursor's hint has the pause chip (`ui::Button::Pause`) beside it. Mappings, lowest first: SDL's own,
  the bundled SDL_GameControllerDB (only for pads SDL maps not already: `AddUnmapped`) (`assets/controllers/gamecontrollerdb.txt`, mdqinc/SDL_GameControllerDB
  at ff333823, zlib, `LICENSE.txt` beside it; this platform's lines embedded at build time by
  `cmake/embed.cmake`), the player's `gamecontrollerdb.txt` in the settings folder, the file
  `SDL_GAMECONTROLLERCONFIG_FILE` names, and `SDL_GAMECONTROLLERCONFIG` over all (SDL gives it
  the user priority: nothing here overrides it). To update the database: download the file at a
  newer commit at least two weeks old, replace it, say the commit here and in CLAUDE.md. The Guide
  button is the system's (Steam, the Deck, Game Bar): no action has it by default; the pause
  menu is Start + Back. The HD chips (`ui::ChipLook`, set each frame by `Input::Poll` through
  `ChipLookFor`) print the driving pad's buttons for the actions they stand for (Xbox letters,
  PlayStation's drawn symbols, Nintendo's letters and +, L, ZL), or the keyboard's keys on keycaps
  when a key was pressed last (keys an input script pushes are marked and do not count); a chip
  of the Xbox look with its default input is the item it always was (same pixels, same hash).
  The right stick moves a cursor on the touch screen, pressed with
  CURSOR PRESS (LT). Over the world, the mouse wheel holds X or Y a few frames per notch (the
  game zooms in with X, out with Y) and a right-button drag holds L or R for a frame every
  three points moved (the game turns the camera continuously while they are held).
- **The HD shell** (`shell.*`, `shellhost.*`, `slots.*`): the pause menu (Start + Back or Esc) and
  the launch panel, on the UI kit in the port's own words (`port/ownwords.*`, five languages).
  `shell.*` is pure: a `Model` and one frame's presses in, a `Result` out (save a slot, load
  one, capture a binding, a setting changed, quit…) and kit items to draw on a 1280 × 720 canvas;
  `shellhost.*` carries the results out with the emulator, the settings file and the window;
  `slots.*` keeps the save states, their thumbnails, the suspend state and the recent ROMs in the
  settings folder. While it is open nothing is emulated. Headless, Start + Back held in a script
  (or `--shell PAGE`) opens it and the frames count on, so `pad` scripts drive it
  (`scripts/shell/`); `tests/shell_test.cpp` checks it without the emulator. The stick cursor,
  the fast forward mark and the toasts are kit items too (`video.cpp`); the 5 × 7 font (`ui.*`)
  is milestone 1's overlays' only.
- **Sound** (`audio.*`): the frames' samples go through `AudioStretch` (WSOLA, from FullDex),
  which stretches them at their own pitch when frames take longer than a DS frame, steered to
  keep about 100 ms queued; volume is applied after it; in fast forward the sound is muted or
  played at normal pitch with the rest skipped.
- **Save backups** (`backup.*`): `backups/<name>-<date>.sav` beside the save when the program
  starts and every half hour, unless identical to the newest copy; the last eight are kept.
- **AUTO**: `HudVisible()` checks row 190 of the top screen, the status bar's bottom border, one
  flat colour (`#415969`) from edge to edge in town and nowhere else: the world is big. The
  touch screen is big instead when the game plays there: a social reaction (`SocialVisible()`:
  the HUD plus row 0 flat `#597192`, the name and relationship box), the shop's catalogue or
  Build Rooms (`WarmMenuVisible()`: all four corners of the bottom screen warm and bright), the
  hotel's vacuum (`VacuumVisible()`: the HUD stays, and row 1 of the bottom screen is
  `#30A2D3` from x 4 to 44, the gauge's top edge, then `#304951` from x 60 to 100; clogged, the
  game tints the screen sepia and the same row has other fixed values), or,
  without the HUD, a dark top screen with a lit bottom one for two seconds in a row (unverified:
  meant for activities not reached yet; the hotel's dark rooms show it for about 30 frames at
  each door, while the world fades out before the panel). `AutoLayout` waits for ten frames of agreement
  before switching, so fades do not flicker; it sets `mainIsTop` only when its answer changes,
  so Tab still swaps by hand in between.
- **Microphone** (`mic.*`): melonDS asks `Platform::Mic_ReadInput` for mono 16-bit samples at
  47.6 kHz while the game listens (`Mic_Start`/`Mic_Stop`, which the game triggers by sampling
  the touch controller's AUX channel). Per the `mic` setting: the blow key feeds a full-scale
  wave of random 200-1000 Hz half-periods, the kind of signal melonDS's own frontend plays for
  its blow hotkey; "real" opens SDL's default capture device only while the game listens and
  resamples it; "off" is silence. main.cpp copies the setting and the key into `Host::` each
  frame; a script's `blow` counts as the key.
- **Clock** (`clock.*`): melonDS's RTC ticks on emulated time and is part of the save state,
  and a direct boot leaves it at 2000-01-01. The frontend sets it to the computer's local time
  after `Reset()` and again after every state load, and holds it during fast forward: before
  each frame it may go to one second before the host's (the host's second when a minute
  starts), so that the game never sees a minute the host has not reached and never one going
  back. Outside fast forward only forward corrections (a pause, sleep). The game calls a
  clock set back cheating. `--clock` fixes the start and turns the holding off.
- **Shots and save states**: PNG with stb_image_write; melonDS's `Savestate`. F5, F7 and F12
  write beside the ROM (`<rom>.quick.st`, `<rom>-shot.png`): an app started from the Finder or
  Explorer has no useful current folder.
- **Starting without a ROM**: `PickRom()` opens the system's file picker (tinyfiledialogs:
  AppleScript on a Mac, the common dialog on Windows, zenity or kdialog on Linux) in the folder
  of the ROM chosen last time, kept in `last-rom.txt` in SDL's per-user folder
  (`SDL_GetPrefPath`). On a Mac and on Linux tinyfd runs those tools through a shell with the
  strings in the command line: only short paths without shell-special or control characters go
  to it (`SafeForPicker()`), and errors show in SDL's message box (`Tell()`), never tinyfd's.
- **Packaging**: `packaging/` holds the icon (drawn by `tools/make_icon.py`), the Mac app's
  `Info.plist`, `make_icns.sh` and the Windows resource file.
- **Looks** (`looks.*`, `melonds_ext/`, `patches/melonDS/`): sharper 3D, widescreen and 60 fps
  in-between frames, ported from Pokémon FullDex's renderer (GPLv3). melonDS stays the 1.1
  tag; CMake applies the patches in `patches/melonDS/` to the submodule at configure time, in
  order, skipping one that `git apply --reverse --check` says is applied already. `0001` records
  every polygon the game submits (`GPU3DLooks::RawCurrent`) while a look is on, calls the hooks
  in `melonds_ext/GPU3D_Wide.cpp` when a frame is flushed and rendered, draws the in-between
  frame, and widens the box test with the view. `0002` records per pixel where the top screen
  showed its 3D layer and how (`GPU2DLooks::Pixel3D`, brightness per line). Their state lives
  in globals, not in melonDS's objects, so that the emulator's memory layout (and its output
  with the looks off) does not change. `GPU3D_Wide.cpp` replays the frame's polygons (x
  squeezed by 256/view width for widescreen), rasterises them on worker threads at the asked
  size (SIMD through the compiler's vector types: SSE2 on x86-64, NEON on arm64), and publishes
  the picture a frame later, in step with the DS's own 3D. `Looks::Frame` then builds the top
  picture: the big 3D where `Pixel3D` says the DS showed 3D (with the same brightness effects),
  the DS's 2D scaled up elsewhere, the wide 3D beside the DS's 256 columns.
  `App::TopImage(w, h)` returns it and `app.topAspect` its shape. In-between frames:
  only when the game draws its 3D every second frame (Sims 2 does, at 30 fps); each polygon
  is paired with the same one in the previous frame and moved halfway (a frame of latency).
- **Main loop**: one `RunFrame()` per frame, keys and touch from the script, the keyboard, the
  mouse and the controller, audio queued to SDL at 48 kHz. `frame` counts the game's frames
  (scripts, shots); `iter` every turn of the loop, paused ones too (`--frames`, window events).

`src/platform.cpp` is melonDS's Platform interface (files, threads, the game's save), adapted
from Pokémon FullDex, as are `controls.h`, `ui.*` and `audiostretch.h` (each says so in its
header); the microphone goes to `mic.*`, network and camera are stubs. `src/host.h` writes the
save and carries the mic mode and the blow key to platform.cpp.

## Commits and pull requests

- One change per commit, with a subject line that says what changed, in plain words. Say why in
  the body when it is not obvious.
- One topic per pull request. Fill in the template's checklist.
- A change players will notice adds a line under `## [Unreleased]` in `CHANGELOG.md`, and
  updates README.md when it changes what it says.

## Releasing

1. Set the version in `CMakeLists.txt` (`project(... VERSION X.Y.Z)`).
2. In `CHANGELOG.md`, rename `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD` and start a new empty
   `## [Unreleased]` above it.
3. Merge to `main`, then tag: `git tag vX.Y.Z && git push origin vX.Y.Z`.
4. `release.yml` checks the tagged commit is on `main`, runs CI on it, checks that
   `CMakeLists.txt` says `X.Y.Z`, builds `sims2-ds-wide-X.Y.Z-linux-x86_64.tar.gz`,
   `sims2-ds-wide-X.Y.Z-macos-universal.zip` (the app, ad-hoc signed, not notarised) and
   `sims2-ds-wide-X.Y.Z-windows-x86_64.zip`, attests their build provenance and creates the GitHub
   release from the changelog section. A tag without its section, or with another version in
   `CMakeLists.txt`, fails on purpose.
