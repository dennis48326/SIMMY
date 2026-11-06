// Port of game/eroom.cpp (partial): the house's walls. ERoom::Init walks the lot's wall entries
// (cFixedWorldImpl::GetWall) row by row and column by column, joining each segment into a run
// (ERoomWall) until a crossing wall splits it; every tile of a run becomes an EIWallPart, a wall
// model chosen by the walls around its ends, turned to its edge and given the tile's wallpaper.
//
// Diagonal walls (ProcDiagonalWalls) run along the lot's diagonals, two faces per tile (one model per
// side, from g_diagWallTable).
//
// Still stubbed: the room lookup tables, rooms of the wall points (RoomManager), walls dressed by objects (doors and
// windows: ObjectIterator), the outside light at the second point, shadows and the wall fading.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstdint>

#include <cmath>
#include <utility>

int ERoomWall::m_tileCount = 0;  // 804FD994
int ERoomWall::m_wallCount = 0;  // 804FD998

namespace {
// 80423A40 kWallShaderOrder: the wall model's parts (the two wallpapered faces, the cut-down top,
// the two caps).
const unsigned int kWallShaderOrder[5] = {0xC26D593A, 0x2B7EF272, 0x0906522F, 0xC9191F0B, 0xA14B735D};

// 80423AAC g_wallTable: wall models by end-cap variant (GetWallType), ten per row; row 0 the plain
// walls, the others those of the wall-dressing objects (word 9 of a row: that object's kind).
const unsigned int g_wallTable[150] = {
    0xCAFD5303, 0xB1F46D33, 0x6F5F6D70, 0xD3C50619, 0xA187AB4B, 0x03343AF9, 0x3143E96E, 0x89E1197C, 0xC2341803, 0x00000000,
    0xF344CC59, 0x2C4AC8FD, 0x131E3C62, 0x4E7BA3D7, 0xEEDF1D28, 0x84700B5A, 0x7DD198FF, 0xAE5E81BC, 0x7FED7272, 0x00000000,
    0x56E77B5C, 0x16FDD659, 0xC29FEC8D, 0x74CCBD73, 0x15A2679E, 0x2D46D805, 0xF7AC9413, 0x7E87AEAA, 0xC6D49397, 0x00000000,
    0x3AEBE781, 0x50251DE5, 0x000ECBE1, 0x321476CF, 0x8855E193, 0x08B77A80, 0x69DCB1A1, 0x2F45AEB3, 0xDAD4B0A6, 0x00000000,
    0xAE3C21F5, 0x7BD5A3AC, 0x0F25C26B, 0x19E4C886, 0xAB759399, 0xF1A2CF77, 0xC5B6293D, 0x44BF239B, 0x4B086CF6, 0x00000000,
    0xD1E6444B, 0x7C631216, 0x24962E48, 0x1E52793C, 0xA04326EE, 0x1CC899B8, 0x1763C6BD, 0x35940EB4, 0xD1A0CAB2, 0x00000003,
    0x85AA4505, 0xCC006E78, 0x8E2C016B, 0xAE310552, 0x9EE31FB8, 0xD286145F, 0x380ABECA, 0x9C2B037D, 0xD203505E, 0x00000003,
    0x9A7338CB, 0x33535A35, 0x86BE6F72, 0x5162311F, 0x71BD8438, 0x590DBE1C, 0x0D181790, 0xBEDF9DE8, 0x248B7CE8, 0x00000001,
    0xA0BF1A52, 0x80C33A31, 0x81603B0B, 0xE2F2511B, 0xC11E255A, 0x2A34FE66, 0xE84EEE17, 0x428289A0, 0xC01748B3, 0x00000002,
    0x3209C71F, 0xCA1113F2, 0x83477A28, 0xA82078D8, 0x27DCB711, 0x8BFAB429, 0xF797DFB6, 0xB596BDCE, 0xB6747B27, 0x00000003,
    0x6B7A9932, 0xC9C8281B, 0x5A92F9CF, 0xABF94331, 0xB37D9F82, 0x4EE9A3A0, 0x5EBBE80E, 0x1E0D3ABC, 0xFCCE5008, 0x00000003,
    0x418DDF63, 0x3A0D3509, 0xA9D84D9A, 0x583C5E23, 0xD64073F8, 0xC3DC1FF8, 0x630A4511, 0x0EAB97C9, 0x80197F92, 0x00000003,
    0x3AB9195D, 0xEE11EF97, 0xBEBBEF03, 0x8C2084BD, 0x844F62E7, 0xA42F364B, 0x4D693A8A, 0xD91C0D4A, 0x3A511273, 0x00000002,
    0x3348FB5A, 0x8394E7DE, 0xB1D6933F, 0xE1A58CF4, 0xD40A81F4, 0xE4BD193B, 0x004D2C60, 0x7EE6D31C, 0xA7A92597, 0x00000003,
    0xA3C4CB53, 0xAC78F483, 0xA4235265, 0xCE499FA9, 0xED728C45, 0x9F036AD8, 0x0F1561AA, 0x4E2D31BE, 0xD9B46E78, 0x00000003,
};
// 80423D04 g_diagWallTable: the diagonal wall models by end-cap variant.
const unsigned int g_diagWallTable[9] = {0x589C54E6, 0x22DED9CA, 0xABAD0D7C, 0x40EFB2E0, 0x703BC787,
                                         0x043F075D, 0x57BA546A, 0x3163E872, 0xFD601934};

// 80423A08 _wallEndCapFnTab: by the segment's bit number (+2 for diagonal sides 3 and 4).
using EndCapFn = int (*)(CTilePt&, TileWalls&, TileWallsSegment, DiagonalSideSelector);
const EndCapFn _wallEndCapFnTab[8] = {_kTopLeftWallsEndCapTest,         _kTopRightWallsEndCapTest,
                                      _kBottomRightWallsEndCapTest,      _kBottomLeftWallsEndCapTest,
                                      _kHorizDiagWallskTopEndCapTest,    _kVertDiagWallskLeftEndCapTest,
                                      _kHorizDiagWallskBottomEndCapTest, _kVertDiagWallskRightEndCapTest};

// 80423A28 _WallSplitTestFnTab: by the segment's bit number.
using SplitFn = bool (*)(TileWalls&, CTilePt&);
const SplitFn _WallSplitTestFnTab[6] = {SplitWallTopLeft,    SplitWallTopRight, SplitWallBottomRight,
                                        SplitWallBottomLeft, SplitWallHorizDiag, SplitWallVertDiag};

TileWalls WallAt(CTilePt tile) { return Globs_pFixedWorld->GetWall(tile); }
CTilePt Offset(CTilePt& tile, int dir) { return tile + CTilePt::sDirections[dir]; }
}  // namespace

// 803068C4
int log2down(int value) {
  int bits = -1;
  do {
    ++bits;
    value >>= 1;
  } while (value != 0);
  return bits;
}

// ---------------------------------------------------------------------------------------------------
// End caps: which ends of the segment meet no wall (bits 1/2: a wall goes on in line beyond the end,
// 4/8: none), mapped to a model variant by EIWallPart::GetWallType.

// 80027E30
int _kTopLeftWallsEndCapTest(CTilePt& tile, TileWalls& walls, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 2)), b = WallAt(Offset(tile, 4)), c = WallAt(Offset(tile, 7));
  TileWalls d = WallAt(Offset(tile, 0));
  unsigned int ends = 0;
  if (!walls.HasWallNotFence(2) && !d.HasWallNotFence(1) && !b.HasWallNotFence(0x20) && !d.HasWallNotFence(0x10))
    ends = a.HasWallNotFence(2) ? 1 : 4;
  TileWalls next = WallAt(Offset(tile, 1));
  d = next;
  if (!walls.HasWallNotFence(8) && !d.HasWallNotFence(1) && !d.HasWallNotFence(0x20) && !c.HasWallNotFence(0x10))
    ends |= a.HasWallNotFence(8) ? 2 : 8;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// 80027810
int _kTopRightWallsEndCapTest(CTilePt& tile, TileWalls& walls, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 0)), b = WallAt(Offset(tile, 3)), c = WallAt(Offset(tile, 4));
  TileWalls d = WallAt(Offset(tile, 6));
  unsigned int ends = 0;
  if (!walls.HasWallNotFence(4) && !b.HasWallNotFence(0x20) && !d.HasWallNotFence(0x10) && !b.HasWallNotFence(2))
    ends = a.HasWallNotFence(4) ? 1 : 4;
  TileWalls next = WallAt(Offset(tile, 2));
  b = next;
  if (!walls.HasWallNotFence(1) && !c.HasWallNotFence(0x20) && !b.HasWallNotFence(0x10) && !b.HasWallNotFence(2))
    ends |= a.HasWallNotFence(1) ? 2 : 8;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// 80027B20
int _kBottomRightWallsEndCapTest(CTilePt& tile, TileWalls& walls, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 3)), b = WallAt(Offset(tile, 0)), c = WallAt(Offset(tile, 5));
  TileWalls d = WallAt(Offset(tile, 6));
  unsigned int ends = 0;
  if (!walls.HasWallNotFence(2) && !b.HasWallNotFence(0x20) && !d.HasWallNotFence(0x10) && !b.HasWallNotFence(4))
    ends = a.HasWallNotFence(2) ? 2 : 8;
  TileWalls next = WallAt(Offset(tile, 1));
  b = next;
  if (!walls.HasWallNotFence(8) && !c.HasWallNotFence(0x20) && !b.HasWallNotFence(0x10) && !b.HasWallNotFence(4))
    ends |= a.HasWallNotFence(8) ? 1 : 4;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// 80027518
int _kBottomLeftWallsEndCapTest(CTilePt& tile, TileWalls& walls, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 1)), b = WallAt(Offset(tile, 5)), c = WallAt(Offset(tile, 7));
  TileWalls d = WallAt(Offset(tile, 3)), e = WallAt(Offset(tile, 2));
  unsigned int ends = 0;
  if (!walls.HasWallNotFence(4) && !b.HasWallNotFence(0x20) && !d.HasWallNotFence(0x10) && !d.HasWallNotFence(8))
    ends = a.HasWallNotFence(4) ? 2 : 8;
  if (!walls.HasWallNotFence(1) && !e.HasWallNotFence(0x20) && !c.HasWallNotFence(0x10) && !e.HasWallNotFence(8))
    ends |= a.HasWallNotFence(1) ? 1 : 4;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// The diagonals' end caps: each end of the diagonal is open (4/8: no wall goes on, 1/2: one does) when
// no diagonal or edge wall meets it there. GC: the tests read the neighbours by sDirections index.

// 80028140
// Segment 0x10, side 2 (the run's first face).
int _kHorizDiagWallskTopEndCapTest(CTilePt& tile, TileWalls&, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 3)), b = WallAt(Offset(tile, 1)), c = WallAt(Offset(tile, 6));
  TileWalls d = WallAt(Offset(tile, 7)), e = WallAt(Offset(tile, 0));
  unsigned int ends = 0;
  if (!c.HasWallNotFence(0x10) && !c.HasWallNotFence(1) && !e.HasWallNotFence(0x20))
    ends = !a.HasWallNotFence(0x20) && !c.HasWallNotFence(8) ? 8 : 2;
  TileWalls next = WallAt(Offset(tile, 2));
  e = next;
  if (!d.HasWallNotFence(0x10) && !d.HasWallNotFence(2) && !e.HasWallNotFence(0x20))
    ends |= !b.HasWallNotFence(0x20) && !d.HasWallNotFence(4) ? 4 : 1;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// 800284A0
// Segment 0x10, side 4.
int _kHorizDiagWallskBottomEndCapTest(CTilePt& tile, TileWalls&, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 2)), b = WallAt(Offset(tile, 0)), c = WallAt(Offset(tile, 6));
  TileWalls d = WallAt(Offset(tile, 7)), e = WallAt(Offset(tile, 1));
  unsigned int ends = 0;
  if (!d.HasWallNotFence(0x10) && !d.HasWallNotFence(4) && !e.HasWallNotFence(0x20))
    ends = !a.HasWallNotFence(0x20) && !d.HasWallNotFence(2) ? 8 : 2;
  TileWalls next = WallAt(Offset(tile, 3));
  e = next;
  if (!c.HasWallNotFence(0x10) && !c.HasWallNotFence(8) && !e.HasWallNotFence(0x20))
    ends |= !b.HasWallNotFence(0x20) && !c.HasWallNotFence(1) ? 4 : 1;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// 80028800
// Segment 0x20, side 3.
int _kVertDiagWallskRightEndCapTest(CTilePt& tile, TileWalls&, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 3)), b = WallAt(Offset(tile, 0)), c = WallAt(Offset(tile, 5));
  TileWalls d = WallAt(Offset(tile, 4)), e = WallAt(Offset(tile, 1));
  unsigned int ends = 0;
  if (!e.HasWallNotFence(0x10) && !c.HasWallNotFence(1) && !c.HasWallNotFence(0x20))
    ends = !a.HasWallNotFence(0x10) && !c.HasWallNotFence(2) ? 4 : 1;
  TileWalls next = WallAt(Offset(tile, 2));
  e = next;
  if (!e.HasWallNotFence(0x10) && !d.HasWallNotFence(8) && !d.HasWallNotFence(0x20))
    ends |= !b.HasWallNotFence(0x10) && !d.HasWallNotFence(4) ? 8 : 2;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// 80028B60
// Segment 0x20, side 1.
int _kVertDiagWallskLeftEndCapTest(CTilePt& tile, TileWalls&, TileWallsSegment, DiagonalSideSelector) {
  TileWalls a = WallAt(Offset(tile, 2)), b = WallAt(Offset(tile, 1)), c = WallAt(Offset(tile, 5));
  TileWalls d = WallAt(Offset(tile, 4)), e = WallAt(Offset(tile, 0));
  unsigned int ends = 0;
  if (!e.HasWallNotFence(0x10) && !d.HasWallNotFence(4) && !d.HasWallNotFence(0x20))
    ends = !a.HasWallNotFence(0x10) && !d.HasWallNotFence(8) ? 4 : 1;
  TileWalls next = WallAt(Offset(tile, 3));
  e = next;
  if (!e.HasWallNotFence(0x10) && !c.HasWallNotFence(2) && !c.HasWallNotFence(0x20))
    ends |= !b.HasWallNotFence(0x10) && !c.HasWallNotFence(1) ? 8 : 2;
  return EIWallPart::GetWallType(static_cast<unsigned char>(ends));
}

// Splits: a run of segments ends at a tile where a crossing wall meets it.

// 80028EC0
bool SplitWallBottomRight(TileWalls& walls, CTilePt&) {
  walls.HasWallNotFence(4);
  return walls.HasWallNotFence(2);
}
// 80028F0C
bool SplitWallTopLeft(TileWalls& walls, CTilePt&) {
  walls.HasWallNotFence(1);
  return walls.HasWallNotFence(2);
}
// 80028F58
bool SplitWallBottomLeft(TileWalls& walls, CTilePt&) {
  walls.HasWallNotFence(8);
  return walls.HasWallNotFence(1);
}
// 80028FA4
bool SplitWallTopRight(TileWalls& walls, CTilePt&) {
  walls.HasWallNotFence(2);
  return walls.HasWallNotFence(1);
}

namespace {
// cFixedWorldImpl::GetWallStorage(tile)'s segment byte has `bits`, for a tile on the lot.
bool StorageHasSegment(CTilePt& tile, unsigned int bits) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  const int xSize = world->GetXSize(), ySize = world->GetYSize();
  if (tile.m_x < 0 || tile.m_x > xSize - 1 || tile.m_y < 0 || tile.m_y > ySize - 1) return false;
  if (!world->m_walls) return false;
  return (world->m_walls->At(tile.m_x, tile.m_y)[0] & bits) != 0;
}
}  // namespace

// 80028FF0
// A 0x10 run (walked with x falling, y rising) splits where a 0x20 diagonal crosses at the corner it
// came from. GC quirk: the neighbours are taken mirrored in x, (xSize - x, y) and (xSize - x - 1, y - 1).
bool SplitWallHorizDiag(TileWalls&, CTilePt& tile) {
  const int xSize = Globs_pFixedWorld->GetXSize();
  CTilePt a(xSize - tile.m_x, tile.m_y, 1);
  CTilePt b(xSize - tile.m_x - 1, tile.m_y - 1, 1);
  return StorageHasSegment(a, 0x20) || StorageHasSegment(b, 0x20);
}

// 800291AC
// A 0x20 run (x and y rising) splits where a 0x10 diagonal is at (x - 1, y) or (x, y - 1).
bool SplitWallVertDiag(TileWalls&, CTilePt& tile) {
  CTilePt a(tile.m_x - 1, tile.m_y, 1);
  CTilePt b(tile.m_x, tile.m_y - 1, 1);
  return StorageHasSegment(a, 0x10) || StorageHasSegment(b, 0x10);
}

// 800371FC
// A wall pattern's wallpaper shader ("WallSet" row +0x08).
unsigned int RemapWallpaperId(unsigned int pattern) {
  const void* rows = ERQuickdata::Ptr(_globals.m_wallSet, 0);
  return ERQuickdata::U32(ERQuickdata::Ptr(rows, pattern * 4), 8);
}

// 80029C24
// A wall part's rooms for its lights: its room, and the room's lighting group.
void WallLocation(unsigned int room, bool, short& lightRoom, short& otherRoom) {
  otherRoom = static_cast<short>(room);
  lightRoom = static_cast<short>(room);
  if (Globs_pRoomManager && (room & 0xffff) < 0xfffb) {
    if (Room* r = Globs_pRoomManager->GetRoom(static_cast<unsigned short>(room)))
      otherRoom = static_cast<short>(r->m_lightingGroup);
  }
}

namespace {
// ResolveRoomID(FTilePt&, int) (800DF228) at a tile's centre (CTilePt::ToFTilePt: fraction 8 of 16):
// the tile's room, 0xFFFB off the lot; a tile split by a diagonal takes the room of the neighbour on
// the centre's side ((x+1, y+1 and the y-1 / y+1 neighbour).
unsigned short ResolveRoomIDAtCentre(CTilePt& tile) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (world->OutOfBounds(tile)) return 0xfffb;
  unsigned short room = world->GetRoom(tile);
  if (room != 0xfffb) return room;
  TileWalls walls = world->GetWall(tile);
  CTilePt a(tile), b(tile);
  if (walls.HasWall(0x10)) {
    a.m_x = static_cast<signed char>(a.m_x + 1);  // the centre is not below the diagonal (8 < 16 - 8)
    b.m_y = static_cast<signed char>(b.m_y + 1);
  } else if (walls.HasWall(0x20)) {
    a.m_x = static_cast<signed char>(a.m_x + 1);  // the centre is not above it (8 < 8)
    b.m_y = static_cast<signed char>(b.m_y - 1);
  }
  if (!world->OutOfBounds(a) && (room = world->GetRoom(a)) == 0xfffb && !world->OutOfBounds(b))
    room = world->GetRoom(b);
  return room;
}
}  // namespace

// 8002B9A4
// The tile; a diagonal wall's point is the neighbour on its side (side 1: x+1, 2: x-1, 3: x-1 y+1,
// 4: x+1 y+1).
void EIWallPart::GetPoint(CTilePt& point) {
  point = m_tile;
  if (!(m_segment & 0x30)) return;
  const int x = m_tile.GetX(), y = m_tile.GetY();
  switch (m_side) {
    case 1: point = CTilePt(x + 1, y, 1); break;
    case 2: point = CTilePt(x - 1, y, 1); break;
    case 3: point = CTilePt(x - 1, y + 1, 1); break;
    case 4: point = CTilePt(x + 1, y + 1, 1); break;
    default: break;
  }
}

// 8002BADC
// The tiles either side of the wall: GetPoint's, and across the segment (1: x-1, 2: y-1, 4: x+1,
// 8: y+1; diagonals by side: 1: x-1 y+1, 2: x+1 y+1, 3: x+1 y-1, 4: x-1 y-1).
// PC: the decompilation leaves y unset for segment 2 and diagonal sides 3 and 4; y-1 by symmetry.
void EIWallPart::GetPoints(CTilePt& first, CTilePt& second) {
  GetPoint(first);
  const int x = m_tile.GetX(), y = m_tile.GetY();
  if (!(m_segment & 0x30)) {
    switch (m_segment) {
      case 1: second = CTilePt(x - 1, y, 1); break;
      case 2: second = CTilePt(x, y - 1, 1); break;
      case 4: second = CTilePt(x + 1, y, 1); break;
      case 8: second = CTilePt(x, y + 1, 1); break;
      default: break;
    }
    return;
  }
  switch (m_side) {
    case 1: second = CTilePt(x - 1, y + 1, 1); break;
    case 2: second = CTilePt(x + 1, y + 1, 1); break;
    case 3: second = CTilePt(x + 1, y - 1, 1); break;
    case 4: second = CTilePt(x - 1, y - 1, 1); break;
    default: break;
  }
}

// 80030964
void DrawSubModelShader(ERC* rc, ESubModelShader* s) {
  rc->SetMipMap(s->m_mipMap, 0);
  rc->DisplayList(s->m_dl);
}

// 8002E84C
void SetAllUpOrDownForList(bool up, std::vector<ERoomWall*>& list) {
  for (ERoomWall* wall : list) wall->SetWallUpDownMode(up ? 0 : 2, true);
}

// ---------------------------------------------------------------------------------------------------
// ERoom

// 8002D778
ERoom::ERoom() {}

// 8002D88C
ERoom::~ERoom() {
  for (auto& list : m_walls) {
    for (ERoomWall* wall : list) wall->SafeDelete();
    list.clear();
  }
  for (ERoomWall* fence : m_fences) fence->SafeDelete();
  m_fences.clear();
}

// 8002F4DC
// Whether the walls as the world now has them may be built: the rooms recomputed, fewer than 0x28 room
// ids and 20 inside rooms; then walls: fewer than 500 runs and 1000 tiles of them (the standard and
// diagonal walls counted without being made); fences: fewer than 100 fence segments on the lot.
bool ERoom::PreviewWallBuild(bool fences) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  world->ComputeRooms(0);
  RoomManager* manager = Globs_pRoomManager;
  unsigned short highest = 0;
  for (auto& entry : manager->m_rooms) {
    Room* room = entry.second;
    if (room && room->m_inUse && !room->IsOutside() && highest < entry.first) highest = static_cast<unsigned short>(entry.first);
  }
  if (highest >= 0x28) return false;
  int inside = 0;
  for (auto& entry : manager->m_rooms)
    if (entry.second && entry.second->m_inUse && !entry.second->IsOutside()) ++inside;
  if (inside >= 0x14) return false;
  if (!fences) {
    int walls = 0, tiles = 0;
    ProcStandardWalls(true, walls, tiles, false);
    ProcStandardWalls(false, walls, tiles, false);
    ProcDiagonalWalls(walls, tiles, false);
    return walls < 500 && tiles < 1000;
  }
  int count = 0;
  const int xSize = world->GetXSize(), ySize = world->GetYSize();
  for (int x = 1; x < xSize - 1; ++x)
    for (int y = 1; y < ySize - 1; ++y) {
      CTilePt tile(x, y, 1);
      TileWalls walls = world->GetWall(tile);
      for (int seg = walls.First(); seg; seg = walls.Next(seg)) {
        if (static_cast<unsigned int>(seg - 1) <= 1) continue;  // segments 1 and 2 are the neighbours'
        const int style = walls.GetStyle(seg);
        if (style == 2 || style == 0xc || style == 0xd || style == 0xe) ++count;
      }
    }
  return count < 100;
}

// 8002FF8C
// The wall off the tile (unless it is a door or window's: styles 3, 5, 6, 0xF, 0x17, 0x16 kill that
// object instead): a diagonal on a tile without a floor gives the tile its half's floor, then the world
// gets the walls without it. Returns the killed object's refund (0).
int ERoom::DeleteWallAtTile(CTilePt& tile, TileWalls& walls, TileWallsSegment segment) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  int refund = 0;
  const int style = walls.GetStyle(segment);
  const bool portal = style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17 || style == 0x16;
  if (portal) {
    SIMMY_STUB("ERoom::DeleteWallAtTile: the door or window in the wall (KillArchitecturalObject)", 0x8002FF8C);
  }
  if (segment == 0x10 || segment == 0x20) {
    if (world->GetFloor(tile) == 0xff) world->SetFloor(tile, walls.GetFloorValue(segment == 0x10 ? 4 : 3));
  }
  walls.RemoveWall(segment);
  world->SetWall(tile, walls);
  return refund;
}

// 8002FE18
// The wall part of segment `segment` at `tile`: in the first of the segment lists whose runs are of it.
EIWallPart* ERoom::GetWallFromTileAndSegment(TileWallsSegment segment, CTilePt& tile) {
  for (int i = 0; i < 4; ++i) {
    std::vector<ERoomWall*>& list = m_walls[i];
    if (list.empty() || list.front()->m_segment != segment) continue;
    for (ERoomWall* wall : list)
      if (EIWallPart* part = wall->GetWallAtTile(tile)) return part;
    return nullptr;
  }
  return nullptr;
}

// 8002FD7C
// The run's part whose point (GetPoint) is the tile.
EIWallPart* ERoomWall::GetWallAtTile(CTilePt& tile) {
  for (EIWallPart* part : m_parts) {
    CTilePt point;
    part->GetPoint(point);
    if (tile.m_x == point.m_x && tile.m_y == point.m_y) return part;
  }
  return nullptr;
}

// 800293A0
// How full the house's wall budget is (1: full): the largest of the inside rooms over 20 (`rooms` their
// count), the wall runs over 500, their tiles over 1000 and the highest inside room id over 39.
double ERoomWall::GetWallMeterValue(int& rooms) {
  RoomManager* manager = Globs_pRoomManager;
  unsigned int n = 0, highest = 0;
  if (manager)
    for (auto& e : manager->m_rooms)
      if (e.second && e.second->m_inUse && !e.second->IsOutside()) {
        ++n;
        if (highest < e.first) highest = e.first;
      }
  rooms = static_cast<int>(n);
  double value = static_cast<float>(n) / 20.0f;
  double tiles = static_cast<float>(static_cast<unsigned int>(m_tileCount)) / 1000.0f;
  const double walls = static_cast<float>(static_cast<unsigned int>(m_wallCount)) / 500.0f;
  double ids = static_cast<float>(highest) / 39.0f;
  if (tiles < walls) tiles = walls;
  if (ids < tiles) ids = tiles;
  if (value < ids) value = ids;
  return value;
}

// 8002EBB8
// The walls: along y, along x, then the diagonals.
void ERoom::Init() {
  int walls = 0, tiles = 0;
  ProcStandardWalls(true, walls, tiles, true);
  ProcStandardWalls(false, walls, tiles, true);
  ProcDiagonalWalls(walls, tiles, true);
  // GC: InitRoomLookupTab (8002DCF0): each room's runs, for the wallpaper preview and cost and the
  // camera's opaque-room test (none of which is ported).
  SIMMY_STUB("ERoom::Init: InitRoomLookupTab (the rooms' wall lists)", 0x8002EBB8);
  ERoomWall::m_wallCount = walls;
  ERoomWall::m_tileCount = tiles;
  SetWallState(_globals.m_house->m_wallMode);
}

// 8002E8A4
void ERoom::SetWallState(EWallUpDownStateType state) {
  const bool up = static_cast<unsigned int>(state.value) < 2;
  for (auto& list : m_walls) SetAllUpOrDownForList(up, list);
}

// 8047547C ERoom::s_wallFadeParmsDC, 804754AC ERoom::s_wallFadeParmsClassic (8003038C: the static
// initialisation): depths -0.75 .. 0.15 with an interactor, -1.71 .. 1.5 without.
WallFadeParms ERoom::s_wallFadeParmsDC(const_cast<char*>("DC Wall Fade Tunning"), -0.75f, 0.15f, false);
WallFadeParms ERoom::s_wallFadeParmsClassic(const_cast<char*>("Classic Wall Fade Tunning"), -1.71f, 1.5f, false);
WallFadeParms* ERoom::s_wallFadeParms = nullptr;

// 800302CC
WallFadeParms::WallFadeParms(char*, float min, float max, bool threeD) {
  m_3d = threeD ? 1 : 0;
  m_max = max;
  m_min = min;
  m_range = max - min;
  m_invRange = 1.0f / (max - min);
}

// 80030340
// GC: the far width is checked against the minimum depth (a typo'd tuning guard); the range is
// recomputed.
void WallFadeParms::Validate() {
  if (m_farWidth < m_min) m_nearWidth = m_min;
  if (m_max < m_min) m_max = m_min;
  m_range = m_max - m_min;
  m_invRange = 1.0f / m_range;
}

// 8002EAB8
// Once the house is shown: in cut-away mode (house mode 1) with a camera, every wall run is faded
// against the interest fade; then each room's opacity for the camera. GC: CalAllRoomOpaque,
// CalRoofOpaque, CalShortDistToCam (the camera's IsRoomWallsOpaque and the roof fade; not ported).
void ERoom::UpdateWallFade(int context) {
  if (!_gd.m_fieldC0 || !_globals.m_house) return;  // DAT_80475C44, DAT_80475E94
  if (_globals.m_house->m_wallMode == 1) {
    if (!_globals.m_cam) return;
    if (s_wallFadeParms && s_wallFadeParms->m_enabled)
      for (auto& list : m_walls) EIWallPart::DoFadeCollisonForList(_interestFade, list);
  }
  (void)context;
  SIMMY_STUB("ERoom::UpdateWallFade: CalAllRoomOpaque, CalRoofOpaque, CalShortDistToCam", 0x8002EAB8);
}

// 8002EA64
void EIWallPart::DoFadeCollisonForList(TrapezoidFade& fade, std::vector<ERoomWall*>& walls) {
  EVec3 pos{0.0f, 0.0f, 0.0f};  // GC: the run's +0x08 (unused)
  for (ERoomWall* wall : walls) DoFadeCollisonWallList(fade, pos, wall->m_parts);
}

// 8002E938
// Each part's alpha in this context (forced parts stay at 1): full is up, some is fading, none is down.
// GC: always against _interestFade, whatever fade it is given.
void EIWallPart::DoFadeCollisonWallList(TrapezoidFade&, EVec3&, std::vector<EIWallPart*>& parts) {
  for (EIWallPart* part : parts) {
    if (!part) continue;
    const double alpha = part->m_forceFadeOff ? 1.0 : _interestFade.CalcAlpha(*reinterpret_cast<EBound3*>(part->m_bounds));
    part->m_partAlpha[GetContext()] = static_cast<float>(alpha);
    if (alpha == 1.0) part->SetWallState(EWallUpDownStateType{0}, false);
    else part->SetWallState(EWallUpDownStateType{alpha > 0.0 ? 1 : 2}, false);
  }
}

// 80030654
void EIWallPart::SetForceFadeOff(bool off) { m_forceFadeOff = off ? 1 : 0; }

// 8002EC60
// Segments 1 and 4 down each column (`alongY`), or 2 and 8 along each row: runs of one segment.
void ERoom::ProcStandardWalls(bool alongY, int& walls, int& tiles, bool create) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  struct Pass {
    std::vector<ERoomWall*>* list;
    int segment;
  };
  const Pass passes[2][2] = {{{&m_walls[3], 1}, {&m_walls[1], 4}}, {{&m_walls[2], 2}, {&m_walls[0], 8}}};
  ERoomWall* current = nullptr;
  for (int p = 0; p < 2; ++p) {
    int outer = world->GetXSize(), inner = world->GetYSize();
    if (!alongY) std::swap(outer, inner);
    for (int o = 0; o < outer; ++o) {
      const Pass& pass = passes[alongY ? 0 : 1][p];
      const int split = log2down(pass.segment);
      for (int i = 0; i < inner; ++i) {
        CTilePt tile(0, 0, 1);
        if (alongY) tile.Set(o, i, 1);
        else tile.Set(i, o, 1);
        TileWalls cell = world->GetWall(tile);
        ProcessCell(*pass.list, current, tile, pass.segment, 0, cell, split, walls, tiles, create);
      }
      if (!create && current) current->SafeDelete();
      current = nullptr;
    }
  }
}

// 8002EF04
// The diagonal walls, along every diagonal line of the lot: first the 0x10 diagonals (lines running
// with x falling and y rising, from the lot's far x edge), each tile's side 2 into the +0x34 runs and
// side 4 into +0x40; then the 0x20 diagonals (x and y rising), side 3 into +0x4C and side 1 into +0x58.
// The lines start along the x edge, then along the y edge (its first tile, the corner, done already).
void ERoom::ProcDiagonalWalls(int& walls, int& tiles, bool create) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  const int xSize = world->GetXSize(), ySize = world->GetYSize();
  const int longest = ySize < xSize ? xSize : ySize;
  ERoomWall* current[2] = {nullptr, nullptr};
  for (int pass = 0; pass < 2; ++pass) {
    int first = 0;
    for (int edge = 0; edge < 2; ++edge) {
      for (int n = first++; n < longest; ++n) {
        int x = edge == 0 ? n : 0, y = edge == 0 ? 0 : n;
        if (x >= xSize || y >= ySize) break;
        int mirrored = xSize - 1 - x;
        do {
          if (pass == 0) {
            CTilePt tile(mirrored, y, 1);
            TileWalls cell = world->GetWall(tile);
            ProcessCell(m_walls[4], current[0], tile, 0x10, 2, cell, 4, walls, tiles, create);
            ProcessCell(m_walls[5], current[1], tile, 0x10, 4, cell, 4, walls, tiles, create);
          } else {
            CTilePt tile(x, y, 1);
            TileWalls cell = world->GetWall(tile);
            ProcessCell(m_walls[6], current[0], tile, 0x20, 3, cell, 5, walls, tiles, create);
            ProcessCell(m_walls[7], current[1], tile, 0x20, 1, cell, 5, walls, tiles, create);
          }
          ++x;
          --mirrored;
          ++y;
        } while (x < xSize && y < ySize);
        if (!create) {
          if (current[0]) current[0]->SafeDelete();
          if (current[1]) current[1]->SafeDelete();
        }
        current[0] = current[1] = nullptr;
      }
    }
  }
}

// 8002F264
// Extends the current run by the tile's segment, starts a new run (none yet, or the split test says
// the tile ends the last), or ends the run (no wall, or a fence).
void ERoom::ProcessCell(std::vector<ERoomWall*>& list, ERoomWall*& current, CTilePt& tile, TileWallsSegment segment,
                        DiagonalSideSelector side, TileWalls& walls, int splitTest, int& wallCount, int& tileCount,
                        bool create) {
  const bool hasWall = walls.HasWall(segment);
  bool fence = false;
  if (hasWall) {
    const int style = walls.GetStyle(segment);
    fence = style == 2 || style == 0xc || style == 0xd || style == 0xe;
  }
  if (!hasWall && !fence) {
    if (!current) return;
    if (!create) current->SafeDelete();
    current = nullptr;
    return;
  }
  if (fence) {
    if (!create) {
      if (current) current->SafeDelete();
      current = nullptr;
    } else if (static_cast<unsigned int>(segment - 1) > 1) {
      TileWallsSegment seg = segment;
      m_fences.push_back(new EFenceWall(seg, tile, WallStyle(walls.GetStyle(segment))));
    }
    current = nullptr;
    return;
  }
  if (current) {
    if (!_WallSplitTestFnTab[splitTest](walls, tile)) {
      if (create) current->AddTile(tile, walls, segment, side, create);
      ++tileCount;
      return;
    }
    if (!create) {
      current->SafeDelete();
      current = nullptr;
      current = new ERoomWall(segment, side, tile, false);
      ++wallCount;
      ++tileCount;
      return;
    }
  } else if (!create) {
    current = new ERoomWall(segment, side, tile, false);
    ++wallCount;
    ++tileCount;
    return;
  }
  current = new ERoomWall(segment, side, tile, create);
  list.push_back(current);
  ++wallCount;
  ++tileCount;
}

// ---------------------------------------------------------------------------------------------------
// ERoomWall

// 800308A0
ERoomWall::ERoomWall() {}

// 8002C5CC
// A run starting at `tile`, in the tile's room; with `create`, its first part.
ERoomWall::ERoomWall(TileWallsSegment segment, DiagonalSideSelector side, CTilePt& tile, bool create) {
  TileWalls walls = Globs_pFixedWorld->GetWall(tile);
  m_start = tile;
  m_end = tile;
  m_segment = segment;
  m_side = side;
  const unsigned short room = Globs_pFixedWorld->GetRoom(m_start);
  if (room != 0xfffb) {
    m_room = room;
  } else if (Globs_pRoomManager) {
    // A tile split by a diagonal: the room of the half on the run's side (sides 1 and 3 match the
    // halves' room sides 1 and 3, sides 2 and 4 the sides 4 and 2), else the other half's.
    Room* first = nullptr;
    Room* second = nullptr;
    Room__Sides firstSide{0}, secondSide{0};
    Globs_pRoomManager->ResolveDiagonal(m_start, &first, &second, &firstSide, &secondSide);
    const int s = static_cast<int>(firstSide);
    int match = 0;
    switch (static_cast<int>(side)) {
      case 1: match = 1; break;
      case 3: match = 3; break;
      case 2: match = 4; break;
      case 4: match = 2; break;
      default: break;
    }
    if (match) {
      Room* r = s == match ? first : second;
      if (r) m_room = r->m_id;  // PC: guard (the GC read through the pointer)
    }
  }
  if (create) m_parts.push_back(new EIWallPart(walls, segment, side, tile));
}

// 8002C7B8
ERoomWall::~ERoomWall() {
  for (EIWallPart* part : m_parts) part->SafeDelete();
  m_parts.clear();
}

// 80030914
void ERoomWall::SafeDelete() { delete this; }

// 8002CD84
void ERoomWall::SetWallUpDownMode(EWallUpDownStateType state, bool force) {
  for (EIWallPart* part : m_parts) part->SetWallState(state, force);
}

// 8002CDD8
void ERoomWall::AddTile(CTilePt& tile, TileWalls& walls, TileWallsSegment segment, DiagonalSideSelector side,
                        bool create) {
  m_end = tile;
  if (create) m_parts.push_back(new EIWallPart(walls, segment, side, tile));
}

// ---------------------------------------------------------------------------------------------------
// EIWallPart

// 800273FC
// The model variant for the ends that meet no wall (see the end-cap tests).
int EIWallPart::GetWallType(unsigned char ends) {
  switch (ends) {
    case 0: return 0;
    case 4: return 4;
    case 2: return 3;
    case 3: return 1;
    case 1: return 2;
    case 8: return 5;
    case 6: return 8;
    case 9: return 7;
    case 0xc: return 6;
    default: return 0;
  }
}

// 80029DF0
EIWallPart::EIWallPart() {
  m_wall = 1;
  m_thisSideData = static_cast<unsigned int>(static_cast<int>(m_room));
  m_thisSideFn = WallLocation;
}

// 80029FA4
// The wall model of the tile's segment, its wallpaper, placed on the tile and turned to the segment;
// its face plane and light points; into the house's level.
EIWallPart::EIWallPart(TileWalls& walls, TileWallsSegment segment, DiagonalSideSelector side, CTilePt& tile) {
  m_fadeMode = 0;
  m_wall = 1;
  m_segment = segment;
  m_side = side;
  m_tile = tile;
  float angle = 0.0f;
  GetWallModelId(tile, walls, segment, side, angle);
  if (!m_wallModel) return;
  SetModel(m_wallModel, false, nullptr, nullptr);
  OptimizeOrderTable();
  if (const unsigned int wallpaper = RemapWallpaperId(walls.GetPattern(segment, side))) ChangeWallpaper(wallpaper);
  if (!walls.HasDiagonal()) {
    m_normal[0] = 0.0f;
    m_normal[1] = 1.0f;
    m_planePoint[0] = 0.0f;
    m_planePoint[1] = -0.5f;
    m_pointA[0] = -0.5f;
    m_pointA[1] = -0.45f;
    m_pointB[0] = 0.5f;
    m_pointB[1] = -0.45f;
  } else {
    m_planePoint[0] = 0.0f;
    m_planePoint[1] = 0.0f;
    m_normal[0] = -0.70710677f;
    m_normal[1] = -0.70710677f;
    m_pointA[0] = -0.5f;
    m_pointA[1] = 0.45f;
    m_pointB[0] = 0.45f;
    m_pointB[1] = -0.5f;
  }
  EMat4 m;
  m.Id();
  EHouse* house = _globals.m_house;
  EVec3 pos;
  pos.x = static_cast<float>(tile.m_y) + house->m_posX;
  pos.y = static_cast<float>(tile.m_x) + house->m_posY;
  pos.z = 0.0f;
  m.Translate(pos);
  m.PreRotateZ(angle);
  SetOrient(m);
  CTilePt first, second;
  GetPoints(first, second);
  m_room = static_cast<short>(ResolveRoomIDAtCentre(first));
  m_room2 = static_cast<short>(ResolveRoomIDAtCentre(second));
  m_thisSideData = static_cast<unsigned int>(static_cast<int>(m_room));
  m_thisSideFn = WallLocation;
  GetShadowModel();
  house->m_level->InsertWall(this, m_shadowModel != nullptr);
}

// 8002BCB4
EIWallPart::~EIWallPart() {
  if (m_shadowModel) {
    m_shadowModel->DelRef();
    m_shadowModel = nullptr;
  }
  if (m_level) m_level->RemoveWall(this);
  if (m_wallpaper) {
    m_wallpaper->DelRef();
    m_wallpaper = nullptr;
  }
}

// 80029ED0
// The model's five wall parts, by kWallShaderOrder, among its first sub-model's shaders (stops at
// a part found twice).
void EIWallPart::OptimizeOrderTable() {
  for (int i = 0; i < 5; ++i) {
    m_slots[i] = nullptr;
    m_slotIndex[i] = 0;
  }
  if (!m_model || m_model->m_subModels.empty()) return;
  const std::vector<ESubModelShader*>& shaders = m_model->m_subModels[0]->m_shaders;
  for (int slot = 0; slot < 5; ++slot) {
    for (size_t i = 0; i < shaders.size(); ++i) {
      if (shaders[i]->m_shaderId != kWallShaderOrder[slot]) continue;
      if (m_slots[slot]) return;
      m_slots[slot] = shaders[i];
      m_slotIndex[slot] = static_cast<signed char>(i);
      break;
    }
  }
}

// 8002BD58
// The wall model: the end-cap variant from the walls around the segment's ends, from the plain or
// diagonal table; `angle` turns the model to the segment.
void EIWallPart::GetWallModelId(CTilePt& tile, TileWalls& walls, TileWallsSegment segment, DiagonalSideSelector side,
                                float& angle) {
  m_wallModel = 0;
  int index = log2down(segment);
  if (static_cast<unsigned int>(side - 3) < 2) index += 2;
  const int variant = _wallEndCapFnTab[index](tile, walls, segment, side);
  angle = static_cast<float>(index) * -3.1415927f * 0.5f;
  const int style = walls.GetStyle(segment);
  if (style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17 || style == 0x16) {
    // A wall dressed by a portal (door, window): the first portal among the objects at the tile picks
    // its row of g_wallTable (its definition's +0xA2; style 0x16, a portal also on the adjacent tile
    // across the segment, the rows four on), whose tenth entry goes to the portal's instance. Without
    // one the plain wall (style 0x16 only while DAT_80475C74 is clear).
    walls.HasDiagonal();
    ObjectIterator it;
    it.init(tile, ObjectIterator__IterateType{0});
    cXObject* portal = nullptr;
    int rowBase = 1;
    if (style != 0x16) {
      while (it.m_current && !IsPortalObject(it.m_current)) ++it;
      portal = it.m_current;
    } else {
      rowBase = 5;
      cXObject* last = nullptr;
      for (; it.m_current; ++it) {
        last = it.m_current;
        if (!IsPortalObject(last)) continue;
        portal = last;
        // GC: matched by master selector (ObjSelector::GetMasterSelector): here the same definition file.
        ObjSelector* master = portal->GetSelector();
        CTilePt across = tile;
        TileWalls::GetAdjacentTile(segment, &across);
        ObjectIterator other;
        bool same = false;
        for (other.init(across, ObjectIterator__IterateType{0}); other.m_current; ++other)
          if (IsPortalObject(other.m_current) && other.m_current->GetSelector() && master &&
              other.m_current->GetSelector()->m_resFile == master->m_resFile) {
            same = true;
            break;
          }
        if (same) break;
      }
      // GC: without a match the last object visited decides (a portal: its row; else the plain wall).
      portal = last && IsPortalObject(last) ? last : nullptr;
    }
    if (portal) {
      const int row = objdef::WallCutRow(portal->GetDef()) + rowBase;
      m_wallModel = g_wallTable[row * 10 + variant];
      // The row's tenth entry is the portal's type (EInstance::SetPortalType: 1 regular, 2 small, 3
      // large window; its portal lights' reach and height, EIPortalPointLight::SetPortal).
      cXObjectImpl* impl = portal->GetObjectImplementation();
      if (ISimInstance* instance = impl ? impl->m_instance : nullptr)
        instance->m_field14 = static_cast<int>(g_wallTable[row * 10 + 9]);
    } else {
      m_wallModel = g_wallTable[variant];
    }
  } else {
    m_wallModel = walls.HasDiagonal() ? g_diagWallTable[variant] : g_wallTable[variant];
  }
  _modelman.IsValid(m_wallModel);
}

// 8002A2D4
// The face plane and the light points in world space, then the model's orientation.
void EIWallPart::SetOrient(EMat4& m) {
  const auto& a = m.m;
  m_worldNormal[0] = m_normal[0] * a[0][0] + m_normal[1] * a[1][0];
  m_worldNormal[1] = m_normal[0] * a[0][1] + m_normal[1] * a[1][1];
  m_planeD = (m_planePoint[0] * a[0][0] + m_planePoint[1] * a[1][0] + a[3][0]) * m_worldNormal[0] +
             (m_planePoint[0] * a[0][1] + m_planePoint[1] * a[1][1] + a[3][1]) * m_worldNormal[1];
  m_worldA[0] = m_pointA[0] * a[0][0] + m_pointA[1] * a[1][0] + a[3][0];
  m_worldA[1] = m_pointA[0] * a[0][1] + m_pointA[1] * a[1][1] + a[3][1];
  m_worldA[2] = 0.0f;
  m_worldB[0] = m_pointB[0] * a[0][0] + m_pointB[1] * a[1][0] + a[3][0];
  m_worldB[1] = m_pointB[0] * a[0][1] + m_pointB[1] * a[1][1] + a[3][1];
  m_worldB[2] = 0.0f;
  EIStaticModel::SetOrient(m);
}

// 8002A408
// Hidden from behind the face; otherwise as its bounds.
unsigned int EIWallPart::VisibilityTest(E3DWindow& win) {
  if (win.m_lookPos[0] * m_worldNormal[0] + win.m_lookPos[1] * m_worldNormal[1] - m_planeD >= 0.0f) {
    const unsigned int result = win.Test(*reinterpret_cast<EBound3*>(m_bounds));
    SetVisible(result != 1);
    return result;
  }
  SetVisible(false);
  return 1;
}

// 8002A4C0
// Lit at its first point; while fading (all five parts) through the order table, otherwise its parts
// drawn now (the top and caps unlit) with the wallpaper shader.
void EIWallPart::Draw(ELevelDrawData& data) {
  if (!m_model) return;
  CalcLights(*reinterpret_cast<EVec3*>(m_worldA), m_lights, false);
  // GC: CalcLights4 here, and the level's lights at the second point (ERLevel::GetLightsAtLocation)
  // merged in, up to four point lights.
  const int context = GetContext();
  if (m_parts[context] == 0x1f) {
    EOrderTableData otd;
    otd.m_pos = m_sphere;
    otd.m_matrix = &m_drawMatrix;
    otd.m_lights = &m_lights;
    otd.m_callback = WallOrderTableCallback;
    otd.m_instance = this;
    InsertInOrderTable(otd);
    return;
  }
  ERC* rc = data.m_rc;
  ELights unlit;
  unlit.m_numPoint = 1;
  data.m_curLights = &m_lights;
  rc->Lights(&m_lights);
  data.m_curLights = nullptr;
  if (data.m_curMatrix != &m_drawMatrix) {
    data.m_curAnim = nullptr;
    data.m_curMatrix = &m_drawMatrix;
    rc->ModelMatrices(&m_drawMatrix, 1);
  }
  unsigned int parts = m_parts[context];
  const size_t digest = static_cast<size_t>(m_slotIndex[1]);
  ERShader* shader = digest < m_digests.size() ? m_digests[digest].m_shader : nullptr;
  if (data.m_curShader != shader) {
    data.m_curShader = shader;
    if (shader && shader->GetShader()) shader->GetShader()->Select(rc, 0);
  }
  for (int i = 0; i < 5; ++i, parts >>= 1) {
    ESubModelShader* s = m_slots[i];
    if (!s || !(parts & 1)) continue;
    if (i > 1 && data.m_curLights != &unlit) {
      data.m_curLights = &unlit;
      rc->Lights(&unlit);
    }
    DrawSubModelShader(rc, s);
  }
  data.m_curLights = nullptr;
}

// 8002B1C4
// A fading wall: the caps and top unlit, then the faces with its lights; the faces and top at the
// fade alpha (the top inversely, the second face at 0.4 of it), without depth writes below 0.9.
void EIWallPart::WallOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  auto* wall = static_cast<EIWallPart*>(otd->m_instance);
  ERC* rc = data.m_rc;
  // GC: also resets a global light parameter block (DAT_804B08BC: 1.5, 0.37).
  ELights unlit;
  unlit.m_numPoint = 1;
  data.m_curLights = &unlit;
  rc->Lights(&unlit);
  data.m_curLights = nullptr;
  const size_t digest = static_cast<size_t>(wall->m_slotIndex[1]);
  ERShader* shader = digest < wall->m_digests.size() ? wall->m_digests[digest].m_shader : nullptr;
  if (data.m_curShader != shader) {
    data.m_curShader = shader;
    if (shader && shader->GetShader()) shader->GetShader()->Select(rc, 0);
  }
  for (int i = 4; i >= 0; --i) {
    ESubModelShader* s = wall->m_slots[i];
    if (!s) continue;
    if (i < 2 && data.m_curLights != otd->m_lights) {
      data.m_curLights = otd->m_lights;
      rc->Lights(otd->m_lights);
    }
    float alpha = 1.0f;
    if (i != 0 && i != 4) {
      alpha = wall->m_partAlpha[GetContext()];
      if (i == 3) alpha *= 0.4f;
      else if (i == 2) alpha = 1.0f - alpha;
    }
    rc->ZTest(true, 2, alpha >= 0.9f ? 0 : 1, 0);
    rc->SetAlpha(alpha);
    rc->SetMipMap(s->m_mipMap, 0);
    rc->DisplayList(s->m_dl);
  }
  data.m_curLights = nullptr;
}

// 8002C1EC
// The faces' shaders become the wallpaper (held while the part is).
void EIWallPart::ChangeWallpaper(unsigned int shaderId) {
  auto* shader = static_cast<ERShader*>(_shaderman.AddRef(shaderId, nullptr, 0));
  ChangeShader(0x2b7ef272, shaderId);
  ChangeShader(0xc26d593a, shaderId);
  if (m_wallpaper) {
    m_wallpaper->DelRef();
    m_wallpaper = nullptr;
  }
  m_wallpaper = shader;
}

// 8002C274
ERShader* EIWallPart::GetWallpaperShader() {
  if (m_wallpaper) return m_wallpaper;
  if (m_model && !m_model->m_subModels.empty() && !m_digests.empty()) return m_digests[0].m_shader;
  return nullptr;
}

// 8002C2C0
// Flags 1 / 2: visible in context 0 / 1.
void EIWallPart::SetVisible(bool visible) {
  const unsigned int bit = GetContext() == 0 ? 1u : 2u;
  if (visible) m_flags |= bit;
  else m_flags &= ~bit;
}

// 8002C32C
unsigned int EIWallPart::GetVisible(int context) {
  if (context == 0) return m_flags & 1;
  if (context != 1) return 0;
  return (m_flags >> 1) & 1;
}

// 8002C35C
// Up: the faces and caps (0x1B) at full alpha; fading: all five parts; down: the cut-down top and
// caps (0x15). Unless forced, a part already settled up or down stays.
void EIWallPart::SetWallState(EWallUpDownStateType state, bool force) {
  const int context = GetContext();
  if (!force) {
    if (m_parts[context] == 0x1b && state == 0 && m_partAlpha[context] == 1.0f) return;
    if (m_parts[context] == 0x15 && state == 2 && m_partAlpha[context] == 0.0f) return;
  }
  if (state == 0) {
    m_partAlpha[context] = 1.0f;
    m_parts[context] = 0x1b;
  } else if (state == 1) {
    m_parts[context] = 0x1f;
  } else {
    m_partAlpha[context] = 0.0f;
    m_parts[context] = 0x15;
  }
}

// 8003065C
void EIWallPart::RealizeShaderTuning() {}

// 8002B504
// The shadow model through the level's shadow projection (ELevelDrawData +0x40) with the wall's second
// slot's shader as a shadow mask, for a wall facing away from the sun (or between two parts of one
// room).
void EIWallPart::DrawShadow(ELevelDrawData& data) {
  if (!m_model || !m_shadowModel) return;
  EMat4* matrix = GetDrawMatrix();
  if (data.m_curMatrix != matrix) {
    data.m_curMatrix = matrix;
    data.m_curAnim = nullptr;
    data.m_rc->ModelMatrices(matrix, 1);
  }
  if (m_room2 != m_room && m_worldNormal[0] * data.m_sunDir.x + m_worldNormal[1] * data.m_sunDir.y < 0.0f) return;
  if (data.m_fieldB0 != &data.m_shadowProj) {
    data.m_fieldB0 = &data.m_shadowProj;
    data.m_rc->ViewMatrix(&data.m_shadowProj, 0, 0.0f);
  }
  ERShader* shader = m_digests[m_slotIndex[1]].m_shader;
  auto* tagged = reinterpret_cast<ERShader*>(reinterpret_cast<uintptr_t>(shader) | 1);
  if (data.m_curShader != tagged) {
    data.m_curShader = tagged;
    if (EShader* s = shader ? shader->GetShader() : nullptr) s->SelectForShadowMask(data.m_rc);
  }
  static_cast<ERModel*>(m_shadowModel)->DrawGeometry(data.m_rc);
}

// 8002B68C
// The level's wall shadow callback (outside): each listed wall part facing away from the sun (or inside
// one room) whose shadow rectangle (its bounds pushed along the sun by their top or bottom) is in view
// draws its shadow model through the shadow projection, with its first shader (its second when it
// has more than one) as a shadow mask.
void EIWallPart::DrawOutsideShadows(std::vector<EInstance*>& walls, EVec3& sunDir, ELevelDrawData& data) {
  EVec3 sun{sunDir.x, sunDir.y, sunDir.z - 0.4f};
  if (sun.x != 0.0f || sun.y != 0.0f || sun.z != 0.0f) {
    const float inv = 1.0f / std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    sun = EVec3{sun.x * inv, sun.y * inv, sun.z * inv};
  }
  const float k = -1.0f / sun.z;
  const float dx = sun.x * k, dy = sun.y * k;
  const int x0 = dx < 0.0f ? 5 : 2, x1 = dx < 0.0f ? 2 : 5;
  const int y0 = dy < 0.0f ? 5 : 2, y1 = dy < 0.0f ? 2 : 5;
  if (data.m_fieldB0 != &data.m_shadowProj) {
    data.m_fieldB0 = &data.m_shadowProj;
    data.m_rc->ViewMatrix(&data.m_shadowProj, 0, 0.0f);
  }
  for (EInstance* inst : walls) {
    auto* wall = static_cast<EIWallPart*>(inst);
    if (wall->m_room2 != wall->m_room && wall->m_worldNormal[0] * sun.x + wall->m_worldNormal[1] * sun.y < 0.0f) continue;
    const float* b = wall->m_bounds;
    float rect[6] = {dx * b[x0] + b[0], dy * b[y0] + b[1], 0.0f, dx * b[x1] + b[3], dy * b[y1] + b[4], 0.0f};
    if (data.m_win->TestAsRect(*reinterpret_cast<EBound3*>(rect))) continue;
    data.m_rc->ModelMatrices(wall->GetDrawMatrix(), 1);
    if (wall->m_digests.empty() || !wall->m_shadowModel) continue;
    ERShader* shader = wall->m_digests[wall->m_digests.size() > 1 ? 1 : 0].m_shader;
    auto* tagged = reinterpret_cast<ERShader*>(reinterpret_cast<uintptr_t>(shader) | 1);
    if (data.m_curShader != tagged) {
      data.m_curShader = tagged;
      if (EShader* s = shader ? shader->GetShader() : nullptr) s->SelectForShadowMask(data.m_rc);
    }
    static_cast<ERModel*>(wall->m_shadowModel)->DrawGeometry(data.m_rc);
  }
  data.m_curAnim = nullptr;
  data.m_curMatrix = nullptr;
}

// 80030560 EIWallPart::GetTypeInfo
// 8003056C EIWallPart::GetTypeName
// 80030578 EIWallPart::GetTypeKey
// 80030584 EIWallPart::GetTypeVersion
// 80030520 EIWallPart::SafeDelete
// 800304A8 EIWallPart::New
// 800304D0 EIWallPart::Construct
// 800304FC EIWallPart::Destruct
// 80030590 EIWallPart::GetTypeInfoStatic
// 8003059C EIWallPart::GetReadVersion
SIMMY_STORABLE_IMPL(EIWallPart, "EIWallPart", 0, EIStaticModel)

// ---------------------------------------------------------------------------------------------------
// Fences

namespace {
// 80423A94 s_wallConfigIdMap: the segments in g_wallConfigs' order.
const int kWallConfigIdMap[6] = {8, 2, 4, 1, 0x20, 0x10};
// 80475620 g_wallConfigs (800372D8 static initialisation): per segment the model's offset in the tile
// (x, y), its turn in eighths of a circle, and an unused word.
struct WallConfig {
  float x, y, rotation;
  int unused;
};
const WallConfig kWallConfigs[6] = {{0.5f, 0.0f, 0.0f, 0},  {0.0f, -0.5f, 2.0f, 0}, {0.0f, 0.5f, 2.0f, 0},
                                    {-0.5f, 0.0f, 0.0f, 0}, {0.0f, 0.0f, 2.0f, 0},  {0.0f, 0.0f, 0.0f, 0}};
}  // namespace

// 80037234
// A wall model's shadow model: the plain wall (g_wallTable's first row) of the same end-cap variant, for
// the models of rows 1..14 (the first nine variants); any other model is its own shadow.
RET_UNKNOWN RemapWallShadowId(unsigned int id) {
  for (int row = 1; row < 15; ++row)
    for (int col = 0; col < 9; ++col)
      if (g_wallTable[row * 10 + col] == id) return g_wallTable[col];
  return id;
}

// 80029D2C
// Walls outside (the side function finds no room on its side) cast a shadow: the wall's model when it
// stands between no rooms at all, its plain variant (RemapWallShadowId) when one side is outside.
void EIWallPart::GetShadowModel() {
  short side = 0, other = 0;
  if (m_thisSideFn) m_thisSideFn(m_thisSideData, false, side, other);
  unsigned int id = 0;
  if (side == 0) {
    if (m_room == 0 && m_room2 == 0) id = m_modelId;
    else if (m_room == 0 || m_room2 == 0) id = static_cast<unsigned int>(RemapWallShadowId(m_modelId));
  }
  if (id) m_shadowModel = _modelman.AddRef(id, nullptr, 0);
}

// 800371C4
// The segment's index in g_wallConfigs (-1: none).
RET_UNKNOWN RemapWallId(TileWallsSegment segment) {
  for (int i = 0; i < 6; ++i)
    if (kWallConfigIdMap[i] == static_cast<int>(segment)) return i;
  return -1;
}

int EIFenceWall::m_nInstances = 0;

// 80029360
// The fence meter: the fence tiles in the house, 100 the most.
double EIFenceWall::GetFenceMeterValue() { return static_cast<float>(m_nInstances) / 100.0f; }

// 80029580
unsigned int EIFenceWall::VisibilityTest(E3DWindow& win) { return win.Test(*reinterpret_cast<EBound3*>(m_bounds)); }

// 80030850
EIFenceWall::EIFenceWall() {}

// 80029890
// The style's fence model (picket 2/0xE, rail 0xC, iron 0xD; the diagonal ones for segments 0x10 and
// 0x20) on the tile: turned by the segment's configuration, moved to its edge, the x and y of the
// matrix swapped (the world's axes); its rooms; into the house's level.
EIFenceWall::EIFenceWall(TileWallsSegment& segment, CTilePt& tile, WallStyle style) {
  ++m_nInstances;
  m_tile = tile;
  const bool diagonal = segment == 0x10 || segment == 0x20;
  unsigned int model = 0;
  switch (static_cast<int>(style)) {
    case 0xc: model = diagonal ? 0x94a968b0 : 0xd0bfc632; break;
    case 0xd: model = diagonal ? 0x53fc260c : 0x3e9e686e; break;
    case 2:
    case 0xe: model = diagonal ? 0xbc6b00f6 : 0xd40a4b23; break;
    default: break;
  }
  SetModel(model, false, nullptr, nullptr);
  m_shadowModel = nullptr;
  const int id = static_cast<int>(RemapWallId(segment));
  const WallConfig& cfg = kWallConfigs[id >= 0 ? id : 5];
  EHouse* house = _globals.m_house;
  EMat4 m;
  m.Id();
  m.RotateZ(cfg.rotation * 0.7853982f);
  EVec3 t;
  t.x = static_cast<float>(tile.m_x) + cfg.y + house->m_posY;
  t.y = static_cast<float>(tile.m_y) + cfg.x + house->m_posX;
  t.z = 0.0f;
  m.PostTranslate(t);
  for (auto& row : m.m) std::swap(row[0], row[1]);
  SetOrient(m);
  CTilePt first, second;
  GetPoints(first, second);
  m_room = static_cast<short>(ResolveRoomIDAtCentre(first));
  m_room2 = static_cast<short>(ResolveRoomIDAtCentre(second));
  GetShadowModel();
  house->m_level->InsertWall(this, m_shadowModel != nullptr);
}

// 80029B94
// (The level and the shadow model are let go by EIWallPart's destructor.)
EIFenceWall::~EIFenceWall() { --m_nInstances; }

// 8002A4A0
// A plain model (no wall fading or wallpaper).
void EIFenceWall::Draw(ELevelDrawData& data) { EIStaticModel::Draw(data); }

// 80030898
RET_UNKNOWN EIFenceWall::IsOpaque() { return 0; }

// 80030718 EIFenceWall::GetTypeInfo
// 80030724 EIFenceWall::GetTypeName
// 80030730 EIFenceWall::GetTypeKey
// 8003073C EIFenceWall::GetTypeVersion
// 800306D8 EIFenceWall::SafeDelete
// 80030660 EIFenceWall::New
// 80030688 EIFenceWall::Construct
// 800306B4 EIFenceWall::Destruct
// 80030748 EIFenceWall::GetTypeInfoStatic
// 80030754 EIFenceWall::GetReadVersion
SIMMY_STORABLE_IMPL(EIFenceWall, "EIFenceWall", 0, EIWallPart)

// 8002C48C
EFenceWall::EFenceWall(TileWallsSegment& segment, CTilePt& tile, WallStyle style) {
  m_parts.push_back(new EIFenceWall(segment, tile, style));
}
