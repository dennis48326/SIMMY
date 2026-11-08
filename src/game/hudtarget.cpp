// Port of game/hudtarget.cpp (HUDTarget) and of the HUD's H2DTarget and MODTarget
// (game/targets_s2c.cpp): the live-mode HUD. See hudtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

int PcPrivateScreenPlayer();  // uiscreens.cpp (PC co-op private screens)
const char* GetHouseNameText(int house);  // 800236C8 (famtarget.cpp)
int& LiveModeFamilyJustMovedIn();  // livemode.cpp: DAT_80475F24
int LiveModeHouseNumber();  // livemode.cpp: DAT_80476230

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
int TwoPlayers() { return _gd.m_fieldEC != 0; }  // DAT_80475C70

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  while (s[n]) ++n;
  return n;
}
void Copy16(unsigned short* dst, const char* src) {
  while ((*dst++ = static_cast<unsigned char>(*src++)) != 0) {}
}
// The movie's copy of a localizable string (GC: SimsAptMalloc'd, after the UI string substitution).
unsigned short* FinishLocalizable(unsigned short* s, bool substitute) {
  if (substitute) UIQDTarget::UIQDSubstitutionStrings(s);
  const std::size_t n = Len16(s);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}
char* NewVariable() {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  return out;
}

// Globs::pSimulator, once live mode has started (DAT_80475C44).
bool SimulatorRunning() { return Globs_pSimulator != nullptr && _gd.m_fieldC0 != 0; }

// The tutorial's stage (GetTutorialLocks(int) 801858FC reads it; no tutorial here).
int tutorialStage = 0;  // 804FDE40 tutorialStage.9907
// GetTutorialLocks (801858FC): the HUD parts the tutorial stage locks (1 HUD, 4 action queue); none outside tutorials.
unsigned int TutorialLocks(int) { return 0; }

// The motive kinds of the panel's bars (MOTTarget::ReadMotiveBar): social, bladder, hygiene, hunger,
// fun, energy, comfort, room.
const int kBarMotive[8] = {0xe, 9, 8, 7, 0xf, 5, 6, 0xd};

// cXPerson::GetMotive (vtable 0x5C: -100..100).
float PersonMotive(cXPerson* person, int motive) {
  if (!person) return 0.0f;
  return static_cast<float>(person->GetMotive(motive));
}
}  // namespace

int H2DTarget::m_nPauseRefCount = 0;  // 804FE2E4

// ---------------------------------------------------------------------------------------------------
// HUDTarget

// 8018591C
HUDTarget::HUDTarget() {
  ResetJobHudProperties();
  AttachTables("HUDTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  UIDrawTable::InstallEntry(0x19, [this](ERC* rc) { Draw(rc); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  for (const char* n : {"HUD_exists", "HUD_quit_requested", "HUD_tutorial"}) m_getTable->InstallCommand(n, get, this);
  for (const char* p : {"0", "1"})
    for (const char* n : {"HUD_bar0", "HUD_bar1", "HUD_bar2", "HUD_bar3", "HUD_bar4", "HUD_bar5", "HUD_mode", "HUD_jobLevel",
                          "HUD_rep", "HUD_next_rep_unlock", "HUD_base_rep"})
      m_getTable->InstallCommand((std::string(p) + n).c_str(), get, this);
  for (const char* n : {"0HUD_rep_title_level", "1HUD_rep_title_level", "HUD_hidden", "HUD_jobPlayer", "HUD_canOpenXAM",
                        "HUD_amilightweight"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"HUD_initialized", "HUD_destroy", "HUD_state", "HUD_hideImmediate", "HUD_gotoXAMFailed",
                        "0HUD_playerState", "0HUD_rep_level_up", "1HUD_playerState", "1HUD_rep_level_up", "HUD_pause",
                        "HUD_modeless_dialog_visible", "HUD_modeless_continue_selected", "HUD_preload_flash"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"0HUD_rep_loc", "1HUD_rep_loc", "0HUD_next_rep_unlock_loc", "1HUD_next_rep_unlock_loc",
                        "0HUD_rep_level_text", "1HUD_rep_level_text", "0HUD_rep_title_text", "1HUD_rep_title_text",
                        "HUD_modeless_dialog_title", "HUD_modeless_dialog_text", "HUD_modeless_continue_text",
                        "HUD_lot_name"})
    m_locTable->InstallCommand(n, loc, this);
  SIMMY_STUB("HUDTarget::HUDTarget: ActionQueueHUD", 0x8018591C);
  InstallHUDSounds(m_name);
  InstallHUDIcons(0);
  InstallHUDIcons(1);
  AttachControllers();
  InitRepValues();
  UpdateClock();
  m_activePlayer = 0;
  m_h2d = new H2DTarget;  // GC: the global DAT_80475BCC too
  _gd.m_h2dTarget = m_h2d;
}

// 801869FC
HUDTarget::~HUDTarget() {
  ReleaseControllers();
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    m_drawTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  if (_gd.m_h2dTarget == m_h2d) _gd.m_h2dTarget = nullptr;
  delete m_h2d;
  m_h2d = nullptr;
}

// 80186C48
// GC: a priority-100 filter on each player's pad (EController::AddFilter "HUDTarget::AttachControllers").
void HUDTarget::AttachControllers() {
  SIMMY_STUB("HUDTarget::AttachControllers: the HUD's controller filters", 0x80186C48);
}
// 80186D2C
void HUDTarget::ReleaseControllers() {}

// 80186DD4
void HUDTarget::IncPause(bool sound) { H2DTarget::IncPause(sound); }
// 80186E04
void HUDTarget::ReleasePause(bool sound) { H2DTarget::ReleasePause(sound); }

// 80186E34
void HUDTarget::SetVariable(char* name, char* value) {
  int player = 0;
  if (std::isdigit(static_cast<unsigned char>(*name))) player = *name++ - '0';
  SetPlayerVariable(player, name, value);
}

// 80186EA0
// What the movie reports: the HUD's show/hide state (and per player), that it is up
// (HUD_initialized) or gone (HUD_destroy), pause requests.
void HUDTarget::SetPlayerVariable(int player, char* name, char* value) {
  if (!std::strcmp("HUD_pause", name)) {
    IncPause(false);
  } else if (!std::strcmp("HUD_state", name)) {
    m_hudState = static_cast<unsigned char>(std::atoi(value));
    if (m_hudState == 1) OnHUDHideStart();
    else if (m_hudState == 0) OnHUDHide();
    else if (m_hudState == 2) OnHUDShow();
  } else if (!std::strcmp("HUD_playerState", name)) {
    const unsigned char state = static_cast<unsigned char>(std::atoi(value));
    if (state == 1) OnPlayerHUDHideStart(player);
    else if (state == 0) OnPlayerHUDHide(player);
    else if (state == 2) OnPlayerHUDShow(player);
    else if (state == 3) m_playerState[player] = 3;
  } else if (!std::strcmp("HUD_rep_level_up", name)) {
  } else if (!std::strcmp("HUD_destroy", name)) {
    m_quit = 0;
    m_initialized = 0;
    UpdateActivePlayer(0, false);
    m_menus[0].Stop();  // GC: and ActionQueueHUD::Stop (not ported)
    if (TwoPlayers()) {
      UpdateActivePlayer(1, false);
      m_menus[1].Stop();
    }
  } else if (!std::strcmp("HUD_initialized", name)) {
    m_quit = 0;
    m_initialized = 1;
    m_menus[0].Start(this, 0);  // GC: and ActionQueueHUD::Start (not ported)
    UpdateActivePlayer(0, true);
    if (TwoPlayers()) {
      m_menus[1].Start(this, 1);
      UpdateActivePlayer(1, true);
    }
  } else if (!std::strcmp("HUD_hideImmediate", name)) {
    OnHUDHideStart();
    OnPlayerHUDHideStart(0);
    OnPlayerHUDHideStart(1);
    OnPlayerHUDHide(0);
    OnPlayerHUDHide(1);
    OnHUDHide();
    m_playerState[1] = 0;
    m_hudState = 0;
    m_playerState[0] = 0;
  } else if (!std::strcmp("HUD_preload_flash", name)) {
  } else if (!std::strcmp("HUD_gotoXAMFailed", name)) {
    m_quit = 0;
  }
}

// 801871C0
char* HUDTarget::GetVariable(char* name) {
  int player = 0;
  if (std::isdigit(static_cast<unsigned char>(*name))) player = *name++ - '0';
  return GetPlayerVariable(player, name);
}

// 80187224
char* HUDTarget::GetPlayerVariable(int player, char* name) {
  char* out = NewVariable();
  auto number = [out](int v) { std::snprintf(out, 0x20, "%d", v); };
  auto flag = [out](bool v) { std::strcpy(out, v ? "1" : "0"); };
  if (!std::strncmp("HUD_bar", name, 7)) {
    const int bar = name[7] - '0';
    OnReadBar(player, static_cast<unsigned int>(bar));
    number(m_bars[player][bar]);
  } else if (!std::strcmp("HUD_rep", name)) {
    number(GetReputationValue(player));
  } else if (!std::strncmp("HUD_jobLevel", name, 0xc)) {
    number(m_jobLevel);
  } else if (!std::strncmp("HUD_mode", name, 0xb)) {
    static const char* const kModes[10] = {nullptr, "sushi", "sculptor", "fireworks", "piercing",
                                           "chopshop", "skater", "fashion", "ferret", "bartender"};
    if (m_mode >= 1 && m_mode <= 9) std::strcpy(out, kModes[m_mode]);
  } else if (!std::strncmp("HUD_tutorial", name, 0xc)) {
    number(m_tutorial[player]);
  } else if (!std::strncmp("HUD_modeless_has_continue", name, 0x19)) {
    flag(false);  // GC: the modeless dialog (+0xD0) has a continue button
  } else if (!std::strncmp("HUD_quit_requested", name, 0x12)) {
    flag(m_quit == 1);
  } else if (!std::strcmp("HUD_next_rep_unlock", name)) {
    number(GetNextRepUnlockValue(player));
  } else if (!std::strcmp("HUD_base_rep", name)) {
    number(m_baseRep[player]);
  } else if (!std::strcmp("HUD_hidden", name)) {
    flag(!ShouldHUDBeVisible(player));
  } else if (!std::strncmp("HUD_exists", name, 10)) {
    // Not while EGlobal +0x458/+0x450/+0x454 (DAT_80476224/8047621C/80476220) are set or a quit is going.
    if (!_globals.m_field458 && !_globals.m_field450 && !_globals.m_field454 && m_quit <= 1) std::strcpy(out, "loaded");
  } else if (!std::strcmp("HUD_rep_title_level", name)) {
    number(0);  // RepTitleSupport::GetRepTitleRepValue (not ported)
  } else if (!std::strncmp("HUD_jobPlayer", name, 0xd)) {
    flag(m_activePlayer != 0);
  } else if (!std::strcmp("HUD_canOpenXAM", name)) {
    flag(true);
  } else if (!std::strcmp("HUD_amilightweight", name)) {
    flag(false);  // DAT_8047615C
  }
  return out;
}

// 801876E0
unsigned short* HUDTarget::GetLocalizable(char* name) {
  int player = 0;
  if (std::isdigit(static_cast<unsigned char>(*name))) player = *name++ - '0';
  return GetPlayerLocalizable(player, name);
}

// 80187744
// Reputation numbers and titles, the modeless dialog's texts, the lot's name.
unsigned short* HUDTarget::GetPlayerLocalizable(int player, char* name) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  char number[16];
  if (!std::strcmp("HUD_rep_loc", name)) {
    std::snprintf(number, sizeof number, "%d", m_rep[player]);
    Copy16(s, number);
    return FinishLocalizable(s, false);
  }
  if (!std::strcmp("HUD_next_rep_unlock_loc", name)) {
    std::snprintf(number, sizeof number, "%d", GetNextRepUnlockValue(player));
    Copy16(s, number);
    return FinishLocalizable(s, false);
  }
  if (!std::strcmp("HUD_lot_name", name)) {
    // The current house's name (DAT_80475F24 set: a family just moved in, the house being entered,
    // DAT_80476230), as its "HouseData" row's text.
    const char* house = nullptr;
    if (LiveModeFamilyJustMovedIn() == 0) {
      if (_globals.m_house) house = _globals.m_house->GetHouseName();
    } else {
      house = GetHouseNameText(LiveModeHouseNumber());
    }
    if (house) {
      const unsigned short* const* text = _globals.HouseNameText(house);
      if (text && *text) {
        unsigned short* d = s;
        for (const unsigned short* t = *text; (*d++ = *t++) != 0;) {}
      }
    }
    return FinishLocalizable(s, true);
  }
  // HUD_modeless_dialog_*, HUD_rep_level_text and HUD_rep_title_text (RepTitleSupport) are not
  // ported: empty.
  return FinishLocalizable(s, true);
}

// 80187A44
// Reputation (the job HUD's career values, RepTitleSupport) is not ported: 0.
void HUDTarget::InitRepValues() {
  for (int p = 0; p < 2; ++p) m_baseRep[p] = m_rep[p] = 0;
}
// 80187B00
int HUDTarget::GetReputationValue(int player) { return m_rep[player]; }
// 80187BC4
int HUDTarget::GetNextRepUnlockValue(int) { return 0; }

// 80187C9C
// GC: the bar's value from the selected sim's motive (ReadMoodBar: bladder, hygiene, hunger, fun,
// energy, mood).
// 80187D10 ReadMoodBar is folded in: motive -100..100 to 0..100 (GC also keeps the raw value at +0xB0).
// The job-score bar of the career screen (counter at +0xC8 in 1..9) is not ported.
void HUDTarget::OnReadBar(int player, unsigned int bar) {
  static const int kMotive[6] = {9, 8, 7, 0xf, 5, 3};  // bladder, hygiene, hunger, fun, energy, mood
  if (bar >= 6) return;
  if (!_globals.GetSelectedPerson(player)) {
    m_bars[player][bar] = 0;
    return;
  }
  const int value = static_cast<int>(PlayerMotive(player, kMotive[bar]));
  m_bars[player][bar] = static_cast<unsigned char>((value + 100) / 2);
}

// 80187E3C
void HUDTarget::ShowHUD() {
  if ((TutorialLocks(m_tutorial[0]) & 1) == 0 && m_initialized) {
    UpdateClock();
    if (m_hudState == 0 && Viewer()) Viewer()->NewCallFunction(const_cast<char*>("showHud"), nullptr, nullptr, 0);
  }
}

// 80187EB4
void HUDTarget::HideHUD() {
  if ((TutorialLocks(m_tutorial[0]) & 1) == 0 && m_initialized && m_hudState == 2 && Viewer())
    Viewer()->NewCallFunction(const_cast<char*>("hideHud"), nullptr, nullptr, 0);
}

// 80187F24
void HUDTarget::OnPlayerHUDShow(int player) {
  m_playerState[player] = 2;
  UpdateActivePlayer(player, true);
}
// 80187F54
void HUDTarget::OnPlayerHUDHide(int player) {
  m_playerState[player] = 0;
  InstallHUDIcons(player);
}
// 80187F80
// GC: also closes the player's action menu and (if the HUD is to go) empties its action queue.
void HUDTarget::OnPlayerHUDHideStart(int player) {
  if (!ShouldHUDBeVisible(player)) UpdateActivePlayer(player, false);
  m_playerState[player] = 1;
}
// 80187FFC
void HUDTarget::OnHUDShow() {}
// 80188000
void HUDTarget::OnHUDHideStart() {}
// 80188004
void HUDTarget::OnHUDHide() {}

// 80188008
// The five meter icons ("mc_mi0".."mc_mi4", "...h2" for player 2): the job HUD's when this player
// has one (m_mode), else the needs'.
void HUDTarget::InstallHUDIcons(int player) {
  unsigned int ids[5] = {0x1b9d263c, 0xf0c635f1, 0x18a80280, 0x514cdf07, 0x472475e0};
  if (player == m_activePlayer) {
    switch (m_mode) {
      case 1: ids[0] = 0xf2925f76, ids[1] = 0x8e2164ce, ids[2] = 0x6ec81f19, ids[3] = 0xfbde5e1e, ids[4] = 0xf95af948; break;
      case 2: ids[0] = 0x2d1d2672, ids[1] = 0xc9960328, ids[2] = 0x6ae9d30f, ids[3] = 0xfbde5e1e, ids[4] = 0x3dc40e44; break;
      case 3: ids[0] = 0x2cbd137c, ids[1] = 0x4302688d, ids[2] = 0xd5131580, ids[3] = 0xfbde5e1e, ids[4] = 0x3dc40e44; break;
      case 4: ids[0] = 0x56aac0be, ids[1] = 0x6ec81f19, ids[2] = 0xcd511ab8, ids[3] = 0xfbde5e1e, ids[4] = 0x85392e0a; break;
      case 5: ids[0] = 0x2264d2ea, ids[1] = 0x6fd3be2d, ids[2] = 0x01f6f1fd, ids[3] = 0xfbde5e1e, ids[4] = 0x4eb8b959; break;
      case 6: ids[0] = 0xbec5f141, ids[1] = 0xcd511ab8, ids[2] = 0x01f6f1fd, ids[3] = 0xfbde5e1e, ids[4] = 0x4eb8b959; break;
      case 7: ids[0] = 0x6e514690, ids[1] = 0x6ec81f19, ids[2] = 0x767d6182, ids[3] = 0xfbde5e1e, ids[4] = 0x4eb8b959; break;
      case 8: ids[0] = 0xc586c559, ids[1] = 0x6ae9d30f, ids[2] = 0xcd511ab8, ids[3] = 0xfbde5e1e, ids[4] = 0x7c2bd737; break;
      case 9: ids[0] = 0x511d90b4, ids[1] = 0x8e2164ce, ids[2] = 0xcd511ab8, ids[3] = 0xfbde5e1e, ids[4] = 0xa929b1d8; break;
      default: break;
    }
  }
  static const char* const kIcons[2][5] = {{"mc_mi0", "mc_mi1", "mc_mi2", "mc_mi3", "mc_mi4"},
                                           {"mc_mi0h2", "mc_mi1h2", "mc_mi2h2", "mc_mi3h2", "mc_mi4h2"}};  // 80436BE8
  if (!m_installed) return;
  UI2D* ui2d = UI2D::Instance();
  for (int i = 0; i < 5; ++i)
    ui2d->InstallEntry(const_cast<char*>(kIcons[player][i]), ids[i], const_cast<char*>(m_name), 0);
}

// 80188348
// The simulator runs, the game is in a live state (GameData +0x8C/+0x90), the movie has the HUD up,
// no quit is going and neither of EGlobal's +0x454/+0x458 requests is (DAT_80476220/80476224); hidden
// while the player's camera director plays a letterboxed shot or forces the HUD off.
int HUDTarget::ShouldHUDBeVisible(int player) const {
  if (!SimulatorRunning() || !_gd.m_field90 || !_gd.m_field8C) return 0;
  if (CameraDirector* d = _globals.CameraDirectorOf(player)) {
    if (d->IsActive() && d->m_field234) return 0;
    if (d->IsForceDisableHud()) return 0;
  }
  if (_globals.m_field458 || _globals.m_field454) return 0;
  return m_initialized && m_quit == 0 ? 1 : 0;
}

// 80188464
void HUDTarget::UpdateTutorialLocks() {
  const bool changed = tutorialStage != m_tutorial[0];
  m_tutorial[0] = tutorialStage;
  m_tutorial[1] = tutorialStage;
  if (changed && tutorialStage == 1) ShowHUD();
}

// 801884B4
// Shows or hides the HUD as the game allows; the speed controls; the clock every 30 frames.
void HUDTarget::Update() {
  UpdateTutorialLocks();
  if (!ShouldHUDBeVisible(0)) {
    if (m_hudState == 2) HideHUD();
    if (m_speedPaused) {
      if (m_speedPaused == 0xfc) ReleasePause(false);
      m_speedPaused = 0;
      // GC: Globs::pSimulator vtable 0x40 (speed 0)
    }
    return;
  }
  if (m_hudState != 2) ShowHUD();
  UpdateSpeedControls();
  if (--m_clockCountdown == 0) {
    m_clockCountdown = 0x1e;
    UpdateClock();
  }
}

// 801885AC
// The game clock into UIDB: "mins", "secs", "hours24", "hours" (12-hour) and "meridian".
// The simulator's clock (vtable 0x30: 0 hours, 5 minutes, 6 seconds).
void HUDTarget::UpdateClock() {
  const int hours24 = SimulatorHour(), minutes = SimulatorMinute(), seconds = SimulatorSecond();
  UIDB::UIDBSetInt(const_cast<char*>("mins"), minutes);
  UIDB::UIDBSetInt(const_cast<char*>("secs"), seconds);
  UIDB::UIDBSetInt(const_cast<char*>("hours24"), hours24);
  static const unsigned short kAm[] = {'a', 'm', 0}, kPm[] = {'p', 'm', 0};
  UIDB::UIDBSetString(const_cast<char*>("meridian"), hours24 < 12 ? kAm : kPm);
  int hours = hours24 > 12 ? hours24 - 12 : hours24;
  if (hours == 0) hours = 12;
  UIDB::UIDBSetInt(const_cast<char*>("hours"), hours);
}

// 801886C0
// GC: the player's selected sim (DAT_80475E88) is attached to or detached from the HUD
// (cXPersonImpl::AttachToHUD). No sims run yet.
void HUDTarget::UpdateActivePlayer(int, bool) {}

// 801887AC
// GC: the speed buttons (pause, play, fast) set the simulator's speed and show the pause or fast
// forward icon (H2DTarget::DisplayPaused/DisplayFastForward). Not ported with the simulator.
void HUDTarget::UpdateSpeedControls() {}

// 80188BF4
void HUDTarget::AddMenu(int player, cXObjectImpl* object, const std::u16string& name, unsigned int shaderId, ERShader* shader) {
  IsMenuAvailable(player);
  m_menus[player & 1].AddMenu(object, name, shaderId, shader);
}

// 80188C54
void HUDTarget::AddMenuItem(int player, cXObjectImpl* object, const MenuInteraction* interaction, const std::u16string& name,
                            unsigned int shaderId, ERShader* shader, int, bool disabled) {
  IsMenuAvailable(player);
  m_menus[player & 1].AddMenuItem(object, interaction, name, shaderId, shader, disabled);
}

// 80188CC4
// An object leaving the world closes a menu that shows it.
void HUDTarget::NotifyMenuRemoveObj(cXObjectImpl* object) {
  if (m_menus[0].IsObjectInMenu(object)) m_menus[0].CloseMenu();
  if (TwoPlayers() && m_menus[1].IsObjectInMenu(object)) m_menus[1].CloseMenu();
}

// 80188D3C
void HUDTarget::CompleteMenu(int player) {
  IsMenuAvailable(player);
  m_menus[player & 1].CompleteMenu();
}

// 80188D7C
int HUDTarget::ShowMenu(int player) {
  IsMenuAvailable(player);
  return m_menus[player & 1].ShowMenu();
}

// 80188DBC
// The tutorial does not lock the menu (lock 2), the menu is ready and the HUD is up.
int HUDTarget::IsMenuAvailable(int player) const {
  return !(TutorialLocks(m_tutorial[player & 1]) & 2) && m_menus[player & 1].m_state == 1 && ShouldHUDBeVisible(player);
}

// 80188E34
void HUDTarget::CloseMenu(int player) { m_menus[player & 1].CloseMenu(); }

// 8018901C
// Each player's menu runs (the pad) and draws (through the CSM widget). GC: and the action queues
// (ActionQueueHUD::Run, not ported).
void HUDTarget::Draw(ERC* rc) {
  m_menus[0].Run();
  m_menus[0].Draw(rc);
  if (TwoPlayers()) {
    m_menus[1].Run();
    m_menus[1].Draw(rc);
  }
}

// 801890CC
void HUDTarget::ResetJobHudProperties() {
  m_mode = 0;
  m_jobLevel = 0;
  for (auto& row : m_bars)
    for (unsigned char& b : row) b = 0;
}

// 80189A9C
int HUDTarget::IsHUDVisible() const { return m_hudState == 2; }
// 80189AB0
int HUDTarget::IsPlayerHUDVisible(int player) const { return m_playerState[player] == 2; }

// ---------------------------------------------------------------------------------------------------
// H2DTarget

// 801B727C
// The H2D button screen (0x31) and each player's HUD widgets; then tells HUDTarget the HUD is up.
H2DTarget::H2DTarget() {
  AttachTables("H2DTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand("0H2D_exists", [this](char* v) { return GetVariable(v); }, this);
  const int last = TwoPlayers();
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (int p = 0; p <= last; ++p)
    for (const char* n : {"H2D_DPAD_UP", "H2D_DPAD_DOWN", "H2D_DPAD_LEFT", "H2D_DPAD_RIGHT", "H2D_PAUSE", "H2D_SELECT",
                          "H2D_CANCEL"})
      m_setTable->InstallCommand((std::to_string(p) + n).c_str(), set, this);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x31, 2);
  for (int p = 0; p <= last; ++p) m_mod[p] = new MODTarget(p);
  for (int p = 0; p <= last; ++p) m_mot[p] = new MOTTarget(p);
  for (int p = 0; p <= last; ++p) m_skl[p] = new SKLTarget(p);
  for (int p = 0; p <= last; ++p) m_waf[p] = new WAFTarget(p);
  for (int p = 0; p <= last; ++p) m_act[p] = new ACTTarget(p);
  for (int p = 0; p <= last; ++p) m_csm[p] = new CSMTarget(p);
  for (int p = 0; p <= last; ++p) m_int[p] = new INTTarget(p);
  m_mdi = new MDITarget;  // mditarget.cpp
  for (int p = 0; p <= last; ++p) {
    AptViewer* v = Viewer();
    if (!v) break;
    m_buttonFilters[p] = v->PushAptButtonFilter(p, const_cast<char*>(m_name));
    for (int cmd : {0x2a, 0x2b, 0x2c, 0x2d, 6}) v->AddCmdToAptButtonFilter(p, m_buttonFilters[p], cmd);
  }
  UIObjectBase::UIRouter_SetVariable(const_cast<char*>("HUD_initialized"), const_cast<char*>("1"));
  _gd.m_h2dTarget = this;
  // Each player's selected sim attaches to the HUD (cXPersonImpl::AttachToHUD).
  for (int p = 0; p <= last; ++p)
    if (cXPerson* person = _globals.GetSelectedPerson(p)) AttachPersonToHUD(person, true);
}

// 801B7B18
H2DTarget::~H2DTarget() {
  while (m_nPauseRefCount) ReleasePause(false);
  H2DDeleteDialogTargets(this);  // the MDITarget and ITBTarget (mditarget.cpp)
  const int last = TwoPlayers();
  for (int p = 0; p <= last; ++p)
    if (AptViewer* v = Viewer()) v->PopAptButtonFilter(p, m_buttonFilters[p]);
  for (MODTarget*& m : m_mod) {
    delete m;
    m = nullptr;
  }
  for (MOTTarget*& m : m_mot) {
    delete m;
    m = nullptr;
  }
  for (SKLTarget*& m : m_skl) {
    delete m;
    m = nullptr;
  }
  for (WAFTarget*& m : m_waf) {
    delete m;
    m = nullptr;
  }
  for (INTTarget*& m : m_int) {
    delete m;
    m = nullptr;
  }
  for (ACTTarget*& m : m_act) {
    delete m;
    m = nullptr;
  }
  for (CSMTarget*& m : m_csm) {
    delete m;
    m = nullptr;
  }
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x31);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
  }
}

// 801B7EB0
// The H2D screen's buttons ("<player>" as the value): the d-pad opens a side panel (left motives,
// up skills, right wants and fears; again or down closes it), Start the pause menu.
void H2DTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  const int p = *value - '0';
  if (p < 0 || p > 1) return;
  // PC co-op private screens (uiscreens.cpp): the H2D screen stays up for the other player (on the GC
  // food creation suspended it). The player using the screen gets none of its buttons; the other
  // player cannot open the pause menu meanwhile (it would pause and suspend food creation under it).
  if (const int owner = PcPrivateScreenPlayer(); owner >= 0) {
    if (p == owner) return;
    if (!std::strcmp("H2D_PAUSE", name)) {
      std::printf("[coop] player %d: pause held off while player %d's food creation is up\n", p + 1, owner + 1);
      return;
    }
  }
  auto toggle = [&](int panel, int icon) {
    CloseSidePanels(p);
    if (m_sidePanel[p] != panel) {
      if (panel == 2 && m_mot[p]) {
        m_mot[p]->ShowMotives();
        m_mot[p]->SetTextButtonPressed(m_textPressed[p] != 0);
      } else if (panel == 3 && m_skl[p]) {
        m_skl[p]->ShowSkills();
        m_skl[p]->SetTextButtonPressed(m_textPressed[p] != 0);
      } else if (panel == 1 && m_waf[p]) {
        m_waf[p]->ShowWantsAndFears();
        m_waf[p]->SetTextButtonPressed(m_textPressed[p] != 0);
      }
      if (m_mod[p]) {
        m_mod[p]->SetTextButtonPressed(m_textPressed[p] != 0);
        m_mod[p]->SetDpadIcons(icon);
      }
      m_sidePanel[p] = panel;
    } else {
      if (m_mod[p]) {
        m_mod[p]->SetDpadIcons(0);
        m_mod[p]->SetTextButtonPressed(false);
      }
      m_sidePanel[p] = 0;
    }
  };
  if (!std::strcmp("H2D_DPAD_LEFT", name)) {
    toggle(2, 6);
  } else if (!std::strcmp("H2D_DPAD_UP", name)) {
    if (m_cancelMode[p]) {
      if (m_act[p]) m_act[p]->MoveCancelCursorUp();
      return;
    }
    toggle(3, 5);
  } else if (!std::strcmp("H2D_DPAD_RIGHT", name)) {
    toggle(1, 7);
  } else if (!std::strcmp("H2D_DPAD_DOWN", name)) {
    if (m_cancelMode[p]) {
      if (m_act[p]) m_act[p]->MoveCancelCursorDown();
      return;
    }
    CloseSidePanels(p);
    if (m_mod[p]) {
      m_mod[p]->SetDpadIcons(8);
      m_mod[p]->SetTextButtonPressed(false);
    }
    m_sidePanel[p] = 0;
  } else if (!std::strcmp("H2D_PAUSE", name)) {
    UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), p);
    m_pausePlayer = p;
    // The selected sim's first and last names (its neighbour's user ObjSelector) for the pause menu's
    // name box. GC: the person's neighbour id (vtable 0x314, +0x1C) through Neighborhood GetNeighbor
    // (vtable 0xC4); PC: the person's neighbour (cXPersonImpl::m_neighbor). It is the sim the player
    // controls now, not the player's primary neighbour (another family member after a switch).
    if (cXPerson* person = _globals.GetSelectedPerson(p)) {
      if (Neighbor* n = PersonNeighbor(person); n && n->m_selector) {
        unsigned short first[0x100] = {}, last[0x100] = {};
        const std::u16string& f = n->m_selector->GetUserName();
        const std::u16string& l = n->m_selector->GetUserLastName();
        for (std::size_t i = 0; i < f.size() && i < 0xff; ++i) first[i] = f[i];
        for (std::size_t i = 0; i < l.size() && i < 0xff; ++i) last[i] = l[i];
        UIDB::UIDBSetString(const_cast<char*>(p == 0 ? "Player1FirstName" : "Player2FirstName"), first);
        UIDB::UIDBSetString(const_cast<char*>(p == 0 ? "Player1LastName" : "Player2LastName"), last);
      }
    }
    // Not while the player's camera director (DAT_80475EE4[p]) letterboxes a shot (+0x234: the lot's
    // intro camera, a cinematic): the pause menu (and so quitting) waits for it.
    if (CameraDirector* d = _globals.CameraDirectorOf(p); d && d->m_field234) return;
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Pause"));
  } else if (!std::strcmp("H2D_SELECT", name)) {
    // A reaches the movie only through the cancel mode's filter.
    if (m_act[p]) m_act[p]->CancelCurrentObject();
  } else if (!std::strcmp("H2D_CANCEL", name)) {
    if (!m_cancelHeld[p]) {
      m_cancelFrames[p] = 0;
      m_cancelHeld[p] = 1;
    }
  }
}

// 801B836C
char* H2DTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char* out = NewVariable();
  if (!std::strcmp("H2D_exists", name)) std::strcpy(out, "loaded");
  return out;
}

// 801B83FC
unsigned short* H2DTarget::GetLocalizable(char*) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  return FinishLocalizable(s, true);
}

// 801B8480
void H2DTarget::Draw(ERC*) {}


// Part of 801B8484 H2DTarget::Update: B (pad command 6, read through the HUD's own button filter
// +0xEC, once the HUD movie has said H2D_CANCEL) held 30 frames turns on the action queue's cancel mode
// (ACTTarget::CancelModeEnabled, with an "ACT Cancel Mode Filter" for A); let go sooner, it cancels the
// selected sim's last action (person vtable 0xC4 CancelLastAction) and, when that worked, the action
// queue shows it (ACTTarget::CancellingLastObject: the icon tinted red, sound 0x048AE94F). Let go in
// cancel mode without having waited, cancel mode ends.
void H2DTarget::UpdateCancelButton(int p) {
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(p)));
  int down = 0;
  if (pad) {
    const unsigned int mask = pad->GetBtnDownMask(m_buttonFilters[p]);
    down = pad->GetCmdResult(6, mask, mask, false) && m_cancelHeld[p] ? 1 : 0;
  }
  if (down == m_cancelHeld[p]) {
    if (!m_cancelMode[p] && down && ++m_cancelFrames[p] > 0x1d) {
      m_cancelMode[p] = m_act[p] ? m_act[p]->CancelModeEnabled(true) : 0;
      if (m_cancelMode[p] && !m_cancelFilter[p]) {
        if (m_mod[p]) m_mod[p]->SetDpadIcons(1);
        if (AptViewer* v = Viewer()) {
          m_cancelFilter[p] = static_cast<int>(v->PushAptButtonFilter(p, const_cast<char*>("ACT Cancel Mode Filter")));
          v->AddCmdToAptButtonFilter(p, static_cast<unsigned int>(m_cancelFilter[p]), 5);
        }
        std::printf("[act] player %d: cancel mode\n", p + 1);
      }
    }
    return;
  }
  if (!down) {
    if (!m_cancelMode[p]) {
      cXPerson* sel = _globals.GetSelectedPerson(p);
      auto* person = sel ? dynamic_cast<cXPersonImpl*>(sel->AsObject()->GetObjectImplementation()) : nullptr;
      if (person) {
        const int cancelled = person->CancelLastAction();
        std::printf("[sim] player %d cancels: %d\n", p + 1, cancelled);
        if (cancelled && m_act[p]) m_act[p]->CancellingLastObject();
      }
    } else if (m_cancelFrames[p] == 0) {
      m_cancelMode[p] = m_act[p] ? m_act[p]->CancelModeEnabled(false) : 0;
    }
  }
  m_cancelHeld[p] = down;
}

// 801B8484
// One widget kind per frame in turn (mood, wants and fears, skills, motives, interest meter, action
// queue), the held text buttons of the side panels, then each player's mood widget shown or hidden
// with the HUD.
void H2DTarget::Update() {
  static int count = 0;  // count_10192
  ++count;
  const int last = TwoPlayers();
  for (int p = 0; p <= last; ++p) {
    if (count % 6 == 0 && m_mod[p]) m_mod[p]->UpdateMood();
    if (count % 6 == 1 && m_waf[p]) m_waf[p]->UpdateWantsAndFears();
    if (count % 6 == 2 && m_skl[p]) m_skl[p]->UpdateSkillsBar();
    if (count % 6 == 3 && m_mot[p]) {
      m_mot[p]->UpdateMotives();
      m_mot[p]->UpdateMotiveBars();
    }
    if (count % 6 == 4 && m_int[p]) m_int[p]->UpdateInterestMeter();
    if (count % 6 == 5 && m_act[p]) m_act[p]->UpdateActionQueue();
    // GC: the side panels' text button (pad command 9) is read here too (not ported).
    UpdateCancelButton(p);
  }
  H2DUpdateIntroThoughtBalloon();  // the ITBTarget's read time (mditarget.cpp)
  for (int p = 0; p <= last; ++p) {
    MODTarget* mod = m_mod[p];
    if (!mod) continue;
    if (!ShouldH2DBeVisible(p)) {
      if (mod->IsMoodActive()) {
        mod->HideMood();
        UIDB::UIDBSetInt(const_cast<char*>("DisableAnalogNavigation"), 0);
      }
      if (m_mot[p]) m_mot[p]->HideMotives();
      if (m_waf[p]) m_waf[p]->HideWantsAndFears();
      if (m_skl[p]) m_skl[p]->HideSkills();
    } else if (!mod->IsMoodActive()) {
      mod->ShowMood();
      UIDB::UIDBSetInt(const_cast<char*>("DisableAnalogNavigation"), 1);
    }
  }
}

// 801B8A38
int H2DTarget::GetPausePlayer() { return m_pausePlayer; }

// 801B8A40
// The first pause stops the simulator (vtable 0x50) and the sounds (cSoundPlayer::PauseSounds, game
// mode 6, build/buy, with `sound`).
void H2DTarget::IncPause(bool sound) {
  if (m_nPauseRefCount == 0 && Globs_pSimulator) Globs_pSimulator->Pause();
  if (m_nPauseRefCount == 0 && Globs_pSimulator && Globs_pSound) {
    Globs_pSound->PauseSounds();
    if (sound) Globs_pSound->SetGameMode(cSoundPlayer::kBuildBuy);
  }
  ++m_nPauseRefCount;
}

// 801B8ACC
void H2DTarget::ReleasePause(bool sound) {
  if ((m_nPauseRefCount == 0 || --m_nPauseRefCount == 0) && Globs_pSimulator) {
    if (sound && Globs_pSound) Globs_pSound->SetGameMode(cSoundPlayer::kLive);
    Globs_pSimulator->Resume();
    if (Globs_pSound) Globs_pSound->ResumeSounds();
  }
}

// 801B8B54
// Hides the player's motives, skills and wants and fears panels.
void H2DTarget::CloseSidePanels(int player) {
  if (m_mot[player]) m_mot[player]->HideMotives();
  if (m_skl[player]) m_skl[player]->HideSkills();
  if (m_waf[player]) m_waf[player]->HideWantsAndFears();
}

// 801B8F8C
// The menu hides the mood widget's d-pad icons while it is up (icon set 1), and gives them back (4).
void H2DTarget::CSIMActive(int player, bool active) {
  if (MODTarget* mod = m_mod[player & 1]) mod->SetDpadIcons(active ? 1 : 4);
}

// 801B9014
// The motives icon's warning on the player's d-pad.
void H2DTarget::MOTInWarning(int player, bool on) {
  if (MODTarget* mod = m_mod[player & 1]) mod->SetWarning(3, on, false);
}

// 801B92B8
void H2DTarget::CSIMSetIconDisable(int player, bool enabled) {
  if (CSMTarget* csm = m_csm[player & 1]) csm->SetIconDisable(enabled);
}

// 801B92E8
// The interest meter mirrors the context menu's up arrow.
void H2DTarget::INTMirrorUpArrow(int player, bool on) {
  if (m_int[player]) m_int[player]->MirrorUpArrow(on);
}

// 801B8BA4
// The player's selected sim changed: the modeless dialogs go, the motive panel re-reads (jumps to) the
// new sim's bars, the sim's want / fear icons are set up, and the player's camera centres on it.
void H2DTarget::SelectedPersonChanged(int player, cXPerson* person) {
  CleanupModelessDialogs();
  if (MOTTarget* mot = m_mot[player & 1]) mot->SelectedPersonChanged(person);
  if (person) PersonInitWantFearIcons(person);  // cXPerson vtable 0x3B4
  if (ESimsCam* cam = _gd.m_players[player & 1].m_cam) cam->CenterOnSelectedSim();  // DAT_80475C84
}

// 801CA44C
// The mood meter (0..10) in sixteen steps ("a<step>" on MOD_meter_swf); a change sounds (unless the sim
// just changed, or within ten updates of the last): empty, falling or rising.
void MODTarget::SetMeterValue(float value, bool sound) {
  if (!m_exists) return;
  unsigned int step = static_cast<unsigned int>(value);
  if (step == 10) {
    step = 0x10;
  } else if (step != 0) {
    const float f = value / 10.0f * 16.0f;
    const unsigned int whole = static_cast<unsigned int>(std::floor(f));
    step = f - static_cast<float>(whole) < 0.5f ? whole : whole + 1;
  }
  if (step == m_meter) return;
  if (m_fieldA4 < 1 && sound) {
    if (step != 0x10) {
      UIAUDIO* audio = UIAUDIO::Instance();
      if (static_cast<int>(step) < 1) audio->PlaySound(0x968c0a16);
      else if (static_cast<int>(step) < static_cast<int>(m_meter)) audio->PlaySound(0x729092ae);
      else audio->PlaySound(0x9cc04cd6);
    }
    m_fieldA4 = 10;
  }
  char anim[16];
  std::snprintf(anim, sizeof anim, "a%i", step);
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("MOD_meter_swf", m_player);
  m_reflow.Reflow_SwfAnimation(anim);
  m_reflow.SubmitReflowBlock();
  m_meter = step;
}

// 801B9088
void H2DTarget::SetMoodMeterValue(int player, float value, bool sound) {
  if (MODTarget* mod = m_mod[player]) mod->SetMeterValue(value, sound);
}

// 801B90CC
// As HUDTarget::ShouldHUDBeVisible (the HUD target must exist).
int H2DTarget::ShouldH2DBeVisible(int player) const {
  if (!_gd.m_hudTarget || !SimulatorRunning() || !_gd.m_field90 || !_gd.m_field8C) return 0;
  if (CameraDirector* d = _globals.CameraDirectorOf(player)) {
    if (d->IsActive() && d->m_field234) return 0;
    if (d->IsForceDisableHud()) return 0;
  }
  if (_globals.m_field458 || _globals.m_field454) return 0;  // DAT_80476224 / DAT_80476220
  return _gd.m_hudTarget->m_quit == 0 ? 1 : 0;
}

// 801B9260
void H2DTarget::DisplayPaused() {
  if (m_mod[0]) m_mod[0]->DisplayPaused();
}
// 801B928C
void H2DTarget::DisplayFastForward() {
  if (m_mod[0]) m_mod[0]->DisplayFastForward();
}

// ---------------------------------------------------------------------------------------------------
// MODTarget

// 801C87EC
// The mood widget of `player`: its variables, its shaders (single player: the full-screen layout's;
// split screen: player 1 the left one's), the mood head drawn by callback, then the screen.
MODTarget::MODTarget(int player) : m_player(player) {
  AttachTables("MODTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  const std::string pre = std::to_string(player);
  m_getTable->InstallCommand((pre + "MOD_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"MOD_show_callback", "MOD_hide_callback", "MOD_text_show_callback", "MOD_text_hide_callback"})
    m_setTable->InstallCommand((pre + n).c_str(), set, this);
  m_locTable->InstallCommand((pre + "MOD_Aspiration").c_str(), [this](char* v) { return GetLocalizable(v); }, this);
  UI2D* ui2d = UI2D::Instance();
  auto shader = [&](const char* id, unsigned int shaderId) {
    ui2d->InstallEntry(const_cast<char*>((pre + id).c_str()), shaderId, const_cast<char*>(m_name), 0);
  };
  if (!TwoPlayers()) {
    shader("MOD_time_money_shdr", 0x1300e8c3);
    shader("MOD_head_backdrop", 0x25a9b6bb);
    shader("MOD_time_shdr", 0x9f93ff74);
    shader("MOD_money_shdr", 0x9f93ff74);
  } else {
    shader("MOD_time_shdr", player == 0 ? 0x0d06e0c4u : 0x9f93ff74u);
    shader("MOD_money_shdr", player == 0 ? 0x0d06e0c4u : 0x9f93ff74u);
    shader("MOD_time_money_shdr", 0x9f93ff74);
    shader("MOD_head_backdrop", 0x9f93ff74);
  }
  shader("MOD_dpad", 0x1e927900);
  shader("MOD_divider0a", 0x3186c4de);
  shader("MOD_divider0b", 0x3186c4de);
  shader("MOD_textBG0", 0x97eb9f9d);
  if (!TwoPlayers())
    UIUserDrawCBTable::Instance()->InstallEntry((pre + "MOD_head").c_str(), [](ERC* rc, structDrawCBparams* params) {
      DrawMoodHeadCallback(rc, params);
    }, m_name);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(player == 0 ? 0x34 : 0x35, player == 0 ? 0 : 1);
}

// 801C9178
MODTarget::~MODTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x34 : 0x35);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UIUserDrawCBTable::Instance()->UnInstallAllName(m_name);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801C9A0C
// The "MOD_head" control: each player's selected sim draws its 3D head (ESim's ESims3DHead) into the
// control's rectangle.
void MODTarget::DrawMoodHeadCallback(ERC* rc, structDrawCBparams* params) {
  for (int p = 0; p < 2; ++p)
    if (cXPerson* person = _globals.GetSelectedPerson(p)) DrawPersonMoodHead(person, rc, params);
}

// 801C92F0
// The movie's tween callbacks.
void MODTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("MOD_text_show_callback", name)) {
    m_textTweening = 0;
    m_textShown = 1;
  } else if (!std::strcmp("MOD_text_hide_callback", name)) {
    UIReflow reflow;
    reflow.ResetVisibility("MOD_short_text_container", false, m_player);
    m_textShown = 0;
    m_textTweening = 0;
  } else if (!std::strcmp("MOD_show_callback", name)) {
    m_tweening = 0;
  } else if (!std::strcmp("MOD_hide_callback", name)) {
    m_tweening = 0;
    m_reflow.ResetVisibility("MOD", false, m_player);
    m_shown = 0;
  }
}

// 801C93F4
// The movie made the screen: the d-pad's icons are set (split screen: the head is hidden).
char* MODTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char* out = NewVariable();
  if (!std::strcmp("MOD_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
    SetDpadIcons(4);
    if (TwoPlayers()) m_reflow.ResetVisibility("MOD_head", false, m_player);
  }
  return out;
}

// 801C94C0
// "MOD_Aspiration": player 1's aspiration points as money text (GC: the neighbourhood's player
// neighbour, vtable 0x168).
unsigned short* MODTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  if (!std::strcmp("MOD_Aspiration", name)) {
    m_aspirationShown = m_aspiration;
    m_aspiration = 0;  // SIMMY_STUB: aspiration points (Neighbor vtable 0x168) not ported
    makeMoneyString(m_aspiration, s, false);
  }
  return FinishLocalizable(s, true);
}

// 801C95D4
// Fades the widget in when the HUD shows it (single player: the time and money box moves next to
// the meter for player 1 in split screen) and out when it goes; times the pause, fast forward and
// aspiration chest icons; fades the short text in or out.
void MODTarget::Update() {
  if (!m_exists) return;
  if (!m_tweening) {
    const bool show = m_wantShown && !m_shown && _gd.m_h2dTarget && _gd.m_h2dTarget->ShouldH2DBeVisible(m_player);
    if (show) {
      m_reflow.ResetFontSizeOverride("MOD_text", 0xc, m_player);
      m_reflow.OpenReflowBlock();
      m_reflow.Reflow_Id("MOD", m_player);
      m_reflow.Reflow_Visibility(true);
      if (m_player == 0) {
        if (TwoPlayers()) {
          m_reflow.Reflow_Id("MOD_time_money_container", 0);
          m_reflow.Reflow_X(0x78);
          m_reflow.Reflow_Y(0x14);
          m_reflow.Reflow_Id("MOD_chest_swf", m_player);
          m_reflow.Reflow_X(0x126);
          m_reflow.Reflow_Id("MOD_money", m_player);
          m_reflow.Reflow_X(0x134);
          m_reflow.Reflow_Id("MOD_aspiration", m_player);
          m_reflow.Reflow_X(0x134);
        }
      } else {
        m_reflow.Reflow_X(0x140);
        m_reflow.Reflow_Id("MOD_time_money_container", m_player);
        m_reflow.Reflow_Visibility(false);
      }
      m_reflow.SubmitReflowBlock();
      m_reflow.OpenTweenBlock("MOD", 0xfa, 0, "MOD_show_callback", m_player);
      m_reflow.Tween_Alpha(0xff);
      m_reflow.SubmitTweenBlock();
      m_shown = 1;
      m_tweening = 1;
    } else if (!m_wantShown && m_shown) {
      m_reflow.OpenTweenBlock("MOD", 0xfa, 0, "MOD_hide_callback", m_player);
      m_reflow.Tween_Alpha(0);
      m_reflow.SubmitTweenBlock();
      m_tweening = 1;
    }
  }
  if (m_player == 0) {
    if (m_ffFrames > 0 && --m_ffFrames == 0) UIReflow().ResetVisibility("MOD_fastforward_swf", false, 0);
    if (m_pauseFrames > 0 && --m_pauseFrames == 0) UIReflow().ResetVisibility("MOD_pause_swf", false, 0);
    if (m_chestFrames) {
      if (--m_chestFrames == 0) {
        m_reflow.ResetSwfAnimation("MOD_chest_swf", "a3", 0);
        m_reflow.ResetVisibility("MOD_aspiration", false, m_player);
        m_reflow.ResetVisibility("MOD_money", true, m_player);
      }
      if (m_aspiration != m_aspirationShown) {
        m_reflow.ResetFontSizeOverride("MOD_aspiration", 0xb, m_player);
        m_aspirationShown = m_aspiration;
      }
    }
  }
  if (m_textWanted != m_textShown && !m_textTweening) {
    UIReflow reflow;
    if (!m_textWanted) {
      reflow.OpenTweenBlock("MOD_short_text_container", 0xfa, 0, "MOD_text_hide_callback", m_player);
      reflow.Tween_Alpha(0);
    } else {
      reflow.ResetVisibility("MOD_short_text_container", true, m_player);
      reflow.OpenTweenBlock("MOD_short_text_container", 0xfa, 0, "MOD_text_show_callback", m_player);
      reflow.Tween_Alpha(0xff);
    }
    reflow.SubmitTweenBlock();
    m_textTweening = 1;
  }
}

// 801C9AE4
void MODTarget::ShowMood() { m_wantShown = 1; }
// 801C9AF0
void MODTarget::HideMood() { m_wantShown = 0; }
// 801C9C84
int MODTarget::IsMoodActive() const { return m_wantShown; }

// 801C9AFC
// The money ("simoleons" in UIDB, which the money text shows) when it changed, and the time text
// redrawn every fifth call. GC: the simulator's family funds (vtable 0xA4); here the player
// family's starting funds (NeighborhoodConstants) until the simulator runs.
void MODTarget::UpdateMood() {
  static int recordOfDollars = -1;  // 804FE3D8 recordOfDollars.11681
  if (!m_exists || !m_shown) return;
  if (m_fieldA4 > 0) --m_fieldA4;
  // The wants and fears icon's warning ends after 15 updates.
  if (m_rightIcon == 2 && ++m_warningFrames > 0xe) SetWarning(4, false, false);
  if (!SimulatorRunning()) return;
  int dollars = SimulatorFunds();  // Globs::pSimulator vtable 0xA4 (the family's funds)
  // PC development aid: SIMMY_MONEY_TEST=<amount> shows that amount instead (the money text's layout).
  static const char* moneyTest = std::getenv("SIMMY_MONEY_TEST");
  if (moneyTest) dollars = std::atoi(moneyTest);
  if (dollars != recordOfDollars) {
    StackString2<256> text;
    GetMoneyString(dollars, text, false);
    UIDB::UIDBSetString(const_cast<char*>("simoleons"), const_cast<unsigned short*>(text.c_str()));
    recordOfDollars = dollars;
    m_reflow.ResetFontSizeOverride("MOD_money", 0xb, m_player);
  }
  if (m_frame++ % 5 == 0) m_reflow.ResetFontSizeOverride("MOD_time", 0xb, m_player);
}

// 801C9C8C
// The action queue's arrows in place of the up/down icons (`arrow` 0 up, 1 down selected, 6 neither).
void MODTarget::SelectDpadArrow(int arrow) {
  if (!m_arrows) {
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id("MOD_dpad_up_swf", m_player);
    m_reflow.Reflow_Visibility(false);
    m_reflow.Reflow_Id("MOD_dpad_down_swf", m_player);
    m_reflow.Reflow_Visibility(false);
    m_reflow.Reflow_Id("MOD_dpad_arrow_down_swf", m_player);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_SwfAnimation(const_cast<char*>(arrow == 1 ? "select" : "appear"));
    m_reflow.Reflow_Id("MOD_dpad_arrow_up_swf", m_player);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_SwfAnimation(const_cast<char*>(arrow == 0 ? "select" : "appear"));
    m_reflow.SubmitReflowBlock();
    m_fieldB0 = 0;
    m_arrows = 1;
    m_fieldAC = 0;
    return;
  }
  if (arrow == 1) {
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id("MOD_dpad_arrow_down_swf", m_player);
    m_reflow.Reflow_SwfAnimation(const_cast<char*>("select"));
    m_reflow.SubmitReflowBlock();
  }
  if (arrow == 0) {
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id("MOD_dpad_arrow_up_swf", m_player);
    m_reflow.Reflow_SwfAnimation(const_cast<char*>("select"));
    m_reflow.SubmitReflowBlock();
  }
}

// 801C9E4C
// The side panel's icon selected ("select") and the others back ("appear"; the down icon "a0"/"a1");
// with the arrows up (and not left/right), the arrows go and the up/down icons come back.
void MODTarget::SelectDpadButton(int panel) {
  if (!m_arrows || panel == 3 || panel == 4) {
    if (m_dpadState == panel) return;
    m_dpadState = panel;
    m_reflow.OpenReflowBlock();
    auto icon = [&](int& state, int id, const char* swf, const char* on, const char* off) {
      if (state == 1) {
        if (panel != id) {
          state = 0;
          m_reflow.Reflow_Id(swf, m_player);
          m_reflow.Reflow_SwfAnimation(const_cast<char*>(off));
        }
      } else if (panel == id) {
        state = 1;
        m_reflow.Reflow_Id(swf, m_player);
        m_reflow.Reflow_SwfAnimation(const_cast<char*>(on));
      }
    };
    icon(m_leftIcon, 3, "MOD_dpad_left_swf", "select", "appear");
    icon(m_upIcon, 2, "MOD_dpad_up_swf", "select", "appear");
    icon(m_rightIcon, 4, "MOD_dpad_right_swf", "select", "appear");
    icon(m_downIcon, 5, "MOD_dpad_down_swf", "a1", "a0");  // DAT_803F3200, DAT_803F0CF4
    m_reflow.SubmitReflowBlock();
    return;
  }
  m_arrows = 0;
  m_dpadState = panel;
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("MOD_dpad_arrow_up_swf", m_player);
  m_reflow.Reflow_Visibility(false);
  m_reflow.Reflow_Id("MOD_dpad_arrow_down_swf", m_player);
  m_reflow.Reflow_Visibility(false);
  m_reflow.Reflow_Id("MOD_dpad_up_swf", m_player);
  m_reflow.Reflow_Visibility(true);
  if (panel == 2) {
    if (m_upIcon != 1) {
      m_upIcon = 1;
      m_reflow.Reflow_SwfAnimation(const_cast<char*>("select"));
    }
  } else if (m_upIcon == 1) {
    m_upIcon = 0;
    m_reflow.Reflow_SwfAnimation(const_cast<char*>("appear"));
  }
  m_reflow.Reflow_Id("MOD_dpad_down_swf", m_player);
  m_reflow.Reflow_Visibility(true);
  if (panel == 5) {
    if (m_downIcon != 1) {
      m_downIcon = 1;
      m_reflow.Reflow_SwfAnimation(const_cast<char*>("a0"));
    }
  } else if (m_downIcon == 1) {
    m_downIcon = 0;
    m_reflow.Reflow_SwfAnimation(const_cast<char*>("a1"));
  }
  m_reflow.SubmitReflowBlock();
}

// 801CA1E8
// A side panel's d-pad icon flashes while it has news (skills: a changed skill, "a1"; motives: a need
// below 2, "a0"; wants and fears: a new want ("a1") or fear ("a0"), which also opens the aspiration
// chest) and goes back ("appear") when it is over. A selected icon is left as it is.
void MODTarget::SetWarning(int panel, bool on, bool want) {
  if (!m_exists) return;
  const char* anim = nullptr;
  const char* swf = nullptr;
  if (panel == 3) {
    swf = "MOD_dpad_left_swf";
    if (m_leftIcon == 0 && on) {
      m_leftIcon = 2;
      anim = "a0";
    } else {
      if (m_leftIcon != 2 || on) return;
      m_leftIcon = 0;
      anim = "appear";
    }
  } else if (panel == 2) {
    swf = "MOD_dpad_up_swf";
    if (m_upIcon == 0 && on) {
      m_upIcon = 2;
      anim = "a1";
    } else {
      if (m_upIcon != 2 || on) return;
      m_upIcon = 0;
      anim = "appear";
    }
  } else if (panel == 4) {
    TriggerAspirationChestDisplay();
    swf = "MOD_dpad_right_swf";
    if (m_rightIcon == 0 && on) {
      m_rightIcon = 2;
      m_warningFrames = 0;
      anim = want ? "a1" : "a0";
    } else {
      if (m_rightIcon != 2 || on) return;
      m_rightIcon = 0;
      anim = "appear";
    }
  } else {
    return;
  }
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id(swf, m_player);
  m_reflow.Reflow_SwfAnimation(const_cast<char*>(anim));
  m_reflow.SubmitReflowBlock();
}

// 801CA648
// The d-pad's side panel icons (MODTarget::eSidePanelIcon): 4 the screen is (re)shown (the arrows go,
// or the selected icon is selected again), 0 the panel closed (the down icon back), 1..3 the action
// queue's arrows (none, up, down selected), 5..8 a panel opened (up, left, right, down).
void MODTarget::SetDpadIcons(int icon) {
  if (!m_exists) return;
  switch (icon) {
    case 4:
      if (static_cast<unsigned int>(m_dpadState - 3) < 2) {
        m_reflow.OpenReflowBlock();
        m_reflow.Reflow_Id("MOD_dpad_arrow_up_swf", m_player);
        m_reflow.Reflow_Visibility(false);
        m_reflow.Reflow_Id("MOD_dpad_arrow_down_swf", m_player);
        m_reflow.Reflow_Visibility(false);
        m_reflow.Reflow_Id("MOD_dpad_up_swf", m_player);
        m_reflow.Reflow_Visibility(true);
        m_reflow.Reflow_Id("MOD_dpad_down_swf", m_player);
        m_reflow.Reflow_Visibility(true);
        m_reflow.SubmitReflowBlock();
        m_arrows = 0;
      } else {
        SelectDpadButton(m_dpadState);
      }
      break;
    case 1: SelectDpadArrow(6); break;
    case 2: SelectDpadArrow(0); break;
    case 3: SelectDpadArrow(1); break;
    case 0:
      if (m_dpadState == 5) break;
      if (!m_arrows) {
        SelectDpadButton(6);
        m_reflow.OpenReflowBlock();
        m_reflow.Reflow_Id("MOD_dpad_down_swf", m_player);
        m_reflow.Reflow_SwfAnimation(const_cast<char*>("disappear"));
        m_reflow.SubmitReflowBlock();
      } else {
        if (m_dpadState == 3) {
          m_reflow.OpenReflowBlock();
          m_reflow.Reflow_Id("MOD_dpad_left_swf", m_player);
          m_reflow.Reflow_SwfAnimation(const_cast<char*>("appear"));
          m_reflow.Reflow_Id("MOD_dpad_down_swf", m_player);
          m_reflow.Reflow_SwfAnimation(const_cast<char*>("disappear"));
          m_reflow.SubmitReflowBlock();
          m_leftIcon = 0;
        }
        if (m_dpadState == 4) {
          m_reflow.OpenReflowBlock();
          m_reflow.Reflow_Id("MOD_dpad_right_swf", m_player);
          m_reflow.Reflow_SwfAnimation(const_cast<char*>("appear"));
          m_reflow.Reflow_Id("MOD_dpad_down_swf", m_player);
          m_reflow.Reflow_SwfAnimation(const_cast<char*>("disappear"));
          m_reflow.SubmitReflowBlock();
          m_rightIcon = 0;
        }
      }
      m_downIcon = 1;
      m_dpadState = 5;
      break;
    case 5: SelectDpadButton(2); break;
    case 6: SelectDpadButton(3); break;
    case 7: SelectDpadButton(4); break;
    case 8: SelectDpadButton(5); break;
    default: break;
  }
}

// 801CA948
void MODTarget::DisplayPaused() {
  if (m_player != 0) return;
  UIReflow().ResetVisibility("MOD_pause_swf", true, 0);
  m_pauseFrames = 6;
}

// 801CA9B0
void MODTarget::DisplayFastForward() {
  if (m_player != 0) return;
  m_reflow.ResetVisibility("MOD_fastforward_swf", true, 0);
  m_ffFrames = 6;
}

// 801CAA04
void MODTarget::TriggerAspirationChestDisplay() {
  if (!m_shown || m_player != 0) return;
  if (!m_chestFrames) {
    m_reflow.ResetSwfAnimation("MOD_chest_swf", "a1", 0);  // DAT_803F3200
    m_reflow.ResetVisibility("MOD_aspiration", true, m_player);
    m_reflow.ResetVisibility("MOD_money", false, m_player);
    m_aspirationShown = 0;
    m_aspiration = 1;
  }
  m_chestFrames = 0x78;
}

// 801CAAB4
void MODTarget::SetTextButtonPressed(bool pressed) { m_textWanted = pressed ? 1 : 0; }

// ---------------------------------------------------------------------------------------------------
// MOTTarget

// 801CAABC
// The motives panel of `player`: its variables, its backdrop, dividers and text backgrounds, the
// bars read once, then the screen.
MOTTarget::MOTTarget(int player) : m_player(player) {
  AttachTables("MOTTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  const std::string pre = std::to_string(player);
  m_getTable->InstallCommand((pre + "MOT_exists").c_str(), [this](char* v) { return GetVariable(v); }, this);
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"MOT_show_callback", "MOT_hide_callback", "MOT_text_show_callback", "MOT_text_hide_callback"})
    m_setTable->InstallCommand((pre + n).c_str(), set, this);
  UI2D* ui2d = UI2D::Instance();
  auto shader = [&](const std::string& id, unsigned int shaderId) {
    ui2d->InstallEntry(const_cast<char*>((pre + id).c_str()), shaderId, const_cast<char*>(m_name), 0);
  };
  shader("MOT_sidepanel_backdrop", 0x7eaf06c1);
  for (int i = 0; i < 8; ++i) {
    const std::string n = std::to_string(i);
    shader("MOT_divider" + n + "a", 0x3186c4de);
    shader("MOT_divider" + n + "b", 0x3186c4de);
    shader("MOT_textBG" + n, 0x97eb9f9d);
  }
  for (int i = 0; i < 8; ++i) {
    m_bar[i] = m_shownBar[i] = ReadMotiveBar(i);
    m_countdown[i] = 0;
    m_frame[i] = 0x16;
    m_trend[i] = 0;
    m_real[i] = 0;
  }
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(player == 0 ? 0x36 : 0x37, 0);
}

// 801CB5A4
MOTTarget::~MOTTarget() {
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(m_player == 0 ? 0x36 : 0x37);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801CB710
// The movie's tween callbacks.
void MOTTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("MOT_text_show_callback", name)) {
    m_textTweening = 0;
    m_textShown = 1;
  } else if (!std::strcmp("MOT_text_hide_callback", name)) {
    UIReflow().ResetVisibility("MOT_short_text_container", false, m_player);
    m_textShown = 0;
    m_textTweening = 0;
  } else if (!std::strcmp("MOT_show_callback", name)) {
    m_tweening = 0;
  } else if (!std::strcmp("MOT_hide_callback", name)) {
    m_tweening = 0;
    UIReflow().ResetVisibility("MOT", false, m_player);  // DAT_803F3710
    m_shown = 0;
  }
}

// 801CB828
char* MOTTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char* out = NewVariable();
  if (!std::strcmp("MOT_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801CB8C0
unsigned short* MOTTarget::GetLocalizable(char*) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  return FinishLocalizable(s, true);
}

// 801CB944
// Fades the panel in (player 2's at x 320) or out as asked, and its short text.
void MOTTarget::Update() {
  if (!m_exists) return;
  if (!m_tweening) {
    if (!m_wantShown) {
      if (m_shown) {
        UIReflow reflow;
        reflow.OpenTweenBlock("MOT", 0xfa, 0, "MOT_hide_callback", m_player);
        reflow.Tween_Alpha(0);
        reflow.SubmitTweenBlock();
        m_tweening = 1;
      }
    } else if (!m_shown) {
      UIReflow reflow;
      reflow.OpenReflowBlock();
      reflow.Reflow_Id("MOT", m_player);
      reflow.Reflow_Visibility(true);
      if (m_player != 0) reflow.Reflow_X(0x140);
      reflow.SubmitReflowBlock();
      reflow.OpenTweenBlock("MOT", 0xfa, 0, "MOT_show_callback", m_player);
      reflow.Tween_Alpha(0xff);
      reflow.SubmitTweenBlock();
      m_shown = 1;
      m_tweening = 1;
    }
  }
  if (m_textWanted != m_textShown && !m_textTweening) {
    UIReflow reflow;
    if (!m_textWanted) {
      reflow.OpenTweenBlock("MOT_short_text_container", 0xfa, 0, "MOT_text_hide_callback", m_player);
      reflow.Tween_Alpha(0);
    } else {
      reflow.ResetVisibility("MOT_short_text_container", true, m_player);
      reflow.OpenTweenBlock("MOT_short_text_container", 0xfa, 0, "MOT_text_show_callback", m_player);
      reflow.Tween_Alpha(0xff);
    }
    reflow.SubmitTweenBlock();
    m_textTweening = 1;
  }
}

// 801CBB58
void MOTTarget::ShowMotives() { m_wantShown = 1; }
// 801CBB64
void MOTTarget::HideMotives() {
  m_textWanted = 0;
  m_wantShown = 0;
}
// 801CBB74
void MOTTarget::UpdateMotives() {}

// 801CBB78
// Each bar is read every fourth call; a changed value moves the shown bar one step at a time (or
// jumps when far off or the sim changed), with the rising/falling/steady animation ("a<frame>")
// and the icon's warning animation below 2. Then the warning on the d-pad (any bar below 2) and the
// mood meter.
void MOTTarget::UpdateMotiveBars() {
  if (!m_exists) return;
  cXPerson* person = _globals.GetSelectedPerson(m_player);  // DAT_80475E88
  bool jump = false;
  if (m_personChanged) {
    if (person != m_person) {
      m_person = person;
      if (person) {
        for (int& c : m_countdown) c = 0;
        ResetRealMotiveValues();
      }
      jump = true;
    }
    m_personChanged = 0;
  }
  if (!person) return;
  for (int i = 0; i < 8; ++i) {
    if (m_countdown[i] != 0) {
      --m_countdown[i];
      continue;
    }
    m_countdown[i] = 4;
    m_bar[i] = ReadMotiveBar(i);
    int step = 0, offset = 0;
    if (m_bar[i] == m_shownBar[i]) {
      offset = 0x16;
      if (m_trend[i] == 0) m_countdown[i] = 0;
      else if (m_trend[i] == 2) offset = 0xb;
      else if (m_trend[i] == 1) offset = 0;
    } else {
      step = m_bar[i] > m_shownBar[i] ? 1 : -1;
      offset = step > 0 ? 0 : 0xb;
    }
    int value;
    const int diff = m_bar[i] > m_shownBar[i] ? m_bar[i] - m_shownBar[i] : m_shownBar[i] - m_bar[i];
    if (!jump && m_bar[i] != 0 && diff <= 3) {
      value = m_shownBar[i] + step;
    } else {
      offset = 0x16;
      m_countdown[i] = 0;
      value = m_bar[i];
    }
    m_shownBar[i] = value;
    const int frame = offset + m_shownBar[i];
    if (m_shown && frame != m_frame[i]) {
      m_frame[i] = frame;
      char id[32], anim[16];
      UIReflow reflow;
      reflow.OpenReflowBlock();
      std::snprintf(id, sizeof id, "MOT_bar%1d", i);
      std::snprintf(anim, sizeof anim, "a%d", frame);
      reflow.Reflow_Id(id, m_player);
      reflow.Reflow_SwfAnimation(anim);
      std::snprintf(id, sizeof id, "MOT_icon%1d", i);
      reflow.Reflow_Id(id, m_player);
      reflow.Reflow_SwfAnimation(const_cast<char*>(m_shownBar[i] < 2 ? "a1" : "a0"));
      reflow.SubmitReflowBlock();
    }
  }
  if (H2DTarget* h2d = _gd.m_h2dTarget) {
    int lowest = m_bar[0];
    for (int i = 1; i < 8; ++i)
      if (m_bar[i] < lowest) lowest = m_bar[i];
    h2d->MOTInWarning(m_player, lowest < 2);
    float mood = (PersonMotive(m_person, 3) + 100.0f) / 20.0f;
    if (mood > 10.0f) mood = 10.0f;
    if (mood < 0.0f) mood = 0.0f;
    h2d->SetMoodMeterValue(m_player, mood, !jump);
  }
}

// 801CBFC0
int MOTTarget::IsMotivesActive() { return m_shown; }

// 801CBFC8
// The bar's motive as 0..10 (rounded); a move of 4 or more since the last one sets the trend.
int MOTTarget::ReadMotiveBar(int bar) {
  if (!m_person) return 0;
  const int real = static_cast<int>(PersonMotive(m_person, kBarMotive[bar]));
  const int delta = real - m_real[bar];
  if ((delta < 0 ? -delta : delta) < 4) {
    m_trend[bar] = 0;
  } else {
    m_trend[bar] = delta < 0 ? 2 : 1;
    m_real[bar] = real;
  }
  const float v = (static_cast<float>(real) + 100.0f) / 20.0f;
  int out = static_cast<int>(v);
  if (v - std::floor(v) > 0.5f) ++out;
  if (out > 10) out = 10;
  if (out < 0) out = 0;
  return out;
}

// 801CC198
void MOTTarget::ResetRealMotiveValues() {
  if (!m_person) return;
  for (int i = 0; i < 8; ++i) {
    m_real[i] = static_cast<int>(PersonMotive(m_person, kBarMotive[i]));
    m_trend[i] = 0;
  }
}

// 801CC2A0
void MOTTarget::SelectedPersonChanged(cXPerson*) { m_personChanged = 1; }
// 801CC2AC
void MOTTarget::SetTextButtonPressed(bool pressed) { m_textWanted = pressed ? 1 : 0; }

// ---------------------------------------------------------------------------------------------------
// H2DTarget: the action queue and interest meter widgets

// 801B8C98
// The action queue left cancel mode: its A filter goes, the d-pad icons come back.
void H2DTarget::ACTCancelModeExited(int player) {
  if (!m_cancelMode[player]) return;
  m_cancelMode[player] = 0;
  if (m_cancelFilter[player]) {
    if (AptViewer* v = Viewer()) v->PopAptButtonFilter(player, static_cast<unsigned int>(m_cancelFilter[player]));
    m_cancelFilter[player] = 0;
    if (m_mod[player]) m_mod[player]->SetDpadIcons(4);
  }
}
// 801B8E84
void H2DTarget::AddAction(int player, Interaction* interaction, const std::u16string& name, unsigned int shader,
                          ERShader* resource, unsigned int insertBefore) {
  if (static_cast<unsigned int>(player) < 2 && m_act[player])
    m_act[player]->AddAction(player, interaction, name, shader, resource, insertBefore);
}
// 801B8EC8
void H2DTarget::RemoveAction(int player, Interaction* interaction) {
  if (static_cast<unsigned int>(player) < 2 && m_act[player]) m_act[player]->RemoveAction(player, interaction);
}
// 801B8F04
void H2DTarget::RemoveAllActions(int player) {
  if (static_cast<unsigned int>(player) < 2 && m_act[player]) m_act[player]->RemoveAllActions(player);
}
// 801B8F40
int H2DTarget::IsActionQueueAvailable(int player) const {
  return static_cast<unsigned int>(player) < 2 && m_act[player] ? m_act[player]->IsActionQueueAvailable(player) : 0;
}
// 801B91E4
void H2DTarget::SetInteractionDisplayValues(int player, unsigned int change, unsigned int relationship) {
  if (m_int[player]) m_int[player]->SetInteractionDisplayValues(change, relationship);
}
// 801B9220
void H2DTarget::TriggerInteractionAnimation(int player, unsigned int daily, unsigned int lifetime, unsigned int bonus) {
  if (m_int[player]) m_int[player]->TriggerInteractionAnimation(daily, lifetime, bonus);
}

// 801899E8
// Back to the main menu (cXObjectImpl::TransitionToNewHouse mode 2): the HUD is quitting.
RET_UNKNOWN HUDTarget::GotoMainMenu() {
  m_quit = 1;
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenu"));
  return 0;
}

// 80189A18
void HUDTarget::RequestQuitGame(bool quit) { m_quit = quit ? 1 : 0; }
