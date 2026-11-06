// @class RoutingSlot after EVec2
// @class RoutingSlot__VerticalPosition
// @class RouteGoal
// @class TileList
// @class XRoute
// Routing (game/slots.cpp RoutingSlot, game/xrouting.cpp XRoute). A routing slot (an object's SLOT
// entry of type 3) says where around an object a person stands to use it: an offset from the object
// (1/16 tiles, in the object's frame), the directions around it that are allowed (flags bits 0..7,
// eighths of a turn from the object's facing), the distances (min, max, optimal, 1/16 tiles), the
// step between candidate spots (resolution), how the person faces there, and score multipliers for
// standing and sitting. XRoute turns a slot into goals (ConstructGoals: candidate spots scored by
// their distance from the optimal one, checked against the world: in bounds, the object's room, no
// wall between, room to stand) and then a path to the best goal it can reach (FindPath).
// GC: FindPath searches the room partition (BuildRoomPartition: free rectangles between walls and
// objects, SpacePartition/Path in game/routing.cpp, an A* over rectangles with smoothing). PC: an A*
// over a quarter-tile grid of the lot (walls, blocked tiles and object footprints, the target object
// left out), smoothed by line of sight; see xrouting.cpp.
#pragma once
#include <vector>

struct RoutingSlot__VerticalPosition {
  SIMMY_ENUM_BODY(RoutingSlot__VerticalPosition)
};

struct RoutingSlot {
  using VerticalPosition = ::RoutingSlot__VerticalPosition;
  // Slot flags (+0x1C).
  enum : unsigned int {
    kDirectionMask = 0xff, kAnyRotation = 0x100, kAbsolute = 0x200, kIgnoreRooms = 0x800,
    kSnapToDirection = 0x1000, kRandomScoring = 0x2000, kFailureTrees = 0x4000, kDifferentAlts = 0x8000,
    kAverageLocation = 0x10000
  };
  // Facings (+0x34): an eighth of a turn from the object's facing, or one of these.
  enum : int { kFaceAway = -1, kFaceTowards = -2, kFaceAnywhere = -3 };
  RoutingSlot();  // 80142994
  RoutingSlot(SlotDescriptor *);  // 80142A28
  // PC: from a quickdata SlotDescriptor (0x48 bytes, big-endian; the GC ctor copies its fields).
  static RoutingSlot FromDescriptor(const unsigned char* desc);
  virtual void WriteSlot(SlotDescriptor *);  // 80142AD8
  void SetIsOnTopOfObject();  // 80142B54
  void AllowDirection(int dir);  // 80142B64
  void AllowAnyRotation();  // 80142B84
  void AllowAnyFacing();  // 80142B94
  void FaceTowardsObject();  // 80142BA0
  void FaceAwayFromObject();  // 80142BAC
  void SetFacingDirection(int dir);  // 80142BB8
  void SetDistances(int minDist, int maxDist, int optimal);  // 80142BC8
  void SetTileDistances(float minDist, float maxDist, float optimal);  // 80142BD8
  void SetMultiplier(int position, int value);  // 80142C30
  virtual ~RoutingSlot();  // 8014548C
  bool IsOnTopOfObject() const;  // 801454C0
  RET_UNKNOWN IsDirectionAllowed(int) const;  // 801454D0
  RET_UNKNOWN IsAnyRotationAllowed() const;  // 801454EC
  bool IsAnyFacingAllowed() const;  // 80145504
  bool IsFacingTowardsObject() const;  // 80145518
  bool IsFacingAwayFromObject() const;  // 8014552C
  RET_UNKNOWN GetFacingDirection() const;  // 80145540
  RET_UNKNOWN GetIgnoreRooms() const;  // 80145548
  void SetIgnoreRooms(bool);  // 80145560
  RET_UNKNOWN GetMinDist() const;  // 80145580
  RET_UNKNOWN GetMaxDist() const;  // 80145588
  RET_UNKNOWN GetOptimalDist() const;  // 80145590
  RET_UNKNOWN GetResolution() const;  // 80145598
  double GetGradient() const;  // 801455A0
  RET_UNKNOWN GetMultiplier(RoutingSlot__VerticalPosition) const;  // 801455A8
  RET_UNKNOWN GetSnapTargetSlot() const;  // 801455B8
  RET_UNKNOWN SnapsToDirection() const;  // 801455C0
  RET_UNKNOWN GetSnapDirection() const;  // 801455D8
  RET_UNKNOWN Absolute() const;  // 80145604
  RET_UNKNOWN HasRandomScoring() const;  // 80145610
  void SetHasRandomScoring(bool);  // 8014561C
  void SetAllowFailureTrees(bool);  // 8014563C
  RET_UNKNOWN GetAllowFailureTrees() const;  // 8014565C
  void SetAllowDifferentAlts(bool);  // 80145668
  RET_UNKNOWN GetAllowDifferentAlts() const;  // 80145688
  void SetUseAverageObjectLocation(bool);  // 80145694
  RET_UNKNOWN GetUseAverageObjectLocation() const;  // 801456B4
  void SetResolution(int);  // 801456C0
  double GetXOffset() const;  // 801456C8
  double GetYOffset() const;  // 801456D0
  double GetAltOffset() const;  // 801456D8

  float m_x = 0.0f;              // +0x04 the offset in the object's frame (1/16 tiles)
  float m_y = 0.0f;              // +0x08
  float m_alt = 0.0f;            // +0x0C
  int m_multiplier[3] = {};      // +0x10 the scores standing, sitting, on the ground (x10)
  unsigned int m_flags = 0;      // +0x1C
  int m_snapTarget = -1;         // +0x20
  int m_minDist = 0x10;          // +0x24 1/16 tiles
  int m_maxDist = 0x10;          // +0x28
  int m_optimalDist = 0x10;      // +0x2C
  float m_gradient = 0.1875f;    // +0x30 score lost per 1/16 tile off the optimal distance
  int m_facing = kFaceTowards;   // +0x34
  int m_resolution = 0x10;       // +0x38 between candidate spots (1/16 tiles)
};

// A place a route may end (0x10 bytes): where (FTilePt, 1/16 tiles: [0] = +0x68, [1] = +0x6C of an
// object's location), its score, the seat object there (sitting goals), the direction bit it came from.
struct RouteGoal {
  int m_pos[2] = {};             // +0x00
  int m_score = 0;               // +0x08
  short m_object = 0;            // +0x0C
  unsigned short m_dirMask = 0;  // +0x0E
  // PC: how the person faces there (eighths of a turn; -1 as it arrives; GC: SetDirectionForGoalSearch
  // sets it on the person while the goals are scored).
  int m_facing = -1;
};

// GC: a list of tiles and points (the route's way points); PC: points in the animation format
// (x = location +0x6C, y = +0x68, 1/16 tiles).
struct TileList {
  TileList() = default;
  RET_UNKNOWN FindNearestPoint(FTilePt *, int);  // 80157C44
  std::vector<EVec2> m_points;
};

struct XRoute {
  XRoute();  // 80152D18
  XRoute(cXObject* person, cXObject* object, RoutingSlot* slot);  // 80152D74
  // The person routing to `slot` of `object` (null: no slot).
  void Construct(cXObject* person, cXObject* object, RoutingSlot* slot);  // 80152DDC
  // One goal at `pos` (FTilePt) facing `dir` (eighths): 0 off the lot, 1 an object in the way (its id in
  // m_blocker), 2 another room, 3 a wall between it and the object, 5 a person in the way, 6 good.
  int EvalTileForGoal(const int pos[2], int dir);  // 8015282C
  RET_UNKNOWN HasCurrentGoal();  // 80152F5C
  int CountGoals() { return static_cast<int>(m_goals.size()); }  // 80152F74
  RouteGoal* GetNthGoal(int n) { return &m_goals[static_cast<size_t>(n)]; }  // 80152F88
  RET_UNKNOWN GetMaxScore();  // 80152F98
  void SetCurrentGoal(int);  // 80152FE8
  void ClearCurrentGoal();  // 80152FF0
  RoutingSlot* GetRoutingSlot() { return &m_slot; }  // 8015303C
  void AddGoal(RouteGoal& goal);  // 80153044
  RET_UNKNOWN GetCurrentGoal();  // 80153248
  // A goal is a seat (goal object) and `person` sits in it.
  bool IsPersonSittingOnChairGoal(cXObject* person);  // 8015325C
  RET_UNKNOWN ShouldIgnore(cXObject *);  // 80153328
  void DoStream(ReconBuffer *, int);  // 801534C8
  void ResetGoals();  // 801534CC
  // The start (ChooseStartingPoint), then the goals (ConstructGoals) with the person's facing kept.
  void BuildGoalList();  // 8015597C
  // The way to the best goal it can reach, in `out` (the start first, the goal last; m_goal says
  // which). 1: found. PC: see the header comment.
  int FindPath(TileList& out);  // 80155AC8
  void ChooseStartingPoint();  // 80158058
  void ConstructGoals();  // 80158318

  std::vector<RouteGoal> m_goals;   // +0x00
  RoutingSlot m_slot;               // +0x10 a copy
  cXObject* m_object = nullptr;     // +0x4C the object routed to (null: a place)
  cXObject* m_person = nullptr;     // +0x50
  int m_goal = -1;                  // +0x54 the goal the path leads to
  int m_maxScore = -1;              // +0x58
  int m_personPos[2] = {};          // +0x64 where the person stood when it began
  int m_start[2] = {};              // +0x70 where the route starts (ChooseStartingPoint)
  int m_startDirs = 0;              // +0x78
  int m_first = 1;                  // +0x7C its first tick
  int m_failure = 0;                // +0x8C why it failed (the tree's "route failure" code)
  short m_blocker = 0;              // +0x90 the object in the way
  int m_maxGoals = 0x30;            // +0xA0
};

// The object's routing slots (GC: cXObjectImpl +0xA0, loaded with its slots by SlotLoader::Load from the
// definition's slot list +0x28; ids from 1000 are the semi-global file's).
const std::vector<RoutingSlot>& ObjectRoutingSlots(cXObjectImpl* object);
// The flags of `object`'s object slot `index` (type 0 entries of its slot list), -1 for none.
int ObjectSlotFlags(cXObjectImpl* object, int index);
// The slot list descriptor of `object`'s object slot `index` (ObjectSlot::ObjectSlot), or null.
const unsigned char* ObjectSlotDescriptor(cXObjectImpl* object, int index);
// The number of object slots of `object` (its slot list's type 0 entries).
int ObjectSlotCount(cXObjectImpl* object);
// An object slot's standard height and altitude (ObjectSlot::SetHeight) from its descriptor.
void ObjectSlotHeight(const unsigned char* descriptor, int& height, float& alt);
// The global routing slots (the global object file's slot list 100; GC ObjectModuleImpl +0x2078).
const std::vector<RoutingSlot>& GlobalRoutingSlots();
// Where `slot` of `object` is (FTilePt; GC: TrySnap's object rotation and iso frame).
void RoutingSlotPosition(cXObjectImpl* object, const RoutingSlot& slot, int out[2]);
// PC stand-in for cXObjectImpl::CanPlace (vtable 0x134) of a person at `pos` (FTilePt): on the lot, no
// wall within reach, no object's footprint but `near`'s group and `person` itself, no other person.
bool PersonCanStandAt(cXObject* person, cXObject* near, const int pos[2]);
