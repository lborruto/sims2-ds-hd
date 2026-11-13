// Remappable controls: which keyboard key and which controller button does each of the DS's
// buttons and the frontend's own actions. Changed in the pause menu's CONTROLS page and kept in
// settings.ini as key_<action>=<SDL keycode> and pad_<action>=<input> lines.
//
// Adapted from Pokémon FullDex (GPLv3), frontend/controls.h; modified for this project, 2026.
// https://github.com/mamesj/pokemon-fulldex
// The bindings form a permutation: giving an action a key that another one had swaps the two, so
// no key ever does two things and none is lost.
#pragma once
#include <string>

namespace sims2hd
{
namespace Controls
{
// The 12 DS buttons first, in the order of DSKey (common.h), then the frontend's actions.
enum Action
{
    A, B, SELECT, START, RIGHT, LEFT, UP, DOWN, R, L, X, Y,
    MENU,      // opens and closes the pause menu
    FAST,      // fast forward while held
    SWAP,      // swaps which screen is big (FOCUS)
    SAVESTATE, // the quick save state
    LOADSTATE,
    BLOW,      // blow into the microphone while held (App::micBlow)
    TOUCH,     // presses the stick touch cursor while held
    PANEL,     // CONSOLE: opens and closes the touch screen's menu (navigator.h); MENU stays the pause menu
    COUNT
};

// Controller inputs: SDL_GameControllerButton's order, then the two triggers, which count as
// pressed past half way.
enum PadInput
{
    IN_A, IN_B, IN_X, IN_Y, IN_BACK, IN_GUIDE, IN_START, IN_LS, IN_RS, IN_LB, IN_RB,
    IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_LT, IN_RT, IN_COUNT
};

// A controller's buttons as printed on it: Xbox's (and most others'), PlayStation's or Nintendo's.
enum Style { XBOX, PLAYSTATION, NINTENDO };

// A binding is a keycode or an Input, or one of these two.
constexpr int kDefault = -1, kNone = -2;

const char* ActionKey(int a);  // in the settings file
const char* ActionName(int a); // in the menu (capitals: the menu's font)
const char* InputKey(int i);
int InputFromKey(const std::string& s);
const char* InputName(int i, int style);
int DefaultKey(int a); // an SDL keycode (SDLK_*); the DS buttons A/B/X/Y/L/R by position (any layout)
int DefaultPad(int a); // a PadInput, or kNone
// A key's name in the menu's font (capitals, digits and : - + . / < > ! ( ) only).
std::string KeyName(int keycode);
// What a key does in the pause menu and the launch panel (`action`: the key's, ActionOfKey):
// the arrows move, Enter / Space / the DS's A key accept; the MENU key (Esc by default) resumes
// before anything but the arrows takes it (even bound to Enter, Space or the DS's A key), so Esc
// resumes from every page; Esc (when it is not MENU),
// Backspace and the DS's B key go back; Delete / X clears, Y, the DS's L / R keys the tabs.
enum class MenuKey { None, Up, Down, Left, Right, A, B, X, Y, Lb, Rb, Menu };
MenuKey MenuKeyOf(int keycode, int action);

struct Bindings
{
    int key[COUNT]; // a keycode, kNone, or kDefault
    int pad[COUNT]; // a PadInput, kNone, or kDefault

    Bindings() { Reset(); }
    void Reset()
    {
        for (int a = 0; a < COUNT; a++) key[a] = pad[a] = kDefault;
    }
    int Key(int a) const { return key[a] == kDefault ? DefaultKey(a) : key[a]; }
    int Pad(int a) const { return pad[a] == kDefault ? DefaultPad(a) : pad[a]; }
    // The player gave `a` a pad input of his own: not its default (a file that names the default,
    // `pad_l=lb` from an older build or a capture of the same button, counts as the default).
    // In the world the DS's L / R turn the camera only on such an input: the shoulders' default
    // (LB / RB) is the menus' pages there, and the right stick turns (navigator.h).
    bool PadOwn(int a) const { return pad[a] != kDefault && pad[a] != DefaultPad(a); }
    // The world's camera turns (PadState::turnL / turnR, what Input::Actions sets): the keyboard's
    // L / R keys held (keyL / keyR), a pad's L / R input held (padL / padR) only when it is the
    // player's own (PadOwn): the shoulders' default is the menus' pages.
    void CameraTurns(bool keyL, bool keyR, bool padL, bool padR, bool& turnL, bool& turnR) const
    {
        turnL = keyL || (PadOwn(L) && padL);
        turnR = keyR || (PadOwn(R) && padR);
    }
    bool Custom() const
    {
        for (int a = 0; a < COUNT; a++)
            if (key[a] != kDefault || pad[a] != kDefault) return true;
        return false;
    }
    // The action a key or a controller input does, or -1.
    int ActionOfKey(int keycode) const
    {
        if (keycode < 0) return -1;
        for (int a = 0; a < COUNT; a++)
            if (Key(a) == keycode) return a;
        return -1;
    }
    int ActionOfPad(int in) const
    {
        if (in < 0) return -1;
        for (int a = 0; a < COUNT; a++)
            if (Pad(a) == in) return a;
        return -1;
    }
    // Gives `a` the key `keycode`; the action that had it gets a's old one.
    void SetKey(int a, int keycode)
    {
        const int old = Key(a);
        for (int b = 0; b < COUNT; b++)
            if (b != a && Key(b) == keycode) key[b] = old;
        key[a] = keycode;
    }
    void SetPad(int a, int in)
    {
        const int old = Pad(a);
        for (int b = 0; b < COUNT; b++)
            if (b != a && Pad(b) == in) pad[b] = old;
        pad[a] = in;
    }
    // One line of the settings file; whether it was one of these.
    bool Apply(const std::string& k, const std::string& v);
    // The keys given again through the swap, from the defaults, once the keyboard's keymap is
    // known (after SDL's video is up): the settings load before it, when the defaults asked by
    // position (A, B, X, Y, L, R, TOUCH MENU) are still 0 and cannot be swapped out of the way.
    void ResolveKeys();
    // The lines for the settings file (only what differs from the defaults).
    std::string Lines() const;
};
}
}
