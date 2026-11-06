// The player's settings: one place for every option, whoever uses it. The comfort work loads and
// saves them (settings.ini in SDL's per-user folder, settings.cpp) and shows them in the settings
// page; the other parts only read them. Add a field here with its default, then its line in
// settings.cpp's Load and Save and its row in shell.cpp; never a setting of your own elsewhere.
#pragma once
#include <string>

#include "controls.h"
#include "layout.h"

namespace sims2wide
{
enum class MicMode { Off, BlowKey, Real }; // Real: the computer's microphone (asks permission)
// How the window scales the DS's pictures: Sharp (nearest to the next whole factor, then smooth to
// the size: crisp pixels without uneven ones), Smooth (bilinear) or Nearest (raw pixels).
enum class Scaling { Sharp, Smooth, Nearest };

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
    Scaling scaling = Scaling::Sharp;
    // The HD interface's size: 0 Auto (from the window's physical size, AutoUiScale), else 75 to
    // 150 percent by 5. Headless: 100 % unless --ui-scale (settings are not loaded there).
    int uiScale = 0;
    bool batterySaver = false;  // the looks at their lightest (3D 1x, 30 fps) without changing their settings
    int windowW = 1280, windowH = 720; // the window's size when it last closed
    bool windowKnown = false;   // (not saved) the file gave window=: else the size comes from the screen
    int windowX = -1, windowY = -1;    // its place when it last closed (-1: centred)
    int volume = 100;           // 0 to 100, by 5
    bool mute = false;
    bool fastSound = false;     // sound during fast forward (at normal pitch, skipping); off: muted
    bool runInBackground = false; // off: the game pauses (and is silent) while the window is not in front
    int fastSpeed = 0;          // fast forward: 0 as fast as the computer goes, else 2..4 times
    bool fastToggle = false;    // the fast forward key: held (off) or pressed once on, once off
    bool fastBoot = true;       // the boot's splashes (which take no touch) run at full speed
    bool hidePointer = true;    // the mouse pointer hidden after a few seconds still
    bool stickCursor = true;    // a controller's right stick moves a cursor on the touch screen
    int cursorSpeed = 100;      // the stick cursor's and the minigames' pointer's speed, percent (50 to 200)
    int stickDeadZone = 25;     // percent of a stick's travel read as centred (5 to 60)
    bool leftHanded = false;    // the two sticks swapped
    int triggerThreshold = 50;  // percent of a trigger's travel past which it is pressed (10 to 90)
    bool rumble = true;         // a short buzz when a controller connects (pads with a motor)
    // The game's language (the DS firmware's, which the game reads when it starts, and the strings
    // the HD screens read) and the HD words': 0 automatic (the computer's), 1 English .. 5 Spanish.
    // A change applies the next time the game starts.
    int language = 0;
    Controls::Bindings controls;

    // settings.ini in SDL's per-user folder ("" when SDL has none).
    static std::string DefaultPath();
    // The looks with the battery saver: what the window shows (the settings keep the player's).
    int Res3D() const { return batterySaver ? 1 : resolution3D; }
    bool Interpolate60() const { return batterySaver ? false : interpolate60; }
    // false: no file yet (the defaults stay) or cannot write.
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
};

// The window's first size on a screen whose usable area (without the task bar or the menu bar) is
// usableW x usableH: 16:9, at most 85 % of it, 1280 x 720 when that fits, never past it; a size
// remembered from another screen made to fit the same way (w, h in and out; aspect kept).
void FitWindow(int usableW, int usableH, bool known, int& w, int& h);
// A remembered place (x, y in and out) moved so that a w x h window lies within the usable area
// (bx, by, bw, bh) when it can: a size made at this start, or shrunk, never runs off the screen.
void FitPlace(int bx, int by, int bw, int bh, int w, int h, int& x, int& y);
// The UI's size Auto picks (0.75 to 1.5 by 0.05) for a canvas mmPerPixel millimetres a canvas
// pixel wide on a display diagonalMm across: the dialogue's capitals (30 px, Nunito's cap height
// 0.705 em) as large as 0.53 degrees of sight at the distance a display that size is seen from
// (AutoViewMm); 1 when either is unknown (0) or not believable.
float AutoUiScale(float mmPerPixel, float diagonalMm);
// The distance (mm) a display diagonalMm across is seen from: 35 cm at 7", 50 at 13", 70 at 24",
// 80 at 32", 2 m at 43", 2.5 m from 55"; straight lines between.
float AutoViewMm(float diagonalMm);
// A UI size setting to its zoom: 0 (Auto) gives `autoScale`, else percent / 100.
inline float UiZoomOf(int setting, float autoScale) { return setting == 0 ? autoScale : setting / 100.0f; }

// The language a locale names ("fr_FR", "de"…): 1-5, else 1 (English).
int LanguageOfLocale(const std::string& locale);
}
