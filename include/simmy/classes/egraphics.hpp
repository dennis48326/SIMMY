// @class EGraphics after EMat4 EVec3 EGlobalManagerClient
// Platform-independent graphics front end (engine/e_graphics.cpp): a 70-slot virtual interface over
// frame, display-list, texture, shader and render-surface management. Offsets in comments are the
// GameCube layout, for cross-reference with the raw decompilation.
#pragma once

struct EGraphics : public EGlobalManagerClient {
  EGraphics();  // 802F0D9C
  virtual ~EGraphics();  // 802F0F34
  virtual void ManagedShutdown();  // 802F0F80
  virtual RET_UNKNOWN Init();  // 802F0F84
  virtual void DeselectTextures();  // 802F1034
  virtual void BeginFrame();  // 802F10C8
  virtual void EndFrame();  // 802F1120
  virtual void DoSwapBuffer(int);  // 802F11A8
  virtual void DoSetupFrameBuffer(int);  // 802F11AC
  virtual void Flush();  // 802F11B0
  virtual void GetOutputRect(TRect<float> &, int);  // 802F1244
  virtual void GetScissorRect(TRect<float> *, TRect<float> &, TRect<float> &);  // 802F12B0
  virtual void SetBackgroundColor(EVec3 &, int, bool);  // 802F12D4
  virtual RET_UNKNOWN GetBackgroundColor(EVec3 &, int &);  // 802F12FC
  virtual EDL* AllocDL(RCMode mode);  // 802F1324
  virtual void FreeDL(EDL* dl, RCMode mode);  // 802F1350
  virtual ERC* AllocRC(RCMode mode);  // 802F1390
  virtual void FreeRC(ERC* rc, RCMode mode);  // 802F13B8
  virtual ERC* Open(RCMode mode);  // 802F13F8
  virtual EDL* Close(ERC* rc);  // 802F1470
  virtual void Execute(EDL* dl, bool release);  // 802F1520
  EDL* AllocDisplayList(RCMode mode);  // 802F1558
  virtual void Destroy(EDL* dl);  // 802F158C
  void DeallocateDL(EDL* dl, RCMode mode);  // 802F15B8
  virtual ETexture* CreateTexture(ETextureDef& def);  // 802F15EC
  virtual void Destroy(ETexture *);  // 802F1690
  virtual EShader* CreateShader(EShaderDef& def);  // 802F1748
  virtual void Destroy(EShader *);  // 802F17EC
  virtual ERenderSurface* CreateRenderSurface(ERenderSurfaceDef& def);  // 802F1840
  virtual void Destroy(ERenderSurface* surface);  // 802F18E4
  virtual EMovie* CreateMovie();  // 802F1954
  virtual void Destroy(EMovie *);  // 802F1988
  virtual void* AllocVertexData(unsigned int size);  // 802F19BC
  virtual void FreeVertexData(void* data);  // 802F1A08
  void ComputeViewport(EViewport& vp, TRect<float>& rect);  // 802F1A58
  virtual void SetUpNormalMapMatrix();  // 802F1B3C
  void LoadSystemFont();  // 802F1BB8
  void DrawTiming();  // 802F1C34
  RET_UNKNOWN GetSystemFont();  // 802F1E98
  void DisplayTiming(bool, EVec2 &);  // 802F1EC8
  void SetCensorRect(EVec3 &, EVec3 &, bool);  // 802F1F10
  virtual RET_UNKNOWN GetFrameBufferTextureCoordinate(EVec2);  // 802F1F9C
  virtual RET_UNKNOWN SetCameraBloom(float, float, float, float);  // 802F1FB4
  virtual RET_UNKNOWN SetBloom();  // 802F2030
  virtual void SetWorldBloom(float, float, float, float);  // 802F2094
  RET_UNKNOWN SetMotionBlur(float, float, float, float);  // 802F20D8
  RET_UNKNOWN GetCameraBloom(float &, float &, float &, float &);  // 802F20EC
  RET_UNKNOWN GetMotionBlur(float &, float &, float &, float &);  // 802F2110
  RET_UNKNOWN SetTint(EVec3 &, EVec3 &);  // 802F2134
  void GetTint(EVec3 &, EVec3 &);  // 802F2170
  virtual void Shutdown();  // 802F21AC
  virtual RET_UNKNOWN FrameBufferClearFlag();  // 802F21B0
  virtual int FrameBufferClearRequestFlag();  // 802F21B8
  virtual void DrawCensorRects(ERC *);  // 802F21C0
  virtual RET_UNKNOWN GetFrameBufferTexture(int);  // 802F21C4
  virtual RET_UNKNOWN GetDepthBufferTexture();  // 802F21CC
  virtual int NeedFlushToDestroy(EDL *);  // 802F21D4
  virtual RET_UNKNOWN NeedFlushToDestroy(ETexture *);  // 802F21DC
  virtual int NeedFlushToDestroy(ERenderSurface *);  // 802F21E4
  virtual int NeedFlushToDestroy(EMovie *);  // 802F21EC
  virtual RET_UNKNOWN GetLargestAvailableTextureMemoryBlock();  // 802F21F4
  virtual RET_UNKNOWN NeedFlushToDestroy(EShader *);  // 802F21FC
  virtual float GetFarZVal();  // 802F2204
  virtual float GetNearZVal();  // 802F2210
  RET_UNKNOWN GetCoordinateSystem();  // 802F221C
  void SetCoordinateSystem(ECoordinateSystem);  // 802F2224
  virtual float GetScreenAspect();  // 802F222C
  RET_UNKNOWN GetScreenXSize();  // 802F2238
  RET_UNKNOWN GetScreenYSize();  // 802F2240
  double GetXPixelSize();  // 802F2248
  double GetYPixelSize();  // 802F2288
  virtual RET_UNKNOWN GetActualScreenXSize();  // 802F22C8
  virtual RET_UNKNOWN GetActualScreenYSize();  // 802F22D0
  void SetScreenXOffset(int);  // 802F22D8
  void SetScreenYOffset(int);  // 802F22E0
  RET_UNKNOWN GetScreenXOffset();  // 802F22E8
  RET_UNKNOWN GetScreenYOffset();  // 802F22F0
  virtual RET_UNKNOWN GetMaxTextureXSize();  // 802F22F8
  virtual RET_UNKNOWN GetMaxTextureYSize();  // 802F2300
  RET_UNKNOWN GetNormalMapMatrix();  // 802F2308
  virtual void GrabFrameBufferChunk32(int, int, int, int, unsigned int *, int *);  // 802F2310
  virtual void DiscardAllVram();  // 802F2314
  virtual void ClearStencilBuffer();  // 802F2318
  virtual void ProcessFrameEffects();  // 802F231C
  void EnableWireframe(bool);  // 802F2320
  void EnableFirstPassRender();  // 802F2328
  void EnableNormalRender();  // 802F2334
  RET_UNKNOWN IsFirstPassRenderOnly();  // 802F2340
  void SetEnableFrameEffects(bool);  // 802F2348
  RET_UNKNOWN IsEnableFrameEffects();  // 802F2350
  RET_UNKNOWN IsInsideBeginEnd();  // 802F2358
  virtual int ManagedStartup();  // 802F2360
  virtual void SelectFrameBuffer(int);  // 802F2368
  // Platform hooks (pure in EGraphics; vtable slots 60/61).
  virtual ETexture* AllocTexture() = 0;
  virtual void FreeTexture(ETexture* texture) = 0;
  virtual EShader* AllocShader() = 0;
  virtual void FreeShader(EShader* shader) = 0;
  virtual ERenderSurface* AllocRenderSurface() = 0;  // slots 64/65
  virtual void FreeRenderSurface(ERenderSurface* surface) = 0;
  virtual EMovie* AllocMovie() = 0;  // slots 66/67
  virtual void FreeMovie(EMovie* movie) = 0;

  int m_field04 = 0;            // +0x04
  int m_initialized = 0;        // +0x08, set by Init
  int m_renderedFrame = 1;      // +0x0C, copied from +0x10 when a frame begins
  int m_frameCounter = 1;       // +0x10
  int m_field14 = 0;            // +0x14
  int m_screenWidth = -1;       // +0x18, filled in by the renderer's InitVideo
  int m_screenHeight = -1;      // +0x1C
  int m_field20 = 0, m_field24 = 0, m_field28 = -1, m_field2C = -1;  // +0x20..+0x2C
  int m_field30 = 0, m_field34 = 0, m_field38 = 0, m_field3C = 0;    // +0x30..+0x3C
  EMat4 m_normalMapMatrix;      // +0x40
  EVec3 m_backgroundColor;      // +0x80
  int m_backgroundAlpha = 0xff; // +0x8C
  int m_field90 = 0;            // +0x90
  int m_field98 = 0;            // +0x98
  // The censor rectangles of this frame (SetCensorRect; drawn and emptied by DrawCensorRects).
  int m_censorUsed[16] = {};    // +0xA4
  EVec3 m_censorMin[16];        // +0xE4 screen x, y (0..1) and depth
  EVec3 m_censorMax[16];        // +0x1A4
  int m_censorPaused[16] = {};  // +0x264 the simulation was paused
  int m_censorCount = 0;        // +0x2A4
  int m_field2CC = 0;           // +0x2CC
  int m_field31C = 0;           // +0x31C
  int m_field320 = 1;           // +0x320
  ERC* m_openRC = nullptr;      // +0x324 the immediate context while it is open
};
