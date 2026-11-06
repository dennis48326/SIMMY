// Port of the HUD's context menu widget, CSMTarget (game/targets_s2c.cpp). See csmtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
int TwoPlayers() { return _gd.m_fieldEC != 0; }  // DAT_80475C70

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  if (s)
    while (s[n]) ++n;
  return n;
}
void Copy16(unsigned short* dst, const std::u16string& s) {
  for (char16_t c : s) *dst++ = static_cast<unsigned short>(c);
  *dst = 0;
}
}  // namespace

// 801A61D0
// The target's names ("<player>CXM..."), the row/arrow/backdrop shaders, then the widget's screen.
CSMTarget::CSMTarget(int player) : m_player(player) {
  const std::string pre = std::to_string(player);
  AttachTables(player == 0 ? "0CXMTarget" : "1CXMTarget");  // 804FE0F8 CXM_TargetName
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand((pre + "CXM_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"CXM_show_callback", "CXM_hide_callback", "CXM_hourglass_show_callback",
                        "CXM_hourglass_hide_callback"})
    m_setTable->InstallCommand((pre + n).c_str(), set, this);
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"CXM_Option0", "CXM_Option1", "CXM_Option2", "CXM_ObjName"})
    m_locTable->InstallCommand((pre + n).c_str(), loc, this);
  UI2D* ui2d = UI2D::Instance();
  auto shader = [&](const char* id, unsigned int shaderId) {
    ui2d->InstallEntry(const_cast<char*>((pre + id).c_str()), shaderId, const_cast<char*>(m_name), 0);
  };
  shader("CXM_arrow_up", 0x90a4eab9);
  shader("CXM_arrow_down", 0x1731b45e);
  shader("CXM_textshdr0", 0x6c87dfc2);
  shader("CXM_textshdr1", 0xa7fc0113);
  shader("CXM_textshdr2", 0x6c87dfc2);
  shader("CXM_nameshdr", 0x9a7dd168);
  shader("CXM_item_backdrop", 0xb6c89b1b);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(m_player == 0 ? 0x32 : 0x33, 0);
}

// 801A6980
CSMTarget::~CSMTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x32 : 0x33);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801A6B34
// The movie's tween callbacks: the list (hidden at the end of its fade out) and the hourglass.
void CSMTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("CXM_show_callback", name)) {
    m_tweening = 0;
  } else if (!std::strcmp("CXM_hide_callback", name)) {
    UIReflow().ResetVisibility("CXM_list", false, m_player);
    m_tweening = 0;
    m_shown = 0;
  } else if (!std::strcmp("CXM_hourglass_show_callback", name)) {
    m_hourglassTweening = 0;
  } else if (!std::strcmp("CXM_hourglass_hide_callback", name)) {
    UIReflow().ResetVisibility("CXM_hourglass_swf", false, m_player);
    m_hourglassTweening = 0;
    m_hourglassShown = 0;
  }
}

// 801A6C3C
char* CSMTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("CXM_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801A6CD4
// "CXM_Option<n>" a row's text, "CXM_ObjName" the object's name (after the UI string substitution).
unsigned short* CSMTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  if (!std::strncmp("CXM_Option", name, 10)) {
    int n = 0;
    std::sscanf(name + 10, "%d", &n);
    if (n >= 0 && n < 6) Copy16(s, m_options[n]);
  } else if (!std::strcmp("CXM_ObjName", name)) {
    Copy16(s, m_objName);
  }
  UIQDTarget::UIQDSubstitutionStrings(s);
  const std::size_t len = Len16(s);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((len + 1) * 2));
  std::memcpy(out, s, (len + 1) * 2);
  SimsAptFree(s);
  return out;
}

// 801A6E00
// Fades the list in (player 1's at x -320 in split screen, unless player 1 is in build/buy) or out as
// the menu asks, and the hourglass while the sim waits or walks.
void CSMTarget::Update() {
  if (!m_exists) return;
  if (!m_tweening) {
    if (m_wantShown && !m_shown) {
      UIReflow r;
      r.OpenReflowBlock();
      r.Reflow_Id("CXM_list", m_player);
      r.Reflow_Visibility(true);
      if (TwoPlayers() && m_player == 0) {
        r.Reflow_X(_gd.GetBuildBuyPlayer() == 0 ? 0 : -320);
      }
      r.Reflow_Id("CXM_IconShader", m_player);
      r.Reflow_Visibility(m_iconEnabled != 0);
      r.Reflow_Id("CXM_item_backdrop", m_player);
      r.Reflow_Visibility(m_iconEnabled != 0);
      r.SubmitReflowBlock();
      r.OpenTweenBlock("CXM_list", 100, 0, "CXM_show_callback", m_player);
      r.Tween_Alpha(0xff);
      r.SubmitTweenBlock();
      m_shown = 1;
      m_tweening = 1;
      if (H2DTarget* h2d = _gd.m_h2dTarget) h2d->CSIMActive(m_player, true);
    } else if (!m_wantShown && m_shown) {
      UIReflow r;
      r.OpenTweenBlock("CXM_list", 0xfa, 0, "CXM_hide_callback", m_player);
      r.Tween_Alpha(0);
      r.SubmitTweenBlock();
      m_tweening = 1;
      if (H2DTarget* h2d = _gd.m_h2dTarget) h2d->CSIMActive(m_player, false);
    }
  }
  // GC: shown with the sim's name and "csm_more" while the selected person is in a context-sim state
  // (vtable 0xAC, then 0x174 or 0x17C) and the player's direct interactor is active.
  const bool context = false;
  if (!context && m_contextShown) m_wantShown = 0;
  // The hourglass while the menu is closed and the selected person waits (GetIdleState, vtable 0x16C,
  // is 3: e.g. the social trees set person data 0 to 3 while the partner is on the way) or walks a
  // route (its animator's +0x10 is 1: SAnimator2::IsRouting).
  m_hourglassWanted = 0;
  if (!m_shown) {
    cXPerson* selected = _globals.GetSelectedPerson(m_player);
    cXObject* object = selected ? selected->AsObject() : nullptr;
    auto* person = object ? dynamic_cast<cXPersonImpl*>(object->GetObjectImplementation()) : nullptr;
    if (person) {
      const SAnimator2* animator = person->GetSAnimator();
      if (person->GetIdleState() == 3 || (animator && animator->m_routing == 1)) m_hourglassWanted = 1;
    }
  }
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;  // PC dev trace
  if (!m_hourglassTweening) {
    if (!m_hourglassWanted) {
      if (m_hourglassShown) {
        UIReflow r;
        r.OpenTweenBlock("CXM_hourglass_swf", 0xfa, 0, "CXM_hourglass_hide_callback", m_player);
        r.Tween_Alpha(0);
        r.SubmitTweenBlock();
        m_hourglassTweening = 1;
        if (trace) std::printf("[hud] player %d: hourglass off\n", m_player + 1);
      }
    } else if (!m_hourglassShown) {
      UIReflow r;
      r.OpenTweenBlock("CXM_hourglass_swf", 0xfa, 0, "CXM_hourglass_show_callback", m_player);
      r.Tween_Alpha(0xff);
      r.SubmitTweenBlock();
      r.ResetVisibility("CXM_hourglass_swf", true, m_player);
      if (TwoPlayers() && m_player == 0) {
        r.OpenReflowBlock();
        r.Reflow_Id("CXM_hourglass", m_player);
        r.Reflow_X(-320);
        r.SubmitReflowBlock();
      }
      m_hourglassShown = 1;
      m_hourglassTweening = 1;
      if (trace) std::printf("[hud] player %d: hourglass on\n", m_player + 1);
    }
  }
}

// 801A764C
void CSMTarget::ShowCSIM() {
  m_wantShown = 1;
  m_contextShown = 0;
}

// 801A7660
void CSMTarget::HideCSIM() {
  if (m_contextShown) return;
  m_wantShown = 0;
}

// 801A7674 IsCSIMActive: inline in csmtarget.hpp.
// 801A7674

// 801A767C
// What the menu shows: the name and the three rows (an empty one hidden), each changed one reflowed
// at font size 11, and the arrows (the interest meter mirrors the up arrow).
void CSMTarget::TempInterface(const std::u16string& name, const std::u16string& option0, const std::u16string& option1,
                              const std::u16string& option2, bool upArrow, bool downArrow) {
  UIReflow r;
  bool open = false;
  auto begin = [&] {
    if (!open) {
      open = true;
      r.OpenReflowBlock();
    }
  };
  if (m_objName != name) {
    begin();
    r.Reflow_Id("CXM_name", m_player);
    m_objName = name;
    r.Reflow_Visibility(!name.empty());
    r.Reflow_Id("CXM_name_text", m_player);
    r.Reflow_FontSizeOverride(0xb);
  }
  const std::u16string* rows[3] = {&option0, &option1, &option2};
  static const char* const kChoice[3] = {"CXM_Choice0", "CXM_Choice1", "CXM_Choice2"};
  for (int i = 0; i < 3; ++i) {
    if (m_options[i] == *rows[i]) continue;
    begin();
    r.Reflow_Id(kChoice[i], m_player);
    m_options[i] = *rows[i];
    r.Reflow_Visibility(!rows[i]->empty());
    r.Reflow_Id(kChoice[i], m_player);
    r.Reflow_FontSizeOverride(0xb);
  }
  if (m_upArrow != static_cast<int>(upArrow)) {
    begin();
    m_upArrow = upArrow;
    r.Reflow_Id("CXM_arrow_up", m_player);
    r.Reflow_Visibility(upArrow);
    if (H2DTarget* h2d = _gd.m_h2dTarget) h2d->INTMirrorUpArrow(m_player, upArrow);
  }
  if (m_downArrow != static_cast<int>(downArrow)) {
    begin();
    m_downArrow = downArrow;
    r.Reflow_Id("CXM_arrow_down", m_player);
    r.Reflow_Visibility(downArrow);
  }
  if (open) r.SubmitReflowBlock();
}

// 801A7A88
// The current row's icon and its backdrop shown or not.
void CSMTarget::SetIconDisable(bool enabled) {
  if (static_cast<int>(enabled) == m_iconEnabled) return;
  m_iconEnabled = enabled;
  if (!m_shown) return;
  UIReflow r;
  r.OpenReflowBlock();
  r.Reflow_Id("CXM_IconShader", m_player);
  r.Reflow_Visibility(m_iconEnabled != 0);
  r.Reflow_Id("CXM_item_backdrop", m_player);
  r.Reflow_Visibility(m_iconEnabled != 0);
  r.SubmitReflowBlock();
}
