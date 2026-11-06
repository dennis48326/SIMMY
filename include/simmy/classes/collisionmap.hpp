// @class Intersection after EVec2
// @class CollisionMap after Intersection TileWallsSegment
// Swept-circle collision for people walking under direct control (game/collisionmap.cpp,
// game/intersection.cpp): a circle moving from `from` to `to` against the walls of the tiles it
// crosses (the tile edges and diagonals TileWalls stores, blocked tiles) and against the footprints of
// the objects on them. Positions are in the world's frame (x = tile y, y = tile x).
#pragma once

// The first contact (0x14 bytes): where the circle's edge touches, the surface normal there, and how
// far along the move it happens (0..1; Reset: 1, no contact).
struct Intersection {
  EVec2 m_point;     // +0x00
  EVec2 m_normal;    // +0x08
  float m_t = 1.0f;  // +0x10
  void Reset() { m_t = 1.0f; }  // 8039DB10
};

struct CollisionMap {
  // The circle (radius `r`) moved from `from` to `to` against the walls; on contact `out` is where it
  // stops (or, sliding, the end of the move pushed back off the wall along its normal). Ghosts pass
  // walls and blocked tiles and keep away only from the lot's edge.
  static int DoesCollideEnvironment(EVec2& from, EVec2& to, float r, bool slide, bool ghost, EVec2& out);  // 80021948
  // Whether tile (x, y) cannot be entered across `segment`: off the lot, a blocked tile (flag 0x10;
  // not for ghosts), or a wall on that edge (not for ghosts; a door or window style 3, 5, 6, 0xF or
  // 0x17 lets people through).
  static int IsMoveBlocked(int x, int y, bool ghost, TileWallsSegment segment);  // 80022050
  // As DoesCollideEnvironment, against the footprints of the objects on the tiles crossed.
  static int DoesCollideObject(EVec2& from, EVec2& to, float r, bool slide, EVec2& out);  // 800221BC
};
