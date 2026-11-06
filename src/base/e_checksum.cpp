// Port of base/e_checksum.cpp. ComputeSymbol is how every resource is identified: a CRC-32 of the
// name upper-cased, with anything but letters and digits folded to '_'. Verified against
// DATA/INDEX.IND: names like "systemfont", "MainMenu" and "GLOBAL_SHADOW_CIRCLE" hash to listed ids.
#include "simmy/types_gen.hpp"

#include <array>
#include <cstring>

namespace {
// constexpr so it is ready before any static initializer (type registration hashes class names).
constexpr std::array<unsigned int, 256> kCrcTable = [] {
  std::array<unsigned int, 256> t{};
  for (unsigned int i = 0; i < 256; ++i) {
    unsigned int c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    t[i] = c;
  }
  return t;
}();
}  // namespace

unsigned int EChecksum_m_table(int i) { return kCrcTable[i & 0xff]; }

// 802C5E44
RET_UNKNOWN EChecksum::Compute(void* data, int size) {
  unsigned int crc = 0xffffffffu;
  const auto* p = static_cast<const unsigned char*>(data);
  for (int i = 0; i < size; ++i) crc = (crc >> 8) ^ kCrcTable[(crc ^ p[i]) & 0xff];
  return static_cast<RET_UNKNOWN>(~crc);
}

// 802C5E94
RET_UNKNOWN EChecksum::Compute(char* s) { return Compute(s, static_cast<int>(std::strlen(s))); }

// 802C5EE0
RET_UNKNOWN EChecksum::ComputeSymbol(char* name, unsigned int seed) {
  const auto* p = reinterpret_cast<const unsigned char*>(name);
  unsigned int crc;
  if (seed == 0) {
    crc = 0xffffffffu;
    // Names starting with a digit start from a different value (table entry 0xA0 ^ 0xFFFFFF).
    if (p[0] >= '0' && p[0] <= '9') crc = kCrcTable[0xa0] ^ 0xffffffu;
  } else {
    crc = ~seed;
  }
  for (; *p; ++p) {
    unsigned int c = *p;
    if (c >= 'a' && c <= 'z') c -= 0x20;
    else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) c = '_';
    crc = (crc >> 8) ^ kCrcTable[(crc & 0xff) ^ c];
  }
  return static_cast<RET_UNKNOWN>(static_cast<unsigned int>(~crc));
}

// 802C5F98
RET_UNKNOWN EChecksum::FNVHashString(char* s, unsigned int seed) {
  unsigned int h = seed ? seed : 2166136261u;
  for (; *s; ++s) h = (h * 16777619u) ^ static_cast<unsigned char>(*s);
  return static_cast<RET_UNKNOWN>(h);
}
