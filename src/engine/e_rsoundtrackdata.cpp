// Port of engine/e_rsoundtrackdata.cpp and engine/e_soundtrackdataman.cpp: sound track programs
// ("soundtrackdatas"), run by cTrack (game/patch.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 8036A238
// Registers 0x32 and 0x37 of older programs are the patch (11) and register 6.
void RemapRegister(signed char& reg) {
  if (reg == 0x32) reg = 0x0B;
  else if (reg == 0x37) reg = 0x06;
}

namespace {
void Remap(unsigned char* b) {
  auto* r = reinterpret_cast<signed char*>(b);
  RemapRegister(*r);
}
}  // namespace

// 8036A144
ERSoundTrackData::ERSoundTrackData() = default;
// 8036A194
ERSoundTrackData::~ERSoundTrackData() = default;

// 8036A278
// The 'STRD' header (version 1) and the instructions; the register operands of each opcode are renumbered
// (A for 4..5, 9, 0x0E, 0x27, 0x33, 0x44..0x45, 0x53..0x58; A, B for 6, 0x10..0x14, 0x4B, 0x4D, 0x59; A, B, C for
// 0x18 and 0x48).
void ERSoundTrackData::Load(EFile* file) {
  if (!file) return;
  EDataHeader header;
  if (!header.Read(*file, 0x53545244 /* 'STRD' */, 1, 1)) return;
  const unsigned int size = header.m_dataSize;
  m_flags = size << 8 | (m_flags & 0xff);
  const unsigned int count = size >> 2;
  m_program.assign(count * 4, 0);
  if (count) file->Read(m_program.data(), static_cast<int>(count * 4));
  for (unsigned int i = 0; i < count; ++i) {
    unsigned char* ins = &m_program[i * 4];
    const int op = static_cast<signed char>(ins[0]);
    switch (op) {
      case 0x04: case 0x05: case 0x09: case 0x0E: case 0x27: case 0x33: case 0x44: case 0x45:
      case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x58:
        Remap(ins + 1);
        break;
      case 0x06: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x4B: case 0x4D: case 0x59:
        Remap(ins + 1);
        Remap(ins + 2);
        break;
      case 0x18: case 0x48:
        Remap(ins + 1);
        Remap(ins + 2);
        Remap(ins + 3);
        break;
      default:
        break;
    }
  }
}

// 8036A650
void ERSoundTrackData::RegisterType(unsigned short) {}
// 8036A6A4
void ERSoundTrackData::CreateCopy() const {}

// 8036A6C4
const unsigned char* ERSoundTrackData::GetInstruction(unsigned int index) { return &m_program[index * 4]; }
// 8036A6D4
unsigned int ERSoundTrackData::GetProgramSize() { return static_cast<unsigned int>(m_program.size() / 4); }

// 8036A608 ERSoundTrackData::GetTypeInfo
// 8036A614 ERSoundTrackData::GetTypeName
// 8036A620 ERSoundTrackData::GetTypeKey
// 8036A62C ERSoundTrackData::GetTypeVersion
// 8036A5C8 ERSoundTrackData::SafeDelete
// 8036A554 ERSoundTrackData::New
// 8036A584 ERSoundTrackData::Construct
// 8036A5A4 ERSoundTrackData::Destruct
// 8036A638 ERSoundTrackData::GetTypeInfoStatic
// 8036A644 ERSoundTrackData::GetReadVersion
SIMMY_STORABLE_IMPL(ERSoundTrackData, "ERSoundTrackData", 1, EResource)

// ---------------------------------------------------------------------------------------------------
// ESoundTrackDataManager (g_soundtrackdataman)

// 803284F8
EResource* ESoundTrackDataManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* program = new ERSoundTrackData;
  program->m_id = id;
  const int start = file->Tell();
  program->Load(file);
  file->Seek(start + static_cast<int>(size), EFile__SeekType::Set);
  return program;
}

// 803285B8
ERSoundTrackData* ESoundTrackDataManager::AddRef(unsigned int id, EFile* file, int pos) {
  return static_cast<ERSoundTrackData*>(EResourceManager::AddRef(id, file, pos));
}
// 803285D8
ERSoundTrackData* ESoundTrackDataManager::AddRef(char* name, EFile* file, int pos) {
  return static_cast<ERSoundTrackData*>(EResourceManager::AddRef(name, file, pos));
}
// 803285F8
int ESoundTrackDataManager::CanCache(EResource*) { return 1; }

// 80328600 global constructors keyed to ESoundTrackDataManager::AllocateAndLoadResource
// (g_soundtrackdataman: src/platform/unported_globals.cpp)
// 8032862C global destructors keyed to ESoundTrackDataManager::AllocateAndLoadResource
