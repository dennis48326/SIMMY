// Port of engine/e_redithtreeset.cpp and engine/e_edithtreesetman.cpp: behaviour tree sets.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

// 8030F484
EREdithTreeSet::EREdithTreeSet() {}
// 8030F4CC
EREdithTreeSet::~EREdithTreeSet() { Deallocate(); }

// 8030F528
void EREdithTreeSet::Deallocate() {
  std::vector<Tree>().swap(m_trees);
  std::vector<unsigned char>().swap(m_image);
}

// 8030F580
void EREdithTreeSet::Write(EStream&) {}
// 8030F5B4
void EREdithTreeSet::Read(EStream&) {}

// 8030F5E8
// The 'EDTH' (version 3) header, then the IFF image. Chunks after the 0x40-byte IFF header follow each
// other by their size (big-endian); the 'BHAV' ones are listed with their (lower-cased) names. GC: data
// without the header is a raw image after a name string (not on the disc).
void EREdithTreeSet::Load(EFile* file, unsigned int) {
  Deallocate();
  EDataHeader header;
  if (!header.Read(*file, 0x45445448 /* 'EDTH' */, 3, 3)) return;
  const unsigned int size = header.m_dataSize;
  m_flags = size << 8 | (m_flags & 0xff);
  m_image.resize(size);
  if (size) file->Read(m_image.data(), static_cast<int>(size));
  unsigned char* base = m_image.data();
  for (int left = static_cast<int>(size) - 0x40, pos = 0x40; left > 0x4b;) {
    unsigned char* chunk = base + pos;
    const unsigned int chunkSize = simmy::be32(chunk + 4);
    if (chunkSize == 0 || static_cast<int>(chunkSize) > left) break;
    if (std::strncmp(reinterpret_cast<char*>(chunk), "BHAV", 4) == 0) {
      char* name = reinterpret_cast<char*>(chunk + 0xc);
      name[0x3f] = 0;
      for (char* c = name; *c; ++c) *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
      m_trees.push_back({simmy::be16(chunk + 8), name, chunk + 0x4c});
    }
    left -= static_cast<int>(chunkSize);
    pos += static_cast<int>(chunkSize);
  }
  std::sort(m_trees.begin(), m_trees.end(), [](const Tree& a, const Tree& b) { return a.m_id < b.m_id; });
}

// 8030F964
const unsigned char* EREdithTreeSet::GetBehaviorTree(int id) const {
  auto it = std::lower_bound(m_trees.begin(), m_trees.end(), static_cast<unsigned int>(id),
                             [](const Tree& t, unsigned int v) { return t.m_id < v; });
  return it != m_trees.end() && it->m_id == static_cast<unsigned int>(id) ? it->m_data : nullptr;
}

// 8030F9D0
int EREdithTreeSet::GetTreeIDByIndex(int index) const {
  --index;
  return index >= 0 && index < static_cast<int>(m_trees.size()) ? static_cast<int>(m_trees[index].m_id) : 0;
}

// 8030F9FC
int EREdithTreeSet::GetTreeIDByName(const char* name) const {
  for (const Tree& t : m_trees)
    if (std::strcmp(t.m_name, name) == 0) return static_cast<int>(t.m_id);
  return 0;
}

// 8030FC5C EREdithTreeSet::GetNumTrees (inline in the header)
// 8030F3C4 IffMapCompare (the sort's comparison in Load)

// 8030FBE8
void EREdithTreeSet::RegisterType(unsigned short) {}
// 8030FC3C
void EREdithTreeSet::CreateCopy() const {}

// 8030FBA0 EREdithTreeSet::GetTypeInfo
// 8030FBAC EREdithTreeSet::GetTypeName
// 8030FBB8 EREdithTreeSet::GetTypeKey
// 8030FBC4 EREdithTreeSet::GetTypeVersion
// 8030FB60 EREdithTreeSet::SafeDelete
// 8030FAEC EREdithTreeSet::New
// 8030FB1C EREdithTreeSet::Construct
// 8030FB3C EREdithTreeSet::Destruct
// 8030FBD0 EREdithTreeSet::GetTypeInfoStatic
// 8030FBDC EREdithTreeSet::GetReadVersion
SIMMY_STORABLE_IMPL(EREdithTreeSet, "EREdithTreeSet", 0, EResource)

// ---------------------------------------------------------------------------------------------------
// EEdithTreeSetMan

// 80324A4C
EAHeap* EEdithTreeSetMan::GetHeap() { return MainHeap(); }

// 80324A6C
EResource* EEdithTreeSetMan::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* set = new EREdithTreeSet;
  set->m_id = id;
  const int start = file->Tell();
  set->Load(file, size);
  file->Seek(start + static_cast<int>(size), EFile__SeekType::Set);
  return set;
}

// 80324B34
EREdithTreeSet* EEdithTreeSetMan::AddRef(unsigned int id, EFile* file, int pos) {
  return static_cast<EREdithTreeSet*>(EResourceManager::AddRef(id, file, pos));
}
// 80324B54
EREdithTreeSet* EEdithTreeSetMan::AddRef(char* name, EFile* file, int pos) {
  return static_cast<EREdithTreeSet*>(EResourceManager::AddRef(name, file, pos));
}
// 80324B74
int EEdithTreeSetMan::CanCache(EResource*) { return 1; }
