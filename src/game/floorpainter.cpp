// Port of game/floorpainter.cpp: the floor tool (InteractorModule::FloorPainter, the player's interactor
// 5; see buildtools.hpp). In place mode A anchors the cursor's tile and a second A paints the rectangle
// from it to the cursor with the floor pattern (with the trigger held: the room under the cursor); in
// remove mode (the catalogue's sell tool) it takes the floors up (pattern 0). Each tile's old pattern is
// counted (the stock changes: the catalogue refunds what was taken up); the stock limits the tiles
// painted. Diagonal tiles keep a floor per half in the wall entry (TileWalls::SetFloorValue; the floor
// array holds 0xFF there). Then EIFloor::TestCreateFloors checks the floors can be built (else the floor
// array goes back to a copy) and the rooms and the house are recomputed.
//
// PC: InteractorVisualizer (the cursor, the rectangle's preview, the pattern's shader resource) is not
// ported; the lot data (HouseImpl vtable 0x8C) is the world side's.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/buildtools.hpp"

#include <algorithm>
#include <cstdio>

bool play_sound(unsigned int id);  // pcttarget.cpp (801D1DDC)
extern int g_unlockAllCheat;       // DAT_8047618C

namespace {
cFixedWorldImpl* World() { return Globs_pFixedWorld; }
int PatternCount() { return ibuild::SetCount(_globals.m_floorSet); }
// A pattern the player may lay (FloorTile +0x14, "purchasable"; GC reads the row without a check).
bool Purchasable(int pattern) {
  const void* row = ibuild::SetRow(_globals.m_floorSet, pattern);
  return row && ERQuickdata::U8(row, 0x14) != 0;
}
// The room of the cursor's tile (x from the engine's y, y from its x).
Room* CursorRoom(const EVec3& snapped, unsigned short& id) {
  CTilePt t(static_cast<int>(snapped.y), static_cast<int>(snapped.x), 1);
  id = World()->GetRoom(t);
  return Globs_pRoomManager ? Globs_pRoomManager->GetRoom(id) : nullptr;
}
}  // namespace

// 80206224
InteractorModule__FloorPainter::InteractorModule__FloorPainter() {
  m_params.id = 5;
  m_ownStock.reserve(static_cast<size_t>(std::max(PatternCount(), 0)));
}
// 802062B8
InteractorModule__FloorPainter::~InteractorModule__FloorPainter() {}

// 80206364
// GC: the visualizer's resources; the extents (1, 0.1); a count per floor pattern.
void InteractorModule__FloorPainter::OnCreate() {
  if (InteractorModule__InteractorVisualizer* v = InteractorVisualizer()) v->CreateResources(*this);
  m_extents.x = 1.0f;
  m_extents.y = 0.1f;
  m_counts.assign(static_cast<size_t>(std::max(PatternCount(), 0)), 0);
}
// 802063D8
void InteractorModule__FloorPainter::OnDestroy() { m_counts.clear(); }


// 80206424 OnStart
// The params kept (the stock changes into the painter's own list unless given one); the cursor at the
// camera's target; the stock. GC: the pattern's shader into the visualizer's resource set.
void InteractorModule__FloorPainter::Start(BuildToolParams& params) {
  m_params = params;
  m_stockChanges = &m_ownStock;
  m_ownStock.clear();
  m_player = params.player;
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
  if (cam) m_pos = cam->m_target;
  ibuild::ClampPosToWorld(m_pos);
  m_leftStick = EVec2{0.0f, 0.0f};
  m_rightStick = EVec2{0.0f, 0.0f};
  m_cam = nullptr;
  m_stock = m_params.stock;
  m_state = 0;
  // The pattern's shader (FloorTile +0x0C) into the visualizer's resource set.
  if (InteractorModule__InteractorVisualizer* v = InteractorVisualizer())
    if (InteractorModule__InteractorResourceSet* set = v->GetInteractorResourceSet(this))
      if (const void* floor = GetSelectedFloorData()) set->AddShader(ERQuickdata::U32(floor, 0xc));
}

// 80206684
void InteractorModule__FloorPainter::OnStop() {}

// 802066E0
void InteractorModule__FloorPainter::UpdateStickState() {
  if (m_leftStick.x == 0.0f && m_leftStick.y == 0.0f) {
    m_state &= 0x3f;
    return;
  }
  if (m_state & 0x40) m_state &= 0x7f;
  else m_state |= 0xc0;
}

// 80206734
// The cursor moved (a tile on the stick's first push, else freely), kept on the lot, snapped to its
// tile's centre. Place mode: whether the selection is valid (0x20 when not) and its count for the
// catalogue's price. Remove mode: the purchasable floors the selection would take up (the rectangle, or
// the cursor's room) as stock changes, their count for the price.
void InteractorModule__FloorPainter::Update(float dt) {
  InteractorModule__Interactor::Update(dt);
  UpdateStickState();
  if (ESimsCam* cam = m_cam) {
    if (!(m_state & 0x80)) ibuild::MoveCursorAlongCameraTargetVector(static_cast<float>(cam->GetCursorSpeed()), m_pos, m_leftStick, cam);
    else ibuild::MoveCursorOneTileAlongCameraTargetVector(m_pos, m_leftStick, cam, 1);
    ibuild::ClampPosToWorld(m_pos);
    ibuild::SnapPositionToTile(m_pos, m_savedPos, 1);
  }
  if (m_stockChanges) m_stockChanges->clear();
  cFixedWorldImpl* w = World();
  if (!IsInRemoveMode()) {
    int count = 0;
    if (!CurrentSelectionIsValid(count)) m_state |= 0x20;
    else m_state &= ~0x20;
    SendBuildItemCountChangeEvent(count);
    return;
  }
  const int n = PatternCount();
  std::vector<int> taken;
  if (w && (m_state & 1)) {
    taken.assign(static_cast<size_t>(n), 0);
    int x0, y0, x1, y1;
    GetSelectedRectPoints(x0, y0, x1, y1);
    for (int x = x0; x <= x1; ++x)
      for (int y = y0; y <= y1; ++y) {
        CTilePt t(x, y, 1);
        const int p = w->GetFloor(t);
        if (p < n && Purchasable(p)) ++taken[static_cast<size_t>(p)];
      }
  } else if (w && (m_state & 2)) {
    taken.assign(static_cast<size_t>(n), 0);
    unsigned short id = 0;
    Room* room = CursorRoom(m_savedPos, id);
    if (room && id)
      for (CTilePt& t : room->m_tiles) {
        const int p = w->GetFloor(t);
        if (p < n && Purchasable(p)) ++taken[static_cast<size_t>(p)];
      }
  }
  int total = 0;
  for (size_t i = 0; i < taken.size(); ++i)
    if (taken[i]) {
      total += taken[i];
      m_ownStock.push_back(BuildSellCount{static_cast<int>(i), taken[i]});
    }
  SendBuildItemCountChangeEvent(total);
}

// 80206D5C
// The callback told 5: remove mode, the count, the stock changes.
void InteractorModule__FloorPainter::SendBuildItemCountChangeEvent(int count) {
  InteractorModule__Interactor__CallbackData data;
  data.m_value = IsInRemoveMode() ? 1 : 0;
  data.m_stock = m_stockChanges;
  data.m_state = 5;
  data.m_user = m_params.user;
  data.m_count = count;
  if (m_params.callback) m_params.callback(&data);
}

// 80206DDC
// The callback told 0 (done) with the tiles painted.
void InteractorModule__FloorPainter::SendSuccessEvent(int count) {
  InteractorModule__Interactor__CallbackData data;
  data.m_value = IsInRemoveMode() ? 1 : 0;
  data.m_stock = m_stockChanges;
  data.m_state = 0;
  data.m_user = m_params.user;
  data.m_count = count;
  if (m_params.callback) m_params.callback(&data);
}

// 80206E5C
// Free: anchor (9: remove, anchored) at the cursor, the anchor sound. Anchored: the rectangle taken up;
// a room: the room taken up (unless invalid). Done: the stock changes counted and the callback told.
void InteractorModule__FloorPainter::HandleActionCmdInRemoveMode() {
  const unsigned char s = m_state;
  if (s == 0) {
    m_state = 9;
    m_anchor = m_savedPos;
    play_sound(0x374bdcea);
    return;
  }
  const int n = PatternCount();
  if (!(s & 1)) {
    if (!(s & 2)) return;
    if (m_stockChanges) m_stockChanges->clear();
    if (!((m_state >> 5) & 1)) {
      m_state |= 0x10;
      std::fill(m_counts.begin(), m_counts.end(), 0);
      if (!RemoveRoom(m_counts.data())) return;
      m_state = 0;
      UpdateStockChanges(m_counts.data(), n);
      SendSuccessEvent(0);
      return;
    }
  } else if (!(s & 0x20)) {
    m_state = s | 0x10;
    std::fill(m_counts.begin(), m_counts.end(), 0);
    if (m_stockChanges) m_stockChanges->clear();
    if (!RemoveFloor(m_counts.data())) return;
    m_state = 0;
    UpdateStockChanges(m_counts.data(), n);
    SendSuccessEvent(0);
    return;
  }
  play_sound(0x6e2778b7);
}

// 8020707C
// Each pattern counted (taken up or painted over) into the stock changes.
void InteractorModule__FloorPainter::UpdateStockChanges(int* counts, int n) {
  for (int i = 0; i < n; ++i)
    if (counts[i] > 0 && m_stockChanges) m_stockChanges->push_back(BuildSellCount{i, counts[i]});
}

// 80207278
// Free: anchor (5: place, anchored). Anchored: the rectangle painted; a room: the room painted (both
// unless invalid). Painted tiles: the stock changes and the callback told; the stock goes down by them.
void InteractorModule__FloorPainter::HandleActionCmdInPlaceMode() {
  const unsigned char s = m_state;
  if (s == 0) {
    m_state = 5;
    m_anchor = m_savedPos;
    play_sound(0x374bdcea);
    return;
  }
  const int n = PatternCount();
  int painted;
  if (!(s & 2)) {
    if (!(s & 1)) return;
    if ((s >> 5) & 1) {
      play_sound(0x6e2778b7);
      return;
    }
    m_state = s | 0x10;
    std::fill(m_counts.begin(), m_counts.end(), 0);
    painted = PaintFloor(m_counts.data());
  } else {
    if ((s >> 5) & 1) {
      play_sound(0x6e2778b7);
      return;
    }
    m_state = s | 0x10;
    std::fill(m_counts.begin(), m_counts.end(), 0);
    painted = PaintRoom(m_counts.data());
  }
  if (painted > 0) {
    UpdateStockChanges(m_counts.data(), n);
    SendSuccessEvent(painted);
  }
  m_state = (s >> 5) & 1;
  m_stock -= painted;
}

// 80207418
// 1 (Y) when free: the stock changes get the pattern's use, then the next tool. 0 (A) by the mode. 2 (the
// trigger) when free: the room of the cursor (6 place, 10 remove). 4 (B): free, leave the tool (with the
// pattern's use counted); anchored, let go.
void InteractorModule__FloorPainter::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand command, float) {
  switch (static_cast<int>(command)) {
    case 1:
      if (m_params.place == 0 && m_state == 0) {
        if (m_stockChanges) m_stockChanges->push_back(BuildSellCount{m_params.index, m_stock - m_params.stock});
        SwapTools();
      }
      return;
    case 0:
      if (m_params.place == 0) HandleActionCmdInRemoveMode();
      else HandleActionCmdInPlaceMode();
      return;
    case 2:
      if (m_state != 0) return;
      m_state = m_params.place != 0 ? 6 : 10;
      m_anchor = m_savedPos;
      return;
    case 4:
      if (m_state == 0) {
        if (m_stockChanges) m_stockChanges->push_back(BuildSellCount{m_params.index, m_stock - m_params.stock});
        CancelSession();
        return;
      }
      if (!(m_state & 1)) return;
      m_state = 0;
      if (m_stockChanges) m_stockChanges->clear();
      return;
    default:
      return;
  }
}

// 802078B0
void InteractorModule__FloorPainter::OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand command) {
  switch (static_cast<int>(command)) {
    case 8: m_leftStick.y = 0.0f; break;
    case 0xb: m_rightStick.x = 0.0f; break;
    case 0xc: m_rightStick.y = 0.0f; break;
    case 7: m_leftStick.x = 0.0f; break;
    case 2:
      if (m_state & 2) {
        m_state = 0;
        if (m_stockChanges) m_stockChanges->clear();
      }
      break;
    default: break;
  }
}

// 8020796C
void InteractorModule__FloorPainter::OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command, float value) {
  switch (static_cast<int>(command)) {
    case 8: m_leftStick.y = value; break;
    case 7: m_leftStick.x = value; break;
    case 0xb: m_rightStick.x = value; break;
    case 0xc: m_rightStick.y = value; break;
    default: break;
  }
}

// 802079B8
void InteractorModule__FloorPainter::PreDraw(InteractorModule__InteractorVisualizer& v) { v.PreDraw(*this); }
// 802079E4
void InteractorModule__FloorPainter::Draw(InteractorModule__InteractorVisualizer& v) { v.Draw(*this); }

// 80207A10
// A tile of the lot's inside (flag 1) that is not locked (0x20); laying a pattern (not 0) needs every
// object there to allow floors under it (data 0x2A bit 0).
int InteractorModule__FloorPainter::IsValidFloorPlacement(CTilePt& tile, int pattern) {
  if ((World()->GetFlags(tile) & 0x21) != 1) return 0;
  if (pattern != 0)
    for (cXObjectImpl* o : ibuild::ObjectsOnTile(tile))
      if (!(o->GetData(0x2a) & 1)) return 0;
  return 1;
}

// 80207AE0
// The tile's floor set to `pattern` (a diagonal tile: both halves in its wall entry, its floor 0xFF); the
// old patterns counted. 1: it changed.
int InteractorModule__FloorPainter::SetFloorTile(CTilePt* tile, int pattern, int* counts) {
  if (!IsValidFloorPlacement(*tile, pattern)) return 0;
  cFixedWorldImpl* w = World();
  TileWalls walls = w->GetWall(*tile);
  int changed = 0;
  if (!walls.HasDiagonal()) {
    const int old = w->SetFloor(*tile, pattern);
    if (old != pattern) {
      if (old != 0) ++counts[old];
      changed = 1;
    }
  } else {
    int sideA, sideB;
    if (walls.HasWall(0x20)) sideA = 3, sideB = 1;
    else sideA = 4, sideB = 2;
    const int oldA = walls.SetFloorValue(static_cast<unsigned short>(pattern), sideA);
    if (oldA != pattern) {
      if (oldA != 0) ++counts[oldA];
      changed = 1;
    }
    const int oldB = walls.SetFloorValue(static_cast<unsigned short>(pattern), sideB);
    if (oldB != pattern) {
      if (oldB != 0) ++counts[oldB];
      changed = 1;
    }
    w->SetWall(*tile, walls);
    w->SetFloor(*tile, 0xff);
  }
  return changed;
}

// 80207C94
// As above for a tile of `room`: a diagonal tile only on the halves inside the room.
int InteractorModule__FloorPainter::SetFloorTile(CTilePt& tile, int pattern, Room* room, int* counts) {
  if (!IsValidFloorPlacement(tile, pattern)) return 0;
  cFixedWorldImpl* w = World();
  TileWalls walls = w->GetWall(tile);
  int changed = 0;
  if (!walls.HasDiagonal()) {
    const int old = w->SetFloor(tile, pattern);
    if (old != pattern) {
      if (old != 0) ++counts[old];
      changed = 1;
    }
  } else {
    DiagonalSideSelector a{0}, b{0};
    CheckDiagForRoomContainment(room, tile, walls, a, b);
    int sideB = b;
    if (a != 0 && b != 0) {
      const int old = walls.SetFloorValue(static_cast<unsigned short>(pattern), a);
      if (old != pattern) {
        if (old != 0) ++counts[old];
        changed = 1;
      }
    } else if (a != 0) {
      sideB = a;
    }
    const int old = walls.SetFloorValue(static_cast<unsigned short>(pattern), sideB);
    if (old != pattern) {
      if (old != 0) ++counts[old];
      changed = 1;
    }
    w->SetWall(tile, walls);
    w->SetFloor(tile, 0xff);
  }
  return changed;
}

// 80207E7C
// The tile's purchasable floor taken up (pattern 0) and counted (a diagonal tile: each half).
void InteractorModule__FloorPainter::RemoveFloorTile(CTilePt* tile, int* counts) {
  if (!IsValidFloorPlacement(*tile, 0)) return;
  cFixedWorldImpl* w = World();
  TileWalls walls = w->GetWall(*tile);
  if (!walls.HasDiagonal()) {
    const int p = w->GetFloor(*tile);
    if (Purchasable(p)) {
      ++counts[p];
      w->SetFloor(*tile, 0);
    }
    return;
  }
  int sideA, sideB;
  if (walls.HasWall(0x20)) sideA = 3, sideB = 1;
  else sideA = 4, sideB = 2;
  const int pa = walls.GetFloorValue(sideA);
  const bool a = Purchasable(pa);
  if (a) {
    ++counts[pa];
    walls.SetFloorValue(0, sideA);
  }
  const int pb = walls.GetFloorValue(sideB);
  const bool b = Purchasable(pb);
  if (b) {
    ++counts[pb];
    walls.SetFloorValue(0, sideB);
  }
  if (a || b) {
    w->SetWall(*tile, walls);
    w->SetFloor(*tile, 0xff);
  }
}

// 80208080
// The rectangle between the anchor and the cursor (x from the engine's y).
void InteractorModule__FloorPainter::GetSelectedRectPoints(int& x0, int& y0, int& x1, int& y1) {
  x0 = static_cast<int>(m_savedPos.y < m_anchor.y ? m_savedPos.y : m_anchor.y);
  y0 = static_cast<int>(m_savedPos.x < m_anchor.x ? m_savedPos.x : m_anchor.x);
  x1 = static_cast<int>(m_anchor.y < m_savedPos.y ? m_savedPos.y : m_anchor.y);
  y1 = static_cast<int>(m_anchor.x < m_savedPos.x ? m_savedPos.x : m_anchor.x);
}

// 8020812C
// The rectangle painted; when the floors could not be built, the floor array as it was and the error
// sound; else the paint sound, the rooms and the house recomputed. The tiles that changed.
int InteractorModule__FloorPainter::PaintFloor(int* counts) {
  cFixedWorldImpl* w = World();
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetFloorLayer());
  int x0, y0, x1, y1;
  GetSelectedRectPoints(x0, y0, x1, y1);
  const int pattern = m_params.index;
  int n = 0;
  for (int x = x0; x <= x1; ++x)
    for (int y = y0; y <= y1; ++y) {
      CTilePt t(x, y, 1);
      if (SetFloorTile(&t, pattern, counts)) ++n;
    }
  if (!EIFloor::TestCreateFloors()) {
    ibuild::RestoreArray(w->GetFloorLayer(), saved);
    play_sound(0x6e2778b7);
    return 0;
  }
  if (pattern != 0) play_sound(0x80e1a9cd);
  w->ComputeRooms(0);
  _globals.RecalcHouse();
  std::printf("[build] player %d laid floor %d on %d tile%s (%d,%d .. %d,%d)\n", m_player + 1, pattern, n, n == 1 ? "" : "s",
              x0, y0, x1, y1);
  return n;
}

// 802083A4
// The rectangle's floors taken up (as PaintFloor checks and rebuilds; the take-up sound).
int InteractorModule__FloorPainter::RemoveFloor(int* counts) {
  cFixedWorldImpl* w = World();
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetFloorLayer());
  int x0, y0, x1, y1;
  GetSelectedRectPoints(x0, y0, x1, y1);
  for (int x = x0; x <= x1; ++x)
    for (int y = y0; y <= y1; ++y) {
      CTilePt t(x, y, 1);
      RemoveFloorTile(&t, counts);
    }
  if (!EIFloor::TestCreateFloors()) {
    ibuild::RestoreArray(w->GetFloorLayer(), saved);
    play_sound(0x6e2778b7);
    return 0;
  }
  play_sound(m_params.index == 0 ? 0x66dede72 : 0x80e1a9cd);
  w->ComputeRooms(0);
  _globals.RecalcHouse();
  std::printf("[build] player %d took up the floor (%d,%d .. %d,%d)\n", m_player + 1, x0, y0, x1, y1);
  return 1;
}

// 80208654
// The cursor's room painted (not the outside, room 0): the tiles that changed, less those that could not.
int InteractorModule__FloorPainter::PaintRoom(int* counts) {
  cFixedWorldImpl* w = World();
  unsigned short id = 0;
  Room* room = CursorRoom(m_savedPos, id);
  if (!room || !id || room->m_tiles.empty()) return 0;
  const int pattern = m_params.index;
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetFloorLayer());
  int failed = 0;
  std::vector<CTilePt> tiles = room->m_tiles;  // ComputeRooms below remakes the room
  for (CTilePt& t : tiles)
    if (!SetFloorTile(t, pattern, room, counts)) ++failed;
  if (!EIFloor::TestCreateFloors()) {
    ibuild::RestoreArray(w->GetFloorLayer(), saved);
    play_sound(0x6e2778b7);
    return -failed;
  }
  play_sound(pattern == 0 ? 0x66dede72 : 0x80e1a9cd);
  w->ComputeRooms(0);
  _globals.RecalcHouse();
  std::printf("[build] player %d laid floor %d in room %d (%d tiles)\n", m_player + 1, pattern, id,
              static_cast<int>(tiles.size()) - failed);
  return static_cast<int>(tiles.size()) - failed;
}

// 80208954
// The cursor's room's floors taken up.
int InteractorModule__FloorPainter::RemoveRoom(int* counts) {
  cFixedWorldImpl* w = World();
  unsigned short id = 0;
  Room* room = CursorRoom(m_savedPos, id);
  if (!room || !id || room->m_tiles.empty()) return 1;
  const std::vector<unsigned char> saved = ibuild::SaveArray(w->GetFloorLayer());
  std::vector<CTilePt> tiles = room->m_tiles;
  for (CTilePt& t : tiles) RemoveFloorTile(&t, counts);
  if (!EIFloor::TestCreateFloors()) {
    ibuild::RestoreArray(w->GetFloorLayer(), saved);
    play_sound(0x6e2778b7);
    return 0;
  }
  play_sound(m_params.index == 0 ? 0x66dede72 : 0x80e1a9cd);
  w->ComputeRooms(0);
  _globals.RecalcHouse();
  return 1;
}

// 80208C74
// A diagonal tile's halves in the room: the half towards x-1 (side 1, or 2 with the 0x10 diagonal) and
// the one towards x+1 (3, or 4).
void InteractorModule__FloorPainter::CheckDiagForRoomContainment(Room* room, CTilePt& tile, TileWalls& walls,
                                                                 DiagonalSideSelector& a, DiagonalSideSelector& b) {
  a = 0;
  b = 0;
  if (!walls.HasDiagonal() || !room) return;
  CTilePt left(tile.GetX() - 1, tile.GetY(), 1), right(tile.GetX() + 1, tile.GetY(), 1);
  const int inLeft = room->IsTileInRoom(left), inRight = room->IsTileInRoom(right);
  if (inLeft) a = walls.HasWall(0x10) ? 2 : 1;
  if (inRight) b = walls.HasWall(0x10) ? 4 : 3;
}

// 80208DA0
// The callback told 3 (the tool left), with the stock changes.
void InteractorModule__FloorPainter::CancelSession() {
  InteractorModule__Interactor__CallbackData data;
  data.m_stock = m_stockChanges;
  data.m_value = IsInRemoveMode() ? 1 : 0;
  data.m_state = 3;
  data.m_user = m_params.user;
  if (m_params.callback) m_params.callback(&data);
}

// 80208E20
// The callback told 4 (the next tool).
void InteractorModule__FloorPainter::SwapTools() {
  InteractorModule__Interactor__CallbackData data;
  data.m_stock = m_stockChanges;
  data.m_value = IsInRemoveMode() ? 1 : 0;
  data.m_state = 4;
  data.m_user = m_params.user;
  if (m_params.callback) m_params.callback(&data);
}

// 80208ECC
// Free: valid. A rectangle: its tiles whose floor may change to the pattern (painting: some, within the
// stock; removing: some). A room (not the outside): its tiles likewise (GC tests the placement at the
// cursor's tile for each).
bool InteractorModule__FloorPainter::CurrentSelectionIsValid(int& count) {
  count = 0;
  cFixedWorldImpl* w = World();
  int n = 0;
  const int pattern = m_params.index;
  if (!(m_state & 1)) {
    if (!(m_state & 2)) return true;
    CTilePt cursor(static_cast<int>(m_savedPos.y), static_cast<int>(m_savedPos.x), 1);
    unsigned short id = 0;
    Room* room = CursorRoom(m_savedPos, id);
    if (room && id)
      for (CTilePt& t : room->m_tiles) {
        const int p = w->GetFloor(t);
        if (IsValidFloorPlacement(cursor, p) && p != pattern) ++n;
      }
    count = n;
    if (m_params.place) return n <= m_stock;
  } else {
    int x0, y0, x1, y1;
    GetSelectedRectPoints(x0, y0, x1, y1);
    for (int x = x0; x <= x1; ++x)
      for (int y = y0; y <= y1; ++y) {
        CTilePt t(x, y, 1);
        const int p = w->GetFloor(t);
        if (IsValidFloorPlacement(t, p) && p != pattern) ++n;
      }
    count = n;
    if (m_params.place) return n <= m_stock && n != 0;
  }
  return n >= 1;
}

// 8020913C
int InteractorModule__FloorPainter::IsInAnchorState() { return m_state & 1; }
// 80209148
bool InteractorModule__FloorPainter::IsInFreeState() { return m_state == 0; }
// 80209158
int InteractorModule__FloorPainter::IsInBuildState() { return (m_state >> 2) & 1; }
// 80209164
int InteractorModule__FloorPainter::IsInRemoveState() { return (m_state >> 3) & 1; }
// 80209170
int InteractorModule__FloorPainter::IsInFillState() { return (m_state >> 1) & 1; }
// 8020917C
int InteractorModule__FloorPainter::IsInConfirmedState() { return (m_state >> 4) & 1; }
// 80209188
int InteractorModule__FloorPainter::IsInInvalidState() { return (m_state >> 5) & 1; }
// 80209194
EVec3& InteractorModule__FloorPainter::GetAnchorPos() { return m_anchor; }
// 8020919C
const void* InteractorModule__FloorPainter::GetSelectedFloorData() { return m_params.data; }
// 802091A4
int InteractorModule__FloorPainter::IsInPlaceMode() { return m_params.place; }
// 802091AC
bool InteractorModule__FloorPainter::IsInRemoveMode() { return m_params.place == 0; }
