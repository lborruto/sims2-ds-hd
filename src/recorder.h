// The unmapped-screen recorder: the first time the game shows a screen the RAM map does not
// know (an (overlay, vtable) pair gamestate.cpp has no name for), save a state and a picture, so
// the maintainer's normal play collects the screens still to map. Window runs only (or --record-dir).
#pragma once
#include <set>
#include <string>
#include <utility>

#include "NDS.h"
#include "app.h"

namespace sims2hd
{
class Recorder
{
public:
    // dir: where captures go ("" : <SDL pref path>/captures).
    explicit Recorder(std::string dir = "");
    // Call every frame after app.game is read; whether it recorded a screen (the window's toast).
    bool Frame(const App& app, long frame, melonDS::NDS& nds);

private:
    std::string dir_;
    std::set<std::pair<int, u32>> done_;   // pairs recorded this run
    std::pair<int, u32> cur_{-2, 0};       // the pair being watched
    int run_ = 0;                          // consecutive frames on it
    int count_ = 0;                        // captures this run
};
}
