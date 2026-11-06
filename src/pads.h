// The controllers: SDL_GameController over every backend SDL has (XInput, RawInput / Windows.Gaming
// .Input, DirectInput and HIDAPI on Windows; IOKit, GameController.framework and HIDAPI on a Mac;
// evdev and HIDAPI on Linux), the mappings (SDL's own, the bundled SDL_GameControllerDB, the
// player's file, then Steam's SDL_GAMECONTROLLERCONFIG over them all), hot-plug, and which pad
// drives: any of them, the last one used, never two at once. When Steam Input wraps the pads
// (the .exe added to Steam as a non-Steam game), SDL sees Steam's virtual pad: while one is
// open, the others are its physical twins and are not read.
#pragma once
#include <SDL.h>

#include <string>
#include <vector>

#include "controls.h"
#include "ui/kit.h"

namespace sims2wide
{
// A controller's inputs as read (the navigator's PadState is the actions they make): the
// buttons in Controls::PadInput's order (the triggers past their threshold), the sticks in
// -1..1, zero inside the dead zone and rescaled from its edge.
struct PadRaw
{
    bool btn[Controls::IN_COUNT] = {};
    float lx = 0, ly = 0, rx = 0, ry = 0;
};

struct PadOptions
{
    float deadZone = 0.25f; // of a stick's travel, round (both axes together)
    float trigger = 0.5f;   // of a trigger's travel, past which it is pressed
    bool rumble = true;     // a short buzz when a controller connects (if it has a motor)
};

class Pads
{
public:
    // Before SDL_Init: SDL's hints this program sets (presses never come while another window
    // has the focus, so Steam's overlay or another app does not leak into the game). Hints
    // given in the environment (Steam's, the player's) win over these.
    static void Hints();
    // After SDL_Init: the bundled SDL_GameControllerDB (this platform's lines), then the player's
    // file (`userFile`, when it exists), then SDL_HINT_GAMECONTROLLERCONFIG_FILE again (so a
    // file Steam or the player names wins over the bundled one). SDL_GAMECONTROLLERCONFIG,
    // which SDL read at SDL_Init, keeps its priority over all of them. Returns the bundled
    // mappings added (-1: none could be read).
    static int LoadMappings(const std::string& userFile);
    // The lines of a mappings file whose GUID SDL has no mapping for yet; how many were added.
    static int AddUnmapped(const char* text, size_t size);
    // gamecontrollerdb.txt in the settings folder ("" when SDL has none).
    static std::string UserMappingsPath();

    void OpenAll(const PadOptions& o);
    void CloseAll();
    // Every event goes through here: controllers added and removed (any time, mid-game), and
    // which one was used last. True when the event was a press or a push of a pad that drives.
    bool Event(const SDL_Event& ev, const PadOptions& o);
    // The driving pad's inputs (none: all zero).
    PadRaw Read(const PadOptions& o) const;
    bool Any() const { return Driving() != nullptr; }
    size_t Count() const { return pads_.size(); }
    // The driving pad: the last one used, else the last one connected (Steam's virtual pads
    // only, when one is open). -1: none.
    SDL_JoystickID Active() const;
    // Its labels (Controls::Style), XBOX without one, and SDL's type for it.
    int Style() const;
    SDL_GameControllerType Type() const;
    // The controllers opened since the last call (the window tells the player how to pause).
    struct Connected
    {
        std::string name;
        int style;
        SDL_GameControllerType type;
    };
    std::vector<Connected> TakeConnected();

    static int StyleOf(SDL_GameControllerType t);
    // Steam's virtual pad, as SDL tells it (SDL_IsJoystickSteamVirtualGamepad).
    static bool SteamVirtual(Uint16 vendor, Uint16 product, Uint16 version);
    // A Valve Steam Controller (not the Deck, not Steam's virtual pad) by its USB ids: through
    // SDL's HIDAPI driver its right pad's click never reaches SDL (R3), its back grip does.
    static bool SteamController(Uint16 vendor, Uint16 product);
    // Whether the game was started by Steam (its environment: SteamGameId, SteamAppId): then
    // Steam Input owns the Steam Controller and gives the game its virtual pad.
    static bool LaunchedBySteam();

private:
    struct Pad
    {
        SDL_GameController* c;
        SDL_JoystickID id;
        bool steam;
        bool rsFromGrip; // a Steam Controller on SDL's HIDAPI driver: R3 is its back grip
        int style;
        SDL_GameControllerType type;
    };
    std::vector<Pad> pads_;
    std::vector<Connected> connected_;
    SDL_JoystickID active_ = -1;

    void Open(int index, const PadOptions& o);
    void Close(SDL_JoystickID id);
    bool Usable(const Pad& p) const;
    const Pad* Find(SDL_JoystickID id) const;
    const Pad* Driving() const;
};

// SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS (on unless the environment says 0): on, SDL's A is the
// button printed A on every pad (on a Switch pad, the right one); off, it is the bottom one, so
// on a Nintendo pad SDL's A is the button printed B (and X is printed Y).
bool NintendoLabels();
// The name printed on a pad's input for a Nintendo pad, `in` swapped when the labels are off.
int PrintedInput(int in, int style, bool nintendoLabels);
// The HD chips' look (ui::SetChipLook): the keyboard's keys bound to the chips' actions when
// the keyboard was used last, else the pad's own buttons in its style (a PlayStation pad's
// Back by its type: Create, Share, Select).
ui::ChipLook ChipLookFor(bool keyboard, int style, const Controls::Bindings& b, bool nintendoLabels,
                         SDL_GameControllerType type = SDL_CONTROLLER_TYPE_UNKNOWN);
// The pause menu's buttons on a pad, by their printed names ("Start + Back", "Options + Create"…).
const char* PauseCombo(int style, SDL_GameControllerType type);
}
