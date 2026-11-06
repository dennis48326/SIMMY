// @class FAMTarget after UIObjectBase UIDialog
// @class PreGameFreeplayState after StateMachineState FAMTarget
// @class PreGameCreateAFamilyState after CreateASimBaseState
// Free play's neighbourhood screen (FAMTarget, game/targets_s2c.cpp; screen 0x44 "FAM",
// s2c_main_families) and the pre-game states around it (game/pregamestate.cpp).
//
// FAMTarget walks a menu tree (FAM_STATE; the movie calls "updateStateFAM" after each change):
//   0 the four free-play lots (game lots 13..16): X opens the lot;
//   1 an occupied lot: 0 enter the house (LaunchLot: "INLEVEL"), 1 evict the family (yes/no);
//   2 an empty lot: 0 move a family in (state 4), 1 bulldoze (yes/no);
//   3 a family without a house: 0 edit it (Create-a-Family, "CAF_"), 1 delete it (yes/no), 2 move it
//     in (HoodManager::TestMoveInFamily / MoveInFamily);
//   4 the eight free-play families (family numbers 20..27): an empty slot makes a new family and
//     goes to Create-a-Family; a family with a house is just picked, one without opens state 3.
// B goes up the tree; at the top it asks to go back to the main menu ("CASBackoutP1"). The lot
// changes (moving in, evicting, bulldozing) are HoodManager's (the world side of moving).
//
// PreGameFreeplayState (state 9) holds the target while free play is set up: "INLEVEL" loads the
// chosen lot into live mode (state 13), "CAF_" calls Create-a-Family (PreGameCreateAFamilyState,
// state 12) behind the loading screen, "CASBackoutP1" returns to the main menu.
#pragma once
#include <deque>

struct FAMTarget : UIObjectBase {
  enum FAM_STATE : int { kLotSelect = 0, kOccupied = 1, kUnoccupied = 2, kManageFamily = 3, kFamilySelect = 4 };

  FAMTarget();  // 801AE88C
  ~FAMTarget() override;  // 801AFB4C
  void UnloadScreen();  // 801AFCAC
  void SetVariable(char* name, char* value) override;  // 801AFCD8
  unsigned short* GetLocalizable(char* name) override;  // 801AFEB8
  char* GetVariable(char* name) override;  // 801B05E0
  void Shutdown();  // 801B070C
  void Update() override;  // 801B0710
  void UpdateMenuState(char* name, char* value);  // 801B0714
  void UpdateText();  // 801B0750
  void OnLeftKeyPressed(char* name, char* value);  // 801B075C
  void OnRightKeyPressed(char* name, char* value);  // 801B0760
  void UpdateShaders();  // 801B0764
  void UpdateOccupiedShaders();  // 801B07B0
  void UpdateLotSelectShaders();  // 801B0950
  void UpdateTempShaders();  // 801B0B18
  void GetFamilyText(char* name, unsigned short* out);  // 801B0B78
  void OnLotSelect();  // 801B0CCC
  FamilyImpl* GetFamilyInFamilyList(int index);  // 801B0D1C
  bool DoesFamilyExistInFamilyChoice(int index);  // 801B0D5C
  int ConvertSelectedFamilyIndexToActualGameIndex(int index);  // 801B0DAC
  int GetNumberOfFamilyMembers(Family* family);  // 801B0DB4
  int IsSelectedLotOccupied();  // 801B0DF8
  void OnCancelDialog();  // 801B0E6C
  void OnDialogSelection(int selection);  // 801B0E70
  void OnReturnFromCreateAFamily(bool canceled);  // 801B1078
  void OnBackoutToMainMenuDialogClose(int selection);  // 801B1128
  void OnCancelKeyPressed(char* name, char* value);  // 801B1158
  void OnUpKeyPressed(char* name, char* value);  // 801B13DC
  void OnDownKeyPressed(char* name, char* value);  // 801B1500
  void OnXKeyPressed(char* name, char* value);  // 801B1640
  void OnCircleKeyPressed(char* name, char* value);  // 801B2604
  void UpdateMenuState(int state);  // 801B2608
  int GetNumberOfChoicesInState(int state);  // 801B2994
  int ConvertSelectedLotIndexToActualGameIndex(int index);  // 801B29F0
  void GoUpMenuTree();  // 801B29F8
  void GetNameForFamilyInLotSelect(char* name, unsigned short* out);  // 801B2B00
  int GetMoneyForFamilyInLotSelect(char* name, unsigned short* out);  // 801B2C14
  void UpdateFamilySelectShaders();  // 801B2CD4
  void GetSelectedLotDescription(unsigned short* out);  // 801B2EC0
  void ConvertNumericMoneyToString(int amount, unsigned short* out);  // 801B2F28
  int GetMoneyForFamily(Family* family, unsigned short* out);  // 801B2F8C
  const unsigned short* GetLotName(int lot);  // 801B3024
  void GetNameForFamily(Family* family, unsigned short* out);  // 801B3074
  void SetCurrentChoice(char* value);  // 801B3130
  FamilyImpl* GetFamilyInLot(int index);  // 801B3204
  void LaunchLot();  // 801B3244
  int GetMoneyForLot(int lot);  // 801B3284
  void EvictFamily();  // 801B3314
  int GetLotForFamily(Family* family);  // 801B3388
  int GetLotForFamily(int index);  // 801B33CC
  const unsigned short* GetLotNameInFamilySelect(char* name);  // 801B340C

  // Shared by the yes/no/ok dialogs of OnXKeyPressed / OnCancelKeyPressed (GC: inline at each).
  void SpawnFamDialog(int buttons, const char* const* selections, const unsigned short* body, bool backout);

  int m_exists = 0;                 // +0x84 the movie asked FAM_exists
  int m_state = kLotSelect;         // +0x88 FAM_STATE
  int m_prevState = 0;              // +0x8C
  std::deque<int> m_stateStack;     // +0x90 the path down the tree
  int m_pushState = 1;              // +0xC0 push the new state (0 when going up)
  UIDialog m_dialog;                // +0xC4
  int m_lot = 0;                    // +0x14C the lot choice (state 0)
  int m_familyChoice = 0;           // +0x150 (state 4)
  int m_numLots = 4;                // +0x154
  int m_numFamilies = 8;            // +0x158
  int m_manageChoice = 0;           // +0x15C (state 3)
  int m_occupiedChoice = 0;         // +0x160 (state 1)
  int m_unoccupiedChoice = 0;       // +0x164 (state 2)
  int m_secondPage = 0;             // +0x168 the family list's second half is showing
  FamilyImpl* m_family = nullptr;   // +0x16C the family being looked at
};

struct PreGameFreeplayState : public StateMachineState {
  explicit PreGameFreeplayState(int id = -1);  // 80095408
  void Startup() override;  // 80094EEC
  void Shutdown() override;  // 8009501C
  void Reset() override;  // 80095060
  void Update(float dt) override;  // 8009508C
  int IsFlowStateRequestPending();  // 80095444
  int GetFlowStateRequest();  // 80095468
  void SetFlowStateCurrent();  // 8009548C

  FAMTarget* m_fam = nullptr;  // +0x1C
};

struct PreGameCreateAFamilyState : public CreateASimBaseState {
  explicit PreGameCreateAFamilyState(int id = -1);  // 80095370
  void Reset() override;  // 80094C6C
  void Update(float dt) override;  // 80094CA0
};
