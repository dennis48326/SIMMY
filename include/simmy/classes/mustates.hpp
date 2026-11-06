// @class MUStatesProcessRequests after StateMachineState
// @class MUStatesFormat after StateMachineState
// @class MUStatesSlotSelectSave after StateMachineState
// @class MUStatesSlotSelectSaveNoSkip after StateMachineState
// @class MUStatesLoadGame after StateMachineState
// @class MUStatesLoadHouse after StateMachineState
// @class MUStatesAutoSaveNew after StateMachineState
// @class MUStatesSaveNewGame after StateMachineState
// @class MUStatesAutoSaveExisting after StateMachineState
// @class MUStatesSaveExistingGame after StateMachineState
// @class MUStatesTransitionAutoSaveExisting after StateMachineState
// @class MUStatesTransitionSaveExistingGame after StateMachineState
// @class MUStatesLoadConfig after StateMachineState
// @class MUStatesDisplay after StateMachineState
// The states of MUStateMachine ('MUST', game/mustates.cpp). ProcessRequests (state 0) waits for a
// request and calls the state that runs it; that state calls others (slot select, format, the
// Display state that shows the MUWrapper dialog) and returns 1 (failed/cancelled) or 2 (done) to it.
// Each state reads the return value of the state it called from the machine (StateMachine::m_returnVal,
// reset to 0 once handled). m_step is the state's own progress through its checks.
#pragma once

struct MUStatesProcessRequests : StateMachineState {
  explicit MUStatesProcessRequests(int id);  // 80093BDC
  ~MUStatesProcessRequests() override;  // 80093C14
  void Update(float dt) override;  // 80091464
  MUWrapper* GetWrapper();  // 80093C48

  int m_step = 0;  // +0x1C 1: the called state asked to be re-run (a result of 2 is then not reported)
};

struct MUStatesFormat : StateMachineState {
  explicit MUStatesFormat(int id);  // 80093C54
  ~MUStatesFormat() override;  // 80093C8C
  void Reset() override;  // 800916F8
  void Draw(ERC* rc) override;  // 80091764
  void Update(float dt) override;  // 800917AC
  MUWrapper* GetWrapper();  // 80093CC0

  int m_step = 0;         // +0x1C
  int m_drawWrapper = 0;  // +0x20 draw the "formatting" info box
};

struct MUStatesSlotSelectSave : StateMachineState {
  explicit MUStatesSlotSelectSave(int id);  // 80093CCC
  ~MUStatesSlotSelectSave() override;  // 80093D04
  void Update(float dt) override;  // 80091A7C
  MUWrapper* GetWrapper();  // 80093D38
};

struct MUStatesSlotSelectSaveNoSkip : StateMachineState {
  explicit MUStatesSlotSelectSaveNoSkip(int id);  // 80093D44
  ~MUStatesSlotSelectSaveNoSkip() override;  // 80093D7C
  void Update(float dt) override;  // 80091AF0
  MUWrapper* GetWrapper();  // 80093DB0
};

struct MUStatesLoadGame : StateMachineState {
  explicit MUStatesLoadGame(int id);  // 80093DBC
  ~MUStatesLoadGame() override;  // 80093DF4
  void Reset() override;  // 80091B78
  void Draw(ERC* rc) override;  // 80091B84
  void Update(float dt) override;  // 80091B88
  MUWrapper* GetWrapper();  // 80093E28

  int m_step = 0;      // +0x1C
  float m_timer = 0;   // +0x20
};

struct MUStatesLoadHouse : StateMachineState {
  explicit MUStatesLoadHouse(int id);  // 80093E34
  ~MUStatesLoadHouse() override;  // 80093E6C
  void Reset() override;  // 8009200C
  void Draw(ERC* rc) override;  // 80092018
  void Update(float dt) override;  // 8009201C
  MUWrapper* GetWrapper();  // 80093EA0

  int m_step = 0;     // +0x1C
  float m_timer = 0;  // +0x20
};

struct MUStatesAutoSaveNew : StateMachineState {
  explicit MUStatesAutoSaveNew(int id);  // 80093EAC
  ~MUStatesAutoSaveNew() override;  // 80093EE4
  void Update(float dt) override;  // 8009217C
  MUWrapper* GetWrapper();  // 80093F18

  int m_step = 0;  // +0x1C
};

struct MUStatesSaveNewGame : StateMachineState {
  explicit MUStatesSaveNewGame(int id);  // 80093F24
  ~MUStatesSaveNewGame() override;  // 80093F6C
  void Reset() override;  // 800922DC
  void Draw(ERC* rc) override;  // 800922F4
  void Update(float dt) override;  // 80092340
  MUWrapper* GetWrapper();  // 80093FA0

  int m_step = 0;            // +0x1C
  int m_overwriteOk = 0;     // +0x20 the player agreed to overwrite the saved game
  float m_timer = 0;         // +0x24
  int m_field28 = 0;         // +0x28
  int m_field2C = 0;         // +0x2C
  float m_field30 = 0;       // +0x30
  float m_saveStartTime = 0; // +0x34 seconds on the system clock when the save started
};

struct MUStatesAutoSaveExisting : StateMachineState {
  explicit MUStatesAutoSaveExisting(int id);  // 80093FAC
  ~MUStatesAutoSaveExisting() override;  // 80093FE4
  void Reset() override;  // 80092988
  void Update(float dt) override;  // 80092998
  MUWrapper* GetWrapper();  // 80094018

  int m_step = 0;     // +0x1C
  int m_retries = 0;  // +0x20 failed saves; after the fourth the player is offered the main menu
};

struct MUStatesSaveExistingGame : StateMachineState {
  explicit MUStatesSaveExistingGame(int id);  // 80094024
  ~MUStatesSaveExistingGame() override;  // 8009405C
  void Reset() override;  // 80092B08
  void Draw(ERC* rc) override;  // 80092B14
  void Update(float dt) override;  // 80092B18
  MUWrapper* GetWrapper();  // 80094090

  int m_step = 0;     // +0x1C
  float m_timer = 0;  // +0x20
};

struct MUStatesTransitionAutoSaveExisting : StateMachineState {
  explicit MUStatesTransitionAutoSaveExisting(int id);  // 8009409C
  ~MUStatesTransitionAutoSaveExisting() override;  // 800940D4
  void Reset() override;  // 80092D44
  void Update(float dt) override;  // 80092D54
  MUWrapper* GetWrapper();  // 80094108

  int m_step = 0;     // +0x1C
  int m_retries = 0;  // +0x20
};

struct MUStatesTransitionSaveExistingGame : StateMachineState {
  explicit MUStatesTransitionSaveExistingGame(int id);  // 80094114
  ~MUStatesTransitionSaveExistingGame() override;  // 8009414C
  void Reset() override;  // 80092ED8
  void Draw(ERC* rc) override;  // 80092EE4
  void Update(float dt) override;  // 80092F5C
  MUWrapper* GetWrapper();  // 80094180

  int m_step = 0;     // +0x1C
  float m_timer = 0;  // +0x20
};

struct MUStatesLoadConfig : StateMachineState {
  explicit MUStatesLoadConfig(int id);  // 8009418C
  ~MUStatesLoadConfig() override;  // 800941C4
  void Reset() override;  // 80093118
  void Draw(ERC* rc) override;  // 80093124
  void Update(float dt) override;  // 80093170
  MUWrapper* GetWrapper();  // 800941F8

  int m_step = 0;  // +0x1C
};

// Shows the MUWrapper dialog set up by the calling state; the wrapper returns to that state.
struct MUStatesDisplay : StateMachineState {
  explicit MUStatesDisplay(int id);  // 80094204
  ~MUStatesDisplay() override;  // 8009423C
  void Startup() override;  // 800931B0
  void Shutdown() override;  // 800931B4
  void Reset() override;  // 800931DC
  void Update(float dt) override;  // 8009320C
  void Draw(ERC* rc) override;  // 80093250
  MUWrapper* GetWrapper();  // 80094270
};
