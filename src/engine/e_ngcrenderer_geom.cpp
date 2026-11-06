// Port of engine/e_ngcrenderer.cpp, continued: the model geometry commands. A model strip is a GX
// display list of index16 vertices over vertex arrays; both stay big-endian as stored on the disc, and
// aurora takes them that way (GXSetArray with le = false, GXCallDisplayList parses the big-endian
// command stream). The other geometry commands (TriStrip, TriStripPacked*, QuadList, ...) are not
// ported yet.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstring>

extern int _ngcRenderEmpty;

int _ngcDrawStrips = 1;        // 804FF294
int _ngcFakeReflection = 0;    // 804FF2B4
int _ngcUseGXDisplayList = 1;  // 804FF250
int _ngcUseWeightBlending = 0; // 804FF350

using simmy::be16;

// 80341EE8
// Sphere-map coordinates for reflective layers: the eye-space normal and position give the reflected
// view vector r = p - 2 (p . n) n; (s, t) = r / |r + (0, 0, |r| ... )| mapped from -1..1 to 0..1.
void ReflectionCoord(Vec& pos, Vec& nrm, float (*posMtx)[4], float (*nrmMtx)[4], Vec& out) {
  if (_ngcFakeReflection) {
    out = Vec{0.5f, 0.5f, 1.0f};
    return;
  }
  Vec n, p, r;
  PSMTXMultVecSR(nrmMtx, &nrm, &n);
  PSVECNormalize(&n, &n);
  PSMTXMultVec(posMtx, &pos, &p);
  PSVECScale(&n, &r, 2.0f * PSVECDotProduct(&p, &n));
  PSVECSubtract(&p, &r, &out);
  out.z = out.z + PSVECMag(&out) + 1.0f;
  PSVECNormalize(&out, &out);
  out.x = out.x * 0.5f + 0.5f;
  out.y = out.y * 0.5f + 0.5f;
}

namespace {
// What both display-list commands share: the vertex descriptors and arrays. Every attribute the
// strip has is an index16 into its array. Returns whether the reflection path applies (a reflective
// layer, and normals and uvs to compute it from).
bool SetupGXDisplayList(ENgcRenderer* r, const EDLEntry* e, bool fixedPoint) {
  const EDLGXDisplayListData& d = *const_cast<EDLEntry*>(e)->Payload<EDLGXDisplayListData>();
  const bool twoUVs = e->b1 != 0;
  const unsigned int n = e->u;
  ENgcRenderStateCache* c = r->m_rsCache;
  c->m_vtxDescTex[5] = 0;
  c->m_dirty |= 0x80000001;
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = 0, c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0;
  for (int i = 0; i < 5; ++i) c->m_vtxDescTex[i] = 0;
  c->m_vtxDescPos = GX_INDEX16;
  const u8 posStride = fixedPoint ? 8 : 16, uvStride = fixedPoint ? 4 : 8;
  GXSetArray(GX_VA_POS, d.positions, n * posStride, posStride, false);
  if (d.normals) {
    c->m_vtxDescNrm = GX_INDEX16;
    GXSetArray(GX_VA_NRM, d.normals, n * 4, 4, false);
  }
  if (d.colors) {
    c->m_vtxDescClr0 = GX_INDEX16;
    GXSetArray(GX_VA_CLR0, d.colors, n * 4, 4, false);
  }
  if (d.uvs) {
    c->m_vtxDescTex[0] = GX_INDEX16;
    const u8 stride = twoUVs ? uvStride * 2 : uvStride;
    GXSetArray(GX_VA_TEX0, d.uvs, n * stride, stride, false);
  }
  const bool reflect = r->m_reflection && d.normals && d.uvs;
  if (!reflect && twoUVs) {
    const u8 stride = uvStride * 2;
    GXSetArray(GX_VA_TEX1, static_cast<const unsigned char*>(d.uvs) + uvStride, n * stride - uvStride, stride, false);
    c->m_vtxDescTex[1] = GX_INDEX16;
  }
  return reflect;
}

// The reflection path: the strip's indices are sent by hand, and the second texture coordinate is
// the sphere-map coordinate computed on the CPU (from position and normal) instead of uv set 1.
template <class EmitCoord>
void DrawReflectedStrip(ENgcRenderer* r, const EDLEntry* e, GXVtxFmt fmt, EmitCoord emit) {
  const EDLGXDisplayListData& d = *const_cast<EDLEntry*>(e)->Payload<EDLGXDisplayListData>();
  const bool twoUVs = e->b1 != 0;
  ENgcRenderStateCache* c = r->m_rsCache;
  c->m_vtxDescTex[1] = GX_DIRECT;
  c->m_dirty |= 0x80000001;
  if (!r->CanDraw()) return;
  c->Apply(false);
  const unsigned char* dl = d.dl;
  const unsigned int count = be16(dl + 1);
  const unsigned char* idx = dl + 3;
  GXBegin(GX_TRIANGLESTRIP, fmt, static_cast<u16>(count));
  for (unsigned int i = 0; i < count; ++i) {
    const unsigned int pos = be16(idx), nrm = be16(idx + 2);
    idx += 4;
    GXPosition1x16(static_cast<u16>(pos));
    GXNormal1x16(static_cast<u16>(nrm));
    if (d.colors) {
      GXColor1x16(be16(idx));
      idx += 2;
    }
    GXTexCoord1x16(be16(idx));
    idx += twoUVs ? 4 : 2;
    emit(pos, nrm, c);
  }
  GXEnd();
}
}  // namespace

// 803477CC
// A fixed-point strip (s16 positions in model units, vertex format 6).
void ENgcRenderer::NgcGXDisplayListInt(EDLEntry* e) {
  _ngcRenderEmpty = 0;
  if (_ngcDrawStrips) {
    if (m_geometryDirty) GeometrySetup(nullptr);
    const EDLGXDisplayListData& d = *e->Payload<EDLGXDisplayListData>();
    if (!SetupGXDisplayList(this, e, true)) {
      if (CanDraw()) {
        m_rsCache->Apply(false);
        GXCallDisplayList(d.dl, d.dlSize);
      }
    } else {
      const auto* pos = static_cast<const unsigned char*>(d.positions);
      const auto* nrm = d.normals;
      DrawReflectedStrip(this, e, GX_VTXFMT6, [&](unsigned int p, unsigned int n, ENgcRenderStateCache* c) {
        Vec v{static_cast<float>(static_cast<short>(be16(pos + p * 8))),
              static_cast<float>(static_cast<short>(be16(pos + p * 8 + 2))),
              static_cast<float>(static_cast<short>(be16(pos + p * 8 + 4)))};
        Vec nv{static_cast<float>(nrm[n * 4]), static_cast<float>(nrm[n * 4 + 1]), static_cast<float>(nrm[n * 4 + 2])};
        Vec st;
        ReflectionCoord(v, nv, c->m_posMtx, c->m_nrmMtx, st);
        // s16 with 12 fractional bits, rounded away from zero.
        const float s = st.x * 4096.0f, t = st.y * 4096.0f;
        GXTexCoord2s16(static_cast<s16>(s + (s < 0.0f ? -0.5f : 0.5f)), static_cast<s16>(t + (t < 0.0f ? -0.5f : 0.5f)));
      });
    }
  }
  m_cursor += EDLEntry::EntriesFor<EDLGXDisplayListData>() - 1;
}

// 80347BE4
// A floating-point strip (f32 positions, vertex format 2).
void ENgcRenderer::NgcGXDisplayList(EDLEntry* e) {
  _ngcRenderEmpty = 0;
  if (_ngcDrawStrips) {
    if (m_geometryDirty) GeometrySetup(nullptr);
    const EDLGXDisplayListData& d = *e->Payload<EDLGXDisplayListData>();
    if (!SetupGXDisplayList(this, e, false)) {
      if (CanDraw()) {
        m_rsCache->Apply(false);
        GXCallDisplayList(d.dl, d.dlSize);
      }
    } else {
      const auto* pos = static_cast<const unsigned char*>(d.positions);
      const auto* nrm = d.normals;
      DrawReflectedStrip(this, e, GX_VTXFMT2, [&](unsigned int p, unsigned int n, ENgcRenderStateCache* c) {
        Vec v{simmy::befloat(pos + p * 16), simmy::befloat(pos + p * 16 + 4), simmy::befloat(pos + p * 16 + 8)};
        Vec nv{static_cast<float>(nrm[n * 4]), static_cast<float>(nrm[n * 4 + 1]), static_cast<float>(nrm[n * 4 + 2])};
        Vec st;
        ReflectionCoord(v, nv, c->m_posMtx, c->m_nrmMtx, st);
        GXTexCoord2f32(st.x, st.y);
      });
    }
  }
  m_cursor += EDLEntry::EntriesFor<EDLGXDisplayListData>() - 1;
}

// ---- Plain strips --------------------------------------------------------------------------------

// 8033C510
// A GX display list for a plain fixed-point strip (vertex format 6): vertex i takes entry i of every
// array (index8 for up to 254 vertices, else index16), preceded by its skinning matrix slot (the low
// bits of the position's pad word, times 3) when it is weighted. The list belongs to `dl` and is
// freed with it. GC: built with the GD library under _ngcCreateGXMutex; the bytes are the same.
unsigned char* ENgcRenderer::CreateGXDisplayList(EDL* dl, GXPrimitive prim, int count, short* positions, short* uvs,
                                                 unsigned char* colors, signed char* normals, unsigned int& size,
                                                 bool twoUVs, bool weights) {
  int attrs = normals ? 2 : 1;
  if (colors) ++attrs;
  if (uvs) attrs += twoUVs ? 2 : 1;
  const int indexSize = count > 0xfe ? 2 : 1;
  size = static_cast<unsigned int>((attrs * indexSize + (weights ? 1 : 0)) * count + 0x22) & ~0x1fu;
  auto* out = static_cast<unsigned char*>(dl->Alloc(size, 0x20));
  std::memset(out, 0, size);  // the padding is GX_NOP
  unsigned char* p = out;
  *p++ = static_cast<unsigned char>(prim | GX_VTXFMT6);
  *p++ = static_cast<unsigned char>(count >> 8);
  *p++ = static_cast<unsigned char>(count);
  const auto* pos = reinterpret_cast<const unsigned char*>(positions);
  for (int i = 0; i < count; ++i) {
    if (weights) *p++ = static_cast<unsigned char>((pos[i * 8 + 7] & 3) * 3);
    auto index = [&]() {
      if (indexSize == 2) *p++ = static_cast<unsigned char>(i >> 8);
      *p++ = static_cast<unsigned char>(i);
    };
    index();
    if (normals) index();
    if (colors) index();
    if (uvs) {
      index();
      if (twoUVs) index();
    }
  }
  return out;
}

int _ngcDrawWeighted = 1;  // 804FF2A8

// 8034932C
// Weighted (smooth-skinned) fixed-point vertices, blended on the CPU: each position by up to four of
// the selected model matrices (weights /255), the normal by the heaviest one (normalized, scaled to
// 127); sent with vertex format 3 under an identity model matrix.
void ENgcRenderer::WeightedBlendInt(GXPrimitive prim, int count, short* positions, signed char* normals, short* uvs,
                                    unsigned char* colors, unsigned char* weights, bool twoUVs) {
  if (!_ngcDrawWeighted) return;
  _ngcRenderEmpty = 0;
  if (!_ngcDrawStrips) return;
  ENgcRenderStateCache* c = m_rsCache;
  if (twoUVs) {
    c->m_vtxDescTex[1] = GX_DIRECT;
    c->m_dirty |= 0x80000001;
  }
  PSMTXIdentity(c->m_modelMtx[0]);
  c->m_dirty |= 2;
  c->Apply(false);
  const auto* pos = reinterpret_cast<const unsigned char*>(positions);
  const auto* uv = reinterpret_cast<const unsigned char*>(uvs);
  GXBegin(prim, GX_VTXFMT3, static_cast<u16>(count));
  for (int i = 0; i < count; ++i) {
    Vec v{static_cast<float>(static_cast<short>(be16(pos))), static_cast<float>(static_cast<short>(be16(pos + 2))),
          static_cast<float>(static_cast<short>(be16(pos + 4)))};
    pos += 8;
    Vec out{0.0f, 0.0f, 0.0f};
    int heaviest = 0;
    unsigned char most = 0;
    for (int m = 0; m < 4; ++m) {
      const unsigned char w = weights[m];
      if (!w) continue;
      Vec t;
      PSMTXMultVec(m_modelMtx[m], &v, &t);
      const float k = static_cast<float>(w) * (1.0f / 255.0f);
      out.x += t.x * k;
      out.y += t.y * k;
      out.z += t.z * k;
      if (w > most) heaviest = m, most = w;
    }
    GXPosition3f32(out.x, out.y, out.z);
    if (normals) {
      Vec n{static_cast<float>(normals[0]), static_cast<float>(normals[1]), static_cast<float>(normals[2])};
      PSMTXMultVecSR(m_modelMtx[heaviest], &n, &n);
      const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
      if (len > 0.0f) n.x /= len, n.y /= len, n.z /= len;
      GXNormal3s8(static_cast<s8>(n.x * 127.0f), static_cast<s8>(n.y * 127.0f), static_cast<s8>(n.z * 127.0f));
      normals += 4;
    }
    if (colors) {
      GXColor4u8(colors[0], colors[1], colors[2], colors[3]);
      colors += 4;
    }
    if (uv) {
      GXTexCoord2s16(static_cast<s16>(be16(uv)), static_cast<s16>(be16(uv + 2)));
      if (twoUVs) {
        GXTexCoord2s16(static_cast<s16>(be16(uv + 4)), static_cast<s16>(be16(uv + 6)));
        uv += 8;
      } else {
        uv += 4;
      }
    }
    weights += 4;
  }
  GXEnd();
}

// 80343264
// A plain fixed-point strip, by its GX display list. Vertex descriptors are index8/16 to match the
// list; weighted strips send a matrix index per vertex and use the four skinning matrices. With a
// reflective layer the indices are sent by hand and uv set 1 is replaced by sphere-map coordinates.
void ENgcRenderer::TriStripPackedInt(EDLEntry* e) {
  _ngcRenderEmpty = 0;
  if (_ngcDrawStrips) {
    if (m_geometryDirty) GeometrySetup(nullptr);
    const EDLTriStripPackedData& d = *e->Payload<EDLTriStripPackedData>();
    const bool twoUVs = e->b1 & 1, weighted = (e->b1 >> 1) & 1;
    const unsigned int count = e->u;
    int type = count > 0xfe ? GX_INDEX16 : GX_INDEX8;
    if (!d.dl || (d.weights && _ngcUseWeightBlending)) type = GX_DIRECT;
    ENgcRenderStateCache* c = m_rsCache;
    c->m_vtxDescTex[5] = 0;
    c->m_vtxDescPnMtx = 0;
    c->m_vtxDescPos = 0;
    c->m_dirty |= 0x80000001;
    c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0;
    for (int i = 0; i < 5; ++i) c->m_vtxDescTex[i] = 0;
    c->m_vtxDescPos = type;
    if (d.normals) c->m_vtxDescNrm = type;
    if (d.colors) c->m_vtxDescClr0 = type;
    if (d.uvs) c->m_vtxDescTex[0] = type;
    if (d.weights && (!d.dl || _ngcUseWeightBlending)) {
      // Weighted vertices are blended on the CPU.
      WeightedBlendInt(GX_TRIANGLESTRIP, static_cast<int>(count),
                       static_cast<short*>(const_cast<void*>(d.positions)), const_cast<signed char*>(d.normals),
                       static_cast<short*>(const_cast<void*>(d.uvs)), const_cast<unsigned char*>(d.colors),
                       const_cast<unsigned char*>(d.weights), twoUVs);
    } else if (!d.dl) {
      // GC: sent the vertices directly (with sphere-mapped uvs on reflective layers).
      SIMMY_STUB("ENgcRenderer::TriStripPackedInt: strips without a GX display list", 0x80343264);
    } else {
      GXSetArray(GX_VA_POS, d.positions, count * 8, 8, false);
      if (d.normals) GXSetArray(GX_VA_NRM, d.normals, count * 4, 4, false);
      if (d.colors) GXSetArray(GX_VA_CLR0, d.colors, count * 4, 4, false);
      if (d.uvs) GXSetArray(GX_VA_TEX0, d.uvs, count * (twoUVs ? 8 : 4), twoUVs ? 8 : 4, false);
      if (!m_reflection || !d.normals || !d.uvs || weighted) {
        if (twoUVs) {
          GXSetArray(GX_VA_TEX1, static_cast<const unsigned char*>(d.uvs) + 4, count * 8 - 4, 8, false);
          c->m_vtxDescTex[1] = type;
        }
        if (weighted) c->m_vtxDescPnMtx = GX_DIRECT;
        if (CanDraw()) {
          c->Apply(weighted);
          GXCallDisplayList(d.dl, d.dlSize);
        }
      } else {
        c->m_vtxDescTex[1] = GX_DIRECT;
        c->m_dirty |= 0x80000001;
        if (CanDraw()) {
          c->Apply(false);
          const auto* pos = static_cast<const unsigned char*>(d.positions);
          auto index = [type](unsigned int i) {
            if (type == GX_INDEX16) GXPosition1x16(static_cast<u16>(i));
            else GXPosition1x8(static_cast<u8>(i));
          };
          GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT6, static_cast<u16>(count));
          for (unsigned int i = 0; i < count; ++i) {
            index(i);                 // position
            index(i);                 // normal
            if (d.colors) index(i);
            index(i);                 // uv set 0
            Vec v{static_cast<float>(static_cast<short>(be16(pos + i * 8))),
                  static_cast<float>(static_cast<short>(be16(pos + i * 8 + 2))),
                  static_cast<float>(static_cast<short>(be16(pos + i * 8 + 4)))};
            Vec nv{static_cast<float>(d.normals[i * 4]), static_cast<float>(d.normals[i * 4 + 1]),
                   static_cast<float>(d.normals[i * 4 + 2])};
            Vec st;
            ReflectionCoord(v, nv, c->m_posMtx, c->m_nrmMtx, st);
            const float s = st.x * 4096.0f, t = st.y * 4096.0f;
            GXTexCoord2s16(static_cast<s16>(s + (s < 0.0f ? -0.5f : 0.5f)), static_cast<s16>(t + (t < 0.0f ? -0.5f : 0.5f)));
          }
          GXEnd();
        }
      }
    }
  }
  m_cursor += EDLEntry::EntriesFor<EDLTriStripPackedData>() - 1;
}

// 80342878 (with 803428D0)
// A float strip the CPU built (native floats: x, y, z, w per position; s, t per uv; RGBA colours;
// s8 normals padded to four bytes). GC: drawn from arrays by index (GX_VTXFMT7); PC: sent directly.
RET_UNKNOWN ENgcRenderer::TriStripPacked(EDLEntry* e) {
  m_cursor += EDLEntry::EntriesFor<EDLTriStripPackedData>() - 1;  // past the payload
  _ngcRenderEmpty = 0;
  if (!_ngcDrawStrips) return 0;
  if (m_geometryDirty) GeometrySetup(nullptr);
  const EDLTriStripPackedData& d = *e->Payload<EDLTriStripPackedData>();
  const unsigned int count = e->u;
  const auto* pos = static_cast<const float*>(d.positions);
  const auto* uv = static_cast<const float*>(d.uvs);
  const unsigned char* colors = d.colors;
  const signed char* normals = d.normals;
  ENgcRenderStateCache* c = m_rsCache;
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = GX_DIRECT;
  c->m_vtxDescNrm = normals ? GX_DIRECT : 0;
  c->m_vtxDescClr0 = colors ? GX_DIRECT : 0;
  for (int i = 0; i < 6; ++i) c->m_vtxDescTex[i] = 0;
  if (uv) c->m_vtxDescTex[0] = GX_DIRECT;
  c->m_dirty |= 0x80000001;
  if (!CanDraw()) return 0;
  c->Apply(false);
  GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT2, static_cast<u16>(count));
  for (unsigned int i = 0; i < count; ++i) {
    GXPosition3f32(pos[i * 4], pos[i * 4 + 1], pos[i * 4 + 2]);
    if (normals) GXNormal3s8(normals[i * 4], normals[i * 4 + 1], normals[i * 4 + 2]);
    if (colors) GXColor4u8(colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3]);
    if (uv) GXTexCoord2f32(uv[i * 2], uv[i * 2 + 1]);
  }
  GXEnd();
  return 0;
}
