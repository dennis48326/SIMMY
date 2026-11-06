// Port of the pause menu's wants and fears submenu, WXFTarget (game/targets_s2c.cpp). See
// wxftarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }

unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

void InstallShader(UIObjectBase* t, const char* name, unsigned int id) {
  UI2D::Instance()->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(t->m_name), 0);
}

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  if (s)
    while (s[n]) ++n;
  return n;
}
// The movie's copy of a localizable string (GC: SimsAptMalloc'd, after the UI string substitution).
unsigned short* FinishLocalizable(unsigned short* s) {
  UIQDTarget::UIQDSubstitutionStrings(s);
  const std::size_t n = Len16(s);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}
void CopyString(unsigned short* dst, const unsigned short* const* slot) {
  const unsigned short* src = slot ? *slot : nullptr;
  std::size_t n = 0;
  if (src)
    for (; src[n] && n < 0xfff; ++n) dst[n] = src[n];
  dst[n] = 0;
}

// The pause menu's player's sim and its neighbour (GC: Neighborhood FindNeighborByID(person
// vtable 0x1B4)).
Neighbor* PlayerNeighbor(int player) { return PersonNeighbor(_globals.GetSelectedPerson(player)); }
}  // namespace

// 801E4F2C
// The submenu for the pause menu's player: its variables, tab and icon shaders, sounds, the screen
// (0x27), the icons' shaders, then it takes every pad command (both players in split screen).
WXFTarget::WXFTarget(int player) : m_player(player) {
  AttachTables("WXFTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"WXF_exists", "WXF_Aspiration", "WXF_CancelEnabled"}) m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"WXF_CANCEL_BUTTON", "WXF_DPAD_LEFT", "WXF_DPAD_RIGHT", "WXF_Set_Menu_State", "WXF_Close_Screen",
                        "WXF_ShowHelp"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"WXF_Fears_Text", "WXF_Wants_Text", "WXF_Icon_Text_0", "WXF_Icon_Text_1", "WXF_Icon_Text_2",
                        "WXF_Icon_Text_3", "WXF_Icon_Text_Fears_0", "WXF_Icon_Text_Fears_1", "WXF_Icon_Text_Fears_2",
                        "WXF_Aspiration_Text", "WXF_Aspiration_Label", "WXF_Aspiration_Description"})
    m_locTable->InstallCommand(n, loc, this);
  InstallShader(this, "WXF_sidepanel_backdrop", 0x7eaf06c1);
  for (const char* n : {"WXF_shdr00", "WXF_shdr01", "WXF_shdr10", "WXF_shdr11", "WXF_shdr20", "WXF_shdr21", "WXF_shdr30",
                        "WXF_shdr31", "WXF_Fears_shdr00", "WXF_Fears_shdr01", "WXF_Fears_shdr10", "WXF_Fears_shdr11",
                        "WXF_Fears_shdr20", "WXF_Fears_shdr21"})
    InstallShader(this, n, 0x9f93ff74);
  UIAUDIO* audio = UIAUDIO::Instance();
  const struct {
    const char* name;
    unsigned int id;
  } sounds[] = {{"WXF_UI_BROWSE_KBD_SAMPLE", 0xcb2fa7cd}, {"WXF_UI_LEVEL_ADJUST_SAMPLE", 0xb837eaf4},
                {"WXF_UI_CANCEL_SAMPLE", 0x2aaff358},     {"WXF_UI_SELECT_SAMPLE", 0x374bdcea},
                {"WXF_UI_BACK_SAMPLE", 0x048ae94f},       {"WXF_SndTabChanged", 0x374bdcea},
                {"WXF_SndError", 0x2aaff358}};
  for (const auto& s : sounds) audio->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(m_name));
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x27, m_player);
  UpdateShaders();
  m_filters[0] = PushFilterAll(0, "WXFTarget");
  m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "WXFTarget") : 0;  // DAT_80475C70 two players
}

// 801E5B14
WXFTarget::~WXFTarget() {
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x27);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801E5CBC
void WXFTarget::SetVariable(char* name, char* value) {
  if (!std::strcmp("WXF_Close_Screen", name)) {
    if (!m_help.m_active) Shutdown();
  } else if (!std::strcmp("WXF_CANCEL_BUTTON", name)) {
    OnCancelKeyPressed(name, value);
  } else if (!std::strcmp("WXF_Set_Menu_State", name)) {
    UpdateMenuState(name, value);
  } else if (!std::strcmp("WXF_DPAD_LEFT", name)) {
    OnLeftKeyPressed(name, value);
  } else if (!std::strcmp("WXF_DPAD_RIGHT", name)) {
    OnRightKeyPressed(name, value);
  } else if (!std::strcmp("WXF_ShowHelp", name)) {
    ShowHelp();
  }
}

// 801E5DDC
// The page headers, the icons' texts ("WXF_Icon..."), the aspiration's title, kind and description.
unsigned short* WXFTarget::GetLocalizable(char* name) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  static const char* const kAspirationLabel[5] = {"cas_asp_romance", "cas_asp_wealth", "cas_asp_knowledge",
                                                  "cas_asp_popularity", "cas_asp_creativity"};
  static const char* const kAspirationDesc[5] = {"cas_asp_romance_desc", "cas_asp_wealth_desc", "cas_asp_knowledge_desc",
                                                 "cas_asp_popularity_desc", "cas_asp_creativity_desc"};
  auto aspiration = [&]() {
    const int a = GetAspirationIcon();
    return a >= 0 && a <= 3 ? a : 4;
  };
  if (!std::strcmp("WXF_Fears_Text", name)) {
    CopyString(s, _globals.GetPDAString(const_cast<char*>("header_fears")));
  } else if (!std::strcmp("WXF_Wants_Text", name)) {
    CopyString(s, _globals.GetPDAString(const_cast<char*>("header_wants")));
  } else if (!std::strncmp(name, "WXF_Icon", 8)) {
    unsigned short* text = UpdateWAFText(name);
    std::size_t n = 0;
    for (; text[n] && n < 0xfff; ++n) s[n] = text[n];
    s[n] = 0;
    SimsAptFree(text);
  } else if (!std::strcmp(name, "WXF_Aspiration_Text")) {
    CopyString(s, _globals.GetNewUIString(const_cast<char*>("cas_title_aspirations")));
  } else if (!std::strcmp(name, "WXF_Aspiration_Label")) {
    CopyString(s, _globals.GetNewUIString(const_cast<char*>(kAspirationLabel[aspiration()])));
  } else if (!std::strcmp(name, "WXF_Aspiration_Description")) {
    CopyString(s, _globals.GetNewUIString(const_cast<char*>(kAspirationDesc[aspiration()])));
  }
  return FinishLocalizable(s);
}

// 801E6194
char* WXFTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("WXF_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  } else if (!std::strcmp("WXF_Aspiration", name)) {
    unsigned int a = static_cast<unsigned int>(GetAspirationIcon());
    if (a > 4) a = 0;
    std::snprintf(out, 0x20, "%d", a);
  } else if (!std::strcmp("WXF_CancelEnabled", name)) {
    std::snprintf(out, 0x20, "%d", m_help.m_active == 0);
  }
  return out;
}

// 801E629C
void WXFTarget::Shutdown() {
  UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));  // DAT_80475EF8
}

// 801E62E8
void WXFTarget::Update() {}

// 801E62EC
// The movie's page: 0 wants, 1 fears (the page's text shown), 2; the tab icons follow.
void WXFTarget::UpdateMenuState(char*, char* value) {
  const int state = std::atoi(value);
  if (state == 0 || state == 1) {
    m_prevState = m_state;
    m_state = state;
    UpdateText();
  } else if (state == 2) {
    m_prevState = m_state;
    m_state = 2;
  }
  UpdateShaders();
}

// 801E6370
void WXFTarget::UpdateText() {
  UIReflow reflow;
  reflow.OpenReflowBlock();
  if (m_state == 0 || m_state == 1) {
    reflow.Reflow_Id("WXF_Wants_Container", 0);
    reflow.Reflow_Visibility(m_state == 0);
    reflow.Reflow_Id("WXF_Fears_Container", 0);
    reflow.Reflow_Visibility(m_state == 1);
  }
  reflow.SubmitReflowBlock();
}

// 801E6454
// Left and right switch between the wants and the fears page (through the movie's own setter).
void WXFTarget::OnLeftKeyPressed(char*, char*) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", m_state == 0);
  SetVariable(const_cast<char*>("WXF_Set_Menu_State"), buf);
}
// 801E64BC
void WXFTarget::OnRightKeyPressed(char*, char*) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", m_state == 0);
  SetVariable(const_cast<char*>("WXF_Set_Menu_State"), buf);
}
// 801E6524
void WXFTarget::OnCancelKeyPressed(char*, char*) {
  UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));
}

// 801E6570
// The tab icons for the page, then each want's and fear's backdrop (by kind) and icon.
void WXFTarget::UpdateShaders() {
  if (m_state == 0) {
    InstallShader(this, "WXF_TabIcon0", 0xf453dee4);
    InstallShader(this, "WXF_TabIcon1", 0xc3d410e2);
    InstallShader(this, "WXF_TabIcon2", 0x1f25b0f1);
  } else if (m_state == 1) {
    InstallShader(this, "WXF_TabIcon0", 0x4c30d93c);
    InstallShader(this, "WXF_TabIcon1", 0xce1b8c37);
    InstallShader(this, "WXF_TabIcon2", 0x1f25b0f1);
  } else {
    InstallShader(this, "WXF_TabIcon0", 0x4c30d93c);
    InstallShader(this, "WXF_TabIcon1", 0xc3d410e2);
    InstallShader(this, "WXF_TabIcon2", 0xb4c6efa1);
  }
  static const char* const kSlots[7][2] = {
      {"WXF_shdr00", "WXF_shdr01"},           {"WXF_shdr10", "WXF_shdr11"},
      {"WXF_shdr20", "WXF_shdr21"},           {"WXF_shdr30", "WXF_shdr31"},
      {"WXF_Fears_shdr00", "WXF_Fears_shdr01"}, {"WXF_Fears_shdr10", "WXF_Fears_shdr11"},
      {"WXF_Fears_shdr20", "WXF_Fears_shdr21"}};
  Neighbor* neighbor = PlayerNeighbor(m_player);
  for (int i = 0; i < 7; ++i) {
    WantFearSlotInfo info;
    NeighborWantFearSlot(neighbor, i, &info);  // Neighbor +0x198 + 4i: WantFear::Bookmark
    InstallShader(this, kSlots[i][0], GetBackgroundShader(info.type));
    InstallShader(this, kSlots[i][1], info.base);
  }
}

// 801E6B84
// The icon's backdrop by want kind (tWantType): 0 want, 0x80 fear, 4 gold, 1 platinum.
unsigned int WXFTarget::GetBackgroundShader(int type) {
  switch (type) {
    case 0: return 0xa0d97dcf;
    case 0x80: return 0x8ca1c6a6;
    case 4: return 0x4364efb7;
    case 1: return 0x087b35fb;
    default: return 0;
  }
}

// 801E6BD8
// The text of the icon "WXF_Icon_Text_<n>" / "..._Fears_<n>" (the fourth want shows on the wants page
// only).
unsigned short* WXFTarget::UpdateWAFText(char* name) {
  UIReflow reflow;
  reflow.ResetVisibility("WXF_Icon_Text_3", m_state == 0, 0);
  static const char* const kNames[7] = {"WXF_Icon_Text_0", "WXF_Icon_Text_1", "WXF_Icon_Text_2", "WXF_Icon_Text_3",
                                        "WXF_Icon_Text_Fears_0", "WXF_Icon_Text_Fears_1", "WXF_Icon_Text_Fears_2"};
  int slot = 0;
  for (int i = 0; i < 7; ++i)
    if (!std::strcmp(name, kNames[i])) slot = i;
  WantFearSlotInfo info;
  NeighborWantFearSlot(PlayerNeighbor(m_player), slot, &info);  // WantFear::Bookmark::GetText(.., 0)
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((info.text.size() + 1) * 2));
  for (std::size_t i = 0; i < info.text.size(); ++i) out[i] = info.text[i];
  out[info.text.size()] = 0;
  return out;
}

// 801E6DF0
void WXFTarget::onHelpDialogSelection(int) {
  if (++m_helpPage < 3) ShowHelp();
  else m_helpPage = 0;
}

// 801E6E30
// The help's page (three: the summary, the second summary, the body); "ok" leads to the next page,
// the last one closes with "close_help".
void WXFTarget::ShowHelp() {
  static const char* const kHelpBodyIDs[3] = {"wf_help_summary_body", "wf_help_summary2_body", "wf_help_body"};  // 803F8108 WXF_HelpBodyIDs
  const unsigned int page = m_helpPage < 3 ? m_helpPage : 2;
  m_help.SpawnHelpDialog(m_player, "wf_help_title", kHelpBodyIDs[page], [this](int choice) { onHelpDialogSelection(choice); },
                         false, page < 2 ? "ok" : nullptr);  // DAT_803EF83C "ok"
}

// 801E6EC0
// The selected sim's aspiration (person data 0x14: 0 romance, 1 wealth, 2 knowledge, 3 popularity,
// 4 creativity).
int WXFTarget::GetAspirationIcon() {
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  return person ? person->GetPersonData(0x14) : 0;
}
