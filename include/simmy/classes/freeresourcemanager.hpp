// @class FreeResourceManager
// Unreferenced-but-cached resources a manager may evict to make room (engine/freeresourcemanager.cpp).
#pragma once
#include <mutex>
#include <vector>

struct FreeResourceManager {
  explicit FreeResourceManager(EResourceManager* owner = nullptr);
  void AddItem(EResource* res);
  int HasItem(EResource* res);
  void RemoveItem(EResource* res);
  int MakeSpace(int count);
  int GetUnreferencedResourceCount();

  std::recursive_mutex m_mutex;
  EResourceManager* m_owner;          // +0x1C (was the owner's mutex)
  std::vector<EResource*> m_items;    // +0x20, up to 800 entries
  int m_capacity = 800;               // +0xCA4
};
