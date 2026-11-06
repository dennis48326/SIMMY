// Port of game/nghresfile.cpp, the memory card side (the read side of the default neighbourhood file is
// in nghresfile.cpp), and of the LZSS encoder of engine/lzsscompressor.cpp.
//
// The save, "Sims 2 Save" (s_nghLayout, 0x126000 bytes, 147 blocks), is fixed-size sections:
//   0x000000  0x4000          the card banner, icon and comments; the options at 0x2000 (OptionsRecon)
//   0x004000  0x2000          the validity mark: 0xDEADBEEF once the save is complete
//   0x006000  0x2000          "host": 'TATT', and each house's 'SIMI' and 'HOUS'
//   0x008000  0xA000          "both": 'NGBH' 'NBRS' 'FAMI' 'User' (the neighbourhood and families)
//   0x012000  36 x 0x800      the family thumbnails ('thmb', ThumbnailCompresser)
//   0x024000  16 x 0x10000    the houses: 'Arry' 'objt' 'ObjM' 'DUMP'
//   0x124000  4 x 0x400       'UidR' registry sections 'Uid1'..'Uid4'
//   0x125000  4 x 0x400       'Uixx' registry sections 'Uix1'..'Uix4' (SerializeUIGameData)
// A section is a 0x1C-byte header (kind, header size, raw size, compressed size, checksum, version 8,
// resource count) then, LZSS-compressed, the resources' type, offset, size and id tables and their
// data. On the card the header and tables are big-endian (the GameCube's own order; the default
// neighbourhood file has them little-endian); the resources' bytes are kept as they are (little-endian,
// NghResFile::IsLittleEndian). The checksum is the CRC-32 (EChecksum::Compute) of the header as stored,
// its checksum field 0, and the compressed data.
//
// The game writes every section into the card cache (MemoryCardCache) first; SaveAllToMemoryCard then
// creates the file and writes the cache out, the validity mark last. Loading reads the whole file into
// the cache and the sections are read from there as needed.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

extern char s_saveFileName[];  // 803DAB08 "Sims 2 Save" (memcardscreens.cpp); also s_configFileName

unsigned int NghResFile::m_uBothSectionChecksum = 0;
unsigned int NghResFile::m_uLastBothSectionChecksum = 0;

namespace {
// 80424150: the layout's kinds (version 8), offsets and totals filled in at start-up; a zero entry ends it.
NghLayout::Entry s_nghSectionLayout[] = {
    {1, 0x4000, 0, 0, 0, 0},       {1, 0x2000, 0, 0, 0, 0},      {1, 0x2000, 0, 0, 0, 0},
    {1, 0xA000, 0, 0, 0, 0},       {0x24, 0x800, 0, 0, 0, 0},    {0x10, 0x10000, 0, 0, 0, 0},
    {4, 0x400, 0, 0, 0x55696452 /*UidR*/, 0x55696431 /*Uid1*/},
    {4, 0x400, 0, 0, 0x55697878 /*Uixx*/, 0x55697831 /*Uix1*/},
    {0, 0, 0, 0, 0, 0}};

// 800D9264 (static initialisation): each kind's offset and total, the counts and the largest section.
// (GC also set up the version 7 layout, s_nghLayout7, for older saves; not ported.)
NghLayout MakeLayout() {
  NghLayout l;
  l.m_version = 8;
  l.m_entries = s_nghSectionLayout;
  for (NghLayout::Entry* e = s_nghSectionLayout; e->count > 0; ++e) {
    if (static_cast<unsigned int>(e->size) > l.m_maxSize) l.m_maxSize = static_cast<unsigned int>(e->size);
    e->offset = l.m_totalSize;
    l.m_numSections += e->count;
    ++l.m_numKinds;
    e->total = static_cast<unsigned int>(e->size * e->count);
    l.m_totalSize += e->total;
  }
  return l;
}

const NghLayout::Entry& Kind(int k) { return s_nghLayout.m_entries[k]; }
enum : int { kBanner = 0, kValid = 1, kHost = 2, kBoth = 3, kThumbs = 4, kHouses = 5 };

constexpr unsigned int kTATT = 0x54415454, kSIMI = 0x53494d49, kHOUS = 0x484f5553, kNGBH = 0x4e474248,
                       kNBRS = 0x4e425253, kFAMI = 0x46414d49, kUser = 0x55736572, kThmb = 0x74686d62,
                       kArry = 0x41727279, kObjt = 0x6f626a74, kObjM = 0x4f626a4d, kDUMP = 0x44554d50;
const char kNghFileExt[] = ".ngh";  // nghfileExt

unsigned int BE32(const unsigned char* p) { return static_cast<unsigned int>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
unsigned int LE32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<unsigned int>(p[3]) << 24; }
void PutBE32(unsigned char* p, unsigned int v) {
  p[0] = static_cast<unsigned char>(v >> 24), p[1] = static_cast<unsigned char>(v >> 16);
  p[2] = static_cast<unsigned char>(v >> 8), p[3] = static_cast<unsigned char>(v);
}
void PutBE16(unsigned char* p, unsigned int v) { p[0] = static_cast<unsigned char>(v >> 8), p[1] = static_cast<unsigned char>(v); }
// A word of a section's header or tables: big-endian from the card (cache), little-endian from the file.
unsigned int Word(const unsigned char* p, bool big) { return big ? BE32(p) : LE32(p); }
unsigned short Half(const unsigned char* p, bool big) { return static_cast<unsigned short>(big ? p[0] << 8 | p[1] : p[1] << 8 | p[0]); }

// NghResFileSectionHeader, host-endian.
struct Header {
  unsigned int kind, hdrSize, rawSize, compSize, checksum, version, count;
};
Header ReadHeader(const unsigned char* p, bool big) {
  return {Word(p, big), Word(p + 4, big), Word(p + 8, big), Word(p + 12, big), Word(p + 16, big), Word(p + 20, big), Word(p + 24, big)};
}
// The header as DecompressSection leaves it at the start of its result (host order on PC).
Header HostHeader(const std::vector<unsigned char>& v) {
  Header h;
  std::memcpy(&h, v.data(), sizeof h);
  return h;
}
void PutHeader(unsigned char* p, const Header& h) {
  const unsigned int w[7] = {h.kind, h.hdrSize, h.rawSize, h.compSize, h.checksum, h.version, h.count};
  for (int i = 0; i < 7; ++i) PutBE32(p + i * 4, w[i]);
}
// The checksum of a stored section: its header (as stored, the checksum field 0) and compressed data.
unsigned int SectionChecksum(const unsigned char* p, unsigned int size) {
  std::vector<unsigned char> copy(p, p + size);
  if (size >= 0x14) std::memset(copy.data() + 0x10, 0, 4);
  return static_cast<unsigned int>(EChecksum::Compute(copy.data(), static_cast<int>(size)));
}

bool ContainsNoCase(const std::string& s, const char* what) {
  const size_t n = std::strlen(what);
  for (size_t i = 0; i + n <= s.size(); ++i) {
    size_t k = 0;
    while (k < n && std::tolower(static_cast<unsigned char>(s[i + k])) == what[k]) ++k;
    if (k == n) return true;
  }
  return false;
}

// 800D3920 deleteList
void FreeList(NghResFile::ResNode* node) {
  while (node) {
    NghResFile::ResNode* next = node->m_next;
    if (Memory__HandleNode* h = node->m_handle) {
      if (h->m_owned) delete[] h->m_data;
      delete h;
    }
    delete node;
    node = next;
  }
}
void FreeList(NghResFile::ResNode** list) {
  if (!list) return;
  FreeList(*list);
  *list = nullptr;
}

// 800D399C calculateDataSizeForList: the bytes of a list's resources (each 4-aligned) and their count.
unsigned int DataSizeOfList(NghResFile::ResNode* node, int& count) {
  unsigned int size = 0;
  count = 0;
  for (; node; node = node->m_next)
    if (node->m_handle) {
      ++count;
      size = ((size + 3) & ~3u) + node->m_handle->m_size;
    }
  return size;
}

Memory__HandleNode* NewHandle(const unsigned char* data, unsigned int size) {
  auto* h = new Memory__HandleNode;
  h->m_size = size;
  h->m_data = size ? new unsigned char[size] : nullptr;
  h->m_owned = 1;
  if (size && data) std::memcpy(h->m_data, data, size);
  return h;
}

StringBuffer& NoName() {
  static StringBuffer name;
  return name;
}

// A section being written: header, tables (count entries each) and data.
struct SectionBuilder {
  std::vector<unsigned char> buf;
  unsigned int types = 0, offsets = 0, sizes = 0, ids = 0, dataPos = 0, tableBytes = 0;
  int count = 0;
  SectionBuilder(int count_, unsigned int dataSize) : count(count_) {
    const unsigned int tables = (static_cast<unsigned int>(count) * 0xe + 3) & ~3u;
    buf.assign(0x1c + tables + dataSize, 0);
    types = 0x1c;
    offsets = types + count * 4;
    sizes = offsets + count * 4;
    ids = sizes + count * 4;
    dataPos = 0x1c + tables;
    tableBytes = 0x1c;
  }
  unsigned int RawSize() const { return static_cast<unsigned int>(buf.size()) - 0x1c; }
};

// LZSS-compresses the section into a block of `blockSize` bytes (header first) and checksums it.
// Empty when it does not fit.
std::vector<unsigned char> CompressSection(SectionBuilder& s, unsigned int kind, unsigned int blockSize) {
  std::vector<unsigned char> out(blockSize, 0);
  const int n = static_cast<int>(LZSSCompressor::Encode(s.buf.data() + 0x1c, static_cast<int>(s.RawSize()), out.data() + 0x1c,
                                                        static_cast<int>(blockSize) - 0x1c));
  if (n < 1) return {};
  Header h{kind, 0x1c, s.RawSize(), static_cast<unsigned int>(n), 0, 8, static_cast<unsigned int>(s.count)};
  PutHeader(out.data(), h);
  PutBE32(out.data() + 0x10, SectionChecksum(out.data(), 0x1c + h.compSize));
  return out;
}
}  // namespace

NghLayout s_nghLayout = MakeLayout();  // 8048632C

// ---------------------------------------------------------------------------------------------------
// The save's size and the card cache's front ends

// 800D26E0
RET_UNKNOWN GetTotalSaveGameSize() { return static_cast<RET_UNKNOWN>(s_nghLayout.m_totalSize); }
// 800D26EC
RET_UNKNOWN GetTotalNumberOfSaveGameSections() { return s_nghLayout.m_numKinds; }
// 800D26F8
RET_UNKNOWN GetNumNghSubSections(NghLayout* layout) { return layout->m_numSections; }
// 800D3860
RET_UNKNOWN getCurrentbuildVerNum(int a, int b, int c, int d) {
  return static_cast<RET_UNKNOWN>(static_cast<unsigned int>(a) << 24 | (b & 0xff) << 16 | (c & 0xff) << 8 | (d & 0xff));
}

// 800D2514
bool PutConfigInMemoryCardCache(char* data, unsigned int size) {
  return Globs_pNghResFile && Globs_pNghResFile->GetMemoryCardCache()->PutConfig(data, size) == 0;
}
// 800D255C
bool GetConfigFromMemoryCardCache(char* data, unsigned int size, int card) {
  return Globs_pNghResFile && Globs_pNghResFile->GetMemoryCardCache()->GetConfig(data, size, card) == 0;
}
// 800D25AC
bool SaveAllMemoryCardCache(int card) {
  return Globs_pNghResFile && Globs_pNghResFile->GetMemoryCardCache()->SaveAllToMemoryCard(card) == 0;
}
// 800D25EC
bool LoadCacheFromMemoryCard(int card) {
  return Globs_pNghResFile && Globs_pNghResFile->GetMemoryCardCache()->LoadCacheFromMemoryCard(card) == 0;
}
// 800D262C
bool LoadCacheFromMemoryCard(NghResFile* ngh, int card) {
  return ngh->GetMemoryCardCache()->LoadCacheFromMemoryCard(card, ngh) == 0;
}

// ---------------------------------------------------------------------------------------------------
// NghLayout

// 800D2700
// The kind holding `offset` (GC: one past it, as counted).
RET_UNKNOWN NghLayout::GetSectionNumberForOffset(unsigned int offset) {
  int n = 0;
  const Entry* e = m_entries;
  if (e->size < 1) return n;
  int left = static_cast<int>(offset) - static_cast<int>(e->total);
  if (left < 0) return n;
  do {
    ++n;
    if (e[1].size < 1) return n;
    left -= static_cast<int>(e[1].total);
    ++e;
  } while (left >= 0);
  return n;
}

// 800D2744
// The sub-section (counting every section of every kind) starting at or holding `offset`.
int NghLayout::GetSubSectionNumberForOffset(unsigned int offset) {
  int n = 0;
  int left = static_cast<int>(offset);
  const Entry* e = m_entries;
  if (e->size < 1) return n;
  for (;;) {
    if (e->count > 0) {
      left -= e->size;
      if (left < 0) return n;
      int k = 0;
      do {
        ++k;
        ++n;
        if (k >= e->count) break;
        left -= e->size;
      } while (left >= 0);
    }
    if (left < 0) return n;
    ++e;
    if (e->size < 1) return n;
  }
}

// 800D27CC
// Whether the section at `offset` belongs to a registered section: its registry id and sub-section
// ('Uid1'..'Uid4', 'Uix1'..'Uix4' by position).
int NghLayout::IsOffsetARegistrySubSection(unsigned int offset, NghResFile__RegistryID& id, NghResFile__SectionID& section) {
  section = 0;
  int kind = 0, sub = 0;
  unsigned int at = 0;
  if (offset != 0) {
    while (m_entries[kind].size != 0) {
      at += static_cast<unsigned int>(m_entries[kind].size);
      if (sub < m_entries[kind].count - 1) {
        ++sub;
      } else if (m_entries[kind].count > 0) {
        ++kind;
        sub = 0;
      }
      if (at == offset) break;
    }
  }
  id = static_cast<int>(m_entries[kind].registryId);
  if (!m_entries[kind].registryId) return 0;
  const unsigned int first = m_entries[kind].sectionId;
  if (first == 0x55696431 /*Uid1*/ || first == 0x55697831 /*Uix1*/) {
    if (sub > 3) return 0;
    section = static_cast<int>(first + static_cast<unsigned int>(sub));  // 'Uid1' + 1 = 'Uid2' ...
    return 1;
  }
  if (sub != 0) return 1;  // GC: other ids leave the sub-section 0
  section = static_cast<int>(first);
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// MemoryCardCache

// 800D29DC
RET_UNKNOWN MemoryCardCache::GetMemoryUsage(NghLayout* layout) {
  int total = 0;
  if (m_sections)
    for (int i = 0; i < layout->m_numSections; ++i)
      if (m_sections[i]) total += static_cast<int>(GetSubSectionMemoryUsage(layout, i));
  return total;
}
// 800D2A68
RET_UNKNOWN MemoryCardCache::GetSubSectionMemoryUsage(NghLayout* layout, int index) {
  if (!m_sections || !m_sections[index]) return 0;
  if (static_cast<unsigned int>(index) < 2) return layout->m_entries[index].size;
  return BE32(m_sections[index] + 0xc) + BE32(m_sections[index] + 4);
}

// 800D2AB4
// An empty cache (a block pointer per sub-section) and the options' block.
int MemoryCardCache::Init(NghLayout* layout) {
  if (m_initialized) return 0;
  m_configValid = 0;
  m_valid = 0;
  m_layout = layout;
  if (m_sections) {
    for (int i = 0; i < layout->m_numSections; ++i) delete[] m_sections[i];
    delete[] m_sections;
  }
  m_sections = new unsigned char*[layout->m_numSections]();
  if (!m_config) m_config = new unsigned char[0x2000]();
  m_initialized = 1;
  return 0;
}

// 800D2BEC
void MemoryCardCache::Destroy() {
  if (m_sections) {
    for (int i = 0; i < m_layout->m_numSections; ++i) delete[] m_sections[i];
    delete[] m_sections;
    m_sections = nullptr;
  }
  delete[] m_config;
  m_config = nullptr;
  m_initialized = 0;
  m_valid = 0;
  m_configValid = 0;
}

// 800D2CC0
// Copies the section at `offset` out (0: done, 1: the cache is empty, 3: not cached). Sections past the
// first two are copied only as far as they hold data, the rest of `dst` zeroed.
int MemoryCardCache::GetSection(NghLayout* layout, char* dst, unsigned int offset, unsigned int size) {
  if (!m_valid) return 1;
  const int n = layout->GetSubSectionNumberForOffset(offset);
  if (!m_sections || !m_sections[n]) return 3;
  const unsigned char* s = m_sections[n];
  if (offset != 0 && offset != Kind(kValid).offset) {
    const unsigned int stored = BE32(s + 0xc) + BE32(s + 4);
    if (stored < size) {
      std::memset(dst, 0, size);
      size = stored;
    }
  }
  std::memcpy(dst, s, size);
  return 0;
}

// 800D2D9C
// Keeps a copy of the section at `offset` (0: done, 1: not initialised).
int MemoryCardCache::PutSection(NghLayout* layout, char* src, unsigned int offset, unsigned int size) {
  if (!m_initialized || !m_sections) return 1;
  const int n = layout->GetSubSectionNumberForOffset(offset);
  delete[] m_sections[n];
  m_sections[n] = new unsigned char[size];
  std::memcpy(m_sections[n], src, size);
  m_valid = 1;
  return 0;
}

// 800D2E7C
// A section read from the card: the first two whole; the others checked against their checksum and
// kept as far as they hold data (4: bad).
int MemoryCardCache::CacheNghSection(NghLayout* layout, char* data, unsigned int offset, unsigned int size) {
  auto* p = reinterpret_cast<unsigned char*>(data);
  unsigned int keep;
  if (offset == 0 || offset == Kind(kValid).offset) {
    keep = static_cast<unsigned int>(layout->m_entries[layout->GetSubSectionNumberForOffset(offset)].size);
  } else {
    // GC: with m_noChecks set the header words were swapped and taken unchecked; it is never set.
    keep = BE32(p + 0xc) + BE32(p + 4);
    if (!m_noChecks) {
      if (keep == 0 || keep > size) return 4;
      if (SectionChecksum(p, keep) != BE32(p + 0x10)) return 4;
    }
  }
  return PutSection(layout, data, offset, keep);
}

// 800D2FBC
int MemoryCardCache::LoadCacheFromMemoryCard(int card) { return LoadCacheFromMemoryCard(card, Globs_pNghResFile); }

// 800D2FE0
// Reads every section of the save on `card` into the cache (the registered ones go to their owners);
// the neighbourhood file then reads from the cache. 0: done, 3: failed.
int MemoryCardCache::LoadCacheFromMemoryCard(int card, NghResFile* ngh) {
  m_noChecks = 0;
  const unsigned int ucard = static_cast<unsigned int>(card);
  int r = _pMemoryCard->OpenForIO(s_saveFileName, ucard);
  const bool opened = r == 1;
  if (opened) {
    std::vector<unsigned char> buf(s_nghLayout.m_maxSize);
    unsigned int offset = 0;
    int kind = 0, sub = 0;
    unsigned int size = static_cast<unsigned int>(Kind(0).size);
    while (size != 0) {
      r = _pMemoryCard->LoadDataChunkS(s_saveFileName, ucard, size, offset, buf.data());
      if (r != 1) break;
      NghResFile__RegistryID id;
      NghResFile__SectionID section;
      if (!s_nghLayout.IsOffsetARegistrySubSection(offset, id, section)) {
        if (CacheNghSection(&s_nghLayout, reinterpret_cast<char*>(buf.data()), offset, size) != 0) {
          r = 0;
          break;
        }
      } else if (ngh) {
        NghResFile__SaveRecord record;
        record.m_size = size;
        record.m_data = buf.data();
        ngh->SectionLoadViaRegistry(id, section, record);
      }
      offset += size;
      if (sub < Kind(kind).count - 1) {
        ++sub;
      } else if (Kind(kind).count > 0) {
        ++kind;
        sub = 0;
      }
      size = static_cast<unsigned int>(Kind(kind).size);
    }
    if (r == 1 && ngh) {
      ngh->m_card = static_cast<unsigned char>(card);
      ngh->m_fileName = s_saveFileName;
      ngh->m_field20 = 0;
    }
    _pMemoryCard->CloseForIO(s_saveFileName, ucard);
  }
  if (r == 1) {
    m_valid = 1;
    return 0;
  }
  return 3;
}

// 800D3258
// The options' block (0x2000 bytes at 0x2000 in the save). 0: read, 3: not.
int MemoryCardCache::LoadConfigFromMemoryCard(int card) {
  const unsigned int ucard = static_cast<unsigned int>(card);
  int r = _pMemoryCard->OpenForIO(s_saveFileName, ucard);  // s_configFileName
  if (r == 1) {
    r = _pMemoryCard->LoadDataChunkS(s_saveFileName, ucard, 0x2000, 0x2000, m_config);
    _pMemoryCard->CloseForIO(s_saveFileName, ucard);
  }
  if (r != 1) return 3;
  m_configValid = 1;
  return 0;
}

// 800D333C
// Writes the save: (re)creates the file (which writes the banner and icon), marks it invalid, writes
// every cached section (the registered ones from their owners; a missing one as 0x200 zero bytes) and
// the options, then marks it valid. 0: saved, 1: nothing to save, 4: failed.
int MemoryCardCache::SaveAllToMemoryCard(int card) {
  if (!m_valid || !m_configValid) return 1;
  const unsigned int ucard = static_cast<unsigned int>(card);
  bool opened = false;
  int r = _pMemoryCard->CreateFileS(s_saveFileName, ucard, static_cast<unsigned int>(GetTotalSaveGameSize()));
  if (r == 1) r = Globs_pNghResFile->ValidateSaveGame(s_saveFileName, card, false);
  if (r == 1) r = _pMemoryCard->OpenForIO(s_saveFileName, ucard);
  if (r == 1) {
    opened = true;
    std::vector<unsigned char> buf(s_nghLayout.m_maxSize);
    unsigned int offset = 0;
    int kind = 0, sub = 0;
    unsigned int size = static_cast<unsigned int>(Kind(0).size);
    while (size != 0) {
      bool registered = false;
      NghResFile__SaveRecord record;
      NghResFile__RegistryID id;
      NghResFile__SectionID section;
      unsigned int n;
      void* data = buf.data();
      if (!s_nghLayout.IsOffsetARegistrySubSection(offset, id, section)) {
        if (GetSection(&s_nghLayout, reinterpret_cast<char*>(buf.data()), offset, size) == 0) {
          n = size;
          if (offset != 0 && offset != Kind(kValid).offset) n = BE32(buf.data() + 0xc) + BE32(buf.data() + 4);
        } else {
          std::memset(buf.data(), 0, 0x200);
          n = 0x200;
        }
      } else {
        registered = true;
        Globs_pNghResFile->SectionSaveViaRegistry(id, section, record);
        n = record.m_size;
        data = record.m_data;
      }
      if (offset == 0) n = 0;  // the banner is the card layer's
      if (n != 0 && (r = _pMemoryCard->SaveDataChunkS(s_saveFileName, ucard, n, offset, data)) != 1) break;
      if (registered) Globs_pNghResFile->SaveCompleteViaRegistry(id, section, record);
      offset += size;
      if (sub < Kind(kind).count - 1) {
        ++sub;
      } else if (Kind(kind).count > 0) {
        ++kind;
        sub = 0;
      }
      size = static_cast<unsigned int>(Kind(kind).size);
    }
  }
  if (opened) _pMemoryCard->CloseForIO(s_saveFileName, ucard);
  if (r == 1 && (r = _pMemoryCard->SaveDataChunkS(s_saveFileName, ucard, 0x2000, 0x2000, m_config)) == 1)
    r = Globs_pNghResFile->ValidateSaveGame(s_saveFileName, card, true);
  return r == 1 ? 0 : 4;
}

// 800D3720
// The options' block, loaded from `card` first if not held (0: copied, 1: not loaded).
int MemoryCardCache::GetConfig(char* dst, unsigned int size, int card) {
  if (!m_configValid) {
    if (!m_config) m_config = new unsigned char[0x2000]();
    if (LoadConfigFromMemoryCard(card) != 0) return 1;
  }
  std::memcpy(dst, m_config, size);
  return 0;
}

// 800D37C8
int MemoryCardCache::PutConfig(char* src, unsigned int size) {
  if (!m_initialized) return 1;
  if (!m_config) m_config = new unsigned char[0x2000]();
  std::memcpy(m_config, src, size);
  m_configValid = 1;
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// NghResFile: sections

// 800D43DC
int NghResFile::PrepareMemCardWrite(char*, unsigned char) {
  ++m_writes;
  return 1;
}
// 800D43FC
int NghResFile::FinishMemCardWrite(char*, unsigned char) {
  if (--m_writes < 1) m_uLastBothSectionChecksum = 0;
  return 1;
}

// 800D442C
// Appends the resources of `type` (from its list) to the section: each one's type, offset, size and id
// in the tables, its bytes 4-aligned in the data.
void NghResFile::WriteList(std::vector<unsigned char>& s, unsigned int type, unsigned int& types, unsigned int& offsets,
                           unsigned int& sizes, unsigned int& ids, unsigned int& dataPos, unsigned int& tableBytes) {
  ResNode** list = findListByResType(type);
  for (ResNode* n = list ? *list : nullptr; n; n = n->m_next) {
    if (!n->m_handle) continue;
    PutBE32(s.data() + types, type);
    types += 4;
    dataPos = (dataPos + 3) & ~3u;
    PutBE32(s.data() + offsets, dataPos);
    offsets += 4;
    const unsigned int size = n->m_handle->m_size;
    if (dataPos + size > s.size()) s.resize(dataPos + size, 0);
    if (size) std::memcpy(s.data() + dataPos, n->m_handle->m_data, size);
    dataPos += size;
    PutBE32(s.data() + sizes, size);
    sizes += 4;
    PutBE16(s.data() + ids, n->m_id);
    ids += 2;
    tableBytes += 0xe;
  }
}

// 800D4558
// 0 the host section ('TATT'), 1 the "both" section ('NGBH'), 2 the thumbnails, 3 the current house.
bool NghResFile::IsSectionLoaded(unsigned int section) const {
  switch (section) {
    case 0: return m_nghLists[2] != nullptr;
    case 1: return m_nghLists[0] != nullptr;
    case 2: return m_userLists[1] != nullptr;
    case 3: return AreHouseContentsLoaded(m_currentHouse + 1);
    default: return false;
  }
}

// 800D4614
void NghResFile::FlushSection(unsigned int section) {
  if (!IsSectionLoaded(section)) return;
  const unsigned int house = m_currentHouse + 1;
  switch (section) {
    case 1:
      for (unsigned int t : {kNGBH, kNBRS, kFAMI, kUser}) FreeList(findListByResType(t));
      break;
    case 0:
      FreeList(findListByResType(kTATT));
      for (unsigned int h = 1; h <= 16; ++h) {
        SetCurrentHouse(h);
        FreeList(findListByResType(kSIMI));
        FreeList(findListByResType(kHOUS));
      }
      SetCurrentHouse(house);
      break;
    case 2:
      FreeList(findListByResType(kThmb));
      for (unsigned int& g : m_thumbGUIDs) g = 0;
      break;
    case 3: FlushHouseContents(house); break;
    default: break;
  }
}

// 800D3F84
void NghResFile::FlushHouseData(unsigned int house) {
  ResNode** lists = m_houseLists[(house - 1) & 15];
  for (int i = 0; i < 6; ++i) FreeList(&lists[i]);
}
// 800D3FE0
// The 'User' list; with `all` also the thumbnails and their GUIDs.
void NghResFile::FlushCharacterData(bool all) {
  FreeList(&m_userLists[0]);
  if (all) {
    FreeList(&m_userLists[1]);
    for (unsigned int& g : m_thumbGUIDs) g = 0;
  }
}
// 800D407C
void NghResFile::FlushNeighborData() {
  for (int i = 0; i < 4; ++i) FreeList(&m_nghLists[i]);
}
// 800D40D0
void NghResFile::FlushAllData() {
  for (unsigned int s = 0; s < 4; ++s) FlushSection(s);
  GetMemoryCardCache()->Destroy();
  FlushAllHouseContents();
  FlushCharacterData(true);
  FlushNeighborData();
}

// 800D47E4
// A section's bytes: from the card cache when the source is the card, else from the .ngh file.
std::vector<unsigned char> NghResFile::ReadSection(unsigned int offset, unsigned int size, bool& fromCache, int& status) {
  std::vector<unsigned char> buf(size, 0);
  status = 0;
  if (!ContainsNoCase(m_fileName, kNghFileExt)) {
    status = 1;
    if (GetMemoryCardCache()->GetSection(&s_nghLayout, reinterpret_cast<char*>(buf.data()), offset, size) != 0) status = 0;
    fromCache = true;
  } else {
    simmy::DataFile* f = simmy::data_open(m_fileName.c_str());  // the game's .ngh (PC: on the disc)
    if (!f) return {};
    if (simmy::data_read_at(f, offset, buf.data(), size) == size) status = 1;
    simmy::data_close(f);
    fromCache = false;
  }
  if (status != 1) return {};
  return buf;
}

// 800D4960
// Checks the section's checksum and decompresses it: the header (host order) then the data. Thumbnails
// (kind 2) are ThumbnailCompresser-coded; one from the .ngh file is then swizzled to C8 tiles and its
// palette's red and blue swapped, as the card holds them.
std::vector<unsigned char> NghResFile::DecompressSection(const unsigned char* p, bool fromCache) {
  const bool big = fromCache;
  const unsigned int hdrSize = Word(p + 4, big), compSize = Word(p + 12, big);
  if (SectionChecksum(p, hdrSize + compSize) != Word(p + 16, big)) return {};
  const Header h = ReadHeader(p, big);
  if (h.version < 7 || h.version > 8) return {};
  std::vector<unsigned char> out(h.hdrSize + h.rawSize, 0);
  std::memcpy(out.data(), &h, sizeof h);
  unsigned char* data = out.data() + h.hdrSize;
  const unsigned char* src = p + h.hdrSize;
  if (h.kind == 2) {
    const int n = ThumbnailCompresser::Decode(const_cast<unsigned char*>(src), static_cast<int>(h.compSize), data,
                                              static_cast<int>(h.rawSize));
    if (n != static_cast<int>(h.rawSize)) return {};
    if (!fromCache && n != 0) {
      std::vector<unsigned char> linear(data, data + h.rawSize);
      ENgcTextureUtil::SwizzleToC8(linear.data() + 0xc, 0x20, 0x20, data + 0xc);
      for (unsigned char* c = data + 0x40c; c < data + 0x80c; c += 4) std::swap(c[0], c[2]);
    }
    return out;
  }
  const int n = static_cast<int>(LZSSCompressor::Decode(const_cast<unsigned char*>(src), static_cast<int>(h.compSize), data,
                                                         static_cast<int>(h.rawSize)));
  if (n != static_cast<int>(h.rawSize)) return {};
  return out;
}

// 800D4BBC
// The save's validity mark (at 0x4000): 0xDEADBEEF, or 0 while it is being written.
int NghResFile::ValidateSaveGame(char* name, int card, bool valid) {
  const unsigned int size = static_cast<unsigned int>(Kind(kValid).size);
  card = static_cast<signed char>(card);
  std::vector<unsigned char> buf(size, 0);
  PutBE32(buf.data(), valid ? 0xDEADBEEFu : 0u);
  int r = _pMemoryCard->OpenForIO(name, static_cast<unsigned int>(card));
  const bool opened = r == 1;
  if (opened) {
    r = _pMemoryCard->SaveDataChunkS(name, static_cast<unsigned int>(card), size, Kind(kValid).offset, buf.data());
    _pMemoryCard->CloseForIO(name, static_cast<unsigned int>(card));
  }
  return r;
}

// 800D4CFC
// A save is valid when its validity mark is 0xDEADBEEF.
RET_UNKNOWN NghResFile::IsSaveGameValid(char* name, int card, bool& valid) {
  valid = true;
  const unsigned int size = Kind(kValid).total;
  card = static_cast<signed char>(card);
  std::vector<unsigned char> buffer(size);
  int r = _pMemoryCard->OpenForIO(name, static_cast<unsigned int>(card));
  if (r == 1) {
    if (_pMemoryCard->LoadDataChunkS(name, static_cast<unsigned int>(card), size, Kind(kValid).offset, buffer.data()) == 1)
      valid = BE32(buffer.data()) == 0xDEADBEEFu;
    else
      valid = false;
    _pMemoryCard->CloseForIO(name, static_cast<unsigned int>(card));
    r = 1;
  }
  return r;
}

namespace {
// The resources of a decompressed section from `first` on (`count` of them), added to the file.
// `tables` is the start of the type table (after the header).
void AddResources(NghResFile& f, const std::vector<unsigned char>& s, unsigned int total, unsigned int first,
                  unsigned int count, bool big) {
  const unsigned char* types = s.data() + HostHeader(s).hdrSize;
  const unsigned char* offsets = types + total * 4;
  const unsigned char* sizes = offsets + total * 4;
  const unsigned char* ids = sizes + total * 4;
  for (unsigned int i = first; i < first + count; ++i) {
    const unsigned int type = Word(types + i * 4, big), offset = Word(offsets + i * 4, big), size = Word(sizes + i * 4, big);
    const auto id = static_cast<short>(Half(ids + i * 2, big));
    const unsigned char* data = offset + size <= s.size() ? s.data() + offset : nullptr;
    f.Add(NewHandle(data, data ? size : 0), static_cast<int>(type), id, NoName(), true);
  }
}
}  // namespace

// 800D4E5C
// The host section: 'TATT', then each house's 'SIMI' and 'HOUS' (one of each, in house order).
// 1: read (or already), 0: not. (GC: a bad section was not checked for and crashed.)
int NghResFile::ReadHostSection(const unsigned char* section, bool fromCache) {
  if (IsSectionLoaded(0)) return 1;
  int status = 1;
  std::vector<unsigned char> read;
  if (!section) {
    read = ReadSection(Kind(kHost).offset, Kind(kHost).total, fromCache, status);
    section = read.data();
  }
  if (status != 1) return status;
  const std::vector<unsigned char> s = DecompressSection(section, fromCache);
  if (s.empty()) return -4;
  const unsigned int total = HostHeader(s).count;
  if (total < 33) return -4;
  AddResources(*this, s, total, 0, 1, fromCache);
  const unsigned int current = m_currentHouse;
  for (unsigned int h = 1; h <= 16; ++h) {
    SetCurrentHouse(h);
    AddResources(*this, s, total, 1 + (h - 1) * 2, 2, fromCache);
  }
  SetCurrentHouse(current + 1);
  return status;
}

// 800D51A8
// The "both" section: the neighbourhood ('NGBH'), neighbours ('NBRS'), families ('FAMI') and 'User'.
int NghResFile::ReadBothSection(const unsigned char* section, bool fromCache) {
  if (IsSectionLoaded(1)) return 1;
  int status = 1;
  std::vector<unsigned char> read;
  if (!section) {
    read = ReadSection(Kind(kBoth).offset, Kind(kBoth).total, fromCache, status);
    section = read.data();
  }
  if (status != 1) return status;
  const std::vector<unsigned char> s = DecompressSection(section, fromCache);
  if (s.empty()) return -4;
  const Header h = HostHeader(s);
  AddResources(*this, s, h.count, 0, h.count, fromCache);
  if (m_storeChecksum && m_field20 == 0) m_uBothSectionChecksum = h.checksum;
  return status;
}

// 800D53E4
int NghResFile::GetThumbIdFromFamilyAndIndex(int family, int index) {
  if (family == 1 && index == 1) return 3;
  if (family < 5) return family;
  return (family - 0x14) * 4 + index + 5;
}
// 800D541C
void NghResFile::StoreFamilyThumbGUID(int family, int index, unsigned int guid) {
  m_thumbGUIDs[(GetThumbIdFromFamilyAndIndex(family, index) - 1) % 36] = guid;
}
// 800D545C
void NghResFile::StoreThumbGUID(unsigned int id, unsigned int guid) { m_thumbGUIDs[(id - 1) % 36] = guid; }
// 800D5470
unsigned int NghResFile::GetThumbGUID(unsigned int id) { return m_thumbGUIDs[(id - 1) % 36]; }
// 800D5484
short NghResFile::GetResIDFromThumbGUID(unsigned int guid) {
  for (short i = 0; i < 36; ++i)
    if (m_thumbGUIDs[i] == guid) return static_cast<short>(i + 1);
  return 0;
}

// 800D54C0
// The 36 thumbnail sections (after the "both" section): each held one is 'thmb' resource i + 1, its
// GUID (the header's count field) remembered.
int NghResFile::ReadThumbSection(const unsigned char* sections, bool fromCache) {
  if (IsSectionLoaded(2)) return 1;
  if (!IsSectionLoaded(1)) return 0;
  const unsigned int size = static_cast<unsigned int>(Kind(kThumbs).size);
  int status = 1;
  for (unsigned int i = 0; i < 36 && status == 1; ++i) {
    std::vector<unsigned char> read;
    const unsigned char* section;
    if (!sections) {
      read = ReadSection(Kind(kThumbs).offset + i * size, size, fromCache, status);
      section = read.data();
    } else {
      section = sections + i * size;
    }
    if (status != 1) break;
    const std::vector<unsigned char> s = DecompressSection(section, fromCache);
    if (s.empty()) continue;  // GC: not checked
    const Header h = HostHeader(s);
    if (h.compSize == 0 || h.count == 0) continue;
    StoreThumbGUID(i + 1, h.count);
    Add(NewHandle(s.data() + h.hdrSize, h.rawSize), static_cast<int>(kThmb), static_cast<short>(i + 1), NoName(), true);
  }
  return status;
}

// 800D56CC
// Builds the host section ('TATT', each house's 'SIMI' and 'HOUS') into the card cache, or `out`.
// 1: done, 0: not cached, -4: does not fit.
int NghResFile::WriteHostSection(void* out) {
  int count = 0, n = 0;
  unsigned int data = (DataSizeOfList(*findListByResType(kTATT), n) + 3) & ~3u;
  count += n;
  const unsigned int current = m_currentHouse;
  for (unsigned int h = 1; h <= 16; ++h) {
    SetCurrentHouse(h);
    const unsigned int simi = DataSizeOfList(*findListByResType(kSIMI), n);
    count += n;
    const unsigned int hous = DataSizeOfList(*findListByResType(kHOUS), n);
    count += n;
    data = (((data + simi + 3) & ~3u) + hous + 3) & ~3u;
  }
  SectionBuilder b(count, data);
  WriteList(b.buf, kTATT, b.types, b.offsets, b.sizes, b.ids, b.dataPos, b.tableBytes);
  for (unsigned int h = 1; h <= 16; ++h) {
    SetCurrentHouse(h);
    WriteList(b.buf, kSIMI, b.types, b.offsets, b.sizes, b.ids, b.dataPos, b.tableBytes);
    WriteList(b.buf, kHOUS, b.types, b.offsets, b.sizes, b.ids, b.dataPos, b.tableBytes);
  }
  SetCurrentHouse(current + 1);
  const unsigned int block = Kind(kHost).total;
  const std::vector<unsigned char> s = CompressSection(b, 0, block);
  if (s.empty()) return -4;
  const unsigned int stored = BE32(s.data() + 0xc) + BE32(s.data() + 4);
  if (!out) {
    if (GetMemoryCardCache()->PutSection(&s_nghLayout, const_cast<char*>(reinterpret_cast<const char*>(s.data())),
                                         Kind(kHost).offset, stored) != 0)
      return 0;
  } else {
    std::memcpy(out, s.data(), block);
  }
  return 1;
}

// 800D5A40
// The "both" section ('NGBH' 'NBRS' 'FAMI' 'User'); its checksum is remembered (HasCardChanged).
// (GC: the block is sized by the section's offset, 0x8000, not its size.)
int NghResFile::WriteBothSection(void* out) {
  int count = 0, n = 0;
  unsigned int data = 0;
  for (unsigned int t : {kNGBH, kNBRS, kFAMI, kUser}) {
    const unsigned int size = DataSizeOfList(*findListByResType(t), n);
    count += n;
    data = (data + size + 3) & ~3u;
  }
  SectionBuilder b(count, data);
  for (unsigned int t : {kNGBH, kNBRS, kFAMI, kUser})
    WriteList(b.buf, t, b.types, b.offsets, b.sizes, b.ids, b.dataPos, b.tableBytes);
  const unsigned int block = Kind(kBoth).offset;
  const std::vector<unsigned char> s = CompressSection(b, 1, block);
  if (s.empty()) return -4;
  const unsigned int checksum = BE32(s.data() + 0x10);
  if (m_storeChecksum) {
    m_uLastBothSectionChecksum = m_uBothSectionChecksum;
    m_uBothSectionChecksum = checksum;
  }
  int r;
  if (!out) {
    r = GetMemoryCardCache()->PutSection(&s_nghLayout, const_cast<char*>(reinterpret_cast<const char*>(s.data())),
                                         Kind(kBoth).offset, BE32(s.data() + 0xc) + BE32(s.data() + 4)) == 0;
  } else {
    r = 1;
    std::memcpy(out, s.data(), block);
  }
  if (m_storeChecksum && r == 1) m_uLastBothSectionChecksum = m_uBothSectionChecksum;
  return r;
}

// 800D5DC4
// The 36 thumbnail sections: thumbnail i + 1 (when it has a GUID and a 'thmb' resource) coded with
// ThumbnailCompresser, its GUID in the header's count field; an empty header otherwise.
int NghResFile::WriteThumbSection(void* out) {
  const unsigned int size = static_cast<unsigned int>(Kind(kThumbs).size);
  auto* dst = static_cast<unsigned char*>(out);
  for (unsigned int i = 0; i < 36; ++i) {
    std::vector<unsigned char> block(size, 0);
    const unsigned int id = i + 1;
    Memory__HandleNode* thumb = GetThumbGUID(id) ? GetByID(static_cast<int>(kThmb), static_cast<short>(id), nullptr) : nullptr;
    Header h{2, 0x1c, thumb ? thumb->m_size : 0, 0, 0, 8, 0};
    if (thumb) {
      h.count = GetThumbGUID(id);
      const int n = ThumbnailCompresser::Encode(thumb->m_data, static_cast<int>(thumb->m_size), block.data() + 0x1c,
                                                static_cast<int>(size) - 0x1c);
      if (n < 1) return -4;
      h.compSize = static_cast<unsigned int>(n);
    }
    PutHeader(block.data(), h);
    PutBE32(block.data() + 0x10, SectionChecksum(block.data(), h.hdrSize + h.compSize));
    int r;
    if (!out) {
      r = GetMemoryCardCache()->PutSection(&s_nghLayout, reinterpret_cast<char*>(block.data()), Kind(kThumbs).offset + i * size,
                                           h.hdrSize + h.compSize) == 0;
    } else {
      r = 1;
      std::memcpy(dst + i * size, block.data(), size);
    }
    if (r != 1) return r;
  }
  return 1;
}

// 800D6774
// The house's world arrays and objects ('Arry' 'objt' 'ObjM' 'DUMP') as its section (cached in whole
// 0x400-byte units). 0: the house is not loaded (or not cached), -4: does not fit.
int NghResFile::SaveHouseContents(unsigned int house, void* out) {
  if (!AreHouseContentsLoaded(house)) return 0;
  const unsigned int current = m_currentHouse;
  SetCurrentHouse(house);
  int count = 0, n = 0;
  unsigned int data = 0;
  for (unsigned int t : {kArry, kObjt, kObjM, kDUMP}) {
    const unsigned int size = DataSizeOfList(*findListByResType(t), n);
    count += n;
    data = (data + size + 3) & ~3u;
  }
  SectionBuilder b(count, data);
  for (unsigned int t : {kArry, kObjt, kObjM, kDUMP})
    WriteList(b.buf, t, b.types, b.offsets, b.sizes, b.ids, b.dataPos, b.tableBytes);
  const unsigned int block = static_cast<unsigned int>(Kind(kHouses).size);
  const std::vector<unsigned char> s = CompressSection(b, 3, block);
  if (s.empty()) {
    m_currentHouse = current;  // GC: left on `house`
    return -4;
  }
  int r;
  if (!out) {
    const unsigned int stored = (BE32(s.data() + 0xc) + BE32(s.data() + 4) + 0x3ff) & ~0x3ffu;
    r = GetMemoryCardCache()->PutSection(&s_nghLayout, const_cast<char*>(reinterpret_cast<const char*>(s.data())),
                                         Kind(kHouses).offset + (house - 1) * block, stored) == 0;
    if (r != 1) --m_writes;
  } else {
    r = 1;
    std::memcpy(out, s.data(), block);
  }
  SetCurrentHouse(current + 1);
  return r;
}

// 800D6C34
// Rereads the host and "both" sections and the house from the cache. GC: around it
// EGlobal::BeginSaveGame/EndSaveGame (a graphics flush; the walls' and floors' memory freed in game)
// and a controller refresh.
int NghResFile::ReloadHouseAndGlobals(unsigned char, unsigned int house) {
  FlushSection(0);
  int r = ReadHostSection(nullptr, false);
  if (r == 1) {
    FlushSection(1);
    r = ReadBothSection(nullptr, false);
    if (r == 1) {
      FlushHouseContents(house);
      r = static_cast<int>(LoadHouseContents(house, nullptr, false));
    }
  }
  return r;
}

// 800D6D04
int NghResFile::SaveHouseAndGlobals(unsigned char, unsigned int house) {
  m_writes = 0;
  int r = WriteHostSection(nullptr);
  if (r == 1 && (r = WriteBothSection(nullptr)) == 1) r = SaveHouseContents(house, nullptr);
  return r;
}

// 800D6DBC
// Before loading: the "both" section's checksum from the cache (HasCardChanged compares with it); the
// card becomes the source. 1: done (or nothing cached), -4: not a section.
int NghResFile::ReadChecksumFromCard(char* name, int card) {
  NghResFile* f = Globs_pNghResFile;
  if (!f) return 0;
  std::vector<unsigned char> buf(0x2000, 0);
  if (f->GetMemoryCardCache()->GetSection(&s_nghLayout, reinterpret_cast<char*>(buf.data()), Kind(kBoth).offset, 0x2000) != 0)
    return 1;
  if (BE32(buf.data() + 4) != 0x1c) return -4;
  m_uBothSectionChecksum = BE32(buf.data() + 0x10);
  f->m_card = static_cast<unsigned char>(card);
  f->m_fileName = name;
  f->m_field20 = 0;
  return 1;
}

// 800D6E94
int NghResFile::ReadCharacterData(char* name, unsigned char card) {
  m_fileName = name;
  m_card = card;
  m_field20 = 0;
  FlushSection(0);
  int r = ReadHostSection(nullptr, false);
  if (r == 1) {
    FlushSection(1);
    r = ReadBothSection(nullptr, false);
  }
  return r;
}

// 800D6F64
int NghResFile::WriteCharacterData() {
  m_writes = 0;
  int r = WriteHostSection(nullptr);
  if (r == 1) r = WriteBothSection(nullptr);
  return r;
}

// 800D77B0
int NghResFile::ReadFromCache() {
  FlushSection(0);
  if (ReadHostSection(nullptr, false) != 1) return -1;
  FlushSection(1);
  if (ReadBothSection(nullptr, false) != 1) return -1;
  FlushSection(2);
  if (ReadThumbSection(nullptr, false) != 1) return -1;
  FlushAllHouseContents();
  return 0;
}

// 800D786C
// The neighbourhood into the card cache as save `name` on `card`: from the default file every house is
// loaded first (so all 16 are written); then the host, "both" and thumbnail sections and every loaded
// house (all but the current one flushed again). On success the card is the source from then on;
// otherwise the old name and card are kept. GC: around it EGlobal::BeginSaveGame/EndSaveGame.
int NghResFile::WriteToMemoryCard(char* name, unsigned char card) {
  int r = 0;
  m_writes = 0;
  m_field188 = 1;
  const std::string oldName = m_fileName;
  const unsigned char oldCard = m_card;
  const unsigned int current = m_currentHouse;
  bool ready = true;
  if (ContainsNoCase(m_fileName, kNghFileExt)) {
    for (unsigned int h = 1; h <= 16 && ready; ++h) {
      r = static_cast<int>(LoadHouseContents(h, nullptr, false));
      ready = r == 1;
    }
  }
  if (ready) {
    m_fileName = name;
    m_card = card;
    r = WriteHostSection(nullptr);
    if (r == 1 && (r = WriteBothSection(nullptr)) == 1 && (r = WriteThumbSection(nullptr)) == 1) {
      unsigned int h = 1;
      for (; h <= 16; ++h) {
        if (!AreHouseContentsLoaded(h)) continue;
        r = SaveHouseContents(h, nullptr);
        if (r != 1) break;
        if (h != current + 1) FlushHouseContents(h);
      }
      if (h > 16) m_field20 = 0;
    }
  }
  if (r != 1) {
    m_fileName = oldName;
    m_card = oldCard;
  }
  return r;
}

// 800D7A30
// Whether the save on `card` is another than the one last read or written (its "both" section's
// checksum differs from both remembered ones). 0: the source is the default file, -7: no save.
int NghResFile::HasCardChanged(int card, bool& changed) {
  if (m_field20 == 1) return 0;
  char* name = const_cast<char*>(m_fileName.c_str());
  const unsigned int ucard = static_cast<unsigned int>(card);
  bool exists = false;
  int r = _pMemoryCard->DoesFileExist(name, ucard, exists);
  if (r != 1) return r;
  if (!exists) return -7;
  r = _pMemoryCard->OpenForIO(name, ucard);
  if (r != 1) return r;
  std::vector<unsigned char> buf(0x2000, 0);
  r = _pMemoryCard->LoadDataChunkS(name, ucard, 0x2000, Kind(kBoth).offset, buf.data());
  if (r == 1) {
    const unsigned int sum = BE32(buf.data() + 0x10);
    changed = sum != m_uBothSectionChecksum && sum != m_uLastBothSectionChecksum;
  }
  _pMemoryCard->CloseForIO(name, ucard);
  return r;
}

// 800D85E4
// The save on `card` (already in the card cache) becomes the neighbourhood: the host, "both" and
// thumbnail sections are read; the houses as they are entered. GC: around it
// EGlobal::BeginSaveGame/EndSaveGame and a controller refresh.
int NghResFile::ReadFromMemoryCard(char* name, unsigned char card) {
  m_fileName = name;
  m_card = card;
  m_field20 = 0;
  FlushSection(0);
  int r = ReadHostSection(nullptr, false);
  if (r == 1) {
    FlushSection(1);
    r = ReadBothSection(nullptr, false);
    if (r == 1) {
      FlushSection(2);
      r = ReadThumbSection(nullptr, false);
      if (r == 1) {
        FlushAllHouseContents();
        m_field20 = 0;
      }
    }
  }
  return r;
}

// 800D8A28
MemoryCardCache* NghResFile::GetMemoryCardCache() {
  if (!m_cache->m_initialized) m_cache->Init(&s_nghLayout);
  return m_cache;
}

// ---------------------------------------------------------------------------------------------------
// NghResFile: the registry of extra sections

// 800D8AA8
int NghResFile::IsIDRegistered(NghResFile__RegistryID id) {
  NghResFile__RegistryNode node;
  return FindNodeInRegistry(id, node);
}
// 800D8AE4
// 1: registered, 0: the id was already.
int NghResFile::RegisterSection(NghResFile__RegistryID id, void (*save)(NghResFile__SectionID, NghResFile__SaveRecord&),
                                void (*load)(NghResFile__SectionID, NghResFile__SaveRecord&),
                                void (*complete)(NghResFile__SectionID, NghResFile__SaveRecord&)) {
  for (const NghResFile__RegistryNode& n : m_registry)
    if (n.m_id == id) return 0;
  NghResFile__RegistryNode node;
  node.m_id = id;
  node.m_save = save;
  node.m_load = load;
  node.m_complete = complete;
  m_registry.push_back(node);
  return 1;
}
// 800D8D10
int NghResFile::FindNodeInRegistry(NghResFile__RegistryID id, NghResFile__RegistryNode& node) {
  for (const NghResFile__RegistryNode& n : m_registry)
    if (n.m_id == id) {
      node = n;
      return 1;
    }
  return 0;
}
// 800D8D90
void NghResFile::SectionSaveViaRegistry(NghResFile__RegistryID id, NghResFile__SectionID section, NghResFile__SaveRecord& record) {
  NghResFile__RegistryNode node;
  if (FindNodeInRegistry(id, node) && node.m_save) node.m_save(section, record);
}
// 800D8E00
void NghResFile::SaveCompleteViaRegistry(NghResFile__RegistryID id, NghResFile__SectionID section, NghResFile__SaveRecord& record) {
  NghResFile__RegistryNode node;
  if (FindNodeInRegistry(id, node) && node.m_complete) node.m_complete(section, record);
}
// 800D8E70
void NghResFile::SectionLoadViaRegistry(NghResFile__RegistryID id, NghResFile__SectionID section, NghResFile__SaveRecord& record) {
  NghResFile__RegistryNode node;
  if (FindNodeInRegistry(id, node) && node.m_load) node.m_load(section, record);
}
// 800D8EE0
void NghResFile::SetStoreChecksum(bool store) { m_storeChecksum = store; }
// 800D8EE8
bool NghResFile::GetStoreChecksum() { return m_storeChecksum != 0; }
// 800D8EF0
void NghResFile::ResetChecksums() {
  m_uLastBothSectionChecksum = 0;
  m_uBothSectionChecksum = 0;
}
// 800D93CC
int NghResFile::GetSourceType() const { return m_field20; }

// ---------------------------------------------------------------------------------------------------
// ThumbnailCompresser

// 800D8F00
// Small data is stored as is (a 0 byte first); a whole thumbnail (a 2 byte first) as its header and
// pixels, then each palette entry's red, green and blue (the big-endian RGBA word's top three bytes,
// low first), then a bit per entry for a non-zero alpha. GC: `outSize` is not checked.
int ThumbnailCompresser::Encode(unsigned char* in, int inSize, unsigned char* out, int) {
  if (inSize < Kind(kThumbs).size) {
    if (inSize < 1) return 0;
    out[0] = 0;
    std::memcpy(out + 1, in, static_cast<size_t>(inSize));
    return inSize + 1;
  }
  out[0] = 2;
  std::memcpy(out + 1, in, 0x40c);
  unsigned char* alpha = out + 0x70d;
  std::memset(alpha, 0, 0x20);
  for (int i = 0; i < 0x100; ++i) {
    const unsigned int v = BE32(in + 0x40c + i * 4);
    unsigned char* rgb = out + 1 + 0x40c + i * 3;
    for (int k = 0; k < 3; ++k) rgb[k] = static_cast<unsigned char>(v >> (8 + 8 * k));
    if (v & 0xff) alpha[i >> 3] |= static_cast<unsigned char>(1 << (i & 7));
  }
  return 0x72d;
}

// 800D90D0
int ThumbnailCompresser::Decode(unsigned char* in, int inSize, unsigned char* out, int) {
  if (inSize == 0) return 0;
  if (in[0] == 0) {
    std::memcpy(out, in + 1, static_cast<size_t>(inSize - 1));
    return inSize - 1;
  }
  if (in[0] != 2) return -1;
  std::memcpy(out, in + 1, 0x40c);
  for (int i = 0; i < 0x100; ++i) {
    const unsigned char* rgb = in + 1 + 0x40c + i * 3;
    unsigned int v = static_cast<unsigned int>(rgb[0]) << 8 | static_cast<unsigned int>(rgb[1]) << 16 |
                     static_cast<unsigned int>(rgb[2]) << 24;
    if (in[0x70d + (i >> 3)] >> (i & 7) & 1) v |= 0xff;
    PutBE32(out + 0x40c + i * 4, v);
  }
  return 0x80c;
}

// ---------------------------------------------------------------------------------------------------
// LZSSCompressor::Encode (engine/lzsscompressor.cpp): Okumura's LZSS, a 4 KB window starting at 0xFEE,
// matches of 3..18 bytes found with binary search trees (InitTree, InsertNode, DeleteNode).

namespace {
constexpr int kN = 4096, kF = 18, kThreshold = 2, kNil = kN;
struct LzssTree {
  std::vector<unsigned char> text = std::vector<unsigned char>(kN + kF - 1, 0);
  std::vector<int> lson = std::vector<int>(kN + 1, 0), rson = std::vector<int>(kN + 257, 0), dad = std::vector<int>(kN + 1, 0);
  int matchPosition = 0, matchLength = 0;

  // 8030A2C4
  void InitTree() {
    for (int i = kN + 1; i <= kN + 256; ++i) rson[i] = kNil;
    for (int i = 0; i < kN; ++i) dad[i] = kNil;
  }
  // 8030A308
  void InsertNode(int r) {
    int cmp = 1;
    const unsigned char* key = &text[r];
    int p = kN + 1 + key[0];
    rson[r] = lson[r] = kNil;
    matchLength = 0;
    for (;;) {
      if (cmp >= 0) {
        if (rson[p] != kNil) {
          p = rson[p];
        } else {
          rson[p] = r;
          dad[r] = p;
          return;
        }
      } else {
        if (lson[p] != kNil) {
          p = lson[p];
        } else {
          lson[p] = r;
          dad[r] = p;
          return;
        }
      }
      int i = 1;
      for (; i < kF; ++i)
        if ((cmp = key[i] - text[p + i]) != 0) break;
      if (i > matchLength) {
        matchPosition = p;
        if ((matchLength = i) >= kF) break;
      }
    }
    dad[r] = dad[p];
    lson[r] = lson[p];
    rson[r] = rson[p];
    dad[lson[p]] = r;
    dad[rson[p]] = r;
    if (rson[dad[p]] == p) rson[dad[p]] = r;
    else lson[dad[p]] = r;
    dad[p] = kNil;
  }
  // 8030A47C
  void DeleteNode(int p) {
    if (dad[p] == kNil) return;
    int q;
    if (rson[p] == kNil) {
      q = lson[p];
    } else if (lson[p] == kNil) {
      q = rson[p];
    } else {
      q = lson[p];
      if (rson[q] != kNil) {
        do q = rson[q];
        while (rson[q] != kNil);
        rson[dad[q]] = lson[q];
        dad[lson[q]] = dad[q];
        lson[q] = lson[p];
        dad[lson[p]] = q;
      }
      rson[q] = rson[p];
      dad[rson[p]] = q;
    }
    dad[q] = dad[p];
    if (rson[dad[p]] == p) rson[dad[p]] = q;
    else lson[dad[p]] = q;
    dad[p] = kNil;
  }
};
}  // namespace

// 8030A604
// Returns the bytes written, -1 when `out` was too small (0 for no input).
RET_UNKNOWN LZSSCompressor::Encode(unsigned char* in, int inSize, unsigned char* out, int outSize) {
  const unsigned char* inPos = in;
  const unsigned char* const inEnd = in + inSize;
  unsigned char* outPos = out;
  unsigned char* const outEnd = out + outSize;
  bool full = false;
  auto getChar = [&]() -> int { return inPos != inEnd ? *inPos++ : -1; };  // GetChar (8030A5A0)
  auto putChar = [&](unsigned char c) {                                      // PutChar (8030A5D0)
    if (outPos == outEnd) full = true;
    else *outPos++ = c;
  };
  LzssTree t;
  t.InitTree();
  unsigned char code[17] = {};
  int codePtr = 1;
  unsigned int mask = 1;
  int s = 0, r = kN - kF, len = 0, codeSize = 0;
  for (int c; len < kF && (c = getChar()) != -1; ++len) t.text[r + len] = static_cast<unsigned char>(c);
  if (len == 0) return 0;
  for (int i = 1; i <= kF; ++i) t.InsertNode(r - i);
  t.InsertNode(r);
  do {
    if (t.matchLength > len) t.matchLength = len;
    if (t.matchLength <= kThreshold) {
      t.matchLength = 1;
      code[0] |= static_cast<unsigned char>(mask);
      code[codePtr++] = t.text[r];
    } else {
      code[codePtr++] = static_cast<unsigned char>(t.matchPosition);
      code[codePtr++] = static_cast<unsigned char>(((t.matchPosition >> 4) & 0xf0) | (t.matchLength - (kThreshold + 1)));
    }
    if ((mask <<= 1) == 0x100) {
      for (int i = 0; i < codePtr; ++i) putChar(code[i]);
      codeSize += codePtr;
      code[0] = 0;
      codePtr = mask = 1;
    }
    const int last = t.matchLength;
    int i = 0;
    for (int c; i < last && (c = getChar()) != -1; ++i) {
      t.DeleteNode(s);
      t.text[s] = static_cast<unsigned char>(c);
      if (s < kF - 1) t.text[s + kN] = static_cast<unsigned char>(c);
      s = (s + 1) & (kN - 1);
      r = (r + 1) & (kN - 1);
      t.InsertNode(r);
    }
    while (i++ < last) {
      t.DeleteNode(s);
      s = (s + 1) & (kN - 1);
      r = (r + 1) & (kN - 1);
      if (--len) t.InsertNode(r);
    }
  } while (len > 0);
  if (codePtr > 1) {
    for (int i = 0; i < codePtr; ++i) putChar(code[i]);
    codeSize += codePtr;
  }
  return full ? -1 : codeSize;
}
