// Port of engine/e_rquickdata.cpp: quickdata resources ("XImages" of named tables).
//
// The GameCube read the image into memory and patched every offset named by the fixup streams into
// an absolute 32-bit pointer. On PC pointer words become big-endian self-relative offsets, the
// language strings become host-endian UTF-16 and each language slot gets a native pointer cell; see
// simmy/classes/erquickdata.hpp for the layout and the accessors callers use.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace qdimage {
namespace {

void put_be32(unsigned char* p, std::uint32_t v) {
  p[0] = static_cast<unsigned char>(v >> 24);
  p[1] = static_cast<unsigned char>(v >> 16);
  p[2] = static_cast<unsigned char>(v >> 8);
  p[3] = static_cast<unsigned char>(v);
}

bool ReadDirectory(EFile* file, EXPORT_DIRECTORY& dir) {
  unsigned char b[0x18];
  if (file->Read(b, sizeof b) != static_cast<int>(sizeof b)) return false;
  dir.m_size = simmy::be32(b + 0x00);
  dir.m_dataOffset = simmy::be32(b + 0x04);
  dir.m_dataSize = simmy::be32(b + 0x08);
  dir.m_fixupOffset = simmy::be32(b + 0x0C);
  dir.m_fixupSize = simmy::be32(b + 0x10);
  dir.m_baseAddress = simmy::be32(b + 0x14);
  return dir.m_size == sizeof b;
}

bool SeekTo(EFile* file, int pos) { return file->Seek(pos, EFile__SeekType::Set) == pos; }

}  // namespace

// 8031BEB4
// applyFixups(void* region, unsigned delta, EFile*, unsigned fileStart, EXPORT_DIRECTORY&): read the
// directory's fixup stream and add `delta` to every listed word of `region`. The stream is a list of
// big-endian words; the low two bits pick the form:
//   0: one word at offset v            1: (v >> 2) consecutive words, starting at the next word's offset
//   2: two words at offset v & ~3      3: three words at offset v & ~3
// PC: returns the listed word offsets (relative to the region) instead of patching, so ReloadXImage
// can encode them for 64-bit pointers.
bool applyFixups(EFile* file, unsigned int fileStart, const EXPORT_DIRECTORY& dir, std::vector<unsigned int>& words) {
  if (!SeekTo(file, static_cast<int>(fileStart + dir.m_fixupOffset))) return false;
  std::vector<unsigned char> raw(dir.m_fixupSize & ~3u);
  if (!raw.empty() && file->Read(raw.data(), static_cast<unsigned int>(raw.size())) != static_cast<int>(raw.size()))
    raw.resize(0);  // the original simply stopped at the end of what it could read
  const std::size_t count = raw.size() / 4;
  for (std::size_t i = 0; i < count;) {
    const std::uint32_t v = simmy::be32(&raw[4 * i++]);
    switch (v & 3) {
      case 0: words.push_back(v); break;
      case 1: {
        if (i >= count) return true;  // run without its start word: end of stream
        const std::uint32_t start = simmy::be32(&raw[4 * i++]);
        for (std::uint32_t k = 0; k < (v >> 2); ++k) words.push_back(start + 4 * k);
        break;
      }
      default:
        for (std::uint32_t k = 0; k < (v & 3); ++k) words.push_back((v & ~3u) + 4 * k);
        break;
    }
  }
  return true;
}

// 8031C2B4
// ReloadXImage(EFile*, void* image, unsigned& size, int language). Reads the XImage at the file's
// position: the image, then the language blob (language clamped to 0 if out of range; negative = none)
// at align4(image size), into `image` (when it is set and `size` is non-zero; the sizes must match) or a
// new allocation. `size` gets image + the largest language blob, as on the GameCube.
// PC: also builds the slot cells (`cellCount` of them, after `size`) and encodes the pointers.
unsigned char* ReloadXImage(EFile* file, unsigned char* image, unsigned int& size, unsigned int& cellCount, int language,
                            unsigned int tail = 0) {
  const int start = file->Tell();
  EXPORT_DIRECTORY header;
  if (!ReadDirectory(file, header)) return nullptr;
  const unsigned int languageCount = (header.m_dataOffset - 0x18u) / 0x18u;
  if (static_cast<int>(languageCount) <= language) language = languageCount ? 0 : -1;

  EXPORT_DIRECTORY lang{};
  unsigned int maxLangSize = 0;
  unsigned int total = header.m_dataSize;
  if (language >= 0) {
    for (unsigned int i = 0; i < languageCount; ++i) {
      EXPORT_DIRECTORY dir;
      if (!ReadDirectory(file, dir)) return nullptr;
      maxLangSize = std::max(maxLangSize, dir.m_dataSize);
      if (static_cast<int>(i) == language) lang = dir;
    }
    total = ((header.m_dataSize + 3) & ~3u) + maxLangSize;
  }
  const unsigned int langStart = total - maxLangSize;  // align4(image size)

  // PC: the fixup streams are read first, so the slot cells can be sized before allocating.
  std::vector<unsigned int> baseWords, langWords;
  if (!applyFixups(file, static_cast<unsigned int>(start), header, baseWords)) return nullptr;
  if (language >= 0 && !applyFixups(file, static_cast<unsigned int>(start), lang, langWords)) return nullptr;
  std::sort(langWords.begin(), langWords.end());
  langWords.erase(std::unique(langWords.begin(), langWords.end()), langWords.end());
  const unsigned int cellsStart = (total + 7) & ~7u;
  const unsigned int cells = static_cast<unsigned int>(langWords.size());

  if (!SeekTo(file, start + static_cast<int>(header.m_dataOffset))) return nullptr;
  bool allocated = false;
  if (!image || size == 0) {
    // PC mod: mods. `tail` zeroed bytes after the cells (the "SimsObjects" image with a mod folder:
    // ERQuickdata::ModTailSize), where mod rows (copies of rows with their own values) live in reach of
    // the image's self-relative pointers (modobjects.cpp). None without mods.
    image = static_cast<unsigned char*>(_quickdataman.Alloc(cellsStart + cells * sizeof(void*) + tail, 8));
    if (image && tail) std::memset(image + cellsStart + cells * sizeof(void*), 0, tail);
    allocated = true;
  } else if (total != size || cells != cellCount) {
    return nullptr;
  }
  if (!image) return nullptr;
  size = total;
  cellCount = cells;
  auto fail = [&]() -> unsigned char* {
    if (allocated) _quickdataman.Free(image);  // (the original also freed a caller's buffer on Reload)
    return nullptr;
  };
  std::memset(image + header.m_dataSize, 0, cellsStart - header.m_dataSize);
  if (file->Read(image, header.m_dataSize) != static_cast<int>(header.m_dataSize)) return fail();
  if (language >= 0) {
    if (!SeekTo(file, start + static_cast<int>(lang.m_dataOffset))) return fail();
    if (file->Read(image + langStart, lang.m_dataSize) != static_cast<int>(lang.m_dataSize)) return fail();
  }

  // Encode the pointers. A pointer word holds (original target - base) = an image offset; it becomes
  // big-endian (target - word address), where pointers to a language slot target the slot's cell.
  const unsigned int end = language >= 0 ? langStart + lang.m_dataSize : header.m_dataSize;
  const auto** cellArray = reinterpret_cast<const void**>(image + cellsStart);
  int bad = 0;
  auto encode = [&](unsigned int word, unsigned int target) {
    const std::int64_t rel = static_cast<std::int64_t>(target) - word;
    if (target > cellsStart + cells * sizeof(void*) || rel == 0) {
      ++bad;
      put_be32(image + word, 0);
    } else {
      put_be32(image + word, static_cast<std::uint32_t>(static_cast<std::int32_t>(rel)));
    }
  };
  std::vector<unsigned int> strings;
  for (unsigned int i = 0; i < cells; ++i) {
    const unsigned int word = langStart + langWords[i];
    if (langWords[i] + 4 > lang.m_dataSize) {
      ++bad;
      cellArray[i] = nullptr;
      continue;
    }
    const unsigned int target = simmy::be32(image + word) - header.m_baseAddress;
    const bool inBlob = target >= langStart && target < end;
    cellArray[i] = target <= end ? image + target : nullptr;
    if (inBlob) strings.push_back(target);
    encode(word, target <= end ? target : ~0u);
  }
  // The language blob's slots point at UTF-16 strings: make them host-endian.
  std::sort(strings.begin(), strings.end());
  strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
  for (unsigned int s : strings)
    for (unsigned int o = s; o + 2 <= end; o += 2) {
      const std::uint16_t c = simmy::be16(image + o);
      std::memcpy(image + o, &c, 2);
      if (!c) break;
    }
  for (unsigned int word : baseWords) {
    if (word + 4 > header.m_dataSize) {
      ++bad;
      continue;
    }
    const unsigned int target = simmy::be32(image + word) - header.m_baseAddress;
    unsigned int resolved = target <= end ? target : ~0u;
    if (target >= langStart && target < end) {
      const auto it = std::lower_bound(langWords.begin(), langWords.end(), target - langStart);
      if (it != langWords.end() && *it == target - langStart)
        resolved = cellsStart + static_cast<unsigned int>(it - langWords.begin()) * sizeof(void*);
    }
    encode(word, resolved);
  }
  if (bad) std::fprintf(stderr, "[quickdata] %d pointer fixups out of range (set to null)\n", bad);
  return image;
}

// 8031C280
unsigned char* LoadXImage(EFile* file, int language) {
  unsigned int size = 0, cells = 0;
  return ReloadXImage(file, nullptr, size, cells, language);
}

// 8031C1F8
unsigned char* LoadXImage(const char* name, int language) {
  EFile* file = nullptr;
  unsigned char* image = nullptr;
  if (_eorFileSys.Create(file, const_cast<char*>(name), const_cast<char*>("rb"), static_cast<int>(0x80000000u),
                         static_cast<int>(0x80000000u), 0)) {
    image = qdimage::LoadXImage(file, language);
    _eorFileSys.Destroy(file);
  }
  return image;
}

}  // namespace qdimage

// 8031B7F4
ERQuickdata::ERQuickdata() {}

// 8031B83C
ERQuickdata::~ERQuickdata() { reset(); }

// 8031C5D0
void* ERQuickdata::operator new(std::size_t size) { return _quickdataman.Alloc(size, 8); }
// 8031C600
void ERQuickdata::operator delete(void* p) { _quickdataman.Free(p); }

// 8031B89C
void ERQuickdata::reset() {
  if (m_image) UnlockImage();
  _quickdataman.Free(m_image);
  m_imageSize = 0;
  m_cellCount = 0;
  m_image = nullptr;
}

// 8031B8F0
void ERQuickdata::UnlockImage() {}
// 8031B8F4
void ERQuickdata::LockImage() {}

// PC mod: mods (see the header).
unsigned int ERQuickdata::ModTailSize() const {
  return m_id == kModTailImage && simmy::mods_enabled() ? simmy::mods_tail_bytes() : 0;
}
unsigned char* ERQuickdata::ModTail() const {
  return m_image && ModTailSize() ? m_image + ((m_imageSize + 7) & ~7u) + m_cellCount * sizeof(void*) : nullptr;
}

// 8031B8F8
void ERQuickdata::Reload(EFile* file) {
  UnlockImage();
  // (The original dropped the result, so a reload with no image leaked a new one.)
  if (unsigned char* image =
          qdimage::ReloadXImage(file, m_image, m_imageSize, m_cellCount, _quickdataman.GetCurrentLanguage()))
    m_image = image;
  LockImage();
}

// 8031B948
void ERQuickdata::Load(EFile* file, int language) {
  reset();
  m_image = qdimage::ReloadXImage(file, m_image, m_imageSize, m_cellCount, language, ModTailSize());
  if (m_image) m_flags = m_imageSize << 8 | (m_flags & 0xff);
  LockImage();
}

// 8031B9B4
int ERQuickdata::getTableIndex(int lo, int hi, const char* name) const {
  for (;;) {
    if (!name) return -1;
    const int span = hi - lo;
    if (span < 0) return -1;
    if (span == 0) return std::strcmp(name, TableName(GetTableByIndex(lo))) == 0 ? lo : -1;
    const int mid = lo + span / 2;
    const int c = std::strcmp(name, TableName(GetTableByIndex(mid)));
    if (c == 0) return mid;
    if (c < 0) hi = mid - 1;
    else lo = mid + 1;
  }
}

// 8031BA74
const void* ERQuickdata::getTable(const char* name) const {
  if (!name || !m_image) return nullptr;  // (PC: also guards a failed load)
  const int index = getTableIndex(0, GetTableCount() - 1, name);
  return index >= 0 ? GetTableByIndex(index) : nullptr;
}

// 8031BADC
int ERQuickdata::getRowIndex(int lo, int hi, const char* name, const void* rowNames) {
  for (;;) {
    if (!name || !rowNames) return -1;
    const int span = hi - lo;
    if (span < 0) return -1;
    if (span == 0) break;
    const int mid = lo + span / 2;
    const int c = std::strcmp(name, String(rowNames, static_cast<unsigned int>(mid) * 4));
    if (c == 0) return mid;
    if (c < 0) hi = mid - 1;
    else lo = mid + 1;
  }
  return std::strcmp(name, String(rowNames, static_cast<unsigned int>(lo) * 4)) == 0 ? lo : -1;
}

// 8031BB94
const void* ERQuickdata::getRow(const void* table, const char* name) const {
  if (!table || !name || !TableRowNames(table)) return nullptr;
  const unsigned int stride = TableRowSize(table) ? TableRowSize(table) : 1;
  const int index = getRowIndex(0, static_cast<int>(TableRowCount(table)) - 1, name, TableRowNames(table));
  return index >= 0 ? TableRows(table) + static_cast<unsigned int>(index) * stride : nullptr;
}

// 8031BC10
// Where table `i`'s rows start; a table without rows starts where the previous one with rows ends.
const unsigned char* ERQuickdata::getStartAddr(int i) const {
  if (const unsigned char* rows = TableRows(GetTableByIndex(i))) return rows;
  for (int j = i - 1; j >= 0; --j) {
    const void* t = GetTableByIndex(j);
    if (const unsigned char* rows = TableRows(t))
      return rows + (TableRowSize(t) ? TableRowSize(t) : 1) * TableRowCount(t);
  }
  return nullptr;
}

// 8031BC88
int ERQuickdata::findTableIndex(int lo, int hi, const void* addr) const {
  const auto a = reinterpret_cast<std::uintptr_t>(addr);
  for (;;) {
    const int span = hi - lo;
    if (span < 0) return -1;
    if (span == 0) break;
    const int mid = lo + span / 2;
    const auto start = reinterpret_cast<std::uintptr_t>(getStartAddr(mid));
    if (a < start) {
      hi = mid - 1;
    } else {
      const void* t = GetTableByIndex(mid);
      if (a < start + std::uintptr_t{TableRowSize(t) ? TableRowSize(t) : 1} * TableRowCount(t)) return mid;
      lo = mid + 1;
    }
  }
  const void* t = GetTableByIndex(lo);
  const auto start = reinterpret_cast<std::uintptr_t>(TableRows(t));
  if (!start || a < start) return -1;
  if (start + std::uintptr_t{TableRowSize(t) ? TableRowSize(t) : 1} * TableRowCount(t) <= a) return -1;
  return lo;
}

// 8031BD8C
// The table holding row address `addr`, and the row's index (null if `addr` is not a row start).
const void* ERQuickdata::findRow(const void* addr, unsigned int* rowIndex) const {
  if (!m_image) return nullptr;
  const int index = findTableIndex(0, GetTableCount() - 1, addr);
  if (index < 0) return nullptr;
  const void* t = GetTableByIndex(index);
  const std::uintptr_t stride = TableRowSize(t) ? TableRowSize(t) : 1;
  const std::uintptr_t delta = reinterpret_cast<std::uintptr_t>(addr) - reinterpret_cast<std::uintptr_t>(TableRows(t));
  if (delta % stride) return nullptr;
  *rowIndex = static_cast<unsigned int>(delta / stride);
  return t;
}

// 8031BE28
bool ERQuickdata::FindTableName(const void* addr, const char** name) const {
  const int index = m_image ? findTableIndex(0, GetTableCount() - 1, addr) : -1;
  *name = index >= 0 ? TableName(GetTableByIndex(index)) : nullptr;
  return index >= 0;
}

// 8031BE9C
unsigned int ERQuickdata::GetRevisionNumber() const { return m_image ? U32(m_image, kImageRevision) : 0; }

// 8031C62C
const void* ERQuickdata::GetImage() const { return m_image; }
// 8031C634
unsigned int ERQuickdata::GetImageSize() const { return m_imageSize; }
