#include "mic.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <mutex>

#include "host.h"
#include "settings.h"

namespace sims2hd::mic
{
namespace
{
// melonDS pulls one sample every 704 ARM9 cycles (Mic.cpp): 33513982 / 704.
constexpr double kRate = 47605.1;

long samplesRead = 0;
bool listening = false;    // between Mic_Start and Mic_Stop

// --- Blow key: a saturated wave, like the recorded blow melonDS's frontend plays ---------------
// Full-scale half-waves of random length (24 to 120 samples: 200 to 1000 Hz), so that a level
// check, a zero-crossing count or a low-pass all read "someone is blowing".
u32 noiseSeed = 0x2545F491;
int blowLeft = 0;
s16 blowLevel = 32767;

s16 BlowSample()
{
    if (blowLeft <= 0)
    {
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        blowLeft = 24 + (int)((noiseSeed >> 16) % 97);
        blowLevel = blowLevel > 0 ? -32768 : 32767;
    }
    blowLeft--;
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (s16)(blowLevel - (blowLevel > 0 ? 1 : -1) * (int)((noiseSeed >> 24) & 7));
}

// --- Real microphone: SDL capture, resampled into a ring the emulator drains ------------------
SDL_AudioDeviceID device = 0;
bool deviceTried = false;
int deviceRate = 0;
std::mutex ringLock;
constexpr int kRing = 1 << 15; // 0.7 s at 47.6 kHz
s16 ring[kRing];
int ringWrite = 0, ringCount = 0;
double carry = 0;   // where the next chunk starts: in [-1, 0) is between lastIn and its first sample
s16 lastIn = 0;

void Capture(void*, Uint8* stream, int len)
{
    const s16* in = (const s16*)stream;
    int n = len / (int)sizeof(s16);
    if (n <= 0 || deviceRate <= 0) return;
    const double step = deviceRate / kRate; // input samples per output sample
    std::lock_guard<std::mutex> lk(ringLock);
    double pos = carry;
    while (pos < n - 1 && ringCount < kRing)
    {
        double a, b, f;
        if (pos < 0) { a = lastIn; b = in[0]; f = pos + 1; }
        else { int i = (int)pos; a = in[i]; b = in[i + 1]; f = pos - i; }
        ring[ringWrite] = (s16)(a + (b - a) * f);
        ringWrite = (ringWrite + 1) % kRing;
        ringCount++;
        pos += step;
    }
    carry = pos - n;
    lastIn = in[n - 1];
}

void OpenDevice()
{
    if (device || deviceTried) return;
    deviceTried = true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { fprintf(stderr, "mic: SDL audio: %s\n", SDL_GetError()); return; }
    SDL_AudioSpec want {}, have {};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = Capture;
    device = SDL_OpenAudioDevice(nullptr, 1, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!device) { fprintf(stderr, "mic: no microphone: %s\n", SDL_GetError()); return; }
    deviceRate = have.freq;
    {
        std::lock_guard<std::mutex> lk(ringLock);
        ringWrite = ringCount = 0;
        carry = 0;
        lastIn = 0;
    }
    SDL_PauseAudioDevice(device, 0);
    fprintf(stderr, "mic: microphone open, %d Hz\n", deviceRate);
}

void CloseDevice()
{
    if (device) SDL_CloseAudioDevice(device);
    device = 0;
    deviceTried = false;
}

int ReadReal(s16* out, int max)
{
    std::lock_guard<std::mutex> lk(ringLock);
    int n = max < ringCount ? max : ringCount;
    int readPos = (ringWrite - ringCount + kRing) % kRing;
    for (int i = 0; i < n; i++)
    {
        out[i] = ring[readPos];
        readPos = (readPos + 1) % kRing;
    }
    ringCount -= n;
    return n;
}
}

void Start()
{
    listening = true;
    if (Host::MicMode == MicMode::Real) OpenDevice();
}

void Stop()
{
    listening = false;
    CloseDevice();
}

int Read(s16* out, int max)
{
    if (max <= 0) return 0;
    // The mode can change in the settings while the game listens: follow it.
    if (Host::MicMode == MicMode::Real && listening) OpenDevice();
    else if (device) CloseDevice();

    int n = max;
    if (Host::MicMode == MicMode::Real && !Host::MicForce) n = device ? ReadReal(out, max) : 0;
    else if (Host::MicForce || (Host::MicMode == MicMode::BlowKey && Host::MicBlow))
        for (int i = 0; i < max; i++) out[i] = BlowSample();
    else
    {
        memset(out, 0, (size_t)max * sizeof(s16));
        blowLeft = 0;
    }
    samplesRead += n;
    return n;
}

long SamplesRead() { return samplesRead; }
}
