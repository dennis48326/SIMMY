// Port of engine/e_ngcaudiosampleman.cpp: the "samples" resource manager (_ngcAudioSampleMan,
// _pAudiosampleman). Each sample's header goes to main memory and its frames to the ARAM heap on GC;
// on PC both stay in host memory. The ARAM heap itself (heapInit/heapAlloc/heapFree...) is not used, but
// its size is: the frames' blocks are counted against the GC heap's 3.5 MB, and a load that would not fit
// first frees cached samples nobody holds, as GC's heapAlloc loop does (without it the cache kept up to
// 800 unused samples, ~40 MB, where GC keeps 3.5 MB). A sample never falls back to the empty "zero page"
// sample the original used when nothing could be freed: it is loaded anyway (PC).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <atomic>
#include <cstring>

namespace {
// GC: heapInit takes 0x380000 bytes of ARAM (AramHeapBegin..AramHeapEnd) for the samples' frames.
constexpr unsigned int kAramSampleHeapSize = 0x380000;
// The bytes of that heap the loaded samples' blocks take (loads on the loader thread, frees anywhere).
std::atomic<unsigned int> s_aramUsed{0};
}  // namespace

EAudioSampleManager* _pAudiosampleman = nullptr;  // 805000F4
ENgcAudioSampleManager _ngcAudioSampleMan;

// 8032CC80
EAudioSampleManager::EAudioSampleManager() { _pAudiosampleman = this; }
// 8032CCC0
EAudioSampleManager::~EAudioSampleManager() = default;
// 8032D94C
ERSampledata* EAudioSampleManager::AddRef(unsigned int id, EFile* file, int pos) {
  return static_cast<ERSampledata*>(EResourceManager::AddRef(id, file, pos));
}
// 8032D96C
ERSampledata* EAudioSampleManager::AddRef(char* name, EFile* file, int pos) {
  return static_cast<ERSampledata*>(EResourceManager::AddRef(name, file, pos));
}
// 8032D98C
int EAudioSampleManager::CanCache(EResource*) { return 1; }

// 8032CCF0
// (+0x54, the archive's access mode, is 1: samples are read in pieces.)
ENgcAudioSampleManager::ENgcAudioSampleManager() { m_archiveWritable = 1; }
// 8032CD34
ENgcAudioSampleManager::~ENgcAudioSampleManager() = default;

// 8032CD64
void ENgcAudioSampleManager::Init(char* name) { EResourceManager::Init(name); }  // GC: heapInit() first
// 8032CDA0
void ENgcAudioSampleManager::Shutdown() { EResourceManager::Shutdown(); }

// 8032CDC0
// The zero-page stand-in is never cached (GC). Every PC sample has its own data.
int ENgcAudioSampleManager::CanCache(EResource* res) {
  return res && !static_cast<ERSampledata*>(res)->m_data ? 0 : 1;
}

// 8032CDE4
// Header, then the frames; a second header and channel at half the resource would be the right channel
// of a stereo sample, but the original's test, kept here, only takes one for a resource no larger than
// 0x61 bytes: every sample is read as one channel (all 7,696 in samples.arc are mono anyway: none has
// room for a second channel). The archive is left just past the resource.
EResource* ENgcAudioSampleManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  const int start = file->Tell();
  // GC: heapAlloc of the frames' block ((size + 0x1F9F) & ~0x1FFF); while it fails and cached samples
  // are left, MakeSpace frees some (the oldest unused) and it is tried again.
  const unsigned int block = (size + 0x1F9F) & ~0x1FFFu;
  while (s_aramUsed.load() + block > kAramSampleHeapSize && GetUnreferencedResourceCount() != 0 && MakeSpace() != 0) {
  }
  auto* sample = new ERSampledata;
  sample->Load(file);
  sample->m_looping = simmy::be16(sample->m_header + 0x4A) != 0;
  const unsigned int bytes = simmy::be32(sample->m_header + 4) >> 1;
  // GC: the frames went to an ARAM block of (size + 0x1F9F) & ~0x1FFF bytes, read in 32-byte units.
  sample->m_data = new unsigned char[(bytes + 0x1F) & ~0x1Fu]();
  file->Read(sample->m_data, bytes);
  if (static_cast<int>(bytes - size + 0x61) < static_cast<int>(bytes)) {  // one channel (GC)
    sample->m_data2 = nullptr;
    sample->m_header2 = nullptr;
  } else {
    file->Seek(start + static_cast<int>(size >> 1), EFile__SeekType::Set);
    sample->LoadStereo(file);
    const unsigned int bytes2 = simmy::be32(sample->m_header2 + 4) >> 1;
    sample->m_data2 = new unsigned char[(bytes2 + 0x1F) & ~0x1Fu]();
    file->Read(sample->m_data2, bytes2);
  }
  file->Seek(start + static_cast<int>(size), EFile__SeekType::Set);
  sample->m_id = id;
  sample->m_aramSize = block;
  s_aramUsed += block;
  return sample;
}

// 8032CFDC
// Frees the sample's frames (the ARAM block on GC).
void ENgcAudioSampleManager::OnDelRef(ERSampledata* sample) {
  s_aramUsed -= sample->m_aramSize;
  sample->m_aramSize = 0;
  delete[] sample->m_data;
  sample->m_data = nullptr;
  delete[] sample->m_data2;
  sample->m_data2 = nullptr;
}
