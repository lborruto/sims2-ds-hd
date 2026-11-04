// Software-drawn overlay: the pause menu, the touch cursor and short messages, drawn into an ARGB
// buffer the size of the window's output and put over the game by video.cpp. Capitals, digits
// and a few signs only: a 5 x 7 bitmap font, scaled by whole pixels.
//
// Adapted from Pokémon FullDex's frontend/ui.h (GPLv3, https://github.com/mamesj/pokemon-fulldex).
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace sims2wide
{
struct Canvas
{
    int w = 0, h = 0;
    std::vector<u32> px; // ARGB; alpha 0 means "show the game underneath"

    void Resize(int nw, int nh);
    // The rectangle drawn into since the last Clear: only that part is cleared and uploaded.
    int x0d = 0, y0d = 0, x1d = 0, y1d = 0;
    bool Dirty() const { return x1d > x0d && y1d > y0d; }
    void Clear();
    void Fill(int x, int y, int rw, int rh, u32 argb);
    static int TextWidth(const std::string& s, int scale) { return (int)s.size() * 6 * scale; }
    void Text(int x, int y, const std::string& s, int scale, u32 argb);
    // The font's scale for this output height (1 at 240 lines, 3 at 720).
    int Scale() const { return h / 240 < 1 ? 1 : h / 240; }
};

// What the menu understands, edge-triggered (one frame each).
struct MenuInput
{
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
    // The mouse: where it is in output pixels (hover chooses the row) and whether it clicked.
    bool hover = false, click = false;
    int mx = 0, my = 0;
};

struct MenuItem
{
    std::string label;
    std::string value; // shown as "< value >", right-aligned; empty for plain actions
    std::string hint;  // one line under the box while the row is chosen
};

struct Menu
{
    int selected = 0;
    std::vector<MenuItem> items;

    // The row activated this frame, or -1; dir is -1 / +1 when left / right was used on a row
    // with a value, 0 for accept. The mouse chooses a row by moving onto it (a pointer resting
    // on the menu does not fight the keyboard); a click on a row activates it (the left third of
    // the box: left, the rest: right or accept); a click outside the box is back.
    int Update(const MenuInput& in, int& dir);
    int hoverItem = -2; // the row the pointer was last seen on

    // Where the box and its rows were last drawn, in the canvas's pixels (for the mouse).
    mutable int drawnX = 0, drawnY = 0, drawnW = 0, drawnH = 0, drawnLineH = 0, drawnFirstY = 0;
    mutable int scroll = 0;
    // The row at a canvas position: its index, -1 for the box but no row, -2 for outside the box.
    int ItemAt(int x, int y) const;

    // The chosen row's hint, or the page's.
    void Draw(Canvas& c, const std::string& title, const std::string& hint) const;
};
}
