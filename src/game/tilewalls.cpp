// Port of game/tilewalls.cpp (partial): unpacking a tile's 8-byte wall entry, reading and editing it
// (the build tools: CanAdd, Add/RemoveWall, SetPattern, SetFloorValue) and the segment helpers.
// Still unported: GenerateRotationLookups (RotateSegment computes the table's entries), Rotate,
// RotateDiagonal.
#include "simmy/types_gen.hpp"

namespace {
bool IsFenceStyle(int style) { return style == 2 || style == 0xc || style == 0xd || style == 0xe; }
}  // namespace

// 80145DA0
TileWalls::TileWalls(TileWallStorage& s) {
  const unsigned char* b = s.m_bytes;
  m_segments = b[0];
  m_placements = b[1];
  m_styles[0] = b[2];
  m_styles[1] = b[3];
  m_styles[4] = b[3];
  m_styles[5] = b[3];
  m_patterns[0] = b[4];
  m_patterns[1] = b[5];
  m_patterns[2] = b[7];
  m_patterns[3] = b[6];
  m_patterns[4] = b[6];
  m_patterns[5] = b[7];
  m_patterns[6] = b[6];
  m_patterns[7] = b[7];
  m_field24 = m_flag28 = m_flag2C = m_flag30 = m_flag34 = 0;
}

// 80145E30
// As above, but the diagonals' pattern slots only for the diagonals present.
TileWalls::TileWalls(TileWallStorage& s, bool a, bool b2, bool c, bool d) {
  const unsigned char* b = s.m_bytes;
  m_segments = b[0];
  m_placements = b[1];
  m_styles[0] = b[2];
  m_styles[1] = b[3];
  m_styles[4] = b[3];
  m_styles[5] = b[3];
  m_patterns[0] = b[4];
  m_patterns[1] = b[5];
  m_patterns[2] = b[7];
  m_patterns[3] = b[6];
  if (m_segments & 0x10) {
    m_patterns[4] = b[6];
    m_patterns[5] = b[7];
  } else {
    m_patterns[4] = m_patterns[5] = 0;
  }
  if (m_segments & 0x20) {
    m_patterns[6] = b[6];
    m_patterns[7] = b[7];
  } else {
    m_patterns[6] = m_patterns[7] = 0;
  }
  m_flag30 = a;
  m_field24 = 0;
  m_flag34 = b2;
  m_flag28 = c;
  m_flag2C = d;
}

// 801460C4
// The edge segment's placement (two bits); 3 for the diagonals.
int TileWalls::GetPlacement(TileWallsSegment segment) const {
  HasWall(segment);
  IsSingleWall(segment);
  const int index = SegmentToIndex(segment, 0);
  if (index > 3) return 3;
  return static_cast<int>(m_placements & (3u << (index * 2))) >> (index * 2);
}

// 80146134
int TileWalls::SetPlacement(TileWalls__SheerPlacement placement, TileWallsSegment segment) {
  HasWall(segment);
  IsSingleWall(segment);
  const int index = SegmentToIndex(segment, 0);
  if (index < 4)
    m_placements = (m_placements & ~(3u << (index * 2))) | (static_cast<unsigned int>(placement.value) << (index * 2));
  return segment;
}

// 8014674C
// The segment's bit number, plus 1 for sides 1 and 4 and 2 for side 3.
int TileWalls::SegmentToIndex(TileWallsSegment segment, DiagonalSideSelector side) {
  int bits = segment;
  int index = 0;
  int guard = 0xff;
  while (((bits ^ 1) & 1) != 0) {
    bits >>= 1;
    ++index;
    guard >>= 1;
    if (guard == 0) break;
  }
  if (side == 1 || side == 4) return index + 1;
  if (side == 3) return index + 2;
  return index;
}

// 801467C4
// The segment of bit `index`, with `side` (when given) the diagonal side: bits 4/5 are the first
// diagonal's sides 0 and 2, bits 5/6/7 the second's 4, 1 and 3.
int TileWalls::IndexToSegment(int index, DiagonalSideSelector* side) {
  int segment = 1 << index;
  if (!side) return segment;
  *side = 0;
  if (segment < 0x10) return segment;
  if (segment == 0x10) {
    *side = 2;
    return segment;
  }
  if (segment == 0x20) {
    *side = 4;
    return 0x10;
  }
  if (segment == 0x40) {
    *side = 1;
    return 0x20;
  }
  if (segment != 0x80) return segment;
  *side = 3;
  return 0x20;
}

// 8014683C
bool TileWalls::IsSingleWall(TileWallsSegment segment) {
  int count = 0;
  for (unsigned int bits = static_cast<unsigned int>(segment.value); bits; bits &= bits - 1) ++count;
  return count == 1 && segment <= 0x20;
}

// 80146880
TileWalls::TileWalls() {}

// 801468B8
TileWalls::TileWalls(TileWalls& other) = default;

// 80146914
TileWalls::~TileWalls() {}

// 8014693C
void TileWalls::operator=(TileWalls& other) {
  m_segments = other.m_segments;
  m_placements = other.m_placements;
  for (int i = 0; i < 6; ++i) m_styles[i] = other.m_styles[i];
  for (int i = 0; i < 8; ++i) m_patterns[i] = other.m_patterns[i];
  m_field24 = other.m_field24;
  m_flag28 = other.m_flag28;
  m_flag2C = other.m_flag2C;
  m_flag30 = other.m_flag30;
  m_flag34 = other.m_flag34;
}

// 80146998
bool TileWalls::HasWall(TileWallsSegment segment) const {
  return (m_segments & static_cast<unsigned int>(segment.value)) == static_cast<unsigned int>(segment.value);
}

// 801469B0
bool TileWalls::HasWall() const { return m_segments != 0; }

// 801469C8
bool TileWalls::HasWallNotFence(TileWallsSegment segment) const {
  if (!HasWall(segment)) return false;
  return !IsFenceStyle(GetStyle(static_cast<int>(m_segments & static_cast<unsigned int>(segment.value))));
}

// 80146A34
bool TileWalls::HasFenceNotWall(TileWallsSegment segment) const {
  if (!HasWall(segment)) return false;
  return IsFenceStyle(GetStyle(static_cast<int>(m_segments & static_cast<unsigned int>(segment.value))));
}

// 80146A9C
bool TileWalls::HasDiagonal() const { return (m_segments & 0x30) != 0; }

// 80146C44
// Side 0 of a diagonal reads as side 4 (the first) or 1 (the second).
unsigned short TileWalls::GetPattern(TileWallsSegment segment, DiagonalSideSelector side) const {
  IsSingleWall(segment);
  HasWall(segment);
  if (segment == 0x10 && side == 0) side = 4;
  else if (segment == 0x20 && side == 0) side = 1;
  return m_patterns[SegmentToIndex(segment, side)];
}

// 80146D80
unsigned short TileWalls::GetStyle(TileWallsSegment segment) const {
  IsSingleWall(segment);
  HasWall(segment);
  return m_styles[SegmentToIndex(segment, 0)];
}

// 80146DD8
// Every slot whose segment is in `segments` (0xFF: all) takes the style.
int TileWalls::SetStyle(WallStyle style, TileWallsSegment segments) {
  if (segments != 0xff) HasWall(segments);
  for (int i = 0; i < 6; ++i)
    if (IndexToSegment(i, nullptr) & segments) m_styles[i] = static_cast<unsigned short>(style.value);
  return segments;
}

// 80146F10
// The lowest segment present (0: none).
int TileWalls::First() const {
  if (m_segments == 0) return 0;
  int segment = 1;
  while (!HasWall(segment)) segment <<= 1;
  IsSingleWall(segment);
  return segment;
}

// 80146F78
// The next segment present after `segment` (0: none).
int TileWalls::Next(TileWallsSegment segment) const {
  IsSingleWall(segment);
  int next = segment;
  do {
    next <<= 1;
    if (HasWall(next)) break;
  } while (next < 0x20);
  return next < 0x20 ? next : 0;
}

// 80146FDC
// A diagonal tile's floor pattern on `side` (sides 1 and 2 the first half).
unsigned short TileWalls::GetFloorValue(DiagonalSideSelector side) const {
  return static_cast<unsigned int>(side - 1) <= 1 ? m_styles[0] : m_patterns[0];
}

// 80146550
// The tile across `segment` (4: x+1, 1: x-1, 2: y-1, 8: y+1).
void TileWalls::GetAdjacentTile(TileWallsSegment segment, CTilePt* tile) {
  int dir;
  switch (static_cast<int>(segment)) {
    case 4: dir = 3; break;
    case 1: dir = 2; break;
    case 2: dir = 0; break;
    case 8: dir = 1; break;
    default: return;
  }
  const CTilePt& d = CTilePt::sDirections[dir];
  tile->m_x = static_cast<signed char>(tile->m_x + d.m_x);
  tile->m_y = static_cast<signed char>(tile->m_y + d.m_y);
  tile->m_level = static_cast<signed char>(tile->m_level + d.m_level);
}

// 80146434
// A diagonal only alone on its tile; with the neighbours' diagonal flags (the second constructor's), not
// the edge a neighbouring diagonal leans on (segment 2: +0x34, 1: +0x30, 4: +0x2C, 8: +0x28), turned by
// the tile's rotation.
int TileWalls::CanAdd(TileWallsSegment segment) const {
  IsSingleWall(segment);
  const unsigned int all = static_cast<unsigned int>(segment.value) | m_segments;
  if (((all & 0x10) && all != 0x10) || ((all & 0x20) && all != 0x20)) return 0;
  int seg = segment;
  if (m_flag28 || m_flag2C || m_flag30 || m_flag34) {
    const int turn = m_field24 ? 4 - m_field24 : 0;
    if (turn) seg = RotateSegment(seg, turn);
    if (seg == 2) return m_flag34 ^ 1;
    if (seg == 1) return m_flag30 ^ 1;
    if (seg == 4) return m_flag2C ^ 1;
    if (seg == 8) return m_flag28 ^ 1;
  }
  return 1;
}

// 80146614
int TileWalls::GetOppositeSegment(TileWallsSegment segment) {
  switch (static_cast<int>(segment)) {
    case 1: return 4;
    case 4: return 1;
    case 2: return 8;
    case 8: return 2;
    case 0x10: return 0x10;
    case 0x20: return 0x20;
    default: return 0;
  }
}

// 80146680
// Directions 0/1 (along y) run on segment 1, 2/3 (along x) on segment 2, 4/5 the 0x20 diagonal, 6/7 0x10.
int TileWalls::DirToWallSeg(TilePtDir dir) {
  const int d = dir;
  if (d < 4) {
    if (d > 1) return 2;
    if (d >= 0) return 1;
  } else {
    if (d < 6) return 0x20;
    if (d < 8) return 0x10;
  }
  return 1;
}

// 80146CCC
// Every pattern slot of `segments` (0xFF: all) on `side` (a diagonal's side 0 is its side 4 / 1).
int TileWalls::SetPattern(unsigned short pattern, TileWallsSegment segments, DiagonalSideSelector side) {
  if (segments != 0xff) HasWall(segments);
  int s = side;
  if (segments == 0x10 && s == 0) s = 4;
  else if (segments == 0x20 && s == 0) s = 1;
  for (int i = 0; i < 8; ++i) {
    DiagonalSideSelector slotSide{0};
    if ((IndexToSegment(i, &slotSide) & segments) && slotSide == s) m_patterns[i] = pattern;
  }
  return segments;
}

// 80146E4C
// A diagonal clears the tile's floor values (style slot 0 and pattern slot 0 hold them).
int TileWalls::AddWall(TileWallsSegment segment) {
  IsSingleWall(segment);
  CanAdd(segment);
  m_segments |= static_cast<unsigned int>(segment.value);
  if (segment == 0x10 || segment == 0x20) {
    m_patterns[0] = 0;
    m_styles[0] = 0;
  }
  return segment;
}

// 80146EB8
void TileWalls::RemoveWall(TileWallsSegment segment) {
  IsSingleWall(segment);
  HasWall(segment);
  m_segments &= ~static_cast<unsigned int>(segment.value);
}

// 80146F04
void TileWalls::RemoveAllWalls() { m_segments = 0; }

// 80146FF8
// A diagonal tile's floor on `side` (sides 1 and 2: style slot 0, else pattern slot 0); the old value.
unsigned short TileWalls::SetFloorValue(unsigned short pattern, DiagonalSideSelector side) {
  unsigned short old;
  if (static_cast<unsigned int>(side - 1) < 2) {
    old = m_styles[0];
    m_styles[0] = pattern;
  } else {
    old = m_patterns[0];
    m_patterns[0] = pattern;
  }
  return old;
}

// 80147020
void TileWalls::ConvertToWorldCoords() {
  if (m_field24) Rotate(4 - m_field24);
}

// 80146AB4
bool TileWalls::HasDiagonalNotFence() const {
  if ((m_segments & 0x10) && !IsFenceStyle(GetStyle(0x10))) return true;
  if ((m_segments & 0x20) && !IsFenceStyle(GetStyle(0x20))) return true;
  return false;
}

// 80146B7C
bool TileWalls::HasDiagonalFence() const {
  if ((m_segments & 0x10) && IsFenceStyle(GetStyle(0x10))) return true;
  if ((m_segments & 0x20) && IsFenceStyle(GetStyle(0x20))) return true;
  return false;
}

// 80147050
// sRotateSegmentLookup[quarters][segments] (GenerateRotationLookups 80145EF0 fills it): each edge
// segment steps round (1 -> 2 -> 4 -> 8 -> 1) a quarter turn at a time; the diagonals swap on odd turns.
int TileWalls::RotateSegment(TileWallsSegment segments, int quarters) {
  quarters &= 3;
  unsigned int r = 0;
  for (unsigned int bit = 1; bit < 0x40; bit <<= 1) {
    if (!(static_cast<unsigned int>(segments.value) & bit)) continue;
    if (bit == 0x10 || bit == 0x20) {
      r |= (quarters & 1) ? (bit ^ 0x30) : bit;
    } else {
      unsigned int b = bit;
      for (int i = 0; i < quarters; ++i) b = b << 1 > 8 ? 1 : b << 1;
      r |= b;
    }
  }
  return static_cast<int>(r);
}
