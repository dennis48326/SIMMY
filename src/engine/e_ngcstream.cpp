// Port of engine/e_ngcstream.cpp, rewritten for PC: ENGCAudioStreamer, music streaming.
// The original had two stream slots, each with an AX voice looping over a 0x14180-byte ARAM ring:
// DVD reads of half a ring at a time from "audiostr.arc", ARAM DMA, and a state machine
// (dvd_callback, activate_stream, arq_callback, stream_data) that refilled each half as the voice
// left it, looped the stream or shut it down at its end. A stereo stream used both slots, started
// together.
//
// Here a slot's reader thread reads the stream's data from the archive into a StreamBuffer while the
// mixer voice plays it (pc_audio.cpp waits for data that has not arrived yet). Handles, the slot
// table, volume, pan, pause and the end of a stream behave as before.
#include "simmy/types_gen.hpp"
#include "simmy/pc_audio.hpp"
#include "simmy/platform.hpp"
#include "simmy/detmath.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

namespace audio = simmy::audio;

namespace {

// One stream slot (GC Stream, 0x78 bytes).
struct StreamSlot {
  enum : unsigned int { kFreeing = 0x200000, kPaused = 0x400000 };
  int state = 0;                       // +0x00 0 free; 5 playing (GC: 1..4 while starting)
  unsigned int handle = 0;             // +0x04
  StreamSlot* right = nullptr;             // +0x08 on the left channel of a stereo stream
  StreamSlot* left = nullptr;              // +0x0C on the right channel
  NGCStreamInfo* info = nullptr;       // +0x10
  unsigned char volume = 0x7F;         // +0x14
  unsigned char pan = 0x40;            // +0x15
  audio::Voice* voice = nullptr;       // +0x1C AX voice
  unsigned int flags = 0;              // +0x74
  // PC: the reader
  std::shared_ptr<audio::StreamBuffer> buffer;
  std::shared_ptr<std::atomic<bool>> cancel;
  std::thread reader;
};

constexpr int kSlots = 2;
StreamSlot streams[kSlots];            // 804DC8F8
unsigned int next_stream_handle = 0;

// GetDB (the same table as e_ngcsamplestream.cpp's).
int VolumeToDb(unsigned char volume) {
  float f = 1.0f - static_cast<float>(0x7F - volume) / 127.0f;
  if (f < 0.1f) f = 0.1f;
  int db = static_cast<int>(simmy::dm::log10(f) * 904.0f);
  if (db > -904) db >>= 2;
  return db;
}

void JoinReader(StreamSlot& s) {
  if (s.cancel) s.cancel->store(true);
  if (s.reader.joinable()) s.reader.join();
  s.cancel.reset();
}

// Reads `size` bytes at `offset` of the archive into the buffer, in 64 KB pieces.
void ReadStream(std::shared_ptr<audio::StreamBuffer> buffer, std::shared_ptr<std::atomic<bool>> cancel,
                unsigned int offset, unsigned int size) {
  // PC: a handle of its own per stream (positional reads of the disc image, safe beside the game's).
  simmy::DataFile* f = simmy::data_open("data/audiostr.arc");
  if (!f) {
    std::fprintf(stderr, "[audio] cannot read data/audiostr.arc\n");
    buffer->failed = true;
    return;
  }
  std::size_t done = 0;
  while (done < size && !cancel->load()) {
    const std::size_t n = simmy::data_read_at(f, static_cast<std::uint64_t>(offset) + done, buffer->data.data() + done,
                                              std::min<std::size_t>(0x10000, size - done));
    if (n == 0) {
      buffer->failed = true;
      break;
    }
    done += n;
    buffer->available.store(done, std::memory_order_release);
  }
  simmy::data_close(f);
}

// get_free: a free slot and a new handle (never 0 or one in use).
unsigned int FindFreeSlot(StreamSlot*& out) {
  for (StreamSlot& s : streams) {
    if (s.state != 0) continue;
    unsigned int handle;
    bool taken;
    do {
      handle = ++next_stream_handle;
      if (handle == 0xFFFFFFFFu || handle == 0) handle = next_stream_handle = 1;
      taken = false;
      for (StreamSlot& o : streams) taken |= o.state != 0 && o.handle == handle;
    } while (taken);
    audio::SetInput(s.voice, -904);
    s.state = 1;
    s.handle = handle;
    s.right = s.left = nullptr;
    out = &s;
    return handle;
  }
  return 0xFFFFFFFFu;
}

// PC: joins the reader threads of free slots (finished, or told to stop), outside the mixer lock.
void ReapReaders() {
  for (StreamSlot& s : streams) {
    bool idle;
    {
      audio::Lock lock;
      idle = s.state == 0;
    }
    if (idle) JoinReader(s);
  }
}

int SlotIndex(unsigned int handle) {
  for (int i = 0; i < kSlots; ++i)
    if (streams[i].state != 0 && streams[i].handle == handle) return i;
  return -1;
}

// init_stream + activate_stream: programs the voice for the stream's ADPCM and starts its reader.
// The voice waits for the start (state 5) until arq_callback's equivalent below.
void InitSlot(StreamSlot& s, NGCStreamInfo* info, unsigned char volume, unsigned char pan) {
  JoinReader(s);  // normally done by ReapReaders; a slot that has only just ended may still have one
  s.flags &= ~(StreamSlot::kFreeing | StreamSlot::kPaused);
  s.volume = volume;
  s.pan = pan;
  s.info = info;
  s.buffer = std::make_shared<audio::StreamBuffer>();
  s.buffer->data.resize(info->m_size);
  s.cancel = std::make_shared<std::atomic<bool>>(false);
  audio::AdpcmState st;
  for (int i = 0; i < 16; ++i) st.coef[i] = info->m_coef[i];
  st.gain = info->m_gain;
  st.predScale = info->m_predScale;
  st.yn1 = info->m_yn1;
  st.yn2 = info->m_yn2;
  const bool loop = (info->m_flags & NGCStreamInfo::kLoop) != 0;
  audio::SetFader(s.voice, -904);
  audio::SetPan(s.voice, s.pan);
  audio::SetAdpcmStream(s.voice, s.buffer, 2, info->m_size * 2 - 1, loop, 2, st);
  audio::SetRate(s.voice, static_cast<float>(info->m_sampleRate));
  s.reader = std::thread(ReadStream, s.buffer, s.cancel, info->m_offset, info->m_size);
  s.state = 2;
}

// arq_callback's last step: unmute and run (unless paused).
void StartSlot(StreamSlot& s) {
  if (!(s.flags & StreamSlot::kPaused)) audio::SetRunning(s.voice, true);
  audio::SetFader(s.voice, VolumeToDb(s.volume));
  audio::SetInput(s.voice, 0);
  s.state = 5;
}

// stream_free: silenced at once; the slot is released by Update.
void FreeSlot(StreamSlot& s) {
  if (s.state == 0 || (s.flags & StreamSlot::kFreeing)) return;
  audio::SetFader(s.voice, -904);
  audio::SetInput(s.voice, -904);
  s.flags |= StreamSlot::kFreeing;
}

// shutdown_stream: stops the voice and frees the slot.
void ShutdownSlot(StreamSlot& s) {
  if (s.state == 0) return;
  audio::SetFader(s.voice, -904);
  audio::SetRunning(s.voice, false);
  if (s.cancel) s.cancel->store(true);
  s.state = 0;
}

}  // namespace

// 8034E004
ENGCAudioStreamer::ENGCAudioStreamer() {
  for (StreamSlot& s : streams) {
    s.state = 0;
    s.voice = audio::AcquireVoice();
    audio::SetVoiceBus(s.voice, audio::kBusMusic);  // PC: the music volume option scales the streams
    // MIXInitChannel: input, aux and fader silent, the slot's pan.
    audio::SetInput(s.voice, -904);
    audio::SetFader(s.voice, -904);
    audio::SetPan(s.voice, s.pan);
  }
  next_stream_handle = 0;
}

// 8034E0E8
ENGCAudioStreamer::~ENGCAudioStreamer() {
  for (int i = kSlots - 1; i >= 0; --i) {
    StreamSlot& s = streams[i];
    {
      audio::Lock lock;
      if (s.state != 0) Free(s.handle);
      ShutdownSlot(s);
    }
    JoinReader(s);
    audio::FreeVoice(s.voice);
    s.voice = nullptr;
    s.buffer.reset();
  }
}

// 8034E174
// On the mixer thread every 5 ms: releases freed slots and those whose voice has played to the end.
void ENGCAudioStreamer::Update() {
  audio::Lock lock;
  for (StreamSlot& s : streams) {
    if (s.state == 0) continue;
    if ((s.flags & StreamSlot::kFreeing) || (s.state == 5 && audio::HasEnded(s.voice))) ShutdownSlot(s);
  }
}

// 8034E248
void ENGCAudioStreamer::Pause(unsigned int handle) {
  audio::Lock lock;
  const int i = SlotIndex(handle);
  if (i == -1) return;
  audio::SetRunning(streams[i].voice, false);
  audio::SetFader(streams[i].voice, -904);
  streams[i].flags |= StreamSlot::kPaused;
}

// 8034E2C8
void ENGCAudioStreamer::Resume(unsigned int handle) {
  audio::Lock lock;
  const int i = SlotIndex(handle);
  if (i == -1) return;
  streams[i].flags &= ~StreamSlot::kPaused;
  if (streams[i].state == 5) audio::SetRunning(streams[i].voice, true);
  audio::SetFader(streams[i].voice, VolumeToDb(streams[i].volume));
}

// 8034E354
unsigned int ENGCAudioStreamer::Allocate(NGCStreamInfo* info, unsigned char volume, unsigned char pan) {
  ReapReaders();
  audio::Lock lock;
  StreamSlot* s = nullptr;
  const unsigned int handle = FindFreeSlot(s);
  if (handle != 0xFFFFFFFFu) {
    InitSlot(*s, info, volume, pan);
    StartSlot(*s);
  }
  info->m_handle = handle;
  return handle;
}

// 8034E404
// Both channels start in the same mixer step, so they stay sample-aligned.
void ENGCAudioStreamer::AllocateStereo(NGCStreamInfo* left, NGCStreamInfo* right, unsigned char volume) {
  ReapReaders();
  audio::Lock lock;
  StreamSlot* l = nullptr;
  StreamSlot* r = nullptr;
  unsigned int lh = FindFreeSlot(l), rh = 0xFFFFFFFFu;
  if (lh != 0xFFFFFFFFu) {
    rh = FindFreeSlot(r);
    if (rh == 0xFFFFFFFFu) {
      lh = 0xFFFFFFFFu;
      l->state = 0;
    } else {
      l->right = r;
      r->left = l;
      InitSlot(*l, left, volume, 0x00);
      InitSlot(*r, right, volume, 0x7F);
      StartSlot(*l);
      StartSlot(*r);
    }
  }
  left->m_handle = lh;
  right->m_handle = rh;
}

// 8034E560
bool ENGCAudioStreamer::Free(unsigned int handle) {
  audio::Lock lock;
  const int i = SlotIndex(handle);
  if (i == -1) return false;
  FreeSlot(streams[i]);
  if (streams[i].right) FreeSlot(*streams[i].right);
  if (streams[i].left) FreeSlot(*streams[i].left);
  return true;
}

// 8034E5E8
bool ENGCAudioStreamer::IsActive(unsigned int handle) {
  audio::Lock lock;
  const int i = SlotIndex(handle);
  return i != -1 && streams[i].state != 0;
}

// 8034E64C
bool ENGCAudioStreamer::AnyIsActive() {
  audio::Lock lock;
  for (StreamSlot& s : streams)
    if (s.state != 0) return true;
  return false;
}

// 8034E6A4
// Applied once the stream (and both channels of a stereo one) is playing; remembered before that.
void ENGCAudioStreamer::SetVolume(unsigned int handle, unsigned char volume) {
  audio::Lock lock;
  const int i = SlotIndex(handle);
  if (i == -1) return;
  StreamSlot& s = streams[i];
  bool playing = s.state == 5;
  if (playing && s.right) playing = s.right->state == 5;
  if (playing && s.left) playing = s.left->state == 5;
  if (s.state == 2 && !(s.flags & StreamSlot::kFreeing)) s.volume = volume;
  if (playing && !(s.flags & StreamSlot::kFreeing)) {
    s.volume = volume;
    audio::SetFader(s.voice, VolumeToDb(volume));
  }
}

// 8034E7D4
void ENGCAudioStreamer::SetPan(unsigned int handle, unsigned char pan) {
  audio::Lock lock;
  const int i = SlotIndex(handle);
  if (i == -1 || streams[i].state == 0) return;
  streams[i].pan = pan;
  audio::SetPan(streams[i].voice, pan);
}
