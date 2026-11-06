// Port of engine/e_ngcrenderer.cpp, continued: renderer state setup and the display-list commands
// that change state (matrices, viewport, material, lights, shader, texture, blend/z/alpha modes),
// the TEV and lighting equations built from them, Execute's dispatch and Rect.
//
// Commands edit ENgcRenderStateCache; nothing reaches GX until a primitive calls Apply. Shader, texture,
// material and light changes set m_geometryDirty, and the next primitive runs GeometrySetup, which
// rebuilds the lighting channels (UpdateLightingEquation) and the TEV stages (UpdateTEVStages).
//
// TEV argument values used below: colour 0 CPREV, 1 APREV, 2 C0, 4 C1, 6 C2, 7 A2, 8 TEXC, 10 RASC,
// 12 ONE, 14 KONST, 15 ZERO; alpha 0 APREV, 3 A2, 4 TEXA, 5 RASA, 6 KONST, 7 ZERO.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstring>

extern int _ngcDrawStats;
extern int _ngcRenderEmpty;

int _ngcTexturesEnabled = 1;                    // 804FF2C4
int _ngcDraw2ndPass = 1;                        // 804FF2A4
int _ngcIgnoreTexGen = 0;                       // 804FF2B8
int _ngcDrawReflection = 1;                     // 804FF2B0
float _ngcAlphaTestThresholdMultiplier = 255.0f;  // 804FF2F0
float _gxLightMultiplier = 0.5f;                // 804FF2EC
int _ngcSkip2D = 0;                             // 804FF25C
int _ngcDraw2D = 1;                             // 804FF298

namespace {
constexpr GXColor kBlack{0, 0, 0, 0};          // _gxBlack
constexpr GXColor kWhite{255, 255, 255, 255};  // _gxWhite

// The engine's compare enums -> GXCompare (_gxZCompareMap.3491, _gxAlphaCompareMap.3495).
constexpr int kZCompareMap[8] = {GX_NEVER, GX_ALWAYS, GX_LEQUAL, GX_LESS, GX_EQUAL, GX_GEQUAL, GX_GREATER, GX_NEVER};
constexpr int kAlphaCompareMap[8] = {GX_NEVER, GX_ALWAYS, GX_LESS, GX_LEQUAL, GX_EQUAL, GX_GEQUAL, GX_GREATER, GX_NEQUAL};

// UpdateLightingEquation's tuning statics (all at their shipped values).
constexpr float kAmbientColorScale = 1.0f;          // _ngcAmbientColorScale.3298
constexpr bool kUseFixedAmbientColor = false;       // _ngcUseFixedAmbientColor.3303 (fixed colour 0.3 grey)
constexpr int kAttnMethod = 3;                      // _ngcAttnMethod.3305
constexpr float kAttn3K0 = 0.8f, kAttn3K1 = 0.0f, kAttn3K2 = 2.0f;  // _ngcAttnMethod3_K*
constexpr float kAttn3A0 = 1.0f, kAttn3A1 = 0.0f, kAttn3A2 = 0.0f;  // _ngcAttnMethod3_A*

GXColor ToGXColor(const float rgb[3], unsigned char alpha) {
  EVec3 v{rgb[0], rgb[1], rgb[2]};
  unsigned char c[3];
  v.ToU8s(c);
  return GXColor{c[0], c[1], c[2], alpha};
}

// GX keeps row-major 3x4 matrices with translation in the last column; EMat4 is its transpose.
void EMat4ToMtx(const EMat4& m, Mtx out) {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 4; ++c) out[r][c] = m.m[c][r];
}
}  // namespace

// ---- Initialisation ------------------------------------------------------------------------------

// 8033D868
void ENgcRenderer::InitData() {
  m_clearColor = GXColor{0, 0, 0, 0x7f};
  m_geometryDirty = 1;
  m_clearZ = 0xffffff;
  m_cursor = nullptr;
  m_numLayers = 0;
  m_material = EMaterial{};
  m_material.m_diffuse[3] = 0;
  m_material.m_ambient[3] = 0;
  m_alpha = 1.0f;
  m_numDirLights = 0;
  m_numPointLights = 0;
  std::memset(m_lights.m_ambient, 0, sizeof m_lights.m_ambient);
  for (ELightsDir& d : m_lights.m_dir) d = ELightsDir{};
  PSMTXIdentity(m_identity);
  PSMTXIdentity(m_modelMtx[0]);
  m_numTextures = 0;
  m_textures[0] = m_textures[1] = nullptr;

  m_rsCache = new ENgcRenderStateCache;
  ENgcRenderStateCache* c = m_rsCache;
  std::memset(static_cast<void*>(c), 0, sizeof *c);
  c->m_dirty = 0xffffffff;
  c->SetDirectVtxDesc();
  c->m_field6E8 = 0;
  c->m_rasterModes = 0;
  c->m_dirty |= 0x80000040;
  c->SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
  c->SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
  c->SetZCompLoc(true);
  c->SetAmbColor(kBlack);
  c->SetMatColor(kBlack);
  for (int i = 0; i < 4; ++i) c->SetChanCtrl(i, false, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_CLAMP, GX_AF_NONE);
  c->m_numTevStages = 1;
  c->m_dirty |= 0x80000080;
  for (int i = 0; i < 9; ++i) {
    c->SetTevSwapMode(i, GX_TEV_SWAP0, GX_TEV_SWAP0);
    c->SetTevOrder(i, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    c->SetTevColorIn(i, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    c->SetTevAlphaIn(i, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    c->SetTevColorOp(i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    c->SetTevAlphaOp(i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  }
  c->m_numTexGens = 1;
  for (int i = 0; i < 6; ++i)
    c->m_texGen[i] = {i, GX_TG_MTX2x4, GX_TG_TEX0 + i, GX_IDENTITY, 0, {}, GX_PTIDENTITY, 0};
  c->SetZMode(true, GX_LEQUAL, true);
  c->m_cullMode = GX_CULL_NONE;
  UpdateLightingEquation();
  UpdateTEVStages();
  const float viW = m_renderMode.viWidth, viH = m_renderMode.viHeight;
  float* vp = c->m_viewport;
  vp[0] = 0, vp[1] = 0, vp[2] = viW, vp[3] = viH, vp[4] = 0, vp[5] = 1;
  c->m_scissor[0] = 0, c->m_scissor[1] = 0;
  c->m_scissor[2] = m_renderMode.viWidth, c->m_scissor[3] = m_renderMode.viHeight;
  c->m_dirty |= 0x80000600;
  c->SetCurrentMtx(GX_PNMTX4);
  c->m_projection = 2;
  c->m_dirty = 0xffffffff;
  m_field465C = 0;
}

// 8033D1F0
// Vertex formats: 0 skinned/packed, 1 float (models), 2 the 2D primitives (Rect), 3/6/7/4 packed
// variants, 5 float position + uv.
void ENgcRenderer::InitGXVertexFormats() {
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  for (int a = GX_VA_TEX0; a <= GX_VA_TEX4; ++a)
    GXSetVtxAttrFmt(GX_VTXFMT1, static_cast<GXAttr>(a), GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  for (int a = GX_VA_TEX0; a <= GX_VA_TEX5; ++a)
    GXSetVtxAttrFmt(GX_VTXFMT2, static_cast<GXAttr>(a), GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_S16, 12);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX1, GX_TEX_ST, GX_S16, 12);
  GXSetVtxAttrFmt(GX_VTXFMT5, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT5, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT6, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GXSetVtxAttrFmt(GX_VTXFMT6, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT6, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT6, GX_VA_TEX0, GX_TEX_ST, GX_S16, 12);
  GXSetVtxAttrFmt(GX_VTXFMT6, GX_VA_TEX1, GX_TEX_ST, GX_S16, 12);
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX0, GX_TEX_ST, GX_F32, 12);
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_TEX0, GX_TEX_ST, GX_F32, 12);
  GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
}

// 8033CEB8
// GameCube: also set up the display copy (y scale, source, filter, gamma) and the break-point and
// draw-sync callbacks; aurora presents the EFB itself.
void ENgcRenderer::InitGX(GXRenderModeObj* rmode) {
  // GXInit sets up GX's register shadows (each one's register address) and the command FIFO. Without
  // it, register writes such as the TEV stages reach aurora with no address and are dropped.
  if (!simmy::platform_headless()) {
    alignas(32) static unsigned char s_fifo[0x10000];
    m_gxFifo = GXInit(s_fifo, sizeof s_fifo);
  }
  GXSetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
  GXSetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
  // GXSetZScaleOffset(1, 0): the default; aurora does not have it.
  // RGBA6: the destination alpha is the shadow and UI stencil (ERC::Stencil).
  GXSetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
  GXSetCullMode(GX_CULL_BACK);
  GXSetDither(GX_TRUE);
  GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
  GXLoadPosMtxImm(m_identity, GX_PNMTX5);
  GXLoadNrmMtxImm(m_identity, GX_PNMTX5);
  InitGXVertexFormats();
  GXSetNumChans(1);
  GXSetNumTevStages(1);
  GXLoadTexMtxImm(m_identity, GX_TEXMTX0, GX_MTX2x4);
  GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_ALPHA, GX_CH_ALPHA, GX_CH_ALPHA);
  GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
  C_MTXOrtho(ENgcRenderStateCache::rectOrthoMtx, 0, 1, 0, 1, 0, -1.001f);
  C_MTXOrtho(ENgcRenderStateCache::screenOrthoMtx, 0, m_renderMode.efbHeight, 0, m_renderMode.fbWidth, 0, -1.001f);
  PSMTXIdentity(ENgcRenderStateCache::projMtx);
  ENgcRenderStateCache* c = m_rsCache;
  PSMTXIdentity(c->m_modelMtx[0]);
  PSMTXIdentity(c->m_viewMtx);
  PSMTXIdentity(c->m_texMtx[0]);
  PSMTXIdentity(c->m_texMtx[1]);
  PSMTXIdentity(c->m_postTexMtx[0]);
  PSMTXIdentity(c->m_postTexMtx[1]);
  c->m_dirty |= 0x8000001e;
}

// 80349F3C
ENgcRenderStateCache* ENgcRenderer::GetRenderStateCache() { return m_rsCache; }

// ---- Lighting ------------------------------------------------------------------------------------

// 8033EDEC
// Channel COLOR0 and ALPHA0 set-up. Lit geometry (geometry mode 8): ambient = material ambient x
// ambient light; directional lights become GX lights placed far along -direction; point lights get
// distance attenuation k0 + k2 d^2/r^2. Unlit geometry takes vertex colours. Mode 0x4000 (pass-through)
// turns lighting off and uses a half-alpha black material.
void ENgcRenderer::UpdateLightingEquation() {
  ENgcRenderStateCache* c = m_rsCache;
  if (c->m_field6E8 & 0x4000) {
    c->SetChanCtrl(GX_COLOR0, false, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_CLAMP, GX_AF_NONE);
    c->SetChanCtrl(GX_ALPHA0, false, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_CLAMP, GX_AF_NONE);
    c->SetAmbColor(kBlack);
    c->SetMatColor(GXColor{0, 0, 0, 0x7f});
    c->m_rasterModes |= 0x40;
    return;
  }
  if (!(c->m_field6E8 & 8)) {
    c->SetChanCtrl(GX_COLOR0, false, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_CLAMP, GX_AF_NONE);
    c->SetChanCtrl(GX_ALPHA0, false, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_CLAMP, GX_AF_NONE);
    c->SetAmbColor(kWhite);
    c->SetMatColor(kWhite);
    return;
  }

  c->m_dirty |= 0x80000000;
  float amb[3];
  for (int i = 0; i < 3; ++i) amb[i] = m_material.m_ambient[i] * m_lights.m_ambient[i] * kAmbientColorScale;
  if (kUseFixedAmbientColor) amb[0] = amb[1] = amb[2] = 0.3f;
  c->SetAmbColor(ToGXColor(amb, 0xff));
  c->SetMatColor(kWhite);

  float dirColor[4][3], pointColor[4][3];
  for (int i = 0; i < m_numDirLights; ++i)
    for (int k = 0; k < 3; ++k) dirColor[i][k] = m_material.m_diffuse[k] * m_lights.m_dir[i].m_color[k];
  for (int i = 0; i < m_numPointLights; ++i)
    for (int k = 0; k < 3; ++k) pointColor[i][k] = m_material.m_diffuse[k] * m_lights.m_point[i].m_color[k];

  unsigned int mask = 0;
  int slot = 0;
  for (int i = 0; i < m_numDirLights; ++i) {
    Vec dir, pos;
    PSMTXMultVec(m_lightMtx, reinterpret_cast<const Vec*>(m_lights.m_dir[i].m_dir), &dir);
    if (PSVECMag(&dir) > 0.0f) PSVECNormalize(&dir, &dir);
    PSVECScale(&dir, &pos, -3.4028234e15f);
    GXLightObj light;
    GXInitLightColor(&light, ToGXColor(dirColor[i], 0xff));
    GXInitLightPos(&light, pos.x, pos.y, pos.z);
    GXInitLightDir(&light, dir.x, dir.y, dir.z);
    GXInitLightSpot(&light, 0.0f, GX_SP_OFF);
    GXInitLightDistAttn(&light, 0.0f, 0.0f, GX_DA_OFF);
    c->SetLight(slot, light);
    mask |= 1u << slot;
    ++slot;
  }
  for (int i = 0; i < m_numPointLights; ++i) {
    const ELightsPoint& p = m_lights.m_point[i];
    const float r = p.m_range;
    if (r < 0.01f) continue;
    Vec pos;
    PSMTXMultVec(c->m_viewMtx, reinterpret_cast<const Vec*>(p.m_pos), &pos);
    GXLightObj light;
    GXInitLightPos(&light, pos.x, pos.y, pos.z);
    GXInitLightDir(&light, 0, 0, 0);
    GXInitLightColor(&light, ToGXColor(pointColor[i], 0xff));
    GXInitLightAttnA(&light, 1.0f, 0.0f, 0.0f);
    static_assert(kAttnMethod == 3, "only the shipped attenuation method is ported");
    GXInitLightAttnA(&light, kAttn3A0, kAttn3A1, kAttn3A2);
    GXInitLightAttnK(&light, kAttn3K0, kAttn3K1 * (1.0f / r), kAttn3K2 * (1.0f / (r * r)));
    c->SetLight(slot, light);
    mask |= 1u << slot;
    ++slot;
  }
  c->SetChanCtrl(GX_COLOR0, true, GX_SRC_REG, GX_SRC_REG, mask, GX_DF_CLAMP, GX_AF_SPOT);
  c->SetChanCtrl(GX_ALPHA0, true, GX_SRC_REG, GX_SRC_REG, mask, GX_DF_CLAMP, GX_AF_SPOT);
}

// ---- TEV -----------------------------------------------------------------------------------------

// 8033F80C
// A stage that passes the rasterised (vertex/lit) colour through.
void ENgcRenderer::SetupTEVStagePassColor(GXTevStageID stage) {
  ENgcRenderStateCache* c = m_rsCache;
  c->SetTevOrder(stage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
  c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
  c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
}

// 8033F9DC
// One-stage texture: texture x rasterised colour, doubled (vertex colours are stored halved so they
// can over-brighten), or, for layers whose combine flag is set, the texture colour alone with
// texture alpha x constant alpha.
void ENgcRenderer::Setup1TEVStageTexture(int layer, int& stage, int texCoord, int& texMap) {
  ENgcRenderStateCache* c = m_rsCache;
  c->SetTevOrder(stage, texCoord, texMap, GX_COLOR0A0);
  if (!m_layers[layer].m_field21) {
    c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
    c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_RASA, GX_CA_TEXA, GX_CA_ZERO);
    c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, true, GX_TEVPREV);
  } else {
    c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
    c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_KONST, GX_CA_TEXA, GX_CA_ZERO);
    c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  }
  c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
  ++stage;
  ++texMap;
}

// 8033FD94
// Two-texture formats (C4_32/C8_32 keep a 16-bit palette split over two textures): stage 1 takes red
// and green from the first (swap table 1), stage 2 adds blue and alpha from the second (swap table 2),
// stage 3 applies the rasterised colour as in Setup1TEVStageTexture.
void ENgcRenderer::Setup2TEVStageTexture(int layer, int& stage, int texCoord, int& texMap) {
  ENgcRenderStateCache* c = m_rsCache;
  GXSetTevColor(GX_TEVREG0, GXColor{0xff, 0xff, 0, 0});
  GXSetTevColor(GX_TEVREG1, GXColor{0, 0, 0xff, 0xff});
  c->SetTevOrder(stage, texCoord, texMap, GX_COLOR_NULL);
  c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
  c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
  c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP1);
  ++stage;
  c->SetTevOrder(stage, texCoord, texMap + 1, GX_COLOR_NULL);
  c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C1, GX_CC_CPREV);
  c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevAlphaIn(stage, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
  c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP2);
  ++stage;
  c->SetTevOrder(stage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  if (!m_layers[layer].m_field21) {
    c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_RASC, GX_CC_CPREV, GX_CC_ZERO);
    c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_RASA, GX_CA_APREV, GX_CA_ZERO);
    c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, true, GX_TEVPREV);
  } else {
    c->SetTevColorIn(stage, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
    c->SetTevAlphaIn(stage, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  }
  c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
  ++stage;
  texMap += 2;
}

// 803404F0
// Builds the TEV program and the texture-coordinate generators for the current shader.
//
// Layer blend modes are PS2-style (A - B) * C + D codes, two bits each (A,B,D: 0 source, 1 dest,
// 2 zero; C: 0 source alpha, 1 dest alpha, 2 constant). Layer 0's code becomes the GX blend mode;
// layer 1's (second pass) is folded into the TEV, with layer 0's result kept in TEVREG2.
void ENgcRenderer::UpdateTEVStages() {
  ENgcRenderStateCache* c = m_rsCache;
  int stage = 0, texMap = 0, texCoord1 = 0;
  if (!m_textures[0] || !_ngcTexturesEnabled || (c->m_field6E8 & 0x4000)) {
    SetupTEVStagePassColor(GX_TEVSTAGE0);
    ++stage;
  } else {
    if (m_textures[0]->GetTEVStageCount() == 1) Setup1TEVStageTexture(0, stage, 0, texMap);
    else Setup2TEVStageTexture(0, stage, 0, texMap);
    texCoord1 = 1;
  }

  bool premultiply = false;  // layer 0 blends as ONE, INVSRCALPHA: scale its colour by its alpha
  bool keepAlpha0 = true;    // the combined alpha is layer 0's
  auto blendCode = [](const EShaderLayer& L) {
    return static_cast<unsigned int>(L.m_blend[0] | L.m_blend[1] << 2 | L.m_blend[2] << 4 | L.m_blend[3] << 6);
  };
  unsigned int blend1 = blendCode(m_layers[1]);
  unsigned int blend0 = blendCode(m_layers[0]);
  auto konstAlpha = [&](GXTevKColorID id, unsigned char alpha) { GXSetTevKColor(id, GXColor{0, 0, 0, alpha}); };

  if (!(c->m_field6E8 & 0x4000)) {
    if (!(c->m_rasterModes & 0x40)) {
      blend0 = 0;
    } else if (m_numLayers > 1 && (blend0 == 0x5a || blend0 == 0x4a || blend0 == 0x6a)) {
      // Layer 0 only supplies alpha for layer 1: drop its colour.
      if (blend1 == 0x58 || blend1 == 0x68 || blend1 == 0x54) {
        c->SetTevColorIn(stage - 1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
        blend0 = blend1 == 0x54 ? 0x44 : 0x100;
      }
    }
    if (m_alpha < 1.0f && (blend0 == 0 || blend0 == 10 || blend0 == 0x1a || blend0 == 0x2a)) {
      blend0 = 0x44;  // fading opaque geometry: switch to alpha blending
      c->m_rasterModes |= 0x40;
    }
    bool blendOn = true;
    switch (blend0) {
    case 0: case 10: case 0x1a: case 0x2a:  // Cs: opaque
      c->SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
      blendOn = false;
      break;
    case 0x44:  // (Cs - Cd) As + Cd
      if (m_numLayers < 2 || m_alpha != 1.0f) {
        c->SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
      } else {
        c->SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        premultiply = true;
      }
      break;
    case 0x48:  // Cs As + Cd
      if (m_numLayers > 1 && m_alpha == 1.0f) {
        keepAlpha0 = false;
        c->SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        premultiply = true;
      } else {
        c->SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
      }
      break;
    case 1:  // (Cd - Cs) As + Cs
      c->SetBlendMode(GX_BM_BLEND, GX_BL_INVSRCALPHA, GX_BL_SRCALPHA, GX_LO_CLEAR);
      break;
    case 100:  // (Cs - Cd) Fix + Cd
      konstAlpha(GX_KCOLOR0, m_layers[0].m_alpha);
      GXSetTevKAlphaSel(static_cast<GXTevStageID>(stage - 1), GX_TEV_KASEL_K0_A);
      c->SetTevAlphaIn(stage - 1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
      c->SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
      break;
    case 0x58:  // Cs Ad + Cd
      c->SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_ONE, GX_LO_CLEAR);
      break;
    case 0x54:  // (Cs - Cd) Ad + Cd
      c->SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_INVDSTALPHA, GX_LO_CLEAR);
      break;
    case 0x68:  // Cs Fix + Cd
      konstAlpha(GX_KCOLOR0, m_layers[0].m_alpha);
      GXSetTevKAlphaSel(static_cast<GXTevStageID>(stage - 1), GX_TEV_KASEL_K0_A);
      c->SetTevAlphaIn(stage - 1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
      c->SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
      break;
    case 0x100:  // additive
      c->SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
      break;
    default:  // 0x4a (Cd) and anything unrecognised: leave the destination
      c->SetBlendMode(GX_BM_BLEND, GX_BL_ZERO, GX_BL_ONE, GX_LO_CLEAR);
      blendOn = false;
      break;
    }
    if (blendOn) c->m_rasterModes |= 0x40;
  }

  if (m_numLayers >= 2 && !(c->m_field6E8 & 0x4000) && _ngcDraw2ndPass) {
    if (premultiply) {
      c->SetTevOrder(stage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
      c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
      c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
      c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_CPREV, GX_CC_APREV, GX_CC_ZERO);
      c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
      c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
      ++stage;
    }
    // Layer 0's result goes to TEVREG2.
    ENgcTevStage& last = c->Tev(stage - 1);
    last.colorOut = GX_TEVREG2;
    last.alphaOut = GX_TEVREG2;

    if (!m_textures[1]) {
      SetupTEVStagePassColor(static_cast<GXTevStageID>(stage));
      ++stage;
    } else if (m_textures[1]->GetTEVStageCount() == 1) {
      Setup1TEVStageTexture(1, stage, texCoord1, texMap);
    } else {
      Setup2TEVStageTexture(1, stage, texCoord1, texMap);
    }

    // Combine stage: CPREV/APREV is layer 1, C2/A2 is layer 0.
    c->SetTevOrder(stage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
    c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    if (keepAlpha0) c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A2);
    else c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
    if (!(m_layers[1].m_field0C & 0x40)) blend1 = 0;
    auto konstColor1 = [&] {
      konstAlpha(GX_KCOLOR1, m_layers[1].m_alpha);
      GXSetTevKColorSel(static_cast<GXTevStageID>(stage), GX_TEV_KCSEL_K1_A);
    };
    switch (blend1) {
    case 0x44: c->SetTevColorIn(stage, GX_CC_C2, GX_CC_CPREV, GX_CC_APREV, GX_CC_ZERO); break;
    case 0x48: c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_CPREV, GX_CC_APREV, GX_CC_C2); break;
    case 0x54: c->SetTevColorIn(stage, GX_CC_C2, GX_CC_CPREV, GX_CC_A2, GX_CC_ZERO); break;
    case 0x58: c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_CPREV, GX_CC_A2, GX_CC_C2); break;
    case 100: konstColor1(); c->SetTevColorIn(stage, GX_CC_C2, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO); break;
    case 0x68: konstColor1(); c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_CPREV, GX_CC_KONST, GX_CC_C2); break;
    case 0x21: konstColor1(); c->SetTevColorIn(stage, GX_CC_CPREV, GX_CC_C2, GX_CC_KONST, GX_CC_ZERO); break;
    case 0: case 10: case 0x1a: case 0x2a:
      c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_ONE, GX_CC_CPREV, GX_CC_ZERO);
      break;
    default: break;
    }
    ++stage;
  }

  // Global alpha (SetAlpha) and the shader colour slot tint the result through KCOLOR3.
  if (m_alpha < 1.0f || m_colorOverride != 0xffffffff) {
    const unsigned int o = m_colorOverride;
    GXSetTevKColor(GX_KCOLOR3, GXColor{static_cast<u8>(o >> 16), static_cast<u8>(o >> 8), static_cast<u8>(o),
                                       static_cast<u8>(static_cast<int>(m_alpha * 255.0f))});
    GXSetTevKColorSel(static_cast<GXTevStageID>(stage), GX_TEV_KCSEL_K3);
    GXSetTevKAlphaSel(static_cast<GXTevStageID>(stage), GX_TEV_KASEL_K3_A);
    c->SetTevOrder(stage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    c->SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    c->SetTevColorIn(stage, GX_CC_ZERO, GX_CC_KONST, GX_CC_CPREV, GX_CC_ZERO);
    c->SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    c->SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_APREV, GX_CA_KONST, GX_CA_ZERO);
    c->SetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
    ++stage;
  }
  c->m_numTevStages = static_cast<unsigned char>(stage);
  c->m_dirty |= 0x80000080;

  // Texture coordinate generation, one per layer.
  m_usesSecondUV = 0;
  m_reflection = 0;
  if (!(c->m_field6E8 & 0x4000)) {
    c->m_numTexGens = 0;
    for (int i = 0; i < m_numLayers; ++i) {
      const int n = c->m_numTexGens;
      ENgcTexGen& g = c->m_texGen[n];
      const EShaderLayer& L = m_layers[i];
      auto withPostMatrix = [&](int src, int envMapped) {
        c->m_field6E8 |= 0x80 << i;
        g = {n, GX_TG_MTX2x4, src, GX_IDENTITY, 0, {}, GX_PTTEXMTX0 + 3 * i, envMapped};
        EMat4ToMtx(m_layerTexMtx[i], c->m_postTexMtx[i]);
        c->m_dirty |= 0x80000010;
      };
      switch (L.m_texGen) {
      case 4: withPostMatrix(GX_TG_POS, L.m_field23 == 1); break;  // projected from position
      case 5: withPostMatrix(GX_TG_TEX0, 0); break;                // scrolled/scaled uv0
      case 6: withPostMatrix(GX_TG_TEX1, 0); m_usesSecondUV = 1; break;
      case 3:  // reflection
        g = {n, GX_TG_MTX2x4, GX_TG_TEX0 + n, GX_IDENTITY, 0, {}, GX_PTIDENTITY, g.envMapped};
        m_reflection = _ngcDrawReflection;
        break;
      case 0: g = {n, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, 0, {}, GX_PTIDENTITY, g.envMapped}; break;
      case 1:
        g = {n, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY, 0, {}, GX_PTIDENTITY, g.envMapped};
        m_usesSecondUV = 1;
        break;
      default: g = {n, GX_TG_MTX2x4, GX_TG_TEX0 + n, GX_IDENTITY, 0, {}, GX_PTIDENTITY, g.envMapped}; break;
      }
      if (_ngcIgnoreTexGen) g = {n, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, 0, {}, GX_PTIDENTITY, g.envMapped};
      ++c->m_numTexGens;
    }
  }
  c->m_dirty |= 0x80000000;
}

// 803451E4
void ENgcRenderer::GeometrySetup(EDLEntry*) {
  UpdateLightingEquation();
  UpdateTEVStages();
  m_geometryDirty = 0;
}

// ---- Shader, texture, material, lights -----------------------------------------------------------

// 803489F8
void ENgcRenderer::SetShader(ENgcShader* shader, unsigned int colorIndex) {
  if (shader->m_magic != 0x900dbeef) return;
  ENgcRenderStateCache* c = m_rsCache;
  if (shader->m_geometryModes & 8) {
    std::memcpy(&m_material, shader->m_material, sizeof m_material);
    c->m_field6E8 |= 8;
  } else {
    c->m_field6E8 &= ~8;
  }
  m_numLayers = shader->m_layerCount;
  for (int i = 0; i < m_numLayers && i < 2; ++i) m_layers[i] = shader->m_layers[i];
  c->m_rasterModes = shader->m_layers[0].m_field0C;
  c->m_dirty |= 0x80000040;

  const unsigned int flags = shader->m_layers[0].m_flags;
  c->SetZMode((flags & 8) != 0, GX_LEQUAL, (flags & 0x10) != 0);
  if (!(flags & 4)) {
    c->SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    c->SetZCompLoc(true);
  } else {
    const float threshold = shader->m_layers[0].m_field14;
    unsigned char ref = static_cast<unsigned char>(static_cast<int>(threshold * _ngcAlphaTestThresholdMultiplier));
    if (threshold > 0.0f && ref == 0) ref = 1;
    c->SetAlphaCompare(GX_GEQUAL, ref, GX_AOP_AND, GX_ALWAYS, 0);
    c->SetZCompLoc(false);
  }

  for (int i = 0; i < 2; ++i) {
    const EShaderLayer& L = shader->m_layers[i];
    EMat4& m = m_layerTexMtx[i];
    if (L.m_flags & 0x40) {
      EVec3 t{shader->m_update ? shader->m_update->scroll[i][0] : 0.0f,
              shader->m_update ? shader->m_update->scroll[i][1] : 0.0f, 0.0f};
      m.Translate(t);
    } else {
      m.Id();
    }
    EVec3 offset{L.m_texOffset[0], L.m_texOffset[1], 0.0f};
    m.PreTranslate(offset);
    EVec3 scale{L.m_texScale[0], L.m_texScale[1], 1.0f};
    m.PreScale(scale);
  }
  if (colorIndex == 0xffffffffu || !((shader->m_layers[0].m_flags | shader->m_layers[1].m_flags) & 0x20) || colorIndex >= 9)
    m_colorOverride = 0xffffffff;
  else
    m_colorOverride = static_cast<unsigned int>(shader->m_field40[colorIndex]);
  m_geometryDirty = 1;
}

// 803489D0
void ENgcRenderer::Shader(EDLEntry* e) { SetShader(static_cast<ENgcShader*>(e->p), 0xffffffff); }

// 8034447C
// Texture stage `stage` (0 or 1): selects it into its GX texture map(s), after the maps the textures
// below it use.
void ENgcRenderer::SetTexture(ENgcTexture* texture, int stage) {
  ENgcTexture*& slot = m_textures[stage];
  if (slot) slot->Unlock();
  if (!texture) {
    if (slot && m_numTextures) --m_numTextures;
  } else if (!slot) {
    ++m_numTextures;
  }
  slot = texture;
  if (texture) {
    int texMap = 0;
    for (int i = 0; i < stage; ++i)
      if (m_textures[i]) texMap += m_textures[i]->GetTEXCount();
    texture->Lock();
    texture->Select(stage, texMap);
  }
}

// 80344440
void ENgcRenderer::Texture(EDLEntry* e) {
  SetTexture(static_cast<ENgcTexture*>(e->p), e->b1);
  m_geometryDirty = 1;
}

// 80345120
void ENgcRenderer::Material(EDLEntry* e) {
  m_material = *static_cast<EMaterial*>(e->p);
  m_geometryDirty = 1;
}

// 80344724
void ENgcRenderer::Lights(EDLEntry* e) {
  m_numDirLights = e->b1;
  m_numPointLights = e->b2;
  auto* src = static_cast<ELights*>(e->p);
  if (!src) {
    m_lights.m_ambient[0] = m_lights.m_ambient[1] = m_lights.m_ambient[2] = 0;
  } else {
    std::memcpy(&m_lights, src, 0xF8);
    const float k = _gxLightMultiplier;
    for (int i = 0; i < m_numDirLights; ++i)
      for (float& v : m_lights.m_dir[i].m_color) v *= k;
    for (int i = 0; i < m_numPointLights; ++i)
      for (float& v : m_lights.m_point[i].m_color) v *= k;
    for (int i = 0; i < 3; ++i) m_lights.m_ambient[i] *= k;
  }
  m_geometryDirty = 1;
}

// 80344714
void ENgcRenderer::SetAlpha(float alpha) {
  m_alpha = alpha;
  m_geometryDirty = 1;
}

// 8034A7E8
void ENgcRenderer::SetAlpha(EDLEntry*) {}  // unused on the GameCube: SetAlpha is immediate-only

// 80346124
void ENgcRenderer::SetBlendMode(EDLEntry* e) {
  const unsigned int v = e->u;
  m_geometryDirty = 1;
  EShaderLayer& L = m_layers[0];
  L.m_blend[1] = (v >> 2) & 3;
  L.m_blend[0] = v & 3;
  L.m_blend[2] = (v >> 4) & 3;
  L.m_blend[3] = (v >> 6) & 3;
  L.m_alpha = static_cast<unsigned char>(v >> 8);
}

// 80346104
void ENgcRenderer::SetCombineMode(EDLEntry* e) {
  const int layer = e->b2 << 8 | e->b3;
  m_layers[layer & 1].m_field21 = static_cast<unsigned char>(e->u);
  m_geometryDirty = 1;
}

// ---- Matrices and viewport -----------------------------------------------------------------------

// 8034405C
void ENgcRenderer::ModelMatrices(EMat4* matrices, int count) {
  const unsigned int n = static_cast<unsigned int>(count) & 0x3ffffff;
  m_numModelMatrices = static_cast<unsigned int>(count);
  if (matrices && n) std::memcpy(m_modelMatrices, matrices, (n < 248 ? n : 248) * sizeof(EMat4));
  m_geometryDirty = 1;
}

// 80344034
void ENgcRenderer::ModelMatrices(EDLEntry* e) { ModelMatrices(static_cast<EMat4*>(e->p), e->b1); }

// 803440AC
// Model matrix slot `slot` (0..3, for skinning) = matrix `index` of the list; 0xfe/0xff = identity.
void ENgcRenderer::ModelMatrixIndex(int slot, int index) {
  if (index >= 0xfe && index < 0x100) {
    PSMTXIdentity(m_modelMtx[slot]);
  } else {
    EMat4ToMtx(m_modelMatrices[index], m_modelMtx[slot]);
  }
  PSMTXCopy(m_modelMtx[slot], m_rsCache->m_modelMtx[slot]);
  m_rsCache->m_dirty |= 0x80000002;
  m_geometryDirty = 1;
}

// 803441C0
void ENgcRenderer::ModelMatrixIndices(EDLEntry* e) {
  for (int i = 0; i < 4; ++i) ModelMatrixIndex(i, e->bytes[i]);
}

// 8034420C
void ENgcRenderer::ViewMatrix(EDLEntry* e) {
  ENgcRenderStateCache* c = m_rsCache;
  EMat4ToMtx(*static_cast<EMat4*>(e->p), c->m_viewMtx);
  PSMTXInvXpose(c->m_viewMtx, m_lightMtx);
  c->m_dirty |= 0x80000004;
  m_geometryDirty = 1;
}

// 803442D8
void ENgcRenderer::ProjectionMatrix(EDLEntry* e) {
  const EMat4& m = *static_cast<EMat4*>(e->p);
  for (int r = 0; r < 4; ++r)
    for (int col = 0; col < 4; ++col) ENgcRenderStateCache::projMtx[r][col] = m.m[col][r];
  m_rsCache->SetProjection(2);
  m_geometryDirty = 1;
}

// 8034439C
void ENgcRenderer::WindowMatrix(EDLEntry* e) { m_windowMtx = *static_cast<EMat4*>(e->p); }

// 80343DDC
void ENgcRenderer::Viewport(EDLEntry* e) {
  const auto* vp = static_cast<EViewport*>(e->p);
  float* v = m_rsCache->m_viewport;
  const float hw = vp->m_scale[0], hh = vp->m_scale[1], cx = vp->m_center[0], cy = vp->m_center[1];
  v[0] = cx - hw;
  v[1] = hh + cy;
  v[2] = (hw + cx) - v[0];
  v[3] = (cy - hh) - v[1];
  v[4] = 0.0f;
  v[5] = 1.0f;
  m_rsCache->m_dirty |= 0x80000200;
}

// 80343EA0
void ENgcRenderer::Scissor(EDLEntry* e) {
  const auto* r = static_cast<const float*>(e->p);
  const float w = (r[2] - r[0]) + 2.0f, h = (r[3] - r[1]) + 2.0f;
  int* s = m_rsCache->m_scissor;
  s[0] = static_cast<int>(static_cast<unsigned int>(r[0]));
  s[1] = static_cast<int>(static_cast<unsigned int>(r[1]));
  s[2] = static_cast<int>(static_cast<unsigned int>(w));
  s[3] = static_cast<int>(static_cast<unsigned int>(h));
  if (w <= 0.0f || h <= 0.0f) s[2] = s[3] = 0;
  m_rsCache->m_dirty |= 0x80000400;
}

// 80343E7C
void ENgcRenderer::ClipRect(EDLEntry*) { m_cursor += EDLEntry::EntriesFor<EDLClipRectData>() - 1; }

// ---- Modes ---------------------------------------------------------------------------------------

namespace {
// Geometry modes 4/2 pick the cull mode (GX culls the opposite winding to the engine's convention).
int CullFromModes(unsigned int modes, bool fromEnable) {
  if (modes & 4) return fromEnable ? GX_CULL_FRONT : GX_CULL_BACK;
  if (modes & 2) return fromEnable ? GX_CULL_BACK : GX_CULL_FRONT;
  return GX_CULL_NONE;
}
}  // namespace

// 80344598
void ENgcRenderer::EnableGeometryModes(EDLEntry* e) {
  ENgcRenderStateCache* c = m_rsCache;
  c->m_dirty |= 0x80000000;
  c->m_field6E8 |= e->u;
  c->m_cullMode = CullFromModes(c->m_field6E8, true);
  m_geometryDirty = 1;
}
// 803445F0
void ENgcRenderer::DisableGeometryModes(EDLEntry* e) {
  ENgcRenderStateCache* c = m_rsCache;
  c->m_dirty |= 0x80000000;
  c->m_field6E8 &= e->u;
  c->m_cullMode = CullFromModes(c->m_field6E8, false);
  m_geometryDirty = 1;
}
// 80344648
void ENgcRenderer::SetGeometryModes(EDLEntry* e) {
  ENgcRenderStateCache* c = m_rsCache;
  c->m_dirty |= 0x80000000;
  c->m_field6E8 = static_cast<int>(e->u);
  c->m_cullMode = CullFromModes(c->m_field6E8, false);
  m_geometryDirty = 1;
}
// 80344698
void ENgcRenderer::EnableRasterModes(EDLEntry* e) {
  m_rsCache->m_rasterModes |= e->u;
  m_rsCache->m_dirty |= 0x80000040;
}
// 803446C4
void ENgcRenderer::DisableRasterModes(EDLEntry* e) {
  m_rsCache->m_rasterModes &= e->u;
  m_rsCache->m_dirty |= 0x80000040;
}
// 803446F0
void ENgcRenderer::SetRasterModes(EDLEntry* e) {
  m_rsCache->m_rasterModes = e->u;
  m_rsCache->m_dirty |= 0x80000040;
}

// 80345220
void ENgcRenderer::ZTest(EDLEntry* e) {
  m_zTest[0] = e->b1;
  m_zTest[1] = e->b2;
  m_zTest[2] = e->b3;
  m_rsCache->SetZMode(e->b1 != 0, kZCompareMap[e->b2 & 7], e->b3 == 0);
}

// 80345284
void ENgcRenderer::AlphaTest(EDLEntry* e) {
  ENgcRenderStateCache* c = m_rsCache;
  if (e->b1) {
    c->SetZCompLoc(false);
    c->SetAlphaCompare(kAlphaCompareMap[e->b2 & 7], e->b3, GX_AOP_AND, GX_ALWAYS, 0);
  } else {
    c->SetZCompLoc(true);
    c->SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
  }
}

// 80343D94
void ENgcRenderer::SaveState(EDLEntry*) { m_rsCache->SaveState(); }
// 80343DB8
void ENgcRenderer::RestoreState(EDLEntry*) { m_rsCache->RestoreState(); }

// ---- Display-list control ------------------------------------------------------------------------

// 80343D24
void ENgcRenderer::DisplayList(EDLEntry* e) {
  m_returnStack[m_returnDepth++] = m_cursor;
  m_currentDL = static_cast<EDL*>(e->p);
  m_cursor = m_currentDL->m_start;
}
// 80343D54
void ENgcRenderer::Goto(EDLEntry* e) { m_cursor = static_cast<EDLEntry*>(e->p); }
// 80343D60
void ENgcRenderer::End(EDLEntry*) {
  if (m_returnDepth != 0) m_cursor = m_returnStack[--m_returnDepth];
  else m_done = 1;
}
// 8034A778
void ENgcRenderer::NoOp(EDLEntry*) {}

// 80341E28
void ENgcRenderer::Execute(EDLEntry* start) {
  using Handler = void (*)(ENgcRenderer*, EDLEntry*);
#define H(name) [](ENgcRenderer* r, EDLEntry* e) { r->name(e); }
  static const Handler kTable[kDL_OpCount] = {
      H(TriStrip), H(TriFan), H(TriList), H(QuadList), H(PointList), H(SpriteList), H(DisplayList), H(Goto),
      H(End), H(Viewport), H(NoOp), H(Scissor), H(ModelMatrices), H(ViewMatrix), H(ProjectionMatrix),
      H(WindowMatrix), H(TextureMatrix), H(Texture), H(EnableGeometryModes), H(DisableGeometryModes),
      H(SetGeometryModes), H(EnableRasterModes), H(DisableRasterModes), H(SetRasterModes), H(Lights),
      H(SendGSDisplayList), H(LineList), H(LineStrip), H(SaveState), H(RestoreState), H(CallbackParam),
      H(Callback), H(GEList), H(Rect), H(ModelMatrixIndices), H(Material), H(MipMapSetup), H(RecalcMatrices),
      H(Debug), H(SetMipMap), H(GeometrySetup), H(DirectRect), H(TriStripPacked), H(ZTest), H(AlphaTest),
      H(RenderSurface), H(Vertex), H(TriIndexed), H(SaveImageData), H(PointListPacked), H(SpriteListPacked),
      H(SetCombineMode), H(SetBlendMode), H(NoOp), H(ClipRect), H(MovieFrame), H(TriStripPackedInt),
      H(VerifyMpg), H(RectList), H(RectListRot), H(ParticleList), H(ParticleListRot), H(LineStreakList),
      H(SetAlpha), H(CopyScreenToTexture), H(Stencil), H(NgcTexInvalidateAll), H(NgcGXDisplayListInt),
      H(NgcGXDisplayList), H(NgcTriListInt), H(NgcTriList), H(NgcScreenTriList), H(ReadMetrics), H(Shader),
      H(ScrambleRect),
  };
#undef H
  m_cursor = start;
  m_done = 0;
  do {
    EDLEntry* e = m_cursor;
    ++m_cursor;
    if (e->op < kDL_OpCount) kTable[e->op](this, e);
    else m_done = 1;
  } while (!m_done);
}

// ---- 2D ------------------------------------------------------------------------------------------

// 803448B4
// A screen-space quad in normalised (0..1) coordinates, drawn with vertex format 2 through the
// identity matrix in slot GX_PNMTX5 and the 0..1 orthographic projection. Vertex colours are halved
// because the texture stage doubles them.
void ENgcRenderer::Rect(EDLEntry* e) {
  if (_ngcSkip2D) return;
  _ngcRenderEmpty = 0;
  if (_ngcDraw2D || _ngcDrawStats) {
    const EDLRectData& d = *e->Payload<EDLRectData>();
    EVec4 color{d.color[0], d.color[1], d.color[2], d.color[3]};
    unsigned char rgba[4];
    color.ToU8s(rgba);
    rgba[0] >>= 1, rgba[1] >>= 1, rgba[2] >>= 1;
    ENgcRenderStateCache* c = m_rsCache;
    SaveState(nullptr);
    c->m_field6E8 &= ~8;
    c->SaveViewportState();
    c->m_viewport[0] += 0.5f;
    c->m_viewport[1] += 0.5f;
    c->m_dirty |= 0x80000200;
    if (m_geometryDirty) GeometrySetup(nullptr);
    c->m_projection = 0;
    c->SetCurrentMtx(GX_PNMTX5);
    c->m_dirty |= 0x80000020;
    c->SetDirectVtxDesc();
    if (CanDraw()) {
      c->Apply(false);
      GXBegin(GX_QUADS, GX_VTXFMT2, 4);
      const float corner[4][4] = {{d.x0, d.y0, d.u0, d.v0}, {d.x0, d.y1, d.u0, d.v1},
                                  {d.x1, d.y1, d.u1, d.v1}, {d.x1, d.y0, d.u1, d.v0}};
      for (const auto& v : corner) {
        GXPosition3f32(v[0], v[1], d.z);
        GXNormal3s8(0, 0, -1);
        GXColor4u8(rgba[0], rgba[1], rgba[2], rgba[3]);
        GXTexCoord2f32(v[2], v[3]);
      }
      GXEnd();
    }
    c->RestoreViewportState();
    RestoreState(nullptr);
    c->m_projection = 2;
    c->SetCurrentMtx(GX_PNMTX4);
    c->m_dirty |= 0x80000020;
  }
  m_cursor += EDLEntry::EntriesFor<EDLRectData>() - 1;
}

namespace {
// The state RectList and RectListRot draw with: like Rect's, but the vertices have no normal.
void BeginRectList(ENgcRenderer* r, ENgcRenderStateCache* c, unsigned int mtx) {
  r->SaveState(nullptr);
  c->m_field6E8 &= ~8;
  if (r->m_geometryDirty) r->GeometrySetup(nullptr);
  c->SaveViewportState();
  c->m_viewport[0] += 0.5f;
  c->m_viewport[1] += 0.5f;
  c->m_dirty |= 0x80000200;
  c->m_projection = 0;
  c->SetCurrentMtx(mtx);
  c->m_dirty |= 0x80000020;
  c->SetDirectVtxDesc();
  c->m_vtxDescNrm = 0;
}
void EndRectList(ENgcRenderer* r, ENgcRenderStateCache* c) {
  c->RestoreViewportState();
  r->RestoreState(nullptr);
  c->m_projection = 2;
  c->SetCurrentMtx(GX_PNMTX4);
  c->m_dirty |= 0x80000020;
}
// One rectangle (x0, y0, x1, y1, u0, v0, u1, v1) as a quad of vertex format 2 without normals.
void RectListQuad(const float* r, float z, const unsigned char rgba[4]) {
  const float corner[4][4] = {{r[0], r[1], r[4], r[5]}, {r[0], r[3], r[4], r[7]},
                              {r[2], r[3], r[6], r[7]}, {r[2], r[1], r[6], r[5]}};
  for (const auto& v : corner) {
    GXPosition3f32(v[0], v[1], z);
    GXColor4u8(rgba[0], rgba[1], rgba[2], rgba[3]);
    GXTexCoord2f32(v[2], v[3]);
  }
}
}  // namespace

// 803472C0
// Screen-space rectangles in normalised (0..1) coordinates (text glyphs), one GX_QUADS batch through
// the identity matrix in GX_PNMTX5. Vertex colours are halved as in Rect.
void ENgcRenderer::RectList(EDLEntry* e) {
  _ngcRenderEmpty = 0;
  if (_ngcDraw2D || _ngcDrawStats) {
    const EDLRectListData& d = *e->Payload<EDLRectListData>();
    EVec4 color{d.color[0], d.color[1], d.color[2], d.color[3]};
    unsigned char rgba[4];
    color.ToU8s(rgba);
    rgba[0] >>= 1, rgba[1] >>= 1, rgba[2] >>= 1;
    const int count = static_cast<int>(e->u);
    const float* rects = static_cast<const float*>(e->p);
    ENgcRenderStateCache* c = m_rsCache;
    BeginRectList(this, c, GX_PNMTX5);
    if (CanDraw()) {
      c->Apply(false);
      GXBegin(GX_QUADS, GX_VTXFMT2, static_cast<u16>((count & 0x3fff) << 2));
      for (int i = 0; i < count; ++i) RectListQuad(rects + i * 8, d.z, rgba);
      GXEnd();
    }
    EndRectList(this, c);
  }
  m_cursor += EDLEntry::EntriesFor<EDLRectListData>() - 1;
}

// 80346E1C
// As RectList, but each rectangle carries a 2D transform (applied about its centre) loaded into
// GX_PNMTX3 and is drawn as its own quad.
void ENgcRenderer::RectListRot(EDLEntry* e) {
  _ngcRenderEmpty = 0;
  if (_ngcDraw2D || _ngcDrawStats) {
    const EDLRectListData& d = *e->Payload<EDLRectListData>();
    EVec4 color{d.color[0], d.color[1], d.color[2], d.color[3]};
    unsigned char rgba[4];
    color.ToU8s(rgba);
    rgba[0] >>= 1, rgba[1] >>= 1, rgba[2] >>= 1;
    const int count = static_cast<int>(e->u);
    const float* r = static_cast<const float*>(e->p);
    ENgcRenderStateCache* c = m_rsCache;
    BeginRectList(this, c, GX_PNMTX3);
    if (CanDraw()) {
      c->Apply(false);
      for (int i = 0; i < count; ++i, r += 16) {
        EVec3 centre{(r[0] + r[2]) * 0.5f, (r[1] + r[3]) * 0.5f, 0.0f};
        EMat4 m;
        m.Id();
        m.m[0][0] = r[8], m.m[0][1] = r[9], m.m[3][0] = r[10];
        m.m[1][0] = r[12], m.m[1][1] = r[13], m.m[3][1] = r[14];
        EVec3 back{-centre.x, -centre.y, 0.0f};
        m.PreTranslate(back);
        m.PostTranslate(centre);
        Mtx mtx;
        EMat4ToMtx(m, mtx);
        GXLoadPosMtxImm(mtx, GX_PNMTX3);
        GXBegin(GX_QUADS, GX_VTXFMT2, 4);
        RectListQuad(r, d.z, rgba);
        GXEnd();
      }
    }
    EndRectList(this, c);
  }
  m_cursor += EDLEntry::EntriesFor<EDLRectListData>() - 1;
}

// 8034615C
// A movie frame: the THP YUV->RGB conversion as four TEV stages over the Y (map 0), U (map 1) and V
// (map 2) I8 textures, drawn by the movie in screen pixels (vertex format 0, GX_PNMTX5 = identity,
// screen orthographic projection) with z and blending off. The constants are the THP SDK's
// (THPGXYuv2RgbSetup), here set directly and through the render-state cache.
// The original skipped the frame while the disc drive was in an error state; there is no drive on PC.
void ENgcRenderer::MovieFrame(EDLEntry* e) {
  if (!CanDraw()) return;  // PC: headless, or no frame open
  auto* movie = static_cast<ENgcMovie*>(static_cast<EMovie*>(e->p));
  // Bound textures are unlocked while the movie draws. (The original indexed m_textures[0] for both
  // slots.)
  for (ENgcTexture* t : m_textures)
    if (t && m_textures[0]) m_textures[0]->Unlock();
  GXSetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
  GXSetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
  GXSetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K1);
  GXSetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_K1_A);
  GXSetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K2);
  GXSetTevColorS10(GX_TEVREG2, GXColorS10{-90, 0, -114, 135});
  GXSetTevKColor(GX_KCOLOR0, GXColor{0x00, 0x00, 0xe2, 0x58});
  GXSetTevKColor(GX_KCOLOR1, GXColor{0xb3, 0x00, 0x00, 0xb6});
  GXSetTevKColor(GX_KCOLOR2, GXColor{0xff, 0x00, 0xff, 0x80});
  GXInvalidateTexAll();
  ENgcRenderStateCache* c = m_rsCache;
  c->SetProjection(1);
  c->SetCurrentMtx(GX_PNMTX5);
  SaveState(nullptr);
  c->SaveViewportState();
  c->m_field6E8 = 0x180;
  c->m_rasterModes = 0x10;  // no blending
  const float w = m_renderMode.fbWidth, h = m_renderMode.efbHeight;
  c->m_viewport[0] = 0.0f, c->m_viewport[1] = 0.0f, c->m_viewport[2] = w, c->m_viewport[3] = h;
  c->m_viewport[4] = 0.0f, c->m_viewport[5] = 1.0f;
  c->m_scissor[0] = 0, c->m_scissor[1] = 0;
  c->m_scissor[2] = m_renderMode.fbWidth, c->m_scissor[3] = m_renderMode.efbHeight;
  c->m_dirty |= 0x80000640;
  c->SetZMode(true, GX_ALWAYS, false);
  c->SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  c->SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
  c->SetZCompLoc(true);
  for (int chan = 0; chan < 4; ++chan) c->SetChanCtrl(chan, false, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_CLAMP, GX_AF_NONE);
  c->m_numTexGens = 2;
  c->m_numTevStages = 4;
  c->m_dirty |= 0x80000080;
  for (int i = 0; i < 2; ++i) {
    ENgcTexGen& g = c->m_texGen[i];
    g.dst = GX_TEXCOORD0 + i, g.type = GX_TG_MTX2x4, g.src = GX_TG_TEX0, g.mtx = GX_IDENTITY;
    g.normalize = 0, g.postMtx = GX_PTIDENTITY;
  }
  // Stage 0: U -> the G and B contributions (A: the G part, kept in alpha).
  c->SetTevOrder(0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
  c->SetTevColorIn(0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_C2);
  c->SetTevAlphaIn(0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_A2);
  c->SetTevColorOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, false, GX_TEVPREV);
  c->SetTevAlphaOp(0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, false, GX_TEVPREV);
  c->SetTevSwapMode(0, GX_TEV_SWAP0, GX_TEV_SWAP0);
  // Stage 1: V -> the R and G contributions.
  c->SetTevOrder(1, GX_TEXCOORD1, GX_TEXMAP2, GX_COLOR_NULL);
  c->SetTevColorIn(1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_CPREV);
  c->SetTevAlphaIn(1, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_APREV);
  c->SetTevColorOp(1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, false, GX_TEVPREV);
  c->SetTevAlphaOp(1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, false, GX_TEVPREV);
  c->SetTevSwapMode(1, GX_TEV_SWAP0, GX_TEV_SWAP0);
  // Stage 2: + Y.
  c->SetTevOrder(2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
  c->SetTevColorIn(2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_CPREV);
  c->SetTevAlphaIn(2, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
  c->SetTevColorOp(2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevAlphaOp(2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(2, GX_TEV_SWAP0, GX_TEV_SWAP0);
  // Stage 3: G comes from the alpha channel (K2 = R:1 G:0 B:1 selects per channel).
  c->SetTevOrder(3, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
  c->SetTevColorIn(3, GX_CC_APREV, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
  c->SetTevAlphaIn(3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
  c->SetTevColorOp(3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevAlphaOp(3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevSwapMode(3, GX_TEV_SWAP0, GX_TEV_SWAP0);
  // Vertex descriptors: direct position and first texture coordinate only.
  c->m_vtxDescPnMtx = 0, c->m_vtxDescPos = 0, c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0;
  for (int& t : c->m_vtxDescTex) t = 0;
  c->m_dirty |= 0x80000001;
  c->m_vtxDescPos = GX_DIRECT;
  c->m_vtxDescTex[0] = GX_DIRECT;
  c->m_dirty |= 0x80000020;
  c->Apply(false);
  GXSetAlphaUpdate(GX_FALSE);
  GXSetNumChans(0);
  movie->GetNextFrame();
  c->m_numTexGens = 1;
  c->RestoreViewportState();
  RestoreState(nullptr);
  c->m_dirty = 0xffffffff;
  c->SetProjection(2);
  c->SetCurrentMtx(GX_PNMTX4);
  GXSetAlphaUpdate(GX_TRUE);
  GXSetNumChans(1);
  for (ENgcTexture* t : m_textures) {
    if (t && m_textures[0]) {
      m_textures[0]->Select(0, 0);
      m_textures[0]->Lock();
    }
  }
}

// 80345314
// Switches rendering between the screen and an offscreen render surface: the surface being left is
// copied into its texture (ENgcRenderSurface::Deselect) and the state saved on entering it comes
// back; a surface entered draws without depth (and is cleared to its background when flagged).
void ENgcRenderer::RenderSurface(EDLEntry* e) {
  static ENgcRenderSurface* s_pRSurface = nullptr;
  auto* surface = static_cast<ENgcRenderSurface*>(static_cast<ERenderSurface*>(e->p));
  if (s_pRSurface) {
    s_pRSurface->Deselect();
    s_pRSurface = nullptr;
    m_rsCache->RestoreState();
    m_rsCache->m_dirty |= 0x80000600;  // PC: aurora reset the viewport and scissor with the framebuffer
  }
  if (!surface) {
    // Selecting the screen re-applied the display copy filter; aurora presents the EFB itself.
    return;
  }
  s_pRSurface = surface;
  surface->Select();
  m_rsCache->SaveState();
  ENgcRenderStateCache* c = m_rsCache;
  c->m_zUpdate = 0;
  c->m_zEnable = 0;
  c->m_zFunc = 7;
  c->m_dirty |= 0x80000040;
  c->m_dirty |= 0x80000600;  // PC: see above
  if (surface->m_flags & 2) {
    EVec4 rect{0.0f, 0.0f, static_cast<float>(surface->m_xSize), static_cast<float>(surface->m_ySize)};
    EVec4 color{surface->m_background[0], surface->m_background[1], surface->m_background[2], 1.0f};
    ClearRect(rect, color);
  }
}

// 8033E1D4
// Fills a rectangle (pixels, made 0..1 of the screen) with a colour: an untextured quad at the far
// plane through one TEV stage passing the vertex colour, writing depth without testing it.
void ENgcRenderer::ClearRect(EVec4& rect, EVec4& color) {
  const float w = static_cast<float>(m_renderMode.viWidth), h = static_cast<float>(m_renderMode.viHeight);
  rect.x /= w;
  rect.z /= w;
  rect.y /= h;
  rect.w /= h;
  unsigned char rgba[4];
  color.ToU8s(rgba);
  ENgcRenderStateCache* c = m_rsCache;
  c->SaveViewportState();
  c->SaveState();
  c->m_viewport[0] = 0.0f;
  c->m_viewport[1] = 0.0f;
  c->m_viewport[2] = w;
  c->m_viewport[3] = h;
  c->m_viewport[4] = 0.0f;
  c->m_viewport[5] = 1.0f;
  c->m_scissor[0] = 0;
  c->m_scissor[1] = 0;
  c->m_scissor[2] = m_renderMode.viWidth;
  c->m_scissor[3] = m_renderMode.viHeight;
  c->m_dirty |= 0x80000600;
  c->SetZMode(false, 7, true);
  c->SetAlphaCompare(7, 0, 1, 7, 0);
  c->m_field6E8 &= ~8;
  c->SetChanCtrl(0, false, 1, 1, 0, 2, 2);
  c->SetChanCtrl(2, false, 1, 1, 0, 2, 2);
  c->m_numTexGens = 0;
  c->m_numTevStages = 1;
  c->m_dirty |= 0x80000080;
  c->SetTevOrder(0, 0xff, 0xff, 4);
  c->SetTevColorIn(0, 0xf, 0xf, 0xf, 10);
  c->SetTevAlphaIn(0, 7, 7, 7, 5);
  c->SetTevColorOp(0, 0, 0, 0, true, 0);
  c->SetTevAlphaOp(0, 0, 0, 0, true, 0);
  c->SetProjection(0);
  c->SetCurrentMtx(GX_PNMTX5);
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = 0, c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0, c->m_vtxDescClr1 = 0;
  for (int i = 0; i < 6; ++i) c->m_vtxDescTex[i] = 0;
  c->m_vtxDescPos = 1;
  c->m_vtxDescClr0 = 1;
  c->m_dirty |= 0x80000001;
  c->Apply(false);
  GXBegin(GX_QUADS, GX_VTXFMT2, 4);
  const float z = c->m_viewport[5];
  const float corner[4][2] = {{rect.x, rect.y}, {rect.x, rect.w}, {rect.z, rect.w}, {rect.z, rect.y}};
  for (const auto& v : corner) {
    GXPosition3f32(v[0], v[1], z);
    GXColor4u8(rgba[0], rgba[1], rgba[2], rgba[3]);
  }
  GXEnd();
  c->RestoreViewportState();
  c->RestoreState();
  c->SetProjection(2);
  c->SetCurrentMtx(GX_PNMTX4);
}
