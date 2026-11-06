// Port of game/ifloor.cpp (partial): the house's floors. The floor table gathers the lot's tiles by
// floor pattern into strips (runs of one pattern and room down a tile column); CreateFloors records
// each pattern's strips of a room into a display list with the pattern's shader, as that room's
// EIFloor, which the level draws.
//
// The floors are lit per vertex (DoLightingCalculation): each room's light strips give the light-grid
// vertices it covers (BuildVertexSpans), the room's point lights are evaluated there
// (EvaluateLightAtVertices) and the strips' vertex colours rewritten with the room's ambient and sun.
//
// Tiles split by a diagonal wall draw as half tiles at the ends of the strips (BuildTable, BuildStrip).
//
// Still stubbed: the preview table and the grass remapping.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/epointlight.hpp"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <utility>

std::vector<EFloorShdTblNode> EFloorShdTblNode::s_table;  // 80478C18 _eFloorOrderTable
int EFloorShdTblNode::s_inited = 0;                       // m_bTableInited
unsigned int EFloorShdTblNode::s_grassShaderId = 0;       // m_grassShaderId
int EFloorShdTblNode::s_stripCount = 0;                   // _m_nStrips
std::vector<EFloorStripInfo> EFloorShdTblNode::s_lightStrips[256];  // m_stripsForLighting

namespace {
std::vector<EIFloor*> s_floors;  // m_floors (ERedBlackTree by room)
int s_stripsCreated = 0;         // numStripsCreated
// _lightingBuffer: the point lights' colour at each light-grid vertex ([x][y], x * 63 + y), written by
// EvaluateLightAtVertices for the vertices of the floor being lit (shared by all floors, as on the GC).
float s_lightingBuffer[ELightGrid::kGridSize * ELightGrid::kGridSize][3];

// The floor patterns: the "FloorSet" rows hold an array of pattern row pointers (its count in the word
// before it); a pattern's +0x0C is its shader, +0x10 its kind (10: not drawn).
const unsigned char* FloorSetArray() {
  return _globals.m_floorSet ? static_cast<const unsigned char*>(ERQuickdata::Ptr(_globals.m_floorSet, 0)) : nullptr;
}
unsigned int FloorSetCount() {
  const unsigned char* rows = FloorSetArray();
  return rows ? ERQuickdata::U32(rows - 4) : 0;
}
const void* FloorSetRow(unsigned int i) { return ERQuickdata::Ptr(FloorSetArray(), i * 4); }
}  // namespace

// ---------------------------------------------------------------------------------------------------
// EFloorShdTblNode

// 80045520
EFloorShdTblNode::EFloorShdTblNode() {}
// 80045544
EFloorShdTblNode::~EFloorShdTblNode() {}

// 80045588
void EFloorShdTblNode::CleanUp() {
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
  m_strips.clear();
}

// 800455D0
// Each floor pattern's shader (pattern 0: the house's lawn), with the floor tile textures loaded.
void EFloorShdTblNode::InitTable() {
  if (s_inited) return;
  s_inited = 1;
  s_grassShaderId = _globals.GetHouseGrassShaderId(_globals.m_house ? _globals.m_house->GetHouseName() : nullptr);
  const unsigned int count = FloorSetCount();
  s_table.resize(count);
  _datasetman.AddRef(const_cast<char*>("floortiles"), nullptr, 0);
  for (unsigned int i = 0; i < count; ++i) {
    const unsigned int id = i == 0 ? s_grassShaderId : ERQuickdata::U32(FloorSetRow(i), 0xc);
    s_table[i].m_shader = static_cast<ERShader*>(_shaderman.AddRef(id, nullptr, 0));
    // PC development aid: SIMMY_DUMP_WORLD=1 lists the floor patterns and their shaders.
    if (std::getenv("SIMMY_DUMP_WORLD")) std::fprintf(stderr, "DBGFLOOR pattern %u shader %08x -> %p kind %u\n", i, id, (void*)s_table[i].m_shader, ERQuickdata::U32(FloorSetRow(i), 0x10));
  }
  _datasetman.DelRef(const_cast<char*>("floortiles"), EResourceManager__DelRefMode{0});
}

// 800456D4
void EFloorShdTblNode::AddStripToTable(int pattern, EFloorStripInfo& info) {
  if (pattern >= 0 && pattern < static_cast<int>(s_table.size())) s_table[pattern].m_strips.push_back(info);
  ++s_stripCount;
}

// 8004571C
// The strip joins its room's list for the floor lighting (BuildVertexSpans).
void EFloorShdTblNode::AddLightStrip(EFloorStripInfo info) { s_lightStrips[info.m_room].push_back(info); }

// 8004575C
// Room::Sides -> TileWalls side (diagonal tiles; BuildTable's diagonal part is not ported).
int ConvertRoomSideToWallSide(int side) {
  switch (side) {
    case 1: return 1;
    case 2: return 4;
    case 3: return 3;
    case 4: return 2;
    default: return 0;
  }
}

// 800457B0
// The light-grid vertices of a room's light strips: a strip of column x from tile `start` to `end`
// covers the vertices start..end+1 on the grid columns x and x+1 (a half tile at either end drops the
// corner its diagonal cuts off: flag 0x80 on column x, 0x40 on column x+1). Each column's covered
// vertices become runs (EFloorVertexSpan). GC: kept two 64-bit masks (the strip's column and the next)
// as the strips come in column order, flushing a column's runs when the strips move past it; the runs
// are the same.
int BuildVertexSpans(const std::vector<EFloorStripInfo>& strips, std::vector<EFloorVertexSpan>& spans) {
  std::map<int, unsigned long long> columns;
  auto setBits = [&](int column, int from, int to) {
    unsigned long long& mask = columns[column];
    for (int b = from; b <= to; ++b)
      if (b >= 0 && b < 64) mask |= 1ull << b;
  };
  for (const EFloorStripInfo& s : strips) {
    int onX[2], onNext[2];
    const unsigned char ends[2] = {s.m_start, s.m_end};
    for (int i = 0; i < 2; ++i) {
      const unsigned int v = ends[i];
      int next = static_cast<int>(v & 0xffffff3fu) + i, here = next;
      if (v & 0xc0) {
        if (!(v & 0x80)) --next;
        else --here;
      }
      onX[i] = here;
      onNext[i] = next;
    }
    setBits(s.m_x, onX[0], onX[1]);
    setBits(s.m_x + 1, onNext[0], onNext[1]);
  }
  const size_t before = spans.size();
  for (const auto& [column, mask] : columns) {
    int first = -1;
    for (int b = 0; b <= 64; ++b) {
      const bool set = b < 64 && ((mask >> b) & 1);
      if (set && first < 0) first = b;
      if (!set && first >= 0) {
        spans.push_back(EFloorVertexSpan{static_cast<unsigned char>(column), static_cast<unsigned char>(first),
                                         static_cast<unsigned char>(b - 1)});
        first = -1;
      }
    }
  }
  return static_cast<int>(spans.size() - before);
}

// 80045A28
// Down each tile column (inside the lot's border), runs of one floor pattern in one room become
// strips of their pattern, and each room's runs its light strips. Patterns of kind 10 and tiles under
// objects that hide the floor are not drawn. A tile split by a diagonal wall has a floor per half (in
// its wall entry): the half towards y - 1 (wall side 2 or 3) is taken as the end of the run before it,
// the other (side 1 or 4) as the tile's pattern and room; when the pattern or room changes there, the
// strip before ends on the tile's half (end flag 0x80 for side 1, 0x40 for side 4) and the next starts
// on the other half. The tile's (lighting) room touches the light grid's vertices at its corners.
// PC: the strips carry the tile's room (the GC: its lighting room; see CreateFloors).
void EFloorShdTblNode::BuildTable(ERLevel* level) {
  InitTable();
  s_stripCount = 0;
  for (std::vector<EFloorStripInfo>& l : s_lightStrips) l.clear();
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (!world || !world->m_floors) return;
  const unsigned int xEnd = static_cast<unsigned int>(world->GetXSize() - 1) & 0xff;
  const unsigned int yEnd = static_cast<unsigned int>(world->GetYSize() - 1) & 0xff;
  const unsigned int patterns = FloorSetCount();
  static const int kCorners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};  // 803D2DC8
  const auto addCorner = [level](int corner, int x, int y, int room) {
    if (level) level->m_lightGrid.AddRoomToVertex(y + kCorners[corner][1], x + kCorners[corner][0], room);
  };
  for (unsigned int x = 1; x < xEnd; ++x) {
    const auto roomOf = [&](unsigned int y) -> unsigned int {
      return world->m_rooms ? *reinterpret_cast<const unsigned short*>(world->m_rooms->At(static_cast<int>(x), static_cast<int>(y))) & 0xff : 0;
    };
    CTilePt column(static_cast<int>(x), 0, 1);
    unsigned int prev = 0xff, prevRoom = roomOf(0);
    int prevLight = static_cast<int>(GetLightRoomID(world->GetRoom(column)));
    unsigned int start = 0, startHalf = 0, last = 0, endHalf = 0;
    unsigned int lightStart = 0, lightHalf = 0;  // where the room's run (its light strip) began
    for (unsigned int y = 1; y < yEnd; ++y) {
      CTilePt tile(static_cast<int>(x), static_cast<int>(y), 1);
      unsigned int pattern = *world->m_floors->At(static_cast<int>(x), static_cast<int>(y));
      TileWalls walls = world->GetWall(tile);
      unsigned int room = roomOf(y);
      int lightRoom = static_cast<int>(GetLightRoomID(static_cast<unsigned short>(room)));
      int vertexRoom = lightRoom;
      const bool diagonal = walls.HasDiagonal();
      int sideB = 0;
      if (!diagonal) {
        pattern &= 0xff;
        if (CheckForObjectsInFloor(tile)) pattern = 0xff;
      } else {
        Room* rooms[2] = {nullptr, nullptr};
        Room__Sides sides[2] = {Room__Sides{0}, Room__Sides{0}};
        if (Globs_pRoomManager) Globs_pRoomManager->ResolveDiagonal(tile, &rooms[0], &rooms[1], &sides[0], &sides[1]);
        struct Half {
          int side;
          unsigned int floor;
          unsigned int room;
        } half[2];
        for (int h = 0; h < 2; ++h) {
          int side = ConvertRoomSideToWallSide(static_cast<int>(sides[h]));
          if (side == 0) side = walls.HasWall(0x20) ? (h == 0 ? 1 : 3) : (h == 0 ? 2 : 4);
          half[h].side = side;
          half[h].floor = walls.GetFloorValue(DiagonalSideSelector{side});
          if (half[h].floor >= patterns) half[h].floor = 0;
          half[h].room = rooms[h] ? rooms[h]->m_id : 0;  // PC: guard (the GC read through the pointer)
        }
        if (half[0].side == 1 || half[0].side == 4) std::swap(half[0], half[1]);
        pattern = half[1].floor & 0xff;
        room = half[1].room & 0xff;
        lightRoom = static_cast<int>(GetLightRoomID(static_cast<unsigned short>(half[1].room)));
        vertexRoom = prevLight;
        sideB = half[1].side;
      }
      if (pattern < 0x48 && pattern < patterns && ERQuickdata::U32(FloorSetRow(pattern), 0x10) == 10) pattern = 0xff;
      const bool changed = pattern != prev;
      // The light grid: the corners on the y - 1 edge take the room the column comes from; a diagonal
      // tile's other corners the corner its first half keeps and the second half's room.
      addCorner(0, static_cast<int>(x), static_cast<int>(y), vertexRoom);
      addCorner(1, static_cast<int>(x), static_cast<int>(y), vertexRoom);
      if (!diagonal) {
        addCorner(2, static_cast<int>(x), static_cast<int>(y), vertexRoom);
        addCorner(3, static_cast<int>(x), static_cast<int>(y), vertexRoom);
      } else {
        const bool four = sideB > 2;
        addCorner(four ? 3 : 2, static_cast<int>(x), static_cast<int>(y), vertexRoom);
        addCorner(four ? 1 : 0, static_cast<int>(x), static_cast<int>(y), lightRoom);
        addCorner(2, static_cast<int>(x), static_cast<int>(y), lightRoom);
        addCorner(3, static_cast<int>(x), static_cast<int>(y), lightRoom);
      }
      if (changed || room != prevRoom) {
        unsigned int nextHalf = 0;
        if (diagonal) {
          endHalf = sideB < 3 ? 0x80 : 0x40;
          nextHalf = ~endHalf & 0xc0;
          last = y;
        }
        unsigned int nextLight = y, nextLightHalf = nextHalf;
        if (prev != 0xff) {
          EFloorStripInfo info{static_cast<unsigned char>(prevRoom), static_cast<unsigned char>(x),
                               static_cast<unsigned char>(start | startHalf), static_cast<unsigned char>(last | endHalf)};
          AddStripToTable(static_cast<int>(prev), info);
          // A room's run ends (or the floor stops being drawn): its light strip.
          if (room != prevRoom || pattern == 0xff) {
            AddLightStrip(EFloorStripInfo{info.m_room, info.m_x, static_cast<unsigned char>(lightStart | lightHalf), info.m_end});
          } else {
            nextLight = lightStart;
            nextLightHalf = lightHalf;
          }
        }
        lightStart = nextLight;
        lightHalf = nextLightHalf;
        endHalf = 0;
        prevRoom = room;
        prevLight = lightRoom;
        start = y;
        startHalf = nextHalf;
        prev = pattern;
      }
      last = y;
    }
    if (prev != 0xff) {
      EFloorStripInfo info{static_cast<unsigned char>(prevRoom), static_cast<unsigned char>(x),
                           static_cast<unsigned char>(start | startHalf), static_cast<unsigned char>(last)};
      AddStripToTable(static_cast<int>(prev), info);
      AddLightStrip(EFloorStripInfo{info.m_room, info.m_x, static_cast<unsigned char>(lightStart | lightHalf), info.m_end});
    }
  }
}

// 80046150
// An object on the tile hides the floor (its definition's flags +0xB6 & 6).
bool EFloorShdTblNode::CheckForObjectsInFloor(CTilePt& tile) {
  ObjectIterator it;
  for (it.init(tile, ObjectIterator__IterateType{0}); it.m_current; ++it) {
    const ObjDefinition* def = it.m_current->GetDef();
    if (def && (objdef::Flags(def) & 6)) return true;
  }
  return false;
}

// 80046A9C
// The strip's vertices: two per tile edge across the column, at the strip's tile offsets plus the
// house's position (the tile column is y, the tiles along it x), grey vertex colours, the texture
// repeating once per tile (centred on the strip); with `normals`, up normals. A half (diagonal) tile
// at either end has a single vertex on its outer edge, on the column's x - 0.5 side (flag 0x40) or its
// x + 0.5 side, so the strip's first or last triangle covers half the tile. The level bounds grow by
// the vertices (GC: not by a starting half's vertex).
void EFloorShdTblNode::BuildStrip(ERC* rc, EFloorStripInfo info, EVec2& offset, EFloorTileStrip* strip, bool normals,
                                  ERLevel*, EBound3& bounds, bool& first) {
  const unsigned int start = info.m_start, end = info.m_end;
  const int tiles = static_cast<int>(end & 0x3f) - static_cast<int>(start & 0x3f) + 1;
  int count = tiles * 2 + 2;
  int pairs = tiles + 1;
  if (start & 0xc0) {
    count = tiles * 2 + 1;
    pairs = tiles;
  }
  const bool halfEnd = (end & 0xc0) != 0;
  if (halfEnd) {
    --count;
    --pairs;
  }
  ++s_stripsCreated;
  const size_t posBytes = (static_cast<size_t>(count) * 16 + 0x1f) & ~size_t{0x1f};
  const size_t uvBytes = (static_cast<size_t>(count) * 8 + 0x1f) & ~size_t{0x1f};
  const size_t colBytes = (static_cast<size_t>(count) * 4 + 0x1f) & ~size_t{0x1f};
  const size_t nrmBytes = normals ? colBytes : 0;
  const size_t total = posBytes + uvBytes + colBytes + nrmBytes;
  auto* block = static_cast<unsigned char*>(_pGfx->AllocVertexData(static_cast<unsigned int>(total)));
  auto* pos = reinterpret_cast<float*>(block);
  auto* colors = block + posBytes;
  auto* uvs = reinterpret_cast<float*>(block + posBytes + colBytes);
  auto* nrm = normals ? reinterpret_cast<signed char*>(block + posBytes + colBytes + uvBytes) : nullptr;
  strip->m_info = info;
  strip->m_count = count;
  strip->m_positions = pos;
  strip->m_size = static_cast<int>(total);
  if (nrm)
    for (int i = 0; i < count; ++i) nrm[i * 4] = 0, nrm[i * 4 + 1] = 0, nrm[i * 4 + 2] = 0x7f, nrm[i * 4 + 3] = 0;
  const float rowX = static_cast<float>(start & 0x3f) + offset.y;
  const float colY = static_cast<float>(info.m_x) + offset.x;
  float u = static_cast<float>(-tiles / 2);
  float d = -0.5f;
  auto grow = [&](const float* v) {
    float* b = reinterpret_cast<float*>(&bounds);
    if (first) {
      b[0] = b[3] = v[0], b[1] = b[4] = v[1], b[2] = b[5] = v[2];
      first = false;
      return;
    }
    for (int a = 0; a < 3; ++a) {
      if (v[a] < b[a]) b[a] = v[a];
      else if (b[3 + a] < v[a]) b[3 + a] = v[a];
    }
  };
  int v = 0;
  if (start & 0xc0) {
    float* a = pos;
    const bool low = (start & 0xc0) == 0x40;
    a[0] = rowX - 0.5f, a[1] = colY + (low ? -0.5f : 0.5f), a[2] = 0.0f, a[3] = 0.0f;
    uvs[0] = u, uvs[1] = low ? 0.0f : 1.0f;
    std::memset(colors, 0x7f, 4);
    v = 1;
    d = 0.5f;
    u += 1.0f;
  }
  for (int i = 0; i < pairs; ++i, v += 2) {
    float* a = pos + v * 4;
    a[0] = rowX + d, a[1] = colY - 0.5f, a[2] = 0.0f, a[3] = 0.0f;
    grow(a);
    uvs[v * 2] = u, uvs[v * 2 + 1] = 0.0f;
    std::memset(colors + v * 4, 0x7f, 4);
    float* b = a + 4;
    b[0] = rowX + d, b[1] = colY + 0.5f, b[2] = 0.0f, b[3] = 0.0f;
    grow(b);
    uvs[v * 2 + 2] = u, uvs[v * 2 + 3] = 1.0f;
    std::memset(colors + (v + 1) * 4, 0x7f, 4);
    d += 1.0f;
    u += 1.0f;
  }
  if (halfEnd) {
    const int last = count - 1;
    float* a = pos + last * 4;
    const bool low = (end & 0xc0) == 0x40;
    a[0] = rowX + d, a[1] = colY + (low ? -0.5f : 0.5f), a[2] = 0.0f, a[3] = 0.0f;
    uvs[last * 2] = u, uvs[last * 2 + 1] = low ? 0.0f : 1.0f;
    grow(a);
    std::memset(colors + last * 4, 0x7f, 4);
  }
  rc->TriStripRef(count, pos, uvs, colors, nrm, nullptr, false);
}

// 80047104
int EFloorShdTblNode::EmptyTable() {
  s_inited = 0;
  for (std::vector<EFloorStripInfo>& l : s_lightStrips) l.clear();
  for (EFloorShdTblNode& n : s_table) n.CleanUp();
  s_table.clear();
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// EFloorTileGroup

// 8004809C
EFloorTileGroup::EFloorTileGroup(ERShader* shader, EDL* dl, EFloorTileStrip* strips, int count, EFloorTileGroup* next) {
  m_dl = dl;
  m_strips = strips;
  m_count = count;
  m_next = next;
  m_shader = shader;
  shader->AddRef();
}

// 800480E4
EFloorTileGroup::~EFloorTileGroup() {
  if (m_dl) _pGfx->Destroy(m_dl);
  for (int i = 0; i < m_count; ++i) _pGfx->FreeVertexData(m_strips[i].m_positions);
  delete[] m_strips;
  if (m_shader) m_shader->DelRef();
}

// ---------------------------------------------------------------------------------------------------
// EIFloor

// 80047234
EIFloor::EIFloor() {
  m_flags = (m_flags & ~0x200u) | kRoomFloor;
  m_room = 0;
  m_groups = nullptr;
}

// 800484B4
// PC: the vertex spans are the floor's own (GC: _vertexSpanPool, reset by CreateFloors).
EIFloor::~EIFloor() {
  Cleanup();
  delete[] m_spans;
  m_spans = nullptr;
}

// 80047D84
void EIFloor::Cleanup() {
  while (EFloorTileGroup* g = m_groups) {
    m_groups = g->m_next;
    delete g;
  }
}

// 80047738
// The room of a floor: itself, and its room's lighting group (the level's room, as for walls, objects
// and sims).
void FloorRoom(unsigned int room, bool, short& thisSide, short& otherSide) {
  thisSide = otherSide = static_cast<short>(room);
  if (Globs_pRoomManager && (room & 0xffff) < 0xfffb) {
    if (Room* r = Globs_pRoomManager->GetRoom(static_cast<unsigned short>(room)))
      otherSide = static_cast<short>(r->m_lightingGroup);
  }
}

// 80047E24
bool EIFloor::TestCreateFloors() { return true; }

// 8004769C
void EIFloor::DestroyFloors() {
  for (EIFloor* f : s_floors) {
    if (f->m_level) f->m_level->RemoveInstance(f);
    delete f;
  }
  s_floors.clear();
}

// 800477D0
// Per room (GC: per room of the room manager that has tiles; inside rooms and the outside), a floor
// whose display lists hold, per pattern, the strips of that room (outside floors also take the other
// outside rooms' strips), drawn with the pattern's shader; into the level.
void EIFloor::CreateFloors(EHouse* house) {
  TestCreateFloors();
  ERLevel* level = house ? house->m_level : nullptr;
  s_stripsCreated = 0;
  EFloorShdTblNode::BuildTable(level);
  // The rooms in use: each inside room, and room 0 for the outside ones.
  RoomManager* manager = Globs_pRoomManager;
  std::vector<unsigned int> rooms;
  if (manager) {
    for (auto& entry : manager->m_rooms) {
      Room* room = entry.second;
      if (!room->m_inUse) continue;
      const int outside = room->IsOutside();
      if (!outside || room->m_id == 0) rooms.push_back(room->m_id);
    }
  } else {
    rooms.push_back(0);
  }
  // Both rooms outside: the strip goes into the outside floor.
  const auto bothOutside = [manager](unsigned int a, unsigned int b) {
    if (!manager) return false;
    Room* ra = manager->GetRoom(static_cast<unsigned short>(a));
    Room* rb = manager->GetRoom(static_cast<unsigned short>(b));
    return ra && rb && ra->IsOutside() && rb->IsOutside();
  };
  for (unsigned int roomId : rooms) {
    auto* floor = new EIFloor();
    floor->m_room = static_cast<int>(roomId);
    floor->m_thisSideData = roomId;
    floor->m_thisSideFn = FloorRoom;
    s_floors.push_back(floor);
    // Its light-grid vertices: its room's light strips (PC: an outside floor also the other outside
    // rooms' it draws; the GC's strips carried the lighting room, which joined them under room 0).
    {
      std::vector<EFloorStripInfo> lightStrips;
      for (unsigned int r = 0; r < 256; ++r)
        if (r == roomId || (!EFloorShdTblNode::s_lightStrips[r].empty() && bothOutside(roomId, r)))
          lightStrips.insert(lightStrips.end(), EFloorShdTblNode::s_lightStrips[r].begin(),
                             EFloorShdTblNode::s_lightStrips[r].end());
      std::vector<EFloorVertexSpan> spans;
      floor->m_spanCount = BuildVertexSpans(lightStrips, spans);
      if (floor->m_spanCount) {
        floor->m_spans = new EFloorVertexSpan[spans.size()];
        std::copy(spans.begin(), spans.end(), floor->m_spans);
      }
    }
    float bounds[6] = {};
    bool first = true;
    EFloorTileGroup* groups = nullptr;
    for (EFloorShdTblNode& node : EFloorShdTblNode::s_table) {
      if (!node.m_shader || node.m_strips.empty()) continue;
      bool normals = false;
      if (EShader* sh = node.m_shader->GetShader())
        for (int l = 0; l < sh->m_layerCount; ++l)
          if (sh->m_layers[l].m_texGen == 3 || sh->m_layers[l].m_texGen == 1) normals = true;
      ERC* rc = nullptr;
      std::vector<EFloorTileStrip> strips;
      for (auto it = node.m_strips.begin(); it != node.m_strips.end();) {
        const unsigned int room = it->m_room;
        if (room != roomId && !bothOutside(roomId, room)) {
          ++it;  // kept for its own room's floor
          continue;
        }
        if (!rc) {
          rc = _pGfx->Open(RCMode::Deferred);
          rc->SetName(const_cast<char*>("EIFloor::CreateFloors"));
        }
        EVec2 offset = house ? EVec2{house->m_posY, house->m_posX} : EVec2{0.0f, 0.0f};
        EFloorTileStrip strip;
        EFloorShdTblNode::BuildStrip(rc, *it, offset, &strip, normals, level, *reinterpret_cast<EBound3*>(bounds), first);
        strips.push_back(strip);
        it = node.m_strips.erase(it);
      }
      if (rc) {
        EDL* dl = _pGfx->Close(rc);
        auto* copy = new EFloorTileStrip[strips.size()];
        std::copy(strips.begin(), strips.end(), copy);
        groups = new EFloorTileGroup(node.m_shader, dl, copy, static_cast<int>(strips.size()), groups);
      }
    }
    floor->m_groups = groups;
    std::memcpy(floor->m_floorBounds, bounds, sizeof bounds);
    floor->SetBounds(*reinterpret_cast<EBound3*>(bounds));
    if (level) level->InsertInstance(floor, nullptr);
  }
  EFloorShdTblNode::EmptyTable();
}

// 80047318
// Each pattern's display list with its shader (blended floors at the house's fade alpha), the model
// matrix the identity, without culling, a little mip bias.
void EIFloor::Draw(ELevelDrawData& data) {
  ERC* rc = data.m_rc;
  rc->ModelMatrixId();
  for (EFloorTileGroup* g = m_groups; g; g = g->m_next) {
    EShader* shader = g->m_shader->GetShader();
    if (shader) shader->Select(rc, 0);
    const bool blend = _globals.m_house && shader && shader->m_layers[0].m_blend[0] == 1 && shader->m_layers[0].m_blend[1] == 0 &&
                       shader->m_layers[0].m_blend[2] == 0 && shader->m_layers[0].m_blend[3] == 0;
    rc->SetAlpha(blend ? _globals.m_house->m_alpha : 1.0f);
    rc->DisableGeometryModes(8);
    rc->SetMipMap(0.2f, 0);
    rc->DisplayList(g->m_dl);
  }
}

// 800474AC
// Clears the floor's shadow stencil: its display lists again, untextured, writing only alpha 0.
void EIFloor::DrawStencil(ELevelDrawData& data) {
  ERC* rc = data.m_rc;
  data.m_curAnim = nullptr;
  data.m_curMatrix = nullptr;
  rc->ModelMatrixId();
  rc->Texture(nullptr, 0);
  rc->Stencil(2, 0, 0);
  for (EFloorTileGroup* g = m_groups; g; g = g->m_next) rc->DisplayList(g->m_dl);
  rc->Stencil(0, 0, 0);
  data.m_curShader = nullptr;
}
// 800475A4
// The floor's display lists with whatever shadow state is set.
void EIFloor::DrawShadow(ELevelDrawData& data) {
  ERC* rc = data.m_rc;
  for (EFloorTileGroup* g = m_groups; g; g = g->m_next) rc->DisplayList(g->m_dl);
}
// 800461E0 EvaluateFloorLights: roomlights.cpp.

// 800462F8
// The floor's vertex colour (0..255, 128 = full) is base + pointLight * scale: base the ambient times
// the material's ambient plus the sun times its diffuse, scale its diffuse.
void EvaluateFloorColors(EShader* shader, EVec3& ambient, EVec3& sun, EVec3& base, EVec3& scale) {
  const float* m = shader->m_material;  // +0x10 diffuse, +0x20 ambient
  base.x = (m[4] * ambient.x + m[0] * sun.x) * 128.0f;
  base.y = (m[5] * ambient.y + m[1] * sun.y) * 128.0f;
  base.z = (m[6] * ambient.z + m[2] * sun.z) * 128.0f;
  scale = EVec3{m[0] * 128.0f, m[1] * 128.0f, m[2] * 128.0f};
}

// 800463A4
// Each spanned vertex (x, y) of the grid (in the world at (y, x) on the floor) gets the sum of its (up
// to four) nearest point lights of the room: colour x the light's cosine on the floor (its height over
// the distance) x (range / distance)^2, capped at 1.
void EvaluateLightAtVertices(int room, ELightGrid& grid, ELightGrid__LightData* lights, EFloorVertexSpan* spans,
                             int count, float (*out)[3]) {
  for (int i = 0; i < count; ++i) {
    const EFloorVertexSpan& span = spans[i];
    const float py = static_cast<float>(span.m_x);
    for (int y = span.m_first; y <= span.m_last; ++y) {
      EVec3 pos{static_cast<float>(y), py, 0.0f};
      float sum[3] = {0.0f, 0.0f, 0.0f};
      const short* idx = grid.GetLightsAtLocation(pos, room);
      for (int n = 0; n < 4 && *idx != -1; ++n, ++idx) {
        const ELightGrid::LightData& l = lights[*idx];
        const float dx = l.m_dir.x - pos.x, dy = l.m_dir.y - pos.y, dz = l.m_dir.z;
        const float inv = 1.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
        float cosine = dz * inv;
        if (cosine <= 0.0f) cosine = 0.0f;
        const float r = l.m_range * inv;
        float att = r * r;
        if (att >= 1.0f) att = 1.0f;
        const float k = cosine * att;
        sum[0] += k * l.m_color.x;
        sum[1] += k * l.m_color.y;
        sum[2] += k * l.m_color.z;
      }
      float* o = out[span.m_x * ELightGrid::kGridSize + y];
      o[0] = sum[0], o[1] = sum[1], o[2] = sum[2];
    }
  }
}

// 80046614
// The floor's point lights at its vertices, then every strip's vertex colours: the two vertices of
// each tile edge across the strip (grid columns x and x+1), half tiles at the ends taking the corner
// their diagonal keeps. The display lists draw the colours in place (TriStripRef).
void EIFloor::DoLightingCalculation() {
  ERLevel* level = static_cast<ERLevel*>(m_level);
  if (!level || !m_spans) return;
  const int room = static_cast<int>(GetLightRoomID(static_cast<unsigned short>(m_room)));
  if (room < 0 || room >= ELightGrid::kMaxRooms) return;  // PC: guard (the GC indexed blindly)
  ELightGrid& grid = level->m_lightGrid;
  EVec3 ambient, sun;
  EvaluateFloorLights(grid, room, ambient, sun);
  EvaluateLightAtVertices(room, grid, grid.m_lights, m_spans, m_spanCount, s_lightingBuffer);
  constexpr int kRow = ELightGrid::kGridSize;
  for (EFloorTileGroup* g = m_groups; g; g = g->m_next) {
    EShader* shader = g->m_shader ? g->m_shader->GetShader() : nullptr;
    if (!shader) continue;
    EVec3 base, scale;
    EvaluateFloorColors(shader, ambient, sun, base, scale);
    auto put = [&](unsigned char* c, int vertex) {
      const float* l = s_lightingBuffer[vertex];
      const int v[3] = {static_cast<int>(l[0] * scale.x + base.x), static_cast<int>(l[1] * scale.y + base.y),
                        static_cast<int>(l[2] * scale.z + base.z)};
      for (int k = 0; k < 3; ++k) c[k] = static_cast<unsigned char>(v[k] > 0xff ? 0xff : v[k]);
      c[3] = 0x80;
    };
    for (int i = 0; i < g->m_count; ++i) {
      EFloorTileStrip& strip = g->m_strips[i];
      const EFloorStripInfo info = strip.m_info;
      const unsigned int start = info.m_start, end = info.m_end;
      const size_t posBytes = (static_cast<size_t>(strip.m_count) * 16 + 0x1f) & ~size_t{0x1f};
      unsigned char* c = reinterpret_cast<unsigned char*>(strip.m_positions) + posBytes;
      const int first = static_cast<int>(info.m_x) * kRow + static_cast<int>(start & 0xffffff3fu);
      int pairs = static_cast<int>(end & 0x3f) - static_cast<int>(start & 0xffffff3fu) + 2;
      const bool halfStart = (start & 0xc0) != 0, halfEnd = (end & 0xc0) != 0;
      int here = first, next = first + kRow;
      if (halfStart) pairs = pairs - 1;
      if (halfEnd) pairs = pairs - 1;
      if (halfStart) {
        put(c, (start & 0xc0) == 0x40 ? first : next);
        c += 4;
        here = first + 1;
        next = first + kRow + 1;
      }
      for (; pairs > 0; --pairs, ++here, ++next, c += 8) {
        put(c, here);
        put(c + 4, next);
      }
      if (halfEnd) put(c, (end & 0xc0) == 0x40 ? here : next);
    }
  }
}

// 80047600
unsigned int EIFloor::VisibilityTest(E3DWindow& win) { return win.Test(*reinterpret_cast<EBound3*>(m_floorBounds)); }

// 80047EA4
const char* EIFloor::GetInstName(EInstance__InstType& type) {
  type = EInstance__InstType{9};
  return "Floor";
}

// 8004850C
void EIFloor::Update() {}

// 800483F8 EIFloor::GetTypeInfo
// 80048404 EIFloor::GetTypeName
// 80048410 EIFloor::GetTypeKey
// 8004841C EIFloor::GetTypeVersion
// 800483B8 EIFloor::SafeDelete
// 8004834C EIFloor::New
// 80048374 EIFloor::Construct
// 80048394 EIFloor::Destruct
// 80048428 EIFloor::GetTypeInfoStatic
// 80048434 EIFloor::GetReadVersion
SIMMY_STORABLE_IMPL(EIFloor, "EIFloor", 0, EInstance)
