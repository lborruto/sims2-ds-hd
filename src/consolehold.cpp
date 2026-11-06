#include "consolehold.h"

#include <cstring>

#include "app.h"
#include "port/popup.h"
#include "port/roominfo.h"

namespace sims2wide
{
Composition ConsoleFresh(const App& app, int topW, int topH, ConsoleBranch* branch)
{
    return BuildConsole(app.ConsoleScreen(), app.consoleMenu, app.consoleFocus, topW, topH, app.Hud(), &app.hudStrings,
                        app.portSupported ? &app.game : nullptr, app.rom.get(), app.Bottom(), app.icons.get(), app.consoleArmed,
                        &app.consolePointer, app.consolePayout, app.consolePictureClosed, app.consoleVeiled, app.consoleWorldOnly, branch,
                        app.consolePrompt.frames);
}

bool ConsoleM1ByDesign(const App& app)
{
    const GameState& g = app.game;
    if (!app.portSupported || !app.rom || app.rom->StringCount() == 0 || !app.hudStrings.ok) return true;
    if (!g.valid && !g.between) return true;   // (RAM not reading: the pixels' screen)
    if (g.placement.valid) return true;
    if (g.front.video.valid) return true;                          // (an in-game video)
    if (g.front.boot.valid && g.front.boot.mode == 4) return true; // (the boot's popup splash)
    if (g.front.load.valid && g.front.load.wireless) return true;
    if (g.front.cas.valid && g.front.cas.page >= 5 && !ConsoleView(app.ConsoleScreen(), &g, app.rom.get()).valid) return true;
    if (g.roomInfo.valid && g.roomInfo.hint && g.roomInfo.hintId < 0) return true;
    return false;
}

void ConsoleHold::Frame(const App& app)
{
    ready_ = false;
    holding_ = false;
    if (app.shown != Layout::Console || !app.nds)
    {
        have_ = false;
        return;
    }
    int tw, th;
    const u32* top = app.TopImage(tw, th);
    const u32* bottom = app.Bottom();
    ConsoleBranch b = ConsoleBranch::Town;
    Composition c = ConsoleFresh(app, tw, th, &b);
    branch_ = b;
    frames++;
    if (IsMilestone1(b)) fresh1++;
    if (!IsMilestone1(b))
    {
        // Kept for a hold: its live crops' pixels copied (the DS draws the next frame over them),
        // nothing of it touchable.
        last_ = c;
        lastBranch_ = b;
        bool topUsed = false;
        for (const ui::UiItem& it : last_.ui) topUsed |= it.kind == ui::UiItem::Image && it.src == top && top != bottom;
        bottom_.assign(bottom, bottom + SW * SH);
        if (topUsed) top_.assign(top, top + (size_t)tw * th);
        for (ui::UiItem& it : last_.ui)
        {
            if (it.kind != ui::UiItem::Image) continue;
            if (it.src == bottom) it.src = bottom_.data();
            else if (topUsed && it.src == top) it.src = top_.data();
        }
        for (Piece& p : last_.pieces) p.touch = false;
        have_ = true;
        since_ = 0;
        lastTw_ = tw;
        lastTh_ = th;
        shown_ = std::move(c);
    }
    else if (have_ && since_ < kMaxFrames && tw == lastTw_ && th == lastTh_ && !ConsoleM1ByDesign(app))
    {
        since_++;
        holding_ = true;
        held++;
        shown_ = last_;
    }
    else
    {
        have_ = false;
        shown1++;
        shown_ = std::move(c);
    }
    tw_ = tw;
    th_ = th;
    ready_ = true;
}

Composition ConsoleHold::Shown(const App& app, int topW, int topH) const
{
    if (ready_ && topW == tw_ && topH == th_) return shown_;
    return ConsoleFresh(app, topW, topH);
}
}
