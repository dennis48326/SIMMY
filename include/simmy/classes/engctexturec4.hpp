// @class ENgcTextureC4 after ENgcTextureBase
// GX C4 texture (engine/e_ngctexturec4.cpp).
#pragma once

struct ENgcTextureC4 : public ENgcTextureBase {
  ENgcTextureC4();
  ~ENgcTextureC4() override;
  void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
};
