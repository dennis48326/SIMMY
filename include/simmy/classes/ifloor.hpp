// @class EFloorShdTblNode after EInstance ERShader EDL
// @class EFloorStripInfo
// @class EFloorTileGroup
// @class EFloorTileStrip
// @class EFloorVertexSpan
// @class EIFloor
// A house's floors (game/ifloor.cpp): the floor patterns of the lot's tiles gathered by pattern into
// strips (a column's run of tiles of one pattern in one room), each pattern's strips of a room
// recorded into a display list drawn with the pattern's shader, one EIFloor per room.
#pragma once
#include <vector>

// A strip of tiles (4 bytes): the room it lights by, the tile column, its first and last tile (low 6
// bits; the high two mark half (diagonal) tiles at the ends).
struct EFloorStripInfo {
  unsigned char m_room = 0;   // +0x00
  unsigned char m_x = 0;      // +0x01
  unsigned char m_start = 0;  // +0x02
  unsigned char m_end = 0;    // +0x03
};

// A built strip (0x10 bytes): its tiles, vertex count and vertex block.
struct EFloorTileStrip {
  EFloorStripInfo m_info;     // +0x00
  int m_count = 0;            // +0x04 vertices
  float* m_positions = nullptr;  // +0x08 the block (positions, colours, uvs, normals)
  int m_size = 0;             // +0x0C
};

// A floor's vertex span for the lighting (3 bytes): a column of light-grid vertices from `m_first` to
// `m_last` (the vertex [x][y] of the grid: world x = y, world y = x).
struct EFloorVertexSpan {
  unsigned char m_x = 0;      // +0x00
  unsigned char m_first = 0;  // +0x01
  unsigned char m_last = 0;   // +0x02
};

// One pattern's strips of a floor (0x14 bytes), in a list.
struct EFloorTileGroup {
  EFloorTileGroup(ERShader* shader, EDL* dl, EFloorTileStrip* strips, int count, EFloorTileGroup* next);  // 8004809C
  ~EFloorTileGroup();  // 800480E4

  EFloorTileGroup* m_next = nullptr;   // +0x00
  EDL* m_dl = nullptr;                 // +0x04
  int m_count = 0;                     // +0x08
  EFloorTileStrip* m_strips = nullptr; // +0x0C
  ERShader* m_shader = nullptr;        // +0x10
};

// The floor table (static): per floor pattern its shader and its strips.
struct EFloorShdTblNode {
  EFloorShdTblNode();  // 80045520
  ~EFloorShdTblNode();  // 80045544
  void CleanUp();  // 80045588
  static void InitTable();  // 800455D0
  static void AddStripToTable(int pattern, EFloorStripInfo& info);  // 800456D4
  static void AddLightStrip(EFloorStripInfo info);  // 8004571C
  static void BuildTable(ERLevel* level);  // 80045A28
  static bool CheckForObjectsInFloor(CTilePt& tile);  // 80046150
  static void BuildStrip(ERC* rc, EFloorStripInfo info, EVec2& offset, EFloorTileStrip* strip, bool normals,
                         ERLevel* level, EBound3& bounds, bool& first);  // 80046A9C
  static int EmptyTable();  // 80047104
  static RET_UNKNOWN RemapFloorAndGrassId(unsigned int);  // 800477A0
  static bool PreviewTable();  // 80047EB8

  ERShader* m_shader = nullptr;          // +0x00
  std::vector<EFloorStripInfo> m_strips; // +0x04 ENodeList

  static std::vector<EFloorShdTblNode> s_table;  // _eFloorOrderTable (one per floor pattern)
  static int s_inited;                   // m_bTableInited
  static unsigned int s_grassShaderId;   // m_grassShaderId
  static int s_stripCount;               // _m_nStrips
  static std::vector<EFloorStripInfo> s_lightStrips[256];  // m_stripsForLighting (per room)
};

// A room's floor (0x130 bytes).
struct EIFloor : public EInstance {
  EIFloor();  // 80047234
  SIMMY_STORABLE_DECL(EIFloor)
  ~EIFloor() override;  // 800484B4
  void DoLightingCalculation() override;  // 80046614
  void Draw(ELevelDrawData& data) override;  // 80047318
  void DrawStencil(ELevelDrawData &) override;  // 800474AC
  void DrawShadow(ELevelDrawData &) override;  // 800475A4
  unsigned int VisibilityTest(E3DWindow& win) override;  // 80047600
  static void DestroyFloors();  // 8004769C
  static void CreateFloors(EHouse* house);  // 800477D0
  void Cleanup();  // 80047D84
  static bool TestCreateFloors();  // 80047E24
  RET_UNKNOWN IsRoomInFloor(unsigned short);  // 80047E54
  const char* GetInstName(EInstance__InstType& type) override;  // 80047EA4
  static void RegisterType(unsigned short);  // 80048440
  void CreateCopy() const;  // 80048494
  void Update() override;  // 8004850C
  virtual RET_UNKNOWN GetNumRoomID();  // 80048510

  int m_room = 0;                         // +0x7C
  int m_roomCount = 0;                    // +0x80 other rooms whose strips it draws
  unsigned short m_rooms[64] = {};        // +0x84
  EFloorTileGroup* m_groups = nullptr;    // +0x104
  int m_spanCount = 0;                    // +0x108
  EFloorVertexSpan* m_spans = nullptr;    // +0x10C
  float m_floorBounds[6] = {};            // +0x110 EBound3
  float m_cutHeight = 0;                  // +0x128 (ERLevel::Draw: the cut-away height)
  int m_hidden = 0;                       // +0x12C (ERLevel::Draw: not drawn this frame)
};
