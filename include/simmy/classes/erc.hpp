// @class ERC after EDL RCMode EVec2 EVec4 EMat4 EViewport ELights EMaterial
// Render context (engine/e_rc.cpp): the API the game draws through. Each call records a command into
// a display list (EDL); an immediate context (RCMode::Immediate) sends each finished command to the
// renderer at once, a deferred one keeps the list for the caller to execute later.
#pragma once

struct ERC {
  ERC();  // 802F64A8
  virtual ~ERC();  // 802F652C
  virtual void Init(RCMode mode);  // 802F6580
  virtual void BeginCommand(int, int);  // 802F6608
  virtual void Send();  // 802F6698
  virtual EDLEntry* NewEntry(int count);  // 802F678C
  static void Connect(EDLEntry *, EDLEntry *);  // 802F682C
  virtual void TriStrip(EGEVert *, int);  // 802F683C
  virtual void TriStrip(int, float *, float *, unsigned char *, signed char *, unsigned char *, bool);  // 802F68EC
  virtual RET_UNKNOWN TriStripRef(int, float *, float *, unsigned char *, signed char *, unsigned char *, bool);  // 802F6A2C
  virtual RET_UNKNOWN TriStrip(int, short *, short *, unsigned char *, signed char *, unsigned char *, bool);  // 802F6A60
  virtual RET_UNKNOWN TriStripRef(int, short *, short *, unsigned char *, signed char *, unsigned char *, bool);  // 802F6BAC
  virtual void TriFan(EGEVert *, int);  // 802F6BE0
  virtual void TriList(EGEVert *, int);  // 802F6BE4
  virtual void QuadList(EGEVert *, int);  // 802F6BE8
  virtual void QuadList(int, float *, float *, unsigned char *, signed char *, bool);  // 802F6BEC
  virtual void LineList(EGEVert *, int);  // 802F6BF0
  virtual void LineStrip(EGEVert *, int);  // 802F6CA4
  virtual void PointList(EGEVert *, int);  // 802F6D58
  virtual void PointList(int, float *, float *, unsigned char *, signed char *, unsigned char *);  // 802F6D5C
  virtual RET_UNKNOWN SpriteList(EGEVert *, int);  // 802F6E88
  virtual RET_UNKNOWN SpriteList(int, float *, float *, unsigned char *, signed char *, unsigned char *);  // 802F6F3C
  virtual void ParticleListInfo(EGEParticleListInfo* info);  // 802F7068
  virtual void* ParticleListBegin(unsigned int count, unsigned int type);  // 802F706C
  virtual void ParticleListEnd(unsigned int);  // 802F7074
  virtual void ParticleList(int, EGEPackedParticle *);  // 802F7078
  virtual void LineStreakList(int, EVec4 &, EVec4 &, EGELineStreak *);  // 802F7134
  virtual RET_UNKNOWN ParticleListRot(int, EGEPackedParticle *);  // 802F7234
  void AddDisplayListReference(EDL* dl);  // 802F72F0
  virtual void DisplayList(EDL *);  // 802F7334
  void ShrinkSmallDisplayList();  // 802F740C
  virtual void Terminate();  // 802F74C0
  virtual void Viewport(EViewport *);  // 802F7578
  virtual void ClipRect(TRect<float> &);  // 802F760C
  virtual void Scissor(TRect<float> *);  // 802F76B4
  virtual void ModelMatrixList(EMat4 *, int);  // 802F7748
  virtual void ModelMatrixIndex(int, int);  // 802F77F0
  virtual void ModelMatrices(EMat4 *, int);  // 802F786C
  virtual void ModelMatrixId();  // 802F7900
  virtual void FlushQueuedMatrices();  // 802F793C
  virtual void ViewMatrix(EMat4 *, int, float);  // 802F7A44
  virtual void ProjectionMatrix(EMat4 *);  // 802F7AD8
  virtual void WindowMatrix(EMat4 *);  // 802F7B6C
  virtual void Texture(ETexture *, int);  // 802F7C00
  virtual void EnableGeometryModes(unsigned int);  // 802F7CAC
  virtual void DisableGeometryModes(unsigned int);  // 802F7D38
  virtual void SetGeometryModes(unsigned int);  // 802F7DC8
  virtual void EnableRasterModes(unsigned int, int);  // 802F7E54
  virtual void DisableRasterModes(unsigned int, int);  // 802F7EE8
  virtual void SetRasterModes(unsigned int, int);  // 802F7F80
  virtual void SaveState();  // 802F8014
  virtual void RestoreState();  // 802F8098
  virtual void Lights(ELights *);  // 802F811C
  virtual void Material(EMaterial *);  // 802F81D8
  virtual void MipMapSetup(EGEVert *, bool, bool);  // 802F826C
  virtual void SetMipMap(float, int);  // 802F8270
  virtual void Callback(void (*)(unsigned int, unsigned short, unsigned char), unsigned int, unsigned short, unsigned char);  // 802F830C
  virtual void RectList(int count, float* rects, EVec4& color, float z);  // 802F83D4
  virtual void RectListRot(int count, float* rects, EVec4& color, float z);  // 802F84B8
  virtual void Rect(EVec2 &, EVec2 &, EVec2 &, EVec2 &, EVec4 &, float);  // 802F8594
  virtual RET_UNKNOWN Rect(EVec2 &, EVec2 &, EVec4 *, EVec2 &, EVec2 &, float);  // 802F869C
  virtual RET_UNKNOWN DirectRect(EVec2 &, EVec2 &, EVec4 &, float);  // 802F87A4
  virtual RET_UNKNOWN EnvironmentMap(bool, bool, int);  // 802F8884
  virtual void TextureMatrix(EMat4 *, ETCTransformSource, bool, bool, int);  // 802F8918
  virtual RET_UNKNOWN RecalcMatrices(int, int);  // 802F8A30
  virtual void Debug(unsigned int, unsigned int);  // 802F8AC4
  virtual void GeometrySetup();  // 802F8B58
  virtual void ZTest(bool, int, int, int);  // 802F8BDC
  virtual void AlphaTest(bool enable, int compare, float ref, int pass);  // 802F8C80
  virtual RET_UNKNOWN Stencil(int, int, int);  // 802F8D44
  virtual void SetColorModulation(int, EVec4);  // 802F8DE0
  virtual void EnableColorModulation(bool);  // 802F8DE4
  virtual void SetBlendMode(int, int, int, int, float, int);  // 802F8DE8
  virtual RET_UNKNOWN SetBlendModePass3(int, int, int, int, int);  // 802F8EC4
  virtual void SetTextureAlphaValue(unsigned char, unsigned char, unsigned char);  // 802F8EC8
  virtual void SetCombineMode(int mode, int layer);  // 802F8ECC
  virtual void RenderSurface(ERenderSurface *, int);  // 802F8F60
  virtual void SaveImageData(ERenderSurface *);  // 802F8FF4
  virtual void Vertex(int, int, float *, float *, unsigned char *, signed char *, unsigned char *);  // 802F9080
  virtual RET_UNKNOWN TriIndexed(int, unsigned char *);  // 802F9150
  virtual void Noop();  // 802F91E4
  virtual RET_UNKNOWN ZClear(float, float, float, float, float);  // 802F9268
  virtual RET_UNKNOWN StencilClear(float, float, float, float, int);  // 802F9494
  virtual RET_UNKNOWN SetDebugMarker(int);  // 802F9674
  virtual void MovieFrame(EMovie *);  // 802F9678
  virtual void SetAlpha(float);  // 802F9704
  virtual void SetAlphaPlane(float, EVec4, float, float);  // 802F9798
  virtual void SetFog(float, float, EVec4);  // 802F979C
  virtual void CopyScreenToTexture(ETexture *, EVec4 &, EVec2 &);  // 802F97A0
  void* Alloc(unsigned int size, int align);  // 802F98C0
  void AllocExternal(void *, int);  // 802F98E4
  void* AllocFlushable(unsigned int size, int align);  // 802F9908
  void AllocFlushableExternal(void *, int);  // 802F9930
  void* AllocAndCopy(void* src, int size);  // 802F9958
  virtual void ProcessFrameEffects(int, void *);  // 802F99B4
  void SetName(const char* name);  // 802F99EC
  void ModelMatrix(EMat4 *);  // 802F9A80
  virtual void LightsRadiosity(EVec3 &, EVec3 &, float);  // 802F9AB8
  void Validate();  // 802F9ABC
  void GetFogColor();  // 802F9AE0
  virtual RET_UNKNOWN AllocVertexData(unsigned int);  // 802F9B0C
  void SetCopyDataCallback(void (*)(void *, void *), void *);  // 802F9B14
  bool IsOpen() const;  // 802F9B20
  virtual void EndCommand();  // 802F9B38

  EDL* m_dl = nullptr;                 // +0x00 the list being recorded
  int m_entriesLeft = 0;               // +0x04 free entries in the current block
  EDLEntry* m_next = nullptr;          // +0x08
  int m_mode = 0;                      // +0x0C RCMode
  int m_terminated = 0;                // +0x10
  int m_commandDone = 0;               // +0x14 a command finished since the last Send
  unsigned char m_openCount = 0;       // +0x18 BeginCommand nesting
  int m_commandType = 0;               // +0x1C 1 = matrices queued, 2 = geometry
  int m_blockCount = 0;                // +0x20
  int m_matIndex[4] = {0xff, 0xff, 0xff, 0xff};  // +0x24 queued ModelMatrixIndex values
  int m_indicesQueued = 0;             // +0x34
  EMat4* m_matList = nullptr;          // +0x38 queued ModelMatrixList
  int m_matListCount = 0;              // +0x3C
  int m_listQueued = 0;                // +0x40
  int m_maxMatrix = -1;                // +0x44 highest matrix index used
  int m_field54 = 0, m_field58 = 0;    // +0x54 copy-data callback
  std::string m_name;                  // +0x6C EString

  static constexpr int kBlockEntries = 0x200;  // entries per allocated block (the last links on)
};
