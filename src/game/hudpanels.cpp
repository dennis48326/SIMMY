// Port of the HUD's side-panel widget targets from game/targets_s2c.cpp: SKLTarget (the skills
// panel, d-pad up). See hudtarget.hpp. MOTTarget (motives) is in hudtarget.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }

// 80439D64 SKL_enumMap: the person data fields of the panel's rows (cooking, mechanical, charisma,
// body, logic, creativity, cleaning).
const int kSkillField[7] = {0xa, 0xc, 0xb, 0x11, 0x12, 0xf, 9};

// cXPerson's person data (GetPersonData: the object's vtable at +4, slot 0x13C): skill points
// 0..1000, which the person takes from its neighbour's person data.
int PersonSkill(cXPerson* person, int field) { return person ? person->GetPersonData(field) : 0; }

unsigned short* NewLocalizable() {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  return s;
}
// The movie's copy, after the UI string substitution (GC: SimsAptMalloc'd).
unsigned short* FinishLocalizable(unsigned short* s) {
  UIQDTarget::UIQDSubstitutionStrings(s);
  std::size_t n = 0;
  while (s[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}
}  // namespace

// 80177BA4
// The number in decimal as UCS-2.
bool UIObjectBase::makeNumberString(int value, unsigned short* out) {
  if (!out) return false;
  char buf[32];
  const int n = std::snprintf(buf, sizeof buf, "%d", value);
  for (int i = 0; i < n; ++i) out[i] = static_cast<unsigned char>(buf[i]);
  out[n] = 0;
  return true;
}

// 801B8FD8
// The skills icon of the player's d-pad flashes while a skill changed (MODTarget::SetWarning, panel 2).
void H2DTarget::SKLInWarning(int player, bool on) {
  if (MODTarget* mod = m_mod[player & 1]) mod->SetWarning(2, on, true);
}

// ---------------------------------------------------------------------------------------------------
// SKLTarget

// 801E02DC
// The skills panel of `player`: its variables, the seven rows' text, backdrop, dividers and text
// backgrounds, then the screen (0x38, player 2's 0x39).
SKLTarget::SKLTarget(int player) : m_player(player) {
  AttachTables(player == 0 ? "0SKLTarget" : "1SKLTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  const std::string pre = std::to_string(player);
  m_getTable->InstallCommand((pre + "SKL_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"SKL_show_callback", "SKL_hide_callback", "SKL_text_show_callback", "SKL_text_hide_callback"})
    m_setTable->InstallCommand((pre + n).c_str(), set, this);
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (int i = 0; i < 7; ++i)
    m_locTable->InstallCommand((pre + "skill" + std::to_string(i) + "_SKL").c_str(), loc, this);
  UI2D* ui2d = UI2D::Instance();
  auto shader = [&](const std::string& id, unsigned int shaderId) {
    ui2d->InstallEntry(const_cast<char*>((pre + id).c_str()), shaderId, const_cast<char*>(m_name), 0);
  };
  shader("SKL_sidepanel_backdrop", 0x7eaf06c1);
  for (int i = 0; i < 7; ++i) {
    const std::string n = std::to_string(i);
    shader("SKL_divider" + n + "a", 0x3186c4de);
    shader("SKL_divider" + n + "b", 0x3186c4de);
    shader("SKL_textBG" + n, 0x97eb9f9d);
  }
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(player == 0 ? 0x38 : 0x39, 0);
}

// 801E0DC8
SKLTarget::~SKLTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x38 : 0x39);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801E0F34
// The movie's tween callbacks.
void SKLTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("SKL_text_show_callback", name)) {
    m_textTweening = 0;
    m_textShown = 1;
  } else if (!std::strcmp("SKL_text_hide_callback", name)) {
    UIReflow().ResetVisibility("SKL_short_text_container", false, m_player);
    m_textShown = 0;
    m_textTweening = 0;
  } else if (!std::strcmp("SKL_show_callback", name)) {
    m_tweening = 0;
  } else if (!std::strcmp("SKL_hide_callback", name)) {
    m_tweening = 0;
    UIReflow().ResetVisibility("SKL", false, m_player);  // DAT_803F59F4
    m_shown = 0;
  }
}

// 801E104C
char* SKLTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("SKL_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801E10E4
// "skill<n>_SKL": the row's skill level (points / 100), remembered as read.
unsigned short* SKLTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  unsigned short* s = NewLocalizable();
  if (m_person && !std::strncmp(name, "skill", 5) && std::isdigit(static_cast<unsigned char>(name[5])) &&
      !std::strcmp(name + 6, "_SKL")) {
    const int row = name[5] - '0';
    if (row < 7) {
      const int points = PersonSkill(m_person, kSkillField[row]);
      makeNumberString(points / 100, s);
      m_skill[row] = points;
    }
  }
  return FinishLocalizable(s);
}

// 801E140C
// Fades the panel in (player 2's at x 320) or out as asked, and its short text.
void SKLTarget::Update() {
  if (!m_exists) return;
  if (!m_tweening) {
    if (!m_wantShown) {
      if (m_shown) {
        UIReflow reflow;
        reflow.OpenTweenBlock("SKL", 0xfa, 0, "SKL_hide_callback", m_player);
        reflow.Tween_Alpha(0);
        reflow.SubmitTweenBlock();
        m_tweening = 1;
      }
    } else if (!m_shown) {
      UIReflow reflow;
      reflow.OpenReflowBlock();
      reflow.Reflow_Id("SKL", m_player);
      reflow.Reflow_Visibility(true);
      if (m_player != 0) reflow.Reflow_X(0x140);
      reflow.SubmitReflowBlock();
      reflow.OpenTweenBlock("SKL", 0xfa, 0, "SKL_show_callback", m_player);
      reflow.Tween_Alpha(0xff);
      reflow.SubmitTweenBlock();
      m_shown = 1;
      m_tweening = 1;
    }
  }
  if (m_textWanted != m_textShown && !m_textTweening) {
    UIReflow reflow;
    if (!m_textWanted) {
      reflow.OpenTweenBlock("SKL_short_text_container", 0xfa, 0, "SKL_text_hide_callback", m_player);
      reflow.Tween_Alpha(0);
    } else {
      reflow.ResetVisibility("SKL_short_text_container", true, m_player);
      reflow.OpenTweenBlock("SKL_short_text_container", 0xfa, 0, "SKL_text_show_callback", m_player);
      reflow.Tween_Alpha(0xff);
    }
    reflow.SubmitTweenBlock();
    m_textTweening = 1;
  }
}

// 801E1628
void SKLTarget::ShowSkills() { m_wantShown = 1; }
// 801E1634
void SKLTarget::HideSkills() {
  m_textWanted = 0;
  m_wantShown = 0;
}

// 801E1644
// A new sim: every row's text refreshed (font size 14) and its animation back to "a0". The same sim: a
// skill that changed gets its text refreshed and the "highlight" animation for 10 updates (and the
// d-pad's skills warning while the panel is closed); when the last highlight ends the warning goes.
void SKLTarget::UpdateSkillsBar() {
  if (!m_exists) return;
  cXPerson* person = _globals.GetSelectedPerson(m_player);  // DAT_80475E88
  H2DTarget* h2d = _gd.m_h2dTarget;                         // DAT_80475BCC
  char id[16];
  if (person != m_person) {
    m_person = person;
    if (!person) return;
    UIReflow reflow;
    reflow.OpenReflowBlock();
    for (int i = 0; i < 7; ++i) {
      m_skill[i] = PersonSkill(person, kSkillField[i]);
      std::snprintf(id, sizeof id, "SKL_text%i", i);
      reflow.Reflow_Id(id, m_player);
      reflow.Reflow_FontSizeOverride(0xe);
      std::snprintf(id, sizeof id, "SKL_swf%i", i);
      reflow.Reflow_Id(id, m_player);
      reflow.Reflow_SwfAnimation(const_cast<char*>("a0"));  // DAT_803F0CF4
      if (h2d) h2d->SKLInWarning(m_player, false);
      m_highlight[i] = 0;
    }
    reflow.SubmitReflowBlock();
    return;
  }
  if (!person) return;
  UIReflow reflow;
  for (int i = 0; i < 7; ++i) {
    const int points = PersonSkill(person, kSkillField[i]);
    if (points == m_skill[i]) continue;
    reflow.OpenReflowBlock();
    std::snprintf(id, sizeof id, "SKL_text%i", i);
    reflow.Reflow_Id(id, m_player);
    reflow.Reflow_FontSizeOverride(0xe);
    if (m_highlight[i] == 0) {
      std::snprintf(id, sizeof id, "SKL_swf%i", i);
      reflow.Reflow_Id(id, m_player);
      reflow.Reflow_SwfAnimation(const_cast<char*>("highlight"));
    }
    reflow.SubmitReflowBlock();
    m_skill[i] = points;
    m_highlight[i] = 10;
    if (!m_wantShown && h2d) h2d->SKLInWarning(m_player, true);
  }
  bool ended = false, running = false;
  for (int i = 0; i < 7; ++i) {
    if (m_highlight[i] <= 0) continue;
    if (--m_highlight[i] == 0) {
      ended = true;
      UIReflow stop;
      std::snprintf(id, sizeof id, "SKL_swf%i", i);
      stop.OpenReflowBlock();
      stop.Reflow_Id(id, m_player);
      stop.Reflow_SwfAnimation(const_cast<char*>("a0"));
      stop.SubmitReflowBlock();
    } else {
      running = true;
    }
  }
  if (ended && !running && h2d) h2d->SKLInWarning(m_player, false);
}

// 801E19BC
int SKLTarget::IsSkillsActive() { return m_shown; }
// 801E19C4
void SKLTarget::SetTextButtonPressed(bool pressed) { m_textWanted = pressed ? 1 : 0; }

// ---------------------------------------------------------------------------------------------------
// PC accessors for the world side (see hudtarget.hpp)

bool NeighborWantFearSlot(Neighbor* neighbor, int slot, WantFearSlotInfo* out) {
  *out = WantFearSlotInfo();
  std::u16string text;
  if (!WantFearSlot(neighbor, slot, &out->type, &out->base, &out->overlay, &text)) return false;
  out->text.assign(text.begin(), text.end());
  return true;
}

void PersonInitWantFearIcons(cXPerson* person) {
  if (Neighbor* n = PersonNeighbor(person)) n->InitWantFearIcons();
}

// Part of 801C9A0C MODTarget::DrawMoodHeadCallback: the person's ESim's (cXObject +0x14) head draws
// itself (ESims3DHead vtable 0x1C: Draw).
void DrawPersonMoodHead(cXPerson* person, ERC* rc, structDrawCBparams*) {
  auto* impl = dynamic_cast<cXPersonImpl*>(person);
  auto* sim = impl ? dynamic_cast<ESim*>(impl->m_instance) : nullptr;
  if (sim && sim->m_head && sim->GetIsModelLoaded()) sim->m_head->Draw(rc);
}

Neighbor* PersonNeighbor(cXPerson* person) {
  auto* impl = dynamic_cast<cXPersonImpl*>(person);
  return impl ? impl->m_neighbor : nullptr;
}

// ---------------------------------------------------------------------------------------------------
// UIReflow parts the side panels use (game/uireflow.cpp)

// 8017D898
// The element's alpha (0..255 as a percentage), a reflow of its own.
void UIReflow::ResetAlpha(char* id, unsigned int alpha, int player) {
  if (m_open) return;
  std::string b = "<reflow><id>";
  b += player ? "1" : "0";
  b += id;
  b += "</id><alpha>";
  b += std::to_string(alpha * 100 / 0xff);
  b += "</alpha></reflow>";
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer))
    v->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Reflow", b.c_str());
}

// 8017ED50
void UIReflow::Reflow_SwfFile(char* file) {
  AddToBlock("<swffn>");
  AddToBlock(file);
  AddToBlock("</swffn>");
}

// ---------------------------------------------------------------------------------------------------
// H2DTarget's wants and fears hooks

// 801B8E3C
void H2DTarget::NewWantFearIcon(int player, int slot, unsigned int base, unsigned int overlay, int anim,
                                Neighbor* neighbor) {
  if (WAFTarget* waf = m_waf[player]) waf->NewWantsAndFearsIcon(slot, base, overlay, anim, neighbor);
}

// 801B9050
// The wants and fears icon of the player's d-pad flashes (MODTarget::SetWarning, panel 4).
void H2DTarget::WAFInWarning(int player, bool on, bool want) {
  if (MODTarget* mod = m_mod[player & 1]) mod->SetWarning(4, on, want);
}

// ---------------------------------------------------------------------------------------------------
// WAFTarget

namespace {
// WAF_SwfTypes (by WAFTarget::Icon::type).
char* const kWafSwfTypes[4] = {const_cast<char*>("s2c_comp_wants_bkgd.swf"), const_cast<char*>("s2c_comp_fears_bkgd.swf"),
                               const_cast<char*>("s2c_comp_wantsfears_bkgd_gold.swf"),
                               const_cast<char*>("s2c_comp_wantsfears_bkgd_platinum.swf")};
char kSlotMachineSwf[] = "s2c_comp_wantsfears_slotmachine.swf";
// The icons' element names: "WAF_swf<n>" and per player "<p>WAF_shdr<n>0" / "<p>WAF_shdr<n>1".
const char* const kWafSwf[7] = {"WAF_swf0", "WAF_swf1", "WAF_swf2", "WAF_swf3", "WAF_swf4", "WAF_swf5", "WAF_swf6"};
char kWafShdr[2][7][2][16];

void InstallShader(UIObjectBase* t, const char* name, unsigned int id) {
  UI2D::Instance()->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(t->m_name), 0);
}
void PlayUISound(unsigned int id) { UIAUDIO::Instance()->PlaySound(id); }

// cXPersonImpl::GetAspirationScore (801256EC, cXPerson vtable 0x8C): the aspiration motive (-100..100)
// as 0..1000.
float PersonAspirationScore(cXPerson* person) {
  return ((static_cast<float>(person->GetMotive(0)) + 100.0f) / 200.0f) * 1000.0f;
}
}  // namespace

// 801E19CC
// The panel of `player`: its variables, shaders (icon slots, meter bars, glow, backdrop, dividers, text
// backgrounds, icon backdrops, callout), the screen (0x3A, player 2's 0x3B), then the meter's masks at
// their start, the bottom bar red and the glow white and clear.
WAFTarget::WAFTarget(int player) : m_player(player) {
  AttachTables(player == 0 ? "0WAFTarget" : "1WAFTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  const std::string pre = std::to_string(player);
  m_getTable->InstallCommand((pre + "WAF_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"WAF_text_show_callback", "WAF_text_hide_callback", "WAF_show_callback", "WAF_hide_callback",
                        "WAF_glow_full_complete", "WAF_glow_away_complete", "WAF_top_tween_complete",
                        "WAF_bottom_tween_complete"})
    m_setTable->InstallCommand((pre + n).c_str(), set, this);
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (int i = 0; i < 7; ++i) m_locTable->InstallCommand((pre + "WAF_Text" + std::to_string(i)).c_str(), loc, this);
  for (int i = 0; i < 7; ++i)
    for (int k = 0; k < 2; ++k) {
      std::snprintf(kWafShdr[player][i][k], sizeof kWafShdr[player][i][k], "%dWAF_shdr%d%d", player, i, k);
      InstallShader(this, kWafShdr[player][i][k], 0x9f93ff74);
    }
  auto shader = [&](const std::string& id, unsigned int shaderId) { InstallShader(this, (pre + id).c_str(), shaderId); };
  shader("WAF_emptybar", 0x4a93d50d);
  shader("WAF_fullbar", 0x8a9a2bec);
  shader("WAF_fullbar_bottom", 0x77c7e935);
  shader("WAF_glow", 0x2a3363bc);
  shader("WAF_glow_bottom", 0xb7e47a0a);
  shader("WAF_sidepanel_backdrop", 0x7eaf06c1);
  for (int i = 1; i <= 9; ++i) shader("WAF_divider" + std::to_string(i), 0x3186c4de);
  for (int i = 1; i <= 7; ++i) shader("WAF_textBG" + std::to_string(i), 0x97eb9f9d);
  shader("WAF_icon_bg_a0", 0xd229e50a);
  shader("WAF_icon_bg_a1", 0x8f16c6bf);
  shader("WAF_icon_bg_a2", 0xd229e50a);
  shader("WAF_icon_bg_b0", 0xd229e50a);
  shader("WAF_icon_bg_b1", 0x8f16c6bf);
  shader("WAF_icon_bg_b2", 0xd229e50a);
  shader("WAF_callout", 0x0562fbd5);
  for (int i = 0; i < 7; ++i) {
    Icon& icon = m_icons[i];
    icon.swf = kWafSwf[i];
    icon.baseName = kWafShdr[player][i][0];
    icon.overlayName = kWafShdr[player][i][1];
    icon.type = i < 4 ? 0 : 1;
  }
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(player == 0 ? 0x3a : 0x3b, 0);
  m_reflow.OpenReflowBlock();
  for (const char* id : {"WAF_mask_bottom_maskAdd", "WAF_mask_bottom_maskSub"}) {
    m_reflow.Reflow_Id(id, player);
    m_reflow.Reflow_Y(0x28);
    m_reflow.Reflow_X(0);
  }
  for (const char* id : {"WAF_mask_maskAdd", "WAF_mask_maskSub"}) {
    m_reflow.Reflow_Id(id, player);
    m_reflow.Reflow_Y(0xf0);
    m_reflow.Reflow_X(0);
  }
  m_reflow.SubmitReflowBlock();
  m_reflow.OpenTweenBlock("WAF_fullbar_bottom", 1, 0, "NO_CALLBACK", player);
  m_reflow.Tween_ColorOnlyOverride(0xff0000);
  m_reflow.SubmitTweenBlock();
  for (const char* id : {"WAF_glow", "WAF_glow_bottom"}) {
    m_reflow.OpenTweenBlock(id, 1, 0, "NO_CALLBACK", player);
    m_reflow.Tween_ColorOnlyOverride(0xffffff);
    m_reflow.SubmitTweenBlock();
  }
  m_reflow.ResetAlpha(const_cast<char*>("WAF_glow"), 0, player);
  m_reflow.ResetAlpha(const_cast<char*>("WAF_glow_bottom"), 0, player);
}

// 801E2D10
WAFTarget::~WAFTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x3a : 0x3b);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801E2EC0
// The movie's tween callbacks: text and panel shown or hidden (hidden: the bars and glow too), the
// meter's bar tweens and the glow's fades done.
void WAFTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("WAF_text_show_callback", name)) {
    m_textTweening = 0;
    m_textShown = 1;
  } else if (!std::strcmp("WAF_text_hide_callback", name)) {
    m_reflow.ResetVisibility("WAF_short_text_container", false, m_player);
    m_textShown = 0;
    m_textTweening = 0;
  } else if (!std::strcmp("WAF_show_callback", name)) {
    m_tweening = 0;
    OnShow();
  } else if (!std::strcmp("WAF_hide_callback", name)) {
    m_tweening = 0;
    m_reflow.ResetVisibility("WAF", false, m_player);  // DAT_803F63EC
    m_shown = 0;
    m_reflow.OpenReflowBlock();
    for (const char* id : {"WAF_fullbar", "WAF_fullbar_bottom", "WAF_fullbar", "WAF_glow"}) {
      m_reflow.Reflow_Id(id, m_player);
      m_reflow.Reflow_Visibility(false);
    }
    m_reflow.SubmitReflowBlock();
  } else if (!std::strcmp("WAF_bottom_tween_complete", name)) {
    OnBottomBarTweenComplete();
  } else if (!std::strcmp("WAF_top_tween_complete", name)) {
    OnTopBarTweenComplete();
  } else if (!std::strcmp("WAF_glow_away_complete", name)) {
    OnGlowEffectComplete();
  } else if (!std::strcmp("WAF_glow_full_complete", name)) {
    m_glowFull = 1;
  }
}

// 801E30C8
char* WAFTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("WAF_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801E3160
// "WAF_Text<n>": the icon's want or fear text.
unsigned short* WAFTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  unsigned short* s = NewLocalizable();
  if (!std::strncmp("WAF_Text", name, 8)) {
    int slot = -1;
    if (std::sscanf(name, "WAF_Text%d", &slot) != 1 || slot < 0 || slot >= 7) {
      SimsAptFree(s);
      return nullptr;
    }
    const std::vector<unsigned short>& t = m_icons[slot].text;
    std::size_t n = 0;
    for (; n < t.size() && n < 0xfff; ++n) s[n] = t[n];
    s[n] = 0;
  }
  return FinishLocalizable(s);
}

// 801E3278
// The sim's icons asked for once there is one; the glow leaves once full; the panel fades in (player
// 2's at x 320; split screen: without the callout) with the icons changed while hidden starting their
// animation, or out; the short text fades in or out (single player).
void WAFTarget::Update() {
  if (!m_exists) return;
  cXPerson* person = _globals.GetSelectedPerson(m_player);  // DAT_80475E88
  if (!m_iconsInitialized && person) {
    m_iconsInitialized = 1;
    PersonInitWantFearIcons(person);  // cXPerson vtable 0x3B4
  }
  if (m_glowFull) {
    m_glowFull = 0;
    GlowEffectAwayStart();
  }
  m_time += _dt;
  if (!m_tweening) {
    if (!m_wantShown) {
      if (m_shown) {
        m_reflow.OpenTweenBlock("WAF", 0xfa, 0, "WAF_hide_callback", m_player);
        m_reflow.Tween_Alpha(0);
        m_reflow.SubmitTweenBlock();
        m_tweening = 1;
      }
    } else if (!m_shown) {
      UIReflow reflow;
      reflow.OpenReflowBlock();
      reflow.Reflow_Id("WAF", m_player);
      reflow.Reflow_Visibility(true);
      if (m_player != 0) reflow.Reflow_X(0x140);
      if (_gd.m_fieldEC) {  // DAT_80475C70 two players
        reflow.Reflow_Id("WAF_callout_container", m_player);
        reflow.Reflow_Visibility(false);
      }
      reflow.SubmitReflowBlock();
      m_reflow.OpenTweenBlock("WAF", 0xfa, 0, "WAF_show_callback", m_player);
      m_reflow.Tween_Alpha(0xff);
      m_reflow.SubmitTweenBlock();
      for (int i = 0; i < 7; ++i)
        if (m_icons[i].delay > 0.0f) {
          m_icons[i].delay = 0.0f;
          StartAnimation(i);
        }
      m_shown = 1;
      m_tweening = 1;
    }
  }
  if (!_gd.m_fieldEC && m_textWanted != m_textShown && !m_textTweening) {
    if (!m_textWanted) {
      m_reflow.OpenTweenBlock("WAF_short_text_container", 0xfa, 0, "WAF_text_hide_callback", m_player);
      m_reflow.Tween_Alpha(0);
      m_reflow.SubmitTweenBlock();
    } else {
      m_reflow.ResetVisibility("WAF_short_text_container", true, m_player);
      m_reflow.OpenTweenBlock("WAF_short_text_container", 0xfa, 0, "WAF_text_show_callback", m_player);
      m_reflow.Tween_Alpha(0xff);
      m_reflow.SubmitTweenBlock();
    }
    m_textTweening = 1;
  }
}

// 801E3574
void WAFTarget::ShowWantsAndFears() { m_wantShown = 1; }
// 801E3580
void WAFTarget::HideWantsAndFears() {
  m_textWanted = 0;
  m_wantShown = 0;
}

// 801E3590
// While the simulator runs: the meter follows the sim's aspiration score (a new sim: set at once) and
// tweens to a changed value; each icon's animation counts down (0.2 a call): a spin ends in the slot
// machine and the new shaders, a flip or a hidden change's delay ends with the icon settled.
void WAFTarget::UpdateWantsAndFears() {
  if (!Globs_pSimulator || Globs_pSimulator->IsPaused()) return;  // vtable 0x64
  if (m_shown) {
    cXPerson* last = m_person;
    m_person = _globals.GetSelectedPerson(m_player);
    if (m_person) {
      const float score = PersonAspirationScore(m_person);
      if (last == m_person) SetAspirationMeterHeight(static_cast<int>(MapGameAspirationValueToMeterValue(score)));
      else UpdateAspirationMeter();
    }
    if (m_meter != m_target && !m_barTweening && !m_glowing) BeginMeterTween();
  }
  if (!m_exists) return;
  for (int i = 0; i < 7; ++i) {
    Icon& icon = m_icons[i];
    if (icon.spin > 0.0f) {
      icon.spin -= 0.2f;
      if (icon.spin <= 0.0f) {
        icon.flip = 2.5f;
        m_reflow.OpenReflowBlock();
        m_reflow.Reflow_Id(icon.swf, m_player);
        m_reflow.Reflow_SwfFile(kSlotMachineSwf);
        m_reflow.Reflow_Id(icon.baseName, m_player);
        m_reflow.Reflow_Visibility(false);
        m_reflow.Reflow_Id(icon.overlayName, m_player);
        m_reflow.Reflow_Visibility(false);
        m_reflow.SubmitReflowBlock();
        PlayUISound(0x3a8de294);
        InstallShader(this, icon.baseName, icon.newBase);
        InstallShader(this, icon.overlayName, icon.newOverlay);
      }
    } else if (icon.flip > 0.0f) {
      icon.flip -= 0.2f;
      if (icon.flip <= 0.0f) FinishSettingIcon(i);
    } else if (icon.delay > 0.0f) {
      icon.delay -= 0.2f;
      if (icon.delay <= 0.0f) {
        InstallShader(this, icon.baseName, icon.newBase);
        InstallShader(this, icon.overlayName, icon.newOverlay);
        FinishSettingIcon(i);
      }
    }
  }
}

// 801E38F8
int WAFTarget::IsWantsAndFearsActive() { return m_shown; }

// 801E3900
// A want or fear changed in `slot`: its kind (the backdrop swf) and text from the neighbour's
// bookmark; a spin or flip (with its sound and the d-pad warning) runs now, or once the panel shows;
// otherwise the icon is set at once.
void WAFTarget::NewWantsAndFearsIcon(int slot, unsigned int base, unsigned int overlay, int anim, Neighbor* neighbor) {
  if (!m_iconsInitialized || slot < 0 || slot >= 7) return;
  WantFearSlotInfo info;
  NeighborWantFearSlot(neighbor, slot, &info);  // Neighbor +0x198: WantFear::Bookmark
  const int type = info.type == 0x80 ? 1 : info.type == 4 ? 2 : info.type == 1 ? 3 : 0;
  Icon& icon = m_icons[slot];
  const int previous = icon.type;
  icon.newBase = base;
  icon.newOverlay = overlay;
  icon.type = type;
  icon.text = info.text;  // GetText(.., neighbor, 1)
  char text[16];
  std::snprintf(text, sizeof text, "WAF_text%i", slot);
  if (anim == kAnimSpin || anim == kAnimFlip) {
    if (anim == kAnimSpin) {
      if (previous == 3) PlayUISound(0x4d250577);
      else if (previous == 2) PlayUISound(0x92469522);
      if (H2DTarget* h2d = _gd.m_h2dTarget) h2d->WAFInWarning(m_player, true, previous != 1);
    }
    icon.dialog = neighbor;
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id(text, m_player);
    m_reflow.Reflow_Visibility(false);
    m_reflow.Reflow_FontSizeOverride(0xe);
    m_reflow.SubmitReflowBlock();
    icon.anim = anim;
    if (!m_wantShown) {
      icon.delay = 2.5f;
      icon.flip = 0.0f;
      icon.spin = 0.0f;
    } else {
      StartAnimation(slot);
    }
  } else {
    InstallShader(this, icon.baseName, icon.newBase);
    InstallShader(this, icon.overlayName, icon.newOverlay);
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id(icon.swf, m_player);
    m_reflow.Reflow_SwfFile(kWafSwfTypes[icon.type]);
    m_reflow.Reflow_Id(text, m_player);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_FontSizeOverride(0xe);
    m_reflow.Reflow_Id(icon.baseName, m_player);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_Id(icon.overlayName, m_player);
    m_reflow.Reflow_Visibility(true);
    m_reflow.SubmitReflowBlock();
    icon.dialog = nullptr;
    icon.base = icon.newBase;
    icon.overlay = icon.newOverlay;
    icon.spin = 0.0f;
    icon.flip = 0.0f;
  }
}

// 801E3C9C
// A spin restarts the backdrop's animation for 3 s; a flip shows the slot machine with the new
// shaders for 0.5 s.
void WAFTarget::StartAnimation(int slot) {
  Icon& icon = m_icons[slot];
  if (icon.anim == kAnimSpin) {
    icon.spin = 3.0f;
    icon.flip = 0.0f;
    icon.delay = 0.0f;
    m_reflow.ResetSwfAnimation(icon.swf, "a0", m_player);  // DAT_803F0CF4
  } else if (icon.anim == kAnimFlip) {
    icon.flip = 0.5f;
    icon.spin = 0.0f;
    icon.delay = 0.0f;
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id(icon.swf, m_player);
    m_reflow.Reflow_SwfFile(kSlotMachineSwf);
    m_reflow.Reflow_Id(icon.baseName, m_player);
    m_reflow.Reflow_Visibility(false);
    m_reflow.Reflow_Id(icon.overlayName, m_player);
    m_reflow.Reflow_Visibility(false);
    m_reflow.SubmitReflowBlock();
    PlayUISound(0x3a8de294);
    InstallShader(this, icon.baseName, icon.newBase);
    InstallShader(this, icon.overlayName, icon.newOverlay);
  }
}

// 801E3E60
// The icon settles: its kind's backdrop, shaders and text shown; a want's dialog follows.
void WAFTarget::FinishSettingIcon(int slot) {
  Icon& icon = m_icons[slot];
  icon.flip = 0.0f;
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id(icon.swf, m_player);
  m_reflow.Reflow_SwfFile(kWafSwfTypes[icon.type]);
  m_reflow.Reflow_Id(icon.baseName, m_player);
  m_reflow.Reflow_Visibility(true);
  m_reflow.Reflow_Id(icon.overlayName, m_player);
  m_reflow.Reflow_Visibility(true);
  char text[16];
  std::snprintf(text, sizeof text, "WAF_text%i", slot);
  m_reflow.Reflow_Id(text, m_player);
  m_reflow.Reflow_Visibility(true);
  m_reflow.SubmitReflowBlock();
  icon.base = icon.newBase;
  icon.overlay = icon.newOverlay;
  icon.newBase = 0;
  icon.newOverlay = 0;
  if (icon.dialog && icon.type != 1) {
    SIMMY_STUB("WAFTarget::FinishSettingIcon: Neighbor::SpawnWantFearDialog", 0x801E3E60);
    icon.dialog = nullptr;
  }
}

// 801E3FB0
// The meter's target (0..100; never exactly on a tier: 50, 75 and 87 go one up).
void WAFTarget::SetAspirationMeterHeight(int height) {
  m_target = height < 0 ? 0.0f : height <= 100 ? static_cast<float>(height) : 100.0f;
  if (m_target == 50.0f || m_target == 75.0f || m_target == 87.0f) m_target += 1.0f;
  m_rising = m_meter <= m_target ? 1 : 0;
}

// 801E4074
// The bar the meter is in tweens: the bottom one up to 50, the top one above.
void WAFTarget::BeginMeterTween() {
  if (m_settled == 1) {
    if (m_target <= 50.0f) {
      if (m_meter <= 50.0f) {
        BeginBottomBarTween();
        return;
      }
    } else if (m_meter < 50.0f) {
      BeginBottomBarTween();
      return;
    }
  } else if (m_meter == 50.0f) {
    if (m_target < 50.0f) BeginBottomBarTween();
    if (m_target <= 50.0f) return;
  } else if (m_meter <= 50.0f) {
    BeginBottomBarTween();
    return;
  }
  BeginTopBarTween();
}

// 801E4140
// The bottom bar (red) shows and its masks slide to the target (up to 50; 6 ms per point).
void WAFTarget::BeginBottomBarTween() {
  m_reflow.OpenTweenBlock("WAF_fullbar", 1, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_ColorOnlyOverride(0xff00);
  m_reflow.SubmitTweenBlock();
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("WAF_fullbar", m_player);
  m_reflow.Reflow_Visibility(false);
  m_reflow.Reflow_Id("WAF_fullbar_bottom", m_player);
  m_reflow.Reflow_Visibility(true);
  m_reflow.Reflow_Id("WAF_mask_maskAdd", m_player);
  m_reflow.Reflow_Y(0xf0);
  m_reflow.Reflow_Id("WAF_mask_maskSub", m_player);
  m_reflow.Reflow_Y(0xf0);
  m_reflow.SubmitReflowBlock();
  m_tweenTo = m_target > 50.0f ? 50.0f : m_target;
  const int y = static_cast<int>((m_tweenTo / 50.0f) * -80.0f + 40.0f);
  const unsigned int ms = static_cast<unsigned int>(static_cast<int>(std::fabs(m_meter - m_tweenTo)) * 6 + 0x21);
  m_reflow.OpenTweenBlock("WAF_mask_bottom_maskAdd", ms, 0, "WAF_bottom_tween_complete", m_player);
  m_reflow.Tween_Y(y);
  m_reflow.SubmitTweenBlock();
  m_reflow.OpenTweenBlock("WAF_mask_bottom_maskSub", ms, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_Y(y);
  m_reflow.SubmitTweenBlock();
  m_settled = 0;
  m_barTweening = 1;
}

// 801E4344
// The top bar shows and its masks slide to the next tier or the target.
void WAFTarget::BeginTopBarTween() {
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("WAF_fullbar", m_player);
  m_reflow.Reflow_Visibility(true);
  m_reflow.Reflow_Id("WAF_fullbar_bottom", m_player);
  m_reflow.Reflow_Visibility(false);
  m_reflow.Reflow_Id("WAF_mask_bottom_maskAdd", m_player);
  m_reflow.Reflow_Y(-0x28);
  m_reflow.Reflow_Id("WAF_mask_bottom_maskSub", m_player);
  m_reflow.Reflow_Y(-0x28);
  m_reflow.SubmitReflowBlock();
  m_tweenTo = CalculateTweenTargetPosition();
  const int y = static_cast<int>(((m_tweenTo - 50.0f) / 50.0f) * -160.0f + 240.0f);
  const float distance = std::fabs(m_meter - static_cast<float>(static_cast<int>(m_tweenTo)));
  const unsigned int ms = static_cast<unsigned int>(static_cast<int>(distance) * 6 + 0x21);
  m_reflow.OpenTweenBlock("WAF_mask_maskAdd", ms, 0, "WAF_top_tween_complete", m_player);
  m_reflow.Tween_Y(y);
  m_reflow.SubmitTweenBlock();
  m_reflow.OpenTweenBlock("WAF_mask_maskSub", ms, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_Y(y);
  m_reflow.SubmitTweenBlock();
  m_settled = 0;
  m_barTweening = 1;
}

// 801E4548
int WAFTarget::OnTopBarTweenComplete() {
  if (m_tweenTo == m_target) m_settled = 1;
  m_barTweening = 0;
  m_meter = m_tweenTo;
  GlowEffectStart();
  if (!m_rising) GetTopBarTweenColor();
  return 1;
}

// 801E45B8
int WAFTarget::OnBottomBarTweenComplete() {
  if (m_tweenTo == m_target) m_settled = 1;
  m_barTweening = 0;
  m_meter = m_tweenTo;
  GlowEffectStart();
  return 1;
}

// 801E460C
// The top bar's colour at a tier: green at 75 and 87, white otherwise.
unsigned int WAFTarget::GetTopBarColorAtTier() {
  const unsigned int colour = m_meter == 75.0f || m_meter == 87.0f ? 0xff00 : 0xffffff;
  m_reflow.OpenTweenBlock("WAF_fullbar", 1, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_ColorOnlyOverride(colour);
  m_reflow.SubmitTweenBlock();
  return colour;
}

// 801E46AC
// Where the top bar tweens to: rising, the next tier (75, 87) above the meter, or the target; falling,
// the tier below, or the target.
float WAFTarget::CalculateTweenTargetPosition() {
  const float shown = m_meter, target = m_target;
  if (target > shown) {
    float tier = 87.0f;
    if (shown <= 87.0f) {
      tier = 75.0f;
      if (shown <= 75.0f) return target <= 50.0f ? 50.0f : target;
    }
    return target <= tier ? tier : target;
  }
  float tier = 75.0f;
  if (shown >= 75.0f) {
    tier = 87.0f;
    if (shown >= 87.0f) return target;
  }
  return tier <= target ? tier : target;
}

// 801E4758
// The aspiration score (0..1000) on the meter's scale: 0..10% fills 0..50, 10..60% 50..75, 60..90%
// 75..87, 90..100% 87..100.
float WAFTarget::MapGameAspirationValueToMeterValue(float score) {
  const float v = score / 10.0f;
  if (v < 10.0f) return v * 5.0f;
  if (v < 60.0f) return ((v - 10.0f) / 50.0f) * 25.0f + 50.0f;
  if (v < 90.0f) return ((v - 60.0f) / 30.0f) * 12.0f + 75.0f;
  if (v <= 100.0f) return ((v - 90.0f) / 10.0f) * 13.0f + 87.0f;
  return v;
}

// 801E4858
// Rising onto a tier (75, 87) the glow fades in; falling onto 87 it goes at once.
int WAFTarget::GlowEffectStart() {
  if (!m_glowing && !m_rising) {
    if (m_meter == 87.0f) {
      m_reflow.OpenTweenBlock("WAF_glow", 1, 0, "NO_CALLBACK", m_player);
      m_reflow.Tween_Alpha(0);
      m_reflow.SubmitTweenBlock();
    }
    return 0;
  }
  if (!m_rising || m_glowing) return 0;
  if (m_meter == 75.0f || m_meter == 87.0f) {
    m_reflow.OpenTweenBlock("WAF_glow", 500, 0, "WAF_glow_full_complete", m_player);
    m_reflow.Tween_Alpha(0xff);
    m_reflow.SubmitTweenBlock();
    m_glowing = 1;
  }
  return 1;
}

// 801E4970
// The full glow fades out again (it stays at the top tier while rising).
void WAFTarget::GlowEffectAwayStart() {
  GetTopBarColorAtTier();
  if (!m_rising || m_meter != 87.0f) {
    m_reflow.OpenTweenBlock("WAF_glow", 500, 0, "WAF_glow_away_complete", m_player);
    m_reflow.Tween_Alpha(0);
    m_reflow.SubmitTweenBlock();
  } else {
    m_glowing = 0;
  }
}

// 801E4A00
void WAFTarget::OnGlowEffectComplete() { m_glowing = 0; }

// 801E4A0C
// The top bar's colour while it moves: rising from 75 white, else green (settled off a tier: white
// from 87, no tween).
unsigned int WAFTarget::GetTopBarTweenColor() {
  const float shown = m_meter;
  unsigned int colour = 0xff00;
  if (m_target == shown && shown != 75.0f && shown != 87.0f) return shown >= 87.0f ? 0xffffff : 0xff00;
  if (!(m_target < shown) && shown >= 75.0f) colour = 0xffffff;
  m_reflow.OpenTweenBlock("WAF_fullbar", 1, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_ColorOnlyOverride(colour);
  m_reflow.SubmitTweenBlock();
  return colour;
}

// 801E4AFC
void WAFTarget::OnShow() {
  m_reflow.OpenReflowBlock();
  for (const char* id : {"WAF_fullbar", "WAF_fullbar_bottom", "WAF_fullbar", "WAF_glow"}) {
    m_reflow.Reflow_Id(id, m_player);
    m_reflow.Reflow_Visibility(true);
  }
  m_reflow.SubmitReflowBlock();
  UpdateAspirationMeter();
}

// 801E4BBC
// The meter set at once to the sim's aspiration: masks, top bar colour (white from 87) and the glow
// (full from 87).
void WAFTarget::UpdateAspirationMeter() {
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  const float score = person ? PersonAspirationScore(person) : 0.0f;
  float value = static_cast<float>(static_cast<int>(MapGameAspirationValueToMeterValue(score)));
  if (value == 50.0f || value == 75.0f || value == 87.0f) value += 1.0f;
  m_target = value;
  m_meter = value;
  const float bottom = value > 50.0f ? 50.0f : value;
  const int y = static_cast<int>((bottom / 50.0f) * -80.0f + 40.0f);
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("WAF_mask_bottom_maskAdd", m_player);
  m_reflow.Reflow_Y(y);
  m_reflow.Reflow_Id("WAF_mask_bottom_maskSub", m_player);
  m_reflow.Reflow_Y(y);
  m_reflow.SubmitReflowBlock();
  const int top = m_meter >= 50.0f ? static_cast<int>(((m_meter - 50.0f) / 50.0f) * -160.0f + 240.0f) : 0xf0;
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("WAF_mask_maskAdd", m_player);
  m_reflow.Reflow_Y(top);
  m_reflow.Reflow_Id("WAF_mask_maskSub", m_player);
  m_reflow.Reflow_Y(top);
  m_reflow.SubmitReflowBlock();
  const unsigned int colour = m_meter >= 87.0f ? 0xffffff : 0xff00;
  m_reflow.OpenTweenBlock("WAF_fullbar", 1, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_ColorOnlyOverride(colour);
  m_reflow.SubmitTweenBlock();
  m_reflow.OpenTweenBlock("WAF_glow", 1, 0, "NO_CALLBACK", m_player);
  m_reflow.Tween_Alpha(m_meter < 87.0f ? 0 : 0xff);
  m_reflow.SubmitTweenBlock();
}

// 801E4F24
void WAFTarget::SetTextButtonPressed(bool pressed) { m_textWanted = pressed ? 1 : 0; }
