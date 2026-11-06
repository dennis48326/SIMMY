// Port of game/e_istaticsubmodel.cpp (partial): one sub-model of a model placed in a level. Still
// stubbed: drawing as a shadow, wire frames and EStream serialisation.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
// Sky domes sort behind everything else (GC: s_vDistantPointToForceSkySortOrder).
float s_vDistantPointToForceSkySortOrder[3] = {1e8f, 1e8f, 0.0f};

// Selects the record's shader (with the instance's modifiable colour) unless current, and draws its
// strips.
void DrawRecord(ELevelDrawData& data, EOrderTableData* otd) {
  EInstance* inst = otd->m_instance;
  const unsigned int color = inst->GetModifiableColor();
  if (data.m_curShader != otd->m_shader) {
    data.m_curShader = otd->m_shader;
    if (EShader* shader = otd->m_shader->GetShader()) shader->Select(data.m_rc, color);
  }
  data.m_rc->DisplayList(otd->m_dl);
}

// The model matrix, unless it is current (it also invalidates the animated-model matrix).
void UseMatrix(ELevelDrawData& data, EMat4* m) {
  if (data.m_curMatrix != m) {
    data.m_curAnim = nullptr;
    data.m_curMatrix = m;
    data.m_rc->ModelMatrices(m, 1);
  }
}
}  // namespace

// 8022F7D8 EIStaticSubModel::GetTypeInfo
// 8022F7E4 EIStaticSubModel::GetTypeName
// 8022F7F0 EIStaticSubModel::GetTypeKey
// 8022F7FC EIStaticSubModel::GetTypeVersion
// 8022F798 EIStaticSubModel::SafeDelete
// 8022F720 EIStaticSubModel::New
// 8022F748 EIStaticSubModel::Construct
// 8022F774 EIStaticSubModel::Destruct
// 8022F808 EIStaticSubModel::GetTypeInfoStatic
// 8022F814 EIStaticSubModel::GetReadVersion
SIMMY_STORABLE_IMPL(EIStaticSubModel, "EIStaticSubModel", 0, EInstance)

// 8022E360
EIStaticSubModel::EIStaticSubModel() {}

// 8022E3F8
EIStaticSubModel::~EIStaticSubModel() {
  Deallocate();
  m_level = nullptr;
}

// 8022E45C
void EIStaticSubModel::Deallocate() { DeallocateModel(); }

// 8022E5A8
// The instance, then (version 0) the model id, the sub-model index and the bound sphere; then the
// model is taken and the draw records built.
void EIStaticSubModel::Load(EFile& file) {
  Deallocate();
  EInstance::Load(file);
  if (m_typeInfo.m_readVersion == 0) {
    m_modelId = simmy::read_u32(file);
    m_subModel = static_cast<int>(simmy::read_u32(file));
    for (float& f : m_sphere) f = simmy::read_float(file);
  }
  SetupModel();
}

// 8022E67C
void EIStaticSubModel::GetBoundSphere(EBoundSphere& out) {
  float* s = reinterpret_cast<float*>(&out);
  for (int i = 0; i < 4; ++i) s[i] = m_sphere[i];
}

// 8022E6A4
void EIStaticSubModel::DeallocateModel() {
  if (m_model) {
    m_model->DelRef();
    m_model = nullptr;
  }
  for (DigestGeomShaderPtr& d : m_digests)
    if (d.m_shader) d.m_shader->DelRef();
  m_digests.clear();
  m_color = 0;
}

// 8022E738
void EIStaticSubModel::SetupModel() {
  DeallocateModel();
  m_model = static_cast<ERModel*>(_modelman.AddRef(m_modelId, nullptr, 0));
  BuildDigests();
}

// 8022E788
// One record per shader of the sub-model: its sort layer, a reference to the shader, the shader's
// flags and the strips' display list. Models that never fade, interest-fade or cull clear the
// instance's settings.
void EIStaticSubModel::BuildDigests() {
  if (!m_model || m_subModel >= static_cast<int>(m_model->m_subModels.size())) return;
  ESubModel* sub = m_model->m_subModels[m_subModel];
  m_color = 0;
  m_digests.clear();
  m_digests.reserve(sub->m_shaders.size());
  for (ESubModelShader* s : sub->m_shaders) {
    DigestGeomShaderPtr d;
    d.m_dl = s->m_dl;
    d.m_node = static_cast<short>(sub->m_field0C);
    d.m_nodeFlags = 0;
    d.m_shader = static_cast<ERShader*>(_shaderman.AddRef(s->m_shaderId, nullptr, 0));
    bool alpha = m_shaderFlags != 0;
    if (d.m_shader) d.UpdateShaderFlags(alpha);
    m_shaderFlags = alpha ? 1 : 0;
    m_digests.push_back(d);
  }
  if (m_model->m_dontFrontPlaneFade) m_fadeMode = 0;
  if (m_model->m_dontInterestFade) m_interestFade = 0;
  if (m_model->m_dontCull) m_cullable = 0;
}

// 8022E8AC
unsigned int EIStaticSubModel::VisibilityTest(E3DWindow& win) {
  return win.Test(*reinterpret_cast<EBound3*>(m_bounds));
}

// 8022E9EC
// Opaque geometry is drawn at once (selecting the level's lights, the model's scale matrix and the
// shader); fading instances and blended shaders go into the order table unless the level draws
// without one. Sky domes (flag 0x2000) use the sky lights, sort behind everything and are drawn
// with the level's sky matrix (scaled down, around the eye); floors (0x400) draw every strip at once with the instance's colour.
void EIStaticSubModel::Draw(ELevelDrawData& data) {
  const bool fading = GetInstAlpha() < 1.0f || m_fieldA4 != 0;
  EOrderTableData otd;
  otd.m_pos = m_sphere;
  otd.m_lights = &m_level->m_lightGrid.m_outside;
  otd.m_matrix = m_model->GetScaleMatrix();
  otd.m_callback = OrderTableCallback;
  otd.m_instance = this;
  if (data.m_curLights != otd.m_lights) {
    data.m_curLights = otd.m_lights;
    data.m_rc->Lights(otd.m_lights);
  }
  if (m_digests.empty()) return;
  const unsigned int flags = m_digests[0].m_flags;
  if (flags & 0x2000) {
    otd.m_lights = &m_level->m_skyLights;
    if (data.m_curLights != otd.m_lights) {
      data.m_curLights = otd.m_lights;
      data.m_rc->Lights(otd.m_lights);
    }
    otd.m_callback = SkyOrderTableCallback;
    otd.m_pos = s_vDistantPointToForceSkySortOrder;
    otd.m_matrix = &data.m_skyMatrix;  // GC: the draw data itself, whose first matrix is the sky's
    for (DigestGeomShaderPtr& d : m_digests) {
      if (fading || (otd.m_flags & 0x8000)) {
        otd.m_dl = d.m_dl;
        otd.m_animController = nullptr;
        otd.m_shader = d.m_shader;
        otd.m_flags = d.m_flags;
        EShader* shader = d.m_shader->GetShader();
        otd.m_sortKey = shader ? static_cast<short>(shader->m_field02) : 0;
        InsertInOrderTable(otd);
      } else {
        UseMatrix(data, otd.m_matrix);
        if (data.m_curShader != d.m_shader) {
          data.m_curShader = d.m_shader;
          if (EShader* shader = d.m_shader->GetShader()) shader->Select(data.m_rc, 0);
        }
        data.m_rc->DisplayList(d.m_dl);
      }
    }
    return;
  }
  if (flags & 0x400) {
    otd.m_callback = FloorOrderTableCallback;
    UseMatrix(data, otd.m_matrix);
    for (DigestGeomShaderPtr& d : m_digests) {
      const unsigned int color = GetModifiableColor();
      if (data.m_curShader != d.m_shader) {
        data.m_curShader = d.m_shader;
        if (EShader* shader = d.m_shader->GetShader()) shader->Select(data.m_rc, color);
      }
      data.m_rc->DisplayList(d.m_dl);
    }
    return;
  }
  for (DigestGeomShaderPtr& d : m_digests) {
    otd.m_dl = d.m_dl;
    otd.m_animController = nullptr;
    otd.m_shader = d.m_shader;
    otd.m_flags = d.m_flags;
    otd.m_callback = (d.m_flags & 0xc) ? FaceOrderTableCallback : OrderTableCallback;
    const bool sorted = (fading || (d.m_flags & 0x8000)) && data.m_noOrderTable == 0;
    if (sorted) {
      EShader* shader = d.m_shader->GetShader();
      otd.m_sortKey = shader ? static_cast<short>(shader->m_field02) : 0;
      InsertInOrderTable(otd);
    } else {
      UseMatrix(data, otd.m_matrix);
      otd.m_callback(data, &otd);
    }
  }
}

// 8022F0A4
void EIStaticSubModel::SkyOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) { DrawRecord(data, otd); }

// 8022F168
// Facing geometry (shader flags 4/8) turns its strips about their position to face the camera: the
// model's scale matrix about the record's position times the inverse camera rotation.
void EIStaticSubModel::FaceOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  auto* inst = static_cast<EIStaticSubModel*>(otd->m_instance);
  EMat4 m = *inst->m_model->GetScaleMatrix();
  const float* p = otd->m_pos;
  EVec3 back{-p[0], -p[1], -p[2]};
  m.PostTranslate(back);
  EMat4 rot;
  rot.Transpose(data.m_win->m_lookAt);
  rot.m[0][3] = rot.m[1][3] = rot.m[2][3] = 0.0f;
  rot.m[3][0] = rot.m[3][1] = rot.m[3][2] = 0.0f;
  EMat4 r;
  r.Mult4x4(m, rot);
  m = r;
  EVec3 to{p[0], p[1], p[2]};
  m.PostTranslate(to);
  data.m_curMatrix = &m;
  data.m_curAnim = nullptr;
  data.m_rc->ModelMatrices(&m, 1);
  data.m_curAnim = nullptr;
  data.m_curMatrix = nullptr;
  DrawRecord(data, otd);
}

// 8022F384
void EIStaticSubModel::OrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) { DrawRecord(data, otd); }

// 8022F448
void EIStaticSubModel::FloorOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) { DrawRecord(data, otd); }

// 8022F50C
int EIStaticSubModel::HasModifiableColor() {
  for (DigestGeomShaderPtr& d : m_digests) {
    EShader* shader = d.m_shader ? d.m_shader->GetShader() : nullptr;
    if (shader && ((shader->m_layers[0].m_flags | shader->m_layers[1].m_flags) & 0x20)) return 1;
  }
  return 0;
}

// 8022F588
void EIStaticSubModel::ModifyColor(unsigned int color) { m_color = color; }
// 8022F590
unsigned int EIStaticSubModel::GetModifiableColor() { return m_color; }

// 8022F598
void EIStaticSubModel::RebuildShaders() {
  for (DigestGeomShaderPtr& d : m_digests)
    if (d.m_shader) d.m_shader->DelRef();
  m_digests.clear();
  if (m_model && m_subModel <= static_cast<int>(m_model->m_subModels.size())) BuildDigests();
}

// 8022F634
void EIStaticSubModel::RealizeShaderTuning() { RebuildShaders(); }

// 8022F668
const char* EIStaticSubModel::GetInstName(EInstance__InstType& type) {
  type = EInstance__InstType{EInstance__InstType::kStaticModel};
  return m_model ? "N/A" : "Unnamed Model";
}
