// Port of game/livemode.cpp, game/gamestateman.cpp,
// game/eorhouse.cpp, game/player.cpp and the house parts of game/gamedata.cpp and game/global.cpp
// (partial): from the end of Create-a-Sim into live mode. The game state manager runs the live mode,
// whose first update calls the staged house load (LiveModeInitState); the house is its level, drawn
// through each player's camera.
//
// Still stubbed: the simulation (Globs::pSimulator, pNeighborhood, pObjectModule, pObjectFolder,
// pRoomManager: the neighbourhood and family, the lot's objects and sims, time of day, free will),
// the house's rooms (ERoom: walls and floors), objects (EIObjectMan), room lights, weather and the
// day/night lighting tables, the player interactors and HUD, the intro camera, sounds, vibration,
// the memory unit requests and the change-house / quit flows.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
ELiveMode* s_liveMode = nullptr;  // DAT_80475C48
int s_houseNumber = -1;           // DAT_80476230 the house being entered (1..16)
int s_enteringHouse = 0;          // DAT_80476234 set by PlayerEnterHouse, taken by LiveModeInitState
int s_familyJustMovedIn = 0;      // DAT_80475F24 the family is new in the house (wants and fears made)
int s_playerNeighbor[2] = {};     // DAT_80475C54 / DAT_80475C58 the players' neighbour ids

int g_livemode8047622C = 0;  // DAT_8047622C (going to another house, not the main menu)
int g_livemode80475E98 = 0;
int s_leftHouse = 0;  // DAT_80476228 the house just left  // DAT_80475E98 the player being drawn

const unsigned int kSimsMachine = 0x53494d53;  // 'SIMS', TheSimsStateMachine

bool BGDone() { return BG::s_bBGComplete != 0; }

}  // namespace

// 80014DB4
void BGExec_SetCurHouse() {
  _globals.SetCurHouse(s_houseNumber);
  BG::s_bBGComplete = 1;
}

// 80014DE8
void BGCall_SetCurHouse() {
  BG::s_bBGComplete = 0;
  _pBackground->ExecuteRoutine(BGExec_SetCurHouse);
}

// 80014C14
// GC: the neighbourhood loads the house's lot contents (objects, sims) from the save file.
// PC: the neighbourhood (NeighborhoodImpl::LoadHouse, HouseImpl::LoadFile) is not ported; its world
// part runs: the house's resources loaded from the save, then the world's tile arrays from them.
namespace {
constexpr int kSimulatorResType = 0x53494d49;  // 'SIMI' (kSimulatorResType; resource id 1)
}  // namespace

// DAT_80475F28: 1 while live mode's init loads the game's first house (it takes the house's saved
// clock); 0 for a change of house (the clock goes on: NeighborhoodImpl::LoadHouse keeps the year,
// month, day, hour, minute and second over HouseImpl::LoadFile). PC: g_pcChangeHouseLoad tells the
// init state (which loads the next house too, see changehouse.cpp) that it is a change of house.
int g_liveModeFirstLoad = 0;
int g_pcChangeHouseLoad = 0;

void BGExec_LoadHouse() {
  SIMMY_STUB("BGExec_LoadHouse: Neighborhood::LoadHouse (the simulator, family, objects)", 0x80014C14);
  if (!Globs_pFixedWorld) {
    // HouseImpl::Initialize (partial): the world and the object module.
    SIMMY_STUB("BGExec_LoadHouse: HouseImpl::Initialize (the house's curves)", 0x800BB4AC);
    CreateTheWorld();
    if (!Globs_pSimulator) Globs_pSimulator = cSimulator::CreateInstance();
    if (!Globs_pObjectModule) ObjectModule::CreateInstance();
    Globs_pSimulator->Init();
    if (!Globs_pRoomManager) Globs_pRoomManager = new RoomManager();
  }
  if (!Globs_pHouse) Globs_pHouse = static_cast<HouseImpl*>(House::CreateInstance());  // GC: HouseImpl::Initialize's owner
  if (Globs_pNghResFile && _globals.m_house) {
    const auto house = static_cast<unsigned int>(_globals.m_house->m_house);
    if (Globs_pNghResFile->LoadHouseContents(house, nullptr, false) == 1) {
      const unsigned int previous = Globs_pNghResFile->GetCurrentHouse();
      Globs_pNghResFile->SetCurrentHouse(house);
      if (Globs_pNeighborhood) Globs_pHouse->m_family = Globs_pNeighborhood->GetFamily(0);  // NeighborhoodImpl::LoadHouse (vtable 0x124)
      Globs_pFixedWorld->Load(Globs_pNghResFile, 0);
      // HouseImpl::LoadFile (partial): the simulator's state (the clock, funds; global 0x14 kept), then
      // the objects.
      // NeighborhoodImpl::LoadHouse (800D0874): a change of house keeps the clock.
      static const short kClock[6] = {cSimulatorImpl::kYear, cSimulatorImpl::kMonth, cSimulatorImpl::kDayOfMonth,
                                      cSimulatorImpl::kHour, cSimulatorImpl::kMinute, cSimulatorImpl::kSecond};
      short clock[6] = {};
      const bool keepClock = g_liveModeFirstLoad == 0 && Globs_pSimulator;
      if (keepClock)
        for (int i = 0; i < 6; ++i) clock[i] = static_cast<short>(Globs_pSimulator->GetGlobal(kClock[i]));
      if (Globs_pSimulator) {
        const short keep = static_cast<short>(Globs_pSimulator->GetGlobal(0x14));
        int version = 0;
        if (ReconLoadObject(Globs_pSimulator, Globs_pNghResFile, kSimulatorResType, 1, &version) != 0)
          ReconLoadObject(Globs_pSimulator, Globs_pNghResFile, kSimulatorResType, 0, nullptr);
        Globs_pSimulator->SetGlobal(0x14, keep);
        Globs_pSimulator->SetGlobal(9, 0);
        std::printf("[sim] house %u: %02d:%02d day %d month %d year %d, speed %d, funds %d, ticks %d\n", house,
                    Globs_pSimulator->GetGlobal(0), Globs_pSimulator->GetGlobal(5), Globs_pSimulator->GetGlobal(1),
                    Globs_pSimulator->GetGlobal(7), Globs_pSimulator->GetGlobal(8), Globs_pSimulator->GetSpeed(),
                    Globs_pSimulator->GetFunds(0), Globs_pSimulator->GetTicks());
      }
      if (Globs_pObjectModule && Globs_pObjectFolder) Globs_pObjectModule->Load(Globs_pNghResFile);
      // HouseImpl::LoadFile ends with the rooms (RoomManager::UpdateRooms; PC: the world's rooms and the
      // objects' room ids), before NeighborhoodImpl::LoadHouse runs the objects' PostLoad: load trees
      // ask about rooms (the room scope).
      if (Globs_pRoomManager) Globs_pFixedWorld->ComputeRooms(0);
      // NeighborhoodImpl::LoadHouse, after HouseImpl::LoadFile and before the objects' PostLoad: the house
      // number (+0x70), the house's family and simulator globals 9 / 10.
      if (Globs_pNeighborhood) Globs_pNeighborhood->GetImpl()->SetHouseNum(static_cast<int>(house));  // GC: +0x70 after LoadFile
      // NeighborhoodImpl::LoadHouse: the house's family is the one living in it (vtable 0x12C), else
      // family 0 (vtable 0x124); simulator global 9 its number.
      if (Globs_pNeighborhood) {
        FamilyImpl* family = Globs_pNeighborhood->GetFamilyInHouse(static_cast<int>(house));
        if (!family || house == 0) family = Globs_pNeighborhood->GetFamily(0);
        Globs_pHouse->m_family = family;
        if (Globs_pSimulator) {
          if (family) Globs_pSimulator->SetGlobal(9, static_cast<short>(family->GetNumber()));
          Globs_pSimulator->SetGlobal(10, static_cast<short>(house));
        }
      }
      // HouseImpl::LoadFile: the objects' PostLoad (their load trees: the scenario makes and places the
      // people of the family in the house).
      if (Globs_pObjectModule) Globs_pObjectModule->PostLoad(Globs_pNghResFile, -1, false);
      if (keepClock) {
        for (int i = 0; i < 6; ++i) Globs_pSimulator->SetGlobal(kClock[i], clock[i]);
        Globs_pSimulator->SetTimeOfDay(TimeOfDay{Globs_pSimulator->ComputeTimeOfDay()});
        std::printf("[sim] house %u: the clock goes on: %02d:%02d day %d\n", house, Globs_pSimulator->GetGlobal(0),
                    Globs_pSimulator->GetGlobal(5), Globs_pSimulator->GetGlobal(1));
      }
      // NeighborhoodImpl::LoadHouse: the time-of-day shaders take the clock's state.
      if (Globs_pSimulator) ERShader::InitDayNightShaders(Globs_pSimulator->GetGlobal(0), Globs_pSimulator->GetGlobal(5));
      // NeighborhoodImpl::LoadHouse (PC stand-in): the family's people onto the lot (the players' sims
      // taken from the people the load trees made).
      MoveFamilyOntoLot(_globals.m_house);
      // HouseImpl::RefreshHouse (partial): the rooms (the world's flood, then the RoomManager's).
      if (Globs_pRoomManager) Globs_pFixedWorld->ComputeRooms(0);
      // PC development aid: SIMMY_DUMP_WORLD=1 prints the lot's floor patterns, the tiles with walls and the
      // walls decoded (TileWalls: segments, placements, styles, patterns).
      if (std::getenv("SIMMY_DUMP_WORLD")) {
        cFixedWorldImpl* w = Globs_pFixedWorld;
        std::fprintf(stderr, "DBGWORLD %dx%d\n", w->m_xSize, w->m_ySize);
        for (int y = 0; y < w->m_ySize; ++y) {
          std::fprintf(stderr, "F ");
          for (int x = 0; x < w->m_xSize; ++x) std::fprintf(stderr, "%02x", *w->m_floors->At(x, y));
          std::fprintf(stderr, "  W ");
          for (int x = 0; x < w->m_xSize; ++x) { const unsigned char* p = w->m_walls->At(x, y); bool any = false; for (int k = 0; k < 8; ++k) any |= p[k] != 0; std::fprintf(stderr, "%c", any ? '#' : '.'); }
          std::fprintf(stderr, "\n");
        }
        for (int y = 0; y < w->m_ySize; ++y) {
          std::fprintf(stderr, "R ");
          for (int x = 0; x < w->m_xSize; ++x) {
            CTilePt tile(x, y, 1);
            const unsigned short r = w->GetRoom(tile);
            std::fprintf(stderr, "%c", r >= 0xfffb ? '?' : (r < 10 ? '0' + r : 'a' + (r - 10) % 26));
          }
          std::fprintf(stderr, "  flags ");
          for (int x = 0; x < w->m_xSize; ++x) std::fprintf(stderr, "%x", *w->m_flags->At(x, y) & 0xf);
          std::fprintf(stderr, "\n");
        }
        for (int y = 0; y < w->m_ySize; ++y)
          for (int x = 0; x < w->m_xSize; ++x) {
            TileWalls t(*reinterpret_cast<TileWallStorage*>(w->m_walls->At(x, y)));
            if (!t.HasWall()) continue;
            std::fprintf(stderr, "WALL %2d,%2d segs %02x place %02x", x, y, t.m_segments, t.m_placements);
            for (int seg = t.First(); seg; seg = t.Next(seg))
              std::fprintf(stderr, " [%02x style %u pattern %u/%u]", seg, t.GetStyle(seg), t.GetPattern(seg, 0), t.GetPattern(seg, 1));
            if (t.HasDiagonal()) std::fprintf(stderr, " floor %u/%u", t.GetFloorValue(1), t.GetFloorValue(3));
            std::fprintf(stderr, "\n");
          }
      }
      Globs_pNghResFile->SetCurrentHouse(previous);
    }
  }
  BG::s_bBGComplete = 1;
}

// 80014C6C
void BGCall_LoadHouse() {
  BG::s_bBGComplete = 0;
  _pBackground->ExecuteRoutine(BGExec_LoadHouse);
}

// 80014CB4
void BGExec_CurHouseInit() {
  if (_globals.m_house) _globals.m_house->Init();
  BG::s_bBGComplete = 1;
}

// 80014CE4
void BGCall_CurHouseInit() {
  BG::s_bBGComplete = 0;
  _pBackground->ExecuteRoutine(BGExec_CurHouseInit);
}

// 8008B9C8
// "housepreNN<suffix>" (NN the house), loading in the background.
unsigned int LoadHousePreloadDataset(const char* suffix) {
  char name[64];
  std::snprintf(name, sizeof name, "housepre%02d%s", s_houseNumber, suffix);
  _datasetman.AddRefAsync(name);
  return EResourceManager::CalcId(name);
}

// 8008B7C4
// GC: the animations, models, characters and shaders every level uses, held while in a level.
void LoadLevelGlobalData() {
  SIMMY_STUB("LoadLevelGlobalData (g_aAnimations, g_levelModels, g_levelCharacters, g_levelShaders)", 0x8008B7C4);
}

// ---------------------------------------------------------------------------------------------------
// Player

// 800594F4
Player::Player() {}

// 8005950C
// The player's pie menu and live-mode camera (made once).
void Player::Reset(int index) {
  m_index = index;
  if (!m_piMenu) m_piMenu = new FlashPiMenu(m_index);
  if (!m_cam) {
    m_cam = new ESimsCam(m_index);  // GC: 0x570 bytes, 16-aligned, zeroed, from the main heap
    if (!m_cam) return;
  }
  m_cam->Init();
}

// 800595B8
// The neighbour this player plays: member m_index of the selected player family (GameData +0xD8).
Neighbor* Player::GetPrimaryNeighbor() {
  m_neighbor = nullptr;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (FamilyImpl* f = hood ? hood->GetFamily(_gd.m_fieldD8) : nullptr)
    if (int* guid = f->GetIndexedMember(m_index)) m_neighbor = hood->FindNeighborByGUID(*guid);
  return m_neighbor;
}

// ---------------------------------------------------------------------------------------------------
// GameData: into the house

// 8003B22C
// A new game: mode flags cleared, both players reset (cameras made), the memory meter.
void GameData::GamePlayReset() {
  m_fieldE4 = 1;
  m_fieldE8 = 0;
  m_fieldF0 = 0;
  m_field84 = 0;
  m_fieldAC = 0;  // DAT_80475C30
  m_fieldB0 = 0;  // DAT_80475C34
  m_fieldB4 = 0;  // DAT_80475C38
  for (int i = 0; i < 2; ++i) m_players[i].Reset(i);
  SIMMY_STUB("GameData::GamePlayReset: EMemoryMeterWin::Init", 0x8003B22C);
}

// 8003B8F8
// The house to enter: the player family's; a family not yet in a house (a new game) takes the start
// lot (its goal data told, GC), any other case lot 9 in story mode (else the start lot). The
// neighbourhood's variable 7 (and, in story mode, 2) is the house; then the players' neighbours. GC:
// a second player's pad is mapped to the first other pad held.
void GameData::PlayerEnterHouse() {
  _globals.m_house = nullptr;
  int house = 0;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  FamilyImpl* f = hood ? hood->GetFamily(m_fieldD8) : nullptr;
  bool entered = false;
  if (f) {
    house = f->GetHouseNumber();
    if (house != -1) {
      s_familyJustMovedIn = f->GetJustMovedIn() && m_fieldE8 ? 1 : 0;
      house = f->GetHouseNumber();
      entered = static_cast<unsigned int>(house - 1) < 0x10;
    } else {
      s_familyJustMovedIn = 1;
      house = _app.GetStartLot();
      if (static_cast<unsigned int>(house - 1) < 0x10) {
        // The start lot is unlocked for the family (its goals and unlocks, GoalUnlock::GrantUnlock kind 2).
        GoalUnlock unlocks = FamilyGoalUnlock(m_fieldD8);
        if (unlocks.m_bits) unlocks.GrantUnlock(IGoalUnlock__UnlockType{2}, static_cast<short>(house));
        f->SaveFamily(Globs_pNghResFile, Globs_iSaveFileVersion);
        entered = true;
      }
    }
  }
  if (!entered) house = m_fieldE8 ? _app.GetStartLot() : 9;
  s_houseNumber = house;
  if (hood) {
    hood->SetNeighborhoodVar(NeighborhoodImpl::kVarEnteredHouse, static_cast<short>(house));
    if (!m_fieldE8) hood->m_vars[NeighborhoodImpl::kVarHouse] = static_cast<short>(house);
  }
  if (Neighbor* n = m_players[0].GetPrimaryNeighbor()) s_playerNeighbor[0] = n->m_id;
  // PC dev switch: SIMMY_TWO_PLAYER=1 goes into the house in two-player mode (the front end's two-player
  // path makes player 2's sim in a second CAS, which is not ported; see person.cpp PlayerSimDescription).
  if (simmy::dev_two_player() && !m_fieldEC) {
    SetTwoPlayerMode(true);
    std::printf("[coop] SIMMY_TWO_PLAYER: two-player mode\n");
  }
  if (m_fieldEC) {
    s_playerNeighbor[1] = 0;
    if (Neighbor* n = m_players[1].GetPrimaryNeighbor()) s_playerNeighbor[1] = n->m_id;
    // Player 2 without a pad takes the first other connected pad.
    if (!_pCtrlMan->GetPlayerMapped(1)) {
      const int first = _pCtrlMan->GetPlayerControllerIndex(0);
      for (int i = 0; i < 4; ++i) {
        EController* pad = i != first ? _pCtrlMan->GetController(static_cast<unsigned int>(i)) : nullptr;
        if (pad && (pad->GetStatus() & 5) == 5) {
          _pCtrlMan->MapPlayerToController(1, static_cast<unsigned int>(i));
          m_field34 = 1;
          break;
        }
      }
    }
    std::printf("[coop] player 2: pad %d%s, neighbour %d\n", _pCtrlMan->GetPlayerControllerIndex(1),
                _pCtrlMan->GetPlayerMapped(1) ? "" : " (not mapped)", s_playerNeighbor[1]);
  }
  std::printf("[hood] entering house %d (family %d: %s)\n", house, f ? f->GetNumber() : -1,
              f ? (f->GetHouseNumber() == -1 ? "moving in" : "its house") : "none");
  s_enteringHouse = 1;
}

// 8003D208
void GameData::GotoLiveMode() {
  _gd.m_field90 = 0;  // DAT_80475C14
  _app.m_gameStates->SetState(EGameStateId{EGameStateId::kLive});
}

// 8003D880
void GameData::UseConfigOptions() { EGlobal::UseOptionsScreenAdjustXY(); }

// ---------------------------------------------------------------------------------------------------
// EGlobal: the current house

// 8003F8E0
void EGlobal::SetCurHouse(int house) {
  delete m_house;
  EVec2 pos{*reinterpret_cast<float*>(&m_fieldA4), *reinterpret_cast<float*>(&m_fieldA8)};  // DAT_80475E70
  m_house = new EHouse(pos, house, nullptr, true, true, true, false);  // GC: 0x128 bytes
}

// 8003FA14
void EGlobal::SetCam(ESimsCam* cam) { m_cam = cam; }
// 8003FA1C
ESimsCam* EGlobal::GetCam() { return m_cam; }

// 8003FBFC
// The house's level: the "HouseData" row of the UI strings.
unsigned int EGlobal::GetHouseLevelId(const char* name) {
  if (!name || !m_strings) return 0;
  const void* table = m_strings->getTable(const_cast<char*>("HouseData"));
  const void* row = table ? m_strings->getRow(table, const_cast<char*>(name)) : nullptr;
  return row ? ERQuickdata::U32(row, 0x24) : 0;
}

// ---------------------------------------------------------------------------------------------------
// EGameState / EGameStateMan / DummyMode

int EGameStateMan::s_currentId = 0;

// 80089D78
EGameState::EGameState() {}
// 800899CC
EGameState::~EGameState() {}
// 80089E28
void EGameState::PauseAllSounds() {}
// 80089E2C
void EGameState::ResumeAllSounds() {}
// 80089E20
RET_UNKNOWN EGameState::InTransportMode() { return 0; }

// 80099834
DummyMode::DummyMode() { m_id = EGameStateId::kDummy; }
// 80099864
DummyMode::~DummyMode() {}
// 800998B8
void DummyMode::Init(int) {}
// 800998BC
void DummyMode::Reset(int) {}
// 800998C0
void DummyMode::Update() {}
// 800998C4
void DummyMode::Draw(ERC*) {}

// 80089A14
EGameStateMan::EGameStateMan() {}
// 80089A3C
EGameStateMan::~EGameStateMan() { m_states.clear(); }

// 80089A80
// The state `id` becomes current: the old one is reset, the new one initialised.
void EGameStateMan::SetState(EGameStateId id) {
  EGameState* next = nullptr;
  for (EGameState* s : m_states)
    if (s->m_id == id) {
      next = s;
      break;
    }
  if (!next) return;
  if (m_current) m_current->Reset(0);
  m_current = next;
  s_currentId = id;
  next->Init(0);
}

// 80089B30
void EGameStateMan::AddState(EGameState* state) {
  m_states.push_back(state);
  state->m_owner = this;
}

// 80089B68
void EGameStateMan::KillState() {
  if (m_current) SetState(EGameStateId{EGameStateId::kDummy});
}

// 80089CE0
void EGameStateMan::Update() {
  if (_globals.m_cheats) SIMMY_STUB("EGameStateMan::Update: ECheats::Update", 0x80089CE0);
  if (m_current) m_current->Update();
}

// 80089D3C
void EGameStateMan::Draw(ERC* rc) {
  if (m_current) m_current->Draw(rc);
}


// ---------------------------------------------------------------------------------------------------
// The houses' day/night lighting (eorhouse.cpp's static initializer, 800254D0, fills these; values
// recovered from it): each LevelLightingEntry (0x64 bytes) as its 25 floats.
namespace {
// 0x804746d0
const float k_s_levelLightingDefaults[11][25] = {
  {0.0f, 0.3499999940395355f, 0.6499999761581421f, 1.0f, 0.4099999964237213f, 0.3499999940395355f, 0.550000011920929f, 1.0f, 0.27000001072883606f, -75.0f, 109.0f, 0.5899999737739563f, 0.6700000166893005f, 1.0f, 0.44999998807907104f, 0.20000000298023224f, 0.0f, 0.5199999809265137f, 0.5699999928474426f, 0.8700000047683716f, 0.6100000143051147f, 1.0f, 1.0f, 0.6499999761581421f, 0.28999999165534973f},
  {4.0f, 0.49000000953674316f, 0.6600000262260437f, 0.800000011920929f, 0.4399999976158142f, 0.4000000059604645f, 0.6000000238418579f, 0.8899999856948853f, 0.23000000417232513f, -75.0f, 109.0f, 0.5899999737739563f, 0.699999988079071f, 1.0f, 0.44999998807907104f, 0.28999999165534973f, 0.0f, 0.5199999809265137f, 0.5699999928474426f, 0.8700000047683716f, 0.6000000238418579f, 1.0f, 1.0f, 0.7799999713897705f, 0.23999999463558197f},
  {4.5f, 0.44999998807907104f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 0.4000000059604645f, 0.6000000238418579f, 1.0f, 0.4000000059604645f, -75.0f, 109.0f, 0.4000000059604645f, 0.6000000238418579f, 1.0f, 0.4000000059604645f, 0.30000001192092896f, 0.0f, 0.44999998807907104f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
  {5.0f, 0.9399999976158142f, 0.699999988079071f, 0.44999998807907104f, 0.44999998807907104f, 0.6399999856948853f, 0.5899999737739563f, 0.9599999785423279f, 0.3199999928474426f, -19.0f, 113.0f, 0.7300000190734863f, 0.6299999952316284f, 0.9100000262260437f, 0.4000000059604645f, 0.30000001192092896f, 0.0f, 0.9900000095367432f, 0.8899999856948853f, 0.7599999904632568f, 0.23999999463558197f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
  {6.0f, 1.0f, 0.699999988079071f, 0.4099999964237213f, 0.75f, 0.8500000238418579f, 0.6499999761581421f, 0.8399999737739563f, 0.699999988079071f, -30.0f, 106.0f, 0.7200000286102295f, 0.6800000071525574f, 1.0f, 0.699999988079071f, 0.30000001192092896f, 0.0f, 0.8899999856948853f, 0.6200000047683716f, 0.44999998807907104f, 0.75f, 1.0f, 1.0f, 0.6499999761581421f, 0.28999999165534973f},
  {7.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.800000011920929f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, -36.0f, 95.0f, 0.800000011920929f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.800000011920929f, 0.800000011920929f, 0.8999999761581421f, 0.5f},
  {12.0f, 1.0f, 0.800000011920929f, 0.6000000238418579f, 1.0f, 0.7400000095367432f, 0.7400000095367432f, 1.0f, 0.8500000238418579f, -64.0f, 34.0f, 0.800000011920929f, 0.800000011920929f, 0.8799999952316284f, 0.8500000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 1.0f, 0.800000011920929f, 1.0f, 0.699999988079071f, 0.699999988079071f, 0.699999988079071f, 0.699999988079071f},
  {17.0f, 1.0f, 0.6000000238418579f, 0.25f, 0.9599999785423279f, 0.699999988079071f, 0.699999988079071f, 0.8799999952316284f, 0.7699999809265137f, -56.0f, 0.0f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.8500000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 1.0f, 0.800000011920929f, 0.800000011920929f, 1.0f, 0.3400000035762787f},
  {18.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.6200000047683716f, 0.6000000238418579f, 0.800000011920929f, 0.6700000166893005f, -43.0f, -47.0f, 0.7699999809265137f, 0.6200000047683716f, 0.8999999761581421f, 0.8199999928474426f, 0.30000001192092896f, 0.0f, 1.0f, 0.699999988079071f, 0.46000000834465027f, 1.0700000524520874f, 0.9800000190734863f, 0.7799999713897705f, 0.8500000238418579f, 0.5f},
  {19.0f, 0.9300000071525574f, 0.4000000059604645f, 0.7699999809265137f, 0.5600000023841858f, 0.46000000834465027f, 0.6399999856948853f, 0.8100000023841858f, 0.3499999940395355f, -19.0f, -54.0f, 0.8700000047683716f, 0.8700000047683716f, 1.0f, 0.5899999737739563f, 0.30000001192092896f, 0.0f, 0.9300000071525574f, 0.6700000166893005f, 0.8399999737739563f, 0.7400000095367432f, 1.0f, 1.0f, 0.800000011920929f, 0.4099999964237213f},
  {20.0f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 0.6000000238418579f, 0.6000000238418579f, 1.0f, 0.3499999940395355f, -40.0f, -53.0f, 0.6000000238418579f, 0.6000000238418579f, 1.0f, 0.3499999940395355f, 0.30000001192092896f, 1.401298464324817e-45f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
};
// 0x80474b1c
const float k_s_levelLightingMesaGallery[11][25] = {
  {0.0f, 0.3499999940395355f, 0.6499999761581421f, 1.0f, 0.23999999463558197f, 0.3499999940395355f, 0.550000011920929f, 1.0f, 0.1599999964237213f, -75.0f, 109.0f, 0.5899999737739563f, 0.6700000166893005f, 1.0f, 0.5199999809265137f, 0.20000000298023224f, 0.0f, 0.5199999809265137f, 0.5699999928474426f, 0.8700000047683716f, 0.6100000143051147f, 1.0f, 1.0f, 0.6499999761581421f, 0.28999999165534973f},
  {4.0f, 0.49000000953674316f, 0.6600000262260437f, 0.800000011920929f, 0.23999999463558197f, 0.4000000059604645f, 0.6000000238418579f, 0.8899999856948853f, 0.1599999964237213f, -75.0f, 109.0f, 0.5899999737739563f, 0.699999988079071f, 1.0f, 0.5199999809265137f, 0.28999999165534973f, 0.0f, 0.5199999809265137f, 0.5699999928474426f, 0.8700000047683716f, 0.6000000238418579f, 1.0f, 1.0f, 0.7799999713897705f, 0.23999999463558197f},
  {4.5f, 0.44999998807907104f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 0.4000000059604645f, 0.6000000238418579f, 1.0f, 0.4000000059604645f, -75.0f, 109.0f, 0.4000000059604645f, 0.6000000238418579f, 1.0f, 0.5199999809265137f, 0.30000001192092896f, 0.0f, 0.44999998807907104f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
  {5.0f, 0.9399999976158142f, 0.699999988079071f, 0.44999998807907104f, 0.44999998807907104f, 0.6399999856948853f, 0.5899999737739563f, 0.9599999785423279f, 0.3199999928474426f, -19.0f, 113.0f, 0.7300000190734863f, 0.6299999952316284f, 0.9100000262260437f, 0.5199999809265137f, 0.30000001192092896f, 0.0f, 0.9900000095367432f, 0.8899999856948853f, 0.7599999904632568f, 0.23999999463558197f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
  {6.0f, 1.0f, 0.699999988079071f, 0.4099999964237213f, 0.75f, 0.8500000238418579f, 0.6499999761581421f, 0.8399999737739563f, 0.699999988079071f, -30.0f, 106.0f, 0.7200000286102295f, 0.6800000071525574f, 1.0f, 0.699999988079071f, 0.30000001192092896f, 0.0f, 0.8899999856948853f, 0.6200000047683716f, 0.44999998807907104f, 0.75f, 1.0f, 1.0f, 0.6499999761581421f, 0.28999999165534973f},
  {7.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.800000011920929f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, -36.0f, 95.0f, 0.800000011920929f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.800000011920929f, 0.800000011920929f, 0.8999999761581421f, 0.5f},
  {12.0f, 1.0f, 0.800000011920929f, 0.6000000238418579f, 1.0f, 0.7400000095367432f, 0.7400000095367432f, 1.0f, 0.8500000238418579f, -64.0f, 34.0f, 0.800000011920929f, 0.800000011920929f, 0.8799999952316284f, 0.8500000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 1.0f, 0.800000011920929f, 1.0f, 0.699999988079071f, 0.699999988079071f, 0.699999988079071f, 0.699999988079071f},
  {17.0f, 1.0f, 0.6000000238418579f, 0.25f, 0.9599999785423279f, 0.699999988079071f, 0.699999988079071f, 0.8799999952316284f, 0.7699999809265137f, -56.0f, 0.0f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.8500000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 1.0f, 0.800000011920929f, 0.800000011920929f, 1.0f, 0.3400000035762787f},
  {18.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.6200000047683716f, 0.6000000238418579f, 0.800000011920929f, 0.6700000166893005f, -43.0f, -47.0f, 0.7699999809265137f, 0.6200000047683716f, 0.8999999761581421f, 0.8199999928474426f, 0.30000001192092896f, 0.0f, 1.0f, 0.699999988079071f, 0.46000000834465027f, 1.0700000524520874f, 0.9800000190734863f, 0.7799999713897705f, 0.8500000238418579f, 0.5f},
  {19.0f, 0.9300000071525574f, 0.4000000059604645f, 0.7699999809265137f, 0.5600000023841858f, 0.46000000834465027f, 0.6399999856948853f, 0.8100000023841858f, 0.3499999940395355f, -19.0f, -54.0f, 0.8700000047683716f, 0.8700000047683716f, 1.0f, 0.5899999737739563f, 0.30000001192092896f, 0.0f, 0.9300000071525574f, 0.6700000166893005f, 0.8399999737739563f, 0.7400000095367432f, 1.0f, 1.0f, 0.800000011920929f, 0.4099999964237213f},
  {20.0f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 0.6000000238418579f, 0.6000000238418579f, 1.0f, 0.3499999940395355f, -40.0f, -53.0f, 0.6000000238418579f, 0.6000000238418579f, 1.0f, 0.5199999809265137f, 0.30000001192092896f, 1.401298464324817e-45f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
};
// 0x80474f68
const float k_s_levelLightingBiodome[2][25] = {
  {0.0f, 0.0f, 0.10000000149011612f, 0.15000000596046448f, 0.25f, 0.0f, 0.10000000149011612f, 0.15000000596046448f, 0.15000000596046448f, -70.0f, 28.5f, 0.6200000047683716f, 0.6200000047683716f, 0.75f, 0.6000000238418579f, 0.25f, 0.0f, 0.44999998807907104f, 0.550000011920929f, 0.800000011920929f, 0.800000011920929f, 1.0f, 1.0f, 0.7200000286102295f, 0.25999999046325684f},
  {12.0f, 0.0f, 0.10000000149011612f, 0.15000000596046448f, 0.25f, 0.0f, 0.10000000149011612f, 0.15000000596046448f, 0.15000000596046448f, -70.0f, 28.5f, 0.6200000047683716f, 0.6200000047683716f, 0.75f, 0.6000000238418579f, 0.25f, 0.0f, 0.44999998807907104f, 0.550000011920929f, 0.800000011920929f, 0.800000011920929f, 1.0f, 1.0f, 0.7200000286102295f, 0.25999999046325684f},
};
// 0x80475030
const float k_s_levelLightingSunsetCanyon[11][25] = {
  {0.0f, 0.3499999940395355f, 0.6499999761581421f, 1.0f, 0.4099999964237213f, 0.3499999940395355f, 0.550000011920929f, 1.0f, 0.27000001072883606f, -75.0f, 109.0f, 0.5899999737739563f, 0.6700000166893005f, 1.0f, 0.6000000238418579f, 0.20000000298023224f, 0.0f, 0.5199999809265137f, 0.5699999928474426f, 0.8700000047683716f, 0.6100000143051147f, 1.0f, 1.0f, 0.6499999761581421f, 0.28999999165534973f},
  {4.0f, 0.49000000953674316f, 0.6600000262260437f, 0.800000011920929f, 0.4399999976158142f, 0.4000000059604645f, 0.6000000238418579f, 0.8899999856948853f, 0.23000000417232513f, -75.0f, 109.0f, 0.5899999737739563f, 0.699999988079071f, 1.0f, 0.6000000238418579f, 0.28999999165534973f, 0.0f, 0.5199999809265137f, 0.5699999928474426f, 0.8700000047683716f, 0.6000000238418579f, 1.0f, 1.0f, 0.7799999713897705f, 0.23999999463558197f},
  {4.5f, 0.44999998807907104f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 0.4000000059604645f, 0.6000000238418579f, 1.0f, 0.4000000059604645f, -75.0f, 109.0f, 0.4000000059604645f, 0.6000000238418579f, 1.0f, 0.6000000238418579f, 0.30000001192092896f, 0.0f, 0.44999998807907104f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
  {5.0f, 0.9399999976158142f, 0.699999988079071f, 0.44999998807907104f, 0.44999998807907104f, 0.6399999856948853f, 0.5899999737739563f, 0.9599999785423279f, 0.3199999928474426f, -19.0f, 113.0f, 0.7300000190734863f, 0.6299999952316284f, 0.9100000262260437f, 0.6000000238418579f, 0.30000001192092896f, 0.0f, 0.9900000095367432f, 0.8899999856948853f, 0.7599999904632568f, 0.23999999463558197f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
  {6.0f, 1.0f, 0.699999988079071f, 0.4099999964237213f, 0.75f, 0.8500000238418579f, 0.6499999761581421f, 0.8399999737739563f, 0.699999988079071f, -30.0f, 106.0f, 0.7200000286102295f, 0.6800000071525574f, 1.0f, 0.699999988079071f, 0.30000001192092896f, 0.0f, 0.8899999856948853f, 0.6200000047683716f, 0.44999998807907104f, 0.75f, 1.0f, 1.0f, 0.6499999761581421f, 0.28999999165534973f},
  {7.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.800000011920929f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, -36.0f, 95.0f, 0.800000011920929f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.800000011920929f, 0.800000011920929f, 0.8999999761581421f, 0.5f},
  {12.0f, 1.0f, 0.800000011920929f, 0.6000000238418579f, 1.0f, 0.7400000095367432f, 0.7400000095367432f, 1.0f, 0.8500000238418579f, -64.0f, 34.0f, 0.800000011920929f, 0.800000011920929f, 0.8799999952316284f, 0.8500000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 1.0f, 0.800000011920929f, 1.0f, 0.699999988079071f, 0.699999988079071f, 0.699999988079071f, 0.699999988079071f},
  {17.0f, 1.0f, 0.6000000238418579f, 0.25f, 0.9599999785423279f, 0.699999988079071f, 0.699999988079071f, 0.8799999952316284f, 0.7699999809265137f, -56.0f, 0.0f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.8500000238418579f, 0.30000001192092896f, 0.0f, 1.0f, 0.800000011920929f, 0.4000000059604645f, 1.0f, 0.800000011920929f, 0.800000011920929f, 1.0f, 0.3400000035762787f},
  {18.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.6200000047683716f, 0.6000000238418579f, 0.800000011920929f, 0.6700000166893005f, -43.0f, -47.0f, 0.7699999809265137f, 0.6200000047683716f, 0.8999999761581421f, 0.8199999928474426f, 0.30000001192092896f, 0.0f, 1.0f, 0.699999988079071f, 0.46000000834465027f, 1.0700000524520874f, 0.9800000190734863f, 0.7799999713897705f, 0.8500000238418579f, 0.5f},
  {19.0f, 0.9300000071525574f, 0.4000000059604645f, 0.7699999809265137f, 0.5600000023841858f, 0.46000000834465027f, 0.6399999856948853f, 0.8100000023841858f, 0.3499999940395355f, -19.0f, -54.0f, 0.8700000047683716f, 0.8700000047683716f, 1.0f, 0.5899999737739563f, 0.30000001192092896f, 0.0f, 0.9300000071525574f, 0.6700000166893005f, 0.8399999737739563f, 0.7400000095367432f, 1.0f, 1.0f, 0.800000011920929f, 0.4099999964237213f},
  {20.0f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 0.6000000238418579f, 0.6000000238418579f, 1.0f, 0.3499999940395355f, -40.0f, -53.0f, 0.6000000238418579f, 0.6000000238418579f, 1.0f, 0.6000000238418579f, 0.30000001192092896f, 1.401298464324817e-45f, 0.699999988079071f, 0.699999988079071f, 1.0f, 0.4000000059604645f, 1.0f, 1.0f, 0.800000011920929f, 0.23000000417232513f},
};

LevelLightingEntry s_levelLightingDefaults[11];      // 804746D0
LevelLightingEntry s_levelLightingMesaGallery[11];   // 80474B1C
LevelLightingEntry s_levelLightingBiodome[2];        // 80474F68
LevelLightingEntry s_levelLightingSunsetCanyon[11];  // 80475030

template <int N>
void Fill(LevelLightingEntry (&dst)[N], const float (&src)[N][25]) {
  static_assert(sizeof(LevelLightingEntry) == 25 * sizeof(float), "LevelLightingEntry is 25 floats");
  for (int i = 0; i < N; ++i) std::memcpy(&dst[i], src[i], sizeof dst[i]);
}

// 80423908 s_levelLighting: each house's table (house 5 the gallery, 7 the canyon, 12 the biodome).
LevelLighting* HouseLighting(int index) {
  static LevelLighting tables[16];
  static bool filled = false;
  if (!filled) {
    Fill(s_levelLightingDefaults, k_s_levelLightingDefaults);
    Fill(s_levelLightingMesaGallery, k_s_levelLightingMesaGallery);
    Fill(s_levelLightingBiodome, k_s_levelLightingBiodome);
    Fill(s_levelLightingSunsetCanyon, k_s_levelLightingSunsetCanyon);
    for (LevelLighting& t : tables) t = LevelLighting{s_levelLightingDefaults, 11};
    tables[4] = LevelLighting{s_levelLightingMesaGallery, 11};
    tables[6] = LevelLighting{s_levelLightingSunsetCanyon, 11};
    tables[11] = LevelLighting{s_levelLightingBiodome, 2};
    filled = true;
  }
  return &tables[index];
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// EHouse

namespace {
// 804239C8 s_houseNames: "house 01" .. "house 16".
const char* HouseNameText(int house) {
  static char names[16][12];
  int i = house - 1;
  if (i < 0) i = 0;
  else if (i > 15) i = 15;
  if (!names[i][0]) std::snprintf(names[i], sizeof names[i], "house %02d", i + 1);
  return names[i];
}
}  // namespace

// 8002371C
// The house `house`: its rooms, objects and level (the house's own from the "HouseData" table unless
// given), which becomes the engine's current level.
EHouse::EHouse(EVec2& pos, int house, ERLevel* level, bool objects, bool rooms, bool, bool isLot) {
  m_isLot = isLot ? 1 : 0;
  m_house = house;
  m_posY = pos.y;
  m_posX = pos.x;
  if (rooms) m_room = new ERoom;
  if (objects) m_objectMan = new EIObjectMan(this);
  if (!level) {
    const unsigned int id = _globals.GetHouseLevelId(GetHouseName());
    m_level = static_cast<ERLevel*>(_levelman.AddRef(id, nullptr, 0));
  } else {
    m_level = level;
    m_level->AddRef();
  }
  _pEngine->m_currentLevel = m_level;
  if (m_level) {
    const int i = m_house - 1 > 15 ? 15 : m_house - 1;
    m_level->m_dayNight = HouseLighting(i < 0 ? 0 : i);
    m_level->m_rainInfo = &LevelRainInfo[i < 0 ? 0 : i];
    m_level->m_sunYawOffset = 0.0f;  // 80423988 s_levelLightingSunYawOffset: 0 for every house
    m_model = _modelman.GetRef(m_level->m_id);
  }
  if (m_level) m_level->SetDrawWallShadowsFunc(EIWallPart::DrawOutsideShadows);
}

// 800239A0
EHouse::~EHouse() { Cleanup(); }

// 800236F8
const char* EHouse::GetHouseName() { return HouseNameText(m_house); }

// 800239EC
void EHouse::Init() {
  // PC development aid: SIMMY_WALLS=0 (up), 1 (cut-away, the default), 2 (down) before the walls are built.
  if (const char* walls = std::getenv("SIMMY_WALLS")) {
    const int mode = std::atoi(walls);
    if (mode >= 0 && mode <= 2) m_wallMode = mode;
    if (m_level) m_level->m_wallFadeState = m_wallMode;  // as SetNextWallMode leaves it
  }
  BuildHouse();
  m_built = 1;
}

// 80023A20
// The objects placed, the walls and floors built, the rooms counted and lit.
void EHouse::BuildHouse() {
  _pGfx->Flush();
  m_field10 = 1;
  if (m_built) Cleanup();
  if (m_objectMan) {
    EIObjectMan::Init();
    m_objectMan->PostLoad();
  }
  if (m_room) m_room->Init();
  ERLevel::GroupWallInstances();
  if (!m_isLot) {
    // The rooms: one more than the highest inside room.
    unsigned int highestRoom = 0;
    if (Globs_pRoomManager)
      for (auto& entry : Globs_pRoomManager->m_rooms) {
        Room* room = entry.second;
        if (room && room->m_inUse && !room->IsOutside() && highestRoom < entry.first) highestRoom = entry.first;
      }
    m_roomCount = static_cast<int>(highestRoom) + 1;
    if (m_level && m_roomCount != m_level->m_roomCount) m_level->SetRoomCount(m_roomCount);
    InitRoomLighting();
    EIFloor::CreateFloors(this);
  }
  m_ready = 1;
  SAnimator2::SetIsRaining(false);
}

// 800251FC
// A room light's rooms: the room, and its lighting group.
void LightLocation(unsigned int room, bool, short& lightRoom, short& otherRoom) {
  otherRoom = static_cast<short>(room);
  lightRoom = static_cast<short>(room);
  if (Globs_pRoomManager && (room & 0xffff) < 0xfffb) {
    if (Room* r = Globs_pRoomManager->GetRoom(static_cast<unsigned short>(room)))
      otherRoom = static_cast<short>(r->m_lightingGroup);
  }
}

// 80025264
// A light per inside room (EILight placed by LightLocation), then the outside lights at the hour and
// the rooms' ambient light, and the level's light grid.
void EHouse::InitRoomLighting() {
  CleanUpRoomLights();
  m_field10 = 1;
  m_roomLights = new EILight*[static_cast<size_t>(m_roomCount)];
  for (int i = 0; i < m_roomCount; ++i) {
    Room* room = Globs_pRoomManager ? Globs_pRoomManager->GetRoom(static_cast<unsigned short>(i)) : nullptr;
    // GC: rooms without a Room are not checked (every id below the count has one).
    if (!room || room->m_lightingGroup == 0) {
      m_roomLights[i] = nullptr;
      continue;
    }
    auto* light = new EIAmbLight();
    light->Init();
    m_roomLights[i] = light;
    light->m_thisSideData = static_cast<unsigned int>(i);
    light->m_thisSideFn = LightLocation;
    if (m_level) m_level->InsertLight(light);
  }
  LevelLightingEntry entry;
  if (m_level) m_level->OutsideLightsInit(m_hour, entry);
  UpdateRoomAmbient(entry);
  if (m_level) {
    m_level->ResetLightLocations();
    m_level->InitializeLightingGrid();
  }
}

// 80024DB8
void EHouse::DestroyWalls() {
  if (m_room) {
    delete m_room;
    m_room = nullptr;
  }
  if (m_level) m_level->ResetLightLocations();
}

// 800253AC
void EHouse::CleanUpRoomLights() {
  if (!m_roomLights) return;
  for (int i = 1; i < m_roomCount; ++i) {
    if (!m_roomLights[i]) continue;
    if (m_level) m_level->RemoveLight(m_roomLights[i]);
    delete m_roomLights[i];
  }
  delete[] m_roomLights;
  m_roomLights = nullptr;
}

// 800246F0
// Each inside room's light takes the entry's ambient colour and intensity (times the room's
// multiplier).
void EHouse::UpdateRoomAmbient(LevelLightingEntry& entry) {
  for (int i = 1; i < m_roomCount; ++i) {
    EILight* light = m_roomLights ? m_roomLights[i] : nullptr;
    if (!light) continue;
    Room* room = Globs_pRoomManager->GetRoom(static_cast<unsigned short>(i));
    light->m_intensity = static_cast<float>(entry.m_field60 * room->GetAmbientLightMultiplier());
    light->m_color = entry.m_field54;
  }
}

// 80024CFC
void EHouse::Cleanup() {
  if (m_objectMan) {
    if (m_level) m_objectMan->RemoveObjectsFromHouse(m_level);
    delete m_objectMan;
    m_objectMan = nullptr;
  }
  ShutdownDirectInteractors();  // PC (GC: InteractorModule::ShutdownInteractorModule)
  RemoveFamilyFromLot();  // PC: the people MoveFamilyOntoLot made (GC: the object module's)
  DestroyWalls();
  EIFloor::DestroyFloors();
  // PC: the level resource outlives the house here (GC freed it), so its wall range is cleared for
  // the next house built on it (after quitting to the main menu).
  if (m_level) m_level->ClearWallIndexSaves();
  if (m_level) {
    if (_pEngine->m_currentLevel == m_level) _pEngine->m_currentLevel = nullptr;
    m_level->DelRef();
    m_level = nullptr;
  }
  m_model = nullptr;
  m_field10 = 1;
  m_ready = 0;
}

// 80024430
// GC: also the interest fade and the walls fading toward the camera, and the rain.
void EHouse::Draw(ERC* rc) {
  if (!m_level) return;
  SetupInterestFade();
  ESimsCam* cam = _globals.m_cam;
  if (cam) EInstance::SetContext(cam->m_player);
  if (m_room) m_room->UpdateWallFade(cam ? cam->m_player : 0);
  if (m_rain) m_rain->RainModeUpdate(_gd.m_fieldEC ? m_minutesPassed * 0.5f : m_minutesPassed);  // DAT_80475C70: two players
  m_level->Draw(rc);
}

// 80023BAC
void EHouse::SetWallState(EWallUpDownStateType state) {
  m_wallMode = state.value;
  if (m_room) m_room->SetWallState(state);
}

// 80023BE0
// Cut-away -> down -> up -> cut-away. GC: no caller in this build (the HUD's wall button is not
// wired); PC: SIMMY_WALLS=0/1/2 picks the mode the house starts in (see EHouse::Init).
void EHouse::SetNextWallMode() {
  m_wallMode = m_wallMode == 1 ? 2 : m_wallMode == 0 ? 1 : 0;
  if (m_room) {
    m_room->SetWallState(EWallUpDownStateType{m_wallMode});
    if (m_level) m_level->m_wallFadeState = m_wallMode;
  }
  SIMMY_STUB("EHouse::SetNextWallMode: the graphics refresh (_pGfx vtable 0x44)", 0x80023BE0);
}

// 80023C84
// The fades for this frame from the drawing camera and its player's point of interest: the wall
// fade's trapezoid from the eye to the cursor (the tuning with an interactor, "DC"), the cursor's
// width across the camera's view. GC: also the object fades (PyramidFade, LineFade), enabled unless
// a camera animation or depth-of-field shot runs, set up around the cursor or the selected sim's
// interaction target (its icon or stack object), wider while the sim's interaction is in progress.
void SetupInterestFade() {
  ESimsCam* cam = _globals.m_cam;
  // GC: DAT_80476108 (a switch only read here) and the camera.
  if (!cam) {
    _interestFade.m_enabled = 0;
    return;
  }
  _interestFade.m_enabled = 1;
  const int player = cam->m_player;
  ERoom::s_wallFadeParms = &ERoom::s_wallFadeParmsDC;  // GC: Classic without a player interactor
  WallFadeParms& p = *ERoom::s_wallFadeParms;
  p.Validate();
  _interestFade.m_range = p.m_range;
  _interestFade.m_offset = p.m_min;
  _interestFade.m_invRange = p.m_invRange;
  _interestFade.m_full = p.m_full;
  _interestFade.m_3d = p.m_3d;
  _interestFade.m_nearWidth = p.m_nearWidth;
  _interestFade.m_farWidth = p.m_farWidth;
  _interestFade.m_least = p.m_least;
  _interestFade.m_gone = p.m_gone;
  _interestFade.m_planeCount = 3;
  _interestFade.m_eye[0] = cam->m_eye.x;
  _interestFade.m_eye[1] = cam->m_eye.y;
  _interestFade.m_eye[2] = cam->m_eye.z;
  const EMat4& frame = cam->m_win.m_lookAtPos;  // ESimsCam +0x100
  _interestFade.m_dir[0] = frame.m[1][0];
  _interestFade.m_dir[1] = frame.m[1][1];
  _interestFade.m_dir[2] = frame.m[1][2];
  _interestFade.m_dirty = 1;
  InteractorModule__InteractorInfo info;  // InteractorManager::GetPlayerInteractorInfo
  GetPlayerInteractorInfo(player, info);
  _interestFade.m_interest[0] = info.m_x;
  _interestFade.m_interest[1] = info.m_y;
  _interestFade.m_interest[2] = info.m_h;
  // The object fades: on unless the intro camera or a letterboxed camera shot runs.
  CameraDirector* director = _globals.m_cameraDirector;
  const bool shot = director && director->m_field234 != 0;
  const bool objectFades = !(s_liveMode && s_liveMode->m_introCamera) && !shot;
  g_pyramidFade.SetEnabled(objectFades);
  g_lineFade.SetEnabled(objectFades);
  g_pyramidFade.m_instant = 0;
  // Across the camera's view (its look direction turned a quarter), half the cursor's width each way.
  float across[3] = {cam->m_win.m_lookDir.y, -cam->m_win.m_lookDir.x, 0.0f};
  if (across[0] != 0.0f || across[1] != 0.0f) {
    const float k = 1.0f / std::sqrt(across[0] * across[0] + across[1] * across[1]);
    across[0] *= k, across[1] *= k;
  }
  const float half = info.m_w * 0.5f;
  float centre[2] = {info.m_x, info.m_y};
  float height = info.m_h;
  float q0[2] = {centre[0] - across[0] * half, centre[1] - across[1] * half};
  float q1[2] = {centre[0] + across[0] * half, centre[1] + across[1] * half};
  // The selected sim's interaction's object (its icon object, else its stack object) and that one's
  // container never fade.
  const void* exclude[2] = {nullptr, nullptr};
  if (auto* person = dynamic_cast<cXPersonImpl*>(_globals.GetSelectedPerson(player))) {
    if (Interaction* action = person->GetCurrentAction()) {
      cXObject* object = action->GetIconObject();
      if (!object) object = action->GetStackObject();
      if (cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr) {
        exclude[0] = static_cast<EInstance*>(impl->m_instance);
        if (cXObject* container = impl->GetContainer())
          if (cXObjectImpl* c = container->GetObjectImplementation()) exclude[1] = static_cast<EInstance*>(c->m_instance);
      }
    }
    // Only things taller than 1.5 fade, 0.25 while depth of field runs; then the fade is instant and
    // total, and the quad wider and higher.
    FrameEffectsManager* fx = director ? director->GetFrameEffectsManager() : nullptr;
    DepthOfField* dof = fx ? fx->DepthOfFieldObject() : nullptr;
    const bool dofOn = dof && dof->m_state != kFrameEffectOff;
    g_pyramidFade.m_minHeight = dofOn ? 0.25f : 1.5f;
    // GC: with the player's director attached to a sim (+0x11C) in a shot or under depth of field, the
    // quad moves to that sim (or spans it and the interaction's object when they are within 2 tiles).
    g_pyramidFade.m_floor = 0.25f;
    if (dofOn) {
      q0[0] -= across[0] * 0.4f, q0[1] -= across[1] * 0.4f;  // widthFudgeFactor (804FD98C)
      q1[0] += across[0] * 0.4f, q1[1] += across[1] * 0.4f;
      g_pyramidFade.m_instant = 1;
      height += 0.4f;  // heightFudgeFactor (804FD990)
      g_pyramidFade.m_floor = 0.0f;
    }
  }
  g_pyramidFade.m_exclude[0] = exclude[0];
  g_pyramidFade.m_exclude[1] = exclude[1];
  g_pyramidFade.m_depthLimits = 1;
  const float quad[12] = {q0[0], q0[1], 0.0f, q1[0], q1[1], 0.0f, q1[0], q1[1], height, q0[0], q0[1], height};
  const float eye[3] = {cam->m_eye.x, cam->m_eye.y, cam->m_eye.z};
  g_pyramidFade.Setup(eye, quad);
  const float point[3] = {centre[0], centre[1], 0.5f};
  g_lineFade.Setup(eye, point);
}

// 800244CC
// The time of day from the simulator's clock (the minute of the day, the minutes since the last update,
// the hour with its fraction), the outside lights for it, the rooms' ambient light.
void EHouse::Update() {
  const int hour = SimulatorHour(), minute = SimulatorMinute(), second = SimulatorSecond();
  const float minuteOfDay = static_cast<float>(hour * 60 + minute);
  m_minutesPassed = m_minuteOfDay <= minuteOfDay ? minuteOfDay - m_minuteOfDay : 1440.0f - m_minuteOfDay + minuteOfDay;
  m_updating = 1;
  m_minuteOfDay = minuteOfDay;
  UpdateWeather();
  m_hour = static_cast<float>(hour) + static_cast<float>(minute) / 60.0f + static_cast<float>(second) / 3600.0f;
  LevelLightingEntry entry;
  if (m_level) m_level->OutsideLightsUpdate(m_hour, entry);
  UpdateRoomAmbient(entry);
  UpdateRoomLights();
  SIMMY_STUB("EHouse::Update: weather, the controller refresh button", 0x800244CC);
  if (m_level) m_level->Update();
  m_updating = 0;
}

// ---------------------------------------------------------------------------------------------------
// ELiveMode

// 8008BAAC
ELiveMode::ELiveMode() {
  m_dontDraw = 1;
  m_id = EGameStateId::kLive;
  s_liveMode = this;
  m_field54 = -1;
}

// 8008BB08
ELiveMode::~ELiveMode() {
  if (s_liveMode == this) s_liveMode = nullptr;
}

// 8008BB68
void ELiveMode::Init(int arg) {
  _gd.m_fieldC0 = 0;  // DAT_80475C44
  SIMMY_STUB("ELiveMode::Init: ObjectFolder reset, InteractorModule::InitializeInteractorModule", 0x8008BB68);
  m_arg = arg;
  m_backgroundSet = 0;
  m_storyStart = 0;
  m_field20 = 0;
  m_field50 = 0;
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
  m_shader = static_cast<ERShader*>(_shaderman.AddRef(0x8e4c2d0bu, nullptr, 0));
  _gd.m_fieldCC = 0;  // DAT_80475C50
  _gd.m_fieldC8 = 0;  // DAT_80475C4C
}

// 8008CA58
// Leaving live mode: the house and everything the level used.
void ELiveMode::Reset(int) {
  SIMMY_STUB("ELiveMode::Reset: heaps, plumb bobs, interactors, objects, particles, effects, rooms", 0x8008CA58);
  SoundFadeOutAndSetMode(cSoundPlayer::kLoading);
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
  _globals.m_cam = nullptr;
  delete _globals.m_house;
  _globals.m_house = nullptr;
  m_dontDraw = 1;
  m_initialized = 0;
}

// 8008DC40
void ELiveMode::GotoStartMode() {
  g_livemode8047622C = 0;
  m_leaving = 1;
  LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{0});
  if (_globals.m_vibrate) _globals.m_vibrate->StopAllVibration();  // GC: DAT_80475EDC unchecked
}

// 8008DC90
// Back to the main menu once the loading screen is up: the game state goes (Reset tears the house
// down) and InLevelBaseState returns to the front end (DAT_80475C18).
void ELiveMode::QuitToMainMenu() {
  _gd.m_field90 = 0;  // DAT_80475C14
  m_leaving = 0;
  if (auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer)) viewer->m_field3A8 = 0;
  SIMMY_STUB("ELiveMode::QuitToMainMenu: PlumbBob::ShutdownAll, the object module (vtable 0x68), "
             "Globs::pHouse (vtable 0x90), the simulator (vtable 0x88 with 0, 0x50); the interactors "
             "go with the house (EHouse::Cleanup)",
             0x8008DC90);
  ESim::DeletePlayerCachedModel();
  if (Globs_pSound) Globs_pSound->QuietAll();
  _gd.m_field94 = 1;  // DAT_80475C18
  _app.m_gameStates->KillState();
}

// 8008DD70
// Not before the house is loaded: the first call runs LiveModeInitState. While leaving for the main
// menu, the loading screen first.
int ELiveMode::IsReadyForUpdate() {
  if (_gd.m_field9C) {
    if (!LoadingScreenStateMachine::LoadInProgress()) GotoStartMode();
    else if (LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn) {
      QuitToMainMenu();
      _gd.m_field2C = 0;  // DAT_80485900 (GC: a separate global)
      _gd.m_fieldA8 = 1;  // DAT_80475C2C
      _gd.m_field9C = 0;
    }
    return 0;
  }
  if (!m_initialized) {
    StateMachineManager::s_pStateMachineManager->FindMachineById(static_cast<int>(kSimsMachine))
        ->CallState(TheSimsStateMachine::kLiveModeInit);
    return 0;
  }
  _gd.m_field90 = 1;  // DAT_80475C14
  if (_gd.m_fieldC8) {
    if (--_gd.m_fieldCC < 1) {
      _gd.m_fieldC8 = 0;
      if (_gd.m_players[0].m_cam) _gd.m_players[0].m_cam->CenterOnSelectedSim();
      if (_gd.m_fieldEC && _gd.m_players[1].m_cam) _gd.m_players[1].m_cam->CenterOnSelectedSim();
    }
    _gd.m_fieldC0 = 1;
  }
  if (m_leaving) {
    if (!g_livemode8047622C) {
      QuitToMainMenu();
      return 0;
    }
    ChangeHouse();
  } else if (m_field28) {
    if (!_gd.m_fieldE8 && !_gd.m_fieldF0) return 0;
    m_field18 = 0.0f;
    m_field14 = 1200.0f;
  }
  if (m_storyStart != 1) return 1;
  m_storyStart = 0;
  if (Globs_pSimulator) Globs_pSimulator->Resume();
  return 1;
}

namespace {
// 80423E6C s_introCameraName: each lot's move-in camera animation (AnimRef rows), by house - 1.
const char* const s_introCameraName[16] = {
    "cam_intro_lot01_story_part01", "cam_intro_lot02_story_part01", "cam_intro_lot03_story_part01",
    "cam_intro_lot04_story_part01", "cam_intro_lot05_story_part01", "cam_intro_lot06_story_part01",
    "cam_intro_lot07_story_part01", "cam_intro_lot08_story_part01", "cam_intro_lot09_story_part01",
    "cam_intro_lot10_story_part01", "cam_intro_lot11_story_part01", "cam_intro_lot12_story_part01",
    nullptr, nullptr, nullptr, nullptr};
}  // namespace

// 8008DF28
// The camera shot when a family arrives on a lot (TriggerIntroCamera; both players' directors in
// two-player games). It runs when the family has just moved in or comes back from CAS (EGlobal +0xB0,
// the CAS mode), not for a CAS return in two-player games, and (on its first step) only while the lot is
// "recently unlocked" (type 2 unlocks of the player family): each lot's story intro plays once.
// Steps (+0x0C): 1 wait until a family member is awake and visible, the camera's player has a selected
// sim and its instance is placed; then the lot is no longer recently unlocked, the loading screen goes
// (free play: straight to 6); 2 start the shot (StartIntroCamera; failing: the directors reset); 3 wait
// for the animation to reach its hold; 4 hold for 10 s of unpaused simulation, then let go
// (EndIntroCamera); a director that is playing something else instead (another shot took it) has the game
// camera put back later (+0x320) and the intro ends; 5 wait for the directors to finish; 6 done.
// Otherwise everything is reset (both directors, the letterbox, both cameras) and the loading screen goes.
void ELiveMode::UpdateIntroCamera() {
  FamilyImpl* family = Globs_pNeighborhood ? Globs_pNeighborhood->GetFamily(1) : nullptr;
  // The house (GC: NghResFile +0x24 + 1; PC: the house's own number, as the PC house load puts the
  // file's current house back afterwards).
  const short lotUnlock = static_cast<short>(_globals.m_house ? _globals.m_house->m_house : 0);
  int skip = 0;
  if (m_introCamera == 1 && family) {
    GoalUnlock unlocks = FamilyGoalUnlock(1);
    skip = unlocks.m_bits && unlocks.IsRecentlyUnlocked(IGoalUnlock__UnlockType{2}, lotUnlock) ? 0 : 1;
  }
  CameraDirector* d0 = _globals.CameraDirectorOf(0);
  CameraDirector* d1 = _globals.CameraDirectorOf(1);
  ESimsCam* cam0 = _gd.m_players[0].m_cam;
  ESimsCam* cam1 = _gd.m_players[1].m_cam;
  const bool run = (!_gd.m_fieldEC || !_globals.m_fieldB0) && (s_familyJustMovedIn || _globals.m_fieldB0) && !skip;
  // PC dev trace: SIMMY_CAM_TRACE=1 prints the intro camera's steps.
  static const bool trace = std::getenv("SIMMY_CAM_TRACE") != nullptr;
  if (trace) {
    static int s_lastStep = -1;
    if (s_lastStep != m_introCamera || !run)
      std::printf("[cam] f%d intro step %d run %d (moved in %d, cas %d, lot %d recently unlocked %d)\n", _framecount,
                  m_introCamera, run ? 1 : 0, s_familyJustMovedIn, _globals.m_fieldB0, lotUnlock, skip ? 0 : 1);
    s_lastStep = m_introCamera;
  }
  auto finish = [&]() {
    gbDrawUI = 1;
    gbUpdateUI = 1;
    LoadingScreenStateMachine::LoadFinishing();
  };
  if (!run) {
    m_introCamera = 0;
    m_introTime = 0.0f;
    _globals.m_fieldAC = 0;
    _globals.m_fieldB0 = 0;
    if (d0) d0->Reset();
    if (d1) d1->Reset();
    if (d0) d0->LetterBox(false);
    if (d1) d1->LetterBox(false);
    if (cam0) cam0->Reset();
    if (cam1) cam1->Reset();
    m_dontDraw = 0;
    finish();
    return;
  }
  switch (m_introCamera) {
    case 1: {
      bool ready = false;
      ESimsCam* cam = _globals.m_cam;
      cXPerson* person = cam ? _globals.GetSelectedPerson(cam->m_player) : nullptr;
      if (Globs_pObjectModule && Globs_pObjectModule->IsFamilyMemberAwakeAndVisible() && _globals.m_cameraDirector && person) {
        cXObject* object = person->AsObject();
        cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
        if (auto* sim = impl ? static_cast<ESim*>(impl->m_instance) : nullptr) {
          const EMat4& m = sim->m_orient;  // (GC: the instance's +0xC0: placed once its position row is set)
          ready = m.m[3][0] != 0.0f || m.m[3][1] != 0.0f || m.m[3][2] != 0.0f || m.m[3][3] != 1.0f;
        }
      }
      if (!ready) return;
      m_introCamera = _gd.m_fieldE8 ? 6 : 2;
      if (family) {
        GoalUnlock unlocks = FamilyGoalUnlock(1);
        if (unlocks.m_bits) unlocks.SetRecentlyUnlocked(IGoalUnlock__UnlockType{2}, lotUnlock, false);
      }
      finish();
      return;
    }
    case 2:
      if (StartIntroCamera(d0, cam0)) {
        if (_gd.m_fieldEC && d1) d1->RegisterCurrentCamera(cam1, false);
        m_introCamera = 3;
      } else {
        m_introCamera = 0;
        _globals.m_fieldB0 = 0;
        _globals.m_fieldAC = 0;
        if (d0) d0->Reset();
        if (_gd.m_fieldEC && d1) d1->Reset();
      }
      m_dontDraw = 0;
      return;
    case 3:
      if (!d0) return;
      if (d0->IsActive() && d0->m_state != CameraDirector::kHold) return;
      m_introCamera = 4;
      d0->SetHoldState(0.0f);
      d0->m_flags &= ~1u;
      m_introTime = 10.0f;
      return;
    case 4:
      if (!d0) return;
      if (d0->IsActive() && d0->m_state != CameraDirector::kHoldTimed) {
        d0->m_field320 = 1;
        if (_gd.m_fieldEC && d1) d1->m_field320 = 1;
        m_introCamera = 6;
        return;
      }
      if (Globs_pSimulator && Globs_pSimulator->IsStalled()) return;
      m_introTime -= _dtlim15;
      if (0.0f <= m_introTime) return;
      m_introTime = 0.0f;
      if (!EndIntroCamera(d0, cam0)) return;
      if (_gd.m_fieldEC) EndIntroCamera(d1, cam1);
      m_introCamera = 5;
      return;
    case 5:
      if (!d0 || d0->IsActive()) return;
      m_introCamera = 6;
      return;
    case 6:
      if (d0) d0->m_flags &= ~1u;
      if (_gd.m_fieldEC && d1) d1->m_flags &= ~1u;
      m_introCamera = 0;
      _globals.m_fieldAC = 0;
      _globals.m_fieldB0 = 0;
      return;
    default:
      m_introCamera = 0;
      _globals.m_fieldAC = 0;
      _globals.m_fieldB0 = 0;
      return;
  }
}

// 8008FBFC
// The game camera reset, then (unless EGlobal +0xAC) the world bloom and motion blur set and the shot
// played on the director: the lot's intro (by house; the lot number kept at +0x54) or, back from CAS,
// "cam_transition_cas_exit" attached to the selected sim's instance. The director is then disabled (no
// other shot can take it) and, in two-player games, the camera drawn full screen.
RET_UNKNOWN ELiveMode::StartIntroCamera(CameraDirector* director, ESimsCam* cam) {
  if (cam) cam->Reset();
  if (_globals.m_fieldAC) return 0;
  _pGfx->SetWorldBloom(2.0f, 0.25f, 2.0f, 1.0f / 3.0f);
  _pGfx->SetMotionBlur(0.0f, 0.0f, 1.0f, 0.15f);
  if (!director) return 0;
  director->m_flags &= ~1u;
  if (cam) director->RegisterCurrentCamera(cam, true);
  EResource* qd = _quickdataman.AddRef(0x0C33DB41u, nullptr, 0);
  auto* quick = static_cast<ERQuickdata*>(qd);
  const void* table = quick ? quick->getTable("AnimRef") : nullptr;
  const char* name;
  if (!_globals.m_fieldB0) {
    const int lot = _globals.m_house ? _globals.m_house->m_house - 1 : 0;  // (GC: NghResFile +0x24; see UpdateIntroCamera)
    m_field54 = lot;
    name = lot >= 0 && lot < 16 ? s_introCameraName[lot] : nullptr;
  } else {
    name = "cam_transition_cas_exit";
  }
  const void* row = name && table ? quick->getRow(table, name) : nullptr;
  if (!row) return 0;
  bool attached = false;
  if (_globals.m_fieldB0 && cam) {
    cXPerson* person = _globals.GetSelectedPerson(cam->m_player);
    cXObject* object = person ? person->AsObject() : nullptr;
    cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
    if (auto* sim = impl ? static_cast<ESim*>(impl->m_instance) : nullptr) {
      director->AttachDummy(nullptr, &sim->m_orient);
      attached = true;
    }
  }
  if (!director->StartAnim(const_cast<AnimRef*>(static_cast<const AnimRef*>(row)), attached, false)) return 0;
  if (cam) cam->SetActiveNoiseSetting(0);
  director->Update();
  director->m_flags |= 1u;
  if (_gd.m_fieldEC && cam) cam->m_fullScreen = 1;
  return 1;
}

// 8008FE28
// Once a family member is awake and visible: the director gives the camera back (ReleaseToGameCamera),
// the noise stops and the camera shares the screen again.
RET_UNKNOWN ELiveMode::EndIntroCamera(CameraDirector* director, ESimsCam* cam) {
  if (!director || !Globs_pObjectModule || !Globs_pObjectModule->IsFamilyMemberAwakeAndVisible()) return 0;
  director->m_flags &= ~1u;
  director->ReleaseToGameCamera();
  if (cam) cam->SetActiveNoiseSetting(-1);
  if (_gd.m_fieldEC && cam) cam->m_fullScreen = 0;
  return 1;
}

// 8008FBD0
void ELiveMode::TriggerIntroCamera(CameraDirector* director) {
  m_introCamera = 1;
  if (!director) return;
  director->m_state = 1;
  director->m_flags |= 1;
  m_dontDraw = 1;
  director->m_field320 = 1;
}

// 8008E4C0
// Each frame once loaded: the cameras (directors, then each player's), the simulation, the house.
void ELiveMode::Update() {
  if (!IsReadyForUpdate()) return;
  if (_globals.m_house) simmy::det_live_update();  // PC: the determinism harness's ticks (pc_det.cpp)
  Player* players = _gd.m_players;
  if (!m_paused) {
    _globals.SetCameraDirector(_globals.CameraDirectorOf(0));
    _globals.SetCam(players[0].m_cam);
  }
  SIMMY_STUB("ELiveMode::Update: CameraManager, free will", 0x8008E4C0);
  ESpriteRenderMan::Instance()->Update();  // GC: after the rooms (DAT_80475ED8)
  if (Globs_pSimulator) Globs_pSimulator->Simulate();
  for (int i = 0; i < 2; ++i)
    if (CameraDirector* d = _globals.CameraDirectorOf(i)) d->Update();
  if (m_introCamera) UpdateIntroCamera();
  // GC: after the cameras, the rooms' statistics when something changed them.
  if (Globs_pRoomManager) Globs_pRoomManager->UpdateRooms();
  if (_globals.m_house && !m_paused) {
    // InteractorManager::Update (PC: the direct-control interactors), then CameraManager::Update
    // moves each player's camera (ESimsCam::Update).
    const int count = _gd.m_fieldEC ? 2 : 1;
    UpdateDirectInteractors(count, _dt);
    for (int i = 0; i < count; ++i)
      if (players[i].m_cam) players[i].m_cam->Update();
  }
  if (_gd.m_field9C) {
    if (Globs_pSimulator) Globs_pSimulator->RestoreTrueDt();
    return;
  }
  const double speed = !Globs_pSimulator ? 1.0 : Globs_pSimulator->IsPausedAtAll() ? 0.0 : Globs_pSimulator->GetSpeedMultiplier();
  _gamedt = m_paused ? 0.0f : static_cast<float>(speed * _dt) * (_gd.m_fieldEC ? 0.5f : 1.0f);
  if (_globals.m_house) {
    const int count = _gd.m_fieldEC ? 2 : 1;
    for (int i = 0; i < count; ++i) {
      ESimsCam* cam = m_paused ? nullptr : players[i].m_cam;
      if (cam) {
        _globals.SetCameraDirector(_globals.CameraDirectorOf(i));
        _globals.SetCam(cam);
      }
    }
    _globals.m_house->Update();
  }
  if (Globs_pSimulator) Globs_pSimulator->RestoreTrueDt();
  // A house change or save asked for (cXObjectImpl::TransitionToNewHouse: DAT_8047621C / DAT_80476224):
  // the HUD goes, the lot and the family are saved; for a change the live mode starts leaving (the
  // loading screen), else the simulator stops here.
  if (_globals.m_field450 || _globals.m_field458) {
    delete _gd.m_hudTarget;  // DAT_80475BC8 (vtable 0xC, 3)
    _gd.m_hudTarget = nullptr;
    if (Globs_pNeighborhood) {
      Globs_pNeighborhood->GetImpl()->SaveHouse(Globs_pNghResFile);
      if (FamilyImpl* f = Globs_pNeighborhood->GetFamily(_gd.m_fieldD8)) f->SaveFamily(Globs_pNghResFile, Globs_iSaveFileVersion);
    }
    if (_globals.m_house) s_leftHouse = _globals.m_house->m_house;  // DAT_80476228
    if (_globals.m_field450) {
      _globals.m_field450 = 0;
      m_leaving = 1;
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{0});
    } else {
      if (Globs_pSimulator) {
        Globs_pSimulator->SetMode(cSimulator__Mode{0});
        Globs_pSimulator->Pause();
      }
      m_field28 = 1;
    }
  }
}

// 8008EA14
// Once loaded and shown: the players' views; "press start" before a story house starts, or the sky
// colour from the sun.
void ELiveMode::Draw(ERC* rc) {
  if (m_dontDraw || !m_initialized) return;
  DrawMain(rc);
  if (m_storyStart == 1) {
    SIMMY_STUB("ELiveMode::Draw: the story mode \"press start\" prompt", 0x8008EA14);
  } else if (!m_leaving) {
    if (_globals.m_house && _globals.m_house->m_level) {
      m_backgroundSet = 1;
      EVec3 ambient;
      EDirLight sun;
      _globals.m_house->m_level->GetSunLight(ambient, sun);
      EGlobal::SetBackgroundColor(ambient.x * 0.9f + sun.m_color.x * 0.100000024f, ambient.y * 0.9f + sun.m_color.y * 0.100000024f,
                                  ambient.z * 0.9f + sun.m_color.z * 0.100000024f);
    }
  } else {
    _pGfx->Flush();
  }
}

namespace {
// 8008BA1C
// The player's bloom and motion blur over its view (depth of field ran inside the level's draw).
// PC: SIMMY_FX_FRAME forces effects on (frameeffects.cpp).
void RenderPostProcessEffects(int player, ERC* rc) {
  FrameEffectsManager* fx = _globals.CameraDirectorOf(player) ? _globals.CameraDirectorOf(player)->GetFrameEffectsManager() : nullptr;
  if (!fx) return;
  simmy_force_frame_effects(fx);
  Bloom* bloom = fx->BloomObject();
  MotionBlur* blur = fx->MotionBlurObject();
  if (bloom) bloom->Update(_dt, rc);
  if (blur) blur->Update(_dt, rc);
}
}  // namespace

namespace {
// PC keyboard and mouse (pc_kbm.cpp): a player's 3D view as drawn, for the mouse's floor point.
void PcNoteView(int player, E3DWindow& win) {
  const float eye[3] = {win.m_lookPos.x, win.m_lookPos.y, win.m_lookPos.z};
  const float dir[3] = {win.m_lookDir.x, win.m_lookDir.y, win.m_lookDir.z};
  const float vp[4] = {win.m_viewportOut.x0, win.m_viewportOut.y0, win.m_viewportOut.x1, win.m_viewportOut.y1};
  simmy::kbm_note_view(player, win.m_lookAtProj.m, eye, dir, vp);
}
}  // namespace

// 8008F7AC
// Each player's view (both side by side in two-player mode unless one is full screen, or only the
// build/buy player's): the window from its camera, then the house.
void ELiveMode::DrawMain(ERC* rc) {
  int views = _gd.m_fieldEC ? 2 : 1;
  Player* players = _gd.m_players;
  if (players[0].m_cam && players[0].m_cam->m_fullScreen) views = 1;
  const bool single = views != 2;
  const int buildBuy = _gd.GetBuildBuyPlayer();
  // PC: the frame is stretched over the window while the halves are drawn (platform.hpp), which sets
  // the aspect GetScreenAspect gives.
  simmy::split_screen_frame(!single && buildBuy == -1);
  for (int i = 0; i < views; ++i) {
    if (!single && buildBuy != -1 && i != buildBuy) continue;
    SIMMY_STUB("ELiveMode::DrawMain: EInstance::SetContext, plumb bob visibility", 0x8008F7AC);
    ESimsCam* cam = m_paused ? nullptr : players[i].m_cam;
    if (!cam) continue;
    E3DWindow& win = cam->m_win;
    _globals.SetCameraDirector(_globals.CameraDirectorOf(i));
    _globals.SetCam(cam);
    if (single || buildBuy != -1) {
      const float fov = cam->GetFov() * static_cast<float>(_pGfx->m_screenHeight) / static_cast<float>(_pGfx->m_screenWidth);
      win.SetProjection(fov, _pGfx->GetScreenAspect(), cam->GetNearPlane(), cam->GetFarPlane());
      win.SetFrustumType(1);
      cam->SetWinPos(win);
      TRect<float> full{0.0f, 0.0f, 1.0f, 1.0f};
      win.SetViewport(full);
      win.Select(rc);
      PcNoteView(i, win);
    } else {
      InitPlayerXWindow(rc, win, i);
    }
    g_livemode80475E98 = i;
    if (_globals.m_house) {
      if (InteractorModule__InteractorVisualizer* v = InteractorVisualizer()) {
        v->SetupDrawCallback(_globals.m_house, i);
        if (_gd.m_fieldEC && players[0].m_cam && players[0].m_cam->m_fullScreen) v->SetupDrawCallback(_globals.m_house, 1);
      }
      ESpriteRenderMan::Instance()->DrawIntoOrderTable();
      _globals.m_house->Draw(rc);
    }
    if (CameraDirector* d = _globals.m_cameraDirector) d->Draw(rc);  // the letterbox of this view's shot
    RenderPostProcessEffects(i, rc);
  }
  if (!single) {
    // PC: no divider when each half has its own window (split=windows); player 1's half is selected
    // as the divider would leave it.
    if (buildBuy == -1 && simmy::split_draws_divider()) DrawSplitScreenDivider(rc);
    else if (buildBuy == -1) InitPlayerXWindow(rc, players[0].m_cam->m_win, 0);
    players[0].m_cam->ForceFullScreenViewport();
    if (players[1].m_cam) players[1].m_cam->ForceFullScreenViewport();
  }
}

// 8008F470
// The bar between the two halves: player 1's window as a 640x480 orthographic view (eye 35 in front),
// a strip 24 wide down the middle in the live-mode shader (0x8e4c2d0b), no z test; then player 1's half
// again.
void ELiveMode::DrawSplitScreenDivider(ERC* rc) {
  _globals.SetCameraDirector(_globals.CameraDirectorOf(0));
  ESimsCam* cam = _gd.m_players[0].m_cam;
  _globals.SetCam(cam);
  E3DWindow& win = cam->m_win;
  win.SetOrthoProjection(0.0f, 640.0f, 480.0f, 0.0f, 1.0f, 1000.0f);
  EVec3 eye{0.0f, 0.0f, 35.0f}, target{0.0f, 0.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
  win.SetLookAt(eye, target, up);
  TRect<float> full{0.0f, 0.0f, 1.0f, 1.0f};
  win.SetViewport(full);
  win.Select(rc);
  EGEVert v[4];
  v[0].normal[2] = 0x7f;
  for (int& c : v[0].color) c = 0x7f;
  v[0].uv2[1] = 1.0f;
  v[1] = v[2] = v[3] = v[0];
  const float x0 = 308.0f, x1 = 332.0f;
  v[0].pos[0] = x0, v[0].pos[1] = 0.0f, v[0].pos[2] = 10.0f;
  v[1].pos[0] = x1, v[1].pos[1] = 0.0f, v[1].pos[2] = 10.0f;
  v[2].pos[0] = x0, v[2].pos[1] = 480.0f, v[2].pos[2] = 10.0f;
  v[3].pos[0] = x1, v[3].pos[1] = 480.0f, v[3].pos[2] = 10.0f;
  v[0].uv[0] = 0.0f, v[0].uv[1] = 0.0f;
  v[1].uv[0] = 0.0f, v[1].uv[1] = 1.0f;
  v[2].uv[0] = 1.0f, v[2].uv[1] = 0.0f;
  v[3].uv[0] = 1.0f, v[3].uv[1] = 1.0f;
  if (EShader* shader = m_shader ? m_shader->GetShader() : nullptr) shader->Select(rc, 0);
  rc->ModelMatrixId();
  rc->ZTest(false, 2, 0, 0);
  rc->Stencil(0, 0, 0);
  rc->TriStrip(v, 4);
  InitPlayerXWindow(rc, win, 0);
}

// 8008F1B8
// Player `player`'s half of the screen in two-player mode: z test on, the global shader, the camera's
// projection at half the frame's aspect, the left or right half as viewport and clip.
// (The GC "coop4x3" build keeps the whole aspect here, 8008F354, for 4:3 halves; on PC the 4x3 split
// mode gets them from an 8:3 frame instead, see pc_splitscreen.cpp.)
void ELiveMode::InitPlayerXWindow(ERC* rc, E3DWindow& win, int player) {
  rc->ZTest(true, 2, 0, 0);
  if (EShader* shader = _globals.m_globalShader ? _globals.m_globalShader->GetShader() : nullptr) shader->Select(rc, 0);
  _globals.SetCameraDirector(_globals.CameraDirectorOf(player));
  ESimsCam* cam = _gd.m_players[player].m_cam;
  _globals.SetCam(cam);
  simmy::split_screen_player(player);  // PC: split=windows projects each half at its window's aspect
  const float fov = _globals.GetCam()->GetFov() * static_cast<float>(_pGfx->m_screenHeight) /
                    static_cast<float>(_pGfx->m_screenWidth);
  win.SetProjection(fov, _pGfx->GetScreenAspect() * 0.5f, cam->GetNearPlane(), cam->GetFarPlane());
  cam->SetWinPos(win);
  TRect<float> half = player == 0 ? TRect<float>{0.0f, 0.0f, 0.5f, 1.0f} : TRect<float>{0.5f, 0.0f, 1.0f, 1.0f};
  win.SetViewport(half);
  win.SetClip(half);
  win.Select(rc);
  PcNoteView(player, win);
}

// 80090090
void ELiveMode::SetInitialized(bool initialized) { m_initialized = initialized ? 1 : 0; }
// 80090098
int ELiveMode::IsInitialized() { return m_initialized; }
// 800900A0
void ELiveMode::SetDontDraw(bool dontDraw) { m_dontDraw = dontDraw ? 1 : 0; }
// 8008EF14
void ELiveMode::PauseAllSounds() {}
// 8008EF18
void ELiveMode::ResumeAllSounds() {}
// 80090038
RET_UNKNOWN ELiveMode::InTransportMode() { return 0; }

// InLevelBaseState: inlevel.cpp.

// ---------------------------------------------------------------------------------------------------
// LiveModeInitState

// 8008FEDC
LiveModeInitState::LiveModeInitState(int id) : StateMachineState(id) {}

// 8008BC24
void LiveModeInitState::Shutdown() {}

// 8008BC28
void LiveModeInitState::Reset() {
  m_stage = 0;
  m_dataset = 0;
  // The UI's pad filters work again in the game (AptViewer +0x3A8: PushAptButtonFilter).
  if (auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer)) viewer->m_field3A8 = 1;
  SIMMY_STUB("LiveModeInitState::Reset: GameData stage timing", 0x8008BC28);
  SoundFadeOutAndSetMode(cSoundPlayer::kLoading);
}

// 8008C9BC
void LiveModeInitState::Draw(ERC*) {}

// 8008CA18
void LiveModeInitState::DatasetFinishAddRefAsync() {
  if (m_dataset && _datasetman.GetRef(m_dataset)) _datasetman.DelRef(m_dataset, EResourceManager__DelRefMode{1});
  m_dataset = 0;
}

// 8008BD2C
// The house's load, one stage per frame (many wait on the background thread): the preload datasets,
// the house and its level (background), its contents, the house built (background), the cameras,
// then the simulation started and back to the live mode.
void LiveModeInitState::Update(float) {
  EShader::UpdateAll(_dt);
  if (_gd.m_field9C) {
    if (!BGDone()) return;
    if (m_dataset) DatasetFinishAddRefAsync();
    OwnerReturnFromState(0);
    return;
  }
  ELiveMode* live = s_liveMode;
  Player* players = _gd.m_players;
  const int stage = m_stage;
  bool advance = true;
  switch (stage) {
    case 0:
      live->m_paused = 1;
      if (Globs_pSound) Globs_pSound->SetGameMode(cSoundPlayer::kLoading);
      g_liveModeFirstLoad = g_pcChangeHouseLoad ? 0 : 1;  // DAT_80475F28 (PC: 0 when it loads the next house)
      g_pcChangeHouseLoad = 0;
      if (!LoadingScreenStateMachine::LoadInProgress()) {
        advance = false;
        break;
      }
      gbUpdateUI = 0;
      for (int i = 0; i < 2; ++i)
        if (CameraDirector* d = _globals.CameraDirectorOf(i)) d->Reset();
      break;
    case 1:
      SIMMY_STUB("LiveModeInitState::Update: ObjectFolder and RoomManager reset, heap compaction", 0x8008BD2C);
      m_stage = 2;
      [[fallthrough]];
    case 2:
      SIMMY_STUB("LiveModeInitState::Update: _pResLoader idle check and flush", 0x8008BD2C);
      m_dataset = LoadHousePreloadDataset("");
      if (Globs_pSound) Globs_pSound->QuietAll();
      break;
    case 3:
      if (!_datasetman.GetRef(m_dataset)) {
        advance = false;
        break;
      }
      _datasetman.DelRef(m_dataset, EResourceManager__DelRefMode{1});
      m_dataset = 0;
      if (Globs_pSound) { Globs_pSound->QuietAll(); Globs_pSound->EnableSound(false); }
      LoadLevelGlobalData();
      _globals.m_house = nullptr;
      SIMMY_STUB("LiveModeInitState::Update: NghResFile::SetCurrentHouse", 0x8008BD2C);
      m_stage = 4;
      [[fallthrough]];
    case 4:
      if (!BGDone()) {
        advance = false;
        break;
      }
      BGCall_SetCurHouse();
      break;
    case 5:
      if (!BGDone()) advance = false;
      break;
    case 6:
      SIMMY_STUB("LiveModeInitState::Update: MUStateMachine::MURequest(2) (the memory unit)", 0x8008BD2C);
      break;
    case 7:
      break;  // GC: MUStateMachine::MUPollForResult(2)
    case 8:
      m_dataset = LoadHousePreloadDataset("b");
      break;
    case 9:
      if (!BGDone() || !_datasetman.GetRef(m_dataset)) {
        advance = false;
        break;
      }
      _datasetman.DelRef(m_dataset, EResourceManager__DelRefMode{1});
      m_dataset = 0;
      BGCall_LoadHouse();
      break;
    case 10:
      if (!BGDone()) {
        advance = false;
        break;
      }
      m_dataset = LoadHousePreloadDataset("c");
      break;
    case 11:
      if (!_datasetman.GetRef(m_dataset)) {
        advance = false;
        break;
      }
      _datasetman.DelRef(m_dataset, EResourceManager__DelRefMode{1});
      m_dataset = 0;
      m_stage = 12;
      [[fallthrough]];
    case 12:
      // The player family lives in the story house now (GC: the house's number, below 13).
      if (_globals.m_house && _globals.m_house->m_house < 0xd && Globs_pNeighborhood)
        Globs_pNeighborhood->GetImpl()->SetPlayerFamilyInHouse(_globals.m_house->m_house);
      break;
    case 13:
      SIMMY_STUB("LiveModeInitState::Update: ObjectModule, the selected person's position, wants and fears", 0x8008BD2C);
      break;
    case 14:
      // The simulator runs: mode 1 (4 for a saved game being continued), resumed, global 0x18 cleared.
      if (Globs_pSimulator) {
        Globs_pSimulator->SetMode(_gd.m_fieldF0 ? 4 : 1);
        Globs_pSimulator->Resume();
        Globs_pSimulator->SetGlobal(0x18, 0);
      }
      m_stage = 15;
      [[fallthrough]];
    case 15:
      if (!BGDone()) {
        advance = false;
        break;
      }
      BGCall_CurHouseInit();
      break;
    case 16:
    case 17:
    case 18:
      if (!BGDone()) {
        advance = false;
        break;
      }
      for (int i = 0; i < 2; ++i)
        if (players[i].m_cam) players[i].m_cam->UpdateWin();
      gbUpdateUI = 1;
      live->m_paused = 0;
      m_stage = 18;
      break;
    case 19:
    case 23:
      if (!BGDone()) advance = false;
      break;
    case 20:
      if (_gd.m_fieldEC && players[1].m_cam) players[1].m_cam->CenterOnSelectedSim();
      break;
    case 21:
      live->m_field14 = 1200.0f;
      live->m_field18 = 0.0f;
      _globals.m_field458 = 0;  // DAT_80476224
      _globals.m_field450 = 0;  // DAT_8047621C (change house requested)
      break;
    case 22:
      if (!BGDone()) {
        advance = false;
        break;
      }
      SIMMY_STUB("LiveModeInitState::Update: BGCall_FlushResQueue", 0x8008BD2C);
      break;
    case 24:
      SIMMY_STUB("LiveModeInitState::Update: the selected sim's representation shaders (ESim::UpdateRepShaders)",
                 0x8008BD2C);
      break;
    case 25:
    case 26:
      if (_globals.m_house) _globals.m_house->Update();
      break;
    case 27:
      if (s_enteringHouse) {
        s_enteringHouse = 0;
        live->m_storyStart = 1;
        live->m_field20 = 0;
        SIMMY_STUB("LiveModeInitState::Update: the simulator paused, vibration stopped", 0x8008BD2C);
      }
      live->m_leaving = 0;
      live->m_field28 = 0;
      if (!_gd.m_fieldE8) {
        if (!live->m_introCamera) {
          live->TriggerIntroCamera(_globals.CameraDirectorOf(0));
          if (_gd.m_fieldEC) live->TriggerIntroCamera(_globals.CameraDirectorOf(1));
        }
      } else {
        for (int i = 0; i < 2; ++i)
          if (players[i].m_cam) players[i].m_cam->Reset();
        live->m_dontDraw = 0;
        gbDrawUI = 1;
        gbUpdateUI = 1;
        LoadingScreenStateMachine::LoadFinishing();
      }
      if (Globs_pHouse) Globs_pHouse->EnterLiveMode();  // vtable 0x6C
      m_stage = 28;
      [[fallthrough]];
    case 28:
      if (_globals.m_house) {
        if (!BGDone()) {
          advance = false;
          break;
        }
        if (Globs_pSound) { Globs_pSound->EnableSound(true); BGCall_SetGameModeAndResumeSounds(); }
      }
      break;
    case 29:
      if (!BGDone()) {
        advance = false;
        break;
      }
      SIMMY_STUB("LiveModeInitState::Update: plumb bobs, interactors, other houses flushed", 0x8008BD2C);
      SAnimator2::InitStaticAnimationElements();
      if (!_gd.m_hudTarget) _gd.m_hudTarget = new HUDTarget;  // GC stage 0x18 (DAT_80475BC8)
      // The cameras start at the tuning's position (GC: ResetPos via the intro camera or ESimsCam::Reset).
      for (int i = 0; i < 2; ++i)
        if (players[i].m_cam) players[i].m_cam->Reset();
      live->m_initialized = 1;
      _gd.m_fieldC0 = 1;  // DAT_80475C44
      g_liveModeFirstLoad = 0;
      OwnerReturnFromState(0);
      break;
    default:
      return;
  }
  if (advance) ++m_stage;
}



// 8003FC60
// The house's lawn shader: the "HouseData" row of the UI strings.
unsigned int EGlobal::GetHouseGrassShaderId(const char* name) {
  if (!name || !m_strings) return 0;
  const void* table = m_strings->getTable(const_cast<char*>("HouseData"));
  const void* row = table ? m_strings->getRow(table, const_cast<char*>(name)) : nullptr;
  return row ? ERQuickdata::U32(row, 0x28) : 0;
}

// PC: DAT_80475C48 for the sound director (game/boxx.cpp).
ELiveMode* LiveModeForSound() { return s_liveMode; }
// PC: changehouse.cpp's access to DAT_8047622C, DAT_80476230 and DAT_80475C54/58.
void LiveModeSetChangingHouse(int on) { g_livemode8047622C = on; }
void LiveModeSetNextHouse(int house) { s_houseNumber = house; }
void LiveModeSetPlayerNeighbor(int player, int id) { s_playerNeighbor[player & 1] = id; }
// PC: DAT_80475F24 and DAT_80475C54 / DAT_80475C58 for the in-game CAS (game/ingamecas.cpp).
int& LiveModeFamilyJustMovedIn() { return s_familyJustMovedIn; }
int* LiveModePlayerNeighbors() { return s_playerNeighbor; }
