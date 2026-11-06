// @class R2LTarget after UIObjectBase UIReflow UIDialog HelpDialog
// The pause menu's relationships submenu (game/targets_s2c.cpp R2LTarget, screen 0x22 "R2L", flow 0x23
// from the pause menu's fourth tab): the selected sim's relationships as a grid of six per page (the
// sim's thumbnail and a face for the relationship level, the value as text), sorted by name; the
// selected one's name, description (story sims), value and level, and its large portrait.
#pragma once

#include <vector>

// PC accessor for the world side: one neighbour player `player`'s selected sim has a relationship
// with, as the panel lists it. GC: HoodManager::GetRelatedPeople(player, person) minus the
// neighbourhood's list 0x13 (Neighborhood vtable 0x124, contains-GUID vtable 0x5C), then per neighbour
// Neighbor::GetRelationship (the value, 0 without one), Neighbor::IsMarriedTo and
// Neighbor::CalculateRelationshipLevel against the player's selected neighbour (-1 none, else an
// index into R2L_IconFaceShaders: 2 archenemy, 3 enemy, 4 neutral, 6 friend, 7 best friend, 8 crush,
// 9 lover).
struct RelatedPerson {
  Neighbor* neighbor = nullptr;
  int value = 0;
  int married = 0;
  int level = -1;
};
// None while the relationships are not ported (SIMMY_STUB).
std::vector<RelatedPerson> PlayerRelatedPeople(int player, cXPerson* person);

// 0x1EC bytes.
struct R2LTarget : UIObjectBase {
  explicit R2LTarget(int player);  // 801DA6F0
  ~R2LTarget() override;  // 801DAF90
  void SetVariable(char* name, char* value) override;  // 801DB22C
  char* GetVariable(char* name) override;  // 801DB534
  unsigned short* GetLocalizable(char* name) override;  // 801DB6C4
  void Update() override;  // 801DBB98
  void SpawnSummaryDialog();  // 801DBB9C
  void SetupRelationships();  // 801DBE40
  void SetupPageShaders();  // 801DC1B4
  void ChangeSelection(int record);  // 801DC7A8
  int RecordIndexToGridIndex(int record);  // 801DCD68
  int GridIndexToRecordIndex(int cell);  // 801DCD88
  int OldGridIndexToRecordIndex(int cell);  // 801DCDB4
  int ScrollLeftEnabled();  // 801DCDE8
  int ScrollRightEnabled();  // 801DCE00

  // the help and summary dialogs
  void onHelpDialogSelection(int);  // 801DCE28
  void onSummaryDialogSelection(int);  // 801DCE2C
  void onSummaryDialogZeroInputCallback();  // 801DCE78
  void onSummaryDialogDialogClosedCallback();  // 801DCE84

  // One listed relationship (0x1C bytes).
  struct Record {
    unsigned int guid = 0;                 // +0x00
    Neighbor* neighbor = nullptr;          // +0x04
    unsigned int thumbnail = 0;            // +0x08 (GC: an ERShader ref from ObjSelector::GetThumbnail)
    std::vector<unsigned short> first;     // +0x0C BString2
    std::vector<unsigned short> last;      // +0x10 BString2
    int value = 0;                         // +0x14
    int married = 0;                       // +0x18
    int level = -1;                        // PC: CalculateRelationshipLevel (GC: asked again when drawn)
  };

  int m_exists = 0;                  // +0x88
  unsigned int m_filters[2] = {};    // +0x8C pushAptButtonFilterALL per player
  int m_player = 0;                  // +0x94
  std::vector<Record> m_records;     // +0x98 / +0x9C count
  int m_selected = -1;               // +0xA0 record
  int m_page = 0;                    // +0xA4 six records a page
  int m_oldPage = -1;                // +0xA8 the page scrolled away from
  std::vector<unsigned short> m_description;  // +0xAC the selected story sim's description
  int m_value = 0;                   // +0xB0 the selected one's relationship value
  int m_level = 4;                   // +0xB4 its level
  int m_dialogActive = 0;            // +0xB8 the summary dialog is up
  UIDialog m_dialog;                 // +0xBC the summary dialog
  HelpDialog m_help;                 // +0x144 (its +0x88, R2L +0x1CC: the help is up)
  UIReflow m_reflow;                 // +0x1D4
};
