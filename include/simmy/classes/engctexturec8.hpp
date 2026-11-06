// @class ENgcTextureC8 after ENgcTextureBase
// GX C8 texture (engine/e_ngctexturec8.cpp).
#pragma once

struct ENgcTextureC8 : public ENgcTextureBase {
  ENgcTextureC8();
  ~ENgcTextureC8() override;
  void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
};
