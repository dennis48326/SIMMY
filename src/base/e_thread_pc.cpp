// PC replacements for the GameCube threading layer:
//   base/e_ngcthread.cpp     EThread    (OSThread -> std::thread)
//   base/e_ngcsemaphore.cpp  ESemaphore (OSSemaphore -> mutex + condition variable)
//   base/e_ngcsleep.cpp      ESleep     (OSAlarm -> sleep_for)
//   base/e_syncobject.cpp    ESyncObject
// Behavior kept from the original: threads are created suspended and run on Start(); every
// EThread sits in a global list (GetThreadObject, PrintAllThreads); the thread entry calls Main().
#include "simmy/types_gen.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>

namespace {
std::recursive_mutex g_listMutex;
EThread* g_threadList = nullptr;  // oldest first (was _threadList)
EThread* g_threadTail = nullptr;  // newest (was DAT_80500038)
std::atomic<int> g_nextThreadId{1};
thread_local int t_threadId = 0;

void link(EThread* t) {
  std::lock_guard lock(g_listMutex);
  t->m_prev = g_threadTail;
  t->m_next = nullptr;
  if (g_threadTail) g_threadTail->m_next = t;
  else g_threadList = t;
  g_threadTail = t;
}

void unlink(EThread* t) {
  std::lock_guard lock(g_listMutex);
  if (t->m_prev) t->m_prev->m_next = t->m_next;
  else if (g_threadList == t) g_threadList = t->m_next;
  if (t->m_next) t->m_next->m_prev = t->m_prev;
  else if (g_threadTail == t) g_threadTail = t->m_prev;
  t->m_prev = t->m_next = nullptr;
}
}  // namespace

// ---- EThread -----------------------------------------------------------------------------------

// 802D80A0
EThread::EThread() = default;

// 802D8114
EThread::EThread(int priority, int stackSize, void* stack) { Create(priority, stackSize, stack); }

// 802D8158
EThread::~EThread() {
  if (m_threadId) Destroy();
}

// 802D81B8
void EThread::DeallocateStack() { m_stackSize = 0; }

// 802E174C
int EThread::SetupStack(int stackSize, void* stack) {
  m_stackSize = stackSize;
  return 1;
}

// 802D8210
int EThread::Create(int priority, int stackSize, void* stack) {
  if (!SetupStack(stackSize, stack)) return 0;
  m_priority = priority;
  m_threadId = g_nextThreadId.fetch_add(1);
  m_resumed = false;
  link(this);
  // Created suspended, like OSCreateThread: the body waits for Start().
  m_host = std::thread([this] {
    t_threadId = m_threadId;
    {
      std::unique_lock lock(m_startMutex);
      m_startCv.wait(lock, [this] { return m_resumed; });
    }
    ThreadEntryPoint(this);
  });
  return 1;
}

// 802D82D8
int EThread::ThreadEntryPoint(void* self) {
  static_cast<EThread*>(self)->Main();
  return 0;
}

// 802D8310
void EThread::Attach(int threadId) {
  DeallocateStack();
  m_threadId = threadId;
  link(this);
}

// 802E1714
void EThread::AttachToCallingThread() { Attach(GetCurrentThreadId()); }

// 802D83BC
void EThread::Destroy() {
  unlink(this);
  if (m_host.joinable()) {
    if (IsCallingThread()) {
      m_host.detach();  // a thread destroying itself: let it finish on its own
    } else {
      // The original cancelled the OS thread outright. A host thread cannot be killed safely, so
      // release it if it never started and otherwise detach.
      {
        std::lock_guard lock(m_startMutex);
        m_resumed = true;
      }
      m_startCv.notify_all();
      m_host.detach();
    }
  }
  m_threadId = 0;
}

// 802D8464
void EThread::Start() {
  {
    std::lock_guard lock(m_startMutex);
    m_resumed = true;
  }
  m_startCv.notify_all();
}

// 802D8488
void EThread::Stop() {
  // OSSuspendThread has no safe host equivalent; nothing in the boot path needs it.
  std::fprintf(stderr, "[thread] Stop() on %s ignored on PC\n", m_name);
}

// 802D84B4
void EThread::SetPriority(int priority) { m_priority = priority; }

// 802D84D8
int EThread::GetPriority() { return m_priority; }

// 802D84FC
bool EThread::IsCallingThread() { return m_threadId != 0 && m_threadId == GetCurrentThreadId(); }

// 802D8538
EThread* EThread::GetThreadObject(int threadId) {
  std::lock_guard lock(g_listMutex);
  for (EThread* t = g_threadList; t; t = t->m_next)
    if (t->m_threadId == threadId) return t;
  return nullptr;
}

// 802D8568
EThread* EThread::GetCallingThreadObject() { return GetThreadObject(GetCurrentThreadId()); }

// 802D8690
int EThread::GetCurrentThreadId() {
  if (!t_threadId) t_threadId = g_nextThreadId.fetch_add(1);  // threads EA did not create (main)
  return t_threadId;
}

// 802D858C
void* EThread::GetStack() { return nullptr; }

// 802D8594
int EThread::GetStackSize() { return m_stackSize; }

// 802D859C
bool EThread::IsStackPtr(void* p) { return false; }

// 802D85CC
EThread* EThread::GetThreadFromStackPtr(void* p) { return nullptr; }

// 802D8610
int EThread::IsValidMemoryBlock(void* p, int size) { return p != nullptr; }

// 802D867C
void EThread::SetThreadName(char* name) { m_name = name; }

// 802D8684
const char* EThread::GetThreadName() { return m_name; }

// 802D868C
void EThread::Main() {}

// 802E17F0
void EThread::ValidateStack() {}

// 802E17F4
void EThread::ValidateAllStacks() {}

// 802E17F8
int EThread::GetStackUsage() { return 0; }

// 802E1800
void EThread::PrintAllThreads() {
  std::lock_guard lock(g_listMutex);
  for (EThread* t = g_threadList; t; t = t->m_next)
    std::fprintf(stderr, "[thread] id %d  prio %2d  %s\n", t->m_threadId, t->m_priority, t->m_name);
}

// ---- ESyncObject -------------------------------------------------------------------------------

// 802D5850
ESyncObject::ESyncObject() = default;

// 802D5864
ESyncObject::~ESyncObject() = default;

// 802D5898
void ESyncObject::Release(unsigned int count, unsigned int* previous) {
  for (unsigned int i = 0; i < count; ++i) Release();
}

// ---- ESemaphore --------------------------------------------------------------------------------

// 802D7758
ESemaphore::ESemaphore() = default;

// 802D77A0
ESemaphore::ESemaphore(int maxCount, int initialCount) { Create(maxCount, initialCount); }

// 802D7800
ESemaphore::~ESemaphore() {
  if (m_created) Destroy();
}

// 802D7854
ESemaphore* ESemaphore::GetObject(int) { return nullptr; }

// 802D785C
int ESemaphore::IsCreated() { return m_created ? 1 : 0; }

// 802D7874
int ESemaphore::Create(int maxCount, int initialCount) {
  if (initialCount == -1) initialCount = maxCount;
  std::lock_guard lock(m_mutex);
  m_count = initialCount;
  m_maxCount = maxCount;
  m_created = true;
  return 1;
}

// 802D78C8
void ESemaphore::Destroy() { m_created = false; }

// 802D78D4
int ESemaphore::Acquire(unsigned int timeout) {
  std::unique_lock lock(m_mutex);
  if (timeout == 0xFFFFFFFFu) {
    m_cv.wait(lock, [this] { return m_count > 0; });
  } else if (m_count <= 0) {
    return 0;  // OSTryWaitSemaphore failed
  }
  --m_count;
  return 1;
}

// 802D792C
int ESemaphore::Release() {
  {
    std::lock_guard lock(m_mutex);
    if (m_count >= m_maxCount) return 1;
    ++m_count;
  }
  m_cv.notify_one();
  return 1;
}

// 802D798C
void ESemaphore::iAcquire() { Acquire(0); }

// 802D79C4
void ESemaphore::iRelease() { Release(); }

// 802D79F8
int ESemaphore::GetCurrentCount() {
  std::lock_guard lock(m_mutex);
  return m_count;
}

// 802D7A00
int ESemaphore::GetMaxCount() { return m_maxCount; }

// 802D7A08
ESemaphore& ESemaphore::operator++() {
  Acquire(0xFFFFFFFFu);
  return *this;
}
// 802D7A50
ESemaphore& ESemaphore::operator++(int) { return ++*this; }
// 802D7A98
ESemaphore& ESemaphore::operator--() {
  Release();
  return *this;
}
// 802D7ADC
ESemaphore& ESemaphore::operator--(int) { return --*this; }

// ---- ESleep ------------------------------------------------------------------------------------

// 802D7B44
ESleep::ESleep() { Create(1, 0); }

// 802D7B9C
ESleep::~ESleep() = default;

// 802D7B20
void ESleep::_tCallback(OSAlarm*, OSContext*) { iRelease(); }

// 802D7BE0
void ESleep::Sleep(unsigned int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
