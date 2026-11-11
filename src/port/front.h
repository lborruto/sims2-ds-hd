// The front end's first screens in HD (milestone 5): the boot screens, the intro video, the title
// and the main menu, driven by the controller (GameState::front, research m5-front-end §1, §5).
//
// Boot (CBGDisplayMenu in the bottom slot, the intro video CVideoScreenMenu in the top one from
// the first splash on): the video large at the left, framed, and the game's own splash (the
// legal text, EA, Maxis, "Touch the Touch Screen to continue.") beside it 1.5x, as the game draws
// them: live pictures, both. Only the last splash (mode 0) waits for a touch, and that touch also
// ends the video and opens the title: there A or Start tap the splash (an A and a START chip
// under it); the earlier splashes take no touch (measured: a tap during them changes nothing), so
// they have no item: the DS's own Start and A are passed on there (MenuView::passKeys), as before
// the view. A popup splash (mode 4) has no view (milestone 1's picture).
//
// The title (CTitleSceenMenu phase 0, "Touch the Touch Screen to begin"): the game's own top
// screen (its backdrop and logo) as large as the canvas holds it, an A chip with the game's blue
// ▶ (sprite 2506) at its foot; A taps the game's big button (the game advances on any release).
// The game's table has no "Press A" (its only words there say "touch"): the chip alone.
//
// The main menu (phases 1-4): the top screen at the left, framed, and the game's four buttons as
// large HD buttons in a panel at the right, in the game's order and words (6 Create-a-Sim, 7
// Load-a-Sim, 39 Settings, 9 DS Wireless), each with the game's ▶ (2506 blue, 2503 green on the
// focus, the game's pressed look) and DS Wireless with its icon (8769); Up / Down the focus, A the
// game's tap on the button (182, 34 + 40 i). The game draws and fades the buttons in over phases
// 1-2 and is dead after a choice (phase 4): the buttons are drawn but disabled then (no focus, A
// dropped), so no press lands mid-fade. DS Wireless opens the game's own Wireless screen (out of
// scope beyond it: a partner DS); Settings, Load-a-Sim and Wireless are later tasks' screens or
// milestone 1's, whose B (the game's own) goes back to the title at phase 0.
//
// Mouse: the title's whole canvas maps onto the game's big button; each HD button onto the
// game's button box; the boot's splash is the touch screen itself.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

constexpr u32 kModFront = 16u << 24;
constexpr u32 kFrTitleId = kModFront + 1, kFrBootId = kModFront + 2, kFrMenuId = kModFront + 0x10; // + the button 0..3

// The game's touch points (DS bottom pixels): the title's big button (its round face), the main
// menu's button boxes (x 170..195, y 22 + 40 i, 26 high; the tap at their middle), the splash.
constexpr Rect kFrBigButtonFrom {64, 41, 128, 128};
constexpr int kFrBigX = 128, kFrBigY = 105;
constexpr int kFrMenuButtons = 4, kFrMenuBoxX = 170, kFrMenuBoxY0 = 22, kFrMenuBoxDy = 40, kFrMenuBoxW = 26, kFrMenuBoxH = 26;
constexpr int kFrSplashX = 128, kFrSplashY = 100;
// How long A / Start hold the splash's touch: while the intro video plays the splash's object
// updates only every 4 to 6 frames (the video's decoding slows its loop), so the touch script's
// usual 3-frame tap fell between two of its polls about one press in four (pad-boot-late); 8
// frames always span one.
constexpr int kFrSplashHold = 8;
// The game's strings: the four buttons' labels.
constexpr int kStrFrMenu[kFrMenuButtons] = {6, 7, 39, 9};
constexpr int kFrWireless = 3; // DS Wireless: greyed (a second DS), its note in the hints
// The game's sprites (descriptors): the buttons' ▶ (normal, pressed), the DS Wireless icon.
constexpr int kSprFrArrow = 2506, kSprFrArrowOn = 2503, kSprFrWireless = 8769;

// HD geometry (canvas): the menu's and the boot's top screen box (the picture fitted at its shape),
// the menu's panel and its buttons, the boot's splash (the touch screen 1.5x) and its hints' row.
constexpr Rect kFrTopBox {24, 72, 768, 576};   // 3x the DS's 4:3
constexpr Rect kFrPanel {816, 48, 440, 624};
constexpr int kFrButtonX = kFrPanel.x + 24, kFrButtonW = kFrPanel.w - 48, kFrButtonY0 = 84, kFrButtonDy = 128, kFrButtonH = 100;
constexpr Rect kFrHints {kFrPanel.x + 24, kFrPanel.y + kFrPanel.h - 64, kFrPanel.w - 48, 40};
constexpr Rect kFrNote {kFrHints.x, kFrHints.y - 16, kFrHints.w, kFrHints.h + 20}; // DS Wireless's note (between the last button and the panel's foot), on a well
constexpr Rect FrNoteText() { return {kFrNote.x + 12, kFrNote.y + 4, kFrNote.w - 24, kFrNote.h - 8}; }   // (its words inside the well)
constexpr int kFrSafeFoot = 684;   // the title's chip plate ends here: the canvas's 5 % safe area (720 - 36)
constexpr Rect kFrSplash {838, 216, 384, 288};
constexpr float kFrLabelSize = 34, kFrLabelMin = 24;
constexpr int kFrArrow = 64, kFrIconW = 56;   // the ▶'s box at the button's right end; DS Wireless's icon before its label
// The HD button i (canvas).
constexpr Rect FrontButtonRect(int i) { return {kFrButtonX, kFrButtonY0 + i * kFrButtonDy, kFrButtonW, kFrButtonH}; }

// Whether the front end shows one of this file's screens, and its view (valid false otherwise:
// milestone 1's front-end picture): the boot (a splash 0-3, 5, 6), the title, the main menu, read
// from RAM, with the game's strings. Built every frame by the navigator's caller and CONSOLE.
// The boot goes by on its own: nothing up yet, the intro video, or a splash that takes no touch
// (the legal, EA and Maxis ones, modes 1-3, and the zoom and blank between, 5-6). False from the
// last splash (mode 0, "Touch the Touch Screen"), a popup splash (4) or any other screen: fast
// boot (main.cpp) runs only while this holds, and never presses anything.
bool BootPassesBy(const GameState& g);
MenuView FrontView(ScreenState s, const GameState* game, const RomData* rom);
// The same without the strings' check (tests): rom null leaves the words empty.
MenuView FrontViewOf(ScreenState s, const GameState& game, const RomData* rom = nullptr);
// The screen drawn into c (empty: the whole canvas is this screen's), the focus on item `focus`.
void BuildFront(Composition& c, const MenuView& v, int focus, ScreenState s, const GameState& game, const RomData& rom, IconCache* icons, int topW, int topH);
// Shared with port/frontmenus.h: `w` x `h` fitted in box at its shape, centred; the top picture's
// 2D part (the DS's 4:3: the widescreen looks' black sides left out); a piece with its 6-pixel
// frame just outside it.
Rect FrontFit(Rect box, int w, int h);
Rect FrontTopFrom(int topW, int topH);
void FrontFramed(Composition& c, Piece p);
// (A check, --measure-menu) the four labels in their buttons.
void MeasureFront(const RomData& rom, BlockMeasure& m);
}
