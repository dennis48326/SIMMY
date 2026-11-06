// Port of game/optionsrecon.cpp and its ReconSaveObject/ReconLoadObject<OptionsRecon> instances
// (linkonce/optionsrecon_*): the options, streamed through a ReconBuffer into the save's config block.
//
// The config block (0x2000 bytes, at 0x2000 in "Sims 2 Save"): +0 the CRC-32 of the rest, +4 the build
// version (0x46090C00), both big-endian; +8 the streamed object: a 12-byte ReconBuilder header (flags,
// version 9, type 0, little-endian) then the fields, little-endian (the GameCube's ReconBuffers swap),
// with the two strings as a 0xFFFE mark and big-endian UTF-16.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstddef>
#include <cstring>
#include <vector>

static_assert(offsetof(OptionsRecon, m_vol1C) == 0x1C && offsetof(OptionsRecon, m_opt24) == 0x24 &&
                  offsetof(OptionsRecon, m_saved00) == 0x2C && offsetof(OptionsRecon, m_saved1C) == 0x48 &&
                  offsetof(OptionsRecon, m_savedOnline10) == 0x50 && offsetof(OptionsRecon, m_str58Hdr) == 0x58 &&
                  offsetof(OptionsRecon, m_strA0Hdr) == 0xA0 && offsetof(OptionsRecon, m_optE8) == 0xE8 &&
                  sizeof(OptionsRecon) == 0xF4,
              "OptionsRecon keeps the GameCube layout (readers index its bytes)");

int s_bPrefsPreserved = 0;           // OptionsRecon::s_bPrefsPreserved
int s_bPrefsPreservedForOnline = 0;  // OptionsRecon::s_bPrefsPreservedForOnline

namespace {
constexpr int kOptionsVersion = 9;
// getCurrentbuildVerNum(0x46, 9, 0xc, 0) (800D3860).
constexpr unsigned int kBuildVersion = 0x46u << 24 | 9u << 16 | 0xcu << 8;

void Recon8(ReconBuffer& b, signed char* v) { b.Recon8(v, 1); }
// The byte read back clamped to 0..10 (a volume).
void ReconVolume(ReconBuffer& b, signed char* v) {
  b.Recon8(v, 1);
  if (b.m_mode == ReconBuffer__Mode::kRead) {
    if (*v < 0) *v = 0;
    if (*v > 10) *v = 10;
  }
}

void PutBE32(unsigned char* p, unsigned int v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (24 - 8 * i));
}
unsigned int BE32(const unsigned char* p) { return static_cast<unsigned int>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

// The options as a ReconObject (type 0).
struct OptionsObject : ReconObject {
  OptionsRecon* m_options;
  explicit OptionsObject(OptionsRecon* o) : m_options(o) {}
  void DoStream(ReconBuffer* b, int version) override { m_options->DoStream(b, version); }
  int GetType() override { return 0; }
};

// 803A02CC ReconSaveObject<OptionsRecon> (its ReconBuilder::Compact): a 12-byte header (flags 0, the
// version, the type; little-endian) and the stream.
std::vector<unsigned char> SaveOptions(OptionsRecon& o, int version) {
  OptionsObject object(&o);
  ReconBuilder builder;
  Memory__HandleNode* h = builder.Compact(&object, version);
  std::vector<unsigned char> out(h->m_data, h->m_data + h->m_size);
  delete[] h->m_data;
  delete h;
  return out;
}
// 803A0328 ReconLoadObject<OptionsRecon> (its ReconBuilder::Reconstitute).
void LoadOptions(OptionsRecon& o, unsigned char* data, int size) {
  OptionsObject object(&o);
  Memory__HandleNode h;
  h.m_data = data;
  h.m_size = static_cast<unsigned int>(size);
  h.m_owned = 0;
  ReconBuilder().Reconstitute(&object, &h, nullptr);
}
}  // namespace

// 8005887C
OptionsRecon::OptionsRecon() { ResetToDefaults(); }
// 800588D0
OptionsRecon::OptionsRecon(OptionsRecon& other) { *this = other; }

// 803A0380
OptionsRecon& OptionsRecon::operator=(OptionsRecon& o) {
  m_opt00 = o.m_opt00, m_opt04 = o.m_opt04, m_opt08 = o.m_opt08, m_opt0C = o.m_opt0C, m_opt10 = o.m_opt10;
  m_opt14 = o.m_opt14, m_opt18 = o.m_opt18;
  m_vol1C = o.m_vol1C, m_vol1D = o.m_vol1D, m_vol1E = o.m_vol1E, m_opt1F = o.m_opt1F, m_opt20 = o.m_opt20;
  m_opt24 = o.m_opt24, m_vol28 = o.m_vol28, m_vol29 = o.m_vol29;
  m_saved00 = o.m_saved00, m_saved04 = o.m_saved04, m_saved08 = o.m_saved08, m_saved10 = o.m_saved10;
  m_saved14 = o.m_saved14, m_saved18 = o.m_saved18, m_saved24 = o.m_saved24;
  m_saved1C = o.m_saved1C, m_saved1D = o.m_saved1D, m_saved1E = o.m_saved1E, m_saved1F = o.m_saved1F;
  m_saved20 = o.m_saved20, m_saved28 = o.m_saved28, m_saved29 = o.m_saved29;
  m_savedOnline10 = o.m_savedOnline10, m_opt54 = o.m_opt54;
  std::memcpy(m_str58, o.m_str58, sizeof m_str58);
  std::memcpy(m_strA0, o.m_strA0, sizeof m_strA0);
  m_optE8 = o.m_optE8, m_optEC = o.m_optEC, m_optF0 = o.m_optF0;
  return *this;
}

// 8005892C
void OptionsRecon::ResetToDefaults() {
  m_opt24 = 0;
  m_vol1E = 10;
  m_opt54 = -1;
  m_opt14 = 1;
  m_opt00 = m_opt04 = m_opt08 = 1;
  m_opt0C = m_opt10 = 0;
  m_vol1C = m_vol1D = m_vol28 = m_vol29 = 10;
  m_opt1F = m_opt20 = 0;
  m_optE8 = m_optEC = m_optF0 = 0;
  m_opt18 = 0;
}

// 8005898C
// The volumes read back are clamped to 0..10; the byte at +0x54 is written but not read back.
void OptionsRecon::DoStream(ReconBuffer* b, int) {
  b->ReconBool(&m_opt00);
  b->ReconBool(&m_opt04);
  b->ReconBool(&m_opt08);
  b->ReconBool(&m_opt0C);
  b->ReconBool(&m_opt10);
  ReconVolume(*b, &m_vol1C);
  ReconVolume(*b, &m_vol1D);
  ReconVolume(*b, &m_vol1E);
  Recon8(*b, &m_opt1F);
  Recon8(*b, &m_opt20);
  if (b->m_mode == ReconBuffer__Mode::kRead) {
    signed char ignored = 0;
    Recon8(*b, &ignored);
  } else {
    Recon8(*b, &m_opt54);
  }
  b->ReconString(m_str58, 0x20);
  b->ReconString(m_strA0, 0x20);
  b->Recon32(&m_optE8, 1);
  b->ReconBool(&m_optEC);
  b->ReconBool(&m_optF0);
  b->ReconBool(&m_opt14);
  b->ReconBool(&m_opt18);
  ReconVolume(*b, &m_vol28);
  ReconVolume(*b, &m_vol29);
  b->ReconBool(&m_opt24);
}

// 80058BFC
// Into the card cache's config block. GC: then refreshed the controllers (_pCtrlMan).
int OptionsRecon::WriteOut() {
  std::vector<unsigned char> block(0x2000, 0);
  const std::vector<unsigned char> stream = SaveOptions(*this, kOptionsVersion);
  std::memcpy(block.data() + 8, stream.data(), stream.size() < 0x1ff8 ? stream.size() : 0x1ff8);
  PutBE32(block.data(), static_cast<unsigned int>(EChecksum::Compute(block.data() + 8, 0x1ff8)));
  PutBE32(block.data() + 4, kBuildVersion);
  return PutConfigInMemoryCardCache(reinterpret_cast<char*>(block.data()), 0x2000) ? 1 : 0;
}

// 80058D24
RET_UNKNOWN OptionsRecon::WriteToMemoryCard(char*, unsigned char) { return 0; }

// 80058D2C
// From the card cache (loading the config block from the card if needed): 1 read, 0 not read, -4 the
// block's checksum or build version is wrong (the version is not checked in a debug mode,
// DAT_80476104, not set on PC).
int OptionsRecon::ReadIn(int card) {
  std::vector<unsigned char> block(0x2000, 0);
  int r = GetConfigFromMemoryCardCache(reinterpret_cast<char*>(block.data()), 0x2000, card) ? 1 : 0;
  if (r == 1) {
    if (BE32(block.data()) == static_cast<unsigned int>(EChecksum::Compute(block.data() + 8, 0x1ff8)) &&
        BE32(block.data() + 4) == kBuildVersion)
      LoadOptions(*this, block.data() + 8, 0x2000);
    else
      r = -4;
  }
  return r;
}

// 80058E54
RET_UNKNOWN OptionsRecon::ReadFromMemoryCard(char*, unsigned char) { return 0; }

// 80058E5C
void OptionsRecon::PreservePreferences() {
  m_saved00 = m_opt00;
  m_saved29 = m_vol29;
  m_saved04 = m_opt04;
  m_saved08 = m_opt08;
  m_saved10 = m_opt10;
  m_saved1C = m_vol1C;
  m_saved1D = m_vol1D;
  m_saved1E = m_vol1E;
  m_saved1F = m_opt1F;
  m_saved20 = m_opt20;
  m_saved14 = m_opt14;
  m_saved18 = m_opt18;
  m_saved24 = m_opt24;
  m_saved28 = m_vol28;
  s_bPrefsPreserved = 1;
}

// 80058EE8
void OptionsRecon::PreservePrefsForOnline() {
  m_savedOnline10 = m_opt10;
  s_bPrefsPreservedForOnline = 1;
}

// 80058EFC
void OptionsRecon::RestorePreferences() {
  m_opt00 = m_saved00;
  m_vol29 = m_saved29;
  m_opt04 = m_saved04;
  m_opt08 = m_saved08;
  m_opt10 = m_saved10;
  m_vol1C = m_saved1C;
  m_vol1D = m_saved1D;
  m_vol1E = m_saved1E;
  m_opt1F = m_saved1F;
  m_opt20 = m_saved20;
  m_opt14 = m_saved14;
  m_opt18 = m_saved18;
  m_opt24 = m_saved24;
  m_vol28 = m_saved28;
}

// 80058F80
void OptionsRecon::RestorePrefsForOnline() { m_opt10 = m_savedOnline10; }
