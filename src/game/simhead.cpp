// Port of game/simhead.cpp: ESims3DHead, the sim's head in the HUD's mood widget. See simhead.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>

ERShader* ESims3DHead::m_pShd = nullptr;
ERShader* ESims3DHead::m_pHeadBorder = nullptr;

namespace {
// The view (ESims3DHead_yfov / _near / _far) and where each player's head goes on the screen
// (_p1head_xoff/_yoff, _p2head_xoff/_yoff, 804FDB00).
const float kYFov = 14.0f, kNear = 0.5f, kFar = 500.0f;
const float kP1HeadX = 0.189063f, kP1HeadY = 0.096f, kP2HeadX = 0.841f, kP2HeadY = 0.788627f;
// The head parts drawn with the body's matrices (kPartGUID.6969, 803D50F0: head, facial feature, the
// four hairs, hat, inner torso); then the glasses (part 0).
const int kHeadParts[] = {1, 0x12, 2, 3, 4, 5, 6, 0x13, 7};
const int kHeadNode = 0xf;
// _RED, _YELLOW (set at start-up).
EVec4 kRed{1.0f, 0.0f, 0.0f, 1.0f}, kYellow{1.0f, 1.0f, 0.0f, 1.0f};

EVec3 Row(const EMat4& m, int r) { return EVec3{m.m[r][0], m.m[r][1], m.m[r][2]}; }
EVec3 Cross(const EVec3& a, const EVec3& b) {
  return EVec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
void Normalize(float* v) {
  if (v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f) return;
  const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] /= l, v[1] /= l, v[2] /= l;
}
void SelectShader(ERShader* r, ERC* rc) {
  if (EShader* s = r ? r->GetShader() : nullptr) s->Select(rc, 0);
}
}  // namespace

// 8006D2FC
ESims3DHead::ESims3DHead(ESim* sim) {
  m_sim = sim;
  m_person = nullptr;
  InitHead(sim->m_person);
  m_lastHead.Id();
}

// 8006D35C
void ESims3DHead::InitShaders() {
  if (m_pShd) return;
  m_pShd = static_cast<ERShader*>(_shaderman.AddRef(0x1a18ca65u, nullptr, 0));
  m_pHeadBorder = static_cast<ERShader*>(_shaderman.AddRef(0x1239c594u, nullptr, 0));
}

// 8006D3CC
void ESims3DHead::ResetShaders() {
  if (m_pShd) {
    m_pShd->DelRef();
    m_pShd = nullptr;
  }
  if (m_pHeadBorder) {
    m_pHeadBorder->DelRef();
    m_pHeadBorder = nullptr;
  }
}

// 8006D418
void ESims3DHead::InitHead(cXPerson* person) {
  m_person = person;
  SetProjection(kYFov, 1.0f, kNear, kFar);
  m_width = 0.0845f;
  m_height = 0.118f;
  m_field39C = 0.0f;
}

// 8006D4C4
ESims3DHead::~ESims3DHead() {}

// 8006D518
void ESims3DHead::Update() {}

// 8006D51C
// The head's viewport (player 1's place, or player 2's in two-player games; player 2's head draws
// nothing in a one-player game). While the model is being rebuilt only the frame round the viewport
// is drawn (yellow; red for player 2). Otherwise the sim's pose is computed and the camera put 1.25
// along the head bone's y axis from a point 0.185 up its x axis (0.14 for children), looking there
// with the bone's x axis up; the depth cleared, z test and alpha test on, an ambient light (grey;
// blue for ghosts, green for green sims) and two lights from behind the camera; the head parts
// drawn with the body's skinning matrices, then the glasses. The window that was current is selected
// again.
// GC: the head's orientation is also blended towards the last frame's (EMat4::BlendQuat, by
// dt * 0.157 * the simulator speed over the angle, at once past 0.47) but the blend's result is not
// used (only the current orientation is kept, +0x358): not ported.
void ESims3DHead::Draw(ERC* rc) {
  const bool isPlayer2 = _globals.GetSelectedPerson(1) == m_sim->m_person;
  EWindow* window = EWindow::m_pCurrentWindow;
  EWindow* window3D = EWindow::m_pCurrent3DWindow;
  const bool two = _gd.m_fieldEC != 0;
  if (isPlayer2 && !two) return;
  float x, y;
  if (two && isPlayer2) x = kP2HeadX, y = kP2HeadY;
  else if (!two) x = kP1HeadX, y = kP2HeadY;
  else x = kP1HeadX, y = kP1HeadY;
  m_pos = EVec3{x, 0.0f, y};
  TRect<float> viewport{m_pos.x, m_pos.z, m_pos.x + m_width, m_pos.z + m_height + 0.006f};
  SetViewport(viewport);
  auto reselect = [&] {
    if (window3D) window3D->Select(rc);
    else if (window) window->Select(rc);
  };
  if (m_sim->HasQueuedOperation()) {
    Select(rc);
    SelectShader(m_pShd, rc);
    rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    rc->ZTest(true, 2, 0, 0);
    rc->AlphaTest(true, 5, 0.5f, 0);
    reselect();
    const bool red = two && _globals.GetSelectedPerson(1) == m_person;
    SelectShader(m_pHeadBorder, rc);
    EVec2 p0{m_clip.x0 - 0.005f, m_clip.y0}, p1{m_clip.x1 + 0.005f, m_clip.y1};
    EVec2 uv0{0.0f, 1.0f}, uv1{1.0f, 0.0f};
    rc->Rect(p0, p1, uv0, uv1, red ? kRed : kYellow, 0.0f);
    return;
  }
  ESim* sim = m_sim;
  EAnimController& anim = sim->m_anim;
  anim.Compute(sim->m_orient, _framecount);  // GC: the instance's GetOrient (vtable 0x114)
  EMat4* matrices = anim.m_matrices;
  const int count = anim.NodeCount();
  if (!matrices || !count) return;
  rc->ModelMatrixList(matrices, count);
  EMat4 head;
  anim.CalcNodeOrient(kHeadNode, head);
  EVec3 target = Row(head, 3);
  head.m[3][0] = head.m[3][1] = head.m[3][2] = 0.0f;
  m_lastHead = head;
  const EVec3 ax = Row(head, 0), ay = Row(head, 1);
  const EVec3 eyeOff{ay.x * 1.25f, ay.y * 1.25f, ay.z * 1.25f};
  const float up = sim->IsAdult() ? 0.185f : 0.14f;
  target.x += ax.x * up, target.y += ax.y * up, target.z += ax.z * up;
  const EVec3 side = Cross(ax, eyeOff);
  EVec3 eye{eyeOff.x + target.x, eyeOff.y + target.y, eyeOff.z + target.z};
  EVec3 upAxis = ax;
  SetLookAt(eye, target, upAxis);
  Select(rc);
  SelectShader(m_pShd, rc);
  rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  rc->ZTest(true, 2, 0, 0);
  rc->AlphaTest(true, 5, 0.5f, 0);

  ELights lights;
  lights.m_numDir = 2;
  lights.m_numPoint = 0;
  if (m_person->IsGhost()) {
    lights.m_ambient[0] = 0.65f, lights.m_ambient[1] = 0.65f, lights.m_ambient[2] = 2.0f;
  } else if (m_person->IsGreen()) {
    lights.m_ambient[0] = 0.8f, lights.m_ambient[1] = 1.8f, lights.m_ambient[2] = 0.65f;
  } else {
    lights.m_ambient[0] = lights.m_ambient[1] = lights.m_ambient[2] = 0.4f;
  }
  ELightsDir& d0 = lights.m_dir[0];
  d0.m_color[0] = 0.35f, d0.m_color[1] = 0.35f, d0.m_color[2] = 0.5f;
  ELightsDir& d1 = lights.m_dir[1];
  d1.m_color[0] = 0.5f, d1.m_color[1] = 0.5f, d1.m_color[2] = 0.45f;
  for (int i = 0; i < 3; ++i) {
    d0.m_dir[i] = side[i] * 10.0f + ax[i] * -3.0f + eyeOff[i] * -10.0f;
    d1.m_dir[i] = side[i] * -10.0f + ax[i] * -3.0f + eyeOff[i] * -10.0f;
  }
  Normalize(d0.m_dir);
  Normalize(d1.m_dir);
  rc->Lights(&lights);
  SimModel* model = sim->m_simModel;
  // GC: also unless ESim +0x62C (not known; never set by the port).
  if (model) {
    for (int part : kHeadParts)
      if (EIStaticModel* p = model->m_parts[part])
        if (p->m_model) p->m_model->Draw(rc);
  }
  rc->ModelMatrixList(matrices, count);
  if (model && model->m_parts[0] && model->m_parts[0]->m_model) model->m_parts[0]->m_model->Draw(rc);
  reselect();
}

// 8006E634
// Just the viewport (see Draw).
void ESims3DHead::Draw2D(ERC*, cXPerson*) {
  const bool two = _gd.m_fieldEC != 0;
  const bool isPlayer2 = _globals.GetSelectedPerson(1) == m_sim->m_person;
  float x, y;
  if (two && isPlayer2) x = kP2HeadX, y = kP2HeadY;
  else if (!two) x = kP1HeadX, y = kP2HeadY;
  else x = kP1HeadX, y = kP1HeadY;
  m_pos = EVec3{x, 0.0f, y};
  TRect<float> viewport{m_pos.x, m_pos.z, m_pos.x + m_width, m_pos.z + m_height + 0.006f};
  SetViewport(viewport);
}
