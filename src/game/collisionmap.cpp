// Port of game/collisionmap.cpp: a person's circle swept against the lot's walls and objects (direct
// control, SAnimator2::CheckCollision). The world's frame is used throughout (x = tile y, y = tile x);
// the object test works in tile axes and swaps back.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>

namespace {
// The contact `best` gives the move from `from` to `to`: where it stops, or (sliding) the end of the
// move pushed back off the surface along its normal (n, in the frame the move is in).
EVec2 Resolve(const EVec2& from, const EVec2& to, const Intersection& best, float nx, float ny, bool slide) {
  const float dx = to.x - from.x, dy = to.y - from.y;
  if (!slide) return EVec2{from.x + dx * best.m_t, from.y + dy * best.m_t};
  const float push = -dx * nx + -dy * ny;
  const float k = push * (1.0f - best.m_t) * 1.01f;
  return EVec2{to.x + nx * k, to.y + ny * k};
}

// cXObjectImpl::ComputeRect (GC 800DF4D0), for the object at its own location: its footprint (a tile,
// GetTileWidth 16 in 1/16 tiles) around the location, less the definition's insets (+0x9A, a nibble
// per side) turned by its direction (data 1, eighths of a turn). In 1/16 tiles: [0] y + half,
// [1] x + half, [2] y - half, [3] x - half (FTilePt is stored y first). GC: people (type 2) also
// take their person data 0x49 off the insets.
void ObjectRect(cXObjectImpl* object, int rect[4]) {
  const int half = 16 / 2;
  const int y = object->m_location[0], x = object->m_location[1];
  rect[3] = x - half;
  rect[2] = y - half;
  rect[1] = x + half;
  rect[0] = y + half;
  const unsigned int insets = static_cast<unsigned short>(ERQuickdata::S16(object->m_def, 0x9a));
  if (!insets) return;
  const int side[4] = {static_cast<int>(insets & 0xf), static_cast<int>(insets >> 4 & 0xf),
                       static_cast<int>(insets >> 8 & 0xf), static_cast<int>(insets >> 12 & 0xf)};
  const int q = (8 - object->GetData(1) % 8) / 2;
  auto at = [&](int i) { return side[((i % 4) + 4) % 4]; };
  rect[2] += at(q);
  rect[1] -= at(q + 1);
  rect[0] -= at(q + 2);
  rect[3] += at(q + 3);
}
}  // namespace

// 80021948
// Every tile the move's bounds (grown by r) touch: the edges crossed in the direction of the move
// where IsMoveBlocked says so, and (on the lot, not for ghosts) the tile's diagonal wall from either
// side; the earliest contact decides.
int CollisionMap::DoesCollideEnvironment(EVec2& from, EVec2& to, float r, bool slide, bool ghost, EVec2& out) {
  const float dx = to.x - from.x, dy = to.y - from.y;
  const float yHi = std::max(from.y, to.y) + r, yLo = std::min(from.y, to.y) - r;
  const float xLo = std::min(from.x, to.x), xHi = std::max(from.x, to.x);
  const int y0 = static_cast<int>((xLo - r) + 0.01f);
  const int y1 = static_cast<int>((xHi + r) - 0.01f) + 1;
  EVec2 move{dx, dy};
  Intersection best, ix;
  int found = 0;
  auto test = [&](EVec2 a, EVec2 b, bool twoSided) {
    if (GetCircleSegmentClosedIX(from, r, move, a, b, ix, twoSided)) {
      found = 1;
      if (ix.m_t < best.m_t) best = ix;
    }
  };
  const int xEnd = static_cast<int>(yHi - 0.01f) + 1;
  for (int tx = static_cast<int>(yLo + 0.01f) - 1; tx <= xEnd; ++tx) {
    const float fx = static_cast<float>(tx), fx1 = static_cast<float>(tx + 1);
    for (int ty = y0 - 1; ty <= y1; ++ty) {
      const float fy = static_cast<float>(ty), fy1 = static_cast<float>(ty + 1);
      if (dx <= 0.0f) {
        if (IsMoveBlocked(tx, ty, ghost, TileWallsSegment(8))) test(EVec2{fy1, fx}, EVec2{fy1, fx1}, false);
      } else if (IsMoveBlocked(tx, ty, ghost, TileWallsSegment(2))) {
        test(EVec2{fy, fx1}, EVec2{fy, fx}, false);
      }
      if (dy <= 0.0f) {
        if (IsMoveBlocked(tx, ty, ghost, TileWallsSegment(4))) test(EVec2{fy1, fx1}, EVec2{fy, fx1}, false);
      } else if (IsMoveBlocked(tx, ty, ghost, TileWallsSegment(1))) {
        test(EVec2{fy, fx}, EVec2{fy1, fx}, false);
      }
      CTilePt tile(tx, ty, 0);
      if (!Globs_pFixedWorld->OutOfBounds(tile) && !ghost) {
        TileWalls walls = Globs_pFixedWorld->GetWall(tile);
        if (walls.HasWall(TileWallsSegment(TileWallsSegment::kDiagonal1))) test(EVec2{fy, fx1}, EVec2{fy1, fx}, true);
        else if (walls.HasWall(TileWallsSegment(TileWallsSegment::kDiagonal2))) test(EVec2{fy, fx}, EVec2{fy1, fx1}, true);
      }
    }
  }
  if (found) out = Resolve(from, to, best, best.m_normal.x, best.m_normal.y, slide);
  return found;
}

// 80022050
int CollisionMap::IsMoveBlocked(int x, int y, bool ghost, TileWallsSegment segment) {
  CTilePt tile(x, y, 0);
  if (!Globs_pFixedWorld || Globs_pFixedWorld->OutOfBounds(tile)) return 1;
  if (Globs_pFixedWorld->GetFlags(tile) & 0x10) return ghost ? 0 : 1;
  if (ghost) return 0;
  TileWalls walls = Globs_pFixedWorld->GetWall(tile);
  if (!walls.HasWall(segment)) return 0;
  const int style = walls.GetStyle(segment);
  const bool portal = style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17;
  return portal ? 0 : 1;
}

// 800221BC
// The objects on every tile the move's bounds (grown by r) touch, each object of a tile's chain that
// people bump into (not CanIntersectPeople, in the world, not a person, not a door): the circle
// against its rectangle (ComputeRect). In tile axes (x, y), then back.
int CollisionMap::DoesCollideObject(EVec2& from, EVec2& to, float r, bool slide, EVec2& out) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return 0;
  EVec2 c{from.y, from.x}, end{to.y, to.x};
  EVec2 move{end.x - c.x, end.y - c.y};
  const float xLo = std::min(c.x, end.x) - r, xHi = std::max(c.x, end.x) + r;
  const float yLo = std::min(c.y, end.y) - r, yHi = std::max(c.y, end.y) + r;
  Intersection best, ix;
  int found = 0;
  for (int tx = static_cast<int>(xLo); tx <= static_cast<int>(xHi); ++tx) {
    for (int ty = static_cast<int>(yLo); ty <= static_cast<int>(yHi); ++ty) {
      CTilePt tile(tx, ty, 0);
      // GC: then the tile object's slot children (GetNextObjectSibling, vtable 0x2FC = HierGetChild);
      // PC: slots are not read, so the tile's object alone.
      if (cXObject* o = module->GetObjectFromID(module->GetTileObjectID(tile))) {
        cXObjectImpl* impl = o->GetObjectImplementation();
        if (!impl || !impl->m_def) continue;
        // CanIntersectPeople (vtable 0x47C, 800EB6C4): a zero-extent object (flag 4) unless its flag 2
        // is set, any other with flag 0x10; IsInWorld (0x1AC); IsPerson (0x4AC); IsDoor (0x54: a portal,
        // type 8).
        const int flags = impl->GetData(8);
        const bool passThrough = ((flags >> 2) & 1) ? (((flags ^ 2) >> 1) & 1) != 0 : ((flags >> 4) & 1) != 0;
        const bool inWorld = (impl->m_location[1] >> 4) > 0 && (impl->m_location[0] >> 4) > 0;
        const int type = objdef::Type(impl->m_def);
        if (passThrough || !inWorld || type == 2 || type == 8) continue;
        int rect[4];
        ObjectRect(impl, rect);
        EVec2 p0{rect[3] * 0.0625f, rect[2] * 0.0625f}, p1{rect[1] * 0.0625f, rect[0] * 0.0625f};
        if (GetCircleRectangleIX(c, r, move, p0, p1, ix)) {
          found = 1;
          if (ix.m_t < best.m_t) best = ix;
        }
      }
    }
  }
  // The normal back in the world's frame.
  if (found) out = Resolve(from, to, best, best.m_normal.y, best.m_normal.x, slide);
  return found;
}
