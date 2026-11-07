// @class DiagonalSideSelector
// @class TileWallStorage
// @class TileWallsSegment
// @class TileWalls
// @class TileWalls__SheerPlacement
// @class WallStyle
// A tile's walls (game/tilewalls.cpp). The world keeps 8 bytes per tile (TileWallStorage, the
// 'Arry' 12 array); TileWalls unpacks them:
//   byte 0  the segments present (TileWallsSegment bits: 1, 2, 4, 8 the edges, 0x10 / 0x20 diagonals)
//   byte 1  the placements (two bits per edge segment, SheerPlacement)
//   byte 2  the style of segment 1 (and the floor pattern of diagonal sides 1 and 2)
//   byte 3  the style of segment 2 (and of the diagonals)
//   byte 4  pattern slot 0 (segment 1; the floor pattern of the other diagonal sides)
//   byte 5  pattern slot 1 (segment 2)
//   byte 6  pattern slots 3, 4 and, with 0x10 / 0x20, 6 (the diagonals' sides)
//   byte 7  pattern slots 2, 5 and, with 0x10 / 0x20, 7
// Patterns are read through SegmentToIndex (the segment's bit number plus 1 for sides 1 and 4,
// plus 2 for side 3). Styles 2, 0xC, 0xD and 0xE are fences.
#pragma once

struct DiagonalSideSelector {
  SIMMY_ENUM_BODY(DiagonalSideSelector)
};

struct TileWallsSegment {
  SIMMY_ENUM_BODY(TileWallsSegment)
  static constexpr int kDiagonal1 = 0x10;
  static constexpr int kDiagonal2 = 0x20;
};

struct TileWalls__SheerPlacement {
  SIMMY_ENUM_BODY(TileWalls__SheerPlacement)
};

// A wall's style (2, 0xC, 0xD, 0xE: fences).
struct WallStyle {
  SIMMY_ENUM_BODY(WallStyle)
};

struct TileWallStorage {
  unsigned char m_bytes[8] = {};
};

struct TileWalls {
  using SheerPlacement = ::TileWalls__SheerPlacement;
  TileWalls(TileWallStorage &);  // 80145DA0
  TileWalls(TileWallStorage &, bool, bool, bool, bool);  // 80145E30
  RET_UNKNOWN GenerateRotationLookups();  // 80145EF0
  int GetPlacement(TileWallsSegment) const;  // 801460C4
  int SetPlacement(TileWalls__SheerPlacement, TileWallsSegment);  // 80146134
  void Rotate(int);  // 801461AC
  int CanAdd(TileWallsSegment) const;  // 80146434
  static void GetAdjacentTile(TileWallsSegment, CTilePt *);  // 80146550
  static int GetOppositeSegment(TileWallsSegment);  // 80146614
  static int DirToWallSeg(TilePtDir);  // 80146680
  static RET_UNKNOWN GetWallBetween(TilePtDir);  // 801466D4
  static int SegmentToIndex(TileWallsSegment, DiagonalSideSelector);  // 8014674C
  static int IndexToSegment(int, DiagonalSideSelector *);  // 801467C4
  static bool IsSingleWall(TileWallsSegment);  // 8014683C
  TileWalls();  // 80146880
  TileWalls(TileWalls &);  // 801468B8
  ~TileWalls();  // 80146914
  void operator=(TileWalls &);  // 8014693C
  bool HasWall(TileWallsSegment) const;  // 80146998
  bool HasWall() const;  // 801469B0
  bool HasWallNotFence(TileWallsSegment) const;  // 801469C8
  bool HasFenceNotWall(TileWallsSegment) const;  // 80146A34
  bool HasDiagonal() const;  // 80146A9C
  bool HasDiagonalNotFence() const;  // 80146AB4
  bool HasDiagonalFence() const;  // 80146B7C
  unsigned short GetPattern(TileWallsSegment, DiagonalSideSelector) const;  // 80146C44
  int SetPattern(unsigned short pattern, TileWallsSegment, DiagonalSideSelector);  // 80146CCC
  unsigned short GetStyle(TileWallsSegment) const;  // 80146D80
  int SetStyle(WallStyle, TileWallsSegment);  // 80146DD8
  int AddWall(TileWallsSegment);  // 80146E4C
  void RemoveWall(TileWallsSegment);  // 80146EB8
  void RemoveAllWalls();  // 80146F04
  int First() const;  // 80146F10
  int Next(TileWallsSegment) const;  // 80146F78
  unsigned short GetFloorValue(DiagonalSideSelector) const;  // 80146FDC
  unsigned short SetFloorValue(unsigned short pattern, DiagonalSideSelector);  // 80146FF8
  void ConvertToWorldCoords();  // 80147020
  static int RotateSegment(TileWallsSegment, int);  // 80147050
  static int RotateDiagonal(DiagonalSideSelector side, int quarters);  // 8014706C

  unsigned int m_segments = 0;          // +0x00 TileWallsSegment bits
  unsigned int m_placements = 0;        // +0x04 two bits per edge segment
  unsigned short m_styles[6] = {};      // +0x08 by SegmentToIndex(segment, 0)
  unsigned short m_patterns[8] = {};    // +0x14 by SegmentToIndex(segment, side)
  int m_field24 = 0;                    // +0x24
  int m_flag28 = 0;                     // +0x28 (the second constructor's third flag)
  int m_flag2C = 0;                     // +0x2C (its fourth)
  int m_flag30 = 0;                     // +0x30 (its first)
  int m_flag34 = 0;                     // +0x34 (its second)
};
