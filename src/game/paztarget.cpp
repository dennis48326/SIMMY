// Port of the pause menu's PAZBase and PAZTarget (game/targets_s2c.cpp). See paztarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
int TwoPlayers() { return _gd.m_fieldEC != 0; }  // DAT_80475C70

// UIObjectBase::pushAptButtonFilterALL (80177B44): a filter passing every command.
unsigned int PushFilterAll(int player, const char* name) {
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

// kRootMenuStrings (803F6E94).
char* kRootMenuStrings[] = {
    const_cast<char*>("PAZ_exists"),         const_cast<char*>("PAZ_GetTabCount"),
    const_cast<char*>("PAZ_GetTabIndex"),    const_cast<char*>("PAZ_GetEyeToyStatus"),
    const_cast<char*>("PAZ_GetBuildBuyStatus"), const_cast<char*>("PAZ_GetProgressStatus"),
    const_cast<char*>("PAZ_GetGameStatus"),  const_cast<char*>("PAZ_X_BUTTON"),
    const_cast<char*>("PAZ_CANCEL"),         const_cast<char*>("PAZ_DPAD_LEFT"),
    const_cast<char*>("PAZ_DPAD_RIGHT"),     const_cast<char*>("PAZ_TabsReady"),
    const_cast<char*>("PAZ_SetCurrTab"),     const_cast<char*>("PAZ_TabSelected"),
    const_cast<char*>("PAZ_Cancel"),         const_cast<char*>("PAZ_TabName"),
    const_cast<char*>("PAZ_Icon0"),          const_cast<char*>("PAZ_Icon1"),
    const_cast<char*>("PAZ_Icon2"),          const_cast<char*>("PAZ_Icon3"),
    const_cast<char*>("PAZ_Icon4"),          const_cast<char*>("PAZ_Icon5"),
    const_cast<char*>("PAZ_Icon6"),          const_cast<char*>("PAZ_Icon7"),
    const_cast<char*>("PAZ_Icon8"),
};

// sRootItems (803F715C): the root pause menu's tabs; B on any of them goes back to the game.
PAZItemInfo sRootItems[8] = {
    {0, "main_wants_fears", 0x7b923e43, 4, 2, 1, 1},     {1, "main_progress", 0x7b923e43, 7, 2, 1, 1},
    {2, "main_career", 0x7b923e43, 8, 2, 1, 1},          {3, "main_relationships", 0x7b923e43, 0xd, 2, 1, 1},
    {4, "main_catalog_buy", 0x7b923e43, 0xe, 2, 1, 1},   {5, "main_catalog_build", 0x7b923e43, 0xf, 2, 1, 1},
    {6, "main_eyetoy", 0x7b923e43, 0x10, 2, 1, 1},       {7, "main_game_options", 0x7b923e43, 0x11, 2, 1, 1},
};

unsigned short* FinishLocalizable(const unsigned short* s) {
  unsigned short buf[0x2000] = {};
  std::size_t n = 0;
  if (s)
    while (s[n] && n < 0x1fff) buf[n] = s[n], ++n;
  UIQDTarget::UIQDSubstitutionStrings(buf);
  n = 0;
  while (buf[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  if (out) std::memcpy(out, buf, (n + 1) * 2);
  return out;
}
}  // namespace

unsigned int PAZTarget::sInitialSelectedItem_Root = 0;

// ---------------------------------------------------------------------------------------------------
// PAZBase

// 801D00E8
PAZBase::PAZBase() = default;

// 801D0190
PAZBase::~PAZBase() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_screen);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    m_drawTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  if (AptViewer* v = Viewer()) {
    v->PopAptButtonFilter(0, m_filters[0]);
    m_filters[0] = 0;
    if (TwoPlayers()) {
      v->PopAptButtonFilter(1, m_filters[1]);
      m_filters[1] = 0;
    }
  }
}

// 801D0318
// The movie's buttons ("<player>" as the value; only the pausing player's count): left and right
// move the cursor, X chooses the tab and B cancels (both once the movie has tweened in, if the tab
// allows it; the movie then tweens out and reports the end, which runs TabSelected or Cancel).
void PAZBase::SetVariable(char* name, char* value) {
  const bool mine = *value - '0' == m_player;
  if (!std::strcmp(m_strings[kDpadLeft], name)) {
    if (mine) on_user_set_selection_index(static_cast<int>(m_selected) - 1);
  } else if (!std::strcmp(m_strings[kDpadRight], name)) {
    if (mine) on_user_set_selection_index(static_cast<int>(m_selected) + 1);
  } else if (!std::strcmp(m_strings[kXButton], name)) {
    if (mine && m_inputEnabled && m_items[m_selected].selectAction != 0) {
      m_cancelled = 0;
      m_state = 2;
      m_inputEnabled = 0;
    }
  } else if (!std::strcmp(m_strings[kCancelButton], name)) {
    if (mine && m_inputEnabled && m_items[m_selected].cancelAction != 0) {
      m_state = 2;
      m_inputEnabled = 0;
      m_cancelled = 1;
    }
  } else if (!std::strcmp(m_strings[kSetCurrTab], name)) {
    int index = 0;
    std::sscanf(value, "%d", &index);
    m_selected = static_cast<unsigned int>(index) % m_count;
    UpdateButtonVisibility();
  } else if (!std::strcmp(m_strings[kTabSelected], name)) {
    static const struct {
      int action;
      const char* flow;
    } kFlows[] = {
        {1, "MainMenu"},          {4, "PAUSE_SUBMENU_WAF"},   {5, "PAUSE_WANTS"},        {6, "PAUSE_FEARS"},
        {7, "PAUSE_PROGRESS"},    {8, "PAUSE_SUBMENU_CAREER"}, {9, "PAUSE_CAREER"},      {10, "PAUSE_SKILLS"},
        {0xb, "PAUSE_PERSONALITY"}, {0xc, "PAUSE_STATS"},     {0xd, "PAUSE_RELATIONSHIPS"}, {0xe, "PAUSE_BUY"},
        {0xf, "PAUSE_BUILD"},     {0x10, "PAUSE_EYETOY"},     {0x11, "PAUSE_SUBMENU_GAME"}, {0x12, "PAUSE_OPTIONS"},
        {0x13, "PAUSE_SAVE"},
    };
    const int action = m_items[m_selected].selectAction;
    for (const auto& f : kFlows)
      if (f.action == action) UIObjectBase::UIRouter_FlowGoto(const_cast<char*>(f.flow));
  } else if (!std::strcmp(m_strings[kCancel], name)) {
    const int action = m_items[m_selected].cancelAction;
    if (action == 2) UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("ingame"));
    else if (action == 3) UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Pause"));
  } else if (!std::strcmp("PAZ_tween_in", name)) {
    m_inputEnabled = 1;
  } else if (!std::strcmp("PAZ_tween_out", name)) {
    SetVariable(m_strings[m_cancelled ? kCancel : kTabSelected], const_cast<char*>(""));
  } else if (!std::strcmp(m_strings[kTabsReady], name)) {
    // The tabs' icons (the unused ones blank).
    for (unsigned int i = 0; i < 9; ++i)
      if (m_installed)
        UI2D::Instance()->InstallEntry(m_strings[kIcon0 + i], i < m_count ? m_items[i].icon : 0x9f93ff74u,
                                       const_cast<char*>(m_name), 0);
  }
}

// 801D0864
// The tab count and index, and which tabs are available: EyeToy never, build and buy unless the
// object module or a player's social mode forbids it, progress unless GameData +0xE8.
char* PAZBase::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp(m_strings[kExists], name)) {
    std::strcpy(out, "loaded");
  } else if (!std::strcmp(m_strings[kGetTabCount], name)) {
    std::snprintf(out, 0x20, "%u", m_count);
  } else if (!std::strcmp(m_strings[kGetTabIndex], name)) {
    std::snprintf(out, 0x20, "%u", m_selected);
  } else if (!std::strcmp(m_strings[kGetEyeToyStatus], name)) {
    std::strcpy(out, "0");
  } else if (!std::strcmp(m_strings[kGetBuildBuyStatus], name)) {
    // GC: "0" when DAT_804761B0 is clear and Globs::pObjectModule (vtable 0x224) or a player's social
    // mode (EGlobal::IsPlayerInSocialMode) is busy; neither runs here.
    std::strcpy(out, "1");
  } else if (!std::strcmp(m_strings[kGetProgressStatus], name)) {
    std::strcpy(out, _gd.m_fieldE8 == 0 ? "1" : "0");  // DAT_80475C6C
  } else if (!std::strcmp(m_strings[kGetGameStatus], name)) {
    std::strcpy(out, "1");
  }
  return out;
}

// 801D0A74
// "TabName": the selected tab's name (PDAStrings).
unsigned short* PAZBase::GetLocalizable(char* name) {
  const unsigned short* text = nullptr;
  if (!std::strcmp(m_strings[kTabName], name) && m_items[m_selected].name) {
    const unsigned short* const* row = _globals.GetPDAString(m_items[m_selected].name);
    text = row ? *row : nullptr;
  }
  return FinishLocalizable(text);
}

// 801D0B64
void PAZBase::Update() {}
// 801D0B68
int PAZBase::GetPausePlayer() { return m_player; }
// 801D0B70
void PAZBase::UpdateButtonVisibility() {}

// 801D0B74
// Installs the movie's variables, loads the screen for the pausing player and takes every button
// (each player's "PauseMenu" filter), then asks for the tabs' icons.
int PAZBase::ConfigureAndLoad(int screen, char** strings, PAZItemInfo* items, unsigned int count) {
  m_screen = screen;
  m_items = items;
  m_count = count;
  m_strings = strings;
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  if (m_installed) {
    for (int i = kExists; i <= kGetGameStatus; ++i) m_getTable->InstallCommand(strings[i], get, this);
    for (int i = kXButton; i <= kCancel; ++i) m_setTable->InstallCommand(strings[i], set, this);
    m_setTable->InstallCommand("PAZ_tween_in", set, this);
    m_setTable->InstallCommand("PAZ_tween_out", set, this);
    m_locTable->InstallCommand(strings[kTabName], [this](char* v) { return GetLocalizable(v); }, this);
  }
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(m_screen, m_player == 0 ? 0 : 1);
  m_filters[0] = PushFilterAll(0, "PauseMenu");
  m_filters[1] = TwoPlayers() ? PushFilterAll(1, "PauseMenu") : 0;
  SetVariable(strings[kTabsReady], const_cast<char*>(""));  // DAT_803EEB58
  // GC: the update entry goes in only when its functor is bound (PAZBase::Update does nothing).
  if (m_installed) UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  UpdateButtonVisibility();
  return 1;
}

// 801D0ED4
// The cursor moves (wrapping) at most every 200 ms once the movie has tweened in: "SetCurrTab".
int PAZBase::on_user_set_selection_index(int index) {
  if (!m_inputEnabled) return 0;
  const unsigned int now = static_cast<unsigned int>(_pClockMan->GetSec(&_sysclock) * 1000.0);
  if (static_cast<float>(now) - static_cast<float>(m_lastMoveMs) < 200.0f) return 0;
  m_lastMoveMs = now;
  if (index < 0) index = static_cast<int>(m_count) - 1;
  char text[40];
  std::snprintf(text, sizeof text, "%u", static_cast<unsigned int>(index) % m_count);
  SetVariable(m_strings[kSetCurrTab], text);
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// PAZTarget

// 801D1040
// The root pause menu for `player`, on the tab chosen last time.
PAZTarget::PAZTarget(int player) {
  m_player = player;
  m_selected = sInitialSelectedItem_Root;
  AttachTables("0PAZTarget");
  UIAUDIO* audio = UIAUDIO::Instance();
  audio->InstallEntry(const_cast<char*>("PAZ_SndOpen"), 0xe1327588u, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PAZ_SndBrowse"), 0x6a7a1517u, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PAZ_SndError"), 0x3804219fu, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PAZ_SndSelect"), 0x5d5565e8u, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PAZ_SndCancel"), 0x76d81c2au, const_cast<char*>(m_name));
  ConfigureAndLoad(0x2c, kRootMenuStrings, sRootItems, 8);
}

// 801D1510
PAZTarget::~PAZTarget() { sInitialSelectedItem_Root = m_selected; }
