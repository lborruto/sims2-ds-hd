// sims2hd: a sharper and wider picture of the DS's 3D scene, drawn beside melonDS's own.
//
// Adapted from Pokémon FullDex (GPLv3), melonDS/src/GPU3D_Wide.h; modified for this project, 2026.
// https://github.com/mamesj/pokemon-fulldex
// Ported from Pokemon FullDex (https://github.com/mamesj/pokemon-fulldex, GPLv3), whose
// melonDS/src/GPU3D_Wide.h this is. The DS draws its 3D scene 256x192 pixels large through a
// 4:3 view. Here the frame's polygons are submitted a second time, with the horizontal clip
// coordinate squeezed for a wider view (so that what lies left and right of the DS's view
// survives clipping), and drawn by a separate rasteriser at the size asked for. The emulated
// state is not touched, except for the box test (GPU3D::BoxTest) when the view is wider than
// the DS's: the game then also draws what is only visible beside its own picture.
//
// melonDS calls it through the hooks patches/melonDS/0001 adds to GPU3D.cpp; the frontend sets
// Wide and reads Front.
#pragma once
#include <vector>

#include "types.h"

namespace melonDS
{
class GPU;
class GPU3D;
struct Vertex;
struct Polygon;

struct WideState
{
    GPU3D* Owner = nullptr;    // the console whose scene is drawn; nullptr turns it all off
    int Width = 0, Height = 0; // size to draw at, set by the frontend
    float ViewWidth = 256;     // width of the view in DS pixels (256 = the DS's own view)
    // Polygons ending a little short of the view's edge are continued to it (GPU3D_Wide.cpp,
    // ExtendToEdges): skies made of panels around the camera.
    bool ExtendEdges = true;

    // The picture that belongs to the frame the 2D renderer has just output: XRGB, with the top
    // byte 0 where no polygon was drawn. Three buffers take turns, so it stays as it is until the
    // one after the next is published: it can be read while the next frame is emulated.
    const u32* Front = nullptr;
    int FrontW = 0, FrontH = 0;
    u64 FrontSerial = 0;       // changes whenever Front does
};
extern WideState Wide;

// Blocks until the picture being drawn in the background is finished.
void WideWait();
// Ends the drawing threads (before shutting down, or to start again with a new console).
void WideShutdown();

// Hooks for GPU3D.cpp.
void WideWaitForTextures();
void WideOnFlush(GPU3D& g, bool interpolate);
void WideOnRender(GPU& gpu, bool showingInterp);
void WideReplay(GPU3D& g, bool interpolate, Vertex* vram, Polygon* pram, Polygon** list, u32& numPolys, u32& matched);
}
