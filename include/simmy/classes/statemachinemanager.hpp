// @class StateMachineManager after StateMachine
// The running state machines (game/statemachine.cpp): front end, loading screen, live mode... Each
// frame the app updates and draws them in the order they were added.
#pragma once

#include <vector>

struct StateMachineManager {
  StateMachine* FindMachineById(int id);  // 80096628
  StateMachine* FindMachine(StateMachine* machine);  // 80096668
  static void Startup();  // 800966A4
  static void Shutdown();  // 800966E0
  void UpdateMachine(float dt);  // 800967A4
  static void UpdateMachines(float dt);  // 80096810
  void DrawMachine(ERC* rc);  // 8009683C
  void DrawTopmostMachine(ERC* rc);  // 800968A0
  static void DrawMachines(ERC* rc);  // 80096904
  static void DrawTopmostMachines(ERC* rc);  // 80096938

  // PC: the inlined vector push_back of the original's callers (e.g. ESimsApp::initContinue), which
  // also linked the machine to the manager and started it.
  void AddMachine(StateMachine* machine);

  std::vector<StateMachine*> m_machines;
  static StateMachineManager* s_pStateMachineManager;
};
