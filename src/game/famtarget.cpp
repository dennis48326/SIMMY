// Port of FAMTarget (game/targets_s2c.cpp), free play's neighbourhood screen, and of the pre-game
// states around it (PreGameFreeplayState, PreGameCreateAFamilyState: game/pregamestate.cpp). See
// famtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

void* SimsAptMalloc(unsigned int size);
void SimsAptFree(void* p);

namespace {
UIScreenManager* Screens() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
const char* SkipDigit(const char* name) { return std::isdigit(static_cast<unsigned char>(*name)) ? name + 1 : name; }
size_t Len16(const unsigned short* s) {
  size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
void Copy16(unsigned short* out, const unsigned short* in) {
  const size_t n = Len16(in);
  for (size_t i = 0; i < n; ++i) out[i] = in[i];
  out[n] = 0;
}
std::vector<unsigned short> Vec16(const unsigned short* s) {
  std::vector<unsigned short> v(s, s + Len16(s));
  v.push_back(0);
  return v;
}
const unsigned short* UIText(const char* name) { return EGlobal::NewUIText(name); }
void InstallShader(FAMTarget* t, const char* name, unsigned int id) {
  if (t->m_installed) UI2D::Instance()->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(t->m_name), 0);
}
void InstallShader(FAMTarget* t, const char* name, ERShader* shader) {
  if (t->m_installed) UI2D::Instance()->InstallEntry(const_cast<char*>(name), shader, const_cast<char*>(t->m_name), 0);
}
void PlaySample(unsigned int id) { UIAUDIO::Instance()->PlaySound(id); }

const unsigned int kBlank = 0x9f93ff74;  // the empty head
// g_SelectFamilyShaders (80437224: 8 rows of 4), g_LotSelectHeadShaders (80439204),
// g_OccupiedHeadShaders (80439600): 0xFF-byte names.
std::string SelectFamilyShader(int row, int head) {
  char b[64];
  std::snprintf(b, sizeof b, "FAM_Select_Row_%02d_Head_%02d_Shader", row, head);
  return b;
}
std::string LotSelectHeadShader(int head) {
  char b[64];
  std::snprintf(b, sizeof b, "FAM_Lot_00_Head_%02d_Shader", head);
  return b;
}
std::string OccupiedHeadShader(int head) {
  char b[64];
  std::snprintf(b, sizeof b, "FAM_Occupied_Head_%02d_Shader", head);
  return b;
}

// A family member's thumbnail into a head shader: the blank head without a member, a neighbour or a
// thumbnail (GC: the three names the loops kept were the same table).
void InstallHead(FAMTarget* t, const std::string& name, FamilyImpl* family, int member) {
  if (!family) {
    InstallShader(t, name.c_str(), kBlank);
    return;
  }
  Neighbor* n = nullptr;
  if (int* guid = family->GetIndexedMember(member)) n = Globs_pNeighborhood->FindNeighborByGUID(*guid);
  if (!n || !n->m_selector) {
    InstallShader(t, name.c_str(), kBlank);
    return;
  }
  ERShader* thumb = nullptr;
  n->m_selector->GetThumbnail(&thumb);
  if (!thumb) {
    InstallShader(t, name.c_str(), kBlank);
    return;
  }
  InstallShader(t, name.c_str(), thumb);
  thumb->DelRef();
}
}  // namespace

// 800236C8 (game/eorhouse.cpp)
// s_houseNames (804239C8): "house 01" .. "house 16", the house clamped to 1..16.
const char* GetHouseNameText(int house) {
  static char names[16][12];
  int i = house - 1;
  if (i < 0) i = 0;
  else if (i > 15) i = 15;
  if (!names[i][0]) std::snprintf(names[i], sizeof names[i], "house %02d", i + 1);
  return names[i];
}

// ---------------------------------------------------------------------------------------------------
// FAMTarget

// 801AE88C
// Its commands, the panels' backgrounds, the heads (blank), the gold ball, the sounds; loads screen
// 0x44 and starts at the lot list.
FAMTarget::FAMTarget() {
  AttachTables("FAMTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  m_getTable->InstallCommand("FAM_exists", get, this);
  m_getTable->InstallCommand("FAM_GETVAR_CURRENT_STATE", get, this);
  for (int i = 0; i < 8; ++i) m_getTable->InstallCommand(("FAM_GETVAR_FAM_HAS_HOUSE_" + std::to_string(i)).c_str(), get, this);
  for (const char* n : {"FAM_CANCEL_BUTTON", "FAM_DPAD_LEFT", "FAM_DPAD_RIGHT", "FAM_DPAD_UP", "FAM_DPAD_DOWN",
                        "FAM_X_BUTTON", "FAM_Set_Menu_State", "FAM_Close_Screen", "FAM_Set_Current_Choice"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n :
       {"FAM_Menu_Title_Text", "FAM_Manage_Title_Text", "FAM_Select_Family_Title_Text", "FAM_Occupied_Title_Text",
        "FAM_Unoccupied_Title_Text", "FAM_Lot_00_Name_Text", "FAM_Lot_01_Name_Text", "FAM_Lot_02_Name_Text",
        "FAM_Lot_03_Name_Text", "FAM_Lot_Description_Text", "FAM_Occupied_Description_Text",
        "FAM_Unoccupied_Description_Text", "FAM_Selected_Family_Money_Text", "FAM_Selected_Family_Name_Text",
        "FAM_Unoccupied_Start_Money_Text", "FAM_Move_In_Text", "FAM_Bulldoze_Text", "FAM_Enter_House_Text",
        "FAM_Evict_Family_Text", "FAM_Occupied_Family_Name_Text", "FAM_Occupied_Family_Money_Text", "FAM_MF_Edit_Text",
        "FAM_MF_Delete_Text", "FAM_MF_Done_Text"})
    m_locTable->InstallCommand(n, loc, this);
  for (const char* prefix : {"FAM_Select_Family_0", "FAM_Select_Money_0", "FAM_Select_Address_0"})
    for (int i = 0; i < 8; ++i) {
      std::string n = prefix + std::to_string(i);
      if (std::strcmp(prefix, "FAM_Select_Address_0") != 0) n += "_Text";
      m_locTable->InstallCommand(n.c_str(), loc, this);
    }
  for (const char* n : {"FAM_Choose_Lot_Background", "FAM_Manage_Family_Background", "FAM_Select_Family_Background",
                        "FAM_Occupied_Lot_Background", "FAM_Unoccupied_Lot_Background"})
    InstallShader(this, n, 0xc2bf6784u);
  for (int i = 0; i < 4; ++i) InstallShader(this, LotSelectHeadShader(i).c_str(), kBlank);
  InstallShader(this, "FAM_Lot_Select_Gold_Ball_Shader", 0x943cb3f9u);
  for (int row = 0; row < 8; ++row)
    for (int head = 0; head < 4; ++head) InstallShader(this, SelectFamilyShader(row, head).c_str(), kBlank);
  for (int i = 0; i < 4; ++i) InstallShader(this, OccupiedHeadShader(i).c_str(), kBlank);
  UIAUDIO* audio = UIAUDIO::Instance();
  auto sample = [&](const char* n, unsigned int id) { audio->InstallEntry(const_cast<char*>(n), id, const_cast<char*>(m_name)); };
  sample("FAM_PANEL_SCROLL", 0x93914889u);
  sample("FAM_SELECT", 0xe5c52c1au);
  sample("FAM_BROWSE", 0xd2ea5ce5u);
  sample("FAM_CLOSE", 0xf5ad110cu);
  sample("FAM_OPEN", 0x6bbc4acbu);
  sample("FAM_ERROR", 0x3804219fu);
  if (UIScreenManager* usm = Screens()) usm->LoadUIScreen(0x44, 0);
  UpdateText();
  UpdateShaders();
  UpdateMenuState(kLotSelect);
  // PC dev trace: SIMMY_FAM_TRACE=1 lists the free-play lots' and families' occupants.
  if (std::getenv("SIMMY_FAM_TRACE")) {
    for (int i = 0; i < 4; ++i) {
      FamilyImpl* f = GetFamilyInLot(i);
      std::printf("[fam] lot %d (house %d): family %d, %d members\n", i, ConvertSelectedLotIndexToActualGameIndex(i),
                  f ? f->GetNumber() : -1, f ? f->CountMembers() : 0);
    }
    for (int i = 0; i < 8; ++i) {
      FamilyImpl* f = GetFamilyInFamilyList(i);
      std::printf("[fam] family slot %d (number %d): %s, %d members, house %d\n", i, ConvertSelectedFamilyIndexToActualGameIndex(i),
                  f ? "present" : "empty", f ? f->CountMembers() : 0, f ? f->GetHouseNumber() : 0);
    }
  }
}

// 801AFB4C
FAMTarget::~FAMTarget() {
  UnloadScreen();
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UI3D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801AFCAC
void FAMTarget::UnloadScreen() {
  if (UIScreenManager* usm = Screens()) usm->UnloadUIScreen(0x44);
}

// 801AFCD8
// The movie's commands (a leading player digit is skipped).
void FAMTarget::SetVariable(char* name, char* value) {
  char* n = const_cast<char*>(SkipDigit(name));
  if (!std::strcmp("FAM_Close_Screen", n)) Shutdown();
  else if (!std::strcmp("FAM_Set_Menu_State", n)) UpdateMenuState(n, value);
  else if (!std::strcmp("FAM_Set_Current_Choice", n)) SetCurrentChoice(value);
  else if (!std::strcmp("FAM_DPAD_LEFT", n)) OnLeftKeyPressed(n, value);
  else if (!std::strcmp("FAM_DPAD_RIGHT", n)) OnRightKeyPressed(n, value);
  else if (!std::strcmp("FAM_X_BUTTON", n)) OnXKeyPressed(n, value);
  else if (!std::strcmp("FAM_CANCEL_BUTTON", n)) OnCancelKeyPressed(n, value);
  else if (!std::strcmp("FAM_DPAD_UP", n)) OnUpKeyPressed(n, value);
  else if (!std::strcmp("FAM_DPAD_DOWN", n)) OnDownKeyPressed(n, value);
}

// 801AFEB8
// The panels' texts: titles, the selected lot's name and description, the family's (or the lot's
// starting) money and name, the lot list, the family list (names, money, the lot each lives on), the
// buttons. Quickdata substitutions are applied.
unsigned short* FAMTarget::GetLocalizable(char* name) {
  const char* n = SkipDigit(name);
  std::vector<unsigned short> out(0x1000, 0);
  unsigned short* buf = out.data();
  unsigned short tmp[0x200] = {};
  const unsigned short* src = nullptr;
  bool copy = true;
  const std::string s = n;
  auto ui = [&](const char* row) { src = UIText(row); };
  if (s == "FAM_Menu_Title_Text") ui("free_select_lot");
  else if (s == "FAM_Manage_Title_Text") ui("free_family_management");
  else if (s == "FAM_Select_Family_Title_Text") ui("free_select_family");
  else if (s == "FAM_Unoccupied_Title_Text" || s == "FAM_Occupied_Title_Text")
    src = GetLotName(ConvertSelectedLotIndexToActualGameIndex(m_lot));
  else if (s == "FAM_Lot_Description_Text" || s == "FAM_Occupied_Description_Text" ||
           s == "FAM_Unoccupied_Description_Text") {
    GetSelectedLotDescription(tmp);
    src = tmp;
  } else if (s == "FAM_Selected_Family_Money_Text") {
    int money = GetMoneyForLot(ConvertSelectedLotIndexToActualGameIndex(m_lot));
    if (GetFamilyInLot(m_lot)) money = GetMoneyForFamilyInLotSelect(const_cast<char*>(n), nullptr);
    ConvertNumericMoneyToString(money, tmp);
    src = tmp;
  } else if (s == "FAM_Selected_Family_Name_Text") {
    GetNameForFamilyInLotSelect(const_cast<char*>(n), tmp);
    src = tmp;
  } else if (s.compare(0, 8, "FAM_Lot_") == 0 && s.size() >= 8) {
    char digits[3] = {s.size() > 8 ? s[8] : '0', s.size() > 9 ? s[9] : '\0', 0};
    src = GetLotName(ConvertSelectedLotIndexToActualGameIndex(std::atoi(digits)));
  } else if (s.compare(0, 17, "FAM_Select_Money_") == 0 || s.compare(0, 19, "FAM_Select_Family_0") == 0) {
    GetFamilyText(const_cast<char*>(n), tmp);
    src = tmp;
  } else if (s.compare(0, 20, "FAM_Select_Address_0") == 0) {
    src = GetLotNameInFamilySelect(const_cast<char*>(n));
    if (!src) copy = false;
  } else if (s == "FAM_Occupied_Family_Name_Text") {
    GetNameForFamily(m_family, tmp);
    src = tmp;
  } else if (s == "FAM_Occupied_Family_Money_Text") {
    ConvertNumericMoneyToString(GetMoneyForFamilyInLotSelect(const_cast<char*>(n), nullptr), tmp);
    src = tmp;
  } else if (s == "FAM_Unoccupied_Start_Money_Text") {
    UIObjectBase::makeMoneyString(GetMoneyForLot(ConvertSelectedLotIndexToActualGameIndex(m_lot)), tmp, true);
    src = tmp;
  } else if (s == "FAM_Move_In_Text") ui("free_move_in");
  else if (s == "FAM_Bulldoze_Text") ui("free_bulldoze");
  else if (s == "FAM_Evict_Family_Text") ui("free_evict");
  else if (s == "FAM_Enter_House_Text") ui("free_enter");
  else if (s == "FAM_MF_Edit_Text") ui("free_family_edit");
  else if (s == "FAM_MF_Delete_Text") ui("free_family_delete");
  else if (s == "FAM_MF_Done_Text") ui("free_family_move");
  else copy = false;
  if (copy) Copy16(buf, src);
  UIQDTarget::UIQDSubstitutionStrings(buf);
  const size_t len = Len16(buf);
  auto* result = static_cast<unsigned short*>(SimsAptMalloc(static_cast<unsigned int>((len + 1) * 2)));
  Copy16(result, buf);
  return result;
}

// 801B05E0
char* FAMTarget::GetVariable(char* name) {
  const char* n = SkipDigit(name);
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("FAM_exists", n)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  } else if (!std::strncmp(n, "FAM_GETVAR_FAM_HAS_HOUSE_", 0x19)) {
    const char digit[2] = {n[0x19], 0};
    std::strcpy(out, GetLotForFamily(std::atoi(digit)) ? "1" : "0");
  } else if (!std::strcmp("FAM_GETVAR_CURRENT_STATE", n)) {
    std::snprintf(out, 0x20, "%d", m_state);
  }
  return out;
}

// 801B070C
void FAMTarget::Shutdown() {}
// 801B0710
void FAMTarget::Update() {}
// 801B0714
void FAMTarget::UpdateMenuState(char*, char* value) { UpdateMenuState(std::atoi(value)); }
// 801B0750
void FAMTarget::UpdateText() {}
// 801B075C
void FAMTarget::OnLeftKeyPressed(char*, char*) {}
// 801B0760
void FAMTarget::OnRightKeyPressed(char*, char*) {}

// 801B0764
void FAMTarget::UpdateShaders() {
  if (m_state == kLotSelect) UpdateLotSelectShaders();
  else if (m_state == kFamilySelect) UpdateFamilySelectShaders();
  else if (m_state == kOccupied) UpdateOccupiedShaders();
}

// 801B07B0
// The heads of the family being looked at.
void FAMTarget::UpdateOccupiedShaders() {
  for (int i = 0; i < 4; ++i) InstallHead(this, OccupiedHeadShader(i), m_family, i);
}

// 801B0950
// The heads of the family on the highlighted lot.
void FAMTarget::UpdateLotSelectShaders() {
  FamilyImpl* family = Globs_pNeighborhood->GetFamilyInHouse(ConvertSelectedLotIndexToActualGameIndex(m_lot));
  for (int i = 0; i < 4; ++i) InstallHead(this, LotSelectHeadShader(i), family, i);
}

// 801B0B18
void FAMTarget::UpdateTempShaders() {
  UIReflow r;
  r.ResetPosition("FAM_Lot_Select_Gold_Ball_Shader", 100, m_lot * 0x50 + 100, 0);
}

// 801B0B78
// "FAM_Select_Family_0N_Text" (the family's name, "free_create" for an empty slot) or
// "FAM_Select_Money_0N_Text" (its money, nothing for an empty slot).
void FAMTarget::GetFamilyText(char* name, unsigned short* out) {
  const bool isName = std::strncmp(name, "FAM_Select_Money_", 0x11) != 0;
  char digits[3] = {};
  std::strncpy(digits, name + (isName ? 0x12 : 0x11), 2);
  FamilyImpl* family = Globs_pNeighborhood->GetFamily(ConvertSelectedFamilyIndexToActualGameIndex(std::atoi(digits)));
  if (!family) {
    if (isName) Copy16(out, UIText("free_create"));
    else out[0] = 0;
  } else if (isName) {
    GetNameForFamily(family, out);
  } else {
    GetMoneyForFamily(family, out);
  }
}

// 801B0CCC
void FAMTarget::OnLotSelect() { UpdateMenuState(IsSelectedLotOccupied() ? kOccupied : kUnoccupied); }

// 801B0D1C
FamilyImpl* FAMTarget::GetFamilyInFamilyList(int index) {
  return Globs_pNeighborhood->GetFamily(ConvertSelectedFamilyIndexToActualGameIndex(index));
}

// 801B0D5C
bool FAMTarget::DoesFamilyExistInFamilyChoice(int index) {
  return Globs_pNeighborhood->GetFamily(ConvertSelectedFamilyIndexToActualGameIndex(index)) != nullptr;
}

// 801B0DAC
// The free-play families are numbers 20..27.
int FAMTarget::ConvertSelectedFamilyIndexToActualGameIndex(int index) { return index + 0x14; }

// 801B0DB4
int FAMTarget::GetNumberOfFamilyMembers(Family* family) {
  return family ? static_cast<FamilyImpl*>(family)->CountMembers() : 0;
}

// 801B0DF8
// A lot with a family of one or more.
int FAMTarget::IsSelectedLotOccupied() {
  FamilyImpl* family = Globs_pNeighborhood->GetFamilyInHouse(ConvertSelectedLotIndexToActualGameIndex(m_lot));
  if (!family) return 0;
  return GetNumberOfFamilyMembers(family) > 0 ? 1 : 0;
}

// 801B0E6C
void FAMTarget::OnCancelDialog() {}

// 801B0E70
// Yes to deleting a family without a house (state 3): its members leave and are deleted, the family
// goes and the neighbourhood is saved (then up the tree); yes to bulldozing (state 2) or evicting
// (state 1) the lot. The panels are refreshed.
void FAMTarget::OnDialogSelection(int selection) {
  if (m_state == kOccupied) {
    if (selection == 0 && m_occupiedChoice != 0 && m_occupiedChoice == 1) EvictFamily();
  } else if (m_state == kManageFamily) {
    if (m_manageChoice == 1 && selection == 1) {
      if (FamilyImpl* family = GetFamilyInFamilyList(m_familyChoice)) {
        while (family->CountMembers() > 0) {
          const int guid = *family->GetIndexedMember(0);
          family->RemoveMember(guid);
          Globs_pNeighborhood->DeleteCharacter(Globs_pNeighborhood->FindNeighborByGUID(guid));
        }
        Globs_pNeighborhood->RemoveFamily(family);
        Globs_pNeighborhood->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
      }
      GoUpMenuTree();
    }
  } else if (m_state == kUnoccupied && m_unoccupiedChoice == 1 && selection == 0) {
    const int lot = ConvertSelectedLotIndexToActualGameIndex(m_lot);
    PlaySample(0xa027b573u);
    HoodManager::DemolishCurrentHouse(lot - 1);
  }
  UpdateText();
  UpdateShaders();
  AptViewer_CallFunction(const_cast<char*>("updateStateFAM"), nullptr, nullptr, 0, nullptr);
}

// 801B1078
// Back from Create-a-Family: the screen again; when it was left, a family made for it (a new one:
// none was being looked at) is removed again.
void FAMTarget::OnReturnFromCreateAFamily(bool canceled) {
  if (UIScreenManager* usm = Screens()) usm->LoadUIScreen(0x44, 0);
  if (canceled) {
    FamilyImpl* family = Globs_pNeighborhood->GetFamily(_gd.m_fieldD8);
    if (!m_family && family) Globs_pNeighborhood->RemoveFamily(family);
  }
  UpdateText();
  UpdateShaders();
}

// 801B1128
void FAMTarget::OnBackoutToMainMenuDialogClose(int selection) {
  if (selection == 1) UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("CASBackoutP1"));
}

// The dialogs' common part: `buttons` buttons with the UI strings named, the body, no title; the
// selection goes to OnDialogSelection (or OnBackoutToMainMenuDialogClose).
void FAMTarget::SpawnFamDialog(int buttons, const char* const* selections, const unsigned short* body, bool backout) {
  m_dialog.m_numSelections = buttons;
  for (int i = 0; i < 3 && selections[i]; ++i) m_dialog.m_sel[i] = Vec16(UIText(selections[i]));
  m_dialog.m_body = Vec16(body);
  m_dialog.m_title = Vec16(nullptr);
  m_dialog.m_field14 = 0;
  if (backout) m_dialog.m_onSelection = [this](int sel) { OnBackoutToMainMenuDialogClose(sel); };
  else m_dialog.m_onSelection = [this](int sel) { OnDialogSelection(sel); };
  m_dialog.m_onZeroInput = [this] { OnCancelDialog(); };
  UIDialog* dialog = &m_dialog;
  m_dialog.m_onClosed = [dialog] { dialog->DialogClosedCallback(); };
  m_dialog.SpawnDialog();
}

// 801B1158
// At the lot list: "cas_confirm_cancel" (no / yes: back to the main menu); else up the tree.
void FAMTarget::OnCancelKeyPressed(char*, char*) {
  if (m_state != kLotSelect) {
    GoUpMenuTree();
    return;
  }
  m_dialog.m_continueType = 0;
  m_dialog.m_field20 = 0;
  static const char* const kSel[3] = {"no", "yes", nullptr};
  SpawnFamDialog(2, kSel, UIText("cas_confirm_cancel"), true);
}

// 801B13DC
// The choice up (wrapping; the family list stops at the top and leaves its second page).
void FAMTarget::OnUpKeyPressed(char*, char*) {
  const int n = GetNumberOfChoicesInState(m_state);
  auto wrap = [n](int& c) {
    --c;
    if (c < 0) c = n - 1;
  };
  if (m_state == kUnoccupied) wrap(m_unoccupiedChoice);
  else if (m_state == kLotSelect) wrap(m_lot);
  else if (m_state == kOccupied) wrap(m_occupiedChoice);
  else if (m_state == kManageFamily) wrap(m_manageChoice);
  else if (m_state == kFamilySelect) {
    if (--m_familyChoice < 0) m_familyChoice = 0;
    if (m_familyChoice == m_numFamilies / 2 - 1 && m_secondPage == 1) m_secondPage = 0;
  }
  UpdateShaders();
  UpdateText();
}

// 801B1500
// The choice down (wrapping; the family list stops at the bottom, its second page from the middle).
void FAMTarget::OnDownKeyPressed(char*, char*) {
  const int n = GetNumberOfChoicesInState(m_state);
  auto wrap = [n](int& c) { c = c + 1 < n ? c + 1 : 0; };
  if (m_state == kUnoccupied) wrap(m_unoccupiedChoice);
  else if (m_state == kLotSelect) wrap(m_lot);
  else if (m_state == kOccupied) wrap(m_occupiedChoice);
  else if (m_state == kManageFamily) wrap(m_manageChoice);
  else if (m_state == kFamilySelect) {
    m_familyChoice = m_familyChoice + 1 < n ? m_familyChoice + 1 : n;
    if (m_familyChoice == m_numFamilies / 2) m_secondPage = 1;
  }
  UpdateShaders();
  UpdateText();
}

// 801B1640
// X (A on GC): see famtarget.hpp for each state. Two players need a family of two or more to enter or
// move in ("twoplayer_need_two_sims").
void FAMTarget::OnXKeyPressed(char*, char*) {
  static const char* const kOk[3] = {"ok", nullptr, nullptr};
  static const char* const kYesNoCancel[3] = {"yes", "no", "cancel"};
  static const char* const kNoYesCancel[3] = {"no", "yes", "cancel"};
  const bool twoPlayer = _gd.m_fieldEC != 0;  // DAT_80475C70
  switch (m_state) {
    case kLotSelect:
      OnLotSelect();
      return;
    case kOccupied:
      if (m_occupiedChoice == 0) {
        if (!m_family) return;
        const int members = GetNumberOfFamilyMembers(m_family);
        if (!twoPlayer || members > 1) {
          LaunchLot();
          return;
        }
        m_dialog.m_continueType = 0;
        SpawnFamDialog(1, kOk, UIText("twoplayer_need_two_sims"), false);
      } else {
        SpawnFamDialog(2, kYesNoCancel, UIText("free_evict_prompt"), false);
      }
      return;
    case kUnoccupied:
      if (m_unoccupiedChoice == 0) {
        UpdateMenuState(kFamilySelect);
        return;
      }
      if (m_unoccupiedChoice != 1) return;
      SpawnFamDialog(2, kYesNoCancel, UIText("free_bulldoze_prompt"), false);
      return;
    case kManageFamily:
      if (m_manageChoice == 0) {
        _globals.m_field448 = ConvertSelectedFamilyIndexToActualGameIndex(m_familyChoice);  // DAT_80476214
        _globals.m_field44C = _globals.m_field448;                                          // DAT_80476218
        UIDB::UIDBSetInt(const_cast<char*>("CAS_lotnumber"), ConvertSelectedLotIndexToActualGameIndex(m_lot));
        UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("CAF_"));
        return;
      }
      if (m_manageChoice == 1) {
        m_dialog.m_continueType = 0;
        m_dialog.m_field14 = 0;
        m_dialog.m_field20 = 0;
        SpawnFamDialog(2, kNoYesCancel, UIText("free_family_delete_prompt"), false);
        return;
      }
      if (m_manageChoice == 2) {
        const int members = GetNumberOfFamilyMembers(m_family);
        const int lot = ConvertSelectedLotIndexToActualGameIndex(m_lot);
        const int test = static_cast<int>(HoodManager::TestMoveInFamily(lot - 1, m_family));
        if (test < 0) {
          // "free_not_enough_money" with the lot's price and the family's money.
          m_dialog.m_continueType = 0;
          m_dialog.m_field20 = 0;
          unsigned short lotMoney[0x100], familyMoney[0x100];
          ConvertNumericMoneyToString(GetMoneyForLot(lot), lotMoney);
          UIDB::UIDBSetString(const_cast<char*>("simoleons"), lotMoney);
          ConvertNumericMoneyToString(GetMoneyForFamily(m_family, nullptr), familyMoney);
          UIDB::UIDBSetString(const_cast<char*>("simoleonsP2"), familyMoney);
          unsigned short body[0x400];
          Copy16(body, UIText("free_not_enough_money"));
          UIQDTarget::UIQDSubstitutionStrings(body);
          SpawnFamDialog(1, kOk, body, false);
          return;
        }
        if (!twoPlayer || members > 1) {
          FamilyImpl* family = GetFamilyInFamilyList(m_familyChoice);
          if (!family) return;
          if (!HoodManager{}.MoveInFamily(lot - 1, family, true)) return;
          UpdateMenuState(kOccupied);
          return;
        }
        m_dialog.m_continueType = 0;
        m_dialog.m_field20 = 0;
        SpawnFamDialog(1, kOk, UIText("twoplayer_need_two_sims"), false);
      }
      return;
    case kFamilySelect: {
      FamilyImpl* family = GetFamilyInFamilyList(m_familyChoice);
      if (!family) {
        m_family = nullptr;
        family = Globs_pNeighborhood->MakeNewFreePlayFamily(ConvertSelectedFamilyIndexToActualGameIndex(m_familyChoice));
        if (!family) return;
        UIDB::UIDBSetInt(const_cast<char*>("CAS_lotnumber"), ConvertSelectedLotIndexToActualGameIndex(m_lot));
        UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("CAF_"));
      } else {
        m_family = family;
        if (family->GetHouseNumber() == 0) UpdateMenuState(kManageFamily);
      }
      _gd.m_fieldD8 = family->GetNumber();  // DAT_80475C5C: the player's family
      _globals.m_field448 = _gd.m_fieldD8;
      _globals.m_field44C = _gd.m_fieldD8;
      return;
    }
    default:
      return;
  }
}

// 801B2604
void FAMTarget::OnCircleKeyPressed(char*, char*) {}

// 801B2608
// Goes to `state` and keeps the path: an empty lot entered from an occupied one (evicted) starts the
// path again at the lot list; an occupied lot entered from the family manager (moved in) likewise;
// the family manager entered from an occupied lot goes back to the lot list. Opening a lot from the
// list takes its family as the player's.
void FAMTarget::UpdateMenuState(int state) {
  const int prev = m_state;
  m_prevState = prev;
  m_state = state;
  bool restart = false;  // the path cleared and set to (lot list, state)
  if (state == kUnoccupied) {
    if (m_pushState) m_family = nullptr;
    if (m_prevState == kOccupied) restart = true;
  } else if (state == kManageFamily) {
    if (prev == kFamilySelect) {
      m_family = GetFamilyInFamilyList(m_familyChoice);
    } else if (prev == kOccupied) {
      m_stateStack.clear();
      m_pushState = 1;
      UpdateMenuState(kLotSelect);
    }
  } else if (state == kOccupied) {
    if (m_pushState && prev == kLotSelect) {
      m_family = Globs_pNeighborhood->GetFamilyInHouse(ConvertSelectedLotIndexToActualGameIndex(m_lot));
      if (m_family) {
        _gd.m_fieldD8 = m_family->GetNumber();
        _globals.m_field448 = _gd.m_fieldD8;
        _globals.m_field44C = _gd.m_fieldD8;
      }
    } else if (m_prevState == kManageFamily) {
      restart = true;
    }
  }
  if (restart) {
    m_stateStack.clear();
    m_stateStack.push_back(kLotSelect);
    m_stateStack.push_back(state);
    m_pushState = 0;
  }
  UpdateShaders();
  if (m_pushState == 1) m_stateStack.push_back(state);
  m_pushState = 1;
  AptViewer_CallFunction(const_cast<char*>("updateStateFAM"), nullptr, nullptr, 0, nullptr);
}

// 801B2994
int FAMTarget::GetNumberOfChoicesInState(int state) {
  switch (state) {
    case kLotSelect: return m_numLots;
    case kOccupied: return 2;
    case kUnoccupied: return 2;
    case kManageFamily: return 3;
    case kFamilySelect: return m_numFamilies;
    default: return -1;
  }
}

// 801B29F0
// The free-play lots are houses 13..16.
int FAMTarget::ConvertSelectedLotIndexToActualGameIndex(int index) { return index + 0xd; }

// 801B29F8
// Back to the state before (not above the top).
void FAMTarget::GoUpMenuTree() {
  if (m_stateStack.size() == 1) return;
  m_pushState = 0;
  m_stateStack.pop_back();
  UpdateMenuState(m_stateStack.back());
}

// 801B2B00
// The lot's family's surname (its first member's), or "free_unoccupied".
void FAMTarget::GetNameForFamilyInLotSelect(char*, unsigned short* out) {
  FamilyImpl* family = Globs_pNeighborhood->GetFamilyInHouse(ConvertSelectedLotIndexToActualGameIndex(m_lot));
  if (!family) {
    Copy16(out, UIText("free_unoccupied"));
    return;
  }
  int* guid = family->GetIndexedMember(0);
  Neighbor* n = guid ? Globs_pNeighborhood->FindNeighborByGUID(*guid) : nullptr;
  if (n && n->m_selector) Copy16(out, reinterpret_cast<const unsigned short*>(n->m_selector->GetUserLastName().c_str()));
}

// 801B2C14
int FAMTarget::GetMoneyForFamilyInLotSelect(char*, unsigned short* out) {
  FamilyImpl* family = Globs_pNeighborhood->GetFamilyInHouse(ConvertSelectedLotIndexToActualGameIndex(m_lot));
  if (!family) return 0;
  const int funds = family->GetFunds();
  if (out) UIObjectBase::makeMoneyString(funds, out, true);
  return funds;
}

// 801B2CD4
// The eight families' heads (blank for an empty slot or a missing member).
void FAMTarget::UpdateFamilySelectShaders() {
  for (int row = 0; row < 8; ++row) {
    FamilyImpl* family = Globs_pNeighborhood->GetFamily(ConvertSelectedFamilyIndexToActualGameIndex(row));
    for (int head = 0; head < 4; ++head) InstallHead(this, SelectFamilyShader(row, head), family, head);
  }
}

// 801B2EC0
// GC: the lot's house name text of the lot index computed but dropped (the house of the register's
// previous value); the description is the "HouseData" row's capitals text of that name.
void FAMTarget::GetSelectedLotDescription(unsigned short* out) {
  const int lot = ConvertSelectedLotIndexToActualGameIndex(m_lot);
  const unsigned short* const* text = _globals.HouseNameCapsText(GetHouseNameText(lot));
  Copy16(out, text ? *text : nullptr);
}

// 801B2F28
void FAMTarget::ConvertNumericMoneyToString(int amount, unsigned short* out) { UIObjectBase::makeMoneyString(amount, out, true); }

// 801B2F8C
int FAMTarget::GetMoneyForFamily(Family* family, unsigned short* out) {
  if (!family) return 0;
  const int funds = static_cast<FamilyImpl*>(family)->GetFunds();
  if (out) UIObjectBase::makeMoneyString(funds, out, true);
  return funds;
}

// 801B3024
// The house's name ("HouseData" row "house NN").
const unsigned short* FAMTarget::GetLotName(int lot) {
  const unsigned short* const* text = _globals.HouseNameText(GetHouseNameText(lot));
  return text ? *text : nullptr;
}

// 801B3074
// The family's surname (its first member's).
void FAMTarget::GetNameForFamily(Family* family, unsigned short* out) {
  if (!family) return;
  int* guid = static_cast<FamilyImpl*>(family)->GetIndexedMember(0);
  Neighbor* n = guid ? Globs_pNeighborhood->FindNeighborByGUID(*guid) : nullptr;
  if (n && n->m_selector) Copy16(out, reinterpret_cast<const unsigned short*>(n->m_selector->GetUserLastName().c_str()));
}

// 801B3130
// The movie moved the highlight: the state's choice (a family picked in the list becomes the player's).
void FAMTarget::SetCurrentChoice(char* value) {
  const int choice = std::atoi(value);
  if (m_state == kLotSelect) {
    m_lot = choice;
    UpdateShaders();
  } else if (m_state == kOccupied) {
    m_occupiedChoice = choice;
  } else if (m_state == kUnoccupied) {
    m_unoccupiedChoice = choice;
  } else if (m_state == kManageFamily) {
    m_manageChoice = choice;
  } else if (m_state == kFamilySelect) {
    m_familyChoice = choice;
    if (FamilyImpl* family = GetFamilyInFamilyList(choice)) {
      _gd.m_fieldD8 = family->GetNumber();
      _globals.m_field448 = _gd.m_fieldD8;
      _globals.m_field44C = _gd.m_fieldD8;
    }
  }
  UpdateShaders();
}

// 801B3204
FamilyImpl* FAMTarget::GetFamilyInLot(int index) {
  return Globs_pNeighborhood->GetFamilyInHouse(ConvertSelectedLotIndexToActualGameIndex(index));
}

// 801B3244
// Into the chosen lot ("INLEVEL": PreGameFreeplayState loads it).
void FAMTarget::LaunchLot() {
  _app.SetStartLot(ConvertSelectedLotIndexToActualGameIndex(m_lot));
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("INLEVEL"));
}

// 801B3284
// The lot's price (HouseInfo +0x0C) from the neighbourhood's house section.
int FAMTarget::GetMoneyForLot(int lot) {
  HouseInfo info;
  info.m_family = -1;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  hood->m_houseNumber = lot;  // GC: NeighborhoodImpl +0x70 set to the lot first
  hood->GetHouseInfo(Globs_pNghResFile, lot, &info);
  return info.m_price;
}

// 801B3314
void FAMTarget::EvictFamily() {
  const int lot = ConvertSelectedLotIndexToActualGameIndex(m_lot);
  PlaySample(0x3fe4572bu);
  HoodManager::EvictFamilyAndLiquidateAssets(lot - 1);
  UpdateMenuState(kUnoccupied);
}

// 801B3388
int FAMTarget::GetLotForFamily(Family* family) { return family ? static_cast<FamilyImpl*>(family)->GetHouseNumber() : 0; }

// 801B33CC
int FAMTarget::GetLotForFamily(int index) {
  FamilyImpl* family = GetFamilyInFamilyList(index);
  return family ? GetLotForFamily(static_cast<Family*>(family)) : 0;
}

// 801B340C
// "FAM_Select_Address_0N": the name of the free-play lot the family lives on (none: null).
const unsigned short* FAMTarget::GetLotNameInFamilySelect(char* name) {
  const size_t len = std::strlen(name);
  if (!len) return nullptr;
  const char digit[2] = {name[len - 1], 0};
  FamilyImpl* family = GetFamilyInFamilyList(std::atoi(digit));
  if (!family) return nullptr;
  for (int lot = 0; lot < 4; ++lot) {
    FamilyImpl* f = GetFamilyInLot(lot);
    if (f && f == family) return GetLotName(ConvertSelectedLotIndexToActualGameIndex(lot));
  }
  return nullptr;
}

// ---------------------------------------------------------------------------------------------------
// PreGameFreeplayState

// 80095408
PreGameFreeplayState::PreGameFreeplayState(int id) : StateMachineState(id) {}

// 80094EEC
// The default names for the players' sims, Create-a-Sim's family mode (3), and the screen.
void PreGameFreeplayState::Startup() {
  UIDB::UIDBSetString(const_cast<char*>("Player1FirstName"), EGlobal::NewUIText("default_player1_first_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player1LastName"), EGlobal::NewUIText("default_player1_last_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player2FirstName"), EGlobal::NewUIText("default_player2_first_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player2LastName"), EGlobal::NewUIText("default_player2_last_name"));
  UIDB::UIDBSetInt(const_cast<char*>("CAS_mode"), 3);
  m_fam = new FAMTarget;
}

// 8009501C
void PreGameFreeplayState::Shutdown() {
  delete m_fam;
  m_fam = nullptr;
}

// 80095060
void PreGameFreeplayState::Reset() {
  LoadingScreenStateMachine::LoadFinishing();
  GameData::SetBlackGraphicsBackgroundColor();
}

// 8009508C
// Back from Create-a-Family (the owner's return value: 1 left, 2 accepted): the screen again. Then the
// UI's flow request, each once the loading screen is up: "INLEVEL" (6) enters the chosen lot (live
// mode, state 13); "CAF_" (4) calls Create-a-Family (state 12); "CASBackoutP1" (0x32) returns to the
// main menu (the front end's movie preloaded first).
void PreGameFreeplayState::Update(float) {
  if (const int ret = m_owner->m_returnVal) {
    m_owner->m_returnVal = 0;
    LoadingScreenStateMachine::LoadFinishing();
    if (m_fam) m_fam->OnReturnFromCreateAFamily(ret == 1);
  }
  if (!IsFlowStateRequestPending()) return;
  const int request = GetFlowStateRequest();
  auto fadedIn = [] {
    return (LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn) != 0;
  };
  if (request == FlowGotoTarget::kInLevel) {
    if (!LoadingScreenStateMachine::LoadInProgress()) {
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
    } else if (fadedIn()) {
      SetFlowStateCurrent();
      gbDrawUI = 0;
      gbUpdateUI = 0;
      // GC: DAT_80476230 (the house) = -1 first; PlayerEnterHouse sets it.
      GameData::UseConfigOptions();
      _gd.PlayerEnterHouse();
      GameData::GotoLiveMode();
      OwnerSetNextState(TheSimsStateMachine::kInLevelStory, 0.0f);
    }
  } else if (request == FlowGotoTarget::kCAF) {
    if (!LoadingScreenStateMachine::LoadInProgress()) {
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{3});
    } else if (fadedIn()) {
      SetFlowStateCurrent();
      if (m_fam) m_fam->UnloadScreen();
      gbDrawUI = 0;
      gbUpdateUI = 0;
      OwnerCallState(TheSimsStateMachine::kPreGameCreateAFamily);
    }
  } else if (request == 0x32) {  // "CASBackoutP1"
    if (!LoadingScreenStateMachine::LoadInProgress()) {
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{0});
    } else if (fadedIn()) {
      _flashman.EResourceManager::AddRef(0xba2f3a39u, nullptr, 0);
      _flashman.DelRef(0xba2f3a39u, EResourceManager__DelRefMode{1});
      SetFlowStateCurrent();
      gbDrawUI = 0;
      gbUpdateUI = 0;
      OwnerReturnFromState(1);
    }
  }
}

// 80095444
int PreGameFreeplayState::IsFlowStateRequestPending() {
  return static_cast<TheSimsStateMachine*>(m_owner)->IsFlowStateRequestPending();
}
// 80095468
int PreGameFreeplayState::GetFlowStateRequest() { return static_cast<TheSimsStateMachine*>(m_owner)->GetFlowStateRequest(); }
// 8009548C
void PreGameFreeplayState::SetFlowStateCurrent() { static_cast<TheSimsStateMachine*>(m_owner)->SetFlowStateCurrent(); }

// ---------------------------------------------------------------------------------------------------
// PreGameCreateAFamilyState

// 80095370
PreGameCreateAFamilyState::PreGameCreateAFamilyState(int id) : CreateASimBaseState(id) {}

// 80094C6C
// Create-a-Sim on the family picked in the neighbourhood screen (EGlobal +0x448).
void PreGameCreateAFamilyState::Reset() {
  CreateASimBaseState::Reset();
  _gd.PrepCreateAFamilyData(_globals.m_field448);  // DAT_80476214
}

// 80094CA0
// Once CAS is accepted or left, back to the neighbourhood screen behind the loading screen (2
// accepted, 1 left).
void PreGameCreateAFamilyState::Update(float dt) {
  CreateASimBaseState::Update(dt);
  const bool accepted = !m_waitFadeIn && m_accepted;
  const bool canceled = !m_waitFadeIn && m_canceled;
  if (!accepted && !canceled) return;
  if (!LoadingScreenStateMachine::LoadInProgress()) {
    LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{2});
  } else if (LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn) {
    if (accepted) OwnerReturnFromState(2);
    else if (canceled) OwnerReturnFromState(1);
  }
}
