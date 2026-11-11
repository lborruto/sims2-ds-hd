# Contributing to sims2-ds-wide

Thanks for helping. sims2-ds-wide plays The Sims 2 (DS) on a computer like a console game: one
window, the melonDS core, and the touch screen's screens redrawn in HD from what the game shows
in its memory. The bar is: nothing of the game ever in the repository, CONSOLE as the default,
and every change checked on real pictures of the game.

It is meant to stay small: one game, one window, the melonDS core as it is. A change has to fit
that.

Contents: [Set up](#set-up) · [Checks](#checks) · [Principles](#principles) ·
[Studying the screens](#studying-the-screens) · [How it works](#how-it-works) ·
[Where the docs are](#where-the-docs-are) · [Commits and pull requests](#commits-and-pull-requests) ·
[Releasing](#releasing).

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

Before opening a pull request, build with no warnings from `src/` and run the unit tests (no ROM
needed; CI runs them too):

    ninja -C build sims2wide
    for t in layout touchscript hotspots screens navigator controls gamestate romdata uikit shell; do
        ninja -C build ${t}_test && ./build/${t}_test || break
    done
    ninja -C build pads_test && SDL_VIDEODRIVER=dummy ./build/pads_test

| Test | What it checks |
|---|---|
| `layout_test` | every touch pixel maps back to itself in every layout and every CONSOLE composition; every HD screen's view (items reachable, inside their panel, ids distinct, layers ascending, the mouse onto the game's targets) on made-up states, at several UI sizes |
| `navigator_test` | CONSOLE's navigator on synthetic screens: menus, popups, the phone, the minigames, the gate |
| `gamestate_test` | the RAM reader on synthetic and random RAM: every part, garbage per part |
| `romdata_test` | the ROM's strings, archive, compressions and sprites on a synthetic ROM: every broken table refused (also once under UBSan and libc++'s hardened mode after a change there, not ASan) |
| `uikit_test` | the UI kit: wrapping, the raster's determinism and hashes, layers and the layer cache, zoom, the Smooth (xBRZ) pictures |
| `shell_test` | the pause menu and launch panel on made-up models: pages, questions, settings rows, the mouse, five languages, the files they write |
| `controls_test`, `pads_test` | bindings and settings files; controllers on SDL's virtual joysticks (hot-plug, two pads, Steam's virtual pad, the mappings' order) |
| `touchscript_test`, `screens_test`, `hotspots_test` | synthesised touches, the HUD's pixel rule, the touch targets' tables |

The scripts need your ROM (European, in git-ignored `roms/`), save states in `shots/`, a save
that has played far ("the complete save") and Pillow. Each takes them from the environment:

    ROM=roms/<rom>.nds SHOTS=shots MORE_SHOTS=shots COMPLETE_SAVE=<a .sav> PYTHON=python3 MAX_SIMS=4 \
        bash tests/console_check.sh   # CONSOLE's pictures and pad scripts (the long one)
    bash tests/gamestate_check.sh     # each save state's game state read from RAM
    bash tests/screens_check.sh       # the bottom screen recognised by its pixels
    bash tests/hotspots_check.sh      # each touch target tapped in the game does what it says
    bash tests/romdata_check.sh       # the game's strings, resources and sprites (RESEARCH= optional)
    bash tests/interp_check.sh        # 60 fps: the walking Sim never torn in an in-between frame

`MAX_SIMS` caps how many emulators a script runs at once (4 by default; set it lower on a slow or
hot machine). The save states the scripts use are your own (never in git): the list, and how each
was made, is in [docs/dev-notes.md](docs/dev-notes.md#build-states-and-checks). A script skips
what it lacks with a message.

**The identity check.** A change to the core's patches, the looks or AUTO keeps the looks-off
output byte for byte the same: from save states of Create-a-Sim, a conversation and free roam,
`--headless --layout auto --frames 200 --shot-every 50` with and without `--compose` (24 shots),
shasums compared with `main`'s build. The HD pictures are compared byte for byte by
`console_check.sh`: a change to the rasteriser is a deliberate re-baseline, never a side effect.

**Performance.** `ui_bench` measures the HD menus through the window's layer cache (not a check,
never in a normal run): `ninja -C build ui_bench && ./build/ui_bench 3` (3 = a 4K screen) prints
milliseconds per tab change and focus move, and a digest of every picture (`--images-cpu`) to
compare two builds byte for byte. Measure more than once, on the same machine.

A change to the menu, the mouse or the keys is also checked in the window with a script of window
events (`press`, `click`, `winshot`, below) and `--frames`, which closes the window by itself.

CI runs on every pull request and push to `main`: actionlint and shellcheck on the workflows,
gitleaks over the whole history, CodeQL on `src/` (once the repository is public), and Release
builds the way the release makes them: Linux (Ubuntu 24.04), the Mac app for both kinds of Mac
(built on an Intel macOS 15 runner; the Intel half is run, the other checked present) and Windows
(MSYS2, checked to need no DLL beyond Windows' own, `pads_test` run on Windows' own SDL build),
each checked with `--version`, and the unit tests. It has no ROM, so it cannot run the game. The
one check to require is "CI result"; a docs-only pull request skips the build and still passes it.

## Principles

These are permanent. A change that needs one of them relaxed needs a conversation first, in an
issue.

- **Nothing of the game in the repository.** No ROM (`.nds`), save (`.sav`), save state
  (`.st`), dump of its files, code, graphics, sound or text, in code, tests, issues or commit
  messages. `.gitignore` covers `roms/`, `shots/`, `*.sav` and `*.st`; never force-add what it
  ignores. Screenshots in `docs/screenshots` are fine. Nothing that tells people where to get a
  ROM. The game's words and pictures are read from the player's ROM at run time.
- **CONSOLE is the default.** The other layouts are there by choice.
- **Read the game, never change it.** No decompilation: the port reads the game's RAM (and only
  reads it) and the ROM's own data, and every address comes with the save states that show it
  holds ([docs/research/ram-map.md](docs/research/ram-map.md)). Without them (another version of
  the game) it falls back to the pixel rules.
- **melonDS as it is.** The core is the upstream 1.1 release. The only changes are the small
  hooks in `patches/melonDS/` for the looks, applied at configure time; with the looks off they
  change nothing (the identity check). Another change to it needs an issue first; most belong
  upstream.
- **No new dependency without discussion.** Today: melonDS, SDL2, stb_image_write,
  stb_truetype, tinyfiledialogs, xBRZ (`src/thirdparty/xbrz`, 1.8 from its SourceForge release,
  unchanged: GPLv3 like the program, which is why it may be used; its licence beside it as
  `LICENSE.txt`; the release archives' `LICENSE` covers it), the Nunito font, SDL_GameControllerDB
  (and Pillow for the scripts and `tools/sheet.py`).
- **The port's own words in five languages.** Where the game's string table has no word, the
  word goes in `src/port/ownwords.*` in English, French, German, Italian and Spanish, short.
- **Measure performance claims**, more than once, on the same save state, with `--headless` (or
  `ui_bench`).
- **Playable from a controller alone, and from the keyboard and mouse alone.**

## Studying the screens

The headless mode runs the game without a window, about four times faster than real time:

| Option | What it does |
|---|---|
| `--headless --frames N` | run N frames, no window |
| `--input FILE` | an input script, one event per line: `frame frames-held key[+key]` or `frame frames-held touch x y` (`scripts/` has examples); the key `blow` blows into the microphone |
| (in a script, CONSOLE) | `pad ACTION` (`a`, `b`, `x`, `y`, `l`/`lb`, `r`/`rb`, `start`, `select`, `menu`, `up`, `down`, `left`, `right`, `lt`; the sticks `lx-` / `lx+` / `ly-` / `ly+`, `rx…`, `ry…`, `rsleft` / `rsright`): the action held as on a controller, through the navigator; `layout NAME` (`auto`, `focus`, `side`, `stacked`, `console`) switches the layout at that frame. Headless and window alike; `--print-screen` also prints the navigator's menu and focus |
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
| `--ui-scale N`, `--game-pictures smooth\|pixel` | the HD interface's size (60 to 150 %) and its game pictures' look; headless defaults to 100 % and Pixel |
| `--game-language N` | the game's own language (the DS firmware's), 1 English to 5 Spanish; headless keeps English otherwise |
| `--fast-boot` | the boot's splashes at full speed, as the window does |
| `--print-console` | CONSOLE's composition (which branch, a held picture) when it changes, and the counts at the end: how many frames showed milestone 1's picture |
| `--print-words` | The words CONSOLE's picture shows (its texts and chips' labels, in order, ` \| ` between) when they change: what a screen says, for the checks |
| `--shell PAGE [--shell-at N]`, `--launch`, `--data-dir DIR`, `--print-shell` | the pause menu opened on a page, the launch panel first, where the save states and recent games go (none headless without it), and what they do |
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

The architecture map (one line per module), the key traps and the recipe for a new HD screen are
in [CLAUDE.md](CLAUDE.md); every module's full notes, the traps found so far and every known
limit are in [docs/dev-notes.md](docs/dev-notes.md). In short:

- `src/main.cpp` runs the frame loop: input, one `RunFrame()`, the layout's picture, sound. The
  window is SDL2 (`video.*`); the controllers are `pads.*`; settings `settings.*`.
- `gamestate.*` reads what the game shows from its RAM every frame (`GameState`); `romdata.*`
  reads the game's strings and sprites from the player's ROM.
- CONSOLE (`console.*`) builds one `Composition` per screen: pieces of the DS screens, overlays
  and HD UI items (`ui/kit.*`, Nunito in `ui/font.*`). Each HD screen is a `port/` module that
  turns its part of `GameState` into a `MenuView` (what the D-pad and A do, as the game's own
  taps) and kit items. The navigator (`navigator.*`) drives the view from the controller; the
  mouse goes back through the same composition.
- The pause menu and the launch panel are `shell.*` (pure, tested without the emulator), carried
  out by `shellhost.*`; the save states and files are `slots.*`.
- The looks (sharper and widescreen 3D, 60 fps) are ported from Pokémon FullDex
  (`melonds_ext/`, `patches/melonDS/`, `looks.*`).

`src/platform.cpp` is melonDS's Platform interface, adapted from Pokémon FullDex, as are
`controls.*`, `ui.*` (the 5 × 7 glyphs) and `audiostretch.h` (each says so in its header).

## Where the docs are

| File | For |
|---|---|
| [README.md](README.md) | players: a two-minute read |
| [docs/manual.md](docs/manual.md) | players: every control, setting and known rough edge |
| [CLAUDE.md](CLAUDE.md) | contributors (and Claude): rules, build, the architecture map, the key traps, adding a screen |
| [docs/dev-notes.md](docs/dev-notes.md) | every module's notes, the save states, the checks' cases, all traps and known limits |
| [docs/research/ram-map.md](docs/research/ram-map.md) | the game's RAM map (European ROM) |
| [CHANGELOG.md](CHANGELOG.md), [SECURITY.md](SECURITY.md) | what changed; how to report a vulnerability |

The pictures in `docs/screenshots` are 1280 × 720 webp (quality 82), made headless from save
states: `--headless --layout console --compose --wide --res3d 2 --interp --game-pictures smooth
--ui-scale 100`, the PNG saved with Pillow. Only the pictures README.md and docs/manual.md show
are kept.

## Commits and pull requests

- Commit with your own identity. One change per commit, with a subject line that says what
  changed, in plain words; say why in the body when it is not obvious.
- One topic per pull request. Fill in the template's checklist.
- A change players will notice adds a line under `## [Unreleased]` in `CHANGELOG.md`, and
  updates README.md, docs/manual.md, CLAUDE.md or docs/dev-notes.md when it changes what they say.

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
