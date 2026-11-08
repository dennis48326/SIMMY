// @class PassiveInfluenceItem
// @class PassiveInfluencePerson after PassiveInfluenceItem
// @class PassiveInfluenceObject after PassiveInfluenceItem
// @class PassiveInfluenceTarget after PassiveInfluenceItem
// @class PassiveInfluenceMap after PassiveInfluenceItem
// @class PassiveInfluenceSystem after PassiveInfluenceMap PassiveInfluencePerson PassiveInfluenceObject PassiveInfluenceTarget
// Passive influence (game/passiveinfluencemap.cpp): objects that change what is around them without
// an interaction. Primitive 0x41 (cXObjectImpl::TryPassiveInfluence) puts an object on the lot's map
// with a shape (a wedge, a half disc, a hexagon round it or a square in front of it) and a range in
// tiles, inside its own room; primitive 0x1D on an object (TrySetMotiveDelta) gives it motive deltas.
// A person standing on one of its tiles has those motives moved every frame (from the person's
// animator, AwarenessManager::PassiveInfluenceAwarenessCheck -> NotifySimulator), up to the delta's
// limit: a smelly bin or outhouse makes people near it less comfortable, a stereo playing raises their
// fun. Objects whose definition +0x0A is 0x1B, 0x2B or 0x3E are targets: once a simulator tick, an
// influencing object of their type on their tile runs their tree entry 0x0D with it as the stack
// object (CheckObjects).
#pragma once
#include <list>
#include <utility>
#include <vector>

// An influence on the map (GC vtable: 0x0C GetObject, 0x14 GetType, 0x1C GetArea, 0x24 the destructor).
struct PassiveInfluenceItem {
  virtual ~PassiveInfluenceItem() = default;
  virtual int GetType() const = 0;  // 0 a person influence, 1 an object influence, 2 a target
  cXObject* GetObject() const { return m_object; }
  std::vector<std::pair<int, int>>& GetArea() { return m_area; }

  cXObject* m_object = nullptr;               // +0x04
  std::vector<std::pair<int, int>> m_area;    // +0x08 the tiles it covers (x, y)
};

// One motive an influence moves (0x0C bytes): the delta a frame (at 30 frames a second) and the limit.
struct PassiveInfluenceMotiveDelta {
  int m_motive = 0;     // +0x00
  float m_delta = 0;    // +0x04
  float m_limit = 0;    // +0x08
};

// 0x30 bytes.
struct PassiveInfluencePerson : PassiveInfluenceItem {
  int GetType() const override { return 0; }
  signed char m_shape = 0;      // +0x18 0 a 60 degree wedge, 1 120, 2 a half disc, 3 round it, 4 a square in front
  signed char m_range = 0;      // +0x19 tiles (at most 0x20)
  signed char m_skillKind = 0;  // +0x1A the operand flags again (the awareness animation's kind)
  unsigned char m_flags = 0;    // +0x1B 1 not the person holding it, 2 not while a person holds it
  const AnimRef* m_skill = nullptr;  // +0x1C the reaction animation (object primitive 0x2C kinds 7/8)
  std::vector<PassiveInfluenceMotiveDelta> m_deltas;  // +0x20
};

// 0x1C bytes.
struct PassiveInfluenceObject : PassiveInfluenceItem {
  int GetType() const override { return 1; }
  signed char m_shape = 0;      // +0x18
  signed char m_range = 0;      // +0x19
  unsigned char m_flags = 0;    // +0x1A
  signed char m_targetType = 0; // +0x1B the targets it influences (their definition +0x0A)
};

// 0x24 bytes.
struct PassiveInfluenceTarget : PassiveInfluenceItem {
  // An influencing object it has run its tree for, and the ticks before it may again.
  struct Applied {
    cXObject* m_object = nullptr;
    int m_ticks = 0;
  };
  int GetType() const override { return 2; }
  signed char m_type = 0;       // +0x18 its definition +0x0A
  std::list<Applied> m_applied; // +0x20
};

// The lot's tiles, each with the influences covering it (0x0C bytes; cFixedWorldImpl +0x40).
struct PassiveInfluenceMap {
  PassiveInfluenceMap(int xSize, int ySize);  // 801133E8
  ~PassiveInfluenceMap();  // 80113470
  void AddItemToMap(PassiveInfluenceItem* item);  // 80113520
  void RemoveItemFromMap(PassiveInfluenceItem* item);  // 80113740
  std::vector<PassiveInfluenceItem*>* GetListForTile(int x, int y);  // 801138F8
  void EmptyMap();  // 80113944
  static PassiveInfluenceItem* NotifySimulator(cXPerson* person, int x, int y);  // 80113A58
  int CheckObjectInfluence(PassiveInfluenceTarget* target);  // 80113A8C
  int ApplyInfluence(PassiveInfluenceObject* influence, PassiveInfluenceTarget* target);  // 80113C68
  int ValidateMissing(PassiveInfluenceItem* item);  // 80113DA8
  void CalculateTargetArea(PassiveInfluenceItem* item);  // 801151C0
  void FilledArc_Line(int x0, int x1, int y, PassiveInfluenceItem* item);  // 80115428
  // The polygon of `sides` (1, 2 a wedge from the centre; 3 half a hexagon; 6 a hexagon) of radius
  // `radius` turned to `angle` degrees about tile (cx, cy): its vertices (tile units) and their count.
  void FilledArc_Polygon(int sides, int radius, int angle, int cx, int cy, int* count, float* verts);  // 80115700
  void FilledArc(int sides, int radius, int angle, int cx, int cy, PassiveInfluenceItem* item);  // 80115928
  void CalculateAffectedArea(PassiveInfluenceItem* item);  // 80115BD8
  void Validate();  // 8011623C

  static PassiveInfluenceMap* s_passiveInfluenceMap;

  int m_xSize = 0;                                       // +0x00
  int m_ySize = 0;                                       // +0x04
  std::vector<std::vector<PassiveInfluenceItem*>> m_tiles;  // +0x08 by x * ySize + y
};

// The simulator's influences (0x18 bytes; cSimulatorImpl +0x174).
struct PassiveInfluenceSystem {
  PassiveInfluenceSystem();  // 80113DB0
  ~PassiveInfluenceSystem();  // 80113E48
  void FlushSystem();  // 80113F04
  void DoStream(ReconBuffer* buffer, int version);  // 80113F3C
  int AddObjectToMap(cXObject* object, signed char skillKind, signed char shape, signed char range,
                     signed char flags);  // 80113F40
  int RemoveObjectFromMap(cXObject* object);  // 8011416C
  void MoveObjectInWorld(cXObject* object);  // 80114344
  PassiveInfluenceItem* NotifySimulator(cXPerson* person, int x, int y);  // 80114704
  int CheckObjects(int ticks);  // 80114B9C
  int SetInfluenceMotiveDelta(cXObject* object, unsigned char motive, float delta, short limit);  // 80114E1C
  int ClearInfluenceMotiveDeltas(cXObject* object);  // 801150A0
  bool SetSkillNameID(cXObject* object, const AnimRef* skill);  // 8011513C
  // PC: an object being freed stops being a target and is forgotten by the targets it influenced (GC:
  // cXObjectImpl::SetLocation's MoveObjectInWorld drops a target when its object leaves the world, which
  // a killed object does first; the port's SetLocation does not call it). Called by ~cXObjectImpl.
  void ForgetObject(cXObject* object);

  static PassiveInfluenceSystem* s_passiveInfluenceSystem;

  std::list<PassiveInfluencePerson*> m_persons;  // +0x00
  std::list<PassiveInfluenceObject*> m_objects;  // +0x08
  std::list<PassiveInfluenceTarget*> m_targets;  // +0x10
};

// PC stand-in for the passive-influence part of AwarenessManager::AwarenessCheck (80012140) ->
// PassiveInfluenceAwarenessCheck (80013D4C), run by SAnimator2::Update each frame: the influences on
// the person's tile move its motives. (The rest of the awareness manager is not ported.)
void PassiveInfluenceAwarenessCheck(SAnimator2* animator);

// The end of cXObjectImpl::SetLocation (800DF784; PC: the port's several location setters call it): an
// object whose FTilePt changed moves on the passive influence map.
void PassiveInfluenceLocationChanged(cXObjectImpl* object, int oldX, int oldY);
