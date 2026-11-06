// @class StartScreenVars after EWindow StateMachine StateMachineState ERShader
// @class TheSimsStartScreenStateMachine after StateMachine StartScreenVars
// @class StartScreenStartState after StateMachineState ERShader
// The start screen (game/startscreen.cpp, state 5 'STRT' of TheSimsStateMachine): the background
// gradient, the localised Sims 2 logo, the copyright notice and a blinking "press start". Start goes
// to the main menu through the UI router ("flowGoto"); the machine then waits for the loading screen
// to fade in and hands over to TheSimsStateMachine::kEnterMainMenu.
#pragma once

// Shared by the start screen states (TheSimsStartScreenStateMachine::s_pVars, 0x38 bytes on GC).
struct StartScreenVars {
  void Startup();  // 800954B0
  void Shutdown();  // 80095570
  void Reset();  // 800955B4
  void Update(float dt);  // 80095638
  void DrawPressStart(ERC* rc);  // 8009567C

  float m_time = 0;            // +0x00 seconds since Reset
  float m_field04 = 0;         // +0x04 15.0 after Reset (use not identified)
  float m_blinkTime = 0;       // +0x08 time into the current blink phase (0.43 s)
  int m_showPressStart = 0;    // +0x0C toggled every blink phase
  float m_textX = 0;           // +0x10 0.5
  float m_textY = 0;           // +0x14 0.805
  float m_textWidth = 0;       // +0x18 0.6
  int m_field1C = 0;           // +0x1C
  EWindow* m_window = nullptr; // +0x20 clip window under the text (Startup)
  int m_field24 = 0;           // +0x24
  int m_field28 = 0;           // +0x28
  int m_field2C = 0;           // +0x2C set by StartScreenStartState::Startup
  int m_field30 = 0;           // +0x30
  float m_timeInStart = 0;     // +0x34 seconds on the start state (start is taken after 4 s)
};

struct TheSimsStartScreenStateMachine : StateMachine {
  TheSimsStartScreenStateMachine() : StateMachine(0, 0) {}
  TheSimsStartScreenStateMachine(int id, int machineId);  // 80095970
  ~TheSimsStartScreenStateMachine() override = default;  // 803A0E90
  void Startup() override;  // 80095780
  void Shutdown() override;  // 80095824
  void Reset() override;  // 80095874
  void Update(float dt) override;  // 800958D4

  static StartScreenVars* s_pVars;  // 8050151C (s_pVars@sda21)
};

// The only state of the start screen machine (id 0).
struct StartScreenStartState : StateMachineState {
  StartScreenStartState() : StateMachineState(0) {}
  ~StartScreenStartState() override = default;  // 803A0F20
  void Startup() override;  // 803A0F54
  void Shutdown() override;  // 803A1000
  void Update(float dt) override;  // 803A1060
  void Draw(ERC* rc) override;  // 803A1328
  int IsFlowStateRequestPending();  // 803A1524
  int GetFlowStateRequest();  // 803A154C
  void SetFlowStateCurrent();  // 803A1574

  ERShader* m_background = nullptr;  // +0x1C s2c_bg_gradient (0x7F5ABC2D)
  ERShader* m_logo = nullptr;        // +0x20 GameData::GetLocalizedSims2Logo
};
