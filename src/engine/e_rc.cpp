// Port of engine/e_rc.cpp (partial): the render context's display-list recording.
//
// Every command is bracketed by BeginCommand/EndCommand and writes one or more EDLEntry records with
// NewEntry. Blocks of kBlockEntries entries are chained with a Goto entry when one fills up. Geometry
// submission (TriStrip etc.), particles and the screen-effect commands are still stubbed.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>
#include <new>

// 802F64A8
ERC::ERC() {}
// 802F652C
ERC::~ERC() = default;

// 802F6580
void ERC::Init(RCMode mode) {
  m_mode = mode;
  m_dl = _pGfx->AllocDisplayList(mode);
  auto* block = static_cast<EDLEntry*>(AllocFlushable(kBlockEntries * sizeof(EDLEntry), 0x20));
  ++m_blockCount;
  m_dl->m_start = block;
  m_maxMatrix = -1;
  m_entriesLeft = kBlockEntries;
  m_next = block;
  m_commandType = 0;
  m_commandDone = 0;
  m_terminated = 0;
  m_openCount = 0;
}

// 802F6608
void ERC::BeginCommand(int type, int) {
  const int prev = m_commandType;
  m_commandType = type;
  if (prev == 1 && type != 1) FlushQueuedMatrices();
  if (type == 2 && prev != 2) GeometrySetup();
  ++m_openCount;
}

// 802F9B38
void ERC::EndCommand() {
  m_commandDone = 1;
  --m_openCount;
}

// 802F9B20
bool ERC::IsOpen() const { return m_openCount != 0; }

// 802F6698
// Immediate contexts: hands the finished list to the renderer and starts a new one. (The original
// carried the list's data allocations over to the new list; execution is synchronous here, so the old
// list is simply freed once it has run.)
void ERC::Send() {
  if (m_commandDone && !m_terminated) {
    Terminate();
    EDL* dl = m_dl;
    m_dl = nullptr;
    _pGfx->Execute(dl, true);
    Init(RCMode::Immediate);
  }
}

// 802F678C
EDLEntry* ERC::NewEntry(int count) {
  EDLEntry* e;
  if (count < m_entriesLeft) {
    e = m_next;
    m_entriesLeft -= count;
  } else {
    auto* block = static_cast<EDLEntry*>(AllocFlushable(kBlockEntries * sizeof(EDLEntry), 0x20));
    if (!block) {
      m_next = nullptr;
      return nullptr;
    }
    ++m_blockCount;
    Connect(m_next, block);
    e = block;
    m_entriesLeft = kBlockEntries - count;
  }
  for (int i = 0; i < count; ++i) new (&e[i]) EDLEntry();
  m_next = e + count;
  return e;
}

// 802F682C
void ERC::Connect(EDLEntry* from, EDLEntry* to) {
  from->p = to;
  from->op = kDL_Goto;
}

// 802F74C0
void ERC::Terminate() {
  BeginCommand(0, 0);
  NewEntry(1)->op = kDL_End;
  m_terminated = 1;
  m_dl->m_terminated = 1;
  m_dl->m_numMatrices = m_maxMatrix + 1;
  // ShrinkSmallDisplayList copied single-block deferred lists into an exact-size allocation; the
  // memory saving does not matter on PC.
  EndCommand();
}

// 802F98C0
void* ERC::Alloc(unsigned int size, int align) { return m_dl->Alloc(size, align); }
// 802F9908
void* ERC::AllocFlushable(unsigned int size, int align) { return m_dl->AllocFlushable(size, align); }
// 802F9958
void* ERC::AllocAndCopy(void* src, int size) {
  if (!src) return nullptr;
  void* dst = Alloc(static_cast<unsigned int>(size), 0x20);
  std::memcpy(dst, src, static_cast<std::size_t>(size));
  return dst;
}

namespace {
// The common shape: one entry with an opcode and a pointer to data copied into the list.
void PointerCommand(ERC* rc, unsigned char op, void* data) {
  rc->BeginCommand(0, 0);
  EDLEntry* e = rc->NewEntry(1);
  e->p = data;
  e->op = op;
  rc->EndCommand();
}
void ValueCommand(ERC* rc, unsigned char op, unsigned int value, unsigned char b1 = 0) {
  rc->BeginCommand(0, 0);
  EDLEntry* e = rc->NewEntry(1);
  e->u = value;
  e->op = op;
  e->b1 = b1;
  rc->EndCommand();
}
}  // namespace

// 802F8F60
// Switches rendering to an offscreen surface (null: back to the screen); see ENgcRenderer::RenderSurface.
void ERC::RenderSurface(ERenderSurface* surface, int output) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->p = surface;
  e->op = kDL_RenderSurface;
  e->b1 = static_cast<unsigned char>(output);
  EndCommand();
}

// 802F8B58
void ERC::GeometrySetup() {
  BeginCommand(0, 0);
  NewEntry(1)->op = kDL_GeometrySetup;
  EndCommand();
}

// ---- Matrices ------------------------------------------------------------------------------------
// Model matrices are queued and only written when the next non-matrix command begins.

// 802F7748
void ERC::ModelMatrixList(EMat4* matrices, int count) {
  BeginCommand(1, 0);
  void* dst = Alloc(static_cast<unsigned int>(count) << 6, 0x20);
  m_matListCount = count;
  m_matList = static_cast<EMat4*>(dst);
  std::memcpy(dst, matrices, static_cast<std::size_t>(count) << 6);
  for (int& i : m_matIndex) i = 0xff;
  m_indicesQueued = 1;
  m_listQueued = 1;
}

// 802F77F0
void ERC::ModelMatrixIndex(int slot, int index) {
  if (static_cast<unsigned int>(index) < 0xf8 && m_maxMatrix < index) m_maxMatrix = index;
  BeginCommand(1, 0);
  m_matIndex[slot] = index;
  m_indicesQueued = 1;
}

// 802F786C
void ERC::ModelMatrices(EMat4* matrices, int count) {
  ModelMatrixList(matrices, count);
  const int n = count < 4 ? count : 4;
  for (int i = 0; i < n; ++i) m_matIndex[i] = i;
  if (m_maxMatrix < count - 1) m_maxMatrix = count - 1;
  m_indicesQueued = 1;
}

// 802F7900
void ERC::ModelMatrixId() { ModelMatrixIndex(0, 0xfe); }

// 802F9A80
void ERC::ModelMatrix(EMat4* matrix) { ModelMatrices(matrix, 1); }

// 802F793C
void ERC::FlushQueuedMatrices() {
  if (m_listQueued) {
    m_listQueued = 0;
    if (m_matList) {
      EDLEntry* e = NewEntry(1);
      e->op = kDL_ModelMatrices;
      e->b1 = static_cast<unsigned char>(m_matListCount);
      e->u = static_cast<unsigned int>(m_matListCount);
      e->p = m_matList;
    }
  }
  if (m_indicesQueued) {
    m_indicesQueued = 0;
    EDLEntry* e = NewEntry(1);
    e->op = kDL_ModelMatrixIndices;
    for (int i = 0; i < 4; ++i) e->bytes[i] = static_cast<unsigned char>(m_matIndex[i]);
  }
  EndCommand();
}

// 802F7A44
void ERC::ViewMatrix(EMat4* m, int, float) { PointerCommand(this, kDL_ViewMatrix, AllocAndCopy(m, 0x40)); }
// 802F7AD8
void ERC::ProjectionMatrix(EMat4* m) { PointerCommand(this, kDL_ProjectionMatrix, AllocAndCopy(m, 0x40)); }
// 802F7B6C
void ERC::WindowMatrix(EMat4* m) { PointerCommand(this, kDL_WindowMatrix, AllocAndCopy(m, 0x40)); }

// ---- Viewport and clipping -----------------------------------------------------------------------

// 802F7578
void ERC::Viewport(EViewport* vp) { PointerCommand(this, kDL_Viewport, AllocAndCopy(vp, 0x20)); }

// 802F760C
void ERC::ClipRect(TRect<float>& rect) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(EDLEntry::EntriesFor<EDLClipRectData>());
  e->op = kDL_ClipRect;
  std::memcpy(e->Payload<EDLClipRectData>()->rect, &rect, sizeof(float) * 4);
  EndCommand();
}

// 802F76B4
void ERC::Scissor(TRect<float>* rect) { PointerCommand(this, kDL_Scissor, AllocAndCopy(rect, 0x10)); }

// ---- State ---------------------------------------------------------------------------------------

// 802F7C00
void ERC::Texture(ETexture* texture, int stage) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->op = kDL_Texture;
  e->b1 = static_cast<unsigned char>(stage);
  e->p = texture;
  if (texture) m_dl->m_textures.push_back(texture);
  EndCommand();
}

// 802F7CAC
void ERC::EnableGeometryModes(unsigned int modes) { ValueCommand(this, kDL_EnableGeometryModes, modes); }
// 802F7D38
void ERC::DisableGeometryModes(unsigned int modes) { ValueCommand(this, kDL_DisableGeometryModes, ~modes); }
// 802F7DC8
void ERC::SetGeometryModes(unsigned int modes) { ValueCommand(this, kDL_SetGeometryModes, modes); }
// 802F7E54
void ERC::EnableRasterModes(unsigned int modes, int pass) {
  ValueCommand(this, kDL_EnableRasterModes, modes, static_cast<unsigned char>(pass));
}
// 802F7EE8
void ERC::DisableRasterModes(unsigned int modes, int pass) {
  ValueCommand(this, kDL_DisableRasterModes, ~modes, static_cast<unsigned char>(pass));
}
// 802F7F80
void ERC::SetRasterModes(unsigned int modes, int pass) {
  ValueCommand(this, kDL_SetRasterModes, modes, static_cast<unsigned char>(pass));
}

// 802F8014
void ERC::SaveState() { ValueCommand(this, kDL_SaveState, 0); }
// 802F8098
void ERC::RestoreState() { ValueCommand(this, kDL_RestoreState, 0); }

// 802F811C
void ERC::Lights(ELights* lights) {
  auto* copy = static_cast<ELights*>(AllocAndCopy(lights, sizeof(ELights)));
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->p = copy;
  e->op = kDL_Lights;
  e->b1 = copy ? static_cast<unsigned char>(copy->m_numDir) : 0;
  e->b2 = copy ? static_cast<unsigned char>(copy->m_numPoint) : 0;
  e->b3 = 0;
  EndCommand();
}

// 802F81D8
void ERC::Material(EMaterial* material) { PointerCommand(this, kDL_Material, AllocAndCopy(material, 0x30)); }

// 802F8594
void ERC::Rect(EVec2& p0, EVec2& p1, EVec2& uv0, EVec2& uv1, EVec4& color, float z) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(EDLEntry::EntriesFor<EDLRectData>());
  e->op = kDL_Rect;
  *e->Payload<EDLRectData>() = {p0.x, p0.y, p1.x, p1.y, uv0.x, uv0.y, uv1.x, uv1.y,
                                {color.x, color.y, color.z, color.w}, z};
  EndCommand();
}

namespace {
// RectList/RectListRot: the rectangles are copied into the list (bytes per rectangle given).
void RectListCommand(ERC* rc, unsigned char op, int count, float* rects, int bytesPer, EVec4& color, float z) {
  if (count == 0) return;
  void* copy = rc->AllocAndCopy(rects, count * bytesPer);
  rc->BeginCommand(0, 0);
  EDLEntry* e = rc->NewEntry(EDLEntry::EntriesFor<EDLRectListData>());
  e->op = op;
  e->u = static_cast<unsigned short>(count);
  e->p = copy;
  *e->Payload<EDLRectListData>() = {{color.x, color.y, color.z, color.w}, z};
  rc->EndCommand();
}
}  // namespace

// 802F83D4
void ERC::RectList(int count, float* rects, EVec4& color, float z) {
  RectListCommand(this, kDL_RectList, count, rects, 8 * sizeof(float), color, z);
}
// 802F84B8
void ERC::RectListRot(int count, float* rects, EVec4& color, float z) {
  RectListCommand(this, kDL_RectListRot, count, rects, 16 * sizeof(float), color, z);
}

// 802F8BDC
void ERC::ZTest(bool enable, int compare, int noWrite, int pass) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->u = static_cast<unsigned int>(pass);
  e->op = kDL_ZTest;
  e->b1 = enable;
  e->b2 = static_cast<unsigned char>(compare);
  e->b3 = static_cast<unsigned char>(noWrite);
  EndCommand();
}

// 802F8C80
void ERC::AlphaTest(bool enable, int compare, float ref, int pass) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->u = static_cast<unsigned int>(pass);
  e->op = kDL_AlphaTest;
  e->b1 = enable;
  e->b2 = static_cast<unsigned char>(compare);
  e->b3 = static_cast<unsigned char>(static_cast<int>(ref * 255.0f));
  EndCommand();
}

// 802F8DE8
// a..d select the TEV blend inputs for layer 0 (two bits each); alpha is the constant alpha.
void ERC::SetBlendMode(int a, int b, int c, int d, float alpha, int pass) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->op = kDL_SetBlendMode;
  e->u = static_cast<unsigned int>(a) | b << 2 | c << 4 | d << 6 |
         (static_cast<unsigned int>(static_cast<int>(alpha * 255.0f)) & 0xffu) << 8;
  e->b2 = static_cast<unsigned char>(pass >> 8);
  e->b3 = static_cast<unsigned char>(pass);
  EndCommand();
}

// 802F8ECC
void ERC::SetCombineMode(int mode, int layer) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->u = static_cast<unsigned int>(mode);
  e->op = kDL_SetCombineMode;
  e->b2 = static_cast<unsigned char>(layer >> 8);
  e->b3 = static_cast<unsigned char>(layer);
  EndCommand();
}

// 802F9678
// Draws the movie's current frame (executed by ENgcRenderer::MovieFrame).
void ERC::MovieFrame(EMovie* movie) { PointerCommand(this, kDL_MovieFrame, movie); }

// 802F9704
void ERC::SetAlpha(float alpha) {
  BeginCommand(0, 0);
  EDLEntry* e = NewEntry(1);
  e->f = alpha;
  e->op = kDL_SetAlpha;
  EndCommand();
}

// ---- Nested lists --------------------------------------------------------------------------------

// 802F72F0
void ERC::AddDisplayListReference(EDL* dl) { m_dl->m_children.push_back(dl); }

// 802F7334
void ERC::DisplayList(EDL* dl) {
  if (dl && dl->m_numMatrices <= m_matListCount) {
    BeginCommand(0, 0);
    m_dl->m_vertCount += dl->m_vertCount;
    EDLEntry* e = NewEntry(1);
    e->p = dl;
    e->op = kDL_DisplayList;
    if (!dl->m_textures.empty() || !dl->m_children.empty()) AddDisplayListReference(dl);
    EndCommand();
  }
}

// 802F99EC
void ERC::SetName(const char* name) { m_name = name ? name : ""; }

// 802F8DE0
void ERC::SetColorModulation(int, EVec4) {}
// 802F8DE4
void ERC::EnableColorModulation(bool) {}

// 802F9798
// Empty on the GameCube.
void ERC::SetAlphaPlane(float, EVec4, float, float) {}
