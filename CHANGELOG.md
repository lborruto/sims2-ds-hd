# Changelog

What changes from one release to the next, in plain words. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/). A tag `vX.Y.Z` creates the GitHub release, with the
Windows, Mac and Linux downloads attached; the release takes its notes from that version's section below.

## [Unreleased]

### Added

- CONSOLE, the new default layout: the game played like a console game. The town fills the
  window, widescreen, sharper and at 60 fps, and everything the touch screen did opens big over
  it, redrawn in HD in the game's own colours, icons and words (read from your copy of the game at
  run time, in its five languages; nothing of the game is in the program). It reads what the game
  shows from its memory (the European version); on another version it falls back to recognising
  the screens by their pixels, with the game's own small pictures.
- The status bar in HD (the time, money and Sanity) on a strip the window's whole width, with the
  camera and menu hints at its right; what you just won or paid shows beside the money.
- Conversations in HD: the speaker's name, the words in large type and their portrait (without the
  DS's bevelled corners); A "Continue" turns the page.
- What A does, in the world: near someone or something, a plate says it ("A Buy", and who or
  what); hold A for the other actions, Left / Right "Other target" for the next thing nearby.
- The camera on the right stick: left / right turns, up / down zooms (the keyboard's Q / W and S /
  A as before).
- The whole town in HD with a controller: the menu's five tabs (Interact, Pockets, Goals,
  Relationships, Options), Give Gift, the game's save slots, popups, notes and the phone.
- The shops and the hotel in HD: the shop's catalogue (buy and sell), Build Rooms, the Registry
  and a guest's check-in, a room's page and its hints, the music panel and the stat panel (its
  pages on white paper, as the game drew them, its tabs named).
- The front end in HD: the intro, the title, the main menu, the game's Settings (volumes, concept
  art, credits, its four pages named in a tab bar), Load-a-Sim and Create-a-Sim (your Sim big, the
  name on the game's keyboard or typed on yours, gender, skin, hair, face, clothes, aspiration),
  drawn with the game's own pictures.
- Every minigame on the controller, in an HD frame of its own: the game's name, its numbers on
  dark wells, a bar of hints that follows what the game asks, and its messages and questions
  redrawn. The Alien Autopsy, the Metal Detector, the painting and the scratch tickets have a
  pointer on the sticks; Moogoo Monkey a green ring; every minigame can be left with a button,
  and the Drencher's, the Rat-A-Rang's and the mixer's help open with Start.
- The vacuum on a controller: A takes the find in the green ring (the one nearest the hand; the
  right stick moves the hand) to the funnel at the bowl's bottom and lets go when the funnel is
  ready, so it is kept and the filter never clogs; the D-pad drives (the keyboard still moves the
  hand with S and the arrows).
- Alien Autopsy on a controller: with the hand, A takes a freed organ to the tray by itself; with
  the tweezers, A takes the nearest parasite (an eating one first) to the flask. The laser still
  cuts with A held.
- The HD screens' look: the game's small pictures at two sizes, each pixel a whole block of screen
  pixels at any window size (with *Game icons* on Pixels); round list arrows; greyed buttons
  where nothing can be done; the focused row a deep blue with white words inside a green ring
  with a light line; no text under 18 points at 100 %. Every button hint names what it does
  (Choose, Scroll, Pages, Adjust, Turn, View), and the D-pad chips show arrowheads for the
  directions they mean.
- A launch panel before the game: Continue (where you quit, with its picture), choose a file or
  drop a `.nds` on the window, your recent games, the language, kept inside a TV's safe area. A
  file that is not The Sims 2 is said there; another region's cartridge is said once over the game.
- Settings: Interface size (Auto, the default, follows your screen's size and the window's; or 60
  to 150 %), Screen scaling (Sharp, the default, Smooth or Pixels), Game icons (Pixels, the DS's
  own, the default; or Smooth, redrawn by xBRZ), Hide the idle pointer (on), Battery saver (3D at
  1x and 30 fps while on), Mute, Pause in the background (on), Sound in fast forward, Stick
  cursor, Cursor speed (50 to 200 %), Swap the sticks (left-handed), Stick dead zone, Trigger
  press at, Buzz on connect (on), Microphone (off, the blow key or a real microphone), Fast
  forward speed (2×, 3×, 4× or as fast as it goes), Fast forward key (Hold or Toggle), Fast boot
  (on: the boot's splashes, which wait for no button, at full speed). The window comes back where
  it was; its first size fits the screen.
- The game's language is the DS's own, set from your computer's (or the one you pick): the
  game's text, its keyboards and the HD screens follow it.
- Every controller, natively or through Steam: XInput, DirectInput, PlayStation, Switch, 8BitDo
  and the rest SDL knows, plugged in or out at any time; SDL_GameControllerDB built in, and your
  own `gamecontrollerdb.txt` in the settings folder. The Steam Controller and the Steam Deck work
  through Steam's default Gamepad layout (added as a non-Steam game); without Steam, the Steam
  Controller needs `SDL_JOYSTICK_HIDAPI_STEAM=1`. The on-screen hints show the controller you used
  last, or your keys when you last typed.
- When the ROM's folder cannot be written, the game saves to the settings folder's `saves/`
  instead, and the pause menu says where; once it can be written again, the newer save goes back
  beside the ROM.
- Messages say when a state is saved or loaded, a screenshot taken, the save backed up, a
  controller connected (with how to pause), or when the game cannot write its save.
- Unknown screens are recorded once each (a picture and a save state in `captures/`, at most 20 a
  run, 200 in all), so they can be sent in.
- AUTO: Keelhaulin' Cards shows the cards big, and the hotel's vacuum its bowl, clogged or not
  (the blow key, M, clears the filter).
- Windows: the window is sharp on high-DPI screens.
- Tools and checks: `--print-game`, `--print-hud`, `--print-town`, `--print-hotel`,
  `--print-front`, `--print-minigame`, `--print-console`, `--print-shell`, `--measure-menu`,
  `--measure-string`, `--dump-string`, `--dump-resource`, `--dump-sprite`, `--language`,
  `--game-language`, `--ui-scale`, `--game-pictures`, `--shell`, `--launch`, `--data-dir`,
  `--compose-scale`, `--no-romdata`; the check scripts run at most `MAX_SIMS` emulators at once.

### Changed

- The pause menu (Start + Back, or Esc, which also resumes from any of its pages) is in HD and
  five languages, with the buttons of the controller you hold. Its Settings have five tabs
  (Display, Graphics, Sound, Controls, Game) changed with LB / RB (Q / W), the rarely needed rows
  under *Advanced*, a line saying what each row does, `‹ ›` on what Left / Right (or A) adjust,
  and Y to put a tab back to its defaults. Controls shows what each button does on your device,
  and Keys and buttons changes any of them (A to change one, X to clear it, Y to set them all in
  turn, the last row to reset them; the pause menu keeps a key and can have one button of its
  own). Four save states with their picture and date: loading one asks first and can be undone
  (asked too), even after a restart. Change game, Open the saves folder, and Quit, which asks
  first and keeps your place. Every question opens on No and B says No; a message closes with A
  or B. Lists stop at their ends.
- CONSOLE is the default layout; AUTO, FOCUS, SIDE and STACKED stay in the pause menu's Settings
  (keys 0 to 4). Sharper 3D, widescreen and 60 fps are on by default; a settings file from before
  CONSOLE that still holds the old defaults is moved to the new ones once, and a file that names a
  layout is never touched.
- The pad's Back button opens the menu, and Select moved to the left stick's click. The Guide /
  PS / Home button no longer opens the pause menu (it belongs to Steam, the Deck and the system):
  Start + Back does.
- LB / RB switch the menus' tabs and pages; in town they no longer turn the camera (the right stick
  does), unless you give L / R buttons of your own.
- With several controllers, the one used last plays (before, every pad's buttons counted at once).
- The game's questions (a minigame's, Create-a-Sim's) open on No: two quick presses of A never say
  yes. In Create-a-Sim's last page, the first B only moves to the cross.
- The quick save state (F5 / F7) moved from beside the ROM to the settings folder; F12 saves the
  window as you see it, at full resolution, named by the date and time, in `screenshots/` there.
- The DS buttons' keys follow their position on any keyboard layout (AZERTY: B on W, Y on Q, L on
  A, R on Z).
- DS Wireless is greyed on the main menu: it needs a second DS.
- The game pauses and goes quiet when its window is not in front (Settings > Pause in the
  background). The Steam overlay does not pause it.
- The volume moves by 5 %.
- The README is short and for players; the full manual is `docs/manual.md`.

### Fixed

- A settings file with a 3D resolution above 4 showed it, and Right then lowered it to 4: it is
  now read as 4.
- Create-a-Sim: changing page no longer flashes the last page's pictures in the new page's cells
  (letters in the hair styles, text over the clothes); they stay empty until the game has drawn them.
- Create-a-Sim: the aspirations' pictures fade out with the screen instead of staying bright.
- The HD menus no longer lag on a 4K screen: a tab change takes about 10 ms instead of 150, a
  focus move about 3 instead of 38.
- The HD screens never flash the DS's own pictures while the game swaps its screens (tabs, the
  phone, Give Gift, the Registry, loading a slot).
- No dead ends on a controller: the vacuum, the furniture editor, the painting (all 240 colours),
  the minigames' help and every popup can be left or used without the mouse.
- Presses on sliders (the volumes) all count, however fast; the focus no longer flickers onto the
  wrong item when a popup comes or goes; the save slots and the phone come back to the menu where
  you were; the phone's B goes back one page instead of hanging up.
- The intro's last screen took A or Start only about three times in four: the touch is now held
  long enough for the game to see it.
- A quick click or a trackpad tap on the touch screen was often lost: every press now holds the
  stylus down for three frames.
- In the casino the screens swapped size now and then (AUTO): a guessed rule is gone.
- The game ran twice too fast on 120 Hz screens: the window keeps the DS's own pace, 59.83
  frames a second.
- A mouse drag that started on the touch screen no longer lifts the stylus when it overshoots
  the screen's edge.
- AUTO no longer flashes the touch screen big at the hotel's doors.
- Widescreen: no more thin cracks of light across the curtain in Create-a-Sim.
- 60 fps: a Sim no longer tears apart for a frame when they start, stop or turn; a model moves
  in one piece, or, the rare time it cannot be followed, takes that one step at 30 fps.
- A failed state load puts back the game as it was a moment before; save states and the place
  kept on quitting are written safely (a failed write keeps the old one).
<!-- The front end, Create-a-Sim, the shop and the minigames (ux audit, task 6). -->
- MooGoo Monkey: before the players are named, the side panel says "Player 1-3" (not the game's
  copyright line).
- The scratch lottery's messages no longer carry a stale "Loading" title.
- The vacuum's side panel shows the money of this session, as the bowl does (it said §0 until
  the vacuum was put away).
- German music mixer: X says "Abspielen" (it said START, beside the help's own Start chip).
- Create-a-Sim: B goes back a page (from the name page, to Keep this Sim?'s cross), Start on Keep
  this Sim? keeps the Sim, and every page says what A, B and Start do; X says what it changes on
  the keyboard. The hair colours, styles, faces and skin figures sit on the HD cells without the
  DS's frames and green rings; the scroll arrows are the menus' own; no highlight on an empty
  cell while a page comes in.
- The title and the boot's last splash say what A / START do (Continue), the title's within the
  screen's safe area; DS Wireless's note sits on a well; Load-a-Sim's places and times are white;
  the credits scroll on a dark well, their headings readable; the German Settings tabs have room.
- The shop: the items no longer touch their cells' rims; the D-pad's hint says Choose; a greyed
  Pay says why (not enough money, no place left, not taken here).
- The minigames: the hints' buttons one size everywhere and large enough to read (the help's
  Start, and the painting's Back, moved up beside the title); the focus is the menus' green ring
  everywhere; a help text open in a loaded save state is redrawn in HD; the cards no longer show
  the DS's score screen (the side panel says it); the save page shows no hints before it is up;
  the vacuum's hint stays "Taking it to the funnel" until the find is kept; the Metal Detector says
  what it found after the card closes, and "None" for no signal; the painting's question stays in
  one place; Walk and Point hints name every stick that does it; the autopsy's quit says the
  game's Quit; Moogoo's Start (quit) is in the manual; the social answers sit on rows.

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
