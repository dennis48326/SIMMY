// @class EDirLight after EVec3
// A directional light as handed around (0x18 bytes): colour, then direction.
#pragma once

struct EDirLight {
  EVec3 m_color;  // +0x00
  EVec3 m_dir;    // +0x0C
};
