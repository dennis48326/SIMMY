// Port of engine/e_ngcsamplestream.cpp, rewritten for PC: ENgcSFXStreamer, sample playback.
// The original owned 62 AX voices with MIX channels and played each sample's DSP-ADPCM frames from
// ARAM; a stereo sample took two voices (left panned hard left, right hard right). Here the voices
// are the PC mixer's (platform/pc_audio.cpp) and play the frames from host memory. The record table,
// its flags and the volume/pan arithmetic are the original's; disable_IRQ/enable_IRQ become the
// mixer lock.
#include "simmy/types_gen.hpp"
#include "simmy/pc_audio.hpp"
#include "simmy/detmath.hpp"

#include <cmath>

namespace audio = simmy::audio;

namespace {

// One voice record (0x44 bytes on GC).
struct SfxVoice {
  enum : unsigned int { kPlaying = 0x80000000u, kPaused = 0x40000000u, kLoop = 0x20000000u, kBit28 = 0x10000000u };
  audio::Voice* voice = nullptr;          // +0x00 AX voice
  const unsigned char* header = nullptr;  // +0x24 sample header; non-null while allocated
  const unsigned char* data = nullptr;    // +0x28 the frames (ARAM address on GC)
  unsigned int nibbles = 0;               // +0x2C header nibble count
  unsigned char volume = 0;               // +0x30 0..127
  unsigned char pan = 0x40;               // +0x31 0..127
  SfxVoice* left = nullptr;               // +0x34 on a right-channel record: its left channel
  SfxVoice* right = nullptr;              // +0x38 on a left-channel record: its right channel
  unsigned int flags = 0;                 // +0x40
};

constexpr int kVoices = 0x3E;
SfxVoice voices[kVoices];  // 804DB4D4

// GetDB: the fader setting (0.1 dB) for a 0..127 volume, about 22.6 dB per decade; -90.4 dB
// (silent) at a tenth of full volume and below.
int VolumeToDb(unsigned char volume) {
  float f = 1.0f - static_cast<float>(0x7F - volume) / 127.0f;
  if (f < 0.1f) f = 0.1f;
  int db = static_cast<int>(simmy::dm::log10(f) * 904.0f);
  if (db > -904) db >>= 2;
  return db;
}

// GetVoiceHandle: the first free record, its flags cleared; -1 when all are taken.
int FindFreeRecord() {
  for (int i = 0; i < kVoices; ++i) {
    if (!voices[i].header) {
      voices[i].flags &= ~(SfxVoice::kPlaying | SfxVoice::kBit28 | SfxVoice::kPaused);
      return i;
    }
  }
  return -1;
}

void AllocateRecord(SfxVoice& v, const unsigned char* header, const unsigned char* data, unsigned char volume,
                    unsigned char pan, bool loop) {
  v.header = header;
  v.data = data;
  v.nibbles = simmy::be32(header + 4);
  v.volume = volume;
  v.pan = pan;
  v.flags = (v.flags & ~SfxVoice::kLoop) | (loop ? SfxVoice::kLoop : 0);
}

// Programs a record's voice from its sample header (AXSetVoiceAddr/Adpcm/Src) and starts it. The
// loop address is the first sample; a one-shot voice "loops" to the zero page on GC, i.e. it ends.
void StartRecord(SfxVoice& v) {
  audio::SetFader(v.voice, VolumeToDb(v.volume));
  audio::SetPan(v.voice, v.pan);
  audio::AdpcmState st;
  for (int i = 0; i < 16; ++i) st.coef[i] = static_cast<short>(simmy::be16(v.header + 0x1C + 2 * i));
  st.gain = simmy::be16(v.header + 0x3C);
  st.predScale = simmy::be16(v.header + 0x3E);
  st.yn1 = static_cast<short>(simmy::be16(v.header + 0x40));
  st.yn2 = static_cast<short>(simmy::be16(v.header + 0x42));
  const bool loop = (v.flags & SfxVoice::kLoop) != 0;
  audio::SetAdpcm(v.voice, v.data, 2, v.nibbles - 1, loop, 2, st);
  audio::SetRate(v.voice, static_cast<float>(simmy::be32(v.header + 8)));  // ratio rate/32028.66 at the DSP rate
  audio::SetRunning(v.voice, true);
  audio::SetMute(v.voice, false);
}

}  // namespace

// 8034C7B8
ENgcSFXStreamer::ENgcSFXStreamer() {
  for (SfxVoice& v : voices) {
    v.header = nullptr;
    v.voice = audio::AcquireVoice();
    // MIXInitChannel: input 0 dB, fader 0 dB, centre pan; then muted until played.
    audio::SetInput(v.voice, 0);
    audio::SetFader(v.voice, 0);
    audio::SetPan(v.voice, 0x40);
    audio::SetMute(v.voice, true);
    v.left = nullptr;
    v.right = nullptr;
  }
}

// 8034C858
ENgcSFXStreamer::~ENgcSFXStreamer() = default;

// 8034C880
// Mutes the voices of one-shot samples that have played to their end.
void ENgcSFXStreamer::Update() {
  audio::Lock lock;
  for (SfxVoice& v : voices) {
    if (!v.header || !v.voice || (v.flags & SfxVoice::kLoop)) continue;
    if ((v.flags & SfxVoice::kPlaying) && !(v.flags & SfxVoice::kPaused) && audio::HasEnded(v.voice)) {
      audio::SetRunning(v.voice, false);
      v.flags &= ~SfxVoice::kPlaying;
      audio::SetMute(v.voice, true);
    }
  }
}

// 8034C92C
bool ENgcSFXStreamer::IsPlaying(unsigned int handle) {
  audio::Lock lock;
  return (voices[handle].flags & SfxVoice::kPlaying) != 0;
}

// 8034C984
// Takes one record for a mono sample (centre pan) or two for a stereo one (hard left and right).
int ENgcSFXStreamer::Allocate(ERSampledata* sample, unsigned char volume) {
  audio::Lock lock;
  const int handle = FindFreeRecord();
  if (handle == -1) return -1;
  SfxVoice& v = voices[handle];
  v.header = sample->m_header;  // taken (GC: set to 1 first)
  const bool loop = (sample->m_looping & 1) != 0;
  if (!sample->m_header2) {
    AllocateRecord(v, sample->m_header, sample->m_data, volume, 0x40, loop);
    v.left = v.right = nullptr;
    return handle;
  }
  const int second = FindFreeRecord();
  if (second == -1) {
    v.header = nullptr;
    return -1;
  }
  SfxVoice& r = voices[second];
  AllocateRecord(v, sample->m_header, sample->m_data, volume, 0x00, loop);
  v.left = nullptr;
  v.right = &r;
  AllocateRecord(r, sample->m_header2, sample->m_data2, volume, 0x7F, loop);
  r.left = &v;
  r.right = nullptr;
  return handle;
}

// 8034CB00
// Starts the sample from the beginning, or resumes it when it was paused while playing.
void ENgcSFXStreamer::Play(unsigned int handle, ERSampledata* sample) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  SfxVoice& v = voices[handle];
  v.flags = ((sample->m_looping & 1) ? SfxVoice::kLoop : 0) | (v.flags & ~SfxVoice::kLoop);
  if (!v.voice || !(v.flags & SfxVoice::kPlaying)) {
    if (v.voice) {
      StartRecord(v);
      if (SfxVoice* r = v.right) {
        StartRecord(*r);
        r->flags = (r->flags & ~SfxVoice::kPaused) | SfxVoice::kPlaying;
      }
      v.flags &= ~SfxVoice::kPaused;
    }
    v.flags |= SfxVoice::kPlaying;
  } else {
    audio::SetRunning(v.voice, true);
    v.flags &= ~SfxVoice::kPaused;
    for (SfxVoice* other : {v.right, v.left}) {
      if (!other) continue;
      audio::SetRunning(other->voice, true);
      other->flags &= ~SfxVoice::kPaused;
    }
  }
}

// 8034D058
void ENgcSFXStreamer::Pause(unsigned int handle) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  SfxVoice& v = voices[handle];
  const bool stop = v.voice && (v.flags & SfxVoice::kPlaying);
  if (stop) audio::SetRunning(v.voice, false);
  v.flags |= SfxVoice::kPaused;
  for (SfxVoice* other : {v.right, v.left}) {
    if (!other) continue;
    if (stop) audio::SetRunning(other->voice, false);
    other->flags |= SfxVoice::kPaused;
  }
}

// 8034D15C
void ENgcSFXStreamer::Free(unsigned int handle) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  SfxVoice& v = voices[handle];
  if (!v.voice || !v.header) return;
  audio::SetRunning(v.voice, false);
  audio::SetMute(v.voice, true);
  v.header = nullptr;
  v.flags &= ~SfxVoice::kPlaying;
  for (SfxVoice* other : {v.right, v.left}) {
    if (!other || !other->header) continue;
    audio::SetRunning(other->voice, false);
    audio::SetMute(other->voice, true);
    other->flags &= ~SfxVoice::kPlaying;
    other->header = nullptr;
  }
}

// 8034D274
// (Only the main voice: the right channel of a stereo sample keeps its pitch.)
void ENgcSFXStreamer::SetPitchBend(unsigned int handle, float bend) {
  if (handle == 0xFFFFFFFFu || !voices[handle].voice) return;
  audio::Lock lock;
  if (!voices[handle].header) return;  // PC: the original read the freed record's stale header
  audio::SetRate(voices[handle].voice, static_cast<float>(simmy::be32(voices[handle].header + 8)) * bend);
}

// 8034D310
void ENgcSFXStreamer::SetLeftVolume(unsigned int handle, float volume) { SetVolume(handle, volume); }

// 8034D330
void ENgcSFXStreamer::SetLeftVolume(unsigned int handle, int dB10) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  if (voices[handle].voice) audio::SetFader(voices[handle].voice, dB10 * 904 / 10000);
}

// 8034D3A4
void ENgcSFXStreamer::SetRightVolume(unsigned int handle, float volume) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  if (SfxVoice* r = voices[handle].right) {
    r->volume = static_cast<unsigned char>(static_cast<int>(volume * 127.0f));
    audio::SetFader(r->voice, VolumeToDb(r->volume));
  }
}

// 8034D43C
void ENgcSFXStreamer::SetRightVolume(unsigned int handle, int dB10) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  if (SfxVoice* r = voices[handle].right) audio::SetFader(r->voice, dB10 * 904 / 10000);
}

// 8034D4B8
void ENgcSFXStreamer::SetVolume(unsigned int handle, float volume) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  SfxVoice& v = voices[handle];
  v.volume = static_cast<unsigned char>(static_cast<int>(volume * 127.0f));
  if (v.voice) audio::SetFader(v.voice, VolumeToDb(static_cast<unsigned char>(static_cast<int>(volume * 127.0f))));
}

// 8034D54C
// -1 (left) .. 1 (right).
void ENgcSFXStreamer::SetPan(unsigned int handle, float pan) {
  if (handle == 0xFFFFFFFFu) return;
  audio::Lock lock;
  const unsigned int p = static_cast<unsigned int>(pan * 63.0f + 64.0f);
  voices[handle].pan = static_cast<unsigned char>(p);
  if (voices[handle].voice) audio::SetPan(voices[handle].voice, static_cast<int>(p & 0xFF));
}
