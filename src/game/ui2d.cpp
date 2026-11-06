// Port of game/ui2d.cpp: the named shaders that the Flash UI's custom-rendered rectangles draw.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

UI2D* UI2D::s_pInstance = nullptr;

// 80175FB0 UI2D::UI2D (defaulted in uiscreen.hpp; the record pool is a std::vector)

// 8017606C
UI2D::~UI2D() {
  while (!m_records.empty()) RemoveEntry(m_records.size() - 1);
}

// 8017610C
// By id the shader is referenced asynchronously (GetShader picks it up once loaded).
void UI2D::AddEntry(const char* name, unsigned int id, ERShader* shader, const char* group, int state) {
  Record r;
  r.name = name;
  r.id = id;
  r.state = state;
  r.group = group ? group : "";
  if (!shader) {
    r.shader = static_cast<ERShader*>(_shaderman.AddRefAsync(id));
    r.async = true;
  } else {
    shader->AddRef();
    r.shader = shader;
  }
  // UI multi-state shaders (kind 2) switch to the entry's state.
  if (r.shader && r.shader->m_multi.m_baseId == 2) {
    r.multi = 1;
    r.stateOk = r.shader->SetShaderASyncForState(static_cast<unsigned char>(state));
    if (r.stateOk) {
      r.multi = static_cast<int>((r.shader->m_shaderFlags >> 2) & 1);
      if (!r.multi) {
        r.stateId = r.shader->GetMultiShaderStateID(state & 0xff);
        if (r.stateId) _shaderman.AddRef(r.stateId, nullptr, 0);
      }
    }
  }
  m_records.push_back(r);
}

// 80176274
void UI2D::RemoveEntry(size_t index) {
  Record& r = m_records[index];
  if (r.stateId) _shaderman.DelRef(r.stateId, EResourceManager__DelRefMode{1});
  if (r.async) _shaderman.DelRefAsync(r.id);
  else if (r.shader) r.shader->DelRef();
  m_records.erase(m_records.begin() + static_cast<long>(index));
}

// 801763B8
bool UI2D::InstallEntry(char* name, unsigned int id, char* group, int state) {
  if (Record* r = FindEntry(name)) RemoveEntry(static_cast<size_t>(r - m_records.data()));
  AddEntry(name, id, nullptr, group, state);
  return true;
}

// 8017643C
bool UI2D::InstallEntry(char* name, ERShader* shader, char* group, int state) {
  if (!shader) return false;
  if (Record* r = FindEntry(name)) RemoveEntry(static_cast<size_t>(r - m_records.data()));
  AddEntry(name, 0, shader, group, state);
  return true;
}

// 801764CC
UI2D::Record* UI2D::FindEntry(const char* name) {
  for (Record& r : m_records)
    if (r.name == name) return &r;
  return nullptr;
}

// 80176584
bool UI2D::UnInstallEntry(char* name) {
  Record* r = FindEntry(name);
  if (r) RemoveEntry(static_cast<size_t>(r - m_records.data()));
  return r != nullptr;
}

// 801765C8
bool UI2D::UnInstallAllEntries(char* group) {
  bool any = false;
  for (size_t i = m_records.size(); i-- > 0;)
    if (m_records[i].group == group) {
      RemoveEntry(i);
      any = true;
    }
  return any;
}

// 80176628
bool UI2D::ContainsEntry(char* name) { return FindEntry(name) != nullptr; }

// 80176658
// The entry's shader once loaded; a UI multi-state shader (kind 2) once it is in the entry's state.
ERShader* UI2D::GetShader(const char* name) {
  Record* r = FindEntry(name);
  if (!r) return nullptr;
  const auto state = static_cast<unsigned char>(r->state);
  if (r->async && (!r->shader || !r->shader->GetShader())) {
    r->shader = static_cast<ERShader*>(_shaderman.GetRef(r->id));
    if (!r->shader || !r->shader->GetShader()) return nullptr;
    if (r->shader->m_multi.m_baseId == 2) {
      r->stateOk = r->shader->SetShaderASyncForState(state);
      r->multi = 1;
    }
  }
  if (r->multi) {
    if (!r->stateOk) {
      r->stateOk = r->shader->SetShaderASyncForState(state);
      return nullptr;
    }
    r->shader->Update(0.0f, 0, 0, 0);
    r->multi = static_cast<int>((r->shader->m_shaderFlags >> 2) & 1);
    if (r->multi) return nullptr;
    r->stateId = r->shader->GetMultiShaderStateID(state);
    if (r->stateId) _shaderman.AddRef(r->stateId, nullptr, 0);
  }
  if (r->shader->m_multi.m_baseId == 2) {
    if (!r->shader->SetShaderASyncForState(state)) return nullptr;
    r->shader->Update(0.0f, 0, 0, 0);
    if (r->shader->m_field50 != static_cast<unsigned int>(r->state)) return nullptr;
  }
  return r->shader;
}

// 801767F8
UI2D* UI2D::Instance() {
  if (!s_pInstance) s_pInstance = new UI2D;
  return s_pInstance;
}

// 80176834
void UI2D::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}

// ---------------------------------------------------------------------------------------------------
// UIButtonImages (game/uisupport.cpp)

UIButtonImages* UIButtonImages::instance = nullptr;

// 801811A0
void UIButtonImages::Startup() {
  if (instance) return;
  instance = new UIButtonImages;
  instance->DefineIDs();
  instance->AddRefAll();
}

// 801811E8
void UIButtonImages::Shutdown() {
  if (!instance) return;
  instance->DelRefAll();
  delete instance;
  instance = nullptr;
}

// 80181244
// Pictures are at least 28 pixels wide (button 1: 20, button 5: 50).
UIButtonImages::UIButtonImages() {
  for (float& f : m_minSize) f = 28.0f;
  m_minSize[1] = 20.0f;
  m_minSize[5] = 50.0f;
}

// 801812AC
UIButtonImages::~UIButtonImages() = default;

// 801812E0
// The GameCube's button shaders (the Xbox and PS2 sets are not used by this build).
void UIButtonImages::DefineIDs() {
  if (m_platform != 3) {
    SIMMY_STUB("UIButtonImages::DefineIDs: Xbox / PS2 button sets", 0x801812E0);
    return;
  }
  static const unsigned int kGameCube[kButtons] = {0x875049c1, 0x1e59187b, 0x31327dcf, 0x6021089a, 0x81f45d63,
                                                   0xd6be9e96, 0xe33be101, 0x943cd197, 0x0d35802d, 0x943cd197,
                                                   0x943cd197, 0xf9e1357c, 0x03ee081f, 0x0d35802d};
  for (int i = 0; i < kButtons; ++i) m_ids[i] = kGameCube[i];
}

// 8018155C
void UIButtonImages::AddRefAll() {
  for (unsigned int id : m_ids) _shaderman.AddRefAsync(id);
  m_referenced = 1;
}

// 801815B0
void UIButtonImages::DelRefAll() {
  for (unsigned int id : m_ids) _shaderman.DelRefAsync(id);
  m_referenced = 0;
}

// 80181604
ERShader* UIButtonImages::GetButtonShader(int button) {
  if (!instance || button < 0 || button >= kButtons) return nullptr;
  if (!instance->m_shaders[button]) instance->m_shaders[button] = static_cast<ERShader*>(_shaderman.GetRef(instance->m_ids[button]));
  return instance->m_shaders[button];
}

// 80181678
double UIButtonImages::GetButtonMinSize(int button) {
  if (!instance || button < 0 || button >= kButtons) return 0.0;
  return instance->m_minSize[button];
}

// 801816A0
void UIButtonImages::LocalizeXBoxImages() {
  if (!instance) Startup();
}
