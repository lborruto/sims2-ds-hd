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
    // The DS's L / R as the world's camera: the keyboard's L / R keys, or a pad input the player
    // gave L / R himself (Controls::Bindings::PadOwn). lb / rb above stay every input of L / R:
    // the menus' pages and tabs; in the world the shoulders' default no longer turns (the right
    // stick does: navigator.h).
    bool turnL = false, turnR = false;
    // A stick cursor exists (a controller and the setting on): Unknown gives A to it. Without one
    // A stays the DS's there (the keyboard alone must still answer a Save or a Quit question).
    bool cursor = false;
    // The pointer's speed (Settings::cursorSpeed: 1 the port's own); never the stylus's drags' cap.
    float speed = 1.0f;
};
}
