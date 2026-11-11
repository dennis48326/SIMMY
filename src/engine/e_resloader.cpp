// Port of engine/e_resloader.cpp: the resource loader and the global resource index.
//
// GameCube: a "Resource Loader" thread consumed EResLoadCmd messages so loads could run in the
// background. PC (for now): IsCallingThread() is always true, so every request takes the original's
// own direct path on the calling thread. Asynchronous requests therefore complete immediately,
// which the game tolerates because it always polls IsLoaded / GetRef before using a resource.
#include "simmy/types_gen.hpp"
#include "simmy/perf.hpp"
#include "simmy/unported_globals.hpp"

// 8030FC90 EResourceLoader::EResourceLoader (defaulted in the header)
// 8030FCA4 EResourceLoader::~EResourceLoader (defaulted in the header)
#include "simmy/platform.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {
// PC mod: mods. A read-only file over a mod resource's bytes (simmy::mods_resource reads them from the
// mod's file), for the resource's own loader (AllocateAndLoadResource) as if it came from the archive.
struct ModMemFile final : public EFile {
  explicit ModMemFile(std::vector<unsigned char>&& data) : m_data(std::move(data)) { m_length = static_cast<int>(m_data.size()); }
  void Destroy() override { delete this; }
  int Read(void* dst, unsigned int size) override {
    const std::size_t left = m_data.size() - m_pos, n = size < left ? size : left;
    if (n) std::memcpy(dst, m_data.data() + m_pos, n);
    m_pos += n;
    return static_cast<int>(n);
  }
  int Write(void*, unsigned int) override { return 0; }
  int Seek(int offset, EFile::SeekType type) override {
    long long p = offset;
    if (type == EFile__SeekType::Cur) p += static_cast<long long>(m_pos);
    else if (type == EFile__SeekType::End) p += static_cast<long long>(m_data.size());
    m_pos = static_cast<std::size_t>(p < 0 ? 0 : (p > static_cast<long long>(m_data.size()) ? static_cast<long long>(m_data.size()) : p));
    return static_cast<int>(m_pos);
  }
  int Tell() const override { return static_cast<int>(m_pos); }
  int Flush() override { return 0; }
  int GetLastError() const override { return 0; }
  void* GetSystemHandle() const override { return nullptr; }
  std::vector<unsigned char> m_data;
  std::size_t m_pos = 0;
};
}  // namespace

static EResourceLoaderImpl _resLoader;
EResourceLoaderImpl* _pResLoader = &_resLoader;  // 805000A4

// 803101E0
EResourceLoaderImpl::EResourceLoaderImpl() { _pResLoader = this; }

// 8031029C
EResourceLoaderImpl::~EResourceLoaderImpl() { deallocateGlobalIndex(); }

// 80310434
void EResourceLoaderImpl::Init() {
  m_reloadsEnabled = 0;
  allocateGlobalIndex();
  m_paused = 0;
  m_initialized = 1;
}

// 80310340
void EResourceLoaderImpl::Shutdown() {
  FlushCommandQueue();
  m_initialized = 0;
}

// 803103C8
void EResourceLoaderImpl::TerminateThread() {}
// 80310410
int EResourceLoaderImpl::IsCallingThread() { return 1; }
// 803104B0
void EResourceLoaderImpl::Update() {}
// 803104B4
void EResourceLoaderImpl::sendCommand(EResLoadCmd*) {}
// 803104DC
void EResourceLoaderImpl::FlushCommandQueue() {}

// 80310588
const unsigned char* EResourceLoaderImpl::AddManager(EResourceManager* mgr) {
  std::lock_guard lock(m_mutex);
  mgr->m_prevManager = m_lastManager;
  if (m_lastManager) m_lastManager->m_nextManager = mgr;
  else m_firstManager = mgr;
  mgr->m_nextManager = nullptr;
  m_lastManager = mgr;
  return getIndexPointer(mgr->m_name);
}

// 80310654
void EResourceLoaderImpl::RemoveManager(EResourceManager* mgr) {
  if (!m_initialized) return;
  FlushCommandQueue();
  std::lock_guard lock(m_mutex);
  if (m_firstManager == mgr) m_firstManager = mgr->m_nextManager;
  else if (mgr->m_prevManager) mgr->m_prevManager->m_nextManager = mgr->m_nextManager;
  if (m_lastManager == mgr) m_lastManager = mgr->m_prevManager;
  else if (mgr->m_nextManager) mgr->m_nextManager->m_prevManager = mgr->m_prevManager;
  mgr->m_prevManager = mgr->m_nextManager = nullptr;
}

// 80310730
int EResourceLoaderImpl::CollapseDelRef(EResourceManager*, unsigned int) { return 0; }
// 80310754
int EResourceLoaderImpl::IsInQueue(EResourceManager*, unsigned int) { return 0; }

// 80310778
EResource* EResourceLoaderImpl::Load(EResourceManager* mgr, unsigned int id, EFile* file, unsigned int pos,
                                     unsigned int size, bool) {
  return DoLoadResource(mgr, id, file, pos, size);
}

// 8031088C
void EResourceLoaderImpl::Unload(EResourceManager* mgr, unsigned int id) {
  if (mgr && id) mgr->DelRef(id, EResourceManager__DelRefMode::Cache);
}

// 80310938
int EResourceLoaderImpl::ReadData(EFile* file, void* dst, unsigned int pos, unsigned int size, bool,
                                  EResLoadCmdResult*) {
  // The original also filled an EResLoadCmdResult {done, error, bytes}; its layout is not
  // recovered yet and every caller seen so far passes null.
  file->Seek(static_cast<int>(pos), EFile__SeekType::Set);
  return file->Read(dst, size);
}

// 80310A74
int EResourceLoaderImpl::ReadData(EResourceManager* mgr, void* dst, unsigned int pos, unsigned int size, bool sync,
                                  EResLoadCmdResult* result) {
  EFile* archive = mgr->GetArchiveFile();
  return archive ? ReadData(archive, dst, pos, size, sync, result) : 0;
}

// 80310B90
int EResourceLoaderImpl::WriteData(EFile* file, void* src, unsigned int pos, unsigned int size, bool,
                                   EResLoadCmdResult*) {
  file->Seek(static_cast<int>(pos), EFile__SeekType::Set);
  return file->Write(src, size);
}

// 8031013C
void EResourceLoaderImpl::GetQueueStats(int& a, int& b, int& c, int& d) { a = b = c = d = 0; }
// 80310160
int EResourceLoaderImpl::GetQueuedCommandCount() { return 0; }
// 80310184
int EResourceLoaderImpl::IsBusy() { return 0; }

// 80310CCC
EResourceManager* EResourceLoaderImpl::FindResourceManager(char* name) {
  std::lock_guard lock(m_mutex);
  return FindResourceManagerInternal(name);
}

// 80310D4C
EResourceManager* EResourceLoaderImpl::FindResourceManagerInternal(char* name) {
  for (EResourceManager* m = m_firstManager; m; m = m->m_nextManager) {
#ifdef _WIN32
    if (_stricmp(m->m_name, name) == 0) return m;
#else
    if (strcasecmp(m->m_name, name) == 0) return m;
#endif
  }
  return nullptr;
}

// 80310DAC
void EResourceLoaderImpl::OpenFiles() {
  FlushCommandQueue();
  std::lock_guard lock(m_mutex);
  for (EResourceManager* m = m_firstManager; m; m = m->m_nextManager) m->GetArchiveFile();
}

// 80310E4C
void EResourceLoaderImpl::CloseArchiveFile(EResourceManager* mgr) { mgr->CloseArchiveFile(); }

// 80310FDC
void EResourceLoaderImpl::PrintLoadedResources() {}
// 80311068
void EResourceLoaderImpl::PrintLoadedResources(char*) {}
// 803110EC
void EResourceLoaderImpl::PrintResourceSizes() {}
// 80311174
void EResourceLoaderImpl::PrintResourceSizes(char*) {}

// 803111F8
int EResourceLoaderImpl::FreeUnreferencedResources() {
  int freed = 0;
  for (EResourceManager* m = m_firstManager; m; m = m->m_nextManager) freed += m->FreeUnreferencedResources();
  return freed;
}

// 80311260
int EResourceLoaderImpl::FreeUnreferencedResources(char* name) {
  EResourceManager* m = FindResourceManager(name);
  return m ? m->FreeUnreferencedResources() : 0;
}

// 80311310
int EResourceLoaderImpl::GetUnreferencedResourceCount() {
  int n = 0;
  for (EResourceManager* m = m_firstManager; m; m = m->m_nextManager) n += m->GetUnreferencedResourceCount();
  return n;
}

// 80311360
int EResourceLoaderImpl::GetUnreferencedResourceCount(char* name) {
  EResourceManager* m = FindResourceManager(name);
  return m ? m->GetUnreferencedResourceCount() : 0;
}

// 803113F0
void EResourceLoaderImpl::deallocateGlobalIndex() {
  m_index[0].clear();
  m_index[1].clear();
}

// 8031146C
// Loads DATA/INDEX.IND and DATA/INDEX1.IND (an EFolder: count, offsets[count + 1], blocks), then
// re-points every registered manager at its block. Retries each open up to 10 times, as the disc
// version did.
void EResourceLoaderImpl::allocateGlobalIndex() {
  deallocateGlobalIndex();
  for (int i = 0; i < 2; ++i) {
    std::string path = "DATA/INDEX" + (i > 0 ? std::to_string(i) : std::string()) + ".IND";
    for (int attempt = 0; attempt < 10; ++attempt) {
      EFile* f = nullptr;
      _eorFileSys.Create(f, const_cast<char*>(path.c_str()), const_cast<char*>("rb"),
                         static_cast<int>(0x80000000u), static_cast<int>(0x80000000u), 0);
      if (!f) continue;
      unsigned char countBE[4];
      f->Read(countBE, 4);
      const unsigned int count = simmy::be32(countBE);
      std::vector<unsigned char> offsets((count + 1) * 4);
      f->Read(offsets.data(), static_cast<unsigned int>(offsets.size()));
      const unsigned int total = simmy::be32(offsets.data() + 4 * count);
      std::vector<unsigned char>& block = m_index[i];
      block.resize(total);
      std::memcpy(block.data(), countBE, 4);
      std::memcpy(block.data() + 4, offsets.data(), offsets.size());
      const unsigned int headerSize = 4 + static_cast<unsigned int>(offsets.size());
      if (total > headerSize) f->Read(block.data() + headerSize, total - headerSize);
      _eorFileSys.Destroy(f);
      std::fprintf(stderr, "[res] %s: %u index blocks\n", path.c_str(), count / 2);
      break;
    }
  }
  for (EResourceManager* m = m_firstManager; m; m = m->m_nextManager) m->m_index = getIndexPointer(m->m_name);
}

// 80311680
// Each index file is a list of (manager name, index block) pairs; the name match is case-insensitive.
const unsigned char* EResourceLoaderImpl::getIndexPointer(const char* managerName) {
  for (const std::vector<unsigned char>& block : m_index) {
    if (block.empty()) continue;
    const unsigned int count = simmy::be32(block.data());
    for (unsigned int i = 0; i + 1 < count; i += 2) {
      const char* name = reinterpret_cast<const char*>(block.data() + simmy::be32(block.data() + 4 + 4 * i));
#ifdef _WIN32
      if (_stricmp(name, managerName) == 0)
#else
      if (strcasecmp(name, managerName) == 0)
#endif
        return block.data() + simmy::be32(block.data() + 4 + 4 * (i + 1));
    }
  }
  return nullptr;
}

// 80311A50
EResource* EResourceLoaderImpl::DoLoadResource(EResourceManager* mgr, unsigned int id, EFile* file, unsigned int pos,
                                               unsigned int size) {
  int restorePos = 0;
  ModMemFile* modFile = nullptr;
  if (!file) {
    if (!mgr->LookupId(id, pos, size)) {
      // PC mod: mods. An id the archive lacks may be a mod's (only when a mod folder exists).
      std::vector<unsigned char> mod;
      if (!simmy::mods_enabled() || !simmy::mods_resource(mgr->m_name, id, mod)) return nullptr;
      size = static_cast<unsigned int>(mod.size());
      file = modFile = new ModMemFile(std::move(mod));
    } else {
      file = mgr->GetArchiveFile();
      if (!file) return nullptr;
      file->Seek(static_cast<int>(pos), EFile__SeekType::Set);
    }
  } else if (std::strcmp(mgr->m_name, "simdatas") == 0) {
    file->Seek(static_cast<int>(pos), EFile__SeekType::Set);
  } else {
    restorePos = file->Tell() + static_cast<int>(size);  // leave the caller's file after this resource
  }
  // PC: the frame-hitch profiler (perf.hpp) times each load (its disc reads count as disc time).
  const bool perfOn = simmy::perf::on();
  const auto perfT0 = perfOn ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  EResource* res;
  {
    SIMMY_PERF_SCOPE(Load);
    res = mgr->AllocateAndLoadResource(file, size, id);
  }
  if (perfOn)
    simmy::perf::count_load(mgr->m_name, id, size,
                            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() - perfT0).count()));
  static const bool log = std::getenv("SIMMY_LOG_LOADS") != nullptr;  // PC: trace every load
  if (log) std::fprintf(stderr, "[load] %-12s %08X %7u bytes %s\n", mgr->m_name, id, size, res ? "" : "FAILED");
  if (res) {
    if ((res->m_flags >> 8) == 0) res->m_flags = (size << 8) | (res->m_flags & 0xff);
    mgr->AddResource(res, id, true);
    if (!restorePos) mgr->LogResourceLoad(res);
  }
  if (restorePos && file->Tell() != restorePos) file->Seek(restorePos, EFile__SeekType::Set);
  if (modFile) modFile->Destroy();
  return res;
}

// 80311C88
void EResourceLoaderImpl::EnableReloads(bool enable) { m_reloadsEnabled = enable; }
// 80311C90
void EResourceLoaderImpl::PauseLoads() { m_paused = 1; }
// 80311C9C
void EResourceLoaderImpl::UnPauseLoads() { m_paused = 0; }
// 80311CC8
int EResourceLoaderImpl::AreLoadsPaused() { return m_paused; }
