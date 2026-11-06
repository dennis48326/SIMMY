// @class EControllerManager after EController
// The four controllers and the player -> controller mapping (engine/e_ctrlmanager.cpp).
#pragma once

struct EControllerManager {
  using MaskFn = EController::MaskFn;

  EControllerManager();
  virtual ~EControllerManager();
  virtual void Update();
  virtual int Init();
  virtual void Shutdown();
  virtual void Flush();

  void MapPlayerToController(unsigned int player, unsigned int controller);
  void SwapPlayerControls(unsigned int a, unsigned int b);
  void UnmapPlayer(unsigned int player);
  int GetPlayerMapped(unsigned int player);
  EController* GetController(unsigned int index);
  int GetPlayerControllerIndex(unsigned int player);
  EController* GetPlayerController(unsigned int player);
  int FindActiveController();
  int GetCmdResultAllCtrl(unsigned int command, MaskFn anyMask, MaskFn allMask, bool exclusive,
                          unsigned int* foundIndex, unsigned int filterId);
  int GetBtnResultAllCtrl(MaskFn mask, bool exclusive, bool any, unsigned int buttons, unsigned int* foundIndex,
                          unsigned int filterId);
  void MapControllerCommands(EBtnToCmdAssoc* map, int count);
  static int GetControllerCount();
  int GetCmdDownAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdPressedAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdReleasedAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdUpAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdRepeatAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnDownAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnPressedAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnReleasedAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnUpAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnRepeatAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnDownAnyAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnPressedAnyAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnReleasedAnyAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnUpAnyAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnRepeatAnyAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdDownExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdPressedExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdReleasedExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdUpExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetCmdRepeatExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnDownExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnPressedExlAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnReleasedExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnUpExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnRepeatExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnDownAnyExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnPressedAnyExlAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnReleasedAnyExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnUpAnyExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int GetBtnRepeatAnyExclAllCtrl(unsigned int, unsigned int*, unsigned int);
  int BitToIndex(unsigned int bit);
  static int IndexToBit(int index);

  static EController* _ctrlPads[4];            // 804FCF78
  static float _ctrlLastActivetime[4][2];      // 804B5FB8 seconds on _sysclock

  unsigned int m_playerController[4] = {};  // 0x00
  int m_playerMapped[4] = {};               // 0x10
};
