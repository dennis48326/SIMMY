// GENERATED stubs for unported functions of game/e_simsdataman.cpp. Do not edit.
#include "simmy/types_gen.hpp"

int EDummyFile::Read(void *, unsigned int) { SIMMY_STUB("EDummyFile::Read(void *, unsigned int)", 0x80022684); return {}; }
int EDummyFile::Write(void *, unsigned int) { SIMMY_STUB("EDummyFile::Write(void *, unsigned int)", 0x8002268C); return {}; }
int EDummyFile::Seek(int, EFile__SeekType) { SIMMY_STUB("EDummyFile::Seek(int, EFile::SeekType)", 0x80022694); return {}; }
int EDummyFile::Tell() const { SIMMY_STUB("EDummyFile::Tell(void) const", 0x800226A0); return {}; }
int EDummyFile::Flush() { SIMMY_STUB("EDummyFile::Flush(void)", 0x800226A8); return {}; }
int EDummyFile::GetLastError() const { SIMMY_STUB("EDummyFile::GetLastError(void) const", 0x800226B0); return {}; }
void* EDummyFile::GetSystemHandle() const { SIMMY_STUB("EDummyFile::GetSystemHandle(void) const", 0x800226B8); return {}; }
void EDummyFile::Destroy() { SIMMY_STUB("EDummyFile::Destroy(void)", 0x800226C0); }
RET_UNKNOWN isResInList(vector<unsigned int, allocator<unsigned int>> &, unsigned int) { SIMMY_STUB("isResInList(vector<unsigned int, allocator<unsigned int> > &, unsigned int)", 0x800226C4); return {}; }
void collectResInfoForSel(ObjSelector *, vector<unsigned int, allocator<unsigned int>> &) { SIMMY_STUB("collectResInfoForSel(ObjSelector *, vector<unsigned int, allocator<unsigned int> > &)", 0x80022718); }
void collectResInfoForMultSel(ObjSelector *, vector<unsigned int, allocator<unsigned int>> &, vector<ObjSelector *, allocator<ObjSelector *>> *) { SIMMY_STUB("collectResInfoForMultSel(ObjSelector *, vector<unsigned int, allocator<unsigned int> > &, vector<ObjSelector *, allocator<ObjSelector *> > *)", 0x800228EC); }
bool ESimsDataManager::compareID(unsigned int &, unsigned int &) { SIMMY_STUB("ESimsDataManager::compareID(unsigned int &, unsigned int &)", 0x80022C3C); return {}; }
void addRefList(vector<unsigned int, allocator<unsigned int>> &, unsigned int *) { SIMMY_STUB("addRefList(vector<unsigned int, allocator<unsigned int> > &, unsigned int *)", 0x80022CB4); }
void delRefList(vector<unsigned int, allocator<unsigned int>> &) { SIMMY_STUB("delRefList(vector<unsigned int, allocator<unsigned int> > &)", 0x80022D58); }
void ObjectSaveTypeTable2::DoStream(ReconBuffer *, int) { SIMMY_STUB("ObjectSaveTypeTable2::DoStream(ReconBuffer *, int)", 0x80022DC4); }
ESimsDataManager::ESimsDataManager() { SIMMY_STUB("ESimsDataManager::ESimsDataManager(void)", 0x80022F04); }
ESimsDataManager::~ESimsDataManager() { SIMMY_STUB("ESimsDataManager::~ESimsDataManager(void)", 0x80022F70); }
double ESimsDataManager::GetLoadProgress() { SIMMY_STUB("ESimsDataManager::GetLoadProgress(void)", 0x80022FA8); return {}; }
EResource* ESimsDataManager::AllocateAndLoadResource(EFile *, unsigned int, unsigned int) { SIMMY_STUB("ESimsDataManager::AllocateAndLoadResource(EFile *, unsigned int, unsigned int)", 0x8002302C); return {}; }
void ESimsDataManager::preloadResources(EEvent &) { SIMMY_STUB("ESimsDataManager::preloadResources(EEvent &)", 0x8002314C); }
void ESimsDataManager::incWorkQueued() { SIMMY_STUB("ESimsDataManager::incWorkQueued(void)", 0x800231C0); }
void ESimsDataManager::decWorkQueued() { SIMMY_STUB("ESimsDataManager::decWorkQueued(void)", 0x80023230); }
void ESimsDataManager::LoadSelectorData(ObjSelector *, bool) { SIMMY_STUB("ESimsDataManager::LoadSelectorData(ObjSelector *, bool)", 0x800232AC); }
void ESimsDataManager::UnloadSelectorData(ObjSelector *, bool) { SIMMY_STUB("ESimsDataManager::UnloadSelectorData(ObjSelector *, bool)", 0x80023344); }
void ESimsDataManager::undoPreload(ObjSelector *) { SIMMY_STUB("ESimsDataManager::undoPreload(ObjSelector *)", 0x800233C4); }
void ESimsDataManager::preload(ObjSelector *) { SIMMY_STUB("ESimsDataManager::preload(ObjSelector *)", 0x80023458); }
void ESimsDataManager::QueueCommand(ESim *, unsigned int) { SIMMY_STUB("ESimsDataManager::QueueCommand(ESim *, unsigned int)", 0x800234F4); }
void ESimsDataManager::FlushCommands() { SIMMY_STUB("ESimsDataManager::FlushCommands(void)", 0x800235B0); }
