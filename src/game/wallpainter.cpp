// Port of game/wallpainter.cpp: the wallpaper tool (InteractorModule::WallPainter, the player's interactor
// 4; see buildtools.hpp). A anchors the cursor's corner, a second A papers the face of the walls along
// the line that the camera sees (the catalogue's sell tool strips it: pattern 0); the old patterns are
// counted as sold (the catalogue refunds them), the stock limits the faces papered. With the trigger held
// the room under the cursor is the selection (PC: not ported, see below).
//
// PC: InteractorVisualizer (the cursor model, its rotation, the line's preview) is not ported; the room
// selection (CountWallsInRoomSelection, FinalizePaperForRoom: ERoom::GetNumWalls and the room's wall
// sides) is SIMMY_STUB; the lot data (HouseImpl vtable 0x8C) is the world side's.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/buildtools.hpp"

#include <cmath>
#include <cstdio>

bool play_sound(unsigned int id);  // pcttarget.cpp (801D1DDC)
extern int g_freeBuildBuyCheat;    // DAT_80476188

namespace {
cFixedWorldImpl* World() { return Globs_pFixedWorld; }
// 804FFB2C InteractorModule::kCursorBaseAdditionalRotation (80226914: pi / 2).
const float kCursorBaseAdditionalRotation = 1.5707964f;
// 805006C4 m_paperToolHeight, 805006C8 m_wallHeight.
const float kPaperToolHeight = 3.5f;
const float kWallHeight = 3.0f;
int PaperIndex(const void* paper) { return static_cast<int>(_globals.GetWallIndex(reinterpret_cast<WallTile*>(const_cast<void*>(paper)))); }
}  // namespace

// 80224060 InteractorModule::WMNullFunc: the tool's debug messages (nothing).

// 80224064
InteractorModule__WallPainter::InteractorModule__WallPainter() { m_params.id = 4; }
// 802240D4
InteractorModule__WallPainter::~InteractorModule__WallPainter() {}
// 80224180
// GC: the visualizer's resources; the extents.
void InteractorModule__WallPainter::OnCreate() {
  m_extents.x = 0.5f;
  m_extents.y = 3.0f;
}
// 802241CC
void InteractorModule__WallPainter::OnDestroy() {}

// 80224200 OnStart
// The cursor at the camera's target; the params kept: the paper, the stock; the sell tool (+0xC0) strips
// (8), else it papers (4). GC: the paper's shader into the visualizer's resource set.
void InteractorModule__WallPainter::Start(BuildToolParams& params) {
  m_flags = 0;
  m_player = params.player;
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
  if (cam) m_pos = EVec3{cam->m_target.x, cam->m_target.y, 0.0f};
  ibuild::ClampPosToWorld(m_pos);
  m_leftStick = EVec2{0.0f, 0.0f};
  m_rightStick = EVec2{0.0f, 0.0f};
  m_cursorRotation = 0.0f;
  m_cam = nullptr;
  m_params = params;
  m_startStock = params.stock;
  m_paper = params.data;
  m_stock = params.stock;
  m_count = 0;
  m_patternSells.clear();
  m_flags |= m_params.sell == 1 ? 8 : 4;
}

// 8022448C
void InteractorModule__WallPainter::OnStop() {}

// 802244C8
void InteractorModule__WallPainter::CancelSession() { CommittTransaction(3); }

// 802244EC
// The callback told `state` with the paper's index, the stock used since the last commit and the patterns
// sold (the list emptied).
void InteractorModule__WallPainter::CommittTransaction(int state) {
  CallbackData data;
  data.m_value = m_params.index;
  data.m_count = m_stock == -1 ? 0 : m_stock - m_startStock;
  data.m_sells = m_patternSells;
  m_patternSells.clear();
  data.m_user = m_params.user;
  data.m_state = state;
  if (m_params.callback) m_params.callback(&data);
  m_count = 0;
  m_startStock = m_stock;
}

// 80224838
// A valid selection papered (room or line) or stripped; done: the paper (or strip) sound, the anchor back
// at the cursor and unanchored, the callback told 0. Else the error sound.
void InteractorModule__WallPainter::HandleFinalizeRequest() {
  m_patternSells.clear();
  int done = 0;
  const unsigned int f = m_flags;
  if (f & 0x80) {
    if (f & 0x10) done = FinalizePaperForRoom();
    else if (f & 4) done = FinalizePaperForLine();
    else if (f & 8) done = FinalizeSellPaperForLine();
  }
  m_flags &= ~0x30u;
  if (done == 1) {
    play_sound((f & 4) ? 0x80e1a9cd : 0x66dede72);
    m_anchor = m_pos;
    m_flags &= ~0x1001u;
    CommittTransaction(0);
  } else {
    play_sound(0x6e2778b7);
  }
}

// 802249C4
void InteractorModule__WallPainter::HandleSwapRequest() {
  m_flags = 0;
  play_sound(0x0064ac15);
  CommittTransaction(4);
}

// 80224A3C
void InteractorModule__WallPainter::HandleExitRequest() {
  if (!(m_flags & 1)) {
    m_flags = 0;
    CancelSession();
  } else {
    m_flags &= ~0x41u;
  }
}

// 80224AAC
// The cursor snapped to the corner; the cursor model turned a quarter either way by the visible side.
void InteractorModule__WallPainter::AdjustCursorPosition() {
  ibuild::SnapPositionToTile(m_pos, m_savedPos, 2);
  if (m_anchor.x == m_savedPos.x && m_anchor.y == m_savedPos.y && m_anchor.z == m_savedPos.z) return;
  const int side = ibuild::GetVisibleSideOfWall(_gd.m_players[m_player & 1].m_cam, m_anchor, m_savedPos, nullptr);
  float r = kCursorBaseAdditionalRotation;
  const bool before = m_anchor.y < m_savedPos.y || (m_savedPos.y == m_anchor.y && m_anchor.x < m_savedPos.x);
  if (side == 0) {
    if (before) r = -kCursorBaseAdditionalRotation;
  } else if (!before) {
    r = -kCursorBaseAdditionalRotation;
  }
  m_cursorRotation = r;
}

// 80224BB8
// A room: valid with walls (selling, or within the stock). A line along x, y or a diagonal: the faces it
// covers on the visible side (EorGetAdjacentTile) counted, papering only those of another pattern (within
// the stock), stripping only papered ones (their patterns counted as sold).
void InteractorModule__WallPainter::ValidateWallSegment() {
  bool valid = false;
  m_patternSells.clear();
  m_count = 0;
  if (m_flags & 0x10) {
    CountWallsInRoomSelection();
    valid = m_count != 0 && ((m_flags & 8) || m_count <= m_stock || m_stock == -1);
  } else {
    EVec3 from = m_anchor, to = m_savedPos;
    const int side = ibuild::GetVisibleSideOfWall(m_cam, from, to, nullptr);
    float dx = to.x - from.x, dy = to.y - from.y;
    const float l = std::sqrt(dx * dx + dy * dy);
    if (dx != 0.0f || dy != 0.0f) dx /= l, dy /= l;
    float d = std::fabs(dx);
    d = dy < 0.0f ? d + dy : d - dy;
    d = std::fabs(d);
    if (dx == 0.0f || dy == 0.0f || d < 0.001f) {
      valid = true;
      cFixedWorldImpl* w = World();
      if (to.x != from.x || to.y != from.y || to.z != from.z) {
        CTilePt a, b;
        ibuild::ConvertVertsToTiles(from, to, a, b);
        ibuild::ForcePointDir(a, b);
        const int dir = CTilePt::GetTileDirection(a, b);
        if (dir != 8) {
          int seg = TileWalls::DirToWallSeg(dir);
          int wallSide = 0;
          ibuild::EorGetAdjacentTile(seg, side == 0, wallSide, a, b);
          const int index = PaperIndex(m_paper);
          for (;;) {
            if (w->OutOfBounds(a) || a == b) break;
            TileWalls walls = w->GetWall(a);
            if (walls.HasWallNotFence(seg)) {
              const int pattern = walls.GetPattern(seg, wallSide);
              if (m_flags & 4) {
                if (pattern != index) ++m_count;
              } else if ((m_flags & 8) && pattern != 0) {
                ibuild::IncrementSellCountForPattern(pattern, m_patternSells);
                ++m_count;
              }
            }
            if ((m_flags & 4) && m_stock != -1 && m_stock < m_count) {
              valid = false;
              break;
            }
            a += CTilePt::sDirections[dir];
            a.m_level = 1;
          }
          if (m_count == 0) valid = false;
        }
      } else {
        valid = false;
      }
    }
  }
  if (valid) m_flags |= 0x80;
  else m_flags &= ~0x80u;
}

// 80225044
void InteractorModule__WallPainter::UpdateStickState() {
  if (m_leftStick.x == 0.0f && m_leftStick.y == 0.0f) {
    m_flags &= ~0x300u;
    return;
  }
  if (m_flags & 0x100) m_flags &= ~0x200u;
  else m_flags |= 0x300;
}

// 80225098
// The anchor kept while the room preview is up (0x400 holds it, 0x800 puts it back); the cursor: in a
// room, on its tile's centre (the room under it); else moved (a tile on the stick's first push, a tile on
// from a new anchor), kept on the lot, and the line checked while anchored. Then the requests and the
// count for the catalogue's price.
void InteractorModule__WallPainter::Update(float dt) {
  m_patternSells.clear();
  InteractorModule__Interactor::Update(dt);
  UpdateStickState();
  m_cursorRotation = 0.0f;
  const unsigned int f = m_flags;
  if (f & 0x400) {
    if (!(f & 0x1000)) {
      m_hold = m_anchor;
      m_flags |= 0x1000;
    }
    m_flags &= ~0x400u;
  } else if (f & 0x800) {
    if (!(f & 0x1000)) {
      m_anchor = EVec3{0.0f, 0.0f, 0.0f};
    } else {
      m_anchor = m_hold;
      m_flags &= ~0x1000u;
    }
    m_anchorDraw = m_anchor;
    m_flags &= ~0x800u;
  }
  if (ESimsCam* cam = m_cam) {
    if (!(m_flags & 0x10)) {
      if (!(m_flags & 2)) {
        if (!(m_flags & 0x200)) ibuild::MoveCursorAlongCameraTargetVector(static_cast<float>(cam->GetCursorSpeed()), m_pos, m_leftStick, cam);
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
      if (m_flags & 1) {
        AdjustCursorPosition();
        ValidateWallSegment();
      } else {
        ibuild::SnapPositionToTile(m_pos, m_savedPos, 2);
        m_anchor = m_savedPos;
        m_anchorDraw = m_savedPos;
        m_count = 0;
      }
    } else {
      ibuild::SnapPositionToTile(m_pos, m_savedPos, 1);
      m_anchor = m_savedPos;
      m_anchorDraw = m_savedPos;
      ValidateWallSegment();
    }
  }
  if (m_flags & 0x20) HandleFinalizeRequest();
  if (m_flags & 0x2000) HandleSwapRequest();
  if (!(m_flags & 4)) SendBuildItemCountChangeEvent(m_stock + m_count);
  else SendBuildItemCountChangeEvent(m_count);
  if (m_flags & 0x40) HandleExitRequest();
}

// 8022544C
// 1 (Y) swaps tools from the strip tool; 0 (A) anchors or finalizes; 2 (the trigger) the room preview
// while unanchored; 4 (B) exit.
void InteractorModule__WallPainter::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand command, float) {
  const unsigned int f = m_flags;
  switch (static_cast<int>(command)) {
    case 1:
      if (f & 8) m_flags = (f & ~1u) | 0x2000;
      return;
    case 0:
      if (!(f & 0x11)) {
        if (!(f & 1)) {
          m_flags = f | 3;
          play_sound(0x374bdcea);
        }
      } else {
        m_flags = f | 0x20;
      }
      return;
    case 2:
      if (!(f & 1) && !(f & 0x10)) m_flags |= 0x10;
      return;
    case 4:
      m_flags |= 0x40;
      return;
    default:
      return;
  }
}

// 80225594
void InteractorModule__WallPainter::OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand command) {
  switch (static_cast<int>(command)) {
    case 8: m_leftStick.y = 0.0f; break;
    case 7: m_leftStick.x = 0.0f; break;
    case 0xb: m_rightStick.x = 0.0f; break;
    case 0xc: m_rightStick.y = 0.0f; break;
    case 2: {
      const unsigned int f = m_flags;
      if (f & 0x10) {
        m_flags = f & ~0x10u;
        if (f & 0x1000) m_flags = (f & ~0x10u) | 0x800;
      }
      break;
    }
    default: break;
  }
}

// 80225658
void InteractorModule__WallPainter::OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command, float value) {
  switch (static_cast<int>(command)) {
    case 8: m_leftStick.y = value; break;
    case 7: m_leftStick.x = value; break;
    case 0xb: m_rightStick.x = value; break;
    case 0xc: m_rightStick.y = value; break;
    default: break;
  }
}

// 802256A4
void InteractorModule__WallPainter::PreDraw(InteractorModule__InteractorVisualizer&) {}
// 802256D0
void InteractorModule__WallPainter::Draw(InteractorModule__InteractorVisualizer&) {}

// 802256FC
void InteractorModule__WallPainter::CountWallsInRoomSelection() {
  SIMMY_STUB("WallPainter::CountWallsInRoomSelection: the room's wall sides (ERoom::GetNumWalls, the room's tiles)", 0x802256FC);
  m_count = 0;
}

// 80225D70
int InteractorModule__WallPainter::FinalizePaperForRoom() {
  SIMMY_STUB("WallPainter::FinalizePaperForRoom: papering the room's wall sides", 0x80225D70);
  return 0;
}

// 802262B8
// The line papered with the paper's pattern on its visible side; the rooms and the house recomputed.
int InteractorModule__WallPainter::FinalizePaperForLine() {
  const int side = ibuild::GetVisibleSideOfWall(_gd.m_players[m_player & 1].m_cam, m_anchor, m_savedPos, nullptr);
  const int r = SubmitPaperLine(m_anchor, m_savedPos, PaperIndex(m_paper), side == 0);
  if (r == 1) {
    if (Globs_pRoomManager) Globs_pRoomManager->ComputeRooms();
    World()->ComputeRooms(0);
    _globals.RecalcHouse();
    std::printf("[build] player %d papered %d wall side%s with pattern %d (stock %d)\n", m_player + 1, m_count,
                m_count == 1 ? "" : "s", PaperIndex(m_paper), m_stock);
  }
  return r;
}

// 80226390
// The line stripped (pattern 0).
int InteractorModule__WallPainter::FinalizeSellPaperForLine() {
  const int side = ibuild::GetVisibleSideOfWall(_gd.m_players[m_player & 1].m_cam, m_anchor, m_savedPos, nullptr);
  const int r = SubmitPaperLine(m_anchor, m_savedPos, 0, side == 0);
  if (r == 1) {
    if (Globs_pRoomManager) Globs_pRoomManager->ComputeRooms();
    World()->ComputeRooms(0);
    _globals.RecalcHouse();
    std::printf("[build] player %d stripped %d wall side%s\n", m_player + 1, m_count, m_count == 1 ? "" : "s");
  }
  return r;
}

// 80226458
// Each wall face of the line (on the side `front` picks) on an editable tile takes `pattern`; the old
// pattern is counted as sold (papering: unless it is the same paper). Papering takes the selection's
// count from the stock (refused beyond it, but with the free cheat). 1: done.
int InteractorModule__WallPainter::SubmitPaperLine(EVec3& from, EVec3& to, int pattern, int front) {
  cFixedWorldImpl* w = World();
  const int index = PaperIndex(m_paper);
  const unsigned int buy = (m_flags >> 2) & 1;
  CTilePt a, b;
  ibuild::ConvertVertsToTiles(from, to, a, b);
  ibuild::ForcePointDir(a, b);
  const int dir = CTilePt::GetTileDirection(a, b);
  if (dir == 8) return 0;
  int seg = TileWalls::DirToWallSeg(dir);
  CTilePt t = a, end = b;
  int side = 0;
  ibuild::EorGetAdjacentTile(seg, front, side, t, end);
  bool charge = true;
  if (m_flags & 4) {
    if (m_stock == -1) charge = false;
    else if (m_stock < m_count && !g_freeBuildBuyCheat && !EGlobal::IsBuildHouseMode()) return 0;
  }
  if (charge && m_stock != -1 && buy == 1) m_stock -= m_count;
  for (int n = 0;;) {
    TileWalls walls = w->GetWall(t);
    if (walls.HasWall(seg) && w->MayEditTile(t)) {
      const int old = walls.GetPattern(seg, 0);
      if (buy == 0) {
        if (old) ibuild::IncrementSellCountForPattern(old, m_patternSells);
      } else if (old != index && old) {
        ibuild::IncrementSellCountForPattern(old, m_patternSells);
      }
      walls.SetPattern(static_cast<unsigned short>(pattern), seg, side);
      w->SetWall(t, walls);
    }
    t = t + CTilePt::sDirections[dir];
    t.m_level = 1;
    const bool done = w->OutOfBounds(t) || t == end;
    ++n;
    if (n >= w->GetMaxSize() || done) break;
  }
  // GC: Globs::pHouse vtable 0x8C ComputeAndStoreLotData (the world side's).
  return 1;
}

// 80226830
unsigned int InteractorModule__WallPainter::GetShaderID() { return m_paper ? ERQuickdata::U32(m_paper, 8) : 0; }

// 8022683C
// The callback told 5 with the selection's count and the patterns it would sell.
void InteractorModule__WallPainter::SendBuildItemCountChangeEvent(int count) {
  CallbackData data;
  data.m_count = count;
  data.m_sells = m_patternSells;
  data.m_state = 5;
  data.m_user = m_params.user;
  if (m_params.callback) m_params.callback(&data);
}

// 80226948
int InteractorModule__WallPainter::IsAnchored() { return (m_flags & 1) ? 1 : 0; }
// 80226960
int InteractorModule__WallPainter::IsValidWallSegment() { return (m_flags >> 7) & 1; }
// 8022696C
int InteractorModule__WallPainter::IsRoomPreview() { return (m_flags >> 4) & 1; }
// 80226978
int InteractorModule__WallPainter::IsBuyMode() { return (m_flags >> 2) & 1; }
// 80226984
int InteractorModule__WallPainter::IsSellMode() { return (m_flags >> 3) & 1; }
// 80226990
double InteractorModule__WallPainter::GetCursorRotationAdjustment() { return m_cursorRotation; }
// 80226998
void InteractorModule__WallPainter::SetAnchorPoint(EVec3& p) { m_anchor = p; }
// 802269B8
void InteractorModule__WallPainter::SetAnchorPointDrawPos(EVec3& p) { m_anchorDraw = p; }
// 802269D8
void InteractorModule__WallPainter::SetHoldPos(EVec3& p) { m_hold = p; }
// 802269F8
EVec3& InteractorModule__WallPainter::GetAnchorPos() { return m_anchor; }
// 80226A00
EVec3& InteractorModule__WallPainter::GetAnchorDrawPos() { return m_anchorDraw; }
// 80226A08
EVec3& InteractorModule__WallPainter::GetHoldPos() { return m_hold; }
// 80226A10
double InteractorModule__WallPainter::GetPaperToolHeight() { return kPaperToolHeight; }
// 80226A18
double InteractorModule__WallPainter::GetWallHeight() { return kWallHeight; }
