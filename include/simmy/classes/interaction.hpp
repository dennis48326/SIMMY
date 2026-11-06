// @class Interaction
// @class ActionQueue after Interaction
// A person's interactions (game/interaction.cpp, game/person.cpp): an Interaction is one entry of an
// object's tree table (TTAB) chosen for a person (from the action menu, an object's push primitive, or
// autonomy) with its priority and flags; the ActionQueue is the person's ring of ten of them (GC
// cXPersonImpl +0x130), the running one first. The HUD's action queue icons (AddActionToHUD,
// RemoveActionFromHUD) belong to the HUD and stay unported here.
#pragma once
#include <string>
#include <vector>

// 0x44 bytes.
struct Interaction {
  // m_flags (+0x3C).
  enum : unsigned int {
    kAutoFirstSelect = 1, kContinuation = 2, kChecked = 4, kAvailable = 8, kHidden = 0x10, kPushToHead = 0x20,
    kCarryNameOver = 0x40, kNameChanged = 0x80, kCancelled = 0x100, kDebugOverride = 0x200, kMustRun = 0x400,
    kDisabled = 0x800, kRunAsSub = 0x8000
  };

  Interaction& operator=(const Interaction& other);  // 800C1068
  Interaction();  // 800C1140
  Interaction(const Interaction& other);  // 800C11D0
  // The tree table entry `index` of `object` for `person`, as the action menu offers it: priority 100.
  Interaction(cXPerson* person, cXObject* object, int index);  // 800C122C
  Interaction(cXPerson *, cXPerson *);  // 800C12F8
  // The same with a priority; the entry's action tree, attenuation and flags (auto first select, must
  // run) are taken from the table.
  Interaction(cXPerson* person, cXObject* object, int index, int priority);  // 800C1574
  virtual ~Interaction();  // 800C171C
  void SetStackVars(short* vars);  // 800C1780
  void SetLocalVars(short *, int);  // 800C17A4
  // The stack object's tree table entry (TTAB, 0x24 bytes: +0 check tree, +2 action tree, +0x10 flags,
  // +0x1C name) or null.
  const unsigned char* GetEntry() const;  // 800C182C
  void SetUniqueID();  // 800C189C
  RET_UNKNOWN GetName() const;  // 800C18B0
  static void SetName(BString2 &);  // 800C1960
  void SetName(int, int);  // 800C199C
  // Its type, id, person, stack and icon objects (by id), entry, stack vars, priority, tree, attenuation
  // and flags. Read: the unique id counter moves past it.
  void DoStream(ReconBuffer* buffer, int version);  // 800C1A68
  cXObject* GetStackObject() const { return m_stackObject; }  // 800C1C74
  cXPerson* GetPerson() const { return m_person; }  // 800C1C7C
  cXObject* GetIconObject() const { return m_iconObject; }  // 800C1C84
  void SetIconObject(cXObject* object) { m_iconObject = object; }  // 800C1C8C
  virtual RET_UNKNOWN GetCTilePt() const;  // 800C1CBC
  RET_UNKNOWN GetTextColor() const;  // 800C1D20
  void SetTextColor(Interaction__tColor);  // 800C1D2C
  int GetType() { return m_type; }  // 800C1D44
  double GetAttenuation() const { return m_attenuation; }  // 800C1D4C
  short GetTreeID() const { return m_tree; }  // 800C1D54
  short* GetStackVars() { return m_stackVars; }  // 800C1D5C
  RET_UNKNOWN GetLocalVars();  // 800C1D64
  RET_UNKNOWN GetLocalVarsCount();  // 800C1D6C
  int GetTreeTabEntryIndex() const { return m_index; }  // 800C1D74
  int GetPriority() const { return m_priority; }  // 800C1D7C
  void SetPriority(int priority) { m_priority = priority; }  // 800C1D84
  RET_UNKNOWN HasID() const;  // 800C1D8C
  int GetID() const { return m_id; }  // 800C1DA4
  void SetID(int id) { m_id = id; }  // 800C1DAC
  RET_UNKNOWN GetAutoFirstSelect() const;  // 800C1DB4
  void SetAutoFirstSelect(bool);  // 800C1DC0
  RET_UNKNOWN GetContinuation() const;  // 800C1DE0
  void SetContinuation(bool);  // 800C1DEC
  RET_UNKNOWN GetChecked() const;  // 800C1E0C
  void SetChecked(bool);  // 800C1E18
  RET_UNKNOWN GetAvailable() const;  // 800C1E38
  void SetAvailable(bool);  // 800C1E44
  RET_UNKNOWN GetHidden() const;  // 800C1E64
  void SetHidden(bool);  // 800C1E70
  RET_UNKNOWN GetPushToHead() const;  // 800C1E90
  void SetPushToHead(bool);  // 800C1E9C
  RET_UNKNOWN GetCarryNameOver() const;  // 800C1EBC
  void SetCarryNameOver(bool);  // 800C1EC8
  RET_UNKNOWN GetNameChanged() const;  // 800C1EE8
  void SetNameChanged(bool);  // 800C1EF4
  RET_UNKNOWN GetCancelled() const;  // 800C1F14
  void SetCancelled(bool);  // 800C1F20
  RET_UNKNOWN GetDebugOverride() const;  // 800C1F40
  void SetDebugOverride(bool);  // 800C1F4C
  RET_UNKNOWN GetMustRun() const;  // 800C1F6C
  void SetMustRun(bool);  // 800C1F78
  RET_UNKNOWN GetDisabled() const;  // 800C1F98
  void SetDisabled(bool);  // 800C1FA4
  RET_UNKNOWN GetRunAsSub() const;  // 800C1FC4
  void SetRunAsSub(bool);  // 800C1FD0
  unsigned int GetFlags() const { return m_flags; }  // 800C1FF0
  void SetFlags(int flags) { m_flags = static_cast<unsigned int>(flags); }  // 800C1FF8
  virtual RET_UNKNOWN SetShader(EActionIcon *) const;  // 800C2000

  // PC: the entry's name (the TTAB's localised string; GC: a BString2 at +0x34).
  const unsigned short* NameText() const;

  int m_type = 0;                    // +0x04 0 from a tree table entry, 2 from the menu
  cXPerson* m_person = nullptr;      // +0x08
  cXObject* m_stackObject = nullptr; // +0x0C the object whose tree table it is
  cXObject* m_iconObject = nullptr;  // +0x10 the queue icon's object
  int m_index = -1;                  // +0x14 the tree table entry
  short m_stackVars[4] = {};         // +0x18 the action tree's params
  std::vector<short> m_localVars;    // +0x20 (GC: a pointer and a count at +0x24)
  int m_priority = 0;                // +0x28 100 the player's, 0x32 pushed, 2 autonomous (person data 0x21)
  short m_tree = 0;                  // +0x2C the action tree
  float m_attenuation = 0.0f;        // +0x30
  int m_id = 0;                      // +0x38 unique (SetUniqueID)
  unsigned int m_flags = 0;          // +0x3C
};

// PC: the items a check tree makes while a menu is built (ObjTestSim::IsMenuInProgress / MakeNewMenuItem:
// primitive 0x32 TryMakeActionString adds one each, e.g. the taxi's "Leave Lot" per unlocked lot): the
// text, the stack vars the action tree gets as params and the check tree's locals.
struct PcMenuItem {
  std::u16string name;
  short vars[4] = {};
  std::vector<short> locals;
};
using PcMenuBuild = std::vector<PcMenuItem>;
extern PcMenuBuild* g_pcMenuBuild;  // objectsim.cpp: set while a menu's check trees run

// 0x2B4 bytes.
struct ActionQueue {
  ActionQueue();  // 80119E50
  int CountVisible() const;  // 80119EB0
  void AddActionToHUD(int);  // 80119F60
  void RemoveActionFromHUD(int);  // 8011A06C
  bool SetActionText(BString2 &, unsigned int);  // 8011A0E8
  RET_UNKNOWN SetIconObject(cXObject *, unsigned int);  // 8011A150
  // Shown in the queue: it has an icon object and is not hidden; a continuation only at the head.
  int IsVisibleAction(const Interaction& action) const;  // 8011A210
  RET_UNKNOWN FindVisualInsertGUID(unsigned int) const;  // 8011A2BC
  int UpdateAction(unsigned int index);  // 8011A35C
  // At `index` from the head (the later ones move up); false when ten are queued.
  int Insert(Interaction& action, unsigned int index);  // 8011A3F8
  int Remove(unsigned int index);  // 8011A538
  void Enqueue(Interaction& action);  // 8011A6E4
  static void DisableHUDAccess(int);  // 8011A714
  void EnableHUDAccess(int);  // 8011A770

  unsigned int Count() const { return m_tail - m_head; }
  Interaction& At(unsigned int index) { return m_actions[(m_head + index) % 10]; }
  const Interaction& At(unsigned int index) const { return m_actions[(m_head + index) % 10]; }

  Interaction m_actions[10];         // +0x000
  unsigned int m_head = 0;           // +0x2A8 (cXPersonImpl +0x3D8)
  unsigned int m_tail = 0;           // +0x2AC (cXPersonImpl +0x3DC)
  bool m_hudAccess = true;           // +0x2B0
  unsigned char m_hudPlayer = 0xff;  // +0x2B1 the player whose HUD shows it (EnableHUDAccess; +0x2B0 0 = shown)
};
