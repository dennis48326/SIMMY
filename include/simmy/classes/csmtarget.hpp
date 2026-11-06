// @class CSMTarget after UIObjectBase
// The HUD's context menu widget ("CXM", screens 0x32/0x33; game/targets_s2c.cpp CSMTarget): the
// player's ActionMenu shows itself here (ActionMenu::Draw -> ShowCSIM + TempInterface): the object's
// name, three rows (the previous, current and next item), up/down arrows when rows are hidden, and the
// current row's icon. It fades in and out with the menu, and shows an hourglass while the sim is busy.
// GC also shows it, with "csm_more", while the selected sim is in a context-sim state (person vtables
// 0xAC/0x174/0x17C with the player's direct interactor): not ported (no such state here).
#pragma once

#include <string>

// 0xD8 bytes (H2DTarget +0x90 per player).
struct CSMTarget : UIObjectBase {
  explicit CSMTarget(int player);  // 801A61D0
  ~CSMTarget() override;  // 801A6980
  void SetVariable(char* name, char* value) override;  // 801A6B34
  char* GetVariable(char* name) override;  // 801A6C3C
  unsigned short* GetLocalizable(char* name) override;  // 801A6CD4
  void Update() override;  // 801A6E00
  void ShowCSIM();  // 801A764C
  void HideCSIM();  // 801A7660
  int IsCSIMActive() const { return m_shown; }  // 801A7674
  void TempInterface(const std::u16string& name, const std::u16string& option0, const std::u16string& option1,
                     const std::u16string& option2, bool upArrow, bool downArrow);  // 801A767C
  void SetIconDisable(bool enabled);  // 801A7A88

  int m_exists = 0;            // +0x84 the movie asked CXM_exists
  int m_shown = 0;             // +0x88
  int m_player = 0;            // +0x8C
  int m_wantShown = 0;         // +0x90 ShowCSIM/HideCSIM
  int m_tweening = 0;          // +0x94
  int m_contextShown = 0;      // +0x98 shown for the context sim (HideCSIM leaves it)
  int m_hourglassWanted = 0;   // +0x9C
  int m_hourglassTweening = 0; // +0xA0
  int m_hourglassShown = 0;    // +0xA4
  std::u16string m_objName;    // +0xB0 CXM_ObjName
  std::u16string m_options[6]; // +0xB4 CXM_Option<n> (0..2 shown)
  int m_upArrow = 0;           // +0xCC
  int m_downArrow = 0;         // +0xD0
  int m_iconEnabled = 1;       // +0xD4
};
