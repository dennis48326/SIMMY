// @class EResourceManager after EResourceMap FreeResourceManager EResourceManager__DelRefMode
// One resource type's manager (engine/e_resourceman.cpp): the archive "DATA/<NAME8>.ARC", its index
// block from DATA/INDEX*.IND (kept big-endian), and the loaded-resource table. Subclasses implement
// AllocateAndLoadResource for their resource type.
#pragma once
#include <mutex>
#include <string>

struct EResourceManager {
  using DelRefMode = ::EResourceManager__DelRefMode;
  EResourceManager();
  virtual ~EResourceManager();
  virtual void Init(char* name);
  virtual void Shutdown();
  virtual EResource* AddRefAsync(unsigned int id);
  virtual EResource* AddRefAsync(char* name);
  virtual EAHeap* GetHeap();
  virtual int CanCache();
  virtual int CanCache(EResource* res);
  // Creates and loads one resource of this manager's type from `file` (positioned at its data).
  virtual EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id);

  void CalcPath();
  EFile* GetArchiveFile();
  bool ArchiveFileIsUsingHDD();
  bool ArchiveFileIsOpen();
  void CloseArchiveFile();
  static int BinarySearch(unsigned int id, const unsigned char* sortedIdsBE, int count);
  int LookupId(unsigned int id, unsigned int& pos, unsigned int& size);
  static int LookupId(EResourceManager* mgr, unsigned int id, unsigned int& pos, unsigned int& size);
  void AddRefAll();  // 80325F60
  void AddDelRefAll();  // 80326288
  void DelRefAll();  // 803265C4
  void PrintLoadedResources();
  void PrintResourceSizes();
  void PrintUnmanagedResources();
  EResource* GetRef(unsigned int id);
  EResource* GetRef(char* name);
  int ReadData(void* dst, unsigned int pos, unsigned int size);
  int PreloadResource(unsigned int id);
  int PreloadResource(char* name);
  EResource* addRef(unsigned int id, EFile* file, unsigned int pos, bool sync);
  int TryIncrementResource(unsigned int id, EResource** out);
  void AddResource(EResource* res, unsigned int id);
  void LogResourceLoad(EResource* res);
  void AddResource(EResource* res, unsigned int id, bool cacheable);
  EResource* AddRef(unsigned int id, EFile* file, int pos);
  EResource* AddRef(char* name, EFile* file, int pos);
  void GetIds(unsigned int *&, int &);  // 80326DE0
  void RefreshResources();  // 80326DFC
  void Refresh(EResource *);  // 80326E20
  void Refresh(unsigned int);  // 80326E90
  void Refresh(char *);  // 80326F24
  void AddRef(EResource* res);
  void DelRef(unsigned int id, EResourceManager__DelRefMode mode);
  void DelRef(char* name, EResourceManager__DelRefMode mode);
  void DelRefAsync(char* name);
  void DelRefAsync(unsigned int id);
  void DelRef(EResource* res, EResourceManager__DelRefMode mode);
  void Detach(EResource* res);
  unsigned int GetPos(unsigned int id);
  unsigned int GetSize(unsigned int id);
  bool IsValid(unsigned int id);
  int IsLoaded(unsigned int id);
  int IsLoaded(char* name);
  void ResourceDestructing(EResource* res);
  static unsigned int CalcId(char* name);
  int MakeSpace();
  bool FreeUnreferencedResources();
  int GetUnreferencedResourceCount();
  void* Alloc(unsigned long size, unsigned int align);
  void Free(void* p);
  int GetOverriddenFile(unsigned int id, unsigned int& pos);
  int IsInitialized();

  std::recursive_mutex m_mutex;              // +0x00 EThreadMutex and +0x1C EMutex
  const char* m_name = nullptr;              // +0x40, e.g. "textures"
  std::string m_archivePath;                 // +0x44, e.g. "DATA/TEXTURES.ARC"
  int m_initialized = 0;                     // +0x48
  const unsigned char* m_index = nullptr;    // +0x4C, big-endian: count, ids[count], {pos,size}[count]
  EFile* m_archive = nullptr;                // +0x50
  int m_archiveWritable = 0;                 // +0x54
  FreeResourceManager m_free{this};          // +0x58
  EResourceMap m_map;                        // +0xD08
  EResourceManager* m_prevManager = nullptr; // +0xD00, loader's manager list
  EResourceManager* m_nextManager = nullptr; // +0xD04
};
