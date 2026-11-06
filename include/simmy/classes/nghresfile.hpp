// @class Memory__HandleNode after NghResFile__RegistryID NghResFile__SectionID
// @class NghResFile
// @class iResFile
// The neighbourhood save file (game/nghresfile.cpp) behind the simulation's resource file interface
// (iResFile, game/iresfile.cpp): the save's sections (neighbourhood, families, characters, the 16
// houses) are LZSS-compressed blocks at fixed offsets (NghLayout); a loaded section's resources (by
// type: 'Arry' world arrays, 'objt'/'ObjM' objects...) are kept in lists per section, as
// Memory::HandleNodes the readers get by type and id.
#pragma once
#include <string>
#include <vector>

// A loaded resource's bytes (Memory::HandleNode, 0x0C bytes).
struct Memory__HandleNode {
  unsigned int m_size = 0;          // +0x00
  unsigned char* m_data = nullptr;  // +0x04
  int m_owned = 1;                  // +0x08 the bytes are freed with the node
};

struct iResFile {
  using OpenFlags = ::iResFile__OpenFlags;
  void Link();  // 8013553C
  void Unlink();  // 8013554C
  iResFile();  // 80135588
  virtual ~iResFile();  // 801355D8
  int GetError();  // 80135624
  void SetError(int error);  // 8013562C
  void Release(Memory__HandleNode* handle);  // 80135634
  void Open(StringBuffer &, iResFile__OpenFlags);  // 801356B4
  virtual RET_UNKNOWN GetLanguage(Memory__HandleNode *);  // 8013579C
  virtual RET_UNKNOWN AddWithLanguage(Memory__HandleNode *, int, short, StringBuffer &, char, bool);  // 801357D8
  virtual RET_UNKNOWN GetByIDAndLanguage(int, short, char, void (*)(void *, int));  // 80135850
  virtual RET_UNKNOWN _dyncastimpl(SCID);  // 801358A8
  RET_UNKNOWN GetResFileData() const;  // 801358B8
  void SetResFileData(ResFile *);  // 801358C0
  // The implementations' interface (GC: pure virtuals of iResFile; PC: no-op defaults for the
  // implementations not ported yet).
  // The resource `id` of `type`, its words byte-swapped by `swizzle` as needed (0: none).
  virtual Memory__HandleNode* GetByID(int, short, void (*)(void*, int)) { return nullptr; }
  // The file's data is little-endian (the GameCube's own saves are big-endian).
  virtual int IsLittleEndian(Memory__HandleNode*) { return 0; }
  virtual int Add(Memory__HandleNode*, int, short, StringBuffer&, bool) { return 0; }
  virtual int Close() { return 0; }
  virtual int Count(int) { return 0; }
  virtual Memory__HandleNode* GetByIndex(int, short, void (*)(void*, int)) { return nullptr; }
  virtual void GetID(Memory__HandleNode*, short* id) { *id = 0; }

  int m_error = 0;  // +0x08
};

struct NghResFile : public iResFile {
  using RegistryID = ::NghResFile__RegistryID;
  using RegistryNode = ::NghResFile__RegistryNode;
  using SaveRecord = ::NghResFile__SaveRecord;
  using SectionID = ::NghResFile__SectionID;
  // A resource in a section's list (0x0C bytes).
  struct ResNode {
    ResNode* m_next = nullptr;
    Memory__HandleNode* m_handle = nullptr;
    unsigned short m_id = 0;
  };
  NghResFile();  // 800D39E4
  ~NghResFile() override;  // 800D3A98
  virtual RET_UNKNOWN Create(StringBuffer &);  // 800D3B94
  virtual RET_UNKNOWN Delete(StringBuffer &);  // 800D3BB8
  virtual RET_UNKNOWN Open(StringBuffer &);  // 800D3BC0
  virtual RET_UNKNOWN CloseForReopen();  // 800D3BC8
  virtual RET_UNKNOWN Reopen();  // 800D3BD0
  int Close() override;  // 800D3BD8
  virtual void Update();  // 800D3BE0
  virtual RET_UNKNOWN Writable();  // 800D3BE4
  virtual void GetFileName(StringBuffer &);  // 800D3BEC
  virtual RET_UNKNOWN ValidFile();  // 800D3C18
  virtual RET_UNKNOWN CountTypes();  // 800D3C20
  virtual RET_UNKNOWN GetIndType(short);  // 800D3C28
  int Count(int type) override;  // 800D3C30
  Memory__HandleNode* GetByID(int type, short id, void (*swizzle)(void*, int)) override;  // 800D3C74
  virtual RET_UNKNOWN GetByName(int, StringBuffer &, void (*)(void *, int));  // 800D3CC8
  // The `index`th (1-based) resource of `type`; GetID then gives its id.
  Memory__HandleNode* GetByIndex(int type, short index, void (*swizzle)(void*, int)) override;  // 800D3CD0
  virtual void GetName(Memory__HandleNode *, StringBuffer &);  // 800D3D3C
  virtual RET_UNKNOWN GetResType(Memory__HandleNode *);  // 800D3D40
  void GetID(Memory__HandleNode* handle, short* id) override;  // 800D3D48 (of the last GetByIndex)
  virtual void GetIndex(Memory__HandleNode *, short *);  // 800D3D58
  virtual void FindUniqueName(int, StringBuffer &);  // 800D3D5C
  virtual RET_UNKNOWN FindUniqueID(int);  // 800D3D60
  virtual void Detach(Memory__HandleNode *);  // 800D3D68
  virtual void Load(Memory__HandleNode *);  // 800D3D6C
  int IsLittleEndian(Memory__HandleNode* handle) override;  // 800D3D70
  virtual void SetID(Memory__HandleNode *, short);  // 800D3D78
  int Add(Memory__HandleNode* handle, int type, short id, StringBuffer& name, bool replace) override;  // 800D3D7C
  virtual void Write(Memory__HandleNode *);  // 800D3F6C
  virtual void Remove(Memory__HandleNode *);  // 800D3F70
  virtual void SetInfo(Memory__HandleNode *, short, StringBuffer &, char);  // 800D3F74
  void SetCurrentHouse(unsigned int house);  // 800D3F78
  void FlushHouseData(unsigned int);  // 800D3F84
  void FlushCharacterData(bool);  // 800D3FE0
  void FlushNeighborData();  // 800D407C
  void FlushAllData();  // 800D40D0
  void init();  // 800D4148
  void reset();  // 800D4258
  ResNode** findListByResType(unsigned int type);  // 800D4348
  int PrepareMemCardWrite(char* name, unsigned char card);  // 800D43DC
  int FinishMemCardWrite(char* name, unsigned char card);  // 800D43FC
  // PC: the section being built is a byte vector; the tables (types, offsets, sizes, ids) are
  // written big-endian at their cursors, as the GameCube held them.
  void WriteList(std::vector<unsigned char>& section, unsigned int type, unsigned int& types, unsigned int& offsets,
                 unsigned int& sizes, unsigned int& ids, unsigned int& dataPos, unsigned int& tableBytes);  // 800D442C
  bool IsSectionLoaded(unsigned int section) const;  // 800D4558
  void FlushSection(unsigned int section);  // 800D4614
  // PC: the section's bytes (empty when it could not be read); `fromCache`: they came from the memory
  // card cache (GameCube byte order) rather than the .ngh file (little-endian header and tables).
  std::vector<unsigned char> ReadSection(unsigned int offset, unsigned int size, bool& fromCache, int& status);  // 800D47E4
  // PC: the header (made host-endian) and the decompressed data after it; empty when the checksum,
  // version or data are bad. `section` is as stored; `fromCache` as for ReadSection.
  std::vector<unsigned char> DecompressSection(const unsigned char* section, bool fromCache);  // 800D4960
  int ValidateSaveGame(char* name, int card, bool valid);  // 800D4BBC
  static RET_UNKNOWN IsSaveGameValid(char *, int, bool &);  // 800D4CFC  [ported]
  int ReadHostSection(const unsigned char* section, bool fromCache);  // 800D4E5C
  int ReadBothSection(const unsigned char* section, bool fromCache);  // 800D51A8
  int GetThumbIdFromFamilyAndIndex(int family, int index);  // 800D53E4
  void StoreFamilyThumbGUID(int family, int index, unsigned int guid);  // 800D541C
  void StoreThumbGUID(unsigned int id, unsigned int guid);  // 800D545C
  unsigned int GetThumbGUID(unsigned int id);  // 800D5470
  short GetResIDFromThumbGUID(unsigned int guid);  // 800D5484
  int ReadThumbSection(const unsigned char* sections, bool fromCache);  // 800D54C0
  int WriteHostSection(void* out);  // 800D56CC
  int WriteBothSection(void* out);  // 800D5A40
  int WriteThumbSection(void* out);  // 800D5DC4
  RET_UNKNOWN WriteToMemoryBlock(void *&, unsigned int &);  // 800D60A0
  static RET_UNKNOWN XBoxReadFromMemoryBlock(void *, unsigned int);  // 800D6204
  unsigned int LoadHouseContents(unsigned int house, void* data, bool fromCache);  // 800D62EC
  int SaveHouseContents(unsigned int house, void* out);  // 800D6774
  void FlushHouseContents(unsigned int house);  // 800D6B40
  void FlushAllHouseContents();  // 800D6BC4
  bool AreHouseContentsLoaded(unsigned int house) const;  // 800D6C08
  int ReloadHouseAndGlobals(unsigned char card, unsigned int house);  // 800D6C34
  int SaveHouseAndGlobals(unsigned char card, unsigned int house);  // 800D6D04
  static int ReadChecksumFromCard(char* name, int card);  // 800D6DBC
  int ReadCharacterData(char* name, unsigned char card);  // 800D6E94
  int WriteCharacterData();  // 800D6F64
  RET_UNKNOWN WriteToFile(char *);  // 800D6FF4
  RET_UNKNOWN WriteToFileInMemoryBlocks(char *);  // 800D70A4
  int ReadFromFile(char* name);  // 800D74F0
  int ReadFromCache();  // 800D77B0
  int WriteToMemoryCard(char* name, unsigned char card);  // 800D786C
  int HasCardChanged(int card, bool& changed);  // 800D7A30
  RET_UNKNOWN CopyDefaultNghToCache(char *);  // 800D7C20
  static RET_UNKNOWN CopyDefaultNghToCard(char *, char *, unsigned char);  // 800D8330
  int ReadFromMemoryCard(char* name, unsigned char card);  // 800D85E4
  RET_UNKNOWN CopyHouse(int, NghResFile &, int);  // 800D86E8
  void GetDefaultNghPath(char *, StackString<260> &);  // 800D8954
  void SetFileName(const char* name);  // 800D89E0
  MemoryCardCache* GetMemoryCardCache();  // 800D8A28
  void SwapTwoByteNumber(unsigned short* v);  // 800D8A70
  void SwapFourByteNumber(unsigned int* v);  // 800D8A84
  int IsIDRegistered(NghResFile__RegistryID id);  // 800D8AA8
  int RegisterSection(NghResFile__RegistryID id, void (*save)(NghResFile__SectionID, NghResFile__SaveRecord&),
                      void (*load)(NghResFile__SectionID, NghResFile__SaveRecord&),
                      void (*complete)(NghResFile__SectionID, NghResFile__SaveRecord&));  // 800D8AE4
  int FindNodeInRegistry(NghResFile__RegistryID id, NghResFile__RegistryNode& node);  // 800D8D10
  void SectionSaveViaRegistry(NghResFile__RegistryID, NghResFile__SectionID, NghResFile__SaveRecord &);  // 800D8D90
  void SaveCompleteViaRegistry(NghResFile__RegistryID, NghResFile__SectionID, NghResFile__SaveRecord &);  // 800D8E00
  void SectionLoadViaRegistry(NghResFile__RegistryID, NghResFile__SectionID, NghResFile__SaveRecord &);  // 800D8E70
  void SetStoreChecksum(bool store);  // 800D8EE0
  bool GetStoreChecksum();  // 800D8EE8
  static void ResetChecksums();  // 800D8EF0
  unsigned int GetCurrentHouse() const;  // 800D93C0
  int GetSourceType() const;  // 800D93CC

  // +0x10..+0x1C: the registered sections (a std::vector of RegistryNode on GC too).
  std::vector<NghResFile__RegistryNode> m_registry;
  int m_field20 = 1;               // +0x20 the source: 1 the default neighbourhood file, 0 the card (cache)
  unsigned int m_currentHouse = 0; // +0x24 house - 1
  std::string m_fileName;          // +0x28 StringBuffer (0x104 chars)
  unsigned char m_card = 0;        // +0x134 the card slot the save is on
  ResNode** m_nghLists = nullptr;  // +0x138 'NGBH' 'NBRS' 'TATT' 'FAMI'
  ResNode** m_houseLists[16] = {}; // +0x13C per house: 'SIMI' 'HOUS' 'Arry' 'objt' 'ObjM' 'DUMP'
  ResNode** m_userLists = nullptr; // +0x17C 'User' 'thmb'
  ResNode* m_byIndex = nullptr;    // +0x180 the node GetByIndex found
  short m_writes = 0;              // +0x184 memory card writes in progress (PrepareMemCardWrite)
  int m_field188 = 1;
  int m_storeChecksum = 1;         // +0x18C remember the "both" section's checksum (HasCardChanged)
  MemoryCardCache* m_cache = nullptr;  // +0x190
  unsigned int m_thumbGUIDs[36] = {};  // +0x194 the thumbnails' GUIDs, by resource id - 1

  static unsigned int m_uBothSectionChecksum;      // the "both" section's checksum when last read or written
  static unsigned int m_uLastBothSectionChecksum;  // the one before
};
