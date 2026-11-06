// Port of game/thesims.cpp's UrbzCreditsState. See urbzcredits.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 8009B188
UrbzCreditsState::UrbzCreditsState(int id) : StateMachineState(id) {}
// 80098D2C
void UrbzCreditsState::Reset() {}
// 80098D30
void UrbzCreditsState::Shutdown() { _pCtrlMan->UnmapPlayer(0); }

// 80098D58
// "MainMenu" ends it (back with 1).
void UrbzCreditsState::Update(float) {
  if (!IsFlowStateRequestPending() || GetFlowStateRequest() != FlowGotoTarget::kMainMenu) return;
  SetFlowStateCurrent();
  OwnerReturnFromState(1);
}

// 8009B1C0
int UrbzCreditsState::IsFlowStateRequestPending() {
  return static_cast<TheSimsStateMachine*>(m_owner)->IsFlowStateRequestPending();
}
// 8009B1E4
int UrbzCreditsState::GetFlowStateRequest() { return static_cast<TheSimsStateMachine*>(m_owner)->GetFlowStateRequest(); }
// 8009B208
void UrbzCreditsState::SetFlowStateCurrent() { static_cast<TheSimsStateMachine*>(m_owner)->SetFlowStateCurrent(); }
