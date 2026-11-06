// Port of engine/e_textureman.cpp: the "textures" resource manager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80328658
EAHeap* ETextureManager::GetHeap() { return TextureHeap(); }

// 80328678
EResource* ETextureManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* texture = new ERTexture;
  texture->m_id = id;
  texture->Load(*file);
  return texture;
}

// 80328750
EResource* ETextureManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 80328770
EResource* ETextureManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
// 80328790
int ETextureManager::CanCache(EResource*) { return 1; }
