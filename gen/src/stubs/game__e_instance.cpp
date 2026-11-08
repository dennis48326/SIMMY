// GENERATED stubs for unported functions of game/e_instance.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN operator>>(EStream &, EInstance *&) { SIMMY_STUB("operator>>(EStream &, EInstance *&)", 0x80228210); return {}; }
RET_UNKNOWN operator>>(EFile &, EInstance *&) { SIMMY_STUB("operator>>(EFile &, EInstance *&)", 0x80228250); return {}; }
void EInstance::Write(EStream &) { SIMMY_STUB("EInstance::Write(EStream &)", 0x802283C8); }
void EInstance::Read(EStream &) { SIMMY_STUB("EInstance::Read(EStream &)", 0x80228468); }
void EInstance::GetBoundSphere(EBoundSphere &) { SIMMY_STUB("EInstance::GetBoundSphere(EBoundSphere &)", 0x802287F4); }
RET_UNKNOWN CompareIntensity(void *, void *) { SIMMY_STUB("CompareIntensity(void *, void *)", 0x80228EA8); return {}; }
void LightArray::Init() { SIMMY_STUB("LightArray::Init(void)", 0x80228EF4); }
void EInstance::RegisterType(unsigned short) { SIMMY_STUB("EInstance::RegisterType(unsigned short)", 0x802293E8); }
void EInstance::CreateCopy() const { SIMMY_STUB("EInstance::CreateCopy(void) const", 0x8022943C); }
void EInstance::GetOtherSide(short &, short &) { SIMMY_STUB("EInstance::GetOtherSide(short &, short &)", 0x80229574); }
void EInstance::SetPortalType(EPortalType) { SIMMY_STUB("EInstance::SetPortalType(EPortalType)", 0x802295C4); }
RET_UNKNOWN EInstance::GetPortalType() { SIMMY_STUB("EInstance::GetPortalType(void)", 0x802295CC); return {}; }
void EInstance::RegisterOtherSideFn(unsigned int, void (*)(unsigned int, bool, short &, short &)) { SIMMY_STUB("EInstance::RegisterOtherSideFn(unsigned int, void (*)(unsigned int, bool, short &, short &))", 0x802295D4); }
RET_UNKNOWN EInstance::GetThisSideData() { SIMMY_STUB("EInstance::GetThisSideData(void)", 0x802295E0); return {}; }
RET_UNKNOWN EInstance::GetThisSideFn() { SIMMY_STUB("EInstance::GetThisSideFn(void)", 0x802295E8); return {}; }
RET_UNKNOWN EInstance::GetOtherSideData() { SIMMY_STUB("EInstance::GetOtherSideData(void)", 0x802295F0); return {}; }
RET_UNKNOWN EInstance::GetOtherSideFn() { SIMMY_STUB("EInstance::GetOtherSideFn(void)", 0x802295F8); return {}; }
void EInstance::RegisterHiddenFn(unsigned int, bool (*)(unsigned int)) { SIMMY_STUB("EInstance::RegisterHiddenFn(unsigned int, bool (*)(unsigned int))", 0x8022960C); }
RET_UNKNOWN EInstance::IsHidden() { SIMMY_STUB("EInstance::IsHidden(void)", 0x80229618); return {}; }
void EInstance::SetShadowInterpolated(bool) { SIMMY_STUB("EInstance::SetShadowInterpolated(bool)", 0x80229654); }
RET_UNKNOWN EInstance::GetBounds() const { SIMMY_STUB("EInstance::GetBounds(void) const", 0x80229660); return {}; }
void EInstance::ReadInstanceData(EStream &, bool) { SIMMY_STUB("EInstance::ReadInstanceData(EStream &, bool)", 0x80229668); }
RET_UNKNOWN EInstance::CanReloadInstanceData() { SIMMY_STUB("EInstance::CanReloadInstanceData(void)", 0x8022966C); return {}; }
void EInstance::GetOwner() { SIMMY_STUB("EInstance::GetOwner(void)", 0x8022967C); }
RET_UNKNOWN EInstance::GetNumRoomID() { SIMMY_STUB("EInstance::GetNumRoomID(void)", 0x80229680); return {}; }
RET_UNKNOWN EInstance::GetRoomID(unsigned int) { SIMMY_STUB("EInstance::GetRoomID(unsigned int)", 0x80229688); return {}; }
void EInstance::GetVelocity(EVec3 &) { SIMMY_STUB("EInstance::GetVelocity(EVec3 &)", 0x80229690); }
void EInstance::GetPos(EVec3 &) { SIMMY_STUB("EInstance::GetPos(EVec3 &)", 0x802296A8); }
void EInstance::GetOrient(EMat4 &) { SIMMY_STUB("EInstance::GetOrient(EMat4 &)", 0x802296C0); }
RET_UNKNOWN EInstance::GetModifiableColor(unsigned int) { SIMMY_STUB("EInstance::GetModifiableColor(unsigned int)", 0x802296F0); return {}; }
RET_UNKNOWN EInstance::IsShadow() { SIMMY_STUB("EInstance::IsShadow(void)", 0x80229704); return {}; }
RET_UNKNOWN EInstance::GetShadowOwner() { SIMMY_STUB("EInstance::GetShadowOwner(void)", 0x8022971C); return {}; }
RET_UNKNOWN EInstance::IsWall() { SIMMY_STUB("EInstance::IsWall(void)", 0x80229724); return {}; }
void EInstance::ShouldInterestFade(bool) { SIMMY_STUB("EInstance::ShouldInterestFade(bool)", 0x8022972C); }
