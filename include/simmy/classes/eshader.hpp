// @class EShader after EShaderDef ETexture
// A shader definition the renderer can select (engine/e_shader.cpp).
#pragma once

struct EShader;
// An animated shader's entry in the update list (0x24 bytes): accumulated texture scroll per layer.
struct EShaderUpdate {
  EShader* shader = nullptr;
  float scroll[2][2] = {};     // +0x04 per layer: u, v (kept in -1..1)
  int pad[2] = {};
  EShaderUpdate* prev = nullptr;  // +0x1C toward the head (older entries)
  EShaderUpdate* next = nullptr;  // +0x20 toward the tail
};

struct EShader : public EShaderDef {
  EShader();
  // Virtuals, in vtable order.
  virtual ~EShader();
  virtual void Select(ERC* rc, unsigned int flags);
  virtual void SelectForShadowMask(ERC* rc);
  virtual int Create(EShaderDef& def);
  virtual void SetSurfaceProperty(unsigned int bits);
  virtual void ClearSurfaceProperty(unsigned int bits);
  virtual void SetTexture(ETexture* texture, int layer);
  virtual void SetMultiTexture(ETexture* texture, int index);
  virtual void Validate();
  virtual void SetAlternateShader(EShader* shader);
  virtual void SetAlphaTestThreshold(float threshold, int layer);
  virtual void UpdateMaterialCoefficients();
  virtual void RemoveGeometryModes(unsigned int modes);
  virtual void AddGeometryModes(unsigned int modes);
  virtual void Update(float dt);
  virtual EShader* Clone();

  void AddToUpdateList();
  void RemoveFromUpdateList();
  static void UpdateAll(float);
  void ChangeMaterial(EMaterial&);
  RET_UNKNOWN UseOverrideTexture(unsigned char);
  ETexture* GetTexture(int layer) const;
  RET_UNKNOWN UsesMipMapping();
  void SetTransform(ERC*);
  void SetScrollSpeed(EVec2&, int, bool);
  void SetShadowAlpha(float);
  EMaterial* GetMaterial() const;
  int IsSurface(unsigned int bits) const;
  unsigned int GetSurfaceProperties() const;
  RET_UNKNOWN CanColorBeModified() const;
  RET_UNKNOWN GetModifiableColor(unsigned int) const;
  void UndoOverrideTexture();
  const EShaderDef& GetShaderDef() const;
  RET_UNKNOWN IsFacer() const;
  RET_UNKNOWN IsCylindricalFacer() const;
  RET_UNKNOWN GetShaderUpdate() const;
  RET_UNKNOWN IsAlphaShader() const;
  bool IsValid();

  unsigned int m_magic = 0x900dbeef;  // +0xE4
  EShaderUpdate* m_update = nullptr;  // +0xE8 entry in the animated-shader update list
  unsigned char m_currentMultiTexture = 0;  // +0xEC
  unsigned char m_fieldED = 0;
  unsigned short m_fieldEE = 0;
};
