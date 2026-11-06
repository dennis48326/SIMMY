// Port of base/e_rtquantize.cpp: ERTQuantize, the octree colour quantiser. See ertquantize.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cfloat>
#include <cstring>
#include <new>

namespace {
float Bits(unsigned int u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
float Clamp255(float v) { return v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v; }
float SquareMag(const EVec3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }  // PSVECSquareMag
}  // namespace

// 802CE6E4
ERTQuantize::ERTQuantize() {
  m_freeList = 1;
  m_nodes = nullptr;
  m_initialized = 0;
}

// 802CE71C
ERTQuantize::~ERTQuantize() { Deallocate(); }

// 802CE76C
void ERTQuantize::Deallocate() {
  if (m_nodes) {
    m_free(m_nodes);
    m_nodes = nullptr;
  }
  m_freeList = 1;
}

// 802CE7BC
// The pool (nodes 1..n-1 on the free list), the YUV matrix (BT.601 luma and chroma, the chroma moved
// to 127.5 and scaled by a third) and its inverse, the cube, an empty cache.
int ERTQuantize::Init(unsigned int colors, unsigned int memory, void* (*alloc)(unsigned int), void (*free)(void*),
                      bool yuv) {
  Deallocate();
  if (!alloc || !free) {
    m_alloc = DefaultAlloc;
    m_free = DefaultFree;
  } else {
    m_alloc = alloc;
    m_free = free;
  }
  if (colors > 0x100) colors = 0x100;
  m_maxColors = colors;
  m_maxNodes = memory / 0x38 - 1;
  void* pool = m_alloc(static_cast<unsigned int>(m_maxNodes * sizeof(ERTQNode)));  // GC: 0x38 a node
  if (!pool) return 0;
  m_nodes = static_cast<ERTQNode*>(pool);
  for (unsigned int i = 0; i < m_maxNodes; ++i) new (&m_nodes[i]) ERTQNode();
  m_freeList = 1;
  for (unsigned int i = 1; i < m_maxNodes; ++i)
    m_nodes[i].m_nextFree = static_cast<unsigned short>(i < m_maxNodes - 1 ? i + 1 : 0);
  m_nodesUsed = 0;
  m_toYuv.Id();
  m_toYuv.m[0][0] = Bits(0x3e991687), m_toYuv.m[1][0] = Bits(0x3f1645a2), m_toYuv.m[2][0] = Bits(0x3de978d5);
  m_toYuv.m[0][1] = Bits(0xbe16f007), m_toYuv.m[1][1] = Bits(0xbe943958), m_toYuv.m[2][1] = Bits(0x3edfb15b);
  m_toYuv.m[0][2] = Bits(0x3f1d70a4), m_toYuv.m[1][2] = Bits(0xbf03d70a), m_toYuv.m[2][2] = Bits(0xbdcccccd);
  EVec3 offset{0.0f, 127.5f, 127.5f};
  m_toYuv.PostTranslate(offset);
  EVec3 scale{1.0f, Bits(0x3eaaaaab), Bits(0x3eaaaaab)};
  m_toYuv.PostScale(scale);
  m_fromYuv.Invert(m_toYuv);
  InitializeCube();
  for (ERTQCacheNode& c : m_cache) c.m_count = 0;
  m_yuv = yuv ? 1 : 0;
  m_initialized = 1;
  return 1;
}

// 802CEA0C
void* ERTQuantize::DefaultAlloc(unsigned int size) { return VMHeap()->Malloc(size, 0); }
// 802CEA44
void ERTQuantize::DefaultFree(void* p) { VMHeap()->Free(p); }
// 802CEA78
void* ERTQuantize::TextureAlloc(unsigned int size) { return TextureHeap()->Malloc(size, 0); }
// 802CEAB0
void ERTQuantize::TextureFree(void* p) { TextureHeap()->Free(p); }

// 802CEAE4
// Ten levels; each level's weight shift (8 down to 0); the root (centre 256) is its own parent and
// can never be pruned.
void ERTQuantize::InitializeCube() {
  m_depth = 10;
  int shift = 0x28;
  for (unsigned int bits = 0xffffffffu; bits; bits >>= 1) --shift;
  for (unsigned int level = 0; level <= m_depth; ++level) {
    m_shift[level] = shift;
    if (shift) --shift;
  }
  EVec3 center{256.0f, 256.0f, 256.0f};
  ERTQNode* root = InitializeNode(0, 0, nullptr, center);
  m_root = NodeToIndex(root);
  root->m_error = 0xffffffffu;
  root->m_parent = m_root;
  m_colors = 0;
}

// 802CEBD0
ERTQNode* ERTQuantize::InitializeNode(unsigned char id, unsigned int level, ERTQNode* parent, EVec3& center) {
  ERTQNode* node = IndexToNode(AllocNode());
  ++m_nodesUsed;
  if (!node) return nullptr;  // PC: GC used the null node
  node->m_parent = NodeToIndex(parent);
  for (unsigned short& c : node->m_child) c = 0;
  node->m_id = id;
  node->m_level = static_cast<unsigned char>(level);
  node->m_children = 0;
  node->m_center = center;
  node->m_unique = 0;
  node->m_error = 0;
  node->m_total[0] = node->m_total[1] = node->m_total[2] = 0.0f;
  return node;
}

// 802CECE0
// The colour (R, G, B bytes) counted in its cache slot; a different colour there is classified first.
void ERTQuantize::AddPixel(unsigned char* rgb) {
  const unsigned int key = static_cast<unsigned int>(rgb[2]) << 16 | static_cast<unsigned int>(rgb[1]) << 8 | rgb[0];
  ERTQCacheNode& slot = m_cache[key % 0x1ff];
  if (slot.m_color == key) {
    ++slot.m_count;
    return;
  }
  if (slot.m_count) FlushAdd(slot);
  slot.m_color = key;
  slot.m_count = 1;
}

// 802CED98
void ERTQuantize::FlushAdd(ERTQCacheNode& node) {
  unsigned char rgb[3] = {static_cast<unsigned char>(node.m_color), static_cast<unsigned char>(node.m_color >> 8),
                          static_cast<unsigned char>(node.m_color >> 16)};
  EVec3 c;
  TransformToYuv(rgb, c);
  Classify(c, static_cast<int>(node.m_count));
}

// 802CEE00
void ERTQuantize::TransformToYuv(unsigned char* rgb, EVec3& out) {
  const float r = rgb[0], g = rgb[1], b = rgb[2];
  if (!m_yuv) {
    out = EVec3{r, g, b};
    return;
  }
  const EMat4& m = m_toYuv;
  for (int j = 0; j < 3; ++j) out[j] = Clamp255(b * m.m[2][j] + r * m.m[0][j] + g * m.m[1][j] + m.m[3][j]);
}

// 802CEFD0
void ERTQuantize::TransformFromYUV(EVec3& in, unsigned char* rgb) {
  if (!m_yuv) {
    for (int j = 0; j < 3; ++j) rgb[j] = static_cast<unsigned char>(static_cast<int>(Clamp255(in[j])));
    return;
  }
  const EMat4& m = m_fromYuv;
  for (int j = 0; j < 3; ++j)
    rgb[j] = static_cast<unsigned char>(static_cast<int>(Clamp255(in[2] * m.m[2][j] + in[0] * m.m[0][j] + in[1] * m.m[1][j] + m.m[3][j])));
}

// 802CF1C8
// Down the tree to the deepest level (making the nodes on the way, pruning the deepest level first
// while the pool runs low), each node on the way weighing the pixels by its level; the leaf sums them.
void ERTQuantize::Classify(EVec3& color, int count) {
  while (m_maxNodes - 10 <= m_nodesUsed) {
    PruneLevel(IndexToNode(m_root));
    --m_depth;
  }
  ERTQNode* node = IndexToNode(m_root);
  for (unsigned int level = 1; m_depth != 0 && level <= m_depth; ++level) {
    unsigned int idx = node->m_center.x < color.x ? 1u : 0u;
    if (node->m_center.y < color.y) idx |= 2;
    if (node->m_center.z < color.z) idx |= 4;
    if (node->m_child[idx] == 0) {
      const float half = 256.0f / static_cast<float>(1 << level);
      node->m_children = static_cast<unsigned char>(node->m_children | 1u << idx);
      EVec3 center;
      for (int j = 0; j < 3; ++j) center[j] = (idx >> j & 1) ? node->m_center[j] + half : node->m_center[j] - half;
      const unsigned short child = NodeToIndex(InitializeNode(static_cast<unsigned char>(idx), level, node, center));
      node->m_child[idx] = child;
      if (!child) return;
      if (level == m_depth) ++m_colors;
    }
    node = IndexToNode(node->m_child[idx]);
    node->m_error += static_cast<unsigned int>(count) << m_shift[level];
  }
  node->m_unique += static_cast<unsigned int>(count);
  const float n = static_cast<float>(count);
  node->m_total[0] += color.x * n;
  node->m_total[1] += color.y * n;
  node->m_total[2] += color.z * n;
}

// 802CF4A8
void ERTQuantize::PruneLevel(ERTQNode* node) {
  if (node->m_children)
    for (unsigned int i = 0; i < 8; ++i)
      if (node->m_children >> i & 1) PruneLevel(IndexToNode(node->m_child[i]));
  if (node->m_level == m_depth) PruneChild(node);
}

// 802CF54C
// The node's pixels go to its parent, the node back to the pool.
void ERTQuantize::PruneChild(ERTQNode* node) {
  ERTQNode* parent = IndexToNode(node->m_parent);
  parent->m_children = static_cast<unsigned char>(parent->m_children & ~(1u << (node->m_id & 0x1f)));
  parent->m_child[node->m_id] = 0;
  parent->m_unique += node->m_unique;
  for (int j = 0; j < 3; ++j) parent->m_total[j] += node->m_total[j];
  --m_nodesUsed;
  FreeNode(NodeToIndex(node));
}

// 802CF634
// The cache flushed (each slot then caches a palette index), the tree reduced to the palette's size,
// the palette made.
void ERTQuantize::Compute() {
  for (ERTQCacheNode& c : m_cache) {
    if (c.m_count) FlushAdd(c);
    c.m_count = 0xffffffffu;
  }
  Reduction();
  m_colors = 0;
  MColormap(IndexToNode(m_root));
  m_state = 2;
}

// 802CF6CC
void ERTQuantize::Reduction() {
  if (m_maxColors >= m_colors) return;
  m_nextThreshold = 1;
  do {
    m_pruneThreshold = m_nextThreshold;
    m_nextThreshold = 0xffffffffu;
    m_colors = 0;
    Reduce(IndexToNode(m_root));
  } while (m_maxColors < m_colors);
}

// 802CF75C
// Nodes with the least weight (at most the threshold) are merged into their parents; the next
// threshold is the least weight left.
void ERTQuantize::Reduce(ERTQNode* node) {
  if (node->m_children)
    for (unsigned int i = 0; i < 8; ++i)
      if (node->m_children >> i & 1) Reduce(IndexToNode(node->m_child[i]));
  if (node->m_unique) ++m_colors;
  const unsigned int e = node->m_error;
  if (e < m_nextThreshold) {
    if (e <= m_pruneThreshold) {
      PruneChild(node);
      return;
    }
    m_nextThreshold = e;
  }
  if (m_pruneThreshold < node->m_error) return;
  PruneChild(node);
}

// 802CF838
void ERTQuantize::MColormap(ERTQNode* node) {
  if (node->m_children)
    for (unsigned int i = 0; i < 8; ++i)
      if (node->m_children >> i & 1) MColormap(IndexToNode(node->m_child[i]));
  if (!node->m_unique) return;
  const float inv = 1.0f / static_cast<float>(node->m_unique);
  m_palette[m_colors] = EVec3{node->m_total[0] * inv, node->m_total[1] * inv, node->m_total[2] * inv};
  node->m_color = static_cast<unsigned char>(m_colors);
  ++m_colors;
}

// 802CF95C
unsigned int ERTQuantize::GetPaletteSize() { return m_colors; }
// 802CF964
unsigned int ERTQuantize::GetFullPaletteSize() { return m_maxColors; }

// 802CF96C
void ERTQuantize::GetPaletteEntry(int index, unsigned char* rgb) {
  if (index < static_cast<int>(m_colors)) {
    TransformFromYUV(m_palette[index], rgb);
  } else {
    rgb[0] = rgb[1] = rgb[2] = 0;
  }
}

// 802CF9B8
// The palette index nearest the colour (cached): by distance to each entry for up to 16 colours,
// else from the deepest node the colour reaches, its parent's leaves.
unsigned int ERTQuantize::GetClosestColor(unsigned char* rgb) {
  const unsigned int key = static_cast<unsigned int>(rgb[2]) << 16 | static_cast<unsigned int>(rgb[1]) << 8 | rgb[0];
  ERTQCacheNode& slot = m_cache[key % 0x1ff];
  if (slot.m_color == key && slot.m_count != 0xffffffffu) return slot.m_count;
  EVec3 c;
  TransformToYuv(rgb, c);
  if (m_colors < 0x11) {
    unsigned int best = 0;
    float bestD = SquareMag(EVec3{m_palette[0].x - c.x, m_palette[0].y - c.y, m_palette[0].z - c.z});
    for (unsigned int i = 1; i < m_colors; ++i) {
      const float d = SquareMag(EVec3{m_palette[i].x - c.x, m_palette[i].y - c.y, m_palette[i].z - c.z});
      if (d < bestD) {
        best = i;
        bestD = d;
      }
    }
    m_closest = best;
  } else {
    ERTQNode* node = IndexToNode(m_root);
    for (;;) {
      unsigned int idx = node->m_center.x < c.x ? 1u : 0u;
      if (node->m_center.y < c.y) idx |= 2;
      if (node->m_center.z < c.z) idx |= 4;
      if (!(node->m_children >> idx & 1)) break;
      node = IndexToNode(node->m_child[idx]);
    }
    m_target = c;
    m_bestDistance = FLT_MAX;
    ClosestColor(IndexToNode(node->m_parent));
  }
  slot.m_color = key;
  slot.m_count = m_closest;
  return slot.m_count;
}

// 802CFC34
void ERTQuantize::ClosestColor(ERTQNode* node) {
  if (node->m_children)
    for (unsigned int i = 0; i < 8; ++i)
      if (node->m_children >> i & 1) ClosestColor(IndexToNode(node->m_child[i]));
  if (!node->m_unique) return;
  const EVec3& p = m_palette[node->m_color];
  const float d = SquareMag(EVec3{p.x - m_target.x, p.y - m_target.y, p.z - m_target.z});
  if (d < m_bestDistance) {
    m_bestDistance = d;
    m_closest = node->m_color;
  }
}

// 802CFDA0
unsigned short ERTQuantize::AllocNode() {
  const unsigned short i = m_freeList;
  if (i) m_freeList = m_nodes[i].m_nextFree;
  return i;
}
// 802CFDCC
void ERTQuantize::FreeNode(unsigned short index) {
  if (!index) return;
  m_nodes[index].m_nextFree = m_freeList;
  m_freeList = index;
}
// 802CFDEC
ERTQNode* ERTQuantize::IndexToNode(unsigned short index) { return index ? &m_nodes[index] : nullptr; }
// 802CFE0C
unsigned short ERTQuantize::NodeToIndex(ERTQNode* node) {
  return node ? static_cast<unsigned short>(node - m_nodes) : 0;
}
