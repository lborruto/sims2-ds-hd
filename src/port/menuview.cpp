#include "port/menuview.h"

#include <cmath>
#include <cstdlib>

namespace sims2wide
{
MenuStep TapStep(int x, int y)
{
    MenuStep s;
    s.kind = MenuStep::Tap;
    s.x = x;
    s.y = y;
    return s;
}

MenuStep DragStep(int x0, int y0, int x1, int y1)
{
    MenuStep s;
    s.kind = MenuStep::Drag;
    s.x = x0;
    s.y = y0;
    s.x2 = x1;
    s.y2 = y1;
    return s;
}

MenuStep KeysStep(u32 keys, int frames)
{
    MenuStep s;
    s.kind = MenuStep::Keys;
    s.keys = keys;
    s.frames = frames;
    return s;
}

MenuStep WaitStep(int frames)
{
    MenuStep s;
    s.kind = MenuStep::Wait;
    s.frames = frames;
    return s;
}

namespace
{
void Mix(u64& h, u64 v) { h = (h ^ v) * 0x100000001B3ull; }

void MixText(u64& h, const std::string& t)
{
    Mix(h, t.size());
    for (char c : t) Mix(h, (u8)c);
}

void MixAction(u64& h, const MenuAction& a)
{
    Mix(h, a.size());
    for (const MenuStep& s : a)
    {
        Mix(h, ((u64)s.kind << 32) | (u32)s.frames);
        Mix(h, ((u64)(u32)s.x << 32) | (u32)s.y);
        Mix(h, ((u64)(u32)s.x2 << 32) | (u32)s.y2);
        Mix(h, s.keys);
    }
}
}

u64 MenuView::Hash() const
{
    u64 h = 0xCBF29CE484222325ull;
    Mix(h, valid);
    Mix(h, items.size());
    for (const ViewItem& it : items)
    {
        Mix(h, ((u64)(u32)it.rect.x << 32) | (u32)it.rect.y);
        Mix(h, ((u64)(u32)it.rect.w << 32) | (u32)it.rect.h);
        Mix(h, ((u64)it.id << 32) | ((u64)(u32)it.group << 1) | (u64)it.enabled);
        MixAction(h, it.action);
        MixAction(h, it.left);
        MixAction(h, it.right);
        MixAction(h, it.buttonX);
        for (int n : it.next) Mix(h, (u64)(u32)n);
        MixText(h, it.word);
        MixText(h, it.xWord);
    }
    for (const MenuAction& a : buttons) MixAction(h, a);
    MixAction(h, back);
    MixAction(h, scrollUp);
    MixAction(h, scrollDown);
    Mix(h, (u64)(u32)scrollGroup);
    Mix(h, ((u64)(u32)initial << 32) | (u32)selected);
    MixText(h, title);
    return h;
}

int MenuFirst(const MenuView& v)
{
    if (MenuFocusable(v, v.initial)) return v.initial;
    for (int i = 0; i < (int)v.items.size(); i++)
        if (v.items[i].enabled) return i;
    return -1;
}

int MenuNeighbour(const MenuView& v, int from, int dx, int dy)
{
    if (from < 0 || from >= (int)v.items.size() || (dx == 0 && dy == 0)) return from;
    // Centres doubled, to stay in whole numbers (as hotspots.cpp's Neighbour).
    auto cx = [&](int i) { return 2 * v.items[i].rect.x + v.items[i].rect.w; };
    auto cy = [&](int i) { return 2 * v.items[i].rect.y + v.items[i].rect.h; };
    const double len = std::sqrt(double(dx * dx + dy * dy));
    int best = from;
    double bestDist = 0, bestAlong = 0;
    for (int i = 0; i < (int)v.items.size(); i++)
    {
        if (i == from || !v.items[i].enabled) continue;
        const double vx = cx(i) - cx(from), vy = cy(i) - cy(from);
        const double dist = std::sqrt(vx * vx + vy * vy);
        if (dist == 0) continue;
        const double along = (vx * dx + vy * dy) / len;
        if (along <= dist * 0.5 + 1e-9) continue; // more than 60 degrees off (or behind)
        if (best == from || dist < bestDist - 1e-9 || (std::abs(dist - bestDist) <= 1e-9 && along > bestAlong))
        {
            best = i;
            bestDist = dist;
            bestAlong = along;
        }
    }
    return best;
}

int MenuMove(const MenuView& v, int from, int dir)
{
    if (from < 0 || from >= (int)v.items.size() || dir < 0 || dir > 3) return from;
    const int to = v.items[from].next[dir];
    if (to != from && MenuFocusable(v, to)) return to;
    static const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
    return MenuNeighbour(v, from, dx[dir], dy[dir]);
}

int MenuKeep(const MenuView& v, u32 id, int old)
{
    for (int i = 0; i < (int)v.items.size(); i++)
        if (v.items[i].id == id && v.items[i].enabled) return i;
    const int n = (int)v.items.size();
    if (n == 0) return -1;
    const int at = old < 0 ? 0 : old >= n ? n - 1 : old;
    for (int d = 0; d < n; d++)
    {
        if (at - d >= 0 && v.items[at - d].enabled) return at - d;
        if (at + d < n && v.items[at + d].enabled) return at + d;
    }
    return -1;
}
}
