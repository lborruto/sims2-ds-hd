#include "screens.h"

namespace sims2wide
{
// A row of the screen is one flat colour from x0 to x1 (sampled every 8 pixels; by default from
// edge to edge, x 8 to 248).
static bool FlatRow(const u32* screen, int y, u32 rgb, int x0 = 8, int x1 = 248)
{
    for (int x = x0; x <= x1; x += 8)
        if ((screen[y * SW + x] & 0xFFFFFF) != rgb) return false;
    return true;
}

// Fixed samples of row y, every `step` pixels from x0.
template <int N>
static bool RowSamples(const u32* screen, int y, int x0, int step, const u32 (&rgb)[N])
{
    for (int i = 0; i < N; i++)
        if ((screen[y * SW + x0 + i * step] & 0xFFFFFF) != rgb[i]) return false;
    return true;
}

bool HudVisible(const u32* top)
{
    // The status bar's frame, none of it under the Sanity bar's tint: the strip's top edge, row
    // 176 #597192, and the row under its bottom border, row 191 #384961, both flat from x 8 to
    // 248; the bottom border itself, row 190 #415969, only under the clock and the money (x 8
    // to 136). Rows 177 to 190 under the bar's box (from x 144) take the bar's colour: row 190
    // is #415969 with the red bar, #495969 with the yellow one (the complete save's casino and
    // late hotel lobby), and the green and silver levels were not seen. Checked on every frame of
    // every situation shot so far: only gameplay matches, the three rows always together.
    return FlatRow(top, 176, 0x597192) && FlatRow(top, 191, 0x384961) && FlatRow(top, 190, 0x415969, 8, 136);
}

bool SocialVisible(const u32* top)
{
    // The top edge of the name and relationship box, from x 5 to 252, on every frame of the
    // Impress game (act-social); on no other screen seen (the HUD's row 190 is another colour).
    return HudVisible(top) && FlatRow(top, 0, 0x597192);
}

bool CardsVisible(const u32* top)
{
    // Keelhaulin' Cards (the casino): the score bar across the top screen, a flat dark brown
    // row 17 (#594120) and the gold line of row 19 (#CB9A38, from x 40: its left end is
    // shaded). The same in every frame of a 4000-frame game; no other state matches.
    return FlatRow(top, 17, 0x594120) && FlatRow(top, 19, 0xCB9A38, 40, 248);
}

bool VacuumVisible(const u32* bottom)
{
    // The vacuum's bowl keeps the town's HUD on top; its frame on the touch screen is told by
    // row 1: the light blue top edge of the gauge (top left, x 4 to 44) and the dark teal frame
    // right of it (x 60 to 100). On every frame of the vacuum (act-vacuum and runs from it), on
    // no other screen seen.
    if (FlatRow(bottom, 1, 0x30A2D3, 4, 44) && FlatRow(bottom, 1, 0x304951, 60, 100)) return true;
    // Clogged (dust touched with the stylus, until the player blows): the game tints the whole
    // screen sepia, dithered, so the same row reads differently but just as steadily (act-vacuum-clog).
    static const struct { int x; u32 rgb; } kClogged[] = {
        {4, 0x51A2BA}, {12, 0x51A2BA}, {20, 0x51A2BA}, {28, 0x459AB6}, {36, 0x459AB6}, {44, 0x459AB6},
        {60, 0x515D59}, {68, 0x515D59}, {76, 0x455555}, {84, 0x455555}, {92, 0x515D59}};
    for (const auto& p : kClogged)
        if ((bottom[SW + p.x] & 0xFFFFFF) != p.rgb) return false;
    return true;
}

static u32 CornerMean(const u32* screen, int x0, int y0)
{
    int r = 0, g = 0, b = 0;
    for (int y = y0; y < y0 + 8; y++)
        for (int x = x0; x < x0 + 8; x++)
        {
            u32 p = screen[y * SW + x];
            r += (p >> 16) & 0xFF; g += (p >> 8) & 0xFF; b += p & 0xFF;
        }
    return ((u32)(r / 64) << 16) | ((u32)(g / 64) << 8) | (u32)(b / 64);
}

bool WarmMenuVisible(const u32* bottom)
{
    // The shop's catalogue (parchment) and Build Rooms (wood) fill the screen with warm, bright
    // colours up to its corners; the panel (#5961A2 to #606EAD), the phone (grey), dialogues
    // and the paused panel are cool.
    const int corners[4][2] = {{0, 0}, {248, 0}, {0, 184}, {248, 184}};
    for (auto& c : corners)
    {
        u32 m = CornerMean(bottom, c[0], c[1]);
        int r = (m >> 16) & 0xFF, g = (m >> 8) & 0xFF, b = m & 0xFF;
        if (r < b + 40 || (r * 2 + g * 5 + b) / 8 < 100) return false;
    }
    return true;
}

// The panel's selected tab: the tab column (x 0 to 34) has five round icons centred on rows 22,
// 57, 92, 127 and 162; the selected one has a green ring (#309A41), no other one any of that
// colour. A tab counts as selected with 16 or more ring pixels in its 35 x 35 square (measured:
// Interact 51, Pockets 45, Goals 26, Relationships 42, Options 47, the same on every frame; 0
// everywhere else, the dimmed panel under a popup included).
static Tab SelectedTab(const u32* bottom)
{
    static const int kRows[5] = {22, 57, 92, 127, 162};
    for (int i = 0; i < 5; i++)
    {
        int n = 0;
        for (int y = kRows[i] - 17; y <= kRows[i] + 17; y++)
            for (int x = 0; x <= 34; x++)
                n += (bottom[y * SW + x] & 0xFFFFFF) == 0x309A41;
        if (n >= 16) return (Tab)(i + 1);
    }
    return Tab::None;
}

ScreenState Recognise(const u32* top, const u32* bottom)
{
    if (!HudVisible(top)) return {CardsVisible(top) ? Screen::Cards : Screen::None, Tab::None};
    if (SocialVisible(top)) return {Screen::Social};
    if (VacuumVisible(bottom)) return {Screen::Vacuum};
    if (WarmMenuVisible(bottom))
    {
        // The catalogue: the bottom edge of the item's description box, row 62 #51518A and row
        // 63 #302008 flat from x 12 to 244, under the text whether it takes one line or two
        // (sit-storebuy, items 3 and 4 selected).
        if (FlatRow(bottom, 62, 0x51518A, 12, 244) && FlatRow(bottom, 63, 0x302008, 12, 244)) return {Screen::Catalogue};
        // Build Rooms: the wood's grain, row 10 #695128 and row 23 #302008 flat from x 36 to
        // 219, behind its title (sit-build).
        if (FlatRow(bottom, 10, 0x695128, 36, 219) && FlatRow(bottom, 23, 0x302008, 36, 219)) return {Screen::Build};
        return {Screen::Unknown};
    }
    // The phone: its green LCD, row 105 #9AB29A from x 34 to 221 (s7, and opened from the panel).
    if (FlatRow(bottom, 105, 0x9AB29A, 34, 221)) return {Screen::Phone};
    // A popup (the goal unlocked box, the tip, Game Paused): the game dims the whole panel behind
    // its box; the panel's top and bottom edges, rows 0 and 191 from x 36 (right of the tab
    // column) to 252, go from #6171B2 to #303859, wherever the box sits (s6, sit-pause). The
    // phone opening fades the panel to black through that same dim (2 frames), so the box must
    // be there too: its fill, row 120 #828ACB from x 49 to 113 and 144 to 200 (left and right of
    // the X, which sits at 126,110 or 126,126), the same in the goal box, the tip and Game Paused.
    if (FlatRow(bottom, 0, 0x303859, 36, 252) && FlatRow(bottom, 191, 0x303859, 36, 252) &&
        FlatRow(bottom, 120, 0x828ACB, 49, 113) && FlatRow(bottom, 120, 0x828ACB, 144, 200))
        return {Screen::Popup};
    if (Tab t = SelectedTab(bottom); t != Tab::None) return {Screen::Panel, t};
    // A conversation: the blue "next" arrow bottom right, row 171 sampled every 3 pixels from
    // x 214 (s5, sit-dialogue, sit-concierge: the same on every frame).
    static const u32 kArrow[] = {0x828ACB, 0x828ACB, 0x383861, 0x71799A, 0x5171A2, 0x5979AA,
                                 0x08288A, 0x08288A, 0x08499A, 0x08499A, 0x7192B2};
    if (RowSamples(bottom, 171, 214, 3, kArrow)) return {Screen::Dialogue};
    // Give Item: the round bow button between the slots and the portrait, row 86 sampled every
    // 3 pixels from x 112 (sit-gift).
    static const u32 kBow[] = {0x71799A, 0x698AAA, 0x000082, 0x000082, 0x7192B2, 0x08288A,
                               0x08288A, 0x08288A, 0x79A2BA, 0x5979AA, 0x202849};
    if (RowSamples(bottom, 86, 112, 3, kBow)) return {Screen::Gift};
    return {Screen::Unknown};
}

ScreenState ScreenTracker::Update(const u32* top, const u32* bottom)
{
    ScreenState now = Recognise(top, bottom);
    if (now == current) { streak = 0; return current; }
    if (now == candidate) streak++;
    else { candidate = now; streak = 1; }
    int need = now.screen == Screen::Popup || now.screen == Screen::Dialogue ? 3
             : now.screen == Screen::Unknown ? 60 : 6;
    if (streak >= need) { current = now; streak = 0; }
    return current;
}

bool ScreensBlack(const u32* top, const u32* bottom)
{
    for (const u32* p : {top, bottom})
        for (int i = 0; i < SW * 192; i++)
        {
            const u32 c = p[i];
            if ((int)((c >> 16) & 0xFF) > kBlackMax || (int)((c >> 8) & 0xFF) > kBlackMax || (int)(c & 0xFF) > kBlackMax) return false;
        }
    return true;
}

const char* ScreenName(Screen s)
{
    static const char* const kNames[] = {"None", "Panel", "Phone", "Dialogue", "Popup", "Gift", "Social",
                                         "Vacuum", "Cards", "Catalogue", "Build", "FrontEnd", "CreateASim",
                                         "SaveSlots", "Drencher", "Unknown", "Loading",
                                         "Autopsy", "Detector", "Art", "MooGoo", "Crime", "Scratch", "Mixer", "MixerSave", "Trading"};
    return kNames[(int)s];
}

const char* TabName(Tab t)
{
    static const char* const kNames[] = {"None", "Interact", "Pockets", "Goals", "Relationships", "Options"};
    return kNames[(int)t];
}

std::string ScreenStateName(ScreenState s)
{
    std::string n = ScreenName(s.screen);
    if (s.tab != Tab::None) n += std::string("/") + TabName(s.tab);
    return n;
}
}
