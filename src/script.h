// Input scripts: one event per line, "<frame> <frames held> <what>", '#' starts a comment.
// The DS's input (headless runs and the window alike): what is a key name (several joined by
// '+') or "touch <x> <y>". The window's own input, for checking the frontend (pushed as real
// SDL events, ignored headless; their frame counts the loop's turns, paused ones too):
// "press <key name>" (SDL's, e.g. Escape, Return, F11, X), "move <x> <y>" (the mouse, in window
// points), "click <x> <y>" (the left button there), "wheel <notches>" (positive: up) and
// "winshot" (the window's picture to the shot folder, w<frame>.png).
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace sims2wide
{
struct InputEvent
{
    int frame, len;
    u32 keys;
    bool touch;
    int tx, ty;
    std::string press; // a key name, or "" (window events: see above)
    bool move = false, click = false, winShot = false;
    int wheel = 0;
};

// Exits with a message on an unreadable file or an unknown key name.
std::vector<InputEvent> LoadScript(const std::string& path);

// The keys and touch the script holds at this frame (keys OR-ed in, touch set when held).
void ApplyScript(const std::vector<InputEvent>& script, long frame, u32& keys, bool& touching, int& tx, int& ty);
// Whether the script asks for the window's picture at this turn of the loop.
bool ScriptWindowShot(const std::vector<InputEvent>& script, long iter);
}
