// @class ENgcTextureC8_32 after ENgcTextureBase
// C8 indices with a 32-bit RGBA palette (engine/e_ngctexturec8_32.cpp). GX palettes are 16-bit,
// so the palette is split into two IA8 TLUTs (R/G and B/A halves) and the texture is bound twice;
// the renderer's TEV stages recombine the halves.
#pragma once

struct ENgcTextureC8_32 : public ENgcTextureBase {
  ENgcTextureC8_32();
  ~ENgcTextureC8_32() override;
  int Create(ETextureDef& def) override;
  void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  unsigned char* UpdatePaletteDeswizzled() override;
  void UpdateEnd() override;
  using ENgcTextureBase::Select;
  void Select(int unused, int texMap) override;
  int GetTEVStageCount() override;
  int GetTEXCount() override;

  GXTexObj m_texObj2{};                // +0x9C
  GXTlutObj m_tlutObj2{};              // +0xBC
  int m_tlut2 = 0;                     // +0xC8
  unsigned char* m_paletteRG = nullptr;  // +0xCC IA8 entries (G, R)
  unsigned char* m_paletteBA = nullptr;  // +0xD0 IA8 entries (A, B)
  unsigned char* m_palette32 = nullptr;  // +0xD4 RGBA staging copy (UpdatePaletteDeswizzled)
};
