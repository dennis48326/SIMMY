// @class ERBinary after EResource
// A binary resource (engine/e_rbinary.cpp, 'BINR' version 6): the data after its header, as stored
// (big-endian; its users read it).
#pragma once

struct ERBinary : public EResource {
  ERBinary();  // 80368278
  SIMMY_STORABLE_DECL(ERBinary)
  ~ERBinary() override;  // 803682C0
  void Deallocate();  // 8036831C
  void Load(EFile* file, unsigned int size);  // 80368360
  using EResource::Load;
  static void RegisterType(unsigned short);  // 80368598
  void CreateCopy() const;  // 803685EC
  void* GetData() { return m_data; }  // 8036860C
  unsigned int GetDataSize() { return m_size; }  // 80368614

  unsigned char* m_data = nullptr;  // +0x14 (GC: from the binary manager's heap)
  unsigned int m_size = 0;          // +0x18
};
