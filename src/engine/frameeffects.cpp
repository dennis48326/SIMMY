// Port of engine/frameeffects.cpp, engine/frameeffectsmanager.cpp, engine/frameeffectsngc.cpp and their
// linkonce helpers (Bloom/DepthOfField constructors, InterpolateLinear<float/EVec3>). See
// include/simmy/classes/frameeffects.hpp.
//
// The GameCube draw (frameeffectsngc.cpp) works on the EFB with GX directly, through the renderer's
// state cache (vertex formats 5 and 1, the identity position matrix GX_PNMTX5, the 0..1 orthographic
// projection):
//  - the 3D window's rectangle of the EFB is copied (RGB565) into the next external frame buffer,
//    which is free until the end of the frame;
//  - motion blur: the previous frame (copied at half size by ENgcRenderer::EndFrame), optionally
//    scaled to 256 >> (level-1) square through the scratch buffer;
//    bloom: the copy drawn at 128x128 through a bright pass (x frameAmp, - threshold, x bloomAmp) and
//    copied to the scratch buffer;
//    depth of field: the copy drawn at 256 >> level square and copied to the scratch buffer;
//  - that texture is blended back over the window with four taps jittered by half a texel (a quarter
//    of the weight each) plus the frame copy: out = blur * c1 x4 + frame * c2.
// PC: aurora keeps a GPU texture per copy destination (GXCopyTex / GXInitTexObj on the same pointer),
// sized from the logical 640x480 to the internal render resolution and resolved from the multisampled
// EFB, so every copy here follows the graphics settings. Widescreen: these draws cover the 3D view,
// which spans the stretched frame, so they are not mapped into the 4:3 centre (g_displayMap2D off
// around them).
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern ENgcRenderer _ngcRenderer;

namespace {
// 803C774C
// a until t0, b from t1, linear between (a zero-length ramp gives b).
float InterpolateLinear(float a, float b, float t0, float t1, float t) {
  if (std::abs(t1 - t0) < 0.001f) return b;
  if (t1 - 0.001f < t) return b;
  if (t0 + 0.001f <= t) return a + ((b - a) * (t - t0)) / (t1 - t0);
  return a;
}

// 803C77A4
void InterpolateLinear(float out[3], const float a[3], const float b[3], float t0, float t1, float t) {
  if (std::abs(t1 - t0) < 0.001f || t1 - 0.001f < t) {
    out[0] = b[0], out[1] = b[1], out[2] = b[2];
  } else if (t0 + 0.001f <= t) {
    const float k = 1.0f / (t1 - t0), d = t - t0;
    for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * d * k;
  } else {
    out[0] = a[0], out[1] = a[1], out[2] = a[2];
  }
}
}  // namespace

// ---- FrameEffect --------------------------------------------------------------------------------

// 80356FB0
// On: fades in (state 2); off (unless already off): fades out (state 8). Either restarts the clock.
void FrameEffect::Enable(bool on) {
  if (!on && m_state == kFrameEffectOff) return;
  m_state = on ? kFrameEffectFadingIn : kFrameEffectFadingOut;
  EffectStateChanged(m_state);
  m_time = 0.0f;
}

// 80357020
void FrameEffect::Update(float dt, ERC* rc) {
  m_time += dt;
  if (IsEffectRamping()) UpdateTargetParameters(m_time);
  if (IsEffectRunning()) Draw(rc);
  UpdateEffectState();
}

// 803570B4
bool FrameEffect::IsEffectRamping() const { return m_state == kFrameEffectFadingIn || m_state == kFrameEffectFadingOut; }

// 803570D4
bool FrameEffect::IsEffectRunning() const { return m_state != kFrameEffectOff; }

// 803570EC
void FrameEffect::UpdateEffectState() {
  if (m_state == kFrameEffectFadingIn && EffectIsFinishedFadingIn(m_time)) {
    m_state = kFrameEffectOn;
    EffectStateChanged(kFrameEffectOn);
    return;
  }
  if (m_state == kFrameEffectFadingOut && EffectIsFinishedFadingOut(m_time)) {
    m_state = kFrameEffectOff;
    EffectStateChanged(kFrameEffectOff);
  }
}

// ---- MotionBlur ---------------------------------------------------------------------------------

// 803571C0
// Fading in: the offset and the blend go from where they were to the target, the level is the
// target's; fading out: both to zero, level 0.
void MotionBlur::UpdateTargetParameters(float time) {
  if (m_state == kFrameEffectFadingIn) {
    m_current.colorOffset = InterpolateLinear(m_start.colorOffset, m_target.colorOffset, 0.0f, m_fadeInTime, time);
    m_current.previousFrameBlend =
        InterpolateLinear(m_start.previousFrameBlend, m_target.previousFrameBlend, 0.0f, m_fadeInTime, time);
    m_current.downSampleLevel = m_target.downSampleLevel;
  } else if (m_state == kFrameEffectFadingOut) {
    m_current.colorOffset = InterpolateLinear(m_start.colorOffset, 0.0f, 0.0f, m_fadeOutTime, time);
    m_current.previousFrameBlend = InterpolateLinear(m_start.previousFrameBlend, 0.0f, 0.0f, m_fadeOutTime, time);
    m_current.downSampleLevel = 0;
  }
}

// 8035728C
void MotionBlur::EffectStateChanged(int state) {
  if (state != kFrameEffectFadingIn && state != kFrameEffectFadingOut) return;
  m_start = m_current;
}

// 803572C0
bool MotionBlur::EffectIsFinishedFadingIn(float time) const { return m_fadeInTime <= time; }
// 803572D8
bool MotionBlur::EffectIsFinishedFadingOut(float time) const { return m_fadeOutTime <= time; }
// 80357658
MotionBlur::~MotionBlur() = default;
// 8035768C
void MotionBlur::SetTargetSettings(MotionBlurSettings& s) { m_target = s; }

// ---- Bloom --------------------------------------------------------------------------------------

namespace {
void DefaultBloomSettings(BloomSettings& s) {
  s.frameAmp = 1.0f;
  s.bloomThreshold = 0.9f;
  s.bloomAmp = 0.7f;
  for (auto& p : s.frameBlends) p[0] = 0.5f, p[1] = 1.0f;
}
}  // namespace

// 803C78E0
Bloom::Bloom() {
  DefaultBloomSettings(m_target);
  DefaultBloomSettings(m_current);
  DefaultBloomSettings(m_start);
}

// 803572F0
// Fading in: the frame and bloom amplifications and the blends are the target's at once, the
// threshold ramps; fading out: the threshold ramps up to 1 (nothing passes).
void Bloom::UpdateTargetParameters(float time) {
  if (m_state == kFrameEffectFadingIn) {
    m_current.frameAmp = m_target.frameAmp;
    m_current.bloomThreshold = InterpolateLinear(m_start.bloomThreshold, m_target.bloomThreshold, 0.0f, m_fadeInTime, time);
    m_current.bloomAmp = m_target.bloomAmp;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 2; ++j) m_current.frameBlends[i][j] = m_target.frameBlends[i][j];
  } else if (m_state == kFrameEffectFadingOut) {
    m_current.bloomThreshold = InterpolateLinear(m_start.bloomThreshold, 1.0f, 0.0f, m_fadeOutTime, time);
  }
}

// 803573BC
void Bloom::EffectStateChanged(int state) {
  if (state != kFrameEffectFadingIn && state != kFrameEffectFadingOut) return;
  m_start = m_current;
}

// 80357444
bool Bloom::EffectIsFinishedFadingIn(float time) const { return m_fadeInTime <= time; }
// 8035745C
bool Bloom::EffectIsFinishedFadingOut(float time) const { return m_fadeOutTime <= time; }
// 803576AC
Bloom::~Bloom() = default;
// 803576E0
void Bloom::SetTargetSettings(BloomSettings& s) { m_target = s; }

// ---- DepthOfField -------------------------------------------------------------------------------

// 803C7A14
DepthOfField::DepthOfField() {
  m_target = {0, {0.7f, 0.7f, 0.7f}, 1.0f};
  m_current = {0, {0.7f, 0.7f, 0.7f}, 1.0f};
  m_start = {0, {0.7f, 0.7f, 0.7f}, 1.0f};
}

// 80357474
// Fading in: the level is the target's, the tint and the blur factor ramp to it; fading out: the
// tint to white and the factor to zero.
void DepthOfField::UpdateTargetParameters(float time) {
  if (m_state == kFrameEffectFadingIn) {
    m_current.downSampleLevel = m_target.downSampleLevel;
    InterpolateLinear(m_current.tint, m_start.tint, m_target.tint, 0.0f, m_fadeInTime, time);
    m_current.blurFactor = InterpolateLinear(m_start.blurFactor, m_target.blurFactor, 0.0f, m_fadeInTime, time);
  } else if (m_state == kFrameEffectFadingOut) {
    const float white[3] = {1.0f, 1.0f, 1.0f};
    InterpolateLinear(m_current.tint, m_start.tint, white, 0.0f, m_fadeOutTime, time);
    m_current.blurFactor = InterpolateLinear(m_start.blurFactor, 0.0f, 0.0f, m_fadeOutTime, time);
  }
}

// 803575E4
void DepthOfField::EffectStateChanged(int state) {
  if (state != kFrameEffectFadingIn && state != kFrameEffectFadingOut) return;
  m_start = m_current;
}

// 80357628
bool DepthOfField::EffectIsFinishedFadingIn(float time) const { return m_fadeInTime <= time; }
// 80357640
bool DepthOfField::EffectIsFinishedFadingOut(float time) const { return m_fadeOutTime <= time; }
// 80357754
DepthOfField::~DepthOfField() = default;
// 80357788
void DepthOfField::SetTargetSettings(DepthOfFieldSettings& s) { m_target = s; }

// ---- FrameEffectsManager ------------------------------------------------------------------------

// 803577B8
void FrameEffectsManager::Init() {
  m_motionBlur = new MotionBlurNGC;
  m_bloom = new BloomNGC;
  m_depthOfField = new DepthOfFieldNGC;
}

// 803578B0
void FrameEffectsManager::Shutdown() {
  delete m_motionBlur;
  m_motionBlur = nullptr;
  delete m_bloom;
  m_bloom = nullptr;
  delete m_depthOfField;
  m_depthOfField = nullptr;
}

// 80357968
MotionBlur* FrameEffectsManager::MotionBlurObject() { return m_motionBlur; }
// 80357970
Bloom* FrameEffectsManager::BloomObject() { return m_bloom; }
// 80357978
DepthOfField* FrameEffectsManager::DepthOfFieldObject() { return m_depthOfField; }

// ---- the GameCube draw (frameeffectsngc.cpp) ----------------------------------------------------

namespace {
struct UV {
  float u, v;
};

// The window rectangle (E3DWindow +0x220, 0..1 of the frame).
TRect<float> CurrentWindowRect() {
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!win) return TRect<float>{0.0f, 0.0f, 1.0f, 1.0f};
  return win->m_viewport;
}

// Vertex descriptors: direct position and the first `texCoords` coordinates, nothing else.
void SetPosTexVtxDesc(ENgcRenderStateCache* c, int texCoords) {
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = 0, c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0;
  c->m_dirty |= 0x80000001;
  for (int i = 0; i < 6; ++i) c->m_vtxDescTex[i] = 0;
  c->m_vtxDescPos = 1;
  for (int i = 0; i < texCoords; ++i) {
    c->m_dirty |= 0x80000001;
    c->m_vtxDescTex[i] = 1;
  }
}

// 80357980
// The rectangle of the EFB `rect` covers in a w x h frame (moved by `offset`) into `dest` (RGB565).
void CopyEFBToBuffer(void* dest, int w, int h, const TRect<float>& rect, const UV& offset) {
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  const unsigned short cw = static_cast<unsigned short>(static_cast<int>(fw * (rect.x1 - rect.x0)));
  const unsigned short ch = static_cast<unsigned short>(static_cast<int>(fh * (rect.y1 - rect.y0)));
  GXSetTexCopySrc(static_cast<unsigned short>(static_cast<int>(fw * rect.x0 + offset.u)),
                  static_cast<unsigned short>(static_cast<int>(fh * rect.y0 + offset.v)), cw, ch);
  GXSetTexCopyDst(cw, ch, GX_TF_RGB565, GX_FALSE);
  GXCopyTex(dest, GX_FALSE);
  GXPixModeSync();
}

// The four corners of a 0..1 quad (triangle strip order) and their offsets into a texture.
const float kCorner[4][2] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}};

// 80357AA0
// `src` drawn into a dstW x dstH area at the window's place (uv offset + corner * uv scale), and that
// area copied into `dest`.
void CopyBufferToBufferScaled(void* dest, GXTexObj* src, int dstW, int dstH, int srcW, int srcH, const TRect<float>& rect,
                              ENgcRenderStateCache* c, const UV& uvOffset, const UV& uvScale) {
  GXLoadTexObj(src, GX_TEXMAP0);
  c->m_numTevStages = 1;
  c->m_numTexGens = 1;
  c->SetTevOrder(0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
  c->SetTevColorIn(0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_ZERO);
  c->SetTevColorOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  SetPosTexVtxDesc(c, 1);
  c->SaveViewportState();
  const UV offset{static_cast<float>(srcW) * rect.x0, static_cast<float>(srcH) * rect.y0};
  const float fw = static_cast<float>(dstW), fh = static_cast<float>(dstH);
  c->m_viewport[0] = fw * rect.x0 + offset.u;
  c->m_viewport[1] = fh * rect.y0 + offset.v;
  c->m_viewport[2] = fw * (rect.x1 - rect.x0);
  c->m_viewport[3] = fh * (rect.y1 - rect.y0);
  c->Apply(false);
  GXInvalidateTexAll();
  GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT5, 4);
  for (const auto& k : kCorner) {
    GXPosition3f32(k[0], k[1], 0.0f);
    GXTexCoord2f32(uvOffset.u + k[0] * uvScale.u, uvOffset.v + k[1] * uvScale.v);
  }
  GXEnd();
  GXDrawDone();
  CopyEFBToBuffer(dest, dstW, dstH, rect, offset);
  c->RestoreViewportState();
}

// 80357DF4
// The bright pass into a dstW x dstH area, copied into `dest`: the frame x frameAmp (above 1 the
// TEV's x2 / x4 scale), minus the threshold, times bloomAmp (above 1 the scale again).
void ConstructBloomMipSurface(void* dest, GXTexObj* src, int dstW, int dstH, float frameAmp, float threshold, float bloomAmp,
                              int srcW, int srcH, const TRect<float>& rect, ENgcRenderStateCache* c) {
  GXLoadTexObj(src, GX_TEXMAP0);
  const unsigned char t = static_cast<unsigned char>(static_cast<int>(threshold * 255.0f));
  GXSetTevColor(GX_TEVREG2, GXColor{t, t, t, 0xff});
  if (frameAmp <= 1.0f) {
    const unsigned char a = static_cast<unsigned char>(static_cast<int>(frameAmp * 255.0f));
    GXSetTevColor(GX_TEVREG1, GXColor{a, a, a, 0xff});
    c->SetTevOrder(0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    c->SetTevColorIn(0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C1, GX_CC_ZERO);
    c->SetTevColorOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  } else {
    c->SetTevOrder(0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    c->SetTevColorIn(0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_ZERO);
    c->SetTevColorOp(0, GX_TEV_ADD, GX_TB_ZERO, frameAmp <= 2.0f ? GX_CS_SCALE_2 : GX_CS_SCALE_4, true, GX_TEVPREV);
  }
  unsigned char stages;
  if (bloomAmp <= 1.0f) {
    c->SetTevOrder(1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    c->SetTevColorIn(1, GX_CC_ZERO, GX_CC_C2, GX_CC_ONE, GX_CC_CPREV);
    c->SetTevColorOp(1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    const unsigned char b = static_cast<unsigned char>(static_cast<int>(bloomAmp * 255.0f));
    GXSetTevColor(GX_TEVREG0, GXColor{b, b, b, 0xff});
    c->SetTevOrder(2, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    c->SetTevColorIn(2, GX_CC_ZERO, GX_CC_CPREV, GX_CC_C0, GX_CC_ZERO);
    c->SetTevColorOp(2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
    stages = 3;
  } else {
    c->SetTevOrder(1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    c->SetTevColorIn(1, GX_CC_ZERO, GX_CC_C2, GX_CC_ONE, GX_CC_CPREV);
    c->SetTevColorOp(1, GX_TEV_SUB, GX_TB_ZERO, bloomAmp <= 2.0f ? GX_CS_SCALE_2 : GX_CS_SCALE_4, true, GX_TEVPREV);
    stages = 2;
  }
  c->m_numTexGens = 1;
  c->m_numTevStages = stages;
  SetPosTexVtxDesc(c, 1);
  c->SaveViewportState();
  const UV offset{static_cast<float>(srcW) * rect.x0, static_cast<float>(srcH) * rect.y0};
  const float fw = static_cast<float>(dstW), fh = static_cast<float>(dstH);
  c->m_viewport[0] = fw * rect.x0 + offset.u;
  c->m_viewport[1] = fh * rect.y0 + offset.v;
  c->m_viewport[2] = fw * (rect.x1 - rect.x0);
  c->m_viewport[3] = fh * (rect.y1 - rect.y0);
  c->Apply(false);
  GXInvalidateTexAll();
  GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT5, 4);
  for (const auto& k : kCorner) {
    GXPosition3f32(k[0], k[1], 0.0f);
    GXTexCoord2f32(k[0], k[1]);
  }
  GXEnd();
  GXDrawDone();
  CopyEFBToBuffer(dest, dstW, dstH, rect, offset);
  c->RestoreViewportState();
}

// 803585C0
// Over the window: `blur` sampled four times half a texel apart (TEV colour 0 each, map 0, coordinates
// 0..3) plus `frame` (TEV colour 1, map 1, coordinate 4); no blending, no z.
void BlendJitteredScratchBufferWithFrameBuffer(GXTexObj* blur, GXTexObj* frame, const GXColor& c1, const GXColor& c2,
                                               const TRect<float>& rect, ENgcRenderStateCache* c, const GXRenderModeObj& rm,
                                               const UV& uvOffset, const UV& uvScale) {
  c->m_numTexGens = 5;
  c->m_numTevStages = 5;
  c->m_dirty |= 0x80000080;
  GXSetTevColor(GX_TEVREG0, c1);
  GXSetTevColor(GX_TEVREG1, c2);
  GXLoadTexObj(blur, GX_TEXMAP0);
  GXLoadTexObj(frame, GX_TEXMAP1);
  c->SetTevOrder(0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
  c->SetTevOrder(1, GX_TEXCOORD1, GX_TEXMAP0, GX_COLOR0A0);
  c->SetTevOrder(2, GX_TEXCOORD2, GX_TEXMAP0, GX_COLOR0A0);
  c->SetTevOrder(3, GX_TEXCOORD3, GX_TEXMAP0, GX_COLOR0A0);
  c->SetTevOrder(4, GX_TEXCOORD4, GX_TEXMAP1, GX_COLOR0A0);
  c->SetTevColorIn(0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
  c->SetTevColorOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  for (int s = 1; s < 4; ++s) {
    c->SetTevColorIn(s, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_CPREV);
    c->SetTevColorOp(s, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  }
  c->SetTevColorIn(4, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C1, GX_CC_CPREV);
  c->SetTevColorOp(4, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  SetPosTexVtxDesc(c, 5);
  c->SaveViewportState();
  c->m_viewport[0] = static_cast<float>(rm.fbWidth) * rect.x0;
  c->m_viewport[1] = static_cast<float>(rm.efbHeight) * rect.y0;
  c->m_viewport[2] = static_cast<float>(rm.fbWidth) * (rect.x1 - rect.x0);
  c->m_dirty |= 0x80000200;
  c->m_viewport[3] = static_cast<float>(rm.efbHeight) * (rect.y1 - rect.y0);
  c->Apply(false);
  const float j = 0.5f / static_cast<float>(GXGetTexObjHeight(blur));
  GXInvalidateTexAll();
  GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT1, 4);
  for (const auto& k : kCorner) {
    GXPosition3f32(k[0], k[1], 0.0f);
    const float u = uvOffset.u + k[0] * uvScale.u, v = uvOffset.v + k[1] * uvScale.v;
    GXTexCoord2f32(u + j, v + j);
    GXTexCoord2f32(u - j, v + j);
    GXTexCoord2f32(u + j, v - j);
    GXTexCoord2f32(u - j, v - j);
    GXTexCoord2f32(k[0], k[1]);
  }
  GXEnd();
  GXDrawDone();
  c->RestoreViewportState();
}

// 80358E38
// The full frame viewport and scissor, no z test or update, alpha compare always passes, blending
// ONE/ZERO, the colour channels unlit (vertex colours), no TEV swaps, texture coordinates i from
// TEXi unchanged, the identity position matrix and the 0..1 orthographic projection.
void SetDefaultRenderStates(ENgcRenderStateCache* c, const GXRenderModeObj& rm) {
  c->m_viewport[0] = 0.0f;
  c->m_viewport[1] = 0.0f;
  c->m_viewport[2] = static_cast<float>(rm.fbWidth);
  c->m_viewport[3] = static_cast<float>(rm.efbHeight);
  c->m_viewport[4] = 0.0f;
  c->m_viewport[5] = 1.0f;
  c->m_scissor[0] = 0;
  c->m_scissor[1] = 0;
  c->m_scissor[2] = rm.fbWidth;
  c->m_scissor[3] = rm.efbHeight;
  c->m_field6E8 &= ~8u;
  c->m_dirty |= 0x80000740;
  c->m_alphaComp1 = GX_ALWAYS;
  c->m_zEnable = 0;
  c->m_zFunc = GX_ALWAYS;
  c->m_zUpdate = 0;
  c->m_alphaComp0 = GX_ALWAYS;
  c->m_alphaRef0 = 0;
  c->m_alphaOp = GX_AOP_OR;
  c->m_alphaRef1 = 0;
  c->m_blendType = GX_BM_BLEND;
  c->m_blendSrc = GX_BL_ONE;
  c->m_blendDst = GX_BL_ZERO;
  c->m_blendLogic = GX_LO_CLEAR;
  c->SetChanCtrl(0, false, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_CLAMP, GX_AF_NONE);
  c->SetChanCtrl(2, false, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_CLAMP, GX_AF_NONE);
  for (int s = 0; s < 8; ++s) c->SetTevSwapMode(s, GX_TEV_SWAP0, GX_TEV_SWAP0);
  for (int i = 0; i < 6; ++i) {
    ENgcTexGen& g = c->m_texGen[i];
    g.dst = GX_TEXCOORD0 + i;
    g.type = GX_TG_MTX2x4;
    g.src = GX_TG_TEX0 + i;
    g.mtx = GX_IDENTITY;
    g.normalize = 0;
    g.postMtx = GX_PTIDENTITY;
    g.envMapped = 0;
  }
  c->m_projection = 0;
  c->m_currentMtx = GX_PNMTX5;
  c->m_dirty |= 0x80000028;
}

// Shared head and tail of the three Draws: the render state saved and set to the defaults, the
// window's part of the EFB copied into the next external frame buffer (`frame`); afterwards the
// state restored, the perspective projection and the model matrix slot back (as the 3D draw had it).
struct EffectPass {
  ENgcRenderStateCache* c;
  GXRenderModeObj rm;
  TRect<float> rect;
  GXTexObj frame;
  bool savedMap2D;

  EffectPass() {
    // GC: ENgcRenderer::s_copyMutex (the triple-buffered XFB copy); PC: one thread, nothing to lock.
    c = _ngcRenderer.GetRenderStateCache();
    rm = _ngcRenderer.GetCurrentRenderMode();
    rect = CurrentWindowRect();
    // PC widescreen: the quads cover the 3D view, which spans the stretched frame.
    savedMap2D = simmy::g_displayMap2D;
    simmy::g_displayMap2D = false;
    c->SaveViewportState();
    c->SaveState();
    SetDefaultRenderStates(c, rm);
    void* xfb = _ngcRenderer.GetNextExternalFrameBuffer();
    GXDrawDone();
    CopyEFBToBuffer(xfb, rm.fbWidth, rm.efbHeight, rect, UV{0.0f, 0.0f});
    const unsigned int h = static_cast<unsigned int>(static_cast<float>(rm.efbHeight) * (rect.y1 - rect.y0));
    GXInitTexObj(&frame, xfb, static_cast<unsigned short>(static_cast<int>(static_cast<float>(rm.fbWidth) * (rect.x1 - rect.x0))),
                 static_cast<unsigned short>(h), GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
  }
  ~EffectPass() {
    c->RestoreViewportState();
    c->RestoreState();
    c->m_projection = 2;
    c->m_currentMtx = GX_PNMTX4;
    c->m_dirty |= 0x80000020;
    // PC: the renderer sets its TEV stages up again before its next primitive (the effect changed
    // them behind its back; the GameCube's next shader change did the same).
    _ngcRenderer.m_geometryDirty = 1;
    simmy::g_displayMap2D = savedMap2D;
  }
};

GXColor Grey(float v, unsigned char a) {
  const unsigned char b = static_cast<unsigned char>(static_cast<int>(v));
  return GXColor{b, b, b, a};
}
}  // namespace

// 8035917C
// The previous frame (or it scaled to 256 >> (level-1)) blended over this one by previousFrameBlend.
void MotionBlurNGC::Draw(ERC*) {
  if (!_ngcRenderer.CanDraw()) return;
  EffectPass p;
  const float blend = m_current.previousFrameBlend;
  const GXColor c1 = Grey(blend * 0.25f * 255.0f, 0x40);
  const GXColor c2 = Grey((1.0f - blend) * 255.0f, 0xff);
  int prevW = 0, prevH = 0;
  void* prev = _ngcRenderer.GetPreviousFrameBuffer(&prevW, &prevH);
  GXTexObj prevTex;
  GXInitTexObj(&prevTex, prev, static_cast<unsigned short>(prevW), static_cast<unsigned short>(prevH), GX_TF_RGB565, GX_CLAMP,
               GX_CLAMP, GX_FALSE);
  const UV winOffset{p.rect.x0, p.rect.y0}, winSize{p.rect.x1 - p.rect.x0, p.rect.y1 - p.rect.y0};
  if (m_current.downSampleLevel < 1) {
    BlendJitteredScratchBufferWithFrameBuffer(&prevTex, &p.frame, c1, c2, p.rect, p.c, p.rm, winOffset, winSize);
  } else {
    const int size = (0x100 >> ((m_current.downSampleLevel - 1) & 0x3f)) & 0xffff;
    void* scratch = _ngcRenderer.GetFrameEffectsScratchMemory();
    CopyBufferToBufferScaled(scratch, &prevTex, size, size, prevW, prevH, p.rect, p.c, winOffset, winSize);
    const float fs = static_cast<float>(size);
    GXTexObj scaled;
    GXInitTexObj(&scaled, scratch, static_cast<unsigned short>(static_cast<int>(fs * (p.rect.x1 - p.rect.x0))),
                 static_cast<unsigned short>(static_cast<int>(fs * (p.rect.y1 - p.rect.y0))), GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
                 GX_FALSE);
    BlendJitteredScratchBufferWithFrameBuffer(&scaled, &p.frame, c1, c2, p.rect, p.c, p.rm, UV{0.0f, 0.0f}, UV{1.0f, 1.0f});
  }
}

// 803595E4
// The bright pass at 128x128 blended over the frame (frame blends pair 1: bloom x4 quarters, frame).
void BloomNGC::Draw(ERC*) {
  if (!_ngcRenderer.CanDraw()) return;
  EffectPass p;
  void* scratch = _ngcRenderer.GetFrameEffectsScratchMemory();
  ConstructBloomMipSurface(scratch, &p.frame, 0x80, 0x80, m_current.frameAmp, m_current.bloomThreshold, m_current.bloomAmp,
                           p.rm.fbWidth, p.rm.efbHeight, p.rect, p.c);
  GXTexObj bloom;
  GXInitTexObj(&bloom, scratch, static_cast<unsigned short>(static_cast<int>((p.rect.x1 - p.rect.x0) * 128.0f)),
               static_cast<unsigned short>(static_cast<int>((p.rect.y1 - p.rect.y0) * 128.0f)), GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
               GX_FALSE);
  const GXColor c1 = Grey(m_current.frameBlends[1][0] * 0.25f * 255.0f, 0x40);
  const GXColor c2 = Grey(m_current.frameBlends[1][1] * 255.0f, 0xff);
  BlendJitteredScratchBufferWithFrameBuffer(&bloom, &p.frame, c1, c2, p.rect, p.c, p.rm, UV{0.0f, 0.0f}, UV{1.0f, 1.0f});
}

// 80359950
// The frame scaled down to 256 >> level and back up (jittered), tinted, blended in by blurFactor.
void DepthOfFieldNGC::Draw(ERC*) {
  if (!_ngcRenderer.CanDraw()) return;
  EffectPass p;
  const int size = 0x100 >> (m_current.downSampleLevel & 0x3f);
  void* scratch = _ngcRenderer.GetFrameEffectsScratchMemory();
  CopyBufferToBufferScaled(scratch, &p.frame, size, size, p.rm.fbWidth, p.rm.efbHeight, p.rect, p.c, UV{0.0f, 0.0f},
                           UV{1.0f, 1.0f});
  const float fs = static_cast<float>(size);
  GXTexObj blur;
  GXInitTexObj(&blur, scratch, static_cast<unsigned short>(static_cast<int>(fs * (p.rect.x1 - p.rect.x0))),
               static_cast<unsigned short>(static_cast<int>(fs * (p.rect.y1 - p.rect.y0))), GX_TF_RGB565, GX_CLAMP, GX_CLAMP,
               GX_FALSE);
  const float k = m_current.blurFactor;
  const float r = m_current.tint[0] * 255.0f * k, g = m_current.tint[1] * 255.0f * k, b = m_current.tint[2] * 255.0f * k;
  const GXColor c1{static_cast<unsigned char>(static_cast<int>(r * 0.25f)), static_cast<unsigned char>(static_cast<int>(g * 0.25f)),
                   static_cast<unsigned char>(static_cast<int>(b * 0.25f)), 0x40};
  const GXColor c2 = Grey((1.0f - k) * 255.0f, 0xff);
  BlendJitteredScratchBufferWithFrameBuffer(&blur, &p.frame, c1, c2, p.rect, p.c, p.rm, UV{0.0f, 0.0f}, UV{1.0f, 1.0f});
}

// 80359D70
MotionBlurNGC::~MotionBlurNGC() = default;
// 80359DA4
BloomNGC::~BloomNGC() = default;
// 80359DD8
DepthOfFieldNGC::~DepthOfFieldNGC() = default;

// ---- PC dev aid ---------------------------------------------------------------------------------

// SIMMY_FX_FRAME=bloom|blur|dof (comma separated; "bloom=<row>" / "dof=<row>" pick a quickdata row of
// FrameEffectsBloomDataElement / FrameEffectsDepthOfFieldDataElement, e.g. dof=stancelover; blur uses
// the camera director's own TurnOnMotionBlur settings): switched on once the effect is off, i.e. the
// effects stay on. For screenshots of effects the game only shows in rare situations.
bool simmy_force_frame_effects(FrameEffectsManager* fx) {
  static const char* env = std::getenv("SIMMY_FX_FRAME");
  if (!env || !*env || !fx) return false;
  std::string s = env;
  bool any = false;
  size_t pos = 0;
  while (pos <= s.size()) {
    size_t end = s.find(',', pos);
    if (end == std::string::npos) end = s.size();
    std::string item = s.substr(pos, end - pos);
    pos = end + 1;
    std::string row;
    if (size_t eq = item.find('='); eq != std::string::npos) row = item.substr(eq + 1), item = item.substr(0, eq);
    EAnimNote note;
    if (item == "bloom" && fx->BloomObject() && fx->BloomObject()->m_state == kFrameEffectOff) {
      note.m_text = row.empty() ? "WhiteIntensity:fadeInTime:0.5" : row;
      HandleBloomEvent(note, fx);
      any = true;
    } else if (item == "dof" && fx->DepthOfFieldObject() && fx->DepthOfFieldObject()->m_state == kFrameEffectOff) {
      note.m_text = row.empty() ? "socialmodedefault:blurFactor:0.6:fadeInTime:0.5" : row;
      HandleDepthOfFieldEvent(note, fx);
      any = true;
    } else if (item == "blur" && fx->MotionBlurObject() && fx->MotionBlurObject()->m_state == kFrameEffectOff) {
      // As CameraDirector's TurnOnMotionBlur (800158C4).
      MotionBlur* mb = fx->MotionBlurObject();
      mb->m_fadeInTime = 0.0f;
      mb->m_target.colorOffset = 0.12f;
      mb->m_target.downSampleLevel = row.empty() ? 0 : std::atoi(row.c_str());
      mb->m_target.previousFrameBlend = 0.5f;
      mb->Enable(true);
      any = true;
    }
    if (end == s.size()) break;
  }
  return any;
}
