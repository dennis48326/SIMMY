// GENERATED stubs for unported functions of game/hudtarget.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void CUCS2ToUTF8(unsigned short *, int, char *) { SIMMY_STUB("CUCS2ToUTF8(unsigned wchar_t *, int, char *)", 0x801858DC); }
RET_UNKNOWN GetTutorialLocks(int) { SIMMY_STUB("GetTutorialLocks(int)", 0x801858FC); return {}; }
RET_UNKNOWN HUDTarget::IsActionQueueAvailable(int) const { SIMMY_STUB("HUDTarget::IsActionQueueAvailable(int) const", 0x80188724); return {}; }
void HUDTarget::AddAction(int, Interaction *, BString2 &, unsigned int, ERShader *, unsigned int) { SIMMY_STUB("HUDTarget::AddAction(int, Interaction *, BString2 &, unsigned int, ERShader *, unsigned int)", 0x80188E60); }
RET_UNKNOWN HUDTarget::RemoveAction(int, Interaction *) { SIMMY_STUB("HUDTarget::RemoveAction(int, Interaction *)", 0x80188F70); return {}; }
void HUDTarget::RemoveAllActions(int) { SIMMY_STUB("HUDTarget::RemoveAllActions(int)", 0x80188FC8); }
void HUDTarget::DrawMotiveValues(ERC *, int) { SIMMY_STUB("HUDTarget::DrawMotiveValues(ERC *, int)", 0x801890C8); }
RET_UNKNOWN HUDTarget::RequestHUD(short, short, short) { SIMMY_STUB("HUDTarget::RequestHUD(short, short, short)", 0x801891C0); return {}; }
RET_UNKNOWN HUDTarget::AddHUDControl(short, short, short &) { SIMMY_STUB("HUDTarget::AddHUDControl(short, short, short &)", 0x801892A8); return {}; }
RET_UNKNOWN HUDTarget::RemoveHUDControl(short, short) { SIMMY_STUB("HUDTarget::RemoveHUDControl(short, short)", 0x801892D4); return {}; }
RET_UNKNOWN HUDTarget::ManageHUD(short, short, short) { SIMMY_STUB("HUDTarget::ManageHUD(short, short, short)", 0x801892FC); return {}; }
RET_UNKNOWN HUDTarget::SetHUDData(short, short) { SIMMY_STUB("HUDTarget::SetHUDData(short, short)", 0x80189360); return {}; }
RET_UNKNOWN HUDTarget::SetJobBarInputRange(int, int, int) { SIMMY_STUB("HUDTarget::SetJobBarInputRange(int, int, int)", 0x80189408); return {}; }
RET_UNKNOWN HUDTarget::ApplyMotiveCurveArray(unsigned int, short) { SIMMY_STUB("HUDTarget::ApplyMotiveCurveArray(unsigned int, short)", 0x80189444); return {}; }
RET_UNKNOWN HUDTarget::CalculateJobScore() { SIMMY_STUB("HUDTarget::CalculateJobScore(void)", 0x801895AC); return {}; }
RET_UNKNOWN HUDTarget::GetHUDData(short, short &) { SIMMY_STUB("HUDTarget::GetHUDData(short, short &)", 0x801897EC); return {}; }
void HUDTarget::AnimateHUDControl(short, HUDTarget__EControlAnimType) { SIMMY_STUB("HUDTarget::AnimateHUDControl(short, HUDTarget::EControlAnimType)", 0x80189864); }
void HUDTarget::GotoCAS(unsigned short, unsigned short, int, int) { SIMMY_STUB("HUDTarget::GotoCAS(unsigned short, unsigned short, int, int)", 0x801898E4); }
RET_UNKNOWN HUDTarget::GetJobBarCount() { SIMMY_STUB("HUDTarget::GetJobBarCount(void)", 0x80189A34); return {}; }
RET_UNKNOWN HUDTarget::GetActionQueueHUD(int) { SIMMY_STUB("HUDTarget::GetActionQueueHUD(int)", 0x80189A50); return {}; }
RET_UNKNOWN HUDTarget::GetHUDMode() const { SIMMY_STUB("HUDTarget::GetHUDMode(void) const", 0x80189A60); return {}; }
bool HUDTarget::HUDHideComplete() const { SIMMY_STUB("HUDTarget::HUDHideComplete(void) const", 0x80189A78); return {}; }
bool HUDTarget::HideInProgress() const { SIMMY_STUB("HUDTarget::HideInProgress(void) const", 0x80189A88); return {}; }
RET_UNKNOWN HUDTarget::GetHudTransitionTarget() { SIMMY_STUB("HUDTarget::GetHudTransitionTarget(void)", 0x80189AC8); return {}; }
