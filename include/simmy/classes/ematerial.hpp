// @class EMaterial
// Lighting material (engine/e_material.h): what EShaderDef::m_material holds and the renderer copies
// to +0x518. 0x30 bytes in the original.
#pragma once

struct EMaterial {
  float m_diffuse[4] = {1, 1, 1, 0};   // +0x00 multiplies the light colours
  float m_ambient[4] = {1, 1, 1, 0};   // +0x10 multiplies the ambient light
  float m_specular[3] = {1, 1, 1};     // +0x20
  float m_power = 10;                  // +0x2C
};
static_assert(sizeof(EMaterial) == 0x30);
