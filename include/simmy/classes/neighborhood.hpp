// @class Neighborhood after CasSimDescriptionS2C ReconBuffer
// @class NeighborhoodImpl after CasSimDescriptionS2C ReconBuffer
// @class Neighbor after CasSimDescriptionS2C ReconBuffer WantFear__Bookmark
// @class RelMatrix after CasSimDescriptionS2C ReconBuffer
// @class RelMatrixImpl after CasSimDescriptionS2C ReconBuffer
// @class RelArray after CasSimDescriptionS2C ReconBuffer
// @class Family after CasSimDescriptionS2C ReconBuffer
// @class FamilyImpl after CasSimDescriptionS2C ReconBuffer
// @class PlayerFamilyImpl after CasSimDescriptionS2C ReconBuffer
// @class UserDataSaveLoad after CasSimDescriptionS2C ReconBuffer
// The neighbourhood (game/neighborhood.cpp, Globs::pNeighborhood): its neighbours (game/neighbor.cpp:
// every sim the game knows, by id, each with its person data, motives, relationships
// (game/relmatrix.cpp) and wants and fears), its families (game/family.cpp, game/playerfamilyimpl.cpp:
// members by GUID, the house they live in, funds; the player families also their inventory, goals and
// clothing unlocks) and its variables (the house being played, levels done...).
//
// All of it lives in the neighbourhood save file's "both" section (NghResFile): 'NGBH' the
// neighbourhood's own stream, 'NBRS' the neighbours, 'FAMI' a family each (id = number, | 0x4000 for a
// player family), 'User' a CAS-made sim's look each (ObjectFolderImpl::LoadUserData/SaveUserData:
// UserDataSaveLoad = GUID + CasSimDescriptionS2C, kept on the sim's user ObjSelector), 'TATT' the
// object type attributes. Each resource is a ReconBuilder stream (little-endian), version
// Globs::iSaveFileVersion (0x61; families 0x12).
//
// For other code (e.g. putting the family's sims on the lot): Globs_pNeighborhood->GetFamilyInHouse(
// house) (or GetFamily(_gd.m_fieldD8), the player's family), its CountMembers()/GetIndexedMember(i)
// GUIDs, FindNeighborByGUID(guid)->m_selector->m_simDesc (the sim's CasSimDescriptionS2C) and the
// selector's user name; NeighborhoodImpl::SimDescriptionOf(guid) does the lookup in one call.
#pragma once
#include <string>
#include <vector>

// ---------------------------------------------------------------------------------------------------
// Relationships

// One relationship row: the values towards the neighbour `m_key` (RelArray, 0x14 bytes on GC: a
// vector of RelInt, then the key at +0x10).
struct RelArray {
  void DoStream(ReconBuffer* buffer, int version);  // 8013532C

  std::vector<int> m_values;  // +0x00 RelInt
  int m_key = 0;              // +0x10 the other neighbour's id
};

struct RelMatrix {
  static RelMatrixImpl* CreateInstance();        // 80134A60
  static void DestroyInstance(RelMatrix* m);     // 80134AA8
  virtual ~RelMatrix() {}
};

// A neighbour's relationships (8 + a vector: 0x14 bytes on GC).
struct RelMatrixImpl : public RelMatrix {
  virtual RET_UNKNOWN Init();  // 80134B18
  virtual void CopyTo(RelMatrix &);  // 80134B20
  virtual int GetArraySize(int key);  // 80134BF0
  virtual void RemoveArray(int key);  // 80134C44
  virtual void SetArraySize(int key, int size);  // 80134D30
  void CreateNewArray(int key);  // 80134E14
  RelArray* FindArray(int key);  // 80134F8C
  virtual int GetValue(int key, int index);  // 80134FCC
  virtual void SetValue(int key, int index, int value);  // 80135008
  virtual void DoStream(ReconBuffer* buffer, int version);  // 801350D4
  virtual void Clear();  // 80135384
  ~RelMatrixImpl() override;  // 80135438
  virtual int CountKeys();  // 801354C0
  virtual int GetNthKey(int index);  // 801354D4
  RelMatrixImpl();  // 8013550C

  std::vector<RelArray*> m_arrays;  // +0x04
};

// ---------------------------------------------------------------------------------------------------
// Neighbor

// 0x1B8 bytes (GC: calloc'd). Person data (+0x70, 0x50 shorts) is what a person's object data
// becomes when it leaves the lot: [2..7] personality (nice, active, -, playful, outgoing, neat) x100,
// [9..0x12] skills, [0x14] aspiration, [0x18], [0x38] [0x39], [0x3A], [0x3F], [0x46] the star sign,
// [0x4A] flags, [0x2E..0x37] and [0x4C..0x4F] interests (Interests::SetInterests).
struct Neighbor {
  // A want or fear (WantFear::Bookmark, 0x0C bytes, wantfear.hpp).
  using Bookmark = WantFear__Bookmark;

  Neighbor();  // 800C8C48
  ~Neighbor();  // 800C8D00
  Neighbor(short id, ObjSelector* selector);  // 800C8D58
  Neighbor(Neighbor &);  // 800C8E10
  void operator=(Neighbor &);  // 800C8E94
  RelMatrixImpl* GetRelations() const;  // 800C90B4
  // The person data kept in the neighbour while the person is off the lot: {field, the person data
  // version it came with} (sPersistentFields, made on first use).
  struct PersDataPair {
    int m_field;
    int m_version;
  };
  static int GetNumPersistentDataFields();  // 800C926C
  static const PersDataPair* GetPersistentDataFieldsByIndex(int index);  // 800C9780
  static int GetLatestPersDataVersion();  // 800C9794
  void DoStream(ReconBuffer* buffer, int version);  // 800C979C
  int GetGUID();  // 800C9A8C
  void SetGUID(int guid);  // 800C9A94
  FamilyImpl* GetFamily();  // 800C9A9C
  bool IsCharacter();  // 800C9B50
  void RecalculateRelationshipData();  // 800C9BA8
  // Relationship types (tRelationshipType): 2 enemy, 3 dislike, 4 neutral, 6 friend, 7 best friend,
  // 8 crush, 9 lover.
  static bool RelCountsAsRel(int level, int type);  // 800C9E30
  static int CalculateRelationshipLevel(int value, int memory);  // 800C9F10
  int CalculateRelationshipLevel(Neighbor* other);  // 800CA024
  int CountRelationshipsOfType(int type);  // 800CA0E8
  int GetFriendCount();  // 800CA208
  int GetEnemyCount();  // 800CA244
  int GetReputationValue();  // 800CA280
  bool IsMarriedTo(Neighbor* other) const;  // 800CA2BC
  int GetRelationship(Neighbor* other, int* value);  // 800CA348
  // Whether it has a relationship with neighbour `id`, and its value (RelArray value 0).
  int GetRelationshipById(int id, int* value);  // 800CA388
  void AdjustRelationship(Neighbor* other, int value);  // 800CA424
  void RemoveAllRelationships();  // 800CA4D0
  void CopyRelationships(Neighbor *);  // 800CA508
  void LoadFromCharacter(ENeighborhoodCustomChar& c);  // 800CA604
  void SaveToCharacter(ENeighborhoodCustomChar& c);  // 800CA758
  double GetAspirationScore();  // 800CA924
  void SetAspirationScore(float score);  // 800CA950
  void ResetInterests();  // 800CA978
  void InitDefaultValues();  // 800CA99C
  void InitWantsFears();  // 800CAA40
  void InitWantFearIcons();  // 800CAAC8
  void SpawnWantFearDialog(unsigned int) const;  // 800CAB20
  // Tells its wants and fears: 0 nothing, 1 a bookmark moved on, 2 an active one was fulfilled (its
  // reward given; platinum ones tell the other neighbours when `tellOthers`).
  int Notify(WantFear__Notification& n, bool tellOthers);  // 800CAD10
  int FindDuplicateBookmark(const Bookmark& b) const;  // 800CAED4
  // Starts want tree `tree` (story trees replace the story wants shown): its root becomes a bookmark
  // and an active slot. False: no such tree, or its root is not valid.
  bool ForceNewTree(unsigned short tree);  // 800CAF58
  // The active slot showing bookmark `b` (an index), -1 (kNoActiveIndex) for none.
  int FindActiveSlotIndex(int b) const;  // 800CB2D0
  void AdvanceBookmark(unsigned int);  // 800CB32C
  static void GetSlotRange(bool fear, unsigned int& first, unsigned int& end);  // 800CB490
  static void GetBookmarkRange(bool fear, unsigned int& first, unsigned int& end);  // 800CB4BC
  int FindBookmarkToReplace(unsigned int first, unsigned int end);  // 800CB4E8
  int CalculateBookmarkAd(const Bookmark& b);  // 800CB5F4
  static double CalcAdMultiplier(float value, float from, float to);  // 800CB644
  // The lottery weight of an ad (AdData, 12 bytes) for this sim and `target` (0: none); 0: not wanted.
  int CalculateWantFearAd(const unsigned char* ad, short target);  // 800CB6A8
  // The bookmark (index) now in active slot `slot`, -1 for none.
  int MakeNewActiveBookmark(unsigned int slot);  // 800CBF84
  int SelectInactiveBookmark(bool fear, bool storyOnly);  // 800CC118
  void StartNewTree(unsigned int bookmark);  // 800CC380
  void UpdateUIifNeeded(unsigned int slot, int anim);  // 800CC418
  void ResetAllWantsAndFears();  // 800CC4E4
  void ShuffleWantFear(int slot);  // 800CC554

  // The active slot of bookmark `b` (-1: none); PC: by index (GC: FindActiveSlotIndex(Bookmark&)).
  int ActiveSlotOf(int b) const;

  int m_persVersion = 0;                // +0x00 the person data's version (0: none)
  short m_id = 0;                       // +0x04 1-based index in the neighbourhood's list
  int m_guid = 0;                       // +0x08 the sim's object GUID
  ObjSelector* m_selector = nullptr;    // +0x0C its object (a user selector for CAS sims)
  RelMatrixImpl* m_relations = nullptr; // +0x10
  int m_field14 = 0;                    // +0x14
  short m_friendCount = 0;              // +0x1C (RecalculateRelationshipData)
  short m_enemyCount = 0;               // +0x1E
  short m_reputation = 0;               // +0x20 the relationships' mean |value| x10, max 999
  short m_shownReputation = 0;          // +0x22 the reputation a player's sim shows
  int m_relationsDirty = 1;             // +0x24 (NeighborhoodImpl::RelationshipsChanged)
  char m_name[0x40] = {};               // +0x28 StringBuffer (buffer at +0x30): the object file's short name
  short m_pers[0x50] = {};              // +0x70 person data
  float m_motives[0x10] = {};           // +0x110 (Motives; [0] the aspiration score)
  float m_motives2[0x10] = {};          // +0x150
  Bookmark m_bookmarks[20];             // +0x194 (GC: new[]'d)
  int m_active[7] = {-1, -1, -1, -1, -1, -1, -1};  // +0x198 the active wants/fears (bookmark index, -1: none)
  int m_field1B4 = 0;                   // +0x1B4
};

// ---------------------------------------------------------------------------------------------------
// Families

struct Family {
  static FamilyImpl* CreateInstance(int& number);  // 800B5F28
  static void DestroyInstance(Family* f);          // 800B5F60
  virtual ~Family();                                // 800B5FA0
  Family();                                         // 800B6AEC
};

// 0x38 bytes.
struct FamilyImpl : public Family {
  explicit FamilyImpl(int number);  // 800B5FD4
  ~FamilyImpl() override;  // 800B6068
  virtual void CreateFamilyInLot();  // 800B6104
  virtual void RemoveFamilyFromLot();  // 800B61CC
  virtual void TestMember(cXPerson *) const;  // 800B62D0
  virtual int* GetMemberByGUID(int guid);  // 800B6334
  virtual bool TestMember(int guid) const;  // 800B6368
  virtual int* GetIndexedMember(int index);  // 800B63A4
  virtual void GetName(StringBuffer2 *);  // 800B63D8
  virtual int CountMembers();  // 800B6414
  virtual int GetFriendCount();  // 800B6428
  virtual int GetNetWorth();  // 800B6464
  virtual int LoadFamily(iResFile* file, int number);  // 800B64A0
  virtual void ClearFamily();  // 800B6530
  virtual void DoStream(ReconBuffer* buffer, int version);  // 800B6548
  bool LoadByResID(iResFile* file, short id, int* version);  // 800B6650
  virtual bool SaveFamily(iResFile* file, int version);  // 800B6754
  virtual void SetName(StringBuffer2 *);  // 800B67D4
  virtual RET_UNKNOWN MyDoCommand(short, int);  // 800B6810
  void AddMember(int guid);  // 800B6818
  virtual void RemoveMember(int guid);  // 800B69D4
  virtual void GetExportName(StringBuffer2 *);  // 800B6A88
  virtual short GetResfileStorageId(short number);  // 800B6B00
  virtual int GetNumber();  // 800B6B08
  virtual void SetNumber(int number);  // 800B6B10
  virtual int GetHouseNumber();  // 800B6B18
  virtual void SetHouseNumber(int house);  // 800B6B20
  virtual int GetCreationOrder();  // 800B6B28
  virtual void SetCreationOrder(int order);  // 800B6B30
  virtual int GetFunds();  // 800B6B38
  virtual void SetFunds(int funds);  // 800B6B40
  virtual int GetHouseValue();  // 800B6B60
  virtual void SetHouseValue(int value);  // 800B6B68
  virtual void SetFriendCount(int count);  // 800B6B70
  virtual void SetHasPhone(bool);  // 800B6B80
  virtual unsigned int GetHasPhone();  // 800B6BA0
  virtual void SetHasBaby(bool);  // 800B6BAC
  virtual unsigned int GetHasBaby();  // 800B6BCC
  virtual void SetJustMovedIn(bool);  // 800B6BD8
  virtual unsigned int GetJustMovedIn();  // 800B6BF8
  virtual void SetIsFreeplay(bool);  // 800B6C04
  virtual unsigned int GetIsFreeplay();  // 800B6C24
  void SetHasP1DoneCAS(bool);  // 800B6C30
  unsigned int GetHasP1DoneCAS();  // 800B6C50
  void SetHasP2DoneCAS(bool);  // 800B6C5C
  unsigned int GetHasP2DoneCAS();  // 800B6C7C
  virtual RET_UNKNOWN GetInventory();  // 800B6C88
  virtual RET_UNKNOWN GetGoalAndUnlockData();  // 800B6C90
  virtual int GetStatValue(int index) const;  // 800B6C98
  virtual int GetStatRef(int index, short** ref);  // 800B6CA0

  // PC: a player family (PlayerFamilyImpl).
  virtual bool IsPlayerFamily() const { return false; }

  enum : unsigned int { kHasPhone = 1, kHasBaby = 2, kJustMovedIn = 4, kFreeplay = 8, kP1DoneCAS = 0x10, kP2DoneCAS = 0x20 };

  std::u16string m_name;          // +0x04 BString2
  int m_number = -1;              // +0x08
  int m_house = 0;                // +0x0C 1..16, 0 none, -1 moving in (a new player family)
  int m_creationOrder = -1;       // +0x10
  int m_funds = 0;                // +0x14
  int m_houseValue = 0;           // +0x18
  int m_friendCount = 0;          // +0x1C
  int m_friendCountDirty = 1;     // +0x20
  std::vector<int> m_members;     // +0x24 FamilyMember: the members' GUIDs
  unsigned int m_flags = 0;       // +0x34
};

// 0xD68 bytes. PC: the parts kept as their streamed data (BBI::InventoryItems, GoalUnlock,
// CasClothingUnlocks are not ported as classes).
struct PlayerFamilyImpl : public FamilyImpl {
  // An inventory slot (BBI::InventoryItem's streamed part: category, subcategory, two bytes, a word).
  struct InventoryItem {
    signed char m_bytes[4] = {};
    int m_word = 0;
  };

  explicit PlayerFamilyImpl(int number);  // 8012CF68
  ~PlayerFamilyImpl() override;  // 8012CFD0
  void ClearFamily() override;  // 8012D038
  void DoStream(ReconBuffer* buffer, int version) override;  // 8012D0A8
  int GetStatValue(int index) const override;  // 8012D140
  int GetStatRef(int index, short** ref) override;  // 8012D160
  short GetResfileStorageId(short number) override;  // 8012D1A4
  virtual RET_UNKNOWN GetClothingUnlocks();  // 8012D1AC
  RET_UNKNOWN GetInventory() override;  // 8012D1B4
  RET_UNKNOWN GetGoalAndUnlockData() override;  // 8012D1BC
  virtual RET_UNKNOWN GetTotalAspirationScore() const;  // 8012D1C4
  virtual void SetTotalAspirationScore(int);  // 8012D1CC
  void ConvertToRegularFamilyOnWrite();  // 8012D1D4
  bool IsPlayerFamily() const override { return true; }

  bool m_haveItem[36] = {};                       // +0x38 BBI::InventoryItems (36 item pointers)
  InventoryItem m_items[36];
  signed char m_clothingUnlocks[2][0x500] = {};   // +0xC8 CasClothingUnlocks: per gender 32 x 40
  signed char m_tattooUnlocks[2][0xF0] = {};      //        then 8 x 30
  short m_stats[0x19] = {};                       // +0xCA8
  short m_goals[0x40] = {};                       // +0xCDC GoalUnlock (+4)
  int m_totalAspirationScore = 0;                 // +0xD60
  int m_convertOnWrite = 0;                       // +0xD64
};

// ---------------------------------------------------------------------------------------------------
// The neighbourhood

struct Neighborhood {
  static Neighborhood* CreateInstance();  // 800CC6D0
  static void DestroyInstance(Neighborhood* n);  // 800CC6F8
  virtual ~Neighborhood() {}
};

// 0xCC bytes.
struct NeighborhoodImpl : public Neighborhood {
  NeighborhoodImpl();  // 800CCAD8
  ~NeighborhoodImpl() override;  // 800CCB84
  virtual void RelationshipsChanged(Neighbor* n);  // 800CCCAC
  virtual void PostSim();  // 800CCD00
  virtual void LevelComplete(int level);  // 800CCD04
  void Unload();  // 800CCE18
  int Load(NghResFile* file);  // 800CCF44
  void UpdateFamilyNumbers();  // 800CD808
  virtual int Save(NghResFile* file, int version);  // 800CD930
  virtual void DoStream(ReconBuffer* buffer, int version);  // 800CDC2C
  virtual int GetHouseNumberForLevel(int);  // 800CDD38
  virtual Neighbor* FindNeighborByID(int id);  // 800CDD40
  // PC: HouseImpl::IsResident (800BC00C) for the port's house stand-in.
  bool IsResidentOfHouse(cXPersonImpl* p);
  virtual Neighbor* FindNeighborByGUID(int guid);  // 800CDD78
  virtual bool FindFamilyAndIndexByGUID(int guid, int& family, int& index);  // 800CDE00
  Neighbor* FindNeighborByType(ObjSelector* selector);  // 800CDEE0
  Neighbor* AddNewNeighbor(ObjSelector* selector);  // 800CDF30
  virtual void LoadPersistentData(cXPerson *);  // 800CE120
  virtual void RemoveNeighborRelationships(Neighbor* n);  // 800CE300
  void RemoveNeighbor(Neighbor* n);  // 800CE388
  virtual void SavePersistentData(cXPerson *);  // 800CE408
  // The neighbour's object selector (its user selector or its type's).
  virtual ObjSelector* GetNeighborSelector(int id);  // 800CE55C
  virtual int GetNextNeighborID(short id);  // 800CE5A4
  // Scope 0x18: data `data` of neighbour `id` (0 its object id on the lot, 1 lives here, 2 person data
  // 0x3A, 5/9 friends, 6 its family's house, 7/8 its family has a phone / baby, 10 reputation).
  virtual int GetNeighborData(short id, short data, short** ref);  // 800CE5F8
  virtual FamilyImpl* GetFamily(int number);  // 800CE968
  virtual FamilyImpl* GetFamilyInHouse(int house);  // 800CEA24
  virtual int GetNumFamilies();  // 800CEB20
  virtual FamilyImpl* GetFamilyByIndex(int index);  // 800CEB34
  int GetFamilyIndex(Family* f);  // 800CEB44
  virtual FamilyImpl* MakeNewFamily();  // 800CEBDC
  virtual FamilyImpl* MakeNewFreePlayFamily(int number);  // 800CEC04
  FamilyImpl* BaseMakeNewFamily(bool player, int number);  // 800CEC64
  virtual int RemoveFamily(Family* f);  // 800CEF48
  virtual int AddToFamily(Neighbor* n, Family* f);  // 800CF028
  virtual int RemoveFromFamily(Neighbor* n);  // 800CF1CC
  virtual void AddAsPlayerRoommate(Neighbor *);  // 800CF320
  virtual void RemovePlayerRoommate();  // 800CF408
  virtual int AddNewCharacter(Neighbor** out);  // 800CF54C
  // A CAS-made sim no family has goes: its people on the lot, its user selector and the neighbour.
  // 0: deleted, -1: not (not a CAS-made sim, or a family has it).
  virtual int DeleteCharacter(Neighbor* n);  // 800CF5C4
  virtual int GetNumCharacters();  // 800CF724
  virtual int GetFamilyFriendsCount(Family* f);  // 800CF794
  virtual int GetFamilyNetWorth(Family* f);  // 800CF7D0
  int MoveIn(Family* f, int house);  // 800CF90C
  void SetPlayerFamilyInHouse(int house);  // 800CF9B0
  virtual RET_UNKNOWN MoveOut(NghResFile *, int, bool);  // 800CFA4C
  void UpdateFamilyFriendsCount(Family *);  // 800D00AC
  virtual void PrepareAndTestLot(StringBuffer &);  // 800D01F8
  // A lot's price: its lot value, its objects' value and its architecture at 0.7.
  int GetHousePrice(cSimulatorImpl* simulator);  // 800D0304
  bool GetFamilyInfo(Family* f, FamilyInfo* info, bool friends);  // 800D03C0
  bool GetFamilyInfo(int number, FamilyInfo* info);  // 800D04C0
  // House `house`'s price, flags and family from its 'SIMI' (the file's current house is set to it).
  bool GetHouseInfo(NghResFile* file, int house, HouseInfo* info);  // 800D0520
  bool GetHouseInfo(NghResFile* file, HouseInfo* info);  // 800D05F8
  virtual bool GetHouseFileInfo(NghResFile* file, int* price, bool* global1A, bool* valued, bool* empty);  // 800D0640
  RET_UNKNOWN compareHouses(int &, int &);  // 800D078C
  virtual RET_UNKNOWN LoadHouse(NghResFile *, int, bool);  // 800D0874
  virtual RET_UNKNOWN SaveHouse(NghResFile *);  // 800D1110
  virtual int GetHouseNumber();  // 800D1294
  virtual void UnloadHouse();  // 800D129C
  virtual void GetLotPosition(int, int *, int *);  // 800D12A0
  virtual RET_UNKNOWN UpdateInstanceVisitorTypes();  // 800D12E0
  virtual int NotifyAllNeighbors(WantFear__Notification& n, Neighbor* except);  // 800D13D4
  virtual int NotifyAllResidents(WantFear__Notification& n, Neighbor* except);  // 800D146C
  virtual int NotifyAllPersons(WantFear__Notification& n, Neighbor* except);  // 800D15B0
  virtual const char16_t* GetNeighborhoodName();  // 800D1884
  virtual int GetHighestLevelCompleted();  // 800D18A8
  virtual int GetNeighborhoodVar(int index) const;  // 800D18B0
  virtual short* GetNeighborhoodVarRef(int index);  // 800D18C0
  virtual void SetNeighborhoodVar(int index, short value);  // 800D18D0
  virtual bool GetPlayerVisitingAnotherHouse() const;  // 800D18E0
  virtual int GetNumNeighborHouses();  // 800D1928
  virtual int GetNeighborHouseByIndex(int index);  // 800D193C
  virtual NeighborhoodImpl* GetImpl();  // 800D194C
  void SetHouseNum(int house);  // 800D1950
  static bool IsFreeplayLot(int house);  // 800D1958

  // PC: the look of the sim `guid` (its user selector's description; null for one without).
  CasSimDescriptionS2C* SimDescriptionOf(int guid);

  enum Var : int {
    kVarLevel = 0,          // the highest level completed
    kVarStory = 1,          // 1: a story mode game (not loaded: kept from the game being played)
    kVarHouse = 2,          // the house the player is in (Save)
    kVarPlayer1 = 4,        // player 1's neighbour id (Load)
    kVarEnteredHouse = 7,   // the house being entered (GameData::PlayerEnterHouse)
    kVarPlayer2 = 0x13,     // player 2's neighbour id
    kVarVisiting = 0x23,    // the house the player is visiting
    kVarCount = 0x24
  };

  char16_t m_name[0x20] = {};               // +0x04 StringBuffer2 (buffer at +0x0C)
  std::vector<int> m_neighborHouses;        // +0x4C
  std::vector<FamilyImpl*> m_families;      // +0x5C sorted by creation order
  short m_field6C = 0;                      // +0x6C (versions 0x5D on)
  short m_visiting = 0;                     // +0x6E kVarVisiting as loaded
  int m_houseNumber = 0;                    // +0x70 the house loaded (SetHouseNum)
  std::vector<Neighbor*> m_neighbors;       // +0x74 NeighborList, by id - 1 (null: a free id)
  short m_vars[kVarCount] = {};             // +0x84
};

extern NeighborhoodImpl* Globs_pNeighborhood;  // Globs::pNeighborhood (GC: a Neighborhood*; GetImpl)
extern int gMoneyForNewFamily;                 // a new family's funds (NeighborhoodConstants)
extern int Globs_iSaveFileVersion;             // 804FDBF8 Globs::iSaveFileVersion (0x61)

// ---------------------------------------------------------------------------------------------------
// The 'User' resource: a CAS-made sim's GUID and look (0x128 bytes on the stack on GC).
struct UserDataSaveLoad {
  void DoStream(ReconBuffer* buffer, int version);  // 800F239C

  int m_guid = 0;                // +0x00
  CasSimDescriptionS2C m_desc;   // +0x04 (names at +0x08 and +0x48)
};

// DoPtrVectorStream<T> (linkonce): the count, then per entry a "present" int and the entry's
// DoStream (entries made or deleted to match when reading).
template <class T>
void DoPtrVectorStream(std::vector<T*>& v, ReconBuffer* b, int version) {
  int n = static_cast<int>(v.size());
  b->Recon32(&n, 1);
  if (n < 0) n = 0;
  while (static_cast<int>(v.size()) > n) {
    delete v.back();
    v.pop_back();
  }
  v.resize(static_cast<size_t>(n), nullptr);
  for (T*& p : v) {
    int present = p != nullptr;
    b->ReconInt(&present, 1);
    if (!present) {
      delete p;
      p = nullptr;
      continue;
    }
    if (!p) p = new T;
    p->DoStream(b, version);
  }
}

// HoodManager::GetRelatedPeople (80044BCC; neighbor.cpp): the neighbours `person`'s neighbour has a
// relationship with, family first.
std::vector<Neighbor*> RelatedPeopleOf(cXPerson* person);
