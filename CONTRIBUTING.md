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
      libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libasound2-dev libpulse-dev

On a Mac, Xcode's command line tools are enough (plus CMake and Ninja, from Homebrew or
`pip install cmake ninja`; CMake 3.21 to 4.x work).

    git clone --recursive https://github.com/lborruto/sims2-ds-wide.git && cd sims2-ds-wide
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    ninja -C build sims2wide

The first build compiles melonDS and SDL (a few minutes); after that, changing `src/` rebuilds
in seconds. `build/sims2wide` is a plain program for the command line on every system.

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

and look at the shots (`tools/sheet.py`). A layout change is checked with `--compose` from save
states of the situations it touches (below). A change to the menu, the mouse or the keys is
checked in the window with a script of window events (`press`, `click`, `winshot`: see the
table below) and `--frames`, which closes the window by itself.

CI runs on every pull request and push to `main`: actionlint and shellcheck on the workflows,
gitleaks over the whole history, CodeQL on `src/` (once the repository is public: code
scanning needs it on the free plan), and Release builds the way the release makes them: Linux
(Ubuntu 24.04), the Mac app for both kinds of Mac (macOS 15, each half run) and Windows (MSYS2,
checked to need no DLL beyond Windows' own), each checked with `--version`. It has no ROM, so it
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
- **The single window is the default.** AUTO stays the default layout; the others are there by
  choice.
- **Learn from the picture, not from the game's code.** Situations are recognised from the
  screens' pixels (like the status bar). A rule comes with the shots that show it holds, in
  every situation surveyed so far.
- **melonDS as it is.** The core is the upstream 1.1 release, unmodified. A change to it needs
  an issue first; most belong upstream.
- **No new dependency without discussion.** Today: melonDS, SDL2, stb_image_write,
  tinyfiledialogs (and Pillow
  for `tools/sheet.py` only).
- **Measure performance claims**, more than once, on the same save state, with `--headless`.
- **Playable from the keyboard and mouse alone.**

## Studying the screens

The headless mode runs the game without a window, about four times faster than real time:

| Option | What it does |
|---|---|
| `--headless --frames N` | run N frames, no window |
| `--input FILE` | an input script, one event per line: `frame frames-held key[+key]` or `frame frames-held touch x y` (`scripts/` has examples) |
| (in a script, window only) | `press KEY` (SDL's key names: `Escape`, `Return`, `F11`, `X`), `move x y` and `click x y` (window points), `wheel n`, `winshot` (the window's picture, `w<frame>.png`): real SDL events for the frontend; their frame counts the loop's turns, paused ones too |
| `--shot-every N`, `--shot-at a,b,c`, `--shot-dir DIR` | both screens as PNG (256 × 384) |
| `--compose` | the window's picture instead (the layout AUTO would show) |
| `--state-load FILE`, `--state-save FILE` | start from a save state, save one at the end |
| `--save FILE` | the game's save (use a scratch one for tests) |

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
`video.*` (the window), `audio.*` (with `audiostretch.h`), `layout.*`, `scene.*` (AUTO's
detection), `ui.*` and `menu.*` (the overlay and the pause menu), `backup.*`, `script.*`,
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
  touch-screen position (`tests/layout_test.cpp` checks every pixel in every layout).
- **Settings** (`settings.*`): one `Settings` struct for every option, whoever uses it; loaded
  from `settings.ini` (plain `key=value`) in SDL's per-user folder when the window opens, never
  headless, saved on every change from the menu and when the program closes (with the window's
  size). Add a field with its default, its two lines in `Load` and `Save`, its row in `menu.cpp`.
- **Controls** (`controls.*`, `input.*`): each action (the 12 DS buttons, MENU, FAST FORWARD,
  SWAP SCREENS, SAVE STATE, LOAD STATE, BLOW, CURSOR PRESS) has a key (an SDL keycode, so the
  key's label on any layout) and a controller input; giving an action a key another one had
  swaps them. Controllers are read through SDL_GameController, labels by SDL's convention (A is
  the button printed A). The right stick moves a cursor on the touch screen, pressed with
  CURSOR PRESS (LT). Over the world, the mouse wheel holds X or Y a few frames per notch (the
  game zooms in with X, out with Y) and a right-button drag holds L or R for a frame every
  three points moved (the game turns the camera continuously while they are held).
- **The pause menu** (`menu.*`, `ui.*`): a `Canvas` the size of the output (ARGB, a 5 × 7
  bitmap font: capitals, digits and a few signs only) uploaded as one texture over the game;
  `Menu` draws a page of rows with values and a hint, and takes keyboard, controller and mouse
  input (hover chooses a row only when the pointer moves onto another one, so a resting pointer
  does not fight the keyboard). While it is open nothing is emulated. The same canvas shows the
  stick cursor, short messages and the fast forward mark.
- **Sound** (`audio.*`): the frames' samples go through `AudioStretch` (WSOLA, from FullDex),
  which stretches them at their own pitch when frames take longer than a DS frame, steered to
  keep about 100 ms queued; volume is applied after it; in fast forward the sound is muted or
  played at normal pitch with the rest skipped.
- **Save backups** (`backup.*`): `backups/<name>-<date>.sav` beside the save when the program
  starts and every half hour, unless identical to the newest copy; the last eight are kept.
- **AUTO**: `HudVisible()` checks row 190 of the top screen, the status bar's bottom border, one
  flat colour (`#415969`) from edge to edge in town and nowhere else. `AutoLayout` waits for ten
  frames of agreement before switching, so fades do not flicker.
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
- **Main loop**: one `RunFrame()` per frame, keys and touch from the script, the keyboard, the
  mouse and the controller, audio queued to SDL at 48 kHz. `frame` counts the game's frames
  (scripts, shots); `iter` every turn of the loop, paused ones too (`--frames`, window events).

`src/platform.cpp` is melonDS's Platform interface (files, threads, the game's save), adapted
from Pokémon FullDex, as are `controls.h`, `ui.*` and `audiostretch.h` (each says so in its
header); network and camera are stubs. `src/host.h` writes the save.

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
