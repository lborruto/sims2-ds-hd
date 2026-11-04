#include "input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sims2wide
{
using namespace Controls;

static constexpr int kZoomFramesPerNotch = 6;   // X or Y held this long per wheel notch
static constexpr int kDragPixelsPerFrame = 3;   // a right-button drag: this many points = one frame of L or R
static constexpr float kStickDead = 0.25f;

void Input::OpenPad(int index)
{
    if (!SDL_IsGameController(index)) return;
    SDL_GameController* c = SDL_GameControllerOpen(index);
    if (!c) { fprintf(stderr, "controller %d: %s\n", index, SDL_GetError()); return; }
    const SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
    for (SDL_GameController* p : pads)
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p)) == id) { SDL_GameControllerClose(c); return; }
    pads.push_back(c);
    switch (SDL_GameControllerGetType(c))
    {
    case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5: padStyle = PLAYSTATION; break;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: padStyle = NINTENDO; break;
    default: padStyle = XBOX; break;
    }
    const char* name = SDL_GameControllerName(c);
    fprintf(stderr, "controller: %s\n", name ? name : "?");
}

void Input::PushScript(const std::vector<InputEvent>& script, long iter, const Video& video) const
{
    for (const InputEvent& e : script)
    {
        const bool down = e.frame == iter, up = e.frame + e.len == iter;
        if (!down && !up) continue;
        SDL_Event ev {};
        if (!e.press.empty())
        {
            const SDL_Keycode k = SDL_GetKeyFromName(e.press.c_str());
            if (k == SDLK_UNKNOWN) { fprintf(stderr, "script: unknown key name %s\n", e.press.c_str()); continue; }
            ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
            ev.key.keysym.sym = k;
            ev.key.keysym.scancode = SDL_GetScancodeFromKey(k);
            ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        }
        else if (e.move && down)
        {
            ev.type = SDL_MOUSEMOTION;
            ev.motion.x = e.tx;
            ev.motion.y = e.ty;
        }
        else if (e.click)
        {
            if (down)
            {
                SDL_Event m {};
                m.type = SDL_MOUSEMOTION;
                m.motion.x = e.tx;
                m.motion.y = e.ty;
                SDL_PushEvent(&m);
            }
            ev.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            ev.button.button = SDL_BUTTON_LEFT;
            ev.button.x = e.tx;
            ev.button.y = e.ty;
            ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
        }
        else if (e.wheel && down)
        {
            ev.type = SDL_MOUSEWHEEL;
            ev.wheel.y = e.wheel;
            ev.wheel.preciseY = (float)e.wheel;
            ev.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
        }
        else continue;
        SDL_PushEvent(&ev);
    }
    (void)video;
}

void Input::OpenPads()
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) OpenPad(i);
}

void Input::ClosePads()
{
    for (SDL_GameController* p : pads) SDL_GameControllerClose(p);
    pads.clear();
}

void Input::HandleEvent(const SDL_Event& ev, App& app, const Video& video, bool menuOpen)
{
    Settings& s = app.settings;
    if (ev.type == SDL_QUIT) app.quit = true;
    else if (ev.type == SDL_CONTROLLERDEVICEADDED) OpenPad(ev.cdevice.which);
    else if (ev.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        for (size_t i = 0; i < pads.size(); i++)
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == ev.cdevice.which)
            {
                SDL_GameControllerClose(pads[i]);
                pads.erase(pads.begin() + i);
                break;
            }
    }
    else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
    {
        const bool down = ev.type == SDL_KEYDOWN;
        const SDL_Keycode k = ev.key.keysym.sym;
        const int a = s.controls.ActionOfKey(k);
        if (!down)
        {
            if (a >= 0) held[a] = false;
            return;
        }
        if (capturing)
        {
            if (ev.key.repeat) return;
            if (k == SDLK_ESCAPE) captureCancel = true;
            else if (k == SDLK_BACKSPACE || k == SDLK_DELETE) newKey = kNone;
            else newKey = k;
            return;
        }
        if (menuOpen)
        {
            // The menu: arrows, Enter and Esc, and the DS's A and B keys as on a console.
            if (k == SDLK_UP) nav.up = true;
            else if (k == SDLK_DOWN) nav.down = true;
            else if (k == SDLK_LEFT) nav.left = true;
            else if (k == SDLK_RIGHT) nav.right = true;
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || a == A) nav.accept = true;
            else if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || a == B) nav.back = true;
            else if (a == MENU && !ev.key.repeat) wantMenu = true;
            return;
        }
        if (a >= 0) held[a] = true;
        if (ev.key.repeat) return;
        if (a == MENU) wantMenu = true;
        else if (a == SAVESTATE) wantSave = true;
        else if (a == LOADSTATE) wantLoad = true;
        else if (a == SWAP) app.mainIsTop = !app.mainIsTop;
        else if (a >= 0) {}
        else if (k == SDLK_0) wantLayout = (int)Layout::Auto;
        else if (k == SDLK_1) wantLayout = (int)Layout::Focus;
        else if (k == SDLK_2) wantLayout = (int)Layout::Side;
        else if (k == SDLK_3) wantLayout = (int)Layout::Stacked;
        else if (k == SDLK_F11) wantFullscreen = true;
        else if (k == SDLK_F12) wantShot = true;
    }
    else if (ev.type == SDL_MOUSEMOTION)
    {
        mouseX = ev.motion.x;
        mouseY = ev.motion.y;
        if (menuOpen)
        {
            float px, py;
            video.ToOutput(mouseX, mouseY, px, py);
            nav.hover = true;
            nav.mx = (int)px;
            nav.my = (int)py;
        }
        else if (rightDown)
        {
            // Turning the camera: the drag's distance, in frames of L (left) or R (right).
            const int dx = mouseX - dragX, frames = dx / kDragPixelsPerFrame;
            rotate = std::clamp(rotate + frames, -120, 120);
            dragX += frames * kDragPixelsPerFrame;
        }
    }
    else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP)
    {
        const bool down = ev.type == SDL_MOUSEBUTTONDOWN;
        mouseX = ev.button.x;
        mouseY = ev.button.y;
        if (ev.button.button == SDL_BUTTON_LEFT)
        {
            if (menuOpen && down)
            {
                float px, py;
                video.ToOutput(mouseX, mouseY, px, py);
                nav.click = true;
                nav.mx = (int)px;
                nav.my = (int)py;
            }
            mouseDown = down && !menuOpen;
        }
        else if (ev.button.button == SDL_BUTTON_RIGHT)
        {
            rightDown = down && !menuOpen && video.OverTop(app, mouseX, mouseY);
            dragX = mouseX;
        }
    }
    else if (ev.type == SDL_MOUSEWHEEL && !menuOpen)
    {
        // Zooming: a notch up is X (closer), down is Y (further), each a few frames held.
        if (!video.OverTop(app, mouseX, mouseY)) return;
        float y = ev.wheel.preciseY;
        if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) y = -y;
        zoomAcc += y * kZoomFramesPerNotch;
        const int frames = (int)zoomAcc;
        zoomAcc -= frames;
        zoom = std::clamp(zoom + frames, -90, 90);
    }
}

bool Input::PadOn(const Settings& s, int action) const
{
    const int in = s.controls.Pad(action);
    return in >= 0 && pad.btn[in];
}

bool Input::PadPressed(const Settings& s, int action) const
{
    const int in = s.controls.Pad(action);
    return in >= 0 && pad.btn[in] && !prevPad.btn[in];
}

void Input::Poll(const App& app, bool menuOpen)
{
    const Settings& s = app.settings;
    prevPad = pad;
    pad = PadState();
    if (pads.empty()) return;
    // Each button pressed on any controller; each stick as far as on the one pushed furthest.
    auto merge = [](float& to, float v) { if (std::fabs(v) > std::fabs(to)) to = v; };
    for (SDL_GameController* c : pads)
    {
        for (int i = 0; i <= IN_RIGHT; i++) pad.btn[i] |= SDL_GameControllerGetButton(c, (SDL_GameControllerButton)i) != 0;
        auto axis = [&](SDL_GameControllerAxis a) { return SDL_GameControllerGetAxis(c, a) / 32767.0f; };
        merge(pad.lx, axis(SDL_CONTROLLER_AXIS_LEFTX));
        merge(pad.ly, axis(SDL_CONTROLLER_AXIS_LEFTY));
        merge(pad.rx, axis(SDL_CONTROLLER_AXIS_RIGHTX));
        merge(pad.ry, axis(SDL_CONTROLLER_AXIS_RIGHTY));
        pad.btn[IN_LT] |= axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0.5f;
        pad.btn[IN_RT] |= axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.5f;
    }
    if (capturing)
    {
        // (Any button, the menu's too: it may be what the action is being given.)
        for (int i = 0; i < IN_COUNT; i++)
            if (pad.btn[i] && !prevPad.btn[i]) { newPad = i; break; }
        return;
    }
    // The menu opens with its button, or with START and BACK together.
    const bool combo = pad.btn[IN_START] && pad.btn[IN_BACK];
    if (PadPressed(s, MENU) || (combo && !prevCombo)) wantMenu = true;
    prevCombo = combo;
    auto stickUp = [](const PadState& p) { return p.btn[IN_UP] || p.ly < -0.5f; };
    auto stickDown = [](const PadState& p) { return p.btn[IN_DOWN] || p.ly > 0.5f; };
    auto stickLeft = [](const PadState& p) { return p.btn[IN_LEFT] || p.lx < -0.5f; };
    auto stickRight = [](const PadState& p) { return p.btn[IN_RIGHT] || p.lx > 0.5f; };
    if (menuOpen)
    {
        nav.up |= stickUp(pad) && !stickUp(prevPad);
        nav.down |= stickDown(pad) && !stickDown(prevPad);
        nav.left |= stickLeft(pad) && !stickLeft(prevPad);
        nav.right |= stickRight(pad) && !stickRight(prevPad);
        nav.accept |= PadPressed(s, A);
        nav.back |= PadPressed(s, B);
        return;
    }
    if (PadPressed(s, SAVESTATE)) wantSave = true;
    if (PadPressed(s, LOADSTATE)) wantLoad = true;
}

void Input::Frame(App& app, const Video& video, long frame, u32& keys, bool& touching, int& tx, int& ty)
{
    const Settings& s = app.settings;
    for (int a = 0; a < kKeyCount; a++)
        if (held[a] || PadOn(s, a)) keys |= 1u << a;
    // The left stick is the D-pad too.
    if (pad.ly < -0.5f) keys |= 1u << kUp;
    if (pad.ly > 0.5f) keys |= 1u << kDown;
    if (pad.lx < -0.5f) keys |= 1u << kLeft;
    if (pad.lx > 0.5f) keys |= 1u << kRight;
    if (PadPressed(s, SWAP)) app.mainIsTop = !app.mainIsTop;
    const bool fastNow = held[FAST] || PadOn(s, FAST);
    if (fastNow != fastPrev) app.fast = fastNow;
    fastPrev = fastNow;
    app.micBlow = held[BLOW] || PadOn(s, BLOW);

    // The mouse's camera: the wheel's zoom and the drag's turn, as the game's own buttons.
    if (zoom > 0) { keys |= 1u << kX; zoom--; }
    else if (zoom < 0) { keys |= 1u << kY; zoom++; }
    if (rotate > 0) { keys |= 1u << kR; rotate--; }
    else if (rotate < 0) { keys |= 1u << kL; rotate++; }

    int mx, my;
    if (mouseDown && video.TouchAt(app, mouseX, mouseY, mx, my)) { touching = true; tx = mx; ty = my; }

    // The stick cursor: faster with a fuller deflection, for both precision and reach.
    cursorPress = held[TOUCH] || PadOn(s, TOUCH);
    if (s.stickCursor && !pads.empty())
    {
        const float mag = std::sqrt(pad.rx * pad.rx + pad.ry * pad.ry);
        if (mag > kStickDead || cursorPress)
        {
            if (mag > kStickDead)
            {
                const float step = 1.0f + 3.5f * mag;
                cursorX = std::clamp(cursorX + pad.rx * step, 0.0f, (float)(SW - 1));
                cursorY = std::clamp(cursorY + pad.ry * step, 0.0f, (float)(SH - 1));
            }
            cursorShownUntil = frame + 180;
        }
        if (cursorPress && !touching) { touching = true; tx = (int)cursorX; ty = (int)cursorY; }
    }
}

void Input::EndFrame()
{
    wantMenu = wantSave = wantLoad = wantShot = wantFullscreen = false;
    wantLayout = -1;
    nav = MenuInput();
    newKey = kDefault;
    newPad = -1;
    captureCancel = false;
}
}
