// Port of engine/e_ngctextureutil.cpp: linear <-> GX tile order. GX stores images as tiles of
// 32 bytes (RGBA8: two 32-byte halves, alpha/red then green/blue), left to right, top to bottom.
#include "simmy/types_gen.hpp"

#include <cstring>

// Copies between linear rows and tiles of tileW x tileH pixels at `bits` per pixel.
static void TileCopy(unsigned char* linear, int width, int height, unsigned char* tiled, int tileW, int tileH,
                     int bits, bool toTiled) {
  const int rowBytes = width * bits / 8, tileRowBytes = tileW * bits / 8;
  for (int ty = 0; ty < height / tileH; ++ty)
    for (int tx = 0; tx < width / tileW; ++tx)
      for (int r = 0; r < tileH; ++r) {
        unsigned char* l = linear + (ty * tileH + r) * rowBytes + tx * tileRowBytes;
        if (toTiled) std::memcpy(tiled, l, tileRowBytes);
        else std::memcpy(l, tiled, tileRowBytes);
        tiled += tileRowBytes;
      }
}

// 80351864
void ENgcTextureUtil::SwizzleToRGB5A3(unsigned char* src, int w, int h, unsigned char* dst) { TileCopy(src, w, h, dst, 4, 4, 16, true); }
// 80351910
void ENgcTextureUtil::SwizzleToC4(unsigned char* src, int w, int h, unsigned char* dst) { TileCopy(src, w, h, dst, 8, 8, 4, true); }
// 803519A4
void ENgcTextureUtil::SwizzleToC8(unsigned char* src, int w, int h, unsigned char* dst) { TileCopy(src, w, h, dst, 8, 4, 8, true); }
// 80351B84
void ENgcTextureUtil::SwizzleFromRGB5A3(unsigned char* src, int w, int h, unsigned char* dst) { TileCopy(dst, w, h, src, 4, 4, 16, false); }
// 80351C30
void ENgcTextureUtil::SwizzleFromC8(unsigned char* src, int w, int h, unsigned char* dst) { TileCopy(dst, w, h, src, 8, 4, 8, false); }

// 80351A5C
// Linear RGBA bytes -> 4x4 tiles: 32 bytes of (A, R) pairs, then 32 bytes of (G, B) pairs.
void ENgcTextureUtil::SwizzleToRGBA8(unsigned char* src, int w, int h, unsigned char* dst) {
  for (int ty = 0; ty < h / 4; ++ty)
    for (int tx = 0; tx < w / 4; ++tx, dst += 64)
      for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
          const unsigned char* p = src + ((ty * 4 + r) * w + tx * 4 + c) * 4;
          unsigned char* ar = dst + (r * 4 + c) * 2;
          ar[0] = p[3];
          ar[1] = p[0];
          ar[32] = p[1];
          ar[33] = p[2];
        }
}

// 80351CE8
void ENgcTextureUtil::SwizzleFromRGBA8(unsigned char* src, int w, int h, unsigned char* dst) {
  for (int ty = 0; ty < h / 4; ++ty)
    for (int tx = 0; tx < w / 4; ++tx, src += 64)
      for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
          unsigned char* p = dst + ((ty * 4 + r) * w + tx * 4 + c) * 4;
          const unsigned char* ar = src + (r * 4 + c) * 2;
          p[3] = ar[0];
          p[0] = ar[1];
          p[1] = ar[32];
          p[2] = ar[33];
        }
}
