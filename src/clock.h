// The DS real-time clock. The game lives on it (rooms take eight real hours, skill points at
// 00:01 / 08:00 / 16:00, night missions) and punishes a clock set back (an alien invasion). melonDS
// keeps the RTC inside the save state and ticks it on emulated time, so the frontend keeps it
// honest: the host's time at boot, the host's time again after a state load (never the state's
// old time), and during fast forward it is held back to real time, a second at a time, never
// crossing a minute before the host does.
#pragma once
#include <string>

#include "NDS.h"

namespace sims2hd::clock
{
// A civil date and time, the RTC's resolution (local time, no zone: the DS has none).
struct Time
{
    int year, month, day, hour, minute, second;
    long Seconds() const; // since 2000-01-01 00:00:00
    static Time FromSeconds(long s);
};
Time HostNow();

// "YYYY-MM-DD HH:MM" (--clock): fixes the start time and keeps the RTC on emulated time alone,
// for deterministic headless runs. False when it does not parse.
bool SetFixed(const std::string& text);

// RTC := the host's time (or the fixed one). At boot and after every state load.
void Seed(melonDS::NDS& nds);

// Once per frame (before it). fast: fast forward is held.
void Frame(melonDS::NDS& nds, bool fast);

Time Read(const melonDS::NDS& nds);

// What Frame() saw, for the headless summary: how often it wrote the RTC, the most seconds
// the RTC was ahead of the host, and how many frames it showed a minute the host had not reached.
struct Stats { long writes = 0; long maxAhead = 0; long minuteAhead = 0; };
const Stats& GetStats();
}
