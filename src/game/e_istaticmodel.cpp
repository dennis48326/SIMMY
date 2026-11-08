// Port of game/e_istaticmodel.cpp and game/e_igameinstance.cpp (partial): a whole model placed in a
// level, static or moved by an EAnimController. Still stubbed: shadows (DrawShadow, DrawAsShadow),
// shader swaps and texture replacement, immediate (UI) drawing, cloning and EStream serialisation.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

// 802277FC EIGameInstance::GetTypeInfo
// 80227808 EIGameInstance::GetTypeName
// 80227814 EIGameInstance::GetTypeKey
// 80227820 EIGameInstance::GetTypeVersion
// 802277BC EIGameInstance::SafeDelete
// 80227750 EIGameInstance::New
// 80227778 EIGameInstance::Construct
// 80227798 EIGameInstance::Destruct
// 8022782C EIGameInstance::GetTypeInfoStatic
// 80227838 EIGameInstance::GetReadVersion
SIMMY_STORABLE_IMPL(EIGameInstance, "EIGameInstance", 0, EInstance)
// 8022E070 EIStaticModel::GetTypeInfo
// 8022E07C EIStaticModel::GetTypeName
// 8022E088 EIStaticModel::GetTypeKey
// 8022E094 EIStaticModel::GetTypeVersion
// 8022E030 EIStaticModel::SafeDelete
// 8022DFB8 EIStaticModel::New
// 8022DFE0 EIStaticModel::Construct
// 8022E00C EIStaticModel::Destruct
// 8022E0A0 EIStaticModel::GetTypeInfoStatic
// 8022E0AC EIStaticModel::GetReadVersion
SIMMY_STORABLE_IMPL(EIStaticModel, "EIStaticModel", 0, EIGameInstance)

// 80227674
EIGameInstance::EIGameInstance() {}
// 802276B0
EIGameInstance::~EIGameInstance() = default;

namespace {
// The shader record's EShader (GC: ERShader +0x14, or its parent's).
EShader* ShaderOf(ERShader* shader) { return shader ? shader->GetShader() : nullptr; }

// The box around `b` (min, max) transformed by `m` (GC: EBound3::Compute(EBound3&, EMat4&)).
void TransformBounds(const float* b, const EMat4& m, float* out) {
  for (int i = 0; i < 3; ++i) {
    out[i] = 1e30f;
    out[i + 3] = -1e30f;
  }
  for (int c = 0; c < 8; ++c) {
    const float x = b[(c & 1) ? 3 : 0], y = b[(c & 2) ? 4 : 1], z = b[(c & 4) ? 5 : 2];
    for (int i = 0; i < 3; ++i) {
      const float v = x * m.m[0][i] + y * m.m[1][i] + z * m.m[2][i] + m.m[3][i];
      if (v < out[i]) out[i] = v;
      if (v > out[i + 3]) out[i + 3] = v;
    }
  }
}

// Normalises a matrix row's x, y, z unless it is zero, and clears its w.
void NormalizeAxis(float* row) {
  const float len = std::sqrt(row[0] * row[0] + row[1] * row[1] + row[2] * row[2]);
  if (row[0] != 0.0f || row[1] != 0.0f || row[2] != 0.0f) {
    for (int i = 0; i < 3; ++i) row[i] /= len;
  }
  row[3] = 0.0f;
}

// The controller's node matrices into the context: a skinned model's display lists index them.
void UseAnimMatrices(ELevelDrawData& data, EAnimController* anim, bool skinned) {
  if (data.m_curAnim == anim) return;
  data.m_curMatrix = nullptr;
  data.m_curAnim = anim;
  const int count = anim->NodeCount();
  if (skinned) data.m_rc->ModelMatrixList(anim->m_matrices, count);
  else data.m_rc->ModelMatrices(anim->m_matrices, count);
}
}  // namespace

// 8022B9A0
EIStaticModel::EIStaticModel() {
  m_drawMatrix.Id();
  m_orient.Id();
  m_flags |= 0x200;
}

// 8022BA50
EIStaticModel::~EIStaticModel() { DeallocateModel(); }

// 8022BB8C
void EIStaticModel::GetBoundSphere(EBoundSphere& out) {
  float* s = reinterpret_cast<float*>(&out);
  for (int i = 0; i < 4; ++i) s[i] = m_sphere[i];
}

// 8022BBB4
// The orientation without the model's scale (axes normalised), then the model and its bounds.
void EIStaticModel::Setup(EInstance* owner, EAnimController* anim) {
  m_orient = m_drawMatrix;
  NormalizeAxis(m_orient.m[0]);
  NormalizeAxis(m_orient.m[1]);
  NormalizeAxis(m_orient.m[2]);
  SetupModel(owner, anim);
  SetupBounds();
}

// 8022BD74
void EIStaticModel::DeallocateModel() {
  if (m_model) {
    m_model->DelRef();
    m_model = nullptr;
  }
  for (DigestGeomShaderPtr& d : m_digests)
    if (d.m_shader) d.m_shader->DelRef();
  m_digests.clear();
  m_color = 0;
}

// 8022BE08
void EIStaticModel::SetModel(unsigned int id, bool skipInShadowPass, EInstance* owner, EAnimController* anim) {
  m_modelId = id;
  m_skipInShadowPass = skipInShadowPass ? 1 : 0;
  m_field14 = 0;
  Setup(owner, anim);
}

// 8022BE8C
// The orientation; the draw matrix is it scaled to the model's units.
void EIStaticModel::SetOrient(EMat4& m) {
  m_orient = m;
  m_drawMatrix = m;
  const float scale = m_model ? m_model->m_scale : 1.0f;
  if (scale != 1.0f) m_drawMatrix.PreScale(scale);
  SetupBounds();
}

// 8022BFF8
// Sets the draw matrix and the orientation as given (no model scale, no bounds).
void EIStaticModel::SlamOrient(EMat4& draw, EMat4& orient) {
  m_drawMatrix = draw;
  m_orient = orient;
}

// 8022C3A0
// Takes the model (unless it is the one already held), keeping the draw matrix in its units.
void EIStaticModel::SetupModel(EInstance* owner, EAnimController* anim) {
  float oldScale = 1.0f;
  if (m_model) {
    if (m_model->m_id == m_modelId) return;
    oldScale = m_model->m_scale;
  }
  m_owner = owner;
  m_animController = anim;
  DeallocateModel();
  if (m_modelId) {
    m_model = static_cast<ERModel*>(_modelman.AddRef(m_modelId, nullptr, 0));
    if (!m_model) return;
    if (m_model->GetNumMorphBases()) m_model->Detach();
    BuildDigests();
  }
  const float scale = m_model ? m_model->m_scale : 1.0f;
  if (scale != oldScale) m_drawMatrix.PreScale(scale / oldScale);
  if (m_model && m_model->HasModifiableColor()) m_color = 0;
}

// 8022C18C
// The sub-model shaders that are floor surfaces (0x400, left out of the draw records) are drawn with
// the floor of the instance's room: each registers a record with the level.
void EIStaticModel::RegisterFloor() {
  if (!m_model || !m_level) return;
  EOrderTableData otd;
  otd.m_lights = &m_lights;
  otd.m_callback = AnimOrderTableCallback;
  otd.m_instance = this;
  if (m_animController) {
    otd.m_animController = m_animController;
    otd.m_skinned = m_model->m_matsInDLs != 0;
  } else {
    otd.m_matrix = GetDrawMatrix();
  }
  for (ESubModel* sub : m_model->m_subModels)
    for (ESubModelShader* s : sub->m_shaders) {
      if (!s->m_shader) s->m_shader = static_cast<ERShader*>(_shaderman.AddRef(s->m_shaderId, nullptr, 0));
      EShader* shader = ShaderOf(s->m_shader);
      if (!shader || !(shader->m_surface & 0x400)) continue;
      otd.m_dl = s->m_dl;
      otd.m_shader = s->m_shader;
      otd.m_flags = shader->m_surface;
      m_level->RegisterFloor(otd);
    }
}

// 8022C49C
// One record per shader of every sub-model (the sub-model's node, the shader and its flags, the
// strips); shaders flagged 0x400 are left out and mark the instance 0x8000. Models that never fade,
// interest-fade or cull clear the instance's settings.
void EIStaticModel::BuildDigests() {
  m_digests.clear();
  m_flags &= ~0x8000u;
  if (m_model->GetShaderCount()) {
    m_skinned = m_model->m_matsInDLs != 0;
    bool alpha = m_shaderFlags != 0;
    for (ESubModel* sub : m_model->m_subModels) {
      for (ESubModelShader* s : sub->m_shaders) {
        s->m_owner = sub;
        DigestGeomShaderPtr d;
        d.m_shader = static_cast<ERShader*>(_shaderman.AddRef(s->m_shaderId, nullptr, 0));
        d.m_srcShaderId = s->m_shaderId;
        if (d.m_shader) d.UpdateShaderFlags(alpha);
        if (!(d.m_flags & 0x400)) {
          d.m_nodeFlags = 0;
          d.m_node = static_cast<short>(sub->m_field0C);
          d.m_dl = s->m_dl;
          m_digests.push_back(d);
        } else {
          _shaderman.DelRef(s->m_shaderId, EResourceManager__DelRefMode{1});
          m_flags |= 0x8000;
        }
      }
    }
    m_shaderFlags = alpha ? 1 : 0;
  }
  if (m_model->m_dontFrontPlaneFade) m_fadeMode = 0;
  if (m_model->m_dontInterestFade) m_interestFade = 0;
  if (m_model->m_dontCull) m_cullable = 0;
}

// 8022C64C
// The bound sphere and box of the model where it stands; owned (sub-object) models get a flat box
// five times their size around their centre.
void EIStaticModel::SetupBounds() {
  if (!m_model) return;
  m_model->CalcOrientedBoundSphere(m_drawMatrix, *reinterpret_cast<EBoundSphere*>(m_sphere));
  float b[6];
  TransformBounds(m_model->m_bounds, m_drawMatrix, b);
  if (m_owner) {
    const float cx = (b[0] + b[3]) * 0.5f, cy = (b[1] + b[4]) * 0.5f, cz = 0.0f;
    float r = b[3] - b[0];
    if (r < b[4] - b[1]) r = b[4] - b[1];
    if (r < b[5] - b[2]) r = b[5] - b[2];
    r *= 5.0f;
    b[0] = cx - r;
    b[1] = cy - r;
    b[2] = cz - 1.0f;
    b[3] = cx + r;
    b[4] = cy + r;
    b[5] = cz + 1.0f;
  }
  EInstance::SetBounds(*reinterpret_cast<EBound3*>(b));
}

// 8022C834
unsigned int EIStaticModel::VisibilityTest(E3DWindow& win) { return win.Test(*reinterpret_cast<EBound3*>(m_bounds)); }

// 8022C860
// Unowned models take the lights at their bound sphere. Faded models and blended shaders go into the
// order table; the rest draw at once with the draw matrix, or the controller's node matrices.
void EIStaticModel::Draw(ELevelDrawData& data) {
  if (!m_model || (m_skipInShadowPass && m_level->m_wallFadeState == 2)) return;
  ELights* lights = nullptr;
  const float alpha = GetInstAlpha();
  EMat4* matrix = GetDrawMatrix();
  if (data.m_curMatrix != matrix) {
    data.m_curAnim = nullptr;
    data.m_curMatrix = matrix;
    data.m_rc->ModelMatrices(matrix, 1);
  }
  if (!m_owner) {
    lights = &m_lights;
    CalcLights(*reinterpret_cast<EVec3*>(m_sphere), m_lights, false);
    if (data.m_curLights != lights) {
      data.m_curLights = lights;
      data.m_rc->Lights(lights);
    }
  }
  bool selected = false;
  for (DigestGeomShaderPtr& d : m_digests) {
    if (alpha < 1.0f || (d.m_flags & 0x8000)) {
      EOrderTableData otd;
      otd.m_flags = d.m_flags & 0x3e00;
      EShader* shader = ShaderOf(d.m_shader);
      otd.m_sortKey = shader ? static_cast<short>(shader->m_field02) : 0;
      otd.m_pos = m_sphere;
      otd.m_matrix = matrix;
      otd.m_lights = lights;
      otd.m_callback = AnimOrderTableCallback;
      otd.m_dl = d.m_dl;
      otd.m_animController = m_animController;
      otd.m_skinned = m_skinned;
      otd.m_shader = d.m_shader;
      otd.m_instance = this;
      InsertInOrderTable(otd);
      continue;
    }
    if (!selected) {
      selected = true;
      if (data.m_curLights != lights) {
        data.m_curLights = lights;
        data.m_rc->Lights(lights);
      }
      if (!m_animController) {
        if (data.m_curMatrix != matrix) {
          data.m_curAnim = nullptr;
          data.m_curMatrix = matrix;
          data.m_rc->ModelMatrices(matrix, 1);
        }
      } else {
        UseAnimMatrices(data, m_animController, m_skinned != 0);
      }
    }
    if (data.m_curShader != d.m_shader) {
      data.m_curShader = d.m_shader;
      if (EShader* shader = ShaderOf(d.m_shader)) shader->Select(data.m_rc, 0);
    }
    data.m_rc->DisplayList(d.m_dl);
  }
}

// 8022CC24
// The model as a shadow: (unless hidden through its hidden function) with its draw matrix, or the
// controller's node matrices once computed, each sub-model not hidden this frame and not excluded
// (surface flags 0x401) as its shader's shadow mask. The shader slot is tagged (| 1) so a normal draw
// selects the shader again.
void EIStaticModel::DrawAsShadow(ELevelDrawData& data) {
  if (!m_model) return;
  if (m_hiddenFn && m_hiddenFn(m_hiddenData)) return;
  EAnimController* anim = m_animController;
  if (!anim || !anim->m_matrices || !(anim->m_flags & 2)) {
    EMat4* matrix = GetDrawMatrix();
    if (data.m_curMatrix != matrix) {
      data.m_curMatrix = matrix;
      data.m_curAnim = nullptr;
      data.m_rc->ModelMatrices(matrix, 1);
    }
  } else {
    UseAnimMatrices(data, anim, m_skinned != 0);
  }
  for (DigestGeomShaderPtr& d : m_digests) {
    if ((d.m_flags & 0x401) || (d.m_nodeFlags & 8)) continue;
    auto* tagged = reinterpret_cast<ERShader*>(reinterpret_cast<uintptr_t>(d.m_shader) | 1);
    if (data.m_curShader != tagged) {
      data.m_curShader = tagged;
      if (EShader* shader = ShaderOf(d.m_shader)) shader->SelectForShadowMask(data.m_rc);
    }
    data.m_rc->DisplayList(d.m_dl);
  }
}

// 8022CE1C
// The model's shadow on the floor of its room: outside through the level's sun projection; inside a
// room flattened along the room's shadow light (CalcShadowLight) under the current view.
void EIStaticModel::DrawShadow(ELevelDrawData& data) {
  if (!m_model) return;
  EAnimController* anim = m_animController;
  if (!anim || !anim->m_matrices || !(anim->m_flags & 2)) {
    EMat4* matrix = GetDrawMatrix();
    if (data.m_curMatrix != matrix) {
      data.m_curMatrix = matrix;
      data.m_curAnim = nullptr;
      data.m_rc->ModelMatrices(matrix, 1);
    }
  } else {
    UseAnimMatrices(data, anim, m_skinned != 0);
  }
  short room = 0, level = 0;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, room, level);
  if (level < 0) return;
  if (level == 0) {
    if (data.m_fieldB0 != &data.m_shadowProj) {
      data.m_fieldB0 = &data.m_shadowProj;
      data.m_rc->ViewMatrix(&data.m_shadowProj, 0, 0.0f);
    }
  } else {
    EVec3 light{0.0f, 0.0f, 0.0f};
    CalcShadowLight(light, level);
    EMat4 flat, proj;
    EVec3 scale{1.0f, 1.0f, 0.0f};
    flat.Scale(scale);
    flat.m[2][0] = -light.x / light.z;
    flat.m[2][1] = -light.y / light.z;
    proj.Mult4x4(flat, static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow)->m_lookAt);
    data.m_rc->ViewMatrix(&proj, 0, 0.0f);
    data.m_fieldB0 = nullptr;
  }
  DrawAsShadow(data);
}

// 8022D08C
// An order-table record of a model: its shader with the instance's modifiable colour, its strips (the
// level has set the matrices and lights).
void EIStaticModel::AnimOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  const unsigned int color = otd->m_instance->GetModifiableColor();
  if (data.m_curShader != otd->m_shader) {
    data.m_curShader = otd->m_shader;
    if (EShader* shader = ShaderOf(otd->m_shader)) shader->Select(data.m_rc, color);
  }
  data.m_rc->DisplayList(otd->m_dl);
}

// 8022D150
// Every sub-model shader `from` draws with shader `to` (its draw record takes a reference to it).
// `from` is the sub-model shader's own id, so a later change replaces the earlier one. PC: the draw
// records keep that id (GC: walked alongside the sub-model shaders, one record each).
void EIStaticModel::ChangeShader(unsigned int from, unsigned int to) {
  if (!m_model) return;
  for (DigestGeomShaderPtr& d : m_digests) {
    if (d.m_srcShaderId != from) continue;
    if (d.m_shader) d.m_shader->DelRef();
    d.m_shader = static_cast<ERShader*>(_shaderman.AddRef(to, nullptr, 0));
  }
}

// 8022D38C
void EIStaticModel::RebuildShaders() {
  if (!m_model) return;
  for (DigestGeomShaderPtr& d : m_digests)
    if (d.m_shader) d.m_shader->DelRef();
  m_digests.clear();
  BuildDigests();
}

// 8022D418
void EIStaticModel::RealizeShaderTuning() {
  RebuildShaders();
  if (m_level) {
    m_level->RemoveInstanceFromRoom(this);
    m_level->AddInstanceToRoom(this);
  }
}

// 8022DAAC
int EIStaticModel::HasModifiableColor() { return m_model ? m_model->HasModifiableColor() : 0; }
// 8022DAE0
void EIStaticModel::ModifyColor(unsigned int color) { m_color = color; }
// 8022DAE8
unsigned int EIStaticModel::GetModifiableColor() { return m_color; }

// 8022DF1C
const char* EIStaticModel::GetInstName(EInstance__InstType& type) {
  type = EInstance__InstType{3};
  return m_model ? "N/A" : "Unnamed Model";
}

// 8022E1A8
void EIStaticModel::GetPos(EVec3& out) { out = EVec3{m_orient.m[3][0], m_orient.m[3][1], m_orient.m[3][2]}; }
// 8022E274
unsigned int EIStaticModel::GetModelId() { return m_modelId; }
// 8022E27C
ERModel* EIStaticModel::GetModel() { return m_model; }
// 8022E284
EMat4* EIStaticModel::GetDrawMatrix() { return &m_drawMatrix; }
// 8022C104
void EIStaticModel::GetOrient(EMat4& out) { out = m_orient; }

// 8022DAF0
// Draws the model at once with the matrices already loaded (UI and sim drawing): every sub-model whose
// node `anim` does not hide, each shader with the instance's colour and its mip-map bias.
void EIStaticModel::DrawImmediate(ERC* rc, EAnimController* anim) {
  if (!m_model) return;
  if (m_hiddenFn && m_hiddenFn(m_hiddenData)) return;
  EACNodeState* states = anim ? anim->m_states : nullptr;
  for (ESubModel* sub : m_model->m_subModels) {
    const int node = static_cast<short>(sub->m_field0C);
    if (node != -1 && states && (states[node].m_flags & EACNodeState::kHidden)) continue;
    for (ESubModelShader* s : sub->m_shaders) {
      const unsigned int color = GetModifiableColor();
      if (EShader* shader = s->GetEShader()) shader->Select(rc, color);
      rc->SetMipMap(s->m_mipMap, 0);
      rc->DisplayList(s->m_dl);
    }
  }
}

// 8022D60C
// Every shader of the model is cloned ("cloneN" of its id) and given `texture` in layer 0 (shader
// `secondShader` is given `second`), unless it already has it.
void EIStaticModel::ReplaceBaseTexture(ERTexture* texture, unsigned int secondShader, ERTexture* second) {
  if (!m_model || m_digests.empty()) return;
  size_t digest = 0;
  for (ESubModel* sub : m_model->m_subModels) {
    for (ESubModelShader* s : sub->m_shaders) {
      if (digest >= m_digests.size()) return;
      ERShader* shader = m_digests[digest].m_shader;
      if (!shader || (shader->m_shaderFlags & 2)) continue;
      EShader* es = shader->GetShader();
      if (!es || ((es->m_surface >> 26) & 1)) continue;
      ERTexture* current = shader->m_textures[0];
      const bool isSecond = shader->m_id == secondShader;
      ERTexture* replacement = isSecond ? second : texture;
      if (!replacement) continue;
      if (current && current->m_id == replacement->m_id) continue;
      char name[12];
      unsigned int retries = 0, n = 2;
      std::snprintf(name, sizeof name, "clone%i", 1);
      unsigned int id = static_cast<unsigned int>(EChecksum::ComputeSymbol(name, shader->m_id));
      while (_shaderman.IsLoaded(id)) {
        if (n > 99) {
          if (retries) {
            ++retries;
            break;
          }
          retries = 1;
          _shaderman.FreeUnreferencedResources();
          n = 1;
        }
        std::snprintf(name, sizeof name, "clone%u", n++);
        id = static_cast<unsigned int>(EChecksum::ComputeSymbol(name, shader->m_id));
      }
      if (retries > 1) return;
      ERShader* clone = shader->Clone(id);
      if (!clone) continue;
      _shaderman.AddResource(clone, id);
      clone->AddRef();
      if (s->m_shader) s->m_shader->DelRef();
      s->m_shader = clone;
      shader->DelRef();
      m_digests[digest].m_shader = clone;
      clone->ReplaceTexture(replacement, 0);
      ++digest;  // (as on GC: only a replaced shader moves on to the next digest)
    }
  }
}
