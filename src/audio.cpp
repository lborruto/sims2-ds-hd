#include "audio.h"

#include <algorithm>

namespace sims2hd
{
// A DS frame: 60 Hz near enough (59.8261); the sound is 48 kHz stereo, 4 bytes a sample.
static constexpr double kFrameTime = 1.0 / 59.8261;

bool Audio::Open()
{
    SDL_AudioSpec want {}, have {};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    return dev != 0;
}

void Audio::Pause(bool on)
{
    if (on == paused || !dev) return;
    paused = on;
    if (on) SDL_ClearQueuedAudio(dev);
    SDL_PauseAudioDevice(dev, on ? 1 : 0);
    lastTick = 0;
}

void Audio::Frame(melonDS::NDS& nds, bool fast, const Settings& s)
{
    const int n = nds.SPU.ReadOutput(buf.data(), 4096);
    if (!dev || n <= 0) return;
    const int queued = (int)SDL_GetQueuedAudioSize(dev) / 4;
    // How much longer than a DS frame the frames are taking: above 1 the sound is stretched to
    // match (AudioStretch), steered towards 100 ms of sound waiting in the queue. Averaged over
    // about a second, so that frames that come quicker and slower in turn do not make the music
    // speed up and slow down with them (the queue takes up the difference).
    const Uint64 t = SDL_GetPerformanceCounter();
    if (lastTick)
    {
        const double dt = (double)(t - lastTick) / SDL_GetPerformanceFrequency();
        if (dt < 0.25) slow += (std::min(dt / kFrameTime, 4.0) - slow) * 0.03;
    }
    lastTick = t;
    if ((fast && !s.fastSound) || s.mute || s.volume == 0) return; // muted: the frame's sound is dropped
    double slowdown = fast ? 1.0 : slow;
    if (stretch.Stretching() && !fast) slowdown *= 1.0 + 0.5 * (4800 - queued) / 4800.0;
    out.clear();
    stretch.Process(buf.data(), n, slowdown, out);
    if (s.volume < 100)
        for (s16& v : out) v = (s16)(v * s.volume / 100);
    // At most 0.2 s queued (and in fast forward only what fits: the rest is skipped), so that
    // the sound never lags far behind the picture.
    if (queued < 48000 / 5 && !out.empty()) SDL_QueueAudio(dev, out.data(), (u32)(out.size() * 2));
}
}
