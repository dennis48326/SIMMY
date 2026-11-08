// Port of game/wallmanipulator.cpp: the wall and fence tool (InteractorModule::WallManipulator, the
// player's interactor 3; see buildtools.hpp). A anchors the cursor's tile corner, a second A builds the
// wall run from the anchor to the cursor along x, y or a diagonal (the destruct tool, the catalogue's
// sell tool: removes it); with the trigger held the selection is a room (four runs round a rectangle).
// A run is built tile by tile into the world (cFixedWorldImpl::SetWall), checked by
// ERoom::PreviewWallBuild (else the world's wall array goes back to a copy), then the rooms and the house
// are recomputed. The stock (what the funds pay for at the wall's price) limits a run; the catalogue's
// callback takes the money (walls bought) or gives it back (walls sold: the styles counted while
// deleting).
//
// PC: InteractorVisualizer (the cursor, the line's preview, the shader resource 0x9B0C648D for fences)
// is not ported; the want/fear events (Neighborhood vtable 0x1A4: kEventAddARoom), the routes'
// invalidation (ObjectModule vtable 0x184), the lot data (HouseImpl vtable 0x8C) and the global
// dispatches (0x91, 0xFC, 0xEC) belong to the world side and are left out (marked).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/buildtools.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

bool play_sound(unsigned int id);  // pcttarget.cpp (801D1DDC)
extern int g_freeBuildBuyCheat;    // DAT_80476188

namespace {
cFixedWorldImpl* World() { return Globs_pFixedWorld; }
bool IsFence(int style) { return style == 2 || style == 0xc || style == 0xd || style == 0xe; }
bool IsPortalStyle(int style) { return style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17; }
// GC 80424A40 m_wallToolHeight.
const float kWallToolHeight = 3.0f;
CTilePt Step(const CTilePt& t, int dir) {
  CTilePt n = t + CTilePt::sDirections[dir];
  n.m_level = 1;
  return n;
}
// An object's wall flags (data 0xD) for `segment` seen from its direction (data 1): bits for the
// segments 1, 8, 2, 4 at `shift` 8 (it sits against them) or 0 (it needs them).
bool ObjectUsesSegment(cXObjectImpl* o, int segment, int b1, int b8, int b2, int b4) {
  const unsigned int flags = static_cast<unsigned int>(o->GetData(0xd));
  const int dir = o->GetData(1);
  const int seg = TileWalls::RotateSegment(segment, (8u - static_cast<unsigned int>(dir)) >> 1 & 3);
  return (seg == 1 && (flags & b1)) || (seg == 8 && (flags & b8)) || (seg == 2 && (flags & b2)) ||
         (seg == 4 && (flags & b4));
}
}  // namespace

// 8021F598
InteractorModule__WallManipulator::InteractorModule__WallManipulator() { m_params.id = 3; }

// 8021F648
InteractorModule__WallManipulator::~InteractorModule__WallManipulator() {}

// 8021F7BC
// GC: the visualizer's resources; the extents (0.5, 3).
void InteractorModule__WallManipulator::OnCreate() {
  if (InteractorModule__InteractorVisualizer* v = InteractorVisualizer()) v->CreateResources(*this);
  m_extents.x = 0.5f;
  m_extents.y = 3.0f;
}

// 8021F808
void InteractorModule__WallManipulator::OnDestroy() {}

// 8021F83C OnStart
// The params kept; fences (+0xF4) take the fence's style and the fence flag, the sell tool (+0xF8) is the
// destruct mode deleting (0x208); the stock; the cursor at the camera's target on the lot.
void InteractorModule__WallManipulator::Start(BuildToolParams& params) {
  m_flags = 0;
  m_player = params.player;
  m_params = params;
  m_startStock = params.stock;
  m_fenceSells.clear();
  m_patternSells.clear();
  m_inventory.clear();
  m_fadeOff.clear();
  m_fence = static_cast<const PCTFenceData*>(params.data);
  if (m_params.fence == 1) {
    m_flags |= 0x100;
    m_style = m_fence ? m_fence->style : 1;
  } else {
    m_style = 1;
  }
  if (m_params.fence == 0) {
    m_flags &= ~0x100u;
  } else {
    m_flags |= 0x100;  // GC: the visualizer's resource set gets shader 0x9B0C648D
    m_fenceIndex = params.index;
  }
  if (m_params.sell) m_flags = (m_flags & ~0x100u) | 0x208;
  m_count = 0;
  m_stock = m_params.stock;
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
  if (cam) m_pos = EVec3{cam->m_target.x, cam->m_target.y, 0.0f};
  ibuild::ClampPosToWorld(m_pos);
  m_cam = nullptr;
  m_leftStick = EVec2{0.0f, 0.0f};
  m_rightStick = EVec2{0.0f, 0.0f};
}

// 8021FB18
void InteractorModule__WallManipulator::OnStop() {}

// 8021FB80
void InteractorModule__WallManipulator::CancelSession() { CommittTransaction(3); }

// 8021FBA4
// The callback told `state` with the style, the stock used since the last commit and what was sold
// (styles, wallpapers, objects; the lists emptied).
void InteractorModule__WallManipulator::CommittTransaction(int state) {
  CallbackData data;
  data.m_value = m_params.index;
  data.m_count = m_stock != -1 ? m_stock - m_startStock : 0;
  data.m_sells = m_fenceSells;
  m_fenceSells.clear();
  data.m_patterns = m_patternSells;
  m_patternSells.clear();
  data.m_inventory = m_inventory;
  m_inventory.clear();
  data.m_user = m_params.user;
  data.m_state = state;
  if (m_params.callback) m_params.callback(&data);
  m_count = 0;
  m_startStock = m_stock;
  // GC: GlobalDispatch(0x91, 0), (0xFC, 0), (0xEC, 1) (the world side's listeners).
}

// 802202C0
// The line from the anchor to the cursor built if it adds a wall and the stock pays for it (or the free
// cheat, or unlimited stock): the rooms checked (PreviewWallBuild), else the walls as they were.
int InteractorModule__WallManipulator::FinalizePlacement() {
  EVec3 from = m_anchor, to = m_savedPos;
  cFixedWorldImpl* w = World();
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetWalls());
  int any = 0;
  const int cost = GetWallLineCost(from, to, any, true, false);
  if (!any) return 0;
  const bool free = g_freeBuildBuyCheat || EGlobal::IsBuildHouseMode() || m_stock == -1;
  if (!free && cost > m_stock) return 0;
  int count = 0;
  if (!SubmitLine(from, to, count, true, false)) return 0;
  ERoom* room = _globals.m_house ? _globals.m_house->m_room : nullptr;
  if (room && room->PreviewWallBuild((m_flags >> 8) & 1)) {
    if (!free) m_stock -= cost;
    w->ComputeRooms(0);
    _globals.RecalcHouse();
    // GC: Globs::pHouse vtable 0x8C ComputeAndStoreLotData (the world side's).
    std::printf("[build] player %d built %d wall segment%s (stock %d)\n", m_player + 1, cost, cost == 1 ? "" : "s", m_stock);
    return 1;
  }
  ibuild::RestoreArray(w->GetWalls(), saved);
  w->ComputeRooms(0);
  return 0;
}

// 80220570
// The line's walls deleted (refunded: the stock grows by what was deleted); when the rooms would then
// fail the check, the walls as they were and the run's ERoomWall dropped, checked again.
int InteractorModule__WallManipulator::FinalizeWallDel() {
  EVec3 from = m_anchor, to = m_savedPos;
  cFixedWorldImpl* w = World();
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetWalls());
  int count = 0;
  if (!SubmitLine(from, to, count, true, true)) return 0;
  ERoom* room = _globals.m_house ? _globals.m_house->m_room : nullptr;
  const bool fences = (m_flags >> 8) & 1;
  if (PreviewNRooms() && room && !room->PreviewWallBuild(fences)) {
    ibuild::RestoreArray(w->GetWalls(), saved);
    w->ComputeRooms(0);
    const std::vector<unsigned char> saved2 = ibuild::SaveArray(w->GetWalls());
    CTilePt a, b;
    ibuild::ConvertVertsToTiles(from, to, a, b);
    SIMMY_STUB("WallManipulator::FinalizeWallDel: ERoom::DeleteERoomWallContainingSegment", 0x80220570);
    if (!room->PreviewWallBuild(fences)) {
      ibuild::RestoreArray(w->GetWalls(), saved2);
      w->ComputeRooms(0);
      return 0;
    }
  }
  if (m_stock != -1 && count != -1) m_stock += count;
  w->ComputeRooms(0);
  _globals.RecalcHouse();
  std::printf("[build] player %d deleted %d wall segment%s\n", m_player + 1, count, count == 1 ? "" : "s");
  return 1;
}

// 80220940
// The room's four sides' cost (0 when one of them adds nothing).
void InteractorModule__WallManipulator::CountWallsInRoomSelection() {
  const float x0 = m_anchor.x < m_savedPos.x ? m_anchor.x : m_savedPos.x;
  const float x1 = m_anchor.x < m_savedPos.x ? m_savedPos.x : m_anchor.x;
  const float y0 = m_anchor.y < m_savedPos.y ? m_anchor.y : m_savedPos.y;
  const float y1 = m_anchor.y < m_savedPos.y ? m_savedPos.y : m_anchor.y;
  const float corners[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
  m_count = 0;
  for (int i = 0; i < 4; ++i) {
    EVec3 a{corners[i][0], corners[i][1], 0.0f}, b{corners[(i + 1) & 3][0], corners[(i + 1) & 3][1], 0.0f};
    int any = 0;
    m_count += GetWallLineCost(a, b, any, true, false);
    if (!any) {
      m_count = 0;
      return;
    }
  }
}

// 80220AFC
// A room round the rectangle from the anchor to the cursor: built when each side adds a wall and the
// stock pays for all four (or free), then checked as FinalizePlacement does.
int InteractorModule__WallManipulator::FinalizeRoom() {
  const float x0 = m_anchor.x < m_savedPos.x ? m_anchor.x : m_savedPos.x;
  const float x1 = m_anchor.x < m_savedPos.x ? m_savedPos.x : m_anchor.x;
  const float y0 = m_anchor.y < m_savedPos.y ? m_anchor.y : m_savedPos.y;
  const float y1 = m_anchor.y < m_savedPos.y ? m_savedPos.y : m_anchor.y;
  cFixedWorldImpl* w = World();
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetWalls());
  const EVec3 sides[4][2] = {{{x0, y0, 0.0f}, {x1, y0, 0.0f}},
                             {{x1, y0, 0.0f}, {x1, y1, 0.0f}},
                             {{x0, y1, 0.0f}, {x1, y1, 0.0f}},
                             {{x0, y1, 0.0f}, {x0, y0, 0.0f}}};
  int any[4] = {};
  int cost = 0;
  for (int i = 0; i < 4; ++i) {
    EVec3 a = sides[i][0], b = sides[i][1];
    cost += GetWallLineCost(a, b, any[i], true, false);
  }
  for (int i = 0; i < 4; ++i)
    if (!any[i]) return 0;
  const bool free = g_freeBuildBuyCheat || EGlobal::IsBuildHouseMode() || m_stock == -1;
  if (!free && cost > m_stock) return 0;
  for (int i = 0; i < 4; ++i) {
    EVec3 a = sides[i][0], b = sides[i][1];
    int count = 0;
    SubmitLine(a, b, count, true, false);
  }
  ERoom* room = _globals.m_house ? _globals.m_house->m_room : nullptr;
  if (room && room->PreviewWallBuild((m_flags >> 8) & 1)) {
    if (!free) m_stock -= cost;
    w->ComputeRooms(0);
    _globals.RecalcHouse();
    std::printf("[build] player %d built a room (%d wall segments, stock %d)\n", m_player + 1, cost, m_stock);
    return 1;
  }
  ibuild::RestoreArray(w->GetWalls(), saved);
  w->ComputeRooms(0);
  return 0;
}

// 80221028
// A valid selection finalised by its mode (room, add, delete); done: a room more than before is a
// want/fear event, the routes are invalidated, the build (add) or sell sound, the anchor back at the
// cursor and unanchored, the callback told 0 (bought or sold). Else the error sound.
void InteractorModule__WallManipulator::HandleFinalizeRequest() {
  m_fenceSells.clear();
  const int before = ibuild::CountRoomsInUse();
  int done = 0;
  if (m_flags & 0x80) {
    if (m_flags & 0x10) done = FinalizeRoom();
    else if (m_flags & 4) done = FinalizePlacement();
    else if (m_flags & 8) {
      ClearWallFadeOffList();
      done = FinalizeWallDel();
    }
  }
  m_flags &= ~0x20u;
  if (done == 1) {
    // A room more: the residents' wants (kEventAddARoom, NotifyAllResidents: vtable 0x1A4).
    if (before < ibuild::CountRoomsInUse() && Globs_pNeighborhood) {
      WantFear__Notification n;
      n.m_event = WantFearEvent::kAddARoom;
      n.m_target = WantFearEvent::kGenericParam;
      n.m_count = WantFearEvent::kGenericParam;
      Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
    }
    // Every person plans its routes again (Globs::pObjectModule vtable 0x184).
    if (Globs_pObjectModule) Globs_pObjectModule->InvalidateAllRoutes();
    play_sound((m_flags & 4) ? 0x80e1a9cd : 0x66dede72);
    m_anchor = m_pos;
    const unsigned int f = m_flags;
    m_flags = f & ~0x15u;
    if (!(f & 0x200)) m_flags = f & ~0x1du;
    CommittTransaction(0);
  } else {
    play_sound(0x6e2778b7);
  }
}

// 802212DC
void InteractorModule__WallManipulator::HandleSwapRequest() {
  m_flags = 0;
  play_sound(0x0064ac15);
  CommittTransaction(4);
}

// 80221344
// Unanchored: the tool is left (3); anchored: let go of the anchor.
void InteractorModule__WallManipulator::HandleExitRequest() {
  const unsigned int f = m_flags;
  if (!(f & 1)) {
    m_flags = 0;
    CancelSession();
    return;
  }
  m_flags = f & ~0x45u;
  if (!(f & 0x200)) m_flags = f & ~0x4du;
}

// 80221394
void InteractorModule__WallManipulator::AdjustCursorPosition() { ibuild::SnapPositionToTile(m_pos, m_savedPos, 2); }

// 802213C0
// A room selection is valid when it costs something within the stock; a line when it runs along x, y or
// a diagonal: its side offset towards the camera, then CheckForAffectedWalls: -1 invalid, 0 only new
// walls (add mode; not for the destruct tool), more: walls to delete (delete mode).
void InteractorModule__WallManipulator::ValidateWallSegment() {
  bool valid = false;
  m_count = 0;
  m_sideOffset = EVec3{0.0f, 0.0f, 0.0f};
  m_affected = 0;
  if (m_flags & 0x10) {
    CountWallsInRoomSelection();
    valid = (m_count > 0 && m_count <= m_stock) || m_stock == -1;
  } else {
    float dx = m_savedPos.x - m_anchor.x, dy = m_savedPos.y - m_anchor.y;
    const float l = std::sqrt(dx * dx + dy * dy);
    if (dx != 0.0f || dy != 0.0f) dx /= l, dy /= l;
    float d = std::fabs(dx);
    d = dy < 0.0f ? d + dy : d - dy;
    d = std::fabs(d);
    if (dx == 0.0f || dy == 0.0f || d < 0.001f) {
      EVec3 a = m_anchor, b = m_savedPos;
      ibuild::GetVisibleSideOfWall(_gd.m_players[m_player & 1].m_cam, a, b, &m_sideOffset);
      a.x += m_sideOffset.x, a.y += m_sideOffset.y, a.z += m_sideOffset.z;
      b.x += m_sideOffset.x, b.y += m_sideOffset.y, b.z += m_sideOffset.z;
      m_affected = CheckForAffectedWalls(a, b);
      valid = true;
      if (m_affected == -1) valid = false;
      else if (m_affected == 0) {
        if (m_flags & 0x200) valid = false;
        else m_flags = (m_flags & ~8u) | 4;
      } else if (!(m_flags & 0x200)) {
        m_flags = (m_flags & ~4u) | 8;
      }
    }
  }
  if (valid) m_flags |= 0x80;
  else m_flags &= ~0x80u;
}

// 802216B8
// The stick's first push (0xC00: one tile), then it moves freely (0x400).
void InteractorModule__WallManipulator::UpdateStickState() {
  if (m_leftStick.x == 0.0f && m_leftStick.y == 0.0f) {
    m_flags &= ~0xc00u;
    return;
  }
  if (m_flags & 0x400) m_flags &= ~0x800u;
  else m_flags |= 0xc00;
}

// 8022170C
void InteractorModule__WallManipulator::ClearWallFadeOffList() {
  for (EIWallPart* part : m_fadeOff) part->m_forceFadeOff = 0;
  m_fadeOff.clear();
}

// 8022176C
// The cursor moved (a tile on the stick's first push, then freely; just anchored: a tile on from the
// anchor), kept on the lot; unanchored, the anchor follows it; anchored, the selection is checked. Then
// the requests (finalize, swap), the count for the catalogue's price, the exit request.
void InteractorModule__WallManipulator::Update(float dt) {
  InteractorModule__Interactor::Update(dt);
  UpdateStickState();
  ClearWallFadeOffList();
  m_fenceSells.clear();
  if (ESimsCam* cam = m_cam) {
    if (!(m_flags & 2)) {
      if (!(m_flags & 0x800)) ibuild::MoveCursorAlongCameraTargetVector(static_cast<float>(cam->GetCursorSpeed()), m_pos, m_leftStick, cam);
      else ibuild::MoveCursorOneTileAlongCameraTargetVector(m_pos, m_leftStick, cam, 2);
    } else {
      ibuild::SnapPositionToTile(m_pos, m_savedPos, 2);
      m_anchor = m_savedPos;
      m_anchorDraw = m_savedPos;
      if (m_leftStick.x == 0.0f && m_leftStick.y == 0.0f) {
        const EVec2 up{0.0f, 1.0f};
        ibuild::MoveCursorOneTileAlongCameraTargetVector(m_pos, up, cam, 2);
      } else {
        ibuild::MoveCursorOneTileAlongCameraTargetVector(m_pos, m_leftStick, cam, 2);
      }
      m_flags &= ~2u;
    }
    ibuild::ClampPosToWorld(m_pos);
    if (!(m_flags & 0x11)) {
      m_count = 0;
      ibuild::SnapPositionToTile(m_pos, m_savedPos, 2);
      m_anchor = m_savedPos;
      m_anchorDraw = m_savedPos;
    } else {
      AdjustCursorPosition();
      ValidateWallSegment();
    }
  }
  if (m_flags & 0x20) HandleFinalizeRequest();
  if (m_flags & 0x1000) HandleSwapRequest();
  SendBuildItemCountChangeEvent(m_count);
  if (m_flags & 0x40) HandleExitRequest();
}

// 802219B4
// 0 (A) anchors (the anchor sound) or, anchored or in a room, asks to finalize; 1 (Y) swaps tools from
// the destruct tool; 2 (the trigger) makes the selection a room while unanchored; 4 (B) asks to exit.
void InteractorModule__WallManipulator::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand command, float) {
  switch (static_cast<int>(command)) {
    case 1:
      if (!(m_flags & 0x200)) return;
      m_flags = (m_flags & ~1u) | 0x1000;
      return;
    case 0:
      if (!(m_flags & 0x11)) {
        m_flags |= 3;
        play_sound(0x374bdcea);
        return;
      }
      m_flags |= 0x20;
      return;
    case 2:
      if (m_flags & 1) return;
      m_flags |= 0x10;
      return;
    case 4:
      m_flags |= 0x40;
      return;
    default:
      return;
  }
}

// 80221A84
void InteractorModule__WallManipulator::OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand command) {
  switch (static_cast<int>(command)) {
    case 8: m_leftStick.y = 0.0f; break;
    case 0xb: m_rightStick.x = 0.0f; break;
    case 0xc: m_rightStick.y = 0.0f; break;
    case 2: m_flags &= ~0x10u; break;
    case 7: m_leftStick.x = 0.0f; break;
    default: break;
  }
}

// 80221B08
void InteractorModule__WallManipulator::OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command, float value) {
  switch (static_cast<int>(command)) {
    case 8: m_leftStick.y = value; break;
    case 7: m_leftStick.x = value; break;
    case 0xb: m_rightStick.x = value; break;
    case 0xc: m_rightStick.y = value; break;
    default: break;
  }
}

// 80221B54
int InteractorModule__WallManipulator::GetTexGenMode() { return (m_flags & 0x80) && (m_flags & 4) ? 2 : 0; }

// 80221B7C
void InteractorModule__WallManipulator::PreDraw(InteractorModule__InteractorVisualizer& v) { v.PreDraw(*this); }
// 80221BA8
void InteractorModule__WallManipulator::Draw(InteractorModule__InteractorVisualizer& v) { v.Draw(*this); }

// 80221BD4
// The line's segments that may be added (or, `remove`, deleted: negative), from its first tile until
// its last or the lot's edge; `any`: one of them may.
int InteractorModule__WallManipulator::GetWallLineCost(EVec3& from, EVec3& to, int& any, bool, bool remove) {
  any = 0;
  cFixedWorldImpl* w = World();
  CTilePt a, b;
  ibuild::ConvertVertsToTiles(from, to, a, b);
  const int dir = CTilePt::GetTileDirection(a, b);
  if (dir == 8) return 0;
  const int segment = TileWalls::DirToWallSeg(dir);
  CTilePt t = a;
  int n = 0;
  for (;;) {
    if (!remove) {
      if (CanChangeTileAdd(t, segment)) any = 1, ++n;
    } else if (CanChangeTileDelete(t, segment)) {
      any = 1, --n;
    }
    t = Step(t, dir);
    if (w->OutOfBounds(t) || t == b) break;
  }
  return n;
}

// 80221DE8
// The line, moved by the side offset, added (HandleAddLine) or deleted (HandleDeleteLine, on the side
// the camera sees); `count`: what was added or deleted (-1 nothing).
bool InteractorModule__WallManipulator::SubmitLine(EVec3& from, EVec3& to, int& count, bool, bool remove) {
  EVec3 a{from.x + m_sideOffset.x, from.y + m_sideOffset.y, from.z + m_sideOffset.z};
  EVec3 b{to.x + m_sideOffset.x, to.y + m_sideOffset.y, to.z + m_sideOffset.z};
  CTilePt ta, tb;
  ibuild::ConvertVertsToTiles(a, b, ta, tb);
  TilePtDir dir{CTilePt::GetTileDirection(ta, tb)};
  count = -1;
  int r;
  if (remove) {
    const int side = ibuild::GetVisibleSideOfWall(_gd.m_players[m_player & 1].m_cam, a, b, nullptr);
    r = HandleDeleteLine(ta, tb, dir, -side);
  } else {
    r = HandleAddLine(ta, tb, dir, 0);
  }
  count = r;
  return count != -1;
}

// 80221F78
// The tile editable and the segment addable (CanAdd); a diagonal only on a tile without objects (but
// kind 9); an edge not against an object that sits against it (wall flags 0x800/0x400/0x100/0x200 by
// its turned segment), on this tile or the one across.
int InteractorModule__WallManipulator::DoesNotConflictWithExistingArchitecture(CTilePt& tile, TileWallsSegment segment) {
  cFixedWorldImpl* w = World();
  TileWalls walls = w->GetWall(tile);
  if (!w->MayEditTile(tile)) return 0;
  if (!walls.CanAdd(segment)) return 0;
  const int seg = segment;
  if (seg == 0x10 || seg == 0x20) {
    for (cXObjectImpl* o : ibuild::ObjectsOnTile(tile))
      if (o->GetType() != 9) return 0;
    return 1;
  }
  if (seg != 1 && seg != 2) return 1;
  for (cXObjectImpl* o : ibuild::ObjectsOnTile(tile))
    if (o->GetType() != 2 && ObjectUsesSegment(o, seg, 0x800, 0x400, 0x100, 0x200)) return 0;
  CTilePt across = tile;
  across += CTilePt::sDirections[seg == 1 ? 2 : 0];
  if (!w->MayEditTile(across)) return 0;
  const int opposite = TileWalls::RotateSegment(seg, 2);
  for (cXObjectImpl* o : ibuild::ObjectsOnTile(across))
    if (o->GetType() != 2 && ObjectUsesSegment(o, opposite, 0x800, 0x400, 0x100, 0x200)) return 0;
  return 1;
}

// 8022237C
// A segment without a wall (fences: or one of another fence style that may be deleted) that is legal
// and conflicts with nothing.
int InteractorModule__WallManipulator::CanChangeTileAdd(CTilePt& tile, TileWallsSegment segment) {
  TileWalls walls = World()->GetWall(tile);
  if (walls.HasWall(segment)) {
    if (!(m_flags & 0x100)) {
      if (walls.HasWallNotFence(segment)) return 0;
    } else {
      const int style = walls.GetStyle(segment);
      if ((IsFence(style) && m_style == style) || !CanChangeTileDelete(tile, segment)) return 0;
    }
  }
  if (!LegalWallTile(tile, segment)) return 0;
  return DoesNotConflictWithExistingArchitecture(tile, segment);
}

// 802224C8
// A wall of the tool's kind (fences: a fence; the destruct tool: anything) on an editable tile that no
// object needs: a door or window (kind 8) in a portal style checks its own wall flags; other objects
// (not people) need the segments of their flags 8/4/1/2, on this tile and across.
int InteractorModule__WallManipulator::CanChangeTileDelete(CTilePt& tile, TileWallsSegment segment) {
  cFixedWorldImpl* w = World();
  TileWalls walls = w->GetWall(tile);
  const bool notFence = walls.HasWallNotFence(segment);
  if (!walls.HasWall(segment)) return 0;
  const bool fence = IsFence(walls.GetStyle(segment));
  if ((m_flags & 0x100) && !fence) return 0;
  if (!(m_flags & 0x300) && !notFence) return 0;
  if (!w->MayEditTile(tile)) return 0;
  const int seg = segment;
  if (seg != 1 && seg != 2) return 1;
  for (cXObjectImpl* o : ibuild::ObjectsOnTile(tile)) {
    if (o->GetType() == 8) {
      const int style = walls.GetStyle(segment);
      if ((IsPortalStyle(style) || style == 0x16) && ObjectUsesSegment(o, seg, 8, 4, 1, 2)) return 0;
      break;
    }
    if (o->GetType() != 2 && ObjectUsesSegment(o, seg, 8, 4, 1, 2)) return 0;
  }
  CTilePt across = tile;
  across += CTilePt::sDirections[seg == 1 ? 2 : 0];
  if (!w->MayEditTile(across)) return 0;
  const int opposite = TileWalls::RotateSegment(seg, 2);
  for (cXObjectImpl* o : ibuild::ObjectsOnTile(across))
    if (o->GetType() != 8 && o->GetType() != 2 && ObjectUsesSegment(o, opposite, 8, 4, 1, 2)) return 0;
  return 1;
}

// 802229E4
// Each tile of the line whose wall is of the tool's kind and may go: deleted (ERoom::DeleteWallAtTile)
// and counted: walls (and fences of the tool's style) as segments refunded, other fence styles as styles
// sold; the wallpaper of both its faces counted as sold patterns. -1: nothing deleted.
int InteractorModule__WallManipulator::HandleDeleteLine(CTilePt& from, CTilePt& to, TilePtDir& dir, int) {
  cFixedWorldImpl* w = World();
  if (dir == 8) return 0;
  int r = -1;
  CTilePt t = from;
  const int seg = TileWalls::DirToWallSeg(dir);
  auto sellPatterns = [&](TileWalls& walls) {
    if (int p = walls.GetPattern(seg, 0)) ibuild::IncrementSellCountForPattern(p, m_patternSells);
    CTilePt across = t;
    TileWalls::GetAdjacentTile(seg, &across);
    TileWalls other = w->GetWall(across);
    if (int p = other.GetPattern(TileWalls::GetOppositeSegment(seg), 0)) ibuild::IncrementSellCountForPattern(p, m_patternSells);
  };
  while (!(t == to)) {
    if (w->OutOfBounds(t)) break;
    TileWalls walls = w->GetWall(t);
    bool has;
    if (m_flags & 0x200) has = walls.HasWall(seg);
    else if (m_flags & 0x100) has = walls.HasFenceNotWall(seg);
    else has = walls.HasWallNotFence(seg);
    if (has && CanChangeTileDelete(t, seg)) {
      int sellStyle = -1;
      if (!(m_flags & 0x100)) {
        if (!(m_flags & 0x200)) {
          sellPatterns(walls);
          r = r == -1 ? 1 : r + 1;
        } else if (walls.HasFenceNotWall(seg)) {
          sellStyle = walls.GetStyle(seg);
        } else if (walls.HasWallNotFence(seg)) {
          IncrementSellCountForStyle(1);
          sellPatterns(walls);
          r = r == -1 ? 1 : r + 1;
        }
      } else {
        const int style = walls.GetStyle(seg);
        if (style == m_style) r = r == -1 ? 1 : r + 1;
        else sellStyle = style;
      }
      if (sellStyle != -1) {
        IncrementSellCountForStyle(sellStyle);
        if (r == -1) r = 0;
      }
      ERoom::DeleteWallAtTile(t, walls, seg);
    }
    t = Step(t, dir);
  }
  return r;
}

// 80222DF0
// The line's walls added tile by tile (AddWallAtTile) until its end or the lot's edge; a tile where it
// cannot be (CanAdd, outside the building bounds) makes the whole line come out again. The tiles walked
// (-1: none).
int InteractorModule__WallManipulator::HandleAddLine(CTilePt from, CTilePt to, TilePtDir& dir, int) {
  cFixedWorldImpl* w = World();
  if (dir == 8) return -1;
  CTilePt t = from;
  int n = 0;
  bool fail = false;
  const int seg = TileWalls::DirToWallSeg(dir);
  while (!(t == to) && !w->OutOfBounds(t)) {
    TileWalls walls = w->GetWall(t);
    if (!walls.CanAdd(seg) || w->OutOfBuildingBounds(t, (seg & 0x30) != 0)) fail = true;
    else AddWallAtTile(t, walls, seg);
    ++n;
    t = Step(t, dir);
  }
  if (fail) {
    while (t != from) {
      t -= CTilePt::sDirections[dir];
      t.m_level = 1;
      TileWalls walls = w->GetWall(t);
      if (CanChangeTileDelete(t, seg)) ERoom::DeleteWallAtTile(t, walls, seg);
    }
    n = 0;
  }
  return n == 0 ? -1 : n;
}

// 80223088
// Both tiles of the segment (the tile alone for 0) on the lot and editable, with nobody standing there
// (GC: a person there has its route's +0x7C cleared) and no object of data 0x3E > 0 (or holding one)
// unless its data 0x22 is set.
int InteractorModule__WallManipulator::LegalWallTile(CTilePt& tile, TileWallsSegment segment) {
  cFixedWorldImpl* w = World();
  if (segment == 0) return !w->OutOfBounds(tile) && w->MayEditTile(tile) ? 1 : 0;
  CTilePt across = tile;
  TileWalls::GetAdjacentTile(segment, &across);
  if (w->OutOfBounds(tile) || w->OutOfBounds(across) || !w->MayEditTile(tile) || !w->MayEditTile(across)) return 0;
  for (int i = 0; i < 2; ++i) {
    for (cXObjectImpl* o : ibuild::ObjectsOnTile(i == 0 ? tile : across)) {
      if (o->GetData(0x22)) continue;
      if (o->GetType() == 2) return 0;  // GC: also cXPersonImpl::GetCurrentRoute()->+0x7C = 0
      if (o->GetData(0x3e) > 0) return 0;
      SIMMY_STUB("WallManipulator::LegalWallTile: the objects in the object's slots (data 0x3E)", 0x80223088);
    }
    if (across == tile) break;
  }
  return 1;
}

// 80223354
// The segment added with the tool's style and no wallpaper on either face, when it may be.
bool InteractorModule__WallManipulator::AddWallAtTile(CTilePt& tile, TileWalls& walls, TileWallsSegment segment) {
  if (!CanChangeTileAdd(tile, segment)) return false;
  cFixedWorldImpl* w = World();
  if (!walls.HasWall(segment) || m_style != walls.GetStyle(segment)) {
    walls.AddWall(segment);
    walls.SetStyle(WallStyle{m_style}, segment);
    walls.SetPattern(0, segment, 0);
    w->SetWall(tile, walls);
    CTilePt across = tile;
    TileWalls::GetAdjacentTile(segment, &across);
    const int opposite = TileWalls::GetOppositeSegment(segment);
    TileWalls other = w->GetWall(across);
    other.SetPattern(0, opposite, 0);
    w->SetWall(across, other);
  }
  return true;
}

// 802234D4
// The rooms recomputed: fewer than 20 inside rooms.
bool InteractorModule__WallManipulator::PreviewNRooms() {
  World()->ComputeRooms(0);
  int n = 0;
  for (auto& e : Globs_pRoomManager->m_rooms)
    if (e.second && e.second->m_inUse && !e.second->IsOutside()) ++n;
  return n < 0x14;
}

// 80223584
void InteractorModule__WallManipulator::IncrementSellCountForStyle(WallStyle style) {
  for (BuildSellCount& c : m_fenceSells)
    if (c.id == style) {
      ++c.count;
      return;
    }
  m_fenceSells.push_back(BuildSellCount{style, 1});
}

// 802237A0
// The line's tiles until its end: each must be legal and conflict with nothing (a diagonal needs its
// four neighbours on the lot's extent); counted (+0xD0). With a wall already there: the destruct tool
// counts it as sold and keeps the walls of both faces up while selected (forced unfaded); the build
// tools refuse (-1). New walls after deleted ones refuse too. The walls to delete (0: none; -1 invalid,
// also when new walls cost more than the stock).
int InteractorModule__WallManipulator::CheckForAffectedWalls(EVec3& from, EVec3& to) {
  cFixedWorldImpl* w = World();
  CTilePt a, b;
  ibuild::ConvertVertsToTiles(from, to, a, b);
  const int dir = CTilePt::GetTileDirection(a, b);
  if (dir == 8) return 0;
  const int seg = TileWalls::DirToWallSeg(dir);
  int r = 0;
  CTilePt t = a, next = a + CTilePt::sDirections[dir];
  m_count = 0;
  if (w->OutOfBounds(t)) return -1;
  ERoom* room = _globals.m_house ? _globals.m_house->m_room : nullptr;
  for (;;) {
    if (w->OutOfBounds(t) || !(t != b)) break;
    if (seg & 0x30)
      for (int d = 0; d < 4; ++d) {
        CTilePt n = t + CTilePt::sDirections[d];
        if (w->OutOfBounds(n) || (w->GetFlags(n) & 0x10)) {
          m_count = 0;
          r = -1;
          break;
        }
      }
    int ok = LegalWallTile(t, seg);
    if (ok) ok = DoesNotConflictWithExistingArchitecture(t, seg);
    static const bool trace = std::getenv("SIMMY_BUILD_TRACE") != nullptr;
    if (trace && !ok && _framecount % 30 == 0)
      std::printf("[build] trace: tile %d,%d segment %d: legal %d, flags %x, walls %x\n", t.GetX(), t.GetY(), seg,
                  LegalWallTile(t, seg), w->GetFlags(t), w->GetWall(t).m_segments);
    if (!ok) {
      m_count = 0;
      r = -1;
      break;
    }
    ++m_count;
    TileWalls walls = w->GetWall(t);
    if (!walls.HasWall(seg)) {
      if (r > 0) {
        m_count = 0;
        r = -1;
        break;
      }
    } else {
      if (!(m_flags & 0x200)) {
        m_count = 0;
        r = -1;
        break;
      }
      IncrementSellCountForStyle(walls.GetStyle(seg));
      if (room) {
        if (EIWallPart* part = room->GetWallFromTileAndSegment(seg, t)) {
          part->m_forceFadeOff = 1;
          m_fadeOff.push_back(part);
        }
        CTilePt across = t;
        TileWalls::GetAdjacentTile(seg, &across);
        if (EIWallPart* part = room->GetWallFromTileAndSegment(TileWalls::GetOppositeSegment(seg), across)) {
          part->m_forceFadeOff = 1;
          m_fadeOff.push_back(part);
        }
      }
      ++r;
    }
    t = next;
    next += CTilePt::sDirections[dir];
    next.m_level = 1;
  }
  if (r == 0 && m_count > 0 && m_stock != -1 && m_stock < m_count) r = -1;
  return r;
}

// 80223ED0
// The callback told 5 with the selection's count and the styles it would sell (the list emptied).
void InteractorModule__WallManipulator::SendBuildItemCountChangeEvent(int count) {
  CallbackData data;
  data.m_count = count;
  data.m_sells = m_fenceSells;
  data.m_state = 5;
  data.m_user = m_params.user;
  if (m_params.callback) m_params.callback(&data);
  m_fenceSells.clear();
}

// 80223F70
int InteractorModule__WallManipulator::IsAnchored() { return (m_flags & 1) ? 1 : 0; }
// 80223F88
int InteractorModule__WallManipulator::IsValidWallSegment() { return (m_flags >> 7) & 1; }
// 80223F94
int InteractorModule__WallManipulator::IsRoomPreview() { return (m_flags >> 4) & 1; }
// 80223FA0
int InteractorModule__WallManipulator::IsBuyMode() { return (m_flags >> 2) & 1; }
// 80223FAC
int InteractorModule__WallManipulator::IsSellMode() { return (m_flags >> 3) & 1; }
// 80223FB8
int InteractorModule__WallManipulator::IsFenceMode() { return (m_flags >> 8) & 1; }
// 80223FC4
int InteractorModule__WallManipulator::IsDestructMode() { return (m_flags >> 9) & 1; }
// 80223FD0
unsigned int InteractorModule__WallManipulator::GetFenceShaderID() { return (m_flags & 0x100) && m_fence ? m_fence->shader : 0; }
// 80223FF0
void InteractorModule__WallManipulator::SetAnchorPoint(EVec3& p) { m_anchor = p; }
// 80224010
void InteractorModule__WallManipulator::SetAnchorPointDrawPos(EVec3& p) { m_anchorDraw = p; }
// 80224030
EVec3& InteractorModule__WallManipulator::GetAnchorPos() { return m_anchor; }
// 80224038
EVec3& InteractorModule__WallManipulator::GetAnchorDrawPos() { return m_anchorDraw; }
// 80224040
double InteractorModule__WallManipulator::GetWallHeight() { return kWallToolHeight; }
