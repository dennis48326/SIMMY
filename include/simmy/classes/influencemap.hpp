// @class ObjectList
// @class InfluenceMap after ObjectList
// The interaction influence map (game/influencemap.cpp): per lot tile, the objects whose interactions
// can be started from it. An object's tree table entries each have an activation area (a custom one in
// the entry, +0x20, when its flags have 0x1000; else a tile round the object's footprint), turned by the
// object's direction; every tile of it that the object's location reaches without a wall
// (CollisionMap::DoesCollideEnvironment) lists the object. ObjectModuleImpl::UpdateInteractionInfluences
// builds it again when the house is rebuilt; cXObjectImpl::SetLocation takes an object out at its old
// place and puts it in at the new one. Its only reader in the release game is the direct-control
// interactor (DirectInteractor::UpdateObjectHighlights: the objects listed at the sim's tile); the area
// test also filters direct control's action menu (ObjTestSim::AppendInteractionsForMenu).
#pragma once
#include <vector>

// The objects listed at one tile (0x10 bytes: a vector of cXObject*), each once.
struct ObjectList {
  ObjectList();  // 80048570
  void AddObject(cXObject* object);  // 80048590
  void RemoveObject(cXObject* object);  // 80048714
  void RemoveAll();  // 80048798

  std::vector<cXObject*> m_objects;  // +0x00
};

// 0x0C bytes.
struct InfluenceMap {
  InfluenceMap(int xSize, int ySize);  // 80048844
  ~InfluenceMap();  // 800488BC
  void RemoveAll();  // 80048968
  // The tiles an entry (a tree table row, 0x24 bytes) of `object` can be started from, in 1/16 tiles:
  // rect[2]..rect[0] along the location's first axis, rect[3]..rect[1] along its second (end exclusive).
  void CalculateAffectedArea(cXObjectImpl* object, const unsigned char* entry, int rect[4]);  // 800489EC
  void AddObjectToMap(cXObjectImpl* object);  // 80048D44
  void AddObjectEntry(cXObjectImpl* object, const unsigned char* entry);  // 80048EA8
  void RemoveObjectFromMap(cXObjectImpl* object);  // 80049074
  void RemoveObjectEntry(cXObjectImpl* object, const unsigned char* entry);  // 800491CC
  ObjectList* GetObjectList(int row, int col);  // 80049294
  int ValidateMissing(cXObjectImpl* object);  // 800492E0
  void Validate();  // 800492E8
  // PC: the object out of every tile list (an object being destroyed; see ~cXObjectImpl).
  void ForgetObject(cXObject* object);

  int m_xSize = 0;                    // +0x00 rows (the location's second axis)
  int m_ySize = 0;                    // +0x04 columns (its first axis)
  std::vector<ObjectList> m_lists;    // +0x08 m_xSize * m_ySize, row by row
};

// PC: the influence map's upkeep from the port's location setters (cXObjectImpl::SetLocation, 800DF784):
// before the location changes the object leaves the map (not people); after it, it is put in again
// unless it is a person or held (in some container chain) by one.
void InfluenceMapObjectLeaving(cXObjectImpl* object);
void InfluenceMapObjectArrived(cXObjectImpl* object);
