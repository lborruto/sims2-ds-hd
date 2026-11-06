// The HD menus' shared model: what a redrawn screen offers the controller. Each screen module
// (port/interact.h, …) builds a MenuView from the game's state every frame: its items (an HD
// rectangle on the 1280 x 720 canvas, the game input that performs it, enabled or not, a group,
// a stable id) and the screen-wide buttons. The navigator (navigator.h) moves its focus over
// the enabled items and runs the focused item's action on A; CONSOLE (console.cpp) draws the
// same view with that focus. The game is always driven through its own input: a tap at a
// measured point of the touch screen, the DS's own buttons for some frames, a drag; never RAM.
//
// Lists longer than the view: a screen module places only the rows the game itself shows (its
// own scroll decides which), and gives the list's `scrollUp` / `scrollDown`: the game's own
// scroll (its arrow, its button). Up / Down past the list's first / last row perform it, and
// the focus follows the next row once the rebuilt view shows it, so the HD list stays in step
// with the game's. Content changing under the menu (a row used up, a target gone): the view is
// rebuilt that frame, the focus stays on the item with the same id or clamps to the nearest
// enabled one, and a press waiting for the gate is dropped when the view's Hash changed.
#pragma once
#include <string>
#include <vector>

#include "common.h"
#include "layout.h"

namespace sims2wide
{
// One step of an action, run in order over game frames by the navigator.
struct MenuStep
{
    enum Kind { Tap, Drag, Keys, Wait } kind = Tap;
    int x = 0, y = 0;     // Tap: the DS touch point; Drag: from
    int x2 = 0, y2 = 0;   // Drag: to
    u32 keys = 0;         // Keys: DS keys (1 << kA, …) held
    int frames = 0;       // Keys / Wait: how long; Tap: 0 = the touch script's default
    bool operator==(const MenuStep&) const = default;
};
using MenuAction = std::vector<MenuStep>; // empty: nothing to perform

MenuStep TapStep(int x, int y);
MenuStep DragStep(int x0, int y0, int x1, int y1);
MenuStep KeysStep(u32 keys, int frames);
MenuStep WaitStep(int frames);

struct ViewItem
{
    Rect rect {};          // canvas pixels: what the focus frames, and the geometry of focus moves
    MenuAction action;     // what A performs (enabled only)
    bool enabled = true;   // false: shown dimmed (greyed, empty), the focus skips it, A never performs it
    int group = 0;         // a list: Up / Down past its ends perform scrollUp / scrollDown (when given)
    u32 id = 0;            // stable across rebuilds (the focus stays on the same id)
    // Left / Right while this item has the focus (a slider's knob: Options' volumes): before the
    // view's screen-wide Left / Right and before a focus move; empty: as without.
    MenuAction left, right;
    // X while this item has the focus (Pockets: the bin for the focused slot), before the view's
    // screen-wide X; empty: as without.
    MenuAction buttonX;
    std::string xWord;    // its hint's word (the frame shows X with it while the item has the focus)
    // Explicit focus moves, item indices for Up, Down, Left, Right (-1: the nearest by geometry,
    // MenuNeighbour): a link the geometry would leave to chance (the panel's phone button above
    // the tab's first row and back).
    int next[4] = {-1, -1, -1, -1};
    // The word A's hint shows while it has the focus (the game's own, e.g. the row's verb); empty:
    // the chip alone.
    std::string word;
};

// Screen-wide buttons (not the focused item's).
enum class MenuButton { Left, Right, X, Y, Count };

struct MenuView
{
    bool valid = false;               // false: the screen keeps milestone 1's composition and HotspotsFor
    std::vector<ViewItem> items;
    // Left / Right, X, Y: performed whatever the focus (Interact: Left / Right the previous / next
    // target); an empty Left / Right moves the focus instead.
    MenuAction buttons[(int)MenuButton::Count];
    // B: empty closes the panel's menu (no tap), as before.
    MenuAction back;
    // The list's own scroll (see above); `scrollGroup` the group it belongs to.
    MenuAction scrollUp, scrollDown;
    int scrollGroup = 0;
    // The item focused when the screen opens (an index; -1: the first enabled one).
    int initial = -1;
    // A selection the game owns (Relationships' Sim, the phone's row): when >= 0 and enabled, the
    // focus is held on that item, and every Up / Down performs scrollUp / scrollDown (the game's
    // own buttons move its selection; the rebuilt view follows it). -1: the navigator's focus.
    int selected = -1;
    // The tab's title, in the game's words (the frame draws it).
    std::string title;

    // What the player sees and what the items do (rects, actions, enabled, ids, moves, words,
    // buttons, the selection, the title): a press held by the gate is dropped when this changes
    // before it runs.
    u64 Hash() const;
};

// The first enabled item (or `initial` when it is enabled), -1 for none.
int MenuFirst(const MenuView& v);
// The nearest enabled item from `from` in the direction (dx, dy): the item centres within 60
// degrees of it, nearest first (greyed rows are skipped as if absent); `from` when none.
int MenuNeighbour(const MenuView& v, int from, int dx, int dy);
// The focused item after a Up (0), Down (1), Left (2) or Right (3) press: the item's explicit
// `next` when it is focusable, else MenuNeighbour; `from` when nothing lies that way.
int MenuMove(const MenuView& v, int from, int dir);
// The focus after a rebuild: the enabled item with `id` when there is one; else, `old` clamped
// to the list, the enabled item at that index (the one that took a removed item's place), else
// the nearest enabled one, the one before before the one after at the same distance; -1 when
// none is enabled.
int MenuKeep(const MenuView& v, u32 id, int old);
// Whether `i` is a focusable item of v.
inline bool MenuFocusable(const MenuView& v, int i) { return i >= 0 && i < (int)v.items.size() && v.items[i].enabled; }
}
