// CONSOLE's picture held through the game's swaps: when a frame would fall to one of milestone
// 1's compositions (the game's own screen framed: ConsoleBranch's *M1) for a moment (an object
// swap, a page in flight, a part of RAM not reading for a frame), the last HD picture stays up,
// for kMaxFrames frames at most, and takes no input (the navigator does not run, no piece takes
// the mouse): a tap never lands on a stale picture.
#pragma once
#include <vector>

#include "compose.h"
#include "console.h"

namespace sims2wide
{
struct App;

// The frame's own composition (BuildConsole with the App's state); branch, when given, receives
// which one it is.
Composition ConsoleFresh(const App& app, int topW, int topH, ConsoleBranch* branch = nullptr);

class ConsoleHold
{
public:
    static constexpr int kMaxFrames = 12;
    // After the frame's state is read and its menu and focus are known (main.cpp, before the
    // shots): what CONSOLE shows this frame. Nothing outside CONSOLE.
    void Frame(const App& app);
    // What CONSOLE shows: the frame's composition, or the held one (built fresh when Frame has
    // not run for this top picture's size).
    Composition Shown(const App& app, int topW, int topH) const;
    // The picture shown is a held one: no input (Input::Steer).
    bool Holding() const { return holding_; }
    // The frame's own branch and the shown one.
    ConsoleBranch Branch() const { return branch_; }
    ConsoleBranch ShownBranch() const { return holding_ ? lastBranch_ : branch_; }
    void Reset() { *this = ConsoleHold(); }

    // Counts over the run (--print-console's summary): frames whose own composition was
    // milestone 1's, frames shown so, frames held.
    long fresh1 = 0, shown1 = 0, held = 0, frames = 0;

private:
    bool ready_ = false, holding_ = false, have_ = false;
    int tw_ = 0, th_ = 0, lastTw_ = 0, lastTh_ = 0, since_ = 0;
    ConsoleBranch branch_ = ConsoleBranch::Town, lastBranch_ = ConsoleBranch::Town;
    Composition shown_ {}, last_ {};
    std::vector<u32> bottom_, top_;   // the held picture's pixels (its images' live crops)
};

// Where milestone 1's picture is the design, not a swap (never held): the furniture editor, DS
// Wireless's slots, the in-game Create-a-Sim pages without their view, a room hint not known,
// RAM or the game's words not reading.
bool ConsoleM1ByDesign(const App& app);
}
