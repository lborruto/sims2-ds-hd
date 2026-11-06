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
#include "ui/kit.h"

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

// Each view module's ids start at its own tag in the top bits, so an id never repeats across tabs
// or screens (the navigator's armed X and the focus's resume are kept by id).
constexpr u32 kModInteract = 1u << 24, kModPockets = 2u << 24, kModGift = 3u << 24, kModGoals = 4u << 24,
              kModRelations = 5u << 24, kModPhone = 6u << 24, kModOptions = 7u << 24, kModPopup = 8u << 24,
              kModNote = 9u << 24, kModCatalogue = 10u << 24;

// ViewItem::next's "no move that way".
constexpr int kNoMove = -2;

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
    // X asks first (a destructive X the game itself does not confirm: Pockets' bin): the first X
    // on the item only arms it (and runs xArm: the game's selection tapped there), the second X
    // on the armed item runs buttonX; a focus move, a tab change, the item going or the menu
    // closing disarms it (the navigator's Armed()).
    bool xAsks = false;
    MenuAction xArm;
    // Explicit focus moves, item indices for Up, Down, Left, Right (-1: the nearest by geometry,
    // MenuNeighbour): a link the geometry would leave to chance (the panel's phone button above
    // the tab's first row and back). kNoMove: none that way (the focus stays: past a list's
    // edge, the list's own scroll).
    int next[4] = {-1, -1, -1, -1};
    // The word A's hint shows while it has the focus (the game's own, e.g. the row's verb); empty:
    // the chip alone.
    std::string word;
    // A performs a tap that closes the screen (a popup's X): A and B then wait for the screen to
    // change (the navigator's closing wait), as after the phone's X.
    bool closes = false;
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
    // focus is held on that item while it is in the list (scrollGroup), and every Up / Down there
    // performs scrollUp / scrollDown (the game's own buttons move its selection; the rebuilt view
    // follows it); an empty one (the game's arrow does nothing that way) is a plain focus move
    // (Up from the first entry: the phone). -1: the navigator's focus.
    int selected = -1;
    // The tab's title, in the game's words (the frame draws it).
    std::string title;
    // A press the gate held while the game was not settled is replayed once it settles (true), or
    // dropped (false: a screen where a late tap would do harm, the Save slots).
    bool replay = true;
    // A screen of its own (Give Gift, the Save slots, the phone, a popup, a note): the navigator
    // takes it as a focus screen whatever the screen state says (the panel's menu: only while open).
    bool own = false;
    // A popup over the screen (port/popup.h): it wins the focus, and when it goes the screen under
    // it comes back as it was (the panel's menu open on the same row: the navigator's resume).
    bool popup = false;
    // B (`back`) closes the screen (the phone's X, a popup's): A and B then wait as after `closes`.
    bool backCloses = false;
    // DS buttons passed on while held (Game Paused's Start), whatever the focus.
    u32 passKeys = 0;
    // (Drawing only, not hashed: set by CONSOLE from the navigator) the id of the item whose X is
    // armed (xAsks), 0 none.
    u32 armed = 0;

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
// `next` when it is focusable (kNoMove: `from`), else MenuNeighbour; `from` when nothing lies
// that way.
int MenuMove(const MenuView& v, int from, int dir);
// The focus after a rebuild: the enabled item with `id` when there is one; else, `old` clamped
// to the list, the enabled item at that index (the one that took a removed item's place), else
// the nearest enabled one, the one before before the one after at the same distance; -1 when
// none is enabled.
int MenuKeep(const MenuView& v, u32 id, int old);
// The UI items from `first` on in ascending still layers, each layer in the order given, the
// images after them (as LayersAt draws them): what every HD screen does last.
void SortUi(std::vector<ui::UiItem>& items, size_t first = 0);
// Whether `i` is a focusable item of v.
inline bool MenuFocusable(const MenuView& v, int i) { return i >= 0 && i < (int)v.items.size() && v.items[i].enabled; }
}
