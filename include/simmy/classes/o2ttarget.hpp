// @class O2TTarget after UIObjectBase HelpDialog UIDialog
// The pause menu's game options (game/targets_s2c.cpp O2TTarget, screen 0x25 "O2T",
// s2c_pause_game.swf): three tabs, save and quit (0), audio (1) and the other options (2: rumble,
// inverted cameras, music and effects volume, screen position, free will). The values live in the
// options block (OptionsRecon, DAT_80475F14); the tab remembers them to tell what changed. Saving
// and quitting go through the memory card screen (MEM_source 4 or 7, "PAUSE_SAVE").
#pragma once

// 0x25C bytes.
struct O2TTarget : UIObjectBase {
  O2TTarget(int flow, int player);  // 801CC2B4
  ~O2TTarget() override;  // 801CD3DC
  void SetVariable(char* name, char* value) override;  // 801CD598
  char* GetVariable(char* name) override;  // 801CDF34
  unsigned short* GetLocalizable(char* name) override;  // 801CE3BC
  void SetTopLevelState(char* name, char* value);  // 801CE920
  void OnTopLevelStateChange(char* name, char* value);  // 801CE974
  void SaveGame(char* name, char* value);  // 801CEA60
  void OnSaveGameComplete(bool ok);  // 801CEA74
  void Shutdown(char* name, char* value);  // 801CEAC0
  void OnStateChange();  // 801CEB2C
  static int AudioOptionsModified();  // 801CEBE8
  static int GameOptionsModified();  // 801CEC3C
  static void RememberAudioOptions();  // 801CEF60
  static void RestoreAudioOptions();  // 801CEF8C
  static void RememberGameOptions();  // 801CF028
  static void RestoreGameOptions();  // 801CF054
  static void RememberOptions();  // 801CF090
  static void RestoreOptions();  // 801CF0EC
  double GetScreenXLocal(signed char x);  // 801CF1D8
  static int GetScreenXGlobal(float x);  // 801CF26C
  double GetScreenYLocal(signed char y);  // 801CF2CC
  static int GetScreenYGlobal(float y);  // 801CF360
  void UpdateText();  // 801CF3C0
  void OnXKeyPressed(char* name, char* value);  // 801CF704
  void OnCancelDialog();  // 801CFA60
  void OnQuitGameDialogClose(int choice);  // 801CFA6C
  void OnUpKeyPressed(char* name, char* value);  // 801CFAE8
  void OnDownKeyPressed(char* name, char* value);  // 801CFB58
  void OnRightKeyPressed(char* name, char* value);  // 801CFBC4
  void OnLeftKeyPressed(char* name, char* value);  // 801CFC40
  void OnCircleKeyPressed(char* name, char* value);  // 801CFCBC
  void UpdateShaders(char* name, char* value);  // 801CFD30
  void ShowHelp();  // 801CFF44
  void StartScreenWizard(char* name, char* value);  // 801CFFC8
  void ExitScreenWizard(char* name, char* value);  // 801D006C
  void QuitGame(char* name, char* value);  // 801D00C0

  void SpawnSaveSettingsDialog();  // 801CEC90
  void OnSaveSettingsDialogClose(int);  // 801CEEE8
  void SpawnQuitDialogBox();  // 801CF780

  int m_exists = 0;          // +0x84
  int m_tab = 0;             // +0x88 0 save and quit, 1 audio, 2 other options
  int m_prevTab = 0;         // +0x8C
  int m_saving = 0;          // +0x90 a save (or quit) was started
  int m_saveRow = 0;         // +0x9C 0 save, 1 quit
  int m_optionRow = 0;       // +0xA0 the options tab's row (0..6)
  UIDialog m_quitBox;        // +0xA4 "save before quitting?" yes / no / cancel
  int m_player = 0;          // +0x1B4
  unsigned int m_filters[2] = {};  // +0x1B8
  UIDialog m_dialog;         // +0x12C the save-settings question
  HelpDialog m_help;         // +0x1C0 the options' help (its m_active, +0x248: the help is up)
  int m_quitDialog = 0;      // +0x250 the quit dialog is up
  int m_closing = 0;         // +0x254
  int m_closeRequested = 0;  // +0x258 (also: the save-settings question is up)

  static O2TTarget* m_pSingletonPtr;  // 804FE4D0
  // The options as the tab found them (O2TTarget's statics).
  static signed char m_CurrentSFXVolume, m_CurrentMusicVolume, m_CurrentAmbientVolume, m_CurrentVoiceVolume;
  static signed char m_CurrentScreenX, m_CurrentScreenY;
  static int m_bRumbleOn, m_bFreeWill, m_bCameraRotate, m_bCameraRotateDC;
};
