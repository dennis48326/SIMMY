// Port of object placement on the world side: game/object.cpp (cXObjectImpl's placement part:
// PlaceSpec, TestAndPlace, CanPlace, Place, Pickup, TestIntersection, ComputeRect, the User* checks,
// Turn), game/mtobject.cpp (cXMTObjectImpl's: a multi-tile object places, turns and picks up all its
// parts) and game/walls.cpp (CheckWallFlags, SectWall). See placement.hpp.
//
// PC: on the port's world model (placement.hpp). Not ported here (logged once): the slot hierarchy
// (objects in other objects' slots, HierarchySite), the objects' trees run on placement events
// (RunTree 5 "allow intersection", 6 wall adjacency changed, 9 placed, 0xA picked up, 0xB/0xC user placed /
// picked up: cXObjectImpl::RunTree runs a tree to its end on the object's stack (TreeSim vtable 0xA4),
// which the port's TreeSim does not offer), the zero-extent override's 0x437 objects. Chair facing
// (UpdateChairFacing), the tiles' support flags (SetSupportFlag) and wall adjacency
// (UpdateWallAdjacency) are ported.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/roomresolve.hpp"

#include <algorithm>
#include <cstdio>

int gPlacementError = 0;                // 804FDC90
cXObject* gPlacementConflict = nullptr; // 804FDC94
int g_handlingPlacementObject = 0;      // 804FDBF4 Globs::s_handlingPlacementObject

namespace {
// 80424300 cXObjectImpl::sXDirTable / 80424324 sYDirTable: the eight neighbours (direction order),
// then the tile itself.
const int kXDir[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
const int kYDir[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

std::vector<PlacementGroup*> s_groups;

cFixedWorldImpl* World() { return Globs_pFixedWorld; }

// The tile of an FTilePt ([0] y, [1] x) on `level`.
CTilePt TileOf(const int pos[2], int level) { return CTilePt(pos[1] >> 4, pos[0] >> 4, level); }

// GC 80151B58 cFixedWorldImpl::OutOfBounds(FTilePt&): its tile on level 1.
bool OutOfBounds(const int pos[2]) {
  cFixedWorldImpl* w = World();
  if (!w) return true;
  CTilePt tile = TileOf(pos, 1);
  return w->OutOfBounds(tile) != 0;
}

// GC 80151B10 cFixedWorldImpl::OutOfGrid(CTilePt&): off the world's arrays.
bool OutOfGrid(CTilePt& tile) {
  cFixedWorldImpl* w = World();
  return !w || tile.GetX() < 0 || tile.GetY() < 0 || tile.GetX() >= w->m_xSize || tile.GetY() >= w->m_ySize;
}

// GC 80151C98 cFixedWorldImpl::IsGrassAt: the tile's floor pattern (GetFloor) is of kind 8 or 9 in the
// "FloorSet" table (EGlobal +0xD4, DAT_80475EA0). PC: here; the world's virtual is not ported.
bool IsGrassAt(CTilePt& tile) {
  cFixedWorldImpl* w = World();
  if (!w || !w->m_floors || OutOfGrid(tile) || !_globals.m_floorSet) return false;
  const unsigned int pattern = *w->m_floors->At(tile.GetX(), tile.GetY());
  const auto* rows = static_cast<const unsigned char*>(ERQuickdata::Ptr(_globals.m_floorSet, 0));
  const unsigned int count = rows ? ERQuickdata::U32(rows - 4) : 0;
  if (pattern >= count) return false;
  const unsigned int kind = ERQuickdata::U32(ERQuickdata::Ptr(rows, pattern * 4), 0x10);
  return kind == 8 || kind == 9;
}

// EGlobal +0x3DC: the "move objects" cheat (in the EOR cheats block).
bool MoveObjectsCheat() { return _globals.ConfigInt(0x3dc) != 0; }

// GC 8015213C cFixedWorldImpl::MayEditTile: not a tile of the lot's locked part (flag 0x20), or the cheat.
bool MayEditTile(CTilePt& tile) {
  cFixedWorldImpl* w = World();
  if (MoveObjectsCheat()) return true;
  if (!w || OutOfGrid(tile)) return false;
  return ((w->GetFlags(tile) ^ 0x20) >> 5) & 1;
}

// 80147050 TileWalls::RotateSegment (sRotateSegmentLookup, GenerateRotationLookups): edge segments
// 1, 2, 4, 8 step round by a quarter turn each; the diagonals 0x10 and 0x20 swap on odd turns.
unsigned int RotateSegment(unsigned int segments, int quarters) {
  quarters &= 3;
  unsigned int r = 0;
  for (unsigned int bit = 1; bit < 0x40; bit <<= 1) {
    if (!(segments & bit)) continue;
    if (bit == 0x10 || bit == 0x20) r |= (quarters & 1) ? (bit ^ 0x30) : bit;
    else {
      unsigned int b = bit;
      for (int i = 0; i < quarters; ++i) b = b << 1 > 8 ? 1 : b << 1;
      r |= b;
    }
  }
  return r;
}

// 800EB688 HasZeroExtent: object flags (data 8) bit 2.
bool HasZeroExtent(cXObjectImpl* o) { return (o->GetData(8) >> 2) & 1; }
// 800EB6C4 CanIntersectPeople: zero-extent objects unless flag 1; others with flag 4.
bool CanIntersectPeople(cXObjectImpl* o) {
  const unsigned int f = static_cast<unsigned int>(o->GetData(8));
  return HasZeroExtent(o) ? ((f ^ 2) >> 1) & 1 : (f >> 4) & 1;
}
// 800E2544 GetRequiredSegment: the wall a wall object needs (wall placement flags, data 0xD: 1 the
// tile's segment 2, 4 its segment 8), turned by its direction.
unsigned int GetRequiredSegment(cXObjectImpl* o) {
  const unsigned int f = static_cast<unsigned int>(o->GetData(0xd)) & 5;
  unsigned int seg = f == 1 ? 2 : f == 4 ? 8 : 0;
  if (seg && o->GetData(1)) seg = RotateSegment(seg, o->GetData(1) / 2);
  return seg;
}
// 800E178C GetTileWidth: the definition's +0x86 (0: a tile, 16). GC: people their person width.
int GetTileWidth(cXObjectImpl* o) {
  const int w = o->m_def ? ERQuickdata::S16(o->m_def, 0x86) : 0;
  return w ? w : 16;
}

// The definition's flags (+0xB6): 1, 2, 4 the zero-extent override kinds.
unsigned short DefFlags(cXObjectImpl* o) { return o->m_def ? objdef::Flags(o->m_def) : 0; }

// 800DF210 / 800C6AD0 IsPartOfMe: the object itself, or a part of its multi-tile group.
bool IsPartOfMe(cXObjectImpl* self, cXObjectImpl* other) {
  if (self == other) return true;
  if (PlacementGroup* g = PlacementGroupOf(self))
    for (auto& p : g->parts)
      if (p.object == other) return true;
  return false;
}

// 800C6EBC ObjectsAtTile / the tile's object chain (ObjectModule vtable 0x254, GetNextObjectSibling):
// the objects whose site is the tile. PC: the module's objects at that tile and level that no other
// object holds (data 2).
std::vector<cXObjectImpl*> ObjectsAtTile(CTilePt& tile) {
  std::vector<cXObjectImpl*> out;
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return out;
  for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject) {
    if (o->m_location[0] < 0 || o->m_location[1] < 0 || o->m_data[2] != 0) continue;
    const CTilePt t = o->GetCTilePt();
    if (t.GetX() == tile.GetX() && t.GetY() == tile.GetY() && t.GetLevel() == tile.GetLevel()) out.push_back(o);
  }
  return out;
}

// 800DF784 cXObjectImpl::SetLocation, PC: the object's FTilePt and level, its tile in the module's
// tile map (the tile's first object), its model moved (ISimInstance::SetObjOrient).
void SetLocation(cXObjectImpl* o, const int pos[2], int level) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (module && o->m_location[0] >= 0 && o->m_location[1] >= 0) {
    CTilePt old = o->GetCTilePt();
    if (module->GetTileObjectID(old) == o->m_id) module->SetTileObjectID(old, 0);
  }
  o->m_location[0] = pos[0];
  o->m_location[1] = pos[1];
  o->m_level = level;
  if (module && pos[0] >= 0 && pos[1] >= 0 && o->m_data[2] == 0) {
    CTilePt tile = o->GetCTilePt();
    if (!module->GetTileObjectID(tile)) module->SetTileObjectID(tile, o->m_id);
  }
  UpdateObjectRoomID(o);  // 800DF784: ResolveRoomID, data 0x1D
}

// 800EA6B0 cXObjectImpl::SimObjectPlaced: the model takes its place and is no longer carried (cursor
// flags 0x40 / 0x80).
void SimObjectPlaced(cXObjectImpl* o) {
  if (!o->m_instance) return;
  o->m_instance->SetCursFlags(o->m_instance->GetCursFlags() & ~0xc0u);
  o->m_instance->SetObjOrient();
}

// Each part's site, from the lead's: the lead's FTilePt and level less `part`'s offsets.
void LeadSite(PlacementGroup::Part& part, const int pos[2], int level, int lead[2], int& leadLevel) {
  lead[1] = pos[1] - part.offset[0] * 16;
  lead[0] = pos[0] - part.offset[1] * 16;
  leadLevel = level - part.offset[2];
}
PlacementGroup::Part* PartOf(PlacementGroup* g, cXObjectImpl* o) {
  for (auto& p : g->parts)
    if (p.object == o) return &p;
  return nullptr;
}

// 800C5F10 cXMTObjectImpl::AssignOffsets: each part's offsets from the lead turned by the lead's
// direction (data 1 & 7: 2 a quarter turn, x <- -y, y <- x).
void AssignOffsets(PlacementGroup* g) {
  if (g->parts.empty()) return;
  const PlacementGroup::Part& lead = g->parts[0];
  const int dir = lead.object->GetData(1) & 7;
  for (auto& p : g->parts) {
    const int x = p.base[0] - lead.base[0], y = p.base[1] - lead.base[1];
    p.offset[2] = p.base[2] - lead.base[2];
    p.offset[0] = p.offset[1] = 0;
    if (dir == 0) p.offset[0] = x, p.offset[1] = y;
    else if (dir == 2) p.offset[0] = -y, p.offset[1] = x;
    else if (dir == 4) p.offset[0] = -x, p.offset[1] = -y;
    else if (dir == 6) p.offset[0] = y, p.offset[1] = -x;
  }
}

// The plain object's CanPlace / Place / Pickup / UserCanPlace (the cXObjectImpl methods).
int PlainCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);
void PlainPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);
void PlainPickup(cXObjectImpl* o);
int PlainUserCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);
}  // namespace

// ---------------------------------------------------------------------------------------------------
// Multi-tile groups (PC)

PlacementGroup* PlacementGroupOf(cXObjectImpl* object) {
  if (!object) return nullptr;
  for (PlacementGroup* g : s_groups)
    for (auto& p : g->parts)
      if (p.object == object) return g;
  return nullptr;
}

// 800C5848 cXMTObjectImpl::Initialize (ObjDefinition::GetMultiTileOffsets 800D9430: sub index >> 8 and
// its low byte; the level from definition +0x70) and 800C58CC SetLeader for each part, then
// AssignOffsets.
PlacementGroup* MakePlacementGroup(const std::vector<cXObjectImpl*>& parts) {
  if (parts.empty()) return nullptr;
  auto* g = new PlacementGroup;
  for (cXObjectImpl* o : parts) {
    PlacementGroup::Part p;
    p.object = o;
    const short sub = o->m_def ? objdef::SubIndex(o->m_def) : 0;
    p.base[0] = static_cast<short>(sub) >> 8;
    p.base[1] = static_cast<unsigned char>(sub & 0xff);
    p.base[2] = o->m_def ? ERQuickdata::S16(o->m_def, 0x70) : 0;
    g->parts.push_back(p);
  }
  AssignOffsets(g);
  s_groups.push_back(g);
  return g;
}

void FreePlacementGroup(PlacementGroup* group) {
  s_groups.erase(std::remove(s_groups.begin(), s_groups.end(), group), s_groups.end());
  delete group;
}

PlacementGroup* FindPlacementGroup(cXObjectImpl* object) {
  if (PlacementGroup* g = PlacementGroupOf(object)) return g;
  if (!object || !object->IsMultiTile() || !Globs_pObjectModule) return nullptr;
  // The parts of the same file and master with the same direction, where their offsets (turned by
  // the direction) put them from one lead tile.
  auto turned = [](cXObjectImpl* o, int& ox, int& oy) {
    const short sub = objdef::SubIndex(o->m_def);
    const int x = static_cast<short>(sub) >> 8, y = static_cast<unsigned char>(sub & 0xff);
    switch (o->GetData(1) & 7) {
      case 2: ox = -y, oy = x; break;
      case 4: ox = -x, oy = -y; break;
      case 6: ox = y, oy = -x; break;
      default: ox = x, oy = y; break;
    }
  };
  int ox = 0, oy = 0;
  turned(object, ox, oy);
  const CTilePt tile = object->GetCTilePt();
  const int leadX = tile.GetX() - ox, leadY = tile.GetY() - oy;
  const short master = objdef::MasterID(object->m_def);
  std::vector<cXObjectImpl*> parts;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
    if (!o->m_selector || !object->m_selector || o->m_selector->m_resFile != object->m_selector->m_resFile ||
        !o->m_def || objdef::MasterID(o->m_def) != master || objdef::SubIndex(o->m_def) == -1 ||
        (o->GetData(1) & 7) != (object->GetData(1) & 7))
      continue;
    turned(o, ox, oy);
    const CTilePt t = o->GetCTilePt();
    if (t.GetX() == leadX + ox && t.GetY() == leadY + oy && t.GetLevel() == tile.GetLevel()) parts.push_back(o);
  }
  if (parts.size() < 2) return nullptr;
  // The lead is the part with the smallest sub index (GC: the group's first part).
  std::sort(parts.begin(), parts.end(), [](cXObjectImpl* a, cXObjectImpl* b) {
    return static_cast<unsigned short>(objdef::SubIndex(a->m_def)) < static_cast<unsigned short>(objdef::SubIndex(b->m_def));
  });
  return MakePlacementGroup(parts);
}

// ---------------------------------------------------------------------------------------------------
// game/walls.cpp

// 8014A560
// Whether the walls of the object's tile suit its wall placement flags (data 0xD) when it faces `dir`.
// The walls are turned to the object's frame first. Flags: 0x40 needs a diagonal, 0x80 allows one;
// 0x20 needs a corner (two walls meeting), 0x10 must not stand in one; bits 0, 3, 1, 2 need a wall on
// the object's segment 2, 1, 4, 8 (any style but the fence-like 1, 7, 8, 9, 0x16: else error 0x16; no
// wall: 6), bits 8, 0xB, 9, 0xA forbid one there (7).
int CheckWallFlags(const int pos[2], int level, int dir, unsigned int flags) {
  cFixedWorldImpl* w = World();
  if (!w) return 1;
  CTilePt tile(pos[1] >> 4, pos[0] >> 4, level);
  TileWalls walls = w->GetWall(tile);
  const bool diagonal = walls.HasDiagonal();
  if ((flags & 0x40) && !diagonal) {
    gPlacementError = 0x17;
    return 0;
  }
  if (!(flags & 0x80) && diagonal) {
    gPlacementError = 7;
    return 0;
  }
  if (flags == 0) return 1;
  // TileWalls::Rotate by (8 - dir) / 2 quarters: the rotated walls' segment s is the tile's segment
  // RotateSegment(s, -quarters).
  const int quarters = static_cast<int>((8u - static_cast<unsigned int>(dir)) >> 1 & 3);
  auto has = [&](unsigned int seg) { return walls.HasWall(RotateSegment(seg, 4 - quarters)); };
  auto style = [&](unsigned int seg) { return walls.GetStyle(RotateSegment(seg, 4 - quarters)); };
  const bool corner = (has(1) && has(2)) || (has(1) && has(8)) || (has(2) && has(4)) || (has(8) && has(4));
  if ((flags & 0x20) && !corner) {
    gPlacementError = 4;
    return 0;
  }
  if ((flags & 0x10) && corner) {
    gPlacementError = 5;
    return 0;
  }
  const unsigned int segs[4] = {2, 1, 4, 8};
  const unsigned int need[4] = {flags & 1, flags >> 3 & 1, flags >> 1 & 1, flags >> 2 & 1};
  const unsigned int forbid[4] = {flags >> 8 & 1, flags >> 0xb & 1, flags >> 9 & 1, flags >> 10 & 1};
  bool wall[4], solid[4];
  for (int i = 0; i < 4; ++i) {
    wall[i] = has(segs[i]);
    solid[i] = false;
    if (wall[i]) {
      const int s = style(segs[i]);
      solid[i] = s == 1 || s == 7 || s == 8 || s == 9 || s == 0x16;
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (forbid[i] && wall[i]) {
      gPlacementError = 7;
      return 0;
    }
    if (need[i] && !solid[i]) {
      gPlacementError = wall[i] ? 0x16 : 6;
      return 0;
    }
  }
  return 1;
}

// 8014A00C
// Whether a wall crosses the inside of `rect` (1/16 tiles: [0] y max, [1] x max, [2] y min, [3] x min)
// on `level`. Walls of styles 3, 5, 6, 0xF and 0x17 do not count.
int SectWall(const int rect[4], int level) {
  cFixedWorldImpl* w = World();
  if (!w) return 0;
  const int xEnd = (rect[1] - 1) >> 4, yEnd = (rect[0] - 1) >> 4;
  for (int y = rect[2] >> 4; y <= yEnd; ++y)
    for (int x = rect[3] >> 4; x <= xEnd; ++x) {
      CTilePt tile(x, y, level);
      if (OutOfGrid(tile)) continue;
      TileWalls walls = w->GetWall(tile);
      for (int seg = walls.First(); seg != 0; seg = walls.Next(seg)) {
        const int s = walls.GetStyle(seg);
        if (s == 3 || s == 5 || s == 6 || s == 0xf || s == 0x17) continue;
        int v, a, b;
        if (seg == 2) v = y << 4, a = rect[2], b = rect[0];
        else if (seg == 1) v = x << 4, a = rect[3], b = rect[1];
        else if (seg == 0x10) v = (y + x + 1) * 0x10, a = rect[0] + rect[1], b = rect[2] + rect[3];
        else if (seg == 0x20) v = (y - x) * 0x10, a = rect[2] - rect[1], b = rect[0] - rect[3];
        else continue;
        if ((a < v && v < b) || (v < a && b < v)) return 1;
      }
    }
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// game/object.cpp

// 800DCB50
int ObjIsInWorld(cXObjectImpl* o) { return (o->m_location[1] >> 4) > 0 && (o->m_location[0] >> 4) > 0; }

// 800DF4D0
// The object's footprint at `pos`: its tile width around it, less the definition's insets (+0x9A, a
// nibble per side) turned by its direction. GC: people also take person data 0x49 off the insets.
void ObjComputeRect(cXObjectImpl* o, const int pos[2], int rect[4]) {
  const int half = GetTileWidth(o) / 2;
  rect[3] = pos[1] - half;
  rect[2] = pos[0] - half;
  rect[1] = pos[1] + half;
  rect[0] = pos[0] + half;
  const unsigned int insets = o->m_def ? static_cast<unsigned short>(ERQuickdata::S16(o->m_def, 0x9a)) : 0;
  if (!insets) return;
  const int side[4] = {static_cast<int>(insets & 0xf), static_cast<int>(insets >> 4 & 0xf),
                       static_cast<int>(insets >> 8 & 0xf), static_cast<int>(insets >> 12 & 0xf)};
  const int q = (8 - o->GetData(1) % 8) / 2;
  rect[2] += side[q % 4];
  rect[1] -= side[(q + 1) % 4];
  rect[0] -= side[(q + 2) % 4];
  rect[3] += side[(q + 3) % 4];
}

// 800DEBB8
// Whether the object fits at `pos`: no other object's footprint on its tile or the eight around it
// overlaps its own (people only when they do not route through it and it cannot stand in people); on
// its own tile, zero-extent objects clash only when they use the same layer (data 0x3F bit 0; bit 1
// unless both are zero-extent) or, both zero-extent, need the same wall; and no wall crosses it unless
// data 0x2A has 0x400. PC: the other object's "allow intersection" tree (RunTree 5) is not run, and
// people count as standing still (cXPerson vtable 0x21C).
int ObjTestIntersection(cXObjectImpl* o, const int pos[2], int level) {
  if (o->GetType() == 2) return 1;  // GC: a routing person fits anywhere (people are not placed here)
  cFixedWorldImpl* w = World();
  if (!w) return 0;
  const bool zero = HasZeroExtent(o);
  const unsigned int layers = static_cast<unsigned int>(o->GetData(0x3f));
  int rect[4];
  ObjComputeRect(o, pos, rect);
  for (int i = 0; i < 9; ++i) {
    CTilePt tile((pos[1] >> 4) + kXDir[i], (pos[0] >> 4) + kYDir[i], level);
    if (w->OutOfBounds(tile)) continue;
    for (cXObjectImpl* other : ObjectsAtTile(tile)) {
      if (IsPartOfMe(o, other)) continue;
      if (other->GetType() == 2) {
        if (CanIntersectPeople(o)) continue;
      } else {
        const bool otherZero = HasZeroExtent(other);
        if (zero || otherZero) {
          if (i == 8) {
            const unsigned int otherLayers = static_cast<unsigned int>(other->GetData(0x3f));
            if (otherLayers & layers & 1) {
              gPlacementError = 0xb;
              return 0;
            }
            if (!zero || !otherZero) {
              if (otherLayers & layers & 2) {
                gPlacementError = 0xb;
                return 0;
              }
            } else {
              const unsigned int seg = GetRequiredSegment(o);
              if (seg == GetRequiredSegment(other) && seg != 0) {
                gPlacementError = 0x16;
                return 0;
              }
            }
          }
          continue;
        }
      }
      int otherRect[4];
      ObjComputeRect(other, other->m_location, otherRect);
      if (otherRect[2] < rect[0] && rect[2] < otherRect[0] && otherRect[3] < rect[1] && rect[3] < otherRect[1]) {
        gPlacementError = 0xb;
        gPlacementConflict = other;
        return 0;
      }
    }
  }
  if (!(o->GetData(0x2a) & 0x400) && SectWall(rect, level)) {
    gPlacementError = 7;
    return 0;
  }
  return 1;
}

PlaceSpec::PlaceSpec(const int p[2], int lvl, cXObjectImpl* c, int s) {
  inWorld = 1;
  const int* at = c ? c->m_location : p;
  pos[0] = at[0];
  pos[1] = at[1];
  container = c;
  slot = c ? s : -1;
  level = lvl;
}

// 800DD6E8
// Testing (place false): whether the object may go where the spec says: on the lot (error 1), on
// levels 1..3 (2), not on a tile a "nothing else here" object (definition flags +0xB6 & 6) holds
// (0xB), not upstairs when data 0x2A has 0x1000 (0x1F), the tile's walls as its wall flags want
// (CheckWallFlags), grass or floor as data 0x2A allows (0x12/0x29, 0x13/0x2A, 0x14), inside or outside
// as data 0x1E wants (9, 10), its footprint free (TestIntersection) and the floor among its allowed
// heights (data 4 bit 0; else 0x18). Placing (place true): the object takes the spec's site.
// PC: a site is the object's tile (see SetLocation); slots (spec.container) are not ported (0xD).
int ObjTestAndPlace(cXObjectImpl* o, PlaceSpec& spec, bool place) {
  cFixedWorldImpl* w = World();
  if (place) {
    // GC: HierSever / HierSetSite on the tile's object chain or the container's slot. PC: the tile map
    // entry follows the location (SetLocation).
    // (PC: the container's slot is data 2 / 3, which the caller writes; ObjectIterator and
    // GetContainedObject read them back.)
    return 1;
  }
  if (!spec.inWorld) return 1;
  if (OutOfBounds(spec.pos)) {
    gPlacementError = 1;
    return 0;
  }
  const int type = o->GetType();
  if (type == 2) SIMMY_STUB("cXObjectImpl::TestAndPlace: people on flag-0x10 tiles (cXPerson vtable 0x21C)", 0x800DD6E8);
  if (static_cast<unsigned int>(spec.level - 1) > 2) {
    gPlacementError = 2;
    return 0;
  }
  CTilePt tile = TileOf(spec.pos, spec.level);
  const unsigned short defFlags = DefFlags(o);
  const bool person = o->m_selector && o->m_selector->GetIsPerson();
  bool zeroOverride = false;
  if (!person) {
    // EGlobal::CheckForZeroExtentOverride (80041F00): another object with flags & 6 on the tile.
    for (cXObjectImpl* other : ObjectsAtTile(tile))
      if (other != o && (DefFlags(other) & 6)) zeroOverride = true;
  }
  if (zeroOverride && objdef::GUID(o->m_def) != 0x437) {
    // An override object holds the tile: only an object with flag 1 joins one with flag 4.
    bool joins = false;
    if (defFlags & 1)
      for (cXObjectImpl* other : ObjectsAtTile(tile))
        if (DefFlags(other) & 4) {
          joins = true;
          break;
        }
    if (!joins) {
      gPlacementError = 0xb;
      return 0;
    }
  } else if (defFlags & 6) {
    // An override object itself: the tile must be free (with flag 4: of objects without flag 1).
    bool found = false, allowed = false;
    for (cXObjectImpl* other : ObjectsAtTile(tile)) {
      if (IsPartOfMe(o, other) || (other->m_def && objdef::GUID(other->m_def) == 0x437)) continue;  // GC: also vtable 0x4C
      found = true;
      if (!(defFlags & 4)) break;
      if (DefFlags(other) & 1) {
        allowed = true;
        break;
      }
    }
    if (found && !allowed) {
      gPlacementError = 0xb;
      return 0;
    }
  }
  const unsigned int flags = static_cast<unsigned short>(o->GetData(0x2a));
  if (tile.GetLevel() > 1 && (flags & 0x1000)) {
    gPlacementError = 0x1f;
    return 0;
  }
  if (type != 2 && CheckWallFlags(spec.pos, spec.level, o->GetData(1), static_cast<unsigned short>(o->GetData(0xd))) != 1)
    return 0;
  if (!(flags & 0x200) && spec.level > 1 && IsGrassAt(tile)) {
    gPlacementError = spec.level == 1 ? 0x12 : 0x29;
    return 0;
  }
  if (!(flags & 1) && !IsGrassAt(tile)) {
    gPlacementError = spec.level == 1 ? 0x13 : 0x2a;
    return 0;
  }
  if (!(flags & 2) && spec.level == 1 && IsGrassAt(tile)) {
    gPlacementError = 0x14;
    return 0;
  }
  if (const short where = o->GetData(0x1e)) {
    CTilePt t = TileOf(spec.pos, spec.level);
    const int outside = w ? w->IsOutside(t) : 0;
    if (where == 1 && !outside) {
      gPlacementError = 9;
      return 0;
    }
    if (where == 2 && outside) {
      gPlacementError = 10;
      return 0;
    }
  }
  const short heights = o->GetData(4);
  if (spec.container) {
    // The container's slot (none: 0xD), free or mine (0xB); a slot at the container's middle (no x/y
    // offset) and an offset one of the same height cannot both hold others (0xB); not heavier than the
    // container carries (data 0x1B against its 0x1C: 0xE); not into itself (0xB); the slot's height
    // (ObjectSlot +8) one of the object's heights (0x18).
    cXObjectImpl* c = spec.container;
    const unsigned char* slot = ObjectSlotDescriptor(c, spec.slot);
    if (!slot) {
      gPlacementError = 0xd;
      return 0;
    }
    auto heldBy = [c](int i) { return c->GetContainedObject(i); };
    cXObjectImpl* held = heldBy(spec.slot);
    if (held && held != o) {
      gPlacementError = 0xb;
      return 0;
    }
    int h = 0;
    float alt = 0.0f;
    ObjectSlotHeight(slot, h, alt);
    const bool centred = ERQuickdata::F32(slot, 4) == 0.0f && ERQuickdata::F32(slot, 8) == 0.0f;
    for (int i = ObjectSlotCount(c) - 1; i >= 0; --i) {
      const unsigned char* other = ObjectSlotDescriptor(c, i);
      int oh = 0;
      float oalt = 0.0f;
      if (other) ObjectSlotHeight(other, oh, oalt);
      if (!other || oh != h) continue;
      cXObjectImpl* in = heldBy(i);
      if (!in || in == o) continue;
      const bool otherCentred = ERQuickdata::F32(other, 4) == 0.0f && ERQuickdata::F32(other, 8) == 0.0f;
      if (otherCentred || centred) {
        gPlacementError = 0xb;
        return 0;
      }
    }
    if (c->GetData(0x1c) < o->GetData(0x1b)) {
      gPlacementError = 0xe;
      return 0;
    }
    for (cXObjectImpl* up = c; up;) {
      if (up == o) {
        gPlacementError = 0xb;
        return 0;
      }
      cXObject* next = up->GetContainer();
      up = next ? next->GetObjectImplementation() : nullptr;
    }
    if (h < 1 || heights == 0 || !(heights & (1 << (h - 1)))) {
      gPlacementError = 0x18;
      return 0;
    }
    return 1;
  }
  if (!ObjTestIntersection(o, spec.pos, spec.level)) return 0;
  if (heights == 0 || !(heights & 1)) {  // the floor is height 1
    gPlacementError = 0x18;
    return 0;
  }
  return 1;
}

namespace {
// 800E2F14 cXObjectImpl::RunTree(entry, ...): the object's tree for the entry, run to its end. PC: not
// run (see the file's header); logged once per entry when the object has such a tree.
void RunTreeEvent(cXObjectImpl* o, int entry) {
  if (!o || !o->GetTreeID(entry)) return;
  static bool logged[0x20] = {};
  if (entry >= 0 && entry < 0x20 && !logged[entry]) {
    logged[entry] = true;
    std::printf("[stub] cXObjectImpl::RunTree: placement event %d of %s not run (TreeSim::RunTree)\n", entry, o->DebugName());
  }
}

// The first object at a tile (the module's tile map) and its tile chain (GetNextObjectSibling, PC: the
// other objects whose site is the tile).
cXObjectImpl* TileObject(int x, int y, int level) {
  CTilePt t(x, y, level);
  cFixedWorldImpl* w = World();
  if (!w || w->OutOfBounds(t)) return nullptr;
  std::vector<cXObjectImpl*> here = ObjectsAtTile(t);
  return here.empty() ? nullptr : here.front();
}

// GC 800EB4B4 cXObjectImpl::IsSupport: GetBuildModeType (800EB488: the master's definition +0x8A, its
// build sub-sort) is 6. PC: here (the member is the world side's).
bool IsSupportObject(cXObjectImpl* o) {
  auto* master = o && o->m_selector ? reinterpret_cast<ObjSelector*>(o->m_selector->GetMasterSelector()) : nullptr;
  return master && master->m_def && ERQuickdata::S16(master->m_def, 0x8a) == 6;
}

// 800DE080 SetSupportFlag: the tile's flag 8 (an object there supports others: counters, tables).
void SetSupportFlag(int x, int y, int level, bool on) {
  CTilePt t(x, y, level);
  cFixedWorldImpl* w = World();
  if (!w || w->OutOfBounds(t)) return;
  unsigned char f = w->GetFlags(t) & 0xf7;
  if (on) f |= 8;
  w->SetFlags(t, f);
}

// 800DE788 cXObjectImpl::UpdateChairFacing(void)
// Whether a chair on one of the four neighbouring tiles faces this object's tile (the first object
// there that is not of zero extent: a chair turned towards it); then the objects at this tile get data
// 8's 0x100 (a chair is pulled up) set or cleared, and those whose flag changed run their "placed"
// tree (0x19's handler: RunTree 9). PC: the objects at the tile are the tile's own (no slots).
void UpdateChairFacingAt(cXObjectImpl* self) {
  cFixedWorldImpl* w = World();
  if (!self || !w) return;
  const CTilePt here = self->GetCTilePt();
  bool facing = false;
  for (int d = 0; d < 8 && !facing; d += 2) {
    CTilePt n(here.GetX() + kXDir[d], here.GetY() + kYDir[d], here.GetLevel());
    if (w->OutOfBounds(n)) continue;
    for (cXObjectImpl* o : ObjectsAtTile(n)) {
      if (HasZeroExtent(o)) continue;
      if (o->IsChair() && o->GetData(1) == ((d + 4) & 7)) facing = true;
      break;
    }
  }
  for (cXObjectImpl* o : ObjectsAtTile(const_cast<CTilePt&>(here))) {
    const unsigned short before = static_cast<unsigned short>(o->m_data[8]);
    o->m_data.WriteVar(8, static_cast<short>(facing ? (before | 0x100) : (before & ~0x100)));
    if (((before >> 8) & 1) != (facing ? 1 : 0) && o->GetTreeID(0x19)) RunTreeEvent(o, 9);
  }
}

// 800DEAE4 cXObjectImpl::UpdateChairFacing(ObjectModule*, x, y, level): the object at the tile.
void UpdateChairFacing(int x, int y, int level) {
  if (cXObjectImpl* o = TileObject(x, y, level)) UpdateChairFacingAt(o);
}

// 800E1594 cXObjectImpl::UpdateWallAdjacency
// An object with a wall-adjacency handler (tree entry 6): data 5's low bits say which of its sides (by
// its direction) have walls (1 back... GC: segments 1, 2, 4, 8 of its turned tile -> 1, 4, 2, 8); a
// change runs the handler (RunTree 6).
void UpdateWallAdjacency(cXObjectImpl* o) {
  if (!o->GetTreeID(6)) return;
  cFixedWorldImpl* w = World();
  unsigned int adj = static_cast<unsigned int>(o->m_data[5]) & ~0x3fu;
  CTilePt t = o->GetCTilePt();
  if (w && !w->OutOfBounds(t)) {
    const int dir = o->GetData(1);
    TileWalls walls = w->GetWall(t);
    const int quarters = static_cast<int>((static_cast<unsigned int>(-dir - (-dir >> 31)) >> 1) & 3);
    const unsigned int seg = static_cast<unsigned int>(TileWalls::RotateSegment(static_cast<int>(walls.m_segments & 0xf), quarters));
    if (seg & 1) adj |= 1;
    if (seg & 2) adj |= 4;
    if (seg & 4) adj |= 2;
    if (seg & 8) adj |= 8;
  }
  if (adj != static_cast<unsigned int>(static_cast<unsigned short>(o->m_data[5]))) {
    o->m_data.WriteVar(5, static_cast<short>(adj));
    RunTreeEvent(o, 6);
  }
}

// 800DE340
int PlainCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  PlaceSpec spec(pos, level, container, slot);
  return ObjTestAndPlace(o, spec, false);
}

// 800DE3C4
// Off its old site (a chair: the tile it faced re-checked; a support: its tile's flag off), onto the new
// one: data 2 / 3 name the container and slot, the location follows (the container's, in a slot); the
// chair facing of its tile and (a chair) of the tile it faces, its wall adjacency, the "placed" tree
// (RunTree 9) of it and its container, the support flag, then the model takes its place
// (SimObjectPlaced).
void PlainPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  if (container) level = container->m_level;
  const int oldX = o->m_location[1] >> 4, oldY = o->m_location[0] >> 4, oldLevel = o->m_level;
  const int oldDir = o->GetData(1) & 7;
  PlaceSpec nowhere(true);
  ObjTestAndPlace(o, nowhere, true);
  const bool chair = o->IsChair(), support = IsSupportObject(o);
  if (chair) UpdateChairFacing(oldX + kXDir[oldDir], oldY + kYDir[oldDir], oldLevel);
  if (support) SetSupportFlag(oldX, oldY, oldLevel, false);
  PlaceSpec spec(pos, level, container, slot);
  ObjTestAndPlace(o, spec, true);
  o->m_data.WriteVar(2, static_cast<short>(container ? container->m_id : 0));
  o->m_data.WriteVar(3, static_cast<short>(spec.slot));
  SetLocation(o, container ? container->m_location : spec.pos, level);
  const int x = o->m_location[1] >> 4, y = o->m_location[0] >> 4;
  UpdateChairFacing(x, y, o->m_level);
  if (chair) {
    const int dir = o->GetData(1) & 7;
    UpdateChairFacing(x + kXDir[dir], y + kYDir[dir], o->m_level);
  }
  UpdateWallAdjacency(o);
  if (!o->IsMultiTile()) RunTreeEvent(o, 9);
  if (container) RunTreeEvent(container, 9);
  if (support) SetSupportFlag(x, y, o->m_level, true);
  if (!o->IsMultiTile()) SimObjectPlaced(o);
}

// 800DE144
// The "picked up" tree (RunTree 10), then off its site and out of the world (location -1, -1 in
// tiles), its chair flag (data 8 0x100) cleared; a chair: the tile it faced re-checked; its container's
// "placed" tree; a support: its tile's flag off.
void PlainPickup(cXObjectImpl* o) {
  if (!o->IsMultiTile()) RunTreeEvent(o, 10);
  const int x = o->m_location[1] >> 4, y = o->m_location[0] >> 4, level = o->m_level;
  const int dir = o->GetData(1) & 7;
  cXObjectImpl* container = o->GetContainer() ? o->GetContainer()->GetObjectImplementation() : nullptr;
  PlaceSpec nowhere(true);
  ObjTestAndPlace(o, nowhere, true);
  o->m_data.WriteVar(2, 0);
  o->m_data.WriteVar(3, -1);
  const int out[2] = {-16, -16};
  SetLocation(o, out, 1);
  o->m_data.WriteVar(8, static_cast<short>(o->m_data[8] & ~0x100));
  if (o->IsChair()) UpdateChairFacing(x + kXDir[dir], y + kYDir[dir], level);
  if (container) RunTreeEvent(container, 9);
  if (IsSupportObject(o)) SetSupportFlag(x, y, level, false);
  if (o->m_instance) o->m_instance->SetObjOrient();
}

// 800E1E30
// The tile is on the lot and may be edited (error 1), the object can go there, at a tile's middle.
// GC: slots of types 5..7 refuse (0xB) unless the move-objects cheat.
int PlainUserCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  gPlacementConflict = nullptr;
  gPlacementError = 0;
  cFixedWorldImpl* w = World();
  CTilePt tile = TileOf(pos, 1);
  if (!w || w->OutOfBounds(tile) || !MayEditTile(tile)) {
    gPlacementError = 1;
    return 0;
  }
  if (!ObjCanPlace(o, pos, level, container, slot) || (pos[1] & 0xf) != 8 || (pos[0] & 0xf) != 8) return 0;
  if (container && !MoveObjectsCheat())
    SIMMY_STUB("cXObjectImpl::UserCanPlace: the slot's type (GetObjectSlot +8: 5..7 refused)", 0x800E1E30);
  return 1;
}
}  // namespace

// PC: UpdateChairFacingAt for other UI-side callers (HoodManager::ResetToPristine).
void ObjUpdateChairFacing(cXObjectImpl* o) { UpdateChairFacingAt(o); }

// 800DE340 cXObjectImpl::CanPlace / 800C6044 cXMTObjectImpl::CanPlace
// A multi-tile object tests every part at its offset from where the lead goes (not in a container:
// 0x10; the lead off the lot: 1); the first error counts (a later 0x1E/0x1F wins).
int ObjCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  PlacementGroup* g = PlacementGroupOf(o);
  PlacementGroup::Part* me = g ? PartOf(g, o) : nullptr;
  if (!me) return PlainCanPlace(o, pos, level, container, slot);
  if (container) {
    gPlacementError = 0x10;
    return 0;
  }
  int lead[2], leadLevel;
  LeadSite(*me, pos, level, lead, leadLevel);
  if (OutOfBounds(lead)) {
    gPlacementError = 1;
    return 0;
  }
  int error = 0;
  for (auto& p : g->parts) {
    const int at[2] = {lead[0] + p.offset[1] * 16, lead[1] + p.offset[0] * 16};
    if (!PlainCanPlace(p.object, at, leadLevel + p.offset[2], nullptr, slot) &&
        (error == 0 || gPlacementError == 0x1e || gPlacementError == 0x1f))
      error = gPlacementError;
  }
  if (error == 0) return 1;
  gPlacementError = error;
  return 0;
}

// 800DE3C4 cXObjectImpl::Place / 800C61E4 cXMTObjectImpl::Place
// Every part to its site (off the lot: picked up), then the models take their places (the group's
// middle is known only when all parts are there). GC: the dynamic group's merge (vtable 0x44/0x4C).
void ObjPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  PlacementGroup* g = PlacementGroupOf(o);
  PlacementGroup::Part* me = g ? PartOf(g, o) : nullptr;
  if (!me) {
    PlainPlace(o, pos, level, container, slot);
    return;
  }
  int lead[2], leadLevel;
  LeadSite(*me, pos, level, lead, leadLevel);
  for (auto& p : g->parts) {
    const int at[2] = {lead[0] + p.offset[1] * 16, lead[1] + p.offset[0] * 16};
    if (!OutOfBounds(pos)) PlainPlace(p.object, at, leadLevel + p.offset[2], container, slot);
    else PlainPickup(p.object);
  }
  for (auto& p : g->parts) SimObjectPlaced(p.object);
}

// 800DE144 cXObjectImpl::Pickup / 800C63E4 cXMTObjectImpl::Pickup: every part.
void ObjPickup(cXObjectImpl* o) {
  if (PlacementGroup* g = PlacementGroupOf(o)) {
    for (auto& p : g->parts) PlainPickup(p.object);
    return;
  }
  PlainPickup(o);
}

// 800E1E30 cXObjectImpl::UserCanPlace / 800C64B8 cXMTObjectImpl::UserCanPlace: every part.
int ObjUserCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  PlacementGroup* g = PlacementGroupOf(o);
  PlacementGroup::Part* me = g ? PartOf(g, o) : nullptr;
  if (!me) return PlainUserCanPlace(o, pos, level, container, slot);
  int lead[2], leadLevel;
  LeadSite(*me, pos, level, lead, leadLevel);
  for (auto& p : g->parts) {
    const int at[2] = {lead[0] + p.offset[1] * 16, lead[1] + p.offset[0] * 16};
    if (!PlainUserCanPlace(p.object, at, leadLevel + p.offset[2], nullptr, 0)) return 0;
  }
  return 1;
}

// 800E1FF4 cXObjectImpl::UserPlace / 800C66AC cXMTObjectImpl::UserPlace: placed, then the "user
// placed" tree (RunTree 0xB) of it, its parts and what they hold (PC: not run).
void ObjUserPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot) {
  ObjPlace(o, pos, level, container, slot);
}

// 800E1AF4 cXObjectImpl::UserCanPickup / 800C6824 cXMTObjectImpl::UserCanPickup
// Unless the move-objects cheat: the object must be movable (data 0x2B bit 1; else 0x25), not in use
// (data 0x3E < 1 and not occupied; else 0x26), and its tile editable (0x27).
int ObjUserCanPickup(cXObjectImpl* o) {
  if (PlacementGroup* g = PlacementGroupOf(o)) {
    for (auto& p : g->parts) {
      gPlacementConflict = nullptr;
      gPlacementError = 0;
      if (MoveObjectsCheat()) continue;
      if (!(p.object->GetData(0x2b) & 2)) return gPlacementError = 0x25, 0;
      if (p.object->GetData(0x3e) >= 1) return gPlacementError = 0x26, 0;
      CTilePt tile = p.object->GetCTilePt();
      if (!OutOfGrid(tile) && !MayEditTile(tile)) return gPlacementError = 0x27, 0;
    }
    return 1;
  }
  gPlacementConflict = nullptr;
  gPlacementError = 0;
  if (MoveObjectsCheat()) return 1;
  if (!(o->GetData(0x2b) & 2)) {
    gPlacementError = 0x25;
    return 0;
  }
  if (o->GetData(0x3e) >= 1) {  // GC: also IsOccupied (vtable 0x1E4) and the objects it holds
    gPlacementError = 0x26;
    return 0;
  }
  CTilePt tile = o->GetCTilePt();
  if (!OutOfGrid(tile) && !MayEditTile(tile)) {
    gPlacementError = 0x27;
    return 0;
  }
  return 1;
}

// 800E2128 cXObjectImpl::UserPickup / 800C68B8: the "user picked up" tree (RunTree 0xC, not run on
// PC), then picked up.
void ObjUserPickup(cXObjectImpl* o) { ObjPickup(o); }

// 800DFA9C cXObjectImpl::Turn / 800C5CE4 cXMTObjectImpl::Turn
// `steps` times the object's rotation step (data 0x17; 0: it does not turn). A multi-tile object turns
// all its parts round the lead (AssignOffsets), picked up and put back where it was if it fits there
// (else turned back). GC: chair facing and wall adjacency.
void ObjTurn(cXObjectImpl* o, int steps) {
  PlacementGroup* g = PlacementGroupOf(o);
  if (!g) {
    if (steps == 0 || o->GetData(0x17) == 0) return;
    o->m_data.WriteVar(1, static_cast<short>((o->GetData(1) + o->GetData(0x17) * steps) & 7));
    return;
  }
  cXObjectImpl* lead = g->parts[0].object;
  const int pos[2] = {lead->m_location[0], lead->m_location[1]};
  const int level = lead->m_level;
  ObjPickup(lead);
  const int delta = (lead->GetData(0x17) * steps) & 7;
  if (delta == 0) return;
  for (auto& p : g->parts) p.object->m_data.WriteVar(1, static_cast<short>((p.object->GetData(1) + delta) & 7));
  AssignOffsets(g);
  if (!ObjCanPlace(lead, pos, level, nullptr, 0) && !OutOfBounds(pos)) {
    for (auto& p : g->parts) p.object->m_data.WriteVar(1, static_cast<short>((p.object->GetData(1) - delta) & 7));
    AssignOffsets(g);
  }
  if (ObjCanPlace(lead, pos, level, nullptr, 0)) ObjPlace(lead, pos, level, nullptr, 0);
}

// 800DB744 cXObjectImpl::GetCurrentValue
// What it sells for: its price (data 0x29; a multi-tile object's lead's), half once it has been used
// (data 0xF of it or any part). GC: 0 for objects whose GUID is not known (GuidIsOk).
int ObjGetCurrentValue(cXObjectImpl* o) {
  PlacementGroup* g = PlacementGroupOf(o);
  if (!g) {
    const int price = o->GetData(0x29);
    return o->GetData(0xf) ? price / 2 : price;
  }
  bool used = false;
  for (auto& p : g->parts)
    if (p.object->GetData(0xf)) used = true;
  const int price = g->parts[0].object->GetData(0x29);
  return used ? price / 2 : price;
}

// 800E22C8 cXObjectImpl::GetWasPlacedFromInventory: object flags (data 8) bit 0x4000.
int ObjGetWasPlacedFromInventory(cXObjectImpl* o) { return (static_cast<unsigned short>(o->GetData(8)) >> 14) & 1; }

// 800E22F8 cXObjectImpl::SetWasPlacedFromInventory
void ObjSetWasPlacedFromInventory(cXObjectImpl* o, bool on) {
  const int f = o->m_data[8];
  o->m_data.WriteVar(8, static_cast<short>(on ? (f | 0x4000) : (f & ~0x4000)));
}
