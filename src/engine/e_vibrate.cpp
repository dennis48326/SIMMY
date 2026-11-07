// Port of engine/e_vibrate.cpp: EVibrate (see evibrate.hpp). The controllers' rumble itself is
// EController's (e_ctrl.cpp) and ENgcController's (e_ngcctrlmanager.cpp: aurora PADControlMotor).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

namespace {
EController* Controller(unsigned int port) { return _pCtrlMan ? _pCtrlMan->GetController(port) : nullptr; }
}  // namespace

// 80307BCC
// GC: also clears the SDK's alignment scratch (s_u8SysAlign).
EVibrate::EVibrate() {
  m_paused = 0;
  m_enabled = 0;
  std::memset(static_cast<void*>(m_ports), 0, sizeof m_ports);
}

// 80307C24
EVibrate::~EVibrate() {}

// 80307C4C
void EVibrate::Enable() {
  if (IsOn()) return;
  m_paused = 0;
  m_enabled = 1;
  for (Port& p : m_ports) p.m_on = 0;
}

// 80307CB0
void EVibrate::Disable() { m_enabled = 0; }
// 80307CBC
int EVibrate::IsOn() { return m_enabled; }

// 80307CC4
int EVibrate::TurnOn(unsigned char port) {
  EController* c = Controller(port);
  m_ports[port].m_on = c ? c->HasVibration() : 0;
  return m_ports[port].m_on;
}

// 80307D40
int EVibrate::IsControllerReady(unsigned char port) { return IsControllerOn(port); }
// 80307D60
void EVibrate::TurnOff(unsigned char port) { m_ports[port].m_on = 0; }
// 80307D74
int EVibrate::IsControllerOn(unsigned char port) { return port < 2 ? m_ports[port].m_on : 0; }

// 80307D94
void EVibrate::Pause() {
  for (unsigned int i = 0; i < 4; ++i)
    if (IsControllerOn(static_cast<unsigned char>(i)))
      if (EController* c = Controller(i)) c->PauseMotors();
  m_paused = 1;
}

// 80307E14
void EVibrate::Resume() {
  if (!m_paused) return;
  m_paused = 0;
  for (unsigned int i = 0; i < 4; ++i)
    if (IsControllerOn(static_cast<unsigned char>(i)))
      if (EController* c = Controller(i)) c->ResumeMotors();
}

// 80307EA0
int EVibrate::VibrateMotorOne(unsigned char port, float strength) {
  EController* c = Controller(port);
  return c ? c->VibrateMotorOne(strength) : 0;
}
// 80307EFC
int EVibrate::VibrateMotorOne(unsigned char port, float strength, float seconds) {
  EController* c = Controller(port);
  return c ? c->VibrateMotorOne(strength, seconds) : 0;
}
// 80307F68
int EVibrate::VibrateMotorTwo(unsigned char port, float strength) {
  EController* c = Controller(port);
  return c ? c->VibrateMotorTwo(strength) : 0;
}
// 80307FC4
int EVibrate::VibrateMotorTwo(unsigned char port, float strength, float seconds) {
  EController* c = Controller(port);
  return c ? c->VibrateMotorTwo(strength, seconds) : 0;
}
// 80308030
int EVibrate::VibrateAll(unsigned char port, float strength1, float strength2) {
  EController* c = Controller(port);
  return c ? c->VibrateAll(strength1, strength2) : 0;
}
// 8030809C
int EVibrate::VibrateAll(unsigned char port, float strength1, float strength2, float seconds1, float seconds2) {
  EController* c = Controller(port);
  return c ? (c->VibrateAll(strength1, strength2, seconds1, seconds2) ? 1 : 0) : 0;
}
// 80308128
int EVibrate::StopMotorOne(unsigned char port) {
  EController* c = Controller(port);
  return c ? c->StopMotorOne() : 0;
}
// 80308174
int EVibrate::StopMotorOne(unsigned char port, float seconds) {
  EController* c = Controller(port);
  return c ? c->StopMotorOne(seconds) : 0;
}
// 803081D0
int EVibrate::StopMotorTwo(unsigned char port) {
  EController* c = Controller(port);
  return c ? (c->StopMotorTwo() ? 1 : 0) : 0;
}
// 8030821C
int EVibrate::StopMotorTwo(unsigned char port, float seconds) {
  EController* c = Controller(port);
  return c ? c->StopMotorTwo(seconds) : 0;
}
// 80308278
int EVibrate::StopVibration(unsigned char port) {
  EController* c = Controller(port);
  return c ? c->StopVibration() : 0;
}

// 803082C4
void EVibrate::StopAllVibration() {
  for (unsigned int i = 0; i < 4; ++i)
    if (IsControllerOn(static_cast<unsigned char>(i)))
      if (EController* c = Controller(i)) c->StopVibration();
}

// 8030833C
int EVibrate::UpdateVibration() { return 1; }
