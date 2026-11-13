// Checks the controllers (src/pads.*) on SDL's virtual joysticks, no hardware: pads plugged in
// and out mid-game, two pads (the last one used drives, a drifting one never leaks in), the
// triggers as buttons past their threshold, the sticks' round dead zone, a pad with buttons
// missing, Steam Input's virtual pad over its physical twin, each pad's type → the chips' look
// (and the keyboard's keys), the menu's names, and the mappings' order (the bundled database,
// the player's file over it, Steam's SDL_GAMECONTROLLERCONFIG over both).
//   ninja -C build pads_test && SDL_VIDEODRIVER=dummy ./build/pads_test
#define SDL_MAIN_HANDLED // (a plain main, on Windows too)
#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "pads.h"
#include "ui/kit.h"

using namespace sims2hd;
using namespace Controls;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static PadOptions opt;
static int rumbles = 0;

static int SDLCALL Rumble(void*, Uint16, Uint16) { rumbles++; return 0; }

// The test's SDL: joysticks and game controllers only, every real backend off (a pad on the
// machine running the test must not count), Steam's virtual pad allowed or not.
static bool Start(bool steamLaunch)
{
    Pads::Hints();
    CHECK(SDL_GetHintBoolean(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, SDL_TRUE) == SDL_FALSE);
    for (const char* h : {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_JOYSTICK_IOKIT, SDL_HINT_JOYSTICK_MFI, SDL_HINT_JOYSTICK_RAWINPUT,
                          SDL_HINT_JOYSTICK_WGI, SDL_HINT_XINPUT_ENABLED, SDL_HINT_DIRECTINPUT_ENABLED})
        SDL_SetHint(h, "0");
    SDL_SetHint("SDL_JOYSTICK_LINUX_CLASSIC", "0");
    SDL_SetHint("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", steamLaunch ? "1" : "0");
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); failures++; return false; }
    return true;
}

// A virtual pad: every button and axis unless masks say otherwise. Its instance id.
static SDL_JoystickID Attach(Uint16 vendor, Uint16 product, const char* name, Uint32 buttons = 0, Uint32 axes = 0, int nbuttons = 15,
                             int naxes = 6)
{
    SDL_VirtualJoystickDesc d;
    SDL_zero(d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = (Uint16)naxes;
    d.nbuttons = (Uint16)nbuttons;
    d.vendor_id = vendor;
    d.product_id = product;
    d.button_mask = buttons;
    d.axis_mask = axes;
    d.name = name;
    d.Rumble = Rumble;
    const int index = SDL_JoystickAttachVirtualEx(&d);
    if (index < 0) { fprintf(stderr, "attach %s: %s\n", name, SDL_GetError()); failures++; return -1; }
    return SDL_JoystickGetDeviceInstanceID(index);
}

static void Detach(SDL_JoystickID id)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_JoystickGetDeviceInstanceID(i) == id) { SDL_JoystickDetachVirtual(i); return; }
}

// The events since the last call, through the pads as the window's loop gives them; how many
// were a driving pad's use.
static int Pump(Pads& pads)
{
    int used = 0;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) used += pads.Event(ev, opt) ? 1 : 0;
    return used;
}

static void Button(SDL_JoystickID id, int b, bool down)
{
    SDL_Joystick* j = SDL_JoystickFromInstanceID(id);
    CHECK(j != nullptr);
    if (j) SDL_JoystickSetVirtualButton(j, b, down ? 1 : 0);
}

static void Axis(SDL_JoystickID id, int a, float v)
{
    SDL_Joystick* j = SDL_JoystickFromInstanceID(id);
    CHECK(j != nullptr);
    if (j) SDL_JoystickSetVirtualAxis(j, a, (Sint16)std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f));
}

// A trigger's virtual axis runs from -32768 (up) to 32767 (down): t in 0..1.
static void Trigger(SDL_JoystickID id, int a, float t)
{
    SDL_Joystick* j = SDL_JoystickFromInstanceID(id);
    if (j) SDL_JoystickSetVirtualAxis(j, a, (Sint16)std::lround(-32768.0f + t * 65535.0f));
}

// A stick's reading: rescaled from the dead zone's edge.
static float Rescaled(float v, float dz) { return (v - dz) / (1 - dz); }

static constexpr Uint16 kMicrosoft = 0x045E, kXboxOne = 0x02EA, kSony = 0x054C, kDs4 = 0x09CC, kNintendo = 0x057E, kSwitchPro = 0x2009;

static void hotplug()
{
    Pads pads;
    opt = PadOptions();
    rumbles = 0;
    pads.OpenAll(opt);
    Pump(pads);
    CHECK(pads.Count() == 0);
    CHECK(!pads.Any());
    CHECK(pads.Active() == -1);
    PadRaw r = pads.Read(opt);
    CHECK(!r.btn[IN_A] && r.lx == 0);
    // Plugged in mid-game.
    const SDL_JoystickID a = Attach(kMicrosoft, kXboxOne, "Xbox One pad");
    Pump(pads);
    CHECK(pads.Count() == 1);
    CHECK(pads.Active() == a);
    CHECK(pads.Style() == XBOX);
    CHECK(rumbles == 1); // its hello buzz
    Button(a, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(Pump(pads) == 1);
    CHECK(pads.Read(opt).btn[IN_A]);
    // Pulled out while A is held: nothing stays pressed.
    Detach(a);
    Pump(pads);
    CHECK(pads.Count() == 0);
    CHECK(!pads.Any());
    CHECK(!pads.Read(opt).btn[IN_A]);
    // Back in, no buzz when rumble is off.
    opt.rumble = false;
    const SDL_JoystickID b = Attach(kMicrosoft, kXboxOne, "Xbox One pad again");
    Pump(pads);
    CHECK(pads.Count() == 1);
    CHECK(pads.Active() == b);
    CHECK(rumbles == 1);
    // Opening what is open already does nothing (the ADDED event after OpenAll at start).
    pads.OpenAll(opt);
    CHECK(pads.Count() == 1);
    pads.CloseAll();
    CHECK(pads.Count() == 0);
    Detach(b);
    Pump(pads);
    opt = PadOptions();
}

static void two_pads()
{
    Pads pads;
    const SDL_JoystickID x = Attach(kMicrosoft, kXboxOne, "Xbox pad");
    const SDL_JoystickID p = Attach(kSony, kDs4, "DualShock 4");
    Pump(pads);
    CHECK(pads.Count() == 2);
    CHECK(pads.Active() == p); // none used yet: the last one connected
    // The Xbox pad's A: it drives.
    Button(x, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(Pump(pads) == 1);
    CHECK(pads.Active() == x);
    CHECK(pads.Style() == XBOX);
    CHECK(pads.Read(opt).btn[IN_A]);
    Button(x, SDL_CONTROLLER_BUTTON_A, false);
    Pump(pads);
    // The PlayStation pad's circle (SDL's B): it drives now, with its labels.
    Button(p, SDL_CONTROLLER_BUTTON_B, true);
    CHECK(Pump(pads) == 1);
    CHECK(pads.Active() == p);
    CHECK(pads.Style() == PLAYSTATION);
    CHECK(pads.Read(opt).btn[IN_B]);
    CHECK(!pads.Read(opt).btn[IN_A]);
    Button(p, SDL_CONTROLLER_BUTTON_B, false);
    Pump(pads);
    // The idle Xbox pad's stick drifting (past the dead zone, short of a push): it does not
    // take the lead and is never read alongside.
    Axis(x, SDL_CONTROLLER_AXIS_LEFTX, 0.35f);
    CHECK(Pump(pads) == 0);
    CHECK(pads.Active() == p);
    CHECK(pads.Read(opt).lx == 0);
    // Pushed for real: it drives, and only its stick is read.
    Axis(x, SDL_CONTROLLER_AXIS_LEFTX, 0.9f);
    CHECK(Pump(pads) == 1);
    CHECK(pads.Active() == x);
    CHECK(std::fabs(pads.Read(opt).lx - Rescaled(0.9f, opt.deadZone)) < 0.01f);
    // Both press A at once: one press (the driving pad's), never two.
    Button(p, SDL_CONTROLLER_BUTTON_A, true);
    Button(x, SDL_CONTROLLER_BUTTON_A, true);
    Pump(pads);
    PadRaw r = pads.Read(opt);
    CHECK(r.btn[IN_A]);
    Button(p, SDL_CONTROLLER_BUTTON_A, false);
    Button(x, SDL_CONTROLLER_BUTTON_A, false);
    Axis(x, SDL_CONTROLLER_AXIS_LEFTX, 0);
    Pump(pads);
    // The driving pad pulled out: the other one drives.
    Detach(x);
    Pump(pads);
    CHECK(pads.Count() == 1);
    CHECK(pads.Active() == p);
    CHECK(pads.Style() == PLAYSTATION);
    pads.CloseAll();
    Detach(p);
    Pump(pads);
}

static void triggers_and_dead_zone()
{
    Pads pads;
    const SDL_JoystickID x = Attach(kMicrosoft, kXboxOne, "Xbox pad");
    Pump(pads);
    Trigger(x, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0);
    Trigger(x, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0);
    Pump(pads);
    PadRaw r = pads.Read(opt);
    CHECK(!r.btn[IN_LT] && !r.btn[IN_RT]);
    // A quarter down: not pressed at the default half, pressed at a 20 % threshold.
    Trigger(x, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0.25f);
    Pump(pads);
    CHECK(!pads.Read(opt).btn[IN_LT]);
    PadOptions low = opt;
    low.trigger = 0.2f;
    CHECK(pads.Read(low).btn[IN_LT]);
    // All the way: pressed, the other trigger still up.
    Trigger(x, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1);
    Pump(pads);
    r = pads.Read(opt);
    CHECK(r.btn[IN_LT] && !r.btn[IN_RT]);
    Trigger(x, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0.8f);
    Pump(pads);
    CHECK(pads.Read(opt).btn[IN_RT]);
    PadOptions high = opt;
    high.trigger = 0.9f;
    CHECK(!pads.Read(high).btn[IN_RT]);
    // The sticks: inside the round dead zone both axes read 0, outside as pushed.
    Axis(x, SDL_CONTROLLER_AXIS_LEFTX, 0.2f);
    Pump(pads);
    CHECK(pads.Read(opt).lx == 0);
    Axis(x, SDL_CONTROLLER_AXIS_LEFTY, 0.2f); // 0.28 off centre, diagonally
    Pump(pads);
    r = pads.Read(opt);
    // Rescaled from the dead zone's edge, the direction kept.
    const float d = Rescaled(std::sqrt(0.08f), opt.deadZone) / std::sqrt(2.0f);
    CHECK(r.lx > 0 && std::fabs(r.lx - d) < 0.01f && std::fabs(r.ly - d) < 0.01f);
    PadOptions wide = opt;
    wide.deadZone = 0.35f;
    r = pads.Read(wide);
    CHECK(r.lx == 0 && r.ly == 0);
    Axis(x, SDL_CONTROLLER_AXIS_RIGHTX, -0.6f);
    Pump(pads);
    r = pads.Read(wide);
    CHECK(std::fabs(r.rx + Rescaled(0.6f, wide.deadZone)) < 0.01f && r.ry == 0);
    // Full: 1 whatever the dead zone.
    Axis(x, SDL_CONTROLLER_AXIS_RIGHTX, 1);
    Pump(pads);
    CHECK(std::fabs(pads.Read(wide).rx - 1) < 0.01f && std::fabs(pads.Read(opt).rx - 1) < 0.01f);
    // Swap the sticks (left_handed): the right stick's push read as the left's and the other way,
    // their clicks too, so the world's camera (the right stick's, navigator.h) is the left one's.
    Axis(x, SDL_CONTROLLER_AXIS_LEFTX, 0);
    Axis(x, SDL_CONTROLLER_AXIS_LEFTY, -1);
    Pump(pads);
    r = pads.Read(opt);
    r.btn[IN_RS] = true;
    SwapSticks(r);
    CHECK(std::fabs(r.lx - 1) < 0.01f && r.ly == 0 && std::fabs(r.ry + 1) < 0.01f && r.rx == 0 && r.btn[IN_LS] && !r.btn[IN_RS]);
    pads.CloseAll();
    Detach(x);
    Pump(pads);
}

static void missing_buttons()
{
    // A small pad: A, B and Start, no sticks, no triggers.
    Pads pads;
    const Uint32 mask = (1u << SDL_CONTROLLER_BUTTON_A) | (1u << SDL_CONTROLLER_BUTTON_B) | (1u << SDL_CONTROLLER_BUTTON_START);
    const SDL_JoystickID m = Attach(0x1234, 0x5678, "Tiny pad", mask, 0, 3, 0);
    Pump(pads);
    CHECK(pads.Count() == 1);
    PadRaw r = pads.Read(opt);
    for (int i = 0; i < IN_COUNT; i++) CHECK(!r.btn[i]);
    CHECK(r.lx == 0 && r.ly == 0 && r.rx == 0 && r.ry == 0);
    Button(m, 2, true); // its third button: Start
    CHECK(Pump(pads) == 1);
    r = pads.Read(opt);
    CHECK(r.btn[IN_START]);
    CHECK(!r.btn[IN_X] && !r.btn[IN_LT] && !r.btn[IN_GUIDE]);
    pads.CloseAll();
    Detach(m);
    Pump(pads);
}

static void types_and_looks()
{
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_XBOX360) == XBOX);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_XBOXONE) == XBOX);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_VIRTUAL) == XBOX);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_UNKNOWN) == XBOX);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_PS3) == PLAYSTATION);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_PS4) == PLAYSTATION);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_PS5) == PLAYSTATION);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO) == NINTENDO);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) == NINTENDO);
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT) == NINTENDO);

    // A pad's type as SDL reads it from its ids.
    Pads pads;
    const SDL_JoystickID sw = Attach(kNintendo, kSwitchPro, "Pro Controller");
    Pump(pads);
    CHECK(pads.Style() == NINTENDO);
    pads.CloseAll();
    Detach(sw);
    Pump(pads);

    using ui::Button;
    Bindings b;
    // Xbox: the pad's own inputs; a chip of the default binding draws exactly as it always did.
    ui::ChipLook look = ChipLookFor(false, XBOX, b, true);
    CHECK(look.face == ui::Face::Xbox);
    CHECK(look.glyph[(int)Button::A] == "a" && look.glyph[(int)Button::LB] == "lb" && look.glyph[(int)Button::Start] == "start");
    CHECK(look.glyph[(int)Button::LT] == "lt" && look.glyph[(int)Button::DPad].empty());
    CHECK(look.glyph[(int)Button::Back] == "back"); // TOUCH MENU's button (the town's menu hint)
    CHECK(look.glyph[(int)Button::Pause] == "start+back");
    ui::SetChipLook(look);
    const ui::UiItem xa = ui::ChipItem(0, 0, Button::A, "", 24);
    CHECK(xa.face == ui::Face::Xbox && xa.glyph.empty());
    CHECK(ui::Hash({xa}) == ui::Hash({[] { ui::UiItem i = ui::ChipItem(0, 0, Button::A, "", 24); return i; }()}));
    const int startXbox = ui::ChipRect(0, 0, Button::Start, "", 24).w;
    // PlayStation: the cross for A; L1 and OPTIONS.
    // PlayStation's Back by its type: the PS5's Create, the PS3's Select, else Share.
    CHECK(ChipLookFor(false, PLAYSTATION, b, true, SDL_CONTROLLER_TYPE_PS5).glyph[(int)Button::Back] == "create");
    CHECK(ChipLookFor(false, PLAYSTATION, b, true, SDL_CONTROLLER_TYPE_PS5).glyph[(int)Button::Pause] == "start+create");
    CHECK(ChipLookFor(false, PLAYSTATION, b, true, SDL_CONTROLLER_TYPE_PS3).glyph[(int)Button::Back] == "select");
    CHECK(ChipLookFor(false, XBOX, b, true, SDL_CONTROLLER_TYPE_PS5).glyph[(int)Button::Back] == "back");
    CHECK(std::string(PauseCombo(PLAYSTATION, SDL_CONTROLLER_TYPE_PS5)) == "Options + Create");
    CHECK(std::string(PauseCombo(XBOX, SDL_CONTROLLER_TYPE_XBOXONE)) == "Start + Back");
    look = ChipLookFor(false, PLAYSTATION, b, true, SDL_CONTROLLER_TYPE_PS4);
    CHECK(look.face == ui::Face::PlayStation && look.glyph[(int)Button::A] == "a" && look.glyph[(int)Button::Back] == "back");
    ui::SetChipLook(look);
    const ui::UiItem pa = ui::ChipItem(4, 4, Button::A, "", 24);
    CHECK(pa.face == ui::Face::PlayStation && pa.glyph == "a");
    CHECK(ui::Hash({pa}) != ui::Hash({xa}));
    CHECK(ui::ChipRect(0, 0, Button::Start, "", 24).w > startXbox); // "OPTIONS" is longer than "START"
    const int backPs = ui::ChipRect(0, 0, Button::Back, "Menu", 24).w;   // "SHARE"
    {
        // The cross is drawn in its blue, no green A.
        std::vector<u32> px(48 * 48);
        ui::Rasterise({pa}, 1, px.data(), 48, 48);
        int blue = 0, green = 0;
        for (u32 c : px)
        {
            const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, bl = c & 0xFF;
            if ((c >> 24) > 200 && bl > 200 && r < 160 && g > 150 && g < 200) blue++;
            if ((c >> 24) > 200 && g > 200 && r < 140 && bl < 120) green++;
        }
        CHECK(blue > 10);
        CHECK(green == 0);
    }
    // Nintendo: A / B as printed; with SDL's labels off its A is the button printed B.
    look = ChipLookFor(false, NINTENDO, b, true);
    CHECK(look.face == ui::Face::Nintendo && look.glyph[(int)Button::A] == "a" && look.glyph[(int)Button::X] == "x");
    look = ChipLookFor(false, NINTENDO, b, false);
    CHECK(look.glyph[(int)Button::A] == "b" && look.glyph[(int)Button::B] == "a");
    CHECK(look.glyph[(int)Button::X] == "y" && look.glyph[(int)Button::Y] == "x");
    CHECK(look.glyph[(int)Button::LB] == "lb");
    ui::SetChipLook(look);
    CHECK(ui::ChipRect(0, 0, Button::Start, "", 24).w < startXbox); // "+"
    CHECK(ui::ChipRect(0, 0, Button::Back, "Menu", 24).w < backPs);   // "-"
    CHECK(PrintedInput(IN_A, NINTENDO, false) == IN_B);
    CHECK(PrintedInput(IN_A, NINTENDO, true) == IN_A);
    CHECK(PrintedInput(IN_A, PLAYSTATION, false) == IN_A);
    CHECK(std::string(InputName(PrintedInput(IN_A, NINTENDO, false), NINTENDO)) == "B");
    CHECK(std::string(InputName(IN_A, PLAYSTATION)) == "CROSS");
    // A remapped action shows the button it is on.
    Bindings m;
    m.SetPad(A, IN_X);
    look = ChipLookFor(false, XBOX, m, true);
    CHECK(look.glyph[(int)Button::A] == "x" && look.glyph[(int)Button::X] == "a");
    // The keyboard: the keys' names on keycaps (the D-pad and LT keep the pad's look).
    look = ChipLookFor(true, PLAYSTATION, b, true);
    CHECK(look.face == ui::Face::Keyboard);
    CHECK(look.glyph[(int)Button::A] == KeyName(b.Key(A)));
    CHECK(look.glyph[(int)Button::Start] == "ENTER");
    CHECK(look.glyph[(int)Button::Back] == KeyName(b.Key(PANEL))); // E on QWERTY once the keymap is known
    CHECK(look.glyph[(int)Button::Pause] == "ESC");                  // the pause menu's key, not a pad's combo
    CHECK(look.glyph[(int)Button::LT].empty() && look.glyph[(int)Button::DPad].empty());
    ui::SetChipLook(look);
    const ui::UiItem ka = ui::ChipItem(0, 0, Button::Start, "", 24);
    CHECK(ka.face == ui::Face::Keyboard && ka.glyph == "ENTER");
    CHECK(ui::ChipRect(0, 0, Button::LT, "", 24).w == [] { ui::SetChipLook({}); return ui::ChipRect(0, 0, Button::LT, "", 24).w; }());
    ui::SetChipLook({});
}

static void steam()
{
#ifdef __APPLE__
    const Uint16 sv = 0x045E, sp = 0x028E; // Steam's virtual pad on a Mac: an Xbox 360 pad, version 0
#else
    const Uint16 sv = 0x28DE, sp = 0x11FF;
#endif
    CHECK(Pads::SteamVirtual(sv, sp, 0));
    CHECK(!Pads::SteamVirtual(kMicrosoft, kXboxOne, 0));
    CHECK(!Pads::SteamVirtual(kSony, kDs4, 0x100));
    Pads pads;
    // Not launched by Steam: Steam's virtual pad is left alone, only the physical one is seen
    // (SDL's drivers do it; SDL's virtual joysticks do not, Pads::Open does).
    SDL_SetHint("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "0");
    const SDL_JoystickID phys = Attach(kSony, kDs4, "DualShock 4");
    const SDL_JoystickID ghost = Attach(sv, sp, "Steam Virtual Gamepad");
    Pump(pads);
    CHECK(pads.Count() == 1);
    CHECK(pads.Active() == phys);
    Detach(ghost);
    Pump(pads);
    // Launched by Steam with Steam Input: its virtual pad is the one read; the physical twin's
    // presses (also seen when Steam does not hide it) never act a second time.
    SDL_SetHint("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "1");
    const SDL_JoystickID virt = Attach(sv, sp, "Steam Virtual Gamepad");
    Pump(pads);
    CHECK(pads.Count() == 2);
    CHECK(pads.Active() == virt);
    CHECK(pads.Style() == XBOX); // Xbox-like, unless Steam tells SDL the real type
    {
        // Each pad opened is told once (the window's toast: how to pause).
        const std::vector<Pads::Connected> c = pads.TakeConnected();
        CHECK(c.size() == 2 && c.back().name == "Steam Virtual Gamepad");
        CHECK(pads.TakeConnected().empty());
    }
    Button(phys, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(Pump(pads) == 0);
    CHECK(pads.Active() == virt);
    CHECK(!pads.Read(opt).btn[IN_A]);
    Button(virt, SDL_CONTROLLER_BUTTON_A, true);
    CHECK(Pump(pads) == 1);
    CHECK(pads.Read(opt).btn[IN_A]);
    Button(virt, SDL_CONTROLLER_BUTTON_A, false);
    Pump(pads);
    CHECK(!pads.Read(opt).btn[IN_A]); // the physical A still held: not read
    // The Guide button is Steam's: it opens no menu by default (no action has it).
    Bindings b;
    CHECK(b.ActionOfPad(IN_GUIDE) == -1);
    CHECK(b.Pad(MENU) == kNone);
    // Steam Input switched off mid-game (its pad goes): the physical pad drives again.
    Detach(virt);
    Pump(pads);
    CHECK(pads.Count() == 1);
    CHECK(pads.Active() == phys);
    CHECK(pads.Read(opt).btn[IN_A]);
    pads.CloseAll();
    Detach(phys);
    Pump(pads);
    SDL_SetHint("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "0");
}

// The Steam Controller and the Deck, through Steam Input and as SDL's drivers show them.
static void steam_pads()
{
    // The ids: SDL's Steam Controllers, not the Deck, not Steam's virtual pad, not a Sony pad.
    for (Uint16 p : {0x1101, 0x1102, 0x1105, 0x1106, 0x1142, 0x1201, 0x1202}) CHECK(Pads::SteamController(0x28DE, p));
    CHECK(!Pads::SteamController(0x28DE, 0x1205)); // the Deck
    CHECK(!Pads::SteamController(0x28DE, 0x11FF)); // Steam's virtual pad
    CHECK(!Pads::SteamController(kSony, 0x1102));
    // The driver off in SDL, on here unless Steam launched the game (the player's own hint stays).
    // SDL's default stays: the Steam Controller driver off unless the player sets it.
    Pads::Hints();
    CHECK(!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAM, SDL_FALSE));
    // Each type's look: Xbox-style glyphs for the Steam Controller, the Deck and Steam's pad.
    CHECK(Pads::StyleOf(SDL_CONTROLLER_TYPE_UNKNOWN) == XBOX);
    Pads pads;
    for (Uint16 product : {0x1102, 0x1205})
    {
        const SDL_JoystickID id = Attach(0x28DE, product, product == 0x1205 ? "Steam Deck" : "Steam Controller", 0, 0, 17);
        Pump(pads);
        CHECK(pads.Count() == 1 && pads.Active() == id);
        CHECK(pads.Style() == XBOX);
        // The full set of a Steam Input default gamepad: faces, bumpers, Start / Back, both clicks, the D-pad.
        const int bs[] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
                          SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK,
                          SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                          SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                          SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
        const int ins[] = {IN_A, IN_B, IN_X, IN_Y, IN_BACK, IN_START, IN_LS, IN_RS, IN_LB, IN_RB, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT};
        for (size_t i = 0; i < 14; i++)
        {
            if (ins[i] == IN_RS && product == 0x1102) continue; // (the click is the grip's, below)
            Button(id, bs[i], true);
            Pump(pads);
            CHECK(pads.Read(opt).btn[ins[i]]);
            Button(id, bs[i], false);
            Pump(pads);
            CHECK(!pads.Read(opt).btn[ins[i]]);
        }
        if (product == 0x1102)
        {
            // The right pad's click never reaches SDL natively: the back grip stands for R3.
            Button(id, SDL_CONTROLLER_BUTTON_PADDLE1, true);
            Pump(pads);
            CHECK(pads.Read(opt).btn[IN_RS]);
            Button(id, SDL_CONTROLLER_BUTTON_PADDLE1, false);
            Pump(pads);
            CHECK(!pads.Read(opt).btn[IN_RS]);
        }
        Trigger(id, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1.0f);
        Trigger(id, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1.0f);
        Pump(pads);
        CHECK(pads.Read(opt).btn[IN_LT] && pads.Read(opt).btn[IN_RT]);
        Detach(id);
        Pump(pads);
        CHECK(pads.Count() == 0);
    }
    // Every default action has an input a Steam Input gamepad gives: none on the Guide.
    Bindings b;
    for (int a = 0; a < COUNT; a++) CHECK(b.Pad(a) != IN_GUIDE);
}

// A mapping line for a virtual pad's GUID, named `name`, A on its button `a`.
static std::string Line(const std::string& guid, const char* name, int a)
{
    return guid + "," + name + ",a:b" + std::to_string(a) + ",b:b" + std::to_string(1 - a) + ",platform:" + SDL_GetPlatform() + ",";
}

static void mappings()
{
    // The virtual pad's GUID, from a first run of SDL.
    if (!Start(false)) return;
    SDL_JoystickID id = Attach(0x1209, 0x7777, "Mapped pad", 0x3, 0, 2, 0);
    char guid[64] = {};
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_JoystickGetDeviceInstanceID(i) == id) SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof guid);
    CHECK(guid[0] != 0);
    // SDL's own mappings (its database, hint-aware for Nintendo pads) are kept: a bundled line for
    // a pad SDL maps already is not added over it.
    std::vector<std::pair<std::string, std::string>> own;
    for (int i = 0; i < SDL_GameControllerNumMappings() && own.size() < 40; i++)
        if (char* m = SDL_GameControllerMappingForIndex(i))
        {
            const std::string line = m;
            SDL_free(m);
            own.push_back({line.substr(0, line.find(',')), line});
        }
    CHECK(own.size() > 5);
    {
        // A file with a line over one of SDL's pads and a new one: only the new one is added.
        const std::string over = own[0].first + ",Bundled Over SDL,a:b5,b:b6,platform:" + SDL_GetPlatform() + ",";
        const std::string fresh = "03000000ab1200000077000000000000,Bundled New,a:b0,b:b1,platform:" + std::string(SDL_GetPlatform()) + ",";
        const std::string text = "# comment\n" + over + "\r\n" + fresh + "\n\n";
        CHECK(Pads::AddUnmapped(text.data(), text.size()) == 1);
        char* m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(own[0].first.c_str()));
        CHECK(m && std::string(m).find("Bundled Over SDL") == std::string::npos);
        SDL_free(m);
        m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString("03000000ab1200000077000000000000"));
        CHECK(m && std::string(m).find("Bundled New") != std::string::npos);
        SDL_free(m);
    }
    // The bundled database: this platform's lines, hundreds of them; SDL's own still as they were.
    CHECK(Pads::LoadMappings("") > 100);
    for (const auto& o : own)
    {
        char* m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(o.first.c_str()));
        CHECK(m && std::string(m) == o.second);
        SDL_free(m);
    }
    // The player's file wins over what is there.
    const std::filesystem::path dir = std::filesystem::temp_directory_path();
    const std::string user = (dir / "sims2hd_pads_test_db.txt").string();
    {
        std::ofstream f(user);
        f << "# a comment\n" << Line(guid, "Player Pad", 0) << "\n";
    }
    Pads::LoadMappings(user); // (the bundled lines again: replacing, so none counted new)
    char* m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(guid));
    CHECK(m && std::string(m).find("Player Pad") != std::string::npos);
    SDL_free(m);
    SDL_Quit();

    // Steam's SDL_GAMECONTROLLERCONFIG (read by SDL at SDL_Init) stays over both.
    const std::string env = Line(guid, "Steam Pad", 1);
#ifdef _WIN32
    _putenv_s("SDL_GAMECONTROLLERCONFIG", env.c_str());
#else
    setenv("SDL_GAMECONTROLLERCONFIG", env.c_str(), 1);
#endif
    if (!Start(false)) return;
    id = Attach(0x1209, 0x7777, "Mapped pad", 0x3, 0, 2, 0);
    Pads::LoadMappings(user);
    m = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(guid));
    CHECK(m && std::string(m).find("Steam Pad") != std::string::npos);
    SDL_free(m);
    // And the pad reads through it: its button 1 is A.
    Pads pads;
    Pump(pads);
    CHECK(pads.Count() == 1);
    Button(id, 1, true);
    Pump(pads);
    CHECK(pads.Read(opt).btn[IN_A]);
    pads.CloseAll();
    SDL_Quit();
#ifdef _WIN32
    _putenv_s("SDL_GAMECONTROLLERCONFIG", "");
#else
    unsetenv("SDL_GAMECONTROLLERCONFIG");
#endif
    std::filesystem::remove(user);
}

int main()
{
    mappings();
    if (!Start(false)) return 1;
    hotplug();
    two_pads();
    triggers_and_dead_zone();
    missing_buttons();
    types_and_looks();
    steam();
    steam_pads();
    SDL_Quit();
    if (failures) { fprintf(stderr, "pads_test: %d failed\n", failures); return 1; }
    printf("pads_test: all passed\n");
    return 0;
}
