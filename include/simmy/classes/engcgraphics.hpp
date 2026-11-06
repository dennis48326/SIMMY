// @class ENgcGraphics
// Platform graphics (engine/e_ngcgraphics.cpp): owns the renderer and frame boundaries.
#pragma once

struct ENgcGraphics : public EGraphics {
  ENgcGraphics();  // 8032F32C
  virtual ~ENgcGraphics();  // 8032F36C
  virtual RET_UNKNOWN Init();  // 8032F39C
  virtual void SetBackgroundColor(EVec3 &, int, bool);  // 8032F5EC
  virtual RET_UNKNOWN GetScreenShot(char *);  // 8032F630
  void Destroy(ERenderSurface* surface) override;  // 8032F634
  using EGraphics::Destroy;
  EDL* AllocDL(RCMode mode) override;  // 8032F674
  void FreeDL(EDL* dl, RCMode mode) override;  // 8032F6BC
  ERC* AllocRC(RCMode mode) override;  // 8032F728
  void FreeRC(ERC* rc, RCMode mode) override;  // 8032F790
  ETexture* AllocTexture() override;  // 8032F7DC
  void FreeTexture(ETexture* texture) override;  // 8032F810
  EShader* AllocShader() override;  // 8032F850
  void FreeShader(EShader* shader) override;  // 8032F884
  ERenderSurface* AllocRenderSurface() override;  // 8032F8C4
  void FreeRenderSurface(ERenderSurface* surface) override;  // 8032F8EC
  EMovie* AllocMovie() override;  // 8032F92C
  void FreeMovie(EMovie* movie) override;  // 8032F954
  void EndFrame() override;  // 8032F994
  virtual void DrawCensorRects(ERC *);  // 8032FCA4
  virtual void ProcessFrameEffects();  // 8032FD48

  EShader* m_defaultShader = nullptr;  // +0x328, created at the end of Init
};
