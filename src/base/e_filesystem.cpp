// Port of base/e_filesystem.cpp: picks a platform file creator for each open request.
#include "simmy/types_gen.hpp"

#include <cstdio>
#include <cstring>

// ---- FileCreator -------------------------------------------------------------------------------

// 802C6A6C
EFileSystem__FileCreator::EFileSystem__FileCreator() = default;

// 802C6AA4
EFileSystem__FileCreator::EFileSystem__FileCreator(CreatorFn fn, EFile__DeviceType device, EFile__IOMode mode,
                                                   EFile__AccessMode access, char* ext)
    : m_fn(fn), m_device(device), m_mode(mode), m_access(access) {
  if (ext) {
    if (*ext == '.') ++ext;
    std::strncpy(m_ext, ext, 3);
    m_ext[3] = '\0';
  }
}

// 802C6B1C
bool EFileSystem__FileCreator::operator==(const EFileSystem__FileCreator& o) const {
  return m_fn == o.m_fn && m_device == o.m_device && m_mode == o.m_mode && m_access == o.m_access &&
         std::memcmp(m_ext, o.m_ext, 4) == 0;
}

// 802C6B7C
// Exact match scores `bit`; a creator registered for "any" (0x80000000) scores 0; a mismatch
// returns 0x80000000, which makes the total negative (rejected).
int EFileSystem__FileCreator::TallyBits(int mine, int wanted, int bit) const {
  if (mine == wanted) return bit;
  if (mine == static_cast<int>(0x80000000u)) return 0;
  return static_cast<int>(0x80000000u);
}

// 802C6BA0
int EFileSystem__FileCreator::GetScore(EFile__DeviceType device, EFile__IOMode mode, EFile__AccessMode access,
                                       char* ext) const {
  unsigned int score = static_cast<unsigned int>(TallyBits(m_device, device, 8)) |
                       static_cast<unsigned int>(TallyBits(m_mode, mode, 4));
  if (static_cast<int>(score) < 0) return static_cast<int>(score);
  score = (score | static_cast<unsigned int>(TallyBits(m_access, access, 2))) & 0x7fffffffu;
  if (ext && *ext) {
    if (*ext == '.') ++ext;
    if (m_ext[0] && std::strncmp(m_ext, ext, 3) == 0) score |= 1;
  }
  return static_cast<int>(score);
}

// 802C6C74
EFileSystem__FileCreator::CreatorFn EFileSystem__FileCreator::GetCreatorCB() const { return m_fn; }

// ---- EFileSystem -------------------------------------------------------------------------------

// 802C6964
EFileSystem::EFileSystem() { EGlobalManager::Register(this, 3); }

// 802C69D0
EFileSystem::~EFileSystem() {
  if (!EGlobalManager::m_shutdownComplete) EGlobalManager::Shutdown();
}

// 802C6A48
void EFileSystem::ManagedShutdown() { m_creators.clear(); }

// 802C6C7C
EFileSystem::FileCreator::CreatorFn EFileSystem::FindCreator(EFile::DeviceType device, EFile::IOMode mode,
                                                             EFile::AccessMode access, char* ext) const {
  int best = -1, bestScore = -1;
  for (int i = 0; i < static_cast<int>(m_creators.size()); ++i) {
    const int score = m_creators[i].GetScore(device, mode, access, ext);
    if (score > bestScore) {
      best = i;
      bestScore = score;
    }
  }
  // The original halted the machine when nothing matched.
  return best >= 0 ? m_creators[best].GetCreatorCB() : nullptr;
}

static void extension_of(const char* name, char* out, size_t outSize) {
  const char* dot = std::strrchr(name, '.');
  const char* slash = std::strpbrk(dot ? dot : name, "/\\");
  std::snprintf(out, outSize, "%s", (dot && !slash) ? dot : "");
}

// 802C6D44
int EFileSystem::Access(char* name, char* mode, EFile::DeviceType device, EFile::AccessMode access) {
  if (!m_initialized) Init(static_cast<int>(0x80000000u));
  EFile::IOMode ioMode;
  if (!ParseMode(mode, ioMode)) return 0;
  if (device == static_cast<int>(0x80000000u)) device = m_defaultDevice;
  ioMode = ioMode & ~EFile__IOMode::Append;
  char ext[260];
  extension_of(name, ext, sizeof(ext));
  FileCreator::CreatorFn fn = FindCreator(device, ioMode, 2, ext);
  if (!fn) return 0;
  EFile* file = fn(nullptr, name, mode, device, 2, 0);
  if (!file) return 0;
  Destroy(file);
  return 1;
}

// 802C6EA8
int EFileSystem::Create(EFile*& file, char* name, char* mode, EFile::DeviceType device, EFile::AccessMode access,
                        unsigned int flags) {
  if (!m_initialized) Init(static_cast<int>(0x80000000u));
  EFile::IOMode ioMode;
  file = nullptr;
  if (!ParseMode(mode, ioMode)) return 0;
  if (device == static_cast<int>(0x80000000u)) device = m_defaultDevice;
  ioMode = ioMode & ~EFile__IOMode::Append;
  char ext[260];
  extension_of(name, ext, sizeof(ext));
  FileCreator::CreatorFn fn = FindCreator(device, ioMode, access, ext);
  if (!fn) {
    std::fprintf(stderr, "[file] no file creator for \"%s\"\n", name);
    return 0;
  }
  file = fn(nullptr, name, mode, device, access, flags);
  return file ? 1 : 0;
}

// 802C7000
void EFileSystem::Destroy(EFile*& file) {
  file->Destroy();
  file = nullptr;
}

// 802C704C
int EFileSystem::Init(EFile::DeviceType defaultDevice) {
  if (m_initialized) return 0;
  if (!EGlobalManager::m_startupComplete) EGlobalManager::Startup();
  m_defaultDevice = defaultDevice;
  m_initialized = 1;
  return 1;
}

// 802C70AC
int EFileSystem::InitHDDFileSystem() { return 1; }
// 802C70B4
int EFileSystem::InitHDD(bool, char*) { return 0; }
// 802C70BC
bool EFileSystem::HDDInitialized() { return m_hddStatus == 0xb; }
// 802C70D0
int EFileSystem::HDDStatus() { return m_hddStatus; }
// 802C70D8
bool EFileSystem::HDDTmpInitialized() { return m_hddTmpStatus == 0xb; }
// 802C70EC
int EFileSystem::HDDTmpStatus() { return m_hddTmpStatus; }
// 802C70F4
bool EFileSystem::HDDSaveInitialized() { return m_hddSaveStatus == 0xb; }
// 802C7108
int EFileSystem::HDDSaveStatus() { return m_hddSaveStatus; }
// 802C7110
int EFileSystem::HDDIsUnformatted() { return 0; }
// 802C7118
int EFileSystem::GetDefaultType() const { return m_defaultDevice; }

// 802C7120
int EFileSystem::RegisterFileCreator(EFile::DeviceType device, EFile::IOMode mode, EFile::AccessMode access,
                                     char* ext, FileCreator::CreatorFn fn) {
  FileCreator c(fn, device, mode, access, ext);
  for (FileCreator& existing : m_creators)
    if (existing == c) return 1;
  m_creators.push_back(c);
  return 1;
}

// 802C71CC
int EFileSystem::ParseMode(char* mode, EFile::IOMode& out) {
  int m = std::strchr(mode, 'r') ? EFile__IOMode::Read : 0;
  if (std::strchr(mode, 'w')) m |= EFile__IOMode::Write | EFile__IOMode::Create;
  if (std::strchr(mode, 'a')) m |= EFile__IOMode::Write | EFile__IOMode::Append;
  if (std::strchr(mode, '+')) m |= EFile__IOMode::Read | EFile__IOMode::Write;
  out = m;
  return 1;
}
