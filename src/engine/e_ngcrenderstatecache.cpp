// Port of engine/e_ngcrenderstatecache.cpp.
//
// The original compared every group of state with a copy of what GX last received and only sent
// what differed (it also had a debug switch, _ngcIgnoreRSCache, to always send everything). The port
// does the same for the groups only this cache sends (vertex descriptors, TEV stages, lights, colour
// channels, texture coordinate generation and matrices): each frame's first Apply (and one after a
// full reset, m_dirty 0xFFFFFFFF) sends everything, later ones what differs from what was sent. The
// rest (viewport, scissor, blending, depth, alpha compare, position matrices, projection) is always
// sent, as other code also sets them directly. SIMMY_RS_CACHE=0 sends everything always.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdlib>
#include <cstring>

Mtx44 ENgcRenderStateCache::rectOrthoMtx;    // 804FCFCC
Mtx44 ENgcRenderStateCache::screenOrthoMtx;  // 804FD00C
Mtx44 ENgcRenderStateCache::projMtx;         // 804FD04C
extern int _ngcStateChangesPerFrame;

// ENgcRSTevFlagMap (filled by a static initialiser at 8034A5D0) and ENgcRSLightFlagMap (80442B40).
const unsigned int ENgcRenderStateCache::kTevFlag[16] = {1u << 0, 1u << 1, 1u << 2,  1u << 3,  1u << 4,  1u << 5,
                                                         1u << 6, 1u << 7, 1u << 8,  1u << 9,  1u << 10, 1u << 11,
                                                         1u << 12, 1u << 13, 1u << 14, 1u << 15};
const unsigned int ENgcRenderStateCache::kLightFlag[4] = {0x100, 0x200, 0x400, 0x800};

namespace {
// SaveState's stack: depth/alpha compare and raster modes (0x28 bytes per entry in the original).
struct SavedState {
  unsigned char zEnable; int zFunc; unsigned char zUpdate;
  int alphaComp0; unsigned char alphaRef0; int alphaOp; int alphaComp1; unsigned char alphaRef1;
  int field6E8; unsigned int rasterModes;
};
SavedState s_stateStack[9];  // 804FD08C
int s_stackPos = 0;

struct SavedViewport {
  float viewport[6];
  int scissor[4];
};
SavedViewport s_viewportStack[5];  // 804FD1F4
int s_vpStackPos = 0;

const GXLightID _gxLightId[8] = {GX_LIGHT0, GX_LIGHT1, GX_LIGHT2, GX_LIGHT3,
                                 GX_LIGHT4, GX_LIGHT5, GX_LIGHT6, GX_LIGHT7};  // 80421FBC
// The environment-mapping matrix (m.1789): (x + y) * sqrt(1/2), z.
Mtx m_1789 = {{0.70710677f, 0.70710677f, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}};
}  // namespace

// 8034A818
// Copies the render state (not the matrices' derived copies).
ENgcRenderStateCache& ENgcRenderStateCache::operator=(ENgcRenderStateCache& o) {
  std::memcpy(this, &o, sizeof *this);
  return *this;
}

// 8034AB78
void ENgcRenderStateCache::SaveState() {
  SavedState& s = s_stateStack[s_stackPos++];
  s = {m_zEnable, m_zFunc, m_zUpdate, m_alphaComp0, m_alphaRef0, m_alphaOp, m_alphaComp1, m_alphaRef1,
       m_field6E8, m_rasterModes};
}

// 8034AC10
void ENgcRenderStateCache::RestoreState() {
  const SavedState& s = s_stateStack[--s_stackPos];
  m_zEnable = s.zEnable;
  m_zFunc = s.zFunc;
  m_zUpdate = s.zUpdate;
  m_alphaComp0 = s.alphaComp0;
  m_alphaRef0 = s.alphaRef0;
  m_alphaOp = s.alphaOp;
  m_alphaComp1 = s.alphaComp1;
  m_alphaRef1 = s.alphaRef1;
  m_field6E8 = s.field6E8;
  m_rasterModes = s.rasterModes;
  m_dirty |= 0x80000040;
}

// 8034ACB8
void ENgcRenderStateCache::SaveViewportState() {
  SavedViewport& s = s_viewportStack[s_vpStackPos++];
  std::memcpy(s.viewport, m_viewport, sizeof m_viewport);
  std::memcpy(s.scissor, m_scissor, sizeof m_scissor);
}

// 8034AD48
void ENgcRenderStateCache::RestoreViewportState() {
  const SavedViewport& s = s_viewportStack[--s_vpStackPos];
  std::memcpy(m_viewport, s.viewport, sizeof m_viewport);
  std::memcpy(m_scissor, s.scissor, sizeof m_scissor);
  m_dirty |= 0x80000600;
}

namespace {
// What GX was last sent of the groups only Apply sends (GC: a second ENgcRenderStateCache).
struct SentState {
  bool valid = false;
  unsigned int frame = 0;
  int vtxDesc[11] = {};
  int numTevStages = -1;
  ENgcTevStage tev[9] = {};
  GXColor amb{}, mat{};
  GXLightObj lights[8] = {};
  ENgcChanCtrl chan[4] = {};
  int numTexGens = -1;
  int texGen[8][6] = {};
  Mtx postTexMtx[2] = {}, texMtx[2] = {};
};
SentState s_sent;
bool RsCacheEnabled() {
  static const bool on = [] {
    const char* e = std::getenv("SIMMY_RS_CACHE");
    return !(e && e[0] == '0');
  }();
  return on;
}
}  // namespace

// 8034ADE8
void ENgcRenderStateCache::Apply(bool skinned) {
  if (!m_dirty) return;
  SentState& sent = s_sent;
  if (m_dirty == 0xffffffffu || !RsCacheEnabled() || sent.frame != static_cast<unsigned int>(_framecount)) sent.valid = false;
  sent.frame = static_cast<unsigned int>(_framecount);
  const bool diff = sent.valid;
  sent.valid = true;
  const int vtxDesc[11] = {m_vtxDescPnMtx, m_vtxDescPos, m_vtxDescNrm, m_vtxDescClr0, m_vtxDescTex[0], m_vtxDescTex[1],
                           m_vtxDescTex[2], m_vtxDescTex[3], m_vtxDescTex[4], m_vtxDescTex[5], 0};
  if (!diff || std::memcmp(vtxDesc, sent.vtxDesc, sizeof vtxDesc) != 0) {
    std::memcpy(sent.vtxDesc, vtxDesc, sizeof vtxDesc);

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_PNMTXIDX, static_cast<GXAttrType>(m_vtxDescPnMtx));
  GXSetVtxDesc(GX_VA_POS, static_cast<GXAttrType>(m_vtxDescPos));
  GXSetVtxDesc(GX_VA_NRM, static_cast<GXAttrType>(m_vtxDescNrm));
  GXSetVtxDesc(GX_VA_CLR0, static_cast<GXAttrType>(m_vtxDescClr0));
  for (int i = 0; i < 6; ++i) GXSetVtxDesc(static_cast<GXAttr>(GX_VA_TEX0 + i), static_cast<GXAttrType>(m_vtxDescTex[i]));
  }

  float vp[6] = {m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3], m_viewport[4], m_viewport[5]};
  int scissorW = m_scissor[2];
  if (static_cast<unsigned int>(m_scissor[0] + scissorW) > 0x27f) scissorW = 0x27f - m_scissor[0];
  int sc[4] = {m_scissor[0], m_scissor[1], scissorW, m_scissor[3]};
  // PC widescreen: 2D into the 4:3 centre of the stretched frame (platform.hpp).
  const f32(*proj)[4] = m_projection == 0 ? rectOrthoMtx : m_projection == 1 ? screenOrthoMtx : m_projection == 2 ? projMtx : nullptr;
  Mtx44 wideProj;
  const bool useWideProj = simmy::g_displayMap2D && proj && simmy::display_map_viewport(vp, sc, proj, wideProj);
  GXSetViewport(vp[0], vp[1], vp[2], vp[3], vp[4], vp[5]);
  GXSetScissor(sc[0], sc[1], sc[2], sc[3]);

  GXSetCullMode(static_cast<GXCullMode>(m_cullMode));

  if (m_rasterModes & 0x40)
    GXSetBlendMode(static_cast<GXBlendMode>(m_blendType), static_cast<GXBlendFactor>(m_blendSrc),
                   static_cast<GXBlendFactor>(m_blendDst), static_cast<GXLogicOp>(m_blendLogic));
  else
    GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
  GXSetZMode(m_zEnable, static_cast<GXCompare>(m_zFunc), m_zUpdate);
  GXSetZCompLoc(m_zCompBeforeTex);
  GXSetAlphaCompare(static_cast<GXCompare>(m_alphaComp0), m_alphaRef0, static_cast<GXAlphaOp>(m_alphaOp),
                    static_cast<GXCompare>(m_alphaComp1), m_alphaRef1);

  m_matrixMode = skinned;
  if (!m_matrixMode) {
    PSMTXConcat(m_viewMtx, m_modelMtx[0], m_posMtx);
    PSMTXInvXpose(m_posMtx, m_nrmMtx);
    GXLoadPosMtxImm(m_posMtx, GX_PNMTX4);
    GXLoadNrmMtxImm(m_nrmMtx, GX_PNMTX4);
    GXSetCurrentMtx(m_currentMtx);
  } else {
    for (int i = 0; i < 4; ++i) {
      Mtx pos, nrm;
      PSMTXConcat(m_viewMtx, m_modelMtx[i], pos);
      PSMTXInvXpose(pos, nrm);
      GXLoadPosMtxImm(pos, static_cast<u32>(i * 3));
      GXLoadNrmMtxImm(nrm, static_cast<u32>(i * 3));
    }
  }

  switch (m_projection) {
  case 0: GXSetProjection(rectOrthoMtx, GX_ORTHOGRAPHIC); break;
  case 1: GXSetProjection(screenOrthoMtx, GX_ORTHOGRAPHIC); break;
  case 2:
    GXSetProjection(projMtx, projMtx[3][2] != -1.0f && projMtx[3][2] == 0.0f ? GX_ORTHOGRAPHIC : GX_PERSPECTIVE);
    break;
  default: break;
  }
  if (useWideProj) GXSetProjection(wideProj, wideProj[3][2] == -1.0f ? GX_PERSPECTIVE : GX_ORTHOGRAPHIC);

  if (!diff || sent.numTevStages != m_numTevStages) {
    sent.numTevStages = m_numTevStages;
    GXSetNumTevStages(m_numTevStages);
  }
  for (int i = 0; i < 9; ++i) {
    const ENgcTevStage& t = m_tev[i];
    if (diff && std::memcmp(&t, &sent.tev[i], sizeof t) == 0) continue;
    sent.tev[i] = t;
    const auto stage = static_cast<GXTevStageID>(i);
    GXSetTevSwapMode(stage, static_cast<GXTevSwapSel>(t.swapRas), static_cast<GXTevSwapSel>(t.swapTex));
    GXSetTevOrder(stage, static_cast<GXTexCoordID>(t.texCoord), static_cast<GXTexMapID>(t.texMap),
                  static_cast<GXChannelID>(t.channel));
    GXSetTevColorIn(stage, static_cast<GXTevColorArg>(t.colorA), static_cast<GXTevColorArg>(t.colorB),
                    static_cast<GXTevColorArg>(t.colorC), static_cast<GXTevColorArg>(t.colorD));
    GXSetTevAlphaIn(stage, static_cast<GXTevAlphaArg>(t.alphaA), static_cast<GXTevAlphaArg>(t.alphaB),
                    static_cast<GXTevAlphaArg>(t.alphaC), static_cast<GXTevAlphaArg>(t.alphaD));
    GXSetTevColorOp(stage, static_cast<GXTevOp>(t.colorOp), static_cast<GXTevBias>(t.colorBias),
                    static_cast<GXTevScale>(t.colorScale), t.colorClamp, static_cast<GXTevRegID>(t.colorOut));
    GXSetTevAlphaOp(stage, static_cast<GXTevOp>(t.alphaOp), static_cast<GXTevBias>(t.alphaBias),
                    static_cast<GXTevScale>(t.alphaScale), t.alphaClamp, static_cast<GXTevRegID>(t.alphaOut));
  }

  if (!diff || std::memcmp(&sent.amb, &m_ambColor, sizeof m_ambColor) != 0) {
    sent.amb = m_ambColor;
    GXSetChanAmbColor(GX_COLOR0A0, m_ambColor);
  }
  if (!diff || std::memcmp(&sent.mat, &m_matColor, sizeof m_matColor) != 0) {
    sent.mat = m_matColor;
    GXSetChanMatColor(GX_COLOR0A0, m_matColor);
  }
  for (int i = 0; i < 8; ++i) {
    if (diff && std::memcmp(&sent.lights[i], &m_lights[i], sizeof m_lights[i]) == 0) continue;
    sent.lights[i] = m_lights[i];
    GXLoadLightObjImm(&m_lights[i], _gxLightId[i]);
  }
  for (int i = 0; i < 4; ++i) {
    const ENgcChanCtrl& c = m_chan[i];
    if (diff && std::memcmp(&sent.chan[i], &c, sizeof c) == 0) continue;
    sent.chan[i] = c;
    GXSetChanCtrl(static_cast<GXChannelID>(i), c.enable, static_cast<GXColorSrc>(c.ambSrc),
                  static_cast<GXColorSrc>(c.matSrc), static_cast<u32>(c.lightMask), static_cast<GXDiffuseFn>(c.diffFn),
                  static_cast<GXAttnFn>(c.attnFn));
  }

  if (!m_matrixMode) GXSetCurrentMtx(m_currentMtx);

  if (!diff || sent.numTexGens != m_numTexGens) {
    sent.numTexGens = m_numTexGens;
    GXSetNumTexGens(m_numTexGens);
  }
  for (int i = 0; i < m_numTexGens; ++i) {
    ENgcTexGen& g = m_texGen[i];
    // Gens that project positions (or, for the first two, normals) get GX_TEXMTX0 + 3i, built from
    // the model matrix (and the environment-mapping matrix when asked).
    if (g.src == GX_TG_POS || (g.src == GX_TG_NRM && i < 2)) {
      g.mtx = GX_TEXMTX0 + i * 3;
      if (!g.envMapped) PSMTXCopy(m_modelMtx[0], m_texMtx[i]);
      else PSMTXConcat(m_1789, m_modelMtx[0], m_texMtx[i]);
      m_dirty |= 0x80000008;
    }
    // PC: a gen reading a texture coordinate the vertices don't have reads garbage on GC but is fatal
    // in aurora's shader builder (CAS shaders set up gens from TEX0/TEX1 over meshes without them). Read
    // the position instead (what the stage then samples is undefined on GC too).
    int src = g.src;
    if (src >= GX_TG_TEX0 && src <= GX_TG_TEX5 && m_vtxDescTex[src - GX_TG_TEX0] == GX_NONE) src = GX_TG_POS;
    const int gen[6] = {g.dst, g.type, src, g.mtx, g.normalize, g.postMtx};
    if (diff && i < 8 && std::memcmp(gen, sent.texGen[i], sizeof gen) == 0) continue;
    if (i < 8) std::memcpy(sent.texGen[i], gen, sizeof gen);
    GXSetTexCoordGen2(static_cast<GXTexCoordID>(g.dst), static_cast<GXTexGenType>(g.type),
                      static_cast<GXTexGenSrc>(src), static_cast<u32>(g.mtx), g.normalize, static_cast<u32>(g.postMtx));
  }
  for (int i = 0; i < 2; ++i) {
    if (!diff || std::memcmp(sent.postTexMtx[i], m_postTexMtx[i], sizeof(Mtx)) != 0) {
      std::memcpy(sent.postTexMtx[i], m_postTexMtx[i], sizeof(Mtx));
      GXLoadTexMtxImm(m_postTexMtx[i], static_cast<u32>(GX_PTTEXMTX0 + i * 3), GX_MTX3x4);
    }
    if (!diff || std::memcmp(sent.texMtx[i], m_texMtx[i], sizeof(Mtx)) != 0) {
      std::memcpy(sent.texMtx[i], m_texMtx[i], sizeof(Mtx));
      GXLoadTexMtxImm(m_texMtx[i], static_cast<u32>(GX_TEXMTX0 + i * 3), GX_MTX3x4);
    }
  }

  ++_ngcStateChangesPerFrame;
  m_dirty = 0;
  m_tevDirty = 0;
  m_lightDirty = 0;
}
