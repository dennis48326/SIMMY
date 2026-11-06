// Port of the house change (game/livemode.cpp: ELiveMode::ChangeHouse, LiveModeChangeHouseStateMachine and
// its states ChangeHouseDemolishHouse / SaveHouse / UnloadHouse / LoadHouse; game/objectsim.cpp:
// cXObjectImpl::TransitionToNewHouse): a sim leaves the lot (the taxi's / car's "Leave Lot", the
// move-a-tron's story unlocks) and the game goes on on another lot.
//
// GC flow: TransitionToNewHouse (generic sim call 0x16) sets the EORGlobals' flags (+0x450 "change house
// requested", +0x460 "going to another house", +0x464 the house); ELiveMode::Update's end then closes the HUD,
// saves the lot and the family (NeighborhoodImpl::SaveHouse, PlayerFamilyImpl::SaveFamily) and starts the
// loading screen (ELiveMode +0x24 leaving); ELiveMode::IsReadyForUpdate, leaving with +0x460 set, calls
// ChangeHouse: the 'SIMS' machine runs state 0x13, the change-house machine: demolish (wait for the loading
// screen, pause the simulator, the objects go), save (the memory card, MU request 7), unload (the house and
// the level's data), load (the house's stages, as LiveModeInitState). PC: the load is the live mode's own
// staged load (LiveModeInitState, which loads DAT_80476230's house): the unload leaves ELiveMode not
// initialised, and its next update calls the init state.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/house.hpp"

#include <cstdio>

// livemode.cpp (PC accessors for DAT_8047622C / DAT_80476230 and the live mode).
void LiveModeSetChangingHouse(int on);
extern int g_pcChangeHouseLoad;  // livemode.cpp
void LiveModeSetNextHouse(int house);
void LiveModeSetPlayerNeighbor(int player, int id);
ELiveMode* LiveModeForSound();

// 80104EC0
// The game leaves this lot (called through the cXObject interface: GC's first argument is the object,
// unused). `mode` 2: back to the main menu (HUDTarget::GotoMainMenu); 3: the house is saved and kept (EOR
// +0x450; the neighbourhood's variable 1 cleared when it is 1); 4: the story's end (the players' neighbour
// ids kept, flow "Movie"); 5: to lot `house` (+0x460, +0x450, +0x464 the house); else to the neighbourhood
// (+0x460, +0x458, house 1).
void cXObjectImpl::TransitionToNewHouse(short, short house, short mode) {
  std::printf("[house] TransitionToNewHouse: mode %d, house %d\n", mode, house);
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (mode == 2) {
    _globals.m_field454 = 1;
    if (_gd.m_hudTarget) _gd.m_hudTarget->GotoMainMenu();
  } else if (mode == 3) {
    _globals.m_field450 = 1;
    if (hood && hood->GetNeighborhoodVar(1) == 1) hood->SetNeighborhoodVar(1, 0);
  } else if (mode == 4) {
    for (int player = 0; player < 2; ++player) {
      cXPerson* p = _globals.GetSelectedPerson(player);
      cXObject* o = p ? p->AsObject() : nullptr;
      auto* person = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
      LiveModeSetPlayerNeighbor(player, person ? person->GetPersonData(0x1f) : 0);  // vtable 0x1B4 (its neighbour id)
    }
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Movie"));
  } else {
    _globals.m_field460 = 1;
    LiveModeSetChangingHouse(1);
    if (mode == 5) {
      _globals.m_field450 = 1;
      _globals.m_field464 = house;
      LiveModeSetNextHouse(house);
    } else {
      _globals.m_field458 = 1;
      _globals.m_field464 = 1;
      LiveModeSetNextHouse(1);
    }
  }
}

// 8008DC0C
void ELiveMode::ChangeHouse() {
  StateMachineManager::s_pStateMachineManager->FindMachineById(0x53494d53)->CallState(TheSimsStateMachine::kLiveModeChangeHouse);
}

// 8008D91C
// Its four states (GC: the heaps compacted first and after).
void LiveModeChangeHouseStateMachine::Startup() {
  AddState(new ChangeHouseDemolishHouse(0));
  AddState(new ChangeHouseSaveHouse(1));
  AddState(new ChangeHouseUnloadHouse(2));
  AddState(new ChangeHouseLoadHouse(3));
}

// 8008DAB8
void LiveModeChangeHouseStateMachine::Shutdown() {
  m_status.ShutdownCurState();
  DeleteAllStates();
}

// 8008DB5C
void LiveModeChangeHouseStateMachine::Reset() {
  _gd.m_field90 = 0;  // DAT_80475C14
  SetNextState(0, 0.0f);
  for (int i = 0; i < 2; ++i)
    if (CameraDirector* d = _globals.CameraDirectorOf(i)) d->Reset();
}

// 8008DBCC
void LiveModeChangeHouseStateMachine::Update(float dt) { StateMachine::Update(dt); }
// 8008DBEC
void LiveModeChangeHouseStateMachine::Draw(ERC* rc) { StateMachine::Draw(rc); }

// 800900A8
ChangeHouseDemolishHouse::ChangeHouseDemolishHouse(int id) : StateMachineState(id) {}
// 800900E0
ChangeHouseSaveHouse::ChangeHouseSaveHouse(int id) : StateMachineState(id) {}
// 80090118
ChangeHouseUnloadHouse::ChangeHouseUnloadHouse(int id) : StateMachineState(id) {}
// 80090150
ChangeHouseLoadHouse::ChangeHouseLoadHouse(int id) : StateMachineState(id) {}

// 8008CC90
void ChangeHouseDemolishHouse::Reset() {
  _gd.m_fieldC0 = 0;  // DAT_80475C44
  LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
}

// 8008CCC0
// Once the loading screen is up: the live mode is not drawn, the simulator stops (mode 0, paused), the
// plumb bobs shut down (GC: and the objects go), then the save.
void ChangeHouseDemolishHouse::Update(float) {
  if (!(LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn)) return;
  if (ELiveMode* live = LiveModeForSound()) live->m_dontDraw = 1;
  if (Globs_pSimulator) {
    Globs_pSimulator->SetMode(cSimulator__Mode{0});
    Globs_pSimulator->Pause();
  }
  PlumbBob::ShutdownAll();
  if (Globs_pObjectModule) Globs_pObjectModule->KillAllObjects();  // vtable 0x6C (as flows 5 / 10: inlevel.cpp)
  OwnerSetNextState(1, 0.0f);
}

// 8008CD78
// The memory card's save of the game (MU request 7: MUStatesTransitionAutoSaveExisting writes the
// neighbourhood, with the lot and the family just saved into it by ELiveMode::Update).
void ChangeHouseSaveHouse::Reset() { MUStateMachine::MURequest(MUStateMachine::kRequestTransitionAutoSave); }

// 8008CD9C
// Its result: done (1) goes on to the unload (DAT_80485900 = 1); failed or cancelled (2, 3) leaves the
// change-house machine.
void ChangeHouseSaveHouse::Update(float) {
  const int r = MUStateMachine::MUPollForResult(MUStateMachine::kRequestTransitionAutoSave);
  if (r == 0) return;
  std::printf("[house] memory card save: %s\n", r == 1 ? "done" : "failed");
  if (r == 2 || r == 3) {
    m_owner->OwnerReturnFromState(1);
  } else if (r == 1) {
    _gd.m_field2C = r;  // DAT_80485900
    OwnerSetNextState(2, 0.0f);
  }
}

// 8008B8A8
// The level's data goes: the global animations, models, characters and shaders (PC: none are held,
// LoadLevelGlobalData is not ported), the sounds no longer used, the routing's path data, and the
// fixed world's arrays (it is left 8 x 8).
void UnloadLevelGlobalData() {
  cGZSndSys::FreeUnusedSounds();
  SIMMY_STUB("UnloadLevelGlobalData: Path::DeallocateAllPathData", 0x8008B8A8);
  if (cFixedWorldImpl* w = Globs_pFixedWorld) {
    w->DeleteArrays();
    w->SetSize(8, 8, false);
  }
}

// 8008CE14
// The house goes (its objects, people, walls, floors, interactors: EHouse::Cleanup) and the current house
// is cleared. PC: as ELiveMode::Reset when the game leaves live mode. GC: also the level's global data, the
// room manager, the object folder, the particle systems and effects.
void ChangeHouseUnloadHouse::Reset() {
  SoundFadeOutAndSetMode(cSoundPlayer::kLoading);  // the music and effects fade (0x20), mode 3
  if (Globs_pHouse) Globs_pHouse->SetFamilyToNull();  // vtable 0x94 (as flows 5 / 10: inlevel.cpp)
  delete _globals.m_house;
  _globals.m_house = nullptr;
  _globals.m_cam = nullptr;
  UnloadLevelGlobalData();
  if (Globs_pRoomManager) Globs_pRoomManager->ResetRoomManager();
  for (psystem* sys : {g_pParticleSystem, g_pParticleWeatherSystem, g_pParticleModelSystem})
    if (sys) PSystemClean(sys);
  SIMMY_STUB("ChangeHouseUnloadHouse::Reset: ObjectFolder vtable 0xCC, InteractorModule::ShutdownInteractorModule "
             "(PC: with the house), Effects::EffectsManager::DeleteAll",
             0x8008CE14);
}

// 8008CF10
void ChangeHouseUnloadHouse::Update(float) { OwnerSetNextState(3, 0.0f); }

// 8008CFD4
void ChangeHouseLoadHouse::Reset() { m_stage = 0; }

// 8008D054
// GC: the house's 0x19 load stages (the next house: NghResFile::SetCurrentHouse(DAT_80476230), the preload
// datasets, BGCall_LoadHouse, its family moved in, the cameras, the simulator started, the HUD made). PC:
// the live mode loads it with its own init state (LiveModeInitState): it is left not initialised and the
// change-house state returns to the in-level state.
void ChangeHouseLoadHouse::Update(float) {
  ELiveMode* live = LiveModeForSound();
  if (live) {
    live->m_initialized = 0;
    live->m_leaving = 0;
  }
  LiveModeSetChangingHouse(0);
  g_pcChangeHouseLoad = 1;  // GC: DAT_80475F28 = 0 (the clock goes on into the next house)
  _globals.m_field460 = 0;
  std::printf("[house] the next house (%d) loads\n", _globals.m_field464);
  m_owner->OwnerReturnFromState(2);
}
