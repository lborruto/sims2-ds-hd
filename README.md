<h1 align="center">The Sims 2 DS, one screen</h1>

<p align="center">
  <strong>The Sims 2 for the Nintendo DS in a single window.</strong><br>
  The town fills the screen, the touch screen's menus sit beside it and work with the mouse. No more two tiny stacked screens.
</p>

<p align="center">
  <a href="https://github.com/lborruto/sims2-ds-wide/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/lborruto/sims2-ds-wide/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <a href="LICENSE"><img alt="Licence: GPL v3" src="https://img.shields.io/badge/licence-GPLv3-blue.svg"></a>
</p>

<p align="center">
  <img src="docs/screenshots/gameplay.webp" alt="The Sims 2 DS in one window: the town at night fills most of it, the phone from the touch screen sits on the right" width="900">
</p>

## What it does

- **One window, the right screen big.** In town, the 3D world takes the window and the touch
  screen's panel (Interact, Pockets, Goals, Relationships, Options, the phone, conversations)
  sits beside it. When the game moves to the touch screen (a social reaction to pick in time,
  the shop's catalogue, Build Rooms) that one takes the window. Title, menus and Create-a-Sim
  show both screens side by side, since both matter there.
- **It switches on its own.** It sees what the game shows (its status bar, a social game, a
  shop) from the screens themselves, so the layout follows the game with nothing to set.
- **The DS clock is your clock.** The game lives by the time of day (rooms take real hours,
  skill points come at set hours), so it reads your computer's time, also after a save state,
  and fast forward never runs it ahead: the game never sees its clock go back.
- **A microphone when the game asks for one.** Blow into it with a key (clearing the vacuum's
  filter, the music mixer); your real microphone is a setting.
- **The mouse is the stylus.** Click the touch screen wherever it is shown.
- **Your keyboard is the DS.** Every button has a key; hold Space to fast forward.
- **Save states and screenshots.** One key each, beside the game's own save.

It runs the game on the [melonDS](https://melonds.kuribo64.net/) emulator core, built in.
It is a proof of concept: see [Good to know](#good-to-know).

<p align="center">
  <img src="docs/screenshots/create-a-sim.webp" alt="Create-a-Sim side by side: the Sim on the left, the name keyboard on the right" width="700">
</p>

## Get started

You need your own copy of the game, dumped from your cartridge (`.nds`). Nothing of the game
is included here.

**Download** from the [releases page](../../releases) once the first version is out:

| Where you play | Download | Start it |
|---|---|---|
| **Windows** (64-bit) | `sims2-ds-wide-…-windows-x86_64.zip` | unzip, double-click `sims2wide.exe`; the first time, More info > Run anyway |
| **Mac** (Apple silicon or Intel, macOS 11+) | `sims2-ds-wide-…-macos-universal.zip` | unzip, move *Sims 2 DS Wide* to Applications; the first time, right click > Open |
| **Linux** (x86_64) | `sims2-ds-wide-…-linux-x86_64.tar.gz` | unpack, run `./sims2wide` (the file picker needs zenity or kdialog) |

It asks for your `.nds` when it starts, in the folder of the last one you chose. A ROM on the command
line (`sims2wide path/to/your-sims2.nds`) skips the question.

**Or build it** (a few minutes):

```bash
git clone --recursive https://github.com/lborruto/sims2-ds-wide.git && cd sims2-ds-wide
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build sims2wide
./build/sims2wide path/to/your-sims2.nds
```

You need CMake (3.21 or later), Ninja and a C++20 compiler (Xcode's command line tools on a
Mac). On Linux, SDL's development packages too: [CONTRIBUTING.md](CONTRIBUTING.md#set-up) lists
them.

## Controls

| Key | Does |
|---|---|
| Arrows | +Control Pad |
| X / Z / S / A | A / B / X / Y |
| Q / W | L / R |
| Enter / Right Shift | Start / Select |
| Mouse | the touch screen, wherever it is shown |
| Tab | swap which screen is big |
| 0 / 1 / 2 / 3 | layout: AUTO / FOCUS / SIDE / STACKED |
| Space (hold) | fast forward |
| F5 / F7 | save state / load state (beside the ROM, `.quick.st`) |
| F12 | screenshot of both screens (beside the ROM, `-shot.png`) |
| Esc | quit |

The game's own save goes beside the ROM, with the same name and `.sav`. A `.sav` from another
emulator or a flash cart works if you put it there.

## Layouts

| Layout | What you see |
|---|---|
| **AUTO** (default) | FOCUS in town (the touch screen big during a social game, in the shop's catalogue or Build Rooms), SIDE everywhere else |
| **FOCUS** | one screen big, the other at the side (Tab swaps them) |
| **SIDE** | both screens side by side |
| **STACKED** | as on the DS, one above the other |

Start with another one: `--layout focus`. Run `sims2wide --help` to see every option,
including the headless mode used to study the game's screens
([CONTRIBUTING.md](CONTRIBUTING.md#studying-the-screens)).

## Looks

Three options make the game look better without changing it. All are off by default.

| Option | What it does | On the command line |
|---|---|---|
| **Sharper 3D** | draws the 3D world 2, 3 or 4 times sharper | `--res3d 3` |
| **Widescreen** | shows more of the town left and right, 16:9 instead of 4:3; the status bar stays in the middle | `--wide` |
| **60 fps** | adds in-between frames: the game draws its 3D at 30 frames a second | `--interp` |

<p align="center">
  <img src="docs/screenshots/widescreen.webp" alt="Widescreen and three times sharper: the town at night in 16:9, the status bar in the middle at the bottom" width="800">
</p>

Widescreen is a stretch for a game made for 4:3: the game is told to draw a wider view, and it
does in town, in conversations and in Create-a-Sim, but a seam can show here and there at the
edges. Each one costs speed; all three together still run well over full speed on a recent Mac.

## Good to know

- **A proof of concept.** It shows the idea works; it is not finished.
- **The side panel is small.** On a 1280 × 720 window it is 320 × 240, a little larger than
  the DS screen. Readable, but a bigger panel is planned.
- **Widescreen is opt-in.** See [Looks](#looks).
- **Most minigames not checked yet.** The social reactions, the shop and Build Rooms are; the
  hotel's activities (the vacuum, cards, painting, the music mixer…) are not. If one shows up
  wrong, Tab swaps the big screen and 2 shows both.
- **Tested with the European game** (code `ASJP`). The American one should work; its automatic
  layout is untested.
- **Not signed by Apple or Microsoft.** Hence the extra click the first time; each download
  comes with a build attestation from GitHub instead ([SECURITY.md](SECURITY.md)).

## Help and docs

| I want to… | Read |
|---|---|
| Build it, hack on it, see how it works | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Know what it reads and writes, report a vulnerability | [SECURITY.md](SECURITY.md) |
| See what changed | [CHANGELOG.md](CHANGELOG.md) |

Found a bug? [Open an issue](https://github.com/lborruto/sims2-ds-wide/issues) with your system,
the version and what the program printed in the terminal. Never attach a ROM, a save or a save
state.

## Licence and credits

GPLv3, see [LICENSE](LICENSE), as melonDS. Built on [melonDS](https://github.com/melonDS-emu/melonDS)
1.1, [SDL2](https://www.libsdl.org/), [stb_image_write](https://github.com/nothings/stb) and
[tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/).
The idea comes from [Pokémon FullDex](https://github.com/mamesj/pokemon-fulldex), which does
this for the DS Pokémon games; `src/platform.cpp` is adapted from it, and the sharper and
widescreen 3D and the in-between frames are ported from its renderer (`src/melonds_ext/`,
`patches/melonDS/`, GPLv3 too).

## Not affiliated, no warranty

An independent, unofficial project. It is not made, endorsed or supported by Electronic Arts,
Maxis, Nintendo or the melonDS team; the names are theirs, used only to say what it works with.
Bring your own copy, dumped from a cartridge you own. No warranty of any kind: keep a copy of
your save.
