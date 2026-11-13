#include "touchscript.h"

#include <algorithm>

#include "common.h"

namespace sims2hd
{
static int Clamp(int v, int hi) { return std::clamp(v, 0, hi); }

void TouchScript::Queue(std::vector<Step> steps)
{
    if (steps.empty() || queue_.size() >= kMaxQueued) return;
    queue_.push_back(std::move(steps));
}

void TouchScript::Tap(int x, int y, int frames) { Hold(x, y, frames); }

void TouchScript::Hold(int x, int y, int frames) { Queue({{{x, y}, std::max(frames, 1)}}); }

void TouchScript::Drag(std::vector<Point> path, int framesPerStep, int holdEnd)
{
    std::vector<Step> steps;
    for (size_t i = 0; i < path.size(); i++)
        steps.push_back({path[i], std::max(i + 1 == path.size() ? holdEnd : framesPerStep, 1)});
    Queue(std::move(steps));
}

void TouchScript::Cancel()
{
    release_ = release_ || !cur_.empty(); // the lift still owed to the DS
    queue_.clear();
    cur_.clear();
}

bool TouchScript::Busy() const { return !cur_.empty() || !queue_.empty(); }

bool TouchScript::Next(int& x, int& y)
{
    if (cur_.empty())
    {
        if (release_) { release_ = false; return false; }
        if (queue_.empty()) return false;
        cur_.assign(queue_.front().begin(), queue_.front().end());
        queue_.pop_front();
    }
    Step& s = cur_.front();
    x = Clamp(s.p.x, SW - 1);
    y = Clamp(s.p.y, SH - 1);
    if (--s.frames <= 0)
    {
        cur_.pop_front();
        if (cur_.empty()) release_ = true;
    }
    return true;
}
}
