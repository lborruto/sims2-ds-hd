// The running program's shared state: the emulated DS and what the window shows. Each part
// (input, video, audio, scene) reads and changes it; main.cpp runs the frame loop.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "GPU.h"
#include "NDS.h"
#include "common.h"
#include "activities.h"
#include "consolehold.h"
#include "gamestate.h"
#include "layout.h"
#include "port/icons.h"
#include "port/prompt.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "scene.h"
#include "screens.h"
#include "settings.h"

namespace sims2hd
{
struct App
{
    std::unique_ptr<melonDS::NDS> nds;

    Layout layout = Layout::Auto;  // what the player chose (AUTO or a fixed one)
    Layout shown = Layout::Side;   // what the window shows this frame (AUTO resolved)
    bool mainIsTop = true;         // FOCUS: which screen is big
    AutoLayout autoLayout;
    GameState game;                // what the game itself says is up, read from RAM every frame (gamestate.h)
    ScreenTracker screens;         // which screen the touch screen shows (screens.h), every frame
    bool consoleMenu = false;      // CONSOLE: the menu is up (the touch screen 3x over the dimmed world)
    int consoleFocus = -1;         // CONSOLE: the focused target, an index in HotspotsFor (-1: none)
    bool consolePictureClosed = false; // CONSOLE: the view's big picture closed by B (Navigator::PictureClosed)
    bool consoleVeiled = false;    // CONSOLE: the panel's menu coming back (Navigator::Returning): the dimmed world, no flash
    int consoleWorldOnly = 0;      // CONSOLE: frames in a row on the panel without its tab / no bottom object (Input::Steer)
    u32 consoleArmed = 0;          // CONSOLE: the HD view item whose X is armed (Navigator::Armed), 0 none
    PointerView consolePointer;    // CONSOLE: a pointing minigame's pointer (the navigator's), drawn by its HD frame
    PromptSteady consolePrompt;    // CONSOLE: how long the Interact target has been steady (Input::Steer), for the world's prompt
    int consolePayout = 0;         // CONSOLE: the money's last change, for kPayoutFrames frames (Input::Steer), 0 none
    ConsoleHold consoleHold;       // CONSOLE: the picture shown, the last HD one held through the game's swaps (consolehold.h)
    // The two above follow the navigator (Input::Steer), unless --console-menu or --console-focus
    // pinned them for a check's picture.
    bool consolePinned = false;
    // (A check) the UI kit's demonstration drawn over every composition (--ui-demo).
    bool uiDemo = false;
    // CONSOLE's screen: the game's own (RAM) when it reads, else the pixel tracker's (gamestate.h).
    ScreenState ConsoleScreen() const { return ConsoleScreenOf(game, screens.Current(), HudVisible(Top()), (game.valid || game.between) && ScreensBlack(Top(), Bottom())); }
    // The game's bottom slot is not switching (true without a RAM reading): the navigator's gate.
    bool Settled() const { return !game.valid || game.settled; }

    Settings settings;

    // The ROM's game code (header 0x0C) and whether the RAM map fits it: only the European ROM
    // (ASJP) has its addresses; any other: GameState is never read (not valid, not between).
    std::string romCode;
    bool portSupported = false;
    // The game's own words for the HD status bar (clock, money, Sanity), read once at start from
    // the ROM's string table in the game's language (port/statusbar.h); not ok when the ROM's
    // data does not read: CONSOLE then keeps the framed-screen compositions (the game's own HUD).
    HudStrings hudStrings;
    // The game's strings and archive, opened once at start in the game's language (or
    // --language's): the HD conversation's words, and the archive the icons are read from
    // (kept open for the run). Null when the ROM's data does not read (or --no-romdata):
    // CONSOLE's conversation is then the framed-screen one, and there are no icons.
    std::unique_ptr<RomData> rom;
    // The game's icons from that archive, decoded and scaled as the HD menus ask (port/icons.h);
    // null with no `rom`.
    std::unique_ptr<IconCache> icons;
    // The status bar's values for CONSOLE: the game's (RAM) on the European ROM, else none.
    const HudState* Hud() const { return portSupported ? &game.hud : nullptr; }

    bool fast = false;             // fast forward held
    bool quit = false;
    bool micBlow = false;          // the "blow into the microphone" key is held
    bool micForce = false;         // CONSOLE's vacuum Y: blows whatever the mic setting

    // The shape of the top screen's picture, width / height: 4/3 on the DS; wider when the
    // widescreen world is on. Layouts size the top screen from it.
    float topAspect = 4.0f / 3.0f;

    std::string shotFile;          // F12

    // The top screen's picture at its real size when the looks are on (looks.cpp builds it after
    // each frame): taller than 192 lines with the sharper 3D, wider than 4:3 with widescreen.
    std::vector<u32> topBig;
    int topBigW = 0, topBigH = 0;

    const u32* Top() const { return nds->GPU.Framebuffer[nds->GPU.FrontBuffer][0].get(); }
    // The top screen as it should be shown: 256 x 192 (Top()) unless the looks made it bigger.
    const u32* TopImage(int& w, int& h) const
    {
        if (!topBig.empty()) { w = topBigW; h = topBigH; return topBig.data(); }
        w = SW; h = SH;
        return Top();
    }
    const u32* Bottom() const { return nds->GPU.Framebuffer[nds->GPU.FrontBuffer][1].get(); }
};
}
