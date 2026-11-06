// @class BBHTarget__eBBHMode
// @class BBHTarget after UIObjectBase FCMTarget
// The build/buy HUD (game/targets_s2c.cpp BBHTarget, screen 0x29 "BBH"), made by the buy/build catalogue
// (PCTTarget) for its player: shown while an object or tool is in use (ShowBuildBuyHUD), it fades in and
// out (tweens with the BBH_show/hide callbacks) and shows the price of what is being placed (BBH_price),
// the family's money (BBH_money, from UIDB "simoleons"), the mode or tool (BBH_mode: place, grab, wall,
// floor, wallpaper and their removal) and the selling pig (BBH_piggy) when a grabbed object would be
// sold, with its fire code meter (FCMTarget).
#pragma once

#include "simmy/enum.hpp"

// BBHTarget::eBBHMode: 0 place, 1 grab, 2 wall, 3 floor, 4 wallpaper, 5 wall remove, 6 floor remove,
// 7 wallpaper remove (the BBH_mode text).
struct BBHTarget__eBBHMode {
  SIMMY_ENUM_BODY(BBHTarget__eBBHMode)
};

// 0xA8 bytes.
struct BBHTarget : UIObjectBase {
  using eBBHMode = ::BBHTarget__eBBHMode;
  explicit BBHTarget(int player);  // 801A31C4
  ~BBHTarget() override;  // 801A3788
  void SetVariable(char* name, char* value) override;  // 801A390C
  char* GetVariable(char* name) override;  // 801A39BC
  unsigned short* GetLocalizable(char* name) override;  // 801A3A54
  void Update() override;  // 801A3CC0
  void SetCurrentPrice(int price);  // 801A3E30
  int IsBuildBuyHUDActive();  // 801A3E94
  void ShowBuildBuyHUD();  // 801A3E9C
  void HideBuildBuyHUD();  // 801A3EA8
  void UpdateBuildBuyHUD();  // 801A3EB4
  void SetSellingPig(bool on);  // 801A3FB4
  void SetBBHMode(BBHTarget__eBBHMode mode);  // 801A4018

  int m_exists = 0;      // +0x84 the movie asked BBH_exists
  int m_active = 0;      // +0x88 shown (after the show tween)
  int m_tweening = 0;    // +0x8C a show/hide tween runs
  int m_wantShown = 0;   // +0x90 ShowBuildBuyHUD / HideBuildBuyHUD
  int m_player = 0;      // +0x94
  int m_price = 0;       // +0x98
  int m_sellingPig = 0;  // +0x9C
  int m_mode = 0;        // +0xA0 eBBHMode
  FCMTarget* m_fcm = nullptr;  // +0xA4 the fire code meter
};
