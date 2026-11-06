// Port of game/aptauxpceorgl.cpp: the Apt callbacks the game supplies (AptAuxPCEorGL). Movies are
// loaded from the "flashes" archive, shapes are drawn as triangles through the render context with
// the current Apt matrix and colour transform, bitmaps become shaders, and edit-text strings are
// laid out and drawn with the game font.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>

extern ENgcRenderer _ngcRenderer;

int iUseStencil = 0;  // 8047622C (AptViewer::Draw clears it)
ERFont* gpFont = nullptr;  // 804FD95C the Apt font (AptAuxPCEorGL_Initialize); used by aptstring.cpp

namespace {
constexpr int kTextureSlots = 0x15E;  // 350
constexpr int kLevels = 48;

AptAuxLayerInfo gLevelInfo[kLevels];
int gTextureSlots[kTextureSlots];
ERTexture* gpTextures[kTextureSlots];
EShader* gpShaders[kTextureSlots];
float pWXH[kTextureSlots][2];

EMat4 vertexMatrix;       // the Apt matrix of what is drawn (setVertexMatrix)
float colourScale[4] = {1, 1, 1, 1};      // A R G B
float colourTranslate[4] = {0, 0, 0, 0};  // A R G B
ERC* gPrc = nullptr;
int _clampAptColorsAt128 = 0;

UIObjectBase* gpUiObjectList[10];

// A loaded edit-text string (allocStrings entries, 0x28 bytes on GC).
struct AllocString {
  int inUse = 0;                    // +0x00
  unsigned int colour = 0;          // +0x04 ARGB
  AptRect bounds;                   // +0x08
  int alignment = 0;                // +0x18
  float fontHeight = 0;             // +0x1C
  std::string source;               // +0x20 as given by Apt (UTF-8, or "qd:"... references)
  std::unique_ptr<AptParagraph> paragraph;  // +0x24
};
AllocString allocStrings[250];
AllocString defaultAllocString;

struct PendingLoad {
  std::string path;
  AptFile* file = nullptr;
  AptAuxLayerInfo* level = nullptr;
  void* apt = nullptr;
  unsigned int aptSize = 0;
  void* constData = nullptr;
  unsigned int constSize = 0;
};
std::vector<PendingLoad> aFinishedLoads;

int gbFinishedFlashInit = 0;
const char* gszFlashInitFile = nullptr;

// _aptTriangleUVidx (804235E0): which uv rect corners the six vertices of a quad's two triangles use.
const int kTriangleUVIndex[12] = {0, 3, 0, 1, 2, 1, 2, 1, 2, 3, 0, 3};

unsigned int BE32(const unsigned char* p) {
  return static_cast<unsigned int>(p[0]) << 24 | static_cast<unsigned int>(p[1]) << 16 |
         static_cast<unsigned int>(p[2]) << 8 | p[3];
}
float BEF(const unsigned char* p) {
  const unsigned int u = BE32(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
}  // namespace

static AptViewer* TheAptViewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
extern bool s_traceOps;

// 800063F8
unsigned int GetGameFontID() { return 0x67f664fb; }

// 80006570
void* SimsAptMalloc(unsigned int size) { return std::malloc(size); }  // GC: AptHeap
// 800065A8
void SimsAptFree(void* p) { std::free(p); }

// 800065F0
void command(char*, char*) {}

// 800065F4
// While a movie is starting (AptAuxPCEorGL_WaitOn), only its "<name>_ready" ends the wait; after
// that every variable goes to the registered UI objects (UIRouter and friends).
void setExternVariable(char* name, char* value) {
  if (!gszFlashInitFile) {
    if (name[0] == '_') {
      if (std::strcmp("_debug", name) == 0) return;
      if (std::strcmp("_r", name) == 0) {
        TheAptViewer()->AllowControllerReading(true);
        AptFlushInputQueue();
        return;
      }
      if (std::strcmp("_f", name) == 0) {
        AptFlushInputQueue();
        return;
      }
    }
    for (int i = 9; i >= 0; --i)
      if (UIObjectBase* o = gpUiObjectList[i]) o->SetVariable(name, value);
    return;
  }
  std::string ready = std::string(gszFlashInitFile) + "_ready";
  if (ready != name && std::strcmp("root_ready", name) != 0 && std::strcmp("s2c_root_ready", name) != 0 &&
      std::strcmp("S2C_root_ready", name) != 0)
    return;
  gbFinishedFlashInit = 1;
  gszFlashInitFile = nullptr;
  TheAptViewer()->AllowControllerReading(true);
  AptFlushInputQueue();
}

// 80006778
AptVar loadVariables(char*) { return AptVar::Str("company=E%20A&game=sims&function=loadVariables"); }
// 800067A0
AptVar loadVariablesNULL() { return AptVar::Str("loadVariablesNULL"); }
// 800067E8
float getStageWidth() { return 640.0f; }
// 800067F4
float getStageHeight() { return 448.0f; }

// 80006800
// "_platform", "_build", "_sku", "_lang", "_enable_debug" are answered here; anything else is
// asked of the UI objects. Unknown names come back as themselves.
AptVar getExternVariable(char* name) {
  std::string answer;
  bool have = false;
  if (name[0] == '_') {
    if (std::strcmp("_platform", name) == 0) answer = "ngc", have = true;
    else if (std::strcmp("_build", name) == 0) answer = "release", have = true;
    else if (std::strcmp("_sku", name) == 0) answer = "usa", have = true;
    else if (std::strcmp("_lang", name) == 0) answer = "en", have = true;
    else if (std::strcmp("_enable_debug", name) == 0) answer = "0", have = true;
  }
  if (!have) {
    for (int i = 9; i >= 0 && !have; --i)
      if (UIObjectBase* o = gpUiObjectList[i])
        if (char* r = o->GetVariable(name)) {
          answer = r;
          SimsAptFree(r);
          have = true;
        }
  }
  return AptVar::Str(have ? answer : std::string(name));
}

// 80006B7C
// Reads <name>.geo into native units and maps each shape character to them (AptAuxIDToChar 1).
void _loadGeometry(char* name, AptAuxLayerInfo* level) {
  std::string file = std::string(name) + ".geo";
  int index = 0, offset = 0, size = 0;
  if (!level->big || !BIG_locateentry(level->big->m_header, const_cast<char*>(file.c_str()), &index, &offset, &size)) return;
  const unsigned char* g = reinterpret_cast<const unsigned char*>(level->big->GetGeometry(const_cast<char*>(file.c_str())));
  if (!g) return;
  auto geo = std::make_unique<AptAuxGeometry>();
  const unsigned int shapeCount = BE32(g + 0x18), shapes = BE32(g + 0x1C);
  const unsigned int mapCount = BE32(g + 0x20), map = BE32(g + 0x24), slots = BE32(g + 0x28);
  int maxTexture = 0;
  for (unsigned int i = 0; i < mapCount; ++i) {
    const int ch = static_cast<int>(BE32(g + map + 8 * i)), tex = static_cast<int>(BE32(g + map + 8 * i + 4));
    geo->textureMap.emplace_back(ch, tex);
    maxTexture = std::max(maxTexture, tex);
  }
  (void)slots;
  geo->textureSlots.assign(static_cast<size_t>(maxTexture) + 1, -1);
  for (unsigned int s = 0; s < shapeCount; ++s) {
    const unsigned char* e = g + shapes + 12 * s;
    auto shape = std::make_unique<AptAuxShape>();
    shape->character = static_cast<int>(BE32(e));
    const unsigned int unitCount = BE32(e + 4), unitList = BE32(e + 8);
    for (unsigned int u = 0; u < unitCount; ++u) {
      const unsigned char* src = g + BE32(g + unitList + 4 * u);
      auto unit = std::make_unique<AptAuxUnit>();
      unit->type = static_cast<int>(BE32(src));
      unit->count = static_cast<int>(BE32(src + 4));
      const unsigned int data = BE32(src + 8);
      const int floats = unit->type == AptAuxUnit::kLineSolid ? unit->count * 4 : unit->count * 6;
      unit->points.resize(static_cast<size_t>(std::max(0, floats)));
      for (int k = 0; k < floats && data; ++k) unit->points[static_cast<size_t>(k)] = BEF(g + data + 4 * k);
      unit->colour = BE32(src + 0xC);
      for (int k = 0; k < 16; ++k) unit->texMatrix[k] = BEF(src + 0x10 + 4 * k);
      unit->textureCharacter = static_cast<int>(BE32(src + 0x50));
      unit->textureSlot = -1;
      for (int k = 0; k < 4; ++k) unit->uv[k] = BEF(src + 0x58 + 4 * k);
      shape->units.push_back(unit.get());
      geo->unitStore.push_back(std::move(unit));
    }
    const size_t ch = static_cast<size_t>(std::max(0, shape->character));
    if (level->idToChar.size() <= ch) level->idToChar.resize(ch + 1);
    level->idToChar[ch].kind = 1;
    level->idToChar[ch].shape = shape.get();
    geo->shapes.push_back(std::move(shape));
  }
  level->geometry = std::move(geo);
}

// 80006D98
// A bitmap character's texture: the NNN.tgq its geometry map names, shared by every bitmap of that
// file; each gets a gTextureSlots entry with an ERTexture and a one-layer shader.
int loadTexture(void* levelp, int character) {
  auto* level = static_cast<AptAuxLayerInfo*>(levelp);
  AptAuxGeometry* geo = level->geometry.get();
  int textureNumber = character;
  int* cached = nullptr;
  if (geo)
    for (auto& m : geo->textureMap)
      if (m.first == character) {
        textureNumber = m.second;
        cached = &geo->textureSlots[static_cast<size_t>(textureNumber)];
        if (*cached != -1) return *cached;
        break;
      }
  int slot = 0;
  while (slot < kTextureSlots && gTextureSlots[slot] != -1) ++slot;
  if (slot >= kTextureSlots) return -1;
  gTextureSlots[slot] = 1;
  char name[32];
  std::snprintf(name, sizeof(name), "%03d.tgq", textureNumber);
  int w = 0, h = 0;
  ERTexture* tex = level->big->GetTexture(name, w, h);
  pWXH[slot][0] = static_cast<float>(w);
  pWXH[slot][1] = static_cast<float>(h);
  gpTextures[slot] = tex;
  EShaderDef def;
  def.m_layerCount = 1;
  def.m_geometryModes = 0;
  def.m_layers[0].m_texture = tex ? tex->m_texture : nullptr;
  def.m_layers[0].m_field0C = 0x50;
  def.m_layers[0].m_field21 = 0;
  gpShaders[slot] = _pGfx->CreateShader(def);
  if (cached) *cached = slot;
  return slot;
}

// 80007168
void freeTexture(void* slotp) {
  const int slot = static_cast<int>(reinterpret_cast<intptr_t>(slotp));
  if (slot < 0 || slot >= kTextureSlots) return;
  if (gpShaders[slot]) {
    _pGfx->Destroy(gpShaders[slot]);
    gpShaders[slot] = nullptr;
  }
  if (gpTextures[slot]) {
    // PC: the movie's texture goes with its FlashBigFile (AptReleaseUnusedFiles); the renderer must not
    // keep it as a stage's current texture (SetTexture unlocks the previous one).
    for (int stage = 0; stage < 2; ++stage)
      if (gpTextures[slot]->m_texture && _ngcRenderer.m_textures[stage] == gpTextures[slot]->m_texture)
        _ngcRenderer.SetTexture(nullptr, stage);
    gpTextures[slot]->DelRef();
    gpTextures[slot] = nullptr;
  }
  gTextureSlots[slot] = -1;
}

// 800071F8
// Records the bitmap's slot and points every textured unit that uses the bitmap at it.
void bindTexture(void* levelp, int character, int slot) {
  auto* level = static_cast<AptAuxLayerInfo*>(levelp);
  if (level->idToChar.size() <= static_cast<size_t>(character)) level->idToChar.resize(static_cast<size_t>(character) + 1);
  level->idToChar[static_cast<size_t>(character)].kind = 2;
  level->idToChar[static_cast<size_t>(character)].slot = slot;
  for (AptAuxIDToChar& e : level->idToChar) {
    if (e.kind != 1 || !e.shape) continue;
    for (AptAuxUnit* u : e.shape->units)
      if ((u->type == AptAuxUnit::kTriTiled || u->type == AptAuxUnit::kTriTiledClipped) && u->textureCharacter == character)
        u->textureSlot = slot;
  }
}

// 80007360
void setColourTransform(const AptCXForm* c) {
  colourScale[0] = c->mulA, colourScale[1] = c->mulR, colourScale[2] = c->mulG, colourScale[3] = c->mulB;
  colourTranslate[0] = c->addA, colourTranslate[1] = c->addR, colourTranslate[2] = c->addG, colourTranslate[3] = c->addB;
}

// 800073B8
void AptAuxPCEorGL_VertexMatrixId() { vertexMatrix.Id(); }

// 800073E0
// The 2D Apt matrix goes into the x/y rows of a 4x4 (row vectors: x' = a x + c y + tx).
void setVertexMatrix(const AptMatrix* m) {
  vertexMatrix.m[0][0] = m->a;
  vertexMatrix.m[0][1] = m->b;
  vertexMatrix.m[1][0] = m->c;
  vertexMatrix.m[1][1] = m->d;
  vertexMatrix.m[3][0] = m->tx;
  vertexMatrix.m[3][1] = m->ty;
}

// No Flash sound or background colour in this game.
// 8000741C
void* loadSound(void*, int, char*) { return nullptr; }
// 80007424
void startSoundStream(void*, int) {}
// 80007428
void startSound(void*, char*) {}
// 8000742C
void freeSound(void*) {}
// 80007430
void setBackgroundColour(unsigned int) {}
// 80007434
void myassert(char*, char*, unsigned int) {}

// 80007438
void* loadRenderingUnit(void* levelp, int character) {
  auto* level = static_cast<AptAuxLayerInfo*>(levelp);
  if (static_cast<size_t>(character) >= level->idToChar.size()) return nullptr;
  return level->idToChar[static_cast<size_t>(character)].shape;
}
// 8000744C
void freeRenderingUnit(void*) {}

// 80007450
void drawRenderingUnit(void* shapep, int maskOp) {
  auto* shape = static_cast<AptAuxShape*>(shapep);
  if (!shape) return;
  for (AptAuxUnit* u : shape->units) {
    if (u->type == AptAuxUnit::kTriSolid) DrawTriSolid(u, maskOp);
    else if (u->type == AptAuxUnit::kTriTiled || u->type == AptAuxUnit::kTriTiledClipped) DrawTriTiledClipped(u, maskOp);
    else if (u->type == AptAuxUnit::kLineSolid) DrawLineSolid(u, maskOp);
  }
}

// 800074F8
// A custom-rendered clip (its "_type"): CCR2D draws the UI2D shader named like the clip over the
// clip's rectangle shape (CCRMA / CCRMS add it to / take it from the mask), CCR3D a 3D display object
// (not ported) and CCRCB the user draw callback installed under the clip's name (UIUserDrawCBTable).
int iUseStencilCCR = 0;  // 80476230
void customControlRender(const char* type, const char* target, void* unitp, const char*) {
  auto* shape = static_cast<AptAuxShape*>(unitp);
  if (!shape) return;
  const char* name = target;
  for (const char* p = target; *p; ++p)
    if (*p == '/') name = p + 1;
  AptViewer* viewer = TheAptViewer();
  if (!viewer || !gPrc) return;
  auto clamp01 = [](float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); };
  // The rectangle: the bounds of the shape's points (two triangles on GC).
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool first = true;
  for (AptAuxUnit* u : shape->units)
    for (size_t i = 0; i + 1 < u->points.size(); i += 2) {
      const float x = u->points[i], y = u->points[i + 1];
      if (first) x0 = x1 = x, y0 = y1 = y, first = false;
      x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
    }
  if (first) return;
  const bool maskAdd = std::strcmp(type, "CCRMA") == 0, maskSub = std::strcmp(type, "CCRMS") == 0;
  if (std::strcmp(type, "CCR2D") != 0 && !maskAdd && !maskSub) {
    if (std::strcmp(type, "CCR3D") == 0) {
      // The display named like the clip, in the clip's box: its corners through the Apt matrix and
      // the viewer's normalized projection (0..1 of the screen).
      if (CUnlockDisplay* d = UI3D::Instance()->GetDisplayObject(const_cast<char*>(name))) {
        const EMat4& m = vertexMatrix;
        const EMat4& np = viewer->m_normalizedProjection;
        auto toScreen = [&](float x, float y, float& sx, float& sy) {
          const float px = x * m.m[0][0] + y * m.m[1][0] + m.m[3][0];
          const float py = x * m.m[0][1] + y * m.m[1][1] + m.m[3][1];
          const float pz = x * m.m[0][2] + y * m.m[1][2] + m.m[3][2];
          sx = pz * np.m[2][0] + px * np.m[0][0] + py * np.m[1][0] + np.m[3][0];
          sy = pz * np.m[2][1] + px * np.m[0][1] + py * np.m[1][1] + np.m[3][1];
        };
        float left, top, right, bottom;
        toScreen(x0, y0, left, top);
        toScreen(x1, y1, right, bottom);
        d->SetDisplayBoxTop(top);
        d->SetDisplayBoxLeft(left);
        d->SetDisplayBoxBottom(bottom);
        d->SetDisplayBoxRight(right);
        d->Draw(gPrc);
      }
    } else if (std::strcmp(type, "CCRCB") == 0) {
      // The callback installed under the control's name gets its corners through the Apt matrix and
      // the clamped colour transform.
      const EMat4& m = vertexMatrix;
      EVec2 p0{x0 * m.m[0][0] + y0 * m.m[1][0] + m.m[3][0], x0 * m.m[0][1] + y0 * m.m[1][1] + m.m[3][1]};
      EVec2 p1{x1 * m.m[0][0] + y1 * m.m[1][0] + m.m[3][0], x1 * m.m[0][1] + y1 * m.m[1][1] + m.m[3][1]};
      EVec4 scale{clamp01(colourScale[1]), clamp01(colourScale[2]), clamp01(colourScale[3]), clamp01(colourScale[0])};
      EVec4 translate{clamp01(colourTranslate[1]), clamp01(colourTranslate[2]), clamp01(colourTranslate[3]),
                      clamp01(colourTranslate[0])};
      structDrawCBparams params;
      params.topLeft = &p0;
      params.bottomRight = &p1;
      params.scale = &scale;
      params.translate = &translate;
      UIUserDrawCBTable::s_drawCBparams = &params;
      UIUserDrawCBTable::Instance()->ExecuteCallback(gPrc, name);
      UIUserDrawCBTable::s_drawCBparams = nullptr;
    }
    return;
  }
  ERShader* r = UI2D::Instance()->GetShader(name);
  EShader* shader = r ? r->GetShader() : nullptr;
  if (!shader) return;
  float z = 0.5f;
  if (maskAdd) {
    gPrc->Stencil(2, 0, 1);
    shader->Select(gPrc, 0);
    gPrc->ZTest(true, 1, 0, 0);
    z = 0.25f;
    ++iUseStencilCCR;
  } else if (maskSub) {
    gPrc->Stencil(2, 0, 1);
    shader->Select(gPrc, 0);
    gPrc->ZTest(true, 1, 0, 0);
    --iUseStencilCCR;
  } else if (iUseStencil == 0 && iUseStencilCCR == 0) {
    gPrc->Stencil(1, 0, 0);
    shader->Select(gPrc, 0);
  } else {
    gPrc->Stencil(1, 0, 1);
    shader->Select(gPrc, 0);
    if (iUseStencil) gPrc->ZTest(true, 5, 1, 0);
    else if (iUseStencilCCR) gPrc->ZTest(true, 4, 1, 0);
    z = 0.25f;
  }
  EVec4 colour{clamp01(colourScale[1]) + clamp01(colourTranslate[1]), clamp01(colourScale[2]) + clamp01(colourTranslate[2]),
               clamp01(colourScale[3]) + clamp01(colourTranslate[3]), clamp01(colourScale[0]) + clamp01(colourTranslate[0])};
  if (!maskAdd && !maskSub && iUseStencilCCR == 0 && iUseStencil == 0) gPrc->ZTest(false, 2, 0, 0);
  // Centre the rectangle on the origin and move the matrix's translation to its centre.
  EMat4 m = vertexMatrix;
  const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
  m.m[3][0] += m.m[0][0] * cx + m.m[1][0] * cy;
  m.m[3][1] += m.m[0][1] * cx + m.m[1][1] * cy;
  const float hw = (x1 - x0) * 0.5f, hh = (y1 - y0) * 0.5f;
  // Stage pixels to the 0..1 rectangle space through the viewer's normalized projection.
  const EMat4& np = viewer->m_normalizedProjection;
  const float sx = np.m[0][0], sy = np.m[1][1];
  float rect[16] = {-hw, -hh, hw, hh, 0.0f, 1.0f, 1.0f, 0.0f,
                    m.m[0][0] * sx, m.m[0][1] * sy, m.m[3][0] * sx + np.m[3][0], 0.0f,
                    m.m[1][0] * sx, m.m[1][1] * sy, m.m[3][1] * sy + np.m[3][1], 0.0f};
  gPrc->RectListRot(1, rect, colour, z);
  // PC widescreen: full-stage art has its outer texel columns drawn over the side bars (pc_hud.cpp).
  const float extend = maskAdd || maskSub ? 0.0f : simmy_apt_hud_extend();
  if (extend > 0.0f && m.m[0][0] != 0.0f) {
    const float e = extend / std::fabs(m.m[0][0]) * 0.5f;  // half the strip, in the clip's units
    for (int side = 0; side < 2; ++side) {
      const float c = side == 0 ? -hw - e : hw + e, u = side == 0 ? 0.001f : 0.999f;
      float strip[16] = {-e, -hh, e, hh, u, 1.0f, u, 0.0f,
                         m.m[0][0] * sx, m.m[0][1] * sy, (m.m[3][0] + m.m[0][0] * c) * sx + np.m[3][0], 0.0f,
                         m.m[1][0] * sx, m.m[1][1] * sy, (m.m[3][1] + m.m[0][1] * c) * sy + np.m[3][1], 0.0f};
      gPrc->RectListRot(1, strip, colour, z);
    }
  }
}

// 8000ADA0
int UTF8ToUCS2(const char* src, int count, unsigned short* dst) {
  int n = 0, i = 0;
  while (i < count && src[i]) {
    const unsigned char c = static_cast<unsigned char>(src[i]);
    unsigned int cp;
    if (c < 0x80) cp = c, i += 1;
    else if ((c & 0xE0) == 0xC0 && i + 1 < count) cp = (c & 0x1Fu) << 6 | (src[i + 1] & 0x3F), i += 2;
    else if ((c & 0xF0) == 0xE0 && i + 2 < count) cp = (c & 0x0Fu) << 12 | (src[i + 1] & 0x3Fu) << 6 | (src[i + 2] & 0x3F), i += 3;
    else cp = c, i += 1;
    dst[n++] = static_cast<unsigned short>(cp);
  }
  dst[n] = 0;
  return n;
}

// 8000ACE4
int UCS2ToUTF8(const unsigned short* src, int count, char* dst) {
  int n = 0;
  for (int i = 0; i < count && src[i]; ++i) {
    const unsigned int c = src[i];
    if (c < 0x80) dst[n++] = static_cast<char>(c);
    else if (c < 0x800) dst[n++] = static_cast<char>(0xC0 | c >> 6), dst[n++] = static_cast<char>(0x80 | (c & 0x3F));
    else
      dst[n++] = static_cast<char>(0xE0 | c >> 12), dst[n++] = static_cast<char>(0x80 | (c >> 6 & 0x3F)),
      dst[n++] = static_cast<char>(0x80 | (c & 0x3F));
  }
  dst[n] = 0;
  return n;
}

// AptParagraph::Build + GetParagraphHeight (8000FC68) as game code uses it to size boxes to text:
// the text from (0, 0) in `width` at `fontSize`, left aligned, white.
float AptAuxParagraphHeight(const unsigned short* text, float width, float fontSize) {
  if (!gpFont || !text) return 0.0f;
  AptParagraph p;
  EVec2 topLeft{0.0f, 0.0f}, bottomRight{width, 0.0f};
  EVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
  p.Build(topLeft, bottomRight, fontSize, white, 0, text);
  return p.GetParagraphHeight();
}

// 80008300
// Takes a free string slot (else the default one, freeing what it held), resolves the text
// ("qd:Table::name" quickdata strings, "uidb:s:" database strings, "getloc:" localizable commands,
// else UTF-8; tokens substituted in the first and the last) and builds its AptParagraph in the
// field's box, font height, colour and alignment. Apt gets the line count and the size (as parts of
// 640x448).
void* allocateString(AptAllocateStringParameters* p) {
  AllocString* s = nullptr;
  for (AllocString& a : allocStrings)
    if (!a.inUse) {
      s = &a;
      break;
    }
  if (!s) {
    s = &defaultAllocString;
    if (s->inUse) *s = AllocString();
  }
  s->inUse = 1;
  s->bounds = p->bounds;
  s->alignment = p->alignment;
  s->colour = p->colour;
  s->fontHeight = p->fontHeight;
  s->source = p->text && *p->text ? p->text : " ";
  if (!s->paragraph) {
    std::vector<unsigned short> buf(0x1000, 0);
    if (s->source.compare(0, 3, "qd:") == 0) {
      if (const unsigned short* t = UIQDTarget::GetQuickdataText(const_cast<char*>(s->source.c_str()))) {
        for (int i = 0; t[i] && i < 0xfff; ++i) buf[static_cast<size_t>(i)] = t[i];
        UIQDTarget::UIQDSubstitutionStrings(buf.data());
      }
    } else if (s->source.compare(0, 7, "uidb:s:") == 0) {
      if (const unsigned short* t = UIDB::UIDBGetString(const_cast<char*>(s->source.c_str() + 7)))
        for (int i = 0; t[i] && i < 0xfff; ++i) buf[static_cast<size_t>(i)] = t[i];
    } else if (s->source.compare(0, 7, "getloc:") == 0) {
      char* name = const_cast<char*>(s->source.c_str() + 7);
      if (unsigned short* t = GetLocalizableCommandTable::Instance()->ExecuteCommand(name, name)) {
        for (int i = 0; t[i] && i < 0xfff; ++i) buf[static_cast<size_t>(i)] = t[i];
        SimsAptFree(t);
      }
    } else {
      UTF8ToUCS2(s->source.c_str(), static_cast<int>(s->source.size()) + 1, buf.data());
      UIQDTarget::UIQDSubstitutionStrings(buf.data());
    }
    if (s_traceOps) {
      std::string utf8(0x3001, '\0');
      UCS2ToUTF8(buf.data(), 0x1000, &utf8[0]);
      std::fprintf(stderr, "[apt text] %s -> \"%s\"\n", s->source.c_str(), utf8.c_str());
    }
    const unsigned int c = s->colour;
    EVec4 colour{static_cast<float>(c >> 16 & 0xff) / 255.0f, static_cast<float>(c >> 8 & 0xff) / 255.0f,
                 static_cast<float>(c & 0xff) / 255.0f, 1.0f};
    EVec2 topLeft{s->bounds.left, s->bounds.top}, bottomRight{s->bounds.right, s->bounds.bottom};
    s->paragraph = std::make_unique<AptParagraph>();
    if (gpFont) s->paragraph->Build(topLeft, bottomRight, s->fontHeight, colour, s->alignment, buf.data());
    int lines = 0;
    for (AptLine* l = s->paragraph->m_first; l; l = l->m_next) ++lines;
    p->lineCount = lines;
    p->sizeX = s->paragraph->m_width / 640.0f;
    p->sizeY = s->paragraph->m_height / 448.0f;
  }
  return s;
}

// The measured size of a field's text (AptCharacterTextInst textWidth / textHeight; GC: kept from
// allocateString's sizes).
void AptMeasureText(AptCIH* field, float& w, float& h) {
  auto* s = static_cast<AllocString*>(field->m_textHandle);
  w = s && s->paragraph ? s->paragraph->m_width : 0.0f;
  h = s && s->paragraph ? s->paragraph->m_height : 0.0f;
}

// 80008A1C
// The field's colour times the Apt colour transform, then its paragraph through the Apt matrix (with
// the transform for the formats' alpha), without depth testing or inside the current stencil mask.
void drawStringEor(void* handle, int maskOp) {
  auto* s = static_cast<AllocString*>(handle);
  if (!gpFont || !s) return;
  auto clamp01 = [](float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); };
  EVec4 scale{clamp01(colourScale[1]), clamp01(colourScale[2]), clamp01(colourScale[3]), clamp01(colourScale[0])};
  EVec4 translate{clamp01(colourTranslate[1]), clamp01(colourTranslate[2]), clamp01(colourTranslate[3]),
                  clamp01(colourTranslate[0])};
  const unsigned int c = s->colour;
  gpFont->Select(gPrc);
  gpFont->m_color = EVec4{static_cast<float>(c >> 16 & 0xff) / 255.0f * scale.x + translate.x,
                          static_cast<float>(c >> 8 & 0xff) / 255.0f * scale.y + translate.y,
                          static_cast<float>(c & 0xff) / 255.0f * scale.z + translate.z, scale.w + translate.w};
  if (!s->paragraph) {
    // No paragraph: the text centred in the box (normalised to the screen).
    const float w = static_cast<float>(_pGfx->m_screenWidth), h = static_cast<float>(_pGfx->m_screenHeight);
    EVec2 centre{(s->bounds.left + s->bounds.right) * 0.5f / w + vertexMatrix.m[3][0] / w,
                 (s->bounds.top + s->bounds.bottom) * 0.5f / h + vertexMatrix.m[3][1] / h};
    std::vector<unsigned short> text(s->source.size() + 1, 0);
    UTF8ToUCS2(s->source.c_str(), static_cast<int>(s->source.size()) + 1, text.data());
    gpFont->DoDrawAlign(gPrc, text.data(), true, centre, EFontAlignX::Center, EFontAlignY::Center, nullptr, nullptr,
                        EFontMatrixType::Scale, nullptr, false);
    return;
  }
  if (iUseStencil == 0) {
    gPrc->Stencil(1, 0, 0);
    gPrc->ZTest(false, 2, 0, 0);
  } else {
    gPrc->Stencil(1, 2, 0);
    gPrc->ZTest(true, 5, 1, 0);
  }
  EMat4 m = vertexMatrix;
  gpFont->SetSize(s->paragraph->m_fontSize, 1.0f, true);
  s->paragraph->Draw(gPrc, &m, &scale, &translate);
  (void)maskOp;
}

// 8000936C
void deallocateString(void* handle, unsigned int) {
  auto* s = static_cast<AllocString*>(handle);
  if (!s) return;
  s->inUse = 0;
  s->source.clear();
  s->paragraph.reset();
}

// 80009420
void hardwareInit(float, float) {}

// 80009424
// Loads the movie's FLSH resource, takes its files, finds <name>.apt and <name>.const, reads the
// geometry and queues the load for AptAuxPCEorGL_FinishAsyncLoads.
int LoadThreadProc(void* p) {
  auto* load = static_cast<PendingLoad*>(p);
  std::string name = load->path;
  const size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) name = name.substr(slash + 1);
  const size_t dot = name.rfind('.');
  if (dot != std::string::npos) name = name.substr(0, dot);
  auto* flash = static_cast<ERFlash*>(_flashman.AddRef(const_cast<char*>(name.c_str()), nullptr, 0));
  if (!flash) {
    std::fprintf(stderr, "[apt] flash movie %s not found\n", name.c_str());
    return 0;
  }
  load->level->big = flash->GetFlashBigData();
  flash->DelRef();
  if (!load->level->big) return 0;
  auto section = [&](const char* ext, unsigned int& size) -> void* {
    std::string n = name + ext;
    int index = 0, offset = 0, sz = 0;
    if (!BIG_locateentry(load->level->big->m_header, const_cast<char*>(n.c_str()), &index, &offset, &sz)) return nullptr;
    size = static_cast<unsigned int>(sz);
    return load->level->big->m_entries[index];
  };
  load->apt = section(".apt", load->aptSize);
  load->level->aptData = load->apt;
  load->constData = section(".const", load->constSize);
  _loadGeometry(const_cast<char*>(name.c_str()), load->level);
  aFinishedLoads.push_back(*load);
  return 0;
}

// 80009594
// Loads at once (GC: on the load thread), completes, then tells the movie through
// handleSWFLoadComplete(name).
void AptAuxPCEorGL_LoadAnimation(char* path, AptFile* file) {
  AptFlushInputQueue();
  PendingLoad load;
  load.path = path;
  load.file = file;
  for (int i = 0; i < kLevels; ++i)
    if (!gLevelInfo[i].inUse) {
      gLevelInfo[i].inUse = 1;
      load.level = &gLevelInfo[i];
      break;
    }
  if (!load.level) {
    std::fprintf(stderr, "[apt] no free movie level for %s\n", path);
    static int dumped = 0;
    if (dumped++ < 2) AptDumpFiles();
    return;
  }
  LoadThreadProc(&load);
  // PC: a movie that could not be read gives its level back (GC: the load thread's failure path).
  if (aFinishedLoads.empty() || aFinishedLoads.back().level != load.level) AptAuxPCEorGL_FreeAnimation(load.level);
  AptAuxPCEorGL_FinishAsyncLoads();
  const char* args[1] = {path};
  AptCallFunction("handleSWFLoadComplete", nullptr, nullptr, 1, args);
}

// 800096B8
void AptAuxPCEorGL_FinishAsyncLoads() {
  std::vector<PendingLoad> loads;
  loads.swap(aFinishedLoads);
  for (PendingLoad& l : loads)
    AptCompleteAnimationAsyncLoad(l.file, l.apt, l.aptSize, l.constData, l.constSize, l.level);
}

// 800097C0
// The constant table is a block of the movie's FlashBigFile (freed with the level).
void AptAuxPCEorGL_FreeConstantTable(void*) {}

// 800097C4
// The movie's level goes back: its FlashBigFile (the .apt/.const/.geo blocks), geometry and
// character map.
void AptAuxPCEorGL_FreeAnimation(void* levelp) {
  auto* level = static_cast<AptAuxLayerInfo*>(levelp);
  if (!level) return;
  delete level->big;
  *level = AptAuxLayerInfo();
}

// 80009918
// Installs the callbacks, resets the transforms and slots and references the game font.
void AptAuxPCEorGL_Initialize(int width, int height, AptAuxFontList*) {
  gAptFuncs.malloc = SimsAptMalloc;
  gAptFuncs.free = SimsAptFree;
  gAptFuncs.setExternVariable = setExternVariable;
  gAptFuncs.getExternVariable = getExternVariable;
  gAptFuncs.loadTexture = loadTexture;
  gAptFuncs.freeTexture = freeTexture;
  gAptFuncs.loadSound = loadSound;
  gAptFuncs.freeSound = freeSound;
  gAptFuncs.startSound = startSound;
  gAptFuncs.loadRenderingUnit = loadRenderingUnit;
  gAptFuncs.loadAnimation = AptAuxPCEorGL_LoadAnimation;
  gAptFuncs.freeAnimation = AptAuxPCEorGL_FreeAnimation;
  gAptFuncs.freeConstantTable = AptAuxPCEorGL_FreeConstantTable;
  gAptFuncs.drawRenderingUnit = drawRenderingUnit;
  gAptFuncs.freeRenderingUnit = freeRenderingUnit;
  gAptFuncs.setColourTransform = setColourTransform;
  gAptFuncs.setVertexMatrix = setVertexMatrix;
  gAptFuncs.bindTexture = bindTexture;
  gAptFuncs.command = command;
  gAptFuncs.loadVariables = loadVariables;
  gAptFuncs.setBackgroundColour = setBackgroundColour;
  gAptFuncs.customControlRender = customControlRender;
  gAptFuncs.drawString = drawStringEor;
  gAptFuncs.getStageHeight = getStageHeight;
  gAptFuncs.deallocateString = deallocateString;
  gAptFuncs.getStageWidth = getStageWidth;
  gAptFuncs.allocateString = allocateString;
  for (int i = 0; i < 4; ++i) colourScale[i] = 1.0f, colourTranslate[i] = 0.0f;
  vertexMatrix.Id();
  hardwareInit(static_cast<float>(width), static_cast<float>(height));
  if (!gpFont && _fontman.IsValid(GetGameFontID()))
    gpFont = static_cast<ERFont*>(_fontman.AddRef(GetGameFontID(), nullptr, 0));
  for (AllocString& s : allocStrings) s = AllocString();
  aFinishedLoads.clear();
  for (AptAuxLayerInfo& l : gLevelInfo) l = AptAuxLayerInfo();
  std::memset(gpUiObjectList, 0, sizeof(gpUiObjectList));
  for (int i = 0; i < kTextureSlots; ++i) {
    pWXH[i][0] = pWXH[i][1] = 0;
    gTextureSlots[i] = -1;
    gpTextures[i] = nullptr;
    gpShaders[i] = nullptr;
  }
  PSystemCreate(&g_pTextParticleSystem, "textpsys");
}

// 80009CF8
void AptAuxPCEorGL_Shutdown() {
  for (AllocString& s : allocStrings) s = AllocString();
  aFinishedLoads.clear();
  for (AptAuxLayerInfo& l : gLevelInfo) l = AptAuxLayerInfo();
  std::memset(gpUiObjectList, 0, sizeof(gpUiObjectList));
  if (gpFont) gpFont->DelRef();
  gpFont = nullptr;
  PSystemDestroy(&g_pTextParticleSystem);
}

// 80009DC4
// Vertex colour of a unit under the Apt colour transform: rgb scaled to 0..128 (the GC TEV doubles
// it), alpha to 0..255.
void GetScaledVertexColor(int* out, unsigned int c) {
  float a = (static_cast<float>(c >> 24) / 255.0f * colourScale[0] + colourTranslate[0]) * 255.0f;
  float r = (static_cast<float>(c >> 16 & 0xff) / 255.0f * colourScale[1] + colourTranslate[1]) * 128.0f;
  float g = (static_cast<float>(c >> 8 & 0xff) / 255.0f * colourScale[2] + colourTranslate[2]) * 128.0f;
  float b = (static_cast<float>(c & 0xff) / 255.0f * colourScale[3] + colourTranslate[3]) * 128.0f;
  auto lo = [](float& f) {
    if (f <= 0.0f) f = 0.0f;
  };
  lo(a), lo(r), lo(g), lo(b);
  const float cap = _clampAptColorsAt128 ? 128.0f : 255.0f;
  auto hi = [cap](float& f) {
    if (f >= cap) f = cap;
  };
  hi(a), hi(r), hi(g), hi(b);
  out[0] = static_cast<int>(r);
  out[1] = static_cast<int>(g);
  out[2] = static_cast<int>(b);
  out[3] = static_cast<int>(a);
}

// 8000A054
void DrawLineSolid(AptAuxUnit* u, int maskOp) {
  gPrc->ModelMatrices(&vertexMatrix, 1);
  if (maskOp == 1) {
    if (iUseStencil == 0) {
      gPrc->Stencil(2, 0, 0);
      gPrc->Texture(nullptr, 0);
      EVec2 p0{0, 0}, p1{1, 1}, uv0{0, 0}, uv1{0, 0};
      EVec4 c{0, 0, 0, 0};
      gPrc->Rect(p0, p1, uv0, uv1, c, 1.0f);
    }
    gPrc->Stencil(2, 0, 1);
    ++iUseStencil;
  } else if (maskOp == -1) {
    gPrc->Stencil(2, 0, 0);
    --iUseStencil;
  } else if (iUseStencil == 0) {
    gPrc->Stencil(1, 0, 0);
  } else {
    gPrc->Stencil(1, 2, 0);
  }
  gPrc->ZTest(false, 2, 0, 0);
  int colour[4];
  GetScaledVertexColor(colour, u->colour);
  int remaining = u->count, first = 0;
  EGEVert verts[40];
  while (remaining > 0) {
    const int n = remaining > 20 ? 20 : remaining;
    for (int i = 0; i < n * 2; ++i) {
      EGEVert& v = verts[i];
      v = EGEVert();
      v.pos[0] = u->points[static_cast<size_t>((first * 2 + i) * 2)];
      v.pos[1] = u->points[static_cast<size_t>((first * 2 + i) * 2 + 1)];
      v.pos[2] = 30.0f;
      for (int k = 0; k < 4; ++k) v.color[k] = colour[k];
    }
    gPrc->LineList(verts, n * 2);
    remaining -= n;
    first += n;
  }
  gPrc->ZTest(true, 2, 0, 0);
  gPrc->Stencil(1, iUseStencil == 0 ? 0 : 2, 0);
}

// 8000A418
// Solid triangles. Mask operations write the mask (alpha) instead of colour: 1 adds the shape,
// -1 takes it away again.
void DrawTriSolid(AptAuxUnit* u, int maskOp) {
  gPrc->ModelMatrices(&vertexMatrix, 1);
  float z = 20.0f;
  if (maskOp == 1) {
    gPrc->ZTest(true, 1, 0, 0);
    z = 10.0f;
    gPrc->Stencil(2, 0, 1);
    gPrc->ZClear(0, 0, 1, 1, 1.0f);
    ++iUseStencil;
  } else if (maskOp == -1) {
    gPrc->ZTest(true, 1, 0, 0);
    gPrc->Stencil(2, 0, 1);
    --iUseStencil;
  } else if (iUseStencil == 0) {
    gPrc->ZTest(false, 2, 0, 0);
  } else {
    gPrc->ZTest(true, 4, 1, 0);
    z = 10.0f;
  }
  int colour[4];
  GetScaledVertexColor(colour, u->colour);
  EGEVert verts[3];
  for (int t = 0; t < u->count; ++t) {
    for (int k = 0; k < 3; ++k) {
      EGEVert& v = verts[k];
      v = EGEVert();
      v.pos[0] = u->points[static_cast<size_t>(t * 6 + k * 2)];
      v.pos[1] = u->points[static_cast<size_t>(t * 6 + k * 2 + 1)];
      v.pos[2] = z;
      for (int c = 0; c < 4; ++c) v.color[c] = colour[c];
    }
    gPrc->TriStrip(verts, 3);
  }
  gPrc->Stencil(1, iUseStencil == 0 ? 0 : 2, 0);
  gPrc->ZTest(true, 2, 0, 0);
}

// 8000A768
// Bitmap triangles (quads in pairs): uvs from the bitmap's rectangle in its texture by
// kTriangleUVIndex, drawn with the bitmap's shader. Mask passes draw them as solid shapes.
void DrawTriTiledClipped(AptAuxUnit* u, int maskOp) {
  if (maskOp == 1 || maskOp == -1) {
    DrawTriSolid(u, maskOp);
    return;
  }
  const int slot = u->textureSlot;
  if (slot < 0 || slot >= kTextureSlots || !gpShaders[slot]) return;
  gPrc->ModelMatrices(&vertexMatrix, 1);
  gpShaders[slot]->Select(gPrc, 0);
  float z = 20.0f;
  if (iUseStencil == 0) {
    gPrc->Stencil(0, 3, 0);
    gPrc->ZTest(false, 2, 0, 0);
  } else {
    gPrc->ZTest(true, 4, 1, 0);
    z = 10.0f;
  }
  int colour[4];
  GetScaledVertexColor(colour, u->colour);
  EGEVert verts[3];
  int uvIndex = 0;
  for (int t = 0; t < u->count; ++t) {
    for (int k = 0; k < 3; ++k) {
      EGEVert& v = verts[k];
      v = EGEVert();
      v.pos[0] = u->points[static_cast<size_t>(t * 6 + k * 2)];
      v.pos[1] = u->points[static_cast<size_t>(t * 6 + k * 2 + 1)];
      v.pos[2] = z;
      v.uv[0] = u->uv[kTriangleUVIndex[uvIndex * 2]];
      v.uv[1] = u->uv[kTriangleUVIndex[uvIndex * 2 + 1]];
      ++uvIndex;
      for (int c = 0; c < 4; ++c) v.color[c] = colour[c];
    }
    if (uvIndex > 5) uvIndex = 0;
    gPrc->TriStrip(verts, 3);
  }
  // PC widescreen: full-stage art has its outer texel columns drawn over the side bars (pc_hud.cpp):
  // a strip on each side with the uvs of the shape's vertices on that edge.
  if (const float extend = simmy_apt_hud_extend(); extend > 0.0f && vertexMatrix.m[0][0] != 0.0f && !u->points.empty()) {
    const size_t n = std::min(u->points.size() / 2, static_cast<size_t>(u->count) * 3);
    float x0 = u->points[0], x1 = x0;
    for (size_t i = 0; i < n; ++i) x0 = std::min(x0, u->points[i * 2]), x1 = std::max(x1, u->points[i * 2]);
    const float e = extend / std::fabs(vertexMatrix.m[0][0]);
    for (int side = 0; side < 2; ++side) {
      const float edge = side == 0 ? x0 : x1, eps = 0.01f * (x1 - x0);
      // The edge's top and bottom vertices and their uvs.
      int top = -1, bottom = -1;
      for (size_t i = 0; i < n; ++i) {
        if (std::fabs(u->points[i * 2] - edge) > eps) continue;
        if (top < 0 || u->points[i * 2 + 1] < u->points[static_cast<size_t>(top) * 2 + 1]) top = static_cast<int>(i);
        if (bottom < 0 || u->points[i * 2 + 1] > u->points[static_cast<size_t>(bottom) * 2 + 1]) bottom = static_cast<int>(i);
      }
      if (top < 0 || bottom < 0 || top == bottom) continue;
      const int ends[2] = {top, bottom};
      EGEVert strip[4];
      for (int k = 0; k < 4; ++k) {
        const int vtx = ends[k >> 1];
        EGEVert& v = strip[k];
        v = EGEVert();
        const float out = side == 0 ? edge - e : edge + e;
        v.pos[0] = (k & 1) ? out : edge;
        v.pos[1] = u->points[static_cast<size_t>(vtx) * 2 + 1];
        v.pos[2] = z;
        const int uv = (vtx % 6) * 2;  // kTriangleUVIndex runs over two triangles' corners
        v.uv[0] = u->uv[kTriangleUVIndex[uv]];
        v.uv[1] = u->uv[kTriangleUVIndex[uv + 1]];
        for (int c = 0; c < 4; ++c) v.color[c] = colour[c];
      }
      gPrc->TriStrip(strip, 4);
    }
  }
  gPrc->ZTest(true, 2, 0, 0);
}

// 8000AA14
void AptAuxPCEorGL_SetRC(ERC* rc) { gPrc = rc; }

// 8000AA1C
// Adds a message handler at the end of the list; returns the one before it.
UIObjectBase* AptAuxPCEorGL_SetUiObject(UIObjectBase* obj) {
  if (!obj) return nullptr;
  AptAuxPCEorGL_RemoveUiObject(obj);
  int i = 0;
  while (i < 10 && gpUiObjectList[i]) ++i;
  if (i < 10) gpUiObjectList[i] = obj;
  return i > 0 ? gpUiObjectList[i - 1] : nullptr;
}

// 8000AABC
int AptAuxPCEorGL_RemoveUiObject(UIObjectBase* obj) {
  int i = 0;
  while (i < 10 && gpUiObjectList[i] != obj) ++i;
  if (i >= 10) return 0;
  for (; i < 9; ++i) gpUiObjectList[i] = gpUiObjectList[i + 1];
  gpUiObjectList[9] = nullptr;
  return 1;
}

// 8000AB48
// Runs the player until the movie reports "<name>_ready".
void AptAuxPCEorGL_WaitOn(char* name) {
  gbFinishedFlashInit = 0;
  gszFlashInitFile = name;
  for (int i = 0; !gbFinishedFlashInit && i < 200000; ++i) AptUpdate(1);
  if (!gbFinishedFlashInit) std::fprintf(stderr, "[apt] %s never reported ready\n", name);
  gszFlashInitFile = nullptr;
}

// 8000AB8C
int AptAuxPCEorGL_LoadFile(char* name) {
  std::string path = "data/flash/";
  path += name;
  AptLoadAnimation(path.c_str(), "_level0");
  return 1;
}
