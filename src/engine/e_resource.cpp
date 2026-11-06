// Port of engine/e_resource.cpp: the resource base class (reference counting, size, id).
// Still stubbed: EStream serialisation (Read/Write), type registration and CreateCopy.
#include "simmy/types_gen.hpp"

// 80311DC8
EResource::EResource() = default;

// 80311DF8
EResource::~EResource() {
  if (m_manager) m_manager->ResourceDestructing(this);
}

// 80312158


// 80311F14
// Version 0 object graphs stored the resource name (an EString: characters up to a NUL); it is read
// and dropped.
void EResource::Load(EFile& file) {
  if (m_typeInfo.m_readVersion == 0)
    while (simmy::read_u8(file) != 0) {
    }
}

// 80311F70
void EResource::DelRef() {
  if (!m_manager) {
    if (--m_refs == 0) SafeDelete();
  } else {
    m_manager->DelRef(this, EResourceManager__DelRefMode::Cache);
  }
}

// 80311FD8
void EResource::Detach() { m_manager->Detach(this); }

// 80312000
void EResource::AddRef() {
  if (!m_manager) ++m_refs;
  else m_manager->AddRef(this);
}

// 80312040
void EResource::Refresh() {
  if (m_manager) m_manager->Refresh(this);
}

// 80312254
int EResource::IsManaged() const { return m_manager != nullptr; }
// 8031226C
int EResource::IsSafeToDelete() { return 1; }
// 80312274
unsigned int EResource::GetResId() const { return m_id; }
// 8031227C
void EResource::SetResId(unsigned int id) { m_id = id; }
// 80312284
int EResource::GetNumRefs() { return m_refs; }
// 8031228C
void EResource::SetResSize(unsigned int size) { m_flags = (size << 8) | (m_flags & 0xff); }
// 8031229C
unsigned int EResource::GetResSize() { return m_flags >> 8; }
// 803122A8
void EResource::SetResName(char*) {}
// 803122AC
const char* EResource::GetResName() { return ""; }
// 803122B8
void EResource::SetCheckPoint(unsigned int) {}
// 803122BC
unsigned int EResource::GetCheckPoint() { return 0; }
// 80312338
void EResource::Init() {}
// 8031233C
void EResource::Refresh(EFile*) {}
// 80312340
void EResource::DelRefSubResources() {}
// 80312344
void EResource::AddRefSubResources() {}
// 80312348
int EResource::TryIncrementSubResources() { return 1; }
// 80312350
void EResource::DetachClone() {}

// 80312198 EResource::GetTypeInfo
// 803121A4 EResource::GetTypeName
// 803121B0 EResource::GetTypeKey
// 803121BC EResource::GetTypeVersion
// 803120E0 EResource::New
// 80312108 EResource::Construct
// 80312134 EResource::Destruct
// 803121C8 EResource::GetTypeInfoStatic
// 803121D4 EResource::GetReadVersion
SIMMY_STORABLE_IMPL(EResource, "EResource", 0, EStorable)
