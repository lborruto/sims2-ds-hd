// PNG pictures: both raw screens, or the window's picture for a layout (headless previews).
#pragma once
#include <string>

#include "common.h"
#include "layout.h"

namespace sims2wide
{
void WriteShot(const std::string& path, const u32* top, const u32* bottom);
// The window's picture on the CPU (nearest scaling).
void WriteCompose(const std::string& path, Layout l, bool mainIsTop, const u32* top, const u32* bottom);
}
