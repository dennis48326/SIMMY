// Port of engine/e_ngcaudio.cpp: EAudio and the platform audio object ENgcAudio (_pActualAudio).
// Music: PlayMusic reads the stream's DSP-ADPCM header from audiostr.arc into a stream-table entry and
// hands it to ENGCAudioStreamer; Update releases streams that have finished. Voices: an EVoice is
// bound to a sample resource and played through ENgcSFXStreamer.
//
// PC: InitAudio opens the mixer (platform/pc_audio.cpp) where the original initialised AI/AX/MIX,
// and the mixer calls AudioFrameCallback every 5 ms as AX did. The original's Update also paused all
// audio while the disc drive reported an error (cover open, no disc); there is no disc drive here.
#include "simmy/types_gen.hpp"
#include "simmy/pc_audio.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

int gAudioEnabled = 1;  // 804FF150 (data)
static int gAudioInitialised = 0;
static int gWarmupMilliCount = -1;
static int bWasPlayingWhenPaused = 0;

ENgcAudio* _pActualAudio = nullptr;  // 804FF15C
ENgcAudio _ngcAudio;                 // global constructor keyed to gAudioEnabled

namespace {
ENgcSFXStreamer* fx = nullptr;
ENGCAudioStreamer* streamer = nullptr;
constexpr int kStreams = 64;
NGCStreamInfo streams[kStreams];         // 804C25E0
unsigned int ActiveSFXHandles[64];       // 804C36E0 streamer handles of bound voices
ERSampledata* ActiveSFXSamples[64];      // 804C37E0
unsigned char dspHeader[0x60];           // 804C2580 the header of the stream being started

void Lock(EMutex& m) { m.Acquire(0xFFFFFFFFu); }
void Unlock(EMutex& m) { m.Release(); }

// Fills a stream-table entry from the header in dspHeader. The data follows the header at `pos`.
void FillStreamInfo(NGCStreamInfo& s, EFile* file, unsigned int pos, const EPMDesc& desc) {
  s.m_file = file;
  s.m_size = (simmy::be32(dspHeader + 0x04) >> 1) & 0x7FFFFFE0u;
  s.m_offset = pos + 0x60;
  s.m_sampleRate = simmy::be16(dspHeader + 0x0A);
  s.m_flags = static_cast<unsigned short>((desc.m_loop & 1) << 15);
  for (int i = 0; i < 16; ++i) s.m_coef[i] = static_cast<short>(simmy::be16(dspHeader + 0x1C + 2 * i));
  s.m_gain = simmy::be16(dspHeader + 0x3C);
  s.m_predScale = simmy::be16(dspHeader + 0x3E);
  s.m_yn1 = static_cast<short>(simmy::be16(dspHeader + 0x40));
  s.m_yn2 = static_cast<short>(simmy::be16(dspHeader + 0x42));
}
}  // namespace

// 8032CB70
EAudio::EAudio() = default;
// 8032B774
EAudio::~EAudio() = default;

// 8032CB84
bool EAudio::AdjustMusicVolume(float delta) {
  float volume = GetMusicVolume() + delta;
  bool clamped = false;
  if (volume > 1.0f) {
    volume = 1.0f;
    clamped = true;
  } else if (volume < 0.0f) {
    volume = 0.0f;
    clamped = true;
  }
  SetMusicVolume(volume);
  return clamped;
}

// 8032B874
EPMDesc::EPMDesc(unsigned int id, bool loop) : m_id(id), m_loop(loop) {}
// 8032B898
EPMDesc::EPMDesc(char* name, bool loop) : m_id(EResourceManager::CalcId(name)), m_loop(loop) {}

// 8032B7A8
ENgcAudio::ENgcAudio() { _pActualAudio = this; }
// 8032B818
ENgcAudio::~ENgcAudio() = default;

// 8032B8EC
// PC: opening the mixer replaces AIInit/AXInit/MIXInit, AXSetMode and MIXSetSoundMode (the mixer is
// always stereo). Without a device, audio stays off (gAudioEnabled = 0) as on a console without it.
void ENgcAudio::InitAudio() {
  if (!gAudioEnabled || gAudioInitialised) return;
  if (!simmy::audio::Open()) {
    gAudioEnabled = 0;
    return;
  }
  if (!fx) fx = new ENgcSFXStreamer;
  if (!streamer) streamer = new ENGCAudioStreamer;
  for (int i = 0; i < kStreams; ++i) {
    streams[i].m_id = 0xFFFFFFFFu;
    streams[i].m_handle = 0xFFFFFFFFu;
    streams[i].m_field04 = -1;
    ActiveSFXHandles[i] = 0xFFFFFFFFu;
  }
  simmy::audio::SetFrameCallback(AudioFrameCallback);  // AXRegisterCallback
  m_musicPaused = 0;
  gAudioInitialised = 1;
}

// 8032B9F0
void ENgcAudio::AudioAlarmHandler(OSAlarm*, OSContext*) {}

// 8032B9F4
void ENgcAudio::Shutdown() {
  if (!gAudioEnabled || !gAudioInitialised) return;
  if (streamer) {
    delete streamer;
    streamer = nullptr;
  }
  simmy::audio::SetFrameCallback(nullptr);
  simmy::audio::Close();  // MIXQuit, AXQuit
  gAudioInitialised = 0;
}

// 8032BA5C
// Stops the current music and starts `desc`'s stream: mono, or stereo when the resource holds two
// channels (the second header at half the resource). PC: the original's comparison was true for any
// resource larger than a header, so it always played the first channel only; the test here looks at
// whether a second channel fits (e.g. the 48 kHz title track is stored as two channels).
void ENgcAudio::PlayMusic(EPMDesc& desc) {
  if (!gAudioEnabled) return;
  StopMusic();
  unsigned int first = 0, second = kStreams;
  while (first < kStreams && streams[first].m_id != 0xFFFFFFFFu) ++first;
  if (first < kStreams) {
    second = first + 1;
    while (second < kStreams && streams[second].m_id != 0xFFFFFFFFu) ++second;
  }
  if (first < kStreams && second < kStreams) {
    auto* res = _audiostreamman.AddRef(desc.m_id, nullptr, 0);
    if (!res) {  // PC: the original assumed the stream exists (audiostr.arc missing or no such id)
      std::fprintf(stderr, "[audio] music stream %08X not found\n", desc.m_id);
    } else {
      Lock(m_streamMutex);
      streams[first].m_id = desc.m_id;
      streams[first].m_handle = 0xFFFFFFFFu;
      Unlock(m_streamMutex);
      EFile* file = res->m_file;
      const unsigned int size = res->m_end - res->m_start + 1;
      file->Seek(static_cast<int>(res->m_start), EFile__SeekType::Set);
      file->Read(dspHeader, 0x60);
      FillStreamInfo(streams[first], file, res->m_start, desc);
      const unsigned int bytes = simmy::be32(dspHeader + 0x04) >> 1;
      if ((size >> 1) < bytes + 0x60) {  // one channel
        streamer->Allocate(&streams[first], 0x7F, 0x40);
      } else {
        _audiostreamman.AddRef(desc.m_id, nullptr, 0);
        Lock(m_streamMutex);
        streams[second].m_handle = 0xFFFFFFFFu;
        streams[second].m_id = desc.m_id;
        Unlock(m_streamMutex);
        const unsigned int pos = res->m_start + (size >> 1);
        file->Seek(static_cast<int>(pos), EFile__SeekType::Set);
        file->Read(dspHeader, 0x60);
        FillStreamInfo(streams[second], file, pos, desc);
        streamer->AllocateStereo(&streams[first], &streams[second], 0x7F);
      }
    }
  }
  m_musicPaused = 0;
  SetMusicVolume(m_musicVolume);
}

// 8032BEDC
unsigned char ENgcAudio::GetIndexFromVoice(EVoice*) { return 0; }
// 8032BEE4
EVoice* ENgcAudio::GetVoiceFromIndex(unsigned char) { return nullptr; }

// 8032BEEC
// Releases the resources of streams that have stopped playing. (GC: first paused every voice and the
// music while the disc drive was in an error state, and resumed them 5 s after it recovered.)
void ENgcAudio::Update() {
  if (!gAudioEnabled || !gAudioInitialised) return;
  const int warmup = gWarmupMilliCount;
  if (warmup == 0) {
    gWarmupMilliCount = -1;
    bWasPlayingWhenPaused = warmup;
    ResumeMusic();
  }
  Lock(m_streamMutex);
  for (int i = 0; i < kStreams; ++i) {
    NGCStreamInfo& s = streams[i];
    if (s.m_id == 0xFFFFFFFFu || s.m_handle == 0xFFFFFFFFu || streamer->IsActive(s.m_handle)) continue;
    _audiostreamman.DelRef(s.m_id, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
    s.m_id = 0xFFFFFFFFu;
    for (int j = i + 1; j < kStreams; ++j) {  // the other channel of a stereo stream
      if (streams[j].m_id != 0xFFFFFFFFu && s.m_handle == streams[j].m_handle) {
        _audiostreamman.DelRef(streams[j].m_id, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
        streams[j].m_id = 0xFFFFFFFFu;
      }
    }
  }
  Unlock(m_streamMutex);
}

// 8032C1FC
void ENgcAudio::StopMusic() {
  if (!gAudioEnabled) return;
  Lock(m_streamMutex);
  for (NGCStreamInfo& s : streams) {
    if (s.m_id == 0xFFFFFFFFu || s.m_handle == 0xFFFFFFFFu) continue;
    if (streamer->Free(s.m_handle))
      _audiostreamman.DelRef(s.m_id, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
    s.m_id = 0xFFFFFFFFu;
  }
  Unlock(m_streamMutex);
}

// 8032C2C8
void ENgcAudio::PauseMusic() {
  if (!gAudioEnabled) return;
  m_musicPaused = 1;
  for (NGCStreamInfo& s : streams)
    if (s.m_id != 0xFFFFFFFFu && s.m_handle != 0xFFFFFFFFu) streamer->Pause(s.m_handle);
}

// 8032C338
void ENgcAudio::ResumeMusic() {
  if (!gAudioEnabled) return;
  m_musicPaused = 0;
  for (NGCStreamInfo& s : streams)
    if (s.m_id != 0xFFFFFFFFu && s.m_handle != 0xFFFFFFFFu) streamer->Resume(s.m_handle);
}

// 8032C3A8
void ENgcAudio::SetMusicVolume(float volume) {
  if (volume > 1.0f) volume = 1.0f;
  if (volume < 0.0f) volume = 0.0f;
  m_musicVolume = volume;
  if (!gAudioEnabled) return;
  for (NGCStreamInfo& s : streams)
    if (s.m_id != 0xFFFFFFFFu && s.m_handle != 0xFFFFFFFFu)
      streamer->SetVolume(s.m_handle, static_cast<unsigned char>(static_cast<int>(m_musicVolume * 127.0f)));
}

// 8032C468
float ENgcAudio::GetMusicVolume() { return m_musicVolume; }

// 8032C470
// Out-of-range pans are reflected back into -1..1.
void ENgcAudio::SetMusicPan(float pan) {
  if (pan < -1.0f) pan = -2.0f - pan;
  else if (pan > 1.0f) pan = 2.0f - pan;
  m_musicPan = pan;
  if (!gAudioEnabled) return;
  for (NGCStreamInfo& s : streams)
    if (s.m_id != 0xFFFFFFFFu && s.m_handle != 0xFFFFFFFFu)
      streamer->SetPan(s.m_handle, static_cast<unsigned char>(static_cast<int>(pan * 63.5f + 63.5f)));
}

// 8032C534
float ENgcAudio::GetMusicPan() { return m_musicPan; }

// 8032C53C
bool ENgcAudio::IsPlayingMusic() {
  if (!gAudioEnabled) return false;
  return ENGCAudioStreamer::AnyIsActive() && m_musicPaused == 0;
}

// 8032C598
// Every 5 ms of output, on the mixer thread (GC: the AX frame callback; MIXUpdateSettings is the
// mixer's own business on PC).
void AudioFrameCallback() {
  if (!gAudioEnabled) return;
  ENGCAudioStreamer::Update();
  ENgcSFXStreamer::Update();
}

// 8032C5D4
EVoice* ENgcAudio::AudioAllocVoice() {
  auto* voice = new EVoice;
  voice->m_handle = -1;
  voice->reset();
  return voice;
}

// 8032C614
void ENgcAudio::FreeVoice(EVoice* voice) {
  UnbindVoice(voice);
  delete voice;
}

// 8032C65C
// Binds the sample `sampleId` (one reference) to a streamer voice. Without audio, or with no voice
// free, the voice is marked disabled.
void ENgcAudio::AudioBindVoice(EVoice* voice, unsigned int sampleId) {
  if (!gAudioEnabled) {
    voice->m_handle = -1;
    voice->m_bound = 1;
    voice->m_disabled = 1;
    return;
  }
  if (voice->m_handle != -1) UnbindVoice(voice);
  Lock(m_voiceMutex);
  ERSampledata* sample = sampleId ? _pAudiosampleman->AddRef(sampleId, nullptr, 0) : nullptr;
  if (sample && sample->m_data) voice->m_handle = fx->Allocate(sample, 0x7F);
  voice->m_sample = sample;
  if (voice->m_handle == -1) {
    voice->m_disabled = 1;
  } else {
    voice->m_disabled = 0;
    for (int i = 0; i < 64; ++i) {
      if (ActiveSFXHandles[i] == 0xFFFFFFFFu) {
        ActiveSFXHandles[i] = static_cast<unsigned int>(voice->m_handle);
        ActiveSFXSamples[i] = voice->m_sample;
        break;
      }
    }
  }
  voice->m_bound = 1;
  Unlock(m_voiceMutex);
}

// 8032C7F8
void ENgcAudio::UnbindVoice(EVoice* voice) {
  Lock(m_voiceMutex);
  if (voice->m_handle != -1) {
    fx->Free(static_cast<unsigned int>(voice->m_handle));
    _pAudiosampleman->DelRef(voice->m_sample, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
    for (int i = 0; i < 64; ++i) {
      if (ActiveSFXHandles[i] == static_cast<unsigned int>(voice->m_handle)) {
        ActiveSFXHandles[i] = 0xFFFFFFFFu;
        break;
      }
    }
    voice->m_handle = -1;
  }
  Unlock(m_voiceMutex);
}

// 8032C8E0
void ENgcAudio::GetVoiceState(EVoice* voice, EVoiceDesc& desc) {
  voice->m_playing = 0;
  if (voice->m_handle != -1) voice->m_playing = fx->IsPlaying(static_cast<unsigned int>(voice->m_handle));
  desc.m_mask = 0x1F;
  desc.m_looping = voice->m_sample ? voice->m_sample->m_looping : 0;
  desc.m_play = voice->m_playing;
  desc.m_pitch = voice->m_pitch;
  desc.m_volumeLeft = voice->m_volumeLeft;
  desc.m_volumeRight = voice->m_volumeRight;
}

// 8032C970
// Volume: the louder side sets the voice's volume and the ratio of the two its pan.
void ENgcAudio::SetVoiceState(EVoice* voice, EVoiceDesc& desc) {
  if (voice->m_disabled) return;
  const unsigned int handle = static_cast<unsigned int>(voice->m_handle);
  if (desc.m_mask & EVoiceDesc::kPlay) {
    if (desc.m_play == 0) fx->Pause(handle);
    else fx->Play(handle, voice->m_sample);
  }
  if (desc.m_mask & EVoiceDesc::kPitch) fx->SetPitchBend(handle, desc.m_pitch);
  if (desc.m_mask & EVoiceDesc::kVolume) {
    float left = desc.m_volumeLeft < 0.0f ? -desc.m_volumeLeft : desc.m_volumeLeft;
    float right = desc.m_volumeRight < 0.0f ? -desc.m_volumeRight : desc.m_volumeRight;
    float pan = 0.0f;
    if (right > left) pan = 1.0f - left / right;
    else if (right < left) pan = right / left - 1.0f;
    fx->SetVolume(handle, right < left ? left : right);
    fx->SetPan(handle, pan);
  }
}

// 8032CAA0
void ENgcAudio::SetSpeakerVolumes(EVoice* voice, int* dB10, unsigned int) {
  if (!gAudioEnabled || !voice || voice->m_disabled) return;
  fx->SetLeftVolume(static_cast<unsigned int>(voice->m_handle), dB10[0]);
  fx->SetRightVolume(static_cast<unsigned int>(voice->m_handle), dB10[1]);
}

// 8032CB0C
bool ENgcAudio::IsPausedDueToDiscError() { return m_pausedForDiscError != 0; }
// 8032CB14
void ENgcAudio::SetDiscErrorCallback(void (*callback)(bool)) { m_discErrorCallback = callback; }

// 8032CC1C
void ENgcAudio::Flush(bool) {}
// 8032CC20
void ENgcAudio::AddEvent(EA_EVENT, EMsgQueue&, unsigned int) {}
// 8032CC24
void ENgcAudio::RemoveEvent(EA_EVENT, EMsgQueue&, unsigned int) {}
