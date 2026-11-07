// Port of the pause menu's game options, O2TTarget (game/targets_s2c.cpp). See o2ttarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/pc_audio.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
OptionsRecon* Options() { return static_cast<OptionsRecon*>(_globals.m_optionsRecon); }  // DAT_80475F14

unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

void CallMovie(const char* function) {
  UIObjectBase::AptViewer_CallFunction(const_cast<char*>(function), nullptr, nullptr, 0, nullptr);
}

// cSoundPlayer::SetOldFXVolume / SetMusicVolume / SetAmbientVolume / SetVOXVolume (Globs::pSound) with
// the options' values (0..10): the sound director's group volumes and the music. (The PC mixer's buses
// stay at unity; the game applies the options itself.)
void SetVolumes() {
  const OptionsRecon* o = Options();
  if (!o || !Globs_pSound) return;
  Globs_pSound->SetOldFXVolume(o->m_vol1C);
  Globs_pSound->SetMusicVolume(o->m_vol1D);
  Globs_pSound->SetAmbientVolume(o->m_vol29);
  Globs_pSound->SetVOXVolume(o->m_vol28);
}
}  // namespace

O2TTarget* O2TTarget::m_pSingletonPtr = nullptr;
signed char O2TTarget::m_CurrentSFXVolume = 0, O2TTarget::m_CurrentMusicVolume = 0,
            O2TTarget::m_CurrentAmbientVolume = 0, O2TTarget::m_CurrentVoiceVolume = 0;
signed char O2TTarget::m_CurrentScreenX = 0, O2TTarget::m_CurrentScreenY = 0;
int O2TTarget::m_bRumbleOn = 0, O2TTarget::m_bFreeWill = 0, O2TTarget::m_bCameraRotate = 0,
    O2TTarget::m_bCameraRotateDC = 0;

// 801CC2B4
// The movie's variables, the tab icons and sounds; loads the screen for `player`, shows the save and
// quit tab, remembers the options and takes every button. (`flow`: the flow that made it, 0x27.)
O2TTarget::O2TTarget(int, int player) {
  m_pSingletonPtr = this;
  m_player = player;
  AttachTables("O2TTarget");
  UIUpdateTable::InstallEntry(0x19, [this] { Update(); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  for (const char* n : {"O2T_exists", "O2T_sfx_vol", "O2T_sfx_step", "O2T_music_vol", "O2T_music_step", "O2T_screen_x",
                        "O2T_screen_x_step", "O2T_screen_y", "O2T_screen_y_step", "O2T_Free_Will",
                        "O2T_invert_cam_rot", "O2T_Get_Cam_Rot_DC", "O2T_CancelEnabled", "O2T_ambience_step",
                        "O2T_ambience_vol", "O2T_voice_step", "O2T_voice_vol", "O2T_Vibrate", "O2T_Invert_CC_Cam",
                        "O2T_Invert_DC_Cam"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"O2T_CANCEL_BUTTON", "O2T_DPAD_LEFT", "O2T_DPAD_RIGHT", "O2T_DPAD_UP", "O2T_DPAD_DOWN",
                        "O2T_X_BUTTON", "O2T_CIRCLE_BUTTON", "O2T_Change_Main_State", "O2T_Save_Game",
                        "O2T_Close_Screen", "O2T_sfx_vol", "O2T_music_vol", "O2T_screen_x", "O2T_screen_y",
                        "O2T_CurrTab", "O2T_TabsReady", "O2T_ShowHelp", "O2T_Invert_CC_Cam", "O2T_Vibrate",
                        "O2T_Free_Will", "O2T_voice_vol", "O2T_ambience_vol", "O2T_Invert_DC_Cam",
                        "O2T_ScreenWizard", "O2T_ScreenWizardExit", "O2T_Quit_Game", "O2T_SettingsSave"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"O2T_TabName", "O2T_Ambience_Vol_Text", "O2T_Voice_Vol_Text", "O2T_Sfx_Vol_Text",
                        "O2T_Controls", "O2T_Simulation", "O2T_Display", "O2T_Invert_CC_Cam_Text",
                        "O2T_Screen_Wizard_Text", "O2T_Save_Game_Text", "O2T_Quit_Game_Text", "O2T_Vibrate_Text",
                        "O2T_Music_Vol_Text", "O2T_Invert_DC_Cam_Text", "O2T_Sound_FX_Vol_Text",
                        "O2T_Invert_CM_Cam_Text", "O2T_Screen_Pos_Text", "O2T_Free_Will_Text"})
    m_locTable->InstallCommand(n, loc, this);
  UI2D* ui2d = UI2D::Instance();
  ui2d->InstallEntry(const_cast<char*>("O2T_Save_Game_Background"), 0xc2bf6784u, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("O2T_Options_Background"), 0xc2bf6784u, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("O2T_ScreenWizard"), 0x42530380u, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("O2T_ScreenWizard2"), 0xb7e82126u, const_cast<char*>(m_name), 0);
  UIAUDIO* audio = UIAUDIO::Instance();
  const struct {
    const char* name;
    unsigned int sound;
  } kSounds[] = {{"O2T_UI_BROWSE_KBD_SAMPLE", 0xcb2fa7cd}, {"O2T_UI_LEVEL_ADJUST_SAMPLE", 0xb837eaf4},
                 {"O2T_UI_CANCEL_SAMPLE", 0x2aaff358},     {"O2T_UI_SELECT_SAMPLE", 0x374bdcea},
                 {"O2T_UI_BACK_SAMPLE", 0x48ae94f},        {"O2T_SndTabChanged", 0x374bdcea},
                 {"O2T_SndError", 0x2aaff358}};
  for (const auto& s : kSounds) audio->InstallEntry(const_cast<char*>(s.name), s.sound, const_cast<char*>(m_name));
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x25, m_player);
  OnTopLevelStateChange(nullptr, nullptr);
  RememberOptions();
  UpdateText();
  m_filters[0] = PushFilterAll(0, "O2TTarget");
  m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "O2TTarget") : 0;
}

// 801CD3DC
O2TTarget::~O2TTarget() {
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  m_pSingletonPtr = nullptr;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x25);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801CD598
// The movie's buttons and option changes. Saving ("O2T_Save_Game", not while a player is in social
// mode or the simulator is busy: a help dialog then) and quitting ("O2T_Quit_Game": the memory card
// screen offers to save first) go to the slot select screen; B goes back to the root pause menu.
void O2TTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  OptionsRecon* o = Options();
  auto startSave = [this](int source) {
    m_saving = 1;
    CallMovie("O2T_GameSaveStart");
    UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), source);
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("PAUSE_SAVE"));
  };
  auto volume = [value] { return static_cast<int>(std::atof(value) * 10.0 + 0.5); };
  if (!std::strcmp("O2T_Change_Main_State", name)) {
    SetTopLevelState(name, value);
  } else if (!std::strcmp("O2T_ScreenWizard", name)) {
    StartScreenWizard(name, value);
  } else if (!std::strcmp("O2T_ScreenWizardExit", name)) {
    ExitScreenWizard(name, value);
  } else if (!std::strcmp("O2T_Quit_Game", name)) {
    if (m_closing || m_help.m_active) return;
    startSave(7);
  } else if (!std::strcmp("O2T_TabsReady", name) || !std::strcmp("O2T_CurrTab", name)) {
    if (!std::strcmp("O2T_CurrTab", name)) {
      const int tab = std::atoi(value);
      m_prevTab = m_tab;
      if (tab == 1) {
        m_tab = 1;
        RememberAudioOptions();
      } else if (tab == 2) {
        m_tab = 2;
        RememberGameOptions();
      } else if (tab == 0) {
        m_tab = 0;
      }
    }
    UpdateShaders(name, value);
  } else if (!std::strcmp("O2T_SettingsSave", name)) {
    OnStateChange();
  } else if (!std::strcmp("O2T_ShowHelp", name)) {
    ShowHelp();
  } else if (!std::strcmp("O2T_voice_vol", name)) {
    if (o) o->m_vol28 = static_cast<signed char>(volume());
    SetVolumes();
  } else if (!std::strcmp("O2T_ambience_vol", name)) {
    if (o) o->m_vol29 = static_cast<signed char>(volume());
    SetVolumes();
  } else if (!std::strcmp("O2T_Save_Game", name)) {
    // GC: EGlobal::IsPlayerInSocialMode (either player) or the simulator busy (vtable 0x34, 0x18)
    // spawn the help dialog "option_social_no_save" / "option_generic_no_save" instead.
    if (m_help.m_active || m_closing) return;
    startSave(4);
  } else if (!std::strcmp("O2T_Close_Screen", name)) {
    m_closeRequested = 1;
    Shutdown(name, value);
  } else if (!std::strcmp("O2T_sfx_vol", name)) {
    if (o) o->m_vol1C = static_cast<signed char>(volume());
    SetVolumes();
  } else if (!std::strcmp("O2T_music_vol", name)) {
    if (o) o->m_vol1D = static_cast<signed char>(volume());
    SetVolumes();
  } else if (!std::strcmp("O2T_screen_x", name)) {
    if (o) o->m_opt1F = static_cast<signed char>(GetScreenXGlobal(static_cast<float>(std::atof(value))));
    SIMMY_STUB("O2TTarget::SetVariable: EGlobal::UseOptionsScreenAdjustXY", 0x801CD598);
  } else if (!std::strcmp("O2T_screen_y", name)) {
    if (o) o->m_opt20 = static_cast<signed char>(GetScreenYGlobal(static_cast<float>(std::atof(value))));
    SIMMY_STUB("O2TTarget::SetVariable: EGlobal::UseOptionsScreenAdjustXY", 0x801CD598);
  } else if (!std::strcmp("O2T_Vibrate", name)) {
    const int on = std::atoi(value);
    if (o) o->m_opt04 = on != 0;
    // Turned on: a tenth of a second's rumble on the player's pad.
    if (on && _globals.m_vibrate && _pCtrlMan)
      _globals.m_vibrate->VibrateAll(static_cast<unsigned char>(_pCtrlMan->GetPlayerControllerIndex(static_cast<unsigned int>(m_player))),
                                     1.0f, 1.0f, 0.1f, 0.1f);
  } else if (!std::strcmp("O2T_Free_Will", name)) {
    if (o) o->m_opt00 = std::atoi(value) != 0;
  } else if (!std::strcmp("O2T_Invert_CC_Cam", name)) {
    if (o) o->m_opt18 = std::atoi(value) != 0;
  } else if (!std::strcmp("O2T_Invert_DC_Cam", name)) {
    if (o) o->m_opt24 = std::atoi(value) != 0;
  } else if (!std::strcmp("O2T_DPAD_LEFT", name)) {
    OnLeftKeyPressed(name, value);
  } else if (!std::strcmp("O2T_DPAD_RIGHT", name)) {
    OnRightKeyPressed(name, value);
  } else if (!std::strcmp("O2T_DPAD_UP", name)) {
    OnUpKeyPressed(name, value);
  } else if (!std::strcmp("O2T_DPAD_DOWN", name)) {
    OnDownKeyPressed(name, value);
  } else if (!std::strcmp("O2T_X_BUTTON", name)) {
    OnXKeyPressed(name, value);
  } else if (!std::strcmp("O2T_CIRCLE_BUTTON", name)) {
    OnCircleKeyPressed(name, value);
  } else if (!std::strcmp("O2T_CANCEL_BUTTON", name)) {
    UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));
  }
}

// 801CDF34
// The options' values for the movie (volumes and screen position as 0..1, steps of 0.1 / 0.02).
char* O2TTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  OptionsRecon* o = Options();
  auto real = [out](double v) { std::snprintf(out, 0x20, "%f", v); };  // DAT_803EF740
  auto flag = [out](int v) { std::snprintf(out, 0x20, "%d", v != 0); };
  auto vol = [&](signed char v) { real(static_cast<float>(v) / 10.0f); };
  if (!std::strcmp("O2T_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  } else if (!std::strcmp("O2T_ambience_step", name) || !std::strcmp("O2T_voice_step", name) ||
             !std::strcmp("O2T_sfx_step", name) || !std::strcmp("O2T_music_step", name)) {
    real(0.1);
  } else if (!std::strcmp("O2T_screen_x_step", name) || !std::strcmp("O2T_screen_y_step", name)) {
    real(0.02);
  } else if (!o) {
  } else if (!std::strcmp("O2T_ambience_vol", name)) {
    vol(o->m_vol29);
  } else if (!std::strcmp("O2T_voice_vol", name)) {
    vol(o->m_vol28);
  } else if (!std::strcmp("O2T_sfx_vol", name)) {
    vol(o->m_vol1C);
  } else if (!std::strcmp("O2T_music_vol", name)) {
    vol(o->m_vol1D);
  } else if (!std::strcmp("O2T_Invert_CC_Cam", name) || !std::strcmp("O2T_invert_cam_rot", name)) {
    flag(o->m_opt18);
  } else if (!std::strcmp("O2T_Invert_DC_Cam", name) || !std::strcmp("O2T_Get_Cam_Rot_DC", name)) {
    flag(o->m_opt24);
  } else if (!std::strcmp("O2T_screen_x", name)) {
    real(GetScreenXLocal(o->m_opt1F));
  } else if (!std::strcmp("O2T_screen_y", name)) {
    real(GetScreenYLocal(o->m_opt20));
  } else if (!std::strcmp("O2T_Vibrate", name)) {
    flag(o->m_opt04);
  } else if (!std::strcmp("O2T_Free_Will", name)) {
    flag(o->m_opt00);
  }
  if (!std::strcmp("O2T_CancelEnabled", name)) std::strcpy(out, !m_saving && !m_help.m_active ? "1" : "0");
  return out;
}

// 801CE3BC
// The tab name (PDAStrings) and the options' labels (NewUIStrings; "Save" from the memory card's).
unsigned short* O2TTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  const unsigned short* const* row = nullptr;
  if (!std::strcmp("O2T_Save_Game_Text", name)) {
    row = _globals.GetMemCardString("ngc_save");
  } else if (!std::strcmp("O2T_TabName", name)) {
    static const char* const kTabs[] = {"header_options_savequit", "header_options_audio", "header_options_other"};
    if (m_tab >= 0 && m_tab <= 2) row = _globals.GetPDAString(kTabs[m_tab]);
  } else {
    static const struct {
      const char* var;
      const char* text;
    } kTexts[] = {
        {"O2T_Ambience_Vol_Text", "option_ambient_volume"}, {"O2T_Voice_Vol_Text", "option_voice_volume"},
        {"O2T_Sfx_Vol_Text", "option_sfx_volume"},          {"O2T_Controls", "option_label_controls"},
        {"O2T_Simulation", "option_label_simulation"},      {"O2T_Display", "option_label_display"},
        {"O2T_Invert_CC_Cam_Text", "option_invert_camera"}, {"O2T_Screen_Wizard_Text", "option_calib_screen"},
        {"O2T_Quit_Game_Text", "pause_quit"},               {"O2T_Vibrate_Text", "option_rumble_ngc"},
        {"O2T_Music_Vol_Text", "option_music_volume"},      {"O2T_Invert_DC_Cam_Text", "option_invert_camera_dc"},
        {"O2T_Sound_FX_Vol_Text", "option_sfx_volume"},     {"O2T_Invert_CM_Cam_Text", "option_invert_camera"},
        {"O2T_Screen_Pos_Text", "option_calib_position"},   {"O2T_Free_Will_Text", "option_freewill"},
    };
    for (const auto& t : kTexts)
      if (!std::strcmp(t.var, name)) row = _globals.GetNewUIString(t.text);
  }
  unsigned short buf[0x1000] = {};
  std::size_t n = 0;
  if (row && *row)
    while ((*row)[n] && n < 0xfff) buf[n] = (*row)[n], ++n;
  UIQDTarget::UIQDSubstitutionStrings(buf);
  n = 0;
  while (buf[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, buf, (n + 1) * 2);
  return out;
}

// 801CE920
void O2TTarget::SetTopLevelState(char* name, char* value) {
  m_prevTab = m_tab;
  m_tab = std::atoi(value);
  OnTopLevelStateChange(name, value);
}

// 801CE974
// The save and quit container shows on tab 0, the options' on tab 2.
void O2TTarget::OnTopLevelStateChange(char*, char*) {
  UIReflow reflow;
  reflow.OpenReflowBlock();
  if (m_tab == 0 || m_tab == 2) {
    reflow.Reflow_Id("O2T_Save_Container", 0);
    reflow.Reflow_Visibility(m_tab == 0);
    reflow.Reflow_Id("O2T_Options_Container", 0);
    reflow.Reflow_Visibility(m_tab == 2);
  }
  reflow.SubmitReflowBlock();
  UpdateText();
}

// 801CEA60
void O2TTarget::SaveGame(char*, char*) {
  m_quitDialog = 0;
  m_saving = 1;
}

// 801CEA74
void O2TTarget::OnSaveGameComplete(bool) {
  CallMovie("O2T_GameSaveDone");
  m_saving = 0;
}

// 801CEAC0
void O2TTarget::Shutdown(char*, char*) {
  if (m_saving || m_help.m_active) return;
  m_closing = 1;
  UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("Pause"));
}

// 801CEB2C
// Leaving an options tab: changed settings ask to be kept (SpawnSaveSettingsDialog), else the movie goes on.
void O2TTarget::OnStateChange() {
  const bool changed = (m_tab == 1 && AudioOptionsModified()) || (m_tab == 2 && GameOptionsModified());
  if (changed && !m_closeRequested) {
    SpawnSaveSettingsDialog();
    return;
  }
  CallMovie("O2T_SettingsSaveDone");
}

// 801CEC90
// "option_save_prompt" with yes / no (yes preselected); the answer goes to OnSaveSettingsDialogClose.
void O2TTarget::SpawnSaveSettingsDialog() {
  auto text = [](const char* id) {
    std::vector<unsigned short> v;
    const unsigned short* const* h = _globals.GetNewUIString(id);
    for (const unsigned short* p = h ? *h : nullptr; p && *p; ++p) v.push_back(*p);
    return v;
  };
  m_dialog.m_continueType = 0;
  m_dialog.m_numSelections = 2;
  m_closeRequested = 1;
  m_dialog.m_sel[0] = text("yes");  // DAT_803EFAC0
  m_dialog.m_sel[1] = text("no");   // DAT_803EFAC4
  m_dialog.m_body = text("option_save_prompt");
  m_dialog.m_field14 = 0;
  m_dialog.m_field20 = 0;
  m_dialog.m_onSelection = [this](int selection) { OnSaveSettingsDialogClose(selection); };
  m_dialog.m_onZeroInput = [this] { m_dialog.ZeroInputCallback(); };
  m_dialog.m_onClosed = [this] { m_dialog.DialogClosedCallback(); };
  m_dialog.SpawnDialog();
}

// 801CEEE8
// "No" puts the tab's options back as they were; then the movie goes on.
void O2TTarget::OnSaveSettingsDialogClose(int selection) {
  if (selection == 1) {
    if (m_tab == 1) RestoreAudioOptions();
    else if (m_tab == 2) RestoreGameOptions();
  }
  CallMovie("O2T_SettingsSaveDone");
  m_closeRequested = 0;
}

// 801CEBE8
int O2TTarget::AudioOptionsModified() {
  const OptionsRecon* o = Options();
  return o && (o->m_vol1C != m_CurrentSFXVolume || o->m_vol1D != m_CurrentMusicVolume ||
               o->m_vol29 != m_CurrentAmbientVolume || o->m_vol28 != m_CurrentVoiceVolume);
}
// 801CEC3C
int O2TTarget::GameOptionsModified() {
  const OptionsRecon* o = Options();
  return o && (o->m_opt04 != m_bRumbleOn || o->m_opt18 != m_bCameraRotate || o->m_opt24 != m_bCameraRotateDC ||
               o->m_opt00 != m_bFreeWill);
}
// 801CEF60
void O2TTarget::RememberAudioOptions() {
  if (const OptionsRecon* o = Options()) {
    m_CurrentAmbientVolume = o->m_vol29;
    m_CurrentVoiceVolume = o->m_vol28;
    m_CurrentSFXVolume = o->m_vol1C;
    m_CurrentMusicVolume = o->m_vol1D;
  }
}
// 801CEF8C
void O2TTarget::RestoreAudioOptions() {
  if (OptionsRecon* o = Options()) {
    o->m_vol1C = m_CurrentSFXVolume;
    o->m_vol1D = m_CurrentMusicVolume;
    o->m_vol29 = m_CurrentAmbientVolume;
    o->m_vol28 = m_CurrentVoiceVolume;
    SetVolumes();
  }
}
// 801CF028
void O2TTarget::RememberGameOptions() {
  if (const OptionsRecon* o = Options()) {
    m_bRumbleOn = o->m_opt04;
    m_bFreeWill = o->m_opt00;
    m_bCameraRotate = o->m_opt18;
    m_bCameraRotateDC = o->m_opt24;
  }
}
// 801CF054
void O2TTarget::RestoreGameOptions() {
  if (OptionsRecon* o = Options()) {
    o->m_opt04 = m_bRumbleOn;
    o->m_opt18 = m_bCameraRotate;
    o->m_opt24 = m_bCameraRotateDC;
    o->m_opt00 = m_bFreeWill;
  }
}
// 801CF090
void O2TTarget::RememberOptions() {
  RememberAudioOptions();
  RememberGameOptions();
  if (const OptionsRecon* o = Options()) {
    m_CurrentScreenX = o->m_opt1F;
    m_CurrentScreenY = o->m_opt20;
  }
}
// 801CF0EC
void O2TTarget::RestoreOptions() {
  RestoreAudioOptions();
  if (OptionsRecon* o = Options()) {
    o->m_opt1F = m_CurrentScreenX;
    o->m_opt20 = m_CurrentScreenY;
  }
  RestoreGameOptions();
}

// 801CF1D8
// An offset -25..25 as 0..1.
double O2TTarget::GetScreenXLocal(signed char x) {
  return x < 0 ? (x + 25) / 25.0f * 0.5f : x / 25.0f * 0.5f + 0.5f;
}
// 801CF2CC
double O2TTarget::GetScreenYLocal(signed char y) {
  return y < 0 ? (y + 25) / 25.0f * 0.5f : y / 25.0f * 0.5f + 0.5f;
}
// 801CF26C
// 0..1 back to the offset.
int O2TTarget::GetScreenXGlobal(float x) {
  const float v = x < 0.5f ? (1.0f - (x + x)) * -25.0f : ((x - 0.5f) + (x - 0.5f)) * 25.0f + 0.5f;
  return static_cast<signed char>(static_cast<int>(v));
}
// 801CF360
int O2TTarget::GetScreenYGlobal(float y) { return GetScreenXGlobal(y); }

// 801CF3C0
// Every label white, the selected row's green.
void O2TTarget::UpdateText() {
  static const char* const kSave[] = {"O2T_Save_Game_Text", "O2T_Quit_Game_Text"};
  static const char* const kOptions[] = {"O2T_Vibrate_Text",      "O2T_Invert_DC_Cam_Text", "O2T_Invert_CM_Cam_Text",
                                         "O2T_Music_Vol_Text",    "O2T_Sound_FX_Vol_Text",  "O2T_Screen_Pos_Text",
                                         "O2T_Free_Will_Text"};
  UIReflow reflow;
  reflow.OpenReflowBlock();
  for (const char* id : kSave) reflow.Reflow_Id(id, 0), reflow.Reflow_ColorOnlyOverride(0xffffff);
  for (const char* id : {"O2T_Vibrate_Text", "O2T_Music_Vol_Text", "O2T_Invert_DC_Cam_Text", "O2T_Sound_FX_Vol_Text",
                         "O2T_Invert_CM_Cam_Text", "O2T_Screen_Pos_Text", "O2T_Free_Will_Text"})
    reflow.Reflow_Id(id, 0), reflow.Reflow_ColorOnlyOverride(0xffffff);
  reflow.SubmitReflowBlock();
  reflow.OpenReflowBlock();
  if (m_tab == 0 && m_saveRow >= 0 && m_saveRow <= 1) {
    reflow.Reflow_Id(kSave[m_saveRow], 0);
    reflow.Reflow_ColorOnlyOverride(0xff00);
  } else if (m_tab == 2 && m_optionRow >= 0 && m_optionRow <= 6) {
    reflow.Reflow_Id(kOptions[m_optionRow], 0);
    reflow.Reflow_ColorOnlyOverride(0xff00);
  }
  reflow.SubmitReflowBlock();
}

// 801CF704
void O2TTarget::OnXKeyPressed(char*, char*) {
  if (m_tab != 0) return;
  if (m_saveRow == 0 || m_saveRow == 1) {
    UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), m_saveRow == 0 ? 4 : 7);
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("PAUSE_SAVE"));
  }
}

// 801CF780
// The options' Quit: "save before quitting?" (the memory card strings) yes / no / cancel: yes saves first
// ("PAUSE_SAVE"), no leaves for the main menu (OnQuitGameDialogClose); no input or cancel closes it.
void O2TTarget::SpawnQuitDialogBox() {
  auto text = [](const unsigned short* const* h) {
    std::vector<unsigned short> v;
    for (const unsigned short* p = h ? *h : nullptr; p && *p; ++p) v.push_back(*p);
    return v;
  };
  m_quitBox.m_continueType = 0;
  m_quitBox.m_field14 = 0;
  m_quitBox.m_numSelections = 3;
  m_quitBox.m_sel[0] = text(_globals.GetNewUIString("yes"));  // DAT_803EFAC0
  m_quitBox.m_sel[1] = text(_globals.GetNewUIString("no"));   // DAT_803EFAC4
  m_quitBox.m_sel[2] = text(_globals.GetNewUIString("cancel"));
  m_quitBox.m_body = text(_globals.GetMemCardString("ngc_yesno_savebeforequit_body"));
  m_quitBox.m_title.clear();  // DAT_803EDFA4 ""
  m_quitBox.m_field20 = 0;
  m_quitBox.m_onSelection = [this](int selection) { OnQuitGameDialogClose(selection); };
  m_quitBox.m_onZeroInput = [this] { OnCancelDialog(); };
  m_quitBox.m_onClosed = [this] { m_quitBox.DialogClosedCallback(); };
  m_quitBox.SpawnDialog();
  CallMovie("O2T_GameSaveStart");
}

// 801CFA60
void O2TTarget::OnCancelDialog() { m_quitDialog = 0; }

// 801CFA6C
void O2TTarget::OnQuitGameDialogClose(int choice) {
  if (choice == 1) {
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenu"));
  } else if (choice == 0) {
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("PAUSE_SAVE"));
  } else {
    CallMovie("O2T_GameSaveDone");
    m_quitDialog = 0;
  }
}

// 801CFAE8
void O2TTarget::OnUpKeyPressed(char*, char*) {
  if (m_tab == 0 && --m_saveRow < 0) m_saveRow = 1;
  else if (m_tab == 2 && --m_optionRow < 0) m_optionRow = 6;
  UpdateText();
}
// 801CFB58
void O2TTarget::OnDownKeyPressed(char*, char*) {
  if (m_tab == 0 && ++m_saveRow > 1) m_saveRow = 0;
  else if (m_tab == 2 && ++m_optionRow > 6) m_optionRow = 0;
  UpdateText();
}
// 801CFBC4
void O2TTarget::OnRightKeyPressed(char*, char*) {
  if (m_tab == 0) return;
  if (m_tab == 2) {
    if (m_optionRow == 3 && m_CurrentMusicVolume < 10) ++m_CurrentMusicVolume;
    else if (m_optionRow == 4 && m_CurrentSFXVolume < 10) ++m_CurrentSFXVolume;
  }
  RestoreOptions();
}
// 801CFC40
void O2TTarget::OnLeftKeyPressed(char*, char*) {
  if (m_tab == 0) return;
  if (m_tab == 2) {
    if (m_optionRow == 3 && m_CurrentMusicVolume > 0) --m_CurrentMusicVolume;
    else if (m_optionRow == 4 && m_CurrentSFXVolume > 0) --m_CurrentSFXVolume;
  }
  RestoreOptions();
}
// 801CFCBC
// The next tab (after the last, the first).
void O2TTarget::OnCircleKeyPressed(char*, char*) {
  char text[16];
  std::snprintf(text, sizeof text, "%d", m_tab == 2 ? 0 : m_tab + 1);
  SetVariable(const_cast<char*>("0O2T_Change_Main_State"), text);
}

// 801CFD30
// The three tab icons, the current one lit.
void O2TTarget::UpdateShaders(char*, char* value) {
  const int tab = std::atoi(value);
  unsigned int icons[3];
  if (tab == 0) icons[0] = 0x813f6e00, icons[1] = 0xbe13dfa7, icons[2] = 0x29b3aeee;
  else if (tab == 2) icons[0] = 0x8b58f71b, icons[1] = 0xbe13dfa7, icons[2] = 0x6e910274;
  else icons[0] = 0x8b58f71b, icons[1] = 0xf1285ff3, icons[2] = 0x29b3aeee;
  if (!m_installed) return;
  static const char* const kIcons[] = {"O2T_TabIcon0", "O2T_TabIcon1", "O2T_TabIcon2"};
  for (int i = 0; i < 3; ++i)
    UI2D::Instance()->InstallEntry(const_cast<char*>(kIcons[i]), icons[i], const_cast<char*>(m_name), 0);
}

// 801CFF44
void O2TTarget::ShowHelp() {
  if (m_closing || m_saving || m_help.m_active) return;
  if (ScreenManager() && ScreenManager()->IsScreenManaged(0x2f)) return;
  m_help.SpawnHelpDialog(m_player, "option_help", nullptr, true, nullptr);
}

// 801CFFC8
void O2TTarget::StartScreenWizard(char*, char* value) {
  if (m_installed)
    UI2D::Instance()->InstallEntry(const_cast<char*>("O2T_ScreenWizard"), std::atoi(value) == 1 ? 0x42530380u : 0xb7e82126u,
                                   const_cast<char*>(m_name), 0);
}
// 801D006C
void O2TTarget::ExitScreenWizard(char*, char*) {
  if (m_installed)
    UI2D::Instance()->InstallEntry(const_cast<char*>("O2T_ScreenWizard"), 0x9f93ff74u, const_cast<char*>(m_name), 0);
}
// 801D00C0
void O2TTarget::QuitGame(char*, char*) {
  m_quitDialog = 1;
  SpawnQuitDialogBox();
}
