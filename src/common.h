// Shared by every part of the frontend.
#pragma once
#include <cstdint>

#include "types.h" // melonDS: u8, u16, u32, s16…

namespace sims2wide
{
using melonDS::u8;
using melonDS::u16;
using melonDS::u32;
using melonDS::u64;
using melonDS::s16;

// One DS screen.
constexpr int SW = 256, SH = 192;

// DS key bits, in the order of melonDS's key mask.
enum DSKey { kA, kB, kSelect, kStart, kRight, kLeft, kUp, kDown, kR, kL, kX, kY, kKeyCount };
}
