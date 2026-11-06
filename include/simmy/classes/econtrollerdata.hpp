// @class EControllerData
// One frame of controller state (engine/e_controllerdata.cpp, 0x84 bytes on GameCube). EController keeps
// two: the raw data accumulated from the pad, and the copy the game reads ("app" data).
#pragma once

struct EControllerData {
  unsigned int m_buttons;          // 0x00 buttons held now
  unsigned int m_lastButtons;      // 0x04
  unsigned int m_down;             // 0x08 held at any point this frame
  unsigned int m_lastDown;         // 0x0C
  unsigned int m_up;               // 0x10 not held at some point this frame
  unsigned int m_lastUp;           // 0x14
  unsigned int m_pressed;          // 0x18 went down this frame
  unsigned int m_lastPressed;      // 0x1C
  unsigned int m_released;         // 0x20 went up this frame
  unsigned int m_lastReleased;     // 0x24
  float m_stick[2][2];             // 0x28 [stick][axis], -1..1
  float m_lastStick[2][2];         // 0x38
  float m_stickDelta[2][2];        // 0x48
  float m_lastStickDelta[2][2];    // 0x58
  int m_stickCrossed[2][2];        // 0x68 the axis passed through zero this frame
  int m_exclusiveUsed;             // 0x78 an "exclusive" query already consumed this frame's input
  unsigned int m_pressedFirst;     // 0x7C per button: the first event this frame was a press
  unsigned int m_gotEvent;         // 0x80 per button: an event already happened this frame

  int GetPressedFirst(unsigned int button) const;
  int GetGotEvent(unsigned int button) const;
  void SetPressedFirst(unsigned int button, bool value);
  void SetGotEvent(unsigned int button, bool value);
  void Clear(bool stable);
  void Reset(bool stable);
  void operator=(EControllerData& other);
};
