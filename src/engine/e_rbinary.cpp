// Port of engine/e_rbinary.cpp and engine/e_binaryman.cpp's loader.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80368278
ERBinary::ERBinary() {}
// 803682C0
ERBinary::~ERBinary() { Deallocate(); }

// 8036831C
void ERBinary::Deallocate() {
  delete[] m_data;
  m_data = nullptr;
  m_size = 0;
}

// 80368360
// The 'BINR' header (version 6), then its data size of bytes.
void ERBinary::Load(EFile* file, unsigned int) {
  Deallocate();
  EDataHeader header;
  if (!header.Read(*file, 0x42494e52 /* 'BINR' */, 6, 6)) return;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  m_data = new unsigned char[header.m_dataSize ? header.m_dataSize : 1];
  m_size = header.m_dataSize;
  file->Read(m_data, static_cast<int>(header.m_dataSize));
}

// 8036849C ERBinary::New
// 803684CC ERBinary::Construct
// 803684EC ERBinary::Destruct
// 80368510 ERBinary::SafeDelete
// 80368550 ERBinary::GetTypeInfo
// 8036855C ERBinary::GetTypeName
// 80368568 ERBinary::GetTypeKey
// 80368574 ERBinary::GetTypeVersion
// 80368580 ERBinary::GetTypeInfoStatic
// 8036858C ERBinary::GetReadVersion
SIMMY_STORABLE_IMPL(ERBinary, "ERBinary", 0, EResource)

// 803230C4
EResource* EBinaryManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* binary = new ERBinary;
  binary->m_id = id;
  binary->Load(file, size);
  return binary;
}

// 8036860C ERBinary::GetData (inline in the header)
// 80368614 ERBinary::GetDataSize (inline)
