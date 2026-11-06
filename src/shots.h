// PNG pictures: both raw screens, or the window's picture for a layout (headless previews).
#pragma once
#include <string>
#include <vector>

#include "common.h"
#include "compose.h"

namespace sims2wide
{
void WriteShot(const std::string& path, const u32* top, const u32* bottom);
// Any RGB picture (the window's, from video.cpp).
void WriteRgb(const std::string& path, int w, int h, const u8* rgb);
// The window's picture on the CPU (nearest scaling): composition c (Video::Composed) drawn with
// its overlays and its UI items. The top screen's picture is topW x topH (App::TopImage: bigger
// than 256 x 192 with the looks on). scale: output pixels per canvas pixel (--compose-scale: the
// pieces nearest, the overlays and the UI drawn at that size).
void WriteCompose(const std::string& path, const Composition& c, const u32* top, int topW, int topH, const u32* bottom, int scale = 1);
// The same picture as RGB bytes (c.cw * scale x c.ch * scale): the save states' thumbnails.
std::vector<u8> RenderCompose(const Composition& c, const u32* top, int topW, int topH, const u32* bottom, int scale = 1);
// One picture of w x h pixels (0xAARRGGBB), its alpha dropped (WriteImage) or kept (WriteRgba).
void WriteImage(const std::string& path, const u32* px, int w, int h);
void WriteRgba(const std::string& path, const u32* px, int w, int h);
}
