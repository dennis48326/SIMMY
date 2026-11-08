// @class cXMTObjectImpl after cXObjectImpl
// A part of a multi-tile object (game/mtobject.cpp). Each tile of a multi-tile object is its own
// object (its definition's master id names the group, its sub index the tile: x = sub >> 8, y = its
// low byte); the parts are chained from the group's lead (the first part made), and every part knows
// the lead. Trees reach the group through the chain (SetToNext kind 3, the interaction leader, the
// partner placement of FindGoodLocation mode 5).
//
// GC: cXMTObjectImpl is its own class next to a cXObjectImpl (+0x00 the object, +0x04 its cXMTObject
// interface) with the chain at +0x08 (next part) and +0x0C (the lead; 0 for the lead itself), its
// offsets from the lead as defined at +0x10/+0x14/+0x18 (x, y, level) and turned by the lead's
// direction at +0x1C/+0x20/+0x24 (AssignOffsets). PC: it derives from cXObjectImpl, so a part is one
// object; the links point at the parts' cXMTObjectImpls directly. Portals (doors, windows; GC
// cXPortalImpl, a cXMTObjectImpl with the portal's data) are cXMTObjectImpls too.
#pragma once
#include <vector>

struct cXMTObjectImpl : public cXObjectImpl {
  cXMTObjectImpl(int, ObjSelector* selector, cXMTObjectImpl* lead, ObjectModule* module);  // 800C3684
  ~cXMTObjectImpl() override;  // 800C489C
  // The offsets from the definition (ObjDefinition::GetMultiTileOffsets) and the level offset (+0x70).
  void Initialize() override;  // 800C5848
  // Joins `lead`'s chain at its end (none: it is a lead itself).
  void SetLeader(cXMTObjectImpl* lead);  // 800C58CC
  // Leaves the chain (a lead hands the group to its next part).
  void RemoveFromChain();  // 800C5BAC
  // The parts' offsets turned by the lead's direction (data 1).
  void AssignOffsets();  // 800C5F10
  // `other` is a part of my group.
  bool IsPartOfMe(cXObject* other);  // 800C6AD0
  static int ReconType();  // 800C6C50
  // The object's stream, then the part's offsets and its lead's id (see the .cpp).
  void ReconStream(ReconBuffer* buffer, int version, bool resize) override;  // 800C6C5C
  void PostLoad(int version, bool) override;  // 800C6DAC
  // The tiles the group covers, from the parts' sub indices.
  void GetNumTiles(int& x, int& y);  // 800C798C
  // The group's lead (as an object), and the part after me (none: I am the last).
  cXMTObjectImpl* GetFirstMultiTileObject();  // 800C7D14
  cXMTObjectImpl* GetNextMultiTileObject();  // 800C7D44

  // Not ported (generated declarations):
  RET_UNKNOWN GetISimInstance();  // 800C5958
  void Turn(int);  // 800C5CE4
  void CanPlace(FTilePt &, int, cXObject *, int);  // 800C6044
  RET_UNKNOWN Place(FTilePt &, int, cXObject *, int);  // 800C61E4
  RET_UNKNOWN Pickup();  // 800C63E4
  RET_UNKNOWN UserCanPlace(FTilePt &, int, cXObject *, int);  // 800C64B8
  void UserPlace(FTilePt &, int, cXObject *, int);  // 800C66AC
  RET_UNKNOWN UserCanPickup();  // 800C6824
  void UserPickup(bool);  // 800C68B8
  RET_UNKNOWN UserCanDelete();  // 800C6A3C
  void Reset(bool) override;  // 800C6B10
  void SetMultiObjectData(int, int);  // 800C6E44
  void MergeDynamic(cXMTObjectImpl* other);  // 800C6F2C (GC: its cXMTObject interface)
  bool IsDynamic();  // 800C71F0
  void RemoveFromDynamic();  // 800C7240
  void UpdateAllAdjacecy();  // 800C7364
  void UpdateDynAdjacency();  // 800C7434
  void MergeInPlace();  // 800C7690
  RET_UNKNOWN GetAverageLocation() const;  // 800C7884
  static void GetMTObjectImplementation();  // 800C7D5C
  void GetISimInstanceBaseVer();  // 800C7D60
  RET_UNKNOWN CAST_IMPL();  // 800C7D88

  // PC: the chain is made from the world when it is first needed (see LinkMultiTileGroup).
  void EnsureLinked();

  cXMTObjectImpl* m_nextPart = nullptr;  // +0x08 the next part of the group
  cXMTObjectImpl* m_lead = nullptr;      // +0x0C the group's lead (none: this is the lead)
  int m_offset[3] = {};                  // +0x10 x, y, level from the lead, as defined
  int m_turned[3] = {};                  // +0x1C the same turned by the lead's direction
  bool m_linked = false;                 // PC: the chain has been made (SetLeader or from the world)
  std::vector<float> m_portalValues;     // portals (GC cXPortalImpl +0x08): its route values, kept from the save
};

// PC: a portal's own stream after its multi-tile part (GC cXPortalImpl::ReconStream; portals are
// cXMTObjectImpls in the port).
void ReconPortalStream(cXMTObjectImpl* portal, ReconBuffer* buffer, int version);

// PC stand-in for the lead ids cXMTObjectImpl::ReconStream reads (its part of the stream comes after the
// object's behaviour stack, which the port does not read): the parts of `part`'s group are found in the
// world (same object file and master id, facing the same way, each where its offsets put it from one
// lead tile; a group made in buy mode is taken from its placement group) and chained, the part of the
// first sub selector leading (GC: MakeNewOutOfWorldObject makes the parts in that order).
void LinkMultiTileGroup(cXMTObjectImpl* part);
// Every multi-tile part of the house is chained (after the house is read).
void LinkMultiTileGroups(ObjectModuleImpl* module);
