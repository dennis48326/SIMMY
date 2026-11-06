// Port of engine/e_ctrl.cpp: one controller's state, filters, command map, auto-repeat and rumble.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

int EController::_nCtrlPads = 0;
static unsigned int lastfilterId_1785 = 0;  // 804FF0D4

static float NowSeconds() { return static_cast<float>(_pClockMan->GetSec(&_sysclock)); }

// 80303E58
EController::EController() {
  m_id = _nCtrlPads++;
  m_status = 0;
  m_topFilter = -1;
  m_raw = new EControllerData;
  m_raw->Clear(true);
  m_app = new EControllerData;
  m_app->Clear(true);
  for (int s = 0; s < 2; ++s) {
    m_axesSwapped[s] = 0;
    m_dirModifier[s][0] = m_dirModifier[s][1] = 1;
  }
  for (unsigned char& c : m_repeatCounter) c = m_repeatDelay;
}

// 80303F9C
EController::~EController() {
  delete m_raw;
  m_raw = nullptr;
  delete m_app;
  m_app = nullptr;
}

// 80304014
void EController::Update() {
  if (m_id == 0) _lastCtrlUpdate = _retracecount;
  m_app->Reset(false);
  *m_app = *m_raw;
  UpdateAutoRepeat();
}

// 80304070
void EController::UpdateAutoRepeat() {
  bool idle = true;
  m_repeat = m_app->m_pressed;
  for (int i = 0; i < 32; ++i) {
    const unsigned int bit = 1u << i;
    if (!(m_app->m_pressed & bit) && (m_app->m_down & bit)) {
      if (--m_repeatCounter[i] == 0) {
        m_repeat |= bit;
        m_repeatCounter[i] = m_repeatRate;
      }
      if (idle) {
        idle = false;
        const float now = NowSeconds();
        EControllerManager::_ctrlLastActivetime[m_id][1] = now;
        EControllerManager::_ctrlLastActivetime[m_id][0] = now;
        if (!m_active) m_active = 1;
      }
    } else {
      m_repeatCounter[i] = m_repeatDelay;
    }
  }
  if (m_active && idle && NowSeconds() - EControllerManager::_ctrlLastActivetime[m_id][0] > 30.0f) m_active = 0;
}

// 803041E0
void EController::ClearAllData() { m_raw->Clear((m_status >> 1) & 1); }
// 8030420C
void EController::ClearAppData(bool stable) { m_app->Clear(stable); }

// 80304230
int EBtnToCmdAssoc::CompareControllerCommands(void* a, void* b) {
  const unsigned int x = static_cast<EBtnToCmdAssoc*>(a)->m_command, y = static_cast<EBtnToCmdAssoc*>(b)->m_command;
  return x < y ? -1 : (x > y ? 1 : 0);
}

// 8030507C
int EBtnToCmdAssoc::CompareControllerCommandToKey(void* key, void* entry) {
  const unsigned int x = *static_cast<unsigned int*>(key), y = static_cast<EBtnToCmdAssoc*>(entry)->m_command;
  return x < y ? -1 : (x > y ? 1 : 0);
}

// 80304250
void EController::SetCommandMap(EBtnToCmdAssoc* map, int count) {
  m_cmdMapCount = count;
  m_cmdMap = map;
  std::qsort(map, count, sizeof(EBtnToCmdAssoc), [](const void* a, const void* b) {
    return EBtnToCmdAssoc::CompareControllerCommands(const_cast<void*>(a), const_cast<void*>(b));
  });
}

// 80304294
void EController::OverrideStatus(int status) { m_status = status; }
// 8030429C
void EController::SwapAxes(int stick) { m_axesSwapped[stick] ^= 1; }
// 803042B4
void EController::InvertAxis(int stick, int axis) { m_dirModifier[stick][axis] = -m_dirModifier[stick][axis]; }

// 803042D4
int EController::IsInUse() {
  if (GetBtnResultAny(GetBtnDownMask(~0u), false, ~0u) || GetBtnResultAny(GetBtnPressedMask(~0u), false, ~0u) ||
      GetBtnResultAny(GetBtnReleasedMask(~0u), false, ~0u))
    return 1;
  return GetStick(0, 0, ~0u) != 0.0 || GetStick(0, 1, ~0u) != 0.0 || GetStick(1, 0, ~0u) != 0.0 ||
         GetStick(1, 1, ~0u) != 0.0;
}

// 8030440C
void EController::Flush() {
  m_app->m_released = 0;
  m_app->m_pressed = 0;
  m_raw->m_released = 0;
  m_raw->m_pressed = 0;
}

// 80304434
int EController::HasVibration() const { return 0; }
// 8030443C
bool EController::IsMotorOneActive() const { return m_motor[0] > 0.0f; }
// 80304458
bool EController::IsMotorTwoActive() const { return m_motor[1] > 0.0f; }

// 80304474
int EController::VibrateMotorOne(float strength) {
  if (!IsStable() || !m_active) return 0;
  m_motor[0] = strength;
  return 1;
}

// 803044E4
int EController::VibrateMotorOne(float strength, float seconds) {
  if (!m_active) return 0;
  m_motorTime[0] = seconds;
  return VibrateMotorOne(strength);
}

// 80304534
int EController::VibrateMotorTwo(float strength) {
  if (!IsStable() || !m_active) return 0;
  m_motor[1] = strength;
  return 1;
}

// 803045A4
int EController::VibrateMotorTwo(float strength, float seconds) {
  if (!m_active) return 0;
  m_motorTime[1] = seconds;
  return VibrateMotorTwo(strength);
}

// 803045F4
int EController::VibrateAll(float strength1, float strength2) {
  return VibrateMotorOne(strength1) && VibrateMotorTwo(strength2);
}

// 80304678
bool EController::VibrateAll(float strength1, float strength2, float seconds1, float seconds2) {
  return VibrateMotorOne(strength1, seconds1) && VibrateMotorTwo(strength2, seconds2);
}

// 80304710
int EController::StopMotorOne() {
  const int stable = IsStable();
  if (stable) m_motor[0] = m_motorTime[0] = 0.0f;
  return stable != 0;
}

// 80304774
int EController::StopMotorOne(float seconds) {
  m_motorTime[0] = seconds;
  return 1;
}

// 80304780
bool EController::StopMotorTwo() {
  const int stable = IsStable();
  if (stable) m_motor[1] = m_motorTime[1] = 0.0f;
  return stable != 0;
}

// 803047E4
int EController::StopMotorTwo(float seconds) {
  m_motorTime[1] = seconds;
  return 1;
}

// 803047F0
int EController::StopVibration() { return StopMotorOne() && StopMotorTwo(); }

// 80304864
int EController::UpdateVibration() {
  if (m_motorTime[0] != 0.0f && (m_motorTime[0] -= _dt) <= 0.0f) {
    m_motorTime[0] = 0.0f;
    StopMotorOne();
  }
  if (m_motorTime[1] != 0.0f && (m_motorTime[1] -= _dt) <= 0.0f) {
    m_motorTime[1] = 0.0f;
    StopMotorTwo();
  }
  return 1;
}

// 80304920
void EController::PauseMotors() {
  m_pausedMotor[0] = m_motor[0];
  m_pausedMotorTime[1] = m_motorTime[1];
  m_pausedMotor[1] = m_motor[1];
  m_pausedMotorTime[0] = m_motorTime[0];
  StopVibration();
}

// 8030497C
void EController::ResumeMotors() {
  VibrateAll(m_pausedMotor[0], m_pausedMotor[1], m_pausedMotorTime[0], m_pausedMotorTime[1]);
}

// 803049C4
void EController::ResetControllerData(bool stable) { m_raw->Reset(stable); }

// 803049E8
void EController::UpdateButtonData(unsigned int buttons) {
  EControllerData& d = *m_raw;
  d.m_lastButtons = d.m_buttons;
  d.m_buttons = buttons;
  const unsigned int rising = d.m_buttons & ~d.m_lastButtons;
  const unsigned int falling = d.m_lastButtons & ~d.m_buttons;
  for (unsigned int i = 0; i < 32; ++i) {
    const unsigned int bit = 1u << i;
    if (d.GetGotEvent(i)) continue;
    if ((rising & bit) && !(falling & bit)) {
      d.SetPressedFirst(i, true);
      d.SetGotEvent(i, true);
    } else if ((falling & bit) && !(rising & bit)) {
      d.SetPressedFirst(i, false);
      d.SetGotEvent(i, true);
    }
  }
  d.m_down |= d.m_buttons;
  d.m_up |= ~d.m_buttons;
  d.m_released |= falling;
  d.m_pressed |= rising;
}

// 80304B10
// Keeps the sample that moved furthest from last frame's value.
void EController::UpdateStickData(int stick, int axis, float value) {
  EControllerData& d = *m_raw;
  const float last = d.m_lastStick[stick][axis];
  if (std::fabs(d.m_stick[stick][axis] - last) < std::fabs(value - last)) {
    d.m_stick[stick][axis] = value;
    d.m_stickDelta[stick][axis] = value - last;
  }
  if (!(last * value > 0.0f)) d.m_stickCrossed[stick][axis] = 1;
}

static int LowestSetBitIndex(unsigned int button) {
  int i = 0;
  for (unsigned int rest = button >> 1; rest; rest = button >> (i + 2)) ++i;
  return i;
}

// 80304B98
void EController::AddPressedEventData(unsigned int button) {
  const int i = LowestSetBitIndex(button);
  if (!m_raw->GetGotEvent(i)) {
    m_raw->SetGotEvent(i, true);
    m_raw->SetPressedFirst(i, true);
  }
  m_raw->m_down |= button;
  m_raw->m_up |= button;
  m_raw->m_pressed |= button;
  m_raw->m_buttons |= button;
}

// 80304C54
void EController::AddReleasedEventData(unsigned int button) {
  const int i = LowestSetBitIndex(button);
  if (!m_raw->GetGotEvent(i)) {
    m_raw->SetGotEvent(i, true);
    m_raw->SetPressedFirst(i, false);
  }
  m_raw->m_down |= button;
  m_raw->m_up |= button;
  m_raw->m_released |= button;
  m_raw->m_buttons &= ~button;
}

// 80304D10
int EController::IsControlFake() { return m_fake; }
// 80304D18
void EController::SetControlFake(bool fake) { m_fake = fake; }
// 80304D20
void EController::SetFakeButtons(unsigned int buttons) { m_fakeButtons = buttons; }

static bool StickMasked(unsigned int filterMask, int stick) {
  return (stick == 0 && (filterMask & 0xF0000)) || (stick == 1 && (filterMask & 0xF00000));
}

// 80304D28
double EController::GetStick(int stick, int axis, unsigned int filterId) {
  if (StickMasked(GetFilterMask(filterId), stick)) return 0.0;
  if (m_axesSwapped[stick]) axis = axis == 0;
  return static_cast<float>(m_dirModifier[stick][axis]) * m_app->m_stick[stick][axis];
}

// 80304DF4
int EController::IsStickFiltered(int stick, int, unsigned int filterId) {
  return StickMasked(GetFilterMask(filterId), stick);
}

// 80304E58
bool EController::StickMoved(int stick, float threshold, unsigned int filterId) {
  if (std::fabs(static_cast<float>(GetStick(stick, 0, filterId) - GetLastStick(stick, 0, filterId))) >= threshold)
    return true;
  return std::fabs(static_cast<float>(GetStick(stick, 1, filterId) - GetLastStick(stick, 1, filterId))) >= threshold;
}

// 80304F1C
double EController::GetLastStick(int stick, int axis, unsigned int filterId) {
  if (StickMasked(GetFilterMask(filterId), stick)) return 0.0;
  if (m_axesSwapped[stick]) axis = axis == 0;
  return static_cast<float>(m_dirModifier[stick][axis]) * m_app->m_lastStick[stick][axis];
}

// 80304FE8
void EController::SetBtnExcl() { m_app->m_exclusiveUsed = 1; }

// 80304FF8
int EController::GetBtnResult(unsigned int mask, bool exclusive, unsigned int buttons) {
  if ((mask & buttons) != buttons) return 0;
  if (!exclusive) return 1;
  if (m_app->m_exclusiveUsed) return 0;
  m_app->m_exclusiveUsed = 1;
  return 1;
}

// 8030503C
int EController::GetBtnResultAny(unsigned int mask, bool exclusive, unsigned int buttons) {
  if (!(mask & buttons)) return 0;
  if (!exclusive) return 1;
  if (m_app->m_exclusiveUsed) return 0;
  m_app->m_exclusiveUsed = 1;
  return 1;
}

// 8030509C
// The first map entry for `command` (the map is sorted by command), or null.
EBtnToCmdAssoc* EController::FindFirstCommand(unsigned int command) {
  if (!command || !m_cmdMap) return nullptr;
  EBtnToCmdAssoc* end = m_cmdMap + m_cmdMapCount;
  EBtnToCmdAssoc* it = std::lower_bound(m_cmdMap, end, command,
                                        [](const EBtnToCmdAssoc& e, unsigned int c) { return e.m_command < c; });
  return it != end && it->m_command == command ? it : nullptr;
}

// 80305130
int EController::GetCmdResult(unsigned int command, unsigned int anyMask, unsigned int allMask, bool exclusive) {
  if (exclusive && m_app->m_exclusiveUsed) return 0;
  EBtnToCmdAssoc* end = m_cmdMap + m_cmdMapCount;
  for (EBtnToCmdAssoc* e = FindFirstCommand(command); e && e != end && e->m_command == command; ++e) {
    if ((anyMask & e->m_buttons) && (allMask & e->m_buttons) == e->m_buttons) {
      if (exclusive) m_app->m_exclusiveUsed = 1;
      return 1;
    }
  }
  return 0;
}

// 803051FC
// Adds a filter that hides `mask` from every filter below it; returns its id, or 0 when full.
unsigned int EController::AddFilter(unsigned int mask, char*, int priority) {
  ClearAppData((m_status >> 1) & 1);
  if (m_topFilter >= 31) return 0;
  const unsigned int id = ++lastfilterId_1785;
  int i = m_topFilter;
  while (i >= 0 && static_cast<unsigned int>(priority) < m_filters[i].m_priority) {
    m_filters[i + 1] = m_filters[i];
    --i;
  }
  m_filters[i + 1] = {id, static_cast<unsigned int>(priority), mask};
  ++m_topFilter;
  return id;
}

// 803052EC
void EController::RemoveFilter(unsigned int id) {
  ClearAppData((m_status >> 1) & 1);
  int i = GetFilterIndex(id);
  if (i < 0) return;
  for (; i < m_topFilter; ++i) m_filters[i] = m_filters[i + 1];
  m_filters[i] = {};
  --m_topFilter;
}

// 803053A8
void EController::MoveFilterToTop(unsigned int id) {
  int i = GetFilterIndex(id);
  if (i < 0) return;
  const EControllerFilter f = m_filters[i];
  while (i < m_topFilter && m_filters[i + 1].m_priority <= f.m_priority) {
    m_filters[i] = m_filters[i + 1];
    ++i;
  }
  m_filters[i] = f;
}

// 803054A4
void EController::SetFilter(unsigned int id, unsigned int mask) {
  const int i = GetFilterIndex(id);
  if (i >= 0) m_filters[i].m_mask = mask;
}

// 803054E8
void EController::AddCmdToFilter(unsigned int id, unsigned int command) { AddBtnToFilter(id, GetCmdButtonMask(command)); }

// 8030552C
void EController::AddCmdsToFilter(unsigned int id, unsigned int* commands, int count) {
  for (int i = 0; i < count; ++i) AddCmdToFilter(id, commands[i]);
}

// 80305588
void EController::RemoveCmdFromFilter(unsigned int id, unsigned int command) {
  RemoveBtnFromFilter(id, GetCmdButtonMask(command));
}

// 803055CC
void EController::AddBtnToFilter(unsigned int id, unsigned int buttons) {
  const int i = GetFilterIndex(id);
  if (i >= 0) m_filters[i].m_mask |= buttons;
}

// 80305618
void EController::RemoveBtnFromFilter(unsigned int id, unsigned int buttons) {
  const int i = GetFilterIndex(id);
  if (i >= 0) m_filters[i].m_mask &= ~buttons;
}

// 80305664
unsigned int EController::GetCmdButtonMask(unsigned int command) {
  unsigned int mask = 0;
  EBtnToCmdAssoc* end = m_cmdMap + m_cmdMapCount;
  for (EBtnToCmdAssoc* e = FindFirstCommand(command); e && e != end && e->m_command == command; ++e)
    mask |= e->m_buttons;
  return mask;
}

// 803056BC
int EController::GetFilterIndex(unsigned int id) {
  if (!id) return -1;
  for (int i = m_topFilter; i >= 0; --i)
    if (m_filters[i].m_id == id) return i;
  return -1;
}

// 80305700
// Buttons hidden from the filter `id` by filters above it; ~0 means "no filter" and hides nothing.
unsigned int EController::GetFilterMask(unsigned int id) {
  if (id == ~0u) return 0;
  unsigned int mask = 0;
  for (int i = GetFilterIndex(id) + 1; i <= m_topFilter; ++i) mask |= m_filters[i].m_mask;
  return mask;
}

// 80305778
unsigned int EController::GetBtnDownMask(unsigned int f) { return m_app->m_down & ~GetFilterMask(f); }
// 803057B0
unsigned int EController::GetBtnUpMask(unsigned int f) { return m_app->m_up & ~GetFilterMask(f); }
// 803057E8
unsigned int EController::GetBtnPressedMask(unsigned int f) { return m_app->m_pressed & ~GetFilterMask(f); }
// 80305820
unsigned int EController::GetBtnReleasedMask(unsigned int f) { return m_app->m_released & ~GetFilterMask(f); }
// 80305858
unsigned int EController::GetBtnRepeatMask(unsigned int f) { return m_repeat & ~GetFilterMask(f); }

// 8030588C
int EController::GetId() { return m_id; }
// 80305894
int EController::GetStatus() { return static_cast<int>(m_status); }
// 8030589C
int EController::IsStable() { return (m_status >> 1) & 1; }

// 803058A8
int EController::GetCmdDown(unsigned int c, unsigned int f) { return GetCmdResult(c, GetBtnDownMask(f), GetBtnDownMask(f), false); }
// 80305908
int EController::GetCmdPressed(unsigned int c, unsigned int f) { return GetCmdResult(c, GetBtnPressedMask(f), GetBtnDownMask(f), false); }
// 80305968
int EController::GetCmdReleased(unsigned int c, unsigned int f) { return GetCmdResult(c, GetBtnReleasedMask(f), GetBtnUpMask(f), false); }
// 803059C8
int EController::GetCmdUp(unsigned int c, unsigned int f) { return GetCmdResult(c, GetBtnUpMask(f), GetBtnUpMask(f), false); }
// 80305A28
int EController::GetCmdRepeat(unsigned int c, unsigned int f) { return GetCmdResult(c, GetBtnRepeatMask(f), GetBtnDownMask(f), false); }
// 80305A88
int EController::GetBtnDown(unsigned int b, unsigned int f) { return GetBtnResult(GetBtnDownMask(f), false, b); }
// 80305AD0
int EController::GetBtnDownAny(unsigned int b, unsigned int f) { return GetBtnResultAny(GetBtnDownMask(f), false, b); }
// 80305B18
int EController::GetBtnUp(unsigned int b, unsigned int f) { return GetBtnResult(GetBtnUpMask(f), false, b); }
// 80305B60
int EController::GetBtnUpAny(unsigned int b, unsigned int f) { return GetBtnResultAny(GetBtnUpMask(f), false, b); }
// 80305BA8
int EController::GetBtnPressed(unsigned int b, unsigned int f) { return GetBtnResult(GetBtnPressedMask(f), false, b); }
// 80305BF0
int EController::GetBtnPressedAny(unsigned int b, unsigned int f) { return GetBtnResultAny(GetBtnPressedMask(f), false, b); }
// 80305C38
int EController::GetBtnReleased(unsigned int b, unsigned int f) { return GetBtnResult(GetBtnReleasedMask(f), false, b); }
// 80305C80
int EController::GetBtnReleasedAny(unsigned int b, unsigned int f) { return GetBtnResultAny(GetBtnReleasedMask(f), false, b); }
// 80305CC8
int EController::GetBtnRepeat(unsigned int b, unsigned int f) { return GetBtnResult(GetBtnRepeatMask(f), false, b); }
// 80305D10
int EController::GetBtnRepeatAny(unsigned int b, unsigned int f) { return GetBtnResultAny(GetBtnRepeatMask(f), false, b); }

// 80305D58
int EController::AreAxesSwapped(int stick) { return m_axesSwapped[stick]; }
// 80305D68
int EController::GetDirectionModifier(int stick, int axis) { return m_dirModifier[stick][axis]; }
