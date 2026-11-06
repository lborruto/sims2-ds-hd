#include "pads.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// The bundled SDL_GameControllerDB (assets/controllers, this platform's lines), embedded at build
// time by cmake/embed.cmake.
extern const unsigned char gamecontrollerdb[];
extern const unsigned long gamecontrollerdb_size;

namespace sims2wide
{
using namespace Controls;

void Pads::Hints()
{
    // No press reaches the game while another window (Steam's overlay is the game's own window,
    // and Steam Input stops feeding the game while it is up) has the focus.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
    // Every backend on (SDL's defaults, said so they cannot be lost): Windows.Gaming.Input and
    // RawInput beside XInput and DirectInput, HIDAPI's drivers (PlayStation, Switch, 8BitDo…).
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    // SDL_HINT_GAMECONTROLLERCONFIG, SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES(_EXCEPT),
    // SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD and the button labels are Steam's and the
    // player's: never set here.
}

std::string Pads::UserMappingsPath()
{
    char* dir = SDL_GetPrefPath("", "sims2-ds-wide");
    if (!dir) return "";
    std::string f = std::string(dir) + "gamecontrollerdb.txt";
    SDL_free(dir);
    return f;
}

int Pads::AddUnmapped(const char* text, size_t size)
{
    // Line by line; a pad SDL maps already (its own database, whose Switch and 8BitDo lines
    // follow SDL_GAMECONTROLLER_USE_BUTTON_LABELS, XInput's, HIDAPI's) keeps SDL's mapping:
    // a line added through the API would replace it.
    int added = 0;
    const std::string all(text, size);
    for (size_t at = 0; at < all.size();)
    {
        size_t end = all.find('\n', at);
        if (end == std::string::npos) end = all.size();
        std::string line = all.substr(at, end - at);
        at = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t comma = line.find(',');
        if (line.empty() || line[0] == '#' || comma == std::string::npos) continue;
        const SDL_JoystickGUID guid = SDL_JoystickGetGUIDFromString(line.substr(0, comma).c_str());
        if (char* had = SDL_GameControllerMappingForGUID(guid)) { SDL_free(had); continue; }
        if (SDL_GameControllerAddMapping(line.c_str()) == 1) added++;
    }
    return added;
}

int Pads::LoadMappings(const std::string& userFile)
{
    const int bundled = AddUnmapped(reinterpret_cast<const char*>(gamecontrollerdb), gamecontrollerdb_size);
    if (!userFile.empty())
        if (SDL_RWops* f = SDL_RWFromFile(userFile.c_str(), "rb"))
        {
            // (The count is of new GUIDs only: a line over a known pad replaces its mapping.)
            if (SDL_GameControllerAddMappingsFromRW(f, 1) < 0) fprintf(stderr, "controllers: %s: %s\n", userFile.c_str(), SDL_GetError());
            else fprintf(stderr, "controllers: mappings read from %s\n", userFile.c_str());
        }
    // SDL read this file at SDL_Init with the same priority as ours: read again, it wins.
    const char* file = SDL_GetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE);
    if (file && *file) SDL_GameControllerAddMappingsFromFile(file);
    return bundled;
}

bool Pads::SteamVirtual(Uint16 vendor, Uint16 product, Uint16 version)
{
#ifdef __APPLE__
    // (Steam's virtual pad on a Mac is an Xbox 360 pad of version 0, as SDL says.)
    return vendor == 0x045E && product == 0x028E && version == 0;
#else
    (void)version;
    return vendor == 0x28DE && product == 0x11FF;
#endif
}

int Pads::StyleOf(SDL_GameControllerType t)
{
    switch (t)
    {
    case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5: return PLAYSTATION;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return NINTENDO;
    default: return XBOX; // Xbox, Steam's virtual pad (unless Steam passes the real type), Stadia, Luna, the rest
    }
}

void Pads::Open(int index, const PadOptions& o)
{
    if (!SDL_IsGameController(index)) return;
    const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
    if (Find(id)) return; // (at start: opened, then its ADDED event)
    SDL_GameController* c = SDL_GameControllerOpen(index);
    if (!c) { fprintf(stderr, "controller %d: %s\n", index, SDL_GetError()); return; }
    SDL_Joystick* j = SDL_GameControllerGetJoystick(c);
    // From the GUID, not SDL_JoystickGetVendor / Product: with Steam's SteamVirtualGamepadInfo
    // those give the real pad's ids behind the virtual one; the GUID keeps the virtual pad's.
    Uint16 vendor = 0, product = 0, version = 0;
    SDL_GetJoystickGUIDInfo(SDL_JoystickGetGUID(j), &vendor, &product, &version, nullptr);
    const bool steam = SteamVirtual(vendor, product, version);
    // Steam's virtual pad in a game Steam did not launch with Steam Input (its desktop
    // configuration, mapping the pad to a mouse and keys): a twin of a physical pad. SDL's own
    // drivers leave it out the same way; this keeps it out whatever driver reports it.
    if (steam && !SDL_GetHintBoolean("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", SDL_FALSE))
    {
        SDL_GameControllerClose(c);
        return;
    }
    const SDL_GameControllerType type = SDL_GameControllerGetType(c);
    pads_.push_back({c, SDL_JoystickInstanceID(j), steam, StyleOf(type), type});
    const char* name = SDL_GameControllerName(c);
    connected_.push_back({name ? name : "", StyleOf(type), type});
    fprintf(stderr, "controller: %s%s\n", name ? name : "?", steam ? " (Steam Input)" : "");
    if (o.rumble && SDL_GameControllerHasRumble(c)) SDL_GameControllerRumble(c, 0x3000, 0x6000, 150);
}

void Pads::Close(SDL_JoystickID id)
{
    for (size_t i = 0; i < pads_.size(); i++)
        if (pads_[i].id == id)
        {
            SDL_GameControllerClose(pads_[i].c);
            pads_.erase(pads_.begin() + i);
            break;
        }
    if (active_ == id) active_ = -1;
}

void Pads::OpenAll(const PadOptions& o)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) Open(i, o);
}

void Pads::CloseAll()
{
    for (const Pad& p : pads_) SDL_GameControllerClose(p.c);
    pads_.clear();
    active_ = -1;
}

const Pads::Pad* Pads::Find(SDL_JoystickID id) const
{
    for (const Pad& p : pads_)
        if (p.id == id) return &p;
    return nullptr;
}

bool Pads::Usable(const Pad& p) const
{
    // Steam Input's virtual pad stands for the physical ones it wraps: while one is open, read
    // only those (Steam normally hides the physical pads itself: SDL_GAMECONTROLLER_IGNORE_DEVICES).
    if (p.steam) return true;
    for (const Pad& q : pads_)
        if (q.steam) return false;
    return true;
}

const Pads::Pad* Pads::Driving() const
{
    if (const Pad* p = Find(active_); p && Usable(*p)) return p;
    for (auto it = pads_.rbegin(); it != pads_.rend(); ++it)
        if (Usable(*it)) return &*it;
    return nullptr;
}

SDL_JoystickID Pads::Active() const
{
    const Pad* p = Driving();
    return p ? p->id : -1;
}

int Pads::Style() const
{
    const Pad* p = Driving();
    return p ? p->style : XBOX;
}

SDL_GameControllerType Pads::Type() const
{
    const Pad* p = Driving();
    return p ? p->type : SDL_CONTROLLER_TYPE_UNKNOWN;
}

std::vector<Pads::Connected> Pads::TakeConnected()
{
    std::vector<Connected> c;
    c.swap(connected_);
    return c;
}

const char* PauseCombo(int style, SDL_GameControllerType type)
{
    if (style == PLAYSTATION) return type == SDL_CONTROLLER_TYPE_PS5 ? "Options + Create" : type == SDL_CONTROLLER_TYPE_PS3 ? "Start + Select" : "Options + Share";
    if (style == NINTENDO) return "+ / -";
    return "Start + Back";
}

bool Pads::Event(const SDL_Event& ev, const PadOptions& o)
{
    switch (ev.type)
    {
    case SDL_CONTROLLERDEVICEADDED: Open(ev.cdevice.which, o); return false;
    case SDL_CONTROLLERDEVICEREMOVED: Close(ev.cdevice.which); return false;
    case SDL_JOYDEVICEADDED:
        if (!SDL_IsGameController(ev.jdevice.which))
        {
            // A pad SDL has no mapping for: what the player needs to write one.
            char guid[64];
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(ev.jdevice.which), guid, sizeof guid);
            const char* name = SDL_JoystickNameForIndex(ev.jdevice.which);
            fprintf(stderr, "controller without a mapping: %s (GUID %s): add a line for it to %s\n", name ? name : "?", guid,
                    UserMappingsPath().c_str());
        }
        return false;
    case SDL_CONTROLLERBUTTONDOWN:
    {
        const Pad* p = Find(ev.cbutton.which);
        if (!p || !Usable(*p)) return false;
        active_ = p->id;
        return true;
    }
    case SDL_CONTROLLERAXISMOTION:
    {
        const Pad* p = Find(ev.caxis.which);
        if (!p || !Usable(*p)) return false;
        const bool trigger = ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
        const float v = std::fabs(ev.caxis.value / 32767.0f);
        // A stick resting off centre (drift) or a trigger brushed never takes the lead.
        if (v <= (trigger ? o.trigger : std::max(o.deadZone, 0.5f))) return false;
        active_ = p->id;
        return true;
    }
    default: return false;
    }
}

PadRaw Pads::Read(const PadOptions& o) const
{
    PadRaw r;
    const Pad* p = Driving();
    if (!p) return r;
    SDL_GameController* c = p->c;
    // (A button or axis the pad does not have reads 0.)
    for (int i = 0; i <= IN_RIGHT; i++) r.btn[i] = SDL_GameControllerGetButton(c, (SDL_GameControllerButton)i) != 0;
    auto axis = [&](SDL_GameControllerAxis a) { return std::clamp(SDL_GameControllerGetAxis(c, a) / 32767.0f, -1.0f, 1.0f); };
    // Round dead zones: a stick inside it reads 0 on both axes; outside, as pushed.
    auto stick = [&](SDL_GameControllerAxis ax, SDL_GameControllerAxis ay, float& x, float& y) {
        x = axis(ax);
        y = axis(ay);
        const float mag = std::sqrt(x * x + y * y);
        if (mag <= o.deadZone) { x = y = 0; return; }
        // Rescaled from the dead zone's edge, so a push starts from 0 and the edge still reads 1.
        const float k = std::min(1.0f, (mag - o.deadZone) / (1 - o.deadZone)) / mag;
        x *= k;
        y *= k;
    };
    stick(SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, r.lx, r.ly);
    stick(SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, r.rx, r.ry);
    r.btn[IN_LT] = axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > o.trigger;
    r.btn[IN_RT] = axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > o.trigger;
    return r;
}

bool NintendoLabels()
{
    return SDL_GetHintBoolean(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, SDL_TRUE);
}

int PrintedInput(int in, int style, bool nintendoLabels)
{
    if (style != NINTENDO || nintendoLabels) return in;
    switch (in)
    {
    case IN_A: return IN_B;
    case IN_B: return IN_A;
    case IN_X: return IN_Y;
    case IN_Y: return IN_X;
    default: return in;
    }
}

ui::ChipLook ChipLookFor(bool keyboard, int style, const Bindings& b, bool nintendoLabels, SDL_GameControllerType type)
{
    using ui::Button;
    ui::ChipLook look;
    look.face = keyboard ? ui::Face::Keyboard : style == PLAYSTATION ? ui::Face::PlayStation : style == NINTENDO ? ui::Face::Nintendo : ui::Face::Xbox;
    // The action each chip stands for (the navigator's PadState: the DS's buttons, and Back the
    // TOUCH MENU); LT is the
    // trigger itself, the D-pad and the sticks are drawn as they are.
    auto action = [](Button k) {
        switch (k)
        {
        case Button::A: return (int)A;
        case Button::B: return (int)B;
        case Button::X: return (int)X;
        case Button::Y: return (int)Y;
        case Button::LB: return (int)L;
        case Button::RB: return (int)R;
        case Button::Start: return (int)START;
        case Button::Back: return (int)PANEL; // the town's menu hint: TOUCH MENU (Back on a pad, E)
        default: return -1;
        }
    };
    for (int i = 0; i < ui::kButtonCount; i++)
    {
        const Button k = (Button)i;
        const int a = action(k);
        if (k == Button::Pause)
        {
            // The pause menu: Start + Back on every pad (Poll's combo, whatever the bindings), the
            // MENU key on the keyboard (Esc).
            if (keyboard) { if (b.Key(MENU) != kNone) look.glyph[i] = KeyName(b.Key(MENU)); }
            else look.glyph[i] = style != PLAYSTATION ? "start+back" : type == SDL_CONTROLLER_TYPE_PS5 ? "start+create"
                               : type == SDL_CONTROLLER_TYPE_PS3 ? "start+select" : "start+back";
            continue;
        }
        if (keyboard)
        {
            if (a >= 0 && b.Key(a) != kNone) look.glyph[i] = KeyName(b.Key(a));
            continue;
        }
        const int in = a >= 0 ? b.Pad(a) : k == Button::LT ? (int)IN_LT : -1;
        if (in >= 0) look.glyph[i] = InputKey(PrintedInput(in, style, nintendoLabels));
        // PlayStation's Back is the PS5's Create, the PS3's Select (else the PS4's Share).
        if (look.glyph[i] == "back" && style == PLAYSTATION)
            look.glyph[i] = type == SDL_CONTROLLER_TYPE_PS5 ? "create" : type == SDL_CONTROLLER_TYPE_PS3 ? "select" : "back";
    }
    return look;
}
}
