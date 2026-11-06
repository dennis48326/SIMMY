// Port of engine/e_modelman.cpp: the "models" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80324ED4
EAHeap* EModelManager::GetHeap() { return ModelHeap(); }

// 80324EF4
EResource* EModelManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* model = new ERModel;  // GC: Alloc(0x180, 8) from this manager, then the constructor
  model->m_id = id;
  model->LoadModel(file);
  return model;
}

// 80324FB8
EResource* EModelManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 80324FD8
EResource* EModelManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
// 80324FF8
int EModelManager::CanCache(EResource*) { return 1; }
