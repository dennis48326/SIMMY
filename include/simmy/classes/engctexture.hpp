// @class ENgcTexture after ETexture ENgcTextureBase
// The texture object handed to the engine (engine/e_ngctexture.cpp). It forwards to the real
// format-specific texture, picked when Create sees the format.
#pragma once

struct ENgcTexture : public ETexture {
  ENgcTexture();
  ~ENgcTexture() override;
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
  using ETexture::Select;
  virtual void Select(int unused, int texMap);
  int GetGXTexFmt();
  void FlushImageMemoryRange(bool sync);
  int GetTEVStageCount();
  int GetTEXCount();
  ENgcTexture* CloneSwizzled();

  ENgcTextureBase* m_impl = nullptr;  // +0x28
};
