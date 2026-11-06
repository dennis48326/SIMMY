// @class MDITarget__ModelessDialog
// @class MDITarget after UIObjectBase
// @class ITBTarget after UIObjectBase
// The HUD's modeless dialogs (game/targets_s2c.cpp): MDITarget (screen 0x42 "MDI",
// s2c_comp_dialogbox.swf) shows the tutorial's hints one after the other in a box at the screen's
// top, each for a time that grows with its text's size, while no modal dialog (G2D) is up; ITBTarget
// (screen 0x41 "ITB") is the intro thought balloon, a bubble that grows out of the sim with a short
// text. H2DTarget owns both (+0xC8 MDITarget, +0xC4 ITBTarget) and hands them the dialogs the trees'
// dialog primitive (EdithDialogPrimitive, kinds 1/2 and 5) makes.
#pragma once

#include <vector>

// 0x24 bytes: one queued hint.
struct MDITarget__ModelessDialog {
  std::vector<unsigned short> m_body;    // +0x00 BString2
  std::vector<unsigned short> m_title;   // +0x04 BString2
  ERShader* m_icon = nullptr;            // +0x08 the dialog's icon (GC: or +0x0C a shader id, AddRefAsync)
  ERShader* m_iconBg = nullptr;          // +0x10 the icon's backdrop
  int m_iconW = 0;                       // +0x14
  int m_iconH = 0;                       // +0x18
  int m_timeMs = 0;                      // +0x1C 1 (no time given) or 2 (the dialog's own), then the ms left
  MDITarget__ModelessDialog* m_next = nullptr;  // +0x20
};

// 0xA0 bytes.
struct MDITarget : UIObjectBase {
  using ModelessDialog = MDITarget__ModelessDialog;
  MDITarget();  // 801C2FCC
  ~MDITarget() override;  // 801C35DC
  void SetVariable(char* name, char* value) override;  // 801C3734
  char* GetVariable(char* name) override;  // 801C37E4
  unsigned short* GetLocalizable(char* name) override;  // 801C3918
  void Update() override;  // 801C3A24
  void ShowModelessDialog();  // 801C3C84
  void HideModelessDialog();  // 801C3C90
  static void UpdateModelessDialogs();  // 801C3C9C
  int IsModelessDialogActive();  // 801C3CA0
  void SpawnModelessDialog(UIDialog* dialog);  // 801C3CA8
  void AddToList(ModelessDialog* dialog);  // 801C3DA0
  int CountModelessDialogs();  // 801C3DF0
  void DeleteAllModelessDialogs();  // 801C3DF8
  void CleanupModelessDialogs();  // 801C3EC4
  void SetupModelessDialog();  // 801C3EE4

  ModelessDialog* m_head = nullptr;  // +0x84 the one shown, then the queue
  int m_count = 0;                   // +0x88
  int m_boxHeight = 0x40;            // +0x8C "MDI_swf_boxHeight"
  int m_exists = 0;                  // +0x90 the screen is loaded
  int m_visible = 0;                 // +0x94
  int m_tweening = 0;                // +0x98 a show/hide tween is running
  int m_show = 0;                    // +0x9C a dialog is set up to be shown
};

// 0xA0 bytes.
struct ITBTarget : UIObjectBase {
  ITBTarget();  // 801BCA3C
  ~ITBTarget() override;  // 801BD088
  void SetVariable(char* name, char* value) override;  // 801BD1F8
  char* GetVariable(char* name) override;  // 801BD32C
  unsigned short* GetLocalizable(char* name) override;  // 801BD3C8
  void Update() override;  // 801BD490
  void ShowIntroThoughtBalloon();  // 801BD870
  void HideIntroThoughtBalloon();  // 801BD87C
  void UpdateIntroThoughtBalloon();  // 801BD888
  int IsIntroThoughtBalloonActive();  // 801BD8F4
  void SpawnIntroThoughtBalloon(UIDialog* dialog);  // 801BD8FC
  double GetUserReadTime();  // 801BDA50

  int m_exists = 0;                      // +0x84
  int m_active = 0;                      // +0x88
  std::vector<unsigned short> m_text;    // +0x8C BString2*
  int m_stage = 0;                       // +0x90 the bubble's grow-in step (1..4, 0 idle)
  int m_tweening = 0;                    // +0x94
  int m_show = 0;                        // +0x98
  float m_readTime = 0.0f;               // +0x9C seconds left to read it (counts on below 0 to -60)
};

// H2DTarget's ITBTarget (+0xC4; PC: kept by mditarget.cpp) and its per-frame part of H2DTarget::Update
// (801B8484): the balloon's read time, and the balloon deleted once read for 5 s more.
void H2DUpdateIntroThoughtBalloon();
void H2DDeleteDialogTargets(H2DTarget* h2d);
// The reward moment dialog's parts of H2DTarget (rmdtarget.cpp; GC H2DTarget +0xCC..+0xE0): Update's
// (a closed one deleted, a waiting one made), IsDialogActive's, the destructor's.
void H2DUpdateRewardMoment();
bool H2DRewardMomentActive();
void H2DDeleteRewardMoment();
