// @class FamilyInfo
// @class ThumbnailLoader
// @class HouseInfo after FamilyInfo
// @class HouseRecon
// @class ObjectSaveTypeTable3 after HouseRecon
// @class ObjectSaveIDTable after HouseRecon
// The free-play side of the neighbourhood (game/hoodmanager.cpp, game/houserecon.cpp): moving a family
// into a lot, evicting it, bulldozing a lot, and the lot information the neighbourhood screen shows
// (NeighborhoodImpl::GetHouseInfo).
//
// A lot that is not being played is changed without loading it: HouseRecon reads only its object type
// table ('objt', ObjectSaveTypeTable3) and object id table (the head of 'ObjM', ObjectSaveIDTable), marks
// the types whose objects must go and writes their GUIDs as the house's 'DUMP' resource. The next time
// the house loads, ObjectModuleImpl::killDemolishedObjects kills its objects of those types. The
// simulator's lot data ('SIMI') and the world arrays ('Arry') are changed in place.
#pragma once
#include <string>
#include <vector>

// NeighborhoodImpl::GetFamilyInfo's record (0x114 bytes on GC).
struct FamilyInfo {
  std::u16string m_name;  // +0x00 StringBuffer2 (0x80 characters)
  int m_netWorth = 0;     // +0x108 GetNetWorth
  int m_friends = 0;      // +0x10C GetFriendCount (only when asked)
  int m_house = 0;        // +0x110
};

// NeighborhoodImpl::GetHouseInfo's record (0x12C bytes on GC; the three flags are words there).
struct HouseInfo {
  bool m_empty = false;    // +0x00 simulator global 0x1A clear and 0x15 clear (GetHouseFileInfo)
  bool m_global1A = false; // +0x04 simulator global 0x1A set
  bool m_valued = false;   // +0x08 simulator global 0x16 set (the lot's value was computed)
  int m_price = 0;         // +0x0C the lot's price (GetHousePrice)
  int m_family = -1;       // +0x10 the number of the family living there (-1: none)
  FamilyInfo m_familyInfo; // +0x14
  int m_house = 0;         // +0x128
};

// The house's object types as its 'objt' resource lists them (0x1C bytes each on GC, 0x800 of them).
struct HouseRecon__Entry {
  int m_guid = 0;                   // +0x00
  int m_initTree = 0;               // +0x04 (version 1 on)
  int m_mainTree = 0;               // +0x08
  short m_saveId = 0;               // +0x0C
  short m_type = -1;                // +0x0E (version 2 on)
  std::string m_name;               // +0x10 BString
  ObjSelector* m_selector = nullptr;// +0x14 the type's selector (ObjectFolder GetSelectorByGUID)
  int m_dump = 0;                   // +0x18 its objects go when the house next loads ('DUMP')
};

// 0x16008 bytes on GC (operator new'd by each HoodManager function).
struct HouseRecon {
  using Entry = HouseRecon__Entry;
  // An object of the house and its type (ObjectSaveIDTable's 8-byte record).
  struct IdRecord {
    short m_id = 0;
    Entry* m_entry = nullptr;
  };
  enum { kMaxEntries = 0x800, kMaxIds = 0x1000 };

  HouseRecon();  // 800451D0
  ~HouseRecon();  // 80045234
  // The entry of type `guid`, -1 for none.
  int findHRSelector(int guid);  // 800452A4
  // The house's type table and object ids, the types its 'DUMP' names marked.
  void LoadHouseData(iResFile* file);  // 800452E8
  // The marked types' GUIDs as the house's 'DUMP' (none: removed).
  void SaveHouseData(iResFile* file, int version);  // 800453EC

  std::vector<IdRecord> m_ids;   // +0x0000 (count +0x8000)
  std::vector<Entry> m_entries;  // +0x8004 (count +0x16004)
};

// ReconLoadObject<ObjectSaveTypeTable3> over 'objt' into a HouseRecon.
struct ObjectSaveTypeTable3 {
  void DoStream(ReconBuffer* buffer, int version);  // 80044E60
  HouseRecon* m_recon = nullptr;
};

// ReconLoadObject<ObjectSaveIDTable> over 'ObjM' (its head: the objects' ids and type ids).
struct ObjectSaveIDTable {
  void DoStream(ReconBuffer* buffer, int version);  // 80045084
  HouseRecon__Entry* findHRSel(short saveId) const;  // 8004516C
  HouseRecon* m_recon = nullptr;
};

extern float gArchValueMultiplier;  // NeighborhoodConstants "architecture value multiplier" (0.7)

// ReconLoadObject<ThumbnailLoader> over a 'thmb' resource: a sim's 32x32 portrait (thumbnail.cpp).
struct ThumbnailLoader {
  void DoStream(ReconBuffer* buffer, int version);  // 801118C4
  static RET_UNKNOWN CreateEmptyThumbnail();  // 80111B74
  static void DuplicateThumbnail(ETexture **, ETexture *);  // 80111D7C
  ObjSelector* m_selector = nullptr;
};
