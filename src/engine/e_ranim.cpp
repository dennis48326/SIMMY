// Port of engine/e_ranim.cpp and engine/e_animman.cpp (loading). Playback is SAnimator2 / EAnimController.
//
// ANIM data, all big-endian: u32 frames, float[3], float[3], nodes (u32 count, 8 words each), u32,
// constants (u32 count, floats), flags (u32 bit count, then the words), float fps, float, u32, u8,
// u32 x4, u8, u8, notes (u32 count, {u32, u32, string}), events (u32 count, {char[0x40], u32 x4}).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

int ERAnim::m_allocatedAnims = 0;

// 8036808C ERAnim::GetTypeInfo
// 80368098 ERAnim::GetTypeName
// 803680A4 ERAnim::GetTypeKey
// 803680B0 ERAnim::GetTypeVersion
// 8036804C ERAnim::SafeDelete
// 80367FD4 ERAnim::New
// 80367FFC ERAnim::Construct
// 80368028 ERAnim::Destruct
// 803680BC ERAnim::GetTypeInfoStatic
// 803680C8 ERAnim::GetReadVersion
SIMMY_STORABLE_IMPL(ERAnim, "ERAnim", 0, EResource)

// 803673AC
ERAnim::ERAnim() { ++m_allocatedAnims; }
// 803674EC
ERAnim::~ERAnim() { --m_allocatedAnims; }

namespace {
struct AnimBytes {
  const unsigned char* p;
  unsigned int u32() { const unsigned int v = simmy::be32(p); p += 4; return v; }
  float f32() { const float v = simmy::befloat(p); p += 4; return v; }
  unsigned char u8() { return *p++; }
};
}  // namespace

// 803675DC / 80367738 / 803677CC / 803678A8: CopyERAnimNodeArray, ...ConstantArray, ...NoteArray,
// ...EventArray, folded into the reader below.

// 803679AC
void ERAnim::Load(EFile* file) {
  EDataHeader header;
  if (!header.Read(*file, 0x414E494D /* 'ANIM' */, 0x11, 0x12)) return;  // (OldLoad accepted no other version)
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  std::vector<unsigned char> data(header.m_dataSize);
  file->Read(data.data(), header.m_dataSize);
  AnimBytes r{data.data()};
  m_frameCount = static_cast<int>(r.u32());
  for (float& f : m_field1C) f = r.f32();
  for (float& f : m_field28) f = r.f32();
  m_nodes.resize(r.u32());
  for (EAnimNodeDataPos& n : m_nodes)
    for (unsigned int& w : n.m_data) w = r.u32();
  m_field40 = r.u32();
  m_constants.resize(r.u32());
  for (float& f : m_constants) f = r.f32();
  m_bitArray.m_size = static_cast<int>(r.u32());
  m_bitArray.m_words.resize((static_cast<unsigned int>(m_bitArray.m_size) + 31) / 32);
  for (unsigned int& w : m_bitArray.m_words) w = r.u32();
  m_fps = r.f32();
  m_field68 = r.f32();
  m_field6C = r.u32();
  m_field72 = r.u8();
  m_field74 = r.u32();
  m_field78 = r.u32();
  m_field7C = r.f32();
  m_field80 = r.f32();
  m_field70 = r.u8();
  m_field71 = r.u8();
  m_notes.resize(r.u32());
  for (EAnimNote& n : m_notes) {
    n.m_time = r.u32();
    n.m_field04 = r.u32();
    n.m_text = reinterpret_cast<const char*>(r.p);
    r.p += n.m_text.size() + 1;
  }
  m_events.resize(r.u32());
  for (EAnimEvent& e : m_events) {
    std::memcpy(e.m_name, r.p, sizeof e.m_name);
    r.p += sizeof e.m_name;
    for (unsigned int& w : e.m_data) w = r.u32();
  }
  if (r.p != data.data() + data.size())
    std::fprintf(stderr, "[anim] %08X: parsed %td of %zu bytes\n", m_id, r.p - data.data(), data.size());
  // Length in ms, at least up to the last note.
  m_durationMs = static_cast<int>(static_cast<float>(m_frameCount) / m_fps * 1000.0f);
  if (!m_notes.empty() && m_durationMs < static_cast<int>(m_notes.back().m_time))
    m_durationMs = static_cast<int>(m_notes.back().m_time);
}

// 80367F3C
void ERAnim::Refresh(EFile* file) { if (file) Load(file); }

// 80322D24
EAHeap* EAnimManager::GetHeap() { return AnimationHeap(); }

// 80322D44
EResource* EAnimManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* anim = new ERAnim;
  anim->m_id = id;
  anim->Load(file);
  return anim;
}

// 80322E08
EResource* EAnimManager::AddRef(unsigned int id, EFile* file, int size) { return EResourceManager::AddRef(id, file, size); }
// 80322E28
EResource* EAnimManager::AddRef(char* name, EFile* file, int size) { return EResourceManager::AddRef(name, file, size); }
// 80322E48
int EAnimManager::CanCache(EResource*) { return 1; }
