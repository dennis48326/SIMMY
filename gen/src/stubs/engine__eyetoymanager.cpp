// GENERATED stubs for unported functions of engine/eyetoymanager.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN EyeToyManager::Open(int frameRate, int enable, bool unknown) { SIMMY_STUB("EyeToyManager::Open(int, int, bool)", 0x80328C08); return {}; }
void EyeToyManager::Close() { SIMMY_STUB("EyeToyManager::Close(void)", 0x80328C50); }
RET_UNKNOWN EyeToyManager::IsPhysicallyConnected(int) { SIMMY_STUB("EyeToyManager::IsPhysicallyConnected(int)", 0x80328CD4); return {}; }
RET_UNKNOWN EyeToyManager::GetPhysicalConnectionCount() { SIMMY_STUB("EyeToyManager::GetPhysicalConnectionCount(void)", 0x80328CDC); return {}; }
RET_UNKNOWN EyeToyManager::OpenInstance(int, void (*)(char *, int), EyeToyDefs__Resolution, bool, void (*)(int)) { SIMMY_STUB("EyeToyManager::OpenInstance(int, void (*)(char *, int), EyeToyDefs::Resolution, bool, void (*)(int))", 0x80328CE4); return {}; }
RET_UNKNOWN EyeToyManager::CloseInstance(int) { SIMMY_STUB("EyeToyManager::CloseInstance(int)", 0x80328CFC); return {}; }
void EyeToyManager::UpdateAllInstances(float dt) { SIMMY_STUB("EyeToyManager::UpdateAllInstances(float)", 0x80328D34); }
void EyeToyManager::UpdateInstance(int, float) { SIMMY_STUB("EyeToyManager::UpdateInstance(int, float)", 0x80328D60); }
void EyeToyManager::LockOutputBuffer(int) { SIMMY_STUB("EyeToyManager::LockOutputBuffer(int)", 0x80328D94); }
void EyeToyManager::ReleaseOutputBuffer(int) { SIMMY_STUB("EyeToyManager::ReleaseOutputBuffer(int)", 0x80328DBC); }
RET_UNKNOWN EyeToyManager::StartStream(int) { SIMMY_STUB("EyeToyManager::StartStream(int)", 0x80328DE4); return {}; }
RET_UNKNOWN EyeToyManager::StopStream(int) { SIMMY_STUB("EyeToyManager::StopStream(int)", 0x80328E1C); return {}; }
void EyeToyManager::ResetStream(int, EyeToyDefs__Resolution) { SIMMY_STUB("EyeToyManager::ResetStream(int, EyeToyDefs::Resolution)", 0x80328E54); }
RET_UNKNOWN EyeToyManager::PauseStreams() { SIMMY_STUB("EyeToyManager::PauseStreams(void)", 0x80328E7C); return {}; }
RET_UNKNOWN EyeToyManager::ResumeStreams() { SIMMY_STUB("EyeToyManager::ResumeStreams(void)", 0x80328E90); return {}; }
void EyeToyManager::GetDimensions(int, int *, int *) { SIMMY_STUB("EyeToyManager::GetDimensions(int, int *, int *)", 0x80328EA0); }
void EyeToyManager::SetAttributes(int, EyeToyDefs__AttribFlags, EyeToyDefs__Attributes *) { SIMMY_STUB("EyeToyManager::SetAttributes(int, EyeToyDefs::AttribFlags, EyeToyDefs::Attributes *)", 0x80328EE4); }
RET_UNKNOWN EyeToyManager::GetAttributes(int) { SIMMY_STUB("EyeToyManager::GetAttributes(int)", 0x80328F0C); return {}; }
RET_UNKNOWN EyeToyManager::ConvertHandleToInstancePtr(int) { SIMMY_STUB("EyeToyManager::ConvertHandleToInstancePtr(int)", 0x80328F44); return {}; }
void EyeToyManager::ConvertInstanceIndexToHandle(int) { SIMMY_STUB("EyeToyManager::ConvertInstanceIndexToHandle(int)", 0x80328F50); }
void EyeToyManager::NormalizeTexture(EyeToyManager__NormalizationParams *) { SIMMY_STUB("EyeToyManager::NormalizeTexture(EyeToyManager::NormalizationParams *)", 0x80328F54); }
void EyeToyManager::CleanUpBufferBetaHack() { SIMMY_STUB("EyeToyManager::CleanUpBufferBetaHack(void)", 0x80328F58); }
RET_UNKNOWN EyeToyManager::IsInstanceDisconnected(int) { SIMMY_STUB("EyeToyManager::IsInstanceDisconnected(int)", 0x80328F5C); return {}; }
