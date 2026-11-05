// Touches made from code, for the console layout's navigator and activities: a tap, a hold or a
// drag along points, in DS touch-screen pixels (0..255 x 0..191, clamped). Scripts run over game
// frames, one at a time; the input layer asks for this frame's touch once per frame.
#pragma once
#include <deque>
#include <vector>

namespace sims2wide
{
struct Point
{
    int x, y;
};

class TouchScript
{
public:
    // Touch (x, y) for this many game frames (a tap of fewer than 3 may be missed by the game).
    void Tap(int x, int y, int frames = 3);
    void Hold(int x, int y, int frames);
    // Touch each point of the path for framesPerStep frames, the last for holdEnd frames.
    void Drag(std::vector<Point> path, int framesPerStep = 2, int holdEnd = 3);
    // Forgets the running script and the queued ones.
    void Cancel();
    // A script is running or waiting.
    bool Busy() const;
    // The script holds the stylus: running, waiting, or on the released frame after a touch.
    bool Owns() const { return Busy() || release_; }
    // Once per game frame: true and the position while touching, false when idle. A frame is
    // released between two scripts, so the DS sees two separate touches.
    bool Next(int& x, int& y);

private:
    struct Step
    {
        Point p;
        int frames;
    };
    static constexpr size_t kMaxQueued = 8;
    void Queue(std::vector<Step> steps);

    std::deque<std::vector<Step>> queue_;
    std::deque<Step> cur_;
    bool release_ = false; // the next frame is the released one after a finished script
};
}
