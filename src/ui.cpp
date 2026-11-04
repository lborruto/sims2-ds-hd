#include "ui.h"

#include <algorithm>
#include <cstring>

namespace sims2wide
{
void Canvas::Resize(int nw, int nh)
{
    if (nw == w && nh == h) return;
    w = nw;
    h = nh;
    px.assign((size_t)w * h, 0);
    x0d = y0d = 0;
    x1d = w;
    y1d = h;
}

void Canvas::Clear()
{
    for (int yy = y0d; yy < y1d; yy++)
        std::fill(px.begin() + (size_t)yy * w + x0d, px.begin() + (size_t)yy * w + x1d, 0u);
    x0d = w;
    y0d = h;
    x1d = y1d = 0;
}

void Canvas::Fill(int x, int y, int rw, int rh, u32 argb)
{
    const int x0 = std::max(x, 0), y0 = std::max(y, 0), x1 = std::min(x + rw, w), y1 = std::min(y + rh, h);
    if (x1 <= x0 || y1 <= y0) return;
    x0d = std::min(x0d, x0);
    y0d = std::min(y0d, y0);
    x1d = std::max(x1d, x1);
    y1d = std::max(y1d, y1);
    for (int yy = y0; yy < y1; yy++)
        std::fill(px.begin() + (size_t)yy * w + x0, px.begin() + (size_t)yy * w + x1, argb);
}

// 5 x 7 glyphs, one row per byte, bit 4 the leftmost pixel.
static const u8* Glyph(char c)
{
    static const u8 font[][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
        {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
        {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // J
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, // W
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, // 3
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
        {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
        {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, // 9
        {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}, // :
        {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // -
        {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, // +
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}, // .
        {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}, // /
        {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}, // <
        {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}, // >
        {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, // !
        {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}, // (
        {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}, // )
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // (space)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08}, // ,
    };
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c >= 'A' && c <= 'Z') return font[c - 'A'];
    if (c >= '0' && c <= '9') return font[26 + c - '0'];
    static const char punct[] = ":-+./<>!() ,";
    const char* p = c ? strchr(punct, c) : nullptr;
    return p ? font[36 + (p - punct)] : nullptr;
}

void Canvas::Text(int x, int y, const std::string& s, int scale, u32 argb)
{
    for (char c : s)
    {
        if (const u8* g = Glyph(c))
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (g[row] & (0x10 >> col)) Fill(x + col * scale, y + row * scale, scale, scale, argb);
        x += 6 * scale;
    }
}

int Menu::Update(const MenuInput& in, int& dir)
{
    dir = 0;
    if (items.empty()) return -1;
    const int n = (int)items.size();
    if (in.up) selected = (selected + n - 1) % n;
    if (in.down) selected = (selected + 1) % n;
    bool left = in.left, right = in.right, accept = in.accept;
    if (in.hover || in.click)
    {
        const int item = ItemAt(in.mx, in.my);
        if (item >= 0 && (in.click || item != hoverItem)) selected = item;
        hoverItem = item;
        if (in.click)
        {
            if (item == -2) return -1; // (the caller treats a click outside as back: see Menu's users)
            if (item >= 0)
            {
                if (items[item].value.empty()) accept = true;
                else if (in.mx < drawnX + drawnW / 3) left = true;
                else right = true;
            }
        }
    }
    if (in.back) return -1;
    if (accept) return selected;
    if (left && !items[selected].value.empty()) { dir = -1; return selected; }
    if (right && !items[selected].value.empty()) { dir = 1; return selected; }
    return -1;
}

int Menu::ItemAt(int x, int y) const
{
    if (drawnW <= 0 || x < drawnX || x >= drawnX + drawnW || y < drawnY || y >= drawnY + drawnH) return -2;
    if (drawnLineH <= 0 || y < drawnFirstY) return -1;
    const int i = scroll + (y - drawnFirstY) / drawnLineH;
    return i < (int)items.size() ? i : -1;
}

void Menu::Draw(Canvas& c, const std::string& title, const std::string& hint) const
{
    const int scale = c.Scale();
    int lineH = 11 * scale;
    // As many rows as fit (closer together down to 9 * scale); the rest scroll into view.
    const int fit = std::max(1, (c.h - 4 - 6 * scale) / (9 * scale) - 3);
    const int shown = std::min((int)items.size(), fit);
    if (selected < scroll) scroll = selected;
    if (selected >= scroll + shown) scroll = selected - shown + 1;
    scroll = std::max(0, std::min(scroll, (int)items.size() - shown));
    const int rows = shown + 3; // the title, the rows, the hint
    if (lineH * rows + 6 * scale > c.h - 4) lineH = std::max(9 * scale, (c.h - 4 - 6 * scale) / rows);
    int boxW = 0;
    for (const MenuItem& it : items)
        boxW = std::max(boxW, Canvas::TextWidth(it.label, scale) + (it.value.empty() ? 0 : Canvas::TextWidth("< " + it.value + " >", scale) + 12 * scale) + 12 * scale);
    boxW = std::max(boxW, Canvas::TextWidth(title, scale) + 12 * scale);
    boxW = std::max(boxW, 200 * scale);
    boxW = std::min(boxW, c.w - 8);
    const int boxH = lineH * rows + 6 * scale;
    const int bx = (c.w - boxW) / 2, by = std::max(2, (c.h - boxH) / 2);
    drawnX = bx;
    drawnY = by;
    drawnW = boxW;
    drawnH = boxH;
    drawnLineH = lineH;
    drawnFirstY = by + lineH + 3 * scale;

    c.Fill(0, 0, c.w, c.h, 0xA0000000);
    c.Fill(bx - scale, by - scale, boxW + 2 * scale, boxH + 2 * scale, 0xFFE8E8E8);
    c.Fill(bx, by, boxW, boxH, 0xFF1C2A44);
    c.Text(bx + (boxW - Canvas::TextWidth(title, scale)) / 2, by + 3 * scale, title, scale, 0xFFFFD75E);

    int y = by + lineH + 5 * scale;
    // (Marks at the right edge where rows are scrolled out of view.)
    if (scroll > 0) c.Text(bx + boxW - 8 * scale, y - lineH + 2 * scale, "+", scale, 0xFF8FA0C0);
    if (scroll + shown < (int)items.size()) c.Text(bx + boxW - 8 * scale, y + shown * lineH - 2 * scale, "+", scale, 0xFF8FA0C0);
    for (int i = scroll; i < scroll + shown; i++)
    {
        const bool sel = i == selected;
        if (sel) c.Fill(bx + 2 * scale, y - 2 * scale, boxW - 4 * scale, lineH, 0xFF3D6FD1);
        const u32 col = sel ? 0xFFFFFFFF : 0xFFC8D0E0;
        c.Text(bx + 6 * scale, y, items[i].label, scale, col);
        if (!items[i].value.empty())
        {
            const std::string v = "< " + items[i].value + " >";
            c.Text(bx + boxW - 6 * scale - Canvas::TextWidth(v, scale), y, v, scale, col);
        }
        y += lineH;
    }
    const std::string& h = selected >= 0 && selected < (int)items.size() && !items[selected].hint.empty() ? items[selected].hint : hint;
    c.Text(bx + (boxW - Canvas::TextWidth(h, scale)) / 2, y + 4 * scale, h, scale, 0xFF8FA0C0);
}
}
