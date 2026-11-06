// @class ETextureDef
// What a texture is (engine/e_texture.h): 0x20 big-endian bytes in front of the pixels in a TXFL
// resource. Formats: 0x80 bit set = data is already in GX tile order. 0x81 CMPR, 0x82 RGB5A3, 0x83 C4,
// 0x84 C8, 0x85 RGBA8, 0x89 C4 + 32-bit palette, 0x8A C8 + 32-bit palette; 0/1 are linear sources.
#pragma once

struct ETextureDef {
  unsigned int m_field00 = 0;
  unsigned int m_field04 = 0;
  unsigned int m_flags = 0;        // +0x08: 1/2 wrap S/T, 0x20 mipmapped, 0x80 GX-swizzled, 0x800 keep linear copy
  unsigned int m_field0C = 0;
  unsigned short m_width = 0x40;   // +0x10
  unsigned short m_height = 0x40;  // +0x12
  unsigned short m_paletteSize = 0;  // +0x14 entries
  unsigned short m_mipCount = 0;   // +0x16
  unsigned char m_format = 1;      // +0x18
  unsigned char m_paletteFormat = 0;  // +0x19, nonzero when a palette follows the pixels
  unsigned char m_bpp = 0x20;      // +0x1A bits per pixel
  unsigned char m_paletteBpp = 0;  // +0x1B bits per palette entry
  const char* m_name = nullptr;    // +0x1C

  // PC: fills the fields from the 0x20 big-endian bytes stored in the archive.
  void ReadBE(const unsigned char* p);
  static constexpr unsigned int kFileSize = 0x20;
};
