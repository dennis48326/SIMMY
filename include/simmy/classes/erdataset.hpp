// @class ERDataset after EResource
// A dataset resource (engine/e_rdataset.cpp): holds references to the resources it bundled.
#pragma once
#include <vector>

struct ERDataset__ResourceInfo {
  unsigned int m_id = 0;
  EResourceManager* m_manager = nullptr;
  EResource* m_resource = nullptr;
};

struct ERDataset : public EResource {
  using ResourceInfo = ::ERDataset__ResourceInfo;
  ERDataset();
  SIMMY_STORABLE_DECL(ERDataset)
  ~ERDataset() override;
  void AddRefSubResources() override;
  void DelRefSubResources() override;
  int TryIncrementSubResources() override;
  void Deallocate();
  void Load(EFile* file, unsigned int size);
  using EResource::Load;
  static void RegisterType(unsigned short);
  void CreateCopy() const;

  std::vector<ResourceInfo> m_resources;  // +0x14
};
