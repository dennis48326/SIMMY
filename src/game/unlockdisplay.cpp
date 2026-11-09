// The UI's 3D model widget (game/unlockdisplay.cpp, unlockdisplayobject.cpp, unlockdisplayobjectsim.cpp):
// see unlockdisplay.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/detmath.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {
float Mag(const EVec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
// PSVECNormalize (callers skip the zero vector).
void Normalize(EVec3& v) {
  const float m = Mag(v);
  if (m != 0.0f) v.x /= m, v.y /= m, v.z /= m;
}
// v as a row vector through the rotation part of m (v.x * row 0 + v.y * row 1 + v.z * row 2).
EVec3 Rotate(const EVec3& v, const EMat4& m) {
  return EVec3{v.z * m.m[2][0] + v.x * m.m[0][0] + v.y * m.m[1][0], v.z * m.m[2][1] + v.x * m.m[0][1] + v.y * m.m[1][1],
               v.z * m.m[2][2] + v.x * m.m[0][2] + v.y * m.m[1][2]};
}
EMat4 RotationXYZ(const EVec3& r) {
  EMat4 m;
  m.RotateX(r.x);
  m.PostRotateY(r.y);
  m.PostRotateZ(r.z);
  return m;
}
// The screen's size (EGraphics +0x18 / +0x1C).
float ScreenW() { return static_cast<float>(_pGfx->m_screenWidth); }
float ScreenH() { return static_cast<float>(_pGfx->m_screenHeight); }

// The beds whose catalogue model is one half (803D5FF0, 803D6014, 803D6038, 803D605C): a model in the
// first column shows the second column's model with the fourth's animation, one in the second the
// first's with the third's.
const unsigned int kBedA[9] = {0xB39ABCC1, 0xFE3B9E34, 0x12CE932F, 0x0E069DC6, 0xFD46ED2D,
                               0xEA2BBC0F, 0x25BCAA65, 0x23DD7AF6, 0xE20D9428};
const unsigned int kBedB[9] = {0x2067B3CB, 0x5429862C, 0x6774289B, 0x66B930BC, 0x9FB893AE,
                               0x5CE2ED7C, 0xF14AF22C, 0x984A5102, 0xF9E07E3F};
const unsigned int kBedC[9] = {0xF37B70FB, 0x197851EB, 0x5E80CF64, 0x2D1E6F9E, 0x14D7FE5C,
                               0xF73ACBEC, 0x76E3D158, 0x4A492B7F, 0x8653179C};
const unsigned int kBedD[9] = {0x684EC62E, 0x74693CBF, 0x0853B344, 0x7C8EB5D6, 0xB6ACB535,
                               0xEB990252, 0x2714ACDD, 0x8222B34C, 0xBBED1AB2};
}  // namespace

// ---------------------------------------------------------------------------------------------------
// CUnlockDisplay

// 800775AC
// A box from (192, 77) to 66 / 102 pixels short of the screen's right / bottom, ambient 0.3 and three
// lights (warm from (1, 0, -1), blue from (0, 1, 1), white from (-1, 0, -0.5)), the camera's offset
// (-0.25, -0.25, 0.125) of the objects' frame, zoom 1.
CUnlockDisplay::CUnlockDisplay() {
  m_state = 0;
  m_boxLeft = 192.0f / ScreenW();
  m_boxTop = 77.0f / ScreenH();
  m_boxRight = (ScreenW() - 66.0f) / ScreenW();
  m_radius = 0.0f;
  m_objects = nullptr;
  m_mode = 0;
  m_models = 0;
  m_sims = 0;
  m_boxBottom = (ScreenH() - 102.0f) / ScreenH();
  std::memset(&m_lights, 0, sizeof m_lights);
  SetAmbientLight(0.3f, 0.3f, 0.3f);
  SetDirectLight(0.95f, 0.8470000029f, 0.7120000124f, 0);
  SetDirectLightDir(EVec3{1.0f, 0.0f, -1.0f}, false, 0);
  SetDirectLight(0.6180000305f, 0.6180000305f, 0.7860000134f, 1);
  SetDirectLightDir(EVec3{0.0f, 1.0f, 1.0f}, false, 1);
  SetDirectLight(0.8429999948f, 0.8429999948f, 0.9990000129f, 2);
  SetDirectLightDir(EVec3{-1.0f, 0.0f, -0.5f}, false, 2);
  m_hidden = 0;
  m_zoom = 1.0f;
  m_preloaded = 0;
  m_shown = 0;
  m_done = 0;
  ManualCameraPos(-0.25f, -0.25f, 0.125f);
}

// 80077854
CUnlockDisplay::~CUnlockDisplay() { destroyObjects(); }

// 800778B0
// The chain for the mode: one model (1), the promotion's models and sims (2), two sims (3).
void CUnlockDisplay::makeObjects() {
  int models = 0, sims = 0;
  if (m_mode == 2) models = m_models, sims = m_sims;
  else if (m_mode == 1) models = 1;
  else if (m_mode == 3) sims = 2;
  int n = 0;
  CUnlockDisplayObject* last = nullptr;
  for (; models > 0; --models) {
    auto* o = new CUnlockDisplayObject;
    if (n == 0) m_objects = o;
    else last->m_next = o;
    ++n;
    last = o;
  }
  for (; sims > 0; --sims) {
    auto* o = new CUnlockDisplayObjectSim;
    if (n == 0) m_objects = o;
    else last->m_next = o;
    ++n;
    last = o;
  }
}

// 800779A0
void CUnlockDisplay::destroyObjects() {
  for (CUnlockDisplayObject* o = m_objects; o;) {
    CUnlockDisplayObject* next = o->m_next;
    delete o;
    o = next;
  }
  m_objects = nullptr;
}

// 80077A0C
// One object (and a bed's other half beside it) with the definition's character, model and animation,
// at `pos`, turned by `rot` and turning by `rotAdd`, looping.
void CUnlockDisplay::SetupForObject(ObjectDefinition* def, EVec3& pos, EVec3& rot, EVec3& rotAdd) {
  m_mode = 1;
  makeObjects();
  CUnlockDisplayObject* o = m_objects;
  o->SetCharacterID(def->m_character);
  o->m_modelId = def->m_model;
  o->SetAnimationID(def->m_anim);
  o->m_next = nullptr;
  o->m_pos = pos;
  o->m_rot = rot;
  o->m_rotAdd = rotAdd;
  o->m_loops = 1;
  unsigned int otherModel = 0, otherAnim = 0;
  if (GetBedOtherHalfIDs(def->m_model, otherModel, otherAnim)) {
    CUnlockDisplayObject* first = m_objects;
    first->m_next = new CUnlockDisplayObject;
    CUnlockDisplayObject* half = first->m_next;
    half->SetCharacterID(otherModel);
    half->m_modelId = otherModel;
    half->SetAnimationID(otherAnim);
    half->m_next = nullptr;
    half->m_pos = pos;
    half->m_rot = rot;
    half->m_rotAdd = rotAdd;
    half->m_loops = 1;
  }
}

// 80077C3C
// A sim taking a bow for the promotion: three times in four (always from career level 9) one sim with
// one of four animations (the last two from level 9), else the sim with a cake (model and character
// 0x3DFF4AE6, animation 0x673FC0DD); 80 replays. Never called in this build.
void CUnlockDisplay::SetupForPromotion(cXPerson* person, void*) {
  m_mode = 2;
  int level = person->GetPersonData(0x39);
  if (level > 9) level = 9;
  unsigned int anim;
  CUnlockDisplayObject* sim;
  const unsigned int r = static_cast<unsigned int>(simmy::ui_rand());
  if (level > 8 || (r >> 2) % 0x3c > 0x13) {
    m_models = 0;
    m_sims = 1;
    makeObjects();
    unsigned int pick = static_cast<unsigned int>(simmy::ui_rand()) >> 8 & 1;
    if (level > 8) pick += 2;
    static const unsigned int kAnims[4] = {0x1038F04B, 0x8931A1F1, 0x9A4A2C03, 0xED4D1C95};
    sim = m_objects;
    anim = kAnims[pick];
  } else {
    m_sims = 1;
    m_models = 1;
    anim = 0x673FC0DD;
    makeObjects();
    CUnlockDisplayObject* cake = m_objects;
    cake->SetCharacterID(0x3DFF4AE6);
    cake->m_modelId = 0x3DFF4AE6;
    cake->SetAnimationID(0x3DFF4AE6);
    sim = m_objects->m_next;
  }
  static_cast<CUnlockDisplayObjectSim*>(sim)->SetPerson(person);
  sim->SetAnimationID(anim);
  sim->m_loops = 0x50;
}

// 80077DD4
void CUnlockDisplay::SetupForSocial(void*) {
  m_mode = 3;
  makeObjects();
}

// 80077DFC
// Updates the objects (unless hidden) and steps: set up -> loading -> every object ready (the radius
// the largest of theirs) -> shown (each object triggered) -> preloaded once all are active; state 5
// finishes it.
void CUnlockDisplay::Update() {
  if (!m_hidden)
    for (CUnlockDisplayObject* o = m_objects; o;) {
      CUnlockDisplayObject* next = o->m_next;
      o->Update();
      o = next;
    }
  switch (m_state) {
    case 0:
      if (m_mode) m_state = 1;
      return;
    case 1:
      m_state = 2;
      [[fallthrough]];
    case 2: {
      bool ready = true;
      for (CUnlockDisplayObject* o = m_objects; o && ready; o = o->m_next) {
        if (!o->m_ready) ready = false;
        else if (m_radius < o->m_radius) m_radius = o->m_radius;
      }
      if (!ready) return;
      m_state = 3;
    }
      [[fallthrough]];
    case 3:
      for (CUnlockDisplayObject* o = m_objects; o; o = o->m_next) o->m_trigger = 1;
      m_shown = 1;
      m_state = 4;
      [[fallthrough]];
    case 4: {
      bool done = true;
      m_preloaded = 1;
      for (CUnlockDisplayObject* o = m_objects; o && done; o = o->m_next) {
        if (!o->m_done) done = false;
        if (!o->IsActive() || !o->m_ready) m_preloaded = 0;
      }
      return;
    }
    case 5:
      m_shown = 0;
      m_done = 1;
      return;
    default:
      return;
  }
}

// 80077FA8
void CUnlockDisplay::SetDisplayBoxLeft(float v) {
  if (v < m_boxRight) m_boxLeft = v;
}
// 80077FC0
void CUnlockDisplay::SetDisplayBoxTop(float v) {
  if (v < m_boxBottom) m_boxTop = v;
}
// 80077FD8
void CUnlockDisplay::SetDisplayBoxRight(float v) {
  if (m_boxLeft < v) m_boxRight = v;
}
// 80077FF0
void CUnlockDisplay::SetDisplayBoxBottom(float v) {
  if (m_boxTop < v) m_boxBottom = v;
}
// 80078008
void CUnlockDisplay::SetViewZoomFactor(float zoom) { m_zoom = zoom; }
// 80078010
void CUnlockDisplay::ManualCameraPos(float x, float y, float z) { m_cameraPos = EVec3{x, y, z}; }

// 80078020
// Once shown (and not hidden or done). An object display looks at the objects' centre from the camera
// offset turned by the first object's rotation, far enough for a 55-degree view (horizontal) to hold
// the radius, 1.05 x, over the zoom; near / far 1.1 radii either side of the centre. Other displays
// look along y at (0, 0, 1.64) from 1.205 in front. Then into the box: the depth cleared, the lights
// (each turned by the first object's rotation, or a relative one by its angles about the view
// direction) and every object; the window that was current is selected again.
void CUnlockDisplay::Draw(ERC* rc) {
  if (!m_shown || m_hidden || m_done) return;
  const float fov = 55.0f;
  EVec3 up{0.0f, 0.0f, 1.0f};
  EVec3 center{0.0f, 0.0f, 0.0f};
  EVec3 eye{0.0f, 0.0f, 0.0f};
  if (m_mode == 1) {
    unsigned int count = 0;
    for (CUnlockDisplayObject* o = m_objects; o; o = o->m_next) {
      ++count;
      center.x += o->m_center.x, center.y += o->m_center.y, center.z += o->m_center.z;
    }
    const float inv = 1.0f / static_cast<float>(count);
    center.z *= inv, center.x *= inv, center.y *= inv;
    CUnlockDisplayObject* o = m_objects;
    if (o->m_next && o)
      for (; o; o = o->m_next) {
        const EVec3 d{o->m_center.x - center.x, o->m_center.y - center.y, o->m_center.z - center.z};
        if (m_radius < Mag(d) + o->m_radius) m_radius = (Mag(d) + o->m_radius) * 0.925f;
      }
    const EMat4 rot = RotationXYZ(m_objects->m_rot);
    const EVec3 dir = Rotate(m_cameraPos, rot);
    EVec3 offset{m_radius * dir.x, m_radius * dir.y, m_radius * dir.z};
    eye = EVec3{center.x + offset.x, center.y + offset.y, center.z + offset.z};
    EVec3 n{eye.x - center.x, eye.y - center.y, eye.z - center.z};
    if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f) Normalize(n);
    const float half = fov * 0.5f * 0.017453292f;
    const float d = m_radius * simmy::dm::sin(1.5707963146269321f - half) / simmy::dm::sin(half);
    const float dist = (d + m_radius) * 1.05f / m_zoom;
    const EVec3& pos = m_objects->m_pos;
    eye = EVec3{center.x + n.x * dist - pos.x, center.y + n.y * dist - pos.y, center.z + n.z * dist - pos.z};
    SetLookAt(eye, center, up);
    const EVec3 toCenter{center.x - eye.x, center.y - eye.y, center.z - eye.z};
    float zFar = m_objects->m_radius * 1.1f + Mag(toCenter);
    float zNear = Mag(toCenter) - m_objects->m_radius * 1.1f;
    if (zNear < 0.0f) zNear = 0.0f;
    if (zFar <= zNear) zFar = zNear + 0.01f;
    const float w = ScreenW(), h = ScreenH();
    SetProjection(fov * h / w, (m_boxRight - m_boxLeft) * w / ((m_boxBottom - m_boxTop) * h), zNear, zFar);
  } else {
    SetProjection(ScreenH() * fov / ScreenW(), _pGfx->GetScreenAspect(), 0.2f, 4.0f);
    center = EVec3{0.0f, 0.0f, 1.64f};
    eye = EVec3{0.0f, 1.205f, 1.64f};
    SetLookAt(eye, center, up);
  }
  TRect<float> box{m_boxLeft, m_boxTop, m_boxRight, m_boxBottom};
  SetViewport(box);
  EWindow* window3D = EWindow::m_pCurrent3DWindow;
  EWindow* window = EWindow::m_pCurrentWindow;
  Select(rc);
  rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  for (unsigned int i = 0; static_cast<int>(i) < static_cast<int>(m_lights.m_numDir); ++i) {
    EVec3 v;
    EMat4 m;
    if (m_lightRelative[i] == 0) {
      m = RotationXYZ(m_objects->m_rot);
      v = m_lightDir[i];
    } else {
      v = EVec3{center.x - eye.x, center.y - eye.y, center.z - eye.z};
      m = RotationXYZ(m_lightDir[i]);
    }
    EVec3 out = Rotate(v, m);
    if (out.x != 0.0f || out.y != 0.0f || out.z != 0.0f) Normalize(out);
    m_lights.m_dir[i].m_dir[0] = out.x;
    m_lights.m_dir[i].m_dir[1] = out.y;
    m_lights.m_dir[i].m_dir[2] = out.z;
  }
  rc->Lights(&m_lights);
  for (CUnlockDisplayObject* o = m_objects; o;) {
    CUnlockDisplayObject* next = o->m_next;
    o->Draw(rc);
    o = next;
  }
  if (window3D) window3D->Select(rc);
  else if (window) window->Select(rc);
}

// 80078B74
void CUnlockDisplay::SetDirectLight(float r, float g, float b, int index) {
  if (index >= 4) return;
  if (static_cast<int>(m_lights.m_numDir) < index + 1) m_lights.m_numDir = static_cast<unsigned int>(index + 1);
  m_lights.m_dir[index].m_color[0] = r;
  m_lights.m_dir[index].m_color[2] = b;
  m_lights.m_dir[index].m_color[1] = g;
}

// 80078BD0
void CUnlockDisplay::SetAmbientLight(float r, float g, float b) {
  m_lights.m_ambient[0] = r;
  m_lights.m_ambient[2] = b;
  m_lights.m_ambient[1] = g;
}

// 80078C08
// GC: computed and dropped (returns nothing); here the result.
bool CUnlockDisplay::HasDrawn() {
  bool all = true;
  for (CUnlockDisplayObject* o = m_objects; o; o = o->m_next) all = all && o->m_hasDrawn;
  return m_objects ? all : false;
}

// 80078C44
// GC: the result is not returned (the loop runs to the end).
bool CUnlockDisplay::ObjectHasMultiColor() {
  bool any = false;
  for (CUnlockDisplayObject* o = m_objects; o; o = o->m_next) {
    if (!any) {
      ERModel* model = o->GetModel();
      if (model && model->HasModifiableColor()) any = true;
    }
  }
  return any;
}

// 80078CAC
void CUnlockDisplay::SetMultiColorNumber(int index) {
  for (CUnlockDisplayObject* o = m_objects; o; o = o->m_next) o->SetMultiColorIndex(index);
}

// 80078CF8
void CUnlockDisplay::SetDirectLightDir(EVec3 dir, bool relative, int index) {
  if (index >= 4) return;
  if (static_cast<int>(m_lights.m_numDir) < index + 1) m_lights.m_numDir = static_cast<unsigned int>(index + 1);
  m_lightRelative[index] = relative ? 1 : 0;
  if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) Normalize(dir);
  m_lightDir[index] = dir;
}

// 80078DAC
void CUnlockDisplay::RemoveAllDirectLights() { m_lights.m_numDir = 0; }

// 80078DB8
int CUnlockDisplay::GetBedOtherHalfIDs(unsigned int model, unsigned int& otherModel, unsigned int& otherAnim) {
  for (int i = 0; i < 9; ++i) {
    if (kBedA[i] == model) {
      otherModel = kBedB[i];
      otherAnim = kBedD[i];
      return 1;
    }
    if (kBedB[i] == model) {
      otherModel = kBedA[i];
      otherAnim = kBedC[i];
      return 1;
    }
  }
  return 0;
}

// 80078FDC
int CUnlockDisplay::IsDone() { return m_done; }
// 80078FE4
void CUnlockDisplay::Hide() { m_hidden = 1; }
// 80078FF0
void CUnlockDisplay::UnHide() { m_hidden = 0; }
// 80078FFC
int CUnlockDisplay::DonePreloadObject() { return m_preloaded; }
// 80079004 operator new / 80079044 operator delete: MainHeap, 16-byte aligned (the host allocator here).

// ---------------------------------------------------------------------------------------------------
// CUnlockDisplayObject

// 80079078
CUnlockDisplayObject::CUnlockDisplayObject() {
  m_orient = new EMat4;
  m_orient->Id();
  m_radius = 0.0f;
  m_instance = new EIStaticModel;
  m_colorIndex = -1;
}

// 800791B0
// The instance, the references taken (the model's own once got, else by id; the animation's), the
// controller and the matrix.
CUnlockDisplayObject::~CUnlockDisplayObject() {
  delete m_instance;
  m_instance = nullptr;
  if (m_state != 0) {
    if (!m_model) _modelman.DelRefAsync(m_modelId);
    else m_model->DelRef();
    if (!m_simple) _animman.DelRefAsync(m_animId);
  }
  if (m_anim) {
    m_anim->Shutdown();
    delete m_anim;
    m_anim = nullptr;
  }
  delete m_orient;
  m_orient = nullptr;
}

// 800792B8
// The model (and the animation, unless simple) loaded.
bool CUnlockDisplayObject::ServiceDataReady() {
  if (!m_model) m_model = static_cast<ERModel*>(_modelman.GetRef(m_modelId));
  if (m_simple) return m_model != nullptr;
  if (!m_animRes) m_animRes = static_cast<ERAnim*>(_animman.GetRef(m_animId));
  if (!m_model) return false;
  return m_animRes != nullptr;
}

// 80079358
// A controller on the character (unless simple) at the model's scale; the instance takes the model.
void CUnlockDisplayObject::Setup() {
  if (!m_simple) {
    m_anim = new EAnimController;
    m_anim->Init(m_characterId, 1, 0);
    m_anim->m_scale = m_model->m_scale;
  }
  m_instance->SetModel(m_modelId, false, nullptr, nullptr);
}

// 800793D8
// Steps with a model (and character + animation unless simple): 0 asks for the resources; 1 once they
// are in, sets up, starts the animation (track 0x10000 at speed 1) and takes the bound box's radius
// and centre (in world units, plus the orientation's position; model 0xEA3318BC's box is cut to 0.3 x
// 0.66 x 0.2 of it, about its base); 2 ready (the colour applied) until triggered; 3 turns by the
// rotation speed and animates, looping (or holding the last frame) until done; 4 done.
void CUnlockDisplayObject::Update() {
  if (!m_modelId || (!m_simple && (!m_animId || !m_characterId))) return;
  switch (m_state) {
    case 0:
      _modelman.AddRefAsync(m_modelId);
      if (!m_simple) _animman.AddRefAsync(m_animId);
      m_state = 1;
      return;
    case 1: {
      if (!ServiceDataReady()) return;
      Setup();
      EMat4 id;
      id.Id();
      if (!m_simple) {
        m_anim->SetTrackAnim(1, m_animId, -1, -1.0f, nullptr);
        m_anim->m_speed = 1.0f;
        m_anim->m_computeCallback = nullptr;
        // GC: +0x68 = this (the callback's data; no callback is set).
        m_anim->SetTrackSpeed(0x10000, 1.0f);
      }
      // EBound3::Compute of the model's box through the identity.
      float b[6];
      for (int i = 0; i < 6; ++i) b[i] = m_model->m_bounds[i];
      if (m_modelId == 0xEA3318BCu) {
        b[0] *= 0.3f;
        b[3] = -b[0];
        b[1] *= 0.66f;
        b[4] = -b[1];
        b[2] = 0.0f;
        b[5] *= 0.2f;
      }
      const EVec3 diag{b[3] - b[0], b[4] - b[1], b[5] - b[2]};
      const float len = Mag(diag);
      const float scale = m_model->m_scale;
      m_radius = len * 0.5f * scale;
      m_center = EVec3{(b[0] + b[3]) * 0.5f * scale, (b[1] + b[4]) * 0.5f * scale, (b[2] + b[5]) * 0.5f * scale};
      m_center.x += m_orient->m[3][0];
      m_center.y += m_orient->m[3][1];
      m_center.z += m_orient->m[3][2];
      m_state = 2;
      return;
    }
    case 2:
      m_ready = 1;
      if (m_colorIndex > -1) SetMultiColorIndex(m_colorIndex);
      if (m_trigger) m_state = 3;
      return;
    case 3: {
      const float t = _dt * 60.0f;
      m_rot.x += m_rotAdd.x * t;
      m_rot.y += m_rotAdd.y * t;
      m_rot.z += m_rotAdd.z * t;
      m_visible = 1;
      if (m_simple) return;
      m_anim->Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
      m_anim->Compute(*m_orient, _framecount);
      if (!m_anim->IsTrackAnimComplete(0x10000)) return;
      if (m_playAndHold) {
        m_anim->SetTrackSpeed(0x10000, 0.0f);
        m_anim->SetTrackPos(0x10000, 0.999f, true);
      } else if (m_loops) {
        m_anim->RestartTrack(0x10000);
        return;
      }
      m_state = 4;
      return;
    }
    case 4:
      m_done = 1;
      return;
    default:
      return;
  }
}

// 80079874
// Once visible: animated, with the controller's node matrices (indexed by the display lists when the
// model's are), else the model's scale matrix times the orientation.
void CUnlockDisplayObject::Draw(ERC* rc) {
  if (!m_visible) return;
  if (!m_simple && m_anim) {
    m_anim->Compute(*m_orient, _framecount);
    const int count = m_anim->NodeCount();
    if (m_instance->m_model && m_instance->m_model->m_matsInDLs) rc->ModelMatrixList(m_anim->m_matrices, count);
    else rc->ModelMatrices(m_anim->m_matrices, count);
    m_instance->DrawImmediate(rc, m_anim);
  } else {
    EMat4 m;
    m.Mult4x4(*m_model->GetScaleMatrix(), *m_orient);
    rc->ModelMatrices(&m, 1);
    m_instance->DrawImmediate(rc, nullptr);
  }
  m_hasDrawn = 1;
}

// 800799A0
bool CUnlockDisplayObject::IsActive() { return m_visible ? m_done == 0 : false; }
// 800799C4
ERModel* CUnlockDisplayObject::GetModel() { return m_model; }

// 800799CC
// Applied once the instance can take it (ready, with a modifiable colour); else kept for later.
void CUnlockDisplayObject::SetMultiColorIndex(int index) {
  m_colorIndex = index;
  if (m_ready && m_instance->HasModifiableColor()) {
    m_instance->ModifyColor(static_cast<unsigned int>(index));
    m_colorIndex = -1;
  }
}

// 80079A54
int CUnlockDisplayObject::IsSimple() { return m_simple; }
// 80079A5C
void CUnlockDisplayObject::SetModelID(unsigned int id) { m_modelId = id; }
// 80079A64
void CUnlockDisplayObject::SetCharacterID(unsigned int id) {
  m_characterId = id;
  m_simple = (id == 0 || m_animId == 0) ? 1 : 0;
}
// 80079A8C
void CUnlockDisplayObject::SetAnimationID(unsigned int id) {
  m_animId = id;
  m_simple = (m_characterId == 0 || id == 0) ? 1 : 0;
}
// 80079AB4
void CUnlockDisplayObject::SetPos(EVec3& pos) { m_pos = pos; }
// 80079AD4
EVec3 CUnlockDisplayObject::GetPos() { return m_pos; }
// 80079AF8
void CUnlockDisplayObject::SetRot(EVec3& rot) { m_rot = rot; }
// 80079B18
EVec3 CUnlockDisplayObject::GetRot() { return m_rot; }
// 80079B3C
void CUnlockDisplayObject::SetRotAdd(EVec3& rot) { m_rotAdd = rot; }
// 80079B5C
EVec3 CUnlockDisplayObject::GetRotAdd() { return m_rotAdd; }
// 80079B80
void CUnlockDisplayObject::SetAnimationLoops(unsigned int loops) { m_loops = loops; }
// 80079B88
void CUnlockDisplayObject::SetPlayAndHold(bool hold) { m_playAndHold = hold ? 1 : 0; }
// 80079B90
void CUnlockDisplayObject::SetOrient(EMat4& m) { *m_orient = m; }
// 80079C18
int CUnlockDisplayObject::IsReady() { return m_ready; }
// 80079C20
void CUnlockDisplayObject::SetTrigger() { m_trigger = 1; }
// 80079C2C
int CUnlockDisplayObject::IsDone() { return m_done; }
// 80079C34
double CUnlockDisplayObject::GetRadius() { return m_radius; }
// 80079C3C
EVec3 CUnlockDisplayObject::GetCenter() { return m_center; }
// 80079C60
int CUnlockDisplayObject::HasDrawn() { return m_hasDrawn; }
// 80079C68
int CUnlockDisplayObject::GetType() { return m_type; }

// ---------------------------------------------------------------------------------------------------
// CUnlockDisplayObjectSim

// 80079C70
CUnlockDisplayObjectSim::CUnlockDisplayObjectSim() {
  m_type = 2;
  m_descPtr = nullptr;
  m_person = nullptr;
  m_sim = nullptr;
  m_anim = nullptr;
}

// 80079CD0
// The ESim (and with it the controller, which is its own).
CUnlockDisplayObjectSim::~CUnlockDisplayObjectSim() {
  m_anim = nullptr;
  delete m_sim;
  m_sim = nullptr;
}

// 80079D48
// An ESim made for the person, its description copied; the sim's controller animates it at speed 1.
void CUnlockDisplayObjectSim::SetPerson(cXPerson* person) {
  m_person = person;
  m_sim = new ESim(m_person);
  if (CasSimDescriptionS2C* d = m_person->GetSimDescription()) m_desc = *d;
  m_descPtr = &m_desc;
  m_anim = &m_sim->m_anim;
  m_sim->m_anim.m_speed = 1.0f;
}

// 80079E38
// The sim, its controller and animation, and no model rebuild pending.
bool CUnlockDisplayObjectSim::ServiceDataReady() {
  bool ready = !(m_sim == nullptr || m_anim == nullptr || m_animId == 0);
  m_sim->UpdateQueuedOperation();
  if (m_sim->HasQueuedOperation()) ready = false;
  return ready;
}

// 80079EAC
void CUnlockDisplayObjectSim::Setup() {}

// 80079EB0
// Posed at its position and rotation, drawn with the controller's matrices (unless rebuilding).
void CUnlockDisplayObjectSim::Draw(ERC* rc) {
  if (!m_visible || !m_anim) return;
  m_sim->UpdateQueuedOperation();
  if (m_sim->HasQueuedOperation()) return;
  EVec3 scale{0.0f, 0.0f, 0.0f};
  EMat4 orient;
  EAnimController::CalcOrientMatrix(m_pos, m_rot, scale, orient);
  m_anim->Compute(orient, _framecount);
  m_sim->DrawSimModel(rc, m_anim->m_matrices, static_cast<unsigned int>(m_anim->NodeCount()));
}

// 80079F78
int CUnlockDisplayObjectSim::IsMale() { return m_descPtr->m_male; }
// 80079F84
int CUnlockDisplayObjectSim::IsAdult() { return 1; }
