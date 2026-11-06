// Port of engine/freeresourcemanager.cpp: cached resources with no references, evicted on demand.
#include "simmy/types_gen.hpp"

#include <algorithm>

// 80328F80
FreeResourceManager::FreeResourceManager(EResourceManager* owner) : m_owner(owner) {}

// 80328FC8
void FreeResourceManager::AddItem(EResource* res) {
  if (!m_owner) return;
  std::unique_lock lock(m_mutex);
  if (static_cast<int>(m_items.size()) == m_capacity) {
    lock.unlock();
    MakeSpace(5);
    lock.lock();
    if (static_cast<int>(m_items.size()) == m_capacity) {
      lock.unlock();
      res->SafeDelete();  // still full: drop it outright
      return;
    }
  }
  m_items.push_back(res);
}

// 80329090
int FreeResourceManager::HasItem(EResource* res) {
  std::lock_guard lock(m_mutex);
  return std::find(m_items.begin(), m_items.end(), res) != m_items.end();
}

// 80329154
void FreeResourceManager::RemoveItem(EResource* res) {
  std::lock_guard lock(m_mutex);
  auto it = std::find(m_items.begin(), m_items.end(), res);
  if (it != m_items.end()) m_items.erase(it);
}

// 80329210
// Frees up to `count` (max 256) of the oldest cached resources; returns the bytes released.
int FreeResourceManager::MakeSpace(int count) {
  if (!m_owner) return 0;
  std::vector<EResource*> victims;
  {
    std::lock_guard ownerLock(m_owner->m_mutex);
    std::lock_guard lock(m_mutex);
    count = std::min({count, static_cast<int>(m_items.size()), 256});
    std::vector<EResource*> keep;
    for (int i = 0; i < count; ++i) {
      EResource* r = m_items[i];
      if (r->IsSafeToDelete()) {
        victims.push_back(r);
        r->m_manager->m_map.Remove(r->m_id);
      } else {
        keep.push_back(r);  // unsafe right now: stays cached
      }
    }
    m_items.erase(m_items.begin(), m_items.begin() + count);
    m_items.insert(m_items.begin(), keep.begin(), keep.end());
  }
  int freed = 0;
  for (EResource* r : victims) {
    freed += 1 + static_cast<int>(r->m_flags >> 8);
    r->SafeDelete();
  }
  return freed;
}

// 803293F0
int FreeResourceManager::GetUnreferencedResourceCount() {
  std::lock_guard lock(m_mutex);
  return static_cast<int>(m_items.size());
}
