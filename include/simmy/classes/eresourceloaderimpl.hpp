// @class EResourceLoaderImpl after EResourceLoader
// The resource loader (engine/e_resloader.cpp). GameCube: a thread fed by a message queue, so
// loads could run asynchronously. PC (for now): every request runs synchronously on the caller,
// i.e. asynchronous requests simply complete early. The global index (DATA/INDEX.IND and
// DATA/INDEX1.IND) is read once and kept big-endian.
#pragma once
#include <mutex>
#include <vector>

struct EResourceLoaderImpl : public EResourceLoader {
  EResourceLoaderImpl();
  virtual ~EResourceLoaderImpl();
  virtual void Init();
  virtual void Shutdown();
  virtual void TerminateThread();
  virtual int IsCallingThread();
  virtual void Update();
  virtual void FlushCommandQueue();
  virtual const unsigned char* AddManager(EResourceManager* mgr);
  virtual void RemoveManager(EResourceManager* mgr);
  virtual EResource* Load(EResourceManager* mgr, unsigned int id, EFile* file, unsigned int pos,
                          unsigned int size, bool sync);
  virtual int CollapseDelRef(EResourceManager* mgr, unsigned int id);
  virtual void Unload(EResourceManager* mgr, unsigned int id);
  virtual int IsInQueue(EResourceManager* mgr, unsigned int id);
  virtual int ReadData(EFile* file, void* dst, unsigned int pos, unsigned int size, bool sync,
                       EResLoadCmdResult* result);
  virtual int ReadData(EResourceManager* mgr, void* dst, unsigned int pos, unsigned int size, bool sync,
                       EResLoadCmdResult* result);
  virtual int WriteData(EFile* file, void* src, unsigned int pos, unsigned int size, bool sync,
                        EResLoadCmdResult* result);
  virtual void GetQueueStats(int& a, int& b, int& c, int& d);
  virtual int GetQueuedCommandCount();
  virtual int IsBusy();
  virtual EResourceManager* FindResourceManager(char* name);
  virtual void OpenFiles();
  virtual void CloseArchiveFile(EResourceManager* mgr);
  virtual void AddRefDelRefAllResources();  // 80310F34
  virtual void PrintLoadedResources();
  virtual void PrintLoadedResources(char* name);
  virtual void PrintResourceSizes();
  virtual void PrintResourceSizes(char* name);
  virtual int FreeUnreferencedResources();
  virtual int FreeUnreferencedResources(char* name);
  virtual int GetUnreferencedResourceCount();
  virtual int GetUnreferencedResourceCount(char* name);
  virtual void EnableReloads(bool enable);
  virtual void PauseLoads();
  virtual void UnPauseLoads();
  virtual int AreLoadsPaused();

  void sendCommand(EResLoadCmd *);  // 803104B4
  EResourceManager* FindResourceManagerInternal(char* name);
  void deallocateGlobalIndex();
  void allocateGlobalIndex();
  const unsigned char* getIndexPointer(const char* managerName);
  void Main();  // 8031173C
  void ProcessMessage(unsigned int);  // 803117A8
  EResource* DoLoadResource(EResourceManager* mgr, unsigned int id, EFile* file, unsigned int pos,
                            unsigned int size);

  std::recursive_mutex m_mutex;                 // +0x35C
  EResourceManager* m_firstManager = nullptr;   // +0x348
  EResourceManager* m_lastManager = nullptr;    // +0x34C
  int m_initialized = 0;                        // +0x350
  std::vector<unsigned char> m_index[2];        // +0x354, DATA/INDEX.IND and DATA/INDEX1.IND
  int m_reloadsEnabled = 0;                     // +0x3E4
  int m_paused = 0;                             // +0x3E8
};
