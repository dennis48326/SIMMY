// Port of engine/e_ctrlmanager.cpp: the four controllers and the player -> controller map.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <utility>

EController* EControllerManager::_ctrlPads[4] = {};
float EControllerManager::_ctrlLastActivetime[4][2] = {};
unsigned int _lastCtrlUpdate = 0;  // 804FF114, _retracecount when controller 0 last updated

// 8032339C
EControllerManager::EControllerManager() {}
// 80323ABC
EControllerManager::~EControllerManager() {}

// 803233C8
int EControllerManager::Init() {
  for (int i = 0; i < 4; ++i) {
    m_playerController[i] = i;
    m_playerMapped[i] = 1;
  }
  return 1;
}

// 803233F4
void EControllerManager::Update() {
  for (EController* c : _ctrlPads) c->Update();
}

// 8032344C
void EControllerManager::Shutdown() {}

// 80323450
void EControllerManager::MapPlayerToController(unsigned int player, unsigned int controller) {
  m_playerController[player] = controller;
  m_playerMapped[player] = 1;
}

// 80323468
// Swaps which player each of two controllers belongs to, and swaps the controllers' input filters.
void EControllerManager::SwapPlayerControls(unsigned int a, unsigned int b) {
  if (a == b) return;
  for (unsigned int& c : m_playerController) {
    if (c == a) c = b;
    else if (c == b) c = a;
  }
  EController* ca = _ctrlPads[a];
  EController* cb = _ctrlPads[b];
  std::swap(ca->m_topFilter, cb->m_topFilter);
  std::swap(ca->m_filters, cb->m_filters);
}

// 803235CC
void EControllerManager::UnmapPlayer(unsigned int player) { m_playerMapped[player] = 0; }
// 803235E0
int EControllerManager::GetPlayerMapped(unsigned int player) { return m_playerMapped[player]; }
// 803235F0
EController* EControllerManager::GetController(unsigned int index) { return index > 3 ? nullptr : _ctrlPads[index]; }

// 80323610
int EControllerManager::GetPlayerControllerIndex(unsigned int player) {
  return player < 4 ? static_cast<int>(m_playerController[player]) : -1;
}

// 8032362C
EController* EControllerManager::GetPlayerController(unsigned int player) {
  const int i = GetPlayerControllerIndex(player);
  return i == -1 ? nullptr : _ctrlPads[i];
}

// 8032366C
int EControllerManager::FindActiveController() {
  for (int i = 0; i < 4; ++i)
    if (_ctrlPads[i]->IsInUse()) return i;
  return -1;
}

// 803236CC
void EControllerManager::Flush() {
  for (EController* c : _ctrlPads) c->Flush();
}

// 80323724
int EControllerManager::GetCmdResultAllCtrl(unsigned int command, MaskFn anyMask, MaskFn allMask, bool exclusive,
                                            unsigned int* foundIndex, unsigned int filterId) {
  int result = 0;
  for (unsigned int i = 0; i < 4 && !result; ++i) {
    EController* c = _pCtrlMan->GetController(i);
    result = c->GetCmdResult(command, (c->*anyMask)(filterId), (c->*allMask)(filterId), exclusive);
    if (result && foundIndex) *foundIndex = i;
  }
  return result;
}

// 80323884
int EControllerManager::GetBtnResultAllCtrl(MaskFn mask, bool exclusive, bool any, unsigned int buttons,
                                            unsigned int* foundIndex, unsigned int filterId) {
  int result = 0;
  for (unsigned int i = 0; i < 4 && !result; ++i) {
    EController* c = _pCtrlMan->GetController(i);
    const unsigned int m = (c->*mask)(filterId);
    result = any ? c->GetBtnResultAny(m, exclusive, buttons) : c->GetBtnResult(m, exclusive, buttons);
    if (result && foundIndex) *foundIndex = i;
  }
  return result;
}

// 803239E8
void EControllerManager::MapControllerCommands(EBtnToCmdAssoc* map, int count) {
  for (unsigned int i = 0; i < 4; ++i) {
    GetController(i)->SetCommandMap(map, count);
    UnmapPlayer(i);
  }
}

// 80323AF0
int EControllerManager::GetControllerCount() { return 4; }

using C = EController;
// 80323AF8
int EControllerManager::GetCmdDownAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnDownMask, &C::GetBtnDownMask, false, i, f); }
// 80323B4C
int EControllerManager::GetCmdPressedAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnPressedMask, &C::GetBtnDownMask, false, i, f); }
// 80323BB8
int EControllerManager::GetCmdReleasedAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnReleasedMask, &C::GetBtnUpMask, false, i, f); }
// 80323C24
int EControllerManager::GetCmdUpAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnUpMask, &C::GetBtnUpMask, false, i, f); }
// 80323C78
int EControllerManager::GetCmdRepeatAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnRepeatMask, &C::GetBtnDownMask, false, i, f); }
// 80323CE4
int EControllerManager::GetBtnDownAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnDownMask, false, false, b, i, f); }
// 80323D34
int EControllerManager::GetBtnPressedAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnPressedMask, false, false, b, i, f); }
// 80323D84
int EControllerManager::GetBtnReleasedAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnReleasedMask, false, false, b, i, f); }
// 80323DD4
int EControllerManager::GetBtnUpAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnUpMask, false, false, b, i, f); }
// 80323E24
int EControllerManager::GetBtnRepeatAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnRepeatMask, false, false, b, i, f); }
// 80323E74
int EControllerManager::GetBtnDownAnyAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnDownMask, false, true, b, i, f); }
// 80323EC4
int EControllerManager::GetBtnPressedAnyAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnPressedMask, false, true, b, i, f); }
// 80323F14
int EControllerManager::GetBtnReleasedAnyAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnReleasedMask, false, true, b, i, f); }
// 80323F64
int EControllerManager::GetBtnUpAnyAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnUpMask, false, true, b, i, f); }
// 80323FB4
int EControllerManager::GetBtnRepeatAnyAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnRepeatMask, false, true, b, i, f); }
// 80324004
int EControllerManager::GetCmdDownExclAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnDownMask, &C::GetBtnDownMask, true, i, f); }
// 80324058
int EControllerManager::GetCmdPressedExclAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnPressedMask, &C::GetBtnDownMask, true, i, f); }
// 803240C4
int EControllerManager::GetCmdReleasedExclAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnReleasedMask, &C::GetBtnUpMask, true, i, f); }
// 80324130
int EControllerManager::GetCmdUpExclAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnUpMask, &C::GetBtnUpMask, true, i, f); }
// 80324184
int EControllerManager::GetCmdRepeatExclAllCtrl(unsigned int c, unsigned int* i, unsigned int f) { return GetCmdResultAllCtrl(c, &C::GetBtnRepeatMask, &C::GetBtnDownMask, true, i, f); }
// 803241F0
int EControllerManager::GetBtnDownExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnDownMask, true, false, b, i, f); }
// 80324240
int EControllerManager::GetBtnPressedExlAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnPressedMask, true, false, b, i, f); }
// 80324290
int EControllerManager::GetBtnReleasedExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnReleasedMask, true, false, b, i, f); }
// 803242E0
int EControllerManager::GetBtnUpExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnUpMask, true, false, b, i, f); }
// 80324330
int EControllerManager::GetBtnRepeatExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnRepeatMask, true, false, b, i, f); }
// 80324380
int EControllerManager::GetBtnDownAnyExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnDownMask, true, true, b, i, f); }
// 803243D0
int EControllerManager::GetBtnPressedAnyExlAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnPressedMask, true, true, b, i, f); }
// 80324420
int EControllerManager::GetBtnReleasedAnyExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnReleasedMask, true, true, b, i, f); }
// 80324470
int EControllerManager::GetBtnUpAnyExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnUpMask, true, true, b, i, f); }
// 803244C0
int EControllerManager::GetBtnRepeatAnyExclAllCtrl(unsigned int b, unsigned int* i, unsigned int f) { return GetBtnResultAllCtrl(&C::GetBtnRepeatMask, true, true, b, i, f); }

// 80324510
int EControllerManager::BitToIndex(unsigned int bit) {
  int i = 0;
  for (unsigned int rest = bit >> 1; rest; rest = bit >> (i + 2)) ++i;
  return i;
}

// 80324534
int EControllerManager::IndexToBit(int index) { return 1 << index; }
