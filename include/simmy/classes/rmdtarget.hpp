// @class RMDTarget after UIObjectBase
// The reward moment dialog (game/targets_s2c.cpp, screen 0x43 "RMD"): a box with a title, the reward's
// 3D model and a button text, for a new recipe, outfit, lot or object unlocked (or a congratulation);
// it holds the game until A (or after 60 s). H2DTarget makes it (SpawnRewardMomentDialog, then its
// Update) and deletes it once it has closed (RewardMomentDialogClosed). See rmdtarget.cpp.
#pragma once
#include <vector>

// The dialog (0xB0 bytes on GC).
struct RMDTarget : UIObjectBase {
  RMDTarget(int kind, int index);  // 801DEDE8
  ~RMDTarget() override;  // 801DF648
  void SetVariable(char* name, char* value) override;  // 801DF838
  char* GetVariable(char* name) override;  // 801DF994
  unsigned short* GetLocalizable(char* name) override;  // 801DFA2C
  void Update() override;  // 801DFB1C
  void SetupWidgets();  // 801DFB98
  void UnloadDialog();  // 801DFFA4
  void StartShowTween();  // 801E014C
  void StartHideTween();  // 801E022C

  int m_exists = 0;                   // +0x84 the movie asked RMD_exists (its screen is up)
  int m_state = 0;                    // +0x88 0 new, 1 showing, 2 shown, 3 hiding, 4 unloaded
  unsigned int m_filters[2] = {};     // +0x8C
  int m_paused = 0;                   // +0x94 holds the game (H2DTarget::IncPause)
  int m_kind = 0;                     // +0x98
  int m_index = 0;                    // +0x9C
  std::vector<unsigned short> m_title;   // +0xA0
  std::vector<unsigned short> m_button;  // +0xA4
  float m_time = 60.0f;               // +0xA8 seconds until it closes by itself (1 with DAT_80476120)
  int m_timedOut = 0;                 // +0xAC
};

