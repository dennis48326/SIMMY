// @class EMutex after ESemaphore
// A binary semaphore (base/e_mutex.cpp). Not recursive, as on the GameCube.
#pragma once

struct EMutex : public ESyncObject {
  EMutex();
  ~EMutex() override;
  virtual void Acquire(char* file, int line, unsigned int timeout);
  int Acquire(unsigned int timeout) override;
  int Release() override;
  virtual void Release(char* file, int line);
  using ESyncObject::Release;
  void iAcquire();
  void iRelease();
  EMutex& operator++();
  EMutex& operator++(int);
  EMutex& operator--();
  EMutex& operator--(int);

  ESemaphore m_sem;  // +0x04
};
