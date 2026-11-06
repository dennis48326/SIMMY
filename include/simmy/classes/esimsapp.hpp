// @class ESimsApp
// game/appmain.cpp: The Sims 2 application (global object `_app`).
#pragma once

struct ESimsApp : public EApp {
  static RET_UNKNOWN GetStartLot();  // 80003FDC
  void SetStartLot(int);  // 80003FE4
  void SetNghName(char *);  // 80003FEC
  void parseCommandLine();  // 80004074
  ESimsApp();
  virtual ~ESimsApp();
  virtual void Shutdown();  // 800043C8
  virtual RET_UNKNOWN GetBuildVersion();  // 800044B4
  static RET_UNKNOWN GetDefaultLanguage();  // 800044C0
  virtual RET_UNKNOWN Init();  // 800044C8
  virtual void ShowInitialDisplay();  // 80004B8C
  void initContinue();  // 80004C80
  virtual void SetGameState(OVERALL_GAME_STATE);  // 80005410
  static void LoadSimulatorGlobs();  // 80005418
  static void UpdateCheats();  // 80005448
  static void UpdateApt();  // 800054BC
  static void UpdateGame();  // 80005514
  static void UpdateShaders(float);  // 80005554
  void UpdateAudio();  // 800056F0
  void UpdateDraw();  // 80005808
  static void UpdateReset();  // 80005ACC
  virtual void Update();  // 80005BA4
  virtual void SetupForMovie();  // 80005CAC
  virtual void CleanupAfterMovie();  // 80005CD4
  virtual void GetMovieHeap();  // 80005CFC
  void TakeBigScreenshot(int, int);  // 80005D2C
  virtual RET_UNKNOWN GetAppName();  // 80006380
  virtual RET_UNKNOWN GetEventTableSize();  // 8000638C
  static RET_UNKNOWN GetNghName();  // 80006394

  EGameStateMan* m_gameStates = nullptr;  // +0x478
  ERC* m_drawRC = nullptr;                // +0x47C the immediate context while UpdateDraw runs
  EWindow* m_window = nullptr;            // +0x480 full-screen window
  char m_field484 = 0;
  int m_field488 = 0;
  int m_field48C = 0;
  int m_field490 = 0;
  int m_field498 = 0;
  int m_bigScreenshot = 0;                // +0x49C TakeBigScreenshot requested
  int m_screenshotTilesX = 0;             // +0x4A0
  int m_screenshotTilesY = 0;             // +0x4A4
};
