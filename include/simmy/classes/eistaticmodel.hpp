// @class EIGameInstance after DigestGeomShaderPtr EACNodeState ELights EMat4 EVec3
// @class EIStaticModel
// A whole model placed in a level (game/e_istaticmodel.cpp), optionally animated by an
// EAnimController (its node matrices, skinned when the model's display lists index matrices), lit by
// the level's lights at its bound sphere. EIGameInstance (game/e_igameinstance.cpp) adds nothing to
// EInstance but its type.
#pragma once
#include <vector>

struct EIGameInstance : public EInstance {
  EIGameInstance();  // 80227674
  SIMMY_STORABLE_DECL(EIGameInstance)
  ~EIGameInstance() override;  // 802276B0
  static void RegisterType(unsigned short);  // 80227844
  void CreateCopy() const;  // 80227898
};

// 0x320 bytes.
struct EIStaticModel : public EIGameInstance {
  EIStaticModel();  // 8022B9A0
  SIMMY_STORABLE_DECL(EIStaticModel)
  ~EIStaticModel() override;  // 8022BA50
  void Write(EStream &) override;  // 8022BAAC
  void Read(EStream &) override;  // 8022BB10
  void GetBoundSphere(EBoundSphere& out) override;  // 8022BB8C
  void Setup(EInstance* owner, EAnimController* anim);  // 8022BBB4
  void DeallocateModel();  // 8022BD74
  void SetModel(unsigned int id, bool skipInShadowPass, EInstance* owner, EAnimController* anim);  // 8022BE08
  RET_UNKNOWN SetModel(char *);  // 8022BE44
  void SetOrient(EMat4& m) override;  // 8022BE8C
  void SlamOrient(EMat4 &, EMat4 &);  // 8022BFF8
  void GetOrient(EMat4 &) override;  // 8022C104
  void RegisterFloor() override;  // 8022C18C
  void SetupModel(EInstance* owner, EAnimController* anim);  // 8022C3A0
  void BuildDigests();  // 8022C49C
  void SetupBounds();  // 8022C64C
  unsigned int VisibilityTest(E3DWindow& win) override;  // 8022C834
  void Draw(ELevelDrawData& data) override;  // 8022C860
  void DrawAsShadow(ELevelDrawData &);  // 8022CC24
  void DrawShadow(ELevelDrawData &) override;  // 8022CE1C
  static void AnimOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 8022D08C
  void ChangeShader(unsigned int, unsigned int);  // 8022D150
  void ChangeShaderState(unsigned int);  // 8022D22C
  void UpdateShaders();  // 8022D2C8
  void RebuildShaders() override;  // 8022D38C
  void RealizeShaderTuning() override;  // 8022D418
  RET_UNKNOWN FindShaderContainingTexture(unsigned int);  // 8022D47C
  RET_UNKNOWN FindShaderSupportingDecal();  // 8022D538
  void ReplaceBaseTexture(ERTexture* texture, unsigned int secondShader, ERTexture* second);  // 8022D60C
  RET_UNKNOWN Clone();  // 8022D88C
  int HasModifiableColor() override;  // 8022DAAC
  void ModifyColor(unsigned int color) override;  // 8022DAE0
  unsigned int GetModifiableColor() override;  // 8022DAE8
  void DrawImmediate(ERC *, EAnimController *);  // 8022DAF0
  void DrawImmediateGhosted(ERC *, EAnimController *);  // 8022DC8C
  const char* GetInstName(EInstance__InstType& type) override;  // 8022DF1C
  static void RegisterType(unsigned short);  // 8022E0B8
  void CreateCopy() const;  // 8022E10C
  void GetPos(EVec3& out) override;  // 8022E1A8
  RET_UNKNOWN IsSkydome() const;  // 8022E1C8
  virtual void GetAnchorPos(EMat4 &, int);  // 8022E1EC
  unsigned int GetModelId();  // 8022E274
  ERModel* GetModel();  // 8022E27C
  virtual EMat4* GetDrawMatrix();  // 8022E284
  RET_UNKNOWN GetOrient() const;  // 8022E28C

  EAnimController* m_animController = nullptr;  // +0x7C its node matrices draw the model
  EMat4 m_drawMatrix;                 // +0x80 the orientation scaled to the model's units
  EMat4 m_orient;                     // +0xC0 the orientation (axes normalised)
  float m_sphere[4] = {};             // +0x100 InstanceData: bound sphere (lights are taken here)
  unsigned int m_modelId = 0;         // +0x110
  unsigned int m_color = 0;           // +0x114 modifiable colour
  std::vector<DigestGeomShaderPtr> m_digests;  // +0x118 one per shader (+0x11C count)
  ERModel* m_model = nullptr;         // +0x120
  int m_skipInShadowPass = 0;         // +0x124 not drawn while the level draws in mode 2
  ELights m_lights;                   // +0x128 at m_sphere, this side
  ELights m_otherSideLights;          // +0x220 the other side (portals; shader flag 0x20)
  int m_skinned = 0;                  // +0x318 the model's display lists index the node matrices
};
