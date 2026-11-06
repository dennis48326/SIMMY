// @class ENgcTextureRGB5A3 after ENgcTextureBase
// GX RGB5A3 texture (engine/e_ngctexturergb5a3.cpp).
#pragma once

struct ENgcTextureRGB5A3 : public ENgcTextureBase {
  ENgcTextureRGB5A3();
  ~ENgcTextureRGB5A3() override;
  void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
};
