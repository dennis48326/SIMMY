// @class EController after EControllerData EBtnToCmdAssoc
// One controller port (engine/e_ctrl.cpp): button/stick state, input filters, command map,
// auto-repeat and rumble. Button bits are EA's, not the SDK's (see ENgcController::GetButtons).
#pragma once

struct EControllerFilter {
  unsigned int m_id;
  unsigned int m_priority;
  unsigned int m_mask;  // buttons hidden from everyone below this filter
};

struct EController {
  using MaskFn = unsigned int (EController::*)(unsigned int filterId);

  EController();
  // Virtuals, in vtable order.
  virtual ~EController();
  virtual int GetStatus();
  virtual int IsStable();
  virtual int HasVibration() const;
  virtual int VibrateMotorOne(float strength);
  virtual int VibrateMotorOne(float strength, float seconds);
  virtual int VibrateMotorTwo(float strength);
  virtual int VibrateMotorTwo(float strength, float seconds);
  virtual int VibrateAll(float strength1, float strength2);
  virtual bool VibrateAll(float strength1, float strength2, float seconds1, float seconds2);
  virtual int StopMotorOne();
  virtual int StopMotorOne(float seconds);
  virtual bool StopMotorTwo();
  virtual int StopMotorTwo(float seconds);
  virtual int StopVibration();
  virtual int UpdateVibration();
  virtual bool IsMotorOneActive() const;
  virtual bool IsMotorTwoActive() const;
  virtual void PauseMotors();
  virtual void ResumeMotors();
  virtual void Flush();
  virtual void Update();

  void UpdateAutoRepeat();
  void ClearAllData();
  void ClearAppData(bool stable);
  void SetCommandMap(EBtnToCmdAssoc* map, int count);
  void OverrideStatus(int status);
  void SwapAxes(int stick);
  void InvertAxis(int stick, int axis);
  int IsInUse();
  void ResetControllerData(bool stable);
  void UpdateButtonData(unsigned int buttons);
  void UpdateStickData(int stick, int axis, float value);
  void AddPressedEventData(unsigned int button);
  void AddReleasedEventData(unsigned int button);
  int IsControlFake();
  void SetControlFake(bool fake);
  void SetFakeButtons(unsigned int buttons);
  double GetStick(int stick, int axis, unsigned int filterId);
  int IsStickFiltered(int stick, int axis, unsigned int filterId);
  bool StickMoved(int stick, float threshold, unsigned int filterId);
  double GetLastStick(int stick, int axis, unsigned int filterId);
  void SetBtnExcl();
  int GetBtnResult(unsigned int mask, bool exclusive, unsigned int buttons);
  int GetBtnResultAny(unsigned int mask, bool exclusive, unsigned int buttons);
  EBtnToCmdAssoc* FindFirstCommand(unsigned int command);
  int GetCmdResult(unsigned int command, unsigned int anyMask, unsigned int allMask, bool exclusive);
  unsigned int AddFilter(unsigned int mask, char* name, int priority);
  void RemoveFilter(unsigned int id);
  void MoveFilterToTop(unsigned int id);
  void SetFilter(unsigned int id, unsigned int mask);
  void AddCmdToFilter(unsigned int id, unsigned int command);
  void AddCmdsToFilter(unsigned int id, unsigned int* commands, int count);
  void RemoveCmdFromFilter(unsigned int id, unsigned int command);
  void AddBtnToFilter(unsigned int id, unsigned int buttons);
  void RemoveBtnFromFilter(unsigned int id, unsigned int buttons);
  unsigned int GetCmdButtonMask(unsigned int command);
  int GetFilterIndex(unsigned int id);
  unsigned int GetFilterMask(unsigned int id);
  unsigned int GetBtnDownMask(unsigned int filterId);
  unsigned int GetBtnUpMask(unsigned int filterId);
  unsigned int GetBtnPressedMask(unsigned int filterId);
  unsigned int GetBtnReleasedMask(unsigned int filterId);
  unsigned int GetBtnRepeatMask(unsigned int filterId);
  int GetId();
  int GetCmdDown(unsigned int command, unsigned int filterId);
  int GetCmdPressed(unsigned int command, unsigned int filterId);
  int GetCmdReleased(unsigned int command, unsigned int filterId);
  int GetCmdUp(unsigned int command, unsigned int filterId);
  int GetCmdRepeat(unsigned int command, unsigned int filterId);
  int GetBtnDown(unsigned int buttons, unsigned int filterId);
  int GetBtnDownAny(unsigned int buttons, unsigned int filterId);
  int GetBtnUp(unsigned int buttons, unsigned int filterId);
  int GetBtnUpAny(unsigned int buttons, unsigned int filterId);
  int GetBtnPressed(unsigned int buttons, unsigned int filterId);
  int GetBtnPressedAny(unsigned int buttons, unsigned int filterId);
  int GetBtnReleased(unsigned int buttons, unsigned int filterId);
  int GetBtnReleasedAny(unsigned int buttons, unsigned int filterId);
  int GetBtnRepeat(unsigned int buttons, unsigned int filterId);
  int GetBtnRepeatAny(unsigned int buttons, unsigned int filterId);
  int AreAxesSwapped(int stick);
  int GetDirectionModifier(int stick, int axis);

  static int _nCtrlPads;  // 805005DC, ids handed out by the constructor

  // Offsets are the GameCube layout (vtable pointer at 0x208).
  int m_id = 0;                          // 0x000
  unsigned int m_status = 0;             // 0x004 bit 0 connected, bit 1 stable, bit 2 ?
  int m_topFilter = -1;                  // 0x008 index of the highest filter, -1 = none
  EControllerFilter m_filters[32] = {};  // 0x00C sorted by priority, highest last
  int m_axesSwapped[2] = {};             // 0x18C per stick
  int m_dirModifier[2][2] = {};          // 0x194 +1 / -1 per stick axis
  EBtnToCmdAssoc* m_cmdMap = nullptr;    // 0x1A4
  int m_cmdMapCount = 0;                 // 0x1A8
  int m_fake = 0;                        // 0x1AC
  unsigned int m_fakeButtons = 0;        // 0x1B0
  unsigned int m_repeat = 0;             // 0x1B4 buttons that auto-repeated this frame
  unsigned char m_repeatCounter[32] = {};  // 0x1B8
  unsigned char m_repeatDelay = 10;      // 0x1D8 frames before the first repeat
  unsigned char m_repeatRate = 3;        // 0x1D9 frames between repeats
  float m_motor[2] = {};                 // 0x1DC strength
  float m_motorTime[2] = {};             // 0x1E4 seconds left, 0 = until stopped
  float m_pausedMotor[2] = {};           // 0x1EC
  float m_pausedMotorTime[2] = {};       // 0x1F4
  EControllerData* m_raw = nullptr;      // 0x1FC accumulated from the hardware
  EControllerData* m_app = nullptr;      // 0x200 what the game reads this frame
  int m_active = 1;                      // 0x204 used within the last 30 seconds
};
