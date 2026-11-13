// Copies of the game's save, in a backups/ folder beside it: one when the program starts and one
// every half hour, the last eight kept. A copy identical to the newest one is not made.
#pragma once
#include <string>

namespace sims2hd
{
struct Backups
{
    std::string savePath;
    unsigned lastTick = 0; // SDL_GetTicks() of the last copy

    // At start, and then from the frame loop (copies when half an hour has passed).
    void Start(const std::string& savePath);
    // Whether a copy was made (the window's toast).
    bool Tick(unsigned nowTicks);
    // The copy itself; what it did, for the terminal and the menu ("" when nothing to copy).
    std::string Copy() const;
};
}
