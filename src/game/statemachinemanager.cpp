// Port of game/statemachine.cpp: StateMachineManager. (StateMachine and StateMachineState follow.)
#include "simmy/types_gen.hpp"

StateMachineManager* StateMachineManager::s_pStateMachineManager = nullptr;

// 80096628
StateMachine* StateMachineManager::FindMachineById(int id) {
  for (StateMachine* m : m_machines)
    if (static_cast<int>(m->GetMachineId()) == id) return m;
  return nullptr;
}

// 80096668
StateMachine* StateMachineManager::FindMachine(StateMachine* machine) {
  for (StateMachine* m : m_machines)
    if (m == machine) return m;
  return nullptr;
}

// 800966A4
void StateMachineManager::Startup() { s_pStateMachineManager = new StateMachineManager; }

// 800966E0
// Releases the list (not the machines, which their owners delete).
void StateMachineManager::Shutdown() {
  delete s_pStateMachineManager;
  s_pStateMachineManager = nullptr;
}

// 800967A4
void StateMachineManager::UpdateMachine(float dt) {
  for (StateMachine* m : m_machines) m->Update(dt);
}
// 80096810
void StateMachineManager::UpdateMachines(float dt) {
  if (s_pStateMachineManager) s_pStateMachineManager->UpdateMachine(dt);
}
// 8009683C
void StateMachineManager::DrawMachine(ERC* rc) {
  for (StateMachine* m : m_machines) m->Draw(rc);
}
// 800968A0
void StateMachineManager::DrawTopmostMachine(ERC* rc) {
  for (StateMachine* m : m_machines) m->DrawTopmost(rc);
}
// 80096904
void StateMachineManager::DrawMachines(ERC* rc) {
  if (s_pStateMachineManager) s_pStateMachineManager->DrawMachine(rc);
}
// 80096938
void StateMachineManager::DrawTopmostMachines(ERC* rc) {
  if (s_pStateMachineManager) s_pStateMachineManager->DrawTopmostMachine(rc);
}

void StateMachineManager::AddMachine(StateMachine* machine) {
  if (!machine || FindMachine(machine)) return;
  m_machines.push_back(machine);
  machine->m_manager = this;
  machine->Startup();
}
