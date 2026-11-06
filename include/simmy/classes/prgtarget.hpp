// @class PRGTarget after UIObjectBase
// The pause menu's "Progress" submenu (game/targets_s2c.cpp PRGTarget, screen 0x28 "PRG", flow 0x1D
// from the pause menu): the player family's aspiration points and nine progress rows (unlocked
// fashions, recipes, objects, promotions, maxed careers, skill points, maxed skills, NPCs helped,
// unlocked lots), each as "n/max" and a percentage, and their overall percentage.
#pragma once

// 0x94 bytes.
struct PRGTarget : UIObjectBase {
  explicit PRGTarget(int player);  // 801D8768
  ~PRGTarget() override;  // 801D9098
  void SetVariable(char* name, char* value) override;  // 801D9228
  void FormatFraction(unsigned short* out, int value, int max);  // 801D92D0
  void FormatPercentage(unsigned short* out, int value, int max);  // 801D9330
  int GetUnlockedFashions();  // 801D9398
  int GetUnlockedRecipies();  // 801D9408
  int GetUnlockedObjects();  // 801D9478
  int GetTotalPromotions();  // 801D94E8
  int GetMaxedCareers();  // 801D9578
  int GetTotalSkillPoints();  // 801D9604
  int GetMaxedSkills();  // 801D97DC
  int GetNPCsHelped();  // 801D9944
  int GetUnlockedLots();  // 801D99A0
  unsigned short* GetLocalizable(char* name) override;  // 801D9A10
  char* GetVariable(char* name) override;  // 801DA538
  void OnCancelKeyPressed(char* name, char* value);  // 801DA5D4
  void UpdateShaders();  // 801DA620
  void Shutdown();  // 801DA624

  int m_exists = 0;               // +0x84
  int m_player = 0;               // +0x88
  unsigned int m_filters[2] = {}; // +0x8C pushAptButtonFilterALL per player
};
