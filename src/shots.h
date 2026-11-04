// PNG pictures: both raw screens, or the window's picture for a layout (headless previews).
#pragma once
#include <string>

#include "common.h"
#include "layout.h"

namespace sims2wide
{
void WriteShot(const std::string& path, const u32* top, const u32* bottom);
// Any RGB picture (the window's, from video.cpp).
void WriteRgb(const std::string& path, int w, int h, const u8* rgb);
// The window's picture on the CPU (nearest scaling). The top screen's picture is topW x topH
// (App::TopImage: bigger than 256 x 192 with the looks on); its shape sizes the top box.
void WriteCompose(const std::string& path, Layout l, bool mainIsTop, const u32* top, int topW, int topH, const u32* bottom);
// One picture of w x h pixels (0xAARRGGBB).
void WriteImage(const std::string& path, const u32* px, int w, int h);
}
