#include "gamestate.h"

#include <cstdio>

namespace sims2wide
{
namespace
{
// EU table (ASJP). Offsets are from P, the manager, or S, the bottom screen object.
constexpr u32 kRamBase = 0x02000000;
constexpr u32 kManagerPtr = 0x02139FA4;                  // P = u32 here
constexpr u32 kBottomObj = 0xDAC, kOverlay = 0xE34, kPopup = 0x8, kSlotState = 0xE42;

struct VtScreen { u32 vt; GScreen screen; };
constexpr VtScreen kPlain[] = {
    {0x0213203C, GScreen::Panel}, {0x02131F08, GScreen::Phone}, {0x02135004, GScreen::Social},
    {0x0212F98C, GScreen::Cards}, {0x02131960, GScreen::FrontEnd},
};
struct OvScreen { int ov; u32 vt; GScreen screen; };
constexpr OvScreen kOverlays[] = {
    {15, 0x0214AA18, GScreen::Dialogue}, {6, 0x0214ABA4, GScreen::Gift},
    {2, 0x0214C194, GScreen::Catalogue}, {1, 0x0214B1D0, GScreen::Build},
    {17, 0x0214C658, GScreen::Vacuum},   {3, 0x021509B0, GScreen::CreateASim},
    {8, 0x0214B1C4, GScreen::SaveSlots}, {18, 0x0214B8BC, GScreen::ItemHand},
};
const char* const kTabs[] = { "Interact", "Pockets", "Goals", "Relationships", "Options" };

struct Reader
{
    RamView r;
    bool In(u32 a, u32 n) const
    {
        if (a < kRamBase) return false;
        u32 o = a - kRamBase;
        return o <= r.mask && n - 1 <= r.mask - o;
    }
    bool R8(u32 a, u32& v) const { if (!In(a, 1)) return false; v = r.ram[a - kRamBase]; return true; }
    bool R16(u32 a, u32& v) const { if (!In(a, 2)) return false; const u8* p = r.ram + (a - kRamBase); v = p[0] | p[1] << 8; return true; }
    bool R32(u32 a, u32& v) const
    {
        if (!In(a, 4)) return false;
        const u8* p = r.ram + (a - kRamBase);
        v = p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
        return true;
    }
    // -1 when unreadable.
    int S8(u32 a) const { u32 v; return R8(a, v) ? (int)v : -1; }
    int S16(u32 a) const { u32 v; return R16(a, v) ? (int)v : -1; }
    int S32(u32 a) const { u32 v; return R32(a, v) ? (int)v : -1; }
};
}

GameState ReadGameState(RamView ram)
{
    GameState g;
    if (!ram.ram) return g;
    Reader m{ram};
    u32 P, S, vt, ovRaw, pop, slot = 0;
    if (!m.R32(kManagerPtr, P) || !m.R32(P + kBottomObj, S) || !m.R32(S, vt)
        || !m.R32(P + kOverlay, ovRaw) || !m.R8(P + kPopup, pop))
        return g;
    g.valid = true;
    g.vtable = vt;
    g.overlay = (int)ovRaw;   // 0xFFFFFFFF is -1
    g.popup = (u8)pop;
    g.settled = m.R8(P + kSlotState, slot) && slot == 3;

    for (const auto& e : kPlain) if (e.vt == vt) { g.screen = e.screen; break; }
    if (g.screen == GScreen::Unknown)
        for (const auto& e : kOverlays) if (e.vt == vt && e.ov == g.overlay) { g.screen = e.screen; break; }

    switch (g.screen)
    {
    case GScreen::Panel:
    {
        int t = m.S32(S + 0x64);
        g.tab = (t >= 0 && t < 5) ? t : -1;
        if (pop >= 1 && pop <= 3) { g.screen = GScreen::Popup; break; }
        if (g.tab == 1) { u32 k; if (m.R32(S + 0x38, k)) g.selection = m.S32(k + 0xE0); }
        else if (g.tab == 3) g.selection = m.S16(S + 0xD0);
        break;
    }
    case GScreen::Phone: g.selection = m.S8(S + 0x89); g.page = m.S8(S + 0x88); break;
    case GScreen::Dialogue: g.page = m.S16(S + 0x34); g.speech = m.S8(S + 0x0C); break;
    case GScreen::Catalogue: g.selection = m.S32(S + 0x254); g.scroll = m.S32(S + 0x268); g.list = m.S32(S + 0x258); break;
    case GScreen::Cards: g.selection = m.S32(S + 0x2E0); break;
    default: break;
    }
    return g;
}

const char* GScreenName(GScreen s)
{
    switch (s)
    {
    case GScreen::FrontEnd: return "FrontEnd";
    case GScreen::CreateASim: return "CreateASim";
    case GScreen::Panel: return "Panel";
    case GScreen::Popup: return "Popup";
    case GScreen::Phone: return "Phone";
    case GScreen::Dialogue: return "Dialogue";
    case GScreen::Gift: return "Gift";
    case GScreen::Catalogue: return "Catalogue";
    case GScreen::Build: return "Build";
    case GScreen::Vacuum: return "Vacuum";
    case GScreen::Social: return "Social";
    case GScreen::Cards: return "Cards";
    case GScreen::SaveSlots: return "SaveSlots";
    case GScreen::ItemHand: return "ItemHand";
    default: return "Unknown";
    }
}

std::string GameStateName(const GameState& g)
{
    if (g.screen == GScreen::Unknown)
    {
        char b[64];
        snprintf(b, sizeof b, "Unknown(ov=%d,vt=0x%08X)", g.overlay, g.vtable);
        return b;
    }
    if (g.screen == GScreen::Panel)
        return std::string("Panel/") + (g.tab >= 0 && g.tab < 5 ? kTabs[g.tab] : "?");
    return GScreenName(g.screen);
}
}
