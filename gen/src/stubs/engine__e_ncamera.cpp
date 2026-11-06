// GENERATED stubs for unported functions of engine/e_ncamera.cpp. Do not edit.
#include "simmy/types_gen.hpp"

ENCamera::ENCamera() { SIMMY_STUB("ENCamera::ENCamera(void)", 0x8035CA0C); }
ENCamera::~ENCamera() { SIMMY_STUB("ENCamera::~ENCamera(void)", 0x8035CA60); }
void ENCamera::Write(EStream &) { SIMMY_STUB("ENCamera::Write(EStream &)", 0x8035CAB4); }
void ENCamera::Read(EStream &) { SIMMY_STUB("ENCamera::Read(EStream &)", 0x8035CC88); }
RET_UNKNOWN ENCamera::Read(unsigned char *) { SIMMY_STUB("ENCamera::Read(unsigned char *)", 0x8035CE84); return {}; }
RET_UNKNOWN operator>>(EStream &, ENCamera &) { SIMMY_STUB("operator>>(EStream &, ENCamera &)", 0x8035D074); return {}; }
void ENCamera::SetCameraPoint(int, ENCameraPoint) { SIMMY_STUB("ENCamera::SetCameraPoint(int, ENCameraPoint)", 0x8035D0C0); }
void ENCamera::SetName(char *) { SIMMY_STUB("ENCamera::SetName(char *)", 0x8035D1E8); }
void ENCamera::SetID(int) { SIMMY_STUB("ENCamera::SetID(int)", 0x8035D20C); }
void ENCamera::SetKeys(int) { SIMMY_STUB("ENCamera::SetKeys(int)", 0x8035D214); }
void ENCamera::GetName() { SIMMY_STUB("ENCamera::GetName(void)", 0x8035D248); }
RET_UNKNOWN ENCamera::GetID() { SIMMY_STUB("ENCamera::GetID(void)", 0x8035D24C); return {}; }
RET_UNKNOWN ENCamera::GetKeys() { SIMMY_STUB("ENCamera::GetKeys(void)", 0x8035D254); return {}; }
RET_UNKNOWN ENCamera::GetCameraPoint(int) { SIMMY_STUB("ENCamera::GetCameraPoint(int)", 0x8035D25C); return {}; }
