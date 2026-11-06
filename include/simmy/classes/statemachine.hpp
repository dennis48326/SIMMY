// @class StateMachineState
// @class StateMachineStatus after StateMachineState
// @class StateMachine after StateMachineState StateMachineStatus
// State machines (game/statemachine.cpp). A machine owns a list of states (by id) and runs one at a
// time; SetNextState switches after an optional delay, CallState runs a state as a subroutine (the
// current status is pushed and restored by ReturnFromState). A machine is itself a state, so machines
// can nest. GC offsets are noted; the GC vtable sat at +0x18, after the state's own members.
#pragma once

#include <deque>
#include <vector>

struct StateMachine;

struct StateMachineState {
  explicit StateMachineState(int id = -1) : m_id(id) {}
  virtual ~StateMachineState() = default;  // 803A0288
  virtual void Startup() {}             // 803A024C
  virtual void Shutdown() {}            // 803A02BC
  virtual void Reset() {}               // 803A02C0
  virtual void Update(float) {}         // 803A02C4
  virtual void Draw(ERC*) {}            // 803A02C8
  virtual void DrawTopmost(ERC*) {}     // 803A0250

  void OwnerSetNextState(int id, float delay);  // 80095AA4
  void OwnerCallState(int id);  // 80095AC8
  void OwnerReturnFromState(int value);  // 80095AEC
  int OwnerGetReturnVal();  // 80095B10
  void OwnerResetReturnVal();  // 80095B1C
  void ValidateHeap(bool);  // 80095B2C (debug heap check, empty in this build)
  void SetFlag(unsigned int flag) { m_flags |= flag; }  // 8039CFFC

  int m_id;                         // +0x00
  int m_field04 = -1;               // +0x04
  StateMachine* m_owner = nullptr;  // +0x08
  int m_stage = -1;                 // +0x0C
  unsigned int m_flags = 0;         // +0x10
  int m_updateOnEnter = 1;          // +0x14 0: skip Update on the frame the state is entered
};

// The running state of a machine (pushed while a called state runs).
struct StateMachineStatus {
  void StatusReset();  // 800963FC
  void SetState(StateMachineState* state);  // 80096424
  void AddToSecondsBeforeNextState(float dt);  // 800964D8
  void AddToSecondsInCurState(float dt);  // 80096504
  void ShutdownCurState();  // 80096514
  void UpdateCurState(float dt);  // 80096568
  void DrawCurState(ERC* rc);  // 800965A8
  void DrawTopmostCurState(ERC* rc);  // 800965E8

  StateMachineState* m_cur = nullptr;      // +0x00
  StateMachineState* m_next = nullptr;     // +0x04
  StateMachineState* m_last = nullptr;     // +0x08
  StateMachineState* m_calling = nullptr;  // +0x0C
  float m_secondsBeforeNext = 0;           // +0x10 (delay before m_next; counts towards 0)
  float m_secondsInCur = 0;                // +0x14
};

struct StateMachine : StateMachineState {
  StateMachine() : StateMachine(0, 0) {}
  StateMachine(int id, int machineId);  // 8009696C
  ~StateMachine() override;  // 80096A68
  void Startup() override;  // 80096AF8
  void Shutdown() override;  // 80096AFC
  void Reset() override;  // 80096B20
  void Update(float dt) override;  // 80095B30
  void Draw(ERC* rc) override;  // 80095D08
  void DrawTopmost(ERC* rc) override;  // 80095D38

  void SetNextState(int id, float delay);  // 80095D78
  void CallState(int id);  // 80095DC4
  void ReturnFromState(int value);  // 80095DFC
  int GetCurStateId() const;  // 80095E0C
  int GetCurStateStage() const;  // 80095E28
  int GetReturnStateId() const;  // 80095E34
  int GetLastStateId() const;  // 80095EA8
  int GetCallingStateId() const;  // 80095EB4
  void RemoveAllStates();  // 80095EC0
  StateMachineState* AddState(StateMachineState* state);  // 80095F10
  void DeleteAllStates();  // 80096080
  float GetTimeInState() const;  // 800961B4
  StateMachineState* FindStateById(int id);  // 800961BC
  StateMachineState* FindState(StateMachineState* state);  // 800961F4
  void SetState(StateMachineState* state);  // 80096224
  void PushStatus();  // 8009625C
  void PopStatus();  // 8009630C
  unsigned int GetMachineId() const;  // 80096B24
  StateMachineManager* GetManager() const;  // 80096B2C
  float GetFrameDeltaTime() const;  // 80096B34
  int GetReturnVal() const;  // 80096B3C
  void ResetReturnVal();  // 80096B44
  void Enable();  // 80096B50
  void Disable();  // 80096B5C
  void PauseUpdate();  // 80096B68
  void UnpauseUpdate();  // 80096B74
  int IsEnabled();  // 80096B80
  int IsPausedUpdate();  // 80096B88

  unsigned int m_machineId;                   // +0x1C four-character tag ('SIMS', 'LdSc', ...)
  StateMachineManager* m_manager = nullptr;   // +0x20
  int m_paused = 0;                           // +0x24
  float m_frameDt = 0;                        // +0x28
  std::vector<StateMachineState*> m_states;   // +0x2C
  StateMachineStatus m_status;                // +0x3C
  std::deque<StateMachineStatus*> m_stack;    // +0x54
  StateMachineState* m_callState = nullptr;   // +0x84
  int m_returnPending = 0;                    // +0x88
  int m_pendingReturnVal = 0;                 // +0x8C
  int m_returnVal = 0;                        // +0x90
  int m_enabled = 1;                          // +0x94
};
