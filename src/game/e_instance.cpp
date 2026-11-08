// Port of game/e_instance.cpp (partial) and DigestGeomShaderPtr from game/e_igameinstance.cpp: the
// base of everything placed in a level. Still stubbed: the weighted shadow direction (the light grid's
// vertex weights are not ported), per-instance lighting (CalcLights, CalcLights4) and EStream serialisation.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>

int EInstance::s_context = 0;  // _instanceContext

// 802293A0 EInstance::GetTypeInfo
// 802293AC EInstance::GetTypeName
// 802293B8 EInstance::GetTypeKey
// 802293C4 EInstance::GetTypeVersion
// 80229360 EInstance::SafeDelete
// 802292F4 EInstance::New
// 8022931C EInstance::Construct
// 8022933C EInstance::Destruct
// 802293D0 EInstance::GetTypeInfoStatic
// 802293DC EInstance::GetReadVersion
SIMMY_STORABLE_IMPL(EInstance, "EInstance", 1, EStorable)

// 80228290
EInstance::EInstance() {}

// 80228370
EInstance::~EInstance() { RemoveFromLevel(); }

// 802285C0
// Version 0 and 1: the id, the level and bound-tree pointers, the flags and the bounds (version 0
// had two more words).
void EInstance::Load(EFile& file) {
  const unsigned short version = m_typeInfo.m_readVersion;
  if (version > 1) return;
  m_id = simmy::read_u32(file);
  EStorable* level = nullptr;
  file >> level;
  m_level = static_cast<ERLevel*>(level);
  file >> m_boundNode;
  m_flags = simmy::read_u32(file);
  for (float& f : m_bounds) f = simmy::read_float(file);
  if (version == 0) {
    simmy::read_u32(file);
    simmy::read_u32(file);
  }
}

// 8022877C
// Outside a level the bounds are set directly; in one, through the level.
void EInstance::SetBounds(EBound3& bounds) {
  if (!m_level) {
    const float* b = reinterpret_cast<const float*>(&bounds);
    for (int i = 0; i < 6; ++i) m_bounds[i] = b[i];
  } else {
    m_level->SetBounds(this, bounds);
  }
}

// 80228818
void EInstance::RemoveFromLevel() {
  if (m_level) m_level->RemoveInstance(this);
}

// 80228F00
// The lights of the instance's room (or, for the other side, of the room beyond its portal): the
// room's ambient and sun, then the point lights at `pos`.
void EInstance::CalcLights(EVec3& pos, ELights& out, bool otherSide) {
  short room = 0, other = 0;
  if (!otherSide) {
    if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, room, other);
    else room = other = 0;
  } else {
    if (m_otherSideFn) m_otherSideFn(m_otherSideData, false, room, other);
    else room = other = -1;
  }
  if (other < 0) other = 0;
  const ELightGrid__RoomData& r = m_level->m_lightGrid.m_rooms[other];
  if (!r.m_ambient.m_light) {
    out.m_ambient[0] = out.m_ambient[1] = out.m_ambient[2] = 0.0f;
  } else {
    out.m_ambient[0] = r.m_ambient.m_color.x;
    out.m_ambient[1] = r.m_ambient.m_color.y;
    out.m_ambient[2] = r.m_ambient.m_color.z;
  }
  if (!r.m_sun.m_light) {
    out.m_numDir = 0;
  } else {
    out.m_dir[0].m_color[0] = r.m_sun.m_color.x;
    out.m_dir[0].m_color[1] = r.m_sun.m_color.y;
    out.m_dir[0].m_color[2] = r.m_sun.m_color.z;
    out.m_dir[0].m_dir[0] = r.m_sun.m_dir.x;
    out.m_dir[0].m_dir[1] = r.m_sun.m_dir.y;
    out.m_dir[0].m_dir[2] = r.m_sun.m_dir.z;
    out.m_numDir = 1;
  }
  m_level->GetLightsAtLocation(pos, other, out);
}

// 8022907C
// The same as CalcLights (the room's ambient and sun, then up to four point lights at `pos`); the
// wall parts use it for their first point (EIWallPart::Draw).
void EInstance::CalcLights4(EVec3& pos, ELights& out, bool otherSide) {
  short room = 0, other = 0;
  if (!otherSide) {
    if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, room, other);
    else room = other = 0;
  } else {
    if (m_otherSideFn) m_otherSideFn(m_otherSideData, false, room, other);
    else room = other = -1;
  }
  if (other < 0) other = 0;
  const ELightGrid__RoomData& r = m_level->m_lightGrid.m_rooms[other];
  if (!r.m_ambient.m_light) {
    out.m_ambient[0] = out.m_ambient[1] = out.m_ambient[2] = 0.0f;
  } else {
    out.m_ambient[0] = r.m_ambient.m_color.x;
    out.m_ambient[1] = r.m_ambient.m_color.y;
    out.m_ambient[2] = r.m_ambient.m_color.z;
  }
  if (!r.m_sun.m_light) {
    out.m_numDir = 0;
  } else {
    out.m_dir[0].m_color[0] = r.m_sun.m_color.x;
    out.m_dir[0].m_color[1] = r.m_sun.m_color.y;
    out.m_dir[0].m_color[2] = r.m_sun.m_color.z;
    out.m_dir[0].m_dir[0] = r.m_sun.m_dir.x;
    out.m_dir[0].m_dir[1] = r.m_sun.m_dir.y;
    out.m_dir[0].m_dir[2] = r.m_sun.m_dir.z;
    out.m_numDir = 1;
  }
  m_level->GetLightsAtLocation(pos, other, out);
}

// 802291F8
void EInstance::InsertInOrderTable(EOrderTableData& data) { m_level->InsertInOrderTable(data); }

// 8022921C
void EInstance::SetContext(int context) { s_context = context; }
// 80229224
int EInstance::GetContext() { return s_context; }

// 8022922C
// The fade alpha of the current context, kept in 0..1.
void EInstance::SetInstAlpha(float alpha) {
  if (alpha <= 0.0f) alpha = 0.0f;
  if (alpha >= 1.0f) alpha = 1.0f;
  m_alpha[s_context] = alpha;
}
// 80229270
float EInstance::GetInstAlpha() { return m_alpha[s_context]; }

// 8022945C
void EInstance::Init() {}
// 80229460
void EInstance::Update() {}
// 80229464
void EInstance::DoAnimation() {}
// 80229468
void EInstance::DoLightingCalculation() {}
// 8022946C
unsigned int EInstance::VisibilityTest(E3DWindow&) { return 0; }
// 80229474
void EInstance::Draw(ELevelDrawData&) {}
// 80229478
void EInstance::DrawStencil(ELevelDrawData&) {}
// 8022947C
void EInstance::DrawShadow(ELevelDrawData&) {}
// 80229480
void EInstance::DrawWireFrame(ERC*) {}
// 8022965C
void EInstance::GetShadowCenter(EVec3&) const {}

// 80228848
// A shadow direction is kept pointing well down: 0.8 more downwards, normalised (never level).
void TruncateLightDirection(EVec3& dir) {
  dir.z -= 0.8f;
  if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) {
    const float inv = 1.0f / std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir = EVec3{dir.x * inv, dir.y * inv, dir.z * inv};
  }
  if (dir.z == 0.0f) dir.z = -0.2f;
}

// 802288D8
// The direction shadows fall at `point` in `room`: the sum of the directions away from up to four of
// the room's point lights above the point (from the light grid), each weighted by the light's
// brightness (luma x range) over the squared distance.
void EInstance::ShadowDirAtPoint(EVec3& point, EVec3& out, int room) {
  out = EVec3{0.0f, 0.0f, 0.0f};
  const ELightGrid& grid = m_level->m_lightGrid;
  const short* lights = grid.GetLightsAtLocation(point, room);
  for (int i = 0; i < 4 && lights[i] != -1; ++i) {
    const ELightGrid__LightData& l = grid.m_lights[lights[i]];
    if (l.m_dir.z < 0.0f) continue;
    const EVec3 d{point.x - l.m_dir.x, point.y - l.m_dir.y, point.z - l.m_dir.z};
    const float sq = d.x * d.x + d.y * d.y + d.z * d.z;
    if (sq <= 0.0f) continue;
    const float w = (l.m_color.z * 0.11f + (l.m_color.x * 0.3f + l.m_color.y * 0.59f)) * l.m_range / sq;
    out.x += d.x * w;
    out.y += d.y * w;
    out.z += d.z * w;
  }
}

// 80228CB4
// As ShadowDirAtPoint with the grid's lights at the point weighted across its cell (up to twelve).
RET_UNKNOWN EInstance::ShadowDirAtPointWeighted(EVec3& point, EVec3& out, int room) {
  out = EVec3{0.0f, 0.0f, 0.0f};
  ELightGrid__LightWeight weights[12];
  ELightGrid& grid = m_level->m_lightGrid;
  grid.GetLightWeightsAtLocation(point, room, weights);
  for (int i = 0; i < 12 && weights[i].m_index != -1; ++i) {
    const ELightGrid__LightData& l = grid.m_lights[weights[i].m_index];
    if (l.m_dir.z < 0.0f) continue;
    const EVec3 d{point.x - l.m_dir.x, point.y - l.m_dir.y, point.z - l.m_dir.z};
    const float sq = d.x * d.x + d.y * d.y + d.z * d.z;
    if (!(0.0f < sq)) continue;
    const float w = weights[i].m_weight * (l.m_color.z * 0.11f + (l.m_color.x * 0.3f + l.m_color.y * 0.59f)) * l.m_range / sq;
    out.x += d.x * w;
    out.y += d.y * w;
    out.z += d.z * w;
  }
  return 0;
}

// 80228A74
// The direction of the instance's shadow in `room`: the room's sun (scaled by its intensity) when it
// has one, else from the point lights at the middle of its footprint on the floor; kept downwards.
void EInstance::CalcShadowLight(EVec3& out, short room) {
  if (room < 0) return;
  const ELightGrid__RoomData& r = m_level->m_lightGrid.m_rooms[room];
  if (!r.m_sun.m_light) {
    EVec3 p{(m_bounds[0] + m_bounds[3]) * 0.5f, (m_bounds[1] + m_bounds[4]) * 0.5f, 0.0f};
    if (m_field0C == 0) ShadowDirAtPoint(p, out, room);
    else ShadowDirAtPointWeighted(p, out, room);
  } else {
    const float k = r.m_sun.m_light->m_intensity;
    out = EVec3{r.m_sun.m_dir.x * k, r.m_sun.m_dir.y * k, r.m_sun.m_dir.z * k};
  }
  TruncateLightDirection(out);
}

// 80228BA0
void EInstance::CalcShadowLightWeighted(EVec3& out, short room) {
  if (room < 0) return;
  const ELightGrid__RoomData& r = m_level->m_lightGrid.m_rooms[room];
  if (!r.m_sun.m_light) {
    EVec3 p{(m_bounds[0] + m_bounds[3]) * 0.5f, (m_bounds[1] + m_bounds[4]) * 0.5f, 0.0f};
    ShadowDirAtPointWeighted(p, out, room);
  } else {
    const float k = r.m_sun.m_light->m_intensity;
    out = EVec3{r.m_sun.m_dir.x * k, r.m_sun.m_dir.y * k, r.m_sun.m_dir.z * k};
  }
  TruncateLightDirection(out);
}
// 80229484
void EInstance::SetOrient(EMat4&) {}
// 80229488
void EInstance::RebuildShaders() {}
// 8022948C
const char* EInstance::GetInstName(EInstance__InstType& type) {
  type = EInstance__InstType{0};
  return "Unnamed inst type";
}
// 802294A0
int EInstance::GetUpdatePriority() { return 0; }
// 802294A8
void EInstance::RegisterFloor() {}

// 802294AC
// Asks the location functions to recompute the room (and the other side's).
void EInstance::ResetLocation() {
  short room = 0, other = 0;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, true, room, other);
  if (m_otherSideFn) m_otherSideFn(m_otherSideData, true, room, other);
}

// 80229528
void EInstance::GetLocationId(short& room, short& otherSide) {
  if (!m_thisSideFn) {
    otherSide = 0;
    room = 0;
  } else {
    m_thisSideFn(m_thisSideData, false, room, otherSide);
  }
}

// 80229600
void EInstance::RegisterThisSideFn(unsigned int data, LocationFn fn) {
  m_thisSideFn = fn;
  m_thisSideData = data;
}

// 80229674
ERLevel* EInstance::GetLevel() { return m_level; }
// 802296E4
int EInstance::HasModifiableColor() { return 0; }
// 802296EC
void EInstance::ModifyColor(unsigned int) {}
// 802296F8
void EInstance::AddedToLevel(ERLevel*) {}
// 802296FC
void EInstance::AboutToBeRemovedFromLevel(ERLevel*) {}
// 80229700
void EInstance::RemovedFromLevel(ERLevel*) {}
// 80229740
void EInstance::RealizeShaderTuning() {}

// 80227530
// The shader's surface flags; a layer blending with the frame buffer (blend mode 1 in any of three
// selectors) marks it 0x8000 (alpha sorted) unless it is 0x800; 0x20 tells the instance.
void DigestGeomShaderPtr::UpdateShaderFlags(bool& alpha) {
  EShader* shader = m_shader->GetShader();
  m_flags = shader->m_surface;
  const EShaderLayer& layer = shader->m_layers[0];
  const bool blends = (layer.m_field0C & 0x40) && (layer.m_blend[0] == 1 || layer.m_blend[1] == 1 || layer.m_blend[3] == 1);
  if (blends && !(m_flags & 0x800)) m_flags |= 0x8000;
  if (m_flags & 0x20) alpha = true;
}
