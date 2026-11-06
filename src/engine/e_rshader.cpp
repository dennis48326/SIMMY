// Port of engine/e_rshader.cpp (partial): loading a shader resource and reference-counting the
// textures it uses; multi-state shaders (weather, time-of-day and UI variants) start in their default
// state's shader, and UpdateAll switches them over time (blending colours when the states share their
// textures).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

// 8031C6DC
ERShader::ERShader() {}

// 8031C7A0
ERShader::~ERShader() {
  Deallocate();
  delete[] m_multi.m_states;  // MultiShaderInfo::DeAlloc
  m_multi.m_states = nullptr;
  delete[] m_multi.m_ids;
  m_multi.m_ids = nullptr;
}


// 8031C808
void ERShader::Deallocate() {
  DelRefSubResources();
  if (m_shader) {
    _shaderman.Free(m_shader->m_layers[0].m_extraData);
    m_shader->m_layers[0].m_extraData = nullptr;
    _shaderman.Free(m_shader->m_layers[0].m_multiTextures);
    m_shader->m_layers[0].m_multiTextures = nullptr;
    _pGfx->Destroy(m_shader);
    m_shader = nullptr;
  }
  m_multiTextureCount = 0;
  m_blendElapsed = 0;
  _shaderman.Free(m_multiTextures);
  _shaderman.Free(m_multiTextureIds);
  _shaderman.Free(m_field68);
  m_field68 = nullptr;
  m_multiTextures = nullptr;
  m_multiTextureIds = nullptr;
}

// 8031C904
// Releases the textures; their ids are kept so AddRefSubResources can load them again.
void ERShader::DelRefSubResources() {
  RemoveFromUpdateList();
  DeallocateIncomingShader();
  if (!(m_shaderFlags & 2)) {
    for (int i = 0; i < 2; ++i) {
      ERTexture* t = m_textures[i];
      if (t && (!m_manager || m_textureIds[i]) && t->m_refs) {
        m_textureIds[i] = t->m_id;
        t->DelRef();
        m_textures[i] = nullptr;
      }
    }
    for (unsigned int i = 0; i < m_multiTextureCount; ++i)
      if (ERTexture* t = m_multiTextures[i]) {
        m_multiTextureIds[i] = t->m_id;
        t->DelRef();
        m_multiTextures[i] = nullptr;
      }
  } else {
    if (m_stateShader) {
      m_stateShader->DelRef();
      m_stateShader = nullptr;
    }
    m_field50 = 0;
    m_localizedState = 0;
    for (int i = 0; i < 2; ++i) {
      m_textures[i] = nullptr;
      m_textureIds[i] = 0;
    }
  }
}

// 8031CA58
void ERShader::DeallocateIncomingShader() {
  if (m_incoming) {
    m_incoming->DelRef();
    m_incoming = nullptr;
  }
  if (m_shaderFlags & 4) {
    _shaderman.DelRefAsync(m_pendingId);
    m_pendingId = 0;
    m_shaderFlags &= ~4u;
  }
  m_shaderFlags &= ~8u;
  _shaderman.Free(m_field68);
  m_field68 = nullptr;
  m_blendElapsed = 0;
}

// 80320654
EShader* ERShader::GetShader() {
  if (m_shader) return m_shader;
  return m_stateShader ? m_stateShader->m_shader : nullptr;
}

// 8031CAF8
void ERShader::AddRefSubResources() {
  if (m_shaderFlags & 2) {
    // The default state's shader (GC: the localized state when the shader is listed in
    // _localizationShaders, which this build leaves empty).
    const unsigned char state = m_defaultShaderState < m_multi.m_count ? m_multi.m_states[m_defaultShaderState] : m_multi.m_states[0];
    SetCurrentShaderForState(state);
    AddToUpdateList();
    return;
  }
  EShader* shader = GetShader();
  if (shader->m_surface & 0x04000000u) {
    for (unsigned int i = 0; i < m_multiTextureCount; ++i)
      if (shader->m_layers[0].m_multiTextures[i]) {
        m_multiTextures[i] = static_cast<ERTexture*>(_textureman.AddRef(m_multiTextureIds[i], nullptr, 0));
        GetShader()->SetMultiTexture(m_multiTextures[i]->m_texture, static_cast<int>(i));
      }
    return;
  }
  for (int i = 0; i < shader->m_layerCount; ++i)
    if (shader->m_layers[i].m_texture) {
      m_textures[i] = static_cast<ERTexture*>(_textureman.AddRef(m_textureIds[i], nullptr, 0));
      if (m_textures[i]) GetShader()->SetTexture(m_textures[i]->m_texture, i);
    }
}

static bool TryTexture(unsigned int id, ERTexture*& out) {
  EResource* res = nullptr;
  if (!_textureman.TryIncrementResource(id, &res)) return false;
  out = static_cast<ERTexture*>(res);
  return true;
}

// 8031CDCC
// Takes references on the textures only if all of them are already loaded.
int ERShader::TryIncrementSubResources() {
  if (m_shaderFlags & 2) {
    if (!m_multi.m_count) return 1;
    const unsigned char state = m_defaultShaderState < m_multi.m_count ? m_multi.m_states[m_defaultShaderState] : m_multi.m_states[0];
    EResource* res = nullptr;
    if (!_shaderman.TryIncrementResource(GetMultiShaderStateID(state), &res)) return 0;
    SetCurrentShaderForState(state);
    if (res) res->DelRef();
    AddToUpdateList();
    return 1;
  }
  EShader* shader = GetShader();
  if (!(shader->m_surface & 0x04000000u)) {
    for (int i = 0; i < shader->m_layerCount; ++i)
      if (shader->m_layers[i].m_texture && !TryTexture(m_textureIds[i], m_textures[i])) {
        for (int j = i - 1; j >= 0; --j)
          if (shader->m_layers[j].m_texture) {
            m_textures[j]->DelRef();
            m_textures[j] = nullptr;
          }
        return 0;
      }
    for (int i = 0; i < shader->m_layerCount; ++i)
      if (shader->m_layers[i].m_texture) GetShader()->SetTexture(m_textures[i]->m_texture, i);
  } else {
    for (unsigned int i = 0; i < m_multiTextureCount; ++i)
      if (shader->m_layers[0].m_multiTextures[i] && !TryTexture(m_multiTextureIds[i], m_multiTextures[i])) {
        for (int j = static_cast<int>(i) - 1; j >= 0; --j)
          if (shader->m_layers[0].m_multiTextures[j]) {
            m_multiTextures[j]->DelRef();
            m_multiTextures[j] = nullptr;
          }
        return 0;
      }
    for (unsigned int i = 0; i < m_multiTextureCount; ++i)
      if (shader->m_layers[0].m_multiTextures[i])
        GetShader()->SetMultiTexture(m_multiTextures[i]->m_texture, static_cast<int>(i));
  }
  return 1;
}

// 8031D4C0
void ERShader::Refresh(EFile* file) {
  if (file) Load(*file);
}

// 8031D500
void ERShader::Load(EFile& file) {
  Deallocate();
  EShaderDef def;
  def.m_geometryModes = 8;
  DoLoad(file, def);
  if (!(m_shaderFlags & 2)) m_shader = _pGfx->CreateShader(def);
}

// 8031ED68
// The resource data is an EShaderDef byte image; returns the bytes after it.
const unsigned char* ERShader::CopyShedData(EShaderDef& def, const unsigned char* data) { return def.ReadBE(data); }

// 8031E2BC
void ERShader::DoLoad(EFile& file, EShaderDef& def) {
  EDataHeader header;
  if (!header.Read(file, 0x53484452 /* 'SHDR' */, 0x16, 0x16)) {
    OldLoad(file, def);
  } else {
    m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
    std::vector<unsigned char> data(header.m_dataSize);
    file.Read(data.data(), header.m_dataSize);
    if (header.m_variant == 1) {
      DoLoadMultiShader(reinterpret_cast<char*>(data.data()), def);
      return;
    }
    m_shaderFlags &= ~2u;
    const unsigned char* rest = CopyShedData(def, data.data());
    for (int i = 0; i < def.m_layerCount; ++i) {
      EShaderLayer& L = def.m_layers[i];
      if (!(L.m_field0C & 0x40)) {
        L.m_blend[3] = 0;
        L.m_blend[0] = L.m_blend[1] = L.m_blend[2] = 2;
      } else if (L.m_blend[0] == 0 && L.m_blend[1] == 2 && L.m_blend[2] == 2 && L.m_blend[3] == 1 &&
                 (L.m_alpha & 0x80)) {
        L.m_alpha = 0x7f;
      }
    }
    for (int i = 0; i < 3; ++i)  // specular color is capped at 0.4
      if (def.m_material[8 + i] > 0.4f) def.m_material[8 + i] = 0.4f;

    if (header.m_version < 0x14 || !(def.m_surface & 0x04000000u)) {
      // Texture slots hold ids: keep them here and leave a 1 ("has a texture") in the def.
      for (int i = 0; i < def.m_layerCount; ++i) {
        const auto id = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(def.m_layers[i].m_texture));
        if (id) m_textureIds[i] = id;
        def.m_layers[i].m_texture = reinterpret_cast<ETexture*>(static_cast<uintptr_t>(id ? 1 : 0));
      }
    } else {
      // Multi-texture: count ids (big-endian) follow the def, then the extra data bytes.
      const unsigned int count = def.m_layers[0].m_multiTextureCount;
      if (count) {
        def.m_layers[0].m_multiTextures = static_cast<ETexture**>(_shaderman.Alloc(count * sizeof(ETexture*), 8));
        m_multiTextures = static_cast<ERTexture**>(_shaderman.Alloc(count * sizeof(ERTexture*), 8));
        m_multiTextureIds = static_cast<unsigned int*>(_shaderman.Alloc(count * 4, 8));
        std::memset(m_multiTextures, 0, count * sizeof(ERTexture*));
        std::memset(m_multiTextureIds, 0, count * 4);
        m_multiTextureCount = count;
        for (unsigned int i = 0; i < count; ++i, rest += 4) {
          const unsigned int id = simmy::be32(rest);
          if (id) m_multiTextureIds[i] = id;
          def.m_layers[0].m_multiTextures[i] = reinterpret_cast<ETexture*>(static_cast<uintptr_t>(id ? 1 : 0));
        }
      }
      if (const unsigned short n = def.m_layers[0].m_extraDataSize) {
        def.m_layers[0].m_extraData = static_cast<unsigned char*>(_shaderman.Alloc(n, 8));
        std::memcpy(def.m_layers[0].m_extraData, rest, n);
      }
    }

    // Scrolling layers are animated.
    bool texGen = false;
    for (int i = 0; i < def.m_layerCount; ++i) {
      EShaderLayer& L = def.m_layers[i];
      if (L.m_scrollU != 0.0f || L.m_scrollV != 0.0f) {
        def.m_surface |= 0x80000000u;
        L.m_flags |= 0x40;
        if (L.m_texGen == 0) L.m_texGen = 5;
        else if (L.m_texGen == 1) L.m_texGen = 6;
      }
      if (static_cast<unsigned char>(L.m_texGen - 4) < 3) texGen = true;
    }
    if (texGen) def.m_surface |= 0x08000000u;

    // Load the textures and put their GX textures into the def.
    if (!(def.m_surface & 0x04000000u)) {
      for (int i = 0; i < def.m_layerCount; ++i) {
        if (!def.m_layers[i].m_texture) continue;
        m_textures[i] = static_cast<ERTexture*>(_textureman.AddRef(m_textureIds[i], nullptr, 0));
        def.m_layers[i].m_texture = m_textures[i] ? m_textures[i]->m_texture : nullptr;
      }
    } else {
      for (unsigned int i = 0; i < m_multiTextureCount; ++i)
        if (def.m_layers[0].m_multiTextures[i]) {
          m_multiTextures[i] = static_cast<ERTexture*>(_textureman.AddRef(m_multiTextureIds[i], nullptr, 0));
          def.m_layers[0].m_multiTextures[i] = m_multiTextures[i] ? m_multiTextures[i]->m_texture : nullptr;
        }
    }
  }
  // Alpha values are stored at half scale.
  def.m_layers[0].m_alpha = static_cast<unsigned char>(std::min(def.m_layers[0].m_alpha * 2, 0xff));
  def.m_layers[1].m_alpha = static_cast<unsigned char>(std::min(def.m_layers[1].m_alpha * 2, 0xff));
}

// 80320534 ERShader::GetTypeInfo
// 80320540 ERShader::GetTypeName
// 8032054C ERShader::GetTypeKey
// 80320558 ERShader::GetTypeVersion
// 803204F4 ERShader::SafeDelete
// 8032047C ERShader::New
// 803204A4 ERShader::Construct
// 803204D0 ERShader::Destruct
// 80320564 ERShader::GetTypeInfoStatic
// 80320570 ERShader::GetReadVersion
SIMMY_STORABLE_IMPL(ERShader, "ERShader", 0, EResource)

// 8031D6FC
// A copy of this shader registered under `id`, with its own EShader (and the same textures), so its
// textures can be replaced without touching the original. None while it is loading or switching state.
// Multi-state and multi-texture shaders are not ported.
ERShader* ERShader::Clone(unsigned int id) {
  if (!m_refs || m_incoming || m_pendingId || !id || id == m_id) return nullptr;
  if (_shaderman.IsLoaded(id)) return nullptr;
  auto* copy = new ERShader();  // GC: EStorable::CreateCopy
  copy->m_id = id;
  copy->m_flags = (m_flags & 0xffffff00u) | (copy->m_flags & 0xff);
  copy->m_multiTextureCount = m_multiTextureCount;
  copy->m_multi.m_baseId = m_multi.m_baseId;
  copy->m_multi.m_blendTime = m_multi.m_blendTime;
  copy->m_multi.m_count = m_multi.m_count;
  copy->m_field50 = m_field50;
  copy->m_shaderFlags = m_shaderFlags;
  copy->m_localizedState = m_localizedState;
  copy->m_field65 = m_field65;
  copy->m_hour = m_hour;
  copy->m_minute = m_minute;
  if (m_shaderFlags & 2) {
    SIMMY_STUB("ERShader::Clone (multi-state shader)", 0x8031D6FC);
    delete copy;
    return nullptr;
  }
  EShader* src = GetShader();
  copy->m_shader = src ? src->Clone() : nullptr;
  if (!copy->m_shader) {
    delete copy;
    return nullptr;
  }
  if ((copy->m_shader->m_surface >> 26) & 1) {
    SIMMY_STUB("ERShader::Clone (multi-texture shader)", 0x8031D6FC);
    copy->m_multiTextureCount = 0;
    copy->m_shader->m_layers[0].m_multiTextures = nullptr;
    copy->m_shader->m_layers[0].m_extraData = nullptr;
  } else {
    for (int i = 0; i < 2; ++i) {
      copy->m_textureIds[i] = m_textureIds[i];
      if (ERTexture* t = m_textures[i]) {
        copy->m_textures[i] = t;
        t->AddRef();
        copy->m_shader->m_layers[i].m_texture = t->m_texture;
      }
    }
  }
  return copy;
}

// 80320110
// Layer `layer` of the shader takes `texture` (the old one is released).
void ERShader::ReplaceTexture(ERTexture* texture, int layer) {
  if (m_shaderFlags & 2) {
    if (m_stateShader) m_stateShader->ReplaceTexture(texture, layer);
    return;
  }
  EShader* shader = GetShader();
  if (!shader || ((shader->m_surface >> 26) & 1) || !texture) return;
  if (m_textures[layer]) {
    m_textures[layer]->DelRef();
    m_textures[layer] = nullptr;
  }
  m_textures[layer] = texture;
  texture->AddRef();
  m_textureIds[layer] = texture->m_id;
  shader->SetTexture(texture->m_texture, layer);
}

// ---------------------------------------------------------------------------------------------------
// Multi-state shaders

unsigned char ERShader::m_defaultShaderState = 0;  // 804FF108

// 8031DCD0
// The state multi-state shaders take when they are loaded (1 while creating a sim).
void ERShader::SetDefaultShaderState(int state) { m_defaultShaderState = static_cast<unsigned char>(state); }

// 8031DCD8
// The resource names states and their shaders: the kind byte, a word, the count, the states, the
// shaders' ids (big-endian). Sorted by state; the default state's shader becomes current and its
// definition is what this shader reports.
void ERShader::DoLoadMultiShader(char* data, EShaderDef& def) {
  const auto* p = reinterpret_cast<const unsigned char*>(data);
  m_shaderFlags |= 2;
  m_multi.m_baseId = p[0];
  if (p[0] == 1) m_shaderFlags |= 0x20;
  else if (p[0] == 0) m_shaderFlags |= 0x10;
  else if (p[0] == 2) m_shaderFlags |= 0x40;
  {
    const unsigned int bits = simmy::be32(p + 1);
    std::memcpy(&m_multi.m_blendTime, &bits, 4);
  }
  m_multi.m_count = p[5];
  delete[] m_multi.m_states;  // MultiShaderInfo::DeAlloc
  delete[] m_multi.m_ids;
  m_multi.m_states = nullptr;
  m_multi.m_ids = nullptr;
  const unsigned int n = m_multi.m_count;
  if (n) {
    m_multi.m_states = new unsigned char[n];
    m_multi.m_ids = new unsigned int[n];
  }
  const unsigned char* q = p + 6;
  for (unsigned int i = 0; i < n; ++i) m_multi.m_states[i] = *q++;
  for (unsigned int i = 0; i < n; ++i, q += 4) m_multi.m_ids[i] = simmy::be32(q);
  for (unsigned int i = 0; i + 1 < n; ++i)
    for (unsigned int j = i + 1; j < n; ++j)
      if (m_multi.m_states[j] < m_multi.m_states[i]) {
        std::swap(m_multi.m_states[i], m_multi.m_states[j]);
        std::swap(m_multi.m_ids[i], m_multi.m_ids[j]);
      }
  if (std::getenv("SIMMY_SHADER_TRACE")) {
    std::printf("[shader] %08X: multi-state kind %d, blend %.2fs, states", m_id, m_multi.m_baseId, m_multi.m_blendTime);
    for (unsigned int i = 0; i < n; ++i) std::printf(" %u=%08X", m_multi.m_states[i], m_multi.m_ids[i]);
    std::printf("\n");
  }
  if (n) {
    SetCurrentShaderForState(m_defaultShaderState < n ? m_multi.m_states[m_defaultShaderState] : m_multi.m_states[0]);
    if (EShader* current = GetShader()) def = *current;
  }
  AddToUpdateList();
}

// 8031DFE0
// The shader of `state` (taken now) becomes current. 1: the shader has that state.
int ERShader::SetCurrentShaderForState(unsigned char state) {
  if (!(m_shaderFlags & 2)) return 0;
  unsigned int i = 0;
  while (i < m_multi.m_count && m_multi.m_states[i] != state) ++i;
  if (i == m_multi.m_count) return 0;
  DeallocateIncomingShader();
  m_localizedState = static_cast<unsigned char>(i);
  m_field50 = m_multi.m_states[i];
  ChangeCurrentShader(static_cast<ERShader*>(_shaderman.AddRef(m_multi.m_ids[i], nullptr, 0)));
  return 1;
}

// 8031E0B0
// `shader` (referenced) stands in for this one: its EShader and textures.
void ERShader::ChangeCurrentShader(ERShader* shader) {
  if (!shader) return;
  if (m_stateShader) {
    _shaderman.DelRef(static_cast<EResource*>(m_stateShader), EResourceManager__DelRefMode{1});
    m_stateShader = nullptr;
  }
  m_stateShader = shader;
  m_shader = nullptr;
  for (int i = 0; i < 2; ++i) {
    m_textures[i] = shader->m_textures[i];
    if (m_textures[i]) m_textureIds[i] = m_textures[i]->m_id;
  }
  // GC: also takes the state shader's multi-texture arrays (+0x30..+0x38). PC: not, as Deallocate frees
  // its own arrays (they would be freed twice); a multi-state shader never reads them.
}

// 8031E164
unsigned int ERShader::GetMultiShaderStateID(int state) {
  if (!(m_shaderFlags & 2)) return 0;
  unsigned int i = 0;
  while (i < m_multi.m_count && m_multi.m_states[i] != static_cast<unsigned char>(state)) ++i;
  return i == m_multi.m_count ? 0 : m_multi.m_ids[i];
}

// ---------------------------------------------------------------------------------------------------
// The multi-state update list: weather (flag 0x10) and time-of-day (flag 0x20) shaders switch state
// as the clock and weather change; a state whose shader uses the same textures blends its colours
// over m_blendTime seconds (flag 8).

namespace {
std::recursive_mutex s_updateListMutex;           // 804B17B8
ERShaderUpdateNode* s_updateList = nullptr;       // 805000C4 the head
ERShaderUpdateNode* s_updateListTail = nullptr;   // 805000C8

// PC development aid: SIMMY_SHADER_TRACE=1 logs multi-state shaders' state changes.
bool ShaderTrace() {
  static const bool on = std::getenv("SIMMY_SHADER_TRACE") != nullptr;
  return on;
}
// PC development aid: SIMMY_SHADER_STATES=0 leaves multi-state shaders in their default state (as
// before UpdateAll was ported), for comparisons.
bool StatesOff() {
  static const bool off = [] {
    const char* v = std::getenv("SIMMY_SHADER_STATES");
    return v && v[0] == '0';
  }();
  return off;
}
}  // namespace

// 8031EF7C
void ERShader::AddToUpdateList() {
  if (m_updateNode) return;
  m_updateNode = static_cast<ERShaderUpdateNode*>(_shaderman.Alloc(sizeof(ERShaderUpdateNode), 8));
  m_updateNode->shader = this;
  std::lock_guard lock(s_updateListMutex);
  ERShaderUpdateNode* n = m_updateNode;
  n->prev = s_updateListTail;
  if (s_updateListTail) s_updateListTail->next = n;
  else s_updateList = n;
  n->next = nullptr;
  s_updateListTail = n;
}

// 8031F040
void ERShader::RemoveFromUpdateList() {
  if (!m_updateNode) return;
  {
    std::lock_guard lock(s_updateListMutex);
    ERShaderUpdateNode* n = m_updateNode;
    if (s_updateList == n) s_updateList = n->next;
    else n->prev->next = n->next;
    if (s_updateListTail == n) s_updateListTail = n->prev;
    else n->next->prev = n->prev;
  }
  _shaderman.Free(m_updateNode);
  m_updateNode = nullptr;
}

// 8031E1DC
// Starts loading the shader of `state` in the background (Update takes it once loaded). 1: the shader
// is in that state or on its way there; 0: it has no such state, or another change is loading.
int ERShader::SetShaderASyncForState(unsigned char state) {
  if (!(m_shaderFlags & 2)) return 0;
  if (m_field50 == state) return 1;
  if (m_shaderFlags & 4) return 0;
  unsigned int i = 0;
  while (i < m_multi.m_count && m_multi.m_states[i] != state) ++i;
  if (i == m_multi.m_count) return 0;
  DeallocateIncomingShader();
  m_field65 = m_localizedState;
  m_localizedState = static_cast<unsigned char>(i);
  m_shaderFlags |= 4;
  m_pendingId = m_multi.m_ids[i];
  _shaderman.AddRefAsync(m_pendingId);
  if (ShaderTrace())
    std::printf("[shader] %08X: state %u -> %u (shader %08X loading)\n", m_id, m_field50, state, m_pendingId);
  return 1;
}

// 8031F11C
// `hour`, `minute`: the clock; `weather`: the house's weather (EHouse +0x30: 0 fair, 1 rain).
void ERShader::Update(float dt, int hour, int minute, int weather) {
  if (!(m_shaderFlags & 2)) return;
  m_hour = static_cast<unsigned char>(hour);
  m_minute = static_cast<unsigned char>(minute);
  if (!(m_shaderFlags & 4)) {
    if (m_shaderFlags & 8) InterpolateShaders(dt);
    else if (m_shaderFlags & 0x20) UpdateTimeOfDayShader();
    else if (m_shaderFlags & 0x10) UpdateWeatherShader(weather);
    return;
  }
  // The state's shader is loading: take it once it is there.
  m_incoming = static_cast<ERShader*>(_shaderman.GetRef(m_pendingId));
  if (!m_incoming) return;
  m_pendingId = 0;
  m_shaderFlags &= ~4u;
  m_field50 = m_multi.m_states[m_localizedState];
  unsigned int multiTexture = 0;
  if (!(m_incoming->m_shaderFlags & 2)) multiTexture = (m_incoming->GetShader()->m_surface >> 26) & 1;
  if (!multiTexture && HasCommonTexture(m_incoming, m_stateShader) && m_multi.m_blendTime != 0.0f) {
    // Same textures: blend the colours from the current shader's over to the new one's.
    m_blendElapsed = 0;
    m_shaderFlags |= 8;
    auto* from = static_cast<ERShaderBlendFrom*>(_shaderman.Alloc(sizeof(ERShaderBlendFrom), 0x10));
    new (from) ERShaderBlendFrom();
    const EShader* cur = GetShader();
    for (int i = 0; i < 2; ++i) {
      from->alphaThreshold[i] = cur->m_layers[i].m_field14;
      std::memcpy(&from->layer18[i], &cur->m_layers[i].m_field18, 4);
      from->alpha[i] = cur->m_layers[i].m_alpha;
    }
    for (int i = 0; i < 12; ++i) from->material[i] = cur->m_material[i];
    m_field68 = from;
    if (ShaderTrace()) std::printf("[shader] %08X: blending to state %u over %.2fs\n", m_id, m_field50, m_multi.m_blendTime);
    InterpolateShaders(0.0f);
  } else {
    if (ShaderTrace()) std::printf("[shader] %08X: now state %u (shader %08X)\n", m_id, m_field50, m_incoming->m_id);
    ChangeCurrentShader(m_incoming);
    m_incoming = nullptr;
  }
}

// 8031F414
// 1 when both are single-state, non-multi-texture shaders with the same layer count and textures.
int ERShader::HasCommonTexture(ERShader* a, ERShader* b) {
  if (!a || !b) return 0;
  unsigned int multiTexture = 0;
  if (!(b->m_shaderFlags & 2)) multiTexture = (b->GetShader()->m_surface >> 26) & 1;
  if (multiTexture) return 0;
  const EShader* sa = a->GetShader();
  const EShader* sb = b->GetShader();
  if (sa->m_layerCount != sb->m_layerCount) return 0;
  for (int i = 0; i < sa->m_layerCount; ++i)
    if (a->m_textureIds[i] != b->m_textureIds[i]) return 0;
  return 1;
}

// 8031F540
void ERShader::UpdateWeatherShader(int weather) { SetShaderASyncForState(static_cast<unsigned char>(weather)); }

// 8031F560
// Time-of-day states are quarter hours (hour * 4 + minute / 15).
void ERShader::UpdateTimeOfDayShader() {
  SetShaderASyncForState(static_cast<unsigned char>(m_hour * 4 + m_minute / 15));
}

// 8031F5A4
void ERShader::InterpolateShaders(float dt) {
  if (dt == 0.0f) return;
  m_blendElapsed += dt;
  if (m_blendElapsed < m_multi.m_blendTime) {
    BlendCurrentShader(m_blendElapsed / m_multi.m_blendTime);
    return;
  }
  // Done: the old shader gets its own colours back and the new one takes over.
  BlendCurrentShader(0.0f);
  ChangeCurrentShader(m_incoming);
  m_incoming = nullptr;
  m_shaderFlags &= ~8u;
  _shaderman.Free(m_field68);
  m_field68 = nullptr;
  if (ShaderTrace()) std::printf("[shader] %08X: now state %u (blended)\n", m_id, m_field50);
}

// 8031F644
// The current shader's colours, alpha thresholds and alphas at `t` between the blend's start and the
// incoming shader's.
void ERShader::BlendCurrentShader(float t) {
  const EShader* to = m_incoming->GetShader();
  EShader* cur = GetShader();
  const ERShaderBlendFrom& from = *m_field68;
  for (int i = 0; i < 12; ++i) cur->m_material[i] = from.material[i] + (to->m_material[i] - from.material[i]) * t;
  cur->UpdateMaterialCoefficients();
  for (int i = 0; i < 2; ++i) {
    cur->SetAlphaTestThreshold(from.alphaThreshold[i] + (to->m_layers[i].m_field14 - from.alphaThreshold[i]) * t, i);
    float to18 = 0;
    std::memcpy(&to18, &to->m_layers[i].m_field18, 4);
    const float v = from.layer18[i] + (to18 - from.layer18[i]) * t;
    std::memcpy(&cur->m_layers[i].m_field18, &v, 4);
    cur->m_layers[i].m_alpha = static_cast<unsigned char>(
        from.alpha[i] + static_cast<int>(static_cast<float>(static_cast<int>(to->m_layers[i].m_alpha) - from.alpha[i]) * t));
  }
}

// 8031FB04
// Every multi-state shader in use, with the clock and weather (taken under the lists' locks, updated
// outside them).
void ERShader::UpdateAll(float dt, int hour, int minute, int weather) {
  if (StatesOff()) return;
  std::vector<ERShader*> shaders;
  {
    std::lock_guard managerLock(_shaderman.m_mutex);
    std::lock_guard listLock(s_updateListMutex);
    int n = 0;
    for (ERShaderUpdateNode* u = s_updateList; u; u = u->next) ++n;
    shaders.reserve(static_cast<size_t>(n));
    for (ERShaderUpdateNode* u = s_updateList; u; u = u->next)
      if (u->shader->m_refs > 0) {
        u->shader->AddRef();
        shaders.push_back(u->shader);
      }
  }
  for (ERShader* s : shaders) s->Update(dt, hour, minute, weather);
  for (ERShader* s : shaders) s->DelRef();
}

// 8031FDCC
// After a house loads: each time-of-day shader takes the state of the clock at once.
void ERShader::InitDayNightShaders(int hour, int minute) {
  if (StatesOff()) return;
  std::vector<ERShader*> shaders;
  {
    std::lock_guard managerLock(_shaderman.m_mutex);
    std::lock_guard listLock(s_updateListMutex);
    for (ERShaderUpdateNode* u = s_updateList; u; u = u->next) {
      u->shader->AddRef();
      shaders.push_back(u->shader);
    }
  }
  for (ERShader* s : shaders)
    if (s->m_shaderFlags & 0x20) s->SetDayNightShaderToClosestTime(hour, minute);
  for (ERShader* s : shaders) s->DelRef();
}

// 80320070
// The last state at or before the clock's quarter hour (before the first: the first). GC: after the
// last state nothing changes.
void ERShader::SetDayNightShaderToClosestTime(int hour, int minute) {
  const unsigned int t = static_cast<unsigned int>(hour * 4 + minute / 15) & 0xff;
  const unsigned int n = m_multi.m_count;
  unsigned int last = 0, i = 0;
  if (n && m_multi.m_states[0] <= t) {
    for (;;) {
      ++i;
      if (i >= n || m_multi.m_states[i] > t) break;
      last = i;
    }
  }
  if (i != n) {
    if (ShaderTrace()) std::printf("[shader] %08X: closest to %02d:%02d is state %u\n", m_id, hour, minute, m_multi.m_states[last]);
    SetCurrentShaderForState(m_multi.m_states[last]);
  }
}

// 80320854
int ERShader::IsStateChangePending() { return (m_shaderFlags >> 2) & 1; }
