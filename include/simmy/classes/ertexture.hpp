// @class ERTexture after EResource ETexture
// A texture resource (engine/e_rtexture.cpp): the TXFL data loaded into an ETexture.
#pragma once

struct ERTexture : public EResource {
  ERTexture();
  SIMMY_STORABLE_DECL(ERTexture)
  ~ERTexture() override;
  void Load(EFile& file) override;
  int IsSafeToDelete() override;
  void Refresh(EFile* file) override;
  using EResource::Refresh;
  void Attach(ETexture* texture);
  void Deallocate();
  void Load(unsigned char* data);
  void LoadFromMemory(unsigned char* data);
  void Select(ERC* rc, int stage);
  static void RegisterType(unsigned short);
  void CreateCopy() const;

  ETexture* m_texture = nullptr;  // +0x14
};
