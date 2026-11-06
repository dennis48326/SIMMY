// GENERATED stubs for unported functions of base/e_ndummy.cpp. Do not edit.
#include "simmy/types_gen.hpp"

ENDummy::ENDummy() { SIMMY_STUB("ENDummy::ENDummy(void)", 0x802CBD84); }
ENDummy::~ENDummy() { SIMMY_STUB("ENDummy::~ENDummy(void)", 0x802CBDD8); }
void ENDummy::Write(EStream &) { SIMMY_STUB("ENDummy::Write(EStream &)", 0x802CBE2C); }
void ENDummy::Read(EStream &) { SIMMY_STUB("ENDummy::Read(EStream &)", 0x802CBF60); }
RET_UNKNOWN ENDummy::Read(unsigned char *) { SIMMY_STUB("ENDummy::Read(unsigned char *)", 0x802CC090); return {}; }
RET_UNKNOWN operator>>(EStream &, ENDummy &) { SIMMY_STUB("operator>>(EStream &, ENDummy &)", 0x802CC1E4); return {}; }
void ENDummy::SetDummyPoint(int, ENDummyPoint) { SIMMY_STUB("ENDummy::SetDummyPoint(int, ENDummyPoint)", 0x802CC230); }
void ENDummy::GetDummyPointOrient(int, EMat4 &, EMat4 &) { SIMMY_STUB("ENDummy::GetDummyPointOrient(int, EMat4 &, EMat4 &)", 0x802CC338); }
void ENDummy::SetName(char *) { SIMMY_STUB("ENDummy::SetName(char *)", 0x802CC450); }
void ENDummy::SetID(int) { SIMMY_STUB("ENDummy::SetID(int)", 0x802CC474); }
void ENDummy::SetKeys(int) { SIMMY_STUB("ENDummy::SetKeys(int)", 0x802CC47C); }
void ENDummy::GetName() { SIMMY_STUB("ENDummy::GetName(void)", 0x802CC4B0); }
RET_UNKNOWN ENDummy::GetID() { SIMMY_STUB("ENDummy::GetID(void)", 0x802CC4B4); return {}; }
RET_UNKNOWN ENDummy::GetKeys() { SIMMY_STUB("ENDummy::GetKeys(void)", 0x802CC4BC); return {}; }
RET_UNKNOWN ENDummy::GetDummyPoint(int) { SIMMY_STUB("ENDummy::GetDummyPoint(int)", 0x802CC4C4); return {}; }
