// Build mode's tools, shared parts: game/interactorutils.cpp's cursor and wall helpers (InteractorModule::
// SnapPositionToTile, ClampPosToWorld, MoveCursor*AlongCameraTargetVector, ConvertVertsToTiles,
// GetVisibleSideOfWall, ForcePointDir, EorGetAdjacentTile, IncrementSellCountForPattern), the camera's
// cursor speed, EGlobal's set lookups, and the PC glue that runs a player's tool (see buildtools.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/buildtools.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

namespace ibuild {

// 80212FE8
// `pos` moved `n` tiles in direction `dir` (TilePtDir: 0 -x, 1 +x, 2 -y, 3 +y, 4 -x -y, 5 +x +y,
// 6 -x +y, 7 +x -y), on the ground.
void GetAdjacentTileCoordinates(int dir, int n, EVec3& pos) {
  const float d = static_cast<float>(n);
  switch (dir) {
    case 0: pos.x -= d; break;
    case 1: pos.x += d; break;
    case 2: pos.y -= d; break;
    case 3: pos.y += d; break;
    case 4: pos.y -= d, pos.x -= d; break;
    case 5: pos.y += d, pos.x += d; break;
    case 6: pos.y += d, pos.x -= d; break;
    case 7: pos.y -= d, pos.x += d; break;
    default: break;
  }
  pos.z = 0.0f;
}

// 802106C4
// Snap 0 none; else the tile's corner (2), centre (1), or the middle of its x (3) or y (4) edge.
void SnapPositionToTile(const EVec3& in, EVec3& out, int snap) {
  if (snap == 0) {
    out.x = in.x;
    out.y = in.y;
  } else {
    int x = static_cast<int>(in.x) << 4, y = static_cast<int>(in.y) << 4;
    if (snap == 1) x |= 8, y |= 8;
    else if (snap == 3) y |= 8;
    else if (snap == 4) x |= 8;
    out.x = static_cast<float>(x) * 0.0625f;
    out.y = static_cast<float>(y) * 0.0625f;
  }
  out.z = in.z;
}

// 80210F7C
// Inside [1, size - 1] on both axes (x by the world's y size, y by its x size: the engine's x is the
// tiles' y).
void ClampPosToWorld(EVec3& pos) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w) return;
  const float xMax = static_cast<float>(w->GetYSize()) - 1.0f, yMax = static_cast<float>(w->GetXSize()) - 1.0f;
  float x = pos.x;
  if (x < 1.0f) x = 1.0f;
  else if (x > xMax) x = xMax;
  float y = pos.y;
  if (y < 1.0f) y = 1.0f;
  else if (y > yMax) y = yMax;
  pos.x = x;
  pos.y = y;
}

namespace {
// The stick turned into the ground's frame: rotated by the camera's heading (atan2 of its view
// direction's x and y; EMat4::RotateZ(-angle)).
void StickToGround(ESimsCam* cam, float sx, float sy, float& gx, float& gy) {
  float dx = cam->m_target.x - cam->m_eye.x, dy = cam->m_target.y - cam->m_eye.y, dz = cam->m_target.z - cam->m_eye.z;
  const float l = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (l != 0.0f) dx /= l, dy /= l;
  const float a = std::atan2(dx, dy);
  const float c = std::cos(a), s = std::sin(a);
  gx = sx * c + sy * s;
  gy = -sx * s + sy * c;
}
}  // namespace

// 80210C58
// A tile in the stick's direction (as the camera sees the ground): snapped first; snap 2 moves a whole
// tile on each axis the direction leans on by more than 22.5 degrees, snap 1 1.42 tiles along it.
// GC: then ESimsCam::ValidateCursorPosition (PC: once per frame after the interactors,
// UpdateDirectInteractors; with the shipped tuning it only recomputes the cursor area, pinning nothing).
void MoveCursorOneTileAlongCameraTargetVector(EVec3& pos, const EVec2& stick, ESimsCam* cam, int snap) {
  if (!cam) return;
  float sx = stick.x, sy = stick.y;
  SnapPositionToTile(pos, pos, snap);
  const float l = std::sqrt(sx * sx + sy * sy);
  if (l != 0.0f) sx /= l, sy /= l;
  if (sx == 0.0f && sy == 0.0f) return;
  float gx, gy;
  StickToGround(cam, sx, sy, gx, gy);
  if (snap == 2) {
    if (gx > 0.3827f) pos.x += 1.0f;
    else if (gx < -0.3827f) pos.x -= 1.0f;
    if (gy > 0.3827f) pos.y += 1.0f;
    else if (gy < -0.3827f) pos.y -= 1.0f;
  } else if (snap == 1) {
    pos.x += gx * 1.42f;
    pos.y += gy * 1.42f;
    SnapPositionToTile(pos, pos, 1);
  }
}

// 8021097C
// The stick (each axis squared, keeping its sign, in place) times the speed and the frame time (at most
// 0.1 s), sped up (the camera parameters' +0x80) past the push +0x84 squared, in the ground's frame.
void MoveCursorAlongCameraTargetVector(float speed, EVec3& pos, EVec2& stick, ESimsCam* cam) {
  if (!cam) return;
  stick.x *= std::fabs(stick.x);
  stick.y *= std::fabs(stick.y);
  float dt = _dt;
  if (dt > 0.1f) dt = 0.1f;
  float mx = stick.x * speed * dt, my = stick.y * speed * dt;
  const SimsCameraParameters* p = cam->m_params;
  const float edge = p ? p->m_field084 * p->m_field084 : 1.0f;
  if (p && (edge < stick.x || stick.x < -edge)) mx *= p->m_field080;
  if (p && (edge < stick.y || stick.y < -edge)) my *= p->m_field080;
  if (mx != 0.0f || my != 0.0f) {
    float gx, gy;
    StickToGround(cam, mx, my, gx, gy);
    pos.x += gx;
    pos.y += gy;
  }
}

// 80212D58
// A wall's two corner vertices to the tiles of its first and last segment (CTilePt x from the engine's
// y, y from its x, level 0): the vertices' half-tile offsets depend on the direction of the line.
void ConvertVertsToTiles(const EVec3& from, const EVec3& to, CTilePt& a, CTilePt& b) {
  float ax = from.x - 0.5f, ay = from.y + 0.5f;
  float bx = to.x - 0.5f, by = to.y + 0.5f;
  const float dx = to.x - from.x, dy = to.y - from.y;
  const int sx = dx < 0.0f ? -1 : 1, sy = dy < 0.0f ? -1 : 1;
  bool addX = true;
  if (dx == 0.0f) {
    if (sy > 0) ay += 1.0f, by += 1.0f;
    ay -= 1.0f, by -= 1.0f;
  } else if (dy != 0.0f) {
    addX = false;
    if (sx < 0 && sy < 0) {
      ay -= 1.0f, by -= 1.0f;
    } else if (sx >= 1) {
      if (sy >= 0) {
        if (sy > 0) ax += 1.0f, bx += 1.0f;
      } else {
        ay -= 1.0f, by -= 1.0f;
        addX = true;
      }
    }
  } else {
    if (sx < 1) ax -= 1.0f, bx -= 1.0f;
  }
  if (addX) ax += 1.0f, bx += 1.0f;
  a.Set(ay, ax, 0);
  b.Set(by, bx, 0);
}

// 80212AD8
// The wall from `a` to `b` (ordered along its line), its normal, and whether the camera looks at the
// normal's side (the answer, 1: the far side); `offset` (when given) a quarter tile towards the visible
// side.
int GetVisibleSideOfWall(ESimsCam* cam, const EVec3& a, const EVec3& b, EVec3* offset) {
  float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
  if (a.x == b.x) {
    y0 = b.y < a.y ? b.y : a.y;
    y1 = b.y < a.y ? a.y : b.y;
  } else if (a.y == b.y) {
    x0 = b.x < a.x ? b.x : a.x;
    x1 = b.x < a.x ? a.x : b.x;
  } else {
    const bool swap = (b.y - a.y) / (b.x - a.x) <= 0.0f ? a.y > b.y : a.x > b.x;
    if (swap) {
      x0 = b.x, y0 = b.y, x1 = a.x, y1 = a.y;
    }
  }
  float ny = -(x1 - x0), nx = y1 - y0;
  const float l = std::sqrt(nx * nx + ny * ny);
  if (l != 0.0f) nx /= l, ny /= l;
  float cx = cam ? cam->m_eye.x - cam->m_target.x : 0.0f, cy = cam ? cam->m_eye.y - cam->m_target.y : 0.0f;
  const float cz = cam ? cam->m_eye.z - cam->m_target.z : 0.0f;
  const float cl = std::sqrt(cx * cx + cy * cy + cz * cz);
  if (cl != 0.0f) cx /= cl, cy /= cl;
  const float dot = cx * nx + cy * ny;
  float ox = nx * -0.25f, oy = ny * -0.25f;
  if (offset) {
    if (dot > 0.0f) ox = -ox, oy = -oy;
    offset->x = ox;
    offset->y = oy;
    offset->z = 0.0f;
  }
  return dot > 0.0f ? 1 : 0;
}

// 802132D8
// The tiles of a wallpaper line ordered from the lower end (the end tile moved past when they were
// swapped); a diagonal line from its lower y, both moved along the diagonal.
void ForcePointDir(CTilePt& a, CTilePt& b) {
  CTilePt p = a, q = b;
  if (p.m_x == q.m_x) {
    const signed char lo = p.m_y < q.m_y ? p.m_y : q.m_y, hi = q.m_y < p.m_y ? p.m_y : q.m_y;
    p.m_y = lo;
    q.m_y = hi;
    if (p != a) ++p.m_y, ++q.m_y;
  } else if (p.m_y == q.m_y) {
    const signed char lo = p.m_x < q.m_x ? p.m_x : q.m_x, hi = q.m_x < p.m_x ? p.m_x : q.m_x;
    p.m_x = lo;
    q.m_x = hi;
    if (p != a) ++p.m_x, ++q.m_x;
  } else {
    const float slope = static_cast<float>(q.m_y - p.m_y) / static_cast<float>(q.m_x - p.m_x);
    if (slope <= 0.0f) {
      if (q.m_y < p.m_y) std::swap(p, q);
      if (p != a) --p.m_x, --q.m_x, ++p.m_y, ++q.m_y;
    } else {
      if (q.m_x < p.m_x) std::swap(p, q);
      if (p != a) ++p.m_x, ++q.m_x, ++p.m_y, ++q.m_y;
    }
  }
  a = p;
  b = q;
}

// 8021358C
// The wallpaper side of a line's segment: a diagonal's side by `front`; seen from the back, segment 1's
// face is the x-1 tile's segment 4 (and segment 2's, seen from the front, the y-1 tile's 8): the
// segment and the line moved across (_PerpTilePointTab 803FD4BC: 2, 3, 1, 0, 7, 6, 4, 5).
void EorGetAdjacentTile(int& segment, int front, int& side, CTilePt& a, CTilePt& b) {
  static const int kPerp[9] = {2, 3, 1, 0, 7, 6, 4, 5, 8};
  const int dir = CTilePt::GetTileDirection(a, b);
  const CTilePt& d = CTilePt::sDirections[kPerp[dir] & 7];
  if (segment == 0x10) {
    side = front == 0 ? 4 : 2;
  } else if (segment == 0x20) {
    side = front == 0 ? 3 : 1;
  } else if (segment == 1 && front == 0) {
    segment = TileWalls::GetOppositeSegment(1);
    a -= d;
    b -= d;
  } else if (segment == 2 && front == 1) {
    segment = TileWalls::GetOppositeSegment(2);
    a += d;
    b += d;
  }
}

// 80213828
void IncrementSellCountForPattern(int pattern, std::vector<BuildSellCount>& counts) {
  for (BuildSellCount& c : counts)
    if (c.id == pattern) {
      ++c.count;
      return;
    }
  counts.push_back(BuildSellCount{pattern, 1});
}

// The build sets (EGlobal +0xD4 "FloorSet", +0xD8 "WallSet", +0xDC "FenceSet"): an array of row
// pointers with its count in the word before it.
const unsigned char* SetArray(const void* set) {
  return set ? static_cast<const unsigned char*>(ERQuickdata::Ptr(set, 0)) : nullptr;
}
int SetCount(const void* set) {
  const unsigned char* rows = SetArray(set);
  return rows ? static_cast<int>(ERQuickdata::U32(rows - 4)) : 0;
}
const void* SetRow(const void* set, int i) {
  const unsigned char* rows = SetArray(set);
  return rows && i >= 0 && i < SetCount(set) ? ERQuickdata::Ptr(rows, static_cast<unsigned int>(i) * 4) : nullptr;
}

// GC _c2DArray::CopyFrom (800D2320) for the tools' undo copies of a world array.
std::vector<unsigned char> SaveArray(const _c2DArray* a) {
  if (!a || !a->m_rows) return {};
  const size_t n = static_cast<size_t>(a->m_xSize) * a->m_ySize * a->m_elemSize;
  return std::vector<unsigned char>(a->m_rows[0], a->m_rows[0] + n);
}
void RestoreArray(_c2DArray* a, const std::vector<unsigned char>& saved) {
  if (a && a->m_rows && !saved.empty()) std::memcpy(a->m_rows[0], saved.data(), saved.size());
}

// The objects on a tile (GC: ObjectIterator over the tile's site): the module's objects at its x and y.
std::vector<cXObjectImpl*> ObjectsOnTile(const CTilePt& tile) {
  std::vector<cXObjectImpl*> out;
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return out;
  for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject) {
    if (o->m_location[0] < 0 || o->m_location[1] < 0 || o->m_data[2] != 0) continue;
    const CTilePt t = o->GetCTilePt();
    if (t.GetX() == tile.GetX() && t.GetY() == tile.GetY()) out.push_back(o);
  }
  return out;
}

// 8021374C SetCameraFilter / 802137C0 ClearCameraFilter
// The player's camera reads its C stick through the tool's input session (ESimsCam::SetControllerFilterId)
// while the catalogue's button filters block everything else; cleared when the tool stops. PC: the
// sessions read the pad unfiltered (~0).
void SetCameraFilter(int player) {
  if (ESimsCam* cam = _gd.m_players[player & 1].m_cam) cam->m_filterId = ~0u;
}
void ClearCameraFilter(int player) {
  if (ESimsCam* cam = _gd.m_players[player & 1].m_cam) cam->m_filterId = 0;  // ESimsCam::ClearControllerFilterId
}

int CountRoomsInUse() {
  int n = 0;
  if (Globs_pRoomManager)
    for (auto& e : Globs_pRoomManager->m_rooms)
      if (e.second && e.second->m_inUse) ++n;
  return n;
}

}  // namespace ibuild

// 8001A00C
// The cursor's speed at the camera's zoom: between the parameters' +0x58 and +0x5C across the zoom range.
double ESimsCam::GetCursorSpeed() {
  const SimsCameraParameters* p = m_params;
  if (!p || p->m_zoomMax == p->m_zoomMin) return 0.0;
  return (m_zoom - p->m_zoomMin) / (p->m_zoomMax - p->m_zoomMin) * (p->m_field05C - p->m_field058) + p->m_field058;
}

// 8004070C
RET_UNKNOWN EGlobal::GetWallIndex(WallTile* row) {
  const int n = ibuild::SetCount(m_wallSet);
  for (int i = 0; i < n; ++i)
    if (ibuild::SetRow(m_wallSet, i) == static_cast<const void*>(row)) return i;
  return 0;
}
// 800406C4
RET_UNKNOWN EGlobal::GetFloorIndex(FloorTile* row) {
  const int n = ibuild::SetCount(m_floorSet);
  for (int i = 0; i < n; ++i)
    if (ibuild::SetRow(m_floorSet, i) == static_cast<const void*>(row)) return i;
  return 0;
}
// 800420AC
RET_UNKNOWN EGlobal::IsBuildHouseMode() { return 0; }

// ---------------------------------------------------------------------------------------------------
// PC glue: the players' tools (GC: the interactor manager's interactors 3, 4 and 5 per player).

namespace {
struct PlayerTools {
  InteractorModule__WallManipulator wall;
  InteractorModule__WallPainter paper;
  InteractorModule__FloorPainter floor;
  InteractorModule__Interactor* active = nullptr;
  int id = 0;
  unsigned int down = 0;  // the semantics' buttons held last frame
};
PlayerTools s_tools[2];

void StopActive(PlayerTools& t) {
  if (!t.active) return;
  if (t.id == 3) t.wall.OnStop();
  else if (t.id == 4) t.paper.OnStop();
  else if (t.id == 5) t.floor.OnStop();
  ibuild::ClearCameraFilter(t.id ? t.active->m_player : 0);
  t.active = nullptr;
  t.id = 0;
}
}  // namespace

bool StartBuildTool(BuildToolParams& params) {
  PlayerTools& t = s_tools[params.player & 1];
  StopActive(t);
  ESimsCam* cam = _gd.m_players[params.player & 1].m_cam;
  switch (params.id) {
    case 3:
      t.wall.OnCreate();
      t.wall.Start(params);
      t.wall.AttachCamera(cam);
      t.active = &t.wall;
      break;
    case 4:
      t.paper.OnCreate();
      t.paper.Start(params);
      t.paper.AttachCamera(cam);
      t.active = &t.paper;
      break;
    case 5:
      t.floor.OnCreate();
      t.floor.Start(params);
      t.floor.AttachCamera(cam);
      t.active = &t.floor;
      break;
    default:
      return false;
  }
  t.id = params.id;
  ibuild::SetCameraFilter(params.player);  // the tools' OnStart
  t.down = ~0u;  // the button that started the tool is held: not a press
  // PC development aid: SIMMY_BUILD_TRACE=2 prints the lot's wall segments (tile rows by x; hex digit of
  // the segment bits 1/2/4/8, D a diagonal, . none).
  if (const char* tr = std::getenv("SIMMY_BUILD_TRACE"))
    if (std::atoi(tr) == 2 && Globs_pFixedWorld) {
      cFixedWorldImpl* w = Globs_pFixedWorld;
      for (int x = 0; x < w->GetXSize(); ++x) {
        std::string row;
        for (int y = 0; y < w->GetYSize(); ++y) {
          CTilePt t(x, y, 1);
          const unsigned int seg = w->GetWall(t).m_segments;
          row += seg & 0x30 ? 'D' : seg ? "0123456789ABCDEF"[seg & 0xf] : '.';
        }
        std::printf("[build] walls x=%2d %s\n", x, row.c_str());
      }
    }
  std::printf("[build] player %d starts tool %d (%s, stock %d)\n", params.player + 1, params.id,
              params.id == 3 ? (params.sell ? "wall sell" : params.fence ? "fence" : "walls")
              : params.id == 4 ? (params.sell ? "wallpaper sell" : "wallpaper")
                               : (params.place ? "floor" : "floor remove"),
              params.stock);
  return true;
}

void StopBuildTool(int player) { StopActive(s_tools[player & 1]); }

bool UpdateBuildTool(int player, float dt) {
  PlayerTools& t = s_tools[player & 1];
  if (!t.active) return false;
  InteractorModule__Interactor* tool = t.active;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(player)));
  auto* g2d = static_cast<G2DTarget*>(_globals.m_g2dTarget);
  if (g2d && g2d->IsG2DSpawned()) pad = nullptr;  // GC: SpawnDialog pauses the interactors
  if (pad) {
    // The tools' semantics (803FE0F8 / 803FCD0C / 803FE2D8): pad command 5 -> 0 (A), 6 -> 4 (B),
    // 0x2F -> 1 (Y), 0x17 -> 2 (L or R); sticks 0 / 1 -> 7 / 8, the C stick -> 0xB / 0xC.
    static const unsigned int kButtons[4][2] = {{5, 0}, {6, 4}, {0x2f, 1}, {0x17, 2}};
    const unsigned int all = ~0u;
    const unsigned int pressedMask = pad->GetBtnPressedMask(all), downMask = pad->GetBtnDownMask(all);
    unsigned int down = 0;
    for (int i = 0; i < 4; ++i)
      if (pad->GetCmdResult(kButtons[i][0], downMask, downMask, false)) down |= 1u << i;
    if (t.down == ~0u) t.down = down;
    auto stick = [&](int s, int axis) {
      float v = static_cast<float>(pad->GetStick(s, axis, all));
      return std::fabs(v) < 0.2f ? 0.0f : v;
    };
    using Cmd = InteractorModule__InteractorInputManager__InteractorCommand;
    tool->OnCommandUpdate(Cmd{7}, stick(0, 0));
    tool->OnCommandUpdate(Cmd{8}, stick(0, 1));
    tool->OnCommandUpdate(Cmd{0xb}, stick(1, 0));
    tool->OnCommandUpdate(Cmd{0xc}, stick(1, 1));
    if (stick(0, 0) == 0.0f) tool->OnCommandReleased(Cmd{7});
    if (stick(0, 1) == 0.0f) tool->OnCommandReleased(Cmd{8});
    (void)pressedMask;
    for (int i = 0; i < 4; ++i) {
      const unsigned int bit = 1u << i;
      if ((down & bit) && !(t.down & bit)) tool->OnCommandPressed(Cmd{static_cast<int>(kButtons[i][1])}, 1.0f);
      else if (!(down & bit) && (t.down & bit)) tool->OnCommandReleased(Cmd{static_cast<int>(kButtons[i][1])});
      if (t.active != tool) return true;  // the press ended or swapped the tool
    }
    t.down = down;
  }
  // PC development aid: SIMMY_BUILD_TRACE=1 prints the tool's cursor and state every 30 frames.
  static const bool trace = std::getenv("SIMMY_BUILD_TRACE") != nullptr;
  if (trace && _framecount % 30 == 0) {
    const InteractorModule__Interactor* i = t.active;
    unsigned int flags = t.active == &t.wall ? t.wall.m_flags : t.active == &t.paper ? t.paper.m_flags : t.floor.m_state;
    const EVec3& anchor = t.active == &t.wall ? t.wall.m_anchor : t.active == &t.paper ? t.paper.m_anchor : t.floor.m_anchor;
    const int count = t.active == &t.wall ? t.wall.m_count : t.active == &t.paper ? t.paper.m_count : 0;
    std::printf("[build] trace p%d tool %d pos %.2f,%.2f snap %.2f,%.2f anchor %.2f,%.2f flags %x count %d stick %.2f,%.2f\n",
                player + 1, t.id, i->m_pos.x, i->m_pos.y, i->m_savedPos.x, i->m_savedPos.y, anchor.x, anchor.y, flags, count,
                i->m_leftStick.x, i->m_leftStick.y);
    if (ESimsCam* c = _gd.m_players[player & 1].m_cam)
      std::printf("[build] trace cam mode %d target %.2f,%.2f,%.2f eye %.2f,%.2f,%.2f zoom %.2f tilt %.2f\n", c->m_mode,
                  c->m_target.x, c->m_target.y, c->m_target.z, c->m_eye.x, c->m_eye.y, c->m_eye.z, c->m_zoom, c->m_tilt);
  }
  if (t.active == &t.wall) t.wall.Update(dt);
  else if (t.active == &t.paper) t.paper.Update(dt);
  else if (t.active == &t.floor) t.floor.Update(dt);
  return true;
}

// InteractorVisualizer::SetupDrawCallback for a player's build tool: its PreDraw (vtable 0xB4).
bool BuildToolPreDraw(int player, InteractorModule__InteractorVisualizer& v) {
  PlayerTools& t = s_tools[player & 1];
  if (!t.active) return false;
  if (t.active == &t.wall) t.wall.PreDraw(v);
  else if (t.active == &t.paper) t.paper.PreDraw(v);
  else if (t.active == &t.floor) t.floor.PreDraw(v);
  return true;
}

// InteractorVisualizer::InteractorOrderTableCallback for a player's build tool: the record's draw state
// to the tool (+0x4C; 1 for the tools' own records, 0 for the visualizer's per-player ones), then its
// Draw (vtable 0xBC). PC: the tool is known by the position the record sorts by.
bool BuildToolDraw(int player, EOrderTableData* otd, InteractorModule__InteractorVisualizer& v) {
  PlayerTools& t = s_tools[player & 1];
  if (!t.active || !otd) return false;
  if (t.active == &t.wall && (otd->m_pos == &t.wall.m_anchorDraw.x || otd->m_pos == &t.wall.m_previewPos.x)) {
    t.wall.m_drawState = otd->m_pos == &t.wall.m_previewPos.x ? 1 : 0;
    t.wall.Draw(v);
  } else if (t.active == &t.paper && otd->m_pos == &t.paper.m_pos.x) {
    t.paper.m_drawState = 0;
    t.paper.Draw(v);
  } else if (t.active == &t.floor && otd->m_pos == &t.floor.m_previewPos.x) {
    t.floor.m_drawState = 1;
    t.floor.Draw(v);
  } else {
    return false;
  }
  return true;
}

bool BuildToolInfo(int player, InteractorModule__InteractorInfo& info) {
  PlayerTools& t = s_tools[player & 1];
  if (!t.active) return false;
  info.m_x = t.active->m_pos.x;
  info.m_y = t.active->m_pos.y;
  info.m_w = info.m_h = 1.0f;
  return true;
}
