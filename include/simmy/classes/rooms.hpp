// @class Room after CTilePt PenaltyRect
// @class Room__Sides
// @class RoomManager
// The lot's rooms (game/rooms.cpp). cFixedWorldImpl::ComputeRooms floods the tile grid between walls
// and gives each closed area a room id (0: the outside); the RoomManager keeps a Room per id with
// its tiles, and the tiles a diagonal wall splits in two (their two halves' rooms and sides).
// Rooms with the same lighting group share their lights (fences do not split lighting groups).
#pragma once
#include <map>
#include <utility>
#include <vector>

// Which half of a tile a diagonal wall leaves to a room (0: the whole tile; 1-4 the halves, 1/3 and
// 2/4 opposite each other).
struct Room__Sides {
  SIMMY_ENUM_BODY(Room__Sides)
};

// 0xA4 bytes.
struct Room {
  using Sides = ::Room__Sides;
  Room(unsigned short id, RoomManager* manager);  // 801374C4
  ~Room();  // 80137578
  void Clear();  // 801376EC
  // The room's static obstacles for the router (BuildRoomPartition), built when first asked for and kept
  // until ClearRoomPartitions.
  std::vector<PenaltyRect>* GetPartition();  // 80137780
  void ComputeRoom();  // 801377D8
  void CollectTileStats(CTilePt &);  // 80137BE0
  void CollectObjectStats(ObjectIterator);  // 80137E44
  void PrintStats();  // 8013825C
  double GetObjectDensity();  // 80138300
  int GetArea();  // 80138358
  void AbsorbNewRoomList(std::vector<CTilePt>& tiles);  // 8013836C
  int IsTileInRoom(CTilePt& tile);  // 801383B4
  int IsOutside();  // 80138420
  void SetOverheadLights(bool on);  // 80138534
  RET_UNKNOWN GetPeopleCount();  // 801385B4
  double GetAmbientLight() const;  // 80138684
  RET_UNKNOWN IsPool();  // 801386D4
  static RET_UNKNOWN Rotate(Room__Sides, int);  // 801386DC
  double GetAmbientLightMultiplier();  // 80138704

  unsigned short m_id = 0;              // +0x00
  unsigned short m_lightingGroup = 0;   // +0x02 rooms sharing lights (ChangeLightingGroup)
  std::vector<CTilePt> m_tiles;         // +0x04
  // +0x14 TArray (0x0C each): the world positions (5 up) of its type 1 light sources (CollectObjectStats).
  // PC: not kept (no reader in the port).
  std::vector<PenaltyRect> m_partition; // +0x24 GetPartition
  int m_inUse = 0;                      // +0x34 holds a computed room
  float m_field38 = 1.0f;               // +0x38 the room's light colour scale (SetRoomIntensityScale)
  float m_field3C = 1.0f;               // +0x3C
  float m_field40 = 1.0f;               // +0x40
  int m_field44 = 0;                    // +0x44 the scale changed (for EHouse::UpdateRoomLights)
  float m_field48 = 0;                  // +0x48 the overhead lights (EHouse::UpdateRoomLights: 0 by day)
  int m_tileStats = 0;                  // +0x4C twice the area (CollectTileStats: 2 a whole tile, 1 a half)
  int m_floorStats = 0;                 // +0x50 of those, the floored ones (same units)
  int m_objectCount = 0;                // +0x54 the objects on its tiles (CollectObjectStats)
  int m_portalLight = 0;                // +0x58 the light of its doors and windows (data 0x33, type 8)
  int m_objectLight = 0;                // +0x5C the light of its other objects (data 0x33)
  int m_data27Sum = 0;                  // +0x60 its objects' data 0x27
  int m_impactSum = 0;                  // +0x64 its objects' room impact (data 7)
  int m_impactCount = 0;                // +0x68 objects with a room impact of 0 or more
  int m_count3B4 = 0;                   // +0x6C objects with data 0x3B 4
  int m_count3B5to7 = 0;                // +0x70 objects with data 0x3B 5..7
  int m_wallSides = 0;                  // +0x74 its wall sides of a scored style (IsScoredStyle)
  int m_paperedSides = 0;               // +0x78 of those, the papered ones
  float m_field7C = 0.0f;               // +0x7C its score (ComputeRoom; scope 0x17 data 4)
  float m_ambientLight = 0.0f;          // +0x80 0..1 (ComputeRoom)
  int m_statsDirty = 1;                 // +0x84 its statistics are to be recollected
  int m_outsideKnown = 0;               // +0x88 IsOutside's answer is cached
  int m_outside = 0;                    // +0x8C
  int m_overheadLights = 0;             // +0x90 its overhead lights are on (SetOverheadLights)
  int m_pool = 0;                       // +0x94 (IsPool)
  RoomManager* m_manager = nullptr;     // +0x98
  int m_lightSources = 0;               // +0x9C its light-giving objects other than doors and windows
  int m_people = 0;                     // +0xA0 the people on its tiles
};

// Both halves of a tile split by a diagonal wall: the room and side of each (0xFFFF: no room yet).
struct DiagonalNode {
  unsigned short m_field00 = 0;
  unsigned short m_room = 0xffff;
  int m_side = 0;
};

// 0x30 bytes (Globs::pRoomManager; GC sRoomMgr too).
struct RoomManager {
  RoomManager();  // 80135DAC
  ~RoomManager();  // 80135E6C
  void RoomScoreChanged(int);  // 80136014
  void RoomLightingChanged(int);  // 801360B0
  void AllRoomsLightingChanged();  // 8013614C
  void AllRoomsScoreChanged();  // 801361E0
  void ComputeRooms();  // 80136274
  void PrintStats();  // 80136584
  Room* GetRoom(unsigned short id);  // 80136644
  Room* GetNewRoom(unsigned short id);  // 801366F0
  void UpdateRooms();  // 80136800
  void OffsetWorld(CTilePt &);  // 80136848
  int ProcessDegenerateTile(CTilePt& tile, unsigned short room, Room__Sides side);  // 80136A2C
  int ResolveDiagonal(CTilePt& tile, Room** room1, Room** room2, Room__Sides* side1, Room__Sides* side2);  // 80136C18
  bool ResolveDiagonal(CTilePt& tile, unsigned short* room1, unsigned short* room2, Room__Sides* side1, Room__Sides* side2);  // 80136DB4
  void ResetRooms();  // 80136EEC
  static void InitLights();  // 80136FB0
  static double GetOutsideAmbientLevel();  // 80136FB4
  void ClearRoomPartitions();  // 80137034
  void ResetRoomManager();  // 801370D4
  static void SetRoomIntensityScale(int, bool, float);  // 80137234
  static void SetRoomIntensityColorScale(int, bool, int, float);  // 80137318
  void ChangeLightingGroup(unsigned short room, unsigned short group);  // 80137428
  static RoomManager* GetRoomManager();  // 8013884C

  // CTilePt::operator< (800B47CC): by x, then y.
  struct TileLess {
    bool operator()(const CTilePt& a, const CTilePt& b) const {
      return a.m_x < b.m_x || (a.m_x == b.m_x && a.m_y < b.m_y);
    }
  };

  std::map<unsigned short, Room*> m_rooms;                                           // +0x00
  std::map<CTilePt, std::pair<DiagonalNode, DiagonalNode>, TileLess> m_diagonals;   // +0x10
  int m_dirty = 0;                      // +0x20 the rooms' statistics are to be recomputed
  int m_lightsInited = 0;               // +0x24
  float m_outdoorScore = 0.0f;          // +0x28
  float m_outdoorObjects = 0.0f;        // +0x2C
};

extern RoomManager* Globs_pRoomManager;  // Globs::pRoomManager
