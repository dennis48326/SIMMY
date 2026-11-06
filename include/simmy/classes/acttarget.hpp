// @class ACTTarget__ACTRecord
// @class ACTTarget after UIObjectBase ACTTarget__ACTRecord
// @class INTTarget after UIObjectBase
// The HUD's action queue widget (game/targets_s2c.cpp ACTTarget, screens 0x3C/0x3D "ACT") and the
// social interest meter (game/targets_tsc3.cpp INTTarget, screens 0x3E/0x3F "INT"), one of each per
// player, made by H2DTarget (+0xB4, +0xBC).
//
// ACT: the selected sim's queued interactions as a column of icons (ACT_item0..17: a frame and the
// object's icon shader each), the running one at the bottom (slot 0 at y 246, each next one 41 above,
// slots 7 and 8 pushed 128 to the side). The person's ActionQueue reports each visible change
// (ActionQueue::AddActionToHUD / RemoveActionFromHUD -> H2DTarget::AddAction / RemoveAction): an
// addition waits in a pending list until the next Update places it in a free record (tweened in from
// 128 to the right), a removal fades the record out (125 ms) and the others move down. Holding B
// (H2DTarget::Update, 30 frames) turns on cancel mode: a selector icon on the head item, d-pad up/down
// move it, A (H2D_SELECT) cancels the item under it (person vtable 0xBC RemoveAction; flag 0x100
// items refuse with a sound); letting go of B leaves it. A short B cancels the last action and tints
// its icon red (CancellingLastObject).
// The widget shows only while the player is not in direct control (the active interactor is not the
// type-0 DirectInteractor) and the sim is not in social mode: on GC that is the classic cursor
// control (SimInteractor, pad command 0x11), which the port does not have. PC development aid:
// SIMMY_ACT_SHOW=1 shows it in direct control too.
//
// INT: the relationship numbers of a social interaction (SetInteractionDisplayValues: the change and
// the relationship; TriggerInteractionAnimation: daily, lifetime, bonus and their sum), shown while the
// selected sim is in social mode and the simulator has given it values (objectsim's social primitive,
// the world side's).
#pragma once

#include <list>
#include <string>

// One icon of the queue (0x4C bytes, 18 at ACTTarget +0xEC).
struct ACTTarget__ACTRecord {
  int m_index = 0;              // +0x00
  int m_used = 0;               // +0x04
  int m_removing = 0;           // +0x08 fading out (freed when its tween ends)
  int m_slot = -1;              // +0x0C the position it is in (ReOrderActionQueue)
  int m_x = 0, m_y = 0;         // +0x10 where it is going
  int m_shownX = 0, m_shownY = 0;  // +0x18 where it was last sent
  int m_tweening = 0;           // +0x20
  const char* m_item = "";      // +0x24 "<p>ACT_item<n>" (its clip)
  const char* m_shader = "";    // +0x28 "<p>ACT_shdr<n>" (the icon)
  const char* m_frame = "";     // +0x2C "<p>ACT_frame<n>"
  const char* m_tween = "";     // +0x30 "<p>ACT_item_tween<n>" (the tweens' callback)
  std::u16string m_text;        // +0x34 (BString2, unused)
  int m_id = 0;                 // +0x38 the interaction's unique id
  Interaction* m_interaction = nullptr;  // +0x3C (found again by id when needed)
  cXPerson* m_person = nullptr; // +0x40
  ACTTarget__ACTRecord* m_next = nullptr;  // +0x44 the next one up the queue
  ACTTarget__ACTRecord* m_prev = nullptr;  // +0x48
};

// 0x658 bytes.
struct ACTTarget : UIObjectBase {
  using ACTRecord = ::ACTTarget__ACTRecord;
  // An addition waiting for Update (iqRecord, 0x20 bytes).
  struct iqRecord {
    cXPerson* person = nullptr;       // +0x00
    Interaction* interaction = nullptr;  // +0x04
    int id = 0;                       // +0x08
    std::u16string name;              // +0x0C
    unsigned int shader = 0;          // +0x10
    ERShader* resource = nullptr;     // +0x14
    unsigned int insertBefore = 0;    // +0x18 the unique id it goes in front of (0: at the end)
  };

  explicit ACTTarget(int player);  // 801A0968
  ~ACTTarget() override;  // 801A19B0
  void SetVariable(char* name, char* value) override;  // 801A1C1C
  char* GetVariable(char* name) override;  // 801A1D3C
  unsigned short* GetLocalizable(char* name) override;  // 801A1DD4
  void Update() override;  // 801A1E58
  void ShowActionQueue();  // 801A21EC
  void HideActionQueue();  // 801A21F8
  void UpdateActionQueue();  // 801A2204
  int IsActionQueueActive();  // 801A23BC
  void AddAction(int player, Interaction* interaction, const std::u16string& name, unsigned int shader,
                 ERShader* resource, unsigned int insertBefore);  // 801A23C4
  void PlaceItem(ACTRecord* record, int player, Interaction* interaction, int id, cXPerson* person,
                 const std::u16string& name, unsigned int shader, ERShader* resource,
                 unsigned int insertBefore);  // 801A25B8
  void RemoveAction(int player, Interaction* interaction);  // 801A2828
  void RemoveAllActions(int player);  // 801A2AAC
  int IsActionQueueAvailable(int player) const;  // 801A2B34
  ACTRecord* GetRecord();  // 801A2B3C
  void ReOrderActionQueue();  // 801A2BA4
  static void DebugListValidation();  // 801A2C30
  Interaction* GetInteractionFromID(int id);  // 801A2C34
  void MoveCancelCursorUp();  // 801A2CDC
  void MoveCancelCursorDown();  // 801A2D30
  int CancelModeEnabled(bool on);  // 801A2D78
  void CancelCurrentObject();  // 801A2E30
  void UpdateCancelModeWidgets();  // 801A2F98
  void ExitCancelMode();  // 801A30A0
  void CancellingLastObject();  // 801A311C

  struct Pos {
    float x = 0, y = 0;
  };
  Pos m_slots[9];                    // +0x84
  int m_exists = 0;                  // +0xCC the movie asked ACT_exists
  int m_shown = 0;                   // +0xD0
  int m_tweening = 0;                // +0xD4
  int m_wantShown = 0;               // +0xD8
  int m_player = 0;                  // +0xDC
  int m_x = 0;                       // +0xE0 player 1's widget moves 320 left in two-player mode
  ACTRecord* m_head = nullptr;       // +0xE4 the running action's icon
  ACTRecord* m_tail = nullptr;       // +0xE8
  ACTRecord m_records[18];           // +0xEC
  int m_cancelMode = 0;              // +0x644
  ACTRecord* m_cursor = nullptr;     // +0x648 the cancel selector's item
  ACTRecord* m_lastCursor = nullptr; // +0x64C (its tint is reset)
  std::list<iqRecord*> m_pending;    // +0x650
};

// 0xE0 bytes.
struct INTTarget : UIObjectBase {
  explicit INTTarget(int player);  // 801F8024
  ~INTTarget() override;  // 801F8720
  void SetVariable(char* name, char* value) override;  // 801F88D4
  char* GetVariable(char* name) override;  // 801F89E4
  unsigned short* GetLocalizable(char* name) override;  // 801F8A7C
  void Update() override;  // 801F8BC0
  void ShowInterestMeter();  // 801F8D98
  void HideInterestMeter();  // 801F8DA4
  void UpdateInterestMeter();  // 801F8DB0
  int IsInterestMeterActive();  // 801F8E8C
  void SetInteractionDisplayValues(unsigned int change, unsigned int relationship);  // 801F8E94
  void TriggerInteractionAnimation(unsigned int daily, unsigned int lifetime, unsigned int bonus);  // 801F8FDC
  void MirrorUpArrow(bool on);  // 801F9154

  int m_exists = 0;          // +0x84
  int m_shown = 0;           // +0x88
  int m_tweening = 0;        // +0x8C
  int m_animating = 0;       // +0x90 an "a1"/"a2" animation runs (INT_anim_end)
  int m_wantShown = 0;       // +0x94
  int m_player = 0;          // +0x98
  int m_hasValues = 0;       // +0x9C
  int m_change = 0;          // +0xA0
  int m_lifetime = 0;        // +0xA4
  int m_relationship = 0;    // +0xA8
  int m_sum = 0;             // +0xAC
  int m_bonus = 0;           // +0xB0
  std::u16string m_bonusText;  // +0xB4 INT_bonus ("+n" when not 0)
  std::u16string m_relData;    // +0xB8 INT_reldata
  std::u16string m_relText;    // +0xBC INT_rel
  std::u16string m_sumText;    // +0xC0 INT_sum
  std::u16string m_rel2Text;   // +0xC4 INT_rel2
  int m_upArrow = 0;         // +0xDC INT_CXM_arrow_up shown
};

// The ActionQueue's HUD side (game/person.cpp, ported in acttarget.cpp) and a PC stand-in for
// cXPersonImpl::AttachToHUD (80129804): a selected person's queue reports to its player's ACT.
void AttachPersonToHUD(cXPerson* person, bool attach);
