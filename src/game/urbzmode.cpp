// Port of game/urbzmode.cpp: UrbzModeNewGameState, the development menu's new game. See urbzmode.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <chrono>
#include <thread>

extern int g_loadedGameSlot;   // DAT_80476214 (game/memcardscreens.cpp)
extern int g_loadedGameSlot2;  // DAT_80476218

// DAT_804761B0: build/buy is always allowed (PAZTarget's build-buy status); set by the development
// menu's new game outside author mode.
int g_devBuildBuyAlways = 0;

namespace {
int s_stageCounter = 0;  // s_stageCounter_6895
int s_oldLot = 0;        // s_oldLot_6894
}  // namespace

// 8009B6DC
UrbzModeNewGameState::UrbzModeNewGameState(int id) : StateMachineState(id) {}

// 8009B258
void UrbzModeNewGameState::Reset() {
  m_stage = 0;
  if (auto* options = static_cast<OptionsRecon*>(_globals.m_optionsRecon)) options->ResetToDefaults();  // DAT_80475F14
}

// 8009B288
// Stages: 0 the loading screen, audio and flags; 1 the default neighbourhood; 2 (5 frames on) the
// "createasim_dev" dataset; 3 it has loaded; 4 player 1's neighbour into a CAS target without UI
// (background); 5 that sim saved into the neighbourhood; 6 the CAS target gone; 7/8 five frames each,
// then the default UI settings and the house contents flushed; 9 family 1 saved homeless (it moves
// into the start lot), the neighbourhood saved, into the house and live mode; then state 13.
void UrbzModeNewGameState::Update(float) {
  int stage = m_stage;
  GameData::StageStartFrame();
  switch (stage) {
    case 0:
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
      _pActualAudio->StopMusic();  // vtable 0x48
      if (Globs_pSound) Globs_pSound->SetGameMode(cSoundPlayer::kLoading);
      g_loadedGameSlot = 1;
      g_loadedGameSlot2 = 0;
      _gd.SetTwoPlayerMode(false);
      if (_gd.m_fieldF0 == 0) g_devBuildBuyAlways = 1;  // DAT_80475C74 (author mode)
      else _globals.SetAuthorModeCheats();
      break;
    case 1:
      s_stageCounter = 5;
      GameData::LoadDefaultNeighborhood();
      break;
    case 2:
      if (--s_stageCounter >= 0) return;
      _datasetman.AddRefAsyncIfNotLoaded(const_cast<char*>("createasim_dev"));
      break;
    case 3:
      if (!_datasetman.GetRef(const_cast<char*>("createasim_dev"))) return;
      _datasetman.DelRef(const_cast<char*>("createasim_dev"), EResourceManager__DelRefMode{1});
      break;
    case 4: {
      if (FamilyImpl* f = Globs_pNeighborhood->GetFamily(1)) f->SetJustMovedIn(true);  // vtable 0x124 / 0x124
      Neighbor* n = _gd.m_players[0].GetPrimaryNeighbor();
      UIDB::UIDBSetInt(const_cast<char*>("CAS_neighborID"), n->m_id);
      s_oldLot = static_cast<int>(_app.GetStartLot());
      n = _gd.m_players[0].GetPrimaryNeighbor();
      const short id = static_cast<short>(n->m_id);
      UIDB::UIDBSetInt(const_cast<char*>("CAS_neighborID"), id);
      _gd.LoadCreateASim(id, 0);
      BGCall_CASTargetInit(true);
      break;
    }
    case 5:
      std::this_thread::sleep_for(std::chrono::milliseconds(16));  // GC: ESleep::Sleep(0x10)
      if (!BG::s_bBGComplete) return;
      _gd.m_casTarget->SaveCASDataToNeighborhood();
      _gd.m_casTarget->Shutdown();
      break;
    case 6:
      if (!_gd.m_casTarget->IsShutdown()) return;
      delete _gd.m_casTarget;  // GC: deleting destructor (vtable +0x0C, 3)
      _gd.m_casTarget = nullptr;
      _app.SetStartLot(s_oldLot);
      s_stageCounter = 5;
      break;
    case 7:
      if (s_stageCounter - 1 >= 0) {
        --s_stageCounter;
        return;
      }
      s_stageCounter = 5;
      break;
    case 8: {
      if (--s_stageCounter >= 0) return;
      EyeToyClient::LoadDefaultSettings();
      // GC passes &m_pLoadBuffers; loading the defaults does not read the data.
      unsigned int defaults[0x100] = {};
      SerializeUIGameData::SerializeUIData(0, defaults, SerializeUIGameData__eUIGameData_Direction{2}, true);
      SerializeUIGameData::SerializeUIData(1, defaults, SerializeUIGameData__eUIGameData_Direction{2}, true);
      Globs_pNghResFile->FlushAllHouseContents();
      break;
    }
    case 9: {
      FamilyImpl* f = Globs_pNeighborhood->GetFamily(1);  // vtable 0x124
      f->SetHouseNumber(-1);                               // vtable 0xB4
      f->SaveFamily(Globs_pNghResFile, Globs_iSaveFileVersion);  // vtable 0x6C
      Globs_pNeighborhood->GetImpl()->Save(Globs_pNghResFile, Globs_iSaveFileVersion);  // 0x194, 0x6C
      _gd.PlayerEnterHouse();
      // GC: author mode also clears DAT_80476234 (livemode.cpp's s_enteringHouse); no author mode on PC.
      if (_gd.m_fieldF0) SIMMY_STUB("UrbzModeNewGameState::Update: author mode DAT_80476234 = 0", 0x8009B288);
      GameData::GotoLiveMode();
      ++m_stage;
      OwnerSetNextState(TheSimsStateMachine::kInLevelStory, 0.0f);
      return;
    }
    case 10:
      OwnerSetNextState(TheSimsStateMachine::kInLevelStory, 0.0f);
      return;
    default:
      return;
  }
  m_stage = stage + 1;
}
