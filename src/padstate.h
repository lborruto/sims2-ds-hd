// What the player holds this frame, as CONSOLE's navigator (navigator.h) and its activities
// (activities.h) read it.
#pragma once

namespace sims2wide
{
// What the player holds this frame, after the bindings (controls.h): the DS's buttons by their
// action, MENU the in-game menu's action (Controls::PANEL), the sticks and triggers as read.
struct PadState
{
    bool a = false, b = false, x = false, y = false, lb = false, rb = false, start = false, select = false,
         menu = false, up = false, down = false, left = false, right = false;
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    // A stick cursor exists (a controller and the setting on): Unknown gives A to it. Without one
    // A stays the DS's there (the keyboard alone must still answer a Save or a Quit question).
    bool cursor = false;
};
}
