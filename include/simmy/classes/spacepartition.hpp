// @class IPoint
// @class IRect
// @class PenaltyRect after IRect
// @class RoutingParams after IPoint PenaltyRect
// @class ASTNode after IPoint
// @class SpacePartition after ASTNode RoutingParams
// @class Path after SpacePartition
// The router's search (game/routing.cpp, game/irect.cpp). XRoute::FindPath (xrouting.cpp) describes the
// room the person routes in as rectangles with penalties (PenaltyRect, 1/16 tiles): the room's walls
// and the tiles that are not the room (BuildRoomPartition, kept by the Room), the objects and people in
// it (grown by half a person's width), and the goals (one rectangle per goal, its penalty how much
// worse it is than the best). SpacePartition splits the free space into rectangles on demand: the one
// round the start, each goal's, and those its search reaches (ExpandRect grows a rectangle until it
// touches a blocking one; BuildSpatialSuccessorList walks round a rectangle's outside to find or make
// its neighbours). Path runs an A* over nodes (a goal and a free rectangle; node 0 the start, node 1 the
// end every goal leads to), then smooths the way points across the shared edges (DoOneSmooth).
// Rectangles are half-open: [top, bottom) x [left, right); a point is (x, y) = (left/right, top/bottom).
// In the lot's terms x is an FTilePt's [1] (the CTilePt x) and y its [0].
#pragma once
#include <vector>

struct IPoint {
  int x = 0;  // +0x00
  int y = 0;  // +0x04
};

struct IRect {
  int top = 0;     // +0x00
  int left = 0;    // +0x04
  int bottom = 0;  // +0x08
  int right = 0;   // +0x0C
  // This rectangle lies inside `outer`.
  RET_UNKNOWN IsContained(IRect* outer) const;  // 800A5A98
  // This becomes the overlap of `a` and `b` (1), or all zero when they do not overlap (0).
  RET_UNKNOWN Intersect(IRect* a, IRect* b);  // 800A5AE4
};

// 0x14 bytes.
struct PenaltyRect : IRect {
  enum : int { kBlocked = 0x7fffffff };
  PenaltyRect() = default;
  PenaltyRect(IRect* r, int penalty);  // 8013896C
  PenaltyRect(int left, int top, int right, int bottom, int penalty);  // 80138998
  int penalty = 0;  // +0x10 kBlocked: cannot be crossed
};

// What FindPath asks the search (0x38 bytes, on its stack).
struct RoutingParams {
  std::vector<PenaltyRect>* goals = nullptr;      // +0x00 one per goal
  std::vector<PenaltyRect>* obstacles = nullptr;  // +0x04 the objects and people
  std::vector<PenaltyRect>* partition = nullptr;  // +0x08 the room's walls and outside (or null)
  IPoint start;                                   // +0x0C
  int noGoalCentre = 0;                           // +0x14 the path does not end at the goal's centre
  int field18 = 0;                                // +0x18
  int field1C = 0;                                // +0x1C
  int limitAspect = 0;                            // +0x20 free rectangles at most maxAspect:1
  int maxAspect = 0;                              // +0x24
  int limitSize = 0;                              // +0x28 free rectangles at most maxSize a side
  int maxSize = 0;                                // +0x2C
  int defaultPenalty = 0;                         // +0x30 of free space no partition rectangle covers
  int smooth = 0;                                 // +0x34 smooth the path
};

// A search node (0x2C bytes): a goal and a free rectangle.
struct ASTNode {
  int goal = -1;           // +0x00 RoutingParams::goals index (-1: none)
  int free = -1;           // +0x04 SpacePartition free rectangle (-1: the end node)
  int succCount = -1;      // +0x08 -1 until CountSuccessors
  int succStart = -1;      // +0x0C first in SpacePartition's successor list
  int penalty = 0;         // +0x10
  IPoint pt;               // +0x14 where the path crosses into it
  int parent = -1;         // +0x1C
  float f = 0.0f;          // +0x20 g + h
  float g = 0.0f;          // +0x24 cost so far
  float h = 0.0f;          // +0x28 estimate to the goal
};

// 0x5C bytes (GC: one, Path::fSpacePartition).
struct SpacePartition {
  SpacePartition();  // 80138C20
  void FindInterfaceRect(int a, int b, IRect* out);  // 80138A3C
  RET_UNKNOWN FindInterfaceRect(ASTNode* a, ASTNode* b, IRect* out);  // 80138ABC
  IPoint FindInterfacePoint(ASTNode* a, ASTNode* b);  // 80138B98
  int GetIntersectingFreeRect(IRect* r);  // 80138C94
  PenaltyRect* GetIntersectingPartitionRect(IRect* r);  // 80138D18
  ASTNode* GetNode(int n);  // 80138E30
  int GetNodeRectangle(int n, IRect* out);  // 80138E70
  bool IsSpatialNode(int n);  // 80138F1C
  double EstimateDistanceToGoal(int n);  // 80138F30
  double MeasureDistance(int from, int to, IPoint* pt);  // 80139070
  void GetTerminals(int* start, int* goal);  // 80139250
  int CountSuccessors(int n);  // 80139264
  int GetNthSuccessor(int n, int i);  // 801397D0
  void Clear();  // 80139844
  void Deallocate();  // 801398BC
  int ExpandRect(PenaltyRect* r);  // 80139C60
  void BuildSpatialSuccessorList(int n);  // 8013A130
  void Init(RoutingParams* params);  // 8013C0EC

  RoutingParams* m_params = nullptr;              // +0x00
  std::vector<PenaltyRect>* m_obstacles = nullptr;  // +0x04
  std::vector<PenaltyRect>* m_partition = nullptr;  // +0x08
  std::vector<PenaltyRect> m_free;                // +0x0C free rectangles found so far
  std::vector<int> m_goalNodes;                   // +0x1C per goal: its node in its own rectangle
  std::vector<int> m_entryNodes;                  // +0x2C per goal: its node in the start's rectangle
  std::vector<int> m_successors;                  // +0x3C
  std::vector<ASTNode> m_nodes;                   // +0x4C
};

// 0x70 bytes (FindPath's stack).
struct Path {
  Path() = default;
  RET_UNKNOWN InitAST();  // 8013ACAC
  // One A* step: 1 done (the path is in m_points, or none was found), 0 more to do.
  RET_UNKNOWN OpenANode();  // 8013AE78
  // One smoothing pass: true when nothing moved at the finest step.
  bool DoOneSmooth();  // 8013BC20
  void InitPath(RoutingParams* params);  // 8013D6AC
  RET_UNKNOWN FindSmallestOpenNode();  // 8013D750
  static void DeallocateAllPathData();  // 8013D7FC

  static SpacePartition fSpacePartition;

  RoutingParams* m_params = nullptr;  // +0x00
  std::vector<int> m_chain;           // +0x04 the nodes from the end back to the start
  std::vector<int> m_pathNodes;       // +0x14 the spatial nodes along it, start first
  std::vector<IPoint> m_points;       // +0x24 the way points
  int m_state = 0;                    // +0x34 FindPath's: 0 done, 1 searching, 2 smoothing
  int m_steps = 0;                    // +0x38
  std::vector<int> m_open;            // +0x3C
  std::vector<int> m_closed;          // +0x4C
  int m_start = 0;                    // +0x5C
  int m_goal = 1;                     // +0x60
  int m_current = 0;                  // +0x64
  int m_goalRect = -1;                // +0x68 the goal the path reaches
  int m_smoothLevel = 0;              // +0x6C
};

// The first rectangle of `rects` overlapping `r`, -1 for none.
int FindIntersectingRect(IRect* r, std::vector<PenaltyRect>* rects);  // 801389B4
// `inner` lies inside `outer` (true when outer is null).
int IsRectInside(IRect* inner, IRect* outer);  // 80139C04
// Whether going a -> b -> c is shorter than *best; if so it becomes the best and *out = b.
bool EvalPoint(IPoint a, IPoint b, IPoint c, IPoint* out, float* best);  // 8013BA9C
