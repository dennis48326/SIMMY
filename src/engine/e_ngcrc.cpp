// Port of engine/e_ngcrc.cpp (partial): the GameCube render context. State commands bypass the display
// list and call the renderer's handler with an entry built on the stack (in both modes, as in the
// original; deferred contexts are used for geometry). Geometry submission is still stubbed.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

extern ENgcRenderer _ngcRenderer;

namespace {
EDLEntry Entry(unsigned char op, unsigned int value = 0, void* ptr = nullptr) {
  EDLEntry e;
  e.op = op;
  e.u = value;
  e.p = ptr;
  return e;
}
}  // namespace

// 80338334
ENgcRC::ENgcRC() {}
// 80338370
ENgcRC::~ENgcRC() = default;

// 803383A0
void ENgcRC::Init(RCMode mode) { ERC::Init(mode); }

// 803383C0
void ENgcRC::EndCommand() {
  m_commandDone = 1;
  --m_openCount;
  if (m_mode == RCMode::Immediate && !m_dl->m_terminated) Send();
}

// 8033ABAC
void ENgcRC::GeometrySetup() {}

// 8033A220
void ENgcRC::Shader(ENgcShader* shader, unsigned int colorIndex) {
  if (shader->m_magic == 0x900dbeef) _ngcRenderer.SetShader(shader, colorIndex);
}

// 8033A268
void ENgcRC::ScrambleRect(EVec2& min, EVec2& max, float z) { _ngcRenderer.ScrambleRect(min, max, z); }

// 8033A290
void ENgcRC::ModelMatrixList(EMat4* matrices, int count) {
  m_matListCount = count;
  _ngcRenderer.ModelMatrices(matrices, count);
}

// 8033A2BC
void ENgcRC::ModelMatrixIndex(int slot, int index) {
  if (m_mode == RCMode::Deferred) {
    ERC::ModelMatrixIndex(slot, index);
  } else {
    if (static_cast<unsigned int>(index) < 0xf8 && m_maxMatrix < index) m_maxMatrix = index;
    _ngcRenderer.ModelMatrixIndex(slot, index);
  }
}

// 8033A310
void ENgcRC::ModelMatrices(EMat4* matrices, int count) {
  ModelMatrixList(matrices, count);
  const int n = count < 4 ? count : 4;
  for (int i = 0; i < n; ++i) ModelMatrixIndex(i, i);
  if (m_maxMatrix < count - 1) m_maxMatrix = count - 1;
}

// 8033A3B0
void ENgcRC::DisplayList(EDL* dl) {
  if (dl && dl->m_numMatrices <= m_matListCount) {
    m_dl->m_vertCount += dl->m_vertCount;
    if (!dl->m_textures.empty() || !dl->m_children.empty()) AddDisplayListReference(dl);
    _ngcRenderer.Execute(dl->m_start);
  }
}

// 8033A430
void ENgcRC::Texture(ETexture* texture, int stage) {
  if (!texture || texture->m_magic == 0x900dbeef)
    _ngcRenderer.SetTexture(static_cast<ENgcTexture*>(texture), stage);
}

// 8033A480
void ENgcRC::SetMipMap(float, int) {}

// 8033A484
void ENgcRC::EnableGeometryModes(unsigned int modes) {
  EDLEntry e = Entry(kDL_EnableGeometryModes, modes);
  _ngcRenderer.EnableGeometryModes(&e);
}
// 8033A4BC
void ENgcRC::DisableGeometryModes(unsigned int modes) {
  EDLEntry e = Entry(kDL_DisableGeometryModes, ~modes);
  _ngcRenderer.DisableGeometryModes(&e);
}
// 8033A4F8
void ENgcRC::SetGeometryModes(unsigned int modes) {
  EDLEntry e = Entry(kDL_SetGeometryModes, modes);
  _ngcRenderer.SetGeometryModes(&e);
}
// 8033A530
void ENgcRC::EnableRasterModes(unsigned int modes, int pass) {
  EDLEntry e = Entry(kDL_EnableRasterModes, modes);
  e.b1 = static_cast<unsigned char>(pass);
  _ngcRenderer.EnableRasterModes(&e);
}
// 8033A56C
void ENgcRC::DisableRasterModes(unsigned int modes, int pass) {
  EDLEntry e = Entry(kDL_DisableRasterModes, ~modes);
  e.b1 = static_cast<unsigned char>(pass);
  _ngcRenderer.DisableRasterModes(&e);
}
// 8033A5AC
void ENgcRC::SetRasterModes(unsigned int modes, int pass) {
  EDLEntry e = Entry(kDL_SetRasterModes, modes);
  e.b1 = static_cast<unsigned char>(pass);
  _ngcRenderer.SetRasterModes(&e);
}

// 8033A5E8
void ENgcRC::Viewport(EViewport* vp) {
  EDLEntry e = Entry(kDL_Viewport, 0, vp);
  _ngcRenderer.Viewport(&e);
}

// 8033A620
void ENgcRC::ClipRect(TRect<float>& rect) {
  struct {
    EDLEntry e;
    EDLClipRectData data;
  } cmd;
  cmd.e.op = kDL_ClipRect;
  std::memcpy(cmd.data.rect, &rect, sizeof cmd.data.rect);
  _ngcRenderer.ClipRect(&cmd.e);
}

// 8033A678
void ENgcRC::Scissor(TRect<float>* rect) {
  EDLEntry e = Entry(kDL_Scissor, 0, rect);
  _ngcRenderer.Scissor(&e);
}

// 8033A6B0
void ENgcRC::ViewMatrix(EMat4* m, int, float) {
  EDLEntry e = Entry(kDL_ViewMatrix, 0, m);
  _ngcRenderer.ViewMatrix(&e);
}
// 8033A6E8
void ENgcRC::ProjectionMatrix(EMat4* m) {
  EDLEntry e = Entry(kDL_ProjectionMatrix, 0, m);
  _ngcRenderer.ProjectionMatrix(&e);
}
// 8033A720
void ENgcRC::WindowMatrix(EMat4* m) {
  EDLEntry e = Entry(kDL_WindowMatrix, 0, m);
  _ngcRenderer.WindowMatrix(&e);
}

// 8033A758
void ENgcRC::SaveState() {
  EDLEntry e = Entry(kDL_SaveState);
  _ngcRenderer.SaveState(&e);
}
// 8033A78C
void ENgcRC::RestoreState() {
  EDLEntry e = Entry(kDL_RestoreState);
  _ngcRenderer.RestoreState(&e);
}

// 8033A7C0
void ENgcRC::Lights(ELights* lights) {
  EDLEntry e = Entry(kDL_Lights, 0, lights);
  e.b1 = lights ? static_cast<unsigned char>(lights->m_numDir) : 0;
  e.b2 = lights ? static_cast<unsigned char>(lights->m_numPoint) : 0;
  _ngcRenderer.Lights(&e);
}

// 8033A81C
void ENgcRC::Material(EMaterial* material) {
  EDLEntry e = Entry(kDL_Material, 0, material);
  _ngcRenderer.Material(&e);
}

// 8033A854
void ENgcRC::Rect(EVec2& p0, EVec2& p1, EVec2& uv0, EVec2& uv1, EVec4& color, float z) {
  struct {
    EDLEntry e;
    EDLRectData data;
  } cmd;
  cmd.e.op = kDL_Rect;
  cmd.data = {p0.x, p0.y, p1.x, p1.y, uv0.x, uv0.y, uv1.x, uv1.y, {color.x, color.y, color.z, color.w}, z};
  _ngcRenderer.Rect(&cmd.e);
}

// 8033A8F0
// Drawn at once in both modes; the renderer reads the caller's rectangles in place.
void ENgcRC::RectList(int count, float* rects, EVec4& color, float z) {
  if (count == 0) return;
  struct {
    EDLEntry e;
    EDLRectListData data;
  } cmd;
  cmd.e.op = kDL_RectList;
  cmd.e.u = static_cast<unsigned short>(count);
  cmd.e.p = rects;
  cmd.data = {{color.x, color.y, color.z, color.w}, z};
  _ngcRenderer.RectList(&cmd.e);
}

// 8033A958
void ENgcRC::RectListRot(int count, float* rects, EVec4& color, float z) {
  if (count == 0) return;
  struct {
    EDLEntry e;
    EDLRectListData data;
  } cmd;
  cmd.e.op = kDL_RectListRot;
  cmd.e.u = static_cast<unsigned short>(count);
  cmd.e.p = rects;
  cmd.data = {{color.x, color.y, color.z, color.w}, z};
  _ngcRenderer.RectListRot(&cmd.e);
}

// 8033ABB0
void ENgcRC::ZTest(bool enable, int compare, int noWrite, int pass) {
  EDLEntry e = Entry(kDL_ZTest, static_cast<unsigned int>(pass));
  e.b1 = enable;
  e.b2 = static_cast<unsigned char>(compare);
  e.b3 = static_cast<unsigned char>(noWrite);
  _ngcRenderer.ZTest(&e);
}

// 8033AE4C
void ENgcRC::AlphaTest(bool enable, int compare, float ref, int pass) {
  float a = ref * 255.0f;
  if (a < 0.0f) a = 0.0f;
  else if (a > 255.0f) a = 255.0f;
  EDLEntry e = Entry(kDL_AlphaTest, static_cast<unsigned int>(pass));
  e.b1 = enable;
  e.b2 = static_cast<unsigned char>(compare);
  e.b3 = static_cast<unsigned char>(static_cast<int>(a));
  _ngcRenderer.AlphaTest(&e);
}

// 8033AF08
void ENgcRC::SetBlendMode(int a, int b, int c, int d, float alpha, int pass) {
  EDLEntry e = Entry(kDL_SetBlendMode, static_cast<unsigned int>(a) | b << 2 | c << 4 | d << 6 |
                                           (static_cast<unsigned int>(static_cast<int>(alpha * 255.0f)) & 0xffu) << 8);
  e.b2 = static_cast<unsigned char>(pass >> 8);
  e.b3 = static_cast<unsigned char>(pass);
  _ngcRenderer.SetBlendMode(&e);
}

// 8033AF80
void ENgcRC::SetCombineMode(int mode, int layer) {
  EDLEntry e = Entry(kDL_SetCombineMode, static_cast<unsigned int>(mode));
  e.b2 = static_cast<unsigned char>(layer >> 8);
  e.b3 = static_cast<unsigned char>(layer);
  _ngcRenderer.SetCombineMode(&e);
}

// 8033AFBC
void ENgcRC::SetAlpha(float alpha) { _ngcRenderer.SetAlpha(alpha); }

// 8033AFE4
void ENgcRC::RenderSurface(ERenderSurface* surface, int output) {
  EDLEntry e = Entry(kDL_RenderSurface, 0, surface);
  e.b1 = static_cast<unsigned char>(output);
  _ngcRenderer.RenderSurface(&e);
}

// ---- Model strips --------------------------------------------------------------------------------

namespace {
// A strip stored as a GX display list over vertex arrays (models' indexed strips). Recorded into the
// list in both modes: a deferred context keeps it (ESubModelShader's display lists), an immediate one
// executes it in EndCommand. The arrays and the display list are referenced in place.
void GXDisplayListCommand(ENgcRC* rc, unsigned char op, unsigned char* dl, int dlSize, int count,
                          const void* positions, const void* uvs, unsigned char* colors, signed char* normals,
                          bool twoUVs) {
  if (dlSize < 0) return;
  rc->BeginCommand(2, 0);
  rc->m_dl->m_vertCount += count;
  EDLEntry* e = rc->NewEntry(EDLEntry::EntriesFor<EDLGXDisplayListData>());
  e->u = static_cast<unsigned int>(count);
  e->op = op;
  e->b1 = twoUVs ? 1 : 0;
  *e->Payload<EDLGXDisplayListData>() = {positions, uvs, colors, normals, dl, static_cast<unsigned int>(dlSize)};
  rc->EndCommand();
}
}  // namespace

// 80339B58
// GC: also flushed the arrays from the CPU data cache (DCStoreRangeNoSync).
void ENgcRC::TriStripDLRef(unsigned char* dl, int dlSize, int count, float* positions, float* uvs,
                           unsigned char* colors, signed char* normals, bool twoUVs) {
  GXDisplayListCommand(this, kDL_NgcGXDisplayList, dl, dlSize, count, positions, uvs, colors, normals, twoUVs);
}

// 80339E38
void ENgcRC::TriStripDLRef(unsigned char* dl, int dlSize, int count, short* positions, short* uvs,
                           unsigned char* colors, signed char* normals, bool twoUVs) {
  GXDisplayListCommand(this, kDL_NgcGXDisplayListInt, dl, dlSize, count, positions, uvs, colors, normals, twoUVs);
}

// 8033ABF4
// Sets depth to `z` inside the rectangle (normalised screen coordinates) without touching colour:
// the default shader, depth test always with writes, blending that keeps the frame buffer, no
// textures. Afterwards the usual alpha test (> 0.5) and depth test (less-equal) are restored.
RET_UNKNOWN ENgcRC::ZClear(float x0, float y0, float x1, float y1, float z) {
  static_cast<ENgcGraphics*>(_pGfx)->m_defaultShader->Select(this, 0);
  AlphaTest(true, 1, 0.5f, 0);
  ZTest(true, 1, 0, 0);
  SetRasterModes(0x40, 0);
  SetBlendMode(2, 2, 2, 1, 0.0f, 0);
  SetCombineMode(0, 0);
  Texture(nullptr, 0);
  Texture(nullptr, 1);
  EVec2 p0{x0, y0}, p1{x1, y1}, uv0{0, 0}, uv1{1, 1};
  EVec4 white{1, 1, 1, 1};
  Rect(p0, p1, uv0, uv1, white, z);
  AlphaTest(true, 5, 0.5f, 0);
  ZTest(true, 2, 0, 0);
  return 0;
}

extern int _ngcUseGXDisplayList;

// 803387D8
// A float strip kept by reference (the arrays must outlive the display list).
RET_UNKNOWN ENgcRC::TriStripRef(int count, float* positions, float* uvs, unsigned char* colors, signed char* normals,
                                unsigned char* weights, bool twoUVs) {
  if (count == 0) return 0;
  BeginCommand(2, 0);
  m_dl->m_vertCount += count;
  EDLEntry* e = NewEntry(EDLEntry::EntriesFor<EDLTriStripPackedData>());
  e->u = static_cast<unsigned short>(count);
  e->op = kDL_TriStripPacked;
  e->b1 = static_cast<unsigned char>(twoUVs ? 1 : 0);
  *e->Payload<EDLTriStripPackedData>() = {positions, uvs, colors, normals, weights, nullptr, 0};
  EndCommand();
  return 0;
}

// 803391C4
// A plain fixed-point strip over vertex arrays referenced in place (models' non-indexed strips). A
// GX display list for it is built now and kept with the list (ENgcRenderer::TriStripPackedInt).
// GC: also flushed the arrays from the CPU data cache.
RET_UNKNOWN ENgcRC::TriStripRef(int count, short* positions, short* uvs, unsigned char* colors, signed char* normals,
                                unsigned char* weights, bool twoUVs) {
  if (count == 0) return 0;
  unsigned int dlSize = 0;
  unsigned char* gxdl = nullptr;
  if (_ngcUseGXDisplayList)
    gxdl = _ngcRenderer.CreateGXDisplayList(m_dl, GX_TRIANGLESTRIP, count, positions, uvs, colors, normals, dlSize,
                                            twoUVs, weights != nullptr);
  BeginCommand(2, 0);
  m_dl->m_vertCount += count;
  EDLEntry* e = NewEntry(EDLEntry::EntriesFor<EDLTriStripPackedData>());
  e->u = static_cast<unsigned short>(count);
  e->op = kDL_TriStripPackedInt;
  e->b1 = static_cast<unsigned char>((twoUVs ? 1 : 0) | (weights ? 2 : 0));
  *e->Payload<EDLTriStripPackedData>() = {positions, uvs, colors, normals, weights, gxdl, dlSize};
  EndCommand();
  return 0;
}

// 803398CC
// Records GX's texture cache invalidation (after textures were written by the CPU or a copy).
void ENgcRC::TextureCacheInvalidate() {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->op = kDL_NgcTexInvalidateAll;
  EndCommand();
}
