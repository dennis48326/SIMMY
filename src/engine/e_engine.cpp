// Port of engine/e_engine.cpp (partial): construction, init and shutdown, resource managers, frame timing.
// Still stubbed: memory-use reporting.
#include "simmy/types_gen.hpp"

#include <algorithm>
#include <cstdlib>
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <thread>

namespace {
// A subsystem's singleton is created by a file that is not ported yet.
int not_ready(const char* what, std::uint32_t addr) {
  SIMMY_STUB(what, addr);
  return 0;
}
}  // namespace

// 802E2994
EEngine::EEngine() {
  EGlobalManager::Register(this, 5);
  m_frameSemaphore.Create(2, 0);
  _retracetime = (_iVideoMode == 1) ? 0.02f : (1.0f / 60.0f);  // 0x3CA3D70A / 0x3C888889
  // TODO(math globals): the original also initialises _mId, _qId, _vZero and _vAxes here.
}

// 802E2AB8
EEngine::~EEngine() {
  if (!EGlobalManager::m_shutdownComplete) EGlobalManager::Shutdown();
  m_frameSemaphore.Destroy();
}

// 802E38E8
int EEngine::ManagedStartup() { return 1; }

// 802E2B3C
void EEngine::ManagedShutdown() { _pEngine = nullptr; }

// 802E2B48
void EEngine::Line() {}
// 802E3238
void EEngine::PrintBanner() {}
// 802E323C
void EEngine::PrintConfiguration() {}

static void compact_all_heaps() {
  for (EAHeap* h : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(),
                    TextureHeap(), AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(),
                    CUIHeap()})
    h->Compact();
  EAHeap::SetNextCheckPoint();
}

// 802E2B4C
int EEngine::Init() {
  compact_all_heaps();
  Line();
  PrintBanner();
  Line();
  PrintConfiguration();
  Line();
  if (!InitSubsystems()) return 0;
  Line();
  EThread::PrintAllThreads();
  Line();
  _pClockMan->Start(&_sysclock);
  _pClockMan->Start(&m_frameClock);
  m_frameSemaphore.Release();
  m_frameSemaphore.Release();
  m_initialized = 1;
  LinkFix();
  _enable_trace_log = 1;
  return 1;
}

// 802E3240
int EEngine::InitSubsystems() {
  if (!_pClockMan) return not_ready("EEngine::InitSubsystems: _pClockMan (engine/e_ngcclock.cpp)", 0x802E3240);
  _pClockMan->Init();
  InitFileSystem();
  if (!_pGfx) return not_ready("EEngine::InitSubsystems: _pGfx (engine/e_ngcgraphics.cpp)", 0x802E3240);
  if (!_pGfx->Init()) return 0;
  _scratchBuffMan.Init();
  if (!InitResourceManagers()) return 0;
  Effects::InitializeEffectsSystem();
  if (!_pCtrlMan) return not_ready("EEngine::InitSubsystems: _pCtrlMan (engine/e_ngcctrlmanager.cpp)", 0x802E3240);
  if (!_pCtrlMan->Init()) return 0;
  if (!_pMemoryCard) return not_ready("EEngine::InitSubsystems: _pMemoryCard (engine/e_ngcmemorycard.cpp)", 0x802E3240);
  if (_pMemoryCard->InitMemoryCard() != 1) return 0;
  if (_pActualAudio) _pActualAudio->InitAudio();
  PSysLibOpen();
  compact_all_heaps();
  EyeToyManager::Open(_iVideoMode == 0 ? 0x3c : 0x32, 1, 0);
  return 1;
}

// 802E340C
void EEngine::InitFileSystem() { _eorFileSys.Init(static_cast<int>(0x80000000u)); }  // device: default

// 802E2CAC
void EEngine::ShutdownThreads(bool waitForThreads) {
  // PC: the console only came here on a reset; on PC every quit does (F1 Quit, closing the window), and
  // it can come while the background thread loads a house or writes a memory card save. Let that
  // routine finish before the audio, effects and resource managers it uses are shut down (the original
  // order flushes the background thread last, in ShutdownResourceManagers).
  if (_pBackground) _pBackground->Flush();
  if (_pCtrlMan) _pCtrlMan->Shutdown();
  if (_pActualAudio) _pActualAudio->Shutdown();
  Effects::ShutdownEffectsSystem();
  ShutdownResourceManagers();
  if (waitForThreads) SignalShutdown();
}

// 802E37D0
void EEngine::Idle() {
  // GameCube: the main thread parks here forever. ENgcEngine overrides this on PC.
  for (;;) std::this_thread::yield();
}

// 802E38C0
void EEngine::Reboot() {}
// 802E38E4
void EEngine::SignalShutdown() {}

// 802E2E38
int EEngine::GetMinRetraces() { return m_predictiveSmoothing ? _d_retraces : 1; }
// 802E38B8
void EEngine::EnableFrameRatePredictiveSmoothing(bool enable) { m_predictiveSmoothing = enable; }
// 802E37B8
void EEngine::ClearCurrentLevel(ERLevel* level) {
  if (m_currentLevel == level) m_currentLevel = nullptr;
}
// 802E38C4
void EEngine::SetCurrentLevel(ERLevel* level) { m_currentLevel = level; }
// 802E38CC
ERLevel* EEngine::GetCurrentLevel() { return m_currentLevel; }
// 802E38D4
void EEngine::SetResetAllowed(bool allowed) { m_resetAllowed = allowed; }
// 802E38DC
bool EEngine::CanReset() { return m_resetAllowed != 0; }

// 802E3438
int EEngine::InitResourceManagers() {
  _pResLoader->Init();
  _pBackground->Init();
  _shaderman.Init(const_cast<char*>("shaders"));
  _textureman.Init(const_cast<char*>("textures"));
  _animman.Init(const_cast<char*>("animations"));
  _modelman.Init(const_cast<char*>("models"));
  if (_pAudiosampleman) _pAudiosampleman->Init(const_cast<char*>("samples"));
  _characterman.Init(const_cast<char*>("characters"));
  _datasetman.Init(const_cast<char*>("datasets"));
  _quickdataman.Init(const_cast<char*>("quickdatas"));
  _flashman.Init(const_cast<char*>("flashes"));
  _particleman.Init(const_cast<char*>("emitters"));
  g_effectsAttachmentMan.Init(const_cast<char*>("effectsAttachments"));
  g_effectsEmitterMan.Init(const_cast<char*>("effectsEmitters"));
  g_effectsSequencerMan.Init(const_cast<char*>("effectsSequencers"));
  _fontman.Init(const_cast<char*>("fonts"));
  _edithtreesetman.Init(const_cast<char*>("edithtreesets"));
  _audiostreamman.Init(const_cast<char*>("audiostreams"));
  _movieman.Init(const_cast<char*>("movies"));
  _binaryman.Init(const_cast<char*>("binaries"));
  g_ambientscoreman.Init(const_cast<char*>("ambientscores"));
  g_soundtrackdataman.Init(const_cast<char*>("soundtrackdatas"));
  g_soundeventman.Init(const_cast<char*>("soundevents"));
  _resPrefetch.Init();
  return 1;
}

// 802E3658
void EEngine::ShutdownResourceManagers() {
  EyeToyManager::Close();
  _resPrefetch.Shutdown();
  _flashman.Shutdown();
  _datasetman.Shutdown();
  if (_pAudiosampleman) _pAudiosampleman->Shutdown();
  _audiostreamman.Shutdown();
  _quickdataman.Shutdown();
  _binaryman.Shutdown();
  _movieman.Shutdown();
  _animman.Shutdown();
  _characterman.Shutdown();
  _modelman.Shutdown();
  _fontman.Shutdown();
  _shaderman.Shutdown();
  _textureman.Shutdown();
  g_ambientscoreman.Shutdown();
  g_soundtrackdataman.Shutdown();
  g_soundeventman.Shutdown();
  g_effectsAttachmentMan.Shutdown();
  g_effectsEmitterMan.Shutdown();
  g_effectsSequencerMan.Shutdown();
  if (_pBackground) _pBackground->Shutdown();
  _pResLoader->Shutdown();
}


// Frame timing globals (engine/e_engine.cpp's small data).
unsigned int _evenodd = 0;       // 804FF060 flips every frame
int _framecount = 0;             // 804FF064
unsigned int _retracecount = 0;  // 804FF068
float _dt = 1.0f / 60.0f;        // 804FF070 seconds of game time this frame
float _invdt = 60.0f;            // 804FF078
float _cputime = 0, _cputime_m1 = 0, _cputime_m2 = 0;  // 804FF080.. CPU seconds of the last frames
float _rendtime = 0, _rendtime_m1 = 0;                  // 804FF08C.. render seconds of the last frames
double _time = 0.0;              // 804FF098 seconds since boot
float _dtlim30 = 1.0f / 60.0f;   // 8050004C _dt capped at 1/30 s
float _dtlim20 = 1.0f / 60.0f;   // 80500050 _dt capped at 1/20 s
float _dtlim15 = 1.0f / 60.0f;   // 80500054 _dt capped at 1/15 s
static int first_3873 = 1;
static float dtHist_3874[2];     // the two previous frame times, for smoothing

// 802E2D5C
// How many display retraces this frame took (1..15), from the measured time or, with predictive
// smoothing, from the slower of the last two CPU and render times.
void EEngine::RetraceUpdate(float seconds) {
  float retraces;
  if (!m_predictiveSmoothing) {
    retraces = seconds / _retracetime;
  } else {
    const float cpu = std::max(_cputime_m1, _cputime);
    const float rend = std::max(_rendtime_m1, _rendtime);
    retraces = std::max(rend, cpu) / _retracetime;
  }
  _d_retraces = std::clamp(static_cast<int>(retraces + 0.5f), 1, 15);
  _retracecount += _d_retraces;
}

// 802E2E50
// While a movie streams from the disc, background resource loads wait. (The original also paused the
// EyeToyManager streams (PauseStreams/ResumeStreams); EyeToy is not ported.)
void EEngine::EnterMovieMode() { _pResLoader->PauseLoads(); }

// 802E2E8C
void EEngine::ExitMovieMode() { _pResLoader->UnPauseLoads(); }

// 802E2EC8
void EEngine::PreFrameUpdate() {
  m_frameSemaphore.Acquire(-1);  // at most two frames in flight (released by the renderer)
  simmy::det_frame_begin();      // PC: SIMMY_DET's virtual clock advances one retrace
  _pClockMan->Update();
  _pClockMan->Start(&m_cpuClock);
  _evenodd = _evenodd == 0;
  ++_framecount;
  const double frame = _pClockMan->GetSec(&m_frameClock);
  _pClockMan->Start(&m_frameClock);
  _time += frame;
  RetraceUpdate(static_cast<float>(frame));
  if (!m_predictiveSmoothing) {
    _dt = static_cast<float>(_d_retraces) * _retracetime;
  } else {
    // Average of this and the two previous frame times, each clamped to 1..15 retraces.
    double t = _retracetime;
    if (frame >= t) t = std::min(frame, static_cast<double>(static_cast<float>(t * 15.0)));
    if (first_3873) {
      first_3873 = 0;
      dtHist_3874[0] = dtHist_3874[1] = _retracetime;
    }
    const float sum = static_cast<float>(t + dtHist_3874[0]) + dtHist_3874[1];
    dtHist_3874[1] = dtHist_3874[0];
    _dt = sum / 3.0f;
    dtHist_3874[0] = static_cast<float>(t);
  }
  // PC development aid: SIMMY_FIXED_DT=1 makes every frame one retrace of game time (repeatable runs).
  // SIMMY_DET (pc_det.cpp) implies it.
  static const bool fixedDt = std::getenv("SIMMY_FIXED_DT") != nullptr || simmy::det_timing();
  if (fixedDt) _dt = _retracetime;
  _dtlim30 = std::min(_dt, 0.033333335f);
  _dtlim20 = std::min(_dt, 0.05f);
  _dtlim15 = std::min(_dt, 0.06666667f);
  _invdt = _dt != 0.0f ? 1.0f / _dt : 0.0f;
  _frag.Update();
  _pCtrlMan->Update();
  EyeToyManager::UpdateAllInstances(_dt);
  _pGfx->BeginFrame();
  _tweak.Update();
  _pBackground->Update();
  _pResLoader->Update();
}

// 802E317C
void EEngine::PostFrameUpdate() {
  _pGfx->DrawTiming();
  _pGfx->EndFrame();
  _cputime_m2 = _cputime_m1;
  _cputime_m1 = _cputime;
  _cputime = static_cast<float>(_pClockMan->GetSec(&m_cpuClock));
}

// 802E31FC
// Asks the renderer to release the frame semaphore once this frame is on screen.
void EEngine::FrameComplete() { _pSched->QueueCompletionEvent(m_frameSemaphore); }
