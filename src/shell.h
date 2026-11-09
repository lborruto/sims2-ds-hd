// The HD shell: the port's own screens around the game, on the UI kit (ui/kit.h) with the port's
// own words (port/ownwords.h) in five languages and the chips of the device used last.
//   The pause menu (Start + Back on a pad, Esc): Resume, Settings (five tabs, LB / RB: Display,
//   Graphics, Sound, Controls, Game; rows with ‹ value › pills, Left / Right or A change them, a note
//   under the list for the focused row, an Advanced group per tab, Y resets the tab after asking,
//   focus on No), Controls (the controls card, and Keys and buttons: the bindings,
//   A captures one, X clears it, Y captures them all in turn), Save states (four slots with their
//   picture and date: A loads a used slot after asking, with focus on No, and keeps the state
//   before the load for Undo; A on an empty slot or X saves) and Quit (asked, focus on No: the
//   host then writes a suspend state the launch panel offers as Continue).
//   The launch panel, before the DS starts: Continue with the last ROM (and where it was left,
//   when a suspend state is kept), Choose a file, the recent ROMs, Language, Quit, the controls
//   card; a message (a wrong file, another cartridge) stays until A.
// Everything here is a function of a Model and the input: no SDL, no emulator. The host
// (main.cpp) fills the model, carries out the Result and draws Build()'s items on a 1280 x 720
// canvas (the window over the game; --compose's pictures; tests/shell_test.cpp).
#pragma once
#include <string>
#include <vector>

#include "controls.h"
#include "layout.h"
#include "settings.h"
#include "slots.h"
#include "ui/kit.h"

namespace sims2wide::shell
{
constexpr int kW = 1280, kH = 720;

enum class Page { Main, Settings, Controls, Keys, States, Launch };
// A question over the page (two buttons, No first and focused), or a message (one, OK).
enum class Ask { None, Quit, Load, Overwrite, Message, ResetTab };

// One frame's presses (edges), and the mouse in canvas pixels.
struct Input
{
    bool up = false, down = false, left = false, right = false, a = false, b = false, x = false, y = false;
    bool lb = false, rb = false; // the previous / next tab (the DS's L and R: a pad's LB / RB, Q / W)
    bool hover = false, click = false; // the pointer moved / the left button went down
    float mx = 0, my = 0;
};

// What the host does.
enum class Act
{
    None,
    Resume,        // close the pause menu
    Quit,          // the quit question's Yes: write the suspend state and stop
    SaveSlot,      // arg: the slot
    LoadSlot,      // arg: the slot (asked: Yes)
    Undo,          // back to the state kept before the last load
    Capture,       // arg: the action whose binding the next key or button takes
    ClearBinding,  // arg: the action (the keyboard's key, or the controller's button: the device used last)
    ResetBindings,
    Setting,       // arg: the Row changed (settings already changed in the model's Settings)
    ResetTab,      // arg: the Tab whose rows went back to their defaults (in the model's Settings): apply each
    Continue,      // launch: the last ROM (with its suspend state, when kept)
    OpenRecent,    // launch: arg the index in Model::recents
    ChooseFile,    // launch: the file picker
    Exit,          // launch: stop the program
    ChangeGame,    // back to the launch panel (the suspend state written first, as Quit)
    OpenFolder,    // the saves' folder in the system's file browser
};
struct Result
{
    Act act = Act::None;
    int arg = 0;
};

// The settings' rows (each in one tab: TabRows). Keys opens the bindings (no value).
enum class Row
{
    Layout, Fullscreen, Scaling, UiSize, Pictures, Res3D, Wide, Fps60, Battery,
    Volume, Mute, FastSound, Background,
    FastSpeed, FastKey, FastBoot,
    HidePointer, StickCursor, CursorSpeed, DeadZone, LeftHanded, Trigger, Rumble,
    Mic, Language, Keys, Count
};
constexpr int kRows = (int)Row::Count;
// The settings' tabs, LB / RB (wrapping).
enum class Tab { Display, Graphics, Sound, Controls, Game, Count };
constexpr int kTabs = (int)Tab::Count;
struct TabLayout
{
    std::vector<Row> rows; // in order: the everyday ones, then from `advanced` on, under the Advanced heading
    int advanced;          // rows.size(): none
};
const TabLayout& TabRows(int tab);
std::string TabName(int tab, int language);
// Battery saver forces the row's look (3D 1x, 30 fps): the row is dimmed, shows the forced value
// and does not change.
bool RowHeld(Row r, const Settings& s);
// The tab's rows back to Settings{}'s values (not the window's size and place, not the bindings).
void ResetTabRows(int tab, Settings& s);

struct Model
{
    int language = 1;                 // the words' language, 1 English .. 5 Spanish
    Settings* settings = nullptr;     // the rows change it in place (the host saves it and applies what must be)
    // The controls card and the bindings: how each action's key and button are printed
    // (glyphs as ui::ChipItemAs takes them; "" none), the controller's face.
    std::string keyGlyph[Controls::COUNT], padGlyph[Controls::COUNT];
    ui::Face padFace = ui::Face::Xbox;
    bool keyboard = true;             // the keyboard was used last (X clears its key, else the button)
    int capturing = -1, captureLeft = 0; // the action being captured, seconds left
    // The save states.
    bool statesOk = false;
    Thumb slots[kStateSlots];         // w == 0: empty
    bool used[kStateSlots] = {};
    bool canUndo = false;
    std::string otherRom[kStateSlots]; // a slot made with another ROM file: its name (the load asks so)
    bool canOpenFolder = false;       // the window, with a folder: "Open the saves folder"
    // The launch panel: the recent ROMs' file names (the first is Continue's), its suspend state.
    std::vector<std::string> recents;
    bool hasSuspend = false;
    Thumb suspend;
    // A message over the page until A ("" none).
    std::string message;
    // A line kept on the pause menu's main page ("" none): the save written elsewhere than beside the ROM.
    std::string banner;
};

class Shell
{
public:
    bool open = false;
    Page page = Page::Main;
    Ask ask = Ask::None;
    int focus = 0, scroll = 0, askFocus = 0, askSlot = 0;
    int tab = 0;           // Settings: the tab shown (kept while the program runs)
    Page keysFrom = Page::Controls; // Keys: where B goes back to (the Controls page, or Settings' Controls tab)
    bool sequence = false; // Keys: every action in turn (Y)

    void Open(Page p);
    void Close() { open = false; ask = Ask::None; sequence = false; }
    void AskQuit() { ask = Ask::Quit; askFocus = 0; }
    Result Update(const Input& in, Model& m);
    // A capture ended (cancelled: the binding kept): the next one of a sequence, or none.
    Result CaptureDone(bool cancelled);
    std::vector<ui::UiItem> Build(const Model& m) const;
    // The page's items (focusable or not), for tests.
    int Count(const Model& m) const;
    bool Focusable(const Model& m, int i) const;
    // Where each item was drawn by the last Build (the mouse; tests).
    const std::vector<Rect>& Rects() const { return rects_; }
    const std::vector<Rect>& TabRects() const { return tabRects_; } // (Settings: the tab bar)

private:
    mutable std::vector<Rect> rects_, askRects_, tabRects_;
    mutable Rect panel_ {};
    void Zoom(std::vector<ui::UiItem>& items) const; // the UI's size (Build's last step)
    int hover_ = -2;
    void Go(Page p, int f);
    void Move(const Model& m, int dir);
};

// The settings rows: a change by dir (-1, +1), the label and the value as shown.
void ChangeRow(Row r, int dir, Settings& s);
std::string RowLabel(Row r, int language);
std::string RowValue(Row r, const Settings& s, int language);
// The row's note under the list while it has the focus (an OwnWordId; every row has one).
int RowNote(Row r);
// The language the shell's words use: the setting's, else the given automatic one.
inline int WordsLanguage(const Settings& s, int automatic) { return s.language >= 1 && s.language <= 5 ? s.language : automatic; }
// The launch panel's entries, in order (Continue only with a recent ROM).
enum class LaunchItem { Continue, Choose, Recent, Language, Quit };
struct LaunchEntry { LaunchItem kind; int arg; };
std::vector<LaunchEntry> LaunchEntries(const Model& m);
// An action's name in the bindings page.
std::string ActionLabel(int action, int language);
// The pause menu's main page, in order (the saves' folder only with Model::canOpenFolder).
enum class MainItem { Resume, Settings, Controls, States, ChangeGame, OpenFolder, Quit };
std::vector<MainItem> MainEntries(const Model& m);
// The pad in the shell (the pause menu and the launch panel): accept is A's own input, else (A
// cleared) the pad's south (east when B owns south); back is B's own input, else east, else (east
// is accept) south while no action owns it, else none (the keyboard's Esc). One press is one
// shell action: X / Y on the accept or back input do nothing there. So a cleared A or B never
// locks the menu (checked in shell_test).
struct PadIns { int accept = -1, back = -1; };
PadIns ShellPadIns(const Controls::Bindings& b);
struct PadNav { bool a = false, b = false, x = false, y = false, lb = false, rb = false; };
PadNav ShellPadNav(const Controls::Bindings& b, const bool* now, const bool* before);
}
