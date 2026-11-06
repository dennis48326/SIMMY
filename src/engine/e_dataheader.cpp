// Port of engine/e_dataheader.cpp.
#include "simmy/types_gen.hpp"

#include <cstdio>
#include <cstring>

// 8035AE70
EDataHeader::EDataHeader() { Clear(); }
// 8035AEA8
EDataHeader::~EDataHeader() { Clear(); }

// 8035AEE8
void EDataHeader::Clear() {
  m_variant = -1;
  m_version = 0;
  m_signature = 0;
  m_dataSize = 0;
  delete[] m_name;
  m_name = nullptr;
}

static unsigned int ReadU32(EFile& file) {
  unsigned char b[4] = {};
  file.Read(b, 4);
  return simmy::be32(b);
}

// 8035B09C
// Reads the header and leaves the file at the resource data. The disc version retried a failed read
// once (two attempts) from the same position.
int EDataHeader::Read(EFile& file, unsigned int signature, unsigned int minVersion, unsigned int maxVersion) {
  const int start = file.Tell();
  for (int attempt = 1; attempt <= 2; ++attempt) {
    Clear();
    file.Seek(start, EFile::SeekType::Set);
    m_version = ReadU32(file);
    m_signature = ReadU32(file);
    m_variant = static_cast<int>(ReadU32(file));
    const unsigned int nameLength = ReadU32(file);
    if (m_signature != signature) {
      std::fprintf(stderr, "TRY%d-Invalid signature! Expected %08X but got %08X\n", attempt, signature, m_signature);
      continue;
    }
    if (m_version > maxVersion) {
      std::fprintf(stderr, "TRY%d-Resource version is newer than this executable!\n", attempt);
      continue;
    }
    if (m_version < minVersion) continue;
    if (nameLength) {
      m_name = new char[nameLength + 1];
      file.Read(m_name, nameLength);
      m_name[nameLength] = 0;
    }
    m_dataSize = ReadU32(file);
    return 1;
  }
  Clear();
  file.Seek(start, EFile::SeekType::Set);
  return 0;
}

// 8035B2B4
bool EDataHeader::VerifySignature(unsigned int signature) { return signature == m_signature; }

// 8035B2C8
void EDataHeader::SetName(char* name) {
  m_name = new char[std::strlen(name) + 1];
  std::strcpy(m_name, name);
}

// 8035B320
char* EDataHeader::GetName() { return m_name; }

// 8035B360
int EDataHeader::IsSignatureValid() {
  switch (m_signature) {  // ANIM BINR CHRC DTST EDTH FLSH FONT LEVL MODL PART SHDR TXFL
  case 0x414E494D: case 0x42494E52: case 0x43485243: case 0x44545354: case 0x45445448: case 0x464C5348:
  case 0x464F4E54: case 0x4C45564C: case 0x4D4F444C: case 0x50415254: case 0x53484452: case 0x5458464C:
    return 1;
  default:
    return 0;
  }
}
