// @class WXFTarget after UIObjectBase HelpDialog
// The pause menu's "Goals: Wants & Fears" submenu (game/targets_s2c.cpp WXFTarget, screen 0x27 "WXF",
// flow 0x1A from the pause menu's first tab): two pages (wants and fears) of the selected sim's active
// wants and fears (icon backdrop by kind, icon, text) and its aspiration (kind, label, description).
// The wants and fears come from the neighbour's bookmarks (NeighborWantFearSlot, hudtarget.hpp).
#pragma once

// 0x134 bytes.
struct WXFTarget : UIObjectBase {
  explicit WXFTarget(int player);  // 801E4F2C
  ~WXFTarget() override;  // 801E5B14
  void SetVariable(char* name, char* value) override;  // 801E5CBC
  unsigned short* GetLocalizable(char* name) override;  // 801E5DDC
  char* GetVariable(char* name) override;  // 801E6194
  void Shutdown();  // 801E629C
  void Update() override;  // 801E62E8
  void UpdateMenuState(char* name, char* value);  // 801E62EC
  void UpdateText();  // 801E6370
  void OnLeftKeyPressed(char* name, char* value);  // 801E6454
  void OnRightKeyPressed(char* name, char* value);  // 801E64BC
  void OnCancelKeyPressed(char* name, char* value);  // 801E6524
  void UpdateShaders();  // 801E6570
  static unsigned int GetBackgroundShader(int type);  // 801E6B84 (tWantType)
  unsigned short* UpdateWAFText(char* name);  // 801E6BD8
  void onHelpDialogSelection(int choice);  // 801E6DF0
  void ShowHelp();  // 801E6E30
  int GetAspirationIcon();  // 801E6EC0

  int m_exists = 0;               // +0x88
  int m_state = 0;                // +0x8C 0 wants, 1 fears, 2 (the movie's third state)
  int m_prevState = 0;            // +0x90
  int m_player = 0;               // +0x94
  unsigned int m_filters[2] = {}; // +0x98 pushAptButtonFilterALL per player
  HelpDialog m_help;              // +0xA0 (its +0x88, WXF +0x128: a help page is up; closing and cancel wait)
  unsigned int m_helpPage = 0;    // +0x130
};
