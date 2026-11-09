// Port of game/actionmenu.cpp: ActionMenu, the HUD's interaction menu, and its MenuItem tree. See
// actionmenu.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

namespace {
cXObjectImpl* s_selectedInstance[2] = {};  // 804FDE28 spSelectedInstance

H2DTarget* H2D() { return _gd.m_h2dTarget; }
CSMTarget* PlayerCSM(int player) {
  H2DTarget* h2d = H2D();
  return h2d && player >= 0 && player < 2 ? h2d->m_csm[player] : nullptr;
}
void PlaySound(unsigned int id) { UIAUDIO::Instance()->PlaySound(id); }

// ObjectModuleImpl::IsValidObject (800F9310): the object is still in the module.
bool IsValidObject(cXObjectImpl* object) {
  if (!object || !Globs_pObjectModule) return false;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
    if (o == object) return true;
  // PC: the lot's people are in the module by id only (not in its object list).
  for (int id = 1; id <= Globs_pObjectModule->GetNumObjects(); ++id) {
    cXObject* x = Globs_pObjectModule->GetObjectFromID(static_cast<short>(id));
    if (x && x->GetObjectImplementation() == object) return true;
  }
  return false;
}

// "0CXM_IconShader"/"1CXM_IconShader" (804FDE30 CXM_IconShader), "0INT_shader"/"1INT_shader"
// (804FDE38 INT_shader).
const char* const kIconShader[2] = {"0CXM_IconShader", "1CXM_IconShader"};
const char* const kIntShader[2] = {"0INT_shader", "1INT_shader"};

// CUCS2ToUTF8
std::string ToUtf8(const std::u16string& s) {
  std::string out;
  for (char16_t c : s) {
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (c < 0x800) {
      out += static_cast<char>(0xc0 | c >> 6);
      out += static_cast<char>(0x80 | (c & 0x3f));
    } else {
      out += static_cast<char>(0xe0 | c >> 12);
      out += static_cast<char>(0x80 | (c >> 6 & 0x3f));
      out += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  return out;
}

// The menu's pad commands (80436B88 filterCmdSet.8739): up, down, accept, cancel and the two side
// commands.
unsigned int kFilterCmds[6] = {1, 2, 5, 6, 0xd, 0xe};
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ActionMenu::MenuItem

// 80182F40
ActionMenu__MenuItem::ActionMenu__MenuItem() {}

// 80182F9C
ActionMenu__MenuItem::~ActionMenu__MenuItem() { ClearMenu(); }

// 80182FF4
// The row's object, interaction (copied), name and icon.
void ActionMenu__MenuItem::Setup(cXObjectImpl* object, const MenuInteraction* interaction, const std::u16string& name,
                                 unsigned int shaderId, ERShader* shader, bool disabled) {
  m_object = object;
  if (interaction) m_interaction = *interaction;
  m_name = name;
  m_utf8 = ToUtf8(m_name);
  // The row holds its icon: a reference on the new shader, the old one's given back.
  if (shader) shader->AddRef();
  if (m_shader) m_shader->DelRef();
  m_disabled = disabled ? 1 : 0;
  m_shaderId = shaderId;
  m_shader = shader;
}

// 801830A8
void ActionMenu__MenuItem::AddItem(ActionMenu__MenuItem* item) {
  item->m_next = m_next;
  m_next = item;
}

// 801830B8
// At the end of the sub menu.
void ActionMenu__MenuItem::AddSubItem(ActionMenu__MenuItem* item) {
  ActionMenu__MenuItem* last = m_sub;
  if (!last) {
    m_sub = item;
    return;
  }
  while (last->GetNextConst()) last = last->GetNext();
  last->AddItem(item);
}

// 80183120
// (GC: the loop's walk was optimised away in the listing; the n-th along the list.)
ActionMenu__MenuItem* ActionMenu__MenuItem::GetNthItem(int n) {
  ActionMenu__MenuItem* item = this;
  while (item && n-- > 0) item = item->m_next;
  return item;
}

// 80183138
int ActionMenu__MenuItem::IsObjectInMenu(cXObjectImpl* object) {
  if (m_object == object) return 1;
  if (m_next && m_next->IsObjectInMenu(object)) return 1;
  if (m_sub && m_sub->IsObjectInMenu(object)) return 1;
  return 0;
}

// 801831B8
void ActionMenu__MenuItem::ClearMenu() {
  m_utf8.clear();
  delete m_next;
  m_next = nullptr;
  delete m_sub;
  m_sub = nullptr;
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
}

// 80183248 GetNext, 80183250 GetNextConst, 80183258 GetSubMenu: inline in actionmenu.hpp.
// 80183248
// 80183250
// 80183258

// 80183260
int ActionMenu__MenuItem::GetMenuItemCount() const {
  int n = 1;
  for (const ActionMenu__MenuItem* i = GetNextConst(); i; i = i->GetNextConst()) ++n;
  return n;
}

// ---------------------------------------------------------------------------------------------------
// ActionMenu

// 801832A8
ActionMenu::ActionMenu() { m_path[0] = &m_root; }

// 80183350
ActionMenu::~ActionMenu() {
  if (m_filter) {
    if (EController* pad = _pCtrlMan->GetPlayerController(m_player)) pad->RemoveFilter(m_filter);
    m_filter = 0;
  }
  m_resource = nullptr;  // GC: EResource::DelRef
}

// 801833D8
int ActionMenu::IsObjectInMenu(cXObjectImpl* object) { return m_root.IsObjectInMenu(object); }

// 801833FC
void ActionMenu::Start(HUDTarget* hud, int player) {
  m_player = player;
  m_state = 1;
  m_hud = hud;
}

// 80183410
// While open: the three rows around the cursor (previous, current, next) and the object's name to
// the player's CSM widget, with the arrows when rows are hidden above or below. A row whose object
// went away ends the menu. Closed: the widget is hidden.
void ActionMenu::Draw(ERC*) {
  CSMTarget* csm = PlayerCSM(m_player);
  if (m_state < 2 || m_state == 3) {
    if (csm && csm->IsCSIMActive()) csm->HideCSIM();
    return;
  }
  MenuItem* menu = Current()->GetSubMenu();
  if (!menu) return;
  m_count = static_cast<unsigned char>(menu->GetMenuItemCount());
  m_visible = m_count > 5 ? 5 : m_count;
  const int ahead = 2 - m_cursor;
  int slot = ahead < 0 ? 0 : ahead;
  MenuItem* item = menu->GetNthItem(ahead > 0 ? 0 : -ahead);
  MenuItem* cur = menu->GetNthItem(m_cursor);
  std::u16string before, current, after, name;
  bool atCurrent = false, haveAfter = false, moreAbove = false;
  int rowsBefore = 0;
  const int count = m_count, cursor = m_cursor;
  while (item && slot < 5) {
    if (!IsValidObject(item->m_object)) {
      m_state = 9;
      TerminateMenu();
      return;
    }
    if (item == cur) {
      // A person's first name, or the object's (its multi-tile master's) catalogue short name.
      name = GetObjectMenuName(item->m_object);
      atCurrent = true;
      current = item->m_name;
    }
    if (atCurrent) {
      if (item != cur && !haveAfter) {
        after = item->m_name;
        haveAfter = true;
      }
    } else {
      before = item->m_name;
      ++rowsBefore;
      moreAbove = rowsBefore > 1;
    }
    item = item->GetNext();
    ++slot;
  }
  if (!csm) return;
  csm->ShowCSIM();
  csm->TempInterface(name, before, current, after, moreAbove, count - cursor > 2);
}

// 801837F8
void ActionMenu::Stop() {
  TerminateMenu();
  m_root.ClearMenu();
  m_field8 = 1;
  m_path[0] = &m_root;
  m_savedCursor[0] = 0;
  m_timer = 0;
  m_cursor = 0;
  m_state = 0;
  for (int i = 1; i < 8; ++i) {
    m_path[i] = nullptr;
    m_savedCursor[i] = 0;
  }
  m_count = 0;
  m_depth = 0;
  m_iconShown = 0;
  if (m_filter) {
    if (EController* pad = _pCtrlMan->GetPlayerController(m_player)) pad->RemoveFilter(m_filter);
    m_filter = 0;
  }
}

// 801838B0
// One row down (state 5) or up, then back to browsing.
void ActionMenu::Animate() {
  if (m_state == 5) {
    if (m_cursor < m_count - 1) ++m_cursor;
  } else if (m_cursor != 0) {
    --m_cursor;
  }
  FillInMenu();
  m_state = 4;
}

// 80183928
// The current row's icon (by id, else its shader) in the CSM widget and the interest meter.
void ActionMenu::UpdateIcon() {
  MenuItem* menu = Current()->GetSubMenu();
  MenuItem* item = menu ? menu->GetNthItem(m_cursor) : nullptr;
  if (!item) return;
  UI2D* ui2d = UI2D::Instance();
  char* group = m_hud ? const_cast<char*>(m_hud->m_name) : nullptr;
  if (item->m_shaderId) {
    if (m_hud && m_hud->m_installed) {
      ui2d->InstallEntry(const_cast<char*>(kIconShader[m_player]), item->m_shaderId, group, 0);
      ui2d->InstallEntry(const_cast<char*>(kIntShader[m_player]), item->m_shaderId, group, 0);
    }
    m_iconShown = 1;
  } else if (item->m_shader) {
    if (m_hud && m_hud->m_installed) {
      ui2d->InstallEntry(const_cast<char*>(kIconShader[m_player]), item->m_shader, group, 0);
      ui2d->InstallEntry(const_cast<char*>(kIntShader[m_player]), item->m_shader, group, 0);
    }
    m_iconShown = 1;
  } else {
    if (!m_iconShown) return;
    if (m_hud && m_hud->m_installed) {
      ui2d->UnInstallEntry(const_cast<char*>(kIconShader[m_player]));
      ui2d->UnInstallEntry(const_cast<char*>(kIntShader[m_player]));
    }
    m_iconShown = 0;
  }
}

// 80183B10
// The open menu's counts, the selected object (spSelectedInstance) and its icon; ends the menu when
// its first row's object went away.
void ActionMenu::FillInMenu() {
  MenuItem* menu = Current()->GetSubMenu();
  if (!menu) return;
  m_count = static_cast<unsigned char>(menu->GetMenuItemCount());
  if (!m_count) return;
  m_visible = m_count > 5 ? 5 : m_count;
  const int ahead = 2 - m_cursor;
  MenuItem* first = menu->GetNthItem(ahead < 0 ? -ahead : 0);
  MenuItem* cur = menu->GetNthItem(m_cursor);
  if (!first || !IsValidObject(first->m_object)) {
    m_state = 9;
    TerminateMenu();
    return;
  }
  // GC: the object's ISimInstance (cXObject +0x18 vtable 0x9C).
  if (cur && cur->m_object) s_selectedInstance[m_player] = cur->m_object;
  UpdateIcon();
}

// 80183C24
// The pad while open: down/up move (with the browse sound), A accepts, B backs out.
void ActionMenu::Run() {
  EController* pad = _pCtrlMan->GetPlayerController(m_player);
  if (m_state < 7) {
    if (m_state >= 5) {
      Animate();
      return;
    }
    if (m_state == 2) {
      m_state = 4;
    } else if (m_state == 3) {
      if (pad) pad->RemoveFilter(m_filter);
      m_state = 1;
      m_filter = 0;
    } else if (m_state == 4 && pad) {
      if (pad->GetCmdResult(2, pad->GetBtnRepeatMask(m_filter), pad->GetBtnDownMask(m_filter), false)) {
        if (m_count < 2 || m_cursor >= m_count - 1) return;
        m_state = 5;
        m_timer = 0;
      } else if (pad->GetCmdResult(1, pad->GetBtnRepeatMask(m_filter), pad->GetBtnDownMask(m_filter), false)) {
        if (m_count < 2 || m_cursor < 1) return;
        m_timer = 0;
        m_state = 6;
      } else {
        if (pad->GetCmdResult(5, pad->GetBtnPressedMask(m_filter), pad->GetBtnDownMask(m_filter), false)) {
          m_timer = 0;
          m_state = 7;
          return;
        }
        if (pad->GetCmdResult(6, pad->GetBtnPressedMask(m_filter), pad->GetBtnDownMask(m_filter), false)) m_state = 8;
        return;
      }
      PlaySound(0x0064ac15);
    }
  } else if (m_state == 8) {
    BackOutMenu();
  } else if (m_state == 7) {
    AcceptMenu();
  } else if (m_state == 9) {
    TerminateMenu();
  }
}

// 80183EA0
// Opens a sub menu for the object (an existing one with the same name and object, else a new row)
// for the rows that follow, until CompleteMenu.
void ActionMenu::AddMenu(cXObjectImpl* object, const std::u16string& name, unsigned int shaderId, ERShader* shader) {
  MenuItem* item = Current()->GetSubMenu();
  for (; item; item = item->GetNext())
    if (item->m_name == name && item->m_object == object) break;
  if (!item) {
    item = new MenuItem;
    item->Setup(object, nullptr, name, shaderId, shader, false);
    Current()->AddSubItem(item);
  }
  ++m_depth;
  m_path[m_depth & 7] = item;
}

// 80183F84
void ActionMenu::AddMenuItem(cXObjectImpl* object, const MenuInteraction* interaction, const std::u16string& name,
                             unsigned int shaderId, ERShader* shader, bool disabled) {
  auto* item = new MenuItem;
  item->Setup(object, interaction, name, shaderId, shader, disabled);
  Current()->AddSubItem(item);
}

// 80184008
void ActionMenu::CompleteMenu() { --m_depth; }

// 80184018
// Opens the built menu at its first row and takes the pad's menu commands. 0: nothing to show.
int ActionMenu::ShowMenu() {
  MenuItem* menu = Current()->GetSubMenu();
  if (!menu) return 0;
  PlaySound(0xcd34abdc);
  m_state = 2;
  m_timer = 0;
  m_count = static_cast<unsigned char>(menu->GetMenuItemCount());
  m_cursor = 0;
  FillInMenu();
  if (EController* pad = _pCtrlMan->GetPlayerController(m_player)) {
    m_filter = pad->AddFilter(0, const_cast<char*>("HUD: menu filter"), 100);
    pad->AddCmdsToFilter(m_filter, kFilterCmds, 6);
  }
  return 1;
}

// 80184108
// Back to the menu above (its row restored), or out of the menu at the top.
void ActionMenu::BackOutMenu() {
  if (!Current()->GetSubMenu()) return;
  PlaySound(0x048ae94f);
  m_cursor = 0;
  m_count = static_cast<unsigned char>(Current()->GetSubMenu()->GetMenuItemCount());
  if (m_depth == 0) {
    if (FlashPiMenu* pi = _gd.m_players[m_player & 1].m_piMenu) pi->BackedOutOfMenu();
    m_lastState = m_state;
    m_state = 9;
  } else {
    m_state = 4;
    const short saved = m_savedCursor[m_depth];
    --m_depth;
    m_cursor = saved;
    FillInMenu();
  }
}

// 80184208
// The current row: a disabled row does nothing, a sub menu opens, an interaction goes to the
// player's FlashPiMenu (GC: with the grab interactor active, the object is grabbed instead).
void ActionMenu::AcceptMenu() {
  m_lastState = 4;
  m_state = 4;
  MenuItem* item = Current()->GetSubMenu()->GetNthItem(m_cursor);
  if (!item || item->m_disabled) return;
  if (item->m_sub) {
    PlaySound(0x374bdcea);
    ++m_depth;
    m_path[m_depth & 7] = item;
    m_savedCursor[m_depth & 7] = m_cursor;
    m_count = static_cast<unsigned char>(Current()->GetSubMenu()->GetMenuItemCount());
    m_cursor = 0;
    FillInMenu();
    return;
  }
  // The player's grab manipulator (InteractorManager::GetPlayerInteractor(player, 2): build/buy's grab
  // mode) takes the row's object in hand (PCTTarget::CreateGrabObjectMenu); else the pie menu's choice.
  m_state = 9;
  if (GrabManipulatorActive(m_player)) {
    PlaySound(0x374bdcea);
    if (item->m_object) GrabTryGrabbingWorldObjectFromId(m_player, static_cast<short>(item->m_object->m_id));
    return;
  }
  FlashPiMenu* pi = _gd.m_players[m_player & 1].m_piMenu;
  if (pi && !pi->ChooseAction(item->m_object, &item->m_interaction, true)) TerminateMenu();
}

// 801843A8
void ActionMenu::CloseMenu() {
  if (m_state == 4 || m_state == 2 || m_state == 5 || m_state == 6) {
    m_state = 9;
    TerminateMenu();
  }
}

// 801843FC
// Once terminating: tells the FlashPiMenu that no row was taken (when the open menu's rows are not
// sub menus), clears the rows and closes (the filter goes on the next run).
void ActionMenu::TerminateMenu() {
  if (m_state != 9) return;
  s_selectedInstance[m_player & 1] = nullptr;
  MenuItem* menu = Current()->GetSubMenu();
  if (menu && !menu->m_sub) {
    if (FlashPiMenu* pi = _gd.m_players[m_player & 1].m_piMenu) pi->ChooseAction(nullptr, &menu->m_interaction, true);
  }
  m_root.ClearMenu();
  m_state = 3;
  m_timer = 8;
  m_depth = 0;
  m_cursor = 0;
  m_path[0] = &m_root;
  for (int i = 1; i < 8; ++i) {
    m_path[i] = nullptr;
    m_savedCursor[i] = 0;
  }
}

// 80184520
// The player's selected sim, while it is a person.
cXPerson* ActionMenu::GetPlayerSim() const { return _globals.GetSelectedPerson(m_player); }

// 8018458C
ActionMenu* GetActionMenu(unsigned int player) {
  if (player > 1) return nullptr;
  HUDTarget* hud = _gd.m_hudTarget;
  return hud ? &hud->m_menus[player] : nullptr;
}

// 801845C4
cXObjectImpl* GetSelectedActionMenuItem(unsigned int player) { return player > 1 ? nullptr : s_selectedInstance[player]; }
