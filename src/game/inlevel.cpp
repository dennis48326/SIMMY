// Port of game/inlevel.cpp: InLevelBaseState, the front-end machine's state while a level runs. It
// updates and draws the game states and serves the UI's in-game flow requests: back to the game,
// the pause menu and its submenus, the slot select screen (saving), quitting to the main menu.
//
// The in-game target globals are GameData fields (DAT_80475BC4 + 4n = GameData +0x40 + 4n), each
// holding a UIObjectBase* (GC: deleted through the vtable at +0x80).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
// Deletes the target a GameData slot holds (GC: vtable +0x80 slot 0xC with 3).
void DeleteTarget(void*& slot) {
  delete static_cast<UIObjectBase*>(slot);
  slot = nullptr;
}
void DeleteHUD() {
  delete _gd.m_hudTarget;  // DAT_80475BC8
  _gd.m_hudTarget = nullptr;
}
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }

// GC: InteractorManager::GetPlayerInteractor(player) vtable 0x7C, ResetInputState: the players'
// interactors drop their held input when a menu takes over (the direct interactors are PC
// stand-ins outside this file; their input stops while H2DTarget's pause count is up).
void ResetPlayerInteractorsInput() {}
}  // namespace

// 80089E30
// The player's active interactor is its direct-control one (not the classic SimInteractor).
bool IsUsingDirectControlInteractor(int player) { return PlayerInteractorType(player) == 0; }

// 8008B4D0
// Each player's control mode for the next lot (EGlobal +0xB8: 1 the classic SimInteractor).
void InLevelBaseState::SavePlayerInteractorState() {
  _globals.m_usedSimInteractor[0] = PlayerInteractorType(0) == 1 ? 1 : 0;
  if (_gd.m_fieldEC) _globals.m_usedSimInteractor[1] = PlayerInteractorType(1) == 1 ? 1 : 0;  // DAT_80475C70
}

// 8008B5B0
InLevelBaseState::InLevelBaseState(int id) : StateMachineState(id) {}
// 8008B6F8
InLevelBaseState::~InLevelBaseState() = default;

// 80089E94
void InLevelBaseState::Reset() {
  m_pauseMenu = 0;
  m_paused = 0;
  _gd.m_field8C = 1;  // DAT_80475C10
}

// 80089EB0
// Leaving the state: the two players' MU slot states (MUStateMachine 80485900..0C) back to their
// defaults, the HUD goes and the HUD's enable flag (DAT_80475C10) is cleared until the next Reset.
void InLevelBaseState::Shutdown() {
  MUStateMachine::s_field900 = 0;
  MUStateMachine::s_field90C = 1;
  MUStateMachine::s_field904 = 0;
  MUStateMachine::s_field908 = 1;
  DeleteHUD();
  _gd.m_field8C = 0;  // DAT_80475C10
}

// 8008B758, 8008B77C, 8008B7A0: TheSimsStateMachine's (the owner's) flow request, used directly.

// Part of 80089F2C (flows 5 and 10, each with its own stage counter): leaving the lot. The HUD goes, the
// simulator pauses; once the dialog box is down the loading screen comes up (control modes kept), then
// the funds are committed and the lot saved into the neighbourhood and onto the memory card (save 5);
// then the objects go, the house's family is dropped and the live mode goes. True once done (the
// counter is back at 0).
static bool LeaveLotStage(int& stage) {
  switch (stage) {
    case 0:
      DeleteHUD();
      if (Globs_pSimulator) Globs_pSimulator->Pause();
      ++stage;
      [[fallthrough]];
    case 1:
      if (auto* g2d = static_cast<G2DTarget*>(_globals.m_g2dTarget))
        if (g2d->ServiceG2DShutdown()) return false;
      InLevelBaseState::SavePlayerInteractorState();
      gbDrawUI = 0;
      gbUpdateUI = 0;
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{1});
      ++stage;
      return false;
    case 2:
      if (!(LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn)) return false;
      if (Globs_pSimulator) Globs_pSimulator->CommitPendingFunds();
      Globs_pNeighborhood->SaveHouse(Globs_pNghResFile);
      MUStateMachine::MURequest(MUStateMachine::kRequestSaveExistingGame);
      ++stage;
      return false;
    case 3:
      if (MUStateMachine::MUPollForResult(MUStateMachine::kRequestSaveExistingGame) != 1) return false;
      ++stage;
      return false;
    case 4:
      if (Globs_pObjectModule) Globs_pObjectModule->KillAllObjects();
      if (Globs_pHouse) Globs_pHouse->SetFamilyToNull();  // vtable 0x94
      if (_app.m_gameStates) _app.m_gameStates->KillState();  // DAT_8046E680
      [[fallthrough]];
    default:
      stage = 0;
      return true;
  }
}

// 80089F2C
// The game states run; then the UI's in-game flow request:
//   8 "ingame": the pause menu and memory card screens go, the pause ends, the HUD is (re)made;
//   9 "HUD_transition": the HUD goes;
//   0x15 "Pause": the side panels and submenus go and the root pause menu (PAZTarget) comes for the
//        player who pressed Start (once the last one's screen is gone), pausing the game;
//   0x17 "SlotSelect": the memory card screen (M2MTarget: save from the pause menu);
//   1 "MainMenu": HandleExitToMainMenu;
//   0x1A: the wants and fears submenu (WXFTarget); 0x1D: progress (PRGTarget); 0x1E: career (CSPTarget);
//   0x23: the relationships submenu (R2LTarget); 0x24 / 0x25: the buy / build catalogue (PCTTarget);
//   0x27 "PAUSE_SUBMENU_GAME": the game options (O2TTarget), or back to them after saving;
//   0x29 "PAUSE_SAVE": the pause menu goes, on to "SlotSelect".
//   10 "Movie": the story's last lot was left (cXObjectImpl::TransitionToNewHouse 4): the HUD goes, the
//        simulator pauses, the dialog box closes, the loading screen comes up, the funds are committed,
//        the lot is saved (NeighborhoodImpl::SaveHouse, the memory card's save 5), the objects go and the
//        ending movie (state 15) follows;
//   0xB..0x14 the PDA screens: the PDA target with its sub-target (logged: the PDA is not in this
//        build's movies; only the EyeToy screen's "PDA_ITOY_CAPTURE_SAMPLE" names one).
//   0x2A / 0x2B "PAUSE_DEATH_TOMBSTONE_PLAYER1/2": the tombstone placed (PCTTarget mode 2).
//   5 "Wardrobe": a wardrobe's Change Clothes: the lot is left as for 10, then in-game Create-a-Sim
//        (state 14, InLevelCreateASimState) is called; it brings live mode back on the same lot.
//   0x19 "MixIngredients": food creation (INGTarget; "ingame" ends it).
// Not ported (logged): the options (0x16: s2c_options is not in this build's flashes.arc) and E2E.
void InLevelBaseState::Update(float) {
  _app.m_gameStates->Update();
  // PC development aid: SIMMY_TOMBSTONE_TEST=<frames> sends player 1's "PAUSE_DEATH_TOMBSTONE_PLAYER1" (what
  // a sim's death sends: objectsim.cpp's tombstone primitive) that many frames after the HUD is up.
  static int s_tombstoneTest = std::getenv("SIMMY_TOMBSTONE_TEST") ? std::atoi(std::getenv("SIMMY_TOMBSTONE_TEST")) : 0;
  if (s_tombstoneTest > 0 && _gd.m_hudTarget && --s_tombstoneTest == 0)
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("PAUSE_DEATH_TOMBSTONE_PLAYER1"));
  // PC development aid: SIMMY_ING_TEST=<frames>[,<level>[,<object>]] opens food creation for player 1 (what a
  // fridge's generic sim call 0x27 does: GameData::GotoXamMode mode 0xC) that many frames after the HUD is up,
  // with that refrigerator level (default 3) and food object (default none: no dish is configured).
  static int s_ingTest = std::getenv("SIMMY_ING_TEST") ? std::atoi(std::getenv("SIMMY_ING_TEST")) : 0;
  if (s_ingTest > 0 && _gd.m_hudTarget && --s_ingTest == 0) {
    int frames = 0, level = 3, object = 0;
    std::sscanf(std::getenv("SIMMY_ING_TEST"), "%d,%d,%d", &frames, &level, &object);
    _gd.GotoXamMode(0, 0xc, object, level);
  }
  // PC development aid: SIMMY_LEAVE_LOT_TEST=<frames>[,<mode>[,<house>]] leaves the lot as a taxi's / car's
  // "Leave Lot" does (generic sim call 0x16: cXObjectImpl::TransitionToNewHouse, changehouse.cpp) that many
  // frames after the HUD is up; mode 5 (default) to lot <house> (default 2), 4 the story's end (flow 10).
  static int s_leaveTest = std::getenv("SIMMY_LEAVE_LOT_TEST") ? std::atoi(std::getenv("SIMMY_LEAVE_LOT_TEST")) : 0;
  if (s_leaveTest > 0 && _gd.m_hudTarget && --s_leaveTest == 0) {
    int frames = 0, mode = 5, house = 2;
    std::sscanf(std::getenv("SIMMY_LEAVE_LOT_TEST"), "%d,%d,%d", &frames, &mode, &house);
    cXObjectImpl::TransitionToNewHouse(0, static_cast<short>(house), static_cast<short>(mode));
  }
  if (_gd.m_field94) {  // DAT_80475C18
    _gd.m_field94 = 0;
    OwnerReturnFromState(2);
    _app.m_gameState = 0;  // ESimsApp::SetGameState(0) (80005410)
  }
  auto* owner = static_cast<TheSimsStateMachine*>(m_owner);
  if (owner->m_returnVal) owner->m_returnVal = 0;
  if (!owner->IsFlowStateRequestPending()) return;
  const int request = owner->GetFlowStateRequest();
  switch (request) {
    case FlowGotoTarget::kInGame: {
      owner->SetFlowStateCurrent();
      DeleteTarget(_gd.m_field6C);  // DAT_80475BF0 INGTarget
      DeleteTarget(_gd.m_field70);  // DAT_80475BF4 PAZTarget
      if (m_pauseMenu) {
        m_pauseMenu = 0;
        if (m_paused) {
          if (_gd.m_hudTarget) _gd.m_hudTarget->ReleasePause(true);
          m_paused = 0;
        }
      }
      // GC: a player without an interactor gets back the one it had (m_directControl: 0 direct, 1 classic;
      // InteractorManager::ChangeActiveInteractor); the PC's interactors stay through the pause menu.
      DeleteTarget(_gd.m_field40);  // DAT_80475BC4 PDATarget
      DeleteTarget(_gd.m_field4C);  // DAT_80475BD0
      DeleteTarget(_gd.m_field54);  // DAT_80475BD8 M2MTarget
      if (!_gd.m_hudTarget) _gd.m_hudTarget = new HUDTarget;
      UIDB::UIDBSetInt(const_cast<char*>("DisableAnalogNavigation"), 1);
      // GC: H2DTarget +0x98 set: StartFire (no fires here).
      return;
    }
    case FlowGotoTarget::kHUDTransition:
      SavePlayerInteractorState();
      DeleteHUD();
      owner->SetFlowStateCurrent();
      return;
    case 0x15: {  // "Pause"
      DeleteTarget(_gd.m_field70);
      if (ScreenManager() && ScreenManager()->IsScreenManaged(0x2c)) return;
      owner->SetFlowStateCurrent();
      ResetPlayerInteractorsInput();
      DeleteTarget(_gd.m_field54);  // M2MTarget
      DeleteTarget(_gd.m_field50);  // DAT_80475BD4 OPTTarget
      DeleteTarget(_gd.m_field58);  // DAT_80475BDC E2ETarget
      DeleteTarget(_gd.m_field5C);  // DAT_80475BE0 WXFTarget
      DeleteTarget(_gd.m_field60);  // DAT_80475BE4 PRGTarget
      DeleteTarget(_gd.m_field64);  // DAT_80475BE8 O2TTarget
      DeleteTarget(_gd.m_field74);  // DAT_80475BF8 PCT/CSP/R2L
      const int player = _gd.m_h2dTarget ? _gd.m_h2dTarget->GetPausePlayer() : 0;
      if (!_gd.m_field70) _gd.m_field70 = static_cast<UIObjectBase*>(new PAZTarget(player));
      if (!m_pauseMenu) {
        m_pauseMenu = 1;
        if (!m_paused) {
          m_paused = 1;
          if (_gd.m_hudTarget) _gd.m_hudTarget->IncPause(true);
        }
        for (int p = 0; p < 2; ++p) m_directControl[p] = IsUsingDirectControlInteractor(p) ? 0 : 1;
      }
      break;
    }
    case FlowGotoTarget::kMainMenu:
      owner->SetFlowStateCurrent();
      HandleExitToMainMenu();
      break;
    case FlowGotoTarget::kSlotSelect:
      owner->SetFlowStateCurrent();
      if (!_gd.m_field54) _gd.m_field54 = static_cast<UIObjectBase*>(new M2MTarget);
      return;
    case 0x27: {  // "PAUSE_SUBMENU_GAME": the game options; back from the memory card screen, the save's end
      DeleteTarget(_gd.m_field74);
      owner->SetFlowStateCurrent();
      int player = 0;
      if (_gd.m_field70) player = static_cast<PAZBase*>(static_cast<UIObjectBase*>(_gd.m_field70))->GetPausePlayer();
      DeleteTarget(_gd.m_field70);
      auto* o2t = static_cast<O2TTarget*>(static_cast<UIObjectBase*>(_gd.m_field64));
      if (!o2t) {
        _gd.m_field64 = static_cast<UIObjectBase*>(new O2TTarget(0x27, player));
        return;
      }
      o2t->OnSaveGameComplete(UIDB::UIDBGetInt(const_cast<char*>("MEM_result")) == 1);
      DeleteTarget(_gd.m_field54);
      return;
    }
    case 0x1a: {  // "PAUSE_SUBMENU_WANTS_FEARS": the wants and fears submenu (WXFTarget)
      owner->SetFlowStateCurrent();
      DeleteTarget(_gd.m_field74);
      int player = 0;
      if (_gd.m_field70) player = static_cast<PAZBase*>(static_cast<UIObjectBase*>(_gd.m_field70))->GetPausePlayer();
      DeleteTarget(_gd.m_field70);
      _gd.m_field5C = static_cast<UIObjectBase*>(new WXFTarget(player));
      return;
    }
    case 0x1d: {  // the progress submenu (PRGTarget)
      owner->SetFlowStateCurrent();
      int player = 0;
      if (_gd.m_field70) player = static_cast<PAZBase*>(static_cast<UIObjectBase*>(_gd.m_field70))->GetPausePlayer();
      DeleteTarget(_gd.m_field70);
      _gd.m_field60 = static_cast<UIObjectBase*>(new PRGTarget(player));
      return;
    }
    case 0x1e: {  // "PAUSE_SUBMENU_CAREER": the career, skills and personality submenu (CSPTarget)
      owner->SetFlowStateCurrent();
      int player = 0;
      if (_gd.m_field70) player = static_cast<PAZBase*>(static_cast<UIObjectBase*>(_gd.m_field70))->GetPausePlayer();
      DeleteTarget(_gd.m_field70);
      DeleteTarget(_gd.m_field74);
      _gd.m_field74 = static_cast<UIObjectBase*>(new CSPTarget(CSPTarget__eTabs{0}, player));
      return;
    }
    case 0x24:    // "PAUSE_BUY": the buy catalogue (PCTTarget mode 0)
    case 0x25: {  // "PAUSE_BUILD": the build catalogue (mode 1)
      owner->SetFlowStateCurrent();
      int player = 0;
      if (_gd.m_field70) player = static_cast<PAZBase*>(static_cast<UIObjectBase*>(_gd.m_field70))->GetPausePlayer();
      DeleteTarget(_gd.m_field70);
      _gd.m_field74 = static_cast<UIObjectBase*>(new PCTTarget(PCTTarget__ePCTMode{request == 0x25 ? 1 : 0}, player));
      return;
    }
    case 0x19: {  // "MixIngredients": food creation (INGTarget), from a fridge's generic sim call 0x27
      owner->SetFlowStateCurrent();
      ResetPlayerInteractorsInput();
      DeleteTarget(_gd.m_field6C);
      _gd.m_field6C = static_cast<UIObjectBase*>(new INGTarget);  // GC: 0x468 bytes
      break;
    }
    case 0x2a:    // "PAUSE_DEATH_TOMBSTONE_PLAYER1": player 1's sim died: its tombstone is placed
    case 0x2b: {  // "PAUSE_DEATH_TOMBSTONE_PLAYER2"
      // The memory card, options, catalogue and pause screens go; the pause menu is made for the player
      // and the game paused as for "Pause" (the pause menu then goes again, as on GC), then the catalogue
      // in tombstone mode (PCTTarget mode 2: PCT_TabsReady -> enter_place_tombstone_mode).
      owner->SetFlowStateCurrent();
      const int player = request == 0x2b ? 1 : 0;
      DeleteTarget(_gd.m_field54);  // M2MTarget
      DeleteTarget(_gd.m_field50);  // OPTTarget
      DeleteTarget(_gd.m_field74);  // PCT/CSP/R2L
      DeleteTarget(_gd.m_field70);  // PAZTarget
      _gd.m_field70 = static_cast<UIObjectBase*>(new PAZTarget(player));
      if (!m_pauseMenu) {
        m_pauseMenu = 1;
        if (!m_paused) {
          m_paused = 1;
          if (_gd.m_hudTarget) _gd.m_hudTarget->IncPause(false);
        }
        for (int p = 0; p < 2; ++p) m_directControl[p] = IsUsingDirectControlInteractor(p) ? 0 : 1;
      }
      DeleteTarget(_gd.m_field70);
      _gd.m_field74 = static_cast<UIObjectBase*>(new PCTTarget(PCTTarget__ePCTMode{2}, player));
      std::printf("[inlevel] player %d's sim died: the tombstone catalogue\n", player + 1);
      return;
    }
    case 0x23: {  // the relationships submenu (R2LTarget)
      owner->SetFlowStateCurrent();
      int player = 0;
      if (_gd.m_field70) player = static_cast<PAZBase*>(static_cast<UIObjectBase*>(_gd.m_field70))->GetPausePlayer();
      DeleteTarget(_gd.m_field70);
      _gd.m_field74 = static_cast<UIObjectBase*>(new R2LTarget(player));
      return;
    }
    case FlowGotoTarget::kWardrobe: {  // "Wardrobe": in-game Create-a-Sim (state 14) once the lot is left
      static int goingToCasStage = 0;  // goingToCasStage_6159
      if (!LeaveLotStage(goingToCasStage)) break;
      owner->SetFlowStateCurrent();
      OwnerCallState(TheSimsStateMachine::kInLevelCreateASim);
      break;
    }
    case FlowGotoTarget::kMovie: {
      static int unloadStage = 0;  // unloadStage_6160
      if (!LeaveLotStage(unloadStage)) return;
      owner->SetFlowStateCurrent();
      OwnerSetNextState(TheSimsStateMachine::kEndgameMovie, 0.0f);
      return;
    }
    case 0xb: case 0xc: case 0xd: case 0xe: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: {
      // GC: the players' interactors drop their input (vtable 0x7C), the memory card screen goes, else
      // the PDA target (PDATarget, 0x178 bytes) is made and given the screen
      // (HandleSubTargetStateChange: FON / REL / INV / MSG / GOL / SKN / EYE / RGT sub-targets).
      owner->SetFlowStateCurrent();
      ResetPlayerInteractorsInput();
      if (_gd.m_field54) {
        DeleteTarget(_gd.m_field54);
        return;
      }
      SIMMY_STUB("InLevelBaseState::Update: the PDA (PDATarget and its sub-targets; no movie of this build uses it)",
                 0x801FC844);
      return;
    }
    case 0x29:  // "PAUSE_SAVE"
      owner->SetFlowStateCurrent();
      UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("SlotSelect"));
      DeleteTarget(_gd.m_field70);
      return;
    default: {
      static int logged = -1;
      if (logged != request) {
        logged = request;
        std::printf("[inlevel] flow request %d (%#x) not ported\n", request, request);
      }
      SIMMY_STUB("InLevelBaseState::Update: the PDA, CAS, change house, options and pause submenu flows",
                 0x80089F2C);
      return;
    }
  }
  UIDB::UIDBSetInt(const_cast<char*>("DisableAnalogNavigation"), 0);
}

// 8008B380
// The pause menu, the HUD and the memory card and game option screens go; ELiveMode then leaves
// for the main menu (DAT_80475C20).
void InLevelBaseState::HandleExitToMainMenu() {
  DeleteTarget(_gd.m_field70);
  DeleteHUD();
  DeleteTarget(_gd.m_field4C);
  DeleteTarget(_gd.m_field54);
  DeleteTarget(_gd.m_field64);
  _gd.m_field9C = 1;
}

// 8008B498
void InLevelBaseState::Draw(ERC* rc) { _app.m_gameStates->Draw(rc); }
