// The PC audio mixer (see simmy/pc_audio.hpp): what AX, MIX and AI did on the GameCube, on an SDL3
// audio stream. Float stereo at 48 kHz, linear-interpolation resampling per voice.
#include "simmy/pc_audio.hpp"
#include "simmy/platform.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

namespace simmy::audio {
namespace {

enum class Source { None, Adpcm, Stream, Pcm };

constexpr int kMaxVoices = 128;
constexpr int kFrameSamples = kOutputRate / 200;  // 5 ms, the AX frame
constexpr int kSilentDb10 = -904;

}  // namespace

struct Voice {
  bool used = false;
  Source source = Source::None;
  // ADPCM (memory or stream)
  const unsigned char* data = nullptr;
  std::shared_ptr<StreamBuffer> stream;
  std::uint32_t cur = 0, end = 0, loopAddr = 0;
  bool loop = false, streamType = false;
  AdpcmState st;
  // PCM queue: interleaved L/R frames in a growing buffer read from `pcmHead`
  std::vector<std::int16_t> pcm;
  std::size_t pcmHead = 0;  // in samples
  std::uint64_t pcmPos = 0;
  std::uint64_t holdFrames = 0;
  // playback
  float rate = 32000.0f;
  double frac = 0.0;
  float s0[2] = {}, s1[2] = {};
  bool primed = false;
  bool running = false, ended = false, mute = false;
  int fader = 0, input = 0, pan = 64;
  int bus = kBusSfx;  // the game's volume option that scales it (SetVoiceBus)
};

namespace {

std::recursive_mutex g_lock;
Voice g_voices[kMaxVoices];
SDL_AudioStream* g_stream = nullptr;
bool g_open = false;
void (*g_frameCallback)() = nullptr;
std::vector<float> g_mix;

// SIMMY_AUDIO_WAV: everything mixed, as 16-bit stereo PCM.
std::FILE* g_wav = nullptr;
std::uint32_t g_wavFrames = 0;

void WavHeader() {
  if (!g_wav) return;
  const std::uint32_t data = g_wavFrames * 4;
  unsigned char h[44];
  auto put32 = [&](int o, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) h[o + i] = static_cast<unsigned char>(v >> (8 * i));
  };
  auto put16 = [&](int o, std::uint32_t v) {
    h[o] = static_cast<unsigned char>(v);
    h[o + 1] = static_cast<unsigned char>(v >> 8);
  };
  std::memcpy(h, "RIFF", 4);
  put32(4, 36 + data);
  std::memcpy(h + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1);
  put16(22, 2);
  put32(24, kOutputRate);
  put32(28, kOutputRate * 4);
  put16(32, 4);
  put16(34, 16);
  std::memcpy(h + 36, "data", 4);
  put32(40, data);
  const long pos = std::ftell(g_wav);
  std::fseek(g_wav, 0, SEEK_SET);
  std::fwrite(h, 1, sizeof h, g_wav);
  std::fseek(g_wav, pos < 44 ? 44 : pos, SEEK_SET);
  std::fflush(g_wav);
}

void WavWrite(const float* mix, int frames) {
  if (!g_wav) return;
  std::int16_t buf[kFrameSamples * 2];
  for (int i = 0; i < frames * 2; ++i)
    buf[i] = static_cast<std::int16_t>(std::lround(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f));
  std::fwrite(buf, sizeof(std::int16_t), static_cast<size_t>(frames) * 2, g_wav);
  g_wavFrames += static_cast<std::uint32_t>(frames);
  if (g_wavFrames % kOutputRate < static_cast<std::uint32_t>(frames)) WavHeader();  // about once a second
}

std::atomic<float> g_busGain[kBusCount] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

float DbToGain(int dB10) { return dB10 <= kSilentDb10 ? 0.0f : std::pow(10.0f, static_cast<float>(dB10) / 200.0f); }

// One DSP-ADPCM sample at the voice's current address. Returns false when the data is not there yet
// (stream underrun: the address stays put) or the voice has ended.
bool DecodeAdpcm(Voice& v, float& out) {
  const unsigned char* data = v.data;
  if (v.source == Source::Stream) {
    // The byte holding the frame header and the byte holding this nibble must have arrived.
    const std::size_t need = (v.cur >> 1) + 1;
    if (need > v.stream->available.load(std::memory_order_acquire)) {
      if (v.stream->failed.load()) {
        v.ended = true;
        v.running = false;
      }
      return false;
    }
    data = v.stream->data.data();
  }
  if ((v.cur & 15) == 0) {  // a frame header: predictor index and scale for the next 14 samples
    v.st.predScale = data[v.cur >> 1];
    v.cur += 2;
  }
  const unsigned char byte = data[v.cur >> 1];
  int nibble = (v.cur & 1) ? (byte & 15) : (byte >> 4);
  if (nibble >= 8) nibble -= 16;
  const int scale = 1 << (v.st.predScale & 15);
  const int index = (v.st.predScale >> 4) & 7;
  int sample = ((nibble * scale) << 11) + v.st.coef[index * 2] * v.st.yn1 + v.st.coef[index * 2 + 1] * v.st.yn2;
  sample = (sample + 1024) >> 11;
  sample = std::clamp(sample, -32768, 32767);
  v.st.yn2 = v.st.yn1;
  v.st.yn1 = static_cast<std::int16_t>(sample);
  out = static_cast<float>(sample) / 32768.0f;
  if (v.cur >= v.end) {
    if (v.loop) {
      v.cur = v.loopAddr;
      // Mid-frame loop start: take the predictor/scale from that frame's header.
      if (v.cur & 15) {
        const std::size_t header = (v.cur & ~15u) >> 1;
        if (v.source == Source::Adpcm) v.st.predScale = v.data[header];
        else if (header < v.stream->available.load()) v.st.predScale = v.stream->data[header];
      }
      if (!v.streamType) v.st.yn1 = v.st.yn2 = 0;
    } else {
      v.ended = true;
    }
  } else {
    ++v.cur;
  }
  return true;
}

// The voice's next input frame into `s` (mono sources fill both channels).
void Fetch(Voice& v, float* s) {
  s[0] = s[1] = 0.0f;
  if (v.ended) return;
  switch (v.source) {
    case Source::Adpcm:
    case Source::Stream: {
      float x;
      if (DecodeAdpcm(v, x)) s[0] = s[1] = x;
      break;
    }
    case Source::Pcm:
      if (v.holdFrames) {
        --v.holdFrames;
      } else if (v.pcmHead + 2 <= v.pcm.size()) {
        s[0] = v.pcm[v.pcmHead] / 32768.0f;
        s[1] = v.pcm[v.pcmHead + 1] / 32768.0f;
        v.pcmHead += 2;
        ++v.pcmPos;
        if (v.pcmHead > 1 << 16 && v.pcmHead * 2 > v.pcm.size()) {  // compact the consumed part
          v.pcm.erase(v.pcm.begin(), v.pcm.begin() + static_cast<std::ptrdiff_t>(v.pcmHead));
          v.pcmHead = 0;
        }
      }
      break;
    case Source::None:
      break;
  }
}

void MixVoice(Voice& v, float* out, int frames) {
  if (!v.primed) {
    Fetch(v, v.s1);
    v.s0[0] = v.s1[0];
    v.s0[1] = v.s1[1];
    v.primed = true;
  }
  const float gain = v.mute ? 0.0f : DbToGain(v.fader) * DbToGain(v.input) * g_busGain[v.bus].load();
  float gl = gain, gr = gain;
  if (v.source != Source::Pcm) {  // pan a mono voice, equal power
    const float a = static_cast<float>(std::clamp(v.pan, 0, 127)) / 127.0f * 1.5707963f;
    gl *= std::cos(a);
    gr *= std::sin(a);
  }
  const double step = static_cast<double>(v.rate) / kOutputRate;
  for (int i = 0; i < frames; ++i) {
    const float f = static_cast<float>(v.frac);
    out[2 * i] += (v.s0[0] + (v.s1[0] - v.s0[0]) * f) * gl;
    out[2 * i + 1] += (v.s0[1] + (v.s1[1] - v.s0[1]) * f) * gr;
    v.frac += step;
    while (v.frac >= 1.0) {
      v.frac -= 1.0;
      v.s0[0] = v.s1[0];
      v.s0[1] = v.s1[1];
      Fetch(v, v.s1);
    }
    if (v.ended && !v.loop) {
      // Let the last sample through the interpolator, then stop.
      v.running = false;
      break;
    }
  }
}

void MixFrame(float* out) {
  std::fill(out, out + kFrameSamples * 2, 0.0f);
  if (g_frameCallback) g_frameCallback();
  for (Voice& v : g_voices)
    if (v.used && v.running && v.source != Source::None) MixVoice(v, out, kFrameSamples);
  for (int i = 0; i < kFrameSamples * 2; ++i) out[i] = std::clamp(out[i], -1.0f, 1.0f);
}

void SDLCALL Feed(void*, SDL_AudioStream* stream, int additional, int) {
  const int frames = (additional / 8 + kFrameSamples - 1) / kFrameSamples * kFrameSamples;
  if (frames <= 0) return;
  g_mix.resize(static_cast<size_t>(frames) * 2);
  {
    std::lock_guard lock(g_lock);
    for (int f = 0; f < frames; f += kFrameSamples) {
      MixFrame(g_mix.data() + 2 * f);
      WavWrite(g_mix.data() + 2 * f, kFrameSamples);
    }
  }
  SDL_PutAudioStreamData(stream, g_mix.data(), frames * 8);
}

void ResetVoice(Voice& v) {
  const bool used = v.used;
  const int bus = v.bus;
  v = Voice{};
  v.used = used;
  v.bus = bus;
}

bool TryOpen(const char* driver) {
  if (driver) SDL_SetHint(SDL_HINT_AUDIO_DRIVER, driver);
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kOutputRate};
  g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, nullptr);
  if (!g_stream) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return false;
  }
  return true;
}

}  // namespace

bool Open() {
  if (g_open) return true;
  if (const char* e = std::getenv("SIMMY_AUDIO"); e && std::strcmp(e, "0") == 0) {
    std::fprintf(stderr, "[audio] disabled (SIMMY_AUDIO=0)\n");
    return false;
  }
  // Headless runs never open a sound card. Elsewhere, fall back to the dummy driver when there is
  // no device, so the game behaves the same (and SIMMY_AUDIO_WAV still records).
  const bool ok = platform_headless() ? TryOpen("dummy") : (TryOpen(nullptr) || TryOpen("dummy"));
  if (!ok) {
    std::fprintf(stderr, "[audio] no audio device: %s\n", SDL_GetError());
    return false;
  }
  // SIMMY_AUDIO_DUMP is the same as SIMMY_AUDIO_WAV (the final stereo mix to a WAV file).
  const char* wavPath = std::getenv("SIMMY_AUDIO_DUMP");
  if (!wavPath || !*wavPath) wavPath = std::getenv("SIMMY_AUDIO_WAV");
  if (const char* path = wavPath; path && *path) {
    g_wav = std::fopen(path, "wb");
    if (g_wav) WavHeader();
    else std::fprintf(stderr, "[audio] cannot write %s\n", path);
  }
  g_open = true;
  SDL_ResumeAudioStreamDevice(g_stream);
  std::fprintf(stderr, "[audio] SDL driver \"%s\", mixing at %d Hz%s%s\n", SDL_GetCurrentAudioDriver(), kOutputRate,
               g_wav ? ", recording to " : "", g_wav ? wavPath : "");
  return true;
}

void Close() {
  if (!g_open) return;
  SDL_DestroyAudioStream(g_stream);  // stops the callback
  g_stream = nullptr;
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  std::lock_guard lock(g_lock);
  g_open = false;
  if (g_wav) {
    WavHeader();
    std::fclose(g_wav);
    g_wav = nullptr;
    std::fprintf(stderr, "[audio] recorded %.1f s\n", static_cast<double>(g_wavFrames) / kOutputRate);
  }
  for (Voice& v : g_voices) v = Voice{};
}

bool IsOpen() { return g_open; }

void SetFrameCallback(void (*callback)()) {
  std::lock_guard lock(g_lock);
  g_frameCallback = callback;
}

void LockMixer() { g_lock.lock(); }
void UnlockMixer() { g_lock.unlock(); }

Voice* AcquireVoice() {
  if (!g_open) return nullptr;
  std::lock_guard lock(g_lock);
  for (Voice& v : g_voices) {
    if (!v.used) {
      v = Voice{};
      v.used = true;
      return &v;
    }
  }
  return nullptr;
}

void FreeVoice(Voice* v) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  *v = Voice{};
}

void SetAdpcm(Voice* v, const unsigned char* data, std::uint32_t startNibble, std::uint32_t endNibble, bool loop,
              std::uint32_t loopNibble, const AdpcmState& state, bool stream) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  const bool running = v->running;
  const int fader = v->fader, input = v->input, pan = v->pan;
  const bool mute = v->mute;
  const float rate = v->rate;
  ResetVoice(*v);
  v->running = running;
  v->fader = fader, v->input = input, v->pan = pan, v->mute = mute, v->rate = rate;
  v->source = Source::Adpcm;
  v->data = data;
  v->cur = startNibble;
  v->end = endNibble;
  v->loop = loop;
  v->loopAddr = loopNibble;
  v->st = state;
  v->streamType = stream;
}

void SetAdpcmStream(Voice* v, std::shared_ptr<StreamBuffer> buffer, std::uint32_t startNibble,
                    std::uint32_t endNibble, bool loop, std::uint32_t loopNibble, const AdpcmState& state) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  SetAdpcm(v, nullptr, startNibble, endNibble, loop, loopNibble, state, true);
  v->source = Source::Stream;
  v->stream = std::move(buffer);
}

void SetPcmQueue(Voice* v) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  const bool running = v->running;
  const int fader = v->fader, input = v->input, pan = v->pan;
  const float rate = v->rate;
  ResetVoice(*v);
  v->running = running;
  v->fader = fader, v->input = input, v->pan = pan, v->rate = rate;
  v->source = Source::Pcm;
}

void PushPcm(Voice* v, const std::int16_t* frames, std::uint32_t count) {
  if (!v || !count) return;
  std::lock_guard lock(g_lock);
  v->pcm.insert(v->pcm.end(), frames, frames + static_cast<size_t>(count) * 2);
}

std::int64_t SyncPcm(Voice* v, std::uint64_t expected) {
  if (!v) return 0;
  std::lock_guard lock(g_lock);
  const std::uint64_t tolerance = static_cast<std::uint64_t>(v->rate * 0.06f);
  if (v->pcmPos + tolerance < expected) {
    const std::size_t queued = (v->pcm.size() - v->pcmHead) / 2;
    const std::size_t drop = static_cast<std::size_t>(std::min<std::uint64_t>(expected - v->pcmPos, queued));
    v->pcmHead += drop * 2;
    v->pcmPos += drop;
    v->holdFrames = 0;
    return static_cast<std::int64_t>(drop);
  }
  if (v->pcmPos > expected + tolerance && v->holdFrames == 0) {
    v->holdFrames = v->pcmPos - expected;
    return -static_cast<std::int64_t>(v->holdFrames);
  }
  return 0;
}

std::uint64_t PcmPosition(Voice* v) {
  if (!v) return 0;
  std::lock_guard lock(g_lock);
  return v->pcmPos;
}

void SetRate(Voice* v, float hz) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  v->rate = std::clamp(hz, 1.0f, 4.0f * kOutputRate);
}

float GetRate(Voice* v) { return v ? v->rate : 0.0f; }

void SetRunning(Voice* v, bool running) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  v->running = running && !v->ended;
}

bool IsRunning(Voice* v) {
  if (!v) return false;
  std::lock_guard lock(g_lock);
  return v->running;
}

bool HasEnded(Voice* v) {
  if (!v) return true;
  std::lock_guard lock(g_lock);
  return v->ended;
}

std::uint32_t CurrentNibble(Voice* v) {
  if (!v) return 0;
  std::lock_guard lock(g_lock);
  return v->cur;
}

void SetFader(Voice* v, int dB10) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  v->fader = dB10;
}

void SetInput(Voice* v, int dB10) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  v->input = dB10;
}

void SetPan(Voice* v, int pan) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  v->pan = pan;
}

void SetMute(Voice* v, bool mute) {
  if (!v) return;
  std::lock_guard lock(g_lock);
  v->mute = mute;
}

int GetFader(Voice* v) { return v ? v->fader : kSilentDb10; }

void SetVoiceBus(Voice* v, int bus) {
  if (!v || bus < 0 || bus >= kBusCount) return;
  std::lock_guard lock(g_lock);
  v->bus = bus;
}
void SetBusVolume(int bus, float gain) {
  if (bus < 0 || bus >= kBusCount) return;
  g_busGain[bus] = std::clamp(gain, 0.0f, 1.0f);
}
float GetBusVolume(int bus) { return bus >= 0 && bus < kBusCount ? g_busGain[bus].load() : 1.0f; }
int GetPan(Voice* v) { return v ? v->pan : 64; }

}  // namespace simmy::audio
