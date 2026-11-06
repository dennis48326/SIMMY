// Port of engine/e_ngctexture.cpp: the engine-facing texture, which picks the GX format class on
// Create and forwards everything to it.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

// 8034F43C
ENgcTexture::ENgcTexture() {}
extern ENgcRenderer _ngcRenderer;

// 8034F480
ENgcTexture::~ENgcTexture() {
  // PC: a texture destroyed while the renderer still has it selected leaves its slot, as
  // SetTexture(nullptr) would. GC's next SetTexture unlocked it through the freed object's base
  // vtable (a no-op); on PC that is a use after free (the genetics screen's parent portraits).
  for (int i = 0; i < 2; ++i)
    if (_ngcRenderer.m_textures[i] == this) {
      _ngcRenderer.m_textures[i] = nullptr;
      if (_ngcRenderer.m_numTextures) --_ngcRenderer.m_numTextures;
    }
  delete m_impl;
  m_impl = nullptr;
}

// 8034F50C
// A copy of this texture whose data is marked as already in GX order.
ENgcTexture* ENgcTexture::CloneSwizzled() {
  ETextureDef def = m_def;
  def.m_flags |= 0x80;
  auto* copy = static_cast<ENgcTexture*>(_pGfx->CreateTexture(def));
  if (!copy) return nullptr;
  copy->UpdateBegin(ETextureUpdateType::Image);
  int pitch, rows, w, h;
  unsigned char* dst = copy->UpdateMipLevel(0, pitch, rows);
  std::memcpy(dst, m_impl->m_image, m_impl->ComputeMipLevelOffset(1, w, h));
  if (m_impl->m_palette)
    std::memcpy(copy->UpdatePalette(), m_impl->m_palette, (m_def.m_paletteSize * m_def.m_paletteBpp + 7) >> 3);
  copy->UpdateEnd();
  return copy;
}

// 8034F6AC
int ENgcTexture::Lock() { return m_impl->Lock(); }
// 8034F6E4
void ENgcTexture::Unlock() { m_impl->Unlock(); }
// 8034F71C
void ENgcTexture::Invalidate() { m_impl->Invalidate(); }
// 8034F754
int ENgcTexture::UpdateBegin(ETextureUpdateType type) { return m_impl->UpdateBegin(type); }
// 8034F78C
unsigned char* ENgcTexture::UpdateMipLevel(int level, int& pitch, int& rows) { return m_impl->UpdateMipLevel(level, pitch, rows); }
// 8034F7C4
unsigned char* ENgcTexture::UpdatePalette() { return m_impl->UpdatePalette(); }
// 8034F7FC
unsigned char* ENgcTexture::UpdatePaletteDeswizzled() { return m_impl->UpdatePaletteDeswizzled(); }
// 8034F834
void ENgcTexture::UpdateEnd() { m_impl->UpdateEnd(); }
// 8034FA38
void ENgcTexture::Select(int unused, int texMap) { m_impl->Select(unused, texMap); }
// 8034FA70
int ENgcTexture::GetGXTexFmt() { return static_cast<int>(m_impl->m_gxFormat); }
// 8034FA7C
void ENgcTexture::FlushImageMemoryRange(bool sync) { m_impl->FlushImageMemoryRange(sync); }
// 8034FAA0
int ENgcTexture::GetTEVStageCount() { return m_impl->GetTEVStageCount(); }
// 8034FAD8
int ENgcTexture::GetTEXCount() { return m_impl->GetTEXCount(); }
// 8034FB10
void ENgcTexture::ClearTileFlags() { m_impl->ClearTileFlags(); }
// 8034FB54
void ENgcTexture::ClearSwizzleFlag() {
  m_def.m_flags &= ~0x80u;
  m_impl->ClearSwizzleFlag();
}
// 8034FB98
void ENgcTexture::SetSwizzleFlag() {
  m_def.m_flags |= 0x80u;
  m_impl->SetSwizzleFlag();
}

// 8034F86C
int ENgcTexture::Create(ETextureDef& def) {
  ETexture::Create(def);
  const unsigned short bpps = static_cast<unsigned short>(def.m_bpp << 8 | def.m_paletteBpp);
  switch (def.m_format) {
  case 0x83: m_impl = new ENgcTextureC4; break;
  case 0x84: m_impl = new ENgcTextureC8; break;
  case 0x81: m_impl = new ENgcTextureCMPR; break;
  case 0x82: m_impl = new ENgcTextureRGB5A3; break;
  case 0x01:
  case 0x85: m_impl = new ENgcTextureRGBA8; break;
  case 0x89: m_impl = new ENgcTextureC4_32; break;
  case 0x8A: m_impl = new ENgcTextureC8_32; break;
  case 0x00:
    if (bpps == 0x820) m_impl = new ENgcTextureC8_32;
    else if (bpps == 0x420) m_impl = new ENgcTextureC4_32;
    break;
  default: break;
  }
  return m_impl ? m_impl->Create(def) : 0;
}
