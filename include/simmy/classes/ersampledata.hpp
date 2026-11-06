// @class ERSampledata after EResource EResourceManager
// @class EAudioSampleManager
// @class ENgcAudioSampleManager
// @class ERAudioStream
// @class EAudioStreamManager
// Audio resources. A sample ("samples", samples.arc; engine/e_ngcraudiosample.cpp and
// e_ngcaudiosampleman.cpp) is a 0x60-byte DSP-ADPCM header followed by its frames, and for a stereo
// sample a second header and channel. The GameCube kept only the headers in main memory and loaded
// the frames into an ARAM heap; on PC they stay in host memory. An audio stream ("audiostreams",
// audiostr.arc; e_audiostreamman.cpp) is never read: ERAudioStream records where it is and the
// streamer reads it while it plays.
//
// DSP-ADPCM header (big-endian, kept as stored): +0x00 sample count, +0x04 nibble count, +0x08 sample
// rate, +0x0C loop flag, +0x0E format, +0x10 loop start, +0x14 loop end, +0x18 current address,
// +0x1C 16 coefficients, +0x3C gain, +0x3E predictor/scale, +0x40 yn1, +0x42 yn2, +0x44 loop
// predictor/scale, yn1, yn2; this game also stores a loop flag at +0x4A.
#pragma once

struct ERSampledata : public EResource {
  static constexpr unsigned int kHeaderSize = 0x60;
  ERSampledata();  // 80338114
  ~ERSampledata() override;  // 8033815C
  void Load(EFile* file);  // 803381E8
  void LoadStereo(EFile* file);  // 80338258
  const unsigned char* GetSampleHeader() const;  // 80338324
  const unsigned char* GetSampleHeader2() const;  // 8033832C

  unsigned char* m_header = nullptr;   // +0x14 the (left) channel's header
  unsigned char* m_header2 = nullptr;  // +0x18 the right channel's header, stereo only
  unsigned char* m_data = nullptr;     // +0x1C the frames (an ARAM address on GC)
  unsigned char* m_data2 = nullptr;    // +0x20 the right channel's frames
  int m_looping = 0;                   // +0x24 header +0x4A != 0
};

struct EAudioSampleManager : public EResourceManager {
  EAudioSampleManager();  // 8032CC80
  ~EAudioSampleManager() override;  // 8032CCC0
  ERSampledata* AddRef(unsigned int id, EFile* file, int pos);  // 8032D94C
  ERSampledata* AddRef(char* name, EFile* file, int pos);  // 8032D96C
  int CanCache(EResource* res) override;  // 8032D98C
  EAHeap* GetHeap() override;  // 8036C33C
};

struct ENgcAudioSampleManager : public EAudioSampleManager {
  ENgcAudioSampleManager();  // 8032CCF0
  ~ENgcAudioSampleManager() override;  // 8032CD34
  void Init(char* name) override;  // 8032CD64
  void Shutdown() override;  // 8032CDA0
  int CanCache(EResource* res) override;  // 8032CDC0
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 8032CDE4
  void OnDelRef(ERSampledata* sample);  // 8032CFDC
  // The ARAM heap (GC only; samples live in host memory on PC).
  void heapInit();  // 8032D0B8
  RET_UNKNOWN heapAlloc(unsigned int);  // 8032D294
  void heapResortSmaller(ARAMBlock *);  // 8032D42C
  void heapResortLarger(ARAMBlock *);  // 8032D4F4
  void heapFindBlock(unsigned int) const;  // 8032D5CC
  RET_UNKNOWN heapIsFreeBlock(ARAMBlock *) const;  // 8032D600
  void heapFree(unsigned int);  // 8032D634
  RET_UNKNOWN heapWalk(bool) const;  // 8032D784
};

// 0x20 bytes on GC (vtable at 8046B958; only linkonce methods).
struct ERAudioStream : public EResource {
  EFile* m_file = nullptr;    // +0x14 the archive
  unsigned int m_start = 0;   // +0x18 first byte in it
  unsigned int m_end = 0;     // +0x1C last byte in it
};

struct EAudioStreamManager : public EResourceManager {
  EAudioStreamManager();  // 80323008
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 80322EA8
  ERAudioStream* AddRef(unsigned int id, EFile* file, int pos);  // 80322FC8
  ERAudioStream* AddRef(char* name, EFile* file, int pos);  // 80322FE8
};

extern ENgcAudioSampleManager _ngcAudioSampleMan;
