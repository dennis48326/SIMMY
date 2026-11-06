// Port of game/plumbbobmodel.cpp, and PlumbBobParms from game/plumbbob.cpp (the rest of PlumbBob,
// the Sims' plumb bobs in the world, is not ported).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

PlumbBobParms g_plumbBobParms;  // PlumbBob::s_plumbBobParms (804855A0, built by PlumbBob's static init)

// 8005B00C
PlumbBobParms::PlumbBobParms() {
  m_field58 = 1;
  m_height = 0.62f;
  m_field24 = 0.3f;
  m_bouncePeriod = 0.5f;
  m_spinSpeed = 3.0f;
  m_field30[0] = 90.0f;
  m_field30[1] = 0.1f;
  m_field30[2] = 30.0f;
  m_field30[3] = 0.05f;
  m_field30[4] = 0.0f;   // +0x40
  m_field30[5] = 2.25f;  // +0x44
  m_field48[0] = -0.5f;
  m_field48[1] = -3.0f;
  m_field48[2] = 25.0f;
  m_field48[3] = -9.0f;
  m_colorScale[0] = 0.51f;
  m_colorScale[1] = 1.99f;
  m_colorScale[2] = 0.72f;
  m_colorScale[3] = 0.51f;
  m_colorScale[5] = 1.99f;
  m_colorScale[4] = 1.22f;
  m_bounceRange[0] = -0.03f;
  m_bounceRange[1] = 0.03f;
  Calculate();
}

// 8005B1CC
void PlumbBobParms::Calculate() { m_bounceRate = 1.0f / m_bouncePeriod; }

// 8005B290
PlumbBobModel::PlumbBobModel() { m_baseHeight = g_plumbBobParms.m_height; }

// 8005B2F4
PlumbBobModel::~PlumbBobModel() {
  if (m_model) {
    m_model->DelRef();
    m_model = nullptr;
  }
  if (m_shadow) {
    m_shadow->DelRef();
    m_shadow = nullptr;
  }
}

namespace {
// Swaps the model held in `slot` for `id` (0 = none); returns the previous id.
unsigned int SwapModel(ERModel*& slot, unsigned int id) {
  const unsigned int old = slot ? slot->m_id : 0;
  if (id != old) {
    if (slot) {
      slot->DelRef();
      slot = nullptr;
    }
    if (id) slot = static_cast<ERModel*>(_modelman.AddRef(id, nullptr, 0));
  }
  return old;
}
}  // namespace

// 8005B360
unsigned int PlumbBobModel::SetModel(unsigned int id) { return SwapModel(m_model, id); }
// 8005B3E0
unsigned int PlumbBobModel::SetShadow(unsigned int id) { return SwapModel(m_shadow, id); }

// 8005B460
// The shadow lies flat at the bob's x, y.
void PlumbBobModel::DrawShadow(ERC* rc) {
  if (!m_shadow) return;
  EMat4 m;
  m.Id();
  m.m[3][0] = m_pos.x;
  m.m[3][1] = m_pos.y;
  m.m[3][2] = 0.0f;
  m.PreScale(m_shadow->m_scale);
  rc->ModelMatrices(&m, 1);
  m_shadow->Draw(rc);
}

// 8005B50C
// Spins about z (wrapping the angle into 0..2pi), bobs between the two bounce offsets (turning round
// every two periods), tilts, scales (bobs bigger than 1 are raised to stay above their owner) and
// draws at m_pos + (0, 0, height).
void PlumbBobModel::Draw(ERC* rc) {
  if (!m_model) return;
  EMat4 m;
  m.Id();
  if (m_spin) {
    float a = g_plumbBobParms.m_spinSpeed * _dt + m_spinAngle;
    m_spinAngle = a;
    if (a < 0.0f) a = 6.2831855f;
    else if (a > 6.2831855f) a = 0.0f;
    m_spinAngle = a;
    m.RotateZ(a);
  }
  m_height = m_baseHeight;
  if (m_bounce) {
    m_bounceTime += _dt;
    if (m_bounceTime > g_plumbBobParms.m_bouncePeriod + g_plumbBobParms.m_bouncePeriod) {
      m_bounceTime = 0.0f;
      m_bounceDir = 1 - m_bounceDir;
    }
    const float from = g_plumbBobParms.m_bounceRange[m_bounceDir], to = g_plumbBobParms.m_bounceRange[1 - m_bounceDir];
    m_height = m_height + m_bounceTime * 0.5f * g_plumbBobParms.m_bounceRate * (to - from) + from;
  }
  if (m_tilt != 0.0f) m.PostRotateX(m_tilt);
  m.m[3][0] = m_pos.x;
  m.m[3][1] = m_pos.y;
  m.m[3][2] = m_pos.z;
  EVec3 s{m_scale, m_scale, m_scale * m_heightScale};
  m.PreScale(s);
  if (m_scale > 1.0f)
    m_height = (m_model->m_bounds2[5] - m_model->m_bounds2[2]) * m_model->m_scale * 0.5f * (m_scale - 1.0f) + m_height;
  m.m[3][2] += m_height;
  m.PreScale(m_model->m_scale);
  rc->ModelMatrices(&m, 1);
  m_model->Draw(rc);
}

// 8005B748
// Width and top height of the bob in world units.
void PlumbBobModel::GetExtents(EVec2& out) const {
  if (!m_model) return;
  const ERModel* model = m_model;
  out.x = model->m_scale * (model->m_bounds[3] - model->m_bounds[0]) * m_scale;
  out.y = m_baseHeight;
  out.y = m_baseHeight + m_pos.z;
  const float halfHeight = model->m_scale * (model->m_bounds[5] - model->m_bounds[2]) * 0.5f;
  if (m_scale > 1.0f) out.y = (m_scale - 1.0f) * halfHeight + out.y;
  out.y = halfHeight * m_scale * m_heightScale + out.y;
}
