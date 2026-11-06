// @class ERTQNode
// @class ERTQCacheNode
// @class ERTQuantize after ERTQNode ERTQCacheNode
// A colour quantiser (base/e_rtquantize.cpp): an octree over the colour cube (RGB, or YUV through a
// matrix with the "yuv" flag) collects the image's pixels (a 0x1FF-entry cache in front), prunes the
// tree while it would outgrow its node pool, then reduces it to at most the palette's size; the
// palette is the mean of each leaf, and a colour maps to the nearest palette entry (directly for
// small palettes, else through the tree). Used to turn rendered portraits into C8 textures
// (SimImageMaker::CopyToFinalImage) and skins into 8-bit textures.
#pragma once

#include <vector>

// 0x38 bytes on GC: a node of the octree (index 0 is null; nodes are kept in a pool by index).
struct ERTQNode {
  EVec3 m_center;              // +0x00 the cube's centre (GC: the free list's next index overlays it)
  float m_total[3] = {};       // +0x0C the sum of the colours (times their counts) at this node
  unsigned int m_error = 0;    // +0x18 the pixels below (weighted by depth)
  unsigned int m_unique = 0;   // +0x1C the pixels ending here
  unsigned short m_parent = 0;  // +0x20
  unsigned short m_child[8] = {};  // +0x22
  unsigned char m_children = 0;  // +0x32 the mask of children present
  unsigned char m_level = 0;     // +0x33
  unsigned char m_color = 0;     // +0x34 the palette index (after Compute)
  unsigned char m_id = 0;        // +0x35 the child slot in its parent
  unsigned short m_nextFree = 0;  // PC: the free list (GC: overlays +0x00)
};

// A cache slot: a 24-bit colour and how many times it was added (-1 after Compute: the colour's
// palette index is cached in the count).
struct ERTQCacheNode {
  unsigned int m_color = 0;
  unsigned int m_count = 0;
};

struct ERTQuantize {
  ERTQuantize();  // 802CE6E4
  virtual ~ERTQuantize();  // 802CE71C
  virtual void Deallocate();  // 802CE76C
  // `colors` the palette's size (at most 0x100), `memory` the node pool's bytes (0x38 a node), the
  // allocator (null: the default), `yuv` classify in YUV. 0 when the pool cannot be had.
  virtual int Init(unsigned int colors, unsigned int memory, void* (*alloc)(unsigned int), void (*free)(void*),
                   bool yuv);  // 802CE7BC
  static void* DefaultAlloc(unsigned int size);  // 802CEA0C
  static void DefaultFree(void* p);  // 802CEA44
  static void* TextureAlloc(unsigned int size);  // 802CEA78
  static void TextureFree(void* p);  // 802CEAB0
  void InitializeCube();  // 802CEAE4
  ERTQNode* InitializeNode(unsigned char id, unsigned int level, ERTQNode* parent, EVec3& center);  // 802CEBD0
  virtual void AddPixel(unsigned char* rgb);  // 802CECE0
  void FlushAdd(ERTQCacheNode& node);  // 802CED98
  void TransformToYuv(unsigned char* rgb, EVec3& out);  // 802CEE00
  void TransformFromYUV(EVec3& in, unsigned char* rgb);  // 802CEFD0
  void Classify(EVec3& color, int count);  // 802CF1C8
  void PruneLevel(ERTQNode* node);  // 802CF4A8
  void PruneChild(ERTQNode* node);  // 802CF54C
  virtual void Compute();  // 802CF634
  void Reduction();  // 802CF6CC
  void Reduce(ERTQNode* node);  // 802CF75C
  void MColormap(ERTQNode* node);  // 802CF838
  virtual unsigned int GetPaletteSize();  // 802CF95C
  unsigned int GetFullPaletteSize();  // 802CF964
  virtual void GetPaletteEntry(int index, unsigned char* rgb);  // 802CF96C
  virtual unsigned int GetClosestColor(unsigned char* rgb);  // 802CF9B8
  void ClosestColor(ERTQNode* node);  // 802CFC34
  unsigned short AllocNode();  // 802CFDA0
  void FreeNode(unsigned short index);  // 802CFDCC
  ERTQNode* IndexToNode(unsigned short index);  // 802CFDEC
  unsigned short NodeToIndex(ERTQNode* node);  // 802CFE0C

  EMat4 m_toYuv;                   // +0x08
  EMat4 m_fromYuv;                 // +0x48
  ERTQCacheNode m_cache[0x1ff];    // +0x88
  void* (*m_alloc)(unsigned int) = nullptr;  // +0x1080
  void (*m_free)(void*) = nullptr;           // +0x1084
  unsigned short m_root = 0;       // +0x1088
  unsigned short m_freeList = 0;   // +0x108A
  ERTQNode* m_nodes = nullptr;     // +0x108C the pool (PC: from m_alloc, constructed in place)
  int m_state = 0;                 // +0x1090 2: computed
  unsigned int m_nodesUsed = 0;    // +0x1094
  unsigned int m_maxColors = 0;    // +0x1098
  unsigned int m_maxNodes = 0;     // +0x109C
  unsigned int m_depth = 0;        // +0x10A0
  int m_initialized = 0;           // +0x10A4
  unsigned int m_colors = 0;       // +0x10A8
  unsigned int m_pruneThreshold = 0;  // +0x10AC
  unsigned int m_nextThreshold = 0;   // +0x10B0
  int m_shift[11] = {};            // +0x10B4 per level
  unsigned int m_closest = 0;      // +0x10E0
  EVec3 m_palette[0x100];          // +0x10E4
  EVec3 m_target;                  // +0x1CE4 (ClosestColor)
  float m_bestDistance = 0.0f;     // +0x1CF0
  int m_yuv = 0;                   // +0x1CF4
};
