// CONSOLE: the game on one 16:9 screen. Per recognised bottom screen (screens.h), a composition
// of pieces of the two DS screens on a 1280 x 720 canvas, as in the approved mockup: in town the
// world fills the canvas with a rail of the touch screen's tab icons and phone at the right; the
// menu is the whole touch screen 3x over the dimmed world; a conversation is the world with the
// game's dialogue box rebuilt wide at the bottom; an activity is the touch screen 3x at the left
// with the world small at the top right. DS pieces are drawn as the game draws them, 2x or 3x;
// frames and the focus ring are overlays in the game's colours.
#pragma once
#include "compose.h"
#include "screens.h"

namespace sims2wide
{
constexpr int kConsoleW = 1280, kConsoleH = 720;

// The game's colours (frame, panel, outline, highlight).
constexpr u32 kFrameArgb = 0xFF828ACB, kPanelArgb = 0xFF5961A2, kOutlineArgb = 0xFF383869, kRingArgb = 0xFF309A41;
// The world behind the menu.
constexpr u32 kDimArgb = 0x8C000000;

// Town: the rail, at the canvas's right edge, vertically centred: the phone icon above the tab
// column, both 2x, in a frame.
constexpr Rect kRailFrame {1168, 130, 88, 460};
constexpr Rect kRailPhoneFrom {222, 4, 30, 30}, kRailPhoneTo {1182, 144, 60, 60};
constexpr Rect kRailTabsFrom {0, 6, 34, 184}, kRailTabsTo {1178, 208, 68, 368};

// The menu (and Popup, Gift): the whole touch screen 3x, centred.
constexpr Rect kMenuTo {256, 72, SW * 3, SH * 3};

// A conversation: the frame, and the dialogue's parts at 2x (from the DS dialogue screen).
constexpr Rect kDialogueFrame {110, 456, 1060, 256};
constexpr Rect kDialogueNameFrom {10, 6, 144, 16}, kDialogueNameTo {126, 464, 288, 32};
constexpr Rect kDialogueTextFrom {8, 26, 152, 98}, kDialogueTextTo {122, 500, 304, 196};
constexpr Rect kDialoguePortraitFrom {164, 30, 76, 90}, kDialoguePortraitTo {1002, 494, 152, 180};
constexpr Rect kDialogueArrowFrom {214, 156, 30, 30}, kDialogueArrowTo {918, 636, 60, 60};
// The text box carried on to the right of the text piece (the game's own box is narrow).
constexpr Rect kDialogueTextBox {124, 514, 776, 188};

// An activity: the touch screen 3x at the left, the world 384 wide at the top right (its height
// from its shape: 288 at the DS's 4:3), both framed; the hints' box (Task 7 writes in it).
constexpr Rect kActivityBottomTo {36, 68, SW * 3, SH * 3};
constexpr int kActivityTopX = 858, kActivityTopY = 68, kActivityTopW = 384;
constexpr Rect kHintBox {850, 400, 400, 252};

// The composition for screen s. menuOpen: the menu is up (in town: the touch screen 3x over the
// dimmed world). focus: the index in HotspotsFor(s) the focus ring frames (-1: none; no ring
// when no touchable bottom piece shows it). topW x topH: the top picture's size (App::TopImage).
// Screen::None (no HUD: title, Create-a-Sim): SIDE.
Composition BuildConsole(ScreenState s, bool menuOpen, int focus, int topW, int topH);
}
