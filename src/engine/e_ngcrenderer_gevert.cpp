// Port of the immediate-vertex primitives used by the Apt UI (engine/e_rc.cpp, e_ngcrc.cpp,
// e_ngcrenderer.cpp): TriStrip and LineList of EGEVert, and the destination-alpha "stencil" the UI
// masks use (the GameCube has no stencil buffer; masks are drawn into the frame buffer's alpha).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <dolphin/gx.h>

extern ENgcRenderer _ngcRenderer;
extern int _ngcRenderEmpty;
extern int _ngcDrawStrips;

// ---- ERC (recording) -----------------------------------------------------------------------------

// 802F683C
void ERC::TriStrip(EGEVert* verts, int count) {
  if (!count) return;
  void* copy = AllocAndCopy(verts, count * static_cast<int>(sizeof(EGEVert)));
  BeginCommand(2, 1);
  m_dl->m_vertCount += count;
  EDLEntry* e = NewEntry(1);
  e->p = copy;
  e->op = kDL_TriStrip;
  e->u = static_cast<unsigned int>(count);
  EndCommand();
}

// 802F6BF0
void ERC::LineList(EGEVert* verts, int count) {
  if (!count) return;
  void* copy = AllocAndCopy(verts, count * static_cast<int>(sizeof(EGEVert)));
  BeginCommand(2, 5);
  m_dl->m_vertCount += count;
  EDLEntry* e = NewEntry(1);
  e->p = copy;
  e->op = kDL_LineList;
  e->u = static_cast<unsigned int>(count);
  EndCommand();
}

// 802F8D44
RET_UNKNOWN ERC::Stencil(int mode, int func, int ref) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->op = kDL_Stencil;
  e->b1 = static_cast<unsigned char>(mode);
  e->b2 = static_cast<unsigned char>(func);
  e->b3 = static_cast<unsigned char>(ref);
  EndCommand();
  return 0;
}

// 802F9494
// Writes `value` into the mask over the rectangle: alpha writes only, z test always, then back to
// the defaults.
RET_UNKNOWN ERC::StencilClear(float x0, float y0, float x1, float y1, int value) {
  ZTest(true, 1, 1, 0);
  AlphaTest(false, 5, 0.5f, 0);
  DisableRasterModes(0x40, 0);
  Stencil(2, 0, value);
  EVec2 p0{x0, y0}, p1{x1, y1}, uv0{0, 0}, uv1{0, 0};
  EVec4 color{1, 0, 0, 0};
  Rect(p0, p1, uv0, uv1, color, 1.0f);
  Stencil(0, 0, 0);
  AlphaTest(true, 5, 0.5f, 0);
  ZTest(true, 2, 0, 0);
  return 0;
}

// ---- ENgcRC (immediate) ------------------------------------------------------------------------------

// 80338428
void ENgcRC::TriStrip(EGEVert* verts, int count) {
  if (!count) return;
  if (m_mode == RCMode::Immediate) {
    m_dl->m_vertCount += count;
    _ngcRenderer.TriStrip(verts, count);
  } else {
    ERC::TriStrip(verts, count);
  }
}

// 8033B350
void ENgcRC::LineList(EGEVert* verts, int count) {
  if (!count) return;
  if (m_mode == RCMode::Immediate) {
    m_dl->m_vertCount += count;
    _ngcRenderer.LineList(verts, count);
  } else {
    ERC::LineList(verts, count);
  }
}

// 8033AEC8
RET_UNKNOWN ENgcRC::Stencil(int mode, int func, int ref) {
  EDLEntry e;
  e.op = kDL_Stencil;
  e.b1 = static_cast<unsigned char>(mode);
  e.b2 = static_cast<unsigned char>(func);
  e.b3 = static_cast<unsigned char>(ref);
  _ngcRenderer.Stencil(&e);
  return 0;
}

// ---- ENgcRenderer ------------------------------------------------------------------------------------

namespace {
// The vertex layout of EGEVert strips: direct position, normal, colour and one uv set.
void SetGEVertDesc(ENgcRenderStateCache* c, bool lines) {
  c->m_vtxDescTex[5] = 0;
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = 0, c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0;
  for (int i = 0; i < 5; ++i) c->m_vtxDescTex[i] = 0;
  c->m_dirty |= 0x80000001;
  c->m_vtxDescPos = 1;
  c->m_vtxDescClr0 = 1;
  if (!lines) {
    c->m_vtxDescNrm = 1;
    c->m_vtxDescTex[0] = 1;
  }
}
}  // namespace

// 80342120
RET_UNKNOWN ENgcRenderer::TriStrip(EDLEntry* e) {
  TriStrip(static_cast<EGEVert*>(e->p), static_cast<int>(e->u & 0xffff));
  return 0;
}

// 80342148
// GC: with a reflection texgen in use (+0x5C8) a second uv set comes from ReflectionCoord; the UI
// never uses one and it is not ported.
void ENgcRenderer::TriStrip(EGEVert* v, int count) {
  _ngcRenderEmpty = 0;
  if (!_ngcDrawStrips) return;
  if (m_geometryDirty) GeometrySetup(nullptr);
  ENgcRenderStateCache* c = m_rsCache;
  if (c->m_field6E8 & 0x40) return;
  SetGEVertDesc(c, false);
  if (!CanDraw()) return;
  c->Apply(false);
  GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT2, static_cast<u16>(count));
  for (int i = 0; i < count; ++i, ++v) {
    GXPosition3f32(v->pos[0], v->pos[1], v->pos[2]);
    GXNormal3s8(static_cast<s8>(v->normal[0]), static_cast<s8>(v->normal[1]), static_cast<s8>(v->normal[2]));
    GXColor4u8(static_cast<u8>(v->color[0]), static_cast<u8>(v->color[1]), static_cast<u8>(v->color[2]),
               static_cast<u8>(v->color[3]));
    GXTexCoord2f32(v->uv[0], v->uv[1]);
  }
  GXEnd();
}

// 80343BD4
void ENgcRenderer::LineList(EDLEntry* e) { LineList(static_cast<EGEVert*>(e->p), static_cast<int>(e->u & 0xffff)); }

// 80343BFC
void ENgcRenderer::LineList(EGEVert* v, int count) {
  if (m_geometryDirty) GeometrySetup(nullptr);
  ENgcRenderStateCache* c = m_rsCache;
  SetGEVertDesc(c, true);
  if (!CanDraw()) return;
  c->Apply(false);
  GXBegin(GX_LINES, GX_VTXFMT2, static_cast<u16>(count));
  for (int i = 0; i < count; ++i, ++v) {
    GXPosition3f32(v->pos[0], v->pos[1], v->pos[2]);
    GXColor4u8(static_cast<u8>(v->color[0]), static_cast<u8>(v->color[1]), static_cast<u8>(v->color[2]),
               static_cast<u8>(v->color[3]));
  }
  GXEnd();
}

// 80347630
// b1: 0 colour and alpha writes, 1 colour only, 2 alpha only; b3 the alpha written (1 -> 0x40).
// b2: 0 normal blending, 1 / 2 blend by inverse / destination alpha (draw outside / inside masks).
void ENgcRenderer::Stencil(EDLEntry* e) {
  if (m_geometryDirty) GeometrySetup(nullptr);
  const unsigned char mode = e->b1, func = e->b2;
  const u8 alpha = e->b3 ? (e->b3 == 1 ? 0x40 : 0) : 0;
  if (mode == 1) {
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetDstAlpha(GX_TRUE, alpha);
  } else if (mode == 0 || mode == 2) {
    GXSetColorUpdate(mode == 0 ? GX_TRUE : GX_FALSE);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetDstAlpha(GX_TRUE, alpha);
  }
  ENgcRenderStateCache* c = m_rsCache;
  if (func == 0) c->SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  else if (func == 2) c->SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_INVDSTALPHA, GX_LO_CLEAR);
  else if (func == 1) c->SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTALPHA, GX_BL_DSTALPHA, GX_LO_CLEAR);
}
