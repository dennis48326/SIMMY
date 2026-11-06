// Port of the censor part of engine/e_ngcrenderer.cpp: ENgcRenderer::ScrambleRect, the mosaic drawn
// over a censored sim (SAnimator2::DrawCensor -> EGraphics::SetCensorRect -> ENgcGraphics::
// DrawCensorRects -> ENgcRC::ScrambleRect).
//
// GC: the EFB was read and written by the CPU (GXPeekARGB / GXPeekZ / GXPokeARGB / GXPokeZ): in the
// rectangle, per block of N x N pixels, one pixel's colour (a random offset along the diagonal) filled
// the whole block, with the censor's depth, where the block's corner lay behind the censor.
// PC: aurora has no EFB peek or poke. The rectangle is copied from the EFB into a texture (GXCopyTex,
// in logical 640x480 units: aurora copies at the render resolution) and each block is drawn as a quad
// of that one texel (nearest sampling) at the censor's depth, with a less-than depth test and depth
// writes - the same picture, the depth test done per pixel instead of at the block's corner. The block
// size stays in logical pixels, so the mosaic looks the same at any render resolution.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdint>
#include <vector>

unsigned int _ngcScrambleRectPixels = 4;  // the block size of the last rectangle

namespace {
// One copy texture per rectangle of the frame (aurora keys copy textures by destination pointer; a
// buffer per slot keeps two rectangles of a frame from sharing one).
std::vector<unsigned char> s_censorCopy[16];
unsigned int s_censorSlot = 0;
unsigned int s_censorFrame = ~0u;

// a <= v <= a + len, as the GameCube clamped to the scissor (unsigned).
unsigned int ClampTo(unsigned int v, unsigned int lo, unsigned int len) {
  if (v < lo) return lo;
  return lo + len < v ? lo + len : v;
}
}  // namespace

// 80348E2C
// The block size by the rectangle's size (over 0.08 of the screen 16 pixels, over 1/37.5 nine, else
// four), the rectangle grown by a block and clamped to the scissor, the blocks' random offsets from
// the system clock (six seeds a second: the mosaic shimmers), each block behind the censor depth `z`
// (the screen depth, -1..0) filled with one of its pixels at that depth.
void ENgcRenderer::ScrambleRect(EVec2& min, EVec2& max, float z) {
  // GC: GXPokeBlendMode(1, 1, 0, 0), GXPokeAlphaMode(ALWAYS, 0), GXDrawDone (the CPU access set-up).
  float size = max.y - min.y;
  if (size < max.x - min.x) size = max.x - min.x;
  if (size <= 0.08f) _ngcScrambleRectPixels = 0.026666665f < size ? 9 : 4;
  else _ngcScrambleRectPixels = 0x10;
  const unsigned int n = _ngcScrambleRectPixels;
  // PC: blocks stay square on screen when the logical 640x480 frame is stretched wider (16:9, the wide
  // and 4:3 split frames): a block is n logical pixels high and n / (the frame's aspect / 4:3) wide.
  // GC: always n x n (4:3).
  const float pixelAspect = simmy::split_screen_aspect() / (4.0f / 3.0f);
  unsigned int nx = n;
  if (pixelAspect > 1.01f || pixelAspect < 0.99f) {
    nx = static_cast<unsigned int>(static_cast<float>(n) / pixelAspect + 0.5f);
    if (nx < 1) nx = 1;
  }
  const float fw = static_cast<float>(m_renderMode.fbWidth), fh = static_cast<float>(m_renderMode.xfbHeight);
  int x0 = static_cast<int>(min.x * fw) - static_cast<int>(nx);
  int y0 = static_cast<int>(min.y * fh) - static_cast<int>(n);
  int x1 = static_cast<int>(max.x * fw) + static_cast<int>(nx);
  int y1 = static_cast<int>(max.y * fh) + static_cast<int>(n);
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  ENgcRenderStateCache* c = m_rsCache;
  const unsigned int sx = static_cast<unsigned int>(c->m_scissor[0]), sy = static_cast<unsigned int>(c->m_scissor[1]);
  const unsigned int sw = static_cast<unsigned int>(c->m_scissor[2]), sh = static_cast<unsigned int>(c->m_scissor[3]);
  const unsigned int bx0 = ClampTo(static_cast<unsigned int>(x0), sx, sw);
  const unsigned int by0 = ClampTo(static_cast<unsigned int>(y0), sy, sh);
  const unsigned int bx1 = ClampTo(static_cast<unsigned int>(x1), sx, sw);
  const unsigned int by1 = ClampTo(static_cast<unsigned int>(y1), sy, sh);
  const double now = _pClockMan->GetSec(&_sysclock);
  unsigned int seed = static_cast<unsigned int>(static_cast<long long>(static_cast<float>(now * 6.0)));
  const unsigned int mask = static_cast<unsigned int>(-static_cast<int>(n));
  const unsigned int xEnd = nx == n ? bx0 + (((bx1 - bx0) - 1 + n) & mask) : bx0 + ((bx1 - bx0) + nx - 1) / nx * nx;
  const unsigned int yEnd = by0 + (((by1 - by0) - 1 + n) & mask);
  if (xEnd <= bx0 || yEnd <= by0) return;

  // PC: the rectangle's pixels (as far as the frame goes) into this slot's copy texture.
  const unsigned int frameW = m_renderMode.fbWidth, frameH = m_renderMode.efbHeight;
  const unsigned int cx0 = bx0 < frameW ? bx0 : frameW - 1, cy0 = by0 < frameH ? by0 : frameH - 1;
  unsigned int cw = (xEnd < frameW ? xEnd : frameW) - cx0, ch = (yEnd < frameH ? yEnd : frameH) - cy0;
  cw = (cw + 1) & ~1u, ch = (ch + 1) & ~1u;  // GX copies even sizes
  if (cx0 + cw > frameW) cw -= 2;
  if (cy0 + ch > frameH) ch -= 2;
  if (static_cast<int>(cw) <= 0 || static_cast<int>(ch) <= 0) return;
  if (s_censorFrame != static_cast<unsigned int>(_framecount)) s_censorFrame = static_cast<unsigned int>(_framecount), s_censorSlot = 0;
  std::vector<unsigned char>& buf = s_censorCopy[s_censorSlot++ & 15];
  buf.resize(static_cast<size_t>(cw) * ch * 2 + 32);
  void* dest = reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(buf.data()) + 31) & ~std::uintptr_t{31});
  if (!CanDraw()) return;
  GXDrawDone();
  GXSetTexCopySrc(static_cast<u16>(cx0), static_cast<u16>(cy0), static_cast<u16>(cw), static_cast<u16>(ch));
  GXSetTexCopyDst(static_cast<u16>(cw), static_cast<u16>(ch), GX_TF_RGB565, GX_FALSE);
  GXCopyTex(dest, GX_FALSE);
  GXPixModeSync();
  GXTexObj tex;
  GXInitTexObj(&tex, dest, static_cast<u16>(cw), static_cast<u16>(ch), GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GXInitTexObjLOD(&tex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);

  // The blocks: the texture's one texel, at the censor depth, depth-tested (less) with writes, opaque.
  const bool savedMap2D = simmy::g_displayMap2D;
  simmy::g_displayMap2D = false;  // the frame's own pixels (the 3D view spans the stretched frame)
  c->SaveViewportState();
  c->SaveState();
  c->m_viewport[0] = 0.0f, c->m_viewport[1] = 0.0f;
  c->m_viewport[2] = static_cast<float>(frameW), c->m_viewport[3] = static_cast<float>(frameH);
  c->m_viewport[4] = 0.0f, c->m_viewport[5] = 1.0f;
  c->m_field6E8 &= ~6u;
  c->m_cullMode = GX_CULL_NONE;
  c->m_rasterModes &= ~0x40u;  // no blending: the block replaces the frame
  c->SetZMode(true, GX_LESS, true);
  c->SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
  c->m_vtxDescPnMtx = 0;
  c->m_vtxDescPos = GX_DIRECT, c->m_vtxDescNrm = 0, c->m_vtxDescClr0 = 0;
  for (int i = 0; i < 6; ++i) c->m_vtxDescTex[i] = 0;
  c->m_vtxDescTex[0] = GX_DIRECT;
  ENgcTexGen& g = c->m_texGen[0];
  g.dst = GX_TEXCOORD0, g.type = GX_TG_MTX2x4, g.src = GX_TG_TEX0, g.mtx = GX_IDENTITY;
  g.normalize = 0, g.postMtx = GX_PTIDENTITY, g.envMapped = 0;
  c->m_numTexGens = 1;
  c->m_numTevStages = 1;
  c->SetTevSwapMode(0, GX_TEV_SWAP0, GX_TEV_SWAP0);
  c->SetTevOrder(0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
  c->SetTevColorIn(0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
  c->SetTevAlphaIn(0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
  c->SetTevColorOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->SetTevAlphaOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  c->m_currentMtx = GX_PNMTX5;  // the identity (InitGX)
  c->m_dirty |= 0x800007ff;
  c->Apply(false);
  // Pixels -> clip space, the depth passed through (screen depth -1..0, as the scene's projection).
  Mtx44 pixels = {{2.0f / static_cast<float>(frameW), 0.0f, 0.0f, -1.0f},
                  {0.0f, -2.0f / static_cast<float>(frameH), 0.0f, 1.0f},
                  {0.0f, 0.0f, 1.0f, 0.0f},
                  {0.0f, 0.0f, 0.0f, 1.0f}};
  GXSetProjection(pixels, GX_ORTHOGRAPHIC);
  GXLoadTexObj(&tex, GX_TEXMAP0);
  GXInvalidateTexAll();
  const float iw = 1.0f / static_cast<float>(cw), ih = 1.0f / static_cast<float>(ch);
  for (unsigned int x = bx0; x < xEnd; x += nx) {
    for (unsigned int y = by0; y < yEnd; y += n) {
      seed = seed * 0x41c64e6du + 0x3039u;
      const unsigned int off = seed % n;
      float u = (static_cast<float>(x + off * nx / n) - static_cast<float>(cx0) + 0.5f) * iw;
      float v = (static_cast<float>(y + off) - static_cast<float>(cy0) + 0.5f) * ih;
      if (u > 1.0f) u = 1.0f - 0.5f * iw;
      if (v > 1.0f) v = 1.0f - 0.5f * ih;
      const float px0 = static_cast<float>(x), py0 = static_cast<float>(y);
      const float px1 = px0 + static_cast<float>(nx), py1 = py0 + static_cast<float>(n);
      GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT5, 4);
      GXPosition3f32(px0, py0, z);
      GXTexCoord2f32(u, v);
      GXPosition3f32(px0, py1, z);
      GXTexCoord2f32(u, v);
      GXPosition3f32(px1, py0, z);
      GXTexCoord2f32(u, v);
      GXPosition3f32(px1, py1, z);
      GXTexCoord2f32(u, v);
      GXEnd();
    }
  }
  c->RestoreViewportState();
  c->RestoreState();
  c->m_projection = 2;
  c->m_currentMtx = GX_PNMTX4;
  c->m_dirty |= 0x80000020;
  m_geometryDirty = 1;
  simmy::g_displayMap2D = savedMap2D;
}
