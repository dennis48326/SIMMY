// Port of engine/e_fontman.cpp: the "fonts" resource manager.
#include "simmy/types_gen.hpp"

// 80324D60
EResource* EFontManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* font = new ERFont;
  font->m_id = id;
  font->Load(*file);
  return font;
}

// 80324E3C
EResource* EFontManager::AddRef(unsigned int id, EFile* file, int size) { return EResourceManager::AddRef(id, file, size); }
// 80324E5C
EResource* EFontManager::AddRef(char* name, EFile* file, int size) { return EResourceManager::AddRef(name, file, size); }
