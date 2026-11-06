// Port of engine/e_rdataset.cpp.
//
// DTST (version 10) data: char name[0x40], u32 count, then per resource: char manager[0x20],
// u32 id, u32 size, u32 skip, the resource's own data (size bytes, starting with its EDataHeader).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80368E40
ERDataset::ERDataset() {}
// 80368E90
ERDataset::~ERDataset() { Deallocate(); }

// 80368F48
void ERDataset::Deallocate() {
  DelRefSubResources();
  std::vector<ResourceInfo>().swap(m_resources);
}

// 80369044
void ERDataset::AddRefSubResources() {
  for (ResourceInfo& r : m_resources)
    if (!r.m_resource && r.m_manager && r.m_id) r.m_resource = r.m_manager->AddRef(r.m_id, nullptr, 0);
}

// 80369104
void ERDataset::DelRefSubResources() {
  for (ResourceInfo& r : m_resources)
    if (r.m_resource) {
      r.m_resource->DelRef();
      r.m_resource = nullptr;
    }
}

// 803691B0
int ERDataset::TryIncrementSubResources() {
  for (ResourceInfo& r : m_resources)
    if (!r.m_resource && r.m_manager && r.m_id && !r.m_manager->TryIncrementResource(r.m_id, &r.m_resource)) {
      DelRefSubResources();
      return 0;
    }
  return 1;
}

static unsigned int ReadU32(EFile* file) {
  unsigned char b[4] = {};
  file->Read(b, 4);
  return simmy::be32(b);
}

// 80369294
void ERDataset::Load(EFile* file, unsigned int size) {
  const int start = file->Tell();
  EDataHeader header;
  if (!header.Read(*file, 0x44545354 /* 'DTST' */, 10, 10)) return;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  _datasetman.SetLoadProgress(0.0f);
  Deallocate();
  char name[0x40];
  file->Read(name, sizeof name);
  const unsigned int count = ReadU32(file);
  m_resources.reserve(count);
  for (unsigned int i = 0; i < count; ++i) {
    char managerName[0x21] = {};
    if (file->Read(managerName, 0x20) != 0x20 || !managerName[0]) break;
    EResourceManager* mgr = _pResLoader->FindResourceManager(managerName);
    if (!mgr) break;
    _datasetman.SetLoadProgress(static_cast<float>(file->Tell() - start) / static_cast<float>(size));
    const unsigned int id = ReadU32(file);
    const unsigned int resSize = ReadU32(file);
    const unsigned int skip = ReadU32(file);
    if (skip) file->Seek(static_cast<int>(skip), EFile__SeekType::Cur);
    if (EResource* res = mgr->AddRef(id, file, static_cast<int>(resSize))) m_resources.push_back({id, mgr, res});
  }
  _datasetman.SetLoadProgress(-1.0f);
}

// 80369834 ERDataset::GetTypeInfo
// 80369840 ERDataset::GetTypeName
// 8036984C ERDataset::GetTypeKey
// 80369858 ERDataset::GetTypeVersion
// 803697F4 ERDataset::SafeDelete
// 80369780 ERDataset::New
// 803697B0 ERDataset::Construct
// 803697D0 ERDataset::Destruct
// 80369864 ERDataset::GetTypeInfoStatic
// 80369870 ERDataset::GetReadVersion
SIMMY_STORABLE_IMPL(ERDataset, "ERDataset", 0, EResource)
