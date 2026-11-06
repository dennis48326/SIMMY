// Port of engine/e_resourceman.cpp: the resource manager base and its id -> resource map.
// Still stubbed: AddRefAll / AddDelRefAll / DelRefAll / Refresh (bulk operations used by tools and
// level transitions), GetIds, and the per-heap *HeapFreeMemory allocator callbacks.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

// ---- EResourceMap ------------------------------------------------------------------------------

// 80327A3C  EResourceMap::EResourceMap (defaulted in the header)

// 80327A5C
EResourceMap::~EResourceMap() { RemoveAll(); }

// 80327A9C
// Index of `id`, or of the slot where it would be inserted.
int EResourceMap::Search(unsigned int id) {
  auto it = std::lower_bound(m_entries.begin(), m_entries.end(), id,
                             [](const auto& e, unsigned int v) { return e.first < v; });
  return static_cast<int>(it - m_entries.begin());
}

// 80327AE8
int EResourceMap::Find(unsigned int id, EResource** out) {
  const int i = Search(id);
  if (i < static_cast<int>(m_entries.size()) && m_entries[i].first == id) {
    if (out) *out = m_entries[i].second;
    return 1;
  }
  return 0;
}

// 80327B5C
void EResourceMap::SetCapacity(int capacity, char*) {
  if (capacity > m_capacity) {
    m_entries.reserve(static_cast<size_t>(capacity));
    m_capacity = capacity;
  }
}

// 80327BD8
int EResourceMap::Insert(EResourceManager* manager, unsigned int id, EResource* res, char* owner) {
  // The original grew by half when full and, failing that, evicted cached resources. A vector
  // grows freely; keep the bookkeeping.
  if (static_cast<int>(m_entries.size()) == m_capacity) SetCapacity(m_capacity + m_capacity / 2 + 1, owner);
  const int i = Search(id);
  if (i < static_cast<int>(m_entries.size()) && m_entries[i].first == id) return 0;
  m_entries.insert(m_entries.begin() + i, {id, res});
  const int n = static_cast<int>(m_entries.size());
  m_maxUsed = std::max(m_maxUsed, n);
  m_maxLoaded = std::max(m_maxLoaded, n - manager->GetUnreferencedResourceCount());
  return 1;
}

// 80327D5C
int EResourceMap::Remove(unsigned int id) {
  const int i = Search(id);
  if (i >= static_cast<int>(m_entries.size()) || m_entries[i].first != id) return 0;
  m_entries[i].second->m_manager = nullptr;
  m_entries.erase(m_entries.begin() + i);
  return 1;
}

// 80327E30
int EResourceMap::GetSize() { return static_cast<int>(m_entries.size()); }
// 80327E38
bool EResourceMap::IsEmpty() { return m_entries.empty(); }

// 80327E48
void EResourceMap::RemoveAll() {
  while (!m_entries.empty()) Remove(m_entries.back().first);
}

// 80327EA0
void EResourceMap::PrintLoadedResources(char*) {}
// 80327EA4
void EResourceMap::PrintResourceSizes(char*) {}
// 80327EA8
void EResourceMap::RefreshResources() {}
// 80327EAC
int EResourceMap::GetNumEntries() { return static_cast<int>(m_entries.size()); }
// 80327EB4
int EResourceMap::GetMaxEntries() { return m_capacity; }
// 80327EBC
int EResourceMap::GetMaxLoaded() { return m_maxLoaded; }
// 80327EC4
int EResourceMap::GetMaxUsed() { return m_maxUsed; }

// ---- EResourceManager --------------------------------------------------------------------------

// 8032561C
EResourceManager::EResourceManager() = default;

// 80325688
EResourceManager::~EResourceManager() {
  if (m_initialized) Shutdown();
}

// 80325728
void EResourceManager::Shutdown() {
  m_map.RemoveAll();
  m_archivePath.clear();
  if (_pResLoader) _pResLoader->RemoveManager(this);
  CloseArchiveFile();
  m_initialized = 0;
}

// 80325798
void EResourceManager::Init(char* name) {
  m_name = name;
  CalcPath();
  m_index = _pResLoader->AddManager(this);
  // Initial table sizes from the original (resources expected per type).
  static const struct { const char* name; int capacity; } kCapacity[] = {
      {"shaders", 800},       {"textures", 800},         {"animations", 500},     {"effectsAttachments", 500},
      {"models", 250},        {"effectsEmitters", 250},  {"samples", 150},        {"characters", 150},
      {"datasets", 100},      {"quickdatas", 20},        {"flashes", 50},         {"emitters", 700},
      {"edithtreesets", 550}, {"binaries", 10},          {"effectsSequencers", 4}, {"fonts", 4},
      {"audiostreams", 4},    {"movies", 4},             {"ambientscores", 4},    {"soundtrackdatas", 4},
      {"levels", 4}};
  int capacity = 16;
  for (const auto& c : kCapacity)
    if (std::strcmp(c.name, name) == 0) capacity = c.capacity;
  m_map.SetCapacity(capacity, name);
  m_initialized = 1;
}

// 80325AD8
// "DATA/" + the first 8 characters of the name + ".ARC", upper-cased: "textures" -> DATA/TEXTURES.ARC,
// "animations" -> DATA/ANIMATIO.ARC.
void EResourceManager::CalcPath() {
  std::string base = std::string(m_name).substr(0, 8);
  m_archivePath = "DATA/" + base + ".ARC";
  for (char& c : m_archivePath) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

// 80325BC0
EFile* EResourceManager::GetArchiveFile() {
  std::lock_guard lock(m_mutex);
  if (!m_archive) {
    // The original also preferred a hard-disk cache copy when one existed (HDDThread).
    _eorFileSys.Create(m_archive, const_cast<char*>(m_archivePath.c_str()), const_cast<char*>("rb"),
                       static_cast<int>(0x80000000u), m_archiveWritable != 0, 0);
    if (!m_archive)
      std::fprintf(stderr, "[res] cannot open archive %s for %s\n", m_archivePath.c_str(), m_name);
  }
  return m_archive;
}

// 80325CE8
bool EResourceManager::ArchiveFileIsUsingHDD() {
  std::lock_guard lock(m_mutex);
  return m_archive && m_archive->m_deviceType == 0;
}

// 80325D8C
bool EResourceManager::ArchiveFileIsOpen() {
  std::lock_guard lock(m_mutex);
  return m_archive != nullptr;
}

// 80325E08
void EResourceManager::CloseArchiveFile() {
  EFile* f;
  {
    std::lock_guard lock(m_mutex);
    f = m_archive;
    m_archive = nullptr;
  }
  if (f) _eorFileSys.Destroy(f);
}

// 80325E6C
int EResourceManager::BinarySearch(unsigned int id, const unsigned char* ids, int count) {
  if (count == 0) return -1;
  int lo = 0, hi = count - 1;
  while (lo < hi) {
    const int mid = (lo + hi) >> 1;
    if (simmy::be32(ids + 4 * mid) < id) lo = mid + 1;
    else hi = mid;
  }
  return simmy::be32(ids + 4 * lo) == id ? lo : -1;
}

// 80325ED0
// Index block (big-endian): count, ids[count] (sorted), then {pos, size} per id.
int EResourceManager::LookupId(unsigned int id, unsigned int& pos, unsigned int& size) {
  if (!m_index) return 0;
  const int count = static_cast<int>(simmy::be32(m_index));
  const int i = BinarySearch(id, m_index + 4, count);
  if (i == -1) return 0;
  const unsigned char* entry = m_index + 4 * (count + 1 + 2 * i);
  pos = simmy::be32(entry);
  size = simmy::be32(entry + 4);
  return 1;
}

// 80327F10
int EResourceManager::LookupId(EResourceManager* mgr, unsigned int id, unsigned int& pos, unsigned int& size) {
  return mgr->LookupId(id, pos, size);
}

// 80326638
void EResourceManager::PrintLoadedResources() {}
// 803266A8
void EResourceManager::PrintResourceSizes() {}
// 80326718
void EResourceManager::PrintUnmanagedResources() {}

// 8032671C
EResource* EResourceManager::GetRef(unsigned int id) {
  if (!id) return nullptr;
  std::lock_guard lock(m_mutex);
  EResource* r = nullptr;
  if (m_map.Find(id, &r) && r->m_refs != 0) return r;
  return nullptr;
}

// 803267D8
EResource* EResourceManager::GetRef(char* name) { return GetRef(CalcId(name)); }

// 80326814
int EResourceManager::ReadData(void* dst, unsigned int pos, unsigned int size) {
  return _pResLoader->ReadData(this, dst, pos, size, true, nullptr);
}

// 8032686C
int EResourceManager::PreloadResource(unsigned int id) {
  if (IsLoaded(id)) return 1;
  if (!_pResLoader->IsInQueue(this, id)) {
    AddRefAsync(id);
    _pResLoader->Unload(this, id);
  }
  return IsLoaded(id);
}

// 80326920
int EResourceManager::PreloadResource(char* name) { return PreloadResource(CalcId(name)); }

// 8032695C
// With a file (a dataset loading its contents), the resource's data is at the file's current position
// and `size` bytes long; if the resource is already loaded those bytes are skipped.
EResource* EResourceManager::addRef(unsigned int id, EFile* file, unsigned int size, bool sync) {
  if (!id) return nullptr;
  EResource* res = nullptr;
  if (!TryIncrementResource(id, &res)) {
    res = _pResLoader->Load(this, id, file, 0, size, sync);
  } else if (size != 0 && file) {
    file->Seek(static_cast<int>(size), EFile__SeekType::Cur);
  }
  return res;
}

// 80326A60
// Takes a reference to an already-loaded resource, reviving it from the free list if needed.
int EResourceManager::TryIncrementResource(unsigned int id, EResource** out) {
  *out = nullptr;
  int ok = 0;
  std::unique_lock lock(m_mutex);
  if (m_map.Find(id, out)) {
    EResource* r = *out;
    if (r->m_refs != 0) {
      ++r->m_refs;
      ok = 1;
    } else if (_pResLoader->IsCallingThread()) {
      if (r->m_loading == 0) m_free.RemoveItem(r);
      ++r->m_loading;
      lock.unlock();
      r->AddRefSubResources();
      lock.lock();
      ++r->m_refs;
      --r->m_loading;
      ok = 1;
    } else if (r->m_loading == 0 && r->TryIncrementSubResources()) {
      m_free.RemoveItem(r);
      ++r->m_refs;
      ok = 1;
    }
  }
  if (!ok) *out = nullptr;
  return ok;
}

// 80326BE4
void EResourceManager::AddResource(EResource* res, unsigned int id) { AddResource(res, id, true); }

// 80326C08
void EResourceManager::LogResourceLoad(EResource*) {}

// 80326C0C
void EResourceManager::AddResource(EResource* res, unsigned int id, bool cacheable) {
  if (!res) return;
  std::lock_guard lock(m_mutex);
  if (m_map.Insert(this, id, res, const_cast<char*>(m_name))) {
    res->m_manager = this;
    res->m_id = id;
    res->m_flags = (cacheable ? 0x80u : 0u) | (res->m_flags & ~0x80u);
  } else {
    EResource* existing = nullptr;
    if (m_map.Find(id, &existing)) ++res->m_refs;
  }
  res->Init();
}

// 80326CF8
EResource* EResourceManager::AddRef(unsigned int id, EFile* file, int pos) {
  return addRef(id, file, static_cast<unsigned int>(pos), true);
}

// 80326D1C
EResource* EResourceManager::AddRef(char* name, EFile* file, int pos) {
  return addRef(CalcId(name), file, static_cast<unsigned int>(pos), true);
}

// 80326D6C
EResource* EResourceManager::AddRefAsync(char* name) { return addRef(CalcId(name), nullptr, 0, false); }

// 80326DB4
EResource* EResourceManager::AddRefAsync(unsigned int id) { return addRef(id, nullptr, 0, false); }

// 80326F60
void EResourceManager::AddRef(EResource* res) {
  std::lock_guard lock(m_mutex);
  ++res->m_refs;
}

// 80326FD0
void EResourceManager::DelRef(unsigned int id, EResourceManager__DelRefMode mode) {
  std::lock_guard lock(m_mutex);
  EResource* r = nullptr;
  m_map.Find(id, &r);
  if (r) DelRef(r, mode);
}

// 8032708C
void EResourceManager::DelRef(char* name, EResourceManager__DelRefMode mode) { DelRef(CalcId(name), mode); }

// 803270D0
void EResourceManager::DelRefAsync(char* name) { DelRefAsync(CalcId(name)); }

// 8032710C
void EResourceManager::DelRefAsync(unsigned int id) {
  if (_pResLoader && _pResLoader->CollapseDelRef(this, id)) return;
  std::unique_lock lock(m_mutex);
  EResource* r = nullptr;
  m_map.Find(id, &r);
  if (!r || r->m_refs == 0 || r->m_loading != 0) {
    lock.unlock();
    _pResLoader->Unload(this, id);
  } else {
    DelRef(r, EResourceManager__DelRefMode::Cache);
  }
}

// 80327234
void EResourceManager::DelRef(EResource* res, EResourceManager__DelRefMode mode) {
  if (_pResLoader && _pResLoader->CollapseDelRef(this, res->m_id)) return;
  if (!m_initialized) return;
  std::unique_lock lock(m_mutex);
  if (res->m_refs == 0) return;
  --res->m_refs;
  const bool cache = CanCache(res) && mode != EResourceManager__DelRefMode::Immediate;
  if (!cache) {
    if (res->m_refs == 0) {
      m_map.Remove(res->m_id);
      lock.unlock();
      res->SafeDelete();
    }
  } else if (res->m_refs == 0) {
    res->DelRefSubResources();
    if (res->m_loading == 0) m_free.AddItem(res);
  }
}

// 8032737C
void EResourceManager::Detach(EResource* res) {
  if (!m_initialized) return;
  std::lock_guard lock(m_mutex);
  m_map.Remove(res->m_id);
}

// 803273D0
unsigned int EResourceManager::GetPos(unsigned int id) {
  unsigned int pos = 0, size = 0;
  LookupId(id, pos, size);
  return pos;
}

// 80327408
unsigned int EResourceManager::GetSize(unsigned int id) {
  unsigned int pos = 0, size = 0;
  LookupId(id, pos, size);
  return size;
}

// 80327444
bool EResourceManager::IsValid(unsigned int id) { return GetSize(id) != 0; }

// 80327474
int EResourceManager::IsLoaded(unsigned int id) {
  std::lock_guard lock(m_mutex);
  return m_map.Find(id, nullptr);
}

// 803274F4
int EResourceManager::IsLoaded(char* name) { return IsLoaded(CalcId(name)); }

// 80327530
void EResourceManager::ResourceDestructing(EResource*) {}

// 80327534
unsigned int EResourceManager::CalcId(char* name) {
  if (!name) return 0;
#ifdef _WIN32
  if (_stricmp(name, "undefined") == 0) return 0;
#else
  if (strcasecmp(name, "undefined") == 0) return 0;
#endif
  return static_cast<unsigned int>(EChecksum::ComputeSymbol(name, 0));
}

// 8032758C
EAHeap* EResourceManager::GetHeap() { return ResourceHeap(); }

// 803275AC
int EResourceManager::MakeSpace() {
  return m_free.GetUnreferencedResourceCount() ? m_free.MakeSpace(3) : 0;
}

// 803275F4
bool EResourceManager::FreeUnreferencedResources() { return m_free.MakeSpace(0x7fffff) != 0; }

// 80327630
int EResourceManager::GetUnreferencedResourceCount() { return m_free.GetUnreferencedResourceCount(); }

// 80327654
void* EResourceManager::Alloc(unsigned long size, unsigned int align) {
  return GetHeap()->MallocAligned(static_cast<unsigned int>(size), align, 0, 0);
}

// 803276AC
void EResourceManager::Free(void* p) {
  if (p) GetHeap()->Free(p);
}

// 803276FC
int EResourceManager::GetOverriddenFile(unsigned int, unsigned int&) { return 0; }

// 80327ECC
int EResourceManager::IsInitialized() { return m_initialized; }
// 80327ED4
int EResourceManager::CanCache() { return 1; }
// 80327EDC
int EResourceManager::CanCache(EResource*) { return CanCache(); }

// The base class had no implementation (a pure virtual); each manager type provides one.
EResource* EResourceManager::AllocateAndLoadResource(EFile*, unsigned int, unsigned int) {
  SIMMY_STUB("EResourceManager::AllocateAndLoadResource (manager type not ported)", 0);
  return nullptr;
}
