// @class TextureUtils after ETexture
// CPU texture helpers (engine/textureutils.cpp): copies between 8-bit palettized and 32-bit RGBA
// textures and pixel blending. 32-bit pixels are handled as the GameCube's 0xRRGGBBAA words (the
// linear bytes R, G, B, A).
#pragma once

struct TextureUtils {
  static int LoadTexture(unsigned int id, ETexture* dst);  // 8030B108
  static RET_UNKNOWN CopyTexture32To8(ETexture *, ETexture *);  // 8030B228
  static int CopyTexture8To32(ETexture* src, ETexture* dst);  // 8030B5AC
  static int CopyTexture32To32(ETexture* src, ETexture* dst);  // 8030B7B0
  static RET_UNKNOWN CopyTexture8To8(ETexture *, ETexture *);  // 8030B960
  static RET_UNKNOWN CopyTexture32To32Half(ETexture *, ETexture *, unsigned char, unsigned int);  // 8030BB9C
  static void VerticalFlip(ETexture* t);  // 8030BE60
  static void MakeTextureSepiaTone(ETexture *);  // 8030BFA0
  static void ConvertColorArrayToSepia(unsigned char *, unsigned char *);  // 8030C1D4
  static void SwapRB(ETexture *);  // 8030C2CC
  static bool PrepareTextureForUpdateBegin(ETexture* t);  // 8030C5B4
  static void PostUpdateEnd(ETexture* t, bool swizzled);  // 8030C618
  static unsigned int BlendPixels32(unsigned int dst, unsigned int src);  // 8030C65C

  // PC: 32-bit pixel words from/to linear RGBA bytes.
  static unsigned int GetPixel(const unsigned char* p) { return unsigned(p[0]) << 24 | unsigned(p[1]) << 16 | unsigned(p[2]) << 8 | p[3]; }
  static void PutPixel(unsigned char* p, unsigned int v) {
    p[0] = static_cast<unsigned char>(v >> 24), p[1] = static_cast<unsigned char>(v >> 16);
    p[2] = static_cast<unsigned char>(v >> 8), p[3] = static_cast<unsigned char>(v);
  }
};
