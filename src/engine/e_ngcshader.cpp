// Port of engine/e_ngcshader.cpp.
#include "simmy/types_gen.hpp"

#include <cstring>

// 8034DC04
ENgcShader::ENgcShader() {}
// 8034DC40
ENgcShader::~ENgcShader() {}

// 8034DE0C
// A two-layer shader whose blend modes amount to "second layer only" is collapsed to one layer
// (layer 1 copied over layer 0), saving a TEV stage.
int ENgcShader::Create(EShaderDef& def) {
  if (!EShader::Create(def)) return 0;
  m_geometryModes &= 0xfffffe1fu;
  if (m_layerCount != 2) return 1;
  auto packed = [](const unsigned char* b) { return b[0] | b[1] << 2 | b[2] << 4 | b[3] << 6; };
  unsigned int first = packed(m_layers[0].m_blend);
  const unsigned int second = packed(m_layers[1].m_blend);
  if (!(m_layers[0].m_field0C & 0x40)) first = 0;
  if (first != 0x4A && first != 0x5A && first != 0x6A) return 1;
  if (second != 0x44 && second != 0x48 && second != 0x64 && second != 0x68) return 1;
  m_layerCount = 1;
  m_layers[0] = m_layers[1];
  return 1;
}

// 8034DF80
void ENgcShader::ChangeMaterial(EMaterial& material) { EShader::ChangeMaterial(material); }

// 8034DC98
// Selects each layer's texture into its stage, then the shader itself.
void ENgcShader::Select(ERC* rc, unsigned int flags) {
  for (int i = 0; i < m_layerCount; ++i) rc->Texture(GetTexture(i), i);
  static_cast<ENgcRC*>(rc)->Shader(this, flags);
}

// 8034DD3C
// A shadow mask: no texture, this shader's state, the pass-through geometry mode (0x4000: unlit black
// at half alpha, the blend left to ERC::Stencil), blending on, no z test.
void ENgcShader::SelectForShadowMask(ERC* rc) {
  rc->Texture(nullptr, 0);
  static_cast<ENgcRC*>(rc)->Shader(this, 0);
  rc->EnableGeometryModes(0x4000);
  rc->EnableRasterModes(0x40, 0);
  rc->ZTest(false, 2, 0, 0);
}
