// @class ENgcController after EController
// A GameCube pad port (engine/e_ngcctrlmanager.cpp, inline functions).
#pragma once

struct ENgcController : public EController {
  ~ENgcController() override;
  int HasVibration() const override;
  int VibrateMotorOne(float strength) override;
  int VibrateMotorTwo(float strength) override;
  int StopMotorOne() override;
  bool StopMotorTwo() override;
  bool IsMotorOneActive() const override;
  bool IsMotorTwoActive() const override;
  using EController::VibrateMotorOne;
  using EController::VibrateMotorTwo;
  using EController::StopMotorOne;
  using EController::StopMotorTwo;

  unsigned int GetButtons(PADStatus& pad);
  double GetStick(PADStatus& pad, int stick, int axis);
  using EController::GetStick;
  void UpdateMotors();

  int m_port = 0;  // 0x20C PAD channel
};
