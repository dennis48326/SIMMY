// Port of game/e_ilight.cpp, game/e_idirlight.cpp and game/e_iamblight.cpp (partial): lights as level
// instances. Still stubbed: EStream serialisation, point-light falloff (CalcLightOnPoint) and the
// colour helpers.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cfloat>

int EILight::m_haveLightsChanged = 0;

// 80228008 EILight::GetTypeInfo
// 80228014 EILight::GetTypeName
// 80228020 EILight::GetTypeKey
// 8022802C EILight::GetTypeVersion
// 80227FC8 EILight::SafeDelete
// 80227F5C EILight::New
// 80227F84 EILight::Construct
// 80227FA4 EILight::Destruct
// 80228038 EILight::GetTypeInfoStatic
// 80228044 EILight::GetReadVersion
SIMMY_STORABLE_IMPL(EILight, "EILight", 0, EInstance)
// 80227418 EIDirLight::GetTypeInfo
// 80227424 EIDirLight::GetTypeName
// 80227430 EIDirLight::GetTypeKey
// 8022743C EIDirLight::GetTypeVersion
// 802273D8 EIDirLight::SafeDelete
// 8022736C EIDirLight::New
// 80227394 EIDirLight::Construct
// 802273B4 EIDirLight::Destruct
// 80227448 EIDirLight::GetTypeInfoStatic
// 80227454 EIDirLight::GetReadVersion
SIMMY_STORABLE_IMPL(EIDirLight, "EIDirLight", 0, EILight)
// 80226F1C EIAmbLight::GetTypeInfo
// 80226F28 EIAmbLight::GetTypeName
// 80226F34 EIAmbLight::GetTypeKey
// 80226F40 EIAmbLight::GetTypeVersion
// 80226EDC EIAmbLight::SafeDelete
// 80226E3C EIAmbLight::New
// 80226E80 EIAmbLight::Construct
// 80226EB8 EIAmbLight::Destruct
// 80226F4C EIAmbLight::GetTypeInfoStatic
// 80226F58 EIAmbLight::GetReadVersion
SIMMY_STORABLE_IMPL(EIAmbLight, "EIAmbLight", 0, EILight)

// 80227984
EILight::EILight() { m_flags |= kLight; }

// 80227C5C
void EILight::Init() { Setup(); }

// 80227D64
// The intensity, per channel when an intensity scale is in use.
EVec3 EILight::GetScaledIntensity() {
  if (m_useIntensityScale &&
      (m_intensityScale.x != 1.0f || m_intensityScale.y != 1.0f || m_intensityScale.z != 1.0f))
    return EVec3{m_intensity * m_intensityScale.x, m_intensity * m_intensityScale.y, m_intensity * m_intensityScale.z};
  return EVec3{m_intensity, m_intensity, m_intensity};
}

// 802280C4
void EILight::LightingParameters(EVec3&, float&, EVec3&, EVec3&) {}
// 802280CC
void EILight::Setup() {}
// 802280D0
int EILight::GetLightType() { return kNone; }

namespace {
// Lights shine everywhere: their bounds are infinite.
void SetInfiniteBounds(EInstance* light) {
  float b[6] = {-FLT_MAX, -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
  light->SetBounds(*reinterpret_cast<EBound3*>(b));
}
}  // namespace

// 802270AC
EIDirLight::EIDirLight() {}

// 80227190
// The direction, no range or falloff, colour times intensity.
void EIDirLight::LightingParameters(EVec3& dir, float& range, EVec3& falloff, EVec3& color) {
  dir = m_dir;
  color = EVec3{m_color.x * m_intensity, m_color.y * m_intensity, m_color.z * m_intensity};
  range = 0.0f;
  falloff = EVec3{};
}

// 80227238
void EIDirLight::Setup() { SetInfiniteBounds(this); }
// 802274D4
int EIDirLight::GetLightType() { return kDirectional; }

// 80226C58
// No direction, range or falloff; colour times intensity.
void EIAmbLight::LightingParameters(EVec3& dir, float& range, EVec3& falloff, EVec3& color) {
  dir = EVec3{};
  range = 0.0f;
  falloff = EVec3{};
  color = EVec3{m_color.x * m_intensity, m_color.y * m_intensity, m_color.z * m_intensity};
}

// 80226D08
void EIAmbLight::Setup() { SetInfiniteBounds(this); }
// 80226FD8
int EIAmbLight::GetLightType() { return kAmbient; }
