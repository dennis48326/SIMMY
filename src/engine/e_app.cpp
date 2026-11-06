// Port of engine/e_app.cpp: the application object and the app thread's main loop.
// Still stubbed: SetupPaths, GetArg and the Get* configuration queries, SetGameState/SetMenuOwner.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

EApp* _pApp = nullptr;  // 804FF058

namespace {
// Movie resources: EResourceManager::AddRef(&_movieman, id, 0, 0) and DelRef(&_movieman, movie, 1).
ERMovie* movie_acquire(unsigned int id) { return _movieman.AddRef(id, nullptr, 0); }
void movie_release(ERMovie* movie) { _movieman.DelRef(movie, EResourceManager__DelRefMode::Cache); }
}  // namespace

// 802E2180
EApp::EApp() { _pApp = this; }

// 802E21E0
EApp::~EApp() = default;

// 802E2290
void EApp::Main() {
  SetupPaths();
  if (_pEngine->Init()) {
    SystemInit();
    while (!m_quit) {
      SystemUpdate();
    }
    Shutdown();
    _pEngine->ShutdownThreads(true);
  }
}

// 802E2414
void EApp::CreateAndStartAppThread() {
  // GameCube: Create(0x18, GetAppStackSize(), 0) + Start() on a new thread. On PC the app runs on the
  // main thread; ENgcEngine::Idle calls RunOnCallingThread() once startup is complete.
  m_name = "Application";
  m_priority = 0x18;
  m_stackSize = GetAppStackSize();
}

// PC only: become the calling thread's EThread object and run Main().
void EApp::RunOnCallingThread() {
  Attach(GetCurrentThreadId());
  Main();
}

// 802E247C
void EApp::SystemInit() { Init(); }

// 802E24B0
void EApp::SetupForMovie() {}
// 802E24B4
void EApp::CleanupAfterMovie() {}

// 802E24B8
void EApp::SystemUpdate() {
  // The original first polled DVDGetDriveStatus() and skipped the frame while the disc was in an
  // error state (cover open, no disc...). There is no disc drive on PC.
  if (m_movieState == 2 && m_movieRequest != 2) {
    _pGfx->Flush();
    _pEngine->ExitMovieMode();
    if (m_movie) {
      m_movie->Stop();
      movie_release(m_movie);
      m_movie = nullptr;
    }
    CleanupAfterMovie();
  }
  if (m_movieRequest == 1) {
    _pActualAudio->StopMusic();
    SetupForMovie();
    m_movie = movie_acquire(m_movieId);
    if (!m_movie) {
      m_movieRequest = 0;
    } else {
      _pEngine->EnterMovieMode();
      m_movie->Start(m_movieX, m_movieY);
      m_movieRequest = 2;
    }
    m_movieId = 0;
  }
  m_movieState = m_movieRequest;
  _pEngine->PreFrameUpdate();
  if (m_movie) m_movie->Update();
  Update();
  _pEngine->PostFrameUpdate();
  if (m_movie && m_movie->IsFinished()) m_movieRequest = 0;
  EThread::ValidateAllStacks();
  simmy::platform_frame_tick();  // PC: window events, placeholder frame, pacing
}

// 802E26F0
void EApp::PlayMovie(unsigned int movieId, int x, int y) {
  m_movieId = movieId;
  m_movieX = x;
  m_movieY = y;
  m_movieRequest = 1;
}

// 802E2710
void EApp::StopMovie() { m_movieRequest = 0; }

// 802E271C
bool EApp::IsMovieStarting() { return m_movieRequest == 1; }

// 802E2734
bool EApp::IsMoviePlaying() { return m_movieRequest == 2; }

// 802E274C
void EApp::SetArgs(int argc, char** argv) {
  m_argString[0] = '\0';
  if (argc > 100) argc = 0;
  for (int i = 0; i < argc; ++i) {
    if (i) std::strncat(m_argString, " ", sizeof(m_argString) - std::strlen(m_argString) - 1);
    std::strncat(m_argString, argv[i], sizeof(m_argString) - std::strlen(m_argString) - 1);
    // The original also stripped double quotes from each argv entry in place.
    char* out = argv[i];
    for (const char* in = argv[i]; *in; ++in)
      if (*in != '"') *out++ = *in;
    *out = '\0';
  }
  m_argv = argv;
  m_argc = argc;
}

// 802E294C
const char* EApp::GetFullArgString() const { return m_argString; }
// 802E2954
int EApp::GetGameState() const { return m_gameState; }
// 802E2964
int EApp::GetMenuOwner() const { return m_menuOwner; }
// 802E2974
void EApp::ShowInitialDisplay() {}
// 802E2978
RET_UNKNOWN EApp::Init() { return 0; }
// 802E297C
void EApp::Update() {}
// 802E2980
void EApp::UpdateAfterHotSync() {}
// 802E2984
int EApp::GetAppStackSize() { return 0x18000; }
// 802E2990
void EApp::Shutdown() {}
