// The player's place manipulator: game/objectmanipulator.cpp (InteractorModule::PlaceManipulator, its
// ObjectManipulator update and the PlacementObject it moves) and game/interactorutils.cpp's object
// helpers and placement legality (InteractorModule::IsLegalToPlaceAtLocation and the rest). See
// placement.hpp for the world side (objectplace.cpp).
//
// As on GC: StartPlacement makes the object of the GUID out of the world (CreateObjectFromGUID: a
// multi-tile object makes all its parts) and each frame the object is placed at the cursor tile
// (ObjectManipulator::Update: picked up, tested with IsLegalToPlaceAtLocation, placed anyway, then
// shaded red where it may not stay: ShadeModelToShowValidState). A (TryPlacingCurrentObject ->
// PlacementObject::Drop -> FinalUserPlaceObject) leaves it there if it is legal and reports 0 to the
// catalogue, else the error sound; B (CancelSession) reports 3 and destroys it; L/R (interactor
// commands 5/6) turn it (PlacementObject::Rotate).
//
// PC stand-ins (marked): the interactor manager is not ported, so the manipulator runs per player from
// UpdateDirectInteractors with the raw pad (GC: InteractorInputManager commands); the cursor moves a tile
// per stick step relative to the camera (GC: MoveCursorAlongCameraTargetVector + SnapPositionToTile; the
// one-tile variant 80210C58 does the same). The grid under the object is its tile model drawn per part
// (PlacementObject::CreateGridObject: no grid object). Not made: the cursor model, the object colour
// (SetObjectColor), slots (PlacementData::inSlot: objects on tables).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/classes/buildtools.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------------------------------
// game/interactorutils.cpp (InteractorModule::)

namespace {
ObjSelector* SelectorByGUID(unsigned int guid) {
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  return folder ? folder->GetSelectorByGUID(static_cast<int>(guid)) : nullptr;
}
cXObjectImpl* ObjectByID(short id) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  cXObject* o = module && id ? module->GetObjectFromID(id) : nullptr;
  return o ? o->GetObjectImplementation() : nullptr;
}
}  // namespace

// 802113D8
// The GUID's master selector made into a new object out of the world (ObjectModule vtable 0x54
// MakeNewOutOfWorldObject). PC: the port's ObjectModule makes one object per selector, so a multi-tile
// object's parts (its sub selectors, as GC's MakeNewOutOfWorldObject walks them) are made here one by
// one and grouped (PlacementGroup); the first part stands for the object, and the lead gets the price
// (data 0x29), as on GC.
cXObjectImpl* CreateObjectFromGUID(unsigned int guid) {
  ObjSelector* selector = SelectorByGUID(guid);
  ObjectModuleImpl* module = Globs_pObjectModule;
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  auto* master = selector ? reinterpret_cast<ObjSelector*>(selector->GetMasterSelector()) : nullptr;
  if (!master || !master->m_def || !module || !folder) return nullptr;
  if (objdef::MasterID(master->m_def) == 0 || objdef::SubIndex(master->m_def) != -1) {
    short id = 0;
    module->MakeNewOutOfWorldObject(master, &id);  // Reset(true): its init tree has run
    return ObjectByID(id);
  }
  // The whole group at once (the world side's MakeNewOutOfWorldObject of the lead selector: every part,
  // linked, then each part's init tree, as GC), then grouped for placement. (PC: the parts were made one
  // by one here, each init tree running before its group was linked: the cot's middle part failed its
  // SetToNext kind 3 in global 0117, TREEASSERT 1100.)
  std::vector<cXObjectImpl*> parts;
  short leadId = 0;
  module->MakeNewOutOfWorldObject(master, &leadId);
  for (auto* p = dynamic_cast<cXMTObjectImpl*>(ObjectByID(leadId)); p; p = p->m_nextPart) parts.push_back(p);
  if (parts.empty()) return nullptr;
  MakePlacementGroup(parts);
  return parts[0];
}

// 80211524
// Sellable objects (data 0x2B bit 3) that hold nothing. PC: slots are not read, so nothing is held.
int CanObjectBeDestroyed(cXObjectImpl* o) { return o && (o->GetData(0x2b) >> 3 & 1); }

// 8021146C
// GC: ObjectModule vtable 0x5C (the group's kill), the house's and the folder's refresh. PC: each part
// of a multi-tile group is killed.
bool DestroyObject(cXObjectImpl* o) {
  if (!CanObjectBeDestroyed(o)) return false;
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return false;
  if (PlacementGroup* g = PlacementGroupOf(o)) {
    std::vector<short> ids;
    for (auto& p : g->parts) ids.push_back(static_cast<short>(p.object->m_id));
    FreePlacementGroup(g);
    for (short id : ids) module->KillObject(id);
  } else {
    module->KillObject(static_cast<short>(o->m_id));
  }
  return true;
}

// 802118C0
cXObjectImpl* GetLeadObjectImpl(cXObjectImpl* o) {
  if (!o) return nullptr;
  PlacementGroup* g = o->IsMultiTile() ? PlacementGroupOf(o) : nullptr;
  return g && !g->parts.empty() ? g->parts[0].object : o;
}

// 80211AC0
bool IsObjectInRange(cXObjectImpl* o) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!o || !w) return false;
  const CTilePt t = o->GetCTilePt();
  return t.GetX() >= 0 && t.GetX() <= w->GetXSize() - 1 && t.GetY() >= 0 && t.GetY() <= w->GetYSize() - 1;
}

// 80211BB4
// At the tile's middle, on the object's level: it can be placed there and the player may put it there.
int CoreIsLegalToPlace(cXObjectImpl* o, int pos[2], int& level) {
  if (!o) return 0;
  pos[1] = (pos[1] & ~0xf) | 8;
  pos[0] = (pos[0] & ~0xf) | 8;
  level = o->GetLevel();
  return ObjCanPlace(o, pos, level, nullptr, 0) && ObjUserCanPlace(o, pos, level, nullptr, 0);
}

// 80211C8C: the group's lead decides.
int IsLegalToPlaceMultiTileAtLocation(cXObjectImpl* o, int pos[2]) {
  int level = 0;
  return CoreIsLegalToPlace(GetLeadObjectImpl(o), pos, level);
}

// 80211CD8
// Legal on the floor; else, when another object is in the way (0xB), in the first slot of an object on
// the tile (the tile's objects and what their slots hold, ObjectIterator) whose height (ObjectSlot +8:
// the descriptor's +0x18) is one of the object's heights (data 4 bit h-1) and where it can be placed
// (CanPlace with that container and slot): `data` names the container and slot. Then it turns the
// container's way (GC: by the object's rotation step, data 0x17; a multi-tile container: the first of
// its sides with room, PC: not ported). None: an object that only goes on surfaces (heights 8) says so
// (0x24).
int IsLegalToPlaceSingleTileAtLocation(cXObjectImpl* o, int pos[2], PlacementData* data) {
  int level = 0;
  if (CoreIsLegalToPlace(o, pos, level)) return 1;
  if (gPlacementError == 0xb && !o->IsMultiTile()) {
    const int error = gPlacementError;
    const unsigned int heights = static_cast<unsigned short>(o->GetData(4));
    CTilePt tile(pos[1] >> 4, pos[0] >> 4, level);
    ObjectIterator it;
    for (it.init(tile, ObjectIterator__IterateType{0}); it.m_current; ++it) {
      cXObjectImpl* c = it.m_current->GetObjectImplementation();
      if (!c || c == o) continue;
      const int count = ObjectSlotCount(c);
      for (int i = 0; i < count; ++i) {
        const unsigned char* slot = ObjectSlotDescriptor(c, i);
        int h = 0;
        float alt = 0.0f;
        if (slot) ObjectSlotHeight(slot, h, alt);
        if (h < 1 || !((1u << (h - 1)) & heights)) continue;
        if (!ObjCanPlace(o, pos, level, c, i)) continue;
        if (data) {
          data->object = static_cast<short>(c->m_id);
          data->slot = static_cast<short>(i);
          data->inSlot = 1;
        }
        if (c->IsMultiTile())
          SIMMY_STUB("IsLegalToPlaceSingleTileAtLocation: a multi-tile container's side with room", 0x80211CD8);
        const int step = o->GetData(0x17);
        if (data && step > 0) {
          const int turns = ((c->GetData(1) & 7) - (o->GetData(1) & 7)) / step;
          if (turns) o->m_data.WriteVar(1, static_cast<short>(((o->GetData(1) + turns * step) % 8 + 8) % 8));
        }
        return 1;
      }
    }
    gPlacementError = error;
    if (heights != 0 && (heights & ~8u) == 0) gPlacementError = 0x24;
  }
  return 0;
}

// 80212128
// Only for an object out of the world (picked up first).
int IsLegalToPlaceAtLocation(cXObjectImpl* o, int pos[2], PlacementData* data) {
  if (!o || ObjIsInWorld(o)) return 0;
  if (!o->IsMultiTile()) return IsLegalToPlaceSingleTileAtLocation(o, pos, data);
  return IsLegalToPlaceMultiTileAtLocation(o, pos);
}

// 802121C4
// The object at its tile's middle for good: picked up, then the player's placement there if it is
// legal and on the lot (UserPlace; level 1), its normal shading and the world updated. 0: it stays
// picked up.
int FinalUserPlaceObject(cXObjectImpl* o) {
  if (!o) return 0;
  const int at[2] = {o->m_location[0], o->m_location[1]};
  const bool inRange = IsObjectInRange(o);
  if (ObjIsInWorld(o)) {
    g_handlingPlacementObject = 1;
    ObjPickup(o);
    g_handlingPlacementObject = 0;
  }
  int pos[2] = {(at[0] & ~0xf) | 8, (at[1] & ~0xf) | 8};
  PlacementData data;
  if (!IsLegalToPlaceAtLocation(o, pos, &data) || !inRange) return 0;
  cXObjectImpl* container = nullptr;
  if (data.inSlot && Globs_pObjectModule)
    if (cXObject* c = Globs_pObjectModule->GetObjectFromID(data.object)) container = c->GetObjectImplementation();
  ObjUserPlace(o, pos, 1, container, container ? data.slot : 0);
  ShadeModelToShowValidState(o, true);
  UpdateAllObjectsInWorldAfterFirstPickupOrFinalPlace(o);
  return 1;
}

// 80212390
// The rooms recomputed (the world's vtable 0x114 ComputeRooms(0)) and the house's object models placed
// again (EIObjectMan::ReOrientHouse: counters see their sinks, hedges their neighbours); for a portal
// (door, window), a light source (cXObjectImpl::GetObjectLightSource 1) or an object with a zero
// extent override (EGlobal::CheckForZeroExtentOverride: definition flags 6) the house is rebuilt
// (EHouse::ReCalcHouse: walls, floors and the lighting grid).
void UpdateAllObjectsInWorldAfterFirstPickupOrFinalPlace(cXObjectImpl* o) {
  if (cFixedWorldImpl* w = Globs_pFixedWorld) w->ComputeRooms(0);
  EHouse* house = _globals.m_house;  // DAT_80475E94
  if (house && house->m_objectMan) house->m_objectMan->ReOrientHouse(false);
  if (!IsPortalObject(o) && o->GetObjectLightSource() != 1 && !_globals.CheckForZeroExtentOverride(o)) return;
  if (house) house->ReCalcHouse();
}

// 80212458
// A light source (or none named) rebuilds the house (EHouse::ReCalcHouse), whose lights' locations are
// the lighting grid.
void RecomputeLightingGrid(cXObjectImpl* o) {
  EHouse* house = _globals.m_house;  // DAT_80475E94
  if (o && o->GetObjectLightSource() != 1) return;
  if (house) house->ReCalcHouse();
}

// 802124B4 InteractorModule::SetDirection
// Turned (a step at a time, at most 8) until it faces `dir`, then put back where it was if it fits.
void SetObjectDirection(int dir, cXObjectImpl* o) {
  if (!o || o->GetData(1) == dir) return;
  const int pos[2] = {o->m_location[0], o->m_location[1]};
  if (ObjIsInWorld(o)) ObjPickup(o);
  for (int i = 0; i < 8 && o->GetData(1) != dir; ++i) ObjTurn(o, 1);
  if (ObjCanPlace(o, pos, o->GetLevel(), nullptr, 0)) ObjPlace(o, pos, o->GetLevel(), nullptr, 0);
}

// 802127C8
// The object's model (a multi-tile object's every part) lit red while it may not stay
// (ISimInstance::SetPlacementError). GC: also the objects in its slots.
void ShadeModelToShowValidState(cXObjectImpl* o, bool valid) {
  if (!o) return;
  if (PlacementGroup* g = PlacementGroupOf(o)) {
    for (auto& p : g->parts)
      if (p.object->m_instance) p.object->m_instance->SetPlacementError(!valid);
    return;
  }
  if (o->m_instance) o->m_instance->SetPlacementError(!valid);
}

// ---------------------------------------------------------------------------------------------------
// game/objectmanipulator.cpp

namespace {
// InteractorModule::PlacementObject (0x34): the object being placed.
struct PlacementObject {
  cXObjectImpl* m_object = nullptr;  // +0x00
  void* m_grid = nullptr;            // +0x04 the grid object (PC: its tile's model, m_gridModel)
  ERModel* m_gridModel = nullptr;    // PC: the grid tile's (GUID 0x437) model, drawn under each part
  int m_origPos[2] = {-16, -16};     // +0x08 where a grabbed object was
  short m_origDir = 0;               // +0x10
  int m_last[3] = {0, -16, 0};       // +0x28

  int CreateNewPlacementObjectFromGuid(unsigned int guid, unsigned char colour);
  static int SetObjectColor(cXObjectImpl* o, unsigned char colour);
  void Pickup();
  int IsLegalToPlaceAtLocation(int pos[2], PlacementData& data);
  void Place(int pos[2], PlacementData& data);
  int Drop();
  int Destroy();
  void Rotate(int direction);
  void SetShaderToValidState(bool valid);
  int GrabExistingObjectInstanceFromId(short id);
  void Initialize();
  int CanObjectBeDestroyed();
  void Reset();
  void ResetDirection();
  void ResetLocation();
  void CreateGridObject();
  void DestroyGridObject();
};

// 8021AB88
// The object's colour index: its model's colour (EIStaticModel::ModifyColor) and data 0x37
// (SetColorIndex, cXObject vtable 0x4C4), for every part of a multi-tile object. GC: the catalogue
// always asks for colour 0 (PCTTarget::enter_place_mode; this build has no recolour choice).
int PlacementObject::SetObjectColor(cXObjectImpl* o, unsigned char colour) {
  if (!o) return 0;
  auto apply = [colour](cXObjectImpl* part) {
    if (part->m_instance) part->m_instance->ModifyColor(colour);
    part->m_data.WriteVar(0x37, colour);  // 800E3118 cXObjectImpl::SetColorIndex
  };
  if (PlacementGroup* g = PlacementGroupOf(o)) {
    for (auto& p : g->parts) apply(p.object);
  } else {
    apply(o);
  }
  return 1;
}

// 8021ACC4
// A new object of the GUID with its colour. GC: also its grid object (CreateGridObject: a 0x437 grid
// tile under each part; not made) and, in split screen with the camera's mode 2, its direction from the
// camera (SetAttr 0).
int PlacementObject::CreateNewPlacementObjectFromGuid(unsigned int guid, unsigned char colour) {
  if (m_object) return 0;
  m_object = CreateObjectFromGUID(guid);
  if (m_object) {
    SetObjectColor(m_object, colour);
    CreateGridObject();
    RecomputeLightingGrid(m_object);
  }
  return m_object ? 1 : 0;
}

// 8021BDB4
void PlacementObject::Pickup() {
  if (m_object && ObjIsInWorld(m_object)) {
    g_handlingPlacementObject = 1;
    ObjPickup(m_object);
    g_handlingPlacementObject = 0;
  }
}

// 8021BE8C
int PlacementObject::IsLegalToPlaceAtLocation(int pos[2], PlacementData& data) {
  if (!m_object) return 0;
  Pickup();
  return ::IsLegalToPlaceAtLocation(m_object, pos, &data);
}

// 8021BEE4
// GC: only where its grid object can be placed (vtable 0x134), and the grid too.
void PlacementObject::Place(int pos[2], PlacementData& data) {
  if (!m_object) return;
  m_last[0] = 0;
  m_last[1] = -16;
  m_last[2] = 0;
  g_handlingPlacementObject = 1;
  cXObjectImpl* container = nullptr;
  if (data.inSlot && Globs_pObjectModule)
    if (cXObject* c = Globs_pObjectModule->GetObjectFromID(data.object)) container = c->GetObjectImplementation();
  ObjPlace(m_object, pos, 1, container, container ? data.slot : 0);
  g_handlingPlacementObject = 0;
}

// 8021C06C
int PlacementObject::Drop() {
  if (!m_object || FinalUserPlaceObject(m_object) != 1) {
    UIAUDIO::Instance()->PlaySound(0x6e2778b7);
    return 0;
  }
  // The group stays known for later moves (PC).
  m_object = nullptr;
  DestroyGridObject();
  UIAUDIO::Instance()->PlaySound(0x80e1a9cd);
  m_last[0] = 0;
  m_last[1] = -16;
  m_last[2] = 0;
  return 1;
}

// 8021C208
int PlacementObject::Destroy() {
  int destroyed = 0;
  if (m_object) {
    g_handlingPlacementObject = 1;
    destroyed = DestroyObject(m_object) ? 1 : 0;
    g_handlingPlacementObject = 0;
    m_object = nullptr;
    DestroyGridObject();
    m_last[0] = 0;
    m_last[1] = -16;
    m_last[2] = 0;
  }
  return destroyed;
}

// 8021C3C8
// A quarter turn (1: to the left, data 1 - 2; 0: to the right, + 2; wrapping 0..6), with the rotate
// sound; the object turned a step at a time and put back where it stood if that is legal. PC: GC
// returns at once when there is no grid object.
void PlacementObject::Rotate(int direction) {
  if (!m_object) return;
  UIAUDIO::Instance()->PlaySound(0x735aadb9);
  int dir = m_object->GetData(1);
  if (direction == 1) dir -= 2;
  else if (direction == 0) dir += 2;
  dir = dir < 0 ? 6 : (dir < 7 ? dir : 0);
  if (dir == m_object->GetData(1)) return;
  int pos[2] = {m_object->m_location[0], m_object->m_location[1]};
  g_handlingPlacementObject = 1;
  if (ObjIsInWorld(m_object)) ObjPickup(m_object);
  for (int i = 0; i < 8 && m_object->GetData(1) != dir; ++i) ObjTurn(GetLeadObjectImpl(m_object), 1);
  PlacementData data;
  if (const unsigned char* res = objdef::ResData(m_object->m_def))
    if (res[7]) data.alignToSlot = 1;
  if (::IsLegalToPlaceAtLocation(m_object, pos, &data)) ObjPlace(m_object, pos, 1, nullptr, 0);
  g_handlingPlacementObject = 0;
}

// 8021CCC0
void PlacementObject::SetShaderToValidState(bool valid) {
  if (m_object) ShadeModelToShowValidState(m_object, valid);
}


// 8021ADE4
// The lead of the object with `id` in hand, if the player may pick it up (UserCanPickup): where it
// was and its direction kept for a cancel. GC: also its grid object (CreateGridObject). PC: a
// multi-tile object's group is found in the world first (FindPlacementGroup).
int PlacementObject::GrabExistingObjectInstanceFromId(short id) {
  cXObjectImpl* o = ObjectByID(id);
  if (!o) return 0;
  FindPlacementGroup(o);
  cXObjectImpl* lead = GetLeadObjectImpl(o);
  if (!lead || !ObjUserCanPickup(lead)) return 0;
  m_object = lead;
  m_origPos[0] = lead->m_location[0];
  m_origPos[1] = lead->m_location[1];
  m_origDir = static_cast<short>(lead->GetData(1));
  CreateGridObject();
  // GC CreateGridObject(pos, object): the grabbed object is picked up by the player (UserPickup(false),
  // vtable 0x164: its "user picked up" trees), its grid tiles placed where it was.
  ObjUserPickup(lead);
  UpdateAllObjectsInWorldAfterFirstPickupOrFinalPlace(lead);
  return 1;
}

// 8021C160
void PlacementObject::Initialize() {
  m_last[0] = 0;
  m_last[1] = -16;
  m_last[2] = 0;
}

// 8021C1D4
int PlacementObject::CanObjectBeDestroyed() { return m_object ? ::CanObjectBeDestroyed(m_object) : 0; }

// 8021C344
void PlacementObject::ResetDirection() {
  if (!m_object) return;
  g_handlingPlacementObject = 1;
  SetObjectDirection(m_origDir, m_object);
  g_handlingPlacementObject = 0;
}

// 8021CA24
// Back where it was grabbed: there if that is still legal, else put there anyway (GC: a multi-tile
// object's parts also stop simulating, vtable 0x23C); the world updated and its shading normal.
void PlacementObject::ResetLocation() {
  if (!m_object) return;
  PlacementData data;
  int pos[2] = {m_origPos[0], m_origPos[1]};
  ::IsLegalToPlaceAtLocation(m_object, pos, &data);
  Pickup();
  const int back[2] = {m_origPos[0], m_origPos[1]};
  ObjPlace(m_object, back, 1, nullptr, 0);
  UpdateAllObjectsInWorldAfterFirstPickupOrFinalPlace(m_object);
  ShadeModelToShowValidState(m_object, true);
}

// 8021C2B4
void PlacementObject::Reset() {
  ResetDirection();
  ResetLocation();
  m_object = nullptr;
  DestroyGridObject();
  Initialize();
}

// 8021AEF8
// The grid under the object: GC makes a multi-tile object of grid tiles (GUID 0x437, CreateGridTile), one
// under each part, which the visualizer draws at the cursor (white, or red where it can't go). PC: the
// grid tile's model is taken from one such object (made out of the world and killed again), else from
// the grid tile's definition, and the visualizer draws it under each part (Draw(PlacementObject&)).
void PlacementObject::CreateGridObject() {
  if (m_gridModel) return;
  static ERModel* s_tile = nullptr;  // the grid tile's model, kept once found
  static bool s_tried = false, s_triedDef = false;
  if (!s_tile && !s_tried) {
    s_tried = true;
    if (cXObjectImpl* grid = CreateObjectFromGUID(0x437)) {
      if (grid->m_instance && grid->m_instance->m_model) {
        s_tile = grid->m_instance->m_model;
        s_tile->AddRef();
      }
      DestroyObject(grid);
    }
  }
  if (!s_tile && !s_triedDef) {
    // PC: the port's object module cannot construct the grid tile object (ConstructObject fails for
    // GUID 0x437, the world side's); its model is the one its definition's first ObjAnimDef names (as
    // the object's instance would load it).
    s_triedDef = true;
    if (ObjSelector* sel = SelectorByGUID(0x437))
      if (const unsigned char* res = sel->m_def ? objdef::ResData(sel->m_def) : nullptr)
        if (const unsigned char* defs = objdef::AnimDefs(res); objdef::AnimDefCount(defs))
          s_tile = static_cast<ERModel*>(_modelman.EResourceManager::AddRef(ERQuickdata::U32(defs, 0), nullptr, 0));
  }
  if (s_tile) {
    s_tile->AddRef();
    m_gridModel = s_tile;
  }
  if (!s_tile)
    SIMMY_STUB("PlacementObject::CreateGridObject: no grid tile model (GUID 0x437's definition)", 0x8021AEF8);
}

// 8021BB94
void PlacementObject::DestroyGridObject() {
  if (m_gridModel) m_gridModel->DelRef();
  m_gridModel = nullptr;
}

// InteractorModule::ObjectManipulator (0xD8 with its subclasses' parameters): the interactor that moves
// a PlacementObject with its cursor. One place manipulator (interactor 6) and one grab manipulator
// (interactor 2) per player.
struct ObjectManipulator {
  bool m_active = false;
  int m_player = 0;
  float m_cursor[2] = {};          // +0x08 / +0x0C the cursor: [0] the FTilePt's y (engine x), [1] its x
  PlacementObject m_placement;     // +0x64
  int m_cursorModel = 1;           // +0xC4 the cursor's models are drawn (GetIsCursorModelEnabled)
  int m_moveWait = 0;              // PC: frames until the stick steps the cursor again
  EVec3 m_drawPos;                 // PC: the cursor in the engine's frame (the draw record's position)
  void SetIsCursorModelEnabled(bool on) { m_cursorModel = on ? 1 : 0; }  // 8021A034
  int GetIsCursorModelEnabled() const { return m_cursorModel; }        // 8021A03C
  virtual ~ObjectManipulator() = default;
  virtual void OnCommandPressed(int command) = 0;
  void Update(float dt);
  void UpdateObjectHighlights(EIObjectMan* objects);
  void CursorBesideSim();
};

// InteractorModule::PlaceManipulator.
struct PlaceManipulator : ObjectManipulator {
  unsigned int m_guid = 0;         // +0xC0 PlaceManipulatorParams: the object
  unsigned char m_colour = 0;      // +0xC4
  int m_keepDirection = 0;         // +0xC8 placing another of the same: turn it like the last one
  short m_direction = 0;           // +0xCC
  int m_cancelEnabled = 1;         // +0xD0

  void StartPlacement();
  void OnCommandPressed(int command) override;
  void TryPlacingCurrentObject();
  void CancelSession();
  void Report(int command);
};
PlaceManipulator s_manipulators[2];

// InteractorModule::GrabManipulator.
struct GrabManipulator : ObjectManipulator {
  void OnCommandPressed(int command) override;
  void TryGrabbingWorldObject();
  void TryGrabbingWorldObjectFromId(short id);
  void DestroyObjectInHand();
  void DropCurrentObject();
  void TryRemoveCurrentObjectFromWorld();
  void CancelCurrentGrab();
  void CancelSession();
  int Report(int command);
};
GrabManipulator s_grabbers[2];

// The cursor beside the player's sim (PC: GC starts at the player's interactor position).
void ObjectManipulator::CursorBesideSim() {
  cXPerson* sim = _globals.GetSelectedPerson(m_player);
  auto* person = sim ? dynamic_cast<cXPersonImpl*>(sim->AsObject()->GetObjectImplementation()) : nullptr;
  if (person) {
    const CTilePt t = person->GetCTilePt();
    m_cursor[1] = static_cast<float>(t.GetX() + 1);
    m_cursor[0] = static_cast<float>(t.GetY());
  } else {
    m_cursor[0] = m_cursor[1] = 8.0f;
  }
}

// The catalogue's PlaceModeCallback (GC: the PlaceManipulatorParams' callback +0xB8 with its user data
// +0xB4: PCTTarget::static_PlaceModeCallback). PC: it never asks for another of the same.
void PlaceManipulator::Report(int command) {
  if (PCTTarget* pct = PCTTarget::s_placing)
    if (pct->m_player == m_player) pct->PlaceModeCallback(command, m_guid);
}

// GC 8021A1FC
void PlaceManipulator::StartPlacement() {
  if (!m_placement.CreateNewPlacementObjectFromGuid(m_guid, m_colour)) return;
  if (m_keepDirection && m_placement.m_object)
    for (int i = 0; i < 4 && m_placement.m_object->GetData(1) != m_direction; ++i) m_placement.Rotate(0);
  SetIsCursorModelEnabled(false);
}

// GC 80219230
// Interactor commands: 0 (A) places, 4 (B) cancels, 5 / 6 turn it left / right.
void PlaceManipulator::OnCommandPressed(int command) {
  if (command == 4) {
    if (m_placement.m_object && m_cancelEnabled) CancelSession();
  } else if (command == 0) {
    if (m_placement.m_object) TryPlacingCurrentObject();
  } else if (command == 5) {
    m_placement.Rotate(1);
  } else if (command == 6) {
    m_placement.Rotate(0);
  }
}

// GC 8021A8D8
// Dropped where it is: marked as placed by the player (SetWasPlacedFromInventory: it can be sold), and
// the catalogue hears 0 (bought). GC: when the callback asks for another, a new one of the same GUID turned the same way.
void PlaceManipulator::TryPlacingCurrentObject() {
  cXObjectImpl* o = m_placement.m_object;
  const short dir = o ? static_cast<short>(o->GetData(1)) : 0;
  if (m_placement.Drop() != 1) return;
  if (o) ObjSetWasPlacedFromInventory(o, true);  // vtable 0x46C
  const CTilePt t = o->GetCTilePt();
  std::printf("[buy] player %d placed %s (#%d) at %d,%d facing %d\n", m_player + 1, o->DebugName(), o->m_id, t.GetX(),
              t.GetY(), dir);
  m_active = false;
  m_direction = dir;
  SetIsCursorModelEnabled(true);
  Report(0);
}

// GC 8021A9EC
// The callback told (3); the object in hand destroyed; a light source's going rebuilds the lighting
// grid (RecomputeLightingGrid(0)).
void PlaceManipulator::CancelSession() {
  m_active = false;
  SetIsCursorModelEnabled(true);
  Report(3);
  const bool light = m_placement.m_object && m_placement.m_object->GetObjectLightSource() == 1;
  m_placement.Destroy();
  if (light) RecomputeLightingGrid(nullptr);
}

// The catalogue's GrabManipulatorCallback (GC: the callback +0xB8 with its data: 0 dropped, 1 grab
// cancelled, 2 to be sold or deleted, 3 the session ended). 1: handled.
int GrabManipulator::Report(int command) {
  if (PCTTarget* pct = PCTTarget::s_placing)
    if (pct->m_player == m_player) return pct->GrabManipulatorCallback(command, m_placement.m_object);
  return 0;
}

// GC 802192E8
// Interactor commands: 0 (A) grabs what is under the cursor or drops what is in hand, 1 sells it, 4 (B)
// puts it back (or, with nothing in hand, leaves grab mode), 5 / 6 turn it.
void GrabManipulator::OnCommandPressed(int command) {
  if (command == 4) {
    if (!m_placement.m_object) CancelSession();
    else CancelCurrentGrab();
  } else if (command == 0) {
    if (!m_placement.m_object) TryGrabbingWorldObject();
    else DropCurrentObject();
  } else if (command == 1) {
    if (m_placement.m_object) TryRemoveCurrentObjectFromWorld();
  } else if (command == 5) {
    m_placement.Rotate(1);
  } else if (command == 6) {
    m_placement.Rotate(0);
  }
}

// GC 80219C74 ObjectManipulator::UpdateObjectHighlights
// With nothing in hand: every object instance whose bounds meet the square of the selection radius
// about the cursor (from the floor to 10 up) and that the player may pick up (data 0x22 0,
// UserCanPickup, vtable 0x15C) is lit in the player's highlight (cursor flag 1, player 2: 8), the others
// not; while the grab menu is open only its current object is. GC: the lit ones also go to the overlap
// search (UpdateOverlapIntersection, vtable 0x94: the nearest one's centre, read by nothing here). PC:
// the cursor is a tile; the square is about its centre.
void ObjectManipulator::UpdateObjectHighlights(EIObjectMan* objects) {
  if (m_placement.m_object || !objects) return;
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
  const float r = cam ? cam->GetCurZoomRatio() * 0.65f + 0.35f : 0.0f;  // Interactor::GetSelectionRadius (802093F0)
  const float cx = m_cursor[0] + 0.5f, cy = m_cursor[1] + 0.5f;
  const float x0 = cx - r, x1 = cx + r, y0 = cy - r, y1 = cy + r, z0 = 0.0f, z1 = 10.0f;
  cXObjectImpl* selected = GetSelectedActionMenuItem(static_cast<unsigned int>(m_player));
  const unsigned int flag = m_player == 0 ? ISimInstance::kHighlight : ISimInstance::kHighlight2;
  for (auto& item : objects->m_items) {
    ISimInstance* inst = item.second;
    if (!inst) continue;
    bool lit = false;
    const float* b = inst->m_bounds;
    if (b[0] <= x1 && x0 <= b[3] && b[1] <= y1 && y0 <= b[4] && b[2] <= z1 && z0 <= b[5]) {
      cXObjectImpl* o = inst->m_xob ? inst->m_xob->GetObjectImplementation() : nullptr;
      if (o && o->GetData(0x22) == 0 && ObjUserCanPickup(o)) lit = true;
    }
    if (selected) lit = inst->m_xob && inst->m_xob->GetObjectImplementation() == selected;
    inst->SetHighlight(flag, lit);
  }
}

// GC 8021A348
// The objects the player lights (EIObjectMan::GetObjectsInRect: UpdateObjectHighlights'): one is
// grabbed; with several the catalogue offers a menu of them (PCTTarget::CreateGrabObjectMenu, after the
// input state is reset, vtable 0x7C). PC: the menu gets their objects.
void GrabManipulator::TryGrabbingWorldObject() {
  std::vector<cXObjectImpl*> found;
  EHouse* house = _globals.m_house;
  if (house && house->m_objectMan) {
    std::vector<ISimInstance*> lit;
    house->m_objectMan->GetObjectsInRect(m_player, lit);
    for (ISimInstance* inst : lit) {
      cXObjectImpl* o = inst->m_xob ? inst->m_xob->GetObjectImplementation() : nullptr;
      if (!o) continue;
      // PC: a multi-tile object once, as its lead (the port's placement moves a group by its lead).
      FindPlacementGroup(o);
      cXObjectImpl* lead = GetLeadObjectImpl(o);
      if (std::find(found.begin(), found.end(), lead) == found.end()) found.push_back(lead);
    }
  }
  if (found.empty()) return;
  if (found.size() > 1) {
    // Several: the catalogue's menu of them (the input state reset first, vtable 0x7C).
    std::printf("[grab] player %d: %d objects under the cursor, menu\n", m_player + 1, static_cast<int>(found.size()));
    if (PCTTarget* pct = PCTTarget::s_placing) pct->CreateGrabObjectMenu(found);
    return;
  }
  TryGrabbingWorldObjectFromId(static_cast<short>(found[0]->m_id));
}

// GC 8021A438
void GrabManipulator::TryGrabbingWorldObjectFromId(short id) {
  if (!m_placement.GrabExistingObjectInstanceFromId(id)) {
    std::printf("[grab] player %d can't pick up #%d (placement error 0x%x)\n", m_player + 1, id, gPlacementError);
    UIAUDIO::Instance()->PlaySound(0x6e2778b7);
    return;
  }
  SetIsCursorModelEnabled(false);
  UIAUDIO::Instance()->PlaySound(0x66dede72);
  std::printf("[grab] player %d picks up %s (#%d)\n", m_player + 1, m_placement.m_object->DebugName(), m_placement.m_object->m_id);
  if (PCTTarget* pct = PCTTarget::s_placing)
    if (pct->m_player == m_player) pct->PickedUpObject(m_placement.m_object);
}

// GC 8021A50C
void GrabManipulator::DestroyObjectInHand() {
  m_placement.Destroy();
  SetIsCursorModelEnabled(true);
}

// GC 8021A564
void GrabManipulator::DropCurrentObject() {
  cXObjectImpl* o = m_placement.m_object;
  if (!o || !m_placement.Drop()) return;
  SetIsCursorModelEnabled(true);
  const CTilePt t = o->GetCTilePt();
  std::printf("[grab] player %d drops %s (#%d) at %d,%d\n", m_player + 1, o->DebugName(), o->m_id, t.GetX(), t.GetY());
  Report(0);
}

// GC 8021A620
// Sellable: the catalogue asks (its sell or delete dialog); it destroys the object when the player says
// yes (GrabDestroyObjectInHand). GC: a callback that answers at once has it destroyed here.
void GrabManipulator::TryRemoveCurrentObjectFromWorld() {
  if (!m_placement.CanObjectBeDestroyed()) {
    std::printf("[grab] player %d can't sell %s (data 0x2b %x)\n", m_player + 1, m_placement.m_object->DebugName(),
                static_cast<unsigned short>(m_placement.m_object->GetData(0x2b)));
    UIAUDIO::Instance()->PlaySound(0x6e2778b7);
    return;
  }
  Report(2);
  UIAUDIO::Instance()->PlaySound(0x6e2778b7);  // the callback did not destroy it at once (local_18 0)
}

// GC 8021A7BC
void GrabManipulator::CancelCurrentGrab() {
  Report(1);
  m_placement.Reset();
  SetIsCursorModelEnabled(true);
}

// GC 8021A854
void GrabManipulator::CancelSession() {
  m_active = false;
  SetIsCursorModelEnabled(true);
  Report(3);
  m_placement.Initialize();
}

// GC 802194E0 ObjectManipulator::Update
// The cursor (PC: a tile per stick step, camera relative) and the commands, then, with an object in
// hand, the cursor kept on the lot with every part of a multi-tile object and the object picked up and
// placed at the cursor's tile, lit red unless legal.
void ObjectManipulator::Update(float) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w) return;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_player)));
  // GC: G2DTarget::SpawnDialog pauses the players' interactors while a dialog box is up (the catalogue's
  // sell dialog takes A and B then).
  auto* g2d = static_cast<G2DTarget*>(_globals.m_g2dTarget);
  if (g2d && g2d->IsG2DSpawned()) pad = nullptr;
  // ... and while the player's action menu is open (the grab menu: its pad filter, priority 100, takes the
  // buttons from the interactor's input session on GC).
  if (ActionMenu* menu = GetActionMenu(static_cast<unsigned int>(m_player)))
    if (menu->m_state > 1) pad = nullptr;
  if (pad) {
    // PC: the raw pad (GC: the interactor's input session and commands).
    const unsigned int all = ~0u;
    auto pressed = [&](unsigned int cmd) { return pad->GetCmdResult(cmd, pad->GetBtnPressedMask(all), pad->GetBtnDownMask(all), false); };
    ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
    const float sx = static_cast<float>(pad->GetStick(0, 0, all)), sy = static_cast<float>(pad->GetStick(0, 1, all));
    if (m_moveWait > 0) --m_moveWait;
    if (cam && (std::fabs(sx) > 0.5f || std::fabs(sy) > 0.5f) && m_moveWait == 0) {
      float hx = cam->m_target.x - cam->m_eye.x, hy = cam->m_target.y - cam->m_eye.y;
      const float l = std::sqrt(hx * hx + hy * hy);
      if (l > 0.0f) hx /= l, hy /= l;
      const float ex = hx * sy + hy * sx, ey = hy * sy - hx * sx;  // engine x / y: forward and right
      m_cursor[0] += std::fabs(ex) > 0.38f ? (ex > 0 ? 1.0f : -1.0f) : 0.0f;
      m_cursor[1] += std::fabs(ey) > 0.38f ? (ey > 0 ? 1.0f : -1.0f) : 0.0f;
      m_moveWait = 8;
    }
    // ObjectManipulator::OnStart's button semantics (803FD970): pad command 5 -> interactor command 0
    // (A), 0x2E -> 1, 6 -> 4 (B), 0x13 -> 5 and 0x14 -> 6 (the L and R triggers).
    if (pressed(5)) OnCommandPressed(0);
    else if (pressed(0x2e)) OnCommandPressed(1);
    else if (pressed(6)) OnCommandPressed(4);
    else if (pressed(0x13)) OnCommandPressed(5);
    else if (pressed(0x14)) OnCommandPressed(6);
  }
  if (!m_active) return;
  cXObjectImpl* o = m_placement.m_object;
  // ClampPosToWorld: inside the lot's edge; a multi-tile object with all its parts.
  const float xMax = static_cast<float>(w->GetXSize() - 2), yMax = static_cast<float>(w->GetYSize() - 2);
  if (PlacementGroup* g = o && o->IsMultiTile() ? PlacementGroupOf(o) : nullptr) {
    PlacementGroup::Part* me = nullptr;
    for (auto& p : g->parts)
      if (p.object == o) me = &p;
    if (me)
      for (auto& p : g->parts) {
        const float px = m_cursor[1] + static_cast<float>(p.offset[0] - me->offset[0]);
        const float py = m_cursor[0] + static_cast<float>(p.offset[1] - me->offset[1]);
        if (px < 1.0f) m_cursor[1] += 1.0f - px;
        if (px > xMax) m_cursor[1] -= px - xMax;
        if (py < 1.0f) m_cursor[0] += 1.0f - py;
        if (py > yMax) m_cursor[0] -= py - yMax;
      }
  } else {
    m_cursor[1] = m_cursor[1] < 1.0f ? 1.0f : (m_cursor[1] > xMax ? xMax : m_cursor[1]);
    m_cursor[0] = m_cursor[0] < 1.0f ? 1.0f : (m_cursor[0] > yMax ? yMax : m_cursor[0]);
  }
  if (!o) return;
  int pos[2] = {static_cast<int>(m_cursor[0]) << 4, static_cast<int>(m_cursor[1]) << 4};
  bool legal = false;
  PlacementData data;
  CTilePt tile(pos[1] >> 4, pos[0] >> 4, 1);
  if (!w->OutOfBounds(tile)) {
    if (!o->GetContainer()) data.alignToSlot = 1;
    legal = m_placement.IsLegalToPlaceAtLocation(pos, data) != 0;
    m_placement.Place(pos, data);
  } else {
    m_placement.Pickup();
    m_placement.Place(pos, data);
  }
  m_placement.SetShaderToValidState(legal);
  static int s_lastError[2] = {-1, -1};
  const int error = legal ? 0 : gPlacementError;
  if (error != s_lastError[m_player & 1]) {
    s_lastError[m_player & 1] = error;
    if (error) std::printf("[buy] player %d: %s can't go at %d,%d (placement error 0x%x)\n", m_player + 1, o->DebugName(),
                           pos[1] >> 4, pos[0] >> 4, error);
  }
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// PC glue (see pcttarget.hpp)

bool StartPlaceManipulator(int player, unsigned int guid) {
  PlaceManipulator& m = s_manipulators[player & 1];
  s_grabbers[player & 1].m_active = false;
  m.m_player = player;
  m.m_guid = guid;
  m.m_placement = PlacementObject();
  m.m_keepDirection = 0;
  m.StartPlacement();
  if (!m.m_placement.m_object) return false;
  m.m_active = true;
  m.CursorBesideSim();
  ibuild::SetCameraFilter(player);  // ObjectManipulator::OnStart
  cXObjectImpl* o = m.m_placement.m_object;
  PlacementGroup* g = PlacementGroupOf(o);
  std::printf("[buy] player %d places %s (#%d%s) from tile %d,%d\n", player + 1, o->DebugName(), o->m_id,
              g ? (", " + std::to_string(g->parts.size()) + " parts").c_str() : "", static_cast<int>(m.m_cursor[1]),
              static_cast<int>(m.m_cursor[0]));
  return true;
}

void StopPlaceManipulator(int player) {
  if (s_manipulators[player & 1].m_active || s_grabbers[player & 1].m_active) ibuild::ClearCameraFilter(player);
  s_manipulators[player & 1].m_active = false;
  s_grabbers[player & 1].m_active = false;
  StopBuildTool(player);  // the build tools (interactors 3, 4, 5) too
}

// GC: InteractorManager::ChangeActiveInteractor to interactor 2 (the grab manipulator) with
// GrabManipulator::OnStart: the cursor kept from the place manipulator when asked (+0xC4), the
// PlacementObject initialised.
bool StartGrabManipulator(int player, bool keepCursor) {
  GrabManipulator& m = s_grabbers[player & 1];
  PlaceManipulator& place = s_manipulators[player & 1];
  place.m_active = false;
  m.m_player = player;
  m.m_placement = PlacementObject();
  m.m_placement.Initialize();
  if (keepCursor) {
    m.m_cursor[0] = place.m_cursor[0];
    m.m_cursor[1] = place.m_cursor[1];
  } else if (!m.m_active) {
    m.CursorBesideSim();
  }
  m.m_active = true;
  ibuild::SetCameraFilter(player);  // ObjectManipulator::OnStart
  std::printf("[grab] player %d grab mode at %d,%d\n", player + 1, static_cast<int>(m.m_cursor[1]), static_cast<int>(m.m_cursor[0]));
  return true;
}

namespace {
// The player's active object manipulator (the place one, else the grab one).
ObjectManipulator* ActiveManipulator(int player) {
  if (s_manipulators[player & 1].m_active) return &s_manipulators[player & 1];
  if (s_grabbers[player & 1].m_active) return &s_grabbers[player & 1];
  return nullptr;
}
// The object manipulator's resources (InteractorVisualizer::CreateResources(ObjectManipulator&): the
// cursor's two models). PC: held here, the manipulators not being InteractorModule::Interactors.
ERModel* CursorModel(int which) {
  static ERModel* s_models[2] = {};
  static const unsigned int kIds[2] = {0x8bc514d2, 0x4ebb9687};
  if (!s_models[which]) s_models[which] = static_cast<ERModel*>(_modelman.AddRef(kIds[which], nullptr, 0));
  return s_models[which];
}
}  // namespace

// 80216D08 InteractorVisualizer::PreDraw(ObjectManipulator&)
// The player's draw record at the manipulator's cursor, into the house's order table.
bool PlaceManipulatorPreDraw(int player, InteractorModule__InteractorVisualizer& v) {
  ObjectManipulator* m = ActiveManipulator(player);
  EOrderTableData* otd = v.m_otd[player & 1];
  InteractorModule__InteractorInfo tool;
  if (!m || !otd || BuildToolInfo(player, tool)) return false;
  m->m_drawPos = EVec3{m->m_cursor[0] + 0.5f, m->m_cursor[1] + 0.5f, 0.0f};  // the engine's frame (tile centre)
  otd->m_pos = &m->m_drawPos.x;
  if (EHouse* house = _globals.m_house) house->InsertHouseObject(otd);
  return true;
}

// 80216D64 InteractorVisualizer::Draw(ObjectManipulator&)
// With the cursor model on (nothing in hand): its two models at the cursor, the second half a tile up, in
// the interactor colour (Player::GetInteractorColor: 1, 1, 3). Then the object in hand's grid.
bool PlaceManipulatorDraw(int player, EOrderTableData* otd, InteractorModule__InteractorVisualizer& v) {
  ObjectManipulator* m = ActiveManipulator(player);
  if (!m || otd->m_pos != &m->m_drawPos.x) return false;
  EVec3 color{1.0f, 1.0f, 3.0f};  // Player::s_interactorColor (80485594)
  if (m->GetIsCursorModelEnabled()) {
    EMat4 mat;
    mat.Id();
    EVec3 at{m->m_drawPos.x, m->m_drawPos.y, 0.0f};
    mat.Translate(at);
    EMat4 first = mat;
    v.DrawLitModel(CursorModel(0), color, &first);  // 0x8BC514D2
    EVec3 up{0.0f, 0.0f, m->m_drawPos.z + 0.5f};
    mat.PostTranslate(up);
    v.DrawLitModel(CursorModel(1), color, &mat);  // 0x4EBB9687
  }
  // 802175EC InteractorVisualizer::Draw(PlacementObject&, EVec3&): the grid tile under each part, white
  // (red, 2 0 0, while the object can't go there: its instance's error light, flag 4), at the part's
  // tile plus the house's offset. GC: also the object itself (it is out of the world while carried; PC:
  // the object stands in the world at the cursor and the house draws it).
  PlacementObject& po = m->m_placement;
  cXObjectImpl* o = po.m_object;
  if (!o || !po.m_gridModel) return true;
  const bool invalid = o->m_instance && (o->m_instance->m_cursFlags & ISimInstance::kErrorLight);
  EVec3 light = invalid ? EVec3{2.0f, 0.0f, 0.0f} : EVec3{1.0f, 1.0f, 1.0f};
  EHouse* house = _globals.m_house;
  const float hx = house ? house->m_posY : 0.0f, hy = house ? house->m_posX : 0.0f;  // EHouse +0x4C, +0x50
  std::vector<cXObjectImpl*> parts;
  if (PlacementGroup* g = o->IsMultiTile() ? PlacementGroupOf(o) : nullptr) {
    for (auto& p : g->parts) parts.push_back(p.object);
  } else {
    parts.push_back(o);
  }
  for (cXObjectImpl* part : parts) {
    EMat4 mat;
    mat.Id();
    EVec3 at{static_cast<float>(part->m_location[0] >> 4) + 0.5f + hx, static_cast<float>(part->m_location[1] >> 4) + 0.5f + hy, 0.0f};
    mat.Translate(at);
    v.DrawLitModel(po.m_gridModel, light, &mat);
  }
  return true;
}

bool GrabManipulatorActive(int player) { return s_grabbers[player & 1].m_active; }
void GrabTryGrabbingWorldObjectFromId(int player, short id) {
  if (s_grabbers[player & 1].m_active) s_grabbers[player & 1].TryGrabbingWorldObjectFromId(id);
}

// GrabManipulator::DestroyObjectInHand for the player's grab manipulator (the sell / delete dialogs).
// The object in hand's selling value comes first (GrabObjectInHand).
void GrabDestroyObjectInHand(int player) { s_grabbers[player & 1].DestroyObjectInHand(); }
cXObjectImpl* GrabObjectInHand(int player) {
  const GrabManipulator& m = s_grabbers[player & 1];
  return m.m_active ? m.m_placement.m_object : nullptr;
}

bool PlaceManipulatorInfo(int player, InteractorModule__InteractorInfo& info) {
  if (BuildToolInfo(player, info)) return true;
  const ObjectManipulator* m = s_manipulators[player & 1].m_active ? static_cast<const ObjectManipulator*>(&s_manipulators[player & 1])
                               : s_grabbers[player & 1].m_active ? &s_grabbers[player & 1]
                                                                 : nullptr;
  if (!m) return false;
  info.m_x = m->m_cursor[0] + 0.5f;  // the engine's x is the tile's y (see InitPlayerPos)
  info.m_y = m->m_cursor[1] + 0.5f;
  info.m_w = info.m_h = 1.0f;
  return true;
}

bool UpdatePlaceManipulator(int player, float dt) {
  if (UpdateBuildTool(player, dt)) return true;
  if (s_manipulators[player & 1].m_active) {
    s_manipulators[player & 1].Update(dt);
    if (EHouse* house = _globals.m_house) s_manipulators[player & 1].UpdateObjectHighlights(house->m_objectMan);
    return true;
  }
  if (s_grabbers[player & 1].m_active) {
    s_grabbers[player & 1].Update(dt);
    // GC: the interactor manager has the active interactor light its objects after its update.
    if (EHouse* house = _globals.m_house) s_grabbers[player & 1].UpdateObjectHighlights(house->m_objectMan);
    return true;
  }
  return false;
}

// PC development aid: SIMMY_PLACE_TEST="<tick>:<action>;..." runs buy/build actions when the simulator
// reaches each tick (from SimulateOneTick), as the place / grab manipulators and the wall tool do:
//   <guid hex>@x,y[,dir]  a new object of that GUID (or object file name) put at the tile's middle (facing dir, eighths) through
//                         the placement path (Place where legal, then FinalUserPlaceObject: UserPlace)
//   grab#<id>             the object picked up by the player (UserPickup) and put back (ResetLocation)
//   wall@x,y,seg          the wall segment deleted (ERoom::DeleteWallAtTile), the rooms and house redone
//   create:<file>@x,y     an object made and put on the tile as TryCreateObject does (dynamic ones merge)
//   chain@x,y             the tile's object chain printed (the tile map's object and its siblings)
//   model#<hex>           the object files whose first ObjAnimDef draws that model printed
//   paperroom@x,y,i[,sell] the room at the tile papered with WallSet entry i (or stripped), wallpainter.cpp
void WallPainterRoomTest(int x, int y, int index, bool sell);  // wallpainter.cpp
int TestCreateObjectAt(const char* fileName, int x, int y);     // objectsim.cpp
void PlacementTestTick(int ticks) {
  static const char* spec = std::getenv("SIMMY_PLACE_TEST");
  if (!spec || !Globs_pObjectModule || !Globs_pFixedWorld) return;
  for (const char* p = spec; p && *p; p = std::strchr(p, ';') ? std::strchr(p, ';') + 1 : nullptr) {
    if (std::atoi(p) != ticks) continue;
    const char* a = std::strchr(p, ':');
    if (!a) continue;
    ++a;
    int x = 0, y = 0, z = 0;
    if (std::strncmp(a, "paperroom@", 10) == 0 && std::sscanf(a + 10, "%d,%d,%d", &x, &y, &z) == 3) {
      WallPainterRoomTest(x, y, z, std::strstr(a, ",sell") != nullptr && std::strstr(a, ",sell") < (std::strchr(a, ';') ? std::strchr(a, ';') : a + std::strlen(a)));
    } else if (std::strncmp(a, "create:", 7) == 0 && std::strchr(a, '@')) {
      const char* at = std::strchr(a, '@');
      const std::string name(a + 7, at);
      if (std::sscanf(at + 1, "%d,%d", &x, &y) == 2) {
        const int id = TestCreateObjectAt(name.c_str(), x, y);
        std::printf("[place] tick %d create %s at %d,%d: #%d\n", ticks, name.c_str(), x, y, id);
      }
    } else if (std::strncmp(a, "chain@", 6) == 0 && std::sscanf(a + 6, "%d,%d", &x, &y) == 2) {
      CTilePt t(x, y, 1);
      std::printf("[place] tick %d chain at %d,%d:", ticks, x, y);
      for (cXObjectImpl* o : TileObjects(t)) std::printf(" %s (0x1a %d 0xe %d)", o->DebugName(), o->GetData(0x1a), o->GetData(0xe));
      std::printf("\n");
    } else if (std::strncmp(a, "wall@", 5) == 0 && std::sscanf(a + 5, "%d,%d,%d", &x, &y, &z) == 3) {
      CTilePt t(x, y, 1);
      TileWalls walls = Globs_pFixedWorld->GetWall(t);
      const int guid = ERoom::DeleteWallAtTile(t, walls, z);
      Globs_pFixedWorld->ComputeRooms(0);
      _globals.RecalcHouse();
      std::printf("[place] tick %d wall %d,%d seg %d: %s (guid 0x%x)\n", ticks, x, y, z,
                  Globs_pFixedWorld->GetWall(t).HasWall(z) ? "kept" : "deleted", static_cast<unsigned int>(guid));
    } else if (std::strncmp(a, "model#", 6) == 0 && Globs_pObjectFolder) {
      // The object files whose first ObjAnimDef draws that model (hex id).
      const unsigned int model = static_cast<unsigned int>(std::strtoul(a + 6, nullptr, 16));
      for (ObjSelector* first : Globs_pObjectFolder->m_selectors)
        for (ObjSelector* sel = first; sel; sel = sel->m_next) {
          const unsigned char* res = sel->m_def ? objdef::ResData(sel->m_def) : nullptr;
          const unsigned char* defs = res ? objdef::AnimDefs(res) : nullptr;
          if (defs && objdef::AnimDefCount(defs) > 0 && ERQuickdata::U32(defs, 0) == model)
            std::printf("[place] model 0x%08x: %s (guid 0x%08x)\n", model, sel->m_fileName, static_cast<unsigned int>(sel->GetGUID()));
        }
    } else if (std::strncmp(a, "grab#", 5) == 0) {
      PlacementObject po;
      const int ok = po.GrabExistingObjectInstanceFromId(static_cast<short>(std::atoi(a + 5)));
      std::printf("[place] tick %d grab #%d: %d\n", ticks, std::atoi(a + 5), ok);
      if (ok) {
        po.ResetLocation();
        po.DestroyGridObject();
      }
    } else {
      unsigned int guid = static_cast<unsigned int>(std::strtoul(a, nullptr, 16));
      const char* at = std::strchr(a, '@');
      if (at && !std::isxdigit(static_cast<unsigned char>(*a)) && Globs_pObjectFolder) {
        // An object file's name instead of the GUID (e.g. scenery_tree_boxwood@10,4).
        const std::string name(a, at);
        for (ObjSelector* first : Globs_pObjectFolder->m_selectors)
          for (ObjSelector* sel = first; sel; sel = sel->m_next)
            if (sel->m_fileName && name == sel->m_fileName && !sel->GetIsMultiTileSubObject()) guid = static_cast<unsigned int>(sel->GetGUID());
      }
      int dir = 0;
      if (!at || std::sscanf(at + 1, "%d,%d,%d", &x, &y, &dir) < 2) continue;
      PlacementObject po;
      if (!po.CreateNewPlacementObjectFromGuid(guid, 0)) {
        std::printf("[place] tick %d: no object 0x%08x\n", ticks, guid);
        continue;
      }
      SetObjectDirection(dir, po.m_object);
      int pos[2] = {y * 16 + 8, x * 16 + 8};
      PlacementData data;
      const int legal = po.IsLegalToPlaceAtLocation(pos, data);
      if (legal) po.Place(pos, data);
      const int dropped = legal ? po.Drop() : 0;
      std::printf("[place] tick %d 0x%08x at %d,%d dir %d: legal %d (error 0x%x) dropped %d\n", ticks, guid, x, y, dir,
                  legal, gPlacementError, dropped);
      if (!dropped && po.m_object) po.Destroy();
    }
  }
}
