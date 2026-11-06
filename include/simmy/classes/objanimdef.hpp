// @class ObjAnimDef
// What an object shows (game/iobject.cpp): its model, animation and shader change, as the
// simulation (or the world map's scenery) asks for them. 0x20 bytes.
#pragma once

struct ObjAnimDef {
  unsigned int m_model = 0;       // +0x00 0: keep
  unsigned int m_field04 = 0;     // +0x04
  unsigned int m_anim = 0;        // +0x08 0: no animation
  unsigned int m_shaderSlot = 0;  // +0x0C the shader to replace (ChageShader)
  unsigned int m_shader = 0;      // +0x10 and its replacement
  unsigned int m_field14[3] = {}; // +0x14
};
