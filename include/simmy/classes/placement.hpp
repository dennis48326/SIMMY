// Object placement: the world side of buy/build mode (game/object.cpp's placement part, game/mtobject.cpp's,
// game/walls.cpp's wall tests) and InteractorModule's legality helpers (game/interactorutils.cpp), used by
// the place manipulator (placemanipulator.cpp).
//
// PC: these run on the port's world model. An object's site is a tile (the ObjectModule's tile map and
// the tile chain) or an object's slot (the slot hierarchy, HierSite); data 2 / 3 name the container and
// slot as GC Place writes them. GC's object classes have these as virtual
// methods (cXObject vtable 0x124..0x15C, overridden by cXMTObjectImpl); here they are free functions that
// dispatch on the object's multi-tile group (see PlacementGroup).
#pragma once

#include <vector>

struct cXObject;
struct cXObjectImpl;
struct CTilePt;

// GC PlacementSpec (0x18; PC: PlaceSpec, the generated class keeps the name): where an object goes. Built by the PlaceSpec constructors (800DCDF4,
// 800DCE00) and cXObjectImpl::GetPlacementSpec (800DCB78).
struct PlaceSpec {
  int inWorld = 0;                    // +0x00 0: out of the world (picked up)
  int pos[2] = {-16, -16};            // +0x04 FTilePt (1/16 tiles): [0] y, [1] x
  cXObjectImpl* container = nullptr;  // +0x0C the object whose slot holds it
  int slot = -1;                      // +0x10
  int level = 0;                      // +0x14
  // 800DCDF4 PlaceSpec(bool): nowhere.
  explicit PlaceSpec(bool) {}
  // 800DCE00 PlaceSpec(FTilePt&, int, cXObjectImpl*, int): on a tile (or in a container's slot,
  // where the container's location counts).
  PlaceSpec(const int p[2], int lvl, cXObjectImpl* c, int s);
};

// GC HierarchySite (0x18; PC: HierSite, the generated class keeps the name): an object's site in the
// slot hierarchy: on a tile (no container) or in entry `slot` of the container's slot vector (entry 0:
// the tile chain, k + 1: object slot k).
struct HierSite {
  int valid = 0;                      // +0x00 in the world
  int pos[2] = {-16, -16};            // +0x04 FTilePt
  cXObjectImpl* container = nullptr;  // +0x0C
  int slot = -1;                      // +0x10 the entry
  int level = 0;                      // +0x14
  HierSite() = default;
  explicit HierSite(const PlaceSpec& spec);              // 800DCE44
  HierSite(cXObjectImpl* c, const int p[2], int slot);   // 800DCEC8
};

// GC InteractorModule::PlacementData (0xC): the slot IsLegalToPlaceSingleTileAtLocation found.
struct PlacementData {
  short object = 0;      // +0x00 the container's id
  short slot = 0;        // +0x02
  int inSlot = 0;        // +0x04 1: put it in that slot
  int alignToSlot = 0;   // +0x08 1: turn it the slot's way
};

// PC stand-in for the cXMTObjectImpl chain of a multi-tile object. GC: each part is a cXMTObjectImpl
// whose +0x08 is the next part, +0x0C the lead (the first part), +0x10/+0x14/+0x18 its x/y/level offset
// from the lead (ObjDefinition::GetMultiTileOffsets: sub index >> 8 and & 0xFF; level: definition
// +0x70), and +0x1C/+0x20/+0x24 those offsets turned by the lead's direction (AssignOffsets). The port's
// ObjectModule makes multi-tile parts as plain objects, so placement keeps the group: the parts it made
// (CreateObjectFromGUID) or found in the world (FindPlacementGroup).
struct PlacementGroup {
  struct Part {
    cXObjectImpl* object = nullptr;
    int base[3] = {};    // +0x10 x, y, level offsets as defined
    int offset[3] = {};  // +0x1C turned by the lead's direction
  };
  std::vector<Part> parts;  // parts[0] is the lead
};

extern int gPlacementError;              // 804FDC90 why the last placement test failed
extern cXObject* gPlacementConflict;     // 804FDC94 the object in the way
extern int g_handlingPlacementObject;    // 804FDBF4 Globs::s_handlingPlacementObject

// The group `object` is a part of (null: a single-tile object or not known).
PlacementGroup* PlacementGroupOf(cXObjectImpl* object);
// The group of an object in the world: its file's parts with the same master and direction where their
// offsets put them from one lead tile (as ISimsWallObjectModel's MultiTileGroupCenter finds them).
PlacementGroup* FindPlacementGroup(cXObjectImpl* object);
// A new group for the parts of a multi-tile object made out of the world (GC: MakeNewOutOfWorldObject
// links them with SetLeader; Initialize reads the offsets).
PlacementGroup* MakePlacementGroup(const std::vector<cXObjectImpl*>& parts);
void FreePlacementGroup(PlacementGroup* group);

// game/walls.cpp
int CheckWallFlags(const int pos[2], int level, int dir, unsigned int flags);  // 8014A560
int SectWall(const int rect[4], int level);                                    // 8014A00C

// game/object.cpp (cXObjectImpl), dispatching to game/mtobject.cpp for multi-tile parts
int ObjIsInWorld(cXObjectImpl* o);                                             // 800DCB50
void ObjComputeRect(cXObjectImpl* o, const int pos[2], int rect[4]);           // 800DF4D0
// The objects whose site is `tile` (GC: the tile's object chain; PC: the module's objects there).
std::vector<cXObjectImpl*> TileObjects(CTilePt& tile);
// cXObjectImpl::CanIntersectPeople (800EB6C4): people may stand in it.
bool ObjCanIntersectPeople(cXObjectImpl* o);
int ObjTestIntersection(cXObjectImpl* o, const int pos[2], int level);         // 800DEBB8
int ObjTestAndPlace(cXObjectImpl* o, PlaceSpec& spec, bool place);         // 800DD6E8
int ObjCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);  // 800DE340 / 800C6044
// cXObjectImpl::FindGoodLocation (800E0F40; objectsim.cpp): the nearest tile from `start` (FTilePt words)
// `object` can stand on, in the start's room with `sameRoom`, free of objects with `emptyTile`.
bool ObjFindGoodLocation(cXObjectImpl* object, const int start[2], int level, bool sameRoom, bool emptyTile, int out[2]);
void ObjPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);    // 800DE3C4 / 800C61E4
void ObjPickup(cXObjectImpl* o);                                               // 800DE144 / 800C63E4
void ObjSetContainedLocations(cXObjectImpl* o);  // 800DF784 (SetLocation's contained-objects loop)
void ObjPortalPartKilled(cXObjectImpl* o);  // a door or window part killed: its wall plain again (KillObject's Pickup)
int ObjUserCanPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);  // 800E1E30 / 800C64B8
void ObjUserPlace(cXObjectImpl* o, const int pos[2], int level, cXObjectImpl* container, int slot);    // 800E1FF4 / 800C66AC
int ObjUserCanPickup(cXObjectImpl* o);                                         // 800E1AF4 / 800C6824
void ObjUserPickup(cXObjectImpl* o);                                           // 800E2128 / 800C68B8
void ObjTurn(cXObjectImpl* o, int steps);                                      // 800DFA9C / 800C5CE4
int ObjGetCurrentValue(cXObjectImpl* o);                                       // 800DB744
int ObjGetWasPlacedFromInventory(cXObjectImpl* o);                             // 800E22C8
void ObjSetWasPlacedFromInventory(cXObjectImpl* o, bool on);                   // 800E22F8
void ObjUpdateChairFacing(cXObjectImpl* o);  // 800DE788 (PC: objectplace.cpp's UpdateChairFacingAt)

// The slot hierarchy (cXObjectImpl's Hier* members, game/object.cpp)
void HierGetSite(cXObjectImpl* o, HierSite& site);           // 800DD15C
void ObjGetPlacementSpec(cXObjectImpl* o, PlaceSpec& spec);  // 800DCB78
cXObjectImpl* HierGetChild(cXObjectImpl* o, int entry);      // 800DD114
cXObjectImpl* HierGetParent(cXObjectImpl* o);                // 800DCF2C
cXObjectImpl* GetNextObjectSibling(cXObjectImpl* o);         // 800DCD4C
cXObjectImpl* GetPrevObjectSibling(cXObjectImpl* o);         // 800DCD70
void HierSever(cXObjectImpl* o);                             // 800DD268
void HierSetSite(cXObjectImpl* o, HierSite& site);           // 800DCFB8
cXObjectImpl* HierGetObject(cXObjectImpl* o, HierSite& site);  // 800DD338
// TestAndPlace(spec, true)'s two halves: leaving the site (the chained object takes it), taking one.
void HierUnlink(cXObjectImpl* o);
void HierLink(cXObjectImpl* o, PlaceSpec& spec);
// PC: a person the walker moved takes the tile it now stands on.
void HierResite(cXObjectImpl* o);

// game/interactorutils.cpp (InteractorModule::)
cXObjectImpl* CreateObjectFromGUID(unsigned int guid);                         // 802113D8
int CanObjectBeDestroyed(cXObjectImpl* o);                                     // 80211524
bool DestroyObject(cXObjectImpl* o);                                           // 8021146C
cXObjectImpl* GetLeadObjectImpl(cXObjectImpl* o);                              // 802118C0
bool IsObjectInRange(cXObjectImpl* o);                                         // 80211AC0
int CoreIsLegalToPlace(cXObjectImpl* o, int pos[2], int& level);               // 80211BB4
int IsLegalToPlaceMultiTileAtLocation(cXObjectImpl* o, int pos[2]);            // 80211C8C
int IsLegalToPlaceSingleTileAtLocation(cXObjectImpl* o, int pos[2], PlacementData* data);  // 80211CD8
int IsLegalToPlaceAtLocation(cXObjectImpl* o, int pos[2], PlacementData* data);  // 80212128
int FinalUserPlaceObject(cXObjectImpl* o);                                     // 802121C4
void UpdateAllObjectsInWorldAfterFirstPickupOrFinalPlace(cXObjectImpl* o);     // 80212390
void RecomputeLightingGrid(cXObjectImpl* o);                                   // 80212458
void SetObjectDirection(int dir, cXObjectImpl* o);                             // 802124B4 SetDirection
void ShadeModelToShowValidState(cXObjectImpl* o, bool valid);                  // 802127C8
