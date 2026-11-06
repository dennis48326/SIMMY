// @class cSimulator after ReconBuffer
// @class cSimulatorImpl
// @class cSimulator__Mode
// @class cSimulator__tFundsPlayerNumber
// @class SimSpeed
// @class ExpenseType
// @class ExpenseReport
// @class TimeOfDay
// @class GameTime
// The simulator (game/simulator.cpp, Globs::pSimulator): the game clock and the lot's tick. Every
// 42 ms of simulated time (scaled by the speed) it runs one tick: each object's behaviour tree
// (cXObjectImpl::Simulate / cXPersonImpl::Simulate, which also runs the people's motives), then two
// game seconds pass (a game minute is 30 ticks, 1.26 s at normal speed). It also keeps the family's
// funds and the day's expenses. Its state is the Edith globals: 0x30 shorts the behaviour trees read
// and write (scope 6), the clock among them.
#pragma once

struct cSimulator__Mode {
  SIMMY_ENUM_BODY(cSimulator__Mode)
};
struct cSimulator__tFundsPlayerNumber {
  SIMMY_ENUM_BODY(cSimulator__tFundsPlayerNumber)
};
// The speed (global 0x10): 0 normal, -1 half, -2 five times, -3 thirty times (UpdateSpeed).
struct SimSpeed {
  SIMMY_ENUM_BODY(SimSpeed)
};
struct ExpenseType {
  SIMMY_ENUM_BODY(ExpenseType)
};
// 0 day, 1 dusk (18:xx), 2 night, 3 dawn (6:xx) (ComputeTimeOfDay).
struct TimeOfDay {
  SIMMY_ENUM_BODY(TimeOfDay)
};
// A day's spending by ExpenseType (0x20 bytes).
struct ExpenseReport {
  void reset();  // 803AD480 (linkonce)
  int m_amounts[8] = {};
};

// The calendar: every month has 30 days, a year 360 (game/timeofday.cpp).
struct GameTime {
  static int CountDaysInMonth(int year, int month);  // 80147088
  static int CountDaysInYear(int year);  // 80147090
  static int SubtractDates(int, int, int, int, int, int);  // 80147098
  static int GetDaysSince1900(int year, int month, int day);  // 801470E4
};

// GC: an interface whose only own code is the factory.
struct cSimulator {
  using Mode = ::cSimulator__Mode;
  using tFundsPlayerNumber = ::cSimulator__tFundsPlayerNumber;
  static cSimulatorImpl* CreateInstance();  // 801401E4
  static void DestroyInstance(cSimulator* simulator);  // 8014020C
  virtual ~cSimulator() = default;
};

// The simulator (GC 0x180 bytes; +0x04 a Commander, unported).
struct cSimulatorImpl : public cSimulator {
  // The Edith globals (index into m_globals, GC +0x14 + 2 * index).
  enum Global : short {
    kHour = 0, kDayOfMonth = 1, kTimeOfDay = 4, kMinute = 5, kSecond = 6, kMonth = 7, kYear = 8,
    kStepOnce = 0xB,  // set: Simulate stops after the next tick
    kFundsLow = 0xD, kFundsHigh = 0xE, kSpeed = 0x10, kPauseFlags = 0x11, kSavedSpeed = 0x12,
    kMode = 0x13, kDaysRunning = 0x1C, kDaysInMonth = 0x1D, kFreeWill = 0x1E, kFreeWillOverride = 0x27,
    kLighting = 0x2A, kCount = 0x30
  };

  cSimulatorImpl();  // 8014024C
  ~cSimulatorImpl() override;  // 8014044C
  void Init();  // 80140324
  void DoStream(ReconBuffer* buffer, int version);  // 80140560
  int DoCommand(short command, int value);  // 80140818
  void CommitPendingFunds();  // 80140924
  int SimulateOneTick();  // 8014095C
  int ComputeTimeOfDay();  // 80140D28
  int HourTimeOfDayLastChanged();  // 80140D70
  int HourTimeOfDayNextChanges();  // 80140DC8
  int TickAllObjects();  // 80140E20
  void SetSpeed(SimSpeed speed);  // 801410EC
  int GetSpeed();  // 80141134
  void UpdateSpeed();  // 8014113C
  void Pause();  // 80141208
  void Resume();  // 80141274
  int IsPaused();  // 801412E8
  int IsStalled();  // 8014131C
  int IsPausedAtAll();  // 80141324
  void SetCurrentHour(int hour);  // 80141350
  int GetFunds(cSimulator__tFundsPlayerNumber player = 0);  // 801413DC
  void SetFunds(int funds, cSimulator__tFundsPlayerNumber player = 0);  // 801413EC
  void Spend(ExpenseType type, int amount, cSimulator__tFundsPlayerNumber player = 0);  // 8014150C
  void ClearHistory();  // 8014157C
  int GetDaysRunning();  // 801415DC
  void SetGlobal(short index, short value);  // 801415E4
  void RestoreTrueDt();  // 801415F4
  void Simulate();  // 80141614
  void GetTodaysExpenses(ExpenseReport* out);  // 80141838
  void GetPreviousExpenses(int day, ExpenseReport* out);  // 80141894
  void GetExpensesHistory(ExpenseReport* out);  // 801418F8
  int GetGlobal(short index);  // 80141964
  int GetGlobalRef(short index, short** ref);  // 801419D4
  int GetLightingIntensity();  // 80141A2C
  int GetStrobeOn();  // 80141A38
  void SetGbaStatus(short);  // 80141A44
  void SetGbaMoney(short);  // 80141A4C
  void SetGbaNumMinigames(short);  // 80141A54
  void PostSecondOfDay(int second);  // 80141A5C
  double GetSpeedMultiplier();  // 80141A84
  int IsStopped();  // 80141A8C
  int GetMode();  // 80141AB8
  void SetMode(cSimulator__Mode mode);  // 80141AC0
  int GetTicks();  // 80141AC8
  int GetTimeOfDay();  // 80141AD0
  void SetTimeOfDay(TimeOfDay time);  // 80141B08
  int GetLotValue();  // 80141B10
  void SetLotValue(int);  // 80141B18
  int GetArchValue();  // 80141B20
  void SetArchValue(int);  // 80141B28
  int GetObjectsValue();  // 80141B58
  void SetObjectsValue(int);  // 80141B60
  SimLoopProbe* GetProbe();  // 80141B90
  void SetProbe(SimLoopProbe* probe);  // 80141B98
  PassiveInfluenceSystem* GetPassiveInfluenceSystem();  // 80141BA0

  short m_globals[kCount] = {};          // +0x14
  int m_funds = 0;                       // +0x74
  int m_pendingFunds = 0;                // +0x78
  int m_ticks = 0;                       // +0x7C
  int m_secondsAdjust = 0;               // +0x80 seconds to catch up (+) or hold back (-), one per tick
  int m_lotValue = 3500;                 // +0x84
  int m_objectsValue = 0;                // +0x88
  int m_archValue = 0;                   // +0x8C
  unsigned int m_randSeed = 0;           // +0x90 the tick's own random sequence
  SimLoopProbe* m_probe = nullptr;       // +0x94
  int m_streaming = 0;                   // +0x98 (paused while DoStream runs)
  int m_stalled = 0;                     // +0x9C the last tick did not finish
  float m_msAccumulated = 0.0f;          // +0xA0 simulated ms not yet ticked
  float m_trueDt = -1.0f;                // +0xA4 _dt before Simulate replaced it
  float m_speedMultiplier = 0.0f;        // +0xA8
  float m_boostTime = 0.0f;              // +0xAC
  float m_boostLength = 0.0f;            // +0xB0
  ExpenseReport m_previous[5];           // +0xB4 the last five days
  ExpenseReport m_today;                 // +0x154
  PassiveInfluenceSystem* m_passiveInfluence = nullptr;  // +0x174 (not ported)
  int m_tickObject = 0;                  // +0x178 the object TickAllObjects is at (0: between ticks)
  int m_simulating = 0;                  // +0x17C
};

extern cSimulatorImpl* Globs_pSimulator;  // Globs::pSimulator

// PC accessors for the HUD and other readers (each is the GC call it stands for).
// The clock: GetGlobal(kHour / kMinute / kSecond) (HUDTarget::UpdateClock).
int SimulatorHour();
int SimulatorMinute();
int SimulatorSecond();
// The family's funds: GetFunds (vtable 0xA4).
int SimulatorFunds();
// 1 while the simulator is paused at all (IsPausedAtAll, vtable 0x74), or there is none.
int SimulatorPaused();
