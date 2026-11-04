// The pause menu (Esc): resume, save and load state, the settings pages (DISPLAY, GRAPHICS,
// SOUND, CONTROLS, with the key and button bindings) and quit. The game stands still while it is
// open. Also the rest of the overlay: the stick touch cursor, short messages, the fast forward mark.
#pragma once
#include <string>
#include <vector>

#include "app.h"
#include "input.h"
#include "ui.h"
#include "video.h"

namespace sims2wide
{
struct PauseMenu
{
    bool open = false;
    Canvas canvas;

    void Toggle(App& app, const Input& input);
    // One frame of the open menu: its input, the changes it makes (settings saved to
    // settingsPath, "" for none), save states, quitting.
    void Frame(App& app, Video& video, Input& input, const std::string& settingsPath);
    // Draws this frame's overlay into canvas; whether there is anything on it.
    bool Draw(const App& app, const Video& video, const Input& input, long frame);
    // A short message at the bottom of the window (two seconds of game time).
    void Say(const std::string& msg, long frame);

private:
    enum Page { MAIN, SETTINGS, DISPLAY, GRAPHICS, SOUND, CONTROLS, KEYS };
    Page page = MAIN;
    Menu menu;
    std::vector<int> ids; // the item id of each row
    int capturing = -1;   // the action whose binding is being captured, or -1
    unsigned captureUntil = 0;
    std::string toast;
    long toastUntil = 0, frameNow = 0;
    std::string pending; // a message to show once the menu has closed

    void Build(const App& app, const Input& input);
    void GoPage(Page p, int selectId, const App& app, const Input& input);
    void Close(App& app);
};
}
