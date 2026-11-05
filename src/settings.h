// The player's settings: one place for every option, whoever uses it. The comfort work loads and
// saves them (settings.ini in SDL's per-user folder, settings.cpp) and shows them in the settings
// page; the other parts only read them. Add a field here with its default, then its line in
// settings.cpp's Load and Save and its row in menu.cpp; never a setting of your own elsewhere.
#pragma once
#include <string>

#include "controls.h"
#include "layout.h"

namespace sims2wide
{
enum class MicMode { Off, BlowKey, Real }; // Real: the computer's microphone (asks permission)

struct Settings
{
    // Playable
    MicMode mic = MicMode::BlowKey;

    // Looks: on by default, for CONSOLE; headless runs never load settings and start with them
    // off (main.cpp), so the checks' output stays the same.
    Layout layout = Layout::Console; // the window's layout (--layout overrides it for one run)
    int resolution3D = 2;      // 1 = DS (192 lines), 2, 3… times sharper
    bool widescreen = true;    // 16:9 world when the game's 3D allows it
    bool interpolate60 = true; // 60 fps in-between frames

    // Comfort
    bool fullscreen = false;    // borderless, the desktop's size (F11)
    bool smoothScaling = false; // linear instead of nearest when scaling the screens
    int windowW = 1280, windowH = 720; // the window's size when it last closed
    int volume = 100;           // 0 to 100
    bool fastSound = false;     // sound during fast forward (at normal pitch, skipping); off: muted
    bool stickCursor = true;    // a controller's right stick moves a cursor on the touch screen
    Controls::Bindings controls;

    // settings.ini in SDL's per-user folder ("" when SDL has none).
    static std::string DefaultPath();
    // false: no file yet (the defaults stay) or cannot write.
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
};
}
