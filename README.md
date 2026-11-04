# The Sims 2 DS, one screen (proof of concept)

The Sims 2 (Nintendo DS, 2005) in a single window, in the spirit of
[Pokémon FullDex](https://github.com/mamesj/pokemon-fulldex): the melonDS core runs the game,
and a small frontend lays out the two DS screens.

- **AUTO** (default): in town the 3D world fills the window and the touch screen's menu panel
  (Interact, Pockets, Goals, Relationships, Options, phone) sits beside it, clickable with the
  mouse. Title, menus and Create-a-Sim show the two screens side by side. Gameplay is
  recognised from the HUD strip under the world view, so there is nothing game-specific to
  dump or ship.
- **FOCUS**, **SIDE**, **STACKED**: the same layouts by hand.

Bring your own ROM (tested with the European one, `ASJP`).

## Build and run (macOS, Linux)

Needs CMake 3.21 to 3.x, Ninja and a C++20 compiler; SDL2 is downloaded and built in.

    git clone --recursive <this repo> && cd sims2-ds-wide
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    ninja -C build sims2wide
    ./build/sims2wide path/to/sims2.nds

| Key | DS |
|---|---|
| Arrows | +Control Pad |
| X / Z / S / A | A / B / X / Y |
| Q / W | L / R |
| Enter / Right Shift | Start / Select |
| Mouse | touch screen (wherever it is shown) |
| Tab | swap the big screen (FOCUS) |
| 0 / 1 / 2 / 3 | AUTO / FOCUS / SIDE / STACKED |
| Space (hold) | fast forward |
| F5 / F7 | save / load state (`quick.st`) |
| F12 | screenshot of both screens (`shot.png`) |

The game's own save is written beside the ROM (`.sav`).

## Studying the screens

`--headless --frames N` runs without a window as fast as it can (about 4× real time), with
`--input` scripts (`scripts/`), `--shot-every` / `--shot-at`, `--state-load` / `--state-save`
and `--compose` (the window's picture instead of the two raw screens).
`tools/sheet.py` makes a contact sheet of shots. Run `build/sims2wide` alone for every option.

## Licence

GPLv3, as melonDS. `src/platform.cpp` is adapted from pokemon-fulldex.
