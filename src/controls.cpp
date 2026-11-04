#include "controls.h"

#include <SDL.h>

#include <cstdlib>
#include <cstring>

namespace sims2wide
{
namespace Controls
{
const char* ActionKey(int a)
{
    static const char* k[COUNT] = {"a", "b", "select", "start", "right", "left", "up", "down", "r", "l", "x", "y",
                                   "menu", "fast", "swap", "savestate", "loadstate", "blow", "touch"};
    return a >= 0 && a < COUNT ? k[a] : "";
}

const char* ActionName(int a)
{
    static const char* n[COUNT] = {"A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L", "X", "Y",
                                   "MENU", "FAST FORWARD", "SWAP SCREENS", "SAVE STATE", "LOAD STATE", "BLOW (MIC)", "CURSOR PRESS"};
    return a >= 0 && a < COUNT ? n[a] : "";
}

const char* InputKey(int i)
{
    static const char* k[IN_COUNT] = {"a", "b", "x", "y", "back", "guide", "start", "ls", "rs", "lb", "rb",
                                      "up", "down", "left", "right", "lt", "rt"};
    return i >= 0 && i < IN_COUNT ? k[i] : "";
}

int InputFromKey(const std::string& s)
{
    for (int i = 0; i < IN_COUNT; i++)
        if (s == InputKey(i)) return i;
    return -1;
}

// As printed on the controller. SDL gives A to the button labelled A on every kind of pad (the
// bottom one on Xbox and PlayStation, the right one on Nintendo's), so each pad's own
// convention (bottom confirms on Xbox, A on the right confirms on a Switch) is the DS's.
const char* InputName(int i, int style)
{
    static const char* xbox[IN_COUNT] = {"A", "B", "X", "Y", "BACK", "GUIDE", "START", "LEFT STICK", "RIGHT STICK", "LB", "RB",
                                         "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT"};
    static const char* ps[IN_COUNT] = {"CROSS", "CIRCLE", "SQUARE", "TRIANGLE", "SHARE", "PS", "OPTIONS", "L3", "R3", "L1", "R1",
                                       "UP", "DOWN", "LEFT", "RIGHT", "L2", "R2"};
    static const char* nin[IN_COUNT] = {"A", "B", "X", "Y", "-", "HOME", "+", "LEFT STICK", "RIGHT STICK", "L", "R",
                                        "UP", "DOWN", "LEFT", "RIGHT", "ZL", "ZR"};
    if (i < 0 || i >= IN_COUNT) return "-";
    return style == PLAYSTATION ? ps[i] : style == NINTENDO ? nin[i] : xbox[i];
}

int DefaultKey(int a)
{
    switch (a)
    {
    case A: return SDLK_x;
    case B: return SDLK_z;
    case X: return SDLK_s;
    case Y: return SDLK_a;
    case L: return SDLK_q;
    case R: return SDLK_w;
    case START: return SDLK_RETURN;
    case SELECT: return SDLK_RSHIFT;
    case UP: return SDLK_UP;
    case DOWN: return SDLK_DOWN;
    case LEFT: return SDLK_LEFT;
    case RIGHT: return SDLK_RIGHT;
    case MENU: return SDLK_ESCAPE;
    case FAST: return SDLK_SPACE;
    case SWAP: return SDLK_TAB;
    case SAVESTATE: return SDLK_F5;
    case LOADSTATE: return SDLK_F7;
    case BLOW: return SDLK_m;
    default: return kNone; // TOUCH: the mouse does it
    }
}

int DefaultPad(int a)
{
    switch (a)
    {
    case A: return IN_A;
    case B: return IN_B;
    case X: return IN_X;
    case Y: return IN_Y;
    case L: return IN_LB;
    case R: return IN_RB;
    case START: return IN_START;
    case SELECT: return IN_BACK;
    case UP: return IN_UP;
    case DOWN: return IN_DOWN;
    case LEFT: return IN_LEFT;
    case RIGHT: return IN_RIGHT;
    case MENU: return IN_GUIDE;
    case FAST: return IN_RT;
    case SWAP: return IN_RS;
    case TOUCH: return IN_LT;
    default: return kNone; // SAVESTATE, LOADSTATE, BLOW: none until given one
    }
}

std::string KeyName(int keycode)
{
    switch (keycode)
    {
    case kNone: return "-";
    case SDLK_RETURN: return "ENTER";
    case SDLK_ESCAPE: return "ESC";
    case SDLK_COMMA: return "COMMA";
    case SDLK_SEMICOLON: return "SEMICOLON";
    case SDLK_QUOTE: return "QUOTE";
    case SDLK_LEFTBRACKET: return "LEFT BRACKET";
    case SDLK_RIGHTBRACKET: return "RIGHT BRACKET";
    case SDLK_BACKSLASH: return "BACKSLASH";
    case SDLK_EQUALS: return "EQUALS";
    case SDLK_BACKQUOTE: return "GRAVE";
    case SDLK_RSHIFT: return "RIGHT SHIFT";
    case SDLK_LSHIFT: return "LEFT SHIFT";
    default: break;
    }
    const char* n = SDL_GetKeyName((SDL_Keycode)keycode);
    std::string s;
    for (const char* p = n; p && *p; p++)
    {
        char c = *p;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || (c && strchr(":-+./<>!()", c));
        s += ok ? c : ' ';
    }
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s.empty() ? "KEY " + std::to_string(keycode) : s;
}

bool Bindings::Apply(const std::string& k, const std::string& v)
{
    const bool isKey = !k.compare(0, 4, "key_"), isPad = !k.compare(0, 4, "pad_");
    if (!isKey && !isPad) return false;
    const std::string name = k.substr(4);
    for (int a = 0; a < COUNT; a++)
        if (name == ActionKey(a))
        {
            if (isKey) { const long kc = atol(v.c_str()); key[a] = kc > 0 ? (int)kc : kNone; }
            else { const int in = InputFromKey(v); pad[a] = in >= 0 ? in : kNone; }
        }
    return true;
}

std::string Bindings::Lines() const
{
    std::string s;
    for (int a = 0; a < COUNT; a++)
    {
        if (key[a] != kDefault) s += "key_" + std::string(ActionKey(a)) + "=" + std::to_string(key[a] == kNone ? 0 : key[a]) + "\n";
        if (pad[a] != kDefault) s += "pad_" + std::string(ActionKey(a)) + "=" + (pad[a] == kNone ? "none" : InputKey(pad[a])) + "\n";
    }
    return s;
}
}
}
