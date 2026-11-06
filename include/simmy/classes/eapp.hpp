// @class EApp
// engine/e_app.cpp: the application object. It is itself the app thread (EApp : EThread); on PC it
// runs on the main thread (see RunOnCallingThread).
#pragma once

struct EApp : public EThread {
  EApp();
  virtual ~EApp();
  virtual void SetupPaths();  // 802E2214
  virtual void Main();
  void CreateAndStartAppThread();
  virtual void SystemInit();
  virtual void SetupForMovie();
  virtual void CleanupAfterMovie();
  virtual void SystemUpdate();
  virtual void PlayMovie(unsigned int movieId, int x, int y);
  virtual void StopMovie();
  virtual bool IsMovieStarting();
  virtual bool IsMoviePlaying();
  void SetArgs(int argc, char** argv);
  RET_UNKNOWN GetArg(char *) const;  // 802E2830
  virtual void GetMovieHeap();  // 802E28F0
  virtual RET_UNKNOWN GetRootDirectory();  // 802E2910
  virtual RET_UNKNOWN GetBuildVersion();  // 802E291C
  virtual RET_UNKNOWN GetAppName();  // 802E2928
  virtual RET_UNKNOWN GetPrefetchBlockSize();  // 802E2934
  virtual RET_UNKNOWN GetScratchBuffSize();  // 802E293C
  virtual RET_UNKNOWN GetEventTableSize();  // 802E2944
  const char* GetFullArgString() const;
  int GetGameState() const;
  virtual void SetGameState(OVERALL_GAME_STATE);  // 802E295C
  int GetMenuOwner() const;
  virtual void SetMenuOwner(OVERALL_GAME_STATE);  // 802E296C
  virtual void ShowInitialDisplay();
  virtual RET_UNKNOWN Init();
  virtual void Update();
  virtual void UpdateAfterHotSync();
  virtual int GetAppStackSize();
  virtual void Shutdown();
  void RunOnCallingThread();  // PC only

  volatile int m_quit = 0;       // +0x340, ends the Main() loop
  int m_argc = 0;                // +0x348
  char** m_argv = nullptr;       // +0x34C
  char m_argString[0x100] = {};  // +0x350, all arguments joined with spaces
  int m_movieState = 0;          // +0x450, 0 none, 1 requested, 2 playing
  int m_movieRequest = 0;        // +0x454
  ERMovie* m_movie = nullptr;    // +0x458, the movie resource being played
  unsigned int m_movieId = 0;    // +0x45C
  int m_movieX = 0;              // +0x460
  int m_movieY = 0;              // +0x464
  int m_gameState = 0;           // +0x468
  int m_menuOwner = 0;           // +0x470
};
