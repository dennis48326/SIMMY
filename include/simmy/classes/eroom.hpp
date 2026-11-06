// @class ERoom after EIStaticModel TileWalls CTilePt EWallUpDownStateType TrapezoidFade WallFadeParms
// @class ERoomWall
// @class EIWallPart
// @class EIFenceWall after EIWallPart
// @class EFenceWall after ERoomWall EIFenceWall
// @class EWallUpDownStateType
// The house's walls (game/eroom.cpp). ERoom::Init walks the lot's tiles and joins each wall segment
// into runs (ERoomWall) along its row or column, split where walls cross; each tile of a run is an
// EIWallPart: a wall model (one of ten end-cap variants, g_wallTable) placed and turned on the
// tile, with the tile's wallpaper. A wall model has five parts (kWallShaderOrder): the two faces,
// the cut-down top, and the caps; the walls up / down state picks which are drawn.
#pragma once
#include <vector>

// 0: walls up, 1: fading, 2: walls down.
struct EWallUpDownStateType {
  SIMMY_ENUM_BODY(EWallUpDownStateType)
};

struct ERoom {
  ERoom();  // 8002D778
  ~ERoom();  // 8002D88C
  void InitRoomLookupTab();  // 8002DCF0
  RET_UNKNOWN GetNumWalls(unsigned short);  // 8002DF7C
  RET_UNKNOWN GetWallPaperCost(unsigned int, unsigned short);  // 8002DFEC
  RET_UNKNOWN DrawWallpaperPreview(ERC *, unsigned short, EVec2 &);  // 8002E0C4
  RET_UNKNOWN IsRoomWallsOpaque(int, int);  // 8002E168
  void CalAllRoomOpaque(int);  // 8002E1C4
  double GetShortDistToCam(int, int);  // 8002E268
  double GetOccludeAlpha(int, int);  // 8002E2C4
  RET_UNKNOWN IsRoomRoofOpaque(int, int);  // 8002E324
  void CalRoofOpaque(int);  // 8002E380
  void CalShortDistToCam(int);  // 8002E74C
  void SetWallState(EWallUpDownStateType state);  // 8002E8A4
  void UpdateWallFade(int context);  // 8002EAB8
  void Init();  // 8002EBB8
  void ProcStandardWalls(bool alongY, int& walls, int& tiles, bool create);  // 8002EC60
  void ProcDiagonalWalls(int& walls, int& tiles, bool create);  // 8002EF04
  void ProcessCell(std::vector<ERoomWall*>& list, ERoomWall*& current, CTilePt& tile, TileWallsSegment segment, DiagonalSideSelector side, TileWalls& walls, int splitTest, int& wallCount, int& tileCount, bool create);  // 8002F264
  bool PreviewWallBuild(bool fences);  // 8002F4DC
  RET_UNKNOWN FindWallContainingSegment(TNodeList<ERoomWall *> &, TileWallsSegment, CTilePt &, CTilePt &);  // 8002FD08
  EIWallPart* GetWallFromTileAndSegment(TileWallsSegment segment, CTilePt& tile);  // 8002FE18
  static RET_UNKNOWN DeleteERoomWallContainingSegment(TileWallsSegment, CTilePt &, CTilePt &);  // 8002FEAC
  static int DeleteWallAtTile(CTilePt& tile, TileWalls& walls, TileWallsSegment segment);  // 8002FF8C
  void KillArchitecturalObject(CTilePt &, TileWallsSegment, int &, bool);  // 80030104

  int m_field00 = 0;                         // +0x00
  // +0x04, +0x10, +0x1C, +0x28: the runs of segments 8, 4, 2 and 1; +0x34 .. +0x58 the diagonals'
  // (GC: TNodeList<ERoomWall*>, 0x0C bytes each, with an array of pointers to them at +0x70).
  std::vector<ERoomWall*> m_walls[8];
  std::vector<ERoomWall*> m_fences;          // +0x64
  // +0x94, +0xA0: ERedBlackTree (the room lookup tables, InitRoomLookupTab)
  float m_field_AC = 300.0f;                 // +0xAC
  float m_field_B0 = 200.0f;                 // +0xB0

  static WallFadeParms s_wallFadeParmsDC;       // 8047547C
  static WallFadeParms s_wallFadeParmsClassic;  // 804754AC
  static WallFadeParms* s_wallFadeParms;        // 804FD99C the set in use (SetupInterestFade)
};

// A run of wall segments (0x40 bytes).
struct ERoomWall {
  static double GetWallMeterValue(int &);  // 800293A0
  virtual RET_UNKNOWN IsWallsOpaque(int);  // 800295AC
  virtual RET_UNKNOWN CalWallsOpaque(int);  // 800295BC
  virtual double CalShortDistToCam(int);  // 80029654
  ERoomWall(TileWallsSegment segment, DiagonalSideSelector side, CTilePt& tile, bool create);  // 8002C5CC
  virtual ~ERoomWall();  // 8002C7B8
  static void GetWallDims(TileWallsSegment, DiagonalSideSelector, EVec3 &, EVec3 &);  // 8002C860
  void SetWallUpDownMode(EWallUpDownStateType state, bool force);  // 8002CD84
  void AddTile(CTilePt& tile, TileWalls& walls, TileWallsSegment segment, DiagonalSideSelector side, bool create);  // 8002CDD8
  RET_UNKNOWN DrawWall(ERC *);  // 8002CE4C
  static RET_UNKNOWN getRoomIdFromPoint(CTilePt &);  // 8002D1F8
  RET_UNKNOWN getWallNormalOnSideOfCursor(EVec3 &, EVec3 &, EVec2 &, EVec2 &);  // 8002D308
  RET_UNKNOWN DrawWallpaperPreview(ERC *, EVec2 &);  // 8002D3C0
  static RET_UNKNOWN GetWallPaperCost(int, unsigned short);  // 8002DE70
  RET_UNKNOWN CountWalls();  // 8002F81C
  RET_UNKNOWN HasSegment(TileWallsSegment, CTilePt &, CTilePt &);  // 8002F84C
  RET_UNKNOWN DeleteWallAtTile(CTilePt &);  // 8002FA34
  void RemoveWallsFromWorld();  // 8002FBEC
  EIWallPart* GetWallAtTile(CTilePt& tile);  // 8002FD7C
  ERoomWall();  // 800308A0
  virtual void SafeDelete();  // 80030914
  double GetShortDistToCam(int);  // 80030954

  static int m_tileCount;                    // 804FD994 (ERoom::Init)
  static int m_wallCount;                    // 804FD998

  CTilePt m_start;                           // +0x00
  CTilePt m_end;                             // +0x03
  std::vector<EIWallPart*> m_parts;          // +0x14 one per tile (GC: TNodeList)
  int m_segment = 0;                         // +0x20 TileWallsSegment
  int m_side = 0;                            // +0x24 DiagonalSideSelector
  unsigned short m_room = 0;                 // +0x28
  int m_field2C = 0;                         // +0x2C
  int m_field30 = 0;                         // +0x30
  float m_field34 = 10000.0f;                // +0x34
  float m_field38 = 10000.0f;                // +0x38
};

// One tile of a wall (0x3B8 bytes).
struct EIWallPart : public EIStaticModel {
  static int GetWallType(unsigned char ends);  // 800273FC
  virtual RET_UNKNOWN IsOpaque();  // 800274A4
  void GetShadowModel();  // 80029D2C
  EIWallPart();  // 80029DF0
  SIMMY_STORABLE_DECL(EIWallPart)
  void OptimizeOrderTable();  // 80029ED0
  EIWallPart(TileWalls& walls, TileWallsSegment segment, DiagonalSideSelector side, CTilePt& tile);  // 80029FA4
  void SetOrient(EMat4& m) override;  // 8002A2D4
  unsigned int VisibilityTest(E3DWindow& win) override;  // 8002A408
  void Draw(ELevelDrawData& data) override;  // 8002A4C0
  static void WallOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 8002B1C4
  void DrawShadow(ELevelDrawData &) override;  // 8002B504
  static void DrawOutsideShadows(std::vector<EInstance*>& walls, EVec3& sunDir, ELevelDrawData& data);  // 8002B68C (PC: the list is a std::vector)
  RET_UNKNOWN GetAdjacentRoom();  // 8002B99C
  void GetPoint(CTilePt &);  // 8002B9A4
  void GetPoints(CTilePt &, CTilePt &);  // 8002BADC
  ~EIWallPart() override;  // 8002BCB4
  void GetWallModelId(CTilePt& tile, TileWalls& walls, TileWallsSegment segment, DiagonalSideSelector side, float& angle);  // 8002BD58
  void ChangeWallpaper(unsigned int shaderId);  // 8002C1EC
  ERShader* GetWallpaperShader();  // 8002C274
  void SetVisible(bool visible);  // 8002C2C0
  unsigned int GetVisible(int context);  // 8002C32C
  void SetWallState(EWallUpDownStateType state, bool force);  // 8002C35C
  static void DoFadeCollisonWallList(TrapezoidFade& fade, EVec3& pos, std::vector<EIWallPart*>& parts);  // 8002E938
  static void DoFadeCollisonForList(TrapezoidFade& fade, std::vector<ERoomWall*>& walls);  // 8002EA64
  static void RegisterType(unsigned short);  // 800305A8
  void CreateCopy() const;  // 800305FC
  RET_UNKNOWN IsDiagonal();  // 80030624
  static bool isDiagonal(TileWallsSegment);  // 8003063C
  void SetForceFadeOff(bool off);  // 80030654
  void RealizeShaderTuning() override;  // 8003065C

  int m_segment = 0;                         // +0x320 TileWallsSegment
  int m_side = 0;                            // +0x324 DiagonalSideSelector
  CTilePt m_tile;                            // +0x328
  unsigned int m_parts[2] = {0xff, 0xff};    // +0x32C per context: the parts drawn (bits by slot)
  ESubModelShader* m_slots[5] = {};          // +0x334 the model's parts by kWallShaderOrder
  signed char m_slotIndex[5] = {};           // +0x348 their sub-model shader (and digest) index
  int m_field350 = 0;                        // +0x350
  unsigned int m_wallModel = 0;              // +0x354
  short m_room = 0;                          // +0x358 the room at its first point
  short m_room2 = 0;                         // +0x35A the room at its second point
  float m_normal[2] = {};                    // +0x35C the face's normal (model space)
  float m_planePoint[2] = {};                // +0x364 a point on the face
  float m_pointA[2] = {};                    // +0x36C the light sample points (model space)
  float m_pointB[2] = {};                    // +0x374
  float m_worldNormal[2] = {};               // +0x37C (SetOrient)
  float m_planeD = 0;                        // +0x384
  float m_worldA[3] = {};                    // +0x388
  float m_worldB[3] = {};                    // +0x394
  ERShader* m_wallpaper = nullptr;           // +0x3A0
  float m_partAlpha[2] = {};                 // +0x3A4 per context: the faces' fade
  EResource* m_shadowModel = nullptr;        // +0x3AC
  int m_forceFadeOff = 0;                    // +0x3B0 never fades (always up)
};

// A fence tile (0x3B8 bytes): one of six fence models (picket, rail, iron; straight or diagonal) on the
// tile's segment, in the house's level like a wall part, never opaque.
struct EIFenceWall : public EIWallPart {
  static double GetFenceMeterValue();  // 80029360
  unsigned int VisibilityTest(E3DWindow& win) override;  // 80029580
  EIFenceWall(TileWallsSegment& segment, CTilePt& tile, WallStyle style);  // 80029890
  ~EIFenceWall() override;  // 80029B94
  void Draw(ELevelDrawData& data) override;  // 8002A4A0
  SIMMY_STORABLE_DECL(EIFenceWall)
  static void RegisterType(unsigned short);  // 80030760
  void CreateCopy() const;  // 800307B4
  EIFenceWall();  // 80030850
  RET_UNKNOWN IsOpaque() override;  // 80030898

  static int m_nInstances;  // the fence meter's count
};

// A fence (0x40 bytes, in ERoom's fence list): one EIFenceWall.
struct EFenceWall : public ERoomWall {
  EFenceWall(TileWallsSegment& segment, CTilePt& tile, WallStyle style);  // 8002C48C
  RET_UNKNOWN IsWallsOpaque(int) override { return 0; }  // linkonce 8039E4B0
  RET_UNKNOWN CalWallsOpaque(int i) override { return i == 0 ? m_field2C : m_field30; }  // linkonce 8039E4B8
};

