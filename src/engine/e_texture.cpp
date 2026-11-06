// Port of engine/e_texture.cpp: the platform-neutral texture object.
#include "simmy/types_gen.hpp"

// PC: the def is stored big-endian in front of the pixels; the name pointer slot is ignored.
void ETextureDef::ReadBE(const unsigned char* p) {
  m_field00 = simmy::be32(p + 0x00);
  m_field04 = simmy::be32(p + 0x04);
  m_flags = simmy::be32(p + 0x08);
  m_field0C = simmy::be32(p + 0x0C);
  m_width = simmy::be16(p + 0x10);
  m_height = simmy::be16(p + 0x12);
  m_paletteSize = simmy::be16(p + 0x14);
  m_mipCount = simmy::be16(p + 0x16);
  m_format = p[0x18];
  m_paletteFormat = p[0x19];
  m_bpp = p[0x1A];
  m_paletteBpp = p[0x1B];
  m_name = nullptr;
}

// 80364F64
ETexture::ETexture() {}
// 80364FC4
ETexture::~ETexture() { m_magic = 0xdeadbeef; }

// 80365014
int ETexture::Create(ETextureDef& def) {
  m_def = def;
  return 1;
}

// 80365074
void ETexture::Validate() {}
// 80365078
void ETexture::Select(int) {}
// 803650E0
int ETexture::GetImageFormat() { return m_def.m_format; }
// 803650E8
int ETexture::GetBitsPerImagePixel() { return m_def.m_bpp; }
// 803650F0
int ETexture::GetXSize() { return m_def.m_width; }
// 803650F8
int ETexture::GetYSize() { return m_def.m_height; }
// 80365100
int ETexture::GetPaletteFormat() { return m_def.m_paletteFormat; }
// 80365108
int ETexture::GetBitsPerPaletteEntry() { return m_def.m_paletteBpp; }
// 80365110
int ETexture::GetPaletteSize() { return m_def.m_paletteSize; }
// 80365118
unsigned int ETexture::GetFlags() { return m_def.m_flags; }
// 80365120
int ETexture::Lock() { return 1; }
// 80365128
void ETexture::Unlock() {}
// 8036512C
void ETexture::Invalidate() {}
// 80365130
void ETexture::DisableMipmapping() {}
// 80365134
unsigned char* ETexture::UpdateMipLevel(int, int&, int&) { return nullptr; }
// 8036513C
unsigned char* ETexture::UpdatePalette() { return nullptr; }
// 80365144
unsigned char* ETexture::UpdatePaletteDeswizzled() { return UpdatePalette(); }
// 80365178
void ETexture::ClearTileFlags() { m_def.m_flags &= ~3u; }
// 80365188
void ETexture::ClearSwizzleFlag() { m_def.m_flags &= ~0x80u; }
// 80365198
void ETexture::SetSwizzleFlag() { m_def.m_flags |= 0x80u; }
// 803651A8
void ETexture::Test1(int, int, int, int) { std::printf("base test\n"); }
// 803651D4
bool ETexture::IsValid() { return m_magic == 0x900dbeef; }
