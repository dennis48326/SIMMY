// Port of engine/locatbig.cpp: lookups in a BIG archive directory. Three header kinds exist:
//   1  0xC0FB: u16 header size - 4, u16 count, entries of 3-byte offset + 3-byte size + name
//   2  "BIGF": u32 total size (little-endian, unused here), u32 count, u32 header size, entries of
//      u32 offset + u32 size + name
//   3  "BIG" + digit: like BIGF with that many bytes per offset/size
// Optional debug info (a letter, three digits and a 4-byte stamp) ends the header.
#include "simmy/types_gen.hpp"

#include <cstring>

namespace {
unsigned char* Bytes(void* p) { return static_cast<unsigned char*>(p); }
// BIG_stricmp's case table: upper-case letters fold to lower case.
int Fold(char c) { return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }
}  // namespace

// 80365D90
unsigned int BIG_getm(void* p, int bytes) {
  const unsigned char* b = Bytes(p);
  switch (bytes) {
    case 1: return b[0];
    case 2: return static_cast<unsigned int>(b[0]) << 8 | b[1];
    case 3: return static_cast<unsigned int>(b[0]) << 16 | static_cast<unsigned int>(b[1]) << 8 | b[2];
    case 4:
      return static_cast<unsigned int>(b[0]) << 24 | static_cast<unsigned int>(b[1]) << 16 |
             static_cast<unsigned int>(b[2]) << 8 | b[3];
    default: return 0;
  }
}

// 80365E20
int BIG_stricmp(char* a, char* b) {
  for (;; ++a, ++b) {
    const int ca = Fold(*a), cb = Fold(*b);
    if (ca != cb) return ca - cb;
    if (!*a) return 0;
  }
}

// 80365EBC
int BIG_typeofheader(void* header) {
  const unsigned char* b = Bytes(header);
  if (b[0] == 0xC0 && b[1] == 0xFB) return 1;
  const unsigned int tag3 = static_cast<unsigned int>(b[0]) << 24 | static_cast<unsigned int>(b[1]) << 16 |
                            static_cast<unsigned int>(b[2]) << 8;
  if ((tag3 | b[3]) == 0x42494746) return 2;  // "BIGF"
  if (tag3 == 0x42494700) return 3;           // "BIG" + bytes per field
  return 0;
}

// 80365F40
unsigned int BIG_sizeofheader(void* header) {
  const int type = BIG_typeofheader(header);
  if (type == 1) return BIG_getm(Bytes(header) + 2, 2) + 4;
  if (type > 0 && type < 4) return BIG_getm(Bytes(header) + 0xC, 4);
  return 0;
}

// 80365FCC
// The debug info is the last 8 header bytes: a letter, a three-digit version and a 4-byte stamp.
int BIG_debuginfo(void* header, int* version, unsigned int* stamp) {
  const unsigned char* end = Bytes(header) + BIG_sizeofheader(header);
  const unsigned char c = end[-8];
  auto digit = [](unsigned char d) { return static_cast<unsigned char>(d - '0') < 10; };
  if (!((static_cast<unsigned char>(c - 'A') < 26 || static_cast<unsigned char>(c - 'a') < 26) && digit(end[-7]) &&
        digit(end[-6]) && digit(end[-5])))
    return 0;
  if (version) *version = (end[-7] - '0') * 100 + (end[-6] - '0') * 10 + (end[-5] - '0');
  if (stamp) *stamp = BIG_getm(const_cast<unsigned char*>(end - 4), 4);
  return 8;
}

// 803660CC
int BIG_version(void* header) {
  int version = 0;
  BIG_debuginfo(header, &version, nullptr);
  return version;
}

// 80366100
// Finds an entry by name (case-insensitive) or, when `name` is null, by `*index`. Returns its name in
// the directory and fills in the index, offset and size; null (offset and size 0) when not found.
char* BIG_locateentryz(void* header, char* name, int* index, int* offset, int* size) {
  const int debug = BIG_debuginfo(header, nullptr, nullptr);
  unsigned char* p = Bytes(header) + 0x10;
  int offBytes = 4, sizeBytes = 4;
  unsigned char* end = Bytes(header) + BIG_sizeofheader(header) - debug;
  const int type = BIG_typeofheader(header);
  if (type == 1) {
    p = Bytes(header) + 6;
    offBytes = sizeBytes = 3;
  } else if (type == 3) {
    offBytes = sizeBytes = static_cast<char>(Bytes(header)[3]) - '0';
    if (sizeBytes > 4) sizeBytes = 4;
  }
  // Offsets wider than 4 bytes keep their low 4 bytes.
  auto readOffset = [&](unsigned char* e) {
    return static_cast<int>(offBytes < 5 ? BIG_getm(e, offBytes) : BIG_getm(e + offBytes - 4, 4));
  };
  for (int i = 0; p < end; ++i) {
    char* entryName = reinterpret_cast<char*>(p + offBytes + sizeBytes);
    const bool hit = name ? BIG_stricmp(entryName, name) == 0 : i == *index;
    if (hit) {
      if (offset) *offset = readOffset(p);
      if (size) *size = static_cast<int>(BIG_getm(p + offBytes, sizeBytes));
      if (name && index) *index = i;
      return entryName;
    }
    p = reinterpret_cast<unsigned char*>(entryName) + std::strlen(entryName) + 1;
  }
  if (offset) *offset = 0;
  if (size) *size = 0;
  return nullptr;
}

// 803665C4
char* BIG_locateentry(void* header, char* name, int* index, int* offset, int* size) {
  return BIG_locateentryz(header, name, index, offset, size);
}

// 803665E4
int BIG_locateoffset(void* header, char* name) {
  int offset = 0;
  if (name) BIG_locateentry(header, name, nullptr, &offset, nullptr);
  return offset;
}

// 80366624
int BIG_locateoffsetz(void* header, char* name) {
  int offset = 0;
  if (name) BIG_locateentryz(header, name, nullptr, &offset, nullptr);
  return offset;
}

// 80366664
void* BIG_pointer(void* header, int index) {
  int offset = 0;
  if (!BIG_locateentry(header, nullptr, &index, &offset, nullptr)) return nullptr;
  return Bytes(header) + offset;
}

// 803666B8
void* BIG_pointerz(void* header, int index) {
  int offset = 0;
  if (!BIG_locateentryz(header, nullptr, &index, &offset, nullptr)) return nullptr;
  return Bytes(header) + offset;
}

// 8036670C
void* BIG_locate(void* header, char* name) {
  const int offset = BIG_locateoffset(header, name);
  return offset ? Bytes(header) + offset : nullptr;
}

// 8036674C
void* BIG_locatez(void* header, char* name) {
  const int offset = BIG_locateoffsetz(header, name);
  return offset ? Bytes(header) + offset : nullptr;
}

// 8036678C
void* BIG_locatesize(void* header, char* name, int* size) {
  int offset = 0;
  BIG_locateentryz(header, name, nullptr, &offset, size);
  return offset ? Bytes(header) + offset : nullptr;
}

// 803667E0
// `names` is a comma-separated list; the commas are restored afterwards.
void BIG_locatemany(void* header, char* names, void** out) {
  for (;;) {
    char* comma = std::strchr(names, ',');
    if (comma) *comma = '\0';
    *out++ = BIG_locate(header, names);
    if (!comma) return;
    *comma = ',';
    names = comma + 1;
  }
}

// 80366870
int BIG_count(void* header) {
  const int type = BIG_typeofheader(header);
  if (type == 1) return static_cast<int>(BIG_getm(Bytes(header) + 4, 2));
  if (type > 0 && type < 4) return static_cast<int>(BIG_getm(Bytes(header) + 8, 4));
  return 0;
}
