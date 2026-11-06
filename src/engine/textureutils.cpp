// Port of engine/textureutils.cpp (partial): copying textures between the 8-bit palettized and 32-bit
// formats through their CPU update interface, and pixel blending. Still stubbed: the 32 -> 8 bit
// quantiser, 8 -> 8 copies, sepia and red/blue swaps.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>
#include <vector>

namespace {
// The palette a CPU copy reads (RGBA words): the deswizzled one for C8 + 32-bit palette (0x8A).
unsigned char* SourcePalette(ETexture* t) {
  return t->m_def.m_format == 0x8a ? t->UpdatePaletteDeswizzled() : t->UpdatePalette();
}
bool Copyable(ETexture* src, ETexture* dst, int srcBpp, int dstBpp) {
  return src && dst && src->m_def.m_format != 0x8f && dst->m_def.m_format != 0x8f && src->m_def.m_bpp == srcBpp &&
         dst->m_def.m_bpp == dstBpp;
}
}  // namespace

// 8030B108
// The texture resource `id` copied into `dst`, converted between 8 and 32 bits as needed.
int TextureUtils::LoadTexture(unsigned int id, ETexture* dst) {
  if (!id || !dst) return 0;
  auto* res = static_cast<ERTexture*>(_textureman.AddRef(id, nullptr, 0));
  if (!res) return 0;
  int ok = 0;
  ETexture* src = res->m_texture;
  if (src && src != dst) {
    const int s = src->m_def.m_bpp, d = dst->m_def.m_bpp;
    if (s == 0x20 && d == 0x20) ok = CopyTexture32To32(src, dst);
    else if (s == 8 && d == 0x20) ok = CopyTexture8To32(src, dst);
    else if (s == 0x20 && d == 8) ok = static_cast<int>(CopyTexture32To8(src, dst));
    else if (s == 8 && d == 8) ok = static_cast<int>(CopyTexture8To8(src, dst));
  }
  res->DelRef();
  return ok;
}

// 8030B5AC
// Each palette index of `src` written as its colour into `dst` (both `dst`'s size).
int TextureUtils::CopyTexture8To32(ETexture* src, ETexture* dst) {
  if (!Copyable(src, dst, 8, 0x20)) return 0;
  const bool srcSwizzled = PrepareTextureForUpdateBegin(src);
  const bool dstSwizzled = PrepareTextureForUpdateBegin(dst);
  const unsigned int w = dst->m_def.m_width, h = dst->m_def.m_height;
  int pitch, rows;
  src->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
  const unsigned char* s = src->UpdateMipLevel(0, pitch, rows);
  const unsigned char* pal = SourcePalette(src);
  dst->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Image});
  unsigned char* d = dst->UpdateMipLevel(0, pitch, rows);
  if (s && pal && d)
    for (unsigned int i = 0; i < w * h; ++i) std::memcpy(d + i * 4, pal + s[i] * 4u, 4);
  dst->UpdateEnd();
  PostUpdateEnd(dst, dstSwizzled);
  src->UpdateEnd();
  PostUpdateEnd(src, srcSwizzled);
  return 1;
}

// 8030B7B0
// The rows of `src` copied into `dst` (row pitch of each).
int TextureUtils::CopyTexture32To32(ETexture* src, ETexture* dst) {
  if (!Copyable(src, dst, 0x20, 0x20)) return 0;
  const bool srcSwizzled = PrepareTextureForUpdateBegin(src);
  const bool dstSwizzled = PrepareTextureForUpdateBegin(dst);
  const unsigned int rows = src->m_def.m_height, srcW = src->m_def.m_width, dstW = dst->m_def.m_width;
  int pitch, n;
  src->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
  const unsigned char* s = src->UpdateMipLevel(0, pitch, n);
  dst->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Image});
  unsigned char* d = dst->UpdateMipLevel(0, pitch, n);
  if (s && d)
    for (unsigned int y = 0; y < rows && y < dst->m_def.m_height; ++y)
      std::memcpy(d + y * dstW * 4, s + y * srcW * 4, (srcW < dstW ? srcW : dstW) * 4u);
  dst->UpdateEnd();
  PostUpdateEnd(dst, dstSwizzled);
  src->UpdateEnd();
  PostUpdateEnd(src, srcSwizzled);
  return 1;
}

// 8030BB9C
// `src` (32-bit) halved into `dst` (32-bit, half its size): each 2x2 block averaged per channel; a
// result whose low byte (alpha) is under `alphaMin` becomes `fill`.
RET_UNKNOWN TextureUtils::CopyTexture32To32Half(ETexture* src, ETexture* dst, unsigned char alphaMin, unsigned int fill) {
  if (!src || !dst || src->m_def.m_format == 0x8f || dst->m_def.m_format == 0x8f || src->m_def.m_bpp != 0x20 ||
      dst->m_def.m_bpp != 0x20 || src->m_def.m_width != dst->m_def.m_width * 2 || src->m_def.m_height != dst->m_def.m_height * 2)
    return 0;
  const bool srcSwizzled = PrepareTextureForUpdateBegin(src);
  const bool dstSwizzled = PrepareTextureForUpdateBegin(dst);
  const unsigned int dh = dst->m_def.m_height, sw = src->m_def.m_width, dw = dst->m_def.m_width;
  src->UpdateBegin(ETextureUpdateType{ETextureUpdateType::PaletteOnly});
  dst->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Image});
  int pitch, rows;
  const unsigned char* s = src->UpdateMipLevel(0, pitch, rows);
  unsigned char* d = dst->UpdateMipLevel(0, pitch, rows);
  if (s && d)
    for (unsigned int y = 0; y < dh; ++y) {
      const unsigned char* r0 = s + (y * 2) * sw * 4;
      const unsigned char* r1 = s + (y * 2 + 1) * sw * 4;
      for (unsigned int x = 0; x < dw; ++x) {
        const unsigned int a = GetPixel(r0 + x * 8), b = GetPixel(r0 + x * 8 + 4), c = GetPixel(r1 + x * 8),
                           e = GetPixel(r1 + x * 8 + 4);
        auto ch = [&](int shift) { return (((a >> shift) & 0xff) + ((b >> shift) & 0xff) + ((c >> shift) & 0xff) + ((e >> shift) & 0xff)) >> 2; };
        const unsigned int low = ch(0);
        unsigned int v = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | low;
        if ((low & 0xff) < alphaMin) v = fill;
        PutPixel(d + (y * dw + x) * 4, v);
      }
    }
  dst->UpdateEnd();
  PostUpdateEnd(dst, dstSwizzled);
  src->UpdateEnd();
  PostUpdateEnd(src, srcSwizzled);
  return 1;
}

// 8030BE60
// The rows reversed (top to bottom).
void TextureUtils::VerticalFlip(ETexture* t) {
  if (!t) return;
  const bool swizzled = PrepareTextureForUpdateBegin(t);
  t->UpdateBegin(ETextureUpdateType{ETextureUpdateType::Full});
  int pitch, rows;
  unsigned char* top = t->UpdateMipLevel(0, pitch, rows);
  const unsigned int n = static_cast<unsigned int>(t->m_def.m_width) * t->m_def.m_bpp >> 3;
  if (top && n) {
    std::vector<unsigned char> tmp(n);
    unsigned char* bottom = top + (t->m_def.m_height - 1) * n;
    for (unsigned int i = t->m_def.m_height >> 1; i; --i, top += n, bottom -= n) {
      std::memcpy(tmp.data(), top, n);
      std::memcpy(top, bottom, n);
      std::memcpy(bottom, tmp.data(), n);
    }
  }
  t->UpdateEnd();
  PostUpdateEnd(t, swizzled);
}

// 8030C5B4
// A GX-swizzled texture is unswizzled for the CPU first (true: swizzle it again after).
bool TextureUtils::PrepareTextureForUpdateBegin(ETexture* t) {
  if (!t) return false;
  const bool swizzled = (t->m_def.m_flags & 0x80) != 0;
  if (swizzled) t->ClearSwizzleFlag();
  return swizzled;
}

// 8030C618
void TextureUtils::PostUpdateEnd(ETexture* t, bool swizzled) {
  if (t && swizzled) t->SetSwizzleFlag();
}

// 8030C65C
// `src` over `dst` by `src`'s alpha (the result is opaque).
unsigned int TextureUtils::BlendPixels32(unsigned int dst, unsigned int src) {
  const unsigned int a = src & 0xff;
  if (a == 0) return dst;
  if (a == 0xff) return src;
  const unsigned int ia = 0xff - a;
  auto ch = [&](int shift) { return ((a * ((src >> shift) & 0xff) + ia * ((dst >> shift) & 0xff)) / 0xff) & 0xff; };
  return ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | 0xff;
}
