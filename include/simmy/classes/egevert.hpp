// @class EGEVert
// An immediate-mode vertex (engine: ERC::TriStrip/LineList(EGEVert*, int)), 0x50 bytes. Normals and
// colours are ints whose low byte the renderer sends (GC: the byte at +3 of each big-endian word).
#pragma once

struct EGEVert {
  float pos[4] = {};         // +0x00 x, y, z, w
  int normal[4] = {};        // +0x10 x, y, z (signed bytes)
  float uv[2] = {};          // +0x20
  float uv2[2] = {};         // +0x28
  int color[4] = {};         // +0x30 r, g, b, a (0..255; rgb halved for the GC TEV doubling)
  float extra[4] = {};       // +0x40
};
static_assert(sizeof(EGEVert) == 0x50, "EGEVert is 0x50 bytes");
