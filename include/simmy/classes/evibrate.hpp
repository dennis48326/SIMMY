// @class EVibrate
// The game's rumble switch (engine/e_vibrate.cpp; EGlobal +0x110): which controllers may rumble, and
// pass-throughs to the controller's motors (EController::VibrateMotorOne/Two/All, Stop..., with
// ENgcController driving the pad's motor through aurora's PADControlMotor).
#pragma once

struct EVibrate {
  EVibrate();  // 80307BCC
  ~EVibrate();  // 80307C24
  void Enable();  // 80307C4C
  void Disable();  // 80307CB0
  int IsOn();  // 80307CBC
  // Port `port` may rumble when its controller has a motor (EController::HasVibration).
  int TurnOn(unsigned char port);  // 80307CC4
  int IsControllerReady(unsigned char port);  // 80307D40
  void TurnOff(unsigned char port);  // 80307D60
  int IsControllerOn(unsigned char port);  // 80307D74
  void Pause();  // 80307D94
  void Resume();  // 80307E14
  int VibrateMotorOne(unsigned char port, float strength);  // 80307EA0
  int VibrateMotorOne(unsigned char port, float strength, float seconds);  // 80307EFC
  int VibrateMotorTwo(unsigned char port, float strength);  // 80307F68
  int VibrateMotorTwo(unsigned char port, float strength, float seconds);  // 80307FC4
  int VibrateAll(unsigned char port, float strength1, float strength2);  // 80308030
  int VibrateAll(unsigned char port, float strength1, float strength2, float seconds1, float seconds2);  // 8030809C
  int StopMotorOne(unsigned char port);  // 80308128
  int StopMotorOne(unsigned char port, float seconds);  // 80308174
  int StopMotorTwo(unsigned char port);  // 803081D0
  int StopMotorTwo(unsigned char port, float seconds);  // 8030821C
  int StopVibration(unsigned char port);  // 80308278
  void StopAllVibration();  // 803082C4
  int UpdateVibration();  // 8030833C

  int m_enabled = 0;  // +0x00
  int m_paused = 0;   // +0x04
  // +0x08 two 0x44-byte port records; only +0x30 of each (+0x38 overall: the port may rumble) is used.
  struct Port {
    unsigned char m_unused[0x30] = {};
    int m_on = 0;     // +0x30
    unsigned char m_unused2[0x10] = {};
  } m_ports[2];
};
