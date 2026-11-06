// @class SkinCompositor after CasSimDescriptionS2C ERTexture
// Composites a sim's skin map quadrant (game/skincompositor.cpp): the skin colour's base skin, then
// every part texture of the quadrant blended into its rectangle (alpha-blended, or copied for map 2),
// with the parts' reflection masks kept in the low alpha bits, into a 32-bit scratch texture that is
// finally copied into the sim's skin texture.
#pragma once

// The skin maps: gSimPartsMapSize (128) pixels square per multiplier.
constexpr short gSimPartsMapSize = 128;
constexpr short gSimPartsMapSizeMultiplier_Game = 1;
extern short gSimPartsMapSizeMultiplier_CAS;  // 2, 1 when the game's models are forced

// 0x28 bytes.
struct SkinCompositor {
  SkinCompositor();  // 80075B10
  ~SkinCompositor();  // 80075B70
  SkinCompositor(const SkinCompositor&) = delete;
  SkinCompositor& operator=(const SkinCompositor&) = delete;
  void Reset();  // 80075BB0
  void CreateSkinQuadrantHiRes(ERTexture* target, unsigned char quadrant, CasSimDescriptionS2C* desc);  // 80075C80
  void CreateSkinQuadrantLoRes(ERTexture* target, unsigned char quadrant, CasSimDescriptionS2C* desc);  // 80075CA4
  int CreateSkinQuadrantPrecomposited(ERTexture* target, unsigned int id, bool highRes);  // 80075CC8
  void CreateSkin(ERTexture* target, unsigned char quadrant, CasSimDescriptionS2C* desc, bool lowRes);  // 80075DEC
  void InitScratchpadTextures(unsigned short size);  // 80075E8C
  void InitSkinTexture();  // 80076000
  void FinalizeSkinTexture();  // 8007604C
  void LoadAllTextureLayers();  // 80076098
  void GetTextureIDs(unsigned int& skin, unsigned int* tattoos, unsigned int* textures, unsigned int* reflections);  // 80076524
  void BlendTexture(eBodyPartS2C part, unsigned int texture, unsigned int reflection);  // 80076618
  void GetFinalReflectionMask();  // 80076DA4

  unsigned short m_size = 0x100;        // +0x00 the map's size in pixels
  int m_targetIs8Bit = 0;               // +0x04 the target is palettized (GC skin maps)
  ETexture* m_scratch = nullptr;        // +0x08 "*skincompositor*", 32-bit RGBA
  unsigned char* m_pixels = nullptr;    // +0x0C the scratch map's pixels while locked
  int m_lowRes = 0;                     // +0x10
  CasSimDescriptionS2C* m_desc = nullptr;  // +0x14
  ERTexture* m_target = nullptr;        // +0x18
  unsigned char m_quadrant = 0;         // +0x1C eSimPartsMapLocation
  unsigned char* m_mask = nullptr;      // +0x20 reflection mask, one byte per pixel
  CasSimPartsS2C* m_simParts = nullptr; // +0x24
};
