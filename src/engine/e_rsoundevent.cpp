// Port of engine/e_rsoundevent.cpp and engine/e_soundeventman.cpp: sound events ("soundevents").
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
unsigned char ReadU8(EFile* file) {
  unsigned char b = 0;
  file->Read(&b, 1);
  return b;
}
unsigned int ReadU32(EFile* file) {
  unsigned char b[4] = {};
  file->Read(b, 4);
  return simmy::be32(b);
}
}  // namespace

// 803699BC
ERSoundEvent::ERSoundEvent() = default;

// 80369A00
// (The program's reference is not released, as on GC.)
ERSoundEvent::~ERSoundEvent() = default;

// 80369A68
// The 'SEVT' header (version 1), the type byte, then (when the next byte is set) the playing parameters,
// the lists of hit patches and the program's id (soundtrackdatas, one reference).
void ERSoundEvent::Load(EFile* file) {
  if (!file) return;
  EDataHeader header;
  if (!header.Read(*file, 0x53455654 /* 'SEVT' */, 1, 1)) return;
  if (header.GetName()) m_name = header.GetName();
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  m_type = ReadU8(file);
  if (!ReadU8(file)) return;
  m_argsType = ReadU8(file);
  m_duckPri = ReadU8(file);
  m_group = ReadU8(file);
  ReadU8(file);  // (unused)
  unsigned char v[2] = {};
  file->Read(v, 2);
  m_volume = simmy::be16(v);
  m_prob = ReadU8(file);
  m_loop = ReadU8(file);
  m_duckFlags = ReadU8(file);
  m_noRestart = ReadU8(file);
  m_superDuck = ReadU8(file);
  m_minRange = ReadU8(file);
  m_maxRange = ReadU8(file);
  const unsigned int lists = ReadU32(file);
  m_hitLists.assign(lists, {});
  for (unsigned int i = 0; i < lists; ++i) {
    const unsigned int n = ReadU32(file);
    m_hitLists[i].resize(n);
    for (unsigned int j = 0; j < n; ++j) {
      m_hitLists[i][j].m_stream = ReadU32(file);
      m_hitLists[i][j].m_sample = ReadU32(file);
    }
  }
  const unsigned int program = ReadU32(file);
  m_program = g_soundtrackdataman.AddRef(program, nullptr, 0);
}

// 80369DE0
void ERSoundEvent::Refresh(EFile* file) {
  if (file) Load(file);
}

// 80369F78
void ERSoundEvent::RegisterType(unsigned short) {}
// 80369FCC
void ERSoundEvent::CreateCopy() const {}

// 8036A050
int ERSoundEvent::GetEventType() { return m_type; }
// 8036A058
int ERSoundEvent::GetVolume() { return m_volume; }
// 8036A060
int ERSoundEvent::GetProb() { return m_prob; }
// 8036A068
void ERSoundEvent::SetVolume(int volume) { m_volume = static_cast<unsigned short>(volume); }
// 8036A070
void ERSoundEvent::SetProb(int prob) { m_prob = static_cast<unsigned char>(prob); }

// 80369F30 ERSoundEvent::GetTypeInfo
// 80369F3C ERSoundEvent::GetTypeName
// 80369F48 ERSoundEvent::GetTypeKey
// 80369F54 ERSoundEvent::GetTypeVersion
// 80369EF0 ERSoundEvent::SafeDelete
// 80369E78 ERSoundEvent::New
// 80369EA0 ERSoundEvent::Construct
// 80369ECC ERSoundEvent::Destruct
// 80369F60 ERSoundEvent::GetTypeInfoStatic
// 80369F6C ERSoundEvent::GetReadVersion
SIMMY_STORABLE_IMPL(ERSoundEvent, "ERSoundEvent", 1, EResource)

// ---------------------------------------------------------------------------------------------------
// ESoundEventManager (g_soundeventman)

// 80328374
EAHeap* ESoundEventManager::GetHeap() { return AudioHeap(); }

// 80328394
EResource* ESoundEventManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* event = new ERSoundEvent;
  event->m_id = id;
  const int start = file->Tell();
  event->Load(file);
  file->Seek(start + static_cast<int>(size), EFile__SeekType::Set);  // PC: past the resource whatever was read
  return event;
}

// 80328458
ERSoundEvent* ESoundEventManager::AddRef(unsigned int id, EFile* file, int pos) {
  return static_cast<ERSoundEvent*>(EResourceManager::AddRef(id, file, pos));
}
// 80328478
ERSoundEvent* ESoundEventManager::AddRef(char* name, EFile* file, int pos) {
  return static_cast<ERSoundEvent*>(EResourceManager::AddRef(name, file, pos));
}
// 80328498
int ESoundEventManager::CanCache(EResource*) { return 1; }

// 803284A0 global constructors keyed to ESoundEventManager::GetHeap (g_soundeventman: game/platform
// globals, src/platform/unported_globals.cpp)
// 803284CC global destructors keyed to ESoundEventManager::GetHeap
