// @class BackgroundImpl after Background
// Background worker (engine/background.cpp): runs queued routines on its own thread.
#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

struct BackgroundImpl : public Background {
  BackgroundImpl();
  virtual ~BackgroundImpl();
  virtual void Init();
  virtual void Shutdown();
  virtual void TerminateThread();
  virtual int IsCallingThread();
  virtual void Update();
  virtual void Flush();
  virtual void ExecuteRoutine(void (*fn)());
  virtual void ExecuteArgRoutine(void (*fn)(void*), void* arg);
  void SendCommand(BackgroundCmd *);  // 802E1F00
  void Main();

  std::thread m_thread;
  std::mutex m_mutex;
  std::condition_variable m_cv, m_idleCv;
  std::deque<std::function<void()>> m_queue;
  bool m_busy = false;
  bool m_stop = false;
  int m_initialized = 0;  // +0x348
};
