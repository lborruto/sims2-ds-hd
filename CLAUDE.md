# sims2-ds-wide — notes for Claude

The Sims 2 (Nintendo DS, 2005) in one window, in the spirit of
[Pokémon FullDex](https://github.com/mamesj/pokemon-fulldex). The melonDS 1.1 core (submodule,
unmodified) runs the game; a small SDL2 frontend (`src/`) lays out the two screens and
picks the layout from what the screens show. No decompilation or recompiler: none exists for
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
- AUTO stays the default layout. melonDS stays unmodified unless the owner agrees.
- One task = one clear commit; `ninja -C build sims2wide` and a headless run before each commit.

## Layout

| Path | Role |
| --- | --- |
| `src/main.cpp` | Command line, starting the DS, the frame loop (input → `RunFrame` → AUTO → shots → sound → picture; paused: menu only) |
| `src/app.h` | Shared state: the `NDS`, layout chosen/shown, `mainIsTop`, fast forward, quit, `micBlow`, `topAspect`, F5/F12 paths |
| `src/settings.*` | `Settings`: every option with its default; `settings.ini` load/save (SDL pref path), not headless |
| `src/controls.*` | Actions, key and controller bindings, defaults, names (from FullDex) |
| `src/input.*` | Keyboard, mouse (stylus; wheel and right drag as the camera), SDL_GameController, stick cursor, script window events |
| `src/video.*` | The SDL window: draws the shown layout and the overlay, fullscreen, scaling, maps window points to touch positions |
| `src/ui.*`, `src/menu.*` | `Canvas` (ARGB overlay, 5 × 7 font) and `Menu` (from FullDex); `PauseMenu` (pages, bindings capture, toasts, cursor) |
| `src/audio.*`, `src/audiostretch.h` | SDL sound queue, WSOLA stretching when slow (from FullDex), volume, fast forward |
| `src/backup.*` | Copies of the `.sav` in `backups/`, at start and every half hour, eight kept |
| `src/layout.*` | `Place()` (canvas per layout, `topAspect`, FOCUS's overlaid panel), `CanvasFit`, `TouchFromOutput`, `OverTop` |
| `tests/layout_test.cpp` | Touch mapping of every pixel in every layout and size (`ninja -C build layout_test`) |
| `src/scene.*` | AUTO's detection from the screens' pixels: `HudVisible()`, `AutoLayout::Update(top, bottom, mainIsTop)` |
| `src/script.*`, `src/shots.*`, `src/state.*`, `src/picker.*` | Headless input scripts; PNG shots and `--compose`; save states; ROM file picker and `Tell()` |
| `src/platform.cpp` | melonDS Platform interface (files, threads, save), adapted from pokemon-fulldex |
| `src/host.h` | Writes the game's save when melonDS reports it changed |
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
(first conversation), `s6` (after it), `s7` (free roam in town) are in git-ignored `shots/`.

## Known traps

- melonDS framebuffers are `0xAARRGGBB`, 256 × 192 each, `GPU.Framebuffer[FrontBuffer][0]` =
  top, `[1]` = bottom.
- HUD detection uses row 190 only (`#415969` from x 8 to 248). Row 186 crosses the clock's
  white text; rows 189 and 191 are not flat.
- Touch mapping goes through the same `Place()` as drawing: change one, the other follows.
  Window points and renderer pixels differ on HiDPI; `Video::ToOutput` converts. FOCUS's panel
  lies over the world: it is drawn second (`Placement::topFirst`, `alpha`) and wins the mouse.
- The game zooms in with X and out with Y; L and R turn the camera continuously while held
  (checked with `scripts`-style runs from `s7`). The wheel and the right-button drag press them.
- Settings are never loaded or saved headless (runs must be the same everywhere); the menu
  never opens there. Window events in a script (`press`, `click`, `winshot`) count loop turns
  (`iter`), not game frames: the game stands still while the menu is open.
- The menu's font has capitals, digits and `:-+./<>!(),` only: write its text in capitals.
- A real pointer resting on the menu sends motion events; hover selects a row only when the
  pointer moves onto another one, otherwise it overrides keyboard navigation.
- Fullscreen keeps the pointer visible: the mouse is the stylus.
- In town the bottom screen often has a popup to close (the X near 126,110 or 126,126) before
  the D-pad moves the Sim.
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
| 1 | POC: melonDS + SDL frontend, headless study of the screens, AUTO layout | ✅ |
| 2 | Repository template (docs, CI, release, issue templates), portfolio page in `lborruto/docs` | 🔄 |
| 3 | Bigger side panel, or a panel over the edge of the world view | ✅ |
| 7 | Comfort: pause menu, settings file, controllers, mouse camera, fullscreen, sound stretching, save backups | ✅ |
| 4 | Widescreen 3D, borrowed from FullDex's `GPU3D_Wide.cpp` | ⏳ |
| 5 | Survey the minigames and the other situations (shops, jobs, night) | ⏳ |
| 6 | Desktop apps: file picker, universal Mac app, Windows .exe (CI and release) | 🔄 (PR #1) |
