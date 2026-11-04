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

and look at the shots (`tools/sheet.py`). A layout change is checked with `--compose` from save
states of the situations it touches (below).

CI runs on every pull request and push to `main`: actionlint and shellcheck on the workflows,
gitleaks over the whole history, and Release builds the way the release makes them: Linux
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

Everything is in `src/main.cpp`, in sections:

- **Input script**: parses `--input` files.
- **Layouts**: `Place()` puts the top and bottom screens on a canvas (FOCUS: 1280 × 720, the big
  screen 960 × 720, the other 320 × 240 at the right, centred). The window scales the canvas;
  mouse clicks go back through the same `Place()` to a touch-screen position.
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
- **Main loop**: one `RunFrame()` per frame, keys and touch from the script, the keyboard and
  the mouse, audio queued to SDL at 48 kHz.

`src/platform.cpp` is melonDS's Platform interface (files, threads, the game's save), adapted
from Pokémon FullDex; network, microphone and camera are stubs. `src/host.h` writes the save.

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
