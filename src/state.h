// Save states: the whole emulated machine to a file and back.
#pragma once
#include <string>

#include "NDS.h"

namespace sims2wide
{
bool SaveState(melonDS::NDS& nds, const std::string& path);
bool LoadState(melonDS::NDS& nds, const std::string& path);
}
