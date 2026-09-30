#include "splatterhouse/common.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

namespace splatterhouse::audio {

static SDL_AudioStream* s_stream = nullptr;
static SDL_AudioDeviceID s_dev = 0;

bool Init(int sampleRate) {
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = sampleRate;

    s_dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!s_dev) {
        spdlog::error("[audio] SDL_OpenAudioDevice fallo: {}", SDL_GetError());
        return false;
    }
    s_stream = SDL_CreateAudioStream(&spec, &spec);
    if (!s_stream) {
        spdlog::error("[audio] SDL_CreateAudioStream fallo: {}", SDL_GetError());
        return false;
    }
    SDL_BindAudioStream(s_dev, s_stream);
    SDL_ResumeAudioDevice(s_dev);
    spdlog::info("[audio] Init {} Hz device={}", sampleRate, (int)s_dev);
    return true;
}

void Shutdown() {
    if (s_stream) { SDL_DestroyAudioStream(s_stream); s_stream = nullptr; }
    if (s_dev) { SDL_CloseAudioDevice(s_dev); s_dev = 0; }
    spdlog::info("[audio] Shutdown");
}

// Llamado desde HLE XAudio2 del guest para push PCM
void SubmitPCM(const float* data, int frames) {
    if (!s_stream || !data) return;
    SDL_PutAudioStreamData(s_stream, data, frames * 2 * sizeof(float));
}

} // namespace splatterhouse::audio
