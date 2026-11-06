// Port of game/targets_s2c.cpp's FCMTarget: the fire code meter. See fcmtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstring>

double GetFireCodeValue(bool poll);  // 800427A8 (pcttarget.cpp)

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
// The bar's height in pixels for a level (of 166 for 100 %), made even.
unsigned int BarHeight(int level) {
  unsigned int h = static_cast<unsigned int>(static_cast<float>(level) / 100.0f * 166.0f);
  return h + ((h + 1) & 1);
}
}  // namespace

// 801B34C0
// Its command tables ("0FCMTarget": FCM_exists; the show/hide, zenith and fire tween callbacks), the
// bar's shaders (empty, full, overlay, cap, bottom, the fire icon), the update (priority 0x4B) and the
// screen 0x40.
FCMTarget::FCMTarget() {
  AttachTables("0FCMTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand("0FCM_exists", [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"0show_callback_FCM", "0hide_callback_FCM", "0tween_zenith_FCM", "0show_fire_tween_FCM",
                        "0hide_fire_tween_FCM"})
    m_setTable->InstallCommand(n, set, this);
  static const struct {
    const char* name;
    unsigned int shader;
  } kShaders[] = {{"0FCM_emptybar", 0x6561346b}, {"0FCM_fullbar", 0x14248653}, {"0FCM_overlay", 0x63f1a823},
                  {"0FCM_cap", 0x95cd2e64},      {"0FCM_bottom", 0xb17a1f42},  {"0FCM_fire", 0x081fc6af}};
  for (const auto& s : kShaders)
    UI2D::Instance()->InstallEntry(const_cast<char*>(s.name), s.shader, const_cast<char*>(m_name), 0);
  m_shownLevel = 0;
  m_level = 0;
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x40, 0);
}

// 801B3B68
FCMTarget::~FCMTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x40);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    if (m_updateTable) m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801B3CC8
// The tweens' ends: shown; hidden (made invisible); the bar at its level; the fire icon in or out.
void FCMTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("show_callback_FCM", name)) {
    m_tweening = 0;
  } else if (!std::strcmp("hide_callback_FCM", name)) {
    m_tweening = 0;
    UIReflow reflow;
    reflow.ResetVisibility("FCM", false, 0);
    m_active = 0;
  } else if (!std::strcmp("tween_zenith_FCM", name)) {
    m_barTweening = 0;
  } else if (!std::strcmp("show_fire_tween_FCM", name)) {
    m_fireTweening = 0;
    m_fireShown = 1;
  } else if (!std::strcmp("hide_fire_tween_FCM", name)) {
    m_fireShown = 0;
    m_fireTweening = 0;
  }
}

// 801B3DD0
char* FCMTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("FCM_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801B3E6C
// No texts: the UI string substitutions of an empty one.
unsigned short* FCMTarget::GetLocalizable(char* name) {
  (void)std::isdigit(static_cast<unsigned char>(*name));
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  UIQDTarget::UIQDSubstitutionStrings(s);
  int n = 0;
  while (s[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}

// 801B3EF0
// Once the movie is there and no tween runs: slides in (x 0, opaque) when asked to show and not shown,
// out (x 0x40, clear) when asked to hide and shown.
void FCMTarget::Update() {
  if (!m_exists || m_tweening) return;
  UIReflow reflow;
  if (!m_wantShown) {
    if (!m_active) return;
    reflow.OpenTweenBlock("FCM", 0xfa, 0, "hide_callback_FCM", 0);
    reflow.Tween_X(0x40);
    reflow.Tween_Alpha(0);
    reflow.SubmitTweenBlock();
  } else {
    if (m_active) return;
    reflow.OpenTweenBlock("FCM", 0xfa, 0, "show_callback_FCM", 0);
    reflow.Tween_X(0);
    reflow.Tween_Alpha(0xff);
    reflow.SubmitTweenBlock();
    m_active = 1;
  }
  m_tweening = 1;
}

// 801B3FF8
// Made visible off to the right with the bar at the current level (its cap above it, the bottom at 0x14A)
// and the fire icon by the level (over 96), then asked to show.
void FCMTarget::ShowFireCodeMeter() {
  m_shownLevel = GetFireCodeMeterLevel();
  const unsigned int h = BarHeight(m_shownLevel);
  const int half = static_cast<int>(h >> 1);
  const int y = 0x146 - half;
  UIReflow reflow;
  reflow.OpenReflowBlock();
  reflow.Reflow_Id("FCM", 0);
  reflow.Reflow_Visibility(true);
  reflow.Reflow_X(0x40);
  reflow.Reflow_Id("FCM_fullbar", 0);
  reflow.Reflow_H(h);
  reflow.Reflow_Y(y);
  reflow.Reflow_Id("FCM_cap", 0);
  reflow.Reflow_Y(y - half);
  reflow.Reflow_Id("FCM_bottom", 0);
  reflow.Reflow_Y(0x14a);
  reflow.Reflow_Id("FCM_fire", 0);
  const bool low = m_shownLevel < 0x61;
  reflow.Reflow_Alpha(low ? 0 : 0xff);
  m_fireShown = low ? 0 : 1;
  reflow.SubmitReflowBlock();
  m_wantShown = 1;
}

// 801B41B8
void FCMTarget::HideFireCodeMeter() { m_wantShown = 0; }

// 801B41C4
// While shown: the level read again (when asked to show); a new level tweens the bar (and its cap) to
// it; the fire icon fades in past 96 or out below.
void FCMTarget::UpdateFireCodeMeter() {
  if (!m_active) return;
  if (m_wantShown) SetMeterHeight(GetFireCodeMeterLevel());
  UIReflow reflow;
  if (m_level != m_shownLevel && !m_barTweening) {
    const unsigned int h = BarHeight(m_level);
    const int half = static_cast<int>(h >> 1);
    const int y = 0x146 - half;
    int diff = m_shownLevel - m_level;
    if (diff < 0) diff = m_level - m_shownLevel;
    const unsigned int ms = static_cast<unsigned int>(diff * 6 + 0x21);
    reflow.OpenTweenBlock("FCM_fullbar", ms, 0, "tween_zenith_FCM", 0);
    reflow.Tween_H(h);
    reflow.Tween_Y(y);
    reflow.SubmitTweenBlock();
    reflow.OpenTweenBlock("FCM_cap", ms, 0, "NO_CALLBACK", 0);
    reflow.Tween_Y(y - half);
    reflow.SubmitTweenBlock();
    m_barTweening = 1;
    m_shownLevel = m_level;
  }
  unsigned int alpha;
  if (m_shownLevel < 0x61) {
    if (!m_fireShown || m_fireTweening) return;
    reflow.OpenTweenBlock("FCM_fire", 0xfa, 0, "hide_fire_tween_FCM", 0);
    alpha = 0;
  } else {
    if (m_fireShown || m_fireTweening) return;
    reflow.OpenTweenBlock("FCM_fire", 0xfa, 0, "show_fire_tween_FCM", 0);
    alpha = 0xff;
  }
  reflow.Tween_Alpha(alpha);
  reflow.SubmitTweenBlock();
  m_fireTweening = 1;
}

// 801B441C
int FCMTarget::GetFireCodeMeterLevel() { return static_cast<int>(GetFireCodeValue(false) * 100.0); }

// 801B4458
int FCMTarget::IsFireCodeMeterActive() { return m_active; }

// 801B4460
void FCMTarget::SetMeterHeight(int level) {
  if (level < 0) m_level = 0;
  else if (level > 100) m_level = 100;
  else m_level = level;
}
