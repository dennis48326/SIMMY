// SimmyLauncher: synthesized sounds (see sfx.hpp). Each sound is a few sine "bubbles" with soft
// envelopes, made once at start-up; playing one replaces the sound that is playing.
#include "sfx.hpp"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include <cmath>
#include <vector>

namespace launcher {

namespace {
constexpr int kRate = 44100;
SDL_AudioStream* g_stream = nullptr;
bool g_enabled = true;
std::vector<float> g_sounds[7];

struct Note {
  float start, length;  // seconds
  float freq, freqEnd;  // Hz (a glide)
  float volume;
};

std::vector<float> Render(const Note* notes, int count) {
  float total = 0.0f;
  for (int i = 0; i < count; ++i) total = std::fmax(total, notes[i].start + notes[i].length);
  std::vector<float> out(static_cast<size_t>(total * kRate) + 64, 0.0f);
  const float twoPi = 6.28318530718f;
  for (int i = 0; i < count; ++i) {
    const Note& n = notes[i];
    const int first = static_cast<int>(n.start * kRate), len = static_cast<int>(n.length * kRate);
    float phase = 0.0f;
    for (int s = 0; s < len; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(len);
      const float f = n.freq + (n.freqEnd - n.freq) * t;
      phase += twoPi * f / kRate;
      // Quick attack, smooth decay; a touch of the octave makes it "bubbly".
      const float attack = std::fmin(1.0f, static_cast<float>(s) / (0.004f * kRate));
      const float env = attack * std::pow(1.0f - t, 2.2f);
      const float v = std::sin(phase) * 0.8f + std::sin(phase * 2.0f) * 0.2f * (1.0f - t);
      out[static_cast<size_t>(first + s)] += v * env * n.volume;
    }
  }
  return out;
}

void Build() {
  const Note hover[] = {{0.0f, 0.045f, 1320.0f, 1480.0f, 0.10f}};
  const Note click[] = {{0.0f, 0.07f, 660.0f, 990.0f, 0.22f}, {0.05f, 0.09f, 990.0f, 1320.0f, 0.16f}};
  const Note back[] = {{0.0f, 0.08f, 880.0f, 620.0f, 0.2f}, {0.06f, 0.09f, 620.0f, 440.0f, 0.14f}};
  const Note toggle[] = {{0.0f, 0.06f, 1046.5f, 1174.7f, 0.16f}};
  const Note launch[] = {{0.00f, 0.22f, 523.25f, 523.25f, 0.18f},
                         {0.09f, 0.22f, 659.25f, 659.25f, 0.18f},
                         {0.18f, 0.24f, 783.99f, 783.99f, 0.18f},
                         {0.27f, 0.55f, 1046.5f, 1046.5f, 0.22f},
                         {0.27f, 0.55f, 1318.5f, 1318.5f, 0.08f}};
  const Note error[] = {{0.0f, 0.16f, 392.0f, 349.2f, 0.2f}, {0.14f, 0.28f, 311.1f, 261.6f, 0.2f}};
  const Note disc[] = {{0.0f, 0.12f, 784.0f, 784.0f, 0.15f},
                       {0.07f, 0.12f, 987.8f, 987.8f, 0.15f},
                       {0.14f, 0.30f, 1174.7f, 1174.7f, 0.17f}};
  g_sounds[static_cast<int>(Sfx::Hover)] = Render(hover, 1);
  g_sounds[static_cast<int>(Sfx::Click)] = Render(click, 2);
  g_sounds[static_cast<int>(Sfx::Back)] = Render(back, 2);
  g_sounds[static_cast<int>(Sfx::Toggle)] = Render(toggle, 1);
  g_sounds[static_cast<int>(Sfx::Launch)] = Render(launch, 5);
  g_sounds[static_cast<int>(Sfx::Error)] = Render(error, 2);
  g_sounds[static_cast<int>(Sfx::Disc)] = Render(disc, 3);
}
}  // namespace

void sfx_init() {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return;
  SDL_AudioSpec spec;
  spec.format = SDL_AUDIO_F32;
  spec.channels = 1;
  spec.freq = kRate;
  g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!g_stream) return;
  Build();
  SDL_ResumeAudioStreamDevice(g_stream);
}

void sfx_shutdown() {
  if (g_stream) SDL_DestroyAudioStream(g_stream);
  g_stream = nullptr;
}

void sfx_enable(bool on) { g_enabled = on; }

void sfx_play(Sfx sound) {
  if (!g_stream || !g_enabled) return;
  const std::vector<float>& s = g_sounds[static_cast<int>(sound)];
  if (s.empty()) return;
  SDL_ClearAudioStream(g_stream);
  SDL_PutAudioStreamData(g_stream, s.data(), static_cast<int>(s.size() * sizeof(float)));
}

}  // namespace launcher
