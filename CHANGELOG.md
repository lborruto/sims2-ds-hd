# Changelog

What changes from one release to the next, in plain words. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/). A tag `vX.Y.Z` creates the GitHub release, with the
Windows, Mac and Linux downloads attached; the release takes its notes from that version's section below.

## [Unreleased]

### Changed

- CONSOLE is now the default layout (the other layouts stay in SETTINGS > DISPLAY > LAYOUT).
- The pad's Back button now opens the TOUCH MENU, and Select moved to L3 (the left stick's
  click), in every layout. Back does nothing outside CONSOLE.
- Looks for existing settings: a settings file from before CONSOLE that still holds the old
  defaults (3D at 1x, 4:3, 30 fps) is moved to the new ones (sharper, widescreen, 60 fps) the
  first time it is loaded. To turn them off, change 3D RESOLUTION, WIDESCREEN WORLD and 60 FPS in
  SETTINGS > GRAPHICS; a file that names a layout is never touched.

### Added

- The status bar in HD: in CONSOLE the clock, money and Sanity meter are redrawn sharp in the
  game's own style and language, read from the game's memory; the town keeps its sides in the
  widescreen world, with the bar in a strip below it.
- Conversations in HD: the speaker's name, the game's own words in large type, and the speaker's
  portrait, in a panel over the whole world; A turns the page. The words are read from your copy
  of the game (nothing of them in the program); with another version of it, the old picture stays.
- A shared HD interface kit (panels, text, button chips) with the Nunito font built in (SIL Open
  Font License, shipped with every download). Checks and tools: `--print-hud`, `--ui-demo`,
  `--compose-scale`, `--measure-string`, `--no-romdata`; `--language` also picks the HD words.

- CONSOLE now asks the game what it is showing, read from its memory, instead of guessing from
  the pixels: every screen is known, the front end (title, menus, Create-a-Sim, save slots) and
  screens not yet named are framed, never the bare two-screen layout, a black load shows only
  the backdrop, and a press waits until the game's screen has settled. It needs the
  European game; on another version CONSOLE falls back to the pixel rules.
- Unknown screens are recorded: a picture and a save state, once per screen, in `captures/` in
  the user folder (at most 20 a run, 200 in all), so they can be sent in and named.
- The game's own texts, read from your ROM at run time in its five languages (nothing of them in
  the program), for the screens to come; `--strings-count`, `--dump-string`, `--language`.
- `--print-game` prints what the game's memory says is up.
- A settings file from a development build before this release that says `layout=auto` (and
  no `settings_version`) starts in CONSOLE, once; v0.1.1's files never named a layout.
- CONSOLE, now the default layout: the game played like a console game. The town fills the
  window, widescreen, sharper and at 60 fps (these are now on by default), with a small rail of
  the touch screen's tabs and phone. One button (Back, or E) opens the touch screen's menu big
  over the town; conversations, popups and the shop get a big picture of their own; the social
  game, Keelhaulin' Cards and the vacuum are played on the buttons, with a box of hints. The
  D-pad moves a highlight, A taps, LB and RB switch tabs, B closes. The other layouts stay in
  SETTINGS > DISPLAY > LAYOUT (key 4 is CONSOLE) and the choice is kept between runs.
- The casino's card game (Keelhaulin' Cards) shows the cards big, the table beside them.
- The DS buttons' keys follow their position on any keyboard layout: on AZERTY, B is on W,
  Y on Q, L on A and R on Z, the same places as on QWERTY.

- The hotel's vacuum: AUTO shows its bowl big, with the world beside it, clogged or not; the
  blow key (M) clears its clogged filter.

### Fixed

- A quick click or a trackpad tap on the touch screen was often lost (the button went down and
  up between two frames): every press now holds the stylus down for three frames.
- In the casino the screens swapped size now and then: a guessed rule made the touch screen big
  in dim places. It is gone; only situations checked in the game switch the layout.
- The game ran twice too fast on 120 Hz screens (MacBook Pro, gaming monitors): the window now
  keeps the DS's own pace, 59.83 frames a second, whatever the screen.
- A mouse drag that started on the touch screen no longer lifts the stylus when it overshoots
  the screen's edge (dragging a find into the vacuum's bag, in its corner, dropped it).
- AUTO no longer flashes the touch screen big for half a second at the hotel's doors (dark
  rooms fade the world out before the panel).
- Widescreen: no more thin cracks of light across the curtain in Create-a-Sim.
  The edges of polygons seen edge-on picked up colours from far along them.

## [0.1.1] - 2026-11-04

First release: The Sims 2 (DS) in one window on Windows, Mac and Linux.

### Added

- The Sims 2 (DS) in one window, on the melonDS 1.1 core.
- AUTO layout: in town, the 3D world big with the touch screen's panel beside it; title, menus
  and Create-a-Sim side by side. Town is recognised from the game's status bar.
- FOCUS, SIDE and STACKED layouts by hand (0 to 3), Tab to swap the big screen.
- Mouse as the stylus, keyboard as the DS buttons, fast forward, save states, screenshots, and
  the game's own save beside the ROM.
- Downloads for Windows (one `.exe`), Mac (an app for Apple silicon and Intel) and Linux.
- Started without a ROM (a double click), it asks for one with the system's file picker and
  opens in the same folder next time.
- Looks, all off by default: sharper 3D (`--res3d N`, 2 to 4 times the DS's resolution), a
  widescreen 16:9 world (`--wide`) and 60 fps in-between frames for the 3D (`--interp`), ported
  from Pokémon FullDex's renderer.
- A headless mode to study the screens: input scripts, shots, save states, and pictures of the
  window's layout (`--compose`).
- The touch screen takes the window when the game plays on it: the timed social reactions
  (Impress and the like), the shop's catalogue and Build Rooms. The world comes back when they
  end.
- The DS microphone: by default a key blows into it (the vacuum's filter, the music mixer); the
  computer's own microphone, or none, as a setting (`--mic`).
- The DS clock is the computer's, also after loading a save state, and fast forward does not
  run it ahead of real time: the game's days, skill points and night missions keep to the
  clock, and it never goes back (the game punishes that). `--clock` fixes it for tests.
- The side panel at twice the DS's size, over the world's right edge, a little see-through;
  every layout follows the world's shape when it is widescreen.
- A pause menu (Esc): resume, save and load state, settings in four pages (display, graphics,
  sound, controls) with a hint for each, quit. Mouse, keyboard and controller all work in it.
- Settings kept between runs (`settings.ini` in the user folder), the window's size too.
- Controllers (Xbox, PlayStation, Switch): every DS button, the triggers for fast forward,
  Guide or Start + Back for the menu, the right stick as a cursor on the touch screen. Every
  key and button can be changed in the menu; M blows into the microphone.
- The mouse works the camera: the wheel zooms, a right-button drag turns it, over the world.
- Fullscreen (F11) and smooth scaling.
- Sound stretched at its own pitch when the emulation falls behind, instead of crackling; a
  volume setting; fast forward muted or at normal pitch.
- Copies of the game's save in `backups/` beside it, at start and every half hour, the last
  eight kept.
- Input scripts can drive the window itself (`press`, `move`, `click`, `wheel`, `winshot`),
  for checking the menu and the mouse.

## [0.1.0] - 2026-11-04

Tagged, never released: the release workflow could not check the tag against `main` in a
private repository (fixed in 0.1.1).
