# Changelog

What changes from one release to the next, in plain words. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/). A tag `vX.Y.Z` creates the GitHub release, with the
Windows, Mac and Linux downloads attached; the release takes its notes from that version's section below.

## [Unreleased]

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
- A headless mode to study the screens: input scripts, shots, save states, and pictures of the
  window's layout (`--compose`).
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
