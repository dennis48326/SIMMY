// @class UrbzModeNewGameState after StateMachineState
// The development menu's new game (game/urbzmode.cpp, front-end state 16, flow request 8 from
// MMUTarget::EnterLot's "HUD_"): the default neighbourhood, the "createasim_dev" dataset, a CAS target
// without its UI that saves player 1's default sim into the neighbourhood, then straight into the
// start lot's live mode (state 13, kInLevelStory). m_stage (+0x0C) is the stage.
#pragma once

struct UrbzModeNewGameState : public StateMachineState {
  explicit UrbzModeNewGameState(int id = -1);  // 8009B6DC
  void Reset() override;                       // 8009B258
  void Update(float dt) override;              // 8009B288
};
