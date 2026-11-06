// @class CreateASimBaseState after StateMachineState
// @class PreGameCasPlayer1State
// @class PreGameState
// @class PreGameCasPlayer2State
// The states between the main menu and the game (game/pregamestate.cpp, game/basestates.cpp):
// PreGameState prepares a new or continued game (the default neighbourhood for a new one) and calls
// Create-a-Sim for player 1; CreateASimBaseState loads the "createasim" dataset, builds the CAS target
// (scene, sims, UI) on the background thread and runs it until the UI asks to leave.
#pragma once

struct PreGameState : public StateMachineState {
  explicit PreGameState(int id = -1);  // 8009526C
  void Reset() override;  // 8009427C
  void Update(float) override;  // 800942F4
  static bool InsertPlayer2Roommate();  // 800948D4
  void SetFlowStateCurrent();  // 800952B4

  int m_calledCas2 = 0;      // +0x1C player 2's CAS was called
  int m_twoPlayersIn = 0;    // +0x20
  int m_done = 0;            // +0x24 enter the game once the loading screen is up
  int m_loading = 1;         // +0x28 the staged start (neighbourhood) is running
  int m_needCas = 1;         // +0x2C
};

struct CreateASimBaseState : public StateMachineState {
  explicit CreateASimBaseState(int id = -1);  // 800898C4
  void Startup() override;  // 800890AC
  void Shutdown() override;  // 80089140
  void Reset() override;  // 800891B8
  void GetDatasetName(char* out);  // 80089214
  void Update(float) override;  // 80089240
  void OnBackOut();  // 80089798
  void Draw(ERC *) override;  // 800898C0
  RET_UNKNOWN CASAccepted();  // 80089910
  RET_UNKNOWN CASCanceled();  // 80089938
  void IsFlowStateRequestPending();  // 80089960
  void GetFlowStateRequest();  // 80089984
  void SetFlowStateCurrent();  // 800899A8

  int m_loadingStages = 1;   // +0x1C the staged load is running
  int m_waitFadeIn = 0;      // +0x20 leaving: wait for the loading screen
  int m_continue = 0;        // +0x24 editing an existing family
  int m_accepted = 0;        // +0x28
  int m_canceled = 0;        // +0x2C
};

struct PreGameCasPlayer1State : public CreateASimBaseState {
  explicit PreGameCasPlayer1State(int id = -1);  // 800952D8
  void Startup() override;  // 800949E4
  void Shutdown() override;  // 800949E8
  void Reset() override;  // 800949EC
  void Update(float) override;  // 80094A58
};

struct PreGameCasPlayer2State : public CreateASimBaseState {
  explicit PreGameCasPlayer2State(int id = -1);  // 80095324
  void Startup() override;  // 80094AF0
  void Shutdown() override;  // 80094B54
  void Reset() override;  // 80094B68
  void Update(float) override;  // 80094BD4
};
