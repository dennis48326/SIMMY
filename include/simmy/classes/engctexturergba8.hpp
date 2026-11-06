// @class ENgcTextureRGBA8 after ENgcTextureBase
// GX RGBA8 texture (engine/e_ngctexturergba8.cpp).
#pragma once

struct ENgcTextureRGBA8 : public ENgcTextureBase {
  ENgcTextureRGBA8();
  ~ENgcTextureRGBA8() override;
  void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
};
