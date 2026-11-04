#include "input.h"

#include <cstdio>
#include <map>

#include "shots.h"
#include "state.h"

namespace sims2wide
{
static const std::map<SDL_Keycode, int> kKeymap = {
    {SDLK_x, kA}, {SDLK_z, kB}, {SDLK_s, kX}, {SDLK_a, kY}, {SDLK_q, kL}, {SDLK_w, kR},
    {SDLK_RETURN, kStart}, {SDLK_RSHIFT, kSelect}, {SDLK_UP, kUp}, {SDLK_DOWN, kDown},
    {SDLK_LEFT, kLeft}, {SDLK_RIGHT, kRight}};

void Input::HandleEvent(const SDL_Event& ev, App& app)
{
    if (ev.type == SDL_QUIT) app.quit = true;
    else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
    {
        bool down = ev.type == SDL_KEYDOWN;
        SDL_Keycode k = ev.key.keysym.sym;
        if (auto it = kKeymap.find(k); it != kKeymap.end())
        {
            if (down) heldKeys |= 1u << it->second; else heldKeys &= ~(1u << it->second);
        }
        else if (down && !ev.key.repeat)
        {
            if (k == SDLK_TAB) app.mainIsTop = !app.mainIsTop;
            else if (k == SDLK_0) app.layout = Layout::Auto;
            else if (k == SDLK_1) app.layout = Layout::Focus;
            else if (k == SDLK_2) app.layout = Layout::Side;
            else if (k == SDLK_3) app.layout = Layout::Stacked;
            else if (k == SDLK_F5) fprintf(stderr, "state save: %s\n", SaveState(*app.nds, app.quickState) ? "ok" : "failed");
            else if (k == SDLK_F7) fprintf(stderr, "state load: %s\n", LoadState(*app.nds, app.quickState) ? "ok" : "failed");
            else if (k == SDLK_F12) WriteShot(app.shotFile, app.Top(), app.Bottom());
            else if (k == SDLK_ESCAPE) app.quit = true;
        }
        if (k == SDLK_SPACE) app.fast = down;
    }
    else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) { mouseDown = true; mouseX = ev.button.x; mouseY = ev.button.y; }
    else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) mouseDown = false;
    else if (ev.type == SDL_MOUSEMOTION) { mouseX = ev.motion.x; mouseY = ev.motion.y; }
}

void Input::Frame(const App& app, const Video& video, u32& keys, bool& touching, int& tx, int& ty) const
{
    keys |= heldKeys;
    int mx, my;
    if (mouseDown && video.TouchAt(app, mouseX, mouseY, mx, my)) { touching = true; tx = mx; ty = my; }
}
}
