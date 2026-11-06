// Port of engine/e_quickdataman.cpp: the "quickdatas" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80325538
EQuickdataManager::EQuickdataManager() { g_quickdatamanLanguage = 0; }

// 8032526C
EAHeap* EQuickdataManager::GetHeap() { return VMHeap(); }

// 8032528C
EResource* EQuickdataManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* quickdata = new ERQuickdata;
  quickdata->m_id = id;
  const int start = file->Tell();
  m_loadingUrbzUi = id == CalcId(const_cast<char*>("urbzui"));
  quickdata->Load(file, GetCurrentLanguage());
  m_loadingUrbzUi = 0;
  file->Seek(start + static_cast<int>(size), EFile__SeekType::Set);
  return quickdata;
}

// 80325358
// Re-reads a loaded quickdata in place (e.g. after the language changed).
void EQuickdataManager::Reload(unsigned int id) {
  EResource* res = nullptr;
  {
    std::lock_guard lock(m_mutex);
    m_map.Find(id, &res);
  }
  if (!res) return;
  _pResLoader->PauseLoads();
  EFile* file = nullptr;
  unsigned int pos = 0, size = 0;
  if (!GetOverriddenFile(id, pos)) {  // (PC: there are no override files)
    LookupId(res->m_id, pos, size);
    file = GetArchiveFile();
    if (file) file->Seek(static_cast<int>(pos), EFile__SeekType::Set);
  }
  if (file) static_cast<ERQuickdata*>(res)->Reload(file);
  _pResLoader->UnPauseLoads();
}

// 803254F8
EResource* EQuickdataManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 80325518
EResource* EQuickdataManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }

// 80325580
int EQuickdataManager::CanCache(EResource*) { return 1; }

// 80325588
int EQuickdataManager::GetCurrentLanguage() const { return g_quickdatamanLanguage; }

// 80325590
void EQuickdataManager::SetCurrentLanguage(int language) {
  FreeUnreferencedResources();
  g_quickdatamanLanguage = language;
}
