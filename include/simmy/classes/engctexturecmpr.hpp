// @class ENgcTextureCMPR after ENgcTextureBase
// GX CMPR texture (engine/e_ngctexturecmpr.cpp).
#pragma once

struct ENgcTextureCMPR : public ENgcTextureBase {
  ENgcTextureCMPR();
  ~ENgcTextureCMPR() override;
  void SwizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
  void UnswizzleImage(unsigned char* src, int width, int height, unsigned char* dst) override;
};
