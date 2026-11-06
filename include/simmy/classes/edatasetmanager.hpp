// @class EDatasetManager after EResourceManager
// The "datasets" manager (engine/e_datasetman.cpp). A dataset is a bundle of other resources. When
// loading is disabled, lookups return the sentinel (EResource*)1 instead of loading.
#pragma once

struct EDatasetManager : public EResourceManager {
  EDatasetManager();
  using EResourceManager::AddRef;
  using EResourceManager::DelRef;
  using EResourceManager::GetRef;
  using EResourceManager::PreloadResource;
  void ManageDasFile(char* name);
  EResource* AddRef(char* name, EFile* file, int size);
  EResource* AddRef(unsigned int id, EFile* file, int size);
  void AddRef(EResource* res);
  EResource* AddRefAsync(char* name) override;
  EResource* AddRefAsync(unsigned int id) override;
  void AddRefAsyncIfNotLoaded(char* name);
  void DelRef(EResource* res, EResourceManager__DelRefMode mode);
  void DelRef(unsigned int id, EResourceManager__DelRefMode mode);
  void DelRef(char* name, EResourceManager__DelRefMode mode);
  double GetLoadProgress() const;
  void SetLoadProgress(float progress);
  int PreloadResource(char* name);
  void PreloadResource(unsigned int id);
  EResource* GetRef(char* name);
  EResource* GetRef(unsigned int id);
  static void SetLoadingEnabled(bool enabled);
  static int GetLoadingEnabled();
  static void OutputDatasets(bool);
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;
  int CanCache(EResource*) override;

  static int m_loadEnabled;   // 804FF118
  static int m_outputEnabled; // 804FF11C
  float m_loadProgress = -1.0f;  // +0xD20, -1 when idle
};
