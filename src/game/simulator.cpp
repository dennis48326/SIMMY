// Port of game/simulator.cpp (cSimulatorImpl) and game/timeofday.cpp (GameTime): the game clock and the
// lot's tick. See simulator.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

cSimulatorImpl* Globs_pSimulator = nullptr;  // Globs::pSimulator
void PlacementTestTick(int ticks);            // placemanipulator.cpp

namespace {
int s_bSkipATimeAdvance = 0;  // bSkipATimeAdvance: the next tick's two seconds are skipped
int s_bStupidMode = 0;        // Globs::bStupidMode
int s_freeWillDefault = 1;    // DAT_804BBBC0 (global 0xF at Init)

// GlobalDispatch (game/thesims.cpp): the simulator's broadcasts (0xBD speed or pause changed, 0xD7 a game
// minute passed, 0xD8 a day, 0xD9 the time of day). PC: of their listeners only the house is ported
// (HouseImpl::DoCommand: 0xD8 the lot data, 0xD9 the rooms' lighting); SIMMY_SIM_TRACE=1 prints them.
void GlobalDispatch(int message, int param) {
  HouseDispatch(static_cast<short>(message), param);
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;
  if (trace && message != 0xd7 && Globs_pSimulator)
    std::printf("[sim] dispatch 0x%x at day %d %02d:%02d:%02d tick %d\n", message,
                Globs_pSimulator->m_globals[cSimulatorImpl::kDaysRunning], Globs_pSimulator->m_globals[0],
                Globs_pSimulator->m_globals[5], Globs_pSimulator->m_globals[6], Globs_pSimulator->m_ticks);
  else if (trace && message == 0xd7 && Globs_pSimulator && Globs_pSimulator->m_globals[5] % 10 == 0)
    std::printf("[sim] %02d:%02d tick %d\n", Globs_pSimulator->m_globals[0], Globs_pSimulator->m_globals[5],
                Globs_pSimulator->m_ticks);
}
}  // namespace

// 803AD480
void ExpenseReport::reset() {
  for (int& a : m_amounts) a = 0;
}

// ---------------------------------------------------------------------------------------------------
// GameTime

// 80147088
int GameTime::CountDaysInMonth(int, int) { return 30; }
// 80147090
int GameTime::CountDaysInYear(int) { return 360; }
// 80147098
int GameTime::SubtractDates(int y1, int m1, int d1, int y2, int m2, int d2) {
  return GetDaysSince1900(y1, m1, d1) - GetDaysSince1900(y2, m2, d2);
}
// 801470E4
int GameTime::GetDaysSince1900(int year, int month, int day) {
  if (year < 1900 || static_cast<unsigned int>(month - 1) > 11) return 0;
  int days = day - 1;
  for (int m = 1; m < month; ++m) days += CountDaysInMonth(year, m);
  return days + (year - 1900) * 360;
}

// ---------------------------------------------------------------------------------------------------
// cSimulator

// 801401E4
cSimulatorImpl* cSimulator::CreateInstance() { return new cSimulatorImpl; }
// 8014020C
void cSimulator::DestroyInstance(cSimulator* simulator) { delete simulator; }

// ---------------------------------------------------------------------------------------------------
// cSimulatorImpl

// 8014024C
cSimulatorImpl::cSimulatorImpl() {
  m_streaming = 0;
  for (ExpenseReport& r : m_previous) r.reset();
  m_today.reset();
  m_lotValue = 3500;
  m_boostLength = 0.0f;
  m_trueDt = -1.0f;
  m_globals[kPauseFlags] = 1;
  m_globals[kMode] = 0;
  m_msAccumulated = 0.0f;
  m_secondsAdjust = 0;
  m_archValue = 0;
  m_objectsValue = 0;
  m_speedMultiplier = 0.0f;
  m_simulating = 0;
  m_boostTime = 0.0f;
  m_passiveInfluence = nullptr;
}

// 8014044C
// GC: also the Commander base.
cSimulatorImpl::~cSimulatorImpl() {
  delete m_passiveInfluence;
  m_passiveInfluence = nullptr;
  if (Globs_pSimulator == this) Globs_pSimulator = nullptr;
}

// 80140324
// A new lot's clock: 1:00 am on the 15th of June 1997, paused, normal speed.
void cSimulatorImpl::Init() {
  for (short& g : m_globals) g = 0;
  m_lotValue = 3500;
  m_globals[kMonth] = 6;
  m_globals[kYear] = 1997;
  m_globals[kSpeed] = 2;
  m_globals[kDayOfMonth] = 15;
  m_globals[kMinute] = 0;
  m_archValue = 0;
  m_objectsValue = 0;
  m_stalled = 0;
  m_tickObject = 0;
  m_ticks = 0;
  m_funds = 0;
  m_pendingFunds = 0;
  m_globals[kHour] = 1;
  m_globals[kTimeOfDay] = static_cast<short>(ComputeTimeOfDay());
  m_globals[kSavedSpeed] = -1;
  m_globals[kPauseFlags] = 1;
  m_globals[0xC] = 4;
  m_globals[kLighting] = 0xff;
  m_globals[kFreeWillOverride] = -1;
  m_globals[0xF] = static_cast<short>(s_freeWillDefault);
  m_randSeed = GetSRandSeed();
  SetSpeed(0);
  m_globals[0x14] = 3;
  // GC: a new system each time without deleting the last (the map keeps reporting to the first one
  // made: PassiveInfluenceSystem::s_passiveInfluenceSystem). PC: the last one is deleted; the new one
  // becomes the one reported to only if none is.
  delete m_passiveInfluence;
  m_passiveInfluence = new PassiveInfluenceSystem;
  cXObjectImpl::overrideDialogReturnCode = 4;
}

// 801404CC
// A version word (1: eight amounts, 0: seven), then the amounts.
void ReconExpReport(ExpenseReport* report, ReconBuffer* b) {
  int version = 1;
  b->Recon32(&version, 1);
  const int n = version == 0 ? 7 : 8;
  for (int i = 0; i < n; ++i) {
    int v = report->m_amounts[i];
    b->ReconInt(&v, 1);
    report->m_amounts[i] = v;
  }
}

// 80140560
// The house's 'SIMI' resource: the globals (a loaded lot stays paused, its pause flag 2 kept), the tick
// count, funds, the random seeds, the lot's values and the expense history.
void cSimulatorImpl::DoStream(ReconBuffer* b, int version) {
  m_streaming = 1;
  const short pauseFlags = m_globals[kPauseFlags];
  const short freeWill = m_globals[kFreeWillOverride];
  const short g19 = m_globals[0x19];
  m_speedMultiplier = 0.0f;
  b->Recon16(m_globals, kCount);
  const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
  if (reading) {
    m_globals[kFreeWillOverride] = freeWill;
    m_globals[kPauseFlags] = static_cast<short>((pauseFlags & 2) | 1);
  }
  m_globals[0x19] = g19;
  SIMMY_STUB("cSimulatorImpl::DoStream: DAT_804761C8 (the tutorial's global 0x19 flag)", 0x80140560);
  m_globals[kLighting] = 0xff;
  m_globals[0xC] = 4;
  m_globals[0xF] = static_cast<short>(s_freeWillDefault);
  b->Recon32(&m_ticks, 1);
  if (reading) m_ticks = 1;
  short tick = static_cast<short>(m_tickObject);
  b->Recon16(&tick, 1);
  m_tickObject = tick;
  SetFunds(m_funds, 0);
  b->Recon32(&m_funds, 1);
  b->Recon32(reinterpret_cast<int*>(&m_randSeed), 1);
  int seed = static_cast<int>(GetSRandSeed());
  b->Recon32(&seed, 1);
  SetSRandSeed(static_cast<unsigned int>(seed));
  b->Recon32(&m_pendingFunds, 1);
  b->ReconInt(&m_lotValue, 1);
  b->ReconInt(&m_objectsValue, 1);
  b->ReconInt(&m_archValue, 1);
  m_globals[0x24] = static_cast<short>(m_archValue / 10000);
  m_globals[0x22] = static_cast<short>(m_objectsValue / 10000);
  m_globals[0x23] = static_cast<short>(m_archValue % 10000);
  m_globals[0x21] = static_cast<short>(m_objectsValue % 10000);
  ReconExpReport(&m_today, b);
  for (ExpenseReport& r : m_previous) ReconExpReport(&r, b);
  if (reading) {
    if (version < 0x40 || static_cast<unsigned int>(m_globals[kSavedSpeed] + 3) > 3) {
      m_globals[kSpeed] = 0;
      m_globals[kSavedSpeed] = 0;
    }
    SetSpeed(m_globals[kSavedSpeed]);
    m_globals[kDaysInMonth] = static_cast<short>(GameTime::CountDaysInMonth(m_globals[kYear], m_globals[kMonth]));
    m_globals[kFreeWill] = static_cast<short>(cXObject::GetFreeWill());
  }
  if (version > 0x5b && m_passiveInfluence) m_passiveInfluence->DoStream(b, version);
  m_streaming = 0;
  m_boostLength = 0.0f;
  m_boostTime = 0.0f;
  UpdateSpeed();
}

// 80140818
// Commands from the trees and the UI: 0xDA add funds, 0x87 skip to the next object, 0xCD re-init,
// 0xF4 / 0xE0 / 0x109 set globals 2 / 3 / 0x20.
int cSimulatorImpl::DoCommand(short command, int value) {
  switch (command) {
    case 0xda: SetFunds(GetFunds(0) + value, 0); return 1;
    case 0x87:
      if (m_tickObject == value) m_tickObject = value + 1;
      return 1;
    case 0xcd: Init(); return 1;
    case 0xf4: m_globals[2] = static_cast<short>(value); return 1;
    case 0xe0: m_globals[3] = static_cast<short>(value); return 1;
    case 0x109: m_globals[0x20] = static_cast<short>(value); return 1;
    default: return 0;
  }
}

// 80140924
void cSimulatorImpl::CommitPendingFunds() {
  if (m_pendingFunds == 0) return;
  m_funds += m_pendingFunds;
  if (m_funds > 999999) m_funds = 999999;
  m_pendingFunds = 0;
}

// 8014095C
// One tick: the objects (with the tick's own random sequence), then two game seconds (three or one
// while catching up with PostSecondOfDay), with the minute, hour, day, month and year rolling over.
int cSimulatorImpl::SimulateOneTick() {
  int message = 0;
  bool hourChanged = false;
  CommitPendingFunds();
  const unsigned int outerSeed = GetSRandSeed();
  SetSRandSeed(m_randSeed);
  const int finished = TickAllObjects();
  m_randSeed = GetSRandSeed();
  SetSRandSeed(outerSeed);
  if (finished) {
    ++m_ticks;
    if (!s_bSkipATimeAdvance) {
      short seconds = m_globals[kSecond];
      m_globals[kSecond] = static_cast<short>(seconds + 2);
      if (m_secondsAdjust > 0) {
        m_globals[kSecond] = static_cast<short>(seconds + 3);
        --m_secondsAdjust;
      } else if (m_secondsAdjust < 0) {
        m_globals[kSecond] = static_cast<short>(seconds + 1);
        ++m_secondsAdjust;
      }
    } else {
      s_bSkipATimeAdvance = 0;
    }
    if (m_globals[kSecond] > 59) {
      m_globals[kSecond] = static_cast<short>(m_globals[kSecond] - 60);
      message = 0xd7;
      const short minute = m_globals[kMinute];
      m_globals[kMinute] = static_cast<short>(minute + 1);
      if (m_globals[kMinute] > 59) {
        m_globals[kMinute] = static_cast<short>(minute - 59);
        hourChanged = true;
        const short hour = m_globals[kHour];
        m_globals[kHour] = static_cast<short>(hour + 1);
        if (m_globals[kHour] > 23) {
          m_globals[kHour] = static_cast<short>(hour - 23);
          ++m_globals[kDaysRunning];
          ++m_globals[kDayOfMonth];
          message = 0xd8;
          if (Globs_pObjectModule) Globs_pObjectModule->DayChanged();
          for (int i = 4; i > 0; --i) m_previous[i] = m_previous[i - 1];
          m_previous[0] = m_today;
          m_today.reset();
          if (m_globals[kDayOfMonth] > GameTime::CountDaysInMonth(m_globals[kYear], m_globals[kMonth])) {
            m_globals[kDayOfMonth] = 1;
            if (++m_globals[kMonth] > 12) {
              m_globals[kMonth] = 1;
              ++m_globals[kYear];
            }
            m_globals[kDaysInMonth] = static_cast<short>(GameTime::CountDaysInMonth(m_globals[kYear], m_globals[kMonth]));
          }
        }
      }
    }
  }
  if (m_passiveInfluence) m_passiveInfluence->CheckObjects(1);
  SIMMY_STUB("cSimulatorImpl::SimulateOneTick: ObjectModule::PostSim", 0x8014095C);
  // PC development aid: SIMMY_FIRE_TEST=<tick>,<object id>[,burn] starts a fire at that object (as TryBurn's
  // StartFireAtObjectLoc does) when the simulator reaches the tick; with ",burn" the burn primitive
  // (0x09, operands 0 / 1: its own tile, even in use) runs on it instead, from player 1's sim's tree.
  static const char* fireTest = std::getenv("SIMMY_FIRE_TEST");
  if (fireTest && m_ticks == std::atoi(fireTest) && std::strchr(fireTest, ',') && Globs_pObjectModule) {
    const int id = std::atoi(std::strchr(fireTest, ',') + 1);
    cXObject* o = Globs_pObjectModule->GetObjectFromID(id);
    ObjSelector* fire = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(0x24c95f99) : nullptr;
    cXPerson* runner = _globals.GetSelectedPerson(0);
    cXObjectImpl* impl = runner && runner->AsObject() ? runner->AsObject()->GetObjectImplementation() : nullptr;
    if (std::strstr(fireTest, ",burn") && impl) {
      StackElem frame;
      frame.m_stackObject = static_cast<short>(id);
      const unsigned char params[8] = {0, 1, 0, 0, 0, 0, 0, 0};
      std::printf("[sim] fire test at tick %d: burn primitive -> %d\n", m_ticks, impl->TryBurn(&frame, params));
    } else {
      std::printf("[sim] fire test at tick %d: %s\n", m_ticks, StartFireAtObjectLoc(o, fire) ? "started" : "refused");
    }
  }
  PlacementTestTick(m_ticks);  // PC development aid: SIMMY_PLACE_TEST (placemanipulator.cpp)
  if (message) GlobalDispatch(message, 0);
  if (hourChanged) {
    const int before = GetTimeOfDay();
    const int now = ComputeTimeOfDay();
    if (now != before) {
      m_globals[kTimeOfDay] = static_cast<short>(now);
      GlobalDispatch(0xd9, 0);
    }
  }
  return finished;
}

// 80140D28
int cSimulatorImpl::ComputeTimeOfDay() {
  const short hour = m_globals[kHour];
  if (hour < 6) return 2;
  if (hour < 7) return 3;
  if (hour < 18) return 0;
  if (hour > 18) return 2;
  return 1;
}

// 80140D70
int cSimulatorImpl::HourTimeOfDayLastChanged() {
  switch (m_globals[kTimeOfDay]) {
    case 0: return 7;
    case 1: return 18;
    case 2: return 19;
    case 3: return 6;
    default: return -1;
  }
}

// 80140DC8
int cSimulatorImpl::HourTimeOfDayNextChanges() {
  switch (m_globals[kTimeOfDay]) {
    case 0: return 18;
    case 1: return 19;
    case 2: return 6;
    case 3: return 7;
    default: return -1;
  }
}

// 80140E20
// Each object in id order: one waiting for a reset (sim flag 4) is reset when its tree has no error;
// an enabled one (flag 1; while paused only with flag 2) counts down its idle ticks or runs its tree.
// A tree that stops (an error) ends the tick there; the next one carries on from that object.
// 1: every object ran (and the simulator is not paused).
int cSimulatorImpl::TickAllObjects() {
  int paused = 0;
  if (IsPausedAtAll() || (GetMode() != 1 && m_globals[0xA] != 0)) paused = 1;
  ObjectModuleImpl* module = Globs_pObjectModule;
  const int count = module ? module->GetNumObjects() : 0;
  if (m_tickObject == 0) {
    m_tickObject = 1;
    m_speedMultiplier = 0.0f;
  }
  for (; m_tickObject <= count; ++m_tickObject) {
    cXObject* object = module->GetObjectFromID(m_tickObject);
    cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
    if (!impl) continue;
    if (module->GetSimFlag(m_tickObject, 4)) {
      if (impl->TreeSimError() == 0) {
        module->SetSimFlag(m_tickObject, 4, false);
      } else {
        impl->HandleError();
        return s_bStupidMode ? 1 : 0;
      }
    }
    if (!module->GetSimFlag(m_tickObject, 1) || (paused && !module->GetSimFlag(m_tickObject, 2))) continue;
    const int idle = module->GetIdleStatus(m_tickObject);
    if (idle >= 1) {
      module->SetIdleStatus(m_tickObject, idle - 1);
      continue;
    }
    if (!impl->Simulate(m_ticks)) return s_bStupidMode ? 1 : 0;
    if (impl->TreeSimError() != 0) {
      impl->HandleError();
      return s_bStupidMode ? 1 : 0;
    }
  }
  m_tickObject = 0;
  // GC: in ObjectModuleImpl::PostSim; the objects killed this tick go.
  module->KillQueueFlush();
  UpdateSpeed();
  return s_bStupidMode ? 1 : !paused;
}

// 801410EC
void cSimulatorImpl::SetSpeed(SimSpeed speed) {
  m_globals[kSpeed] = static_cast<short>(static_cast<int>(speed));
  UpdateSpeed();
  m_msAccumulated = 0.0f;
  GlobalDispatch(0xbd, 0);
}

// 80141134
int cSimulatorImpl::GetSpeed() { return m_globals[kSpeed]; }

// 8014113C
void cSimulatorImpl::UpdateSpeed() {
  float mult = 0.0f;
  if (!IsPausedAtAll() && !IsStopped()) {
    switch (m_globals[kSpeed]) {
      case -1: mult = 0.5f; break;
      case -2: mult = 5.0f; break;
      case -3: mult = 30.0f; break;
      default: mult = 1.0f; break;
    }
  }
  m_speedMultiplier = mult;
}

// 80141208
void cSimulatorImpl::Pause() {
  if (!IsPaused()) GlobalDispatch(0xbd, 0);
  m_speedMultiplier = 0.0f;
  m_globals[kPauseFlags] = static_cast<short>(m_globals[kPauseFlags] | 1);
}

// 80141274
void cSimulatorImpl::Resume() {
  if (IsPaused()) {
    GlobalDispatch(0xbd, 0);
    m_msAccumulated = 0.0f;
  }
  m_globals[kPauseFlags] = static_cast<short>(m_globals[kPauseFlags] & ~1);
  UpdateSpeed();
}

// 801412E8
int cSimulatorImpl::IsPaused() { return IsPausedAtAll(); }
// 8014131C
int cSimulatorImpl::IsStalled() { return m_stalled; }
// 80141324
int cSimulatorImpl::IsPausedAtAll() { return m_streaming != 0 || m_globals[kPauseFlags] != 0; }

// 80141350
void cSimulatorImpl::SetCurrentHour(int hour) {
  m_globals[kHour] = static_cast<short>(hour);
  m_globals[kMinute] = 0;
  GlobalDispatch(0xd7, 0);
  const int before = GetTimeOfDay();
  const int now = ComputeTimeOfDay();
  if (now != before) {
    m_globals[kTimeOfDay] = static_cast<short>(now);
    GlobalDispatch(0xd9, 0);
  }
}

// 801413DC
int cSimulatorImpl::GetFunds(cSimulator__tFundsPlayerNumber) { return m_funds + m_pendingFunds; }

// 801413EC
// The new balance takes effect at the next tick (pending); the globals 0xD/0xE show it in tens of
// thousands. A rise tells every person's wants (NotifyAllPersons): kEventFamilyFundsOfX and
// kEventFamilyFundsPlat, the count the new balance in hundreds.
void cSimulatorImpl::SetFunds(int funds, cSimulator__tFundsPlayerNumber) {
  const int balance = m_funds, pending = m_pendingFunds;
  if (funds == balance + pending) return;
  const int delta = funds - balance;
  m_pendingFunds = delta;
  if (balance + delta > 999999) {
    m_pendingFunds = 999999 - balance;
    funds = 999999;
  }
  m_globals[kFundsHigh] = static_cast<short>(funds / 10000);
  m_globals[kFundsLow] = static_cast<short>(funds % 10000);
  if (delta - pending > 0 && Globs_pNeighborhood) {
    WantFear__Notification n;
    n.m_target = WantFearEvent::kGenericParam;
    n.m_count = static_cast<short>(funds / 100);
    n.m_event = WantFearEvent::kFamilyFundsOfX;
    Globs_pNeighborhood->NotifyAllPersons(n, nullptr);
    n.m_event = WantFearEvent::kFamilyFundsPlat;
    n.m_count = static_cast<short>(funds / 100);
    Globs_pNeighborhood->NotifyAllPersons(n, nullptr);
  }
}

// 8014150C
void cSimulatorImpl::Spend(ExpenseType type, int amount, cSimulator__tFundsPlayerNumber player) {
  SetFunds(m_funds + m_pendingFunds - amount, player);
  const int t = type;
  if (t >= 0 && t < 8) m_today.m_amounts[t] += amount;
}

// 8014157C
void cSimulatorImpl::ClearHistory() {
  m_today.reset();
  for (ExpenseReport& r : m_previous) r.reset();
  m_globals[kDaysRunning] = 0;
}

// 801415DC
int cSimulatorImpl::GetDaysRunning() { return m_globals[kDaysRunning]; }

// 801415E4
void cSimulatorImpl::SetGlobal(short index, short value) {
  if (index >= 0 && index < kCount) m_globals[index] = value;
}

// 801415F4
void cSimulatorImpl::RestoreTrueDt() {
  _dt = m_trueDt;
  m_trueDt = -1.0f;
}

// 80141614
// Once a frame: the frame's time (at most 1/15 s) times the speed is added up (at most 2.52 s ahead), and
// a tick runs per 42 ms of it. _dt becomes the simulated frame time (RestoreTrueDt puts it back).
// A speed boost (global 0x2F, seconds) counts down meanwhile.
void cSimulatorImpl::Simulate() {
  m_simulating = 1;
  UpdateSpeed();
  float ms = _dt * 1000.0f;
  if (ms >= 66.67f) ms = 66.67f;
  if (!IsPausedAtAll()) {
    const float speed = static_cast<float>(GetSpeedMultiplier());
    m_msAccumulated = ms * speed + m_msAccumulated;
    const float over = m_msAccumulated - 2520.0f;
    if (over > 0.0f) {
      m_msAccumulated = 2520.0f;
      ms = ms - over / speed;
    }
  }
  m_trueDt = _dt;
  _dt = ms * 0.001f;
  if (!IsPausedAtAll()) {
    if (m_globals[0x25] < 1) {
      m_boostLength = 0.0f;
      m_boostTime = 0.0f;
    } else {
      if (m_boostTime == 0.0f) m_boostLength = static_cast<float>(m_globals[0x25]);
      m_boostTime += _dt;
      short left = static_cast<short>(static_cast<int>(m_boostLength - m_boostTime));
      if (left < 0) left = 0;
      m_globals[0x25] = left;
    }
    while (m_msAccumulated >= 42.0f) {
      m_msAccumulated -= 42.0f;
      m_stalled = SimulateOneTick() ^ 1;
      if (m_globals[kStepOnce] != 0) {
        m_globals[kStepOnce] = 0;
        break;
      }
      if (IsPausedAtAll()) m_msAccumulated = 0.0f;
    }
  }
  m_simulating = 0;
  UpdateSpeed();
  // PC development aid: SIMMY_SIM_TRACE=1 prints the clock every 120 frames.
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;
  static int frames = 0;
  if (trace && ++frames % 120 == 0)
    std::printf("[sim] frame %d: %02d:%02d:%02d day %d, tick %d (object %d), paused %d, speed x%.1f, stalled %d\n",
                frames, m_globals[kHour], m_globals[kMinute], m_globals[kSecond], m_globals[kDayOfMonth], m_ticks,
                m_tickObject, IsPausedAtAll(), m_speedMultiplier, m_stalled);
  if (trace && frames % 120 == 0) TraceSelectedPerson(0);
}

// 80141838
void cSimulatorImpl::GetTodaysExpenses(ExpenseReport* out) { *out = m_today; }
// 80141894
void cSimulatorImpl::GetPreviousExpenses(int day, ExpenseReport* out) {
  if (day >= 0 && day < 5) *out = m_previous[day];
}
// 801418F8
// The last three days (today and the two before it) added up.
void cSimulatorImpl::GetExpensesHistory(ExpenseReport* out) {
  const ExpenseReport* days[3] = {&m_previous[0], &m_previous[1], &m_today};
  for (int i = 0; i < 8; ++i) {
    int sum = 0;
    for (const ExpenseReport* d : days) sum += d->m_amounts[i];
    out->m_amounts[i] = sum;
  }
}

// 80141964
// Globals 0x2E / 0x2F are the players' cheats (EGlobal::GetPlayerCheats; PC: none).
int cSimulatorImpl::GetGlobal(short index) {
  if (index == 0x2e || index == 0x2f) m_globals[index] = 0;
  return index >= 0 && index < kCount ? m_globals[index] : 0;
}

// 801419D4
// The global a tree may write in place (not 3, 0x20, 0x2E, 0x2F).
int cSimulatorImpl::GetGlobalRef(short index, short** ref) {
  if (index < 0 || index >= kCount || index == 3 || index == 0x20 || index == 0x2e || index == 0x2f) return 0;
  *ref = &m_globals[index];
  return 1;
}

// 80141A2C
int cSimulatorImpl::GetLightingIntensity() { return static_cast<unsigned short>(m_globals[kLighting]) & 0x1ff; }
// 80141A38
int cSimulatorImpl::GetStrobeOn() { return static_cast<unsigned short>(m_globals[kLighting]) >> 9 & 1; }
// 80141A44
void cSimulatorImpl::SetGbaStatus(short v) { m_globals[0x2B] = v; }
// 80141A4C
void cSimulatorImpl::SetGbaMoney(short v) { m_globals[0x2C] = v; }
// 80141A54
void cSimulatorImpl::SetGbaNumMinigames(short v) { m_globals[0x2D] = v; }
// 80141A5C
void cSimulatorImpl::PostSecondOfDay(int second) {
  m_secondsAdjust = second - (m_globals[kSecond] + m_globals[kMinute] * 60 + m_globals[kHour] * 3600);
}
// 80141A84
double cSimulatorImpl::GetSpeedMultiplier() { return m_speedMultiplier; }
// 80141A8C
int cSimulatorImpl::IsStopped() { return m_simulating == 0 && m_tickObject != 0; }
// 80141AB8
int cSimulatorImpl::GetMode() { return m_globals[kMode]; }
// 80141AC0
void cSimulatorImpl::SetMode(cSimulator__Mode mode) { m_globals[kMode] = static_cast<short>(static_cast<int>(mode)); }
// 80141AC8
int cSimulatorImpl::GetTicks() { return m_ticks; }
// 80141AD0
int cSimulatorImpl::GetTimeOfDay() { return GetGlobal(kTimeOfDay); }
// 80141B08
void cSimulatorImpl::SetTimeOfDay(TimeOfDay time) { m_globals[kTimeOfDay] = static_cast<short>(static_cast<int>(time)); }
// 80141B10
int cSimulatorImpl::GetLotValue() { return m_lotValue; }
// 80141B18
void cSimulatorImpl::SetLotValue(int v) { m_lotValue = v; }
// 80141B20
int cSimulatorImpl::GetArchValue() { return m_archValue; }
// 80141B28
void cSimulatorImpl::SetArchValue(int v) {
  m_archValue = v;
  m_globals[0x24] = static_cast<short>(v / 10000);
  m_globals[0x23] = static_cast<short>(v % 10000);
}
// 80141B58
int cSimulatorImpl::GetObjectsValue() { return m_objectsValue; }
// 80141B60
void cSimulatorImpl::SetObjectsValue(int v) {
  m_objectsValue = v;
  m_globals[0x22] = static_cast<short>(v / 10000);
  m_globals[0x21] = static_cast<short>(v % 10000);
}
// 80141B90
SimLoopProbe* cSimulatorImpl::GetProbe() { return m_probe; }
// 80141B98
void cSimulatorImpl::SetProbe(SimLoopProbe* probe) { m_probe = probe; }
// 80141BA0
PassiveInfluenceSystem* cSimulatorImpl::GetPassiveInfluenceSystem() { return m_passiveInfluence; }

// ---------------------------------------------------------------------------------------------------
// PC accessors

int SimulatorHour() { return Globs_pSimulator ? Globs_pSimulator->GetGlobal(cSimulatorImpl::kHour) : 8; }
int SimulatorMinute() { return Globs_pSimulator ? Globs_pSimulator->GetGlobal(cSimulatorImpl::kMinute) : 0; }
int SimulatorSecond() { return Globs_pSimulator ? Globs_pSimulator->GetGlobal(cSimulatorImpl::kSecond) : 0; }
int SimulatorFunds() { return Globs_pSimulator ? Globs_pSimulator->GetFunds(0) : 0; }
int SimulatorPaused() { return Globs_pSimulator ? Globs_pSimulator->IsPausedAtAll() : 1; }
