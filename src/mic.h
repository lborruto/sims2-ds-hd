// The DS microphone: what melonDS hears, per the "mic" setting. The game needs it (blowing clears
// the vacuum's filter; the music mixer records voice).
//   BlowKey  while the blow key is held (Host::MicBlow), a loud blow-like signal, the way melonDS's
//            own frontend feeds a recorded blow: a saturated wave that any level check passes.
//   Real     the computer's microphone through SDL, opened only while the game listens
//            (Mic_Start / Mic_Stop), resampled to the rate melonDS asks for.
//   Off      silence.
// melonDS's Platform::Mic_* (platform.cpp) land here; the mode and the key come from host.h.
#pragma once
#include "common.h"

namespace sims2wide::mic
{
void Start();
void Stop();
// Fills out with up to max samples (mono, signed 16-bit, 47.6 kHz); returns how many.
int Read(s16* out, int max);

// How many samples the game has read so far, for headless checks.
long SamplesRead();
}
