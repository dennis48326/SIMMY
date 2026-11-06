// Port of engine/e_shaderman.cpp: the "shaders" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 803281DC
EAHeap* EShaderManager::GetHeap() { return ShaderHeap(); }

// 803281FC
EResource* EShaderManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* shader = new ERShader;
  shader->m_id = id;
  shader->Load(*file);
  return shader;
}

// 803282D4
EResource* EShaderManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 803282F4
EResource* EShaderManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
// 80328314
int EShaderManager::CanCache(EResource*) { return 1; }
