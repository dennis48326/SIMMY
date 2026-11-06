// @class EResourceMap
// Sorted (id -> resource) table of a manager's loaded resources (engine/e_resourceman.cpp).
#pragma once
#include <utility>
#include <vector>

struct EResourceMap {
  EResourceMap() = default;
  ~EResourceMap();
  int Search(unsigned int id);
  int Find(unsigned int id, EResource** out);
  void SetCapacity(int capacity, char* owner);
  int Insert(EResourceManager* manager, unsigned int id, EResource* res, char* owner);
  int Remove(unsigned int id);
  int GetSize();
  bool IsEmpty();
  void RemoveAll();
  void PrintLoadedResources(char* owner);
  void PrintResourceSizes(char* owner);
  static void RefreshResources();
  int GetNumEntries();
  int GetMaxEntries();
  int GetMaxLoaded();
  int GetMaxUsed();

  std::vector<std::pair<unsigned int, EResource*>> m_entries;  // +0x00 array / +0x04 count
  int m_capacity = 0;   // +0x08 (budget from the original; the vector grows as needed)
  int m_maxUsed = 0;    // +0x0C
  int m_maxLoaded = 0;  // +0x10
};
