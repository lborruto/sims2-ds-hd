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
- Conversations in HD: the speaker's name, the words in large type and their portrait; A turns
  the page.
- What A does, in the world: near someone or something, a plate says it ("A Buy", and who or
  what); hold A for the other actions, Left / Right for the next thing nearby.
- The camera on the right stick: left / right turns, up / down zooms (the keyboard's Q / W and S /
  A as before).
- The whole town in HD with a controller: the menu's five tabs (Interact, Pockets, Goals,
  Relationships, Options), Give Gift, the game's save slots, popups, notes and the phone.
- The shops and the hotel in HD: the shop's catalogue (buy and sell), Build Rooms, the Registry
  and a guest's check-in, a room's page and its hints, the music panel and the stat panel.
- The front end in HD: the intro, the title, the main menu, the game's Settings (volumes, concept
  art, credits), Load-a-Sim and Create-a-Sim (your Sim big, the name on the game's keyboard or
  typed on yours, gender, skin, hair, face, clothes, aspiration).
- Every minigame on the controller, in an HD frame of its own: the game's name, its numbers, a bar
  of hints that follows what the game asks, and its messages and questions redrawn. The Alien
  Autopsy, the Metal Detector, the painting and the scratch tickets have a pointer on the sticks;
  Moogoo Monkey a green ring; every minigame can be left with a button, and the Drencher's, the
  Rat-A-Rang's and the mixer's help open with Start.
- A new pause menu (Start + Back, or Esc), in HD and five languages, with the buttons of the
  controller you hold: Settings as one list, the Controls card, Keys and buttons (A to change one,
  X to clear it, Y to set them all in turn), four save states with their picture and date
  (loading one asks first, and can be undone, even after a restart), Change game, Open the saves
  folder, and Quit, which asks first and keeps your place.
- A launch panel before the game: Continue (where you quit, with its picture), choose a file or
  drop a `.nds` on the window, your recent games, the language. A file that is not The Sims 2 is
  said there; another region's cartridge is said once over the game.
- Settings: Interface size (Auto follows your screen's size and the window's, or 60 to 150 %),
  Game pictures (Smooth: the game's icons and portraits redrawn with soft edges by xBRZ; or
  Pixels), Picture filter (Sharp, Smooth or Pixels), Battery saver, Mute, Run in background, fast
  forward speed (2×, 3×, 4× or maximum) and a press-on / press-off fast forward key, Fast boot,
  cursor speed, swap the sticks (left-handed), hide the idle mouse pointer, the sticks' dead zone,
  how far the triggers go down, rumble. The window comes back where it was; its first size fits
  the screen.
- The game's language is the DS's own, set from your computer's (or the one you pick): the
  game's text, its keyboards and the HD screens follow it.
- Fast boot: the boot's splashes, which wait for no button, go by at full speed.
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

- The pause menu's Settings in five tabs (Display, Graphics, Sound, Controls, Game), changed with
  LB / RB (Q / W), the rarely needed rows under *Advanced*, a line saying what every row does,
  `‹ ›` arrows on what Left / Right change, and Y to put a tab back to its defaults (it asks
  first, on No). Layout is no longer the first row. Battery saver dims the two rows it holds and
  shows what it holds them at; Language says it applies at the next start. Keys and buttons is
  in the Controls tab too, and says the pause menu can have one button of its own.
- Clearer names: *Screen scaling* (was Picture filter), *Game icons* (was Game pictures), *Pause in
  the background* (was Run in background, the other way round), *Buzz on connect* (was Rumble),
  fast forward *Hold / Toggle*, and the real microphone reads "Real microphone".
- The game's icons and portraits in the HD menus are drawn with the DS's own pixels by default.
  Settings kept from an earlier version move to Pixels once (they all said Smooth, the old
  default); Smooth stays a choice in *Game icons*, and once chosen again it stays.
- With *Game icons* on Pixels, the game's icons stay crisp at any window size: they are enlarged
  from the DS's own pixels, never from a copy already enlarged and then blurred again.
- Easier to read from the sofa: the focused row is a deep blue with white words (its words were
  white on light lavender), the focus ring has a light line inside the green (a dark one on light
  buttons) so it shows without its colour, notes and headings are white, and no text is smaller than 18 points at 100 % (27
  pixels on a 1080p TV), the buttons' names in their chips (START + BACK, LB, keys) included.
- The launch panel keeps out of the screen's outer edge, where some TVs cut the picture.
- The D-pad chips that mean Up / Down or Left / Right show two arrowheads (˄ ˅, ‹ ›) instead of a
  bar that read as a plus or a minus.
- The town's menus, Give Gift, notes and the phone: the game's small pictures at two sizes only
  (buttons and arrows smaller, items and the menu's tabs larger), each pixel a whole block of
  screen pixels at any window size. The counted items in Pockets and the phone button are the
  game's own pictures (no more dark squares or blue band around them).
- Portraits (Interact, Give Gift, Goals, Relationships, conversations, the hotel's Registry and
  room page) without the DS's bevelled corners around them.
- Every button hint says what it does: Up / Down "Choose" (Goals, Relationships, the phone), Left /
  Right "Adjust" on a volume and "Other target" on Interact, A "Continue" in a conversation.
- The phone's highlighted row has the same focus ring as every other menu; the world's prompt,
  Relationships' labels and Interact's neighbouring names are easier to read, and buttons that do
  nothing now (Use, Trash, Give Gift) look like the shops' greyed ones.
- The shops, the hotel, the front end, Create-a-Sim and the stat panel draw the game's small
  pictures at one of two sizes, the same for every item of a kind: a shop's items no longer mix
  big and small ones, and they stay crisp at any window size.
- The stat panel's pages sit on white paper, as the game drew its pictures for, in dark ink; its
  tabs name the pages and its X sits beside the title.
- One look for every list's arrows (the shop, Build Rooms, the music panel): round buttons with a
  drawn arrow. DS Wireless's icon is drawn sharp, and the shop's greyed words are readable.
- Every button hint says what it does: the D-pad's (Choose, Scroll, Pages, Adjust), Create-a-Sim's
  (Turn, Select) and the concept art's (View).
- The front end's Settings name their four pages in the tab bar, as the stat panel does; the concept
  art's cells show the art without the DS's frame; DS Wireless is greyed the way the shop greys.
- The minigames' numbers and button bar sit on dark wells and are never smaller than 18 points:
  the cards' line, MooGoo's players and the long German words fit without shrinking further.
- The vacuum on a controller: A takes a find for you. The hand goes to the find in the green ring
  (the one nearest the hand; the right stick moves the hand), presses where no dust lies over it,
  carries it to the funnel at the bowl's bottom and lets go when the funnel is ready, so it is
  kept and the filter never clogs. With no find in the bowl A does nothing. The hint said to drag
  finds to the box: the game only keeps what is dropped in the funnel. On a controller the D-pad always drives now (the keyboard still moves the hand with S and the arrows).
- Alien Autopsy on a controller: with the hand, A takes a freed organ to the tray by itself; with
  the tweezers, A takes the nearest parasite (an eating one first) to the flask. The laser still
  cuts with A held.
- Create-a-Sim draws the game's own pictures from your copy of the game: the page tabs as round
  buttons, ♀ / ♂ (the one not chosen grey), the arrows, the clothes and their categories, and the
  aspirations twice their size, the ones not chosen properly dimmed.
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
- Questions (Quit? Overwrite this save? a minigame's) open on No: two quick presses of A never say
  yes. In Create-a-Sim's last page, the first B only moves to the cross.
- The quick save state (F5 / F7) moved from beside the ROM to the settings folder; F12 saves the
  window as you see it, at full resolution, named by the date and time, in `screenshots/` there.
- The DS buttons' keys follow their position on any keyboard layout (AZERTY: B on W, Y on Q, L on
  A, R on Z).
- DS Wireless is greyed on the main menu: it needs a second DS.
- The game pauses and goes quiet when its window is not in front (Settings > Run in background).
  The Steam overlay does not pause it.
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
