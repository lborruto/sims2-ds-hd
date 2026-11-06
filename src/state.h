// Save states: the whole emulated machine to a file and back.
#pragma once
#include <string>
#include <vector>

#include "common.h"

#include "NDS.h"

namespace sims2wide
{
bool SaveState(melonDS::NDS& nds, const std::string& path);
bool LoadState(melonDS::NDS& nds, const std::string& path);
// The machine in memory and back (a load's way back when the load fails half-way).
bool SaveStateTo(melonDS::NDS& nds, std::vector<u8>& out);
bool LoadStateFrom(melonDS::NDS& nds, std::vector<u8>& data);
}
