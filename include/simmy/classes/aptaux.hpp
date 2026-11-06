// @class AptAuxUnit
// @class AptAuxLayerInfo after AptAuxUnit
// The game's Apt render/load layer (game/aptauxpceorgl.cpp, "AptAuxPCEorGL"): the callbacks in
// gAptFuncs that load textures, shapes and strings and draw them through the ERC.
//
// A movie's <name>.geo ("Apt PCGL geometry file", big-endian) holds every shape as a list of units:
// solid triangles, textured triangles or lines, already in the shape's coordinates. On GC
// _loadGeometry fixed its offsets in place; on PC it is read into these native structs.
#pragma once

#include <memory>
#include <vector>

// One draw unit of a shape (0x68 bytes on GC).
struct AptAuxUnit {
  enum : int { kTriSolid = 1, kTriTiled = 4, kTriTiledClipped = 5, kLineSolid = 6 };
  int type = 0;                  // +0x00
  int count = 0;                 // +0x04 triangles (or lines)
  std::vector<float> points;     // +0x08 x, y pairs: 3 per triangle, 2 per line
  unsigned int colour = 0;       // +0x0C ARGB
  float texMatrix[16] = {};      // +0x10
  int textureCharacter = 0;      // +0x50 the bitmap character
  int textureSlot = -1;          // +0x54 gTextureSlots index (bindTexture)
  float uv[4] = {};              // +0x58 u0, v0, u1, v1 of the bitmap in its texture
};

// A shape's units (the geometry file's 12-byte character entries).
struct AptAuxShape {
  int character = 0;
  std::vector<AptAuxUnit*> units;
};

struct AptAuxGeometry {
  std::vector<std::unique_ptr<AptAuxShape>> shapes;
  std::vector<std::unique_ptr<AptAuxUnit>> unitStore;
  std::vector<std::pair<int, int>> textureMap;  // (bitmap character, texture file number)
  std::vector<int> textureSlots;                // per texture file number: gTextureSlots index or -1
};

// What a character id resolves to (AptAuxIDToChar): 1 a shape's units, 2 a bound texture slot.
struct AptAuxIDToChar {
  int kind = 0;
  AptAuxShape* shape = nullptr;
  int slot = -1;
};

// One loaded movie level (gLevelInfo entries, 0x20 bytes on GC).
struct AptAuxLayerInfo {
  int inUse = 0;                          // +0x00
  FlashBigFile* big = nullptr;            // +0x04
  std::unique_ptr<AptAuxGeometry> geometry;  // +0x08
  std::vector<AptAuxIDToChar> idToChar;   // +0x0C
  void* aptData = nullptr;                // +0x1C
};
