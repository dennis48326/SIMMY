// Port of game/iobjectman.cpp (EIObjectMan, partial): the house's 3D object instances, one per
// simulation object, by handle. Ported: making an object's instance (the model class from its
// definition), putting it in the house's level, orienting them and taking them out again. Still
// stubbed: the bound-rectangle display list, the room-visibility callbacks, portal lights, counting
// and the rectangle queries.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
constexpr unsigned int kDestinationGUID = 0x7c4;  // "TEMP destination": no model
}  // namespace

// 800549B0
EIObjectMan::EIObjectMan(EHouse* house) { m_house = house; }

// 80054A94
// GC: also frees the bound-rectangle display list.
EIObjectMan::~EIObjectMan() {
  m_items.clear();
  m_nextHandle = 0;
}

// 80054A08
// GC: records the unit rectangle display list (_pBoundRectDL) object bounds are drawn with.
void EIObjectMan::Init() { SIMMY_STUB("EIObjectMan::Init: the bound rectangle display list", 0x80054A08); }

// 80054DD8
void EIObjectMan::AttachObject(ISimInstance* instance) { m_items[m_nextHandle++] = instance; }

// 80054E14
// The object's instance, kept under the next handle and put in `level` (sky objects with the sky
// dome).
ISimInstance* EIObjectMan::AddObject(cXObject* object, ERLevel* level) {
  ISimInstance* instance = AllocSimsObjectInstance(object);
  if (!instance) return nullptr;
  m_items[m_nextHandle++] = instance;
  if (level) {
    if (!instance->m_digests.empty() && (instance->m_digests[0].m_flags & 0x2000)) level->InsertSkydomeInstance(instance);
    else level->InsertInstance(instance, nullptr);
    instance->InsertSubModelsInHouse(level);
    // Its room and hidden state through its object (PC: by the object's id, see ThisSide); vehicles
    // have no room.
    const unsigned int id = static_cast<unsigned short>(object->GetID());
    if (!object->IsVehicle()) {
      instance->m_thisSideFn = ThisSide;
      instance->m_otherSideFn = OtherSide;
      instance->m_otherSideData = id;  // GC: the instance (PC: its object's id, see OtherSide)
    }
    instance->m_thisSideData = id;
    instance->m_hiddenData = id;
    instance->m_hiddenFn = Hidden;
    // A window carries light between its two rooms: a portal light on each side.
    cXObjectImpl* impl = object->GetObjectImplementation();
    if (impl && IsPortalObject(object) && impl->GetTreeID(0xf) == 0) {  // cXObjectImpl::IsWindow (800EAD98)
      if (!instance->m_field14) instance->m_field14 = 1;
      for (int side = 0; side < 2; ++side) {
        auto* light = new EIPortalPointLight;
        (side ? instance->m_light2 : instance->m_light1) = light;
        light->SetPortal(instance, side != 0);
        level->InsertLight(light);
      }
    }
  }
  return instance;
}

// 80054FF4
void EIObjectMan::RemoveObjectsFromHouse(ERLevel* level) {
  for (auto& item : m_items) {
    level->RemoveInstance(item.second);
    item.second->RemoveSubModelsFromHouse(level);
    // PC: the lights the instance put in the level leave it too (GC: ~ISimInstance's RemoveLight on the
    // light's own level; a PC light has none, so the level kept freed window lights after a change of
    // house: the light grid used them).
    for (EILight* light : {item.second->m_light1, item.second->m_light2}) if (light) level->RemoveLight(light);
    if (auto* model = dynamic_cast<ISimsObjectModel*>(item.second)) {
      if (model->m_bulb) level->RemoveLight(model->m_bulb);
      for (EILight* light : model->m_animLights) level->RemoveLight(light);
    }
  }
  for (auto& item : m_items) delete item.second;
  m_items.clear();
}

// 800550C0
// An instance for objects with models (ObjDefinition::ResData): wall objects (ResData +5) and counter
// tops (+6), multi-tile objects, the boxwood hedge (a shrub), the rest plain; then created from the
// object, with a shadow unless its flags say none.
ISimInstance* EIObjectMan::AllocSimsObjectInstance(cXObject* object) {
  const ObjDefinition* def = object->GetDef();
  if (!def || objdef::GUID(def) == kDestinationGUID) return nullptr;
  const unsigned char* res = objdef::ResData(def);
  const unsigned char* animDefs = objdef::AnimDefs(res);
  if (!objdef::AnimDefCount(animDefs) || !ERQuickdata::U32(animDefs, 0)) return nullptr;
  const unsigned int model = ERQuickdata::U32(animDefs, 0);
  ISimsObjectModel* instance = nullptr;  // GC: 0x4C0 bytes, 16-aligned, cleared, from the main heap
  if (objdef::MasterID(def) == 0) {
    if (res[5] == 1) {
      instance = new ISimsWallObjectModel();
    } else {
      if (res[6] == 1) SIMMY_STUB("EIObjectMan::AllocSimsObjectInstance: ISimsCounterTopObject (as a plain object)", 0x800550C0);
      else if (model == EResourceManager::CalcId(const_cast<char*>("boxwood_hedge")))
        SIMMY_STUB("EIObjectMan::AllocSimsObjectInstance: IShrubObject (as a plain object)", 0x800550C0);
      instance = new ISimsObjectModel();
    }
  } else {
    object->IsMultiTile();  // (GC: called, its result unused)
    if (res[5] == 1) instance = new ISimsWallObjectModel();
    else if (res[6] == 1) {
      SIMMY_STUB("EIObjectMan::AllocSimsObjectInstance: ISimsCounterTopObject (as a multi-tile object)", 0x800550C0);
      instance = new ISimsMultiTileObjectModel();
    } else {
      instance = new ISimsMultiTileObjectModel();
    }
  }
  instance->Create(object, m_house);
  if (!(objdef::Flags(def) & 6)) instance->CreateShadow();
  return instance;
}

// 800553E0
void EIObjectMan::PostLoad() { ReOrientHouse(false); }

// 80055990
// Every object instance (not people) placed again from its object.
void EIObjectMan::ReOrientHouse(bool) {
  for (auto& item : m_items)
    if (!item.second->GetIsPerson()) item.second->SetObjOrient();
}

// 80054D54
// Out of the manager, then deleted (out of its level).
void EIObjectMan::FreeSimsObjectInstance(ISimInstance* instance) {
  for (auto it = m_items.begin(); it != m_items.end(); ++it)
    if (it->second == instance) {
      m_items.erase(it);
      break;
    }
  delete instance;
}
