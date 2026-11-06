// Port of engine/e_submodel.cpp (partial): a sub-model is a list of ESubModelShaders. Morphing and
// lattice deformation are not ported.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

using simmy::be32;

// 802FAC50
ESubModel::ESubModel() {}

// 802FAC88
ESubModel::~ESubModel() {
  m_model = nullptr;
  for (ESubModelShader* s : m_shaders) delete s;
  m_shaders.clear();
}

// 802FACD4
void ESubModel::DelRefSubResources() {
  for (ESubModelShader* s : m_shaders) s->DelRefSubResources();
}

// 802FAD30
void ESubModel::AddRefSubResources() {
  for (ESubModelShader* s : m_shaders) s->AddRefSubResources();
}

// 802FAD8C
int ESubModel::TryIncrementSubResources() {
  for (std::size_t i = 0; i < m_shaders.size(); ++i) {
    if (!m_shaders[i]->TryIncrementSubResources()) {
      while (i-- > 0) m_shaders[i]->DelRefSubResources();
      return 0;
    }
  }
  return 1;
}

// 802FAE20
unsigned int ESubModel::GetCompositeShaderFlags() { return 0; }

// 802FAE2C
// A word for +0x0C, the shader count, then each ESubModelShader.
int ESubModel::Read(const unsigned char* src, int version) {
  m_field0C = static_cast<int>(be32(src));
  const int count = static_cast<int>(be32(src + 4));
  const unsigned char* p = src + 8;
  for (ESubModelShader* s : m_shaders) delete s;
  m_shaders.clear();
  m_shaders.reserve(count);
  for (int i = 0; i < count; ++i) {
    auto* shader = new ESubModelShader;
    m_shaders.push_back(shader);
    if (m_model && m_model->GetNumMorphBases()) shader->m_morph = 1;
    shader->m_owner = this;
    p += shader->Read(p, version);
  }
  return static_cast<int>(p - src);
}

// 802FAF20
// Per shader: select it, set its mip-map bias and call its display list.
void ESubModel::Draw(ERC* rc) {
  for (ESubModelShader* s : m_shaders) {
    if (EShader* shader = s->GetEShader()) shader->Select(rc, 0);
    rc->SetMipMap(s->m_mipMap, 0);
    rc->DisplayList(s->m_dl);
  }
}

// 802FB004
// The strips only, in whatever shader is selected.
void ESubModel::DrawGeometry(ERC* rc) {
  for (ESubModelShader* s : m_shaders) {
    rc->SetMipMap(s->m_mipMap, 0);
    rc->DisplayList(s->m_dl);
  }
}

// 802FB09C
void ESubModel::DrawAsShadow(ERC* rc) {
  for (ESubModelShader* s : m_shaders) {
    if (EShader* shader = s->GetEShader()) shader->SelectForShadowMask(rc);
    rc->SetMipMap(s->m_mipMap, 0);
    rc->DisplayList(s->m_dl);
  }
}

// 802FB17C
void ESubModel::DrawNormals(ERC* rc) {
  for (ESubModelShader* s : m_shaders) s->DrawNormals(rc);
}

// 802FB1E0
void ESubModel::DrawWireFrame(ERC* rc) {
  for (ESubModelShader* s : m_shaders) s->DrawWireFrame(rc);
}

// 802FB244
void ESubModel::BuildDisplayList(bool a, char* name, bool weights, float minX, float maxX) {
  for (ESubModelShader* s : m_shaders) s->BuildDisplayList(a, name, weights, minX, maxX);
}

// 802FB2D8
void ESubModel::BuildDisplayList(ESMBuildDisplayListData* data) {
  for (ESubModelShader* s : m_shaders) s->BuildDisplayList(data);
}

// 802FB33C
void ESubModel::GetMinMaxX(float* minX, float* maxX) {
  for (ESubModelShader* s : m_shaders) s->GetMinMaxX(minX, maxX);
}

// 802FB3A8
int ESubModel::ApplyMorph() {
  for (ESubModelShader* s : m_shaders) s->ApplyMorph();
  return 1;
}

// 802FB408
void ESubModel::ResetMorph() {
  for (ESubModelShader* s : m_shaders) s->ResetMorph();
}

// 802FB464
int ESubModel::Morph(float* weights) {
  for (ESubModelShader* s : m_shaders) s->Morph(weights);
  return 1;  // GC: the last shader's result in r3 (always 1)
}

// 802FB4CC
void ESubModel::LatticeDeform() {
  for (ESubModelShader* s : m_shaders) s->LatticeDeform();
}

// 802FB528
// Shader for shader (the target has the same layout).
void ESubModel::RegisterMorphTarget(ESubModel* target, int slot) {
  for (std::size_t i = 0; i < m_shaders.size(); ++i) m_shaders[i]->RegisterMorphTarget(target->m_shaders[i], slot);
}

// 802FB59C
void ESubModel::UnRegisterMorphTarget(int slot) {
  for (ESubModelShader* s : m_shaders) s->UnRegisterMorphTarget(slot);
}

// 802FB600
void ESubModel::GenerateMorphTargetDeltas(ESubModel* base) {
  if (!base || base->m_shaders.size() != m_shaders.size()) return;
  for (std::size_t i = 0; i < m_shaders.size(); ++i) m_shaders[i]->GenerateMorphTargetDeltas(base->m_shaders[i]);
}

// 802FB678
void ESubModel::ParameterizeVerts(std::vector<BSplineVolume*>* lattices) {
  for (ESubModelShader* s : m_shaders) s->ParameterizeVerts(lattices);
}

// 802FB738
void ESubModel::FreeStripResource() {
  for (ESubModelShader* s : m_shaders) s->FreeStripResource();
}

// 802FB794
void ESubModel::FreeMorphStripResource() {
  for (ESubModelShader* s : m_shaders) s->FreeMorphStripResource();
}

// 802FB6DC
void ESubModel::FreeMorphResource() {
  for (ESubModelShader* s : m_shaders) s->FreeMorphResource();
}

// 802FB7F0
int ESubModel::HasModifiableColor() {
  for (ESubModelShader* s : m_shaders)
    if (s->CanColorBeModified()) return 1;
  return 0;
}

// 802FB860
unsigned int ESubModel::GetModifiableColor(unsigned int index) {
  for (ESubModelShader* s : m_shaders)
    if (s->CanColorBeModified()) return s->GetModifiableColor(index);
  return 0;
}

// 802FB8E0
void ESubModel::TransformModelUV(EVec2& scale, EVec2& offset, unsigned int shaderId, EVec2& scale2, EVec2& offset2,
                                 bool weldable) {
  for (ESubModelShader* s : m_shaders) s->TransformModelUV(scale, offset, shaderId, scale2, offset2, weldable);
}
