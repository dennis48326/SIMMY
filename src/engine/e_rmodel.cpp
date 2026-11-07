// Port of engine/e_rmodel.cpp (partial): loading a MODL resource, drawing it, and morphing it (morph
// targets and lattices). Shadows, welding, hierarchical (skeleton) drawing and attachments are not
// ported.
//
// The data after the EDataHeader is read in the original's order (see ermodel.hpp): light positions,
// attachment vertices, lattices, dummies, cameras, lights, the scale, the sub-models (each a list of
// ESubModelShaders), the bounds and the flags. All 3,631 models on the disc parse to their exact size.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

using simmy::be32;
using simmy::befloat;

namespace {
constexpr unsigned int kSignature = 0x4D4F444C;  // 'MODL'

// The element records that are kept as stored (their classes are not ported). Each returns the
// record's size; the layouts are those of the classes' Read(unsigned char*).
std::size_t PointListSize(const unsigned char* p, std::size_t pointSize) {  // ENDummy/ENCamera::Read
  if (be32(p) != 0) return 4;
  return 0x4C + be32(p + 0x48) * pointSize;
}
// CopyTArray<T>: a count, then the elements.
int CopyRecords(std::vector<std::vector<unsigned char>>& out, const unsigned char* src,
                std::size_t (*size)(const unsigned char*)) {
  const unsigned char* p = src + 4;
  const int count = static_cast<int>(be32(src));
  out.clear();
  for (int i = 0; i < count; ++i) {
    const std::size_t n = size(p);
    out.emplace_back(p, p + n);
    p += n;
  }
  return static_cast<int>(p - src);
}
void ReadFloats(float* dst, const unsigned char* src, int count) {
  for (int i = 0; i < count; ++i) dst[i] = befloat(src + i * 4);
}
// CopyTArray<BSplineVolume>: a count, then each lattice (BSplineVolume::Read).
int ReadLattices(std::vector<BSplineVolume*>& out, const unsigned char* src) {
  for (BSplineVolume* v : out) delete v;
  out.clear();
  const int count = static_cast<int>(be32(src));
  const unsigned char* p = src + 4;
  for (int i = 0; i < count; ++i) {
    auto* v = new BSplineVolume;
    out.push_back(v);
    p += v->Read(p);
  }
  return static_cast<int>(p - src);
}
}  // namespace

// 80317DEC
ERModel::ERModel() {}

// 80318014
ERModel::~ERModel() {
  DeallocateScaleMatrix();
  if (m_attachmentVerts) {
    _modelman.Free(m_attachmentVerts);
    m_attachmentVerts = nullptr;
  }
  m_attachmentColor = 0;
  m_attachmentId = 0xff;
  m_numAttachmentVerts = 0;
  for (ESubModel* s : m_subModels) delete s;
  m_subModels.clear();
  for (BSplineVolume* v : m_lattices) delete v;
  m_lattices.clear();
}

// 803180F4
void ERModel::Refresh(EFile* file) {
  if (file) LoadModel(file);
}

// 8031811C
void ERModel::DelRefSubResources() {
  for (ESubModel* s : m_subModels) s->DelRefSubResources();
}

// 80318180
void ERModel::AddRefSubResources() {
  for (ESubModel* s : m_subModels) s->AddRefSubResources();
}

// 803181E4
// All or nothing: if one sub-model's shaders are not resident, the ones already taken are released.
int ERModel::TryIncrementSubResources() {
  for (std::size_t i = 0; i < m_subModels.size(); ++i) {
    if (!m_subModels[i]->TryIncrementSubResources()) {
      while (i-- > 0) m_subModels[i]->DelRefSubResources();
      return 0;
    }
  }
  return 1;
}

// 80318280
// A count, then that many 4x4 matrices.
int ERModel::ReadAttachmentVert(const unsigned char* src) {
  m_numAttachmentVerts = static_cast<int>(be32(src));
  const unsigned char* p = src + 4;
  if (m_attachmentVerts) {
    _modelman.Free(m_attachmentVerts);
    m_attachmentVerts = nullptr;
  }
  if (m_numAttachmentVerts) {
    m_attachmentVerts = static_cast<EMat4*>(_modelman.Alloc(m_numAttachmentVerts * sizeof(EMat4), 0x10));
    for (int i = 0; i < m_numAttachmentVerts; ++i, p += 0x40) {
      new (&m_attachmentVerts[i]) EMat4;
      ReadFloats(&m_attachmentVerts[i].m[0][0], p, 16);
    }
  }
  return static_cast<int>(p - src);
}

// 80318324
void ERModel::LoadModel(EFile* file) {
  if (!file) return;
  EDataHeader header;
  if (!header.Read(*file, kSignature, 0x39, 0x3a)) return;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  DeallocateScaleMatrix();
  // GC: a temporary buffer from the root heap (or VM heap).
  std::vector<unsigned char> buffer(header.m_dataSize);
  file->Read(buffer.data(), header.m_dataSize);
  const unsigned char* src = buffer.data();

  m_loadFlags = 0;
  m_numLightPos = static_cast<int>(be32(src));
  ReadFloats(m_lightPos, src + 4, 12);
  m_attachmentColor = src[0x34];
  const unsigned char* p = src + 0x35;
  p += ReadAttachmentVert(p);
  p += ReadLattices(m_lattices, p);
  p += CopyRecords(m_dummies, p, [](const unsigned char* q) { return PointListSize(q, 0x50); });
  p += CopyRecords(m_cameras, p, [](const unsigned char* q) { return PointListSize(q, 0x60); });
  p += CopyRecords(m_lightInfo, p, [](const unsigned char*) { return std::size_t{0x1C}; });  // SimsLightInfo::Read
  m_field1C = p[0] != 0;
  m_scale = befloat(p + 1);
  const int count = static_cast<int>(be32(p + 5));
  p += 9;
  for (ESubModel* s : m_subModels) delete s;
  m_subModels.clear();
  m_subModels.reserve(count);
  for (int i = 0; i < count; ++i) {
    auto* sub = new ESubModel;
    m_subModels.push_back(sub);
    sub->m_model = this;
    p += sub->Read(p, static_cast<int>(header.m_version));
  }
  ReadFloats(m_boundSphere, p, 4);
  ReadFloats(m_bounds, p + 0x10, 6);
  ReadFloats(m_bounds2, p + 0x28, 6);
  m_loadFlags = p[0x40];
  m_matsInDLs = p[0x41] != 0;
  m_field18 = p[0x42] != 0;
  m_field1C = p[0x43] != 0;
  FinishLoad(static_cast<char>(m_loadFlags));
}

// 80318664
// Unpacks the load flags and, unless this is a morph target, builds every shader's display list.
void ERModel::FinishLoad(char flags) {
  const unsigned int f = static_cast<unsigned char>(flags);
  m_weldable = (f >> 1) & 1;
  m_hierarchical = f & 1;
  m_dontFrontPlaneFade = (f >> 2) & 1;
  m_isWall = 0;
  m_isMorphTarget = (f >> 4) & 1;
  m_isLattice = (f >> 5) & 1;
  m_dontInterestFade = (f >> 6) & 1;
  m_dontCull = (f >> 7) & 1;
  if (m_isLattice) ParameterizeVerts();
  if (!m_isMorphTarget) {
    ESMBuildDisplayListData data;
    data.m_name = "";  // DAT_80420800
    data.m_model = this;
    data.m_minX = 1e8f;
    data.m_maxX = -1e8f;
    data.m_hasBlend = 0;
    for (ESubModel* s : m_subModels) s->BuildDisplayList(&data);
    m_hasBlend = data.m_hasBlend;
  }
}

// 80318778
int ERModel::GetShaderCount() {
  int n = 0;
  for (ESubModel* s : m_subModels) n += static_cast<int>(s->m_shaders.size());
  return n;
}

// 80318874
// The bound sphere moved by `m` (its radius by m's largest scale).
void ERModel::CalcOrientedBoundSphere(EMat4& m, EBoundSphere& out) {
  float* o = reinterpret_cast<float*>(&out);
  const float scale = m.GetMaxScale();
  const float* c = m_boundSphere;
  for (int i = 0; i < 3; ++i) o[i] = c[2] * m.m[2][i] + c[0] * m.m[0][i] + c[1] * m.m[1][i] + m.m[3][i];
  o[3] = m_boundSphere[3] * scale;
}

// 803187AC
// The model-to-world scale as a matrix, made on first use.
EMat4* ERModel::GetScaleMatrix() {
  if (!m_scaleMatrix) {
    m_scaleMatrix = new (_modelman.Alloc(sizeof(EMat4), 0x10)) EMat4;
    if (m_scale == 0.0f) {
      m_scaleMatrix->Id();
    } else {
      EVec3 s{m_scale, m_scale, m_scale};
      m_scaleMatrix->Scale(s);
    }
  }
  return m_scaleMatrix;
}

// 8031882C
void ERModel::DeallocateScaleMatrix() {
  if (m_scaleMatrix) {
    _modelman.Free(m_scaleMatrix);
    m_scaleMatrix = nullptr;
  }
}

// 80318950
void ERModel::DrawGeometry(ERC* rc) {
  for (ESubModel* s : m_subModels) s->DrawGeometry(rc);
}
// 803189BC
void ERModel::DrawAsShadow(ERC* rc) {
  for (ESubModel* s : m_subModels) s->DrawAsShadow(rc);
}
// 80318B84
// The model as a shadow at `alpha` onto the stencilled area (blended by the inverse destination alpha:
// stencil function 1), pass-through and unlit: each sub-model shader not marked 1 as its shadow mask.
void ERModel::DrawShadow(ERC* rc, float alpha) {
  rc->Stencil(0, 1, 1);
  rc->EnableGeometryModes(0x4000);
  rc->DisableGeometryModes(8);
  for (ESubModel* sub : m_subModels) {
    for (ESubModelShader* s : sub->m_shaders) {
      EShader* shader = s->GetEShader();
      if (!shader || (shader->m_surface & 1)) continue;
      shader->SetShadowAlpha(alpha);
      shader->SelectForShadowMask(rc);
      rc->SetMipMap(s->m_mipMap, 0);
      rc->DisplayList(s->m_dl);
    }
  }
  rc->DisableGeometryModes(0x4000);
  rc->EnableGeometryModes(8);
  rc->Stencil(0, 0, 0);
}

// 80318DA4
// As DrawShadow, onto the area whose destination alpha is set (stencil function 2), writing alpha 0.
void ERModel::DrawShadowWithStencilInverted(ERC* rc) {
  rc->Stencil(0, 2, 0);
  rc->EnableGeometryModes(0x4000);
  rc->DisableGeometryModes(8);
  for (ESubModel* sub : m_subModels) {
    for (ESubModelShader* s : sub->m_shaders) {
      EShader* shader = s->GetEShader();
      if (!shader || (shader->m_surface & 1)) continue;
      shader->SelectForShadowMask(rc);
      rc->SetMipMap(s->m_mipMap, 0);
      rc->DisplayList(s->m_dl);
    }
  }
  rc->DisableGeometryModes(0x4000);
  rc->EnableGeometryModes(8);
  rc->Stencil(0, 0, 0);
}

// 80318A28
void ERModel::DrawNormals(ERC* rc) {
  for (ESubModel* s : m_subModels) s->DrawNormals(rc);
}
// 80318A94
void ERModel::DrawWireFrame(ERC* rc) {
  for (ESubModel* s : m_subModels) s->DrawWireFrame(rc);
}

// 80318B00
// GC: the count of TArray<ENLattice> (+0x110), which LoadModel never fills (the lattices that are
// read are the BSplineVolumes, GetNumMorphBases).
int ERModel::GetLatticeCount() { return 0; }
// 80318B08
int ERModel::GetDummyCount() { return static_cast<int>(m_dummies.size()); }
// 80318B10
int ERModel::GetCameraCount() { return static_cast<int>(m_cameras.size()); }

// 80318FB0
void ERModel::DrawHierarchical(ERC* rc, EMat4* matrices, EACNodeState* states, int count) {
  if (!count || !matrices) return;
  if (m_matsInDLs == 0) rc->ModelMatrices(matrices, count);
  else rc->ModelMatrixList(matrices, count);
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (m_field1C == 0 || !win) {
    for (ESubModel* s : m_subModels) {
      const int node = s->m_field0C;
      if (node < 0 || !(states[node].m_flags & EACNodeState::kHidden)) s->Draw(rc);
    }
    return;
  }
  // Sorted along the view direction by their nodes' positions (GC: an EFloatTree, read in key order).
  EVec3 dir = win->m_lookDir;
  if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) {
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir.x /= len, dir.y /= len, dir.z /= len;
  }
  const float d = -(win->m_lookPos.x * dir.x + win->m_lookPos.y * dir.y + win->m_lookPos.z * dir.z);
  std::vector<std::pair<float, ESubModel*>> sorted;
  for (ESubModel* s : m_subModels) {
    const int node = s->m_field0C;
    if (node < 0) {
      s->Draw(rc);
    } else if (!(states[node].m_flags & EACNodeState::kHidden)) {
      const EMat4& m = matrices[node];
      sorted.emplace_back(dir.z * m.m[3][2] + dir.x * m.m[3][0] + dir.y * m.m[3][1] + d, s);
    }
  }
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const std::pair<float, ESubModel*>& a, const std::pair<float, ESubModel*>& b) { return a.first < b.first; });
  for (auto& e : sorted) e.second->Draw(rc);
}

// 80318B18
// Each sub-model selects its shaders and calls their display lists. The caller has set the model
// matrices (usually object-to-world times GetScaleMatrix).
void ERModel::Draw(ERC* rc) {
  for (ESubModel* s : m_subModels) s->Draw(rc);
}

// ---------------------------------------------------------------------------------------------------
// Morphing. A model morphs by offsets added up in its shaders' morph strips: ResetMorph clears them
// (when a weight changed), MorphLattices adds the lattices' moves (BlendVolumes by the lattice
// weights, then each lattice vertex's move), MorphTargets adds the registered targets' offsets times
// their weights, ApplyMorph makes the positions and display lists (and clears the dirty flag).

// 80319E88
// This model (a morph target) as offsets from `base`.
void ERModel::GenerateMorphTargetDeltas(ERModel* base) {
  if (!base || base->m_subModels.size() != m_subModels.size()) return;
  for (std::size_t i = 0; i < m_subModels.size(); ++i) m_subModels[i]->GenerateMorphTargetDeltas(base->m_subModels[i]);
}

// 80319F08
// `target` into slot `slot` (0..12; GC: slot 12 overlaps weight 0), if the slot is free.
int ERModel::RegisterMorphTarget(ERModel* target, int slot) {
  if (static_cast<unsigned int>(slot) >= 13 || m_subModels.size() != target->m_subModels.size()) return 0;
  if (!m_morphTargets[slot]) {
    m_morphTargets[slot] = target;
    for (std::size_t i = 0; i < m_subModels.size(); ++i) m_subModels[i]->RegisterMorphTarget(target->m_subModels[i], slot);
    m_morphWeights[slot] = 0.0f;
  }
  return 1;
}

// 80319FD0
// The model `id` as a target (it keeps a reference while registered).
ERModel* ERModel::RegisterMorphTarget(unsigned int id, int slot) {
  auto* target = static_cast<ERModel*>(_modelman.AddRef(id, nullptr, 0));
  if (target && !RegisterMorphTarget(target, slot)) {
    target->DelRef();
    return nullptr;
  }
  return target;
}

// 8031A040
// The last reference to the slot's target: unregistered; otherwise only a reference is dropped
// (the target stays in its slot, as on the GC).
void ERModel::UnRegisterMorphTarget(int slot) {
  ERModel* target = m_morphTargets[slot];
  if (!target || target->m_refs <= 0) return;
  if (target->m_refs == 1) {
    for (ESubModel* s : m_subModels) s->UnRegisterMorphTarget(slot);
    target->DelRef();
    m_morphTargets[slot] = nullptr;
    m_morphWeights[slot] = 0.0f;
  } else {
    target->DelRef();
  }
}

// 8031A108
void ERModel::UnRegisterMorphTarget(ERModel* target) {
  if (!target) return;
  for (int i = 0; i < 12; ++i)
    if (m_morphTargets[i] == target) {
      UnRegisterMorphTarget(i);
      return;
    }
}

// 8031A174
int ERModel::SetMorphTargetWeight(unsigned int slot, float weight) {
  if (slot > 12) return 0;
  if (m_morphWeights[slot] != weight) {
    m_morphWeights[slot] = weight;
    m_morphDirty = 1;
  }
  return 1;
}

// 8031A1AC
void ERModel::ResetMorph() {
  if (!m_morphDirty) return;
  for (ESubModel* s : m_subModels) s->ResetMorph();
}

// 8031A21C
int ERModel::MorphTargets() {
  int result = 0;
  if (!m_morphDirty) return result;
  for (ESubModel* s : m_subModels) {
    result = s->Morph(m_morphWeights);
    if (!result) return 0;
  }
  return result;
}

// 8031A2A4
void ERModel::MorphLattices() {
  if (!m_morphDirty) return;
  for (BSplineVolume* v : m_lattices) v->BlendVolumes();
  for (ESubModel* s : m_subModels) s->LatticeDeform();
}

// 8031A34C
void ERModel::ApplyMorph() {
  if (!m_morphDirty) return;
  for (ESubModel* s : m_subModels) s->ApplyMorph();
  m_morphDirty = 0;
}

// 8031A3C4
void ERModel::CleanMorphTargets(bool keepClean) {
  for (int i = 0; i < 12; ++i) UnRegisterMorphTarget(i);
  if (!keepClean) m_morphDirty = 1;
}

// 8031A41C
void ERModel::FreeAllMorphAndLatticeResource(bool keepClean) {
  CleanMorphTargets(keepClean);
  for (BSplineVolume* v : m_lattices) delete v;
  m_lattices.clear();
  FreeMorphResource();
}

// 8031A458
void ERModel::FreeStripResource() {
  for (ESubModel* s : m_subModels) s->FreeStripResource();
}

// 8031A4BC
void ERModel::FreeMorphStripResource() {
  for (ESubModel* s : m_subModels) s->FreeMorphStripResource();
}

// 8031A520
void ERModel::SetLatticeWeight(int lattice, int volume, float weight) {
  if (m_lattices[lattice]->SetVolumeWeights(volume, weight)) m_morphDirty = 1;
}

// 8031A56C
void ERModel::ResetLatticeWeights() {
  for (int i = 0; i < static_cast<int>(m_lattices.size()); ++i)
    for (int v = 0; v < m_lattices[i]->GetNumVolumes(); ++v) SetLatticeWeight(i, v, 0.0f);
}

// 8031A5FC
int ERModel::GetNumMorphLattice(int lattice) { return m_lattices[lattice]->GetNumVolumes(); }

// 8031A628
int ERModel::GetNumMorphBases() { return static_cast<int>(m_lattices.size()); }

// 8031A630
// Where each vertex sits in the lattice it belongs to (lattice models, FinishLoad).
void ERModel::ParameterizeVerts() {
  for (ESubModel* s : m_subModels) s->ParameterizeVerts(&m_lattices);
}

// 8031A698
void ERModel::FreeMorphResource() {
  for (ESubModel* s : m_subModels) s->FreeMorphResource();
}

// 8031A6FC
int ERModel::HasModifiableColor() {
  for (ESubModel* s : m_subModels)
    if (s->HasModifiableColor()) return 1;
  return 0;
}

// 8031A774
unsigned int ERModel::GetModifiableColor(unsigned int index) {
  for (ESubModel* s : m_subModels)
    if (s->HasModifiableColor()) return s->GetModifiableColor(index);
  return 0;
}

// 8031B428
int ERModel::IsHierarchical() { return m_hierarchical; }
// 8031B430
int ERModel::IsWeldable() { return m_weldable; }
// 8031B438
int ERModel::IsMorphTarget() { return m_isMorphTarget; }
// 8031B440
int ERModel::IsLattice() { return m_isLattice; }
// 8031B448
int ERModel::AreMatsInDLs() { return m_matsInDLs; }
// 8031B450
int ERModel::DontFrontPlaneFade() { return m_dontFrontPlaneFade; }
// 8031B458
int ERModel::DontInterestFade() { return m_dontInterestFade; }
// 8031B460
int ERModel::DontCull() { return m_dontCull; }
// 8031B468
int ERModel::IsWall() { return m_isWall; }
// 8031B470
unsigned char ERModel::GetAttachmentColor() { return m_attachmentColor; }
// 8031B478
void ERModel::ClearAttachmentID() { m_attachmentId = 0xff; }
// 8031B484
int ERModel::GetNumAttachmentVerts() { return m_numAttachmentVerts; }
// 8031B48C
int ERModel::GetNumLightPos() { return m_numLightPos; }
// 8031B494
float* ERModel::GetLightPos() { return m_lightPos; }

// 8031B2A4 ERModel::GetTypeInfo
// 8031B2B0 ERModel::GetTypeName
// 8031B2BC ERModel::GetTypeKey
// 8031B2C8 ERModel::GetTypeVersion
// 8031B264 ERModel::SafeDelete
// 8031B1EC ERModel::New
// 8031B214 ERModel::Construct
// 8031B240 ERModel::Destruct
// 8031B2D4 ERModel::GetTypeInfoStatic
// 8031B2E0 ERModel::GetReadVersion
SIMMY_STORABLE_IMPL(ERModel, "ERModel", 0, EResource)

// 8031B0C0
// The uvs of every sub-model moved into a rectangle of a composited texture, once per model.
void ERModel::TransformModelUV(EVec2& scale, EVec2& offset, unsigned int shaderId, EVec2& scale2, EVec2& offset2) {
  if (m_uvTransformed) return;
  m_uvTransformed = 1;
  for (ESubModel* sub : m_subModels) sub->TransformModelUV(scale, offset, shaderId, scale2, offset2, m_weldable != 0);
}
