// Port of engine/e_audiostreamman.cpp: the "audiostreams" resource manager. A stream is not read: the
// resource records where it sits in audiostr.arc for the streamer (see ersampledata.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

EAudioStreamManager _audiostreamman;  // 804B3858

// 80323008
// (+0x54, the archive's access mode, is 1: streams are read in pieces.)
EAudioStreamManager::EAudioStreamManager() { m_archiveWritable = 1; }

// 80322EA8
EResource* EAudioStreamManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  const int start = file->Tell();
  auto* stream = new ERAudioStream;  // GC: 0x20 bytes from MainHeap
  stream->m_start = static_cast<unsigned int>(start);
  stream->m_end = static_cast<unsigned int>(start) + size - 1;
  stream->m_id = id;
  stream->m_file = file;
  file->Seek(static_cast<int>(size), EFile__SeekType::Cur);
  return stream;
}

// 80322FC8
ERAudioStream* EAudioStreamManager::AddRef(unsigned int id, EFile* file, int pos) {
  return static_cast<ERAudioStream*>(EResourceManager::AddRef(id, file, pos));
}
// 80322FE8
ERAudioStream* EAudioStreamManager::AddRef(char* name, EFile* file, int pos) {
  return static_cast<ERAudioStream*>(EResourceManager::AddRef(name, file, pos));
}
