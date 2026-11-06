// @class EDataHeader
// The header in front of every resource in the archives (engine/e_dataheader.cpp), big-endian:
//   u32 version, u32 signature ('SHDR', 'TXFL', 'MODL'...), u32 variant, u32 nameLength,
//   char name[nameLength], u32 dataSize, then dataSize bytes of resource data.
#pragma once

struct EDataHeader {
  EDataHeader();
  ~EDataHeader();
  void Clear();
  void Write(EStream&);
  int Read(EFile& file, unsigned int signature, unsigned int minVersion, unsigned int maxVersion);
  bool VerifySignature(unsigned int signature);
  void SetName(char* name);
  char* GetName();
  int IsSignatureValid();

  char* m_name = nullptr;          // +0x00
  unsigned int m_version = 0;      // +0x04
  unsigned int m_signature = 0;    // +0x08
  int m_variant = -1;              // +0x0C (shaders: 1 = multi-state shader)
  unsigned int m_dataSize = 0;     // +0x10
};
