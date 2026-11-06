// @class ENgcTextureBase after ETexture EMutex
// A GX texture in one format (engine/e_ngctexturebase.cpp): the GX-ordered image, an optional linear
// copy for CPU updates, and a TLUT for paletted formats. Subclasses pick the format and the swizzle.
#pragma once

struct ENgcTextureBase : public ETexture {
  ENgcTextureBase(unsigned int gxFormat, int tileWidth, int tileHeight);
  ~ENgcTextureBase() override;
  int Lock() override;
  void Unlock() override;
  void Invalidate() override;
  int UpdateBegin(ETextureUpdateType type) override;
  unsigned char* UpdateMipLevel(int level, int& pitch, int& rows) override;
  unsigned char* UpdatePalette() override;
  unsigned char* UpdatePaletteDeswizzled() override;
  void ClearTileFlags() override;
  void ClearSwizzleFlag() override;
  void SetSwizzleFlag() override;
  void UpdateEnd() override;
  int Create(ETextureDef& def) override;
  virtual int GetTEVStageCount();
  virtual int GetTEXCount();
  using ETexture::Select;
  virtual void Select(int unused, int texMap);
  virtual void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) = 0;
  virtual void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst);

  void FlushImageMemoryRange(bool sync);
  static int GetPaletteLocation();
  int ComputeMipLevelOffset(int level, int& width, int& height);
  int ImageSize() { int w, h; return ComputeMipLevelOffset(m_def.m_mipCount > 1 ? m_def.m_mipCount : 1, w, h); }

  EMutex m_mutex;                    // +0x28
  int m_locks = 0;                   // +0x44
  GXTexObj m_texObj{};               // +0x48
  int m_wrapS = 0;                   // +0x68 (GX_CLAMP / GX_REPEAT)
  int m_wrapT = 0;                   // +0x6C
  unsigned int m_gxFormat;           // +0x70
  unsigned char* m_image = nullptr;  // +0x74 GX tile order, 32-byte aligned
  unsigned char* m_linear = nullptr; // +0x78 linear copy for CPU updates
  unsigned char m_tileWidth;         // +0x7C
  unsigned char m_tileHeight;        // +0x7D
  int m_texObjReady = 0;             // +0x80
  GXTlutObj m_tlutObj{};             // +0x84
  unsigned char* m_palette = nullptr;  // +0x90
  int m_tlut = 0;                    // +0x94 GX_TLUT0..15
  int m_updateType = 0;              // +0x98
};
