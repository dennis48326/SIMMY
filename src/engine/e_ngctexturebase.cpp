// Port of engine/e_ngctexturebase.cpp and the per-format classes (e_ngctexturec4.cpp, ..._c8_32.cpp,
// e_ngctexturecmpr.cpp, e_ngctexturergb5a3.cpp, e_ngctexturergba8.cpp).
//
// Each texture owns its GX image (m_image, tile order, as GX reads it; aurora converts it on load).
// Archive textures normally arrive already swizzled (flag 0x80) and are copied straight in.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

int _ngcTexCount = 0;  // live GX textures
int _ngcTexMem = 0;    // bytes of GX image memory
static int sTlut_2208 = 0;
static float lodBias_2212 = 0.0f;

static unsigned char* TexAlloc(unsigned int size) { return static_cast<unsigned char*>(_textureman.Alloc(size, 0x20)); }

// 8034FC40
ENgcTextureBase::ENgcTextureBase(unsigned int gxFormat, int tileWidth, int tileHeight)
    : m_gxFormat(gxFormat), m_tileWidth(static_cast<unsigned char>(tileWidth)),
      m_tileHeight(static_cast<unsigned char>(tileHeight)) {}

// 8034FCBC
ENgcTextureBase::~ENgcTextureBase() {
  --_ngcTexCount;
  _ngcTexMem -= ImageSize();
  delete[] m_linear;
  _textureman.Free(m_image);
  _textureman.Free(m_palette);
  m_palette = m_linear = m_image = nullptr;
}

// 8034FDA0
void ENgcTextureBase::FlushImageMemoryRange(bool) {}  // DCFlushRange: no CPU/GPU cache split on PC

// 8034FE10
int ENgcTextureBase::Lock() {
  ++m_locks;
  return 1;
}
// 8034FE28
void ENgcTextureBase::Unlock() { --m_locks; }
// 8034FE38
void ENgcTextureBase::Invalidate() {}

// 8034FE3C
int ENgcTextureBase::Create(ETextureDef& def) {
  m_texObjReady = 0;
  if (!ETexture::Create(def)) return 0;
  const int size = ImageSize();
  ++_ngcTexCount;
  _ngcTexMem += size;
  m_image = TexAlloc(size);
  if (!(m_def.m_flags & 0x80)) m_linear = new unsigned char[size];  // linear source to swizzle from
  if (m_gxFormat - GX_TF_C4 < 3)  // C4, C8, C14X2: room for the TLUT
    m_palette = TexAlloc((m_def.m_paletteSize * m_def.m_paletteBpp + 7) >> 3);
  return 1;
}

// 8034FF3C
// TLUT slots are handed out round-robin; 16 of them is plenty for one draw.
int ENgcTextureBase::GetPaletteLocation() {
  sTlut_2208 = (sTlut_2208 + 1) % 16;
  return sTlut_2208;
}

// 8034FF64
void ENgcTextureBase::Select(int, int texMap) {
  if (!m_texObjReady) {
    m_texObjReady = 1;
    if (!m_palette) {
      GXInitTexObj(&m_texObj, m_image, m_def.m_width, m_def.m_height, static_cast<GXTexFmt>(m_gxFormat),
                   static_cast<GXTexWrapMode>(m_wrapS), static_cast<GXTexWrapMode>(m_wrapT),
                   (m_def.m_flags & 0x20) ? GX_TRUE : GX_FALSE);
    } else {
      m_tlut = GetPaletteLocation();
      GXInitTlutObj(&m_tlutObj, m_palette, GX_TL_RGB5A3, m_def.m_paletteSize);
      GXInitTexObjCI(&m_texObj, m_image, m_def.m_width, m_def.m_height, static_cast<GXCITexFmt>(m_gxFormat),
                     static_cast<GXTexWrapMode>(m_wrapS), static_cast<GXTexWrapMode>(m_wrapT), GX_FALSE,
                     static_cast<u32>(m_tlut));
    }
    if (!(m_def.m_flags & 0x20)) {
      GXInitTexObjLOD(&m_texObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    } else {
      const float maxLod = static_cast<float>(m_def.m_mipCount > 0 ? m_def.m_mipCount - 1 : 0);
      GXInitTexObjLOD(&m_texObj, m_palette ? GX_LIN_MIP_NEAR : GX_LIN_MIP_LIN, GX_LINEAR, 0.0f, maxLod,
                      lodBias_2212, GX_FALSE, GX_TRUE, GX_ANISO_4);
    }
  }
  if (m_palette) {
    m_tlut = GetPaletteLocation();
    GXInitTexObjTlut(&m_texObj, static_cast<u32>(m_tlut));
    GXLoadTlut(&m_tlutObj, static_cast<u32>(m_tlut));
  }
  GXLoadTexObj(&m_texObj, static_cast<GXTexMapID>(texMap));
}

// 8035010C
int ENgcTextureBase::UpdateBegin(ETextureUpdateType type) {
  m_mutex.Acquire(0xFFFFFFFFu);
  m_updateType = type;
  return 1;
}

// 8035014C
// Where to write mip `level`: the GX image for pre-swizzled data, otherwise the linear copy.
unsigned char* ENgcTextureBase::UpdateMipLevel(int level, int& pitch, int& rows) {
  unsigned char* base;
  if (m_def.m_flags & 0x80) {
    base = m_image;
  } else {
    if (!m_linear && m_updateType != 0) m_linear = new unsigned char[ImageSize()];
    base = m_linear ? m_linear : m_image;
  }
  const int offset = ComputeMipLevelOffset(level, pitch, rows);
  pitch = (pitch * m_def.m_bpp + 7) >> 3;
  return base + offset;
}

// 80350238
unsigned char* ENgcTextureBase::UpdatePalette() { return m_palette; }

// 80350240
void ENgcTextureBase::UpdateEnd() {
  if (m_updateType != ETextureUpdateType::None) {
    m_wrapS = (m_def.m_flags & 1) ? GX_REPEAT : GX_CLAMP;
    m_wrapT = (m_def.m_flags & 2) ? GX_REPEAT : GX_CLAMP;
    if (!(m_def.m_flags & 0x80)) {
      if (m_updateType != ETextureUpdateType::PaletteOnly) {
        const int levels = m_def.m_mipCount > 1 ? m_def.m_mipCount : 1;
        for (int i = 0; i < levels; ++i) {
          int w, h;
          const int offset = ComputeMipLevelOffset(i, w, h);
          SwizzleImage(m_linear + offset, w, h, m_image + offset);
        }
      }
      if (!(m_def.m_flags & 0x800) && m_linear) {
        delete[] m_linear;
        m_linear = nullptr;
      }
    }
  }
  m_texObjReady = 0;  // PC: re-init the GX object so aurora picks up the new pixels
  m_mutex.Release();
}

// 803503A8
// Byte offset of mip `level` (each level 32-byte aligned); width/height = that level's size rounded
// up to whole tiles.
int ENgcTextureBase::ComputeMipLevelOffset(int level, int& width, int& height) {
  auto alignUp = [](int v, int a) { return (v - 1 + a) & -a; };
  int w = m_def.m_width, h = m_def.m_height, offset = 0;
  const int bits = m_def.m_bpp == 24 ? 32 : m_def.m_bpp;
  width = alignUp(w, m_tileWidth);
  height = alignUp(h, m_tileHeight);
  for (; level > 0; --level) {
    offset += (((width * height * bits + 7) >> 3) + 0x1f) & ~0x1f;
    w >>= 1;
    h >>= 1;
    width = alignUp(w, m_tileWidth);
    height = alignUp(h, m_tileHeight);
  }
  return offset;
}

// 80350470
int ENgcTextureBase::GetTEVStageCount() { return 1; }
// 80350478
int ENgcTextureBase::GetTEXCount() { return 1; }

// 80350480
// Rebuilds the linear copy from the GX image (so the CPU can edit it) and clears the swizzled flag.
void ENgcTextureBase::ClearSwizzleFlag() {
  if (m_def.m_flags & 0x80) {
    if (!m_linear) m_linear = new unsigned char[ImageSize()];
    UpdateBegin(ETextureUpdateType::Image);
    const int levels = m_def.m_mipCount > 1 ? m_def.m_mipCount : 1;
    for (int i = 0; i < levels; ++i) {
      int w, h;
      const int offset = ComputeMipLevelOffset(i, w, h);
      UnswizzleImage(m_image + offset, w, h, m_linear + offset);
    }
    UpdateEnd();  // still flagged swizzled: keeps the linear copy for the CPU update that follows
  }
  m_def.m_flags &= ~0x80u;
}

// 803505B8
void ENgcTextureBase::ClearTileFlags() {
  m_wrapT = m_wrapS = 0;
  m_def.m_flags &= ~3u;
}

// 803505D4
unsigned char* ENgcTextureBase::UpdatePaletteDeswizzled() { return UpdatePalette(); }
// 80350608
void ENgcTextureBase::SetSwizzleFlag() { m_def.m_flags |= 0x80u; }
// 80350618
void ENgcTextureBase::UnswizzleImage(unsigned char*, int, int, unsigned char*) {}

// ---- Formats -----------------------------------------------------------------------------------

// 8035061C
ENgcTextureC4::ENgcTextureC4() : ENgcTextureBase(GX_TF_C4, 8, 8) {}
// 80350664
ENgcTextureC4::~ENgcTextureC4() {}
// 803506BC
void ENgcTextureC4::SwizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleToC4(s, w, h, d); }
void ENgcTextureC4::UnswizzleImage(unsigned char*, int, int, unsigned char*) {}  // (base: none)

// 80350C84
ENgcTextureC8::ENgcTextureC8() : ENgcTextureBase(GX_TF_C8, 8, 4) {}
// 80350CCC
ENgcTextureC8::~ENgcTextureC8() {}
// 80350D24
void ENgcTextureC8::SwizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleToC8(s, w, h, d); }
// 80350D54
void ENgcTextureC8::UnswizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleFromC8(s, w, h, d); }

// 80351494
ENgcTextureCMPR::ENgcTextureCMPR() : ENgcTextureBase(GX_TF_CMPR, 8, 8) {}
// 803514DC
ENgcTextureCMPR::~ENgcTextureCMPR() {}
// 80351534
void ENgcTextureCMPR::SwizzleImage(unsigned char*, int, int, unsigned char*) {}  // only ever stored pre-swizzled
void ENgcTextureCMPR::UnswizzleImage(unsigned char*, int, int, unsigned char*) {}

// 8035159C
ENgcTextureRGB5A3::ENgcTextureRGB5A3() : ENgcTextureBase(GX_TF_RGB5A3, 4, 4) {}
// 803515E4
ENgcTextureRGB5A3::~ENgcTextureRGB5A3() {}
// 8035163C
void ENgcTextureRGB5A3::SwizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleToRGB5A3(s, w, h, d); }
// 8035166C
void ENgcTextureRGB5A3::UnswizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleFromRGB5A3(s, w, h, d); }

// 80351700
ENgcTextureRGBA8::ENgcTextureRGBA8() : ENgcTextureBase(GX_TF_RGBA8, 4, 4) {}
// 80351748
ENgcTextureRGBA8::~ENgcTextureRGBA8() {}
// 803517A0
void ENgcTextureRGBA8::SwizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleToRGBA8(s, w, h, d); }
// 803517D0
void ENgcTextureRGBA8::UnswizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleFromRGBA8(s, w, h, d); }

// ---- 32-bit palettes: two IA8 TLUTs ------------------------------------------------------------

// Shared by C4_32 and C8_32. In GX-swizzled data the two half palettes are stored one after the other.
template <class T>
static int Create32(T* t, ETextureDef& def) {
  if (!t->ENgcTextureBase::Create(def)) return 0;
  if (!(t->m_def.m_flags & 0x80)) {
    t->m_paletteRG = TexAlloc(def.m_paletteSize * 2u);
    t->m_paletteBA = TexAlloc(def.m_paletteSize * 2u);
  } else {
    t->m_paletteRG = t->m_palette;
    t->m_paletteBA = t->m_palette + def.m_paletteSize * 2u;
  }
  return 1;
}

// RGBA entries -> (G, R) and (A, B) IA8 halves.
template <class T>
static void SplitPalette(T* t, const unsigned char* rgba) {
  for (unsigned int i = 0; i < t->m_def.m_paletteSize; ++i, rgba += 4) {
    t->m_paletteRG[i * 2] = rgba[1];
    t->m_paletteRG[i * 2 + 1] = rgba[0];
    t->m_paletteBA[i * 2] = rgba[3];
    t->m_paletteBA[i * 2 + 1] = rgba[2];
  }
}

template <class T>
static void Select32(T* t, int texMap) {
  if (!t->m_texObjReady) {
    t->m_texObjReady = 1;
    t->m_tlut = ENgcTextureBase::GetPaletteLocation();
    GXInitTlutObj(&t->m_tlutObj, t->m_paletteRG, GX_TL_IA8, t->m_def.m_paletteSize);
    GXInitTexObjCI(&t->m_texObj, t->m_image, t->m_def.m_width, t->m_def.m_height,
                   static_cast<GXCITexFmt>(t->m_gxFormat), static_cast<GXTexWrapMode>(t->m_wrapS),
                   static_cast<GXTexWrapMode>(t->m_wrapT), GX_FALSE, static_cast<u32>(t->m_tlut));
    t->m_tlut2 = ENgcTextureBase::GetPaletteLocation();
    GXInitTlutObj(&t->m_tlutObj2, t->m_paletteBA, GX_TL_IA8, t->m_def.m_paletteSize);
    GXInitTexObjCI(&t->m_texObj2, t->m_image, t->m_def.m_width, t->m_def.m_height,
                   static_cast<GXCITexFmt>(t->m_gxFormat), static_cast<GXTexWrapMode>(t->m_wrapS),
                   static_cast<GXTexWrapMode>(t->m_wrapT), GX_FALSE, static_cast<u32>(t->m_tlut2));
    const bool mips = t->m_def.m_flags & 0x20;
    const float maxLod = mips ? static_cast<float>(t->m_def.m_mipCount > 0 ? t->m_def.m_mipCount - 1 : 0) : 0.0f;
    const GXTexFilter minFilter = mips ? GX_LIN_MIP_NEAR : GX_LINEAR;
    GXInitTexObjLOD(&t->m_texObj, minFilter, GX_LINEAR, 0.0f, maxLod, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXInitTexObjLOD(&t->m_texObj2, minFilter, GX_LINEAR, 0.0f, maxLod, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
  }
  t->m_tlut = ENgcTextureBase::GetPaletteLocation();
  t->m_tlut2 = ENgcTextureBase::GetPaletteLocation();
  GXInitTexObjTlut(&t->m_texObj, static_cast<u32>(t->m_tlut));
  GXLoadTlut(&t->m_tlutObj, static_cast<u32>(t->m_tlut));
  GXInitTexObjTlut(&t->m_texObj2, static_cast<u32>(t->m_tlut2));
  GXLoadTlut(&t->m_tlutObj2, static_cast<u32>(t->m_tlut2));
  GXLoadTexObj(&t->m_texObj, static_cast<GXTexMapID>(texMap));
  GXLoadTexObj(&t->m_texObj2, static_cast<GXTexMapID>(texMap + 1));
}

// 80350750
ENgcTextureC4_32::ENgcTextureC4_32() : ENgcTextureBase(GX_TF_C4, 8, 8) {}
// 803507A4
ENgcTextureC4_32::~ENgcTextureC4_32() {
  if (!(m_def.m_flags & 0x80)) {
    _textureman.Free(m_paletteRG);
    _textureman.Free(m_paletteBA);
  }
}
// 80350828
int ENgcTextureC4_32::Create(ETextureDef& def) { return Create32(this, def); }
// 803508C0
void ENgcTextureC4_32::SwizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleToC4(s, w, h, d); }
void ENgcTextureC4_32::UnswizzleImage(unsigned char*, int, int, unsigned char*) {}
unsigned char* ENgcTextureC4_32::UpdatePaletteDeswizzled() { return UpdatePalette(); }
// 803508F0
void ENgcTextureC4_32::UpdateEnd() {
  if (m_updateType != ETextureUpdateType::PaletteOnly && !(m_def.m_flags & 0x80)) SplitPalette(this, m_palette);
  ENgcTextureBase::UpdateEnd();
}
// 803509C4
void ENgcTextureC4_32::Select(int, int texMap) { Select32(this, texMap); }
// 80350C10
int ENgcTextureC4_32::GetTEVStageCount() { return 2; }
// 80350C18
int ENgcTextureC4_32::GetTEXCount() { return 2; }

// 80350DE8
ENgcTextureC8_32::ENgcTextureC8_32() : ENgcTextureBase(GX_TF_C8, 8, 4) {}
// 80350E40
ENgcTextureC8_32::~ENgcTextureC8_32() {
  if (!(m_def.m_flags & 0x80)) {
    _textureman.Free(m_paletteRG);
    _textureman.Free(m_paletteBA);
  }
  _textureman.Free(m_palette32);
}
// 80350EE4
int ENgcTextureC8_32::Create(ETextureDef& def) { return Create32(this, def); }
// 80350F7C
void ENgcTextureC8_32::SwizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleToC8(s, w, h, d); }
// 80350FAC
void ENgcTextureC8_32::UnswizzleImage(unsigned char* s, int w, int h, unsigned char* d) { ENgcTextureUtil::SwizzleFromC8(s, w, h, d); }

// 80350FDC
// An RGBA copy of the palette for the CPU to edit; UpdateEnd splits it back into the two halves.
unsigned char* ENgcTextureC8_32::UpdatePaletteDeswizzled() {
  m_palette32 = TexAlloc(m_def.m_paletteSize * 4u);
  if (m_palette32)
    for (unsigned int i = 0; i < m_def.m_paletteSize; ++i) {
      m_palette32[i * 4 + 0] = m_paletteRG[i * 2 + 1];
      m_palette32[i * 4 + 1] = m_paletteRG[i * 2];
      m_palette32[i * 4 + 2] = m_paletteBA[i * 2 + 1];
      m_palette32[i * 4 + 3] = m_paletteBA[i * 2];
    }
  return m_palette32;
}

// 80351098
void ENgcTextureC8_32::UpdateEnd() {
  if (m_updateType != ETextureUpdateType::PaletteOnly && (m_palette32 || !(m_def.m_flags & 0x80)))
    SplitPalette(this, m_palette32 ? m_palette32 : m_palette);
  _textureman.Free(m_palette32);
  m_palette32 = nullptr;
  ENgcTextureBase::UpdateEnd();
}

// 803511D4
void ENgcTextureC8_32::Select(int, int texMap) { Select32(this, texMap); }
// 80351420
int ENgcTextureC8_32::GetTEVStageCount() { return 2; }
// 80351428
int ENgcTextureC8_32::GetTEXCount() { return 2; }
