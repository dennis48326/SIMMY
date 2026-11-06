// @class ELights
// The light set a render context hands the renderer (engine/e_lights.h), 0xF8 bytes: ambient,
// up to four directional lights and four point lights. ENgcRenderer::Lights copies it to +0x650.
#pragma once

struct ELightsDir {            // 0x18 bytes
  float m_color[3];
  float m_dir[3];              // direction in world space
};
struct ELightsPoint {          // 0x20 bytes
  float m_pos[3];
  float m_range;               // +0x0C; lights under 0.01 are skipped
  float m_color[3];            // +0x10
  float m_pad;
};
struct ELights {
  float m_ambient[4] = {};     // +0x00
  ELightsDir m_dir[4] = {};    // +0x10
  ELightsPoint m_point[4] = {};  // +0x70
  unsigned int m_numDir = 0;   // +0xF0
  unsigned int m_numPoint = 0; // +0xF4
};
static_assert(sizeof(ELights) == 0xF8);
