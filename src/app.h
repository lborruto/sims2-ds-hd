// The running program's shared state: the emulated DS and what the window shows. Each part
// (input, video, audio, scene) reads and changes it; main.cpp runs the frame loop.
#pragma once
#include <memory>
#include <string>

#include "GPU.h"
#include "NDS.h"
#include "common.h"
#include "layout.h"
#include "scene.h"
#include "settings.h"

namespace sims2wide
{
struct App
{
    std::unique_ptr<melonDS::NDS> nds;

    Layout layout = Layout::Auto;  // what the player chose (AUTO or a fixed one)
    Layout shown = Layout::Side;   // what the window shows this frame (AUTO resolved)
    bool mainIsTop = true;         // FOCUS: which screen is big
    AutoLayout autoLayout;

    Settings settings;

    bool fast = false;             // fast forward held
    bool quit = false;
    bool micBlow = false;          // the "blow into the microphone" key is held

    // The shape of the top screen's picture, width / height: 4/3 on the DS; wider when the
    // widescreen world is on. Layouts size the top screen from it.
    float topAspect = 4.0f / 3.0f;

    std::string quickState;        // F5 / F7
    std::string shotFile;          // F12

    const u32* Top() const { return nds->GPU.Framebuffer[nds->GPU.FrontBuffer][0].get(); }
    const u32* Bottom() const { return nds->GPU.Framebuffer[nds->GPU.FrontBuffer][1].get(); }
};
}
