// GENERATED stubs for unported functions of game/e_ilight.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN operator>>(EStream &, EILight *&) { SIMMY_STUB("operator>>(EStream &, EILight *&)", 0x80227904); return {}; }
RET_UNKNOWN operator>>(EFile &, EILight *&) { SIMMY_STUB("operator>>(EFile &, EILight *&)", 0x80227944); return {}; }
void EILight::Write(EStream &) { SIMMY_STUB("EILight::Write(EStream &)", 0x80227A2C); }
void EILight::Read(EStream &) { SIMMY_STUB("EILight::Read(EStream &)", 0x80227B40); }
void EILight::GetScaledIntColor(float, unsigned int *) { SIMMY_STUB("EILight::GetScaledIntColor(float, unsigned int *)", 0x80227C90); }
RET_UNKNOWN EILight::TestOverlap(EInstance *) { SIMMY_STUB("EILight::TestOverlap(EInstance *)", 0x80227DE0); return {}; }
void EILight::RegisterType(unsigned short) { SIMMY_STUB("EILight::RegisterType(unsigned short)", 0x80228050); }
void EILight::CreateCopy() const { SIMMY_STUB("EILight::CreateCopy(void) const", 0x802280A4); }
void EILight::CalcFullIntensityLightOnPoint(EVec3 &, float &) { SIMMY_STUB("EILight::CalcFullIntensityLightOnPoint(EVec3 &, float &)", 0x802280C8); }
void EILight::SetIntensityScale(EVec3 &) { SIMMY_STUB("EILight::SetIntensityScale(EVec3 &)", 0x802280D8); }
void EILight::UseScaleIntensity(bool) { SIMMY_STUB("EILight::UseScaleIntensity(bool)", 0x802280F8); }
void EILight::GetPosition(EVec3 &) { SIMMY_STUB("EILight::GetPosition(EVec3 &)", 0x80228100); }
void EILight::GetIntensityScale() { SIMMY_STUB("EILight::GetIntensityScale(void)", 0x8022813C); }
void EILight::SetIntensity(float) { SIMMY_STUB("EILight::SetIntensity(float)", 0x80228160); }
double EILight::GetIntensity() const { SIMMY_STUB("EILight::GetIntensity(void) const", 0x80228168); return {}; }
void EILight::SetColor(EVec3 &) { SIMMY_STUB("EILight::SetColor(EVec3 &)", 0x80228170); }
RET_UNKNOWN EILight::GetColor() const { SIMMY_STUB("EILight::GetColor(void) const", 0x80228190); return {}; }
void EILight::Enable(bool) { SIMMY_STUB("EILight::Enable(bool)", 0x80228198); }
RET_UNKNOWN EILight::IsEnabled() { SIMMY_STUB("EILight::IsEnabled(void)", 0x802281A0); return {}; }
double EILight::GetFalloffEnd() { SIMMY_STUB("EILight::GetFalloffEnd(void)", 0x802281A8); return {}; }
void EILight::SetHaveLightsChanged(bool) { SIMMY_STUB("EILight::SetHaveLightsChanged(bool)", 0x802281B4); }
RET_UNKNOWN EILight::HaveLightsChanged() { SIMMY_STUB("EILight::HaveLightsChanged(void)", 0x802281BC); return {}; }
