// @class FCMTarget after UIObjectBase
// The fire code meter (game/targets_s2c.cpp FCMTarget, screen 0x40 "FCM"): a bar beside the build/buy
// HUD (BBHTarget makes one and shows, hides and updates it with itself) filled by how close the lot is to
// the fire code's limit (GetFireCodeValue, in percent: FCM_fullbar's height out of 166 pixels with its
// cap above), slid in from the right; past 96 % the fire icon (FCM_fire) fades in. The meter tweens to a
// new level (6 ms a percent more than 33 ms) and does not move again until it got there (the
// tween_zenith_FCM callback).
#pragma once

// 0xAC + UIReflow bytes.
struct FCMTarget : UIObjectBase {
  FCMTarget();  // 801B34C0
  ~FCMTarget() override;  // 801B3B68
  void SetVariable(char* name, char* value) override;  // 801B3CC8
  char* GetVariable(char* name) override;  // 801B3DD0
  unsigned short* GetLocalizable(char* name) override;  // 801B3E6C
  void Update() override;  // 801B3EF0
  void ShowFireCodeMeter();  // 801B3FF8
  void HideFireCodeMeter();  // 801B41B8
  void UpdateFireCodeMeter();  // 801B41C4
  static int GetFireCodeMeterLevel();  // 801B441C
  int IsFireCodeMeterActive();  // 801B4458
  void SetMeterHeight(int level);  // 801B4460

  int m_exists = 0;       // +0x84 the movie asked FCM_exists
  int m_active = 0;       // +0x88 shown (after the show tween)
  int m_tweening = 0;     // +0x8C a show/hide tween runs
  int m_wantShown = 0;    // +0x90 ShowFireCodeMeter / HideFireCodeMeter
  int m_fireTweening = 0; // +0x94 the fire icon's tween runs
  int m_fireShown = 0;    // +0x98 the fire icon is up
  int m_barTweening = 0;  // +0x9C the bar's tween runs
  int m_level = 0;        // +0xA0 the level asked for (0..100)
  int m_shownLevel = 0;   // +0xA4 the level the bar shows
};
