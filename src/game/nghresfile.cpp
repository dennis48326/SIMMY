// Port of game/nghresfile.cpp (partial), the resource file base of game/iresfile.cpp and the LZSS
// decoder of engine/lzss.cpp: the neighbourhood save read from the default neighbourhood file
// (DEFAULT.NGH, laid out like the memory card save: fixed-size sections, each a little-endian header
// and an LZSS-compressed block), and a house's resources loaded into its lists.
//
// The memory card side (the card cache, the host, family and thumbnail sections, every write path)
// is in nghsave.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

extern char s_saveFileName[];  // 803DAB08 "Sims 2 Save" (memcardscreens.cpp)

namespace {
// 80424128: the resource types of the neighbourhood's own lists and of each house's lists.
const unsigned int kNghTypes[4] = {0x4e474248 /*NGBH*/, 0x4e425253 /*NBRS*/, 0x54415454 /*TATT*/, 0x46414d49 /*FAMI*/};
const unsigned int kHouseTypes[6] = {0x53494d49 /*SIMI*/, 0x484f5553 /*HOUS*/, 0x41727279 /*Arry*/,
                                     0x6f626a74 /*objt*/, 0x4f626a4d /*ObjM*/, 0x44554d50 /*DUMP*/};
const unsigned int kUserTypes[2] = {0x55736572 /*User*/, 0x74686d62 /*thmb*/};  // s_userIndex

// s_nghLayout (version 8; 80424150): the sections' sizes, by kind, in file order.
struct LayoutEntry {
  int count;
  int size;
};
const LayoutEntry kLayout[8] = {{1, 0x4000}, {1, 0x2000}, {1, 0x2000}, {1, 0xA000},
                                {0x24, 0x800}, {0x10, 0x10000}, {4, 0x400}, {4, 0x400}};
// Where kind `kind` starts in the file.
int LayoutOffset(int kind) {
  int off = 0;
  for (int i = 0; i < kind; ++i) off += kLayout[i].count * kLayout[i].size;
  return off;
}
const char kNghFileExt[] = ".ngh";  // nghfileExt

// A section's header (NghResFileSectionHeader, 0x1C bytes; little-endian in the file).
struct SectionHeader {
  unsigned int kind;      // +0x00 2: a thumbnail
  unsigned int hdrSize;   // +0x04
  unsigned int rawSize;   // +0x08
  unsigned int compSize;  // +0x0C
  unsigned int checksum;  // +0x10 EChecksum::Compute of header and data with this field 0
  unsigned int version;   // +0x14 7 or 8
  unsigned int count;     // +0x18 resources in the section
};
unsigned int LE32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<unsigned int>(p[3]) << 24; }
unsigned int BE32(const unsigned char* p) { return static_cast<unsigned int>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

bool ContainsNoCase(const std::string& s, const char* what) {
  const size_t n = std::strlen(what);
  for (size_t i = 0; i + n <= s.size(); ++i) {
    size_t k = 0;
    while (k < n && std::tolower(static_cast<unsigned char>(s[i + k])) == what[k]) ++k;
    if (k == n) return true;
  }
  return false;
}

// 800D3920
void DeleteList(NghResFile::ResNode* node) {
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
}  // namespace

// ---------------------------------------------------------------------------------------------------
// LZSSCompressor

// 8030A920
// LZSS with a 4 KB window starting at 0xFEE: flag bytes (low bit first) mark literals; references
// are 12 bits of position and 4 of length - 3. Returns the bytes written, -1 when `out` was too small.
RET_UNKNOWN LZSSCompressor::Decode(unsigned char* in, int inSize, unsigned char* out, int outSize) {
  std::vector<unsigned char> window(0x1011, 0);
  const unsigned char* inEnd = in + inSize;
  unsigned char* const outStart = out;
  unsigned char* const outEnd = out + outSize;
  bool full = false;
  auto put = [&](unsigned char c) {
    if (out < outEnd) *out++ = c;
    else full = true;
  };
  unsigned int r = 0xfee, flags = 0;
  for (;;) {
    flags >>= 1;
    if (!(flags & 0x100)) {
      if (in >= inEnd) break;
      flags = *in++ | 0xff00;
    }
    if (flags & 1) {
      if (in >= inEnd) break;
      const unsigned char c = *in++;
      put(c);
      window[r] = c;
      r = (r + 1) & 0xfff;
    } else {
      if (in + 1 >= inEnd) break;
      const unsigned int a = *in++, b = *in++;
      const unsigned int pos = a | (b & 0xf0) << 4;
      for (unsigned int k = 0; k <= (b & 0xf) + 2; ++k) {
        const unsigned char c = window[(pos + k) & 0xfff];
        put(c);
        window[r] = c;
        r = (r + 1) & 0xfff;
      }
    }
  }
  return full ? -1 : out - outStart;
}

// ---------------------------------------------------------------------------------------------------
// iResFile

// 80135588
iResFile::iResFile() {}
// 801355D8
iResFile::~iResFile() {}
// 80135624
int iResFile::GetError() { return m_error; }
// 8013562C
void iResFile::SetError(int error) { m_error = error; }

// 80135634
// A resource got with GetByID is done with. GC: the file's own release first (vtable 0xD8).
void iResFile::Release(Memory__HandleNode* handle) {
  if (GetError() || !handle) return;
  if (handle->m_owned) delete[] handle->m_data;
  delete handle;
}

// ---------------------------------------------------------------------------------------------------
// NghResFile

// 800D39E4
NghResFile::NghResFile() {
  m_cache = new MemoryCardCache;
  init();
}

// 800D3A98
NghResFile::~NghResFile() {
  FlushAllHouseContents();
  for (int i = 0; i < 4; ++i) DeleteList(m_nghLists ? m_nghLists[i] : nullptr);
  delete[] m_nghLists;
  for (ResNode**& l : m_houseLists) {
    if (!l) continue;
    for (int i = 0; i < 6; ++i) DeleteList(l[i]);
    delete[] l;
    l = nullptr;
  }
  for (int i = 0; i < 2; ++i) DeleteList(m_userLists ? m_userLists[i] : nullptr);
  delete[] m_userLists;
  if (m_cache) {
    m_cache->Destroy();
    delete m_cache;
    m_cache = nullptr;
  }
}

// 800D4148
// The empty lists: the neighbourhood's four types, each house's six, the user sections' two.
void NghResFile::init() {
  m_nghLists = new ResNode*[4]();
  for (ResNode**& l : m_houseLists) l = new ResNode*[6]();
  m_userLists = new ResNode*[2]();
}

// 800D89E0
void NghResFile::SetFileName(const char* name) { m_fileName = name ? name : ""; }

// 800D3F78
void NghResFile::SetCurrentHouse(unsigned int house) { m_currentHouse = house - 1; }
// 800D93C0
unsigned int NghResFile::GetCurrentHouse() const { return m_currentHouse + 1; }

// 800D4348
// The list of resources of `type`: the neighbourhood's, the current house's or the user sections'.
NghResFile::ResNode** NghResFile::findListByResType(unsigned int type) {
  for (int i = 0; i < 4; ++i)
    if (kNghTypes[i] == type) return &m_nghLists[i];
  for (int i = 0; i < 6; ++i)
    if (kHouseTypes[i] == type) return &m_houseLists[m_currentHouse & 15][i];
  for (int i = 0; i < 2; ++i)
    if (kUserTypes[i] == type) return &m_userLists[i];
  return nullptr;
}

// 800D3C74
// The resource `id` of `type` in its list (GC: the swizzle function is not applied here).
Memory__HandleNode* NghResFile::GetByID(int type, short id, void (*)(void*, int)) {
  ResNode** list = findListByResType(static_cast<unsigned int>(type));
  if (!list) return nullptr;
  for (ResNode* n = *list; n; n = n->m_next)
    if (n->m_id == static_cast<unsigned short>(id)) return n->m_handle;
  return nullptr;
}

// 800D3C30
int NghResFile::Count(int type) {
  ResNode** list = findListByResType(static_cast<unsigned int>(type));
  short n = 0;
  for (ResNode* r = list ? *list : nullptr; r; r = r->m_next) ++n;
  return n;
}

// 800D3CD0
Memory__HandleNode* NghResFile::GetByIndex(int type, short index, void (*)(void*, int)) {
  ResNode** list = findListByResType(static_cast<unsigned int>(type));
  short i = 1;
  for (ResNode* r = list ? *list : nullptr; r; r = r->m_next, ++i) {
    if (i != index) continue;
    m_byIndex = r;
    return r->m_handle;
  }
  return nullptr;
}

// 800D3D48
void NghResFile::GetID(Memory__HandleNode*, short* id) { *id = m_byIndex ? static_cast<short>(m_byIndex->m_id) : 0; }

// 800D3D70
// The save's resources are little-endian.
int NghResFile::IsLittleEndian(Memory__HandleNode*) { return 1; }

// 800D3BD8
int NghResFile::Close() { return 0; }

// 800D3D7C
// Adds `handle` as resource `id` of `type` (at the list's end), or replaces the one with that id; a
// null handle removes it.
int NghResFile::Add(Memory__HandleNode* handle, int type, short id, StringBuffer&, bool) {
  ResNode** list = findListByResType(static_cast<unsigned int>(type));
  if (!list) return 0;
  ResNode* prev = nullptr;
  for (ResNode* n = *list; n; prev = n, n = n->m_next) {
    if (n->m_id != static_cast<unsigned short>(id)) continue;
    if (handle && handle->m_size) {
      if (n->m_handle) {
        if (n->m_handle->m_owned) delete[] n->m_handle->m_data;
        delete n->m_handle;
      }
      n->m_handle = handle;
      return 0;
    }
    if (handle) {
      if (handle->m_owned) delete[] handle->m_data;
      delete handle;
    }
    (prev ? prev->m_next : *list) = n->m_next;
    n->m_next = nullptr;
    DeleteList(n);
    return 0;
  }
  if (!handle || !handle->m_size) return 0;
  auto* node = new ResNode;
  node->m_handle = handle;
  node->m_id = static_cast<unsigned short>(id);
  if (!*list) {
    *list = node;
  } else {
    ResNode* last = *list;
    while (last->m_next) last = last->m_next;
    last->m_next = node;
  }
  return 0;
}

// 800D8A70
void NghResFile::SwapTwoByteNumber(unsigned short* v) { *v = static_cast<unsigned short>(*v >> 8 | *v << 8); }

// 800D8A84
void NghResFile::SwapFourByteNumber(unsigned int* v) {
  const unsigned int x = *v;
  *v = x >> 24 | (x >> 8 & 0xff00) | (x << 8 & 0xff0000) | x << 24;
}

// 800D6C08
bool NghResFile::AreHouseContentsLoaded(unsigned int house) const {
  ResNode** lists = m_houseLists[(house - 1) & 15];
  return lists && lists[2] != nullptr;
}

// 800D6B40
// The house's world arrays and objects go ('Arry', 'objt', 'ObjM', 'DUMP').
void NghResFile::FlushHouseContents(unsigned int house) {
  ResNode** lists = m_houseLists[(house - 1) & 15];
  for (int i = 2; i < 6; ++i) {
    DeleteList(lists[i]);
    lists[i] = nullptr;
  }
}

// 800D6BC4
void NghResFile::FlushAllHouseContents() {
  for (unsigned int h = 1; h <= 16; ++h) FlushHouseContents(h);
}

// 800D62EC
// The house's section (from the .ngh file or the card cache, or given): checksum checked, decompressed,
// its resources (type, offset, size and id tables after the header) added to the house's lists. 1:
// loaded (or already), 0: not read, -4: the section is bad. The file's header and tables are
// little-endian, the card's (cache) big-endian; the checksum covers the header as stored (its checksum
// field 0) and the compressed data.
unsigned int NghResFile::LoadHouseContents(unsigned int house, void* data, bool) {
  if (AreHouseContentsLoaded(house)) return 1;
  const int sectionSize = kLayout[5].size;
  const int sectionOffset = LayoutOffset(5) + static_cast<int>(house - 1) * sectionSize;
  std::vector<unsigned char> file;
  const unsigned char* src = static_cast<const unsigned char*>(data);
  bool fromCache = false;
  if (!src) {
    file.assign(static_cast<size_t>(sectionSize), 0);
    if (!ContainsNoCase(m_fileName, kNghFileExt)) {
      fromCache = true;
      MemoryCardCache* cache = GetMemoryCardCache();
      if (cache->GetSection(&s_nghLayout, reinterpret_cast<char*>(file.data()), static_cast<unsigned int>(sectionOffset), 0x400) != 0)
        return 0;
      const unsigned int stored = (BE32(file.data() + 4) + BE32(file.data() + 12) + 0x3ff) & ~0x3ffu;
      if (stored > file.size() ||
          cache->GetSection(&s_nghLayout, reinterpret_cast<char*>(file.data()), static_cast<unsigned int>(sectionOffset), stored) != 0)
        return 0;
    } else {
      simmy::DataFile* f = simmy::data_open(m_fileName.c_str());  // the game's .ngh (PC: on the disc)
      if (!f) return 0;
      const bool ok = simmy::data_read_at(f, static_cast<std::uint64_t>(sectionOffset), file.data(), file.size()) == file.size();
      simmy::data_close(f);
      if (!ok) return 0;
    }
    src = file.data();
  }
  auto word = [fromCache](const unsigned char* p) { return fromCache ? BE32(p) : LE32(p); };
  auto half = [fromCache](const unsigned char* p) {
    return static_cast<unsigned short>(fromCache ? p[0] << 8 | p[1] : p[1] << 8 | p[0]);
  };
  const unsigned int previous = m_currentHouse;
  SetCurrentHouse(house);
  SectionHeader h{word(src), word(src + 4), word(src + 8), word(src + 12), word(src + 16), word(src + 20), word(src + 24)};
  if (h.hdrSize + h.compSize > static_cast<unsigned int>(sectionSize)) {
    m_currentHouse = previous;
    return static_cast<unsigned int>(-4);
  }
  std::vector<unsigned char> sum(src, src + h.hdrSize + h.compSize);
  std::memset(sum.data() + 16, 0, 4);
  if (static_cast<unsigned int>(EChecksum::Compute(sum.data(), static_cast<int>(sum.size()))) != h.checksum ||
      h.version < 7 || h.version > 8) {
    m_currentHouse = previous;
    return static_cast<unsigned int>(-4);
  }
  std::vector<unsigned char> raw(h.hdrSize + h.rawSize);
  std::memcpy(raw.data(), src, h.hdrSize);
  const int n = static_cast<int>(LZSSCompressor::Decode(const_cast<unsigned char*>(src + h.hdrSize), static_cast<int>(h.compSize),
                                                         raw.data() + h.hdrSize, static_cast<int>(h.rawSize)));
  if (n != static_cast<int>(h.rawSize)) {
    m_currentHouse = previous;
    return static_cast<unsigned int>(-4);
  }
  const unsigned char* types = raw.data() + h.hdrSize;
  const unsigned char* offsets = types + h.count * 4;
  const unsigned char* sizes = offsets + h.count * 4;
  const unsigned char* ids = sizes + h.count * 4;
  static StringBuffer noName;
  for (unsigned int i = 0; i < h.count; ++i) {
    const unsigned int type = word(types + i * 4), offset = word(offsets + i * 4), size = word(sizes + i * 4);
    const auto id = static_cast<short>(half(ids + i * 2));
    auto* handle = new Memory__HandleNode;
    handle->m_size = size;
    handle->m_data = size ? new unsigned char[size] : nullptr;
    handle->m_owned = 1;
    if (size && offset + size <= raw.size()) std::memcpy(handle->m_data, raw.data() + offset, size);
    Add(handle, static_cast<int>(type), id, noName, true);
  }
  m_currentHouse = previous;  // GC: SetCurrentHouse(previous + 1)
  return 1;
}

// 800D74F0
// The default neighbourhood from the data folder: the file becomes the save's source, its host,
// "both" and thumbnail sections are read now and the houses as they are entered. 0: read, -1: not.
// GC: around it EGlobal::BeginSaveGame/EndSaveGame (a graphics flush).
int NghResFile::ReadFromFile(char* name) {
  simmy::DataFile* f = simmy::data_open(name);  // PC: from the disc image (or the --data folder)
  if (!f) return -1;
  auto read = [f](int kind, std::vector<unsigned char>& buf) {
    buf.assign(static_cast<size_t>(kLayout[kind].count * kLayout[kind].size), 0);
    return simmy::data_read_at(f, static_cast<std::uint64_t>(LayoutOffset(kind)), buf.data(), buf.size()) == buf.size();
  };
  m_fileName = name;  // PC: the game path; the houses are read from it as they are entered
  m_card = 0;
  m_field20 = 1;
  std::vector<unsigned char> buf;
  bool ok = false;
  FlushSection(0);
  if (read(2, buf) && ReadHostSection(buf.data(), false) == 1) {
    FlushSection(1);
    if (read(3, buf) && ReadBothSection(buf.data(), false) == 1) {
      FlushSection(2);
      ok = read(4, buf) && ReadThumbSection(buf.data(), false) == 1;
    }
  }
  simmy::data_close(f);
  if (!ok) {
    m_fileName.clear();
    return -1;
  }
  FlushAllHouseContents();
  return 0;
}

// 800D7C20
// A new game's neighbourhood goes into the memory card cache, which becomes the save's source: every
// section of the default file is cached, then the 16 houses are read and written back through the cache
// (in the save's own format) and dropped from memory. 1: copied. PC: ReadFromFile has read the host,
// "both" and thumbnail sections already; they are written to the cache as the save writes them (GC
// copied the file's sections raw). The source only switches to the card when every house went in.
RET_UNKNOWN NghResFile::CopyDefaultNghToCache(char*) {
  if (m_fileName.empty()) return 0;
  for (unsigned int h = 1; h <= 16; ++h)
    if (LoadHouseContents(h, nullptr, false) != 1) return 0;
  const std::string file = m_fileName;
  m_fileName = s_saveFileName;
  m_card = 0;
  m_field20 = 0;
  int r = WriteHostSection(nullptr) == 1 && WriteBothSection(nullptr) == 1 && WriteThumbSection(nullptr) == 1;
  for (unsigned int h = 1; r && h <= 16; ++h) r = SaveHouseContents(h, nullptr) == 1;
  if (!r) {
    m_fileName = file;  // PC: the houses stay readable from the file
    m_field20 = 1;
    return 0;
  }
  FlushAllHouseContents();
  return 1;
}
