// Port of game/siminteractor.cpp (FadeSquare, InteractorModule::SimInteractor: the classic cursor
// control), the interactor base's selection radius and highlights (game/interactor.cpp), and the parts of
// the interactor manager that switch a player between direct and classic control
// (InteractorModule::InitializeInteractorModule's restore, ChangeActiveInteractor,
// UpdateActivePlayerInteractors for the classic interactor) as PC stand-ins. See siminteractor.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/siminteractor.hpp"
#include "simmy/classes/buildtools.hpp"
#include "simmy/platform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

bool PlayerInSocialMode(int player);  // socialmode.cpp

namespace {
const unsigned int kSquareModel = 0x967660ea;  // FadeSquare's tile square

cXPersonImpl* PersonImpl(cXPerson* p) {
  cXObject* o = p ? p->AsObject() : nullptr;
  return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
}

// InteractorModule::IsCameraDirectorInControl (80213700, as in directinteractor.cpp)
bool IsCameraDirectorInControl(int player) {
  CameraDirector* d = _globals.CameraDirectorOf(player);
  return d && d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame;
}

// InteractorModule::IsPosOutsideWorld (8021109C, as in directinteractor.cpp)
bool IsPosOutsideWorld(const EVec3& p) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w) return true;
  return p.x < 1.0f || static_cast<float>(w->m_ySize) - 1.0f < p.x || p.y < 1.0f || static_cast<float>(w->m_xSize) - 1.0f < p.y;
}


// EIObjectMan::GetObjects / GetObjectsInRect (80054D08 / 80054CA0): the house's object instances, all or
// those the player highlights (cursor flag 1 for player 1, 8 for player 2). PC: read from the object
// manager's map here (iobjectman.cpp is the world side's).
std::vector<ISimInstance*> HouseInstances(EIObjectMan* man) {
  std::vector<ISimInstance*> out;
  if (!man) return out;
  for (auto& item : man->m_items)
    if (item.second) out.push_back(item.second);
  return out;
}
unsigned int HighlightFlag(int player) { return player == 0 ? ISimInstance::kHighlight : ISimInstance::kHighlight2; }

// ---- the PC interactor manager's state ----
struct PlayerInteractors {
  int active = -1;    // InteractorManager +0x1C: the active type (-1 none)
  int pending = -2;   // +0x2C / +0x3C: the requested type (-2 nothing requested)
  InteractorModule__SimInteractor* sim = nullptr;
};
PlayerInteractors s_players[2];
// PC keyboard and mouse: the classic control led by the mouse, and this frame's floor point for it.
bool s_mouseClassic[2] = {};
bool s_mouseTargetSet[2] = {};
EVec3 s_mouseTarget[2];
InteractorModule__InteractorVisualizer* s_visualizer = nullptr;  // InteractorManager::s_pVisualizer
bool s_initialized = false;

// InteractorModule::InitializeInteractorModule (80210260), PC part: the visualizer, the classic
// interactors (CreatePlayerInteractorSets: OnCreate), and each player's control mode as it was left
// (EGlobal +0xB8: 1 classic). GC: PlumbBob::StartupAll is ELiveMode's (at the house's start).
void InitializeInteractorModule() {
  if (s_initialized) return;
  s_initialized = true;
  s_visualizer = new InteractorModule__InteractorVisualizer;
  s_visualizer->Initialize();
  PlumbBob::StartupAll();
  const int players = _gd.m_fieldEC ? 2 : 1;  // DAT_80475C70
  for (int p = 0; p < players; ++p) {
    PlayerInteractors& s = s_players[p];
    s.sim = new InteractorModule__SimInteractor;
    s.sim->OnCreate();
    ChangeActiveInteractor(p, _globals.m_usedSimInteractor[p] == 1 ? 1 : 0);
  }
}

void TurnOffHighlights(int player) {
  EHouse* house = _globals.m_house;
  for (ISimInstance* inst : HouseInstances(house ? house->m_objectMan : nullptr)) inst->SetHighlight(HighlightFlag(player), false);
}

// The input session's buttons (803FDC74: pad command 5 -> 0 (A), 0x11 -> 1 (Y), 0x30 -> 0x10 (Z),
// 0xD -> 0xE, 0xE -> 0xD) and sticks (803FDC9C: left -> 7, 8; C stick -> 0xB, 0xC), through the
// camera's pad filter, delivered before the update (InteractorInputManager::InstanceData::Update).
void FeedSimInteractor(InteractorModule__SimInteractor* in, int player) {
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(player)));
  if (!pad) return;
  ESimsCam* cam = _gd.m_players[player & 1].m_cam;
  const unsigned int filter = cam ? cam->m_filterId : 0u;
  in->OnCommandUpdate(7, static_cast<float>(pad->GetStick(0, 0, filter)));
  in->OnCommandUpdate(8, static_cast<float>(pad->GetStick(0, 1, filter)));
  in->OnCommandUpdate(0xb, static_cast<float>(pad->GetStick(1, 0, filter)));
  in->OnCommandUpdate(0xc, static_cast<float>(pad->GetStick(1, 1, filter)));
  const unsigned int pressed = pad->GetBtnPressedMask(filter), down = pad->GetBtnDownMask(filter),
                     released = pad->GetBtnReleasedMask(filter);
  static const struct {
    unsigned int pad;
    int command;
  } kButtons[] = {{5, 0}, {0x11, 1}, {0x30, 0x10}, {0xd, 0xe}, {0xe, 0xd}};
  for (const auto& b : kButtons) {
    if (pad->GetCmdResult(b.pad, pressed, down, false)) in->OnCommandPressed(b.command, 1.0f);
    else if (pad->GetCmdResult(b.pad, released, released, false)) in->OnCommandReleased(b.command);
  }
}
}  // namespace

float InteractorModule__SimInteractor::s_invalidPos = -1000.0f;

// 802119EC
// One player; or the other player's interactor takes input: classic, direct or social mode (7).
bool InteractorModule::AllPlayersActiveInputInteractors(int player) {
  if (!_gd.m_fieldEC) return true;  // DAT_80475C70
  const int other = player == 1 ? 0 : 1;
  const int type = PlayerInteractorType(other);
  return type == 1 || type == 0 || PlayerInSocialMode(other);
}

// ---------------------------------------------------------------------------------------------------
// InteractorModule::Interactor (the base's virtuals the classic interactor uses)

// 802093F0
// Larger as the camera zooms out: 0.35 .. 1 tiles.
double InteractorModule__Interactor::GetSelectionRadius() {
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;  // CameraManager::GetCamera
  if (!cam) return 0.0;
  return cam->GetCurZoomRatio() * 0.65f + 0.35f;
}

// 80209454
void InteractorModule__Interactor::UpdateObjectHighlights(EIObjectMan* objects) {
  for (ISimInstance* inst : HouseInstances(objects)) inst->SetHighlight(HighlightFlag(m_player), false);
}

// ---------------------------------------------------------------------------------------------------
// FadeSquare

// 8021CE00
FadeSquare::FadeSquare() {
  m_model = static_cast<ERModel*>(_modelman.AddRef(kSquareModel, nullptr, 0));  // GC: AddRefAsync
  StopDraw();
}
// 8021CE4C
FadeSquare::~FadeSquare() {
  if (m_model) m_model->DelRef();
}
// 8021CEB0
void FadeSquare::StartDraw(EVec3& pos, bool fadeIn) {
  m_fadeIn = fadeIn ? 1 : 0;
  m_pos = pos;
  m_time = 0.5f;
  m_fading = 0;
}
// 8021CEE4
void FadeSquare::StartFade() { m_fading = 1; }
// 8021CEF0
void FadeSquare::StopDraw() {
  m_fading = 1;
  m_time = 0.0f;
}
// 8021CF08
// While fading the time runs down; a square fading in starts fading out (half a second) when it is in.
void FadeSquare::Update(float dt) {
  if (!m_model) m_model = static_cast<ERModel*>(_modelman.AddRef(kSquareModel, nullptr, 0));  // GC: GetRef
  if (!m_fading) return;
  m_time -= dt;
  if (m_fadeIn && m_time <= 0.0f) {
    m_fadeIn = 0;
    m_time += 0.5f;
  }
}
// 8021CFB0
bool FadeSquare::ShouldDraw() { return 0.0f < m_time; }

// 8021CFCC
// Lit in `color` (ambient: the colour times the plumb bob tuning's 0.3, and a light across the view),
// scaled by its time (fading in: from four times its size) and as transparent.
void FadeSquare::Draw(ERC* rc, EVec3& color) {
  if (!m_model) return;
  ELights lights;
  lights.m_numDir = 1;
  lights.m_numPoint = 0;
  if (ESimsCam* cam = _globals.GetCam()) {
    float d[3] = {-(cam->m_target.y - cam->m_eye.y), cam->m_target.x - cam->m_eye.x, cam->m_target.z - cam->m_eye.z};
    const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l != 0.0f) d[0] /= l, d[1] /= l, d[2] /= l;
    for (int i = 0; i < 3; ++i) lights.m_dir[0].m_dir[i] = d[i];
  }
  const float k = g_plumbBobParms.m_field24;  // DAT_804855C4
  lights.m_dir[0].m_color[0] = color.x;
  lights.m_dir[0].m_color[1] = color.y;
  lights.m_dir[0].m_color[2] = color.z;
  lights.m_ambient[0] = color.x * k;
  lights.m_ambient[1] = color.y * k;
  lights.m_ambient[2] = color.z * k;
  EMat4 m;
  m.Id();
  m.PostTranslate(m_pos);
  m.PreScale(m_model->m_scale * (m_fadeIn ? m_time * 4.0f + 1.0f : m_time + 0.5f));
  rc->Lights(&lights);
  rc->ModelMatrices(&m, 1);
  rc->SetAlpha(m_fadeIn ? 0.5f - m_time : m_time);
  m_model->Draw(rc);
}

// ---------------------------------------------------------------------------------------------------
// InteractorModule::SimInteractor

// 8021D250
InteractorModule__SimInteractor::InteractorModule__SimInteractor() { m_shadowSave = ~0u; }
// 8021D364
InteractorModule__SimInteractor::~InteractorModule__SimInteractor() {}
// 8021D3F4
void InteractorModule__SimInteractor::OnCreate() {
  if (InteractorModule__InteractorVisualizer* v = InteractorVisualizer()) v->CreateResources(*this);
}
// 8021D428
void InteractorModule__SimInteractor::OnDestroy() {
  if (InteractorModule__InteractorVisualizer* v = InteractorVisualizer()) v->DestroyResources(*this);
}

// 8021D45C
// For `player`: the selected person leaves social mode (person data 0x28 cleared), the input session
// opens (PC: FeedSimInteractor), the camera follows the cursor, which waits for the sim's position; the
// squares are cleared and the beam grows in again.
void InteractorModule__SimInteractor::OnStart(int player) {
  m_player = player;
  if (cXPersonImpl* p = PersonImpl(_globals.GetSelectedPerson(player))) {
    p->SetFirstPlayerInSocialMode(false);
    p->SetPersonData(0x28, 0);
  }
  SetupCamera();
  m_pos = EVec3{s_invalidPos, s_invalidPos, s_invalidPos};
  ResetInputState();
  m_plumbBob = static_cast<unsigned int>(player) < 2 ? PlumbBob::s_pPlumbBob[player] : nullptr;
  m_cam = nullptr;
  m_state = 0;
  m_field218[0] = m_field218[1] = m_field218[2] = 0;
  m_cutToSim = 0;
  m_snapped = 0;
  m_particlesOn = 0;
  m_overlapId = 0;
  m_field228 = 0;
  for (FadeSquare& sq : m_squares) sq.StopDraw();
  m_square = 0;
  m_snapTime = 0.0f;
}

// 8021D6DC
// The player last used the classic control (EGlobal +0xB8); the session closes, the bob flies home.
void InteractorModule__SimInteractor::OnStop() {
  _globals.m_usedSimInteractor[m_player & 1] = 1;
  m_plumbBob = static_cast<unsigned int>(m_player) < 2 ? PlumbBob::s_pPlumbBob[m_player] : nullptr;
  if (m_plumbBob) m_plumbBob->SetState(3);
}

// 8021D764
void InteractorModule__SimInteractor::ParseControls() {
  const float neutral = 0.0f;  // 80473560
  m_leftNeutral = 0;
  m_rightNeutral = 0;
  if (std::fabs(m_leftStick.x) < neutral && std::fabs(m_leftStick.y) < neutral) m_leftNeutral = 1;
  if (std::fabs(m_rightStick.x) < neutral && std::fabs(m_rightStick.y) < neutral) m_rightNeutral = 1;
}

// 8021D7D4
// The cursor: placed on the sim when the interactor starts (with a square fading in there); the left
// stick moves it (at the camera's cursor speed, along the camera's view, not off the lot). Each tile it
// enters gets a square and the last one fades; the plumb bob stays over the sim. A opens the action menu
// for what is highlighted (state 2 until the stick moves or A is let go); Y brings cursor and camera back
// to the sim. The plumb bob is updated after.
void InteractorModule__SimInteractor::Update(float dt) {
  InteractorModule__Interactor::Update(dt);
  ParseControls();
  if (m_snapTime < 1000.0f) m_snapTime += _dt;
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  if (!person) return;
  cXPersonImpl* impl = PersonImpl(person);
  if (impl && impl->IsWaitingForSocialMode() == 1) impl->CheckSecondPlayerForFailedSocialModeEntry();
  EVec3 sim{0.0f, 0.0f, 0.0f};
  InteractorModule::GetPersonPosition(&sim, person, false, true);
  auto invalid = [this] { return m_pos.x == s_invalidPos && m_pos.y == s_invalidPos && m_pos.z == s_invalidPos; };
  // GC: also only once the world says so (cFixedWorld vtable 0x6C is 0).
  if (!m_particlesOn && m_plumbBob) {
    m_plumbBob->SetParticleDrawState(true);
    m_particlesOn = 1;
    if (invalid()) {
      m_pos = EVec3{sim.x, sim.y, 0.0f};
      EVec3 c{static_cast<float>(static_cast<int>(m_pos.x)) + 0.5f, static_cast<float>(static_cast<int>(m_pos.y)) + 0.5f, 0.05f};
      m_squares[m_square].StartDraw(c, true);
      m_squares[m_square].StartFade();
    }
  }
  if (invalid()) {
    m_pos = EVec3{1.0f, 1.0f, 0.0f};
    EVec3 c{1.5f, 1.5f, 0.05f};
    m_squares[m_square].StartDraw(c, true);
    m_squares[m_square].StartFade();
  }
  if (m_plumbBob) m_plumbBob->m_updates = 0;
  for (FadeSquare& sq : m_squares) sq.Update(_dt);
  if (m_cam && !IsCameraDirectorInControl(m_player)) {
    if (m_restorePos == 0) {
      EVec3 mouse;
      if (PcTakeMouseCursor(m_player, mouse)) {
        // PC: the mouse puts the cursor on the floor point under it (kept on the lot, as the stick).
        if (cFixedWorldImpl* w = Globs_pFixedWorld) {
          const float maxX = static_cast<float>(w->m_ySize) - 1.001f, maxY = static_cast<float>(w->m_xSize) - 1.001f;
          m_pos.x = std::min(std::max(mouse.x, 1.0f), maxX);
          m_pos.y = std::min(std::max(mouse.y, 1.0f), maxY);
        }
      } else if (m_leftNeutral == 0) {
        EVec3 p = m_pos;
        ibuild::MoveCursorAlongCameraTargetVector(static_cast<float>(m_cam->GetCursorSpeed()), p, m_leftStick, m_cam);
        if (!IsPosOutsideWorld(p)) m_pos = p;
        else m_leftStick = EVec2{0.0f, 0.0f};
      }
      if (m_state == 1) {
        const float dx = sim.x - m_pos.x, dy = sim.y - m_pos.y;
        if (dx * dx + dy * dy <= 0.0625f) m_overlapCentre.z = sim.z;
        const float follow = 0.0f;  // DAT_80473524 (never set: the cursor keeps its height)
        m_pos.z = (m_overlapCentre.z - m_pos.z) * follow + m_pos.z;
        if (m_plumbBob) {
          m_plumbBob->SetState(3);
          m_plumbBob->m_target = sim;
        }
        if (static_cast<int>(m_prevPos.x) != static_cast<int>(m_pos.x) || static_cast<int>(m_prevPos.y) != static_cast<int>(m_pos.y)) {
          m_squares[m_square].StartFade();
          EVec3 c{static_cast<float>(static_cast<int>(m_pos.x)) + 0.5f, static_cast<float>(static_cast<int>(m_pos.y)) + 0.5f, 0.05f};
          for (FadeSquare& sq : m_squares)
            if (sq.m_pos.x == c.x && sq.m_pos.y == c.y && sq.m_pos.z == c.z) sq.StopDraw();
          m_square = (m_square + 1) % 10;
          m_squares[m_square].StartDraw(c, false);
        }
        if (m_actionMenu) {
          m_state = 2;
          ExecuteActionMenu();
          m_actionMenu = 0;
        }
      } else {
        // 0: the first update; 2: the menu was opened, until the stick moves (or A is let go).
        int v = m_state;
        if (1 < v) v = v == 2 ? m_leftNeutral : -1;
        if (v == 0) m_state = 1;
      }
    } else {
      m_restorePos = 0;
      m_pos = m_savedPos;
    }
    if (m_cutToSim) {
      m_cutToSim = 0;
      m_snapTimer = 0.0f;
      ImmediatelyCutCursorAndCameraToPos(sim);
    }
  }
  if (IsCameraDirectorInControl(m_player)) {
    if (!m_plumbBob) return;
    ReturnPlumbobToSim(m_plumbBob, sim);
  }
  if (m_plumbBob) {
    m_plumbBob->Update(m_player);
    if (m_plumbBob->m_model) m_plumbBob->m_model->GetExtents(m_extents);
    if (!(m_plumbBob->m_flags & 0x10)) {
      if (!IsSimulatorPaused()) m_plumbBob->SetParticleVisibility(true);
    } else if (IsSimulatorPaused()) {
      m_plumbBob->SetParticleVisibility(false);
    }
  }
}

// 8021DF90
// The beam grows in over a second, half a second after the start (0.01 .. 1).
double InteractorModule__SimInteractor::GetBeamScale() {
  float s = m_snapTime - 0.5f;
  if (1.0f <= s) s = 1.0f;
  if (s <= 0.01f) s = 0.01f;
  return s;
}

// 8021DFF4
void InteractorModule__SimInteractor::GetInteractorInfo(InteractorModule__InteractorInfo& info) {
  info.m_x = m_pos.x;
  info.m_y = m_pos.y;
  info.m_w = m_extents.x;
  info.m_h = m_extents.y;
}

// 8021E018
void InteractorModule__SimInteractor::PreDraw(InteractorModule__InteractorVisualizer& v) { v.PreDraw(*this); }
// 8021E044
void InteractorModule__SimInteractor::Draw(InteractorModule__InteractorVisualizer& v) { v.Draw(*this); }

// 8021E070
// A asks for the action menu; Y for the cursor back at the sim; Z (unless the pause menu is up) goes
// back to direct control, closing the HUD's menu and cancelling the sim's queued actions (all but the
// last while it sleeps). 0xD / 0xE pick the next / previous sim of the family and put the cursor on it.
// While the sim is in social mode, A, Z and the sim switches are ignored.
void InteractorModule__SimInteractor::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand command, float) {
  const int cmd = command;
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  cXPersonImpl* impl = PersonImpl(person);
  if (impl && (impl->IsInSocialMode() || impl->IsSecondPlayerInSocialMode()) && (cmd == 0 || cmd == 0x10 || cmd == 0xd || cmd == 0xe))
    return;
  switch (cmd) {
    case 0:
      m_actionMenu = 1;
      break;
    case 1:
      m_cutToSim = 1;
      break;
    case 0x10:
      if (_gd.m_field70) break;  // DAT_80475BF4: the pause menu (PAZTarget)
      PcSetMouseClassic(m_player, false);
      ChangeActiveInteractor(m_player, 0);
      if (HUDTarget* hud = _gd.m_hudTarget) hud->CloseMenu(m_player);
      if (impl) {
        if (!impl->IsSleeping()) impl->CancelAllActions();
        else impl->CancelAllButLastActions();
      }
      std::printf("[ctl] player %d: direct control\n", m_player + 1);
      break;
    case 0xd:
    case 0xe: {
      CameraDirector* d = _globals.CameraDirectorOf(m_player);  // DAT_80475EE4
      if (d && d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame) {
        if (d->m_cancelMode == 0) return;
        d->ReleaseCurrentCamera();
      }
      if (!_gd.m_hudTarget) return;
      if (cmd == 0xe) _globals.ReverseSelectedPerson(m_player);
      else _globals.AdvanceSelectedPerson(m_player);
      if (cXPerson* next = _globals.GetSelectedPerson(m_player)) {
        const CTilePt t = next->AsObject() ? next->AsObject()->GetCTilePt() : CTilePt();
        // The person's location (vtable 0x344) in tiles: SnapToPos (vtable 0xC4).
        EVec3 p{static_cast<float>(t.GetY()), static_cast<float>(t.GetX()), 0.0f};
        m_pos = p;
        InteractorModule::CheckNewSimInSocialMode(static_cast<unsigned int>(m_player));
      }
      break;
    }
    case 4:
      if (impl) impl->CancelLastAction();
      break;
    default:
      break;
  }
}

// 8021E4CC
// Letting go of A ends the menu state; of Y the cut request; the sticks come to rest.
void InteractorModule__SimInteractor::OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand command) {
  switch (static_cast<int>(command)) {
    case 0:
      m_actionMenu = 0;
      if (m_state == 2) m_state = 1;
      ResetSnapTimer();
      break;
    case 1: m_cutToSim = 0; break;
    case 7: m_leftStick.x = 0.0f; break;
    case 8: m_leftStick.y = 0.0f; break;
    case 0xb: m_rightStick.x = 0.0f; break;
    case 0xc: m_rightStick.y = 0.0f; break;
    default: break;
  }
}

// 8021E598
void InteractorModule__SimInteractor::OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command, float value) {
  switch (static_cast<int>(command)) {
    case 7: m_leftStick.x = value; break;
    case 8: m_leftStick.y = value; break;
    case 0xb: m_rightStick.x = value; break;
    case 0xc: m_rightStick.y = value; break;
    default: break;
  }
}

// 8021E5D8
void InteractorModule__SimInteractor::ResetInputState() {
  InteractorModule__Interactor::ResetInputState();
  m_cutToSim = 0;
  m_actionMenu = 0;
}

// 8021E610
// The bob gets its shadow back, bounces and spins again; the sim's data 0x24 is set to 0x32 and the
// animator's pause multiplier back to 1.
void InteractorModule__SimInteractor::ReturnPlumbBobToOwner() {
  if (!m_plumbBob) return;
  const unsigned int shadow = m_shadowSave;
  m_shadowSave = ~0u;
  m_plumbBob->SetShadow(shadow);
  if (m_plumbBob->m_model) {
    m_plumbBob->m_model->m_bounce = 1;
    m_plumbBob->m_model->m_spin = 1;
  }
  if (cXPersonImpl* p = PersonImpl(_globals.GetSelectedPerson(m_player))) {
    p->SetPersonData(0x24, 0x32);
    SIMMY_STUB("SimInteractor::ReturnPlumbBobToOwner: SAnimator2::SetPauseMultiplier(1)", 0x8021E610);
  }
}

// 8021E6E0
// The cursor there at once and the camera with it (ESimsCam::ForceTarget).
void InteractorModule__SimInteractor::ImmediatelyCutCursorAndCameraToPos(EVec3& pos) {
  m_cut = 1;
  m_pos = pos;
  if (m_cam) m_cam->ForceTarget(pos);
}

// 8021E72C
// The cursor jumps to the sim when the bob is within 3 tiles of it (the camera too), else the bob flies.
void InteractorModule__SimInteractor::TryCutToSim() {
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  if (!m_plumbBob || !person) return;
  EVec3 p;
  InteractorModule::GetPersonPosition(&p, person, true, false);
  const EVec3 d{p.x - m_plumbBob->m_pos.x, p.y - m_plumbBob->m_pos.y, p.z - m_plumbBob->m_pos.z};
  if (9.0f < d.x * d.x + d.y * d.y + d.z * d.z) {
    m_plumbBob->SetState(3);
    return;
  }
  m_plumbBob->m_target = p;
  m_plumbBob->m_pos = p;
  m_pos = p;
  if (ESimsCam* cam = _gd.m_players[m_player & 1].m_cam) cam->ForceTarget(m_pos);
  ReturnPlumbBobToOwner();
}

// 8021E890
void InteractorModule__SimInteractor::ResetSnapTimer() {
  m_snapped = 0;
  m_snapTimer = 0.0f;  // DAT_80473568
}

// 8021E8A8
// Nothing is selected before the cursor first moves (state 0).
double InteractorModule__SimInteractor::GetSelectionRadius() {
  const double r = InteractorModule__Interactor::GetSelectionRadius();
  return m_state == 0 ? 0.0 : r;
}

// 8021E8E8
// The nearest overlapping object (by its centre) this pass; null starts a pass. PC: the object manager
// does not report overlaps (UpdateObjectHighlights finds the objects), so this is not called.
void InteractorModule__SimInteractor::UpdateOverlapIntersection(InteractorModule__Interactor__OverlapData*) {
  m_prevOverlapCentre = m_overlapCentre;
  for (float& f : m_overlapRect) f = 0.0f;
  m_overlapCentre = EVec3{m_pos.x, m_pos.y, 0.0f};
  m_overlapDist = 3.4e38f;
}

// 8021EA98
// A: the player's pie menu for the highlighted objects (EIObjectMan::GetObjectsInRect), the one the sim
// holds first; none gives the "Go Here" menu at the cursor. The refuse sound when the HUD's menu is not
// available.
void InteractorModule__SimInteractor::ExecuteActionMenu() {
  HUDTarget* hud = _gd.m_hudTarget;
  if (!hud || !hud->IsMenuAvailable(m_player)) {
    UIAUDIO::Instance()->PlaySound(0x3804219f);
    m_actionMenu = 0;
    return;
  }
  EHouse* house = _globals.m_house;
  std::vector<std::pair<float, cXObjectImpl*>> lit;
  for (ISimInstance* inst : HouseInstances(house ? house->m_objectMan : nullptr)) {
    if (!(inst->m_cursFlags & HighlightFlag(m_player))) continue;
    cXObjectImpl* o = inst->m_xob ? inst->m_xob->GetObjectImplementation() : nullptr;
    if (!o) continue;
    const float cx = (inst->m_bounds[0] + inst->m_bounds[3]) * 0.5f - m_pos.x, cy = (inst->m_bounds[1] + inst->m_bounds[4]) * 0.5f - m_pos.y;
    lit.emplace_back(cx * cx + cy * cy, o);
  }
  // PC: nearest first, one part per multi-tile object, at most four (the action menu's object rows).
  std::sort(lit.begin(), lit.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<cXObjectImpl*> objects;
  std::vector<short> groups;
  for (const auto& l : lit) {
    const short group = l.second->m_def ? objdef::MasterID(l.second->m_def) : 0;
    if (group && std::find(groups.begin(), groups.end(), group) != groups.end()) continue;
    if (group) groups.push_back(group);
    objects.push_back(l.second);
    if (objects.size() >= 4) break;
  }
  if (!objects.empty())
    if (cXPersonImpl* p = PersonImpl(_globals.GetSelectedPerson(m_player)))
      if (cXObjectImpl* held = p->GetContainedObject(0)) {
        auto it = std::find(objects.begin(), objects.end(), held);
        if (it != objects.end()) {
          objects.erase(it);
          objects.insert(objects.begin(), held);
        }
      }
  std::printf("[ctl] player %d: action menu at %.2f, %.2f (%d objects)\n", m_player + 1, m_pos.x, m_pos.y, static_cast<int>(objects.size()));
  if (FlashPiMenu* pi = _gd.m_players[m_player & 1].m_piMenu)
    if (pi->CreateObjectMenuFromObjList(objects)) UIAUDIO::Instance()->PlaySound(0x374bdcea);
  ResetInputState();
}

// 8021ECB0
void InteractorModule__SimInteractor::ReturnPlumbobToSim(PlumbBob*, EVec3&) { m_state = 0; }

// 8021ECBC
// While the simulator's speed is 0, or the other player's interactor takes no input. PC: also while the
// pause menu holds the game (as DirectInteractor::IsSimulatorPaused).
int InteractorModule__SimInteractor::IsSimulatorPaused() {
  if (H2DTarget::m_nPauseRefCount != 0) return 1;
  if (Globs_pSimulator && Globs_pSimulator->GetSpeedMultiplier() == 0.0) return 1;
  return InteractorModule::AllPlayersActiveInputInteractors(m_player) ? 0 : 1;
}

// 8021ED2C
// The objects whose bounds meet the square of the selection radius about the cursor (from the floor to
// 10 up) light up in the player's colour when they have something to do (ISimInstance::HasInteractions);
// the selected sim itself only while its motive 0xB is negative.
void InteractorModule__SimInteractor::UpdateObjectHighlights(EIObjectMan* objects) {
  const unsigned int flag = HighlightFlag(m_player);
  cXPersonImpl* self = PersonImpl(_globals.GetSelectedPerson(m_player));
  for (ISimInstance* inst : HouseInstances(objects)) {
    if (self && inst == self->m_instance && self->GetMotive(0xb) >= 0) {
      inst->SetHighlight(flag, false);
      continue;
    }
    const float r = static_cast<float>(GetSelectionRadius());
    const float x0 = m_pos.x - r, x1 = m_pos.x + r, y0 = m_pos.y - r, y1 = m_pos.y + r, z0 = 0.0f, z1 = 10.0f;
    const float* b = inst->m_bounds;
    const bool overlaps = b[0] <= x1 && x0 <= b[3] && b[1] <= y1 && y0 <= b[4] && b[2] <= z1 && z0 <= b[5];
    inst->SetHighlight(flag, overlaps && inst->HasInteractions(m_player) != 0);
  }
}

// 8021EFC8
// The camera follows the cursor (mode 0).
void InteractorModule__SimInteractor::SetupCamera() {
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
  if (cam) cam->SetMode(ESimsCam__CameraMode{0});
}

// 8021F00C
// GC: the chosen interaction to its person's action queue (vtable 0xB4, AddAction). PC: FlashPiMenu's
// ChooseAction queues the menu's choice itself (StartMenuInteraction).
RET_UNKNOWN InteractorModule__SimInteractor::ChooseAction(Interaction*) { return 0; }
// 8021F0AC
void InteractorModule__SimInteractor::SetupCutToSim() { m_cutToSim = 1; }
// 8021F0B8
FadeSquare* InteractorModule__SimInteractor::GetFadeSquare(int i) { return &m_squares[i]; }
// 8021F0C8
void InteractorModule__SimInteractor::SetPlumbBobShadowSave(unsigned int id) {
  if (id != ~0u) m_shadowSave = id;
}
// 8021F0D8
unsigned int InteractorModule__SimInteractor::GetPlumbBobShadowSave() {
  const unsigned int id = m_shadowSave;
  m_shadowSave = ~0u;
  return id;
}

// ---------------------------------------------------------------------------------------------------
// PC interactor manager

int PlayerInteractorType(int player) { return s_players[player & 1].active; }

void ChangeActiveInteractor(int player, int type) {
  if (player < 0 || player > 1) return;
  s_players[player].pending = type;  // GC: refused while a change is pending (the request bit)
}

InteractorModule__SimInteractor* PlayerSimInteractor(int player) { return s_players[player & 1].sim; }
InteractorModule__InteractorVisualizer* InteractorVisualizer() { return s_visualizer; }

bool UpdateSimInteractor(int player, float dt) {
  if (player < 0 || player > 1) return false;
  if (!_globals.GetSelectedPerson(player) && !s_initialized) return false;
  InitializeInteractorModule();
  PlayerInteractors& s = s_players[player];
  if (!s.sim) return false;
  // InteractorManager::UpdateActivePlayerInteractors: a requested change stops the old interactor and
  // starts the new one (vtables 0x2C OnStop, 0x24 OnStart). The direct interactor starts itself when
  // UpdateDirectInteractors makes it.
  if (s.pending != -2) {
    const int type = s.pending;
    s.pending = -2;
    if (type != s.active) {
      if (s.active == 1) {
        s.sim->OnStop();
        TurnOffHighlights(player);  // PC: GC's direct interactor redoes the highlights (UpdateObjectHighlights)
      } else if (s.active == 0) {
        if (InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player)) d->OnStop();
      }
      s.active = type;
      if (type == 1) {
        s.sim->OnStart(player);
      } else if (type == 0) {
        if (InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player)) d->OnStart(player);
      }
      std::printf("[ctl] player %d: interactor %d\n", player + 1, type);
    }
  }
  if (s.active != 1) return false;
  if (PlayerInSocialMode(player)) return true;  // GC: the social-mode interactor (7) has the player
  InteractorModule__SimInteractor* in = s.sim;
  FeedSimInteractor(in, player);
  ESimsCam* cam = _gd.m_players[player & 1].m_cam;  // CameraManager::GetCamera
  s_mouseTargetSet[player] = false;
  if (player == KbmPlayer()) {
    // PC keyboard and mouse (pc_kbm.cpp): where the stick reaches the cursor, the mouse puts it on the
    // floor point under it; the stick (keys) hands a mouse-led classic control back to direct control.
    const simmy::KbmTick& t = simmy::kbm_tick();
    EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(player)));
    const bool free = pad && cam && !pad->IsStickFiltered(0, 0, cam->m_filterId);
    if (free && s_mouseClassic[player] && (t.flags & simmy::kKbmKeysMoved) && !(t.flags & simmy::kKbmCursor) && !_gd.m_field70) {
      s_mouseClassic[player] = false;
      in->m_leftStick = EVec2{0.0f, 0.0f};
      ChangeActiveInteractor(player, 0);
      if (HUDTarget* hud = _gd.m_hudTarget) hud->CloseMenu(player);
      std::printf("[ctl] player %d: direct control (keys)\n", player + 1);
    } else if (free && (t.flags & simmy::kKbmCursor)) {
      s_mouseClassic[player] = true;
      s_mouseTargetSet[player] = true;
      s_mouseTarget[player] = EVec3{t.cx, t.cy, 0.0f};
    }
  }
  in->AttachCamera(cam);
  in->Update(dt);
  if (EHouse* house = _globals.m_house) in->UpdateObjectHighlights(house->m_objectMan);
  // (InteractorManager then has the camera recompute the cursor area: UpdateDirectInteractors.)
  in->AttachCamera(nullptr);
  // PC dev trace: SIMMY_CTL_TRACE=1 prints the cursor, the plumb bob and the lit objects every 60 frames.
  static const bool trace = std::getenv("SIMMY_CTL_TRACE") != nullptr;
  if (trace && _framecount % 60 == 0) {
    int lit = 0;
    EHouse* house = _globals.m_house;
    for (ISimInstance* inst : HouseInstances(house ? house->m_objectMan : nullptr))
      if (inst->m_cursFlags & HighlightFlag(player)) ++lit;
    PlumbBob* bob = in->m_plumbBob;
    std::printf("[ctl] f%d p%d cursor %.2f %.2f state %d r %.2f lit %d bob %.2f %.2f %.2f model %p flags %x\n", _framecount,
                player + 1, in->m_pos.x, in->m_pos.y, in->m_state, in->GetSelectionRadius(), lit, bob ? bob->m_pos.x : 0.0f,
                bob ? bob->m_pos.y : 0.0f, bob ? bob->m_pos.z : 0.0f, bob && bob->m_model ? static_cast<void*>(bob->m_model->m_model) : nullptr,
                bob ? bob->m_flags : 0u);
  }
  return true;
}

bool SimInteractorInfo(int player, InteractorModule__InteractorInfo& info) {
  PlayerInteractors& s = s_players[player & 1];
  if (s.active != 1 || !s.sim) return false;
  s.sim->GetInteractorInfo(info);
  return true;
}

// ---- PC keyboard and mouse (pc_kbm.cpp) ----

int KbmPlayer() {
  if (!_pCtrlMan) return -1;
  const int players = _gd.m_fieldEC ? 2 : 1;  // DAT_80475C70
  for (int p = 0; p < players; ++p)
    if (_pCtrlMan->GetPlayerMapped(static_cast<unsigned int>(p)) && _pCtrlMan->GetPlayerControllerIndex(static_cast<unsigned int>(p)) == 0)
      return p;
  // Before the front end maps a pad (or a player 1 left unmapped): player 1's default, pad 1.
  return !_pCtrlMan->GetPlayerMapped(0) && _pCtrlMan->GetPlayerControllerIndex(0) == 0 ? 0 : -1;
}

bool PcMouseClassic(int player) { return s_mouseClassic[player & 1] && PlayerInteractorType(player) == 1; }
void PcSetMouseClassic(int player, bool on) { s_mouseClassic[player & 1] = on; }

bool PcTakeMouseCursor(int player, EVec3& pos) {
  if (!s_mouseTargetSet[player & 1]) return false;
  s_mouseTargetSet[player & 1] = false;
  pos = s_mouseTarget[player & 1];
  return true;
}

// What pc_kbm.cpp makes the frame's input from (read before the game's update; only the input it
// makes counts, and that is recorded with the pads).
void simmy::kbm_game_state(simmy::KbmGame& g) {
  g = simmy::KbmGame{};
  const int p = KbmPlayer();
  g.player = p;
  if (p < 0 || !_globals.m_house || !_gd.m_players || _gd.m_casTarget) return;
  ESimsCam* cam = _gd.m_players[p & 1].m_cam;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(static_cast<unsigned int>(p))));
  if (!cam || !pad || H2DTarget::m_nPauseRefCount != 0 || _gd.m_field70 || IsCameraDirectorInControl(p)) return;
  g.live = true;
  // The player's interaction menu open (the object list, its interactions): the wheel moves in it.
  HUDTarget* hud = _gd.m_hudTarget;
  const bool menuOpen = hud && hud->m_menus[p & 1].m_state > 1;
  g.cameraFree = !menuOpen && !pad->IsStickFiltered(1, 0, cam->m_filterId);
  InteractorModule__InteractorInfo info;
  const int type = PlayerInteractorType(p);
  const bool tool = PlaceManipulatorInfo(p, info);  // build / buy mode's cursors
  g.cursorFree = !menuOpen && !tool && (type == 0 || type == 1) && !PlayerInSocialMode(p) && _globals.GetSelectedPerson(p) &&
                 !pad->IsStickFiltered(0, 0, cam->m_filterId);
  const PlayerInteractors& s = s_players[p & 1];
  g.classicReady = type == 1 && s.pending == -2 && s.sim && s.sim->m_state == 1;
  g.mouseClassic = PcMouseClassic(p);
  g.cursorZ = 0.0f;
}

// PC keyboard and mouse: what the mouse points at (pc_kbm.cpp MouseToFloor).
bool simmy::kbm_pick(int player, const float o[3], const float d[3], float floorZ, float& x, float& y) {
  EHouse* house = _globals.m_house;
  static const bool off = std::getenv("SIMMY_KBM_NOPICK") != nullptr;  // tests: the floor only
  if (!house || d[2] >= 0.0f || off) return false;
  const float floorT = (floorZ - o[2]) / d[2];
  const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  std::vector<std::pair<float, ISimInstance*>> hits;
  for (ISimInstance* inst : HouseInstances(house->m_objectMan)) {
    if (!inst->m_xob) continue;
    const float* b = inst->m_bounds;
    if (!(b[3] > b[0] && b[4] > b[1] && b[5] > b[2]) || b[5] < floorZ + 0.05f) continue;
    // Not the big ones (trees: their box is mostly air over the lawn), only what fits about a 2x3-tile bed.
    if (b[3] - b[0] > 3.2f || b[4] - b[1] > 3.2f || b[5] - b[2] > 3.0f) continue;
    // The slab test against the box.
    float t0 = 0.0f, t1 = floorT;
    bool miss = false;
    for (int a = 0; a < 3 && !miss; ++a) {
      if (std::fabs(d[a]) < 1e-9f) {
        miss = o[a] < b[a] || o[a] > b[a + 3];
        continue;
      }
      float ta = (b[a] - o[a]) / d[a], tb = (b[a + 3] - o[a]) / d[a];
      if (ta > tb) std::swap(ta, tb);
      t0 = std::max(t0, ta), t1 = std::min(t1, tb);
      miss = t0 > t1;
    }
    // (A box the eye is in or right in front of it, cut by the near plane, is looked through.)
    if (!miss && t0 * len > 0.75f) hits.emplace_back(t0, inst);
  }
  std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  // (Only reads: input making must not change the game. ISimInstance::HasInteractions would refresh its
  // cache, drawing the UI's random numbers and running check trees, so it is not asked here; the
  // cursor's own highlight decides what the object offers.)
  cXPersonImpl* self = PersonImpl(_globals.GetSelectedPerson(player));
  for (const auto& h : hits) {
    if (self && h.second == self->m_instance) continue;  // the player's own sim does not hide the floor
    // A point inside the box's footprint under the ray: the entry point, nudged to the box's centre.
    const float* b = h.second->m_bounds;
    const float px = o[0] + d[0] * h.first, py = o[1] + d[1] * h.first;
    x = std::min(std::max(px, b[0] + 0.05f), b[3] - 0.05f);
    y = std::min(std::max(py, b[1] + 0.05f), b[4] - 0.05f);
    static const bool trace = std::getenv("SIMMY_KBM_TRACE") != nullptr;
    if (trace && _framecount % 30 == 0) {
      cXObjectImpl* o = h.second->m_xob ? h.second->m_xob->GetObjectImplementation() : nullptr;
      std::printf("[kbm] pick %s: box %.2f,%.2f,%.2f..%.2f,%.2f,%.2f at t %.2f -> %.2f, %.2f\n", o ? o->DebugName() : "?", b[0], b[1],
                  b[2], b[3], b[4], b[5], h.first, x, y);
    }
    return true;
  }
  return false;
}

void ShutdownSimInteractors() {
  for (int p = 0; p < 2; ++p) {
    PlayerInteractors& s = s_players[p];
    if (s.sim) {
      if (s.active == 1) s.sim->OnStop();
      s.sim->OnDestroy();
      delete s.sim;
    }
    s = PlayerInteractors{};
    s_mouseClassic[p] = false;
    s_mouseTargetSet[p] = false;
  }
  delete s_visualizer;
  s_visualizer = nullptr;
  PlumbBob::ShutdownAll();  // GC: ELiveMode's
  s_initialized = false;
}
