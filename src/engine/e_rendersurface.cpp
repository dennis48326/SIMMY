// Port of engine/e_rendersurface.cpp and engine/e_ngcrendersurface.cpp: offscreen render targets.
//
// GameCube: a selected surface was drawn into the top-left of the EFB and copied into its RGBA8
// texture (GXCopyTex into the texture's image memory) when the renderer switched away from it.
// PC: aurora gives the surface a framebuffer of its own while selected (GXCreateFrameBuffer /
// GXRestoreFrameBuffer), so the screen's EFB is left alone, and the GXCopyTex destination (the
// texture's image pointer) makes GX texture objects on that memory use the copy. The copy stays on
// the GPU; GetImageData and CopyToTexture read it back through aurora's readback, a few frames late.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <dolphin/gx/GXAurora.h>

#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------------------------------
// ERenderSurface

// 80364DAC
ERenderSurface::ERenderSurface() {
  m_flags = 3;
  m_field1C = 1;
  m_field18 = 1;
  m_background[0] = m_background[1] = m_background[2] = 0.0f;
  m_xSize = -1;
  m_ySize = -1;
}
// 80364E24
ERenderSurface::~ERenderSurface() = default;
// 80364E58
int ERenderSurface::Create(ERenderSurfaceDef& def) {
  static_cast<ERenderSurfaceDef&>(*this) = def;
  return 1;
}
// 80364EB8
int ERenderSurface::GetXSize() { return m_xSize; }
// 80364EC0
int ERenderSurface::GetYSize() { return m_ySize; }
// 80364EC8
void ERenderSurface::SetFlags(unsigned int flags) { m_flags = flags; }
// 80364ED0
int ERenderSurface::SetSize(int x, int y, int field18) {
  m_field18 = field18;
  m_xSize = x;
  m_ySize = y;
  return 1;
}
// 80364EE8
void ERenderSurface::GetOutputRect(TRect<float>& out) {
  out.y0 = 0.0f;
  out.x1 = static_cast<float>(m_xSize - 1);
  out.x0 = 0.0f;
  out.y1 = static_cast<float>(m_ySize - 1);
}
// 80364F5C
unsigned int ERenderSurface::GetFlags() { return m_flags; }

// ---------------------------------------------------------------------------------------------------
// ENgcRenderSurface

// 8034C260
ENgcRenderSurface::ENgcRenderSurface() = default;
// 8034C2A8
ENgcRenderSurface::~ENgcRenderSurface() {
  if (m_readbackStream) AuroraReadbackDestroyStream(m_readbackStream);
}
// 8034C2D8
void ENgcRenderSurface::DestroySubtextures() {
  if (m_texture && m_texture->m_impl) GXDestroyCopyTex(m_texture->m_impl->m_image);  // PC: the GPU copy
  _pGfx->Destroy(static_cast<ETexture*>(m_texture));
  m_texture = nullptr;
}
// 8034C314
ETexture* ENgcRenderSurface::GetTexture() { return m_texture; }

// 8034C31C
// An RGBA8 texture of the surface's size receives the copies.
int ENgcRenderSurface::Create(ERenderSurfaceDef& def) {
  ETextureDef t;
  t.m_field00 = 0;
  t.m_field04 = 0;
  t.m_flags = 0x80;
  t.m_width = static_cast<unsigned short>(def.m_xSize);
  t.m_height = static_cast<unsigned short>(def.m_ySize);
  t.m_paletteSize = 0;
  t.m_mipCount = 0;
  t.m_format = 1;
  t.m_paletteFormat = 0;
  t.m_bpp = 0x20;
  t.m_paletteBpp = 0;
  t.m_name = nullptr;
  m_texture = static_cast<ENgcTexture*>(_pGfx->CreateTexture(t));
  if (m_texture) m_texture->FlushImageMemoryRange(true);
  ERenderSurface::Create(def);
  return 1;
}
// 8034C3E4
void ENgcRenderSurface::GetOutputRect(TRect<float>& out) { ERenderSurface::GetOutputRect(out); }
// 8034C404
void ENgcRenderSurface::SetBackgroundColor(EVec3& color) {
  m_background[0] = color.x;
  m_background[2] = color.z;
  m_background[1] = color.y;
}

namespace {
// PC: brings the GPU copy back into the texture's image memory (the GameCube's copy wrote there).
// Returns false until a readback has completed.
bool ReadBackCopy(ENgcRenderSurface* s) {
  if (!s->m_texture || !s->m_texture->m_impl) return false;
  ENgcTextureBase* t = s->m_texture->m_impl;
  s->m_readbackWanted = true;
  if (!s->m_readbackStream) s->m_readbackStream = AuroraReadbackCreateStream();
  AuroraReadbackDest dest{0, t->m_image, GXGetTexBufferSize(t->m_def.m_width, t->m_def.m_height, GX_TF_RGBA8, 0, 0),
                          GX_FALSE};
  return AuroraReadbackGet(s->m_readbackStream, &dest, 1) && dest.found;
}
}  // namespace

// 8034C424
// The surface as linear RGBA8.
void ENgcRenderSurface::GetImageData(void* out) {
  ReadBackCopy(this);
  ENgcTextureUtil::SwizzleFromRGBA8(m_texture->m_impl->m_image, m_xSize, m_ySize, static_cast<unsigned char*>(out));
}

// 8034C45C
// Copies the surface into another texture: the same format as is, or (32 to 16 bits) as RGB5A3
// with alpha only where the source alpha is nonzero.
int ENgcRenderSurface::CopyToTexture(ETexture* texture) {
  ReadBackCopy(this);
  auto* dst = static_cast<ENgcTexture*>(texture);
  const unsigned char srcBpp = m_texture->m_def.m_bpp;
  int pitch = 0, rows = 0;
  if (dst->m_def.m_bpp == srcBpp) {
    dst->UpdateBegin(ETextureUpdateType::Image);
    unsigned char* out = dst->UpdateMipLevel(0, pitch, rows);
    ENgcTextureBase* src = m_texture->m_impl;
    int w = 0, h = 0;
    std::memcpy(out, src->m_image, static_cast<size_t>(src->ComputeMipLevelOffset(1, w, h)));
    dst->UpdateEnd();
  } else {
    if (dst->m_def.m_bpp != 0x10 || srcBpp != 0x20) return 0;
    dst->ClearSwizzleFlag();
    dst->UpdateBegin(ETextureUpdateType::Image);
    const unsigned int height = m_texture->m_def.m_height, width = m_texture->m_def.m_width;
    auto* out = reinterpret_cast<unsigned short*>(dst->UpdateMipLevel(0, pitch, rows));
    m_texture->ClearSwizzleFlag();
    m_texture->UpdateBegin(ETextureUpdateType::PaletteOnly);
    const unsigned char* in = m_texture->UpdateMipLevel(0, pitch, rows);
    for (unsigned int i = 0; i < height * width; ++i) {
      const unsigned char* p = in + i * 4;  // R, G, B, A
      out[i] = static_cast<unsigned short>((p[3] ? 0x8000 : 0) | (p[0] >> 3) << 10 | (p[1] >> 3) << 5 | p[2] >> 3);
    }
    m_texture->UpdateEnd();
    m_texture->SetSwizzleFlag();
    dst->UpdateEnd();
    dst->SetSwizzleFlag();
  }
  return 1;
}

// 8034C70C
void ENgcRenderSurface::Select() {
  m_selected = 1;
  GXCreateFrameBuffer(static_cast<u32>(m_xSize), static_cast<u32>(m_ySize));  // PC: the surface's own framebuffer
}

// 8034C718
// Copies what was drawn into the texture (clearing it) and lets texture fetches see the copy.
void ENgcRenderSurface::Deselect() {
  GXSetCopyFilter(GX_FALSE, nullptr, GX_FALSE, nullptr);
  GXSetTexCopySrc(0, 0, static_cast<u16>(m_xSize), static_cast<u16>(m_ySize));
  ENgcTextureBase* t = m_texture ? m_texture->m_impl : nullptr;
  if (t) {
    GXSetTexCopyDst(t->m_def.m_width, t->m_def.m_height, static_cast<GXTexFmt>(m_texture->GetGXTexFmt()), GX_FALSE);
    GXCopyTex(t->m_image, GX_TRUE);
    if (m_readbackWanted && m_readbackStream) {
      AuroraReadbackCopySrc src{0, t->m_image};
      AuroraReadbackCaptureCopies(m_readbackStream, &src, 1);
    }
  }
  GXPixModeSync();
  GXInvalidateTexAll();
  GXRestoreFrameBuffer();  // PC: back to the screen
  m_selected = 0;
}
