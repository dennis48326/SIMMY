// Port of engine/e_controllerdata.cpp.
#include "simmy/types_gen.hpp"

// 803659CC
int EControllerData::GetPressedFirst(unsigned int button) const { return (m_pressedFirst & (1u << button)) != 0; }
// 803659EC
int EControllerData::GetGotEvent(unsigned int button) const { return (m_gotEvent & (1u << button)) != 0; }

// 80365A0C
void EControllerData::SetPressedFirst(unsigned int button, bool value) {
  if (value) m_pressedFirst |= 1u << button;
  else m_pressedFirst &= ~(1u << button);
}

// 80365A44
void EControllerData::SetGotEvent(unsigned int button, bool value) {
  if (value) m_gotEvent |= 1u << button;
  else m_gotEvent &= ~(1u << button);
}

// 80365A7C
void EControllerData::Clear(bool stable) {
  m_exclusiveUsed = 0;
  m_lastDown = m_lastPressed = m_lastReleased = 0;
  for (int s = 0; s < 2; ++s)
    for (int a = 0; a < 2; ++a) {
      m_stick[s][a] = m_lastStick[s][a] = 0.0f;
      m_stickDelta[s][a] = m_lastStickDelta[s][a] = 0.0f;
      m_stickCrossed[s][a] = 0;
    }
  m_up = m_lastUp = stable ? 0u : 0xFFFFFFFFu;
  m_released = 0;
  m_buttons = m_lastButtons = 0;
  m_down = 0;
  m_pressed = 0;
  for (unsigned int i = 0; i < 32; ++i) {
    SetGotEvent(i, false);
    SetPressedFirst(i, false);
  }
}

// 80365B60
void EControllerData::Reset(bool stable) {
  m_lastDown = m_down;
  m_lastUp = m_up;
  m_lastPressed = m_pressed;
  m_lastReleased = m_released;
  m_exclusiveUsed = 0;
  m_pressed = m_released = m_down = 0;
  for (int s = 0; s < 2; ++s)
    for (int a = 0; a < 2; ++a) {
      m_lastStick[s][a] = m_stick[s][a];
      m_lastStickDelta[s][a] = m_stickDelta[s][a];
      m_stickDelta[s][a] = 0.0f;
      m_stickCrossed[s][a] = 0;
    }
  m_up = stable ? 0u : 0xFFFFFFFFu;
  for (unsigned int i = 0; i < 32; ++i) {
    SetGotEvent(i, false);
    SetPressedFirst(i, false);
  }
}

// 80365C60
// Copies everything except m_exclusiveUsed.
void EControllerData::operator=(EControllerData& o) {
  m_buttons = o.m_buttons;
  m_lastButtons = o.m_lastButtons;
  m_down = o.m_down;
  m_lastDown = o.m_lastDown;
  m_up = o.m_up;
  m_lastUp = o.m_lastUp;
  m_pressed = o.m_pressed;
  m_released = o.m_released;
  m_lastPressed = o.m_lastPressed;
  m_lastReleased = o.m_lastReleased;
  for (int s = 0; s < 2; ++s)
    for (int a = 0; a < 2; ++a) {
      m_stick[s][a] = o.m_stick[s][a];
      m_lastStick[s][a] = o.m_lastStick[s][a];
      m_stickDelta[s][a] = o.m_stickDelta[s][a];
      m_lastStickDelta[s][a] = o.m_lastStickDelta[s][a];
      m_stickCrossed[s][a] = o.m_stickCrossed[s][a];
    }
  for (unsigned int i = 0; i < 32; ++i) {
    SetGotEvent(i, o.GetGotEvent(i));
    SetPressedFirst(i, o.GetPressedFirst(i));
  }
}
