// @class ETexture after ETextureDef ETextureUpdateType
// A texture as the engine sees it (engine/e_texture.cpp). Platform subclasses own the pixel memory.
#pragma once

struct ETexture {
  ETexture();
  // Virtuals, in vtable order.
  virtual ~ETexture();
  virtual void Select(int stage);
  virtual int Lock();
  virtual void Unlock();
  virtual void Invalidate();
  virtual void DisableMipmapping();
  virtual int UpdateBegin(ETextureUpdateType type) = 0;
  // Returns where to write mip `level`; pitch = bytes per row, rows = rows.
  virtual unsigned char* UpdateMipLevel(int level, int& pitch, int& rows);
  virtual unsigned char* UpdatePalette();
  virtual unsigned char* UpdatePaletteDeswizzled();
  virtual void ClearTileFlags();
  virtual void ClearSwizzleFlag();
  virtual void SetSwizzleFlag();
  virtual void UpdateEnd() = 0;
  virtual int Create(ETextureDef& def);
  virtual void Test1(int, int, int, int);
  virtual void Validate();

  int GetImageFormat();
  int GetBitsPerImagePixel();
  int GetXSize();
  int GetYSize();
  int GetPaletteFormat();
  int GetBitsPerPaletteEntry();
  int GetPaletteSize();
  unsigned int GetFlags();
  bool IsValid();

  ETextureDef m_def;                 // +0x00
  unsigned int m_magic = 0x900dbeef;  // +0x20, 0xdeadbeef once destroyed
};
