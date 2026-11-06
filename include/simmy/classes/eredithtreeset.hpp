// @class EREdithTreeSet after EResource EStorable EResourceManager
// @class EEdithTreeSetMan
// An object file's behaviour trees (engine/e_redithtreeset.cpp, "edithtreesets" manager): an IFF image
// ("IFF FILE 2.5:TYPE FOLLOWED BY SIZE", 0x40-byte header) of chunks {type[4], size (BE, header
// included), id16, flags16, name[0x40], data}. The 'BHAV' chunks are the trees (big-endian, read in
// place; see behavior.hpp), found by id.
#pragma once
#include <vector>

struct EREdithTreeSet : public EResource {
  EREdithTreeSet();  // 8030F484
  SIMMY_STORABLE_DECL(EREdithTreeSet)
  ~EREdithTreeSet() override;  // 8030F4CC
  void Deallocate();  // 8030F528
  void Write(EStream &) override;  // 8030F580
  void Read(EStream &) override;  // 8030F5B4
  void Load(EFile* file, unsigned int size);  // 8030F5E8
  using EResource::Load;
  // The tree (its data: header then nodes) with chunk id `id`, or null.
  const unsigned char* GetBehaviorTree(int id) const;  // 8030F964
  int GetTreeIDByIndex(int index) const;  // 8030F9D0 (1-based)
  int GetTreeIDByName(const char* name) const;  // 8030F9FC (names are lower-cased at load)
  static void RegisterType(unsigned short);  // 8030FBE8
  void CreateCopy() const;  // 8030FC3C
  int GetNumTrees() const { return static_cast<int>(m_trees.size()); }  // 8030FC5C

  // One 'BHAV' chunk (GC 0xC bytes, sorted by id: IffMapCompare 8030F3C4).
  struct Tree {
    unsigned int m_id = 0;
    const char* m_name = nullptr;
    const unsigned char* m_data = nullptr;
  };
  std::vector<unsigned char> m_image;  // +0x14 the IFF image
  std::vector<Tree> m_trees;           // +0x18 count, +0x1C the map
};

struct EEdithTreeSetMan : public EResourceManager {
  EEdithTreeSetMan() = default;
  void AddRef(EResource* res) { EResourceManager::AddRef(res); }  // PC: the base overload (MSVC: no using-declaration beside same-signature AddRefs)
  EAHeap* GetHeap() override;  // 80324A4C
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 80324A6C
  EREdithTreeSet* AddRef(unsigned int id, EFile* file, int pos);  // 80324B34
  EREdithTreeSet* AddRef(char* name, EFile* file, int pos);  // 80324B54
  int CanCache(EResource *) override;  // 80324B74
};
