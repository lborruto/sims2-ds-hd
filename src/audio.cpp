#include "audio.h"

namespace sims2wide
{
bool Audio::Open()
{
    SDL_AudioSpec want {}, have {};
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024;
    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    return dev != 0;
}

void Audio::Frame(melonDS::NDS& nds, bool fast)
{
    int n = nds.SPU.ReadOutput(buf.data(), 4096);
    // At most 0.2 s queued, so that the sound never lags far behind the picture.
    if (dev && !fast && SDL_GetQueuedAudioSize(dev) < 48000 * 4 / 5) SDL_QueueAudio(dev, buf.data(), n * 4);
}
}
