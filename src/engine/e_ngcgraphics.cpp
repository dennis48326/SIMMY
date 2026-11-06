// Port of engine/e_ngcgraphics.cpp (partial): the platform graphics object and its init.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

EGraphics* _pGfx = nullptr;          // 804FF208
static ENgcGraphics _ngcGraphics;    // global constructor keyed to _pGfx
ENgcRenderer _ngcRenderer;           // engine/e_ngcrenderer.cpp's renderer object

// 8032F32C
ENgcGraphics::ENgcGraphics() { _pGfx = this; }

// 8032F36C
ENgcGraphics::~ENgcGraphics() = default;

// 8032F39C
RET_UNKNOWN ENgcGraphics::Init() {
  if (!EGraphics::Init()) return 0;
  if (!_ngcRenderer.Init(m_screenWidth, m_screenHeight)) return 0;
  // The default shader: one untextured layer (vertex color).
  EShaderDef def;
  def.m_geometryModes = 0;
  EShaderLayer& layer = def.m_layers[0];
  layer.m_field0C = 0x40;
  layer.m_flags = 0x10;
  layer.m_field14 = 0.0f;
  layer.m_blend[0] = 2;
  layer.m_blend[1] = 2;
  layer.m_blend[2] = 2;
  layer.m_blend[3] = 1;
  layer.m_alpha = 0;
  m_defaultShader = CreateShader(def);
  return 1;
}

// 8032F5EC
void ENgcGraphics::SetBackgroundColor(EVec3& color, int alpha, bool clear) {
  EGraphics::SetBackgroundColor(color, alpha, clear);
  _ngcRenderer.SetClearColor(color, clear);
}

int _ngcPerfStatsEnabled = 0;  // GX performance counters overlay (debug)

// 8032F994
// The debug build could overlay GX performance counters here (_ngcPerfStatsEnabled); that needs the
// GX perf API, which aurora does not have.
void ENgcGraphics::EndFrame() { EGraphics::EndFrame(); }

// 8032F7DC
ETexture* ENgcGraphics::AllocTexture() { return new ENgcTexture; }

// 8032F810
void ENgcGraphics::FreeTexture(ETexture* texture) { delete texture; }

// 8032F850
EShader* ENgcGraphics::AllocShader() { return new ENgcShader; }

// 8032F884
void ENgcGraphics::FreeShader(EShader* shader) { delete shader; }

// 8032F634
void ENgcGraphics::Destroy(ERenderSurface* surface) {
  static_cast<ENgcRenderSurface*>(surface)->DestroySubtextures();
  EGraphics::Destroy(surface);
}

// 8032F8C4
ERenderSurface* ENgcGraphics::AllocRenderSurface() { return new ENgcRenderSurface; }

// 8032F8EC
void ENgcGraphics::FreeRenderSurface(ERenderSurface* surface) { delete surface; }

// 8032F92C
EMovie* ENgcGraphics::AllocMovie() { return new ENgcMovie; }

// 8032F954
void ENgcGraphics::FreeMovie(EMovie* movie) { delete movie; }

// 8032F674
EDL* ENgcGraphics::AllocDL(RCMode) { return new EDL(0x400); }
// 8032F6BC
void ENgcGraphics::FreeDL(EDL* dl, RCMode) { delete dl; }

// 8032F728
// Deferred contexts are allocated per use; the immediate one is a single static object.
ERC* ENgcGraphics::AllocRC(RCMode mode) {
  if (mode == RCMode::Deferred) return new ENgcRC;
  static ENgcRC s_immediateRC;  // _ngcImmediateRC.2862
  return &s_immediateRC;
}
// 8032F790
void ENgcGraphics::FreeRC(ERC* rc, RCMode mode) {
  if (mode == RCMode::Deferred) delete rc;
}

// 8032FCA4
// The frame's censor rectangles, each scrambled into a mosaic at its depth (ERC vtable 0x344:
// ENgcRC::ScrambleRect), then emptied.
void ENgcGraphics::DrawCensorRects(ERC* rc) {
  for (int i = 0; i < m_censorCount; ++i) {
    EVec2 min{m_censorMin[i].x, m_censorMin[i].y}, max{m_censorMax[i].x, m_censorMax[i].y};
    static_cast<ENgcRC*>(rc)->ScrambleRect(min, max, m_censorMax[i].z);
  }
  m_censorCount = 0;
}
