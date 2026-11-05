#include "settings.h"

#include <SDL.h>

#include <algorithm>
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

bool Settings::Load(const std::string& path)
{
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    bool hasLayout = false;
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
        else if (k == "resolution_3d") resolution3D = std::clamp(atoi(v.c_str()), 1, 8);
        else if (k == "widescreen") widescreen = on;
        else if (k == "interpolate_60") interpolate60 = on;
        else if (k == "fullscreen") fullscreen = on;
        else if (k == "smooth_scaling") smoothScaling = on;
        else if (k == "volume") volume = std::clamp(atoi(v.c_str()), 0, 100);
        else if (k == "fast_forward_sound") fastSound = on;
        else if (k == "stick_cursor") stickCursor = on;
        else if (k == "window")
        {
            const size_t x = v.find('x');
            if (x != std::string::npos)
            {
                windowW = std::clamp(atoi(v.c_str()), 320, 16384);
                windowH = std::clamp(atoi(v.c_str() + x + 1), 240, 16384);
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
    return true;
}

bool Settings::Save(const std::string& path) const
{
    if (path.empty()) return false;
    std::ofstream out(path);
    if (!out) return false;
    out << "# The Sims 2 DS, one screen: settings. Changed from the pause menu (Esc).\n"
        << "settings_version=2\n"
        << "mic=" << MicKey(mic) << "\n"
        << "layout=" << LayoutName(layout) << "\n"
        << "resolution_3d=" << resolution3D << "\n"
        << "widescreen=" << (widescreen ? 1 : 0) << "\n"
        << "interpolate_60=" << (interpolate60 ? 1 : 0) << "\n"
        << "fullscreen=" << (fullscreen ? 1 : 0) << "\n"
        << "smooth_scaling=" << (smoothScaling ? 1 : 0) << "\n"
        << "window=" << windowW << "x" << windowH << "\n"
        << "volume=" << volume << "\n"
        << "fast_forward_sound=" << (fastSound ? 1 : 0) << "\n"
        << "stick_cursor=" << (stickCursor ? 1 : 0) << "\n"
        << controls.Lines();
    return out.good();
}
}
