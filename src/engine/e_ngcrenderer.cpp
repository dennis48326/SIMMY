// Port of engine/e_ngcrenderer.cpp (partial): construction, video/GX bring-up on aurora, and the
// frame begin/end/swap commands. Still stubbed: display-list execution, TEV/lighting, primitives.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>

ENgcRenderer* _pRend = nullptr;  // _pRend
int _tvHZ = 60;

// 8033B50C
ENgcRenderer::ENgcRenderer() {
  // The original also initialised its lighting defaults (three white lights, intensity 10) and an
  // array of light descriptors; those move here with the lighting code.
  _tvHZ = (_iVideoMode == 1) ? 50 : 60;
  _pRend = this;
}

// 8033B6EC
ENgcRenderer::~ENgcRenderer() = default;

// 8033B734
bool ENgcRenderer::Init(int& width, int& height) { return InitVideo(width, height) != 0; }

// 8033D5B4
int ENgcRenderer::InitVideo(int& width, int& height) {
  // GameCube: overscan-adjusted NTSC/PAL interlaced mode, two external frame buffers in main RAM,
  // VI setup and retrace callbacks. On PC aurora owns the swapchain; keep the render mode so the
  // game's size queries and copy setup see the same 640x448/480 numbers.
  m_renderMode = (_iVideoMode == 1) ? GXPal528IntDf : GXNtsc480IntDf;
  width = m_renderMode.fbWidth;
  height = m_renderMode.efbHeight;
  // The two external frame buffers, the frame effects' scratch buffer and the previous frame's
  // half-size copy (PC: GXCopyTex destinations: aurora keeps a texture per destination).
  const unsigned int stride = (m_renderMode.fbWidth + 15u) * 2u & 0x1ffe0u;
  for (void*& xfb : m_xfb) xfb = std::calloc(stride * m_renderMode.xfbHeight, 1);
  m_fxScratch = std::calloc(GXGetTexBufferSize(0x100, 0x100, GX_TF_RGB565, GX_FALSE, 0), 1);
  m_prevFrame = std::calloc(GXGetTexBufferSize(m_renderMode.fbWidth >> 1, m_renderMode.efbHeight >> 1, GX_TF_RGB565, GX_FALSE, 0), 1);
  m_xfbIndex = 0;
  m_xfbCurrent = m_xfb[0];
  InitData();
  InitGX(&m_renderMode);
  GXSetColorUpdate(GX_TRUE);
  GXSetAlphaUpdate(GX_TRUE);
  GXSetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
  GXSetCopyClear(m_clearColor, m_clearZ);
  GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
  return 1;
}

// 8033CA48
void ENgcRenderer::SetClearColor(EVec3& color, bool clear) {
  m_clearEnabled = clear;
  unsigned char rgb[3];
  color.ToU8s(rgb);
  m_clearColor = GXColor{rgb[0], rgb[1], rgb[2], 0x7f};
  m_clearZ = 0xffffff;
}

// ---- Frame commands -------------------------------------------------------------------------
// The GameCube queued these to the renderer; Queue* runs them at once on the calling (app) thread.

float _ngcBrightness = 1.0f;  // display brightness, 0..2
int _fps = 60;                // 804FF07C
int _ngcFrameCount = 0;
int _ngcVerts = 0, _ngcStrips = 0, _ngcStateChangesPerFrame = 0, _ngcDrawStats = 0, _ngcRenderEmpty = 1;
float _ngcAveStripLength = 0, _ngcAveClocksPerVert = 0;
int _vertsperframe = 0;
float _aveclockspervert = 0, _avestriplength = 0;

// PC stand-in for VIGetRetraceCount(): display retraces since the first call, at _tvHZ.
// (On the game's clock: SIMMY_DET makes it virtual.)
static int RetraceCount() {
  static const unsigned long long start = simmy::game_clock_ns();
  return static_cast<int>(static_cast<double>(simmy::game_clock_ns() - start) * 1e-9 * _tvHZ);
}

// 8033E7F4
void ENgcRenderer::QueueBeginFrame(int buffer) {
  ENgcRendCommand cmd;
  cmd.m_type = ENgcRendCommand::kBeginFrame;
  cmd.m_arg0 = buffer;
  BeginFrame(&cmd);
}

// 8033E858
void ENgcRenderer::QueueEndFrame(EEvent& done) {
  ENgcRendCommand cmd;
  cmd.m_type = ENgcRendCommand::kEndFrame;
  cmd.m_done = &done;
  EndFrame(&cmd);
}

// 8033E888
void ENgcRenderer::QueueSwap(int evenOdd, int minRetraces) {
  ENgcRendCommand cmd;
  cmd.m_type = ENgcRendCommand::kSwap;
  cmd.m_arg0 = evenOdd;
  cmd.m_arg1 = minRetraces;
  Swap(&cmd);
}

// 8033E824
void ENgcRenderer::QueueDisplayList(EDL* dl, bool release) {
  ENgcRendCommand cmd;
  cmd.m_type = ENgcRendCommand::kDisplayList;
  cmd.m_dl = dl;
  cmd.m_release = release;
  DisplayList(&cmd);
}

// 8033E94C
void ENgcRenderer::DisplayList(ENgcRendCommand* cmd) {
  Execute(cmd->m_dl->m_start);
  if (cmd->m_release) _pGfx->DeallocateDL(cmd->m_dl, RCMode::Immediate);
}

// 8033E8BC
void ENgcRenderer::Flush() {
  ENgcRendCommand cmd;
  cmd.m_type = 5;
  Flush(&cmd);
}

extern ENgcRenderer _ngcRenderer;

// 8033ECB8
void ENgcRenderer::Flush(ENgcRendCommand*) {
  // PC: aurora processes GX work only inside its frame (none headless or between frames).
  if (!simmy::platform_headless() && _ngcRenderer.m_pcFrameOpen) GXDrawDone();
}

// 8033E8E8
// PC: also opens the aurora frame that this frame's GX work is recorded into.
void ENgcRenderer::BeginFrame(ENgcRendCommand*) {
  _ngcRenderEmpty = 1;
  _ngcAveStripLength = 0;
  _ngcAveClocksPerVert = 0;
  _ngcStrips = 0;
  _ngcVerts = 0;
  _ngcStateChangesPerFrame = 0;
  _pGfx->m_renderedFrame = _pGfx->m_frameCounter;
  _ngcDrawStats = 0;
  if (!m_pcFrameOpen) m_pcFrameOpen = simmy::platform_begin_frame();
  if (m_pcFrameOpen) GXSetCopyClear(m_clearColor, m_clearZ);
}

// 8033E9AC
// GameCube: copies the EFB to a half-size texture (used by the next frame's motion blur), applies
// brightness through the copy filter, copies to the XFB and releases the frame event.
// PC: the half-size copy, the brightness clamp and the hand-off to aurora (the XFB copy and its
// render-state save/restore are aurora's present).
void ENgcRenderer::EndFrame(ENgcRendCommand* cmd) {
  if (m_pcFrameOpen) {
    // The frame at half size, box-filtered (the next frame's motion blur reads it).
    GXSetTexCopySrc(0, 0, m_renderMode.fbWidth, m_renderMode.efbHeight);
    GXSetTexCopyDst(m_renderMode.fbWidth >> 1, m_renderMode.efbHeight >> 1, GX_TF_RGB565, GX_TRUE);
    GXCopyTex(m_prevFrame, GX_FALSE);
  }
  if (_ngcBrightness < 0.0f) _ngcBrightness = 0.0f;
  else if (_ngcBrightness > 2.0f) _ngcBrightness = 2.0f;
  if (m_pcFrameOpen) {
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_TRUE);
    simmy::platform_end_frame();
    m_pcFrameOpen = false;
  }
  if (cmd->m_done) cmd->m_done->Release();
  m_geometryDirty = 1;
}

// 8033EC3C
void ENgcRenderer::Swap(ENgcRendCommand*) {
  // CycleToNextFrameBuffer (VISetNextFrameBuffer + VIWaitForRetrace) is aurora's present + the frame
  // pacing in simmy::platform_frame_tick.
  const int now = RetraceCount();
  _d_retraces = std::max(1, now - m_lastRetrace);
  _fps = _tvHZ / _d_retraces;
  m_lastRetrace = now;
  // CycleToNextFrameBuffer: the other external frame buffer is drawn next.
  m_xfbIndex = (m_xfbIndex + 1) % 2;
  m_xfbCurrent = m_xfb[m_xfbIndex];
  _vertsperframe = _ngcVerts;
  _aveclockspervert = _ngcAveClocksPerVert;
  _avestriplength = _ngcAveStripLength;
  ++_ngcFrameCount;
}

// ---- frame effects support (see src/engine/frameeffects.cpp) ----

// 80349F44
void* ENgcRenderer::GetNextExternalFrameBuffer() { return m_xfbCurrent; }

// 80349F4C
void* ENgcRenderer::GetPreviousFrameBuffer(int* width, int* height) {
  *width = m_renderMode.fbWidth >> 1;
  *height = m_renderMode.efbHeight >> 1;
  return m_prevFrame;
}

// 80349F6C
GXRenderModeObj ENgcRenderer::GetCurrentRenderMode() { return m_renderMode; }

// 80349FD4
void* ENgcRenderer::GetFrameEffectsScratchMemory() { return m_fxScratch; }
