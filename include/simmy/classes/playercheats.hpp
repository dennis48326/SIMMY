// @class PlayerCheats after EClock EController
// The button-code cheat reader (game/appmain.cpp, `g_playerCheats` 8046E1E0, 0x28 bytes): a stopwatch
// (started at boot by ESimsApp::Init) and a ring of the last six single-button presses on player 1's
// pad. ESimsApp::UpdateCheats hands it every frame with a press; a press that completes one of the
// eleven codes in EGlobal's table (EGlobal::InitPlayerCheats) toggles that cheat.
#pragma once

struct PlayerCheats : public EClock {
  int Capture(EController* pad);  // 80005D40
  int GetNextIndex(int& index);  // 80005F60
  int IsSingleButton(unsigned short buttons);  // 80005F84
  void PurgeBtnMemory();  // 80005FD4
  unsigned short CreateBtnMask();  // 80005FFC

  // (+0x0..+0xF: the EClock)
  float m_lastPressMs = 0.0f;  // +0x10 the clock's time (ms) of the last accepted press
  int m_index = 0;  // +0x14 the ring slot the next press goes in
  int m_purge = 0;  // +0x18 the ring is cleared before the next press (a wrong button or a pause of 1 s)
  unsigned short m_buttons[6] = {};  // +0x1C the ring of presses (EA button bits)
};
