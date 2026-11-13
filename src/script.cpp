#include "script.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

#include "layout.h"

namespace sims2hd
{
static const std::map<std::string, int> kKeyNames = {
    {"a", kA}, {"b", kB}, {"select", kSelect}, {"start", kStart}, {"right", kRight}, {"left", kLeft},
    {"up", kUp}, {"down", kDown}, {"r", kR}, {"l", kL}, {"x", kX}, {"y", kY}};

// Sets the field of the navigator's action `name`; false for an unknown name.
static bool PadAction(const std::string& name, PadState& p)
{
    static const std::map<std::string, bool PadState::*> kPad = {
        {"a", &PadState::a}, {"b", &PadState::b}, {"x", &PadState::x}, {"y", &PadState::y},
        {"l", &PadState::lb}, {"lb", &PadState::lb}, {"r", &PadState::rb}, {"rb", &PadState::rb},
        {"start", &PadState::start}, {"select", &PadState::select}, {"menu", &PadState::menu},
        {"up", &PadState::up}, {"down", &PadState::down}, {"left", &PadState::left}, {"right", &PadState::right}};
    // The sticks, held over: "lx-" left, "lx+" right, "ly-" up, "ly+" down (rx, ry the right one);
    // "lt" the slow trigger.
    static const std::map<std::string, std::pair<float PadState::*, float>> kAxes = {
        {"lx-", {&PadState::lx, -1}}, {"lx+", {&PadState::lx, 1}}, {"ly-", {&PadState::ly, -1}}, {"ly+", {&PadState::ly, 1}},
        {"rx-", {&PadState::rx, -1}}, {"rx+", {&PadState::rx, 1}}, {"ry-", {&PadState::ry, -1}}, {"ry+", {&PadState::ry, 1}},
        {"lt", {&PadState::lt, 1}}};
    if (auto ax = kAxes.find(name); ax != kAxes.end()) { p.*(ax->second.first) = ax->second.second; return true; }
    // The right stick pushed fully left / right (Create-a-Sim's model turns).
    if (name == "rsleft" || name == "rsright") { p.rx = name == "rsleft" ? -1.0f : 1.0f; return true; }
    auto it = kPad.find(name);
    if (it == kPad.end()) return false;
    p.*(it->second) = true;
    return true;
}

std::vector<InputEvent> LoadScript(const std::string& path)
{
    std::vector<InputEvent> evs;
    std::ifstream in(path);
    if (!in) { fprintf(stderr, "cannot read input script %s\n", path.c_str()); exit(1); }
    std::string line;
    while (std::getline(in, line))
    {
        if (auto h = line.find('#'); h != std::string::npos) line.resize(h);
        std::istringstream ss(line);
        InputEvent e {};
        std::string what;
        if (!(ss >> e.frame >> e.len >> what)) continue;
        if (what == "touch") { e.touch = true; ss >> e.tx >> e.ty; }
        else if (what == "press") { ss >> std::ws; std::getline(ss, e.press); }
        else if (what == "move") { e.move = true; ss >> e.tx >> e.ty; }
        else if (what == "click") { e.click = true; ss >> e.tx >> e.ty; }
        else if (what == "wheel") ss >> e.wheel;
        else if (what == "winshot") e.winShot = true;
        else if (what == "type")
        {
            // Text typed on the keyboard (Create-a-Sim's name): the rest of the line, "<bs>" a backspace.
            ss >> std::ws;
            std::getline(ss, e.text);
            for (size_t at; (at = e.text.find("<bs>")) != std::string::npos;) e.text.replace(at, 4, "\b");
            if (e.text.empty()) { fprintf(stderr, "type: no text\n"); exit(1); }
        }
        else if (what == "layout")
        {
            std::string name;
            ss >> name;
            for (Layout l : {Layout::Auto, Layout::Focus, Layout::Side, Layout::Stacked, Layout::Console})
                if (name == LayoutName(l)) e.layout = (int)l;
            if (e.layout < 0) { fprintf(stderr, "unknown layout %s\n", name.c_str()); exit(1); }
        }
        else if (what == "pad")
        {
            ss >> e.pad;
            PadState p;
            if (!PadAction(e.pad, p)) { fprintf(stderr, "unknown pad action %s\n", e.pad.c_str()); exit(1); }
        }
        else
        {
            std::stringstream ks(what);
            std::string k;
            while (std::getline(ks, k, '+'))
            {
                if (k == "blow") { e.blow = true; continue; }
                auto it = kKeyNames.find(k);
                if (it == kKeyNames.end()) { fprintf(stderr, "unknown key %s\n", k.c_str()); exit(1); }
                e.keys |= 1u << it->second;
            }
        }
        evs.push_back(e);
    }
    return evs;
}

void ApplyScript(const std::vector<InputEvent>& script, long frame, u32& keys, bool& blow, bool& touching, int& tx, int& ty)
{
    for (const auto& e : script)
        if (frame >= e.frame && frame < e.frame + e.len && e.press.empty() && e.pad.empty() && e.layout < 0 && !e.move && !e.click && !e.wheel && !e.winShot && e.text.empty())
        {
            keys |= e.keys;
            if (e.blow) blow = true;
            if (e.touch) { touching = true; tx = e.tx; ty = e.ty; }
        }
}

void ScriptPad(const std::vector<InputEvent>& script, long frame, PadState& pad)
{
    pad = PadState();
    for (const auto& e : script)
        if (!e.pad.empty() && frame >= e.frame && frame < e.frame + e.len) PadAction(e.pad, pad);
}

int ScriptLayout(const std::vector<InputEvent>& script, long frame)
{
    int l = -1;
    for (const auto& e : script)
        if (e.layout >= 0 && e.frame == frame) l = e.layout;
    return l;
}

bool ScriptWindowShot(const std::vector<InputEvent>& script, long iter)
{
    for (const auto& e : script)
        if (e.winShot && e.frame == iter) return true;
    return false;
}
}

namespace sims2hd
{
std::string ScriptText(const std::vector<InputEvent>& script, long frame)
{
    std::string t;
    for (const InputEvent& e : script)
        if (!e.text.empty() && e.frame == frame) t += e.text;
    return t;
}
}
