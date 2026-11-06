// @class UIAnalog after UIObjectBase
// The analog sticks for the Apt movies (game/uisupport.cpp): "0LeftAnalog_X" .. "1RightAnalog_Y" read
// player 1's or 2's stick through the screen's current button filter (CAS's morph sliders use it).
#pragma once

// 0x84 bytes (EGlobal +0x134).
struct UIAnalog : UIObjectBase {
  UIAnalog();  // 80180994
  ~UIAnalog() override;  // 80180ED4
  char* GetVariable(char* name) override;  // 80181018
};
