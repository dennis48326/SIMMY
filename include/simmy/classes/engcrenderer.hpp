// @class ENgcRenderer after EVec3 ENgcRendCommand EMat4 EMaterial ELights EShaderDef ENgcRenderStateCache EDL
// GameCube renderer (engine/e_ngcrenderer.cpp). It executes the game's display lists with GX calls,
// which aurora implements. It derives from EThread but never ran as a thread: its Queue* methods run
// on the calling (app) thread.
#pragma once

struct ENgcRenderer : public EThread {
  ENgcRenderer();  // 8033B50C
  virtual ~ENgcRenderer();  // 8033B6EC
  void ShowPerfGraphs(bool);  // 8033B720
  virtual RET_UNKNOWN GetCurrentTexture(int);  // 8033B724
  bool Init(int& width, int& height);  // 8033B734
  void CreateGXDisplayList(EDL *, GXPrimitive, int, unsigned short *, int, float *, float *, unsigned char *, signed char *, unsigned int &, bool);  // 8033B764
  RET_UNKNOWN CreateGXDisplayList(EDL *, GXPrimitive, int, unsigned short *, int, short *, short *, unsigned char *, signed char *, unsigned int &, bool);  // 8033BBF8
  RET_UNKNOWN CreateGXDisplayList(EDL *, GXPrimitive, int, float *, float *, unsigned char *, signed char *, unsigned int &, bool);  // 8033C08C
  unsigned char* CreateGXDisplayList(EDL* dl, GXPrimitive prim, int count, short* positions, short* uvs, unsigned char* colors, signed char* normals, unsigned int& size, bool twoUVs, bool weights);  // 8033C510
  void SetClearColor(EVec3& color, bool clear);  // 8033CA48
  RET_UNKNOWN GetExternalFrameBuffer();  // 8033CA98
  void CycleToNextFrameBuffer();  // 8033CAB8
  static void VIPreRetraceCallback(unsigned long);  // 8033CCD4
  void VIPostRetraceCallback(unsigned long);  // 8033CDEC
  static void BreakPtCallback();  // 8033CDF0
  static void DrawSyncCallback(unsigned short);  // 8033CE20
  static RET_UNKNOWN AddDrawSync();  // 8033CE28
  void InitGX(GXRenderModeObj *);  // 8033CEB8
  void InitGXVertexFormats();  // 8033D1F0
  int InitVideo(int& width, int& height);  // 8033D5B4
  void InitData();  // 8033D868
  void ClearRect(EVec4 &, EVec4 &);  // 8033E1D4
  void QueueBeginFrame(int);  // 8033E7F4
  void QueueDisplayList(EDL *, bool);  // 8033E824
  void QueueEndFrame(EEvent &);  // 8033E858
  void QueueSwap(int, int);  // 8033E888
  void Flush();  // 8033E8BC
  void BeginFrame(ENgcRendCommand *);  // 8033E8E8
  void DisplayList(ENgcRendCommand *);  // 8033E94C
  void EndFrame(ENgcRendCommand *);  // 8033E9AC
  void Swap(ENgcRendCommand *);  // 8033EC3C
  static void Flush(ENgcRendCommand *);  // 8033ECB8
  void PushAndClearViewport();  // 8033ECD8
  void RestoreViewport();  // 8033EDC8
  void UpdateLightingEquation();  // 8033EDEC
  void SetupTEVStagePassColor(GXTevStageID);  // 8033F80C
  void Setup1TEVStageTexture(int layer, int& stage, int texCoord, int& texMap);  // 8033F9DC
  void Setup2TEVStageTexture(int layer, int& stage, int texCoord, int& texMap);  // 8033FD94
  void UpdateTEVStages();  // 803404F0
  void Execute(EDLEntry *);  // 80341E28
  RET_UNKNOWN TriStrip(EDLEntry *);  // 80342120
  void TriStrip(EGEVert *, int);  // 80342148
  void WeightedBlend(GXPrimitive, int, float *, signed char *, float *, unsigned char *, unsigned char *, bool);  // 803424E0
  RET_UNKNOWN TriStripPacked(EDLEntry *);  // 80342878
  void TriStripPacked(int, float *, float *, unsigned char *, signed char *, unsigned char *, unsigned char *, bool);  // 803428D0
  void TriStripPackedInt(EDLEntry* e);  // 80343264
  void LineList(EDLEntry *);  // 80343BD4
  void LineList(EGEVert *, int);  // 80343BFC
  void DisplayList(EDLEntry *);  // 80343D24
  void Goto(EDLEntry *);  // 80343D54
  void End(EDLEntry *);  // 80343D60
  void SaveState(EDLEntry *);  // 80343D94
  void RestoreState(EDLEntry *);  // 80343DB8
  void Viewport(EDLEntry *);  // 80343DDC
  void ClipRect(EDLEntry *);  // 80343E7C
  void Scissor(EDLEntry *);  // 80343EA0
  void ModelMatrices(EDLEntry *);  // 80344034
  void ModelMatrices(EMat4 *, int);  // 8034405C
  void ModelMatrixIndex(int, int);  // 803440AC
  void ModelMatrixIndices(EDLEntry *);  // 803441C0
  void ViewMatrix(EDLEntry *);  // 8034420C
  void ProjectionMatrix(EDLEntry *);  // 803442D8
  void WindowMatrix(EDLEntry *);  // 8034439C
  void TextureMatrix(EDLEntry *);  // 8034443C
  void Texture(EDLEntry *);  // 80344440
  void SetTexture(ENgcTexture *, int);  // 8034447C
  void EnableGeometryModes(EDLEntry *);  // 80344598
  void DisableGeometryModes(EDLEntry *);  // 803445F0
  void SetGeometryModes(EDLEntry *);  // 80344648
  void EnableRasterModes(EDLEntry *);  // 80344698
  void DisableRasterModes(EDLEntry *);  // 803446C4
  void SetRasterModes(EDLEntry *);  // 803446F0
  void SetAlpha(float);  // 80344714
  void Lights(EDLEntry *);  // 80344724
  void Rect(EDLEntry *);  // 803448B4
  void DirectRect(EDLEntry *);  // 80344CA0
  void Material(EDLEntry *);  // 80345120
  void MipMapSetup(EDLEntry *);  // 803451A4
  void Debug(EDLEntry *);  // 803451BC
  void GeometrySetup(EDLEntry *);  // 803451E4
  void ZTest(EDLEntry *);  // 80345220
  void AlphaTest(EDLEntry *);  // 80345284
  void RenderSurface(EDLEntry *);  // 80345314
  void TriIndexed(EDLEntry *);  // 8034548C
  void SaveImageData(EDLEntry *);  // 803454A4
  void SpriteListPacked(EDLEntry *);  // 803454BC
  void SpriteListPacked(int, float *, float *, unsigned char *, signed char *, unsigned char *);  // 803454F8
  void QuadList(EDLEntry *);  // 803457B0
  void QuadList(int, float *, float *, unsigned char *, signed char *, unsigned char *, unsigned char *, bool);  // 80345808
  void SetCombineMode(EDLEntry *);  // 80346104
  void SetBlendMode(EDLEntry *);  // 80346124
  void MovieFrame(EDLEntry *);  // 8034615C
  void RectListRot(EDLEntry *);  // 80346E1C
  void RectList(EDLEntry *);  // 803472C0
  void Stencil(EDLEntry *);  // 80347630
  void NgcTexInvalidateAll(EDLEntry *);  // 803477AC
  void NgcGXDisplayListInt(EDLEntry *);  // 803477CC
  void NgcGXDisplayList(EDLEntry *);  // 80347BE4
  void NgcTriListInt(EDLEntry *);  // 80347FD8
  void NgcTriList(EDLEntry *);  // 80348358
  void NgcScreenTriList(EDLEntry *);  // 80348684
  void ReadMetrics(EDLEntry *);  // 803489A8
  void Shader(EDLEntry *);  // 803489D0
  void SetShader(ENgcShader *, unsigned int);  // 803489F8
  void ScrambleRect(EDLEntry *);  // 80348DDC
  void ScrambleRect(EVec2 &, EVec2 &, float);  // 80348E2C
  void WeightedBlendInt(GXPrimitive, int, short *, signed char *, short *, unsigned char *, unsigned char *, bool);  // 8034932C
  RET_UNKNOWN SimpleSkinning(GXPrimitive, int, short *, signed char *, short *, unsigned char *, unsigned char *, bool);  // 80349640
  RET_UNKNOWN ScaleTexture(GXTexObj *, void *, int, float);  // 8034991C
  RET_UNKNOWN DrawTextureToFullViewportWithJitter(float, float);  // 80349A70
  void DrawFrameWithBloomAndMotionBlur(float, float);  // 80349B94
  void ProcessThumbnail(void *, int, float, float);  // 80349CF8
  RET_UNKNOWN ProcessFrameEffects(float, float, float, float, EVec3 &, EVec3 &);  // 80349F30
  ENgcRenderStateCache* GetRenderStateCache();  // 80349F3C
  void* GetNextExternalFrameBuffer();  // 80349F44
  void* GetPreviousFrameBuffer(int* width, int* height);  // 80349F4C
  GXRenderModeObj GetCurrentRenderMode();  // 80349F6C
  void* GetFrameEffectsScratchMemory();  // 80349FD4
  void* ParticleListBegin(unsigned int count, unsigned int type);  // 80349FDC
  void ParticleListEnd(unsigned int count);  // 8034A02C
  RET_UNKNOWN GetRenderMode();  // 8034A51C
  void GetZTestSettings(unsigned char &, unsigned char &, unsigned char &);  // 8034A538
  RET_UNKNOWN AcquireCopyMutex();  // 8034A554
  static void ReleaseCopyMutex();  // 8034A594
  void TriFan(EDLEntry *);  // 8034A628
  void TriList(EDLEntry *);  // 8034A640
  void LineStrip(EDLEntry *);  // 8034A658
  void PointList(EDLEntry *);  // 8034A670
  void SpriteList(EDLEntry *);  // 8034A688
  void SendGSDisplayList(EDLEntry *);  // 8034A6A0
  void CallbackParam(EDLEntry *);  // 8034A6B8
  void Callback(EDLEntry *);  // 8034A6D0
  void GEList(EDLEntry *);  // 8034A6E8
  void RecalcMatrices(EDLEntry *);  // 8034A700
  void SetMipMap(EDLEntry *);  // 8034A718
  static void Vertex(EDLEntry *);  // 8034A730
  static void PointListPacked(EDLEntry *);  // 8034A754
  void NoOp(EDLEntry *);  // 8034A778
  void VerifyMpg(EDLEntry *);  // 8034A77C
  void ParticleList(EDLEntry *);  // 8034A794
  void ParticleListRot(EDLEntry *);  // 8034A7AC
  static void LineStreakList(EDLEntry *);  // 8034A7C4
  void SetAlpha(EDLEntry *);  // 8034A7E8
  void CopyScreenToTexture(EDLEntry *);  // 8034A800

  GXRenderModeObj m_renderMode{};  // +0x364 (fbWidth +0x368, efbHeight +0x36A, viWidth +0x372, viHeight +0x374)
  int m_xfbIndex = 0;              // +0x340
  int m_clearEnabled = 1;          // +0x360
  int m_lastRetrace = 0;           // +0x35C
  bool m_pcFrameOpen = false;      // PC: aurora_begin_frame succeeded for this frame
  unsigned int m_colorOverride = 0xffffffff;  // +0x3A0 RGB the TEV substitutes (shader colour slot), or ~0
  float m_alpha = 1.0f;            // +0x3A4 SetAlpha
  GXColor m_clearColor{0, 0, 0, 0x7f};  // +0x3AC
  unsigned int m_clearZ = 0xffffff;     // +0x3B0
  Mtx m_lightMtx{};                // +0x3B4 inverse transpose of the view matrix (light directions)
  Mtx m_identity{};                // +0x3E4
  unsigned char m_copyFilter[7] = {};  // +0x396 vertical filter
  unsigned char m_aa = 0;              // +0x37D
  EMat4 m_windowMtx;               // +0x498
  ENgcRenderStateCache* m_rsCache = nullptr;  // +0x4D8
  EDLEntry* m_cursor = nullptr;    // +0x4DC next entry Execute runs
  int m_done = 0;                  // +0x4E0 set by End at the outermost level
  EDLEntry* m_returnStack[10] = {};  // +0x4E4 DisplayList calls
  int m_returnDepth = 0;           // +0x50C
  int m_geometryDirty = 1;         // +0x510 material/lights/shader changed: GeometrySetup before drawing
  int m_numLayers = 0;             // +0x514 texture layers of the current shader
  EMaterial m_material;            // +0x518
  EShaderLayer m_layers[2];        // +0x548 the current shader's layers
  int m_reflection = 0;            // +0x5C8
  int m_usesSecondUV = 0;          // +0x5CC
  EMat4 m_layerTexMtx[2];          // +0x5D0 scroll/scale/offset of each layer's coordinates
  ELights m_lights;                // +0x650
  unsigned char m_numDirLights = 0, m_numPointLights = 0;  // +0x768
  EMat4 m_modelMatrices[248];      // +0x770 ModelMatrices
  unsigned int m_numModelMatrices = 0;  // +0x4570
  Mtx m_modelMtx[4]{};             // +0x4574 selected model matrices (GX layout)
  unsigned char m_numTextures = 0; // +0x4634
  ENgcTexture* m_textures[2] = {}; // +0x4638
  EDL* m_currentDL = nullptr;      // +0x4640
  unsigned char m_zTest[3] = {};   // +0x4644 last ZTest arguments
  int m_field465C = 0;             // +0x465C
  void* m_particleList = nullptr;  // +0x465C ParticleListBegin: the sprites (EGEPackedParticle)
  unsigned int m_particleCount = 0;  // +0x4660
  unsigned int m_particleType = 0;   // +0x4664
  GXFifoObj* m_gxFifo = nullptr;   // +0x3A8
  void* m_xfb[2] = {};             // +0x344 the external frame buffers (PC: copy destinations only)
  void* m_xfbCurrent = nullptr;    // +0x34C the one being drawn (the frame effects' full-size copy)
  void* m_fxScratch = nullptr;     // +0x464C 256x256 RGB565: the frame effects' scaled copies
  void* m_prevFrame = nullptr;     // +0x4650 half-size RGB565 copy of the last frame (EndFrame)

  // PC: GX drawing is only possible inside an aurora frame (and never headless).
  bool CanDraw() const { return m_pcFrameOpen; }
};
