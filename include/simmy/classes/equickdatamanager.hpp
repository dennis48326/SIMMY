// @class EQuickdataManager after EResourceManager
// The "quickdatas" manager (engine/e_quickdataman.cpp). Loads ERQuickdata images in the current
// language. The language word at +0xD20 is still the global g_quickdatamanLanguage (written by
// ESimsApp::Init and read by GameData), see simmy/unported_globals.hpp.
#pragma once

struct EQuickdataManager : public EResourceManager {
  EQuickdataManager();
  using EResourceManager::AddRef;
  EAHeap* GetHeap() override;
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;
  void Reload(unsigned int id);
  EResource* AddRef(unsigned int id, EFile* file, int pos);
  EResource* AddRef(char* name, EFile* file, int pos);
  int CanCache(EResource*) override;
  int GetCurrentLanguage() const;
  void SetCurrentLanguage(int language);

  int m_loadingUrbzUi = 0;  // +0xD24, set while "urbzui" loads
};
