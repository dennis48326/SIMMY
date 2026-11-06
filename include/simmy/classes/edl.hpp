// @class EDLEntry
// @class EDL after EDLEntry
// Display lists (engine/e_dl.cpp): a chain of render commands that ERC records and ENgcRenderer::Execute
// runs. On the GameCube an entry was 8 bytes, an opcode byte, three argument bytes and one word that
// held either a value or a pointer, and commands with inline data (a Rect, a ClipRect) took several
// entries. On PC the word cannot hold a pointer, so an entry is 16 bytes with separate value and
// pointer fields, and inline data follows the header as a payload struct (EDLEntry::Payload).
#pragma once

#include <cstddef>
#include <vector>

// Opcodes: the index into ENgcRenderer's jump table.
enum EDLOp : unsigned char {
  kDL_TriStrip = 0, kDL_TriFan = 1, kDL_TriList = 2, kDL_QuadList = 3, kDL_PointList = 4,
  kDL_SpriteList = 5, kDL_DisplayList = 6, kDL_Goto = 7, kDL_End = 8, kDL_Viewport = 9,
  kDL_Scissor = 11, kDL_ModelMatrices = 12, kDL_ViewMatrix = 13, kDL_ProjectionMatrix = 14,
  kDL_WindowMatrix = 15, kDL_TextureMatrix = 16, kDL_Texture = 17, kDL_EnableGeometryModes = 18,
  kDL_DisableGeometryModes = 19, kDL_SetGeometryModes = 20, kDL_EnableRasterModes = 21,
  kDL_DisableRasterModes = 22, kDL_SetRasterModes = 23, kDL_Lights = 24, kDL_SendGSDisplayList = 25,
  kDL_LineList = 26, kDL_LineStrip = 27, kDL_SaveState = 28, kDL_RestoreState = 29,
  kDL_CallbackParam = 30, kDL_Callback = 31, kDL_GEList = 32, kDL_Rect = 33,
  kDL_ModelMatrixIndices = 34, kDL_Material = 35, kDL_MipMapSetup = 36, kDL_RecalcMatrices = 37,
  kDL_Debug = 38, kDL_SetMipMap = 39, kDL_GeometrySetup = 40, kDL_DirectRect = 41,
  kDL_TriStripPacked = 42, kDL_ZTest = 43, kDL_AlphaTest = 44, kDL_RenderSurface = 45,
  kDL_Vertex = 46, kDL_TriIndexed = 47, kDL_SaveImageData = 48, kDL_PointListPacked = 49,
  kDL_SpriteListPacked = 50, kDL_SetCombineMode = 51, kDL_SetBlendMode = 52, kDL_NoOp = 53,
  kDL_ClipRect = 54, kDL_MovieFrame = 55, kDL_TriStripPackedInt = 56, kDL_VerifyMpg = 57,
  kDL_RectList = 58, kDL_RectListRot = 59, kDL_ParticleList = 60, kDL_ParticleListRot = 61,
  kDL_LineStreakList = 62, kDL_SetAlpha = 63, kDL_CopyScreenToTexture = 64, kDL_Stencil = 65,
  kDL_NgcTexInvalidateAll = 66, kDL_NgcGXDisplayListInt = 67, kDL_NgcGXDisplayList = 68,
  kDL_NgcTriListInt = 69, kDL_NgcTriList = 70, kDL_NgcScreenTriList = 71, kDL_ReadMetrics = 72,
  kDL_Shader = 73, kDL_ScrambleRect = 74,
  kDL_OpCount = 75
};

struct EDLEntry {
  unsigned char op = 0;                   // GC +0
  unsigned char b1 = 0, b2 = 0, b3 = 0;   // GC +1..+3
  union {                                 // GC +4 when it holds a value
    unsigned int u;
    float f;
    unsigned char bytes[4];
  };
  void* p = nullptr;                      // GC +4 when it holds a pointer
  EDLEntry() : u(0) {}

  // Inline data stored in the entries after this one.
  template <class T> T* Payload() { return reinterpret_cast<T*>(this + 1); }
  template <class T> static constexpr int EntriesFor() {
    return 1 + static_cast<int>((sizeof(T) + sizeof(EDLEntry) - 1) / sizeof(EDLEntry));
  }
};
static_assert(sizeof(EDLEntry) == 16, "EDLEntry is 16 bytes on PC");

// Inline payloads (GC offsets from the header entry).
struct EDLRectData {          // kDL_Rect: 8 GC entries
  float x0, y0, x1, y1;       // +0x08 corners (normalised screen)
  float u0, v0, u1, v1;       // +0x18 texture coordinates
  float color[4];             // +0x28
  float z;                    // +0x38
};
struct EDLClipRectData {      // kDL_ClipRect: 3 GC entries
  float rect[4];
};
// kDL_RectList / kDL_RectListRot: 4 GC entries. The header holds the rectangle count in u (GC +2,
// a u16) and the rectangles in p (GC +4): 8 floats each (x0, y0, x1, y1, u0, v0, u1, v1), plus for
// RectListRot 8 more (m00, m01, m30, -, m10, m11, m31, -: a 2D transform about the rectangle's centre).
struct EDLRectListData {
  float color[4];             // +0x08
  float z;                    // +0x18
};

// kDL_NgcGXDisplayList / kDL_NgcGXDisplayListInt: 4 GC entries. A strip stored as a GX display list
// (index16 vertices) over vertex arrays, all big-endian as on the disc. The header holds the array
// length in u (GC +4) and in b1 whether the uvs are two interleaved sets. Int: s16 positions (x, y, z,
// pad) and s16 uvs; otherwise f32 positions (x, y, z, w) and f32 uvs. Normals are s8 x, y, z, pad and
// colours RGBA8.
struct EDLGXDisplayListData {
  const void* positions;      // +0x08
  const void* uvs;            // +0x0C
  const unsigned char* colors;   // +0x10
  const signed char* normals;    // +0x14
  const unsigned char* dl;    // +0x18
  unsigned int dlSize;        // +0x1C
};

// kDL_TriStripPackedInt: 4 GC entries. A plain (non-indexed) fixed-point strip: the header holds the
// vertex count in u (GC +2) and in b1 bit 0 two uv sets, bit 1 per-vertex weights (skinning). The
// arrays are big-endian as on the disc (s16 positions x, y, z, matrix slot; s16 uvs; RGBA8 colours;
// s8 normals). `dl` is the GX display list ENgcRenderer::CreateGXDisplayList built over them (vertex
// i uses entry i of every array), or null.
struct EDLTriStripPackedData {
  const void* positions;         // +0x08
  const void* uvs;               // +0x0C
  const unsigned char* colors;   // +0x10
  const signed char* normals;    // +0x14
  const unsigned char* weights;  // +0x18
  const unsigned char* dl;       // +0x1C
  unsigned int dlSize;           // +0x04
};

struct EDL {
  EDL() = default;
  explicit EDL(int blockSize);  // 80363014
  virtual ~EDL();  // 803630A0
  void Validate();  // 8036311C
  EDLEntry* GetStart();  // 80363378
  void* Alloc(unsigned int size, int align);  // 80363380
  void AllocExternal(void *, int);  // 803633A0
  void* AllocFlushable(unsigned int size, int align);  // 803633C0
  void AllocFlushableExternal(void *, int);  // 803633E4
  RET_UNKNOWN GetVertCount();  // 80363408
  double GetAverageStripLength();  // 80363410

  // PC: the two EAllocGroups (+0x00 ordinary, +0x18 flushable) are one list of owned blocks.
  struct Block { void* ptr; std::size_t align; };
  std::vector<Block> m_blocks;
  std::vector<ETexture*> m_textures;  // +0x30 textures the list selects (validated with it)
  std::vector<EDL*> m_children;       // +0x3C display lists it calls
  EDLEntry* m_start = nullptr;        // +0x48
  int m_terminated = 0;               // +0x4C
  int m_vertCount = 0;                // +0x54
  int m_numMatrices = 0;              // +0x60 model matrices the list indexes (callers must supply)
};
