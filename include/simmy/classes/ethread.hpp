// @class EThread
// PC replacement for EA's thread wrapper (base/e_ngcthread.cpp). The GameCube version embedded an
// OSThread plus a hand-allocated stack (0x338 bytes); here a host std::thread does the work.
// Threads are created suspended and run when Start() is called, as on the GameCube.
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

struct EThread {
  EThread();
  EThread(int priority, int stackSize, void* stack);
  virtual ~EThread();
  void DeallocateStack();
  int Create(int priority, int stackSize, void* stack);
  static int ThreadEntryPoint(void* self);
  void Attach(int threadId);
  void Destroy();
  void Start();
  void Stop();
  void SetPriority(int priority);
  int GetPriority();
  bool IsCallingThread();
  static EThread* GetThreadObject(int threadId);
  static EThread* GetCallingThreadObject();
  void* GetStack();
  int GetStackSize();
  bool IsStackPtr(void* p);
  static EThread* GetThreadFromStackPtr(void* p);
  static int IsValidMemoryBlock(void* p, int size);
  void SetThreadName(char* name);
  const char* GetThreadName();
  virtual void Main();
  static int GetCurrentThreadId();
  void AttachToCallingThread();
  int SetupStack(int stackSize, void* stack);
  static void ValidateStack();
  static void ValidateAllStacks();
  static int GetStackUsage();
  static void PrintAllThreads();

  int m_threadId = 0;             // was +0x318: the OSThread*; now a SIMMY thread id (0 = none)
  int m_stackSize = 0;            // was +0x320; kept for diagnostics only
  int m_priority = 0;             // GameCube priority (0 = highest, 31 = lowest)
  const char* m_name = "";        // was +0x32C
  EThread* m_prev = nullptr;      // was +0x330, global thread list
  EThread* m_next = nullptr;      // was +0x334
  std::thread m_host;
  std::mutex m_startMutex;
  std::condition_variable m_startCv;
  bool m_resumed = false;
};
