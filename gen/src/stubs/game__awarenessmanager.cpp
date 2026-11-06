// GENERATED stubs for unported functions of game/awarenessmanager.cpp. Do not edit.
#include "simmy/types_gen.hpp"

AwarenessManager::AwarenessManager() { SIMMY_STUB("AwarenessManager::AwarenessManager(void)", 0x80011EEC); }
void AwarenessManager::Init(SAnimator2 *, EAnimController *) { SIMMY_STUB("AwarenessManager::Init(SAnimator2 *, EAnimController *)", 0x80011F4C); }
void AwarenessManager::Reset() { SIMMY_STUB("AwarenessManager::Reset(void)", 0x80012034); }
AwarenessManager::~AwarenessManager() { SIMMY_STUB("AwarenessManager::~AwarenessManager(void)", 0x800120EC); }
void AwarenessManager::AwarenessCheck(unsigned int) { SIMMY_STUB("AwarenessManager::AwarenessCheck(unsigned int)", 0x80012140); }
void AwarenessManager::handleAwarenessAnimations(unsigned int) { SIMMY_STUB("AwarenessManager::handleAwarenessAnimations(unsigned int)", 0x8001220C); }
void AwarenessManager::SetIsRaining(bool) { SIMMY_STUB("AwarenessManager::SetIsRaining(bool)", 0x80012250); }
RET_UNKNOWN AwarenessManager::AwarenessAutoRunCheck(float) { SIMMY_STUB("AwarenessManager::AwarenessAutoRunCheck(float)", 0x80012258); return {}; }
void AwarenessManager::SetPlayerControl(bool) { SIMMY_STUB("AwarenessManager::SetPlayerControl(bool)", 0x800122B0); }
RET_UNKNOWN AwarenessManager::GetFirstNodeRotation(float, float) { SIMMY_STUB("AwarenessManager::GetFirstNodeRotation(float, float)", 0x800122D8); return {}; }
RET_UNKNOWN AwarenessManager::GetSecondNodeRotation(float) { SIMMY_STUB("AwarenessManager::GetSecondNodeRotation(float)", 0x800123D4); return {}; }
RET_UNKNOWN AwarenessManager::AwarenessAnimateHeadLeadsMotion(unsigned int, EACTrack *, EMat4 &, ERCharacter *, EACNodeState *) { SIMMY_STUB("AwarenessManager::AwarenessAnimateHeadLeadsMotion(unsigned int, EACTrack *, EMat4 &, ERCharacter *, EACNodeState *)", 0x80012494); return {}; }
RET_UNKNOWN AwarenessManager::AwarenessAnimateTorsoLeadsMotion(unsigned int, EACTrack *, EMat4 &, ERCharacter *, EACNodeState *) { SIMMY_STUB("AwarenessManager::AwarenessAnimateTorsoLeadsMotion(unsigned int, EACTrack *, EMat4 &, ERCharacter *, EACNodeState *)", 0x80012688); return {}; }
RET_UNKNOWN AwarenessManager::SetAwarenessTargetAngle(cXObject *) { SIMMY_STUB("AwarenessManager::SetAwarenessTargetAngle(cXObject *)", 0x8001287C); return {}; }
void AwarenessManager::handlePassiveInfluenceAnimation(unsigned int) { SIMMY_STUB("AwarenessManager::handlePassiveInfluenceAnimation(unsigned int)", 0x80012940); }
void AwarenessManager::handleMemoryAwarenessAnimation(unsigned int) { SIMMY_STUB("AwarenessManager::handleMemoryAwarenessAnimation(unsigned int)", 0x80012A74); }
void AwarenessManager::handleAwarenessTurningAnimation() { SIMMY_STUB("AwarenessManager::handleAwarenessTurningAnimation(void)", 0x80012ADC); }
RET_UNKNOWN AwarenessManager::SetAwareOfObject(cXObject *) { SIMMY_STUB("AwarenessManager::SetAwareOfObject(cXObject *)", 0x80012B8C); return {}; }
RET_UNKNOWN AwarenessManager::SetAwareOfObjectKilled(cXObject *) { SIMMY_STUB("AwarenessManager::SetAwareOfObjectKilled(cXObject *)", 0x80012BD0); return {}; }
RET_UNKNOWN AwarenessManager::ClearAwareOfObject(cXObject *) { SIMMY_STUB("AwarenessManager::ClearAwareOfObject(cXObject *)", 0x80012C04); return {}; }
RET_UNKNOWN AwarenessManager::IsTimeToCheckMemoryAwareness(unsigned int) { SIMMY_STUB("AwarenessManager::IsTimeToCheckMemoryAwareness(unsigned int)", 0x80012C4C); return {}; }
RET_UNKNOWN AwarenessManager::CanSeePlayer() { SIMMY_STUB("AwarenessManager::CanSeePlayer(void)", 0x80012D9C); return {}; }
void AwarenessManager::GetActiveMemoryCategory(cXObject *, int &, int &) { SIMMY_STUB("AwarenessManager::GetActiveMemoryCategory(cXObject *, int &, int &)", 0x80012F50); }
RET_UNKNOWN AwarenessManager::StarMemoryAwarenessSprite() { SIMMY_STUB("AwarenessManager::StarMemoryAwarenessSprite(void)", 0x8001303C); return {}; }
bool AwarenessManager::SetPendingMemoryAwarenessAnim(AwarenessManager__eAwarenessAction, int) { SIMMY_STUB("AwarenessManager::SetPendingMemoryAwarenessAnim(AwarenessManager::eAwarenessAction, int)", 0x800131C4); return {}; }
void AwarenessManager::StartMemoryAwarenessAnimation(AwarenessManager__eAwarenessAction, int) { SIMMY_STUB("AwarenessManager::StartMemoryAwarenessAnimation(AwarenessManager::eAwarenessAction, int)", 0x80013228); }
void AwarenessManager::SetMemoryAwarenessActionToTry(cXObject *) { SIMMY_STUB("AwarenessManager::SetMemoryAwarenessActionToTry(cXObject *)", 0x800133B8); }
void AwarenessManager::MemoryAwarenessClearAction() { SIMMY_STUB("AwarenessManager::MemoryAwarenessClearAction(void)", 0x800134F8); }
void AwarenessManager::UpdateAwarenessAngle(unsigned int) { SIMMY_STUB("AwarenessManager::UpdateAwarenessAngle(unsigned int)", 0x800137C0); }
void AwarenessManager::MemoryAwarenessCheck(unsigned int) { SIMMY_STUB("AwarenessManager::MemoryAwarenessCheck(unsigned int)", 0x80013960); }
void AwarenessManager::StopPassiveInfluenceAnimation() { SIMMY_STUB("AwarenessManager::StopPassiveInfluenceAnimation(void)", 0x80013AAC); }
RET_UNKNOWN AwarenessManager::StartPassiveInfluenceAnimation(AnimRef *) { SIMMY_STUB("AwarenessManager::StartPassiveInfluenceAnimation(AnimRef *)", 0x80013BA4); return {}; }
void AwarenessManager::RainAwarenessCheck() { SIMMY_STUB("AwarenessManager::RainAwarenessCheck(void)", 0x80013CEC); }
void AwarenessManager::PassiveInfluenceAwarenessCheck(unsigned int) { SIMMY_STUB("AwarenessManager::PassiveInfluenceAwarenessCheck(unsigned int)", 0x80013D4C); }
RET_UNKNOWN AwarenessManager::GetPlayerObject(int) { SIMMY_STUB("AwarenessManager::GetPlayerObject(int)", 0x80014028); return {}; }
double AwarenessManager::GetDeltaAngleToTargetObject(cXObject *) { SIMMY_STUB("AwarenessManager::GetDeltaAngleToTargetObject(cXObject *)", 0x80014050); return {}; }
RET_UNKNOWN AwarenessManager::GetAwarenessAction() { SIMMY_STUB("AwarenessManager::GetAwarenessAction(void)", 0x80014248); return {}; }
RET_UNKNOWN AwarenessManager::GetAwareOfObject() { SIMMY_STUB("AwarenessManager::GetAwareOfObject(void)", 0x80014250); return {}; }
RET_UNKNOWN AwarenessManager::IsMemoryAwarenessAnimPending() { SIMMY_STUB("AwarenessManager::IsMemoryAwarenessAnimPending(void)", 0x80014258); return {}; }
RET_UNKNOWN AwarenessManager::IsAutoCarryActive() { SIMMY_STUB("AwarenessManager::IsAutoCarryActive(void)", 0x80014264); return {}; }
void AwarenessManager::SetTimeMultiplier(float) { SIMMY_STUB("AwarenessManager::SetTimeMultiplier(float)", 0x80014270); }
void AwarenessManager::SetPauseMultiplier(float) { SIMMY_STUB("AwarenessManager::SetPauseMultiplier(float)", 0x80014278); }
RET_UNKNOWN AwarenessManager::GetStateFlags() { SIMMY_STUB("AwarenessManager::GetStateFlags(void)", 0x80014280); return {}; }
RET_UNKNOWN AwarenessManager::ShouldAwarenessBeActive(unsigned int) { SIMMY_STUB("AwarenessManager::ShouldAwarenessBeActive(unsigned int)", 0x80014288); return {}; }
RET_UNKNOWN AwarenessManager::IsMemoryAwarenessAnimDone() { SIMMY_STUB("AwarenessManager::IsMemoryAwarenessAnimDone(void)", 0x80014378); return {}; }
void AwarenessManager::getPassiveInfluenceSkillID(signed char, AnimRef *&) { SIMMY_STUB("AwarenessManager::getPassiveInfluenceSkillID(signed char, AnimRef *&)", 0x800143E0); }
void AwarenessManager::getAwarenessSkillID(int, int, AnimRef *&) { SIMMY_STUB("AwarenessManager::getAwarenessSkillID(int, int, AnimRef *&)", 0x80014450); }
RET_UNKNOWN AwarenessManager::shouldAutoCarry() { SIMMY_STUB("AwarenessManager::shouldAutoCarry(void)", 0x800145B4); return {}; }
void AwarenessManager::startAutoCarry() { SIMMY_STUB("AwarenessManager::startAutoCarry(void)", 0x8001471C); }
void AwarenessManager::endAutoCarry() { SIMMY_STUB("AwarenessManager::endAutoCarry(void)", 0x80014A3C); }
