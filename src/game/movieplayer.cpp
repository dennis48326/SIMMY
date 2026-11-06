// Port of game/movieplayer.cpp: the story's ending movie (see movieplayer.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80058820
MoviePlayerEndgameMovieState::MoviePlayerEndgameMovieState(int id) : StateMachineState(id) {}

// 800585FC
// The player's cached sim models go, the resource loader is flushed and unreferenced resources
// freed, the game's sounds pause, every heap is compacted, the music volume goes to 1 and the
// localized ending movie plays.
void MoviePlayerEndgameMovieState::Reset() {
  ESim::DeletePlayerCachedModel();
  const unsigned int movie = static_cast<unsigned int>(GameData::GetLocalizedSimsEndingMovie());
  _pResLoader->FlushCommandQueue();
  _pResLoader->FreeUnreferencedResources();
  if (Globs_pSound) Globs_pSound->PauseSounds();
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
  EAHeap::SetNextCheckPoint();
  _pActualAudio->SetMusicVolume(1.0f);
  _app.PlayMovie(movie, -1, -1);
}

// 8005871C
// Once the movie is over: the sounds resume and the game goes on.
void MoviePlayerEndgameMovieState::Update(float) {
  if (!_app.IsMovieStarting() && !_app.IsMoviePlaying()) {
    if (Globs_pSound) Globs_pSound->ResumeSounds();
    GotoGame();
  }
}

// 80058778
// Into live mode (state 13) behind the loading screen, the HUD's flow ("HUD_").
void MoviePlayerEndgameMovieState::GotoGame() {
  // GC: DAT_80475F24 (livemode.cpp's file-local s_familyJustMovedIn) = 0 first; PC: not reachable from
  // here (livemode.cpp is shared); LiveModeInitState sets it again on entering.
  LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
  GameData::GotoLiveMode();
  OwnerSetNextState(TheSimsStateMachine::kInLevelStory, 0.0f);
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("HUD_"));
  SetFlowStateCurrent();
}

// 800587E8
void MoviePlayerEndgameMovieState::Draw(ERC*) {}

// 800587EC
void MoviePlayerEndgameMovieState::Shutdown() {
  _app.IsMoviePlaying();
  GameData::SetUglyGraphicsBackgroundColor();
}

// 80058858
void MoviePlayerEndgameMovieState::SetFlowStateCurrent() {
  static_cast<TheSimsStateMachine*>(m_owner)->SetFlowStateCurrent();
}
