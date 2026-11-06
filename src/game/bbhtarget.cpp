// Port of game/targets_s2c.cpp's BBHTarget: the build/buy HUD. See bbhtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
}  // namespace

// 801A31C4
// Its command tables under the names with the player digit (the strings at 804FE0C0..: "0BBH_exists";
// "0BBH_show/hide_callback"; "0BBH_price", "0BBH_money", "0BBH_mode"), the piggy and price tag shaders, the update (priority 0x4B), the screen 0x29 and its fire code meter.
BBHTarget::BBHTarget(int player) : m_player(player) {
  AttachTables("0BBH");  // 803EDEAC
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand("0BBH_exists", [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  m_setTable->InstallCommand("0BBH_show_callback", set, this);
  m_setTable->InstallCommand("0BBH_hide_callback", set, this);
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"0BBH_price", "0BBH_money", "0BBH_mode"}) m_locTable->InstallCommand(n, loc, this);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0BBH_piggy"), 0x8556ecaf, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0BBH_tag"), 0x12ae0eec, const_cast<char*>(m_name), 0);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x29, 0);
  m_fcm = new FCMTarget();
}

// 801A3788
BBHTarget::~BBHTarget() {
  delete m_fcm;  // GC: +0xA4's deleting destructor
  m_fcm = nullptr;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x29);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    if (m_updateTable) m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801A390C
// The tweens' ends: shown, or hidden (made invisible).
void BBHTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("BBH_show_callback", name)) {
    m_tweening = 0;
    m_active = 1;
  } else if (!std::strcmp("BBH_hide_callback", name)) {
    m_tweening = 0;
    UIReflow reflow;
    reflow.ResetVisibility("BBH", false, 0);  // 803EDEB4
    m_active = 0;
  }
}

// 801A39BC
char* BBHTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("BBH_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801A3A54
// BBH_price: the price as money; BBH_mode: the mode's PDA string ("cat_mode_place", "cat_mode_grab",
// "cat_tool_wall", "cat_tool_floor", "cat_tool_wallpaper", "cat_tool_wall_remove",
// "cat_tool_floor_remove", "cat_tool_wallpaper_remove"); then the UI string substitutions (BBH_money
// is "<money>").
unsigned short* BBHTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  if (!std::strcmp(name, "BBH_price")) {
    UIObjectBase::makeMoneyString(m_price, s, true);
  } else if (!std::strcmp(name, "BBH_mode")) {
    static const char* const kModes[8] = {"cat_mode_place",       "cat_mode_grab",        "cat_tool_wall",
                                          "cat_tool_floor",       "cat_tool_wallpaper",   "cat_tool_wall_remove",
                                          "cat_tool_floor_remove", "cat_tool_wallpaper_remove"};
    if (m_mode >= 0 && m_mode < 8) {
      const unsigned short* const* h = _globals.GetPDAString(kModes[m_mode]);
      const unsigned short* text = h ? *h : nullptr;
      int i = 0;
      for (; text && text[i] && i < 0xfff; ++i) s[i] = text[i];
      s[i] = 0;
    }
  }
  UIQDTarget::UIQDSubstitutionStrings(s);
  int n = 0;
  while (s[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}

// 801A3CC0
// Once the movie is there and no tween runs: fades in (made visible, the price at font size 12) when
// asked to show and not shown, out when asked to hide and shown; the fire code meter with it.
void BBHTarget::Update() {
  if (!m_exists || m_tweening) return;
  UIReflow reflow;
  if (!m_wantShown) {
    if (m_active) {
      reflow.OpenTweenBlock("BBH", 0xfa, 0, "BBH_hide_callback", 0);
      reflow.Tween_Alpha(0);
      reflow.SubmitTweenBlock();
      m_tweening = 1;
      if (m_fcm) m_fcm->HideFireCodeMeter();
    }
  } else if (!m_active) {
    reflow.OpenReflowBlock();
    reflow.Reflow_Id("BBH", 0);
    reflow.Reflow_Visibility(true);
    reflow.Reflow_Id("BBH_price", 0);
    reflow.Reflow_FontSizeOverride(0xc);
    reflow.SubmitReflowBlock();
    reflow.OpenTweenBlock("BBH", 0xfa, 0, "BBH_show_callback", 0);
    reflow.Tween_Alpha(0xff);
    reflow.SubmitTweenBlock();
    m_sellingPig = 0;
    m_tweening = 1;
    if (m_fcm) m_fcm->ShowFireCodeMeter();
  }
}

// 801A3E30
void BBHTarget::SetCurrentPrice(int price) {
  if (!m_exists || m_price == price) return;
  m_price = price;
  UIReflow reflow;
  reflow.ResetFontSizeOverride("BBH_price", 0xc, 0);
}

// 801A3E94
int BBHTarget::IsBuildBuyHUDActive() { return m_active; }
// 801A3E9C
void BBHTarget::ShowBuildBuyHUD() { m_wantShown = 1; }
// 801A3EA8
void BBHTarget::HideBuildBuyHUD() { m_wantShown = 0; }

// 801A3EB4
// The money text when the funds changed (player 1's sim selected); the fire code meter's update while
// the HUD is asked to show.
void BBHTarget::UpdateBuildBuyHUD() {
  static int recordOfDollars = -1;  // recordOfDollars.7512
  if (_globals.GetSelectedPerson(0)) {
    const int dollars = SimulatorFunds();  // Globs::pSimulator vtable 0xA4
    if (dollars != recordOfDollars) {
      StackString2<256> text;
      GetMoneyString(dollars, text, false);
      UIDB::UIDBSetString(const_cast<char*>("simoleons"), const_cast<unsigned short*>(text.c_str()));
      recordOfDollars = dollars;
      UIReflow reflow;
      reflow.ResetFontSizeOverride("BBH_money", 0xc, 0);
    }
  }
  if (m_exists && m_wantShown && m_fcm) m_fcm->UpdateFireCodeMeter();
}

// 801A3FB4
void BBHTarget::SetSellingPig(bool on) {
  if (static_cast<int>(on) == m_sellingPig) return;
  m_sellingPig = on;
  UIReflow reflow;
  reflow.ResetVisibility("BBH_piggy", on, 0);
}

// 801A4018
void BBHTarget::SetBBHMode(BBHTarget__eBBHMode mode) {
  if (m_mode == mode) return;
  m_mode = mode;
  UIReflow reflow;
  reflow.ResetFontSizeOverride("BBH_mode", 0x10, 0);
}
