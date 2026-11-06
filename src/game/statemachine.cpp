// Port of game/statemachine.cpp: StateMachineState, StateMachineStatus and StateMachine.
// (StateMachineManager is in statemachinemanager.cpp.)
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80095AA4
void StateMachineState::OwnerSetNextState(int id, float delay) { m_owner->SetNextState(id, delay); }
// 80095AC8
void StateMachineState::OwnerCallState(int id) { m_owner->CallState(id); }
// 80095AEC
void StateMachineState::OwnerReturnFromState(int value) { m_owner->ReturnFromState(value); }
// 80095B10
int StateMachineState::OwnerGetReturnVal() { return m_owner->m_returnVal; }
// 80095B1C
void StateMachineState::OwnerResetReturnVal() { m_owner->m_returnVal = 0; }

// 800963FC
void StateMachineStatus::StatusReset() { *this = StateMachineStatus{}; }

// 80096424
// Switching to the current state again only resets it.
void StateMachineStatus::SetState(StateMachineState* state) {
  if (state == m_cur) {
    if (state) state->Reset();
    return;
  }
  if (m_cur) m_cur->Shutdown();
  m_last = m_cur;
  m_cur = state;
  if (state) {
    state->Startup();
    m_secondsInCur = 0;
  }
}

// 800964D8
// As in the original: adds while the value is positive (so a positive delay never reaches zero by
// itself) and clamps at zero. In practice SetNextState is called with a delay of 0.
void StateMachineStatus::AddToSecondsBeforeNextState(float dt) {
  if (m_secondsBeforeNext <= 0.0f) return;
  m_secondsBeforeNext += dt;
  if (m_secondsBeforeNext < 0.0f) m_secondsBeforeNext = 0;
}

// 80096504
void StateMachineStatus::AddToSecondsInCurState(float dt) { m_secondsInCur += dt; }

// 80096514
void StateMachineStatus::ShutdownCurState() {
  if (m_cur) {
    m_cur->Shutdown();
    m_cur = nullptr;
  }
}
// 80096568
void StateMachineStatus::UpdateCurState(float dt) {
  if (m_cur) m_cur->Update(dt);
}
// 800965A8
void StateMachineStatus::DrawCurState(ERC* rc) {
  if (m_cur) m_cur->Draw(rc);
}
// 800965E8
void StateMachineStatus::DrawTopmostCurState(ERC* rc) {
  if (m_cur) m_cur->DrawTopmost(rc);
}

// 8009696C
StateMachine::StateMachine(int id, int machineId) : StateMachineState(id), m_machineId(machineId) {}

// 80096A68
// The statuses still on the stack are released; the states belong to the subclass (DeleteAllStates).
StateMachine::~StateMachine() {
  for (StateMachineStatus* s : m_stack) delete s;
}

// 80096AFC
void StateMachine::Shutdown() { m_status.ShutdownCurState(); }

// 80095B30
// Order per frame: pending calls (push the status, enter the called state), a pending return (leave
// the state, pop), then the delayed switch, then the current state's update. A state entered this
// frame is reset first and skips its update if it asked to (m_updateOnEnter == 0).
void StateMachine::Update(float dt) {
  if (!m_enabled || m_paused) return;
  m_frameDt = dt;
  m_status.AddToSecondsInCurState(dt);
  bool entered = false;
  while (m_callState) {
    entered = true;
    StateMachineState* called = m_callState;
    m_callState = nullptr;
    StateMachineState* caller = m_status.m_cur;
    PushStatus();
    m_status.StatusReset();
    m_status.m_calling = caller;
    ValidateHeap(false);
    SetState(called);
    ValidateHeap(false);
    m_returnVal = 0;
  }
  if (m_returnPending) {
    StateMachineState* leaving = m_status.m_cur;
    m_status.ShutdownCurState();
    PopStatus();
    m_status.m_last = leaving;
    m_returnPending = 0;
    m_returnVal = m_pendingReturnVal;
  }
  m_status.AddToSecondsBeforeNextState(dt);
  if (m_status.m_next && m_status.m_secondsBeforeNext == 0.0f) {
    entered = true;
    do {
      StateMachineState* next = m_status.m_next;
      m_status.m_next = nullptr;
      ValidateHeap(false);
      m_status.SetState(next);
      ValidateHeap(false);
    } while (m_status.m_next && m_status.m_secondsBeforeNext == 0.0f);
  }
  if (entered) {
    ValidateHeap(true);
    m_status.m_cur->Reset();
    ValidateHeap(false);
    if (!m_status.m_cur->m_updateOnEnter) return;
  }
  m_status.UpdateCurState(dt);
}

// 80095D08
void StateMachine::Draw(ERC* rc) {
  if (m_enabled) m_status.DrawCurState(rc);
}



// 80095D38
void StateMachine::DrawTopmost(ERC* rc) {
  if (_gd.m_drawTopmost && m_enabled) m_status.DrawTopmostCurState(rc);
}

// 80095D78
void StateMachine::SetNextState(int id, float delay) {
  if (StateMachineState* s = FindStateById(id)) {
    m_status.m_secondsBeforeNext = delay;
    m_status.m_next = s;
  }
}

// 80095DC4
void StateMachine::CallState(int id) {
  if (StateMachineState* s = FindStateById(id)) m_callState = s;
}

// 80095DFC
void StateMachine::ReturnFromState(int value) {
  m_pendingReturnVal = value;
  m_returnPending = 1;
}

// 80095E0C
int StateMachine::GetCurStateId() const { return m_status.m_cur ? m_status.m_cur->m_id : -1; }
// 80095E28
int StateMachine::GetCurStateStage() const { return m_status.m_cur->m_stage; }
// 80095E34
int StateMachine::GetReturnStateId() const { return m_stack.back()->m_cur->m_id; }
// 80095EA8
int StateMachine::GetLastStateId() const { return m_status.m_last->m_id; }
// 80095EB4
int StateMachine::GetCallingStateId() const { return m_status.m_calling->m_id; }

// 80095EC0
void StateMachine::RemoveAllStates() {
  m_status.ShutdownCurState();
  m_status.m_next = nullptr;
  m_status.m_secondsBeforeNext = 0;
  m_states.clear();
}

// 80095F10
StateMachineState* StateMachine::AddState(StateMachineState* state) {
  if (!FindState(state)) {
    m_states.push_back(state);
    state->m_owner = this;
  }
  return state;
}

// 80096080
void StateMachine::DeleteAllStates() {
  m_status.ShutdownCurState();
  m_status.m_next = nullptr;
  m_status.m_secondsBeforeNext = 0;
  for (StateMachineState*& s : m_states) {
    delete s;
    s = nullptr;
  }
  m_states = {};
}

// 800961B4
float StateMachine::GetTimeInState() const { return m_status.m_secondsInCur; }

// 800961BC
StateMachineState* StateMachine::FindStateById(int id) {
  for (StateMachineState* s : m_states)
    if (s->m_id == id) return s;
  return nullptr;
}
// 800961F4
StateMachineState* StateMachine::FindState(StateMachineState* state) {
  for (StateMachineState* s : m_states)
    if (s == state) return s;
  return nullptr;
}

// 80096224
void StateMachine::SetState(StateMachineState* state) {
  m_status.SetState(state);
  m_returnVal = 0;
}

// 8009625C
void StateMachine::PushStatus() { m_stack.push_back(new StateMachineStatus(m_status)); }

// 8009630C
void StateMachine::PopStatus() {
  StateMachineStatus* s = m_stack.back();
  m_status = *s;
  m_stack.pop_back();
  delete s;
}

// 80095B2C
void StateMachineState::ValidateHeap(bool) {}  // debug heap check, empty in this build
// 80096AF8
void StateMachine::Startup() {}
// 80096B20
void StateMachine::Reset() {}
// 80096B24
unsigned int StateMachine::GetMachineId() const { return m_machineId; }
// 80096B2C
StateMachineManager* StateMachine::GetManager() const { return m_manager; }
// 80096B34
float StateMachine::GetFrameDeltaTime() const { return m_frameDt; }
// 80096B3C
int StateMachine::GetReturnVal() const { return m_returnVal; }
// 80096B44
void StateMachine::ResetReturnVal() { m_returnVal = 0; }
// 80096B50
void StateMachine::Enable() { m_enabled = 1; }
// 80096B5C
void StateMachine::Disable() { m_enabled = 0; }
// 80096B68
void StateMachine::PauseUpdate() { m_paused = 1; }
// 80096B74
void StateMachine::UnpauseUpdate() { m_paused = 0; }
// 80096B80
int StateMachine::IsEnabled() { return m_enabled; }
// 80096B88
int StateMachine::IsPausedUpdate() { return m_paused; }
