// Port of game/pregamestate.cpp (PreGameState, PreGameCasPlayer1State), the CreateASimBaseState of
// game/basestates.cpp, the CAS target's background call (game/thesims.cpp) and ESimsApp's start lot
// (game/appmain.cpp). Still stubbed: player 2 (CAS, roommate), sounds and the save file flush.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern int g_loadedGameSlot;   // DAT_80476214 (game/memcardscreens.cpp)
extern int g_loadedGameSlot2;  // DAT_80476218

namespace {
// PC development aid: the CAS Apt commands that end Create-a-Sim (CASTarget::HandleNav -> GotoGame,
// then the memory card screens saving the new game) are not hooked up yet. With SIMMY_CAS_ACCEPT=
// <frames> set, that many frames after the CAS UI comes up the family is taken as done and saved
// (MEM_result 1, then the memory card screens' "ingame"); the wardrobe's CAS uses
// SIMMY_CAS_ACCEPT_WARDROBE instead.
int s_autoAcceptFrames = -1;  // reset for each CAS (CreateASimBaseState::Reset)
bool DebugAutoAccept() {
  int& s_frames = s_autoAcceptFrames;
  static const char* s_env = std::getenv("SIMMY_CAS_ACCEPT");
  // The wardrobe's CAS (CAS_mode 1) has its own: SIMMY_CAS_ACCEPT_WARDROBE (none: the pad decides).
  static const char* s_wardrobeEnv = std::getenv("SIMMY_CAS_ACCEPT_WARDROBE");
  const char* env = UIDB::UIDBGetInt(const_cast<char*>("CAS_mode")) == 1 ? s_wardrobeEnv : s_env;
  if (!env || !_gd.m_casTarget) {
    s_frames = -1;
    return false;
  }
  if (s_frames < 0) s_frames = std::atoi(env);
  if (s_frames > 0 && --s_frames == 0) {
    std::printf("[cas] SIMMY_CAS_ACCEPT: CAS taken as done\n");
    UIDB::UIDBSetInt(const_cast<char*>("MEM_result"), 1);
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("ingame"));
    return true;
  }
  return false;
}

int s_startLot = 0;           // ESimsApp::s_startLot
bool s_casTargetSkipUI = false;  // s_casTargetSkipUI
}  // namespace

// 80003FDC
RET_UNKNOWN ESimsApp::GetStartLot() { return s_startLot; }
// 80003FE4
void ESimsApp::SetStartLot(int lot) { s_startLot = lot; }

// 80014E60
// Makes the CAS target (its scene and sims) on the background thread.
void BGExec_CASTargetInit() {
  if (!_gd.m_casTarget) _gd.m_casTarget = new CASTarget(s_casTargetSkipUI);  // GC: 0x1638 bytes
  BG::s_bBGComplete = 1;
}

// 80014EB4
void BGCall_CASTargetInit(bool skipUI) {
  BG::s_bBGComplete = 0;
  s_casTargetSkipUI = skipUI;
  _pBackground->ExecuteRoutine(BGExec_CASTargetInit);
}

// 80014BCC
// GC: GameData::LoadDefaultNeighborhood on the background thread (the neighbourhood, its families and
// lots for a new game).
// PC: on the calling thread (GC: BGExec_LoadDefaultNeighborhood on the background thread).
void BGCall_LoadDefaultNeighborhood() {
  BG::s_bBGComplete = 0;
  GameData::LoadDefaultNeighborhood();
  BG::s_bBGComplete = 1;
}

// ---------------------------------------------------------------------------------------------------
// PreGameState

// 8009526C
PreGameState::PreGameState(int id) : StateMachineState(id) {}

// 8009427C
void PreGameState::Reset() {
  SIMMY_STUB("PreGameState::Reset: GameData state 3, stage timing", 0x8009427C);
  UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 0);
  m_stage = 0;
  m_loading = 1;
  m_calledCas2 = 0;
  m_twoPlayersIn = 0;
  m_done = 0;
  m_needCas = 1;
}

// 800942F4
// The staged start: lot 9, and for a new game the default neighbourhood (background). Then player 1
// creates the family (CAS) behind the loading screen; back from it, the game starts once the loading
// screen is up (or back to the menu when CAS was left).
void PreGameState::Update(float) {
  auto* owner = static_cast<TheSimsStateMachine*>(m_owner);
  if (m_loading) {
    const int stage = m_stage;
    if (stage == 0) {
      _gd.m_fieldE4 = 1;
      _app.SetStartLot(9);
      _globals.m_usedSimInteractor[0] = _globals.m_usedSimInteractor[1] = 0;  // DAT_80475E84/85: direct control
      // PC dev switch: SIMMY_TWO_PLAYER=1 is the front end's two-player choice (MMUTarget, players
      // screen: GameData::SetTwoPlayerMode), so the GC flow below runs: player 1's CAS, player 2's
      // roommate (InsertPlayer2Roommate) and player 2's CAS.
      if (simmy::dev_two_player() && !_gd.m_fieldEC) {
        _gd.SetTwoPlayerMode(true);
        std::printf("[coop] SIMMY_TWO_PLAYER: two-player game (two CAS)\n");
      }
      if (!_gd.m_field84) {
        // Continuing a story: player 1 has made the family already, so no CAS.
        FamilyImpl* f = !_gd.m_fieldE8 && Globs_pNeighborhood ? Globs_pNeighborhood->GetFamily(1) : nullptr;
        if (f && f->GetHasP1DoneCAS()) {
          m_needCas = 0;
          _gd.m_fieldE4 = 0;
        }
      } else {
        g_loadedGameSlot = 1;
        g_loadedGameSlot2 = _gd.m_fieldEC != 0;
        if (_gd.m_fieldF0) SIMMY_STUB("PreGameState::Update: EGlobal::SetAuthorModeCheats", 0x800942F4);
        BGCall_LoadDefaultNeighborhood();
      }
      ++m_stage;
    } else if (stage == 1) {
      if (BG::s_bBGComplete) ++m_stage;
    } else {
      if (_gd.m_fieldE8) OwnerSetNextState(TheSimsStateMachine::kPreGameFreeplay, 0.0f);
      m_loading = 0;
    }
    return;
  }
  if (m_done) {
    if (!LoadingScreenStateMachine::LoadInProgress()) {
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
      return;
    }
    // GC: DAT_80476230 (the house) = -1 first; PlayerEnterHouse sets it.
    gbDrawUI = 0;
    gbUpdateUI = 0;
    GameData::UseConfigOptions();
    _gd.PlayerEnterHouse();
    GameData::GotoLiveMode();
    OwnerSetNextState(TheSimsStateMachine::kInLevelStory, 0.0f);
    return;
  }
  auto family1 = [] { return Globs_pNeighborhood ? Globs_pNeighborhood->GetFamily(1) : nullptr; };
  // Player 2 makes their sim over in CAS (behind the loading screen): the roommate moved in for them.
  auto callCas2 = [&] {
    gbDrawUI = 0;
    gbUpdateUI = 0;
    LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{1});
    if (FamilyImpl* f = family1()) f->m_flags |= FamilyImpl::kP2DoneCAS;
    if (Globs_pSound) Globs_pSound->SetGameMode(cSoundPlayer::kLoading);
    OwnerCallState(TheSimsStateMachine::kPreGameCasPlayer2);
    m_calledCas2 = 1;
  };
  if (!m_needCas && !m_twoPlayersIn) {
    // Continuing a story with two players: player 2 gets a roommate (and makes them over) first.
    if (_gd.m_fieldEC && InsertPlayer2Roommate()) {
      UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 1);
      SoundFadeOutAndSetMode(cSoundPlayer::kLoading);
      callCas2();
      m_twoPlayersIn = 1;
      return;
    }
    m_done = 1;
    return;
  }
  const int ret = owner->m_returnVal;
  if (ret == 0) {
    if (!_gd.m_fieldEC) {
      gbDrawUI = 0;
      gbUpdateUI = 0;
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{1});
      // Player 1 makes family 1 (moving in on the way into the game).
      if (FamilyImpl* f = family1()) {
        f->m_flags |= FamilyImpl::kP1DoneCAS;
        f->SetHouseNumber(-1);
      }
      _pActualAudio->StopMusic();
      SoundFadeOutAndSetMode(cSoundPlayer::kLoading);
    } else {
      // Two players: player 1's CAS first (PlayerNumber 0), then player 2's (PlayerNumber 1).
      const int playerNumber = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
      if (playerNumber != 0) {
        if (playerNumber != 1 || !InsertPlayer2Roommate()) return;
        callCas2();
        return;
      }
      gbDrawUI = 0;
      gbUpdateUI = 0;
      if (FamilyImpl* f = family1()) {
        f->m_flags |= FamilyImpl::kP1DoneCAS;
        f->SetHouseNumber(-1);
      }
      SoundFadeOutAndSetMode(cSoundPlayer::kLoading);
    }
    OwnerCallState(TheSimsStateMachine::kPreGameCasPlayer1);
    return;
  }
  owner->m_returnVal = 0;
  if (ret == 1) {
    // CAS was left: back to the main menu.
    if (m_calledCas2) {
      // Player 2 backed out: the roommate goes home.
      if (Globs_pNeighborhood) Globs_pNeighborhood->RemovePlayerRoommate();
      m_calledCas2 = 0;
      UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 0);
      if (!m_twoPlayersIn) return;
    }
    OwnerReturnFromState(1);
    return;
  }
  if (_gd.m_fieldEC && !m_calledCas2) {
    UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 1);
    return;
  }
  m_done = 1;
}

// 800948D4
// A new two-player game whose family has no second member: family 2's first member moves in as player
// 2's roommate (true then). Either way the neighbourhood's variable 0x13 is player 2's neighbour.
bool PreGameState::InsertPlayer2Roommate() {
  if (!g_loadedGameSlot) return false;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (!hood) return false;
  const bool inserted = _gd.m_players[1].GetPrimaryNeighbor() == nullptr;
  if (inserted) {
    FamilyImpl* f = hood->GetFamily(2);
    int* guid = f ? f->GetIndexedMember(0) : nullptr;
    if (Neighbor* n = guid ? hood->FindNeighborByGUID(*guid) : nullptr) hood->AddAsPlayerRoommate(n);
  }
  if (Neighbor* n = _gd.m_players[1].GetPrimaryNeighbor())
    hood->SetNeighborhoodVar(0x13, static_cast<short>(n->m_id));
  g_loadedGameSlot2 = 1;
  return inserted;
}

// ---------------------------------------------------------------------------------------------------
// PreGameCasPlayer2State

// 80095324
PreGameCasPlayer2State::PreGameCasPlayer2State(int id) : CreateASimBaseState(id) {}

// 80094AF0
// The UI listens to player 2's pad (or to any pad when player 2 has none). GC: also sets a CAS-UI flag
// (DAT_8046E698, game/targets_s2c.cpp), not ported.
void PreGameCasPlayer2State::Startup() {
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer)) v->m_field3AC = v->GetPlayersController(1) ? -1 : 1;
}

// 80094B54
void PreGameCasPlayer2State::Shutdown() {
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer)) v->m_field3AC = -1;
}

// 80094B68
// Player 2's neighbour gets default values and the CAS loads its sim data.
void PreGameCasPlayer2State::Reset() {
  CreateASimBaseState::Reset();
  if (Neighbor* n = _gd.m_players[1].GetPrimaryNeighbor()) {
    n->InitDefaultValues();
    UIDB::UIDBSetInt(const_cast<char*>("CAS_neighborID"), n->m_id);
    _gd.LoadCreateASim(n->m_id, 0);
  }
}

// 80094BD4
void PreGameCasPlayer2State::Update(float dt) {
  CreateASimBaseState::Update(dt);
  if (!m_waitFadeIn && m_accepted) OwnerReturnFromState(2);
  else if (!m_waitFadeIn && m_canceled) OwnerReturnFromState(1);
}

// ---------------------------------------------------------------------------------------------------
// CreateASimBaseState

// 800898C4
CreateASimBaseState::CreateASimBaseState(int id) : StateMachineState(id) {}

// 800890AC
void CreateASimBaseState::Startup() {
  SIMMY_STUB("CreateASimBaseState::Startup: HDDThread::SetPriorityHigh", 0x800890AC);
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
}

// 80089140
// GC: also drops the neighbourhood custom characters' face textures.
void CreateASimBaseState::Shutdown() {
  ERShader::SetDefaultShaderState(0);
  SIMMY_STUB("CreateASimBaseState::Shutdown: custom character textures", 0x80089140);
  m_continue = 0;
}

// 800891B8
void CreateASimBaseState::Reset() {
  s_autoAcceptFrames = -1;  // PC dev aid (SIMMY_CAS_ACCEPT): each CAS (player 2's too) counts afresh
  m_accepted = 0;
  m_canceled = 0;
  OwnerResetReturnVal();
  m_stage = 0;
  m_waitFadeIn = 0;
  m_loadingStages = 1;
}

// 80089214
void CreateASimBaseState::GetDatasetName(char* out) { std::strcpy(out, "createasim"); }

// 80089240
// The staged load: the "createasim" dataset, the CAS target (background), its first screen, then the
// loading screen goes and the UI runs. Afterwards the UI's flow requests are taken.
void CreateASimBaseState::Update(float) {
  if (const int ret = OwnerGetReturnVal()) {
    OwnerResetReturnVal();
    if (ret == 2) m_accepted = 1;
    return;
  }
  if (m_loadingStages) {
    char name[32];
    switch (m_stage) {
      case 0:
        ERShader::SetDefaultShaderState(1);
        GetDatasetName(name);
        _datasetman.AddRefAsyncIfNotLoaded(name);
        ++m_stage;
        break;
      case 1:
        GetDatasetName(name);
        if (!_datasetman.GetRef(name)) break;
        _datasetman.DelRef(name, EResourceManager__DelRefMode{1});
        ++m_stage;
        break;
      case 2:
        if (!BG::s_bBGComplete) break;
        BGCall_CASTargetInit(false);
        ++m_stage;
        break;
      case 3:
        if (!BG::s_bBGComplete) break;
        _gd.m_casTarget->GotoInitialScreen();
        ++m_stage;
        SIMMY_STUB("CreateASimBaseState::Update: NghResFile::FlushAllHouseContents", 0x80089240);
        break;
      case 4:
        SIMMY_STUB("CreateASimBaseState::Update: NghResFile::FlushAllHouseContents", 0x80089240);
        ++m_stage;
        break;
      case 5:
      case 6:
      case 7:
        ++m_stage;
        break;
      default:
        m_loadingStages = 0;
        gbDrawUI = 1;
        gbUpdateUI = 1;
        LoadingScreenStateMachine::LoadFinishing();
        break;
    }
    return;
  }
  if (m_waitFadeIn) {
    if (!(LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn)) return;
    m_waitFadeIn = 0;
    if (_gd.m_field9C) {
      _gd.m_field9C = 0;
      _gd.m_fieldA8 = 1;
      SIMMY_STUB("CreateASimBaseState::Update: DAT_80485900", 0x80089240);
    }
    return;
  }
  if (_gd.m_field9C) {
    m_waitFadeIn = 1;
    _pCtrlMan->UnmapPlayer(1);
    if (auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer)) viewer->m_field3A8 = 0;
    m_canceled = 1;
  }
  auto* owner = static_cast<TheSimsStateMachine*>(m_owner);
  // PC dev aid: player 2's CAS is already in the "ingame" flow state (player 1's ended there), so the
  // auto-accept's flow request does not register; it is taken directly.
  static bool autoAccepted = false;
  if (DebugAutoAccept() && UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber")) == 1) autoAccepted = true;
  if (!_gd.m_casTarget) autoAccepted = false;
  if (!autoAccepted && !owner->IsFlowStateRequestPending()) return;
  const int request = autoAccepted ? FlowGotoTarget::kInGame : owner->GetFlowStateRequest();
  if (request == FlowGotoTarget::kInGame) {
    // Back from the memory card screens (the slot the new game is saved in): on to the game when the
    // save went through, else back to CAS.
    auto* m2m = static_cast<UIObjectBase*>(_gd.m_field54);
    const int result = UIDB::UIDBGetInt(const_cast<char*>("MEM_result"));
    if (result == 0) {
      owner->SetFlowStateCurrent();
      delete m2m;
      _gd.m_field54 = nullptr;
    } else if (result == 1) {
      _gd.m_casTarget->Shutdown();
      if (!_gd.m_casTarget->IsShutdown()) return;
      owner->SetFlowStateCurrent();
      delete _gd.m_casTarget;
      _gd.m_casTarget = nullptr;
      delete m2m;
      _gd.m_field54 = nullptr;
      if (!m_continue) {
        _gd.m_fieldBC = 4;  // DAT_80475C40 (the game mode: free play)
        SIMMY_STUB("CreateASimBaseState::Update: options block (DAT_80475F14 +0x0C) = 0", 0x80089240);
      } else {
        _gd.m_fieldBC = 3;
      }
      m_accepted = 1;
    }
    return;
  }
  if (request == FlowGotoTarget::kFreeplay) {
    // CAS left for the free play menu.
    _gd.m_casTarget->Shutdown();
    if (!_gd.m_casTarget->IsShutdown()) return;
    owner->SetFlowStateCurrent();
    delete _gd.m_casTarget;
    _gd.m_casTarget = nullptr;
    delete static_cast<UIObjectBase*>(_gd.m_field54);
    _gd.m_field54 = nullptr;
    m_canceled = 1;
    return;
  }
  if (request == FlowGotoTarget::kSlotSelect) {
    // The memory card screens save the new game.
    owner->SetFlowStateCurrent();
    if (_gd.m_field54) return;
    UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), 2);
    _gd.m_field54 = new M2MTarget;  // GC: 0x198 bytes
    return;
  }
  if (request == 7) {
    owner->SetFlowStateCurrent();
  } else if (request == FlowGotoTarget::kCAS) {
    owner->SetFlowStateCurrent();
    if (!_gd.m_casTarget) {
      _gd.m_casTarget = new CASTarget(false);
      _gd.m_casTarget->GotoInitialScreen();
    }
  } else if (request == 0x32 || request == 0x33) {
    OnBackOut();
  }
}

// 80089798
// Leaving CAS for the main menu: the target shuts down behind the loading screen, then goes.
void CreateASimBaseState::OnBackOut() {
  if (!_gd.m_casTarget->m_shuttingDown) _gd.m_casTarget->Shutdown();
  if (!_gd.m_casTarget->IsShutdown()) return;
  static_cast<TheSimsStateMachine*>(m_owner)->SetFlowStateCurrent();
  delete _gd.m_casTarget;
  _gd.m_casTarget = nullptr;
  SIMMY_STUB("CreateASimBaseState::OnBackOut: M2MTarget, front-end flash 0xba2f3a39", 0x80089798);
  SoundFadeOutAndSetMode(cSoundPlayer::kFrontEnd);
  m_canceled = 1;
}

// 800898C0
void CreateASimBaseState::Draw(ERC*) {}

// ---------------------------------------------------------------------------------------------------
// PreGameCasPlayer1State

// 800952D8
PreGameCasPlayer1State::PreGameCasPlayer1State(int id) : CreateASimBaseState(id) {}
// 800949E4
void PreGameCasPlayer1State::Startup() {}
// 800949E8
void PreGameCasPlayer1State::Shutdown() {}

// 800949EC
// GC: player 1's neighbour gets default values and the CAS loads its sim data.
void PreGameCasPlayer1State::Reset() {
  CreateASimBaseState::Reset();
  if (Neighbor* n = _gd.m_players[0].GetPrimaryNeighbor()) {
    n->InitDefaultValues();
    UIDB::UIDBSetInt(const_cast<char*>("CAS_neighborID"), n->m_id);
    _gd.LoadCreateASim(n->m_id, 0);
  }
}

// 80094A58
// Back to the pre-game with 2 when CAS was accepted, 1 when it was left.
void PreGameCasPlayer1State::Update(float dt) {
  CreateASimBaseState::Update(dt);
  if (!m_waitFadeIn && m_accepted) OwnerReturnFromState(2);
  else if (!m_waitFadeIn && m_canceled) OwnerReturnFromState(1);
}
