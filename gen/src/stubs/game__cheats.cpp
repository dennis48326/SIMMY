// GENERATED stubs for unported functions of game/cheats.cpp. Do not edit.
#include "simmy/types_gen.hpp"

ECheats::ECheats() { SIMMY_STUB("ECheats::ECheats(void)", 0x8001ECAC); }
ECheats::~ECheats() { SIMMY_STUB("ECheats::~ECheats(void)", 0x8001ECF4); }
void ECheats::Init(EGlobal &) { SIMMY_STUB("ECheats::Init(EGlobal &)", 0x8001ED7C); }
void ECheats::Reset() { SIMMY_STUB("ECheats::Reset(void)", 0x80020C0C); }
void ECheats::EmptyLookupList() { SIMMY_STUB("ECheats::EmptyLookupList(void)", 0x80020C40); }
void ECheats::ReadCheatsFile() { SIMMY_STUB("ECheats::ReadCheatsFile(void)", 0x80020CB0); }
void ECheats::SetGodMode(bool) { SIMMY_STUB("ECheats::SetGodMode(bool)", 0x80020CC4); }
void ECheats::WriteCheatsFile() { SIMMY_STUB("ECheats::WriteCheatsFile(void)", 0x80020D08); }
void ECheats::Update() { SIMMY_STUB("ECheats::Update(void)", 0x80020D0C); }
void ECheats::EnableCheats() { SIMMY_STUB("ECheats::EnableCheats(void)", 0x80020D10); }
void ECheats::DisableCheats() { SIMMY_STUB("ECheats::DisableCheats(void)", 0x80020F08); }
ECheatLookup::ECheatLookup() { SIMMY_STUB("ECheatLookup::ECheatLookup(void)", 0x800210E4); }
ECheatLookup::ECheatLookup(int, char *, CheatType, void *, char *, bool, bool) { SIMMY_STUB("ECheatLookup::ECheatLookup(int, char *, CheatType, void *, char *, bool, bool)", 0x80021124); }
RET_UNKNOWN ECheatLookup::WriteCheat(EFile *, int &) { SIMMY_STUB("ECheatLookup::WriteCheat(EFile *, int &)", 0x800211C8); return {}; }
void ECheatLookup::hash() const { SIMMY_STUB("ECheatLookup::hash(void) const", 0x80021420); }
void ECheatLookup::hash(char *) { SIMMY_STUB("ECheatLookup::hash(char *)", 0x80021444); }
bool ECheatLookup::compare(char *) const { SIMMY_STUB("ECheatLookup::compare(char *) const", 0x80021464); return {}; }
void ECheatDMI::GetDescription(char *, int) { SIMMY_STUB("ECheatDMI::GetDescription(char *, int)", 0x80021498); }
void ECheatDMI::GetValue(char *) { SIMMY_STUB("ECheatDMI::GetValue(char *)", 0x800214E0); }
void ECheatDMI::ButtonPress(EDebugMenuButton) { SIMMY_STUB("ECheatDMI::ButtonPress(EDebugMenuButton)", 0x80021600); }
void ECheatDMI::ButtonPress(EDebugMenuButton, float) { SIMMY_STUB("ECheatDMI::ButtonPress(EDebugMenuButton, float)", 0x800217B8); }
ECheatDMI::ECheatDMI(ECheatLookup *) { SIMMY_STUB("ECheatDMI::ECheatDMI(ECheatLookup *)", 0x800218EC); }
RET_UNKNOWN ECheatDMI::GetType() { SIMMY_STUB("ECheatDMI::GetType(void)", 0x80021930); return {}; }
RET_UNKNOWN ECheatDMI::GetVariable() { SIMMY_STUB("ECheatDMI::GetVariable(void)", 0x8002193C); return {}; }
