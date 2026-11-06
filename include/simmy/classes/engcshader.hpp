// @class ENgcShader after EShader
// GX side of a shader (engine/e_ngcshader.cpp). Selecting one hands it to the render context, which
// sets up the TEV stages.
#pragma once

struct ENgcShader : public EShader {
  ENgcShader();
  ~ENgcShader() override;
  void Select(ERC* rc, unsigned int flags) override;
  void SelectForShadowMask(ERC* rc) override;
  int Create(EShaderDef& def) override;
  virtual void ChangeMaterial(EMaterial& material);
};
