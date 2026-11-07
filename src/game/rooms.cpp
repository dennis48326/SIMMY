// Port of game/rooms.cpp: the lot's rooms. The room table (rooms by id, lighting groups), the tiles
// split by diagonal walls, Room's tiles and whether it is outside, the room statistics (the room score
// from its tiles and objects, ambient light, the outdoor score by time of day: RoomScoreConstants,
// CollectTileStats, CollectObjectStats, ComputeRoom), room partitions and overhead lights. Not ported:
// the debug PrintStats, OffsetWorld (lot size changes) and Rotate.
#include "simmy/types_gen.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/motiveeffects.hpp"

#include <cstdio>
#include <cstdlib>

RoomManager* Globs_pRoomManager = nullptr;  // 804FDC08 Globs::pRoomManager

namespace {
RoomManager* sRoomMgr = nullptr;  // the last RoomManager made
}  // namespace

// ---------------------------------------------------------------------------------------------------
// Room

// 801374C4
Room::Room(unsigned short id, RoomManager* manager) : m_id(id), m_manager(manager) { Clear(); }

// 80137578
Room::~Room() { Clear(); }

// 801376EC
// Forgets its statistics (and its light positions); the tiles stay.
void Room::Clear() {
  m_pool = 0;
  m_statsDirty = 1;
  m_field48 = 0;
  m_inUse = 0;
  m_tileStats = 0;
  m_objectCount = 0;
  m_portalLight = 0;
  m_objectLight = 0;
  m_data27Sum = 0;
  m_impactSum = 0;
  m_impactCount = 0;
  m_count3B4 = 0;
  m_count3B5to7 = 0;
  m_floorStats = 0;
  m_wallSides = 0;
  m_paperedSides = 0;
  m_field7C = 0.0f;
  m_ambientLight = 0.0f;
  m_outsideKnown = 0;
  m_outside = 0;
  m_lightSources = 0;
  m_people = 0;
}

namespace {
// RoomScoreConstants (group 3 of the global object file's float constants, GlobalConstantsClient(3);
// UpdateConstants 801358D0): the room score's tuning, with the code's defaults.
float sLightContributionFactor = 30.0f;
float sAreaScoreClip = 60.0f;
float sAreaScoreLowLimit = -30.0f;
float sAreaScoreRange = 60.0f;
float sLightScoreLowLimit = -20.0f;
float sLightScoreRange = 40.0f;
float sEffectiveAreaLowClip = 10.0f;
float sEffectiveAreaHighClip = 45.0f;
float sRoomImpactMultiplier = 10.0f;
float sOutdoorBadMultiplier = 2.0f;
float sOutdoorGoodMultiplier = 1.0f;
float sOutdoorObjectDivisor = 3.0f;
float sOutdoorDaylightBonus = 0.0f;
float sOutdoorDawnDuskBonus = -20.0f;
float sOutdoorNightBonus = -50.0f;
float sFloorScoreLowLimit = -40.0f;
float sFloorScoreRange = 40.0f;
float sWallScoreLowLimit = -40.0f;
float sWallScoreRange = 40.0f;
int sConstantsLoaded = 0;  // sConstantsLoaded.1650

// 80424364 segSrch.2008 / 80424378: a split tile's diagonal and its two sides.
const int kSegSearch[2][3] = {{0x10, 2, 4}, {0x20, 1, 3}};

// 80485E38 sLightingParameters (Globs::pLightingParameters): nothing on the disc writes it, so its
// outside ambient level (+0xC) stays 0 (GC too).
float sLightingParameters[0x11] = {};
}  // namespace

namespace {
void LoadRoomScoreConstants();
}  // namespace

// 801358D0
// The tuning from group 3 (GlobalConstantsClient(3), GetRoomScoreConstantsClient) of the global object
// file's float constants (FloatConstants::Load / Get), with the code's defaults. PC: the client object
// (sTheClient) is not made; ComputeRooms runs the body.
void RoomScoreConstants::UpdateConstants() { LoadRoomScoreConstants(); }

namespace {
void LoadRoomScoreConstants() {
  auto get = [](const char* name, float def) { return GlobalFloatConstant(3, name, def); };
  sLightContributionFactor = get("object light contribution factor", 30.0f);
  sAreaScoreClip = get("area clip to score", 60.0f);
  sAreaScoreLowLimit = get("area score for zero tiles", -30.0f);
  const float areaClip = get("area score for clip tile", 30.0f);
  sLightScoreLowLimit = get("light score for 0.0", -20.0f);
  const float lightOne = get("light score for 1.0", 20.0f);
  sEffectiveAreaLowClip = get("room impact effective area min", 10.0f);
  sEffectiveAreaHighClip = get("room impact effective area max", 45.0f);
  sRoomImpactMultiplier = get("room impact multiplier", 10.0f);
  sOutdoorBadMultiplier = get("outdoor room impact factor", 2.0f);
  sOutdoorGoodMultiplier = get("outdoor good object count factor", 1.0f);
  sOutdoorObjectDivisor = get("outdoor object divisor", 3.0f);
  sOutdoorDaylightBonus = get("outdoor daylight bonus", 0.0f);
  sOutdoorDawnDuskBonus = get("outdoor dawn/dusk bonus", -20.0f);
  sOutdoorNightBonus = get("outdoor nighttime bonus", -50.0f);
  sFloorScoreLowLimit = get("floor score for 0%", -40.0f);
  const float floorFull = get("floor score for 100%", 0.0f);
  sWallScoreLowLimit = get("wall score for 0%", -40.0f);
  const float wallFull = get("wall score for 100%", 0.0f);
  sAreaScoreRange = areaClip - sAreaScoreLowLimit;
  sLightScoreRange = lightOne - sLightScoreLowLimit;
  sWallScoreRange = wallFull - sWallScoreLowLimit;
  sFloorScoreRange = floorFull - sFloorScoreLowLimit;
  if (std::getenv("SIMMY_ROOM_TRACE"))
    std::printf("[room] constants: light x%.1f, area clip %.0f (%.0f..%+.0f), light %.0f..%+.0f, impact area %.0f..%.0f x%.1f, "
                "outdoor bad x%.1f good x%.1f /%.1f, bonus day %.0f dawn/dusk %.0f night %.0f, floor %.0f..%+.0f, wall %.0f..%+.0f\n",
                sLightContributionFactor, sAreaScoreClip, sAreaScoreLowLimit, sAreaScoreRange, sLightScoreLowLimit,
                sLightScoreRange, sEffectiveAreaLowClip, sEffectiveAreaHighClip, sRoomImpactMultiplier, sOutdoorBadMultiplier,
                sOutdoorGoodMultiplier, sOutdoorObjectDivisor, sOutdoorDaylightBonus, sOutdoorDawnDuskBonus, sOutdoorNightBonus,
                sFloorScoreLowLimit, sFloorScoreRange, sWallScoreLowLimit, sWallScoreRange);
}
}  // namespace

// 80137B58
// Wall styles that count for the room score: the walls (1, 7, 8, 9), the low walls and railings (3, 5,
// 6, 0xF, 0x17) and 0x16.
int IsScoredStyle(int style) {
  if (style == 1 || style == 7 || style == 8 || style == 9) return 1;
  if (style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17) return 1;
  return style == 0x16 ? 1 : 0;
}

// 80137BE0
// One of its tiles: outside, 2 to the area; inside, a whole tile 2 to the area (2 more to the floored
// area with a floor) and each scored wall side on it (papered: also counted as such); a tile split by a
// diagonal 1 to the area (1 to the floored area if either half has a floor) and its diagonal wall (by
// its style; papered on either side).
void Room::CollectTileStats(CTilePt& tile) {
  if (IsOutside()) {
    m_tileStats += 2;
    return;
  }
  cFixedWorldImpl* w = Globs_pFixedWorld;
  TileWalls walls = w->GetWall(tile);
  if (!walls.HasWall()) {
    m_tileStats += 2;
    if (w->GetFloor(tile)) m_floorStats += 2;
    return;
  }
  if (!walls.HasDiagonal()) {
    m_tileStats += 2;
    if (w->GetFloor(tile)) m_floorStats += 2;
    for (int seg = walls.First(); seg != 0; seg = walls.Next(seg)) {
      if (!IsScoredStyle(walls.GetStyle(seg))) continue;
      ++m_wallSides;
      if (walls.GetPattern(seg, 0)) ++m_paperedSides;
    }
    return;
  }
  const int* search = kSegSearch[0];
  m_tileStats += 1;
  if (!walls.HasWall(search[0])) search = kSegSearch[1];
  bool papered = false, floored = false;
  const int style = walls.GetStyle(search[0]);
  for (int i = 0; i < 2; ++i) {
    if (walls.GetPattern(search[0], search[1 + i])) papered = true;
    if (walls.GetFloorValue(search[1 + i])) floored = true;
  }
  if (IsScoredStyle(style)) {
    ++m_wallSides;
    if (papered) ++m_paperedSides;
  }
  if (floored) m_floorStats += 1;
}

// 80137E44
// The objects at one of its tiles (the tile's object and what it holds): their count, the data 0x3B
// kinds (4; 5..7), people, the light givers (CanContributeLight: data 0x28 bit 0x400; doors and windows,
// type 8, to the portal light, the others counted and to the object light), data 0x27 and the room impact
// (data 7, with the count of those not negative). GC: also each type 1 light source's world position
// (5 up) in +0x14 (PC: not kept, nothing reads it).
void Room::CollectObjectStats(ObjectIterator it) {
  while (it.m_current) {
    cXObjectImpl* o = it.m_current->GetObjectImplementation();
    ++m_objectCount;
    const int kind = o->GetData(0x3b);
    if (kind < 8) {
      if (kind < 5) {
        if (kind == 4) ++m_count3B4;
      } else {
        ++m_count3B5to7;
      }
    }
    if (o->GetType() == 2) ++m_people;
    if (o->CanContributeLight()) {
      if (o->GetType() != 8) ++m_lightSources;
      if (const int light = o->GetLightingContribution()) {
        if (o->GetType() == 8) m_portalLight += light;
        else m_objectLight += light;
      }
    }
    m_data27Sum += o->GetData(0x27);
    const int impact = o->GetData(7);
    m_impactSum += impact;
    if (impact >= 0) ++m_impactCount;
    ++it;
  }
}

// 801377D8
// Inside: the ambient light from the objects' light over the area (x the contribution factor) and the
// doors' and windows' (likewise, times the outside's level, at most that level), 0 under 0.01, 1 over
// 0.99; the score: the area's (clipped) score, the light's, the room impact over the effective area,
// the floored share's and the papered walls' share's, clamped to +-100. Outside: the ambient light is
// the outside's; the score the impact-free objects' count x the good factor plus the impact x the bad
// factor (not clamped).
void Room::ComputeRoom() {
  if (!IsOutside()) {
    const float area = static_cast<float>(GetArea());
    const double objectLight =
        static_cast<double>(static_cast<float>(static_cast<float>(static_cast<double>(m_objectLight) / 100.0) / area) *
                            sLightContributionFactor);
    const double outside = RoomManager::GetOutsideAmbientLevel();
    double portal = static_cast<double>(static_cast<float>(static_cast<double>(m_portalLight) / 100.0));
    portal = static_cast<double>(static_cast<float>(
        static_cast<float>(static_cast<float>(portal / static_cast<double>(area)) * static_cast<float>(outside)) *
        sLightContributionFactor));
    if (outside < portal) portal = outside;
    float light = static_cast<float>(portal + objectLight);
    if (light < 0.01f) light = 0.0f;
    else if (light > 0.99f) light = 1.0f;
    m_ambientLight = light;
    float clipped = area;
    if (sAreaScoreClip < clipped) clipped = sAreaScoreClip;
    const double areaScore = static_cast<double>((clipped / sAreaScoreClip) * sAreaScoreRange + sAreaScoreLowLimit);
    const double lightScore = static_cast<double>(m_ambientLight * sLightScoreRange + sLightScoreLowLimit);
    float effective = sEffectiveAreaLowClip;
    if (sEffectiveAreaLowClip <= area) effective = area > sEffectiveAreaHighClip ? sEffectiveAreaHighClip : area;
    const float score = static_cast<float>(areaScore + lightScore) +
                        (static_cast<float>(m_impactSum) * sRoomImpactMultiplier) / effective +
                        (static_cast<float>(m_floorStats) / static_cast<float>(m_tileStats)) * sFloorScoreRange +
                        sFloorScoreLowLimit +
                        (static_cast<float>(m_paperedSides) / static_cast<float>(m_wallSides)) * sWallScoreRange +
                        sWallScoreLowLimit;
    m_field7C = score;
    if (score < -100.0f) m_field7C = -100.0f;
    else if (score > 100.0f) m_field7C = 100.0f;
    return;
  }
  m_ambientLight = static_cast<float>(RoomManager::GetOutsideAmbientLevel());
  m_field7C = static_cast<float>(m_impactCount) * sOutdoorGoodMultiplier + static_cast<float>(m_impactSum) * sOutdoorBadMultiplier;
}

// 80138684
// The ambient light; a room with its overhead lights on, no light-giving objects and someone in it is
// lit at least 0.8; a pool always 0.8.
double Room::GetAmbientLight() const {
  if (m_pool == 0) {
    const double v = m_ambientLight;
    if (m_overheadLights == 0 || m_lightSources != 0 || m_people < 1 || 0.8f <= v) return v;
  }
  return 0.8f;
}

// 801386D4
RET_UNKNOWN Room::IsPool() { return m_pool; }

// 80138534
// An inside room's overhead lights on or off (not a pool); a change tells the room's lighting
// (GlobalDispatch 0xED) when it has no light-giving objects and its ambient light is under 0.8.
void Room::SetOverheadLights(bool on) {
  if (IsOutside() || m_overheadLights == (on ? 1 : 0) || m_pool != 0) return;
  m_overheadLights = on ? 1 : 0;
  if (m_lightSources == 0 && m_ambientLight < 0.8f) HouseDispatch(0xed, m_id);
}

// 801385B4
// The people (not ghosts) whose current room it is (GC: the module's people; PC: the people by id).
RET_UNKNOWN Room::GetPeopleCount() {
  ObjectModuleImpl* module = Globs_pObjectModule;
  int n = 0;
  const int count = module ? module->GetNumObjects() : 0;
  for (int id = 1; id <= count; ++id) {
    cXObject* x = module->GetObjectFromID(id);
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    auto* p = o && o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr;
    if (p && !p->IsGhost() && p->GetCurrentRoom() == m_id) ++n;
  }
  return n;
}

// 80138704
// 1 to 1.25 by the share of its tiles holding a door or window (object type 8).
double Room::GetAmbientLightMultiplier() {
  float portals = 0.0f;
  for (CTilePt& t : m_tiles) {
    cXObject* object = Globs_pObjectModule->GetObjectFromID(Globs_pObjectModule->GetTileObjectID(t));
    // GC: the object's type (cXObject vtable 0x31C), the definition's type.
    if (object && objdef::Type(object->GetDef()) == 8) portals += 1.0f;
  }
  float share = portals / static_cast<float>(GetArea());
  if (!(share >= 0.0f)) share = 0.0f;
  else if (share > 1.0f) share = 1.0f;
  return share * 0.25f + 1.0f;
}

// 80138358
int Room::GetArea() { return m_tileStats / 2; }

// 8013836C
void Room::AbsorbNewRoomList(std::vector<CTilePt>& tiles) {
  Clear();
  m_tiles = tiles;
  m_statsDirty = 1;
  m_inUse = 1;
}

// 801383B4
int Room::IsTileInRoom(CTilePt& tile) {
  for (CTilePt& t : m_tiles)
    if (t == tile) return 1;
  return 0;
}

// 80138420
// Outside when its first tile still in it has the world's outside flag; a room whose tiles all moved
// on is outside when it has a single tile. Cached until Clear.
int Room::IsOutside() {
  if (!m_inUse) return 0;
  if (!m_outsideKnown) {
    for (CTilePt& t : m_tiles) {
      if (Globs_pFixedWorld->GetRoom(t) == m_id) {
        m_outside = (Globs_pFixedWorld->GetFlags(t) >> 2) & 1;
        m_outsideKnown = 1;
        break;
      }
    }
    if (!m_outsideKnown) {
      m_outside = m_tiles.size() == 1 ? 1 : 0;
      m_outsideKnown = 1;
    }
  }
  return m_outside;
}

// ---------------------------------------------------------------------------------------------------
// RoomManager

// 80135DAC
RoomManager::RoomManager() { sRoomMgr = this; }

// 80135E6C
RoomManager::~RoomManager() {
  ResetRoomManager();
  if (sRoomMgr == this) sRoomMgr = nullptr;
}

// 8013884C
RoomManager* RoomManager::GetRoomManager() { return sRoomMgr; }

// 80136644
// The room with the id (none for the markers 0xFFFB and up).
Room* RoomManager::GetRoom(unsigned short id) {
  if (id >= 0xfffb) return nullptr;
  auto it = m_rooms.find(id);
  return it == m_rooms.end() ? nullptr : it->second;
}

// 801366F0
// The room with the id, cleared, or a new one; in use either way.
Room* RoomManager::GetNewRoom(unsigned short id) {
  Room* room;
  auto it = m_rooms.find(id);
  if (it == m_rooms.end()) {
    room = new Room(id, this);
    m_rooms.emplace(id, room);
  } else {
    room = it->second;
    room->Clear();
  }
  room->m_inUse = 1;
  return room;
}

// 80136EEC
// Every room in use cleared, and the diagonal tiles forgotten.
void RoomManager::ResetRooms() {
  for (auto& entry : m_rooms)
    if (entry.second->m_inUse) entry.second->Clear();
  m_diagonals.clear();
}

// 80136014
void RoomManager::RoomScoreChanged(int id) {
  auto it = m_rooms.find(static_cast<unsigned short>(id));
  if (it == m_rooms.end() || !it->second) return;
  it->second->m_statsDirty = 1;
  m_dirty = 1;
}

// 801361E0
void RoomManager::AllRoomsScoreChanged() {
  for (auto& entry : m_rooms)
    if (entry.second && entry.second->m_inUse) entry.second->m_statsDirty = 1;
  m_dirty = 1;
}

// 80137780
std::vector<PenaltyRect>* Room::GetPartition() {
  if (m_partition.empty()) BuildRoomPartition(m_id, &m_partition);
  return &m_partition;
}

// 80138300
double Room::GetObjectDensity() {
  return static_cast<float>(static_cast<float>(m_objectCount) / static_cast<float>(m_tileStats));
}

// 80137034
// Each room's partition (+0x24) is built again when next asked for.
void RoomManager::ClearRoomPartitions() {
  for (auto& entry : m_rooms)
    if (entry.second) entry.second->m_partition.clear();
}

// 801370D4
void RoomManager::ResetRoomManager() {
  for (auto& entry : m_rooms) delete entry.second;
  m_rooms.clear();
}

// 80137428
// Every room in the lighting group of room `room` moves to `group`.
void RoomManager::ChangeLightingGroup(unsigned short room, unsigned short group) {
  Room* r = GetRoom(room);
  if (!r) return;
  const unsigned short old = r->m_lightingGroup;
  for (auto& entry : m_rooms)
    if (entry.second->m_lightingGroup == old) entry.second->m_lightingGroup = group;
}

// 80136A2C
// The room `room` reached half `side` of a tile split by a diagonal wall: the tile's first visit
// records that half (the other half, opposite, waits for its room); a later visit from another room
// takes the other half. The same room reaching both halves makes it one tile of that room again:
// 0 (the caller then gives the tile the room), else 1.
int RoomManager::ProcessDegenerateTile(CTilePt& tile, unsigned short room, Room__Sides side) {
  int opposite = 0;
  switch (static_cast<int>(side)) {
    case 2: opposite = 4; break;
    case 1: opposite = 3; break;
    case 3: opposite = 1; break;
    case 4: opposite = 2; break;
    default: break;
  }
  auto it = m_diagonals.find(tile);
  if (it == m_diagonals.end()) {
    DiagonalNode first, second;
    first.m_room = room;
    first.m_side = side;
    second.m_room = 0xffff;
    second.m_side = opposite;
    m_diagonals.emplace(tile, std::make_pair(first, second));
    return 1;
  }
  if (it->second.first.m_room == room) {
    m_diagonals.erase(it);
    return 0;
  }
  it->second.second.m_side = side;
  it->second.second.m_room = room;
  return 1;
}

// 80136DB4
// The rooms and sides of a split tile's halves (0 when the tile is not split).
bool RoomManager::ResolveDiagonal(CTilePt& tile, unsigned short* room1, unsigned short* room2, Room__Sides* side1,
                                  Room__Sides* side2) {
  auto it = m_diagonals.find(tile);
  if (it == m_diagonals.end()) {
    *room1 = 0;
    *room2 = 0;
    *side1 = 0;
    *side2 = 0;
    return false;
  }
  *room1 = it->second.first.m_room;
  *side1 = it->second.first.m_side;
  *room2 = it->second.second.m_room;
  *side2 = it->second.second.m_side;
  return true;
}

// 80136C18
// The Rooms of a tile's halves: both the tile's room when it is not split (0), a half without a room
// yet takes the first half's.
int RoomManager::ResolveDiagonal(CTilePt& tile, Room** room1, Room** room2, Room__Sides* side1, Room__Sides* side2) {
  auto it = m_diagonals.find(tile);
  if (it == m_diagonals.end()) {
    Room* room = GetRoom(Globs_pFixedWorld->GetRoom(tile));
    *room1 = room;
    *room2 = room;
    *side1 = 0;
    *side2 = 0;
    return 0;
  }
  *room1 = GetRoom(it->second.first.m_room);
  *side1 = it->second.first.m_side;
  if (it->second.second.m_room < 0xfffb) {
    *room2 = GetRoom(it->second.second.m_room);
    *side2 = it->second.second.m_side;
  } else {
    *room2 = *room1;
    *side1 = 0;
    *side2 = 0;
  }
  return 1;
}

// 80136FB0
void RoomManager::InitLights() {}

// 80136FB4
// The outside's ambient level: the lighting parameters' level (Globs::pLightingParameters +0xC) by the
// time of day (day 1, dawn and dusk 0.5, night 0).
double RoomManager::GetOutsideAmbientLevel() {
  float f = 1.0f;
  const int tod = Globs_pSimulator ? Globs_pSimulator->GetTimeOfDay() : 0;
  if (tod == 1 || tod == 3) f = 0.5f;
  else if (tod == 2) f = 0.0f;
  return static_cast<double>(sLightingParameters[3] * f);
}

// 80136274
// Each room in use with stale statistics collects them again (per tile its walls and floor, and the
// objects there: the tile's object iterator), then its score and light (ComputeRoom; a changed ambient
// light goes to GlobalDispatch 0xED); the inside rooms' area goes to the simulator (global 0x1B). The
// outdoor score: the outside rooms' scores over the outdoor divisor plus the time of day's bonus,
// clamped to +-100.
void RoomManager::ComputeRooms() {
  if (!sConstantsLoaded) {
    LoadRoomScoreConstants();  // RoomScoreConstants::UpdateConstants(&sTheClient)
    sConstantsLoaded = 1;
  }
  if (!m_lightsInited) {
    InitLights();
    m_lightsInited = 1;
  }
  short area = 0;
  for (auto& entry : m_rooms) {
    Room* room = entry.second;
    if (!room) continue;
    if (room->m_inUse && room->m_statsDirty) {
      const double before = room->GetAmbientLight();
      room->Clear();
      room->m_inUse = 1;
      room->m_statsDirty = 0;
      for (CTilePt& tile : room->m_tiles) {
        room->CollectTileStats(tile);
        ObjectIterator it;
        it.init(tile, ObjectIterator__IterateType{0});
        room->CollectObjectStats(it);
      }
      room->ComputeRoom();
      if (before != room->GetAmbientLight()) {
        // GC: GlobalDispatch(0xED, the room): HouseImpl::DoCommand -> RoomLightingChanged.
        RoomLightingChanged(room->m_id);
      }
      if (std::getenv("SIMMY_ROOM_TRACE"))
        std::printf("[room] %u: %s area %d floored %d/%d walls %d/%d objects %d impact %d (%d) light %d+%d -> ambient %.2f score %.1f\n",
                    room->m_id, room->IsOutside() ? "outside" : "inside", room->GetArea(), room->m_floorStats,
                    room->m_tileStats, room->m_paperedSides, room->m_wallSides, room->m_objectCount, room->m_impactSum,
                    room->m_impactCount, room->m_objectLight, room->m_portalLight, room->m_ambientLight, room->m_field7C);
    }
    if (room->m_inUse && !room->IsOutside()) area = static_cast<short>(area + room->GetArea());
  }
  m_outdoorObjects = 0.0f;
  for (auto& entry : m_rooms) {
    Room* room = entry.second;
    if (room && room->m_inUse && room->IsOutside()) m_outdoorObjects += room->m_field7C;
  }
  m_outdoorScore = m_outdoorObjects / sOutdoorObjectDivisor;
  const int tod = Globs_pSimulator ? Globs_pSimulator->GetTimeOfDay() : 0;
  if (tod == 1 || tod == 3) m_outdoorScore += sOutdoorDawnDuskBonus;
  else if (tod == 0) m_outdoorScore += sOutdoorDaylightBonus;
  else if (tod == 2) m_outdoorScore += sOutdoorNightBonus;
  if (m_outdoorScore < -100.0f) m_outdoorScore = -100.0f;
  else if (m_outdoorScore > 100.0f) m_outdoorScore = 100.0f;
  if (Globs_pSimulator) Globs_pSimulator->SetGlobal(0x1b, area);
  m_dirty = 0;
}

// 80136800
// After a change: the rooms again, then the house's lot data (HouseImpl vtable 0x74,
// PrepareForBudgetWindow).
void RoomManager::UpdateRooms() {
  if (m_dirty) {
    ComputeRooms();
    if (Globs_pHouse) Globs_pHouse->PrepareForBudgetWindow();
  }
}
