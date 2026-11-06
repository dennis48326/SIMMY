// @class ESleep
// PC replacement for base/e_ngcsleep.cpp (an OSAlarm releasing a semaphore).
#pragma once

struct ESleep : public ESemaphore {
  ESleep();
  ~ESleep();
  void _tCallback(OSAlarm* alarm, OSContext* context);
  void Sleep(unsigned int milliseconds);
};
