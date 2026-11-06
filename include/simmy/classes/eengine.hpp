// @class EEngine after ESemaphore EClock EEvent
// engine/e_engine.cpp. Layout recovered from the constructor and accessors (offsets are the
// GameCube ones, kept for cross-reference with the raw decompilation).
#pragma once

struct EEngine : public EGlobalManagerClient {
  EEngine();
  virtual ~EEngine();
  virtual int ManagedStartup();
  virtual void ManagedShutdown();
  static void Line();
  virtual int Init();
  virtual void ShutdownThreads(bool waitForThreads);
  virtual void EnterMovieMode();  // 802E2E50
  virtual void ExitMovieMode();  // 802E2E8C
  virtual void PreFrameUpdate();  // 802E2EC8
  virtual void PostFrameUpdate();  // 802E317C
  static void PrintBanner();
  static void PrintConfiguration();
  virtual int InitSubsystems();
  virtual void InitFileSystem();
  virtual int InitResourceManagers();  // 802E3438
  virtual void ShutdownResourceManagers();  // 802E3658
  virtual void Idle();
  virtual void GetDMARingMemoryUseInfo(HeapUseInfo *);  // 802E37D4
  virtual void GetTextureRingMemoryUseInfo(HeapUseInfo *);  // 802E37F8
  virtual void GetMatrixRingMemoryUseInfo(HeapUseInfo *);  // 802E381C
  virtual void GetMemoryUseSystemInfo(HeapUseInfo *);  // 802E3840
  virtual void GetMemoryUseDmaInfo(HeapUseInfo *);  // 802E3868
  virtual void Reboot();
  virtual void SignalShutdown();

  void RetraceUpdate(float);  // 802E2D5C
  int GetMinRetraces();
  void FrameComplete();  // 802E31FC
  void ClearCurrentLevel(ERLevel *);  // 802E37B8
  void EnableFrameRatePredictiveSmoothing(bool enable);
  void SetCurrentLevel(ERLevel* level);
  ERLevel* GetCurrentLevel();
  void SetResetAllowed(bool allowed);
  bool CanReset();

  int m_initialized = 0;          // +0x04, set once Init() succeeds
  int m_predictiveSmoothing = 0;  // +0x08, see PreFrameUpdate
  int m_resetAllowed = 1;         // +0x0C
  EClock m_frameClock;                    // +0x10, frame timer (EClock layout not recovered yet)
  EClock m_cpuClock;                      // +0x20, CPU-time timer
  ERLevel* m_currentLevel = nullptr;      // +0x30
  EEvent m_frameSemaphore;                // +0x34, renderer <-> game frame handshake
};
