// Port of game/e_ipointlight.cpp (EIPointLight), game/e_iportalpointlight.cpp (EIPortalPointLight)
// and game/e_ispotlight.cpp (EISpotLight, partly): lights with a position. A point light's range for
// the lighting grid is a quarter of its falloff end; moving it to another tile or room rebuilds the
// grid. A portal light sits 0.8 in front of (or behind) its window at a height by the window's size
// and is as strong as the window lets through (its colour comes from the light grid: the outside's
// sun or the other room's lights).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/epointlight.hpp"
#include "simmy/detmath.hpp"

#include <cfloat>
#include <cmath>

namespace {
// 804FE9D4.. (outer radius, intensity: [0] outside, [1] inside)
const float c_outerLarge[2] = {12.0f, 24.0f};
const float c_outerRegular[2] = {7.0f, 14.0f};
const float c_outerSmall[2] = {6.0f, 8.0f};
const float c_intensityLarge[2] = {0.28f, 0.4f};
const float c_intensityRegular[2] = {0.24f, 0.4f};
const float c_intensitySmall[2] = {0.22f, 0.4f};
const float _portalOffset = 0.8f;  // 804FEA04

// EBound3::Compute(radius, out, centre): the box of a sphere.
void SphereBounds(EInstance* inst, const EVec3& c, float r) {
  float b[6] = {c.x - r, c.y - r, c.z - r, c.x + r, c.y + r, c.z + r};
  inst->SetBounds(*reinterpret_cast<EBound3*>(b));
}
void InfiniteBounds(EInstance* inst) {
  float b[6] = {-FLT_MAX, -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
  inst->SetBounds(*reinterpret_cast<EBound3*>(b));
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// EIPointLight

// 8022A4B4 EIPointLight::New
// 8022A4DC EIPointLight::Construct
// 8022A4FC EIPointLight::Destruct
// 8022A520 EIPointLight::SafeDelete
// 8022A560 EIPointLight::GetTypeInfo
// 8022A56C EIPointLight::GetTypeName
// 8022A578 EIPointLight::GetTypeKey
// 8022A584 EIPointLight::GetTypeVersion
// 8022A590 EIPointLight::GetTypeInfoStatic
// 8022A59C EIPointLight::GetReadVersion
SIMMY_STORABLE_IMPL(EIPointLight, "EIPointLight", 0, EILight)

// 80229DCC
EIPointLight::EIPointLight() {
  m_pos = EVec3{0.0f, 0.0f, 0.0f};
  m_tileX = 0;
  m_room = -1;
  m_falloffStart = 10.0f;
  m_falloffEnd = 20.0f;
  m_useFalloff = 1;
  m_tileY = 0;
  Setup();
}

// 8022A008
// Full-intensity light at `point`: the intensity (0.5 when 0), by the falloff (falloffEnd / 4d)^2 in
// the ground plane, at most 1.
void EIPointLight::CalcFullIntensityLightOnPoint(EVec3& point, float& out) {
  const float dy = point.y - m_pos.y, dx = point.x - m_pos.x;
  const float d = std::sqrt(dx * dx + dy * dy);
  out = m_intensity;
  if (m_intensity == 0.0f) out = 0.5f;
  if (d != 0.0f) {
    float k = 1.0f;
    if (m_useFalloff) {
      k = m_falloffEnd / (d * 4.0f);
      k = k * k;
    }
    out = k * out;
    if (1.0f < out) out = 1.0f;
  }
}

// 8022A0E0
// Range: a quarter of the falloff end; the position; colour times intensity (times the intensity
// scale when one is set and not 1).
void EIPointLight::LightingParameters(EVec3& dir, float& range, EVec3& pos, EVec3& color) {
  range = m_falloffEnd * 0.25f;
  GetPosition(pos);
  const float k = m_intensity;
  color = EVec3{k * m_color.x, k * m_color.y, k * m_color.z};
  const EVec3& s = m_intensityScale;
  if ((s.x != 0.0f || s.y != 0.0f || s.z != 0.0f) && (s.x != 1.0f || s.y != 1.0f || s.z != 1.0f)) {
    color.x *= s.x;
    color.z *= s.z;
    color.y *= s.y;
  }
  dir = EVec3{0.0f, 0.0f, 0.0f};
}

// 8022A278
// A move to another tile (or room) means the lights have changed.
void EIPointLight::SetPosition(EVec3& pos) {
  m_pos = pos;
  short level = 0, room = 0;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, level, room);
  const int x = static_cast<int>(pos.x), y = static_cast<int>(pos.y);
  if (x != m_tileX || y != m_tileY || m_room != room) {
    m_tileX = x;
    m_room = room;
    m_tileY = y;
    EILight::m_haveLightsChanged = 1;
  }
}

// 8022A354
// Bounded by its falloff (unbounded without one).
void EIPointLight::Setup() {
  if (!m_useFalloff) InfiniteBounds(this);
  else SphereBounds(this, m_pos, m_falloffEnd);
}

// 8022A61C
int EIPointLight::GetLightType() { return kPoint; }
// 8022A624
void EIPointLight::GetPosition(EVec3& out) { out = m_pos; }
// 8022A644
double EIPointLight::GetFalloffEnd() { return m_falloffEnd; }
// 8022A64C EIPointLight::GetRoomId (inline in the header)

// ---------------------------------------------------------------------------------------------------
// EIPortalPointLight

// 8022AF8C EIPortalPointLight::New
// 8022AFB4 EIPortalPointLight::Construct
// 8022AFD4 EIPortalPointLight::Destruct
// 8022AFF8 EIPortalPointLight::SafeDelete
// 8022B038 EIPortalPointLight::GetTypeInfo
// 8022B044 EIPortalPointLight::GetTypeName
// 8022B050 EIPortalPointLight::GetTypeKey
// 8022B05C EIPortalPointLight::GetTypeVersion
// 8022B068 EIPortalPointLight::GetTypeInfoStatic
// 8022B074 EIPortalPointLight::GetReadVersion
SIMMY_STORABLE_IMPL(EIPortalPointLight, "EIPortalPointLight", 0, EILight)

// 8022A720
// Its brightness: intensity times (falloff end / 4)^2.
double EIPortalPointLight::GetLumina() const {
  const float r = m_falloffEnd * 0.25f;
  return m_intensity * r * r;
}

// 8022A740
EIPortalPointLight::EIPortalPointLight() {
  m_flags |= 0x80000;
  m_portal = nullptr;
  EVec3 zero{0.0f, 0.0f, 0.0f};
  SetPosition(zero);
  m_falloffStart = 0.0f;
  m_useFalloff = 1;
  m_falloffEnd = c_outerRegular[0];
  m_side = 0;
  m_intensity = c_intensityRegular[0];
  m_field88 = 1;
  Setup();
}

// 8022A7E8
void EIPortalPointLight::SetPortal(EInstance* portal, bool side) {
  m_portal = portal;
  m_side = side ? 1 : 0;
  Update();
}

// 8022A9DC
// Lit only across rooms (dark when both sides are the same room); colour as the grid set it.
void EIPortalPointLight::LightingParameters(EVec3& dir, float& range, EVec3& pos, EVec3& color) {
  range = m_falloffEnd * 0.25f;
  GetPosition(pos);
  dir = EVec3{0.0f, 0.0f, 0.0f};
  short a = 0, here = 0, b = 0, there = -1;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, a, here);
  if (m_otherSideFn) m_otherSideFn(m_otherSideData, false, b, there);
  if (here == there) color = EVec3{0.0f, 0.0f, 0.0f};
  else color = EVec3{m_color.x * m_intensity, m_color.y * m_intensity, m_color.z * m_intensity};
}

// 8022AB40
void EIPortalPointLight::Setup() {
  if (!m_useFalloff) {
    InfiniteBounds(this);
    return;
  }
  EVec3 pos;
  GetPosition(pos);
  SphereBounds(this, pos, m_falloffEnd);
}

// 8022AC50
// Its rooms are the portal's (swapped on the other side); placed _portalOffset in front of (behind)
// the portal at the height of its size (small 2.5, regular 2, large 1) with that size's range and
// intensity (inside or out).
void EIPortalPointLight::Update() {
  EInstance* p = m_portal;
  if (!m_side) {
    m_thisSideData = p->m_thisSideData;
    m_thisSideFn = p->m_thisSideFn;
    m_otherSideFn = p->m_otherSideFn;
    m_otherSideData = p->m_otherSideData;
  } else {
    m_thisSideData = p->m_otherSideData;
    m_thisSideFn = p->m_otherSideFn;
    m_otherSideFn = p->m_thisSideFn;
    m_otherSideData = p->m_thisSideData;
  }
  EMat4 m;
  static_cast<EIStaticModel*>(p)->GetOrient(m);
  const float off = m_side ? -_portalOffset : _portalOffset;
  EVec3 pos{off * m.m[1][0] + m.m[3][0], off * m.m[1][1] + m.m[3][1], off * m.m[1][2] + m.m[3][2]};
  short a = 0, room = 0;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, a, room);
  const int inside = room != 0 ? 1 : 0;
  switch (p->m_field14) {
    case 2:
      m_falloffEnd = c_outerSmall[inside];
      pos.z = 2.5f;
      m_intensity = c_intensitySmall[inside];
      break;
    case 1:
      m_falloffEnd = c_outerRegular[inside];
      pos.z = 2.0f;
      m_intensity = c_intensityRegular[inside];
      break;
    case 3:
      m_falloffEnd = c_outerLarge[inside];
      pos.z = 1.0f;
      m_intensity = c_intensityLarge[inside];
      break;
    default: break;
  }
  SetPosition(pos);
}

// 8022AE6C
// On while switched on and the two sides are different rooms.
RET_UNKNOWN EIPortalPointLight::IsEnabled() {
  short a = 0, here = 0, b = 0, there = -1;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, a, here);
  if (m_otherSideFn) m_otherSideFn(m_otherSideData, false, b, there);
  return there != here && m_field88 != 0;
}

// 8022B0F4
int EIPortalPointLight::GetLightType() { return kPortal; }
// 8022B0FC EIPortalPointLight::GetPortal (inline in the header)
// 8022B104
RET_UNKNOWN EIPortalPointLight::IsPointLight() { return 1; }
// 8022B10C EIPortalPointLight::GetRoomId (EIPointLight's)

// ---------------------------------------------------------------------------------------------------
// EISpotLight

// 8022B76C EISpotLight::New
// 8022B794 EISpotLight::Construct
// 8022B7B4 EISpotLight::Destruct
// 8022B7D8 EISpotLight::SafeDelete
// 8022B818 EISpotLight::GetTypeInfo
// 8022B824 EISpotLight::GetTypeName
// 8022B830 EISpotLight::GetTypeKey
// 8022B83C EISpotLight::GetTypeVersion
// 8022B848 EISpotLight::GetTypeInfoStatic
// 8022B854 EISpotLight::GetReadVersion
SIMMY_STORABLE_IMPL(EISpotLight, "EISpotLight", 0, EILight)

// 8022B1E0
EISpotLight::EISpotLight() { Setup(); }

// 8022B4E8
// Bounded by its falloff sphere (or nothing), then cut to the half-spaces its cone reaches: along each
// axis, a direction more than the cone's half angle + 90 degrees from the light's direction is out of
// reach, so the bounds stop at the light's position on that side. The cosines of the half angles are
// kept for the cone test. (A spot light is not taken by the light grid: its type is none.)
void EISpotLight::Setup() {
  float b[6];
  if (!m_useFalloff) {
    b[0] = b[1] = b[2] = -3.4028235e+38f;
    b[3] = b[4] = b[5] = 3.4028235e+38f;
  } else {
    b[0] = m_pos.x - m_falloffEnd, b[1] = m_pos.y - m_falloffEnd, b[2] = m_pos.z - m_falloffEnd;
    b[3] = m_pos.x + m_falloffEnd, b[4] = m_pos.y + m_falloffEnd, b[5] = m_pos.z + m_falloffEnd;
  }
  const float reach = m_outerAngle * 0.5f + 1.5707964f;
  const float pos[3] = {m_pos.x, m_pos.y, m_pos.z};
  const float dir[3] = {m_dir.x, m_dir.y, m_dir.z};
  for (int i = 0; i < 3; ++i) {
    if (simmy::dm::acos(dir[i]) < reach) {          // +axis . dir
      if (reach <= simmy::dm::acos(-dir[i])) b[i] = pos[i];  // -axis out of reach
    } else {
      b[3 + i] = pos[i];
    }
  }
  SetBounds(*reinterpret_cast<EBound3*>(b));
  m_cosInner = simmy::dm::cos(m_innerAngle * 0.5f);
  m_cosOuter = simmy::dm::cos(m_outerAngle * 0.5f);
}
