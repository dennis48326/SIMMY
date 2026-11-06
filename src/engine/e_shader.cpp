// Port of engine/e_shader.cpp (partial): the shader definition and its accessors. The animated-shader
// update list, material changes and transforms come with the renderer.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// PC: SHDR data is CopyShedData's byte image of an EShaderDef, big-endian.
const unsigned char* EShaderDef::ReadBE(const unsigned char* p) {
  m_layerCount = p[0];
  m_field01 = p[1];
  m_field02 = simmy::be16(p + 2);
  m_surface = simmy::be32(p + 4);
  m_geometryModes = simmy::be32(p + 8);
  m_field0C = simmy::be32(p + 0xC);
  for (int i = 0; i < 12; ++i) m_material[i] = simmy::befloat(p + 0x10 + 4 * i);
  for (int i = 0; i < 9; ++i) m_field40[i] = static_cast<int>(simmy::be32(p + 0x40 + 4 * i));
  for (int l = 0; l < 2; ++l) {
    const unsigned char* q = p + 0x64 + 0x40 * l;
    EShaderLayer& L = m_layers[l];
    // The texture slot holds a texture id; ERShader::DoLoad turns it into a flag, then a texture.
    L.m_texture = reinterpret_cast<ETexture*>(static_cast<uintptr_t>(simmy::be32(q)));
    L.m_multiTextures = nullptr;  // CopyShedData clears both pointer slots
    L.m_extraData = nullptr;
    L.m_field0C = simmy::be32(q + 0xC);
    L.m_flags = simmy::be32(q + 0x10);
    L.m_field14 = simmy::befloat(q + 0x14);
    L.m_field18 = simmy::be32(q + 0x18);
    for (int i = 0; i < 4; ++i) L.m_blend[i] = q[0x1C + i];
    L.m_alpha = q[0x20];
    L.m_field21 = q[0x21];
    L.m_texGen = q[0x22];
    L.m_field23 = q[0x23];
    L.m_multiTextureCount = q[0x24];
    L.m_field25 = q[0x25];
    L.m_extraDataSize = simmy::be16(q + 0x26);
    L.m_scrollU = simmy::befloat(q + 0x28);
    L.m_scrollV = simmy::befloat(q + 0x2C);
    for (int i = 0; i < 2; ++i) {
      L.m_texScale[i] = simmy::befloat(q + 0x30 + 4 * i);
      L.m_texOffset[i] = simmy::befloat(q + 0x38 + 4 * i);
    }
  }
  return p + kFileSize;
}

// 802F9BA8
EShader::EShader() {}
// 802F9D68
EShader::~EShader() {
  RemoveFromUpdateList();
  m_magic = 0xdeadbeef;
}

// 802F9DC8
int EShader::Create(EShaderDef& def) {
  static_cast<EShaderDef&>(*this) = def;
  if (m_surface & 0x84000000u) AddToUpdateList();
  return 1;
}

// 802F9E0C
EShader* EShader::Clone() {
  EShader* copy = _pGfx->CreateShader(*this);
  if (!copy) return nullptr;
  copy->m_magic = m_magic;
  copy->m_currentMultiTexture = m_currentMultiTexture;
  copy->m_fieldED = m_fieldED;
  copy->m_fieldEE = m_fieldEE;
  return copy;
}

// 802FA474
ETexture* EShader::GetTexture(int layer) const {
  if (!(m_surface & 0x04000000u)) return m_layers[layer].m_texture;
  if (m_layers[0].m_multiTextureCount && m_layers[0].m_multiTextures)
    return m_layers[0].m_multiTextures[m_currentMultiTexture];
  return nullptr;
}

// 802FA578
void EShader::Validate() {
  if (!(m_surface & 0x04000000u)) {
    for (int i = 0; i < m_layerCount; ++i)
      if (m_layers[i].m_texture) m_layers[i].m_texture->Validate();
  } else {
    for (int i = 0; i < m_layers[0].m_multiTextureCount; ++i)
      if (m_layers[0].m_multiTextures[i]) m_layers[0].m_multiTextures[i]->Validate();
  }
}

// 802FAAA8
void EShader::Select(ERC*, unsigned int) {}
// 802FA9AC
// The shadow mask is the shader itself unless the platform's shader says otherwise.
void EShader::SelectForShadowMask(ERC* rc) { Select(rc, 0); }
// 802FA9E4
// Nothing on the GameCube: the shadow's darkness is the stencil alpha (ENgcRenderer::Stencil).
void EShader::SetShadowAlpha(float) {}
// 802FAAAC
EMaterial* EShader::GetMaterial() const { return reinterpret_cast<EMaterial*>(const_cast<float*>(m_material)); }
// 802FAAB4
void EShader::SetSurfaceProperty(unsigned int bits) { m_surface |= bits; }
// 802FAAC4
void EShader::ClearSurfaceProperty(unsigned int bits) { m_surface &= ~bits; }
// 802FAAD4
int EShader::IsSurface(unsigned int bits) const { return (m_surface & bits) != 0; }
// 802FAAEC
unsigned int EShader::GetSurfaceProperties() const { return m_surface; }
// 802FAB18
void EShader::SetTexture(ETexture* texture, int layer) { m_layers[layer].m_texture = texture; }
// 802FAB28
void EShader::SetMultiTexture(ETexture* texture, int index) { m_layers[0].m_multiTextures[index] = texture; }
// 802FAB48
void EShader::SetAlternateShader(EShader*) {}
// 802FAB4C
const EShaderDef& EShader::GetShaderDef() const { return *this; }
// 802FABC0
void EShader::SetAlphaTestThreshold(float threshold, int layer) { m_layers[layer].m_field14 = threshold; }
// 802FABD0
void EShader::UpdateMaterialCoefficients() {}
// 802FABD4
bool EShader::IsValid() { return m_magic == 0x900dbeef; }
// 802FABF0
void EShader::RemoveGeometryModes(unsigned int) {}
// 802FABF4
void EShader::AddGeometryModes(unsigned int) {}

// ---- Animated shaders ----------------------------------------------------------------------------
// Shaders with scrolling layers or frame-animated multi-textures sit in a list that UpdateAll steps
// once per frame. (The original guarded the list with a mutex; it is only touched by the app thread.)

static EShaderUpdate* s_updateHead = nullptr;  // EShader::m_updateList
static EShaderUpdate* s_updateTail = nullptr;  // 80500064

// 802F9E88
void EShader::AddToUpdateList() {
  if (m_update) return;
  m_update = new EShaderUpdate;
  m_update->shader = this;
  m_update->prev = s_updateTail;
  if (s_updateTail) s_updateTail->next = m_update;
  else s_updateHead = m_update;
  m_update->next = nullptr;
  s_updateTail = m_update;
}

// 802F9FA8
void EShader::RemoveFromUpdateList() {
  if (!m_update) return;
  EShaderUpdate* u = m_update;
  if (s_updateHead == u) s_updateHead = u->next;
  else u->prev->next = u->next;
  if (s_updateTail == u) s_updateTail = u->prev;
  else u->next->prev = u->prev;
  delete u;
  m_update = nullptr;
}

// 802FA084
void EShader::UpdateAll(float dt) {
  for (EShaderUpdate* u = s_updateHead; u; u = u->next) u->shader->Update(dt);
}

// 802FA130
void EShader::Update(float dt) {
  EShaderLayer& base = m_layers[0];
  // Frame-animated multi-texture: m_field25 frames per image, m_extraDataSize images, optional order.
  if ((m_surface & 0x06000000u) == 0x04000000u && base.m_field25 != 0 && base.m_extraDataSize != 0) {
    if (dt > 0.0f) ++m_fieldEE;
    if (m_fieldEE >= base.m_field25) {
      m_fieldEE = 0;
      m_fieldED = static_cast<unsigned char>((m_fieldED + 1) % base.m_extraDataSize);
    }
    if (base.m_extraData) m_currentMultiTexture = base.m_extraData[m_fieldED];
  }
  // Scrolling layers.
  if ((m_surface & 0x80000000u) && m_update) {
    for (int i = 0; i < m_layerCount && i < 2; ++i) {
      const EShaderLayer& L = m_layers[i];
      if (!(L.m_flags & 0x40)) continue;
      float* s = m_update->scroll[i];
      s[0] += dt * L.m_scrollU;
      s[1] += dt * L.m_scrollV;
      for (float* c : {&s[0], &s[1]}) {
        while (*c > 1.0f) *c -= 1.0f;
        while (*c < -1.0f) *c += 1.0f;
      }
    }
  }
}
