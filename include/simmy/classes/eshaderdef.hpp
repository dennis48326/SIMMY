// @class EShaderDef
// A material + up to two texture layers (engine/e_shader.h). In the archives it is 0xE4 big-endian
// bytes; at run time the texture slots hold pointers, so on PC it is a native struct filled by ReadBE.
#pragma once

struct EShaderLayer {
  ETexture* m_texture = nullptr;      // +0x00 file: texture id (0 = none); later the ETexture
  ETexture** m_multiTextures = nullptr;  // +0x04 layer 0 of a multi-texture shader: one per frame
  unsigned char* m_extraData = nullptr;  // +0x08 layer 0 of a multi-texture shader: extra bytes
  unsigned int m_field0C = 8;         // +0x0C (bit 0x40: blend modes below are in use)
  unsigned int m_flags = 0x18;        // +0x10 (0x40: texture scrolls)
  float m_field14 = 0.5f;             // +0x14 alpha test threshold
  unsigned int m_field18 = 0;         // +0x18
  unsigned char m_blend[4] = {0, 1, 0, 1};  // +0x1C TEV blend selectors
  unsigned char m_alpha = 0xff;       // +0x20 (doubled on load, 0x7f.. -> 0xff)
  unsigned char m_field21 = 0, m_texGen = 0, m_field23 = 0;  // +0x21.. (0x22: texgen mode)
  unsigned char m_multiTextureCount = 0;  // +0x24
  unsigned char m_field25 = 0;
  unsigned short m_extraDataSize = 0; // +0x26
  float m_scrollU = 0, m_scrollV = 0; // +0x28
  float m_texScale[2] = {1, 1};      // +0x30 texture coordinate scale
  float m_texOffset[2] = {};          // +0x38 texture coordinate offset
};

struct EShaderDef {
  unsigned char m_layerCount = 1;     // +0x00
  unsigned char m_field01 = 0;
  unsigned short m_field02 = 0;
  unsigned int m_surface = 0;         // +0x04 surface properties (0x04000000 multi-texture, 0x80000000 animated)
  unsigned int m_geometryModes = 8;   // +0x08
  unsigned int m_field0C = 0;
  float m_material[12] = {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 10};  // +0x10 EMaterial: colors, specular, power
  int m_field40[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};      // +0x40
  EShaderLayer m_layers[2];           // +0x64

  // PC: fills the fields from the 0xE4 big-endian bytes of a SHDR resource; returns the bytes after.
  const unsigned char* ReadBE(const unsigned char* p);
  static constexpr unsigned int kFileSize = 0xE4;
};
