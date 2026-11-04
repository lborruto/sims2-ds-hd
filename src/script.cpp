#include "script.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace sims2wide
{
static const std::map<std::string, int> kKeyNames = {
    {"a", kA}, {"b", kB}, {"select", kSelect}, {"start", kStart}, {"right", kRight}, {"left", kLeft},
    {"up", kUp}, {"down", kDown}, {"r", kR}, {"l", kL}, {"x", kX}, {"y", kY}};

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
        if (frame >= e.frame && frame < e.frame + e.len)
        {
            keys |= e.keys;
            if (e.blow) blow = true;
            if (e.touch) { touching = true; tx = e.tx; ty = e.ty; }
        }
}
}
