// @class ObjSelector after ReconBuffer CTilePt ERQuickdata EMat4 SpriteSlot
// @class ObjectFolderImpl
// @class ObjectModuleImpl
// @class EdithVariableSet
// @class cXObject
// @class cXObjectImpl
// @class ObjectSaveTypeTable
// @class EIObjectMan
// @class ObjectIterator
// @class ObjectIterator__IterateType
// @class ObjectModule__SimFlag
// @class cXObject__MiscFlag
// The simulation's objects, as far as live mode draws them:
//  - the object database (game/objectfolder.cpp, Globs::pObjectFolder): an ObjSelector per object
//    definition of the "SimsObjects" quickdata (ObjDefinition rows: GUID, type, the models and states
//    in ObjDefinition::ResData), found by GUID;
//  - the house's objects (game/objectmodule.cpp, Globs::pObjectModule): read from the house's 'objt'
//    (the save's type table: saved type id -> GUID) and 'ObjM' (each object's id, type and state)
//    resources;
//  - the objects themselves (game/object.cpp: cXObject, the interface; cXObjectImpl): location,
//    attributes and the 72 object data variables (EdithVariableSet);
//  - their 3D instances in the house (game/iobjectman.cpp, EIObjectMan: ISimsObjectModel and kin).
// The behaviour side (the trees and primitives, the simulator, interactions) is in objectsim.cpp,
// treesim.cpp, person.cpp and their kin (see docs/simulation.md).
#pragma once
#include <map>
#include <string>
#include <vector>

namespace simmy {
struct ModObject;  // platform.hpp (PC mod: mods)
}

// ObjDefinition rows ("SimsObjects" quickdata, 0xC4 bytes, big-endian; pointer fields self-relative,
// see ERQuickdata). GC: an ObjDefinition struct read in place.
namespace objdef {
inline short Type(const ObjDefinition* d) { return ERQuickdata::S16(d, 0x12); }          // 2 person, 8 portal
inline short MasterID(const ObjDefinition* d) { return ERQuickdata::S16(d, 0x14); }      // multi-tile group
inline short SubIndex(const ObjDefinition* d) { return ERQuickdata::S16(d, 0x16); }      // its tile (-1 lead)
inline unsigned int GUID(const ObjDefinition* d) { return ERQuickdata::U32(d, 0x1c); }
inline short NPCFlag(const ObjDefinition* d) { return ERQuickdata::S16(d, 0x20); }
inline unsigned short Flags(const ObjDefinition* d) { return ERQuickdata::U16(d, 0xb6); }
inline short WallCutRow(const ObjDefinition* d) { return ERQuickdata::S16(d, 0xa2); }  // g_wallTable row - 1 (portals)
// ObjDefinition::ResData (0x38 bytes): +0x00 the ObjAnimDef table (0x20 each, count at [-4]), +0x05 a
// wall object, +0x06 a counter top.
inline const unsigned char* ResData(const ObjDefinition* d) {
  return static_cast<const unsigned char*>(ERQuickdata::Ptr(d, 0xc0));
}
inline const unsigned char* AnimDefs(const unsigned char* res) {
  return res ? static_cast<const unsigned char*>(ERQuickdata::Ptr(res, 0)) : nullptr;
}
inline unsigned int AnimDefCount(const unsigned char* defs) { return defs ? ERQuickdata::U32(defs - 4, 0) : 0; }
}  // namespace objdef

// ObjectModule's per-object sim flags (GetSimFlag): 1 enabled, 2 runs while paused, 4 waits for a reset.
struct ObjectModule__SimFlag {
  SIMMY_ENUM_BODY(ObjectModule__SimFlag)
};
// cXObjectImpl's misc flags (+0x84): 0x100 a tree error is pending (Error / HandleError).
struct cXObject__MiscFlag {
  SIMMY_ENUM_BODY(cXObject__MiscFlag)
};

// An object definition in the database (0xA8 bytes).
struct ObjSelector {
  ObjSelector();  // 801104DC
  ~ObjSelector();  // 801105A0
  RET_UNKNOWN IsPreloaded() const;  // 80110674
  void Writable();  // 80110754
  RET_UNKNOWN loadFile();  // 80110790
  RET_UNKNOWN GetEffectiveTreeTableID();  // 801107DC
  bool TestFromSameFile(ObjSelector *) const;  // 80110828
  RET_UNKNOWN GetMasterSelector();  // 80110840
  void GetOriginal();  // 8011089C
  unsigned int GetGUID() const;  // 801108DC
  void SetObjectName(char *);  // 801108E8
  static void ChangedDef();  // 801108EC
  RET_UNKNOWN GetCatalogName();  // 801108F0
  RET_UNKNOWN GetCatalogDescription();  // 8011094C
  RET_UNKNOWN GetCatalogShortName();  // 801109A8
  const std::u16string& GetUserName();  // 80110A2C
  void SetUserName(const std::u16string& name);  // 80110A80
  const std::u16string& GetUserLastName();  // 80110B04
  void SetUserLastName(const std::u16string& name);  // 80110B58
  void GetUserFullName(BString2 &);  // 80110BB4
  RET_UNKNOWN GetBigThumbnail(ERShader **);  // 80110C80
  bool GetThumbnail(ERShader **);  // 80110D44
  void SetThumbnail(ETexture *);  // 80110F24
  void DestroyThumbnail();  // 80111004
  RET_UNKNOWN GetThumbnailGraphicIndex();  // 80111054
  static RET_UNKNOWN GetThumbnailHasShadow();  // 80111060
  static RET_UNKNOWN GetRuntimeHasShadow();  // 80111090
  RET_UNKNOWN GetHasGraphics();  // 801110C0
  static RET_UNKNOWN GetHasInteractions();  // 801110F0
  RET_UNKNOWN GetIsMultiTileSubObject();  // 80111148
  bool GetIsPerson();  // 80111170
  RET_UNKNOWN GetShadowBrightness();  // 80111188
  RET_UNKNOWN GetInitTreeVersion();  // 80111194
  RET_UNKNOWN GetMainTreeVersion();  // 801111FC
  RET_UNKNOWN GetCatalogRating(int);  // 80111264
  RET_UNKNOWN GetFnTable();  // 801113AC
  RET_UNKNOWN GetCatalogResource();  // 8011148C
  void GetShortFilename(char* out, int capacity);  // 8011150C (GC: into a StringBuffer)
  RET_UNKNOWN CountTypeAttributes();  // 80111588
  RET_UNKNOWN GetTypeAttributes();  // 801115E8
  RET_UNKNOWN GetTreeTable();  // 80111648
  CasSimDescriptionS2C* GetNormalSimDescription() const;  // 801116A0
  // (GC: inline) the sim's own description: a CAS-made sim's (user selectors), else none.
  CasSimDescriptionS2C* GetSimDescription() const { return m_simDesc; }
  void ResetAnimTables();  // 801116E8
  RET_UNKNOWN GetAdultAnimTable();  // 80111744
  RET_UNKNOWN GetChildAnimTable();  // 801117A4
  RET_UNKNOWN GetAdultToChildAnimTable();  // 80111804
  RET_UNKNOWN GetChildToAdultAnimTable();  // 80111864

  int m_row = -1;                          // +0x04 the definition's row in the quickdata
  int m_row2 = -1;                         // +0x08
  const char* m_fileName = "";             // +0x18 the object file's name ("template_person" for CAS sims)
  const ObjDefinition* m_def = nullptr;    // +0x38 (GC: ObjectDataObjDefinition at +0x1C, its +0x1C)
  // A user selector's own definition (GC: a copy of the template person's ObjDefinition with the
  // GUID at +0x1C and the 'User' resource id at +0xBC; PC: m_def stays the template, these override).
  bool m_user = false;
  unsigned int m_userGuid = 0;
  short m_userResId = 0;
  std::u16string m_userName;               // +0x84 BString2*
  std::u16string m_userLastName;           // +0x88 BString2*
  CasSimDescriptionS2C* m_simDesc = nullptr;  // +0x8C a CAS-made sim's look (owned)
  // A service NPC's record (+0x90; cXPersonImpl::GetServiceNPC): its "Sim::NPC" row of the CAS quickdata
  // (big-endian: +0 gender, 0 male; +4 kind: 0 adult, 1 child, 2 dog, 3 cat, 4 monkey; +0x10 its name).
  const unsigned char* m_npc = nullptr;
  ERShader* m_thumbnail = nullptr;         // +0x94 the sim's thumbnail (GetThumbnail; held)
  ObjectFolderImpl* m_folder = nullptr;    // +0x50
  const void* m_resFile = nullptr;         // +0x54 its "ResFile" row
  int m_index = 0;                         // +0x70 selectors before it
  unsigned int m_flags = 0;                // +0x7C bit 1: saved with other tree versions
  short m_saveId = 0;                      // +0x6C its type id in the house being loaded ('objt')
  ObjSelector* m_next = nullptr;           // +0xA4 the next with the same GUID low byte
  // PC mod: mods (modobjects.cpp). A mod object's catalogue name and description (GetCatalogName /
  // GetCatalogDescription return their slots instead of the object file's) and its mod's name.
  std::u16string m_modName, m_modDesc;
  const unsigned short* m_modNameSlot = nullptr;
  const unsigned short* m_modDescSlot = nullptr;
  const char* m_mod = nullptr;
  // A mod surface's top over this selector's tile (tiles; -1: the base's slot heights) and its mod
  // object (counter pieces; the mod list is immutable after start-up).
  float m_modSurface = -1.0f;
  const simmy::ModObject* m_modObject = nullptr;
};

struct ObjectFolderImpl {
  ObjectFolderImpl();  // 800EC684
  virtual ~ObjectFolderImpl();  // 800EC84C
  virtual RET_UNKNOWN CountSelectors();  // 800ECA9C
  virtual ObjSelector* GetSelectorByGUID(int guid);  // 800ECAE4
  virtual ObjSelector* GetNextSelector(ObjSelector* after);  // 800ECB6C
  virtual RET_UNKNOWN GetSubTileSelector(ObjSelector *, int, int, int);  // 800ECD38
  virtual RET_UNKNOWN GetLeadSelector(ObjSelector *);  // 800ECE08
  virtual RET_UNKNOWN GetNthSubSelector(ObjSelector *, int);  // 800ECED8
  virtual RET_UNKNOWN GetQueueShaderByGUID(int);  // 800ECFA4
  virtual RET_UNKNOWN GetMasterSelector(ObjSelector *);  // 800ED028
  virtual void Init(char* path, bool user);  // 800ED0E4
  virtual void Destroy();  // 800ED1B4
  void DestroySelector(ObjSelector *);  // 800ED6C0
  ObjSelector* AddUserSelector(int guid, short resId);  // 800ED964
  static RET_UNKNOWN GetSemiGlobalID(unsigned int);  // 800EDD00
  virtual int AddSelector(ObjDefinition* def, char* name, char* fileName, ResFile* file, short, Sim__NPC* npc);  // 800EDD94
  virtual RET_UNKNOWN OpenResFile(ObjSelector *);  // 800EE148
  virtual void CloseResFile(ObjSelector *);  // 800EE20C
  void LoadDatabase();  // 800EE2B4
  void AddModObjects();  // PC mod: mods (modobjects.cpp): the selectors of the mods' objects
  virtual void ReconSelector(ObjSelector** selector, ReconBuffer* buffer, bool bySaveId, int version);  // 800EE98C
  virtual RET_UNKNOWN GetSelectorByBehavior(Behavior *);  // 800EEB60
  virtual void ReconBehavior(Behavior **, ReconBuffer *, int);  // 800EED2C
  virtual RET_UNKNOWN GetPersonGlobNamespaceID();  // 800EEE9C
  virtual RET_UNKNOWN GetFileByID(unsigned int);  // 800EF088
  virtual void LoadUserData(iResFile* file);  // 800EF184
  virtual void SaveUserData(iResFile* file);  // 800EF4FC
  virtual ObjSelector* CreateNewUserSelector();  // 800EF9E8
  virtual RET_UNKNOWN RemoveSelector(ObjSelector *);  // 800EFA48
  virtual RET_UNKNOWN ForceDataPreload(ObjSelector *, bool);  // 800EFB8C
  virtual void forceDataPreload(ObjSelector *, bool);  // 800EFCB8
  virtual void CreatingInstance(ObjSelector *);  // 800EFD20
  virtual void DeletingInstance(ObjSelector *);  // 800EFD90
  virtual void CreatingResFile(iResFile *);  // 800EFDC4
  virtual void DeletingResFile(iResFile *);  // 800EFDC8
  virtual void PrepareForModuleLoad(iResFile* file);  // 800EFDCC
  virtual void PrepareForModuleSave(iResFile *);  // 800F03E4
  virtual ObjSelector* GetPlaceholder();  // 800F0884
  virtual void FreeUnusedData();  // 800F08C0
  virtual void DeleteUserSelectors();  // 800F0B18
  virtual void DeleteSelectorAnimLists();  // 800F0F34
  void UnloadData(ObjSelector *);  // 800F111C
  // An object type's attribute values (ObjectTypeAttrBlock, 0x0C bytes).
  struct TypeAttrBlock {
    int m_guid = 0;                 // +0x00
    std::vector<short> m_values;    // +0x04 count, +0x08 values
  };
  virtual TypeAttrBlock* GetTypeAttrBlock(int guid);  // 800F1264
  virtual void Save(iResFile* file);  // 800F129C
  virtual void Load(iResFile* file);  // 800F12CC
  virtual void DoStream(ReconBuffer* buffer, int version);  // 800F12FC
  // The "RumbleDataElement" row named `name` of the objects' quickdata (RumbleDataElement: +0 the
  // first motor's strength, +2 the second's in 255ths, +4 seconds), or null.
  virtual const void* GetRumbleByName(char* name);  // 800F1520
  virtual RET_UNKNOWN GetCameraBloomByName(char *);  // 800F1578
  virtual RET_UNKNOWN GetCameraBlurByName(char *);  // 800F15D0
  virtual void GetAnimPreloadList(ChecksumList &);  // 800F165C
  virtual void SuspendObjectFiles();  // 800F1798
  virtual void ResumeObjectFiles();  // 800F179C
  virtual RET_UNKNOWN DoCommand(short, int);  // 800F17A0
  virtual void GetTreeTable(ObjSelector *);  // 800F17A8
  virtual RET_UNKNOWN LoadResourceByIndex(unsigned int, short, ObjectDataInterface &, unsigned int, unsigned int);  // 800F1A8C
  virtual RET_UNKNOWN LoadResourceByID(unsigned int, short, ObjectDataInterface &, unsigned int);  // 800F1B98
  virtual RET_UNKNOWN SaveResource(unsigned int, short, StringBuffer &, ObjectDataInterface &);  // 800F1C00
  virtual RET_UNKNOWN GetResourceCount(unsigned int, unsigned int, unsigned int);  // 800F1C44
  virtual void RemoveResourceByID(unsigned int, short, unsigned int);  // 800F1CB8
  virtual RET_UNKNOWN ChangeResourceID(unsigned int, short, short, unsigned int);  // 800F1DCC
  virtual RET_UNKNOWN GetNamespaceName(unsigned int, StringBuffer &);  // 800F1EE8
  virtual RET_UNKNOWN FindUniqueResourceName(unsigned int, unsigned int, StringBuffer &);  // 800F1F6C
  virtual RET_UNKNOWN FindUniqueResourceID(unsigned int, unsigned int, short &);  // 800F1FF0
  virtual RET_UNKNOWN IsNamespaceWriteable(unsigned int);  // 800F2074
  virtual RET_UNKNOWN IsBehaviorWriteable(unsigned int);  // 800F207C
  virtual void GetStringFromNamespaceID(unsigned int, StringBuffer &, short, short);  // 800F2084
  virtual RET_UNKNOWN ClearAllCaches();  // 800F2158
  virtual RET_UNKNOWN GetNamespaceSelector(unsigned int);  // 800F2480
  virtual void FlushNamespaceCaches();  // 800F2960
  virtual void GetPath();  // 800F29C4
  virtual RET_UNKNOWN GetBehaviorFinder();  // 800F29E8
  virtual RET_UNKNOWN GetObjectsDatabase();  // 800F29F0

  static int fGlobalFileID;  // ObjectFolder::fGlobalFileID: the "global" ResFile row (-1: none)

  int m_initialized = 0;                     // +0x38
  ObjSelector* m_selectors[0x100] = {};      // +0x40 by GUID & 0xFF
  std::vector<TypeAttrBlock*> m_typeAttrBlocks;  // +0x660 ('TATT')
  const ObjDefinition* m_templateDef = nullptr;  // +0x6B8 the template person's (user selectors copy it)
  short m_lastUserResId = 0;                 // +0x6BC the highest 'User' resource id
  std::vector<unsigned int> m_saveGuids;     // +0x670 the loading house's type ids -> GUIDs
  std::vector<unsigned int> m_unknownGuids;  // +0x680 saved types the database lacks
  ERQuickdata* m_objects = nullptr;          // +0x690 "SimsObjects" quickdata (0x0C33DB41)
  ERQuickdata* m_cas = nullptr;              // +0x694 CAS quickdata (0x2A2AF469)
  int m_templatePersonFile = -1;             // +0x698
  int m_user = 0;                            // +0x65C
};

// The 'objt' resource: per saved type its GUID, tree versions, save id, type and name.
struct ObjectSaveTypeTable {
  void DoStream(ReconBuffer* buffer, int version);  // 800EC08C
  ObjectFolderImpl* m_folder = nullptr;
};

// The house's objects.
struct ObjectModuleImpl {
  ObjectModuleImpl();  // 800F2CBC
  virtual ~ObjectModuleImpl();  // 800F2DCC
  virtual RET_UNKNOWN Save(iResFile *);  // 800F2FD4
  virtual int Load(iResFile* file);  // 800F30BC
  void killDemolishedObjects(iResFile *);  // 800F36E4
  virtual void PostLoad(iResFile *, int, bool);  // 800F39DC
  virtual RET_UNKNOWN SelectPlayerSims();  // 800F3A5C
  virtual void DoReconObject(ReconBuffer *, cXObjectImpl **);  // 800F3B1C
  virtual void DoReconPerson(ReconBuffer *, cXPersonImpl **);  // 800F3BF0
  virtual void DoStream(ReconBuffer* buffer, int version);  // 800F3C94
  virtual void Init();  // 800F45A8
  virtual void LoadGlobalRoutingSlots();  // 800F461C
  virtual void Destroy();  // 800F46C0
  virtual RET_UNKNOWN GetObjectByGUID(int);  // 800F48F8
  // The person on the lot whose selector has `guid` (PC: or whose neighbour has it).
  virtual cXPerson* GetPersonByGUID(int guid);  // 800F4A14
  virtual cXObject* ConstructObject(ObjSelector* selector, cXMTObject* lead);  // 800F4AB0
  // A new object of `selector` out of the world (location -1, -1 tiles), its instance made and its
  // behaviour started (init, then main); its id in `id`. 0: none.
  virtual int MakeNewOutOfWorldObject(ObjSelector* selector, short* id);  // 800F4BD0
  void KillOutOfWorldObject(short, bool);  // 800F50FC
  // The object goes: off its tile, out of the list, its instance freed (people holding it let go).
  virtual void KillObject(short id);  // 800F5668
  // The objects queued to die (AddToKillQueue) are killed.
  void KillQueueFlush();  // 800F587C
  virtual RET_UNKNOWN PostSim(bool);  // 800F59B4
  virtual void DayChanged();  // 800F5A98
  virtual void UpdateRooms(int);  // 800F5B10
  virtual void UpdateInteractionInfluences();  // 800F5DD0
  virtual void KillAllObjects();  // 800F5E78
  virtual void KillObjectsOutsideBounds(int, int, int, int);  // 800F60CC
  void UpdateSimObjects();  // 800F62DC
  virtual void AddToKillQueue(short id, bool now);  // 800F6690
  virtual void ClearKillQueue();  // 800F69B0
  virtual RET_UNKNOWN IsFamilyMemberAwakeAndVisible();  // 800F69BC
  virtual RET_UNKNOWN DoCommand(short, int);  // 800F6AB4
  virtual void PreviewAnimation(short, short, short, bool);  // 800F6C48
  virtual RET_UNKNOWN ForceAllLocations();  // 800F6DDC
  virtual RET_UNKNOWN GetAnObjectID(short);  // 800F6E50
  virtual int AddObject(cXObject* object, short id, bool);  // 800F719C
  void RemoveObject(cXObject *);  // 800F799C
  virtual void DisableBuyAndBuild(cXObject *);  // 800F7C6C
  virtual void EnableBuyAndBuild(cXObject *);  // 800F7E34
  virtual RET_UNKNOWN CheckIntegrity();  // 800F7F28
  virtual void ClearAwarenessOfObject(cXObject *);  // 800F8178
  virtual void CleanupPeople(cXObject *);  // 800F8214
  virtual void EnqueueObjectDialog(cXObject *, StackElem *, DialogParam *);  // 800F82C0
  virtual void EnqueueObjectDialog(ObjSelector *, DialogParam *);  // 800F82C4
  virtual void BroadcastMessage(char *, int);  // 800F82C8
  virtual void SendMessage(cXObject *, char *, int);  // 800F837C
  virtual RET_UNKNOWN UpdateWallAdjacencies();  // 800F8444
  virtual void InvalidateAllRoutes();  // 800F84D8
  virtual void SkillAccessed(cXPerson *, int, bool);  // 800F8550
  virtual void MotiveAccessed(cXPerson *, int, bool);  // 800F8554
  virtual void PersonalityAccessed(cXPerson *, int, bool);  // 800F8558
  virtual void RelationshipAccessed(cXObject *, cXObject *, int, bool);  // 800F855C
  virtual void RelationshipAccessed(Neighbor *, Neighbor *, int, bool);  // 800F8560
  RET_UNKNOWN OffsetObjectMap(CTilePt &);  // 800F85D8
  virtual void OffsetWorld(CTilePt &);  // 800F86F8
  virtual RET_UNKNOWN GetTutorialObject();  // 800F8C54
  virtual RET_UNKNOWN SetTutorialObject(cXObject *);  // 800F8C6C
  virtual void ShowTutorialInfo();  // 800F8CF8
  virtual void ComputeStats(int *, int *, bool *, bool *, bool *);  // 800F8D44
  virtual RET_UNKNOWN FillInObjectStats(RoomManager *, HouseStats &);  // 800F90E8
  virtual int IsSelectionSwitchingAllowed();  // 800F9290
  // `object` is one of the module's objects (GC: a walk of its object list).
  int IsValidObject(cXObject* object);  // 800F9310
  RET_UNKNOWN IsValidPerson(cXPerson *);  // 800F9384
  virtual RET_UNKNOWN AdvanceSelectedPerson(int);  // 800F9434
  virtual void GetSelectedPerson(int);  // 800F945C
  virtual void ClearIdleStatus(int);  // 800F9480
  // Per object id: sim flags (1 enabled, 2 runs while paused, 4 waits for a reset) and the ticks it
  // still idles (-1: none).
  virtual void SetSimFlag(int id, ObjectModule__SimFlag flag, bool on);  // 800F9578
  virtual int GetSimFlag(int id, ObjectModule__SimFlag flag);  // 800F95B0
  virtual int GetIdleStatus(int id);  // 800F95D8
  virtual void SetIdleStatus(int id, int ticks);  // 800F95F0
  virtual cXObject* GetObjectFromID(int id);  // 800F9620
  virtual int GetTileObjectID(CTilePt& tile);  // 800F9668
  virtual void SetTileObjectID(CTilePt& tile, short id);  // 800F96E4
  // PC: cXObjectImpl::HierSever / HierSetSite (800DD268 / 800DCFB8) for an object's tile: it gives up
  // the tile map entry it holds (the tile it was placed on, m_siteTile), or takes its current tile's.
  void SeverTile(cXObjectImpl* object);
  void SetSiteTile(cXObjectImpl* object);
  virtual RET_UNKNOWN CreateNeighbor(Neighbor *);  // 800F974C
  virtual RET_UNKNOWN GetObject(int);  // 800F97CC
  // The highest object id (GC: the object list's size and the people's slots).
  virtual int GetNumObjects();  // 800F9820
  virtual RET_UNKNOWN GetNextPerson(unsigned short);  // 800F9860
  virtual RET_UNKNOWN GetFolder();  // 800F9928
  virtual void AddObject(cXObject *, short);  // 800F9930
  virtual RET_UNKNOWN GetFirst();  // 800F9968
  virtual RET_UNKNOWN GetSim();  // 800F9980
  virtual RET_UNKNOWN GetPeople(int);  // 800F9988
  virtual RET_UNKNOWN GetNumPeople();  // 800F99A8
  virtual RET_UNKNOWN GetPortal(int);  // 800F99BC
  virtual RET_UNKNOWN GetNumPortals();  // 800F99CC
  virtual RET_UNKNOWN GetGlobalRoutingSlot(int);  // 800F99E0
  virtual RET_UNKNOWN GetNumGlobalRoutingSlots();  // 800F99F0
  virtual bool IsBuyAndBuildDisabled();  // 800F9A10
  virtual RET_UNKNOWN GetSimMemory(int, int, int);  // 8013FFB4
  virtual RET_UNKNOWN SetSimMemory(int, int, int, int);  // 80140028
  virtual RET_UNKNOWN GetDominantMemory(int, int, int);  // 80140144

  short m_tileMap[0x40 * 0x40] = {};     // +0x34 an object id per tile (x + 64 y)
  cXObjectImpl* m_first = nullptr;        // +0x2C the objects, linked through m_nextObject
  int m_loaded = 0;                       // +0x18
  std::vector<cXObject*> m_byId;          // +0x2090 by id - 1
  std::vector<unsigned int> m_simFlags;   // +0x1C by id - 1: flags << 16 | idle ticks
  std::vector<short> m_killQueue;          // +0x2088 objects to kill at the end of the tick
  int m_killingAll = 0;                    // +0x208C KillAllObjects runs
};

// An object's data variables (game/object.cpp): 72 shorts, of which only the ones set are kept (GC: a
// byte_key_map<72> with a 72-bit key set; +0x1E.. caches eight of them).
struct EdithVariableSet {
  EdithVariableSet();  // 800E3280
  EdithVariableSet(EdithVariableSet &);  // 800E32EC
  void Initialize(cXObjectImpl *);  // 800E33F4
  ~EdithVariableSet();  // 800E3FD4
  short& operator[](int index);  // 800E4040
  int WriteVar(int index, short value);  // 800E4558
  void ReconStream(ReconBuffer* buffer, int version);  // 800E583C
  RET_UNKNOWN Insert(pair<unsigned int, short> &);  // 800E5DBC
  RET_UNKNOWN RemoveInstanceVariable(unsigned char);  // 800E7778
  void ShrinkToExactSize(byte_key_map<72, short, allocator<short>> &);  // 800E7A5C
  void ReconOldFdata(ReconBuffer *, unsigned int);  // 800E7D30
  void Reset();  // 800E7F64
  RET_UNKNOWN Contains(int);  // 800E806C
  static RET_UNKNOWN GetCategoryVariableMap(unsigned short);  // 800E84FC
  static RET_UNKNOWN CreateCategoryVariableMap(unsigned short);  // 800E8794

  enum { kCount = 72 };
  // 804FDCAC: set while a save is read (WriteVar then does not tag the graphics).
  static int s_serializing;
  // 804FDCB0: data 0 (the graphic) was written; TryExpression tags the model of the object written
  // (TagGraphicsField) and clears it.
  static int s_bTaggedGraphicField;
  unsigned int m_keys[3] = {};  // +0x00 which are set
  short m_values[kCount] = {};  // GC +0x0C: the set ones, in key order
};

// The simulation's interface to an object (GC: an abstract class, a virtual base of the
// implementations). PC: the virtual methods live mode uses, defaulting to "nothing" as the GC's
// pure virtuals have no body.
struct cXObject {
  using MiscFlag = ::cXObject__MiscFlag;
  cXObject() = default;
  virtual ~cXObject();  // 800D9484
  static RET_UNKNOWN GetPersonWidth();  // 800D95EC
  static RET_UNKNOWN GetFreeWill();  // 800D95F4
  static RET_UNKNOWN GetAutoCenter();  // 800D95FC
  static void SetAutoCenter(bool);  // 800D9604
  static RET_UNKNOWN GetAutoReset();  // 800D960C
  static void SetAutoReset(bool);  // 800D9614
  static RET_UNKNOWN GetGlobalInitTreeID();  // 800D961C
  static RET_UNKNOWN GetGlobalMainTreeID();  // 800D9624
  static void SetFreeWill(bool);  // 800DB954
  static void SetGlobalFreeWillOverride(bool);  // 800DB9A0
  static RET_UNKNOWN GetWallBlockFlagsAtTile(CTilePt &, int);  // 800E28AC
  cXObject(int);  // 800EA894
  void setObjectImpl(cXObjectImpl *);  // 800EA98C
  void setPersonImpl(cXPersonImpl *);  // 800EA998
  void setMTObjectImpl(cXMTObjectImpl *);  // 800EA9A4
  void setPortalImpl(cXPortalImpl *);  // 800EA9B0
  RET_UNKNOWN CAST_IMPL();  // 800EA9BC
  virtual cXObjectImpl* GetObjectImplementation() { return nullptr; }  // vtable 0x524
  virtual int GetID() { return 0; }                                    // 0x32C
  virtual CTilePt GetCTilePt() const { return CTilePt(); }             // 0x35C
  virtual ObjSelector* GetSelector() { return nullptr; }               // 0x36C
  virtual const ObjDefinition* GetDef() { return nullptr; }            // 0x314
  virtual int GetData(int) { return 0; }                               // 0x264
  virtual bool IsRoof() { return false; }                              // 0x2AC
  virtual int IsMultiTile() { return 0; }                              // 0x394
  virtual cXObject* GetContainer() { return nullptr; }                 // 0x2D4
  virtual void ReconStream(ReconBuffer*, int, bool) {}                 // 0x4EC
  virtual bool IsVehicle() { return false; }                           // 0x4B4
  virtual int HaveGraphicsBeenTagged() { return 0; }                   // 0x4DC
};

// An object (0x158 bytes).
struct cXObjectImpl : public cXObject {
  cXObjectImpl(int, ObjSelector* selector, ObjectModule* module);  // 800D962C
  virtual void Initialize();  // 800DA3D8
  virtual void Reset(bool);  // 800DA848
  void ApplyModInitData();  // PC mod: mods (modobjects.cpp): a mod object's init_data after its init tree
  void JustBorn();  // 800DAC94
  virtual void PostLoad(int, bool);  // 800DADBC
  virtual void PreSave();  // 800DB168
  virtual void Cleanup(cXObject *);  // 800DB16C
  RET_UNKNOWN GetHilite();  // 800DB5C4
  void SetHilite(int);  // 800DB5D0
  RET_UNKNOWN GetCurrentValue();  // 800DB744
  void DayPassed();  // 800DB9A8
  void UpdateAge();  // 800DBB18
  ~cXObjectImpl() override;  // 800DBC54
  RET_UNKNOWN GetObstacleAtLocation(FTilePt &, int, bool);  // 800DC968
  RET_UNKNOWN GetRootObject(FTilePt &, int);  // 800DCA2C
  RET_UNKNOWN GetPlacementInfo(FTilePt *, int *, cXObject **, short *, bool *);  // 800DCACC
  RET_UNKNOWN IsInWorld();  // 800DCB50
  void GetPlacementSpec(PlacementSpec *);  // 800DCB78
  void GetNextObjectSibling();  // 800DCD4C
  RET_UNKNOWN GetPrevObjectSibling();  // 800DCD70
  void HierGetParent();  // 800DCF2C
  RET_UNKNOWN HierGetSlot(int);  // 800DCF84
  void HierSetSite(HierarchySite *);  // 800DCFB8
  void HierGetChild(int);  // 800DD114
  void HierGetSite(HierarchySite *);  // 800DD15C
  void HierSever();  // 800DD268
  RET_UNKNOWN HierGetObject(HierarchySite *);  // 800DD338
  bool IsContained();  // 800DD408
  RET_UNKNOWN GetContainerID();  // 800DD444
  RET_UNKNOWN GetContainedSlotNum();  // 800DD49C
  cXObject* GetContainer() override;  // 800DD4C8
  RET_UNKNOWN GetObjectSlot(int);  // 800DD578
  void ExtractContainedSims();  // 800DD5E4
  RET_UNKNOWN TestAndPlace(PlacementSpec *, bool);  // 800DD6E8
  void Pickup();  // 800DE144
  RET_UNKNOWN CanPlace(FTilePt &, int, cXObject *, int);  // 800DE340
  RET_UNKNOWN Place(FTilePt &, int, cXObject *, int);  // 800DE3C4
  RET_UNKNOWN GetSpriteSlot();  // 800DE780
  void UpdateChairFacing();  // 800DE788
  void UpdateChairFacing(ObjectModule *, int, int, int);  // 800DEAE4
  RET_UNKNOWN TestIntersection(FTilePt &, int);  // 800DEBB8
  bool IsPartOfMe(cXObject *);  // 800DF210
  static void ForceLocation();  // 800DF224
  void ComputeRect(FTilePt &, FTileRect *);  // 800DF4D0
  void SetLocation(FTilePt &, int);  // 800DF784
  void Turn(int);  // 800DFA9C
  double GetSlotHeight(int);  // 800DFBFC
  void GetTypeName(BString &);  // 800DFCEC
  // The distance in tiles to `other`.
  double CalcShortDistance(cXObject* other);  // 800DFE38
  double CalcShortDistanceIn16thsOfTiles(cXObject* other);  // 800DFEE0
  double CalcDistanceWithPentalties(cXObject *);  // 800E0080
  void CalcShortDistance(FTilePt *);  // 800E03A8
  void CalcDistance(cXObject *);  // 800E041C
  void ReconHeader(ReconBuffer* buffer, int version);  // 800E04CC
  void ReconStream(ReconBuffer* buffer, int version, bool resize) override;  // 800E05BC
  void ReconSlots(ReconBuffer* buffer, int version);  // 800E0BA8
  static RET_UNKNOWN ReconType();  // 800E0F34
  RET_UNKNOWN FindGoodLocation(FindGoodLocationParams &, FTilePt *);  // 800E0F40
  RET_UNKNOWN GetFrontFaceDirection();  // 800E1540
  void UpdateWallAdjacency();  // 800E1594
  static RET_UNKNOWN IsBeingDraggedAround();  // 800E1784
  RET_UNKNOWN GetTileWidth();  // 800E178C
  RET_UNKNOWN GetAgeInMinutes();  // 800E184C
  cXObjectImpl* GetInteractionLeader();  // 800E19D8 (mtobject.cpp)
  RET_UNKNOWN UserCanPickup();  // 800E1AF4
  RET_UNKNOWN UserCanDelete();  // 800E1CB8
  RET_UNKNOWN IsDeletedByEvict();  // 800E1D80
  void UserCanPlace(FTilePt &, int, cXObject *, int);  // 800E1E30
  RET_UNKNOWN UserPlace(FTilePt &, int, cXObject *, int);  // 800E1FF4
  static void UserPickup(bool);  // 800E2128
  RET_UNKNOWN IsFromCatalog();  // 800E225C
  RET_UNKNOWN GetWasPlacedFromInventory();  // 800E22C8
  RET_UNKNOWN SetWasPlacedFromInventory(bool);  // 800E22F8
  RET_UNKNOWN ShouldAutoRotate();  // 800E2364
  RET_UNKNOWN GetRequiredSegment();  // 800E2544
  // Autonomy may pick it (GC: cached in sim flag 8).
  bool CanChooseAutonomously();  // 800E2604
  void UpdateSimFlags();  // 800E2748
  RET_UNKNOWN GetWallBlockFlags();  // 800E2848
  RET_UNKNOWN GetAverageLocation() const;  // 800E2A68
  int GetLevel() const;  // 800E2AC4
  void SetLevel(int);  // 800E2ACC
  CTilePt GetCTilePt() const override;  // 800E2AD4
  RET_UNKNOWN IsSpriteVisible(short);  // 800E2B18
  RET_UNKNOWN SyncObjectIsReady(signed char);  // 800E2B70
  RET_UNKNOWN SetSyncObject(signed char);  // 800E2D34
  RET_UNKNOWN SyncObjectIsReady();  // 800E2E04
  int RunTree(int entry, short stackObject, short* params);  // 800E2F14 (entry: ObjEntryPoint)
  void SetDrawLabel(bool);  // 800E2FE4
  RET_UNKNOWN OverrideRenderPosition(float &, float &);  // 800E2FF8
  void SetOverrideRenderPosition(float, float);  // 800E301C
  void GetOverrideRenderPosition(float &, float &);  // 800E3030
  void ClearOverrideRenderPosition();  // 800E3044
  bool IsVehicle() override;  // 800E3050
  bool IsPerson();  // 800E3094
  RET_UNKNOWN GetColorIndex() const;  // 800E30D8
  RET_UNKNOWN SetColorIndex(unsigned char);  // 800E3118
  RET_UNKNOWN IsInteractionTile();  // 800E318C
  void GetNumTiles(int &, int &);  // 800E324C
  static void ContainsData(int);  // 800E325C
  static void SimObjectPlaced(ISimInstance *);  // 800EA6B0
  void Kill();  // 800EAA00
  RET_UNKNOWN GetNumAttr();  // 800EAA3C
  virtual void CastToObjectImpl();  // 800EAA44
  void SetMiscFlag(cXObject__MiscFlag flag, bool on);  // 800EAA48
  int GetMiscFlag(cXObject__MiscFlag flag);  // 800EAA68
  RET_UNKNOWN IsRenderingRoot();  // 800EAA80
  RET_UNKNOWN IsEmissive();  // 800EAABC
  RET_UNKNOWN GetSyncObject();  // 800EAB38
  void ClearSyncObject();  // 800EAB40
  RET_UNKNOWN WaitingForSync();  // 800EAB4C
  void RunTree(char *);  // 800EAB54
  static RET_UNKNOWN GetFreeWill();  // 800EABC4
  static RET_UNKNOWN GetAutoCenter();  // 800EABCC
  static void SetAutoCenter(bool);  // 800EABD4
  static RET_UNKNOWN GetAutoReset();  // 800EABDC
  static void SetAutoReset(bool);  // 800EABE4
  RET_UNKNOWN HierCountSlots();  // 800EABEC
  bool RequiresWallAdjacency();  // 800EAC00
  void GetFnTable();  // 800EAC64
  // The tree of entry point `entry` (0 init, 1 main, ...) from the selector's function table (OBJf).
  short GetTreeID(int entry);  // 800EAC88
  RET_UNKNOWN IsOccupied();  // 800EACE8
  RET_UNKNOWN IsPortal();  // 800EAD18
  RET_UNKNOWN IsWindow();  // 800EAD98
  RET_UNKNOWN IsDoor();  // 800EAE40
  void SetData(int, short);  // 800EAEE8
  void SetTemp(int, short);  // 800EAF0C
  void SetAttr(int, short);  // 800EAF1C
  static RET_UNKNOWN GetObjectProbe();  // 800EAF2C
  void SetObjectProbe(ObjectProbe *);  // 800EAF34
  RET_UNKNOWN GetFolder();  // 800EAF38
  bool SimIndependent();  // 800EAF44
  int SimEnabled();  // 800EAF80
  void EnableSim(bool on);  // 800EAFC0
  int GetIdleStatus();  // 800EB004
  void SetIdleStatus(int ticks);  // 800EB040
  void ClearIdleStatus();  // 800EB080
  RET_UNKNOWN GetRect();  // 800EB0BC
  int GetData(int index) override;  // 800EB0C4
  RET_UNKNOWN GetTemp(int);  // 800EB0EC
  RET_UNKNOWN GetAttr(int);  // 800EB0FC
  RET_UNKNOWN GetModule();  // 800EB10C
  void GetAdultAnimTable();  // 800EB114
  void GetChildAnimTable();  // 800EB138
  RET_UNKNOWN HideForCutaway();  // 800EB15C
  bool IsRoof() override;  // 800EB1D0
  RET_UNKNOWN CountObjectSlots();  // 800EB218
  // The object in its slot `slot` (PC: the one whose data 2 / 3 name it and the slot).
  cXObjectImpl* GetContainedObject(int slot);  // 800EB230
  RET_UNKNOWN GetRoom();  // 800EB2A0
  const ObjDefinition* GetDef() override;  // 800EB2CC
  int GetType();  // 800EB2D4
  int GetID() override;  // 800EB2E0
  void GetLocation(FTilePt *) const;  // 800EB2E8
  RET_UNKNOWN GetLocation();  // 800EB2FC
  void GetTreeTab();  // 800EB304
  ObjSelector* GetSelector() override;  // 800EB328
  ObjSelector* GetSelector() const;  // 800EB330
  Behavior* GetBehavior();  // 800EB338
  void GetSelFile();  // 800EB344
  int IsMultiTile() override;  // 800EB378
  RET_UNKNOWN GetFlags();  // 800EB394
  RET_UNKNOWN GetWallPlacementFlags();  // 800EB3C0
  // Its relationships to other objects (+0x60; made on first use, PC).
  RelMatrixImpl* GetRelMatrix();  // 800EB3EC
  RET_UNKNOWN GetNumRoutingSlots();  // 800EB3F4
  RET_UNKNOWN GetRoutingSlot(int);  // 800EB414
  RET_UNKNOWN GetSize();  // 800EB424
  void GetSim();  // 800EB450
  RET_UNKNOWN GetBuildModeType();  // 800EB488
  bool IsSupport();  // 800EB4B4
  int CanContributeLight();  // 800EB4F8
  int GetLightingContribution();  // 800EB528
  int GetObjectLightSource();  // 800EB554
  bool IsBroken();  // 800EB580
  bool IsDirty();  // 800EB5BC
  RET_UNKNOWN IsBurning();  // 800EB5F8
  RET_UNKNOWN CanBurn();  // 800EB628
  RET_UNKNOWN IsFireproof();  // 800EB658
  RET_UNKNOWN HasZeroExtent();  // 800EB688
  RET_UNKNOWN CanIntersectPeople();  // 800EB6C4
  bool IsChair();  // 800EB758
  static cXObject* GetObjectFromID(short id);  // 800EB7BC
  RET_UNKNOWN GetNext();  // 800EB7F4
  void GetFirst();  // 800EB80C
  RET_UNKNOWN GetNextImpl();  // 800EB844
  void GetFirstImpl();  // 800EB84C
  int HaveGraphicsBeenTagged() override;  // 800EB89C
  void TagGraphicsField();  // 800EB8C4
  RET_UNKNOWN GetName() const;  // 800EB8D0
  cXObjectImpl* GetObjectImplementation() override;  // 800EB8DC
  void SetOverrideDialogReturnCode(TreeReturnCode);  // 800EB8E0
  RET_UNKNOWN GetDebugName(char *, int) const;  // 800EB940
  virtual void Error(short code);  // 800EBA98
  void HandleError();  // 800EBB30
  // PC: the object's behaviour stack (GC: its TreeSimImpl base) and its error.
  int TreeSimError() const;
  void ClearTreeSimError();
  const char* DebugName();
  // PC: the behaviour started as cXObjectImpl::Reset does: the stack reset to the main tree, enabled.
  void StartBehavior();
  // PC: the behaviour carries on from the stack read from the save (StartBehavior without the reset).
  void ResumeBehavior();
  // PC: the tree stack popped a frame (GC: TreeSimImpl's vtable 0x1C, cXPersonImpl::StackJustPopped).
  virtual void TreeStackPopped() {}
  void GetErrorString(StringBuffer &);  // 800EBE38
  bool AllowIdleOptimization();  // 800F9E6C
  void HandleBreakpoint(StackElem *, BehaviorNode *);  // 800F9F20
  int TryUserEvent(StackElem* frame, const unsigned char* params);  // 800F9F44
  RET_UNKNOWN TryUIEffect(StackElem *, XPrimParam *);  // 800FA1E8
  int TryTestObjectType(StackElem* frame, const unsigned char* params);  // 800FA1F4
  int TryIsPersonInArea(StackElem* frame, const unsigned char* params);  // 800FA314
  RET_UNKNOWN UseKnownGoodLocation(TFGL_ACTIONS, short, FTilePt *, int, short);  // 800FAAA4
  // Primitive 0x10 (objectsim.cpp; GC: with the action, place, level and object as outputs).
  int TryFindGoodLocation(StackElem* frame, const unsigned char* params);  // 800FADF4
  int TrySetBalloon(StackElem* frame, const unsigned char* params);  // 800FBA00
  int TryCallNamedTree(StackElem* frame, const unsigned char* params);  // 800FBF14
  RET_UNKNOWN ParseUIString(BString2 &, StackElem *, short *, ObjSelector **, bool);  // 800FC7DC
  int TryMakeActionString(StackElem* frame, const unsigned char* params);  // 800FD890
  int TryPushAction(StackElem* frame, const unsigned char* params);  // 800FDAA8
  int TryFind5WorstMotives(StackElem* frame, const unsigned char* params);  // 800FE068
  int TryRelationship2(StackElem* frame, const unsigned char* params);  // 800FE28C
  int TryRelationship(StackElem* frame, const unsigned char* params);  // 800FEAD4
  RET_UNKNOWN TryTutorial(StackElem *, XPrimParam *);  // 800FF1DC
  int TryBurn(StackElem* frame, const unsigned char* params);  // 800FF520
  int TryCreateObject(StackElem* frame, const unsigned char* params);  // 800FFF24
  int TryPreloadObject(StackElem* frame, const unsigned char* params);  // 80100C84
  int TryDropOnto(StackElem* frame, const unsigned char* params);  // 80100D4C
  int TryBudget(StackElem* frame, const unsigned char* params);  // 80100F88
  int TrySetToNext(StackElem* frame, const unsigned char* params);  // 80101110
  int TryFindFunctionalObject(StackElem* frame, const unsigned char* params);  // 80102418
  int TryCallFunctionalTree(StackElem* frame, const unsigned char* params);  // 80102840
  int TryGenericSimCall(StackElem* frame, const unsigned char* params);  // 80102A24
  static RET_UNKNOWN ShowAllObjectDialogs(StackElem *, XPrimParam *);  // 801043A4
  static void TransitionToNewHouse(short, short, short);  // 80104EC0
  void RewriteDialogParamIndices(unsigned char* param);  // 8010502C
  int TryDialog(StackElem* frame, const unsigned char* params, bool testing);  // 8010515C
  static int sFreeWill;                // 804FDC78 the options' Free Will (cXObject::SetFreeWill)
  static int sGlobalFreeWillOverride;  // 804FDC7C (written only)
  static int overrideDialogReturnCode;  // 4 while no dialog answer is waiting
  RET_UNKNOWN TryShowString(StackElem *, XPrimParam *);  // 80105420
  int TryKillObject(StackElem* frame, const unsigned char* params, short* id);  // 8010553C
  void KillSelf(bool);  // 801056EC
  int TryIdle(StackElem* frame, const unsigned char* params);  // 80105780
  int TryUpdate(StackElem* frame, const unsigned char* params);  // 801058E0
  int TryGrab(StackElem* frame, const unsigned char* params);  // 80105AFC
  int TryTreeBreak(StackElem* frame, const unsigned char* params);  // 80105D70
  int TryRandom(StackElem* frame, const unsigned char* params);  // 80105DD4
  int TryRoomEffect(StackElem* frame, const unsigned char* params);  // 80105F54
  int TryCheckObject(StackElem* frame, const unsigned char* params);  // 80106270
  int TryGbaCommunication(StackElem* frame, const unsigned char* params);  // 80106568
  int TryAnimateObject(StackElem* frame, const unsigned char* params);  // 801065B0
  int TryCompleteGoal(StackElem* frame, const unsigned char* params);  // 80106798
  int InventoryCommandParam_GetGuid(const unsigned char* params);  // 801069B4
  int InventoryCommandParam_GetCount(const unsigned char* params);  // 80106A28
  int TryInventoryCommand(StackElem* frame, const unsigned char* params);  // 80106A8C
  int TryCameraControl(StackElem* frame, const unsigned char* params);  // 80106D7C
  int TryDistanceTo(StackElem* frame, const unsigned char* params);  // 80107CDC
  int TryDirectionTo(StackElem* frame, const unsigned char* params);  // 80107EA0
  int TryNotifyStackObject(StackElem* frame, const unsigned char* params);  // 8010816C
  RET_UNKNOWN TryParticleEffect(StackElem *, XPrimParam *);  // 80108384
  // Primitive 0x3F: a person's memory of another (eight 3-bit memories in relationship variable 2).
  int TryMemory(StackElem* frame, const unsigned char* params);  // 80108450
  int TryPassiveInfluence(StackElem* frame, const unsigned char* params);  // 80108EC8
  int TryWantsAndFearsNotify(StackElem* frame, const unsigned char* params);  // 80109048
  int TryChangeWantFearTree(StackElem* frame, const unsigned char* params);  // 80109264
  int TrySetMotiveDelta(StackElem* frame, const unsigned char* params);  // 80109344
  int TryAnimate(StackElem* frame, const unsigned char* params);  // 8010958C
  // One tick of its behaviour (the tree runs until a primitive waits), then the data 0x19 countdown.
  // 1: it ran (0: its tree stopped).
  virtual int Simulate(int ticks);  // 801096C4
  // Primitive `op` of `frame` with its node's operands (8 big-endian bytes).
  virtual int TryElement(StackElem* frame, short op, const unsigned char* params);  // 80109770
  RET_UNKNOWN TryKillSounds(StackElem *, KillSoundsParam *);  // 80109BEC
  RET_UNKNOWN TryPlaySound(StackElem *, PlaySoundParam *);  // 80109C30
  virtual int GosubObjectTree(cXObject* object, const short* params, short tree, bool setIcon);  // 80109DF0
  RET_UNKNOWN RunTree(Behavior *, short, char *, short *);  // 80109EC4
  int TryExpression(const unsigned char* params);  // 80109F54
  static void GetShortComboValue(signed char combo, short* scope, short* data);  // 8010AA24
  // Variable `data` of scope `scope`: its value, and where it lives (a short, or a float for motives)
  // when it can be written. 1: read, -1: an error (reported).
  int InterpValue(int scope, int data, short** ref, float** fref, short* value, short** extra = nullptr);  // 8010AAC8
  int InterpWriteValue(int scope, int data, short** ref, short value);  // 8010D160
  RET_UNKNOWN TryFindTreeNew(StackElem *, FindTreeNewParam *);  // 8010D3BC
  int TryDrop();  // 8010D3C8
  static void Backtrace();  // 8010D5AC
  int TrySnap(StackElem* frame, const unsigned char* params);  // 8010D5B0
  RET_UNKNOWN TrySnap(FTilePt, int, cXObject *, int, bool, int, bool);  // 8010E084
  RET_UNKNOWN TryHUDCreate(StackElem *, XPrimParam *);  // 8010E674
  RET_UNKNOWN TryHUDAddItem(StackElem *, XPrimParam *);  // 8010E8B4
  RET_UNKNOWN TryHUDManage(StackElem *, XPrimParam *);  // 8010EC30
  RET_UNKNOWN TryHUDData(StackElem *, XPrimParam *);  // 8010ED00
  static void ChangeSelectedSimL(int);  // 8010FAB0
  static void ChangeSelectedSimR(int);  // 8010FB10

  short* m_attrs = nullptr;               // +0x08 attributes
  int m_attrCount = 0;                    // +0x0C
  short* m_sprites = nullptr;             // +0x10 sprite visibility
  short m_spriteCount = 0;                // +0x14
  short m_temps[8] = {};                  // +0x16
  EdithVariableSet m_data;                // +0x28 the object data variables (GetData)
  cXObjectImpl* m_nextObject = nullptr;   // +0x5C the module's next object
  short m_id = 0;                         // +0x64
  int m_location[2] = {};                 // +0x68 FTilePt in 1/16 tiles: x, y
  int m_field70[4] = {};                  // +0x70 (ReconHeader)
  int m_level = 1;                        // +0x80
  int m_miscFlags = 0;                    // +0x84 (GetMiscFlag: 0x100 a tree error is pending)
  const ObjDefinition* m_def = nullptr;   // +0x88
  ObjSelector* m_selector = nullptr;      // +0x8C
  RelMatrixImpl* m_relations = nullptr;   // +0x60 its relationships to objects (from the save; RelMatrix::CreateInstance)
  std::vector<short> m_slotContents;      // +0x90 its object slots' contents (ObjectSlot +4: the id held; ReconSlots)
  int m_overrideRender = 0;               // +0xCC the render position is overridden (SetOverrideRenderPosition)
  float m_overrideRenderPos[2] = {};      // +0xD0 x, y
  int m_graphicsTagged = 0;               // +0xE8 the model must restart its animation (HaveGraphicsBeenTagged)
  ISimInstance* m_instance = nullptr;     // TreeSim +0x10: its 3D instance (EGlobal::AllocInstance)
  std::vector<SpriteSlot> m_spriteSlots;  // +0xB0 its sprite slots (people: one; sprites.hpp)
  TreeSimImpl* m_treeSim = nullptr;       // GC: the TreeSimImpl base (+0x00 points at it)
  int m_errorRepeats = 0;                 // PC: tree errors in a row (see HandleError)
  int m_lastErrorTick = 0;                // PC: the simulator tick of the last one
  int m_siteTile = -1;                    // PC: the tile map entry it holds (x + 64 y; GC: its HierarchySite)
};

// The house's 3D object instances (0x14 bytes), by handle (GC: an ERedBlackTree).
struct EIObjectMan {
  EIObjectMan(EHouse* house);  // 800549B0
  static RET_UNKNOWN IsValidID(unsigned int);  // 800549F4
  static void Init();  // 80054A08
  ~EIObjectMan();  // 80054A94
  RET_UNKNOWN Find(ISimInstance *);  // 80054B54
  static bool AllPlayersActiveInputInteractors();  // 80054B88
  void TurnOffAllHighlights(unsigned int);  // 80054C4C
  void GetObjectsInRect(int player, std::vector<ISimInstance*>& out);  // 80054CA0 (PC: a vector for the TNodeList)
  void GetObjects(TNodeList<ISimInstance *> &);  // 80054D08
  void FreeSimsObjectInstance(ISimInstance* instance);  // 80054D54
  void AttachObject(ISimInstance* instance);  // 80054DD8
  ISimInstance* AddObject(cXObject* object, ERLevel* level);  // 80054E14
  void RemoveObjectsFromHouse(ERLevel* level);  // 80054FF4
  bool IsEmpty();  // 800550B0
  ISimInstance* AllocSimsObjectInstance(cXObject* object);  // 800550C0
  void PostLoad();  // 800553E0
  void CountObjects(float &, float &, float &, float &);  // 80055404
  void ReOrientHouse(bool);  // 80055990

  unsigned int m_nextHandle = 0;                  // +0x00
  std::map<unsigned int, ISimInstance*> m_items;  // +0x04
  EHouse* m_house = nullptr;                      // +0x10
};

struct ObjectIterator__IterateType {
  SIMMY_ENUM_BODY(ObjectIterator__IterateType)
  enum : int { kAll = 0 };
};

// The objects at a tile: the one the module's tile map names, then (GC) the objects in its slots, depth
// first (0x0C bytes).
struct ObjectIterator {
  using IterateType = ::ObjectIterator__IterateType;
  ObjectIterator() = default;
  cXObject* init(CTilePt& tile, ObjectIterator__IterateType type);  // 800F29F8
  ObjectIterator(cXObject* object, ObjectIterator__IterateType type);  // 800F2A6C
  bool operator==(const ObjectIterator& other) const;  // 800F2A80
  ObjectIterator& operator++();  // 800F2AC0

  cXObject* m_root = nullptr;     // +0x00
  cXObject* m_current = nullptr;  // +0x04 null at the end
  int m_type = 0;                 // +0x08
};

// A cXPortal (TreeSim::_dyncastimpl kind 6): objects ObjectModuleImpl::ConstructObject makes portals of,
// the doors and windows (multi-tile, type 8).
inline bool IsPortalObject(cXObject* object) {
  const ObjDefinition* def = object ? object->GetDef() : nullptr;
  return def && objdef::MasterID(def) != 0 && objdef::Type(def) == 8;
}

extern ObjectFolderImpl* Globs_pObjectFolder;  // Globs::pObjectFolder
extern ObjectModuleImpl* Globs_pObjectModule;  // Globs::pObjectModule
