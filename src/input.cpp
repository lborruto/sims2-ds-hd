#include "input.h"

#include "port/cas.h"
#include "port/minigame.h"
#include "port/popup.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sims2wide
{
using namespace Controls;

static constexpr int kZoomFramesPerNotch = 6;   // X or Y held this long per wheel notch
static constexpr int kDragPixelsPerFrame = 3;   // a right-button drag: this many points = one frame of L or R

// Marks the keys an input script pushes (a window id no window has): they are not the
// player's, so the chips do not turn to the keyboard for them.
static constexpr Uint32 kScriptedWindow = 0x5C5C5C5Cu;

PadOptions Input::PadOptionsOf(const Settings& s)
{
    PadOptions o;
    o.deadZone = s.stickDeadZone / 100.0f;
    o.trigger = s.triggerThreshold / 100.0f;
    o.rumble = s.rumble;
    return o;
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
            ev.key.windowID = kScriptedWindow;
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

void Input::OpenPads(const Settings& s)
{
    pads.OpenAll(PadOptionsOf(s));
}

void Input::ClosePads()
{
    pads.CloseAll();
}

void Input::HandleEvent(const SDL_Event& ev, App& app, const Video& video, bool menuOpen)
{
    Settings& s = app.settings;
    // Controllers plugged in or out, and which one drives (the last used).
    if (pads.Event(ev, PadOptionsOf(s))) lastKeyboard = false;
    if (ev.type == SDL_QUIT) wantQuit = true;
    else if (ev.type == SDL_WINDOWEVENT)
    {
        if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST || ev.window.event == SDL_WINDOWEVENT_MINIMIZED) focused = false;
        else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) focused = true;
    }
    else if (ev.type == SDL_TEXTINPUT)
    {
        // Create-a-Sim's name: each character (UTF-8) in turn, a few queued at most.
        if (!menuOpen && TypingName(app))
            for (const char* p = ev.text.text; *p;)
            {
                const char* q = p + 1;
                while (*q && ((unsigned char)*q & 0xC0) == 0x80) q++;
                if (typed.size() < 16) typed.emplace_back(p, q);
                p = q;
            }
    }
    else if (ev.type == SDL_DROPFILE)
    {
        if (ev.drop.file) droppedFile = ev.drop.file;
        SDL_free(ev.drop.file);
    }
    else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
    {
        const bool down = ev.type == SDL_KEYDOWN;
        if (down && ev.key.windowID != kScriptedWindow) lastKeyboard = true;
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
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || a == A) nav.a = true;
            else if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || a == B) nav.b = true;
            else if (k == SDLK_DELETE || a == X) nav.x = true;
            else if (a == Y) nav.y = true;
            else if (a == MENU && !ev.key.repeat) wantMenu = true;
            return;
        }
        // Create-a-Sim's Name page: a key that types (a letter, a digit, a sign) types, its action
        // resting; Backspace erases. (The text comes as SDL_TEXTINPUT, after this.)
        if (TypingName(app) && ev.key.windowID != kScriptedWindow)
        {
            if (k == SDLK_BACKSPACE) { if (typed.size() < 16) typed.emplace_back("\b"); return; }
            if (k >= SDLK_SPACE && !(k & SDLK_SCANCODE_MASK) && k != SDLK_DELETE) return;
        }
        if (a >= 0) held[a] = true;
        if (ev.key.repeat) return;
        if (a == MENU) wantMenu = true;
        else if (a == SAVESTATE) wantSave = true;
        else if (a == LOADSTATE) { wantLoad = true; lastMoney_ = -1; }
        else if (a == SWAP) app.mainIsTop = !app.mainIsTop;
        else if (a >= 0) {}
        else if (k == SDLK_0) wantLayout = (int)Layout::Auto;
        else if (k == SDLK_1) wantLayout = (int)Layout::Focus;
        else if (k == SDLK_2) wantLayout = (int)Layout::Side;
        else if (k == SDLK_3) wantLayout = (int)Layout::Stacked;
        else if (k == SDLK_4) wantLayout = (int)Layout::Console;
        else if (k == SDLK_F11) wantFullscreen = true;
        else if (k == SDLK_F12) wantShot = true;
    }
    else if (ev.type == SDL_MOUSEMOTION)
    {
        mouseMovedAt = SDL_GetTicks();
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
        mouseMovedAt = SDL_GetTicks();
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
            int tx, ty;
            stylusDrag = mouseDown && video.TouchAt(app, mouseX, mouseY, tx, ty, &dragPiece);
            if (stylusDrag) { tapFrames = 3; tapX = tx; tapY = ty; }
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
    panelPulse = false;
    // The HD chips: the keys when the keyboard was used last, else the driving pad's buttons.
    padStyle = pads.Style();
    const bool labels = NintendoLabels();
    if (!pads.Any()) lastKeyboard = true; // (no controller: the keyboard's keys, whatever was pressed)
    ui::SetChipLook(ChipLookFor(lastKeyboard, padStyle, s.controls, labels, pads.Type()));
    // One controller drives: the last one used (pads.h), its sticks outside the dead zone.
    pad = pads.Read(PadOptionsOf(s));
    // Left-handed: the two sticks (and their clicks) swapped; the camera with them.
    if (s.leftHanded) SwapSticks(pad);
    if (!pads.Any()) { panelButton = ReleasePulse(); return; }
    // TOUCH MENU on the pad: on release, spoiled by Start (Start + Back opens the pause menu
    // whichever goes down first), by the pause menu and by a binding being captured.
    {
        const int in = s.controls.Pad(PANEL);
        const bool spoil = (pad.btn[IN_START] && in != IN_START) || capturing || menuOpen;
        panelPulse = panelButton.Update(in >= 0 && pad.btn[in], spoil);
    }
    if (capturing)
    {
        // Any button, the menu's too (it may be what the action is being given), taken when it is
        // let go: Start and Back down together then are the cancel (Esc on a pad).
        bool any = false;
        for (int i = 0; i < IN_COUNT; i++)
        {
            if (pad.btn[i] && !prevPad.btn[i])
            {
                if (captureFirst < 0) captureFirst = i;
                captureSeen |= 1u << i;
            }
            any |= pad.btn[i];
        }
        if (captureFirst >= 0 && !any)
        {
            if ((captureSeen & (1u << IN_START)) && (captureSeen & (1u << IN_BACK))) captureCancel = true;
            else newPad = captureFirst;
            captureFirst = -1;
            captureSeen = 0;
        }
        return;
    }
    // The menu opens with its button, or with START and BACK together.
    const bool combo = pad.btn[IN_START] && pad.btn[IN_BACK];
    if (PadPressed(s, MENU) || (combo && !prevCombo)) wantMenu = true;
    prevCombo = combo;
    auto stickUp = [](const PadRaw& p) { return p.btn[IN_UP] || p.ly < -0.5f; };
    auto stickDown = [](const PadRaw& p) { return p.btn[IN_DOWN] || p.ly > 0.5f; };
    auto stickLeft = [](const PadRaw& p) { return p.btn[IN_LEFT] || p.lx < -0.5f; };
    auto stickRight = [](const PadRaw& p) { return p.btn[IN_RIGHT] || p.lx > 0.5f; };
    if (menuOpen)
    {
        // A direction held repeats: after 18 frames, then every 6.
        auto rep = [&](bool now, bool before, int& held) {
            if (!now) { held = 0; return false; }
            held++;
            return !before || (held > 18 && (held - 18) % 6 == 0);
        };
        nav.up |= rep(stickUp(pad), stickUp(prevPad), navHeld[0]);
        nav.down |= rep(stickDown(pad), stickDown(prevPad), navHeld[1]);
        nav.left |= rep(stickLeft(pad), stickLeft(prevPad), navHeld[2]);
        nav.right |= rep(stickRight(pad), stickRight(prevPad), navHeld[3]);
        // (A cleared or moved A / B never locks the menu: without a pad input, the pad's own south / east.)
        nav.a |= shell::PadPress(s.controls, A, IN_A, pad.btn, prevPad.btn);
        nav.b |= shell::PadPress(s.controls, B, IN_B, pad.btn, prevPad.btn);
        nav.x |= PadPressed(s, X);
        nav.y |= PadPressed(s, Y);
        return;
    }
    if (PadPressed(s, SAVESTATE)) wantSave = true;
    if (PadPressed(s, LOADSTATE)) { wantLoad = true; lastMoney_ = -1; }
}

PadState Input::Actions(const Settings& s) const
{
    auto on = [&](int a) { return held[a] || PadOn(s, a); };
    PadState p = scriptPad;
    p.a |= on(A);
    p.b |= on(B);
    p.x |= on(X);
    p.y |= on(Y);
    p.lb |= on(L);
    p.rb |= on(R);
    // The world's camera: the keyboard's L / R keys, a pad's only when the player gave them one.
    p.turnL |= held[L] || (s.controls.PadOwn(L) && PadOn(s, L));
    p.turnR |= held[R] || (s.controls.PadOwn(R) && PadOn(s, R));
    p.start |= on(START);
    p.select |= on(SELECT);
    // The keyboard's on the press; the pad's on release (Poll: Start + Back is the pause menu).
    p.menu |= held[PANEL] || panelPulse;
    p.up |= on(UP);
    p.down |= on(DOWN);
    p.left |= on(LEFT);
    p.right |= on(RIGHT);
    // The sticks and triggers as read (the triggers past half way are buttons too).
    if (pads.Any())
    {
        p.lx = pad.lx;
        p.ly = pad.ly;
        p.rx = pad.rx;
        p.ry = pad.ry;
        p.lt = pad.btn[IN_LT] ? 1.0f : 0.0f;
        p.rt = pad.btn[IN_RT] ? 1.0f : 0.0f;
    }
    p.cursor = s.stickCursor && pads.Any();
    p.speed = s.cursorSpeed / 100.0f;
    return p;
}

void Input::FocusForDrawing(App& app, bool screenChanged)
{
    if (app.shown != Layout::Console || app.consolePinned || !steering) return;
    const GameState* game = app.portSupported ? &app.game : nullptr;
    const MenuView v = ConsoleView(app.ConsoleScreen(), game, app.rom.get());
    if (!v.valid || !(navigator.ViewFocus() || v.own || v.popup)) return;
    if (!screenChanged && v.popup == navigator.PopupUp()) return;
    app.consoleFocus = navigator.FocusFor(v);
}

void Input::Steer(App& app, const PadState& p, u32& keys)
{
    if (app.shown == Layout::Console)
    {
        // The panel's HD menu (port/panelmenu.h), built from the game's state as CONSOLE draws it.
        const ScreenState screen = app.ConsoleScreen();
        const GameState* game = app.portSupported ? &app.game : nullptr;
        const MenuView view = ConsoleView(screen, game, app.rom.get()); // (the same CONSOLE draws: port/popup.h)
        // What RAM says on the minigame (activities.h): the cards' question and message, the
        // pointing games' tools and sliders, a question of the game's popup.
        navigator.SetActivityRam(MinigameRam(game));
        navigator.Update(screen, p, touchScript, keys, app.Settled(), &view);
        steering = true;
        TypeName(app);
        if (!app.consolePinned) { app.consoleMenu = navigator.MenuOpen(); app.consoleFocus = navigator.Focus(); }
        app.consoleArmed = navigator.Armed();
        app.consolePictureClosed = navigator.PictureClosed();
        app.consoleVeiled = navigator.Returning();
        app.consoleWorldOnly = screen.screen == Screen::Unknown && game && (game->between || game->screen == GScreen::Panel || game->vtable == 0)
                                   ? app.consoleWorldOnly + 1
                                   : 0;
        app.consolePointer = {navigator.PointerShown(), navigator.PointerDown(), navigator.QuitArmed(), navigator.PointerX(), navigator.PointerY(),
                              navigator.Mark(), navigator.AskFocus()};
        // The money's last change, shown on the panel's world for kPayoutFrames frames (an
        // animation's payout: the slot machine's coin then its win; a ticket's prize).
        // (Only on the panel and the activities: the front end, Create-a-Sim and a load show another save's money.)
        if (screen.screen == Screen::FrontEnd || screen.screen == Screen::CreateASim || screen.screen == Screen::Loading) lastMoney_ = -1;   // (None is the animations: the slot machine's payout shows there)
        else if (game && game->valid && game->hud.valid)
        {
            if (lastMoney_ >= 0 && game->hud.money != lastMoney_) { app.consolePayout = game->hud.money - lastMoney_; payoutFrames_ = kPayoutFrames; }
            lastMoney_ = game->hud.money;
        }
        if (payoutFrames_ > 0 && --payoutFrames_ == 0) app.consolePayout = 0;
        // The vacuum's Y (headless runs read it here; the window's Frame adds the BLOW binding); the
        // mixer's Y (its VOX): the computer's microphone when the settings choose it, else the blow.
        const bool voiceBlow = navigator.Voice() && app.settings.mic != MicMode::Real;
        app.micBlow = navigator.Blow() || voiceBlow;
        app.micForce = navigator.Blow() || voiceBlow;
    }
    else if (steering)
    {
        // CONSOLE left: no touch running on, no menu, no key held by the navigator.
        navigator.Leave(touchScript);
        steering = false;
        if (!app.consolePinned) { app.consoleMenu = false; app.consoleFocus = -1; }
        app.consoleArmed = 0;
        app.consolePictureClosed = false;
        app.consoleVeiled = false;
        app.consoleWorldOnly = 0;
        app.consolePointer = {};
        app.consolePayout = 0;
        payoutFrames_ = 0;
        lastMoney_ = -1;
        app.micBlow = false;
        app.micForce = false;
    }
}

bool Input::TypingName(const App& app)
{
    if (app.shown != Layout::Console || !app.portSupported || !app.rom) return false;
    const CasState& cs = app.game.front.cas;
    return app.game.valid && cs.valid && cs.page == 0;
}

void Input::TypeName(App& app)
{
    if (typed.empty()) { typeTries = 0; return; }
    if (!TypingName(app)) { typed.clear(); typeTries = 0; return; }
    if (typeWait > 0) { typeWait--; return; }
    // One at a time, when the game shows its keyboard, nothing else runs and the stylus is free.
    const CasState& cs = app.game.front.cas;
    if (cs.switching || !cs.nameKnown || navigator.Running() || touchScript.Owns() || !app.Settled()) return;
    const CasType t = CasTypeAction(cs, *app.rom, typed.front());
    for (const MenuStep& st : t.taps) touchScript.Tap(st.x, st.y);
    // The key's page shown first (its label tapped): the key itself once the game has turned it,
    // at most three tries; a character the game has not, or a full name, is dropped.
    if (t.done || ++typeTries > 3)
    {
        typed.erase(typed.begin());
        typeTries = 0;
    }
    typeWait = t.taps.empty() ? 0 : kCasTypeWait;
}

void Input::PointerTouch(const App& app, bool& touching, int& tx, int& ty)
{
    // A pointing minigame's stylus (activities.h): down at the pointer while the navigator says so
    // (the mouse's drag before it keeps the stylus it holds; a touch script after it wins).
    if (app.shown == Layout::Console && steering && navigator.PointerDown() && !touching)
    {
        touching = true;
        tx = std::clamp((int)navigator.PointerX(), 0, SW - 1);
        ty = std::clamp((int)navigator.PointerY(), 0, SH - 1);
    }
}

void Input::ScriptTouch(bool& touching, int& tx, int& ty)
{
    // A touch script (the console layout's navigator and activities) owns the stylus, through its
    // released frames too, over the mouse, its tap latch and the stick: two taps stay two taps.
    if (touchScript.Owns())
    {
        int sx, sy;
        touching = touchScript.Next(sx, sy);
        if (touching) { tx = sx; ty = sy; }
    }
}

void Input::Frame(App& app, const Video& video, long frame, u32& keys, bool& touching, int& tx, int& ty)
{
    const Settings& s = app.settings;
    // CONSOLE: the buttons go through the navigator; elsewhere they are the DS's.
    Steer(app, Actions(s), keys);
    if (app.shown != Layout::Console)
    {
        for (int a = 0; a < kKeyCount; a++)
            if (held[a] || PadOn(s, a)) keys |= 1u << a;
        // The left stick is the D-pad too.
        if (pad.ly < -0.5f) keys |= 1u << kUp;
        if (pad.ly > 0.5f) keys |= 1u << kDown;
        if (pad.lx < -0.5f) keys |= 1u << kLeft;
        if (pad.lx > 0.5f) keys |= 1u << kRight;
    }
    if (PadPressed(s, SWAP)) app.mainIsTop = !app.mainIsTop;
    const bool fastNow = held[FAST] || PadOn(s, FAST);
    if (s.fastToggle) { if (fastNow && !fastPrev) app.fast = !app.fast; } // pressed once on, once off
    else if (fastNow != fastPrev) app.fast = fastNow;
    fastPrev = fastNow;
    app.micBlow = held[BLOW] || PadOn(s, BLOW) || (app.shown == Layout::Console && (navigator.Blow() || (navigator.Voice() && s.mic != MicMode::Real)));

    // The mouse's camera: the wheel's zoom and the drag's turn, as the game's own buttons.
    if (zoom > 0) { keys |= 1u << kX; zoom--; }
    else if (zoom < 0) { keys |= 1u << kY; zoom++; }
    if (rotate > 0) { keys |= 1u << kR; rotate--; }
    else if (rotate < 0) { keys |= 1u << kL; rotate++; }

    int mx, my;
    if (mouseDown && stylusDrag) { video.TouchClampedAt(app, dragPiece, mouseX, mouseY, mx, my); touching = true; tx = mx; ty = my; }
    else if (tapFrames > 0) { touching = true; tx = tapX; ty = tapY; }
    if (tapFrames > 0) tapFrames--;

    // The stick cursor: faster with a fuller deflection, for both precision and reach.
    // In CONSOLE only where no table of targets is (navigator.h), with A as its press there too;
    // in the vacuum it is the hand, there without a controller too (the D-pad moves it while X or
    // A is held: activities.h).
    const bool console = app.shown == Layout::Console;
    const bool hand = console && navigator.Hand();
    cursorPress = held[TOUCH] || PadOn(s, TOUCH) || (console && navigator.CursorPress());
    if (console && !navigator.CursorScreen()) cursorShownUntil = 0; // (not left over from a screen before)
    if ((s.stickCursor && pads.Any() && (!console || navigator.CursorScreen())) || hand)
    {
        // (The stick reads 0 inside its dead zone: Pads::Read.)
        const float mag = pads.Any() ? std::sqrt(pad.rx * pad.rx + pad.ry * pad.ry) : 0.0f;
        const bool stepped = hand && (navigator.HandDx() != 0 || navigator.HandDy() != 0);
        if (mag > 0 || stepped || cursorPress)
        {
            if (mag > 0)
            {
                const float step = (1.0f + 3.5f * mag) * s.cursorSpeed / 100.0f;
                cursorX = std::clamp(cursorX + pad.rx * step, 0.0f, (float)(SW - 1));
                cursorY = std::clamp(cursorY + pad.ry * step, 0.0f, (float)(SH - 1));
            }
            if (stepped)
            {
                cursorX = std::clamp(cursorX + navigator.HandDx(), 0.0f, (float)(SW - 1));
                cursorY = std::clamp(cursorY + navigator.HandDy(), 0.0f, (float)(SH - 1));
            }
            cursorShownUntil = frame + 180;
        }
        if (hand) cursorShownUntil = frame + 180; // the vacuum's hand never hides
        if (cursorPress && !touching) { touching = true; tx = (int)cursorX; ty = (int)cursorY; }
    }
    // A pointing minigame without its HD frame (no strings): the pointer drawn as the stick cursor's cross.
    if (console && navigator.PointerShown() && !MinigameFramed(app.ConsoleScreen(), app.portSupported ? &app.game : nullptr, app.rom.get()))
    {
        cursorX = navigator.PointerX();
        cursorY = navigator.PointerY();
        cursorPress = navigator.PointerDown();
        cursorShownUntil = frame + 2;
    }
    PointerTouch(app, touching, tx, ty);
    ScriptTouch(touching, tx, ty);
}

void Input::EndFrame()
{
    wantMenu = wantSave = wantLoad = wantShot = wantFullscreen = wantQuit = false;
    wantLayout = -1;
    nav = shell::Input();
    newKey = kDefault;
    newPad = -1;
    captureCancel = false;
}
}
