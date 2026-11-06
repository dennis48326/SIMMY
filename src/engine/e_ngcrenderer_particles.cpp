// Port of the sprite-particle path and the quad lists of engine/e_ngcrenderer.cpp (ENgcRenderer::ParticleListBegin /
// ParticleListEnd), engine/e_ngcrc.cpp (ENgcRC::ParticleListInfo / Begin / End) and engine/e_rc.cpp
// (ERC's: nothing recorded). The particle code (game/emitterspr3d.cpp) fills a list of packed
// sprites (EGEPackedParticle) between Begin and End; End draws them as camera- or emitter-facing
// quads straight through GX (vertex format 7: position, colour, uv), the corners worked out on the
// CPU from model matrix 0 (the facing matrix) and the view matrix, with the identity position matrix.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

extern int _ngcRenderEmpty;
extern int _ngcDrawStrips;

extern ENgcRenderer _ngcRenderer;

// 802F7068
void ERC::ParticleListInfo(EGEParticleListInfo*) {}
// 802F706C
void* ERC::ParticleListBegin(unsigned int, unsigned int) { return nullptr; }
// 802F7074
void ERC::ParticleListEnd(unsigned int) {}

// 8033B3B0
void ENgcRC::ParticleListInfo(EGEParticleListInfo*) {}
// 8033B3B4
void* ENgcRC::ParticleListBegin(unsigned int count, unsigned int type) { return _ngcRenderer.ParticleListBegin(count, type); }
// 8033B3DC
void ENgcRC::ParticleListEnd(unsigned int count) { _ngcRenderer.ParticleListEnd(count); }

// 80349FDC
// Room for `count` sprites (0x40 bytes each), handed back to be filled.
void* ENgcRenderer::ParticleListBegin(unsigned int count, unsigned int type) {
  m_particleList = std::malloc(static_cast<size_t>(count) << 6 ? static_cast<size_t>(count) << 6 : 1);
  m_particleType = type;
  m_particleCount = count;
  return m_particleList;
}

namespace {
// PSMTXMultVecSR / PSMTXMultVec on GX (row-major 3x4) matrices.
void MultVecSR(const Mtx m, const float v[3], float out[3]) {
  const float x = v[0], y = v[1], z = v[2];
  out[0] = m[0][0] * x + m[0][1] * y + m[0][2] * z;
  out[1] = m[1][0] * x + m[1][1] * y + m[1][2] * z;
  out[2] = m[2][0] * x + m[2][1] * y + m[2][2] * z;
}
void MultVec(const Mtx m, const float v[3], float out[3]) {
  MultVecSR(m, v, out);
  out[0] += m[0][3], out[1] += m[1][3], out[2] += m[2][3];
}
}  // namespace

// 8034A02C
// `count` sprites of the list as quads: each corner is the centre -/+ the half width along the
// facing matrix's first axis and -/+ the half height along its second (model matrix 0 transposed),
// taken into view space by the cache's view matrix and drawn with the identity matrix (GX_PNMTX5);
// colour from the low bytes of the packed colour (alpha 0..127 doubled); uv u0,v0 .. u1,v1. Count 0
// only frees the list.
void ENgcRenderer::ParticleListEnd(unsigned int count) {
  if (count == 0) {
    std::free(m_particleList);
    m_particleList = nullptr;
    return;
  }
  if (m_geometryDirty) GeometrySetup(nullptr);
  ENgcRenderStateCache* c = m_rsCache;
  c->m_vtxDescTex[5] = 0;
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = 0;
  c->m_dirty |= 0x80000001;
  c->m_vtxDescNrm = 0;
  c->m_vtxDescClr0 = 0;
  for (int i = 0; i < 5; ++i) c->m_vtxDescTex[i] = 0;
  c->m_vtxDescPos = 1;
  c->m_vtxDescClr0 = 1;
  c->m_vtxDescTex[0] = 1;
  c->m_currentMtx = GX_PNMTX5;
  c->m_dirty |= 0x80000000;
  Mtx facing;
  PSMTXTranspose(c->m_modelMtx[0], facing);
  const auto* p = static_cast<const EGEPackedParticle*>(m_particleList);
  if (CanDraw()) {
    c->Apply(false);
    GXBegin(GX_QUADS, GX_VTXFMT7, static_cast<u16>((count & 0x3fff) << 2));
    for (unsigned int i = 0; i < count; ++i, ++p) {
      float right[3] = {p->m_half[0], 0.0f, 0.0f};
      float up[3] = {0.0f, p->m_half[1], 0.0f};
      MultVecSR(facing, right, right);
      MultVecSR(facing, up, up);
      const unsigned char a = static_cast<unsigned char>((p->m_color[3] & 0x7f) << 1);
      const unsigned char r = static_cast<unsigned char>(p->m_color[0]);
      const unsigned char g = static_cast<unsigned char>(p->m_color[1]);
      const unsigned char b = static_cast<unsigned char>(p->m_color[2]);
      const float sx[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
      const float sy[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
      const float u[4] = {p->m_uv[0], p->m_uv[0], p->m_uv[2], p->m_uv[2]};
      const float v[4] = {p->m_uv[1], p->m_uv[3], p->m_uv[3], p->m_uv[1]};
      for (int k = 0; k < 4; ++k) {
        float corner[3] = {p->m_pos[0] + sx[k] * right[0] + sy[k] * up[0], p->m_pos[1] + sx[k] * right[1] + sy[k] * up[1],
                           p->m_pos[2] + sx[k] * right[2] + sy[k] * up[2]};
        MultVec(c->m_viewMtx, corner, corner);
        GXPosition3f32(corner[0], corner[1], corner[2]);
        GXColor4u8(r, g, b, a);
        GXTexCoord2f32(u[k], v[k]);
      }
    }
    GXEnd();
  }
  c->m_currentMtx = GX_PNMTX4;
  c->m_dirty |= 0x80000000;
  std::free(m_particleList);
  m_particleList = nullptr;
}

// ---- Quad lists (the effects system's particles: FastParticleEmitter) ------------------------------

namespace {
// A recorded quad list (GC: the arrays copied into the display list, +0x08..+0x1C of the entry).
struct EDLQuadListData {
  const float* verts;            // x, y, z, (pad) per vertex
  const float* uvs;              // s, t (and a second pair with twoUVs)
  const unsigned char* colors;   // RGBA
  const signed char* normals;    // x, y, z, (pad)
  unsigned char* dl;             // a GX display list over the arrays (_ngcUseGXDisplayList)
  unsigned int dlSize;
};

// (ENgcRenderer::CreateGXDisplayList 80345F24 with GX_QUADS) The quads as a GX display list of
// indices into the arrays (vertex format 7: GC's CreateGXDisplayList ORs format 7 into the primitive;
// the port's model version uses 6 for its fixed-point strips): per vertex its position, normal,
// colour and uv(s) index, 8-bit below 0xFF vertices else 16-bit, padded to 32 bytes with GX_NOP.
unsigned char* CreateQuadGXDisplayList(EDL* dl, int count, bool normals, bool colors, bool uvs, bool twoUVs,
                                       unsigned int& size) {
  int attrs = normals ? 2 : 1;
  if (colors) ++attrs;
  if (uvs) attrs += twoUVs ? 2 : 1;
  const int indexSize = count > 0xfe ? 2 : 1;
  size = static_cast<unsigned int>(attrs * indexSize * count + 0x22) & ~0x1fu;
  auto* out = static_cast<unsigned char*>(dl->Alloc(size, 0x20));
  std::memset(out, 0, size);
  unsigned char* p = out;
  *p++ = static_cast<unsigned char>(GX_QUADS | GX_VTXFMT7);
  *p++ = static_cast<unsigned char>(count >> 8);
  *p++ = static_cast<unsigned char>(count);
  for (int i = 0; i < count; ++i)
    for (int a = 0; a < attrs; ++a) {
      if (indexSize == 2) *p++ = static_cast<unsigned char>(i >> 8);
      *p++ = static_cast<unsigned char>(i);
    }
  return out;
}
}  // namespace

extern int _ngcUseGXDisplayList;
void ReflectionCoord(Vec& pos, Vec& nrm, float (*posMtx)[4], float (*nrmMtx)[4], Vec& out);  // 80341EE8 (geom)

// 802F6BEC
void ERC::QuadList(int, float*, float*, unsigned char*, signed char*, bool) {}

// 8033B020
// Immediate: drawn at once from the caller's arrays. Deferred: the arrays are copied into the list
// (32-byte aligned) and a quad-list entry recorded.
void ENgcRC::QuadList(int count, float* verts, float* uvs, unsigned char* colors, signed char* normals, bool twoUVs) {
  if (count == 0) return;
  if (m_mode == RCMode::Immediate) {
    m_dl->m_vertCount += count;
    _ngcRenderer.QuadList(count, verts, uvs, colors, normals, nullptr, nullptr, twoUVs);
    return;
  }
  const unsigned int nVerts = static_cast<unsigned int>(count) * 0x10;
  const unsigned int nUVs = uvs ? (twoUVs ? nVerts : static_cast<unsigned int>(count) << 3) : 0;
  const unsigned int nColors = colors ? static_cast<unsigned int>(count) << 2 : 0;
  const unsigned int nNormals = normals ? static_cast<unsigned int>(count) << 2 : 0;
  auto copy = [this](const void* src, unsigned int size) -> void* {
    if (!src || !size) return nullptr;
    void* dst = Alloc(size, 0x20);
    std::memcpy(dst, src, size);
    return dst;
  };
  EDLQuadListData d;
  d.verts = static_cast<const float*>(copy(verts, nVerts));
  d.uvs = static_cast<const float*>(copy(uvs, nUVs));
  d.normals = static_cast<const signed char*>(copy(normals, nNormals));
  d.colors = static_cast<const unsigned char*>(copy(colors, nColors));
  d.dl = nullptr;
  d.dlSize = 0;
  if (_ngcUseGXDisplayList)
    d.dl = CreateQuadGXDisplayList(m_dl, count, d.normals != nullptr, d.colors != nullptr, d.uvs != nullptr, twoUVs, d.dlSize);
  BeginCommand(2, 8);
  m_dl->m_vertCount += count;
  EDLEntry* e = NewEntry(EDLEntry::EntriesFor<EDLQuadListData>());
  e->u = static_cast<unsigned int>(count);
  e->op = kDL_QuadList;
  e->b1 = twoUVs ? 1 : 0;
  *e->Payload<EDLQuadListData>() = d;
  EndCommand();
}

// 803457B0
void ENgcRenderer::QuadList(EDLEntry* e) {
  m_cursor += EDLEntry::EntriesFor<EDLQuadListData>() - 1;  // past the payload
  const EDLQuadListData& d = *e->Payload<EDLQuadListData>();
  QuadList(static_cast<int>(e->u), const_cast<float*>(d.verts), const_cast<float*>(d.uvs), const_cast<unsigned char*>(d.colors),
           const_cast<signed char*>(d.normals), d.dl, reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(d.dlSize)),
           e->b1 != 0);
}

// 80345808
// `count` vertices (four per quad) through vertex format 7: position, normal, colour, uv (the second
// set when the shader uses it) - sent directly, or (recorded lists) indexed from the arrays by the
// list's GX display list. A reflective shader with normals and uvs gets sphere-mapped uvs instead
// (ReflectionCoord in the eye space of the current model view), sent directly.
void ENgcRenderer::QuadList(int count, float* verts, float* uvs, unsigned char* colors, signed char* normals,
                            unsigned char* dl, unsigned char* dlSizeArg, bool twoUVs) {
  // PC: a recorded display list carries the second uv index whenever the list had two sets; when the
  // shader turns the second set off (below) the indices would not match the descriptor, so the
  // arrays are sent directly instead.
  if (dl && twoUVs && !m_usesSecondUV) dl = nullptr;
  _ngcRenderEmpty = 0;
  if (!_ngcDrawStrips) return;
  if (m_geometryDirty) GeometrySetup(nullptr);
  ENgcRenderStateCache* c = m_rsCache;
  const int type = dl ? (count < 0xff ? GX_INDEX8 : GX_INDEX16) : GX_DIRECT;
  c->m_vtxDescNrm = 0;
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = 0;
  c->m_dirty |= 0x80000001;
  c->m_vtxDescClr0 = 0;
  for (int i = 0; i < 6; ++i) c->m_vtxDescTex[i] = 0;
  c->m_vtxDescPos = type;
  if (normals) c->m_vtxDescNrm = type;
  if (colors) c->m_vtxDescClr0 = type;
  if (uvs) {
    c->m_vtxDescTex[0] = type;
    if (m_usesSecondUV) {
      if (!twoUVs) m_usesSecondUV = 0;
      else c->m_vtxDescTex[1] = type;
    }
  }
  c->m_dirty |= 0x80000001;
  const bool reflect = m_reflection && normals && uvs;
  if (dl && !reflect) {
    GXSetArray(GX_VA_POS, verts, static_cast<u32>(count) * 16, 0x10, false);
    if (normals) GXSetArray(GX_VA_NRM, normals, static_cast<u32>(count) * 4, 4, false);
    if (colors) GXSetArray(GX_VA_CLR0, colors, static_cast<u32>(count) * 4, 4, false);
    if (uvs) GXSetArray(GX_VA_TEX0, uvs, static_cast<u32>(count) * (twoUVs ? 16 : 8), twoUVs ? 0x10 : 8, false);
    if (uvs && twoUVs) GXSetArray(GX_VA_TEX1, uvs + 2, static_cast<u32>(count) * 16 - 8, 0x10, false);
    if (!CanDraw()) return;
    c->Apply(false);
    GXCallDisplayList(dl, static_cast<u32>(reinterpret_cast<std::uintptr_t>(dlSizeArg)));
    return;
  }
  if (reflect) {
    // GC: from the display list's indices when there is one (the same vertices in order).
    if (dl) {
      c->m_vtxDescPos = GX_DIRECT;
      c->m_vtxDescNrm = GX_DIRECT;
      if (colors) c->m_vtxDescClr0 = GX_DIRECT;
      c->m_vtxDescTex[0] = GX_DIRECT;
      if (c->m_vtxDescTex[1]) c->m_vtxDescTex[1] = GX_DIRECT;
    }
    if (!CanDraw()) return;
    c->Apply(false);
    GXBegin(GX_QUADS, GX_VTXFMT7, static_cast<u16>(count));
    for (int i = 0; i < count; ++i) {
      GXPosition3f32(verts[i * 4], verts[i * 4 + 1], verts[i * 4 + 2]);
      GXNormal3s8(normals[i * 4], normals[i * 4 + 1], normals[i * 4 + 2]);
      if (colors) GXColor4u8(colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3]);
      Vec v{verts[i * 4], verts[i * 4 + 1], verts[i * 4 + 2]};
      Vec nv{static_cast<float>(normals[i * 4]), static_cast<float>(normals[i * 4 + 1]), static_cast<float>(normals[i * 4 + 2])};
      Vec st;
      ReflectionCoord(v, nv, c->m_posMtx, c->m_nrmMtx, st);
      GXTexCoord2f32(st.x, st.y);
      if (c->m_vtxDescTex[1]) GXTexCoord2f32(st.x, st.y);  // PC: GC sent one set (the descriptor would read past it)
    }
    GXEnd();
    return;
  }
  if (!CanDraw()) return;
  c->Apply(false);
  GXBegin(GX_QUADS, GX_VTXFMT7, static_cast<u16>(count));
  for (int i = 0; i < count; ++i) {
    GXPosition3f32(verts[i * 4], verts[i * 4 + 1], verts[i * 4 + 2]);
    if (normals) GXNormal3s8(normals[i * 4], normals[i * 4 + 1], normals[i * 4 + 2]);
    if (colors) GXColor4u8(colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3]);
    if (uvs) {
      const float* uv = uvs + i * (twoUVs ? 4 : 2);
      GXTexCoord2f32(uv[0], uv[1]);
      if (m_usesSecondUV) GXTexCoord2f32(uv[2], uv[3]);
    }
  }
  GXEnd();
}

// 802F9AB8
// The outside radiosity light (ERLevel::DrawOrderTableEntry for records lit by it): nothing on the
// GameCube (ENgcRC keeps ERC's empty one).
void ERC::LightsRadiosity(EVec3&, EVec3&, float) {}
