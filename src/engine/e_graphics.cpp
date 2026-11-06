// Port of engine/e_graphics.cpp (partial): construction, init, background colour, screen queries.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

// 802F0D9C
EGraphics::EGraphics() {
  EGlobalManager::Register(this, 4);
  SetCameraBloom(0.0f, 0.0f, 0.0f, 0.0f);
  SetWorldBloom(2.0f, 0.25f, 2.0f, 1.0f / 3.0f);
  SetMotionBlur(0.0f, 0.0f, 1.0f, 0.15f);
  EVec3 tint(0.5f, 0.5f, 0.5f), tintAdd(0.0f, 0.0f, 0.0f);
  SetTint(tint, tintAdd);
}

// 802F0F34
EGraphics::~EGraphics() = default;

// 802F2360
int EGraphics::ManagedStartup() { return 1; }

// 802F0F80
void EGraphics::ManagedShutdown() {}

// 802F0F84
RET_UNKNOWN EGraphics::Init() {
  m_initialized = 1;
  EVec3 black(0.0f, 0.0f, 0.0f);
  SetBackgroundColor(black, 0xff, true);
  SetUpNormalMapMatrix();
  return _pSched->Init() != 0;
}

// 802F12D4
void EGraphics::SetBackgroundColor(EVec3& color, int alpha, bool clear) {
  m_backgroundColor = color;
  m_frameCounter = clear;
  m_backgroundAlpha = alpha;
}

// 802F12FC
RET_UNKNOWN EGraphics::GetBackgroundColor(EVec3& color, int& alpha) {
  color = m_backgroundColor;
  alpha = m_backgroundAlpha;
  return 0;
}

// 802F21B8
int EGraphics::FrameBufferClearRequestFlag() { return m_frameCounter; }

// 802F1B3C
void EGraphics::SetUpNormalMapMatrix() {
  // Maps normals from [-1,1] to texture space [0,1]: scale by 0.5 then offset by 0.5.
  EVec3 half(0.5f, 0.5f, 0.0f);
  m_normalMapMatrix.Scale(half);
  EVec3 offset(0.5f, 0.5f, 1.0f);
  m_normalMapMatrix.PostTranslate(offset);
}

// 802F2204
float EGraphics::GetFarZVal() { return 0.0f; }
// 802F2210
float EGraphics::GetNearZVal() { return 1.0f; }
// 802F222C
// GC: 4:3. PC: the real frame's aspect while the co-op split views stretch the frame over a wider
// window (platform.hpp, "Co-op split screen"), else 4:3.
float EGraphics::GetScreenAspect() { return simmy::split_screen_aspect(); }
// 802F22C8
RET_UNKNOWN EGraphics::GetActualScreenXSize() { return m_screenWidth; }
// 802F22D0
RET_UNKNOWN EGraphics::GetActualScreenYSize() { return m_screenHeight; }

// 802F10C8
void EGraphics::BeginFrame() {
  _pSched->QueueSetupFrameBuffer(static_cast<int>(_evenodd));
  EWindow::m_pCurrentWindow = nullptr;
  m_field04 = 1;                       // in a frame
}

// 802F1120
void EGraphics::EndFrame() {
  DeselectTextures();
  m_field04 = 0;
  _pEngine->FrameComplete();
  _pSched->QueueSwapBuffer(static_cast<int>(_evenodd), _pEngine->GetMinRetraces());
}

// 802F11B0
// Waits for the GPU (on the app thread only): the open immediate context sends what it holds, then the
// scheduler flushes (ENgcScheduler::Flush -> ENgcRenderer::Flush: GXDrawDone). Callers use it before
// freeing what the GPU may still read (FreeVertexData, render surfaces) and before reading pixels back.
void EGraphics::Flush() {
  if (!_pApp->IsCallingThread()) return;
  if (m_openRC && !m_openRC->m_openCount) m_openRC->Send();  // GC: vtable 0x14
  _pSched->Flush();                                             // GC: vtable 0x44
}

// 802F15EC
ETexture* EGraphics::CreateTexture(ETextureDef& def) {
  ETexture* texture = AllocTexture();
  if (!texture) return nullptr;
  ++m_field34;  // live textures
  if (texture->Create(def)) return texture;
  Destroy(texture);
  return nullptr;
}

// 802F1690
// (The original first deselected all textures if this one was bound in the render-state cache.)
void EGraphics::Destroy(ETexture* texture) {
  if (!texture) return;
  FreeTexture(texture);
  --m_field34;
}

// 802F1840
ERenderSurface* EGraphics::CreateRenderSurface(ERenderSurfaceDef& def) {
  ERenderSurface* surface = AllocRenderSurface();
  if (!surface) return nullptr;
  ++m_field30;  // live render surfaces
  if (surface->Create(def)) return surface;
  Destroy(surface);
  return nullptr;
}

// 802F18E4
void EGraphics::Destroy(ERenderSurface* surface) {
  if (!surface) return;
  Flush();
  --m_field30;
  FreeRenderSurface(surface);
}

// 802F1748
EShader* EGraphics::CreateShader(EShaderDef& def) {
  EShader* shader = AllocShader();
  if (!shader) return nullptr;
  ++m_field38;  // live shaders
  if (shader->Create(def)) return shader;
  Destroy(shader);
  return nullptr;
}

// 802F17EC
void EGraphics::Destroy(EShader* shader) {
  if (!shader) return;
  FreeShader(shader);
  --m_field38;
}

// 802F1954
EMovie* EGraphics::CreateMovie() { return AllocMovie(); }

// 802F1988
void EGraphics::Destroy(EMovie* movie) { FreeMovie(movie); }

// 802F21EC
int EGraphics::NeedFlushToDestroy(EMovie*) { return 1; }
// 802F21E4
int EGraphics::NeedFlushToDestroy(ERenderSurface*) { return 1; }
// 802F21D4
int EGraphics::NeedFlushToDestroy(EDL*) { return 1; }

// 802F19BC
// Model vertex arrays and GX display lists: 32-byte aligned, from the model heap.
void* EGraphics::AllocVertexData(unsigned int size) { return ModelHeap()->MallocAligned(size, 0x20, 0, 0); }

// 802F1A08
void EGraphics::FreeVertexData(void* data) {
  Flush();
  ModelHeap()->Free(data);
}

// 802F1A58
// The viewport transform of a rectangle in output pixels: half extents (y flipped) and centre, with
// the depth range between GetNearZVal and GetFarZVal.
void EGraphics::ComputeViewport(EViewport& vp, TRect<float>& r) {
  const float zFar = GetFarZVal(), zNear = GetNearZVal();
  const float hh = (r.y1 - r.y0) * 0.5f, hw = (r.x1 - r.x0) * 0.5f;
  const float hz = (zFar - zNear) * 0.5f;
  vp.m_scale[0] = hw;
  vp.m_scale[1] = -hh;
  vp.m_scale[2] = hz;
  vp.m_scale[3] = 1.0f;
  vp.m_center[0] = hw + r.x0;
  vp.m_center[1] = hh + r.y0;
  vp.m_center[2] = zNear + hz;
  vp.m_center[3] = 0.0f;
}

// ---- Display lists and render contexts -----------------------------------------------------------

// 802F1324
EDL* EGraphics::AllocDL(RCMode) { return new EDL(0x1000); }
// 802F1350
void EGraphics::FreeDL(EDL* dl, RCMode) { delete dl; }
// 802F1390
ERC* EGraphics::AllocRC(RCMode) { return new ERC; }
// 802F13B8
void EGraphics::FreeRC(ERC* rc, RCMode) { delete rc; }

// 802F1558
EDL* EGraphics::AllocDisplayList(RCMode mode) { return AllocDL(mode); }
// 802F15B8
void EGraphics::DeallocateDL(EDL* dl, RCMode mode) { FreeDL(dl, mode); }
// 802F158C
void EGraphics::Destroy(EDL* dl) {
  if (dl) DeallocateDL(dl, RCMode::Deferred);
}

// 802F13F8
// Starts recording. The immediate context is remembered while open.
ERC* EGraphics::Open(RCMode mode) {
  ERC* rc = AllocRC(mode);
  if (rc) {
    rc->Init(mode);
    if (mode == RCMode::Immediate) m_openRC = rc;
  }
  return rc;
}

// 802F1470
// Finishes recording. A deferred context's list is returned for the caller to execute (and free);
// an immediate one's is sent to the renderer now.
EDL* EGraphics::Close(ERC* rc) {
  if (!rc) return nullptr;
  rc->Terminate();
  rc->m_dl->Validate();
  const int mode = rc->m_mode;
  EDL* dl = rc->m_dl;
  FreeRC(rc, mode);
  if (mode != RCMode::Immediate) return dl;
  Execute(dl, true);
  m_openRC = nullptr;
  return nullptr;
}

// 802F1520
void EGraphics::Execute(EDL* dl, bool release) { _pSched->QueueDisplayList(dl, release); }

// 802F1244
void EGraphics::GetOutputRect(TRect<float>& r, int) {
  r = TRect<float>{0, 0, static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)};
}
// 802F12B0
void EGraphics::GetScissorRect(TRect<float>* out, TRect<float>& rect, TRect<float>&) { *out = rect; }

// 802F21B0
// Whether this frame's buffer gets cleared and drawn (+0x0C, latched from +0x10 at BeginFrame).
RET_UNKNOWN EGraphics::FrameBufferClearFlag() { return m_renderedFrame; }

// 802F1F10
// One more censor rectangle for this frame (up to 16): its corners in screen space (x, y 0..1, z the
// depth) and the flag (GC: the simulator's time of day is night).
void EGraphics::SetCensorRect(EVec3& min, EVec3& max, bool flag) {
  if (m_censorCount == 16) return;
  m_censorUsed[m_censorCount] = 1;
  m_censorMin[m_censorCount] = min;
  m_censorMax[m_censorCount] = max;
  m_censorPaused[m_censorCount] = flag ? 1 : 0;
  ++m_censorCount;
}

// 802F21C0
void EGraphics::DrawCensorRects(ERC*) {}
