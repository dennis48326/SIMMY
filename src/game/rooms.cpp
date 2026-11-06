// Port of game/rooms.cpp (partial): the lot's rooms. Ported: the room table (rooms by id, lighting
// groups), the tiles split by diagonal walls, Room's tiles and whether it is outside. Still stubbed:
// the room statistics (the room score from its tiles and objects, ambient light, the outdoor score
// by time of day: RoomScoreConstants, CollectTileStats, CollectObjectStats, ComputeRoom), room
// partitions and overhead lights.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

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
// Forgets its statistics; the tiles stay. GC: also empties the +0x14 array.
void Room::Clear() {
  m_inUse = 0;
  m_field48 = 0;
  m_tileStats = 0;
  m_field7C = 0.0f;
  m_statsDirty = 1;
  m_outsideKnown = 0;
  m_outside = 0;
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

// 80137034
// GC: empties each room's partition array (+0x24); the port keeps none.
void RoomManager::ClearRoomPartitions() {}

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

// 80136274
// Each room in use with stale statistics recollects them; the inside rooms' area goes to the
// simulator. GC: also the outdoor score by time of day (RoomScoreConstants, the simulator's time).
void RoomManager::ComputeRooms() {
  if (!m_lightsInited) {
    InitLights();
    m_lightsInited = 1;
  }
  short area = 0;
  for (auto& entry : m_rooms) {
    Room* room = entry.second;
    if (!room) continue;
    if (room->m_inUse && room->m_statsDirty) {
      room->Clear();
      room->m_inUse = 1;
      room->m_statsDirty = 0;
      // GC: per tile CollectTileStats and CollectObjectStats (ObjectIterator over the tile), then
      // ComputeRoom; a changed ambient light is dispatched (0xED). PC: the tile count only.
      room->m_tileStats = static_cast<int>(room->m_tiles.size()) * 2;
    }
    if (room->m_inUse && !room->IsOutside()) area = static_cast<short>(area + room->GetArea());
  }
  SIMMY_STUB("RoomManager::ComputeRooms: room statistics and the outdoor score (RoomScoreConstants, simulator)",
             0x80136274);
  (void)area;  // GC: the simulator's global 0x1B (the house's inside area)
  m_dirty = 0;
}

// 80136800
void RoomManager::UpdateRooms() {
  if (m_dirty) {
    ComputeRooms();
    SIMMY_STUB("RoomManager::UpdateRooms: the house's refresh (Globs::pHouse vtable 0x74)", 0x80136800);
  }
}
