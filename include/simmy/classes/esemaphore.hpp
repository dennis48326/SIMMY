// @class ESemaphore
// PC replacement for base/e_ngcsemaphore.cpp (was an OSSemaphore). Counting semaphore with a
// maximum count; Acquire(0xFFFFFFFF) blocks, any other timeout is a non-blocking try.
#pragma once
#include <condition_variable>
#include <mutex>

struct ESemaphore : public ESyncObject {
  ESemaphore();
  ESemaphore(int maxCount, int initialCount);
  virtual ~ESemaphore();
  ESemaphore* GetObject(int index);
  int IsCreated();
  int Create(int maxCount, int initialCount);
  void Destroy();
  virtual int Acquire(unsigned int timeout);
  virtual int Release();
  using ESyncObject::Release;
  void iAcquire();
  void iRelease();
  int GetCurrentCount();
  int GetMaxCount();
  ESemaphore& operator++();
  ESemaphore& operator++(int);
  ESemaphore& operator--();
  ESemaphore& operator--(int);

  bool m_created = false;   // was +0x4 (pointer to the embedded OSSemaphore)
  int m_maxCount = 0;       // was +0x8
  int m_count = 0;          // was +0xC (OSSemaphore count)
  std::mutex m_mutex;
  std::condition_variable m_cv;
};
