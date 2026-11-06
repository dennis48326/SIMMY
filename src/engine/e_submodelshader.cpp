// Port of engine/e_submodelshader.cpp (partial): one shader of a sub-model and its triangle strips.
// Read copies each strip's vertex arrays and (for indexed strips) its GX display list as stored, big-
// endian; BuildDisplayList records the strips into a deferred EDL that drawing calls. Morphing (morph
// targets and lattices) works on a second set of strips with their own positions (m_morphStrips):
// the morphs add their offsets up there, ApplyMorph adds the base positions and records m_dl from
// them. Positions stay big-endian (GX reads them), so they are read and written through Load/StorePos.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

using simmy::be16;
using simmy::be32;
using simmy::befloat;

int ESubModelShader::m_vertsLoaded = 0;  // 805003B0

namespace {
const char kFile[] = "c:/BuildAgent/cm3-build22-NGC/CMBuild/src/common/engine/graphics/e_submodelshader.cpp";
}

// 802FBA24 VertexDataInitialization and 802FBA28 VertexDataDeinitialization are empty.

// 802FBA2C
// Vertex arrays and display lists live in the graphics heap (32-byte aligned for GX).
void* ESubModelShader::VertexDataAllocation(int size, const char*, int, const char*) {
  return _pGfx->AllocVertexData(static_cast<unsigned int>(size));
}

// 802FBA64
void ESubModelShader::VertexDataDeallocation(void* data, bool free) {
  if (free) _pGfx->FreeVertexData(data);
}

// 802FBAF4
ESubModelShader::ESubModelShader() {}

// 802FBB70
ESubModelShader::~ESubModelShader() { Deallocate(); }

// 802FBBC8
void ESubModelShader::DelRefSubResources() {
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
}

// 802FBC08
void ESubModelShader::AddRefSubResources() {
  if (m_shaderId) m_shader = static_cast<ERShader*>(_shaderman.AddRef(m_shaderId, nullptr, 0));
}

// 802FBC54
int ESubModelShader::TryIncrementSubResources() {
  if (m_shaderId) {
    EResource* res = nullptr;
    if (!_shaderman.TryIncrementResource(m_shaderId, &res)) return 0;
    m_shader = static_cast<ERShader*>(res);
  }
  return 1;
}

// 802FBCA0
// The original returned only the positions (and the indices of non-indexed strips) to the heap; the
// display list, uvs, normals, colours and weights were passed with free = false. On PC every array is
// a separate allocation and all are freed (the indices of indexed strips point into the display list).
void ESubModelShader::DeallocateStripData(ESMSStrip* strip) {
  if (strip->m_gxDL) strip->m_indices = nullptr;
  VertexDataDeallocation(strip->m_gxDL, true);
  VertexDataDeallocation(strip->m_uvs, true);
  VertexDataDeallocation(strip->m_normals, true);
  VertexDataDeallocation(strip->m_colors, true);
  VertexDataDeallocation(strip->m_weights, true);
  VertexDataDeallocation(strip->m_indices, true);
  VertexDataDeallocation(strip->m_positions, true);
  strip->m_weights = nullptr;
  strip->m_gxDL = nullptr;
  strip->m_indices = nullptr;
  strip->m_positions = nullptr;
  strip->m_uvs = nullptr;
  strip->m_normals = nullptr;
  strip->m_colors = nullptr;
}

// 802FBD70
void ESubModelShader::Deallocate() {
  if (m_dl) {
    if (_pGfx->NeedFlushToDestroy(m_dl)) _pGfx->Flush();
    _pGfx->Destroy(m_dl);
    m_dl = nullptr;
  }
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
  FreeStripResource();
  FreeMorphStripResource();
  FreeMorphResource();
}

// 802FBE3C
void ESubModelShader::FreeStripResource() {
  for (ESMSStrip& s : m_strips) DeallocateStripData(&s);
  m_strips.clear();
  m_strips.shrink_to_fit();
}

// 802FBEAC
// Morph strips share their uv, normal, colour and weight arrays with the base strips. PC: also the
// GX display list (the GC did not free display lists here; the port frees them with the strip).
void ESubModelShader::FreeMorphStripResource() {
  for (ESMSStrip& s : m_morphStrips) {
    s.m_gxDL = nullptr;
    s.m_colors = nullptr;
    s.m_indices = nullptr;
    s.m_uvs = nullptr;
    s.m_weights = nullptr;
    s.m_normals = nullptr;
    DeallocateStripData(&s);
  }
  m_morphStrips.clear();
  m_morphStrips.shrink_to_fit();
  for (ESubModelShader*& t : m_targets) t = nullptr;
}

// 802FBF48
// The lattice list and its per-vertex parameters.
void ESubModelShader::FreeMorphResource() { m_latticeList.clear(); }

// 802FBFEC
// Records one strip. Indexed strips (flag 0x20) are GX display lists over the vertex arrays; the
// others are plain strips in vertex order. Returns the triangles drawn.
int ESubModelShader::CreateRCPrimitive(unsigned int count, ERC* rc, ESMSStrip* s, bool, bool, bool) {
  if (count < 3) return 0;
  const bool twoUVs = (m_flags & kTwoUVs) != 0;
  auto* ngc = static_cast<ENgcRC*>(rc);  // the strip commands are ENgcRC's own (GC: called by slot)
  if (m_flags & kIndexed) {
    if (m_flags & kFixedPoint)
      ngc->TriStripDLRef(s->m_gxDL, s->m_gxDLSize, s->m_vertCount, static_cast<short*>(s->m_positions),
                         static_cast<short*>(s->m_uvs), s->m_colors, s->m_normals, twoUVs);
    else
      ngc->TriStripDLRef(s->m_gxDL, s->m_gxDLSize, s->m_vertCount, static_cast<float*>(s->m_positions),
                         static_cast<float*>(s->m_uvs), s->m_colors, s->m_normals, twoUVs);
  } else {
    if (m_flags & kFixedPoint)
      ngc->TriStripRef(s->m_vertCount, static_cast<short*>(s->m_positions), static_cast<short*>(s->m_uvs),
                       s->m_colors, s->m_normals, s->m_weights, twoUVs);
    else  // float plain strips (ENgcRC::TriStripRef(int, float*, ...)) are not ported
      SIMMY_STUB("ESubModelShader::CreateRCPrimitive: float non-indexed strips", 0x802FBFEC);
  }
  return s->m_vertCount - 2;
}

// 802FC110
// Fixed point: s16 x, y, z, pad per vertex; otherwise f32 x, y, z, w.
int ESubModelShader::ReadPositions(const unsigned char* src, ESMSStrip* s, bool) {
  const int size = s->m_vertCount * ((m_flags & kFixedPoint) ? 8 : 16);
  s->m_positions = VertexDataAllocation(size, kFile, (m_flags & kFixedPoint) ? 0x68a : 0x6b1, "Vertex Positions");
  std::memcpy(s->m_positions, src, static_cast<std::size_t>(size));
  return size;
}

// 802FC24C
int ESubModelShader::ReadTexcoords(const unsigned char* src, ESMSStrip* s) {
  if (!(m_flags & kUVs)) return 0;
  const int components = (m_flags & kTwoUVs) ? 4 : 2;
  const int bytes = (m_flags & kFixedPoint) ? 2 : 4;
  const int size = s->m_vertCount * components * bytes;
  s->m_uvs = VertexDataAllocation(size, kFile, 0x6e5, "Vertex Texcoords");
  std::memcpy(s->m_uvs, src, static_cast<std::size_t>(size));
  return size;
}

// 802FC2F8
// RGBA8; the alpha byte is forced to 255.
int ESubModelShader::ReadColors(const unsigned char* src, ESMSStrip* s) {
  if (!(m_flags & kColors)) return 0;
  const int size = s->m_vertCount * 4;
  s->m_colors = static_cast<unsigned char*>(VertexDataAllocation(size, kFile, 0x6fa, "Vertex Colors"));
  std::memcpy(s->m_colors, src, static_cast<std::size_t>(size));
  for (int i = 0; i < s->m_vertCount; ++i) s->m_colors[i * 4 + 3] = 0xff;
  return size;
}

// 802FC3B8
// Before version 0x3A normals were three bytes; they are padded to four.
int ESubModelShader::ReadNormalsOld(const unsigned char* src, ESMSStrip* s) {
  if (!(m_flags & kNormals)) return 0;
  s->m_normals = static_cast<signed char*>(VertexDataAllocation(s->m_vertCount * 4, kFile, 0x725, "Vertex Normals"));
  for (int i = 0; i < s->m_vertCount; ++i) {
    std::memcpy(s->m_normals + i * 4, src + i * 3, 3);
    s->m_normals[i * 4 + 3] = 0x7f;
  }
  return s->m_vertCount * 3;
}

// 802FC4B0
int ESubModelShader::ReadNormals(const unsigned char* src, ESMSStrip* s) {
  if (!(m_flags & kNormals)) return 0;
  const int size = s->m_vertCount * 4;
  s->m_normals = static_cast<signed char*>(VertexDataAllocation(size, kFile, 0x74b, "Vertex Normals"));
  std::memcpy(s->m_normals, src, static_cast<std::size_t>(size));
  return size;
}

// 802FC564
// Weighted (skinned) strips: four weights per vertex. If the file named no matrix slots, every slot
// whose weight is non-zero somewhere is used, in order.
int ESubModelShader::ReadWeights(const unsigned char* src, ESMSStrip* s, bool weighted) {
  if (!weighted) return 0;
  const int size = s->m_vertCount * 4;
  s->m_weights = static_cast<unsigned char*>(VertexDataAllocation(size, kFile, 0x777, "Vertex Weights"));
  std::memcpy(s->m_weights, src, static_cast<std::size_t>(size));
  if (s->m_numMatIndices == 0) {
    unsigned int used[4] = {};
    for (int i = 0; i < s->m_vertCount; ++i)
      for (int k = 0; k < 4; ++k) used[k] = (used[k] | (s->m_weights[i * 4 + k] != 0)) != 0;
    for (int k = 0; k < 4; ++k)
      if (used[k]) s->m_matIndex[s->m_numMatIndices++] = static_cast<unsigned char>(k);
  }
  return size;
}

// 802FCDA4
// Indexed strips: the index count, a byte, the display list size and the offset of the first index in
// it, then the GX display list (GX_TRIANGLESTRIP of index16 vertices, big-endian as GX reads it).
// Returns the vertices the strip draws.
unsigned int ESubModelShader::ReadIndices(const unsigned char* src, ESMSStrip* s, unsigned int& bytes) {
  unsigned int count = static_cast<unsigned int>(s->m_vertCount);
  if (!(m_flags & kIndexed)) {
    s->m_gxDLSize = 0;
    bytes = 0;
    return count;
  }
  count = be32(src);
  const unsigned int dlSize = be32(src + 5), offset = be32(src + 9);
  s->m_gxDLSize = static_cast<int>(dlSize);
  s->m_gxDL = static_cast<unsigned char*>(_pGfx->AllocVertexData(dlSize));
  std::memcpy(s->m_gxDL, src + 13, dlSize);
  s->m_indices = s->m_gxDL + offset;
  // GC: OptimizeStripsForMemory then merged duplicate fixed-point positions, uvs and normals and
  // rewrote the indices. It only saves memory (the drawn result is the same), so it is skipped and
  // the arrays keep one entry per stored vertex.
  bytes = 13 + dlSize;
  return count;
}

// 802FCED4
// The vertex format, the shader id, the strip count (and a byte per strip), the mip-map bias, then
// tagged records until tag 6: 0 a strip, 1 a skinning matrix slot (pad, matrix, slot), 2/4 weighted
// strips follow (2 also marks them skinned), 3/5 unweighted strips follow.
int ESubModelShader::Read(const unsigned char* src, int version) {
  Deallocate();
  m_flags = be32(src);
  const unsigned int shaderId = be32(src + 4);
  m_shader = static_cast<ERShader*>(_shaderman.AddRef(shaderId, nullptr, 0));
  m_shaderId = shaderId;
  const unsigned int stripCount = be32(src + 8);
  const unsigned char* p = src + 12;
  m_strips.assign(stripCount, ESMSStrip{});
  p += stripCount;  // one unused byte per strip
  const unsigned int mip = be32(p);
  std::memcpy(&m_mipMap, &mip, 4);
  p += 4;

  bool done = false;
  int numMatrixRecords = 0, numSkinRecords = 0;
  bool weighted = false;
  unsigned char skinned = 0;
  unsigned int stripsRead = 0;
  ESMSStrip* strip = stripCount ? &m_strips[0] : nullptr;
  if (strip) strip->m_skinned = 0;
  while (!done) {
    const unsigned char tag = *p++;
    switch (tag) {
    case 0: {  // a strip
      strip->m_vertCount = static_cast<int>(be32(p));
      p += 4;
      p += ReadPositions(p, strip, weighted);
      p += ReadTexcoords(p, strip);
      p += ReadColors(p, strip);
      p += version < 0x3a ? ReadNormalsOld(p, strip) : ReadNormals(p, strip);
      p += ReadWeights(p, strip, weighted);
      ++stripsRead;
      unsigned int bytes = 0;
      strip->m_stripVerts = static_cast<unsigned short>(ReadIndices(p, strip, bytes));
      p += bytes;
      strip = stripsRead < stripCount ? &m_strips[stripsRead] : nullptr;
      if (strip) strip->m_skinned = skinned;
      break;
    }
    case 1: {  // matrix `p[1]` in skinning slot `p[2]`
      ++numMatrixRecords;
      strip->m_matIndex[p[2]] = p[1];
      strip->m_hasMatIndices = 1;
      ++strip->m_numMatIndices;
      p += 3;
      break;
    }
    case 2:
      ++numSkinRecords;
      skinned = 1;
      if (strip) strip->m_skinned = 1;
      weighted = true;
      break;
    case 3:
      ++numSkinRecords;
      skinned = 0;
      if (strip) strip->m_skinned = 0;
      weighted = false;
      break;
    case 4: weighted = true; break;
    case 5: weighted = false; break;
    case 6: done = true; break;
    default: break;
    }
  }
  if (stripCount == 1 && numMatrixRecords == 0 && numSkinRecords == 0) m_flags |= kSingleStrip;
  for (const ESMSStrip& s : m_strips) m_vertsLoaded += s.m_vertCount;
  return static_cast<int>(p - src);
}

// 802FD24C
void ESubModelShader::DrawNormals(ERC*) {}
// 802FD250
void ESubModelShader::DrawWireFrame(ERC*) {}

namespace {
// Vertex positions as GX reads them: fixed point s16 x, y, z, pad (4.12) or f32 x, y, z, w, big-endian.
void Put16(unsigned char* p, short v) {
  p[0] = static_cast<unsigned char>(static_cast<unsigned short>(v) >> 8);
  p[1] = static_cast<unsigned char>(v);
}
void PutFloat(unsigned char* p, float f) {
  unsigned int u;
  std::memcpy(&u, &f, 4);
  p[0] = static_cast<unsigned char>(u >> 24), p[1] = static_cast<unsigned char>(u >> 16);
  p[2] = static_cast<unsigned char>(u >> 8), p[3] = static_cast<unsigned char>(u);
}
EVec3 LoadPos(const ESMSStrip& s, int v, bool fixed) {
  const auto* p = static_cast<const unsigned char*>(s.m_positions);
  if (fixed) {
    p += v * 8;
    return EVec3(static_cast<float>(static_cast<short>(be16(p))) * (1.0f / 4096.0f),
                 static_cast<float>(static_cast<short>(be16(p + 2))) * (1.0f / 4096.0f),
                 static_cast<float>(static_cast<short>(be16(p + 4))) * (1.0f / 4096.0f));
  }
  p += v * 16;
  return EVec3(befloat(p), befloat(p + 4), befloat(p + 8));
}
void StorePos(ESMSStrip& s, int v, const EVec3& pos, bool fixed) {
  auto* p = static_cast<unsigned char*>(s.m_positions);
  if (fixed) {
    p += v * 8;
    for (int c = 0; c < 3; ++c) Put16(p + c * 2, static_cast<short>(static_cast<int>(pos[c] * 4096.0f)));
  } else {
    p += v * 16;
    for (int c = 0; c < 3; ++c) PutFloat(p + c * 4, pos[c]);
  }
}
// The fourth component (pad or w) of `from`'s vertex into `to`'s.
void CopyW(ESMSStrip& to, const ESMSStrip& from, int v, bool fixed) {
  const int size = fixed ? 2 : 4, off = fixed ? v * 8 + 6 : v * 16 + 12;
  std::memcpy(static_cast<unsigned char*>(to.m_positions) + off, static_cast<const unsigned char*>(from.m_positions) + off,
              static_cast<std::size_t>(size));
}
// The lattice a vertex colour's red channel names: red * 2 is the low byte of the lattice's id (GC:
// the byte at BSplineVolume +0x83, the big-endian id's last).
bool LatticeMatches(unsigned char red, const BSplineVolume* volume) {
  return red != 0 && static_cast<unsigned int>(red) * 2 == (volume->m_id & 0xff);
}
}  // namespace

// 802FD254
void ESubModelShader::MorphBreakUpStrip(TArray<ESMSStrip, TArrayERModelAllocator>&, TArray<ESMSStrip, TArrayERModelAllocator>&) {}

// 802FD258
// The morph strips: the base strips with positions of their own.
void ESubModelShader::AllocateMorph() {
  if (m_strips.empty()) return;
  m_morphStrips = m_strips;
  for (ESMSStrip& s : m_morphStrips) {
    const bool fixed = (m_flags & kFixedPoint) != 0;
    s.m_positions = VertexDataAllocation(s.m_vertCount * (fixed ? 8 : 16), kFile, fixed ? 0xb2e : 0xb33, "Vertex Positions");
  }
}

// 802FD330
// A target of the same shape (strip for strip, vertex for vertex) into `slot`.
int ESubModelShader::RegisterMorphTarget(ESubModelShader* target, int slot) {
  if (m_strips.size() != target->m_strips.size()) return 0;
  for (std::size_t i = 0; i < m_strips.size(); ++i)
    if (m_strips[i].m_vertCount != target->m_strips[i].m_vertCount) return 0;
  if (m_morphStrips.empty()) {
    AllocateMorph();
    ResetMorph();
  }
  m_targets[slot] = target;
  return 1;
}

// 802FD3E8
void ESubModelShader::UnRegisterMorphTarget(int slot) { m_targets[slot] = nullptr; }

// 802FD3FC
// No offsets yet.
void ESubModelShader::ResetMorph() {
  for (ESMSStrip& s : m_morphStrips)
    std::memset(s.m_positions, 0, static_cast<std::size_t>(s.m_vertCount) * ((m_flags & kFixedPoint) ? 8 : 16));
}

// 802FD484
// The morph strips' positions become base + offsets, and the display list is recorded from them.
// GC: the old list was destroyed at once; PC flushes first if the GPU may still read it.
int ESubModelShader::ApplyMorph() {
  const bool fixed = (m_flags & kFixedPoint) != 0;
  for (std::size_t i = 0; i < m_morphStrips.size(); ++i) {
    ESMSStrip& m = m_morphStrips[i];
    const ESMSStrip& b = m_strips[i];
    for (int v = 0; v < m.m_vertCount; ++v) {
      const EVec3 bp = LoadPos(b, v, fixed), mp = LoadPos(m, v, fixed);
      StorePos(m, v, EVec3(bp.x + mp.x, bp.y + mp.y, bp.z + mp.z), fixed);
      CopyW(m, b, v, fixed);
    }
  }
  if (m_morphStrips.empty()) return 0;
  if (m_dl) {
    if (_pGfx->NeedFlushToDestroy(m_dl)) _pGfx->Flush();
    _pGfx->Destroy(m_dl);
    m_dl = nullptr;
  }
  ERC* rc = _pGfx->Open(RCMode::Deferred);
  rc->SetName(const_cast<char*>("ESubModelShader::ApplyMorph"));
  for (ESMSStrip& s : m_morphStrips) {
    if (s.m_hasMatIndices)
      for (unsigned int i = 0; i < s.m_numMatIndices; ++i) rc->ModelMatrixIndex(static_cast<int>(i), s.m_matIndex[i]);
    CreateRCPrimitive(s.m_stripVerts, rc, &s, true, false, true);
  }
  m_dl = _pGfx->Close(rc);
  return 1;
}

// 802FD960
int ESubModelShader::GetMorphedVertex(EVec3& out, short strip, short vertex) {
  if (m_morphStrips.empty() || strip >= static_cast<short>(m_morphStrips.size())) return 0;
  const ESMSStrip& s = m_morphStrips[strip];
  if (vertex >= static_cast<short>(s.m_vertCount)) return 0;
  out = LoadPos(s, vertex, (m_flags & kFixedPoint) != 0);
  return 1;
}

// 802FDAAC
// A morph target's positions become its offsets from `base` (once).
void ESubModelShader::GenerateMorphTargetDeltas(ESubModelShader* base) {
  if (!base || base->m_strips.size() != m_strips.size() || m_deltasMade) return;
  const bool fixed = (m_flags & kFixedPoint) != 0;
  for (std::size_t i = 0; i < m_strips.size(); ++i) {
    ESMSStrip& t = m_strips[i];
    for (int v = 0; v < t.m_vertCount; ++v) {
      const EVec3 tp = LoadPos(t, v, fixed), bp = LoadPos(base->m_strips[i], v, fixed);
      StorePos(t, v, EVec3(tp.x - bp.x, tp.y - bp.y, tp.z - bp.z), fixed);
    }
  }
  m_deltasMade = 1;
}

// 802FDDC4
// Adds every registered target's offsets times its weight to the morph strips.
int ESubModelShader::Morph(float* weights) {
  const bool fixed = (m_flags & kFixedPoint) != 0;
  for (std::size_t i = 0; i < m_morphStrips.size(); ++i) {
    ESMSStrip& m = m_morphStrips[i];
    for (int v = 0; v < m.m_vertCount; ++v) {
      EVec3 acc = LoadPos(m, v, fixed);
      for (int slot = 0; slot < 12; ++slot) {
        const float w = weights[slot];
        if (w != 0.0f && m_targets[slot]) {
          const EVec3 d = LoadPos(m_targets[slot]->m_strips[i], v, fixed);
          acc.x = acc.x + d.x * w;
          acc.y = acc.y + d.y * w;
          acc.z = acc.z + d.z * w;
        }
      }
      StorePos(m, v, acc, fixed);
    }
  }
  return 1;
}

// 802FE1A4
// The lattices that some vertex of this shader names (in its colour). 0 if none does.
int ESubModelShader::CreateLatticeList(std::vector<BSplineVolume*>* lattices) {
  if (lattices->empty()) return 0;
  m_latticeList.clear();
  for (BSplineVolume* volume : *lattices) {
    bool found = false;
    for (const ESMSStrip& s : m_strips) {
      if (!s.m_colors) continue;
      for (int v = 0; v < s.m_vertCount && !found; ++v) found = LatticeMatches(s.m_colors[v * 4], volume);
      if (found) {
        ELatticeVerts entry;
        entry.m_volume = volume;
        m_latticeList.push_back(std::move(entry));
        break;
      }
    }
  }
  return m_latticeList.empty() ? 0 : 1;
}

// 802FE400
// Room for the parameters of each lattice's vertices (counted, then the count restarts at 0).
void ESubModelShader::AllocateParameterizeVerts() {
  for (const ESMSStrip& s : m_strips) {
    if (!s.m_colors) continue;
    for (int v = 0; v < s.m_vertCount; ++v) {
      if (!s.m_colors[v * 4]) continue;
      for (ELatticeVerts& e : m_latticeList)
        if (LatticeMatches(s.m_colors[v * 4], e.m_volume)) {
          ++e.m_count;
          break;
        }
    }
  }
  for (ELatticeVerts& e : m_latticeList) {
    e.m_params.clear();
    if (e.m_count) {
      e.m_params.assign(static_cast<std::size_t>(e.m_count), ELatticeVertParam{});
      e.m_count = 0;
    }
  }
}

// 802FE5CC
// Each vertex's place in the lattice its colour names; the shader gets morph strips, which draw
// without the colours (they only named the lattices).
void ESubModelShader::ParameterizeVerts(std::vector<BSplineVolume*>* lattices) {
  if (!CreateLatticeList(lattices)) return;
  AllocateParameterizeVerts();
  const bool fixed = (m_flags & kFixedPoint) != 0;
  for (std::size_t i = 0; i < m_strips.size(); ++i) {
    const ESMSStrip& s = m_strips[i];
    if (!s.m_colors) continue;
    for (int v = 0; v < s.m_vertCount; ++v) {
      if (!s.m_colors[v * 4]) continue;
      for (ELatticeVerts& e : m_latticeList) {
        if (!LatticeMatches(s.m_colors[v * 4], e.m_volume)) continue;
        ELatticeVertParam& prm = e.m_params[static_cast<std::size_t>(e.m_count)];
        prm.m_vertex = static_cast<unsigned int>(i) << 16 | static_cast<unsigned int>(v);
        prm.m_params = e.m_volume->ParameterizeVert(LoadPos(s, v, fixed), prm.m_cell);
        ++e.m_count;
        break;
      }
    }
  }
  if (m_morphStrips.empty()) {
    AllocateMorph();
    ResetMorph();
  }
  for (ESMSStrip& s : m_morphStrips) s.m_colors = nullptr;
}

// 802FE910
// Adds each lattice vertex's move (the blended lattice at its parameters minus where it is).
void ESubModelShader::LatticeDeform() {
  const bool fixed = (m_flags & kFixedPoint) != 0;
  for (ELatticeVerts& e : m_latticeList) {
    for (int k = 0; k < e.m_count; ++k) {
      const ELatticeVertParam& prm = e.m_params[static_cast<std::size_t>(k)];
      const EVec3 p = e.m_volume->GetPosition(&prm.m_params, prm.m_cell);
      const int strip = static_cast<int>(prm.m_vertex >> 16), v = static_cast<int>(prm.m_vertex & 0xffff);
      const EVec3 bp = LoadPos(m_strips[strip], v, fixed);
      const EVec3 mp = LoadPos(m_morphStrips[strip], v, fixed);
      StorePos(m_morphStrips[strip], v, EVec3(mp.x + (p.x - bp.x), mp.y + (p.y - bp.y), mp.z + (p.z - bp.z)), fixed);
    }
  }
}

// Wall models fade with distance along x: each vertex gets weights (w, w, 255 - w, 255 - w) with w
// its x position mapped from [minX, maxX] to 0..255.
void ESubModelShader::MakeWallWeights(ESMSStrip& s, float minX, float maxX) {
  auto* w = static_cast<unsigned char*>(VertexDataAllocation(s.m_vertCount * 4, kFile, 0xdb2, "Vertex Weights"));
  for (int i = 0; i < s.m_vertCount; ++i) {
    const auto x = static_cast<short>(be16(static_cast<unsigned char*>(s.m_positions) + i * 8));
    float t = (static_cast<float>(x) - minX) * (1.0f / (maxX - minX));
    if (!(t > 0.0f)) t = 0.0f;
    else if (t >= 1.0f) t = 1.0f;
    const auto c = static_cast<unsigned char>(static_cast<int>(t * 255.0f));
    w[i * 4 + 0] = w[i * 4 + 1] = c;
    w[i * 4 + 2] = w[i * 4 + 3] = static_cast<unsigned char>(0xff - c);
  }
  VertexDataDeallocation(s.m_weights, true);
  s.m_weights = w;
}

namespace {
// What BuildDisplayList checks of the shader: layer 0's texture-coordinate mode other than plain uv0,
// uv1 or reflection, or either layer's modifiable-colour flag, needs the per-vertex colour modulation.
bool NeedsColorModulation(const EShader* sh) {
  if (!sh) return false;
  const unsigned char texGen = sh->m_layers[0].m_texGen;
  const bool modulated = texGen != 0 && texGen != 2 && texGen != 3;
  return modulated || ((sh->m_layers[0].m_flags | sh->m_layers[1].m_flags) & 0x20) != 0;
}
}  // namespace

// 802FED28
// Records every strip into a new deferred display list (m_dl), with the skinning matrix slots of the
// strips that have them.
void ESubModelShader::BuildDisplayList(ESMBuildDisplayListData* data) {
  const float minX = data->m_minX, maxX = data->m_maxX;
  const bool wall = data->m_model && data->m_model->m_isWall;
  if (m_dl) {
    _pGfx->Destroy(m_dl);
    m_dl = nullptr;
  }
  ERC* rc = _pGfx->Open(RCMode::Deferred);
  rc->SetName(data->m_name);
  EShader* sh = GetEShader();
  rc->EnableColorModulation(NeedsColorModulation(sh));
  for (ESMSStrip& s : m_strips) {
    if (s.m_hasMatIndices)
      for (unsigned int i = 0; i < s.m_numMatIndices; ++i) rc->ModelMatrixIndex(static_cast<int>(i), s.m_matIndex[i]);
    if (wall) MakeWallWeights(s, minX, maxX);
    CreateRCPrimitive(s.m_stripVerts, rc, &s, true, wall, false);
  }
  rc->EnableColorModulation(false);
  m_dl = _pGfx->Close(rc);
  if (sh) {
    const EShaderLayer& L = sh->m_layers[0];
    const bool blends = (L.m_field0C & 0x40) && (L.m_blend[0] == 1 || L.m_blend[1] == 1 || L.m_blend[3] == 1);
    data->m_hasBlend = data->m_hasBlend || blends;
  }
}

// 802FF260
void ESubModelShader::BuildDisplayList(bool, char* name, bool weights, float minX, float maxX) {
  if (m_dl) {
    _pGfx->Destroy(m_dl);
    m_dl = nullptr;
  }
  ERC* rc = _pGfx->Open(RCMode::Deferred);
  rc->SetName(name);
  rc->EnableColorModulation(NeedsColorModulation(GetEShader()));
  for (ESMSStrip& s : m_strips) {
    if (s.m_hasMatIndices)
      for (unsigned int i = 0; i < s.m_numMatIndices; ++i) rc->ModelMatrixIndex(static_cast<int>(i), s.m_matIndex[i]);
    if (weights) MakeWallWeights(s, minX, maxX);
    CreateRCPrimitive(s.m_stripVerts, rc, &s, true, weights, false);
  }
  rc->EnableColorModulation(false);
  m_dl = _pGfx->Close(rc);
}

// 802FF6F8
// The x range of the fixed-point positions.
void ESubModelShader::GetMinMaxX(float* minX, float* maxX) {
  for (const ESMSStrip& s : m_strips) {
    const auto* pos = static_cast<const unsigned char*>(s.m_positions);
    for (int i = 0; i < s.m_vertCount; ++i) {
      const float x = static_cast<float>(static_cast<short>(be16(pos + i * 8)));
      if (x <= *minX) *minX = x;
      if (*maxX <= x) *maxX = x;
    }
  }
}

// 802FF7D0
unsigned int ESubModelShader::CanColorBeModified() {
  const EShader* sh = GetEShader();
  return sh ? ((sh->m_layers[0].m_flags | sh->m_layers[1].m_flags) >> 5) & 1 : 0;
}

// 802FF810
unsigned int ESubModelShader::GetModifiableColor(unsigned int index) {
  const EShader* sh = GetEShader();
  return sh ? static_cast<unsigned int>(sh->m_field40[index]) : 0;
}

// 802FF844
// Every uv (both sets) scaled and moved, by the second transform when this is shader `shaderId`; the
// display list is rebuilt (morph targets have none).
void ESubModelShader::TransformModelUV(EVec2& scale, EVec2& offset, unsigned int shaderId, EVec2& scale2, EVec2& offset2,
                                       bool weldable) {
  EVec2 s = scale, o = offset;
  if (m_shaderId == shaderId) s = scale2, o = offset2;
  if (s.x == 1.0f && s.y == 1.0f && o.x == 0.0f && o.y == 0.0f) return;
  const bool two = (m_flags & kTwoUVs) != 0;
  const int perVert = two ? 4 : 2;
  auto put16 = [](unsigned char* p, int v) {
    p[0] = static_cast<unsigned char>(v >> 8);
    p[1] = static_cast<unsigned char>(v);
  };
  for (ESMSStrip& strip : m_strips) {
    auto* uv = static_cast<unsigned char*>(strip.m_uvs);
    if (!uv) continue;
    for (int v = 0; v < strip.m_vertCount; ++v) {
      for (int c = 0; c < perVert; ++c) {
        const float k = (c & 1) ? s.y : s.x, add = (c & 1) ? o.y : o.x;
        if (m_flags & kFixedPoint) {
          unsigned char* p = uv + (v * perVert + c) * 2;
          const float f = static_cast<float>(static_cast<short>(be16(p))) * (1.0f / 4096.0f);
          put16(p, static_cast<int>((k * f + add) * 4096.0f));
        } else {
          unsigned char* p = uv + (v * perVert + c) * 4;
          float f = befloat(p) * k + add;
          unsigned int u;
          std::memcpy(&u, &f, 4);
          p[0] = static_cast<unsigned char>(u >> 24), p[1] = static_cast<unsigned char>(u >> 16);
          p[2] = static_cast<unsigned char>(u >> 8), p[3] = static_cast<unsigned char>(u);
        }
      }
    }
  }
  if (m_owner && m_owner->m_model && !m_owner->m_model->m_isMorphTarget)
    BuildDisplayList(weldable, const_cast<char*>("ESubModelShader::TransformModelUV"), false, 0.0f, 0.0f);
}
