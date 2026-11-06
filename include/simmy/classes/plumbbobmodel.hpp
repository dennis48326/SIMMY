// @class PlumbBobParms after ERModel EMat4 EVec2 EVec3
// @class PlumbBobModel
// The plumb bob (game/plumbbobmodel.cpp, and the tuning values of game/plumbbob.cpp): a model drawn
// spinning about z and optionally bobbing up and down, plus an optional shadow model.
#pragma once

// PlumbBob's tuning values (0x64 bytes; GC: a tweakable object with its vtable at +0x60).
struct PlumbBobParms {
  PlumbBobParms();  // 8005B00C
  static void Validate();  // 8005B1AC
  void Calculate();  // 8005B1CC

  float m_colorScale[6] = {};      // +0x00 (not used by the model)
  float m_bounceRange[2] = {};     // +0x18 height offsets the bounce moves between
  float m_height = 0;              // +0x20 height above its owner
  float m_field24 = 0;             // +0x24
  float m_bouncePeriod = 0;        // +0x28 seconds per half bounce
  float m_spinSpeed = 0;           // +0x2C radians per second
  float m_field30[6] = {};         // +0x30
  float m_field48[4] = {};         // +0x48
  int m_field58 = 0;               // +0x58
  float m_bounceRate = 0;          // +0x5C 1 / m_bouncePeriod (Calculate)
};

// PlumbBob::s_plumbBobParms (804855A0). PlumbBob itself is not ported, so this stands alone.
extern PlumbBobParms g_plumbBobParms;

struct PlumbBobModel {
  PlumbBobModel();  // 8005B290
  ~PlumbBobModel();  // 8005B2F4
  unsigned int SetModel(unsigned int id);  // 8005B360
  unsigned int SetShadow(unsigned int id);  // 8005B3E0
  void DrawShadow(ERC* rc);  // 8005B460
  void Draw(ERC* rc);  // 8005B50C
  void GetExtents(EVec2& out) const;  // 8005B748

  float m_scale = 1.0f;          // +0x00
  float m_heightScale = 1.0f;    // +0x04 extra z scale
  float m_tilt = 0;              // +0x08 rotation about x (radians)
  EVec3 m_pos;                   // +0x0C
  float m_baseHeight = 0;        // +0x18
  float m_height = 0;            // +0x1C this frame's height (base + bounce)
  int m_bounce = 0;              // +0x20
  int m_spin = 0;                // +0x24
  ERModel* m_model = nullptr;    // +0x28
  ERModel* m_shadow = nullptr;   // +0x2C
  float m_bounceTime = 0;        // +0x30
  float m_spinAngle = 0;         // +0x34
  int m_bounceDir = 0;           // +0x38
};
