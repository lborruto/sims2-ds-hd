#include "menu.h"

#include <algorithm>
#include <cstdio>

#include "state.h"

namespace sims2wide
{
enum Id
{
    M_RESUME, M_SAVESTATE, M_LOADSTATE, M_SETTINGS, M_QUIT,
    M_DISPLAY, M_GRAPHICS, M_SOUND, M_CONTROLS, M_BACK,
    M_FULLSCREEN, M_SMOOTH, M_LAYOUT,
    M_RES3D, M_WIDE, M_FPS60,
    M_VOLUME, M_FFSOUND,
    M_STICK, M_MIC, M_KEYS, M_RESET,
    M_ACTION0 // + the action
};

static std::string OnOff(bool b) { return b ? "ON" : "OFF"; }

void PauseMenu::Toggle(App& app, const Input& input)
{
    if (open) { Close(app); return; }
    open = true;
    app.fast = false;
    app.micBlow = false;
    GoPage(MAIN, M_RESUME, app, input);
}

void PauseMenu::Close(App& app)
{
    open = false;
    capturing = -1;
    if (!pending.empty()) { Say(pending, frameNow); pending.clear(); }
    (void)app;
}

void PauseMenu::Say(const std::string& msg, long frame)
{
    toast = msg;
    toastUntil = frame + 120;
}

void PauseMenu::Build(const App& app, const Input& input)
{
    const Settings& s = app.settings;
    menu.items.clear();
    ids.clear();
    auto add = [&](int id, const std::string& label, const std::string& value = "", const std::string& hint = "") {
        menu.items.push_back({label, value, hint});
        ids.push_back(id);
    };
    switch (page)
    {
    case MAIN:
        add(M_RESUME, "RESUME");
        add(M_SAVESTATE, "SAVE STATE", "", "THE QUICK SAVE STATE, BESIDE THE ROM");
        add(M_LOADSTATE, "LOAD STATE", "", "BACK TO THE QUICK SAVE STATE");
        add(M_SETTINGS, "SETTINGS");
        add(M_QUIT, "QUIT", "", "THE GAME'S OWN SAVE IS ALREADY ON DISK");
        break;
    case SETTINGS:
        add(M_DISPLAY, "DISPLAY", "", "FULLSCREEN, SCALING, LAYOUT");
        add(M_GRAPHICS, "GRAPHICS", "", "SHARPER 3D, WIDESCREEN, 60 FPS");
        add(M_SOUND, "SOUND", "", "VOLUME, SOUND IN FAST FORWARD");
        add(M_CONTROLS, "CONTROLS", "", "KEYS, CONTROLLER, TOUCH CURSOR, MICROPHONE");
        add(M_BACK, "BACK");
        break;
    case DISPLAY:
        add(M_FULLSCREEN, "FULLSCREEN", OnOff(s.fullscreen), "THE WHOLE SCREEN, NO BORDER (F11)");
        add(M_SMOOTH, "SMOOTH SCALING", OnOff(s.smoothScaling), "SOFTER PIXELS WHEN SCALED");
        add(M_LAYOUT, "LAYOUT", app.layout == Layout::Auto ? "AUTO" : app.layout == Layout::Focus ? "FOCUS" : app.layout == Layout::Side ? "SIDE" : app.layout == Layout::Console ? "CONSOLE" : "STACKED",
            "AUTO: FOCUS IN TOWN, SIDE ELSEWHERE (KEYS 0-3)");
        add(M_BACK, "BACK");
        break;
    case GRAPHICS:
        add(M_RES3D, "3D RESOLUTION", std::to_string(s.resolution3D) + "X" + (s.resolution3D == 1 ? " (DS)" : ""), "THE 3D WORLD DRAWN SHARPER");
        add(M_WIDE, "WIDESCREEN WORLD", OnOff(s.widescreen), "16:9 WORLD WHERE THE GAME'S 3D ALLOWS");
        add(M_FPS60, "60 FPS", OnOff(s.interpolate60), "IN-BETWEEN FRAMES FOR SMOOTHER MOTION");
        add(M_BACK, "BACK");
        break;
    case SOUND:
        add(M_VOLUME, "VOLUME", std::to_string(s.volume), "0 TO 100");
        add(M_FFSOUND, "FAST FORWARD SOUND", s.fastSound ? "ON" : "MUTED", "ON: NORMAL PITCH, SKIPPING AHEAD");
        add(M_BACK, "BACK");
        break;
    case CONTROLS:
        add(M_STICK, "STICK TOUCH CURSOR", OnOff(s.stickCursor), "RIGHT STICK MOVES, CURSOR PRESS TOUCHES");
        add(M_MIC, "MICROPHONE", s.mic == MicMode::Off ? "OFF" : s.mic == MicMode::Real ? "MICROPHONE" : "BLOW KEY",
            "BLOW KEY: THE BLOW KEY. MICROPHONE: THE REAL ONE (ASKS)");
        add(M_KEYS, "KEYS AND BUTTONS", "", "CHANGE WHAT EACH KEY AND BUTTON DOES");
        add(M_BACK, "BACK");
        break;
    case KEYS:
        for (int a = 0; a < Controls::COUNT; a++)
        {
            std::string v = capturing == a ? "PRESS A KEY OR BUTTON"
                : Controls::KeyName(s.controls.Key(a)) + " / " + Controls::InputName(s.controls.Pad(a), input.padStyle);
            add(M_ACTION0 + a, Controls::ActionName(a), v, "ENTER: CHANGE. ESC: KEEP. BACKSPACE: NO KEY");
        }
        add(M_RESET, "RESET TO DEFAULTS");
        add(M_BACK, "BACK");
        break;
    }
}

void PauseMenu::GoPage(Page p, int selectId, const App& app, const Input& input)
{
    page = p;
    Build(app, input);
    menu.selected = 0;
    menu.scroll = 0;
    for (size_t i = 0; i < ids.size(); i++)
        if (ids[i] == selectId) menu.selected = (int)i;
}

void PauseMenu::Frame(App& app, Video& video, Input& input, const std::string& settingsPath)
{
    Settings& s = app.settings;
    auto save = [&]() { if (!settingsPath.empty() && !s.Save(settingsPath)) fprintf(stderr, "cannot write %s\n", settingsPath.c_str()); };
    if (input.wantMenu) { Close(app); return; }

    if (capturing >= 0)
    {
        // The next key or controller input goes to the action; Esc keeps what it had.
        bool done = false;
        if (input.newKey != Controls::kDefault) { s.controls.SetKey(capturing, input.newKey); done = true; }
        else if (input.newPad >= 0) { s.controls.SetPad(capturing, input.newPad); done = true; }
        else if (input.captureCancel || SDL_GetTicks() > captureUntil) done = true;
        if (done)
        {
            capturing = -1;
            input.capturing = false;
            save();
            Build(app, input);
        }
        return;
    }

    MenuInput in = input.nav;
    // A click beside the box goes back (closes the menu on the main page).
    if (in.click && menu.ItemAt(in.mx, in.my) == -2) { in.back = true; in.click = false; }
    int dir = 0;
    const int hit = menu.Update(in, dir);
    if (in.back || (hit >= 0 && ids[hit] == M_BACK))
    {
        switch (page)
        {
        case MAIN: Close(app); break;
        case SETTINGS: GoPage(MAIN, M_SETTINGS, app, input); break;
        case KEYS: GoPage(CONTROLS, M_KEYS, app, input); break;
        default: GoPage(SETTINGS, page == DISPLAY ? M_DISPLAY : page == GRAPHICS ? M_GRAPHICS : page == SOUND ? M_SOUND : M_CONTROLS, app, input); break;
        }
        return;
    }
    if (hit < 0) return;
    const int id = ids[hit];
    bool changed = true;
    switch (id)
    {
    case M_RESUME: Close(app); return;
    case M_SAVESTATE:
        pending = SaveState(*app.nds, app.quickState) ? "STATE SAVED" : "STATE NOT SAVED";
        Close(app);
        return;
    case M_LOADSTATE:
        pending = LoadState(*app.nds, app.quickState) ? "STATE LOADED" : "NO STATE TO LOAD";
        Close(app);
        return;
    case M_QUIT: app.quit = true; return;
    case M_SETTINGS: GoPage(SETTINGS, M_DISPLAY, app, input); return;
    case M_DISPLAY: GoPage(DISPLAY, M_FULLSCREEN, app, input); return;
    case M_GRAPHICS: GoPage(GRAPHICS, M_RES3D, app, input); return;
    case M_SOUND: GoPage(SOUND, M_VOLUME, app, input); return;
    case M_CONTROLS: GoPage(CONTROLS, M_STICK, app, input); return;
    case M_KEYS: GoPage(KEYS, M_ACTION0, app, input); return;
    case M_FULLSCREEN: s.fullscreen = !s.fullscreen; video.SetFullscreen(s.fullscreen); break;
    case M_SMOOTH: s.smoothScaling = !s.smoothScaling; video.SetSmooth(s.smoothScaling); break;
    case M_LAYOUT:
    {
        static const Layout order[] = {Layout::Auto, Layout::Focus, Layout::Side, Layout::Stacked};
        int i = 0; // (CONSOLE, from --layout, is not in the cycle yet: it goes on from AUTO)
        while (i < 4 && order[i] != app.layout) i++;
        if (i == 4) i = 0;
        app.layout = order[(i + 4 + (dir < 0 ? -1 : 1)) % 4];
        changed = false;
        break;
    }
    case M_RES3D: s.resolution3D = std::clamp(s.resolution3D + (dir < 0 ? -1 : 1), 1, 4); break;
    case M_WIDE: s.widescreen = !s.widescreen; break;
    case M_FPS60: s.interpolate60 = !s.interpolate60; break;
    case M_VOLUME: s.volume = std::clamp(s.volume + (dir < 0 ? -10 : 10), 0, 100); break;
    case M_FFSOUND: s.fastSound = !s.fastSound; break;
    case M_STICK: s.stickCursor = !s.stickCursor; break;
    case M_MIC:
    {
        const int m = ((int)s.mic + 3 + (dir < 0 ? -1 : 1)) % 3;
        s.mic = (MicMode)m;
        break;
    }
    case M_RESET: s.controls.Reset(); break;
    default:
        if (id >= M_ACTION0)
        {
            capturing = id - M_ACTION0;
            captureUntil = SDL_GetTicks() + 5000;
            input.capturing = true;
            changed = false;
        }
        break;
    }
    if (changed) save();
    Build(app, input);
}

bool PauseMenu::Draw(const App& app, const Video& video, const Input& input, long frame)
{
    frameNow = frame;
    int ow, oh;
    video.OutputSize(ow, oh);
    canvas.Resize(ow, oh);
    canvas.Clear();
    const int sc = canvas.Scale();
    bool used = false;
    int cx, cy, screenW;
    // (Where the piece showing that point draws it; hidden when no piece shows it.)
    if (!open && frame < input.cursorShownUntil && video.TouchToOutput(app, input.cursorX, input.cursorY, cx, cy, screenW))
    {
        // The stick cursor: a cross on the touch screen, yellow while pressing.
        const int arm = screenW / 40 + 3, t = oh / 360 + 1;
        const u32 col = input.cursorPress ? 0xFFFFD75E : 0xFFFFFFFF;
        canvas.Fill(cx - arm - 1, cy - t - 1, arm * 2 + 3, t * 2 + 3, 0xFF000000);
        canvas.Fill(cx - t - 1, cy - arm - 1, t * 2 + 3, arm * 2 + 3, 0xFF000000);
        canvas.Fill(cx - arm, cy - t, arm * 2 + 1, t * 2 + 1, col);
        canvas.Fill(cx - t, cy - arm, t * 2 + 1, arm * 2 + 1, col);
        used = true;
    }
    if (!open && app.fast)
    {
        canvas.Text(ow - Canvas::TextWidth(">>", sc) - 4 * sc, 4 * sc, ">>", sc, 0xFFFFFFFF);
        used = true;
    }
    if (!open && frame < toastUntil)
    {
        const int tw = Canvas::TextWidth(toast, sc) + 8 * sc, x = (ow - tw) / 2, y = oh - 20 * sc;
        canvas.Fill(x, y, tw, 13 * sc, 0xC0000000);
        canvas.Text(x + 4 * sc, y + 3 * sc, toast, sc, 0xFFFFFFFF);
        used = true;
    }
    if (open)
    {
        static const char* titles[] = {"PAUSED", "SETTINGS", "DISPLAY", "GRAPHICS", "SOUND", "CONTROLS", "KEYS AND BUTTONS"};
        static const char* hints[] = {"ESC: BACK TO THE GAME", "ESC: BACK", "ESC: BACK", "ESC: BACK", "ESC: BACK", "ESC: BACK", "ESC: BACK"};
        menu.Draw(canvas, titles[page], hints[page]);
        used = true;
    }
    return used;
}
}
