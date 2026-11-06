// Input scripts: one event per line, "<frame> <frames held> <what>", '#' starts a comment.
// The DS's input (headless runs and the window alike): what is a key name (several joined by
// '+'), "blow" (the microphone's blow key) or "touch <x> <y>". The window's own input, for
// checking the frontend (pushed as real SDL events, ignored headless; their frame counts the
// loop's turns, paused ones too): "press <key name>" (SDL's, e.g. Escape, Return, F11, X),
// "move <x> <y>" (the mouse, in window points), "click <x> <y>" (the left button there),
// "wheel <notches>" (positive: up) and "winshot" (the window's picture to the shot folder,
// w<frame>.png). CONSOLE's navigator (headless runs and the window alike): "pad <action>", the
// action held as on a controller: a, b, x, y, l (or lb), r (or rb), start, select, menu (the
// touch menu's action, PANEL), up, down, left, right, rsleft / rsright (the right stick pushed
// fully that way); "layout <auto|focus|side|stacked|console>"
// switches the layout at that frame (as the 0-3 keys and the pause menu do).
#pragma once
#include <string>
#include <vector>

#include "common.h"
#include "navigator.h"

namespace sims2wide
{
struct InputEvent
{
    int frame, len;
    u32 keys;
    bool blow = false;
    bool touch;
    int tx, ty;
    std::string press; // a key name, or "" (window events: see above)
    std::string pad;   // a navigator action, or ""
    int layout = -1;   // a Layout, or -1

    bool move = false, click = false, winShot = false;
    int wheel = 0;
};

// Exits with a message on an unreadable file or an unknown key name.
std::vector<InputEvent> LoadScript(const std::string& path);

// The keys, blow and touch the script holds at this frame (keys OR-ed in, the others set when
// held).
void ApplyScript(const std::vector<InputEvent>& script, long frame, u32& keys, bool& blow, bool& touching, int& tx, int& ty);
// The navigator's actions the script's "pad" events hold at this frame (pad is reset first).
void ScriptPad(const std::vector<InputEvent>& script, long frame, PadState& pad);
// The layout the script switches to at this frame, or -1.
int ScriptLayout(const std::vector<InputEvent>& script, long frame);
// Whether the script asks for the window's picture at this turn of the loop.
bool ScriptWindowShot(const std::vector<InputEvent>& script, long iter);
}
