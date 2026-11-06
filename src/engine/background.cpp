// Port of engine/background.cpp: a worker thread that runs queued routines in order.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <cstdlib>

static BackgroundImpl _background;
BackgroundImpl* _pBackground = nullptr;  // 8050003C

// 802E1BFC Background::Background (defaulted in the header)
// 802E1C10  Background::~Background (defaulted in the header)

// 802E1C44
BackgroundImpl::BackgroundImpl() { _pBackground = this; }

// 802E1CB0
BackgroundImpl::~BackgroundImpl() { TerminateThread(); }

// 802E1DE4
void BackgroundImpl::Init() {
  m_stop = false;
  m_thread = std::thread([this] { Main(); });
  m_initialized = 1;
}

// 802E1D24
void BackgroundImpl::Shutdown() {
  Flush();
  TerminateThread();
  m_initialized = 0;
  _pBackground = nullptr;
}

// 802E1D98
void BackgroundImpl::TerminateThread() {
  {
    std::lock_guard lock(m_mutex);
    m_stop = true;
  }
  m_cv.notify_all();
  if (m_thread.joinable()) m_thread.join();
}

// 802E2104
int BackgroundImpl::IsCallingThread() { return std::this_thread::get_id() == m_thread.get_id(); }

// 802E1E50
void BackgroundImpl::Update() {}

// PC test hook: SIMMY_TEST_QUIT_BG=N asks the game to quit (as the F1 overlay's Quit does) when the Nth
// background routine is queued, so the exit path is tested while a house load or a save is running.
static void TestQuitOnRoutine() {
  static const int at = [] {
    const char* e = std::getenv("SIMMY_TEST_QUIT_BG");
    return e ? std::atoi(e) : 0;
  }();
  static int count = 0;
  if (at > 0 && ++count == at) {
    std::fprintf(stderr, "[test] background routine %d queued: quitting\n", count);
    simmy::platform_request_quit();
  }
}

// 802E1E54
void BackgroundImpl::ExecuteRoutine(void (*fn)()) {
  {
    std::lock_guard lock(m_mutex);
    m_queue.emplace_back([fn] { fn(); });
  }
  m_cv.notify_one();
  TestQuitOnRoutine();
  // PC: with SIMMY_DET (pc_det.cpp) the caller waits for the routine, so that a load takes the same
  // number of game frames on every run (the frames the front end ran before the house decide the UI's
  // timing phase in it).
  if (simmy::det_timing()) Flush();
}

// 802E1EA8
void BackgroundImpl::ExecuteArgRoutine(void (*fn)(void*), void* arg) {
  {
    std::lock_guard lock(m_mutex);
    m_queue.emplace_back([fn, arg] { fn(arg); });
  }
  m_cv.notify_one();
  TestQuitOnRoutine();
  if (simmy::det_timing()) Flush();  // PC: SIMMY_DET (see ExecuteRoutine)
}

// 802E1F00
void BackgroundImpl::SendCommand(BackgroundCmd*) {}

// 802E1F28
void BackgroundImpl::Flush() {
  if (!m_thread.joinable() || IsCallingThread()) return;
  std::unique_lock lock(m_mutex);
  m_idleCv.wait(lock, [this] { return m_queue.empty() && !m_busy; });
}

// 802E1FD4
void BackgroundImpl::Main() {
  for (;;) {
    std::function<void()> job;
    {
      std::unique_lock lock(m_mutex);
      m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
      if (m_queue.empty()) return;  // stopping
      job = std::move(m_queue.front());
      m_queue.pop_front();
      m_busy = true;
    }
    job();
    {
      std::lock_guard lock(m_mutex);
      m_busy = false;
    }
    m_idleCv.notify_all();
  }
}
