#include "settings.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace sims2wide
{
std::string Settings::DefaultPath()
{
    char* dir = SDL_GetPrefPath("", "sims2-ds-wide");
    if (!dir) return "";
    std::string f = std::string(dir) + "settings.ini";
    SDL_free(dir);
    return f;
}

static const char* MicKey(MicMode m) { return m == MicMode::Off ? "off" : m == MicMode::Real ? "real" : "blow"; }
// ui_scale=: "auto", or a whole number of percent (rounded to 5, held to 60-150); anything else
// (garbage, empty, a number with words after it) is Auto.
static int UiScaleKey(const std::string& v)
{
    if (v.empty() || v.size() > 6 || !std::all_of(v.begin(), v.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) return 0;
    return std::clamp((atoi(v.c_str()) + 2) / 5 * 5, 60, 150);
}
static const char* ScalingKey(Scaling c) { return c == Scaling::Smooth ? "smooth" : c == Scaling::Nearest ? "nearest" : "sharp"; }

int Res3DStepIndex(int n)
{
    int i = 0;
    while (i + 1 < kRes3DStepCount && kRes3DSteps[i + 1] <= n) i++;
    return i;
}

int Res3DStep(int n) { return kRes3DSteps[Res3DStepIndex(n)]; }

int LanguageOfLocale(const std::string& l)
{
    const std::string p = l.substr(0, 2);
    return p == "fr" ? 2 : p == "de" ? 3 : p == "it" ? 4 : p == "es" ? 5 : 1;
}

void FitWindow(int usableW, int usableH, bool known, int& w, int& h)
{
    if (usableW <= 0 || usableH <= 0) return;
    const int maxW = usableW * 85 / 100, maxH = usableH * 85 / 100;
    if (!known)
    {
        // 16:9: 1280 x 720 when it fits, else as large as 85 % of the screen holds.
        w = 1280;
        h = 720;
    }
    if (w <= maxW && h <= maxH) return;
    const double k = std::min((double)maxW / w, (double)maxH / h);
    w = std::max(320, (int)(w * k) & ~1);
    h = std::max(240, (int)(h * k) & ~1);
}

void FitPlace(int bx, int by, int bw, int bh, int w, int h, int& x, int& y)
{
    if (bw <= 0 || bh <= 0) return;
    x = std::max(bx, std::min(x, bx + bw - w));
    y = std::max(by, std::min(y, by + bh - h));
}

bool Settings::Load(const std::string& path)
{
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    bool hasLayout = false, hasScaling = false;
    int version = 0; // no settings_version line: a file from before 2
    while (std::getline(in, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        const bool on = v == "1" || v == "on" || v == "true";
        if (controls.Apply(k, v)) continue;
        if (k == "mic") mic = v == "off" ? MicMode::Off : v == "real" ? MicMode::Real : MicMode::BlowKey;
        else if (k == "settings_version") version = atoi(v.c_str());
        else if (k == "layout")
        {
            hasLayout = true;
            for (Layout l : {Layout::Console, Layout::Auto, Layout::Focus, Layout::Side, Layout::Stacked})
                if (v == LayoutName(l)) layout = l;
        }
        else if (k == "resolution_3d") resolution3D = Res3DStep(atoi(v.c_str())); // (one of the row's steps: else Right went elsewhere)
        else if (k == "widescreen") widescreen = on;
        else if (k == "interpolate_60") interpolate60 = on;
        else if (k == "fullscreen") fullscreen = on;
        // (Before the scaling setting: on was smooth; off, the old default, gets the new one.)
        else if (k == "smooth_scaling") { if (on && !hasScaling) scaling = Scaling::Smooth; }
        else if (k == "scaling")
        {
            hasScaling = true;
            scaling = v == "smooth" ? Scaling::Smooth : v == "nearest" ? Scaling::Nearest : Scaling::Sharp;
        }
        else if (k == "battery_saver") batterySaver = on;
        else if (k == "ui_scale") uiScale = UiScaleKey(v);
        else if (k == "game_pictures") smoothPictures = v != "pixel";
        else if (k == "mute") mute = on;
        else if (k == "run_in_background") runInBackground = on;
        else if (k == "fast_forward_speed") { const int n = atoi(v.c_str()); fastSpeed = n >= 2 ? std::min(n, 4) : 0; }
        else if (k == "fast_forward_toggle") fastToggle = on;
        else if (k == "fast_boot") fastBoot = on;
        else if (k == "hide_pointer") hidePointer = on;
        else if (k == "cursor_speed") cursorSpeed = std::clamp((atoi(v.c_str()) + 12) / 25 * 25, 50, 200); // (the row's steps of 25)
        else if (k == "left_handed") leftHanded = on;
        else if (k == "window_position")
        {
            const size_t c = v.find(',');
            if (c != std::string::npos)
            {
                windowX = std::clamp(atoi(v.c_str()), -16384, 16384);
                windowY = std::clamp(atoi(v.c_str() + c + 1), -16384, 16384);
            }
        }
        else if (k == "volume") volume = std::clamp(atoi(v.c_str()), 0, 100);
        else if (k == "fast_forward_sound") fastSound = on;
        else if (k == "stick_cursor") stickCursor = on;
        else if (k == "stick_dead_zone") stickDeadZone = std::clamp(atoi(v.c_str()), 5, 60);
        else if (k == "trigger_threshold") triggerThreshold = std::clamp(atoi(v.c_str()), 10, 90);
        else if (k == "rumble") rumble = on;
        else if (k == "language") language = std::clamp(atoi(v.c_str()), 0, 5);
        else if (k == "window")
        {
            const size_t x = v.find('x');
            if (x != std::string::npos)
            {
                windowW = std::clamp(atoi(v.c_str()), 320, 16384);
                windowH = std::clamp(atoi(v.c_str() + x + 1), 240, 16384);
                windowKnown = true;
            }
        }
        else fprintf(stderr, "settings: unknown key %s\n", k.c_str());
    }
    // A file from before CONSOLE (no layout= line) whose looks are exactly what v0.1.1 wrote
    // by default (1x, 4:3, 30 fps) was never changed by hand: it gets the new defaults.
    if (!hasLayout && resolution3D == 1 && !widescreen && !interpolate60)
    {
        const Settings d;
        resolution3D = d.resolution3D;
        widescreen = d.widescreen;
        interpolate60 = d.interpolate60;
    }
    // layout= is written since CONSOLE became the default (after v0.1.1, which never wrote it);
    // a file without a settings_version that says auto comes from a build before version 2:
    // by the owner's choice it starts in CONSOLE, once (the next save writes version 2 and the
    // file then says what the player chose).
    if (version < 2 && layout == Layout::Auto) layout = Layout::Console;
    // Version 3 (the ux round): the game's pictures default to Pixel. Every file before it wrote
    // game_pictures=smooth, the old default, whether chosen or not: it loads as Pixel, once (the
    // next save writes version 3, so a Smooth chosen after that stays).
    if (version < 3) smoothPictures = false;
    return true;
}

bool Settings::Save(const std::string& path) const
{
    if (path.empty()) return false;
    std::ofstream out(path);
    if (!out) return false;
    out << "# The Sims 2 DS, one screen: settings. Changed from the pause menu (Start + Back, or Esc).\n"
        << "settings_version=3\n"
        << "mic=" << MicKey(mic) << "\n"
        << "layout=" << LayoutName(layout) << "\n"
        << "resolution_3d=" << resolution3D << "\n"
        << "widescreen=" << (widescreen ? 1 : 0) << "\n"
        << "interpolate_60=" << (interpolate60 ? 1 : 0) << "\n"
        << "fullscreen=" << (fullscreen ? 1 : 0) << "\n"
        << "scaling=" << ScalingKey(scaling) << "\n"
        << "battery_saver=" << (batterySaver ? 1 : 0) << "\n"
        << "ui_scale=" << (uiScale == 0 ? std::string("auto") : std::to_string(uiScale)) << "\n"
        << "game_pictures=" << (smoothPictures ? "smooth" : "pixel") << "\n"
        << "window=" << windowW << "x" << windowH << "\n"
        << "window_position=" << windowX << "," << windowY << "\n"
        << "volume=" << volume << "\n"
        << "mute=" << (mute ? 1 : 0) << "\n"
        << "fast_forward_sound=" << (fastSound ? 1 : 0) << "\n"
        << "run_in_background=" << (runInBackground ? 1 : 0) << "\n"
        << "fast_forward_speed=" << fastSpeed << "\n"
        << "fast_forward_toggle=" << (fastToggle ? 1 : 0) << "\n"
        << "fast_boot=" << (fastBoot ? 1 : 0) << "\n"
        << "hide_pointer=" << (hidePointer ? 1 : 0) << "\n"
        << "stick_cursor=" << (stickCursor ? 1 : 0) << "\n"
        << "cursor_speed=" << cursorSpeed << "\n"
        << "stick_dead_zone=" << stickDeadZone << "\n"
        << "left_handed=" << (leftHanded ? 1 : 0) << "\n"
        << "trigger_threshold=" << triggerThreshold << "\n"
        << "rumble=" << (rumble ? 1 : 0) << "\n"
        << "language=" << language << "\n"
        << controls.Lines();
    return out.good();
}

float AutoViewMm(float d)
{
    // (diagonal in inches, distance in mm)
    static const float kTable[][2] = {{7, 350}, {13, 500}, {24, 700}, {32, 800}, {43, 2000}, {55, 2500}};
    const float in = d / 25.4f;
    if (in <= kTable[0][0]) return kTable[0][1];
    for (size_t i = 1; i < sizeof kTable / sizeof kTable[0]; i++)
        if (in <= kTable[i][0])
        {
            const float t = (in - kTable[i - 1][0]) / (kTable[i][0] - kTable[i - 1][0]);
            return kTable[i - 1][1] + t * (kTable[i][1] - kTable[i - 1][1]);
        }
    return kTable[5][1];
}

float AutoUiScale(float mmPerPixel, float diagonalMm)
{
    // Unknown, or not a display's (a projector's or a TV's made-up sizes: under 4" or over 120").
    if (!(mmPerPixel > 0.02f && mmPerPixel < 3.0f) || !(diagonalMm > 100 && diagonalMm < 3050)) return 1;
    const float cap = 30 * 0.705f * mmPerPixel;                     // the dialogue's capitals at 100 %, mm
    const float want = AutoViewMm(diagonalMm) * std::tan(0.53f * 3.14159265f / 180); // 0.53 degrees there
    const float z = std::round(want / cap * 20) / 20;
    return std::clamp(z, 0.6f, 1.5f);
}
}
