// Port of engine/e_datasetman.cpp: the "datasets" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

static EResource* const kLoadingDisabled = reinterpret_cast<EResource*>(1);

int EDatasetManager::m_loadEnabled = 1;
int EDatasetManager::m_outputEnabled = 0;

// 80324904
EDatasetManager::EDatasetManager() {}
// 80324598
void EDatasetManager::ManageDasFile(char*) {}

// 8032459C
EResource* EDatasetManager::AddRef(char* name, EFile* file, int size) {
  ManageDasFile(name);
  return m_loadEnabled ? EResourceManager::AddRef(name, file, size) : kLoadingDisabled;
}

// 80324954
EResource* EDatasetManager::AddRef(unsigned int id, EFile* file, int size) { return EResourceManager::AddRef(id, file, size); }

// 803245F0
void EDatasetManager::AddRef(EResource* res) {
  if (res && res != kLoadingDisabled) EResourceManager::AddRef(res);
}

// 80324620
EResource* EDatasetManager::AddRefAsync(char* name) {
  ManageDasFile(name);
  return m_loadEnabled ? EResourceManager::AddRefAsync(name) : kLoadingDisabled;
}

// 80324974
EResource* EDatasetManager::AddRefAsync(unsigned int id) { return EResourceManager::AddRefAsync(id); }

// 8032466C
void EDatasetManager::AddRefAsyncIfNotLoaded(char* name) {
  if (!GetRef(name)) {
    _pResLoader->FlushCommandQueue();
    AddRefAsync(name);
  } else {
    ManageDasFile(name);
  }
}

// 803246EC
void EDatasetManager::DelRef(EResource* res, EResourceManager__DelRefMode mode) {
  if (res && res != kLoadingDisabled) EResourceManager::DelRef(res, mode);
}
// 803249B4
void EDatasetManager::DelRef(unsigned int id, EResourceManager__DelRefMode mode) { EResourceManager::DelRef(id, mode); }
// 803249D4
void EDatasetManager::DelRef(char* name, EResourceManager__DelRefMode mode) { EResourceManager::DelRef(name, mode); }

// 8032471C
double EDatasetManager::GetLoadProgress() const { return m_loadProgress; }
// 80324780
void EDatasetManager::SetLoadProgress(float progress) { m_loadProgress = progress; }

// 80324788
int EDatasetManager::PreloadResource(char* name) {
  ManageDasFile(name);
  return m_loadEnabled ? EResourceManager::PreloadResource(CalcId(name)) : 1;
}
// 80324994
void EDatasetManager::PreloadResource(unsigned int id) { EResourceManager::PreloadResource(id); }

// 803247D4
EResource* EDatasetManager::GetRef(char* name) { return GetRef(CalcId(name)); }
// 80324810
EResource* EDatasetManager::GetRef(unsigned int id) { return m_loadEnabled ? EResourceManager::GetRef(id) : kLoadingDisabled; }

// 80324848
void EDatasetManager::SetLoadingEnabled(bool enabled) { m_loadEnabled = enabled; }
// 80324850
int EDatasetManager::GetLoadingEnabled() { return m_loadEnabled; }
// 80324858
void EDatasetManager::OutputDatasets(bool) {}

// 80324724
EResource* EDatasetManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* dataset = new ERDataset;
  dataset->m_id = id;
  dataset->Load(file, size);
  return dataset;
}

// 8032494C
int EDatasetManager::CanCache(EResource*) { return 1; }
