// @class UrbzCreditsState after StateMachineState
// The credits state of the front-end machine (game/thesims.cpp, state 17, flow 0x18 "Credits"): it
// waits for "MainMenu" (1) and returns; player 1's pad is unmapped when it ends. The main menu's own
// credits are CRDTarget's overlay (no state change); no movie of this build asks for "Credits".
#pragma once

struct UrbzCreditsState : public StateMachineState {
  explicit UrbzCreditsState(int id = -1);  // 8009B188
  void Reset() override;  // 80098D2C
  void Shutdown() override;  // 80098D30
  void Update(float dt) override;  // 80098D58
  int IsFlowStateRequestPending();  // 8009B1C0
  int GetFlowStateRequest();  // 8009B1E4
  void SetFlowStateCurrent();  // 8009B208
};
