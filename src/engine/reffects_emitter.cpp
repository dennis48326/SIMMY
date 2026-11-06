// Port of engine/reffects_emitter.cpp (REffectsEmitter, the 'EEMT' resource) and
// engine/effects_emitterman.cpp (its manager, g_effectsEmitterMan: effectse.arc).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace {
constexpr unsigned int kBirthSize = 0x90;
constexpr unsigned int kBlockSize = kBirthSize + sizeof(Effects__FastParticleEmitterInitData);  // 0x180
static_assert(sizeof(Effects__FastParticleParticleInitData) == 0x90, "birth data layout");
static_assert(sizeof(Effects__FastParticleEmitterInitData) == 0xf0, "emitter data layout");
static_assert(offsetof(Effects__FastParticleEmitterInitData, m_maxBursts) == 0x44, "layout");
static_assert(offsetof(Effects__FastParticleEmitterInitData, m_flags) == 0x48, "layout");
static_assert(offsetof(Effects__FastParticleEmitterInitData, m_burstMin) == 0x64, "layout");
static_assert(offsetof(Effects__FastParticleEmitterInitData, m_fieldA8) == 0xa8, "layout");
static_assert(offsetof(Effects__FastParticleEmitterInitData, m_frameTime) == 0xb0, "layout");
static_assert(offsetof(Effects__FastParticleEmitterInitData, m_textureId) == 0xc0, "layout");

void Swap32(unsigned char* p) {
  std::swap(p[0], p[3]);
  std::swap(p[1], p[2]);
}

// PC: the block as host values (GC: used as stored). The birth data is all floats; the description
// is words except for four shorts and the bytes at 0xA8..0xAF; its tool tail (0xC4..) is left.
void SwapBlock(unsigned char* b) {
  for (unsigned int off = 0; off < kBirthSize; off += 4) Swap32(b + off);
  unsigned char* e = b + kBirthSize;
  for (unsigned int off = 0; off < 0xc4; off += 4) {
    if (off == 0x44 || off == 0x64) {
      std::swap(e[off], e[off + 1]);
      std::swap(e[off + 2], e[off + 3]);
    } else if (off == 0xa8 || off == 0xac) {
      // bytes
    } else {
      Swap32(e + off);
    }
  }
}
}  // namespace

// 8036A7B4
REffectsEmitter::REffectsEmitter() {}

// 8036A804
REffectsEmitter::~REffectsEmitter() { Deallocate(); }

// 8036A860
void REffectsEmitter::Deallocate() {
  DelRefSubResources();
  if (m_birth) {
    delete[] reinterpret_cast<unsigned char*>(m_birth);
    m_birth = nullptr;
    m_init = nullptr;
  }
}

// 8036A8C4
void REffectsEmitter::DelRefSubResources() {
  if (m_texture) {
    m_texture->DelRef();
    m_texture = nullptr;
  }
}

// 8036A904
// The description's texture.
void REffectsEmitter::AddRefSubResources() {
  m_texture = nullptr;
  if (m_init && m_init->m_textureId)
    m_texture = static_cast<ERTexture*>(_textureman.AddRef(m_init->m_textureId, nullptr, 0));
}

// 8036A964
int REffectsEmitter::TryIncrementSubResources() {
  if (m_init && m_init->m_textureId) {
    EResource* tex = nullptr;
    if (!_textureman.TryIncrementResource(m_init->m_textureId, &tex)) return 0;
    m_texture = static_cast<ERTexture*>(tex);
  }
  return 1;
}

// 8036A9D0
// The 'EEMT' header (version 1), then the block: birth data at 0, the description at 0x90.
void REffectsEmitter::Load(EFile* file) {
  Deallocate();
  if (!file) return;
  EDataHeader header;
  if (!header.Read(*file, 0x45454d54 /* 'EEMT' */, 1, 1)) return;
  const unsigned int size = header.m_dataSize;
  m_flags = size << 8 | (m_flags & 0xff);
  // PC: at least a whole block (zero-filled). GC: 64-byte aligned from the main heap.
  const unsigned int alloc = size < kBlockSize ? kBlockSize : size;
  auto* block = new unsigned char[alloc];
  std::memset(block, 0, alloc);
  m_birth = reinterpret_cast<BirthData*>(block);
  m_init = reinterpret_cast<InitData*>(block + kBirthSize);
  file->Read(block, size);
  SwapBlock(block);
  AddRefSubResources();
}

// 8036AAC4
void REffectsEmitter::Refresh(EFile* file) {
  if (!file) return;
  Deallocate();
  Load(file);
  m_refreshed = 1;
}

// 8036AB7C REffectsEmitter::New
// 8036ABAC REffectsEmitter::Construct
// 8036ABCC REffectsEmitter::Destruct
// 8036ABF0 REffectsEmitter::SafeDelete
// 8036AC30 REffectsEmitter::GetTypeInfo
// 8036AC3C REffectsEmitter::GetTypeName
// 8036AC48 REffectsEmitter::GetTypeKey
// 8036AC54 REffectsEmitter::GetTypeVersion
// 8036AC60 REffectsEmitter::GetTypeInfoStatic
// 8036AC6C REffectsEmitter::GetReadVersion
SIMMY_STORABLE_IMPL(REffectsEmitter, "REffectsEmitter", 1, EResource)
// 8036AD78 REffectsEmitter::GetEmitterInitData (inline in the header)
// 8036AD80 REffectsEmitter::GetParticleBirthData
// 8036AD88 REffectsEmitter::GetTexture
// 8036AD90 REffectsEmitter::Refreshed
// 8036AD98 REffectsEmitter::RefreshClear

// 80328958
EResource* EffectsEmitterManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* r = new REffectsEmitter;
  r->m_id = id;
  r->Load(file);
  return r;
}
// 80328A18
EResource* EffectsEmitterManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 80328A38
EResource* EffectsEmitterManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
// 80328A58 global constructors keyed to EffectsEmitterManager::AllocateAndLoadResource (g_effectsEmitterMan)
// 80328A84 global destructors keyed to EffectsEmitterManager::AllocateAndLoadResource
