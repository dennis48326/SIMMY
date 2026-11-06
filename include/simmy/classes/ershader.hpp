// @class ERShader after EResource EShader ERTexture
// A shader resource (engine/e_rshader.cpp): an EShader plus the texture resources it references.
// Multi-state shaders (flag 2: weather, time-of-day or UI variants): the current state's shader stands
// in (m_stateShader); UpdateAll switches them (weather / time of day) and blends the colours over.
#pragma once

struct ERShader;
// A node of the multi-state update list (0xC bytes).
struct ERShaderUpdateNode {
  ERShader* shader = nullptr;          // +0x00
  ERShaderUpdateNode* prev = nullptr;  // +0x04
  ERShaderUpdateNode* next = nullptr;  // +0x08
};

// The shader's colours as they were when a blend started (0x44 bytes, BlendCurrentShader's start).
struct ERShaderBlendFrom {
  float alphaThreshold[2] = {};  // +0x00 per layer (layer +0x14)
  float layer18[2] = {};         // +0x08 per layer (layer +0x18)
  float material[12] = {};       // +0x10 EMaterial (def +0x10..+0x3C)
  unsigned char alpha[2] = {};   // +0x40 per layer (layer +0x20)
};

// A multi-state shader's states (0x14 bytes): its kind, the states (sorted) and each state's shader.
struct ERShader__MultiShaderInfo {
  int m_baseId = -1;           // +0x3C the kind (0 localized, 1, 2: flag 0x10 / 0x20 / 0x40)
  float m_blendTime = 0;       // +0x40 seconds over which a state change blends (0: switch at once)
  unsigned char m_count = 0;   // +0x44
  unsigned char* m_states = nullptr;  // +0x48 the state numbers
  unsigned int* m_ids = nullptr;      // +0x4C each state's shader resource
};

struct ERShader : public EResource {
  ERShader();
  SIMMY_STORABLE_DECL(ERShader)
  ~ERShader() override;
  void Load(EFile& file) override;
  void Refresh(EFile* file) override;
  using EResource::Refresh;
  void DelRefSubResources() override;
  void AddRefSubResources() override;
  int TryIncrementSubResources() override;
  void DetachClone() override;
  virtual void Update(float, int, int, int);

  void Deallocate();
  void DeallocateIncomingShader();
  void CreateEShaderForTexture(ETexture*);
  ERShader* Clone(unsigned int id);
  static void SetDefaultShaderState(int state);  // 8031DCD0
  static unsigned char m_defaultShaderState;     // 804FF108
  void DoLoadMultiShader(char*, EShaderDef&);
  int SetCurrentShaderForState(unsigned char state);  // 8031DFE0
  void ChangeCurrentShader(ERShader*);
  unsigned int GetMultiShaderStateID(int state);  // 8031E164
  int SetShaderASyncForState(unsigned char state);  // 8031E1DC
  void DoLoad(EFile& file, EShaderDef& def);
  void OldLoad(EFile&, EShaderDef&);
  const unsigned char* CopyShedData(EShaderDef& def, const unsigned char* data);
  RET_UNKNOWN CopyIntermediateShedData(EShaderDef&, char*);
  void AddToUpdateList();
  void RemoveFromUpdateList();
  int HasCommonTexture(ERShader* a, ERShader* b);  // 8031F414
  void UpdateWeatherShader(int);
  void UpdateTimeOfDayShader();
  void InterpolateShaders(float);
  void BlendCurrentShader(float);
  static void UpdateAll(float dt, int hour, int minute, int weather);  // 8031FB04
  static void InitDayNightShaders(int hour, int minute);  // 8031FDCC
  void SetDayNightShaderToClosestTime(int, int);
  void ReplaceTexture(ERTexture* texture, int layer);
  void SwapTexture(ERTexture*, int);
  RET_UNKNOWN HasTexture(unsigned int);
  static void RegisterType(unsigned short);
  void CreateCopy() const;
  EShader* GetShader();
  void SetSupportDecal(bool);
  RET_UNKNOWN GetSupportDecal();
  RET_UNKNOWN IsDayNightShader();
  RET_UNKNOWN GetRTextureForRenderPass(int);
  RET_UNKNOWN GetCurrentShader();
  bool SupportsMultiShaderStates();
  RET_UNKNOWN GetStateCount();
  RET_UNKNOWN GetState(unsigned int);
  RET_UNKNOWN GetCurrentGraphicsState();
  int IsStateChangePending();  // 80320854
  RET_UNKNOWN IsMultiShader() const;
  RET_UNKNOWN IsMultiTextureShader();
  RET_UNKNOWN GetTimeState();
  void Select(ERC*, unsigned int);
  static void SelectForShadowMask(ERC*);
  RET_UNKNOWN CanColorBeModified();
  RET_UNKNOWN GetModifiableColor(unsigned int);

  EShader* m_shader = nullptr;          // +0x14
  ERShader* m_stateShader = nullptr;    // +0x18 multi-state: the shader of the current state
  ERShader* m_incoming = nullptr;       // +0x1C multi-state: the state being switched to
  ERTexture* m_textures[2] = {};        // +0x20 one per layer
  unsigned int m_textureIds[2] = {};    // +0x28
  unsigned int m_multiTextureCount = 0;  // +0x30
  ERTexture** m_multiTextures = nullptr;     // +0x34
  unsigned int* m_multiTextureIds = nullptr; // +0x38
  ERShader__MultiShaderInfo m_multi;    // +0x3C
  unsigned char m_field50 = 0;          // +0x50
  ERShaderUpdateNode* m_updateNode = nullptr;  // +0x54 in the multi-state update list
  unsigned int m_shaderFlags = 0;       // +0x58 (2 = multi-state, 4 = async state load pending, 8 = ?)
  unsigned int m_pendingId = 0;         // +0x5C
  float m_blendElapsed = 0;             // +0x60 seconds into the blend (flag 8)
  unsigned char m_localizedState = 0;   // +0x64 index of the current state
  unsigned char m_field65 = 0;          // +0x65 the previous index (SetShaderASyncForState)
  unsigned char m_hour = 0, m_minute = 0;  // +0x66 the clock Update was last given
  ERShaderBlendFrom* m_field68 = nullptr;  // +0x68 the colours a blend started from
};
