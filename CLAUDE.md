# sims2-ds-hd: notes for Claude

The Sims 2 (Nintendo DS, 2005) played on a computer like a console game, in the spirit of
[Pokémon FullDex](https://github.com/mamesj/pokemon-fulldex). The melonDS 1.1 core (submodule,
plus small looks hooks in `patches/melonDS/`) runs the game; a C++20 / SDL2 frontend (`src/`)
shows it in one 1280 × 720 window. CONSOLE, the default layout, reads what the game shows from
its RAM (European ROM, `ASJP`) and redraws each touch-screen screen in HD (`src/port/`), driven by
a controller. No decompilation: the port reads RAM and the ROM's own data at run time and never
writes RAM. The full per-module notes, traps and known limits are in
[docs/dev-notes.md](docs/dev-notes.md); the RAM map in [docs/research/ram-map.md](docs/research/ram-map.md).

## Permanent rules

- **No game content in git, ever**: no ROM (`.nds`), save (`.sav`), save state (`.st`), dump of
  the game's files, code, graphics, sound or text strings. `roms/` and `shots/` are git-ignored;
  never force-add. Screenshots in `docs/screenshots` are fine. Never say where to get a ROM, nor
  name ROM sets' files.
- No personal paths, host names, user names or e-mail addresses in anything committed.
- Commit with your own identity. Never push `main` without the maintainer's go-ahead.
- CONSOLE stays the default layout; AUTO, FOCUS, SIDE and STACKED are chosen by hand.
- melonDS stays the upstream 1.1 tag: changes go in `patches/melonDS/` (applied by CMake), only
  with the maintainer's agreement; never a fork.
- The port's own words (where the game's string table has none) only through `src/port/ownwords.*`,
  in all five languages. Nothing new uses the 5 × 7 font (`src/ui.*`).
- Docs: README.md is short and for players; `docs/manual.md` is the player's full manual;
  technical detail goes here, in `docs/dev-notes.md` and CONTRIBUTING.md. A change that alters
  what one of them says updates it. Player-visible changes add a CHANGELOG line.
- One task = one clear commit; build, the unit tests and a headless run before each commit.

## Build and test

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # CMake 3.21 to 4.x
    ninja -C build sims2hd
    ./build/sims2hd roms/<rom>.nds --headless --frames 400 --state-load shots/s7.st \
        --save shots/scratch.sav --compose --shot-at 399 --shot-dir shots/c

- Automated runs always pass `--headless --frames N` (without it a window opens and waits) and a
  scratch `--save`. Headless defaults to CONSOLE with the looks off and never loads settings.
- Unit tests (no ROM): `ninja -C build <name>_test && ./build/<name>_test` for layout,
  touchscript, hotspots, screens, navigator, controls, gamestate, romdata, uikit, shell and pads
  (`SDL_VIDEODRIVER=dummy`). `ui_bench` is a measure, not a check.
- Scripts (need the ROM, save states in `shots/`, a complete save and Pillow):
  `ROM=… SHOTS=… MORE_SHOTS=… COMPLETE_SAVE=… PYTHON=… MAX_SIMS=4 bash tests/console_check.sh`,
  and `gamestate_check`, `hotspots_check`, `romdata_check`, `screens_check`, `interp_check` the same way.
  `MAX_SIMS` caps the emulators run at once (4; lower on a hot machine). Run long scripts in the
  background.
- The identity check: with the looks off, `--layout auto` output from the 24-shot set (see
  CONTRIBUTING.md) stays byte for byte the same as `main`'s. Rasteriser changes are deliberate
  re-baselines of `console_check`'s pictures, never side effects.
- `romdata_test` is also run once under UBSan with libc++'s hardened mode after a change there.
- Town states run with `--clock "2026-01-01 12:00"` (hotel ones 14:00 or later, never earlier);
  the complete save's with `--clock "2026-11-05 12:05"`. The states list is in docs/dev-notes.md.

## Architecture map

- `main.cpp`: command line, window, launch panel, the frame loop (input → `RunFrame` → layout → picture → sound).
- `app.h`: shared state (the NDS, layout, game state, the ROM's strings and icons).
- `settings.*`: every option, `settings.ini`, `AutoUiScale`. `controls.*`: actions and bindings.
- `input.*`, `pads.*`: keyboard, mouse, controllers (hot-plug, Steam Input, mappings, chips' look).
- `video.*`: the SDL window, scaling, `ui::LayerCache` per layer, toasts, mouse → touch mapping.
- `shell.*` (pure), `shellhost.*`, `slots.*`: the pause menu, launch panel, save states, suspend, save fallback.
- `gamestate.*`: `ReadGameState` reads the game's screen manager and every screen's part from RAM.
- `romdata.*`, `port/icons.*`: the ROM's strings (5 languages), archive, decompression, sprites.
- `console.*`: CONSOLE's composition per screen (`BuildConsole`); `consolehold.*` holds HD through swaps.
- `navigator.*`: CONSOLE's focus and buttons over a `MenuView`; the gate on `settled`; the world's camera.
- `activities.*`, `padstate.h`: minigames on buttons (pointer, schemes, questions).
- `compose.*`, `layout.*`: pieces + overlays + UI items; the same composition draws and maps the mouse.
- `ui/kit.*`, `ui/font.*`: the HD kit (panels, text, chips, images, zoom, xBRZ Smooth) and Nunito.
- `port/menuview.*`: the shared model of an HD screen (items, actions, ids, moves).
- `port/*`: one module per HD screen (panelmenu, interact, pockets, gift, goals, relations, options,
  popup, phone, note, catalogue, build, checkin, roominfo, music, statpanel, front, frontmenus,
  cas, minigame, dialogue, statusbar, prompt); `port/ownwords.*` the port's own words.
- `screens.*`, `hotspots.*`, `scene.*`: the pixel rules (AUTO, and CONSOLE without RAM) and touch targets.
- `touchscript.*`: queued taps, holds and drags. `script.*`, `shots.*`, `state.*`: headless scripts, PNGs, states.
- `looks.*`, `melonds_ext/`, `patches/melonDS/`: sharper / widescreen 3D and 60 fps (from FullDex).
- `audio.*`, `audiostretch.h`, `mic.*`, `clock.*`, `backup.*`, `recorder.*`, `picker.*`, `platform.cpp`; `prefdir.*` the per-user folder (and the move from the old name's).

## Key traps

1. RAM addresses are the European ROM's (`kEU`). Any other ROM: `App::game` stays invalid and
   CONSOLE falls back to the pixel rules (`screens.*`). Never cache a heap object: `S` is read
   again and its vtable checked every frame.
2. `GameState::settled` is the only "the game is ready" signal: a tap before it lands on the old
   screen. The navigator's gate holds presses until settled; a view with `replay` false drops them.
3. Every part of `GameState` is range-checked and valid on its own; garbage makes only that part
   invalid. An invalid part means milestone 1's picture, never a guess.
4. Touch mapping goes through the same `Composition` as drawing: change one, the other follows.
   Invisible (alpha 0) pieces carry the mouse onto the game's own targets.
5. The UI raster must stay deterministic (premultiplied floats, banded threads giving the same
   bytes); `console_check` compares pictures byte for byte and `uikit_test` pins hashes.
6. Images (`ImageItem`) are drawn above every still item: nothing of the UI may lie over a picture.
7. Items go in ascending layers (`SortUi`); a layer's change rasters only that layer. Put
   fast-changing things (clock, focus, pointer) on their own layer.
8. Builders lay out at 100 %; the UI's size is applied after, per group, by `ZoomUi`. Never read
   `ui::UiScale()` in a builder. The navigator's rects stay the design's.
9. Headless never loads settings and starts with the looks off: outputs must be the same on every
   machine. Window-only settings must change no headless output.
10. Never set the DS clock back (the game punishes it): `clock::Seed` after reset and state loads.
11. The game's `$` is its simoleon glyph: draw §. Strings carry `{xx}` escapes and `@` placeholders:
    drop them with `DropEscapes` / `MenuWords`, never show them. `--measure-menu` fails on a cut word
    or a letter Nunito lacks.
12. The HD crops come from fixed DS rows (`HudCropRows`): moving them moves every picture and hash.
13. A press on the touch screen holds the stylus at least 3 frames; some screens poll every 4-6
    frames (the boot's last splash: 8).
14. melonDS writes the cartridge's save on a state load: back it up first (`ShellHost::Load` does).
15. State files hold the game's memory: never commit, attach or upload one (captures included).

## Adding an HD screen

1. **Read it**: add a part to `GameState` (`gamestate.*`) read only on its own screen (vtable,
   overlay), every field range-checked, `valid` on its own; a `--print-*` line; tests on made-up RAM
   in `gamestate_test` and on save states in `gamestate_check.sh`. Document addresses in
   `docs/research/ram-map.md`.
2. **Model it**: a `port/<name>.*` module with `<Name>View(...)` → `MenuView` (items with stable
   ids under a new `kMod…` tag, actions as the game's own taps / keys, explicit moves where the
   geometry is ambiguous, `own`, `replay`, `closes` / `backCloses`) and `Build<Name>(...)` → kit
   items (frame on layer 0, content on 1, images last, `SortUi`), plus invisible mouse pieces onto
   the game's targets, and `Measure<Name>` for `--measure-menu`.
3. **Wire it**: `ConsoleView` (`port/popup.*`) returns its view on that screen; `BuildConsole`
   draws it; `consolehold.*` knows its branch; `PopupOver` if popups can open over it.
4. **Words**: the game's strings by id; anything else via `ownwords.*` in five languages.
5. **Check it**: `layout_test` (every enabled item reachable, inside the panel, ids distinct,
   layers ascending, mouse round trips), `navigator_test` for new behaviour, `console_check.sh`
   pictures and `pad` scripts from a save state, `--measure-menu` in every language.

## Where the detail lives

- [docs/dev-notes.md](docs/dev-notes.md): every module's notes, the save states, the checks' cases,
  all traps and every known limit.
- [docs/research/ram-map.md](docs/research/ram-map.md): the game's RAM map (European ROM).
- [CONTRIBUTING.md](CONTRIBUTING.md): setup, checks, the headless flags, releasing.
- [docs/manual.md](docs/manual.md): what players see and press.
- [CHANGELOG.md](CHANGELOG.md): what changed for players.
