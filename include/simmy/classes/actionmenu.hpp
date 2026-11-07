// @class ActionMenu__MenuItem
// @class ActionMenu after ActionMenu__MenuItem
// @class FlashPiMenu
// The live-mode interaction menu (game/actionmenu.cpp ActionMenu, game/flashpimenu.cpp FlashPiMenu).
// A on an object (DirectInteractor::ExecuteActionMenu) gives the player's FlashPiMenu the objects the
// interactor highlights; it collects each object's interactions (CollectInteractionsForObject: the
// object's tree table entries whose check trees pass for the sim) and builds the HUD's ActionMenu for
// that player: one sub menu per object (object name) when there are several, the interactions in it,
// names with a "/" in a sub menu of their own ("Sit/Watch TV"). The ActionMenu (HUDTarget +0x740 and
// +0x7F4, run and drawn by HUDTarget::Draw) takes the pad through a priority-100 filter while it is
// open (up/down move, A accepts or opens a sub menu, B backs out) and shows three rows at a time
// through the player's CSMTarget (the "CXM" widget: object name, previous / current / next item,
// arrows when more are hidden). An accepted interaction goes back to FlashPiMenu::ChooseAction, which
// hands it to the player's interactor (GC: DirectInteractor::ChooseAction -> the person's action
// queue).
//
// PC: the simulator's Interaction, InteractionList and ObjTestSim are not ported; a MenuInteraction
// stands in (the person, the object, the tree table entry, its name and flags), and the chosen one is
// started as the person's object tree (cXPersonImpl::GosubObjectTree, as the A-button stand-in did)
// instead of being queued.
#pragma once

#include <string>
#include <vector>

// PC stand-in for the GC Interaction (0x44 bytes: +0x08 person, +0x0C stack object, +0x10 icon
// object, +0x14 tree table entry, +0x34 name, +0x3C flags: bits 12..14 the text colour, 0x800 shown
// disabled).
struct MenuInteraction {
  cXPersonImpl* person = nullptr;      // +0x08
  cXObjectImpl* object = nullptr;      // +0x0C the stack object
  cXObjectImpl* icon = nullptr;        // +0x10
  int entry = -1;                      // +0x14 the object's tree table entry
  short actionTree = 0;                // the entry's action tree (TTAB +2)
  std::u16string name;                 // +0x34 (GetName: the entry's text)
  unsigned int flags = 0;              // +0x3C
  short vars[4] = {};                  // +0x18 the action tree's params (a check tree's menu item)
  int GetTextColor() const { return static_cast<int>(flags >> 12 & 7); }  // (Interaction::GetTextColor)
};
using MenuInteractionList = std::vector<MenuInteraction>;  // InteractionList
// socialmode.cpp: the social-mode interactor's ChooseAction (null: backed out); false when the player is
// not in social mode.
bool SocialModeChooseAction(int player, const MenuInteraction* m);

// One row of the menu, or a sub menu (0x64 bytes).
struct ActionMenu__MenuItem {
  ActionMenu__MenuItem();  // 80182F40
  ~ActionMenu__MenuItem();  // 80182F9C
  void Setup(cXObjectImpl* object, const MenuInteraction* interaction, const std::u16string& name, unsigned int shaderId,
             ERShader* shader, bool disabled);  // 80182FF4
  void AddItem(ActionMenu__MenuItem* item);  // 801830A8
  void AddSubItem(ActionMenu__MenuItem* item);  // 801830B8
  ActionMenu__MenuItem* GetNthItem(int n);  // 80183120
  int IsObjectInMenu(cXObjectImpl* object);  // 80183138
  void ClearMenu();  // 801831B8
  ActionMenu__MenuItem* GetNext() { return m_next; }  // 80183248
  const ActionMenu__MenuItem* GetNextConst() const { return m_next; }  // 80183250
  ActionMenu__MenuItem* GetSubMenu() { return m_sub; }  // 80183258
  int GetMenuItemCount() const;  // 80183260

  cXObjectImpl* m_object = nullptr;    // +0x00
  MenuInteraction m_interaction;       // +0x04
  std::u16string m_name;               // +0x48
  std::string m_utf8;                  // +0x4C the name as UTF-8 (CUCS2ToUTF8)
  unsigned int m_shaderId = 0;         // +0x50 the icon by id
  ERShader* m_shader = nullptr;        // +0x54 or as a shader (a thumbnail)
  int m_disabled = 0;                  // +0x58
  ActionMenu__MenuItem* m_sub = nullptr;   // +0x5C
  ActionMenu__MenuItem* m_next = nullptr;  // +0x60
};

// One player's menu (0xB4 bytes). m_state: 0 stopped, 1 ready, 2 just shown, 3 closed (the filter
// goes next run), 4 browsing, 5 / 6 moving down / up, 7 accept, 8 back, 9 terminate.
struct ActionMenu {
  using MenuItem = ::ActionMenu__MenuItem;
  ActionMenu();  // 801832A8
  ~ActionMenu();  // 80183350
  int IsObjectInMenu(cXObjectImpl* object);  // 801833D8
  void Start(HUDTarget* hud, int player);  // 801833FC
  void Draw(ERC* rc);  // 80183410
  void Stop();  // 801837F8
  void Animate();  // 801838B0
  void UpdateIcon();  // 80183928
  void FillInMenu();  // 80183B10
  void Run();  // 80183C24
  void AddMenu(cXObjectImpl* object, const std::u16string& name, unsigned int shaderId, ERShader* shader);  // 80183EA0
  void AddMenuItem(cXObjectImpl* object, const MenuInteraction* interaction, const std::u16string& name,
                   unsigned int shaderId, ERShader* shader, bool disabled);  // 80183F84
  void CompleteMenu();  // 80184008
  int ShowMenu();  // 80184018
  void BackOutMenu();  // 80184108
  void AcceptMenu();  // 80184208
  void CloseMenu();  // 801843A8
  void TerminateMenu();  // 801843FC
  cXPerson* GetPlayerSim() const;  // 80184520

  // not ported yet
  void Start(INVTarget *, int);  // 801837E4

  MenuItem* Current() { return m_path[m_depth]; }

  HUDTarget* m_hud = nullptr;   // +0x00
  short m_timer = 0;            // +0x04
  short m_cursor = 0;           // +0x06 the current row
  unsigned char m_field8 = 1;   // +0x08
  unsigned char m_state = 0;    // +0x09
  unsigned char m_lastState = 0;  // +0x0A
  MenuItem m_root;              // +0x0C
  MenuItem* m_path[8] = {};     // +0x70 the open menus, m_path[0] the root
  short m_savedCursor[8] = {};  // +0x90 each open menu's row
  unsigned char m_depth = 0;    // +0xA0
  unsigned char m_visible = 0;  // +0xA1 rows shown (at most 5)
  unsigned char m_count = 0;    // +0xA2 rows in the open menu
  int m_iconShown = 0;          // +0xA4
  int m_player = 0;             // +0xA8
  unsigned int m_filter = 0;    // +0xAC the pad filter while open
  ERShader* m_resource = nullptr;  // +0xB0
};

// The player's menu builder (0x10 bytes; Player +0x08).
struct FlashPiMenu {
  explicit FlashPiMenu(int player);  // 80039D70
  virtual ~FlashPiMenu();  // 80039D94
  virtual void Update();  // 80039DE0
  static void CleanUpAllMenus();  // 80039DE4
  int CreateMenuForGoHere(bool immediate);  // 80039DE8
  int CreateObjectMenuFromObjList(const std::vector<cXObjectImpl*>& objects);  // 80039F14
  void AddObjectInteractionsToMenu(cXObjectImpl* object, bool single);  // 80039FF4
  int CreateInteractionMenu(cXObjectImpl* object, MenuInteractionList& list);  // 8003A2E0
  void CleanupDestinationObject();  // 8003A6AC
  void Die();  // 8003A784
  void Kill();  // 8003A7EC
  int ChooseAction(cXObjectImpl* object, const MenuInteraction* interaction, bool die);  // 8003AAB0
  void BackedOutOfMenu();  // 8003ACA0
  int GetPlayerId();  // 8003AFCC

  void CreateGoHereObjectForMenu();  // 8003A7F8

  // not ported yet
  FlashPiMenu();  // 8003AFB8

  cXObjectImpl* m_destination = nullptr;  // +0x00 the "go here" object
  int m_dead = 0;                         // +0x04
  int m_player = 0;                       // +0x08
};

// GetActionMenu (8018458C): player's menu in the HUD (null without a HUD or past player 2).
ActionMenu* GetActionMenu(unsigned int player);
// GetSelectedActionMenuItem (801845C4): the instance of the open menu's current object (GC: its
// ISimInstance, spSelectedInstance; PC: the object).
cXObjectImpl* GetSelectedActionMenuItem(unsigned int player);
// GetObjectMenuName (8003ACFC): a person's first name, or the object's catalogue short name.
std::u16string GetObjectMenuName(cXObjectImpl* object);
// GetShaderForObject (8003ADC4): the object's menu icon shader id; *thumbnail a person's portrait
// (not ported: null).
unsigned int GetShaderForObject(cXObjectImpl* object, ERShader** thumbnail);
// CollectInteractionsForObject (80041B9C/80041BE0, game/global.cpp): what player `player`'s sim can
// do with the object, at most `max`. PC stand-in for ObjTestSim::AppendInteractionsForMenu (with its
// direct-control filter: only entries whose activation area holds the sim).
void CollectInteractionsForObject(cXObjectImpl* object, MenuInteractionList& list, int player, int max);
// PC: starts a chosen interaction (GC: the interactor's ChooseAction -> cXPerson vtable 0xB4, the
// action queue). 1: started.
int StartMenuInteraction(const MenuInteraction& interaction);
