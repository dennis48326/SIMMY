// @class ENgcClockMan
// Clock manager (engine/e_ngcclock.cpp); OSGetTime is replaced by std::chrono::steady_clock.
#pragma once

struct ENgcClockMan {
  ENgcClockMan() = default;
  virtual ~ENgcClockMan() = default;
  virtual void Init();
  virtual void Update();
  virtual void Start(EClock* clock);
  void UpdateData(EClock* clock);
  virtual double GetSec(EClock* clock);
  virtual double GetSecDouble(EClock* clock);
};
