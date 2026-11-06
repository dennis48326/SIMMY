// @class PAZItemInfo
// @class PAZBase after UIObjectBase PAZItemInfo
// @class PAZTarget after PAZBase
// The in-game pause menu (game/targets_s2c.cpp): PAZBase runs a row of tabs on a custom flash screen
// (the movie's "<prefix>_*" variables, named by a string table), PAZTarget is the root pause menu
// (screen 0x2C "PAZ", s2c_pause_mainmenu.swf): wants and fears, progress, career, relationships, buy,
// build, EyeToy and the game options. Choosing a tab asks the game for its flow ("PAUSE_*",
// InLevelBaseState::Update); cancelling goes back to the game ("ingame").
#pragma once

// One tab (0x1C bytes; sRootItems 803F715C).
struct PAZItemInfo {
  unsigned int index = 0;         // +0x00
  const char* name = nullptr;     // +0x04 the tab's PDAStrings row (GetPDAString)
  unsigned int icon = 0;          // +0x08 its icon shader
  int selectAction = 0;           // +0x0C what X does (the PAUSE_* flow, 0: nothing)
  int cancelAction = 0;           // +0x10 what B does (2 back to the game, 3 the root pause menu)
  int field14 = 0;                // +0x14
  int field18 = 0;                // +0x18
};

// 0xBC bytes.
struct PAZBase : UIObjectBase {
  // The movie's variable names (kRootMenuStrings 803F6E94: 0x19 entries).
  enum : int {
    kExists, kGetTabCount, kGetTabIndex, kGetEyeToyStatus, kGetBuildBuyStatus, kGetProgressStatus,
    kGetGameStatus, kXButton, kCancelButton, kDpadLeft, kDpadRight, kTabsReady, kSetCurrTab,
    kTabSelected, kCancel, kTabName, kIcon0,
  };
  PAZBase();  // 801D00E8
  ~PAZBase() override;  // 801D0190
  void SetVariable(char* name, char* value) override;  // 801D0318
  char* GetVariable(char* name) override;  // 801D0864
  unsigned short* GetLocalizable(char* name) override;  // 801D0A74
  void Update() override;  // 801D0B64
  int GetPausePlayer();  // 801D0B68
  static void UpdateButtonVisibility();  // 801D0B70
  int ConfigureAndLoad(int screen, char** strings, PAZItemInfo* items, unsigned int count);  // 801D0B74
  int on_user_set_selection_index(int index);  // 801D0ED4

  int m_player = 0;                   // +0x84 the player who paused
  int m_state = 1;                    // +0x88 2 once a tab was chosen or the menu cancelled
  unsigned int m_selected = 0;        // +0x8C the tab under the cursor
  unsigned int m_lastMoveMs = 0;      // +0x90 when the cursor last moved (200 ms apart)
  int m_field94 = 0;                  // +0x94
  int m_inputEnabled = 0;             // +0x98 the movie finished tweening in
  int m_cancelled = 1;                // +0xA0 the tween out ends in Cancel (1) or TabSelected (0)
  int m_screen = 0;                   // +0xA4
  PAZItemInfo* m_items = nullptr;     // +0xA8
  unsigned int m_count = 0;           // +0xAC
  char** m_strings = nullptr;         // +0xB0
  unsigned int m_filters[2] = {};     // +0xB4 each player's "PauseMenu" button filter
};

struct PAZTarget : PAZBase {
  explicit PAZTarget(int player);  // 801D1040
  ~PAZTarget() override;  // 801D1510

  // not ported yet
  virtual RET_UNKNOWN GetTLA() const;  // 801E70B8

  static unsigned int sInitialSelectedItem_Root;  // 804FE50C the tab to start on (the last one)
};
