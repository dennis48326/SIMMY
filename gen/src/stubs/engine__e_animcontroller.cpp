// GENERATED stubs for unported functions of engine/e_animcontroller.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void EAnimController::ProcessEvents(TNodeList<EACEventRange *> &, bool) { SIMMY_STUB("EAnimController::ProcessEvents(TNodeList<EACEventRange *> &, bool)", 0x802E8864); }
double EAnimController::AddEventRange(TNodeList<EACEventRange *> &, EACTrack *, float, float) { SIMMY_STUB("EAnimController::AddEventRange(TNodeList<EACEventRange *> &, EACTrack *, float, float)", 0x802E89EC); return {}; }
RET_UNKNOWN EAnimController::GetSlavePos(EACTrack *, EACTrack *) { SIMMY_STUB("EAnimController::GetSlavePos(EACTrack *, EACTrack *)", 0x802E9410); return {}; }
void EAnimController::GetAnimDistance(eTrackFlags, EVec3 &) { SIMMY_STUB("EAnimController::GetAnimDistance(eTrackFlags, EVec3 &)", 0x802E94A8); }
RET_UNKNOWN EAnimController::GetAnimTrans(eTrackFlags, EVec3 &) { SIMMY_STUB("EAnimController::GetAnimTrans(eTrackFlags, EVec3 &)", 0x802E94D8); return {}; }
void EAnimController::GetAnimTranslate(eTrackFlags, EVec3 &, EVec3 &) { SIMMY_STUB("EAnimController::GetAnimTranslate(eTrackFlags, EVec3 &, EVec3 &)", 0x802E96A8); }
double EAnimController::GetAnimTime(eTrackFlags) { SIMMY_STUB("EAnimController::GetAnimTime(eTrackFlags)", 0x802E9960); return {}; }
double EAnimController::GetAnimVelocity(eTrackFlags, EVec3 &) { SIMMY_STUB("EAnimController::GetAnimVelocity(eTrackFlags, EVec3 &)", 0x802E9B04); return {}; }
void EAnimController::PrintTracks() { SIMMY_STUB("EAnimController::PrintTracks(void)", 0x802E9C0C); }
void EAnimController::ComputeInverse(EMat4 &, int) { SIMMY_STUB("EAnimController::ComputeInverse(EMat4 &, int)", 0x802EA1DC); }
void EAnimController::PrepareForCollision(EMat4 &) { SIMMY_STUB("EAnimController::PrepareForCollision(EMat4 &)", 0x802EA28C); }
void EAnimController::PostMultNodeAndDescendents(int, EMat4 &) { SIMMY_STUB("EAnimController::PostMultNodeAndDescendents(int, EMat4 &)", 0x802EA2CC); }
void EAnimController::PreMultNodeAndDescendents(int, EMat4 &) { SIMMY_STUB("EAnimController::PreMultNodeAndDescendents(int, EMat4 &)", 0x802EA404); }
void EAnimController::CalcTightBoundBox(EMat4 &, EBound3 &, bool *) { SIMMY_STUB("EAnimController::CalcTightBoundBox(EMat4 &, EBound3 &, bool *)", 0x802EA53C); }
void EAnimController::Layer(float, EACNodeState &, EACNodeState &) { SIMMY_STUB("EAnimController::Layer(float, EACNodeState &, EACNodeState &)", 0x802EB340); }
void EAnimController::CallbackIntermediateComputeMatrices(EMat4 &) { SIMMY_STUB("EAnimController::CallbackIntermediateComputeMatrices(EMat4 &)", 0x802EB53C); }
void EAnimController::StopTrack(eTrackFlags) { SIMMY_STUB("EAnimController::StopTrack(eTrackFlags)", 0x802EC5A0); }
void EAnimController::TransferTrack(eTrackFlags, eTrackFlags) { SIMMY_STUB("EAnimController::TransferTrack(eTrackFlags, eTrackFlags)", 0x802EC6E8); }
void EAnimController::SetStaticBlendTrackAnim(eTrackFlags, unsigned int, float, int, float, EACTrack *) { SIMMY_STUB("EAnimController::SetStaticBlendTrackAnim(eTrackFlags, unsigned int, float, int, float, EACTrack *)", 0x802ECAF8); }
RET_UNKNOWN EAnimController::SetTrackAnim(eTrackFlags, char *) { SIMMY_STUB("EAnimController::SetTrackAnim(eTrackFlags, char *)", 0x802ED7EC); return {}; }
RET_UNKNOWN EAnimController::GetTrackAnimName(eTrackFlags) { SIMMY_STUB("EAnimController::GetTrackAnimName(eTrackFlags)", 0x802ED840); return {}; }
void EAnimController::SetAllTrackIntensities(float) { SIMMY_STUB("EAnimController::SetAllTrackIntensities(float)", 0x802EDBF0); }
double EAnimController::GetTrackBlendTarget(eTrackFlags) { SIMMY_STUB("EAnimController::GetTrackBlendTarget(eTrackFlags)", 0x802EDC70); return {}; }
void EAnimController::BlendAllOutgoingTracks(float, float, bool) { SIMMY_STUB("EAnimController::BlendAllOutgoingTracks(float, float, bool)", 0x802EE06C); }
void EAnimController::SetTrackBlend(eTrackFlags, float) { SIMMY_STUB("EAnimController::SetTrackBlend(eTrackFlags, float)", 0x802EE1DC); }
void EAnimController::SetTrackBlendHermiteSafe(EACTrack *, float, float, float, float) { SIMMY_STUB("EAnimController::SetTrackBlendHermiteSafe(EACTrack *, float, float, float, float)", 0x802EE4D8); }
void EAnimController::SetTrackBlendHermiteSafe(eTrackFlags, float, float, float, float) { SIMMY_STUB("EAnimController::SetTrackBlendHermiteSafe(eTrackFlags, float, float, float, float)", 0x802EE4F8); }
RET_UNKNOWN EAnimController::SetTrackBlendHermite(eTrackFlags, float, float, float, float) { SIMMY_STUB("EAnimController::SetTrackBlendHermite(eTrackFlags, float, float, float, float)", 0x802EE6F8); return {}; }
void EAnimController::SetTrackPhaseLock(EACTrack *, EACTrack *, float) { SIMMY_STUB("EAnimController::SetTrackPhaseLock(EACTrack *, EACTrack *, float)", 0x802EEA60); }
void EAnimController::SetProceduralTrack(eTrackFlags, void (*)(unsigned int, EACTrack *, ERCharacter *, EVec3 &, EVec3 &), void (*)(unsigned int, EACTrack *, EMat4 &, ERCharacter *, EACNodeState *), unsigned int) { SIMMY_STUB("EAnimController::SetProceduralTrack(eTrackFlags, void (*)(unsigned int, EACTrack *, ERCharacter *, EVec3 &, EVec3 &), void (*)(unsigned int, EACTrack *, EMat4 &, ERCharacter *, EACNodeState *), unsigned int)", 0x802EEAFC); }
void EAnimController::SetTrackPhase(eTrackFlags, float) { SIMMY_STUB("EAnimController::SetTrackPhase(eTrackFlags, float)", 0x802EEEC0); }
void EAnimController::SetTrackBlendFactors(eTrackFlags, float *) { SIMMY_STUB("EAnimController::SetTrackBlendFactors(eTrackFlags, float *)", 0x802EF230); }
RET_UNKNOWN EAnimController::GetTrackFrameCount(eTrackFlags) { SIMMY_STUB("EAnimController::GetTrackFrameCount(eTrackFlags)", 0x802EF374); return {}; }
double EAnimController::GetTrackFrame(eTrackFlags) { SIMMY_STUB("EAnimController::GetTrackFrame(eTrackFlags)", 0x802EF4DC); return {}; }
void EAnimController::ClearTrackAnimComplete(eTrackFlags) { SIMMY_STUB("EAnimController::ClearTrackAnimComplete(eTrackFlags)", 0x802EFAC0); }
RET_UNKNOWN EAnimController::VisibilityTest(EMat4 &) { SIMMY_STUB("EAnimController::VisibilityTest(EMat4 &)", 0x802EFF38); return {}; }
void EAnimController::CalcVisibilitySphere(EMat4 &, EBoundSphere &) { SIMMY_STUB("EAnimController::CalcVisibilitySphere(EMat4 &, EBoundSphere &)", 0x802EFFCC); }
void EAnimController::Draw(ERC *, ERModel *, EMat4 &) { SIMMY_STUB("EAnimController::Draw(ERC *, ERModel *, EMat4 &)", 0x802F03B4); }
RET_UNKNOWN EAnimController::CalcNodePos(int) { SIMMY_STUB("EAnimController::CalcNodePos(int)", 0x802F0428); return {}; }
RET_UNKNOWN EAnimController::GetNodeMatrix(int) { SIMMY_STUB("EAnimController::GetNodeMatrix(int)", 0x802F0678); return {}; }
void EAnimController::SetNodeIgnoreAnimatedVisibility(int, bool) { SIMMY_STUB("EAnimController::SetNodeIgnoreAnimatedVisibility(int, bool)", 0x802F0A30); }
void EAnimController::SetNodeVisible(int, bool) { SIMMY_STUB("EAnimController::SetNodeVisible(int, bool)", 0x802F0A8C); }
void EAnimController::CalcBillinearCoeff(float, float, float, float &, float &, float &, float &) { SIMMY_STUB("EAnimController::CalcBillinearCoeff(float, float, float, float &, float &, float &, float &)", 0x802F0CCC); }
